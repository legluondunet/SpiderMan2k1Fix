/* Spider-Man 2001 (PC): guarded native DirectInput8 reacquisition
 * after DIERR_NOTACQUIRED, reusing the game's already-existing Acquire/retry.
 *
 * Observed under WinDbg on Windows 11:
 *   keyboard GetDeviceData at 0x00501BC3 -> HRESULT 0x8007000C
 *   mouse    GetDeviceData at 0x00501D02 -> HRESULT 0x8007000C
 *
 * Original logic only reacquires after DIERR_INPUTLOST (0x8007001E).
 * The game already has a correct per-device Acquire/retry block. When
 * the FIRST GetDeviceData returns DIERR_NOTACQUIRED AND the game's own
 * active flag equals 1, this experiment redirects the existing JNE
 * to that same original block. DIERR_INPUTLOST and DI_OK continue down
 * their original paths. Never calls DirectInput itself from the DLL.
 *
 * Strict safety measures:
 * - permanently requested, with an exact SHA-256-checked caller
 * - 32-bit image must have original 0x400000 base and complete signatures
 * - install only from game's render thread, at Flip #120 (not at startup)
 * - two six-byte JNE replacements; no DirectInput interface or IAT hooks
 * - branch offsets calculated from ACTUAL in-process addresses, NOT
 *   from temporary patch arrays (regression guard for withdrawn test)
 * - stubs preserve all general-purpose registers and the stack
 * - dynamic code memory remains allocated for the process lifetime
 * - keyboard edit rolled back if the mouse patch cannot be installed
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "input_notacquired_retry.h"

#define SPIDER_IMAGE_BASE 0x00400000u
#define SPIDER_GAME_ACTIVE 0x006B78F0u
#define SPIDER_KEY_GETDATA 0x00501BC3u
#define SPIDER_MOUSE_GETDATA 0x00501D02u
#define SPIDER_KEY_JNE 0x00501BCBu
#define SPIDER_MOUSE_JNE 0x00501D0Au
#define SPIDER_KEY_ACQUIRE 0x00501BD1u
#define SPIDER_MOUSE_ACQUIRE 0x00501D10u
#define SPIDER_KEY_NORMAL 0x00501C51u
#define SPIDER_MOUSE_NORMAL 0x00501D90u
#define SPIDER_DIERR_NOTACQUIRED 0x8007000Cu
#define SPIDER_DIERR_INPUTLOST 0x8007001Eu

/* Exact 3-byte COM call, 5-byte CMP, 6-byte JNE on both native x86 paths. */
static const BYTE getdata_signature[14] = {
  0xFF,0x51,0x28, 0x3D,0x1E,0x00,0x07,0x80,
  0x0F,0x85,0x80,0x00,0x00,0x00
};
static const BYTE original_jne[6] = {0x0F,0x85,0x80,0x00,0x00,0x00};

static volatile LONG requested=0, installed=0;
static volatile LONG retries_keyboard=0, retries_mouse=0;
static LONG reported_keyboard=0, reported_mouse=0;
static void (*write_log)(const char *)=NULL;
static BYTE *retained_executable_page=NULL;

static LONG snapshot(volatile LONG *v) {
  return InterlockedCompareExchange(v,0,0);
}
static void append32(BYTE **p, uint32_t v) {
  memcpy(*p,&v,4);
  *p+=4;
}
static void rel32(BYTE *field, const BYTE *target) {
  /* field lives at its FINAL executable address, not on the C stack. */
  uint32_t from=(uint32_t)(uintptr_t)field+4u;
  uint32_t to=(uint32_t)(uintptr_t)target;
  uint32_t delta=to-from;
  memcpy(field,&delta,4);
}
static BOOL rel8(BYTE *opcode, const BYTE *target) {
  intptr_t delta=(intptr_t)target-((intptr_t)opcode+2);
  if(delta < -128 || delta > 127) return FALSE;
  opcode[1]=(BYTE)(int8_t)delta;
  return TRUE;
}
static void emit_jmp(BYTE **p, const BYTE *destination) {
  BYTE *disp;
  *(*p)++=0xE9;
  disp=*p;
  append32(p,0u);
  rel32(disp,destination);
}
static void emit_je(BYTE **p, const BYTE *destination) {
  BYTE *disp;
  *(*p)++=0x0F;
  *(*p)++=0x84;
  disp=*p;
  append32(p,0u);
  rel32(disp,destination);
}
static void emit_cmp_eax(BYTE **p,uint32_t value) {
  *(*p)++=0x3D;
  append32(p,value);
}
static void emit_lock_inc(BYTE **p,volatile LONG *counter) {
  *(*p)++=0xF0; /* lock inc dword ptr [absolute counter] */
  *(*p)++=0xFF;
  *(*p)++=0x05;
  append32(p,(uint32_t)(uintptr_t)counter);
}
static BOOL make_stub(BYTE **next, BYTE *end, const BYTE *original_acquire,
                      const BYTE *original_normal,volatile LONG *count) {
  BYTE *p=*next;
  BYTE *not_na,*inactive,*other;
  if(end-p<80) return FALSE;

  /* Is the first GetDeviceData HRESULT DIERR_NOTACQUIRED? */
  emit_cmp_eax(&p,SPIDER_DIERR_NOTACQUIRED);
  not_na=p; *p++=0x75;*p++=0; /* jne original DIERR_INPUTLOST handling */

  /* Do NOT call Acquire while the game itself considers focus lost. */
  *p++=0x83;*p++=0x3D; /* cmp dword ptr [006B78F0], +1 */
  append32(&p,SPIDER_GAME_ACTIVE);
  *p++=0x01;
  inactive=p; *p++=0x75;*p++=0; /* jne original normal path */

  emit_lock_inc(&p,count);
  emit_jmp(&p,original_acquire);

  other=p;
  if(!rel8(not_na,other)) return FALSE;
  /* Preserve game's normal DIERR_INPUTLOST behavior. */
  emit_cmp_eax(&p,SPIDER_DIERR_INPUTLOST);
  emit_je(&p,original_acquire);
  if(!rel8(inactive,p)) return FALSE;
  emit_jmp(&p,original_normal);

  *next=p;
  return TRUE;
}
static BOOL replace_original_jne(BYTE *where,const BYTE *destination) {
  BYTE patch[6]={0xE9,0,0,0,0,0x90};
  DWORD old,ignored;
  uint32_t jump_rel32;

  if(memcmp(where,original_jne,sizeof(original_jne))!=0) return FALSE;

  /* CRITICAL: jump is executed at 'where'. Using the address of patch[1]
   * would produce a wild jump: that was the withdrawn test's failure. */
  jump_rel32=(uint32_t)(uintptr_t)destination-
             ((uint32_t)(uintptr_t)where+5u);
  memcpy(patch+1,&jump_rel32,4);

  if(!VirtualProtect(where,sizeof(patch),PAGE_EXECUTE_READWRITE,&old))
    return FALSE;
  memcpy(where,patch,sizeof(patch));
  FlushInstructionCache(GetCurrentProcess(),where,sizeof(patch));
  if(!VirtualProtect(where,sizeof(patch),old,&ignored) && write_log)
    write_log("DI8-RETRY: WARNING unable to restore page protection after patch");
  return TRUE;
}
static BOOL restore_original_jne(BYTE *where) {
  DWORD old,ignored;
  if(!VirtualProtect(where,sizeof(original_jne),PAGE_EXECUTE_READWRITE,&old))
    return FALSE;
  memcpy(where,original_jne,sizeof(original_jne));
  FlushInstructionCache(GetCurrentProcess(),where,sizeof(original_jne));
  if(!VirtualProtect(where,sizeof(original_jne),old,&ignored) && write_log)
    write_log("DI8-RETRY: WARNING unable to restore page protection after rollback");
  return TRUE;
}
static void try_install(void) {
  BYTE *image=(BYTE *)GetModuleHandleA(NULL);
  BYTE *page,*next,*key_stub,*mouse_stub;
  DWORD old;
  if(!image || sizeof(void*)!=4 || (uintptr_t)image!=SPIDER_IMAGE_BASE ||
     memcmp((void *)(uintptr_t)SPIDER_KEY_GETDATA,
            getdata_signature,sizeof(getdata_signature))!=0 ||
     memcmp((void *)(uintptr_t)SPIDER_MOUSE_GETDATA,
            getdata_signature,sizeof(getdata_signature))!=0) {
    if(write_log)
      write_log("DI8-RETRY: DISABLED: unsupported layout/signature; original EXE unchanged");
    return;
  }

  page=(BYTE *)VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  if(!page) {
    if(write_log) write_log("DI8-RETRY: DISABLED: VirtualAlloc failed");
    return;
  }
  next=page;
  key_stub=next;
  if(!make_stub(&next,page+4096,
                (const BYTE *)(uintptr_t)SPIDER_KEY_ACQUIRE,
                (const BYTE *)(uintptr_t)SPIDER_KEY_NORMAL,
                &retries_keyboard)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(write_log) write_log("DI8-RETRY: DISABLED: keyboard stub failed");
    return;
  }
  mouse_stub=next+16;
  next=mouse_stub;
  if(!make_stub(&next,page+4096,
                (const BYTE *)(uintptr_t)SPIDER_MOUSE_ACQUIRE,
                (const BYTE *)(uintptr_t)SPIDER_MOUSE_NORMAL,
                &retries_mouse) ||
     !VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(write_log) write_log("DI8-RETRY: DISABLED: mouse stub/RX failed");
    return;
  }
  FlushInstructionCache(GetCurrentProcess(),page,(SIZE_T)(next-page));

  if(!replace_original_jne((BYTE *)(uintptr_t)SPIDER_KEY_JNE,key_stub)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(write_log) write_log("DI8-RETRY: DISABLED: keyboard patch failed");
    return;
  }
  if(!replace_original_jne((BYTE *)(uintptr_t)SPIDER_MOUSE_JNE,mouse_stub)) {
    /* If rollback fails, keep the executable page allocated: never leave
     * the live branch pointing at freed memory. */
    if(restore_original_jne((BYTE *)(uintptr_t)SPIDER_KEY_JNE))
      VirtualFree(page,0,MEM_RELEASE);
    else retained_executable_page=page;
    if(write_log)
      write_log("DI8-RETRY: DISABLED: mouse patch failed; keyboard rollback attempted");
    return;
  }

  retained_executable_page=page;
  InterlockedExchange(&installed,1);
  if(write_log)
    write_log("DI8-RETRY: active at Flip #120; exact x86 branch patch verified; original Acquire/retry reused, foreground only");
}
void SpiderDI8Retry_Init(void (*log_fn)(const char *)) {
  write_log=log_fn;
  /* Permanent native DIERR_NOTACQUIRED recovery, requested only after
   * the supported game's SHA-256 was checked. Its existing in-game
   * signature verification and Flip #120 installation remain mandatory;
   * no unstable DirectInput COM-vtable hooks are enabled. */
  InterlockedExchange(&requested,1);
  if(write_log)
    write_log("DI8-RETRY: native input recovery requested permanently; guarded installation at Flip #120");
}

void SpiderDI8Retry_OnFlip(long flip_index) {
  if(snapshot(&requested)!=1) return;
  /* The original game calls Flip on its render thread. Delayed install
   * avoids patching keyboard/mouse while startup devices are being created. */
  if(flip_index==120) try_install();
  if(snapshot(&installed)!=1 || flip_index%120!=0) return;
  {
    LONG key=snapshot(&retries_keyboard),mouse=snapshot(&retries_mouse);
    if(key!=reported_keyboard || mouse!=reported_mouse) {
      char line[180];
      snprintf(line,sizeof(line),
        "DI8-RETRY: redirected DIERR_NOTACQUIRED to game's Acquire: keyboard=%ld mouse=%ld",
        (long)key,(long)mouse);
      if(write_log) write_log(line);
      reported_keyboard=key;
      reported_mouse=mouse;
    }
  }
}
