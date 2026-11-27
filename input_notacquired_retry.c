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
#include "escape_batch_trace.h"

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
    /* Otherwise the live keyboard branch may still target this RX page:
     * deliberately retain it until process termination. */
    if(write_log)
      write_log("DI8-RETRY: DISABLED: mouse patch failed; keyboard rollback attempted");
    return;
  }

  /* Both installed branches now target page: keep RX memory until exit. */
  InterlockedExchange(&installed,1);
  if(write_log)
    write_log("DI8-RETRY: active at Flip #120; exact x86 branch patch verified; original Acquire/retry reused, foreground only");
}

/* #176 opt-in observation, independent of the existing retry branch.
 * At 0x501C9B the original loop has already processed its batch. EBX
 * is still saved; [ESP+0x0C] = returned count, [ESP+0x10] = records.
 * Only Escape data is copied. The native latch is read AFTER processing.
 * No calls into DirectInput, no input writes, no per-poll file logging. */
#define ESC_BATCH_SITE 0x00501C9Bu
#define ESC_BATCH_LATCH 0x006B792Du
#define ESC_BATCH_CAP 64u
#define ESC_BATCH_LIMIT 512u
typedef struct EscapeObservation {
  unsigned serial,count;
  SpiderEscapeBatch edges;
  unsigned latch;
} EscapeObservation;
static int escape_requested=0,escape_installed=0;
static DWORD escape_thread=0;
static unsigned escape_poll=0,escape_used=0,escape_lost=0,escape_bad=0;
static unsigned escape_reported=0,escape_samples=0;
static long escape_previous_flip=0;
static int escape_last_physical=-1,escape_last_latch=-1;
static EscapeObservation escape_records[ESC_BATCH_CAP];

static void __cdecl escape_observe(const uint32_t *events,unsigned count) {
  SpiderEscapeBatch edges;
  EscapeObservation *r;
  if(!escape_installed||escape_reported>=ESC_BATCH_LIMIT||
     GetCurrentThreadId()!=escape_thread)return;
  ++escape_poll;
  if(count>16u){++escape_bad;return;}
  if(!SpiderEscapeBatch_Decode(events,count,&edges))return;
  if(escape_used>=ESC_BATCH_CAP){++escape_lost;return;}
  r=&escape_records[escape_used++];
  r->serial=escape_poll;r->count=count;r->edges=edges;
  r->latch=*(const volatile BYTE *)(uintptr_t)ESC_BATCH_LATCH;
}
static void install_escape_batch_trace(void) {
  static const BYTE native_loop[]={
    0x8b,0x54,0x24,0x0c,0x33,0xf6,0x85,0xd2,0x76,0x25,
    0x8d,0x4c,0x24,0x10,0x8b,0xf2,0xb3,0x80,0xb0,0xff,
    0x84,0x59,0x04,0x8b,0x39,0x74,0x08,
    0x88,0x87,0x2c,0x79,0x6b,0x00,0xeb,0x06,
    0x88,0x9f,0x2c,0x79,0x6b,0x00,0x83,0xc1,0x14,0x4a,0x75,0xe5
  };
  static const BYTE native_exit[]={0x5b,0x8b,0xc6,0x5f,0x5e,0x81,0xc4,0x44,0x01,0x00,0x00,0xc3};
  BYTE *site=(BYTE *)(uintptr_t)ESC_BATCH_SITE,*page,*p,*disp;
  BYTE patch[5]={0xe9,0,0,0,0};
  DWORD old,ignored,delta;
  if(!escape_requested || escape_installed)return;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=SPIDER_IMAGE_BASE ||
     memcmp((const void *)(uintptr_t)0x00501C6Cu,native_loop,sizeof(native_loop))||
     memcmp(site,native_exit,sizeof(native_exit))){
    if(write_log)write_log("ESC-BATCH: refused; original loop/exit signature mismatch");
    return;
  }
  page=(BYTE *)VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  if(!page){if(write_log)write_log("ESC-BATCH: allocation failed");return;}
  p=page;
  *p++=0x9c; /* PUSHFD */
  *p++=0x60; /* PUSHAD: saved context = 36 bytes, original ESP unchanged below */
  /* Original [ESP+0x0C] count is now [ESP+0x30].
   * Original [ESP+0x10] first record is now [ESP+0x34].
   * Compute both before pushing C arguments. */
  {const BYTE args[]={0x8b,0x44,0x24,0x30,0x8d,0x4c,0x24,0x34,0x50,0x51};
   memcpy(p,args,sizeof(args));p+=sizeof(args);}
  *p++=0xe8;disp=p;append32(&p,0u);
  rel32(disp,(const BYTE *)(uintptr_t)escape_observe);
  {const BYTE restore[]={0x83,0xc4,0x08,0x61,0x9d};
   memcpy(p,restore,sizeof(restore));p+=sizeof(restore);}
  /* Original 5 bytes: POP EBX; MOV EAX,ESI; POP EDI; POP ESI. */
  memcpy(p,native_exit,5);p+=5;
  emit_jmp(&p,site+5);
  if(!VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)){
    VirtualFree(page,0,MEM_RELEASE);if(write_log)write_log("ESC-BATCH: RX protection failed");return;
  }
  FlushInstructionCache(GetCurrentProcess(),page,(SIZE_T)(p-page));
  delta=(DWORD)((uintptr_t)page-(uintptr_t)(site+5));
  memcpy(patch+1,&delta,4);
  if(!VirtualProtect(site,sizeof(patch),PAGE_EXECUTE_READWRITE,&old)){
    VirtualFree(page,0,MEM_RELEASE);if(write_log)write_log("ESC-BATCH: CALL-site protection failed");return;
  }
  escape_thread=GetCurrentThreadId();
  escape_installed=1;
  memcpy(site,patch,sizeof(patch));
  FlushInstructionCache(GetCurrentProcess(),site,sizeof(patch));
  if(!VirtualProtect(site,sizeof(patch),old,&ignored) && write_log)
    write_log("ESC-BATCH: warning: native page protection restoration failed");
  if(write_log)write_log("ESC-BATCH: active at Flip #120; native buffered Escape after loop, no event or latch changes");
}
static void escape_trace_on_flip(long flip_index) {
  unsigned i;
  char line[256];
  int physical,latch;
  if(!escape_installed||GetCurrentThreadId()!=escape_thread)return;
  if(escape_reported>=ESC_BATCH_LIMIT)return;
  for(i=0;i<escape_used && escape_reported<ESC_BATCH_LIMIT;i++){
    const EscapeObservation *r=&escape_records[i];
    snprintf(line,sizeof(line),
      "ESC-BATCH: after-flip=%ld before-flip=%ld poll=%u count=%u down=%04X up=%04X latch=%02X down-overwritten=%d",
      escape_previous_flip,flip_index,r->serial,r->count,(unsigned)r->edges.down,
      (unsigned)r->edges.up,r->latch,SpiderEscapeBatch_DownOverwritten(r->edges));
    if(write_log)write_log(line);
    ++escape_reported;
  }
  if(escape_lost||escape_bad){
    snprintf(line,sizeof(line),"ESC-BATCH: omitted=%u invalid-count=%u",escape_lost,escape_bad);
    if(write_log)write_log(line);
  }
  escape_used=escape_lost=escape_bad=0;
  if(escape_reported>=ESC_BATCH_LIMIT){
    if(write_log)write_log("ESC-BATCH: report cap reached; further batches not observed");
    return;
  }
  /* OS high-bit and native latch are sampled only on Flip. A short
   * physical tap between observations is not proof of missing DI data. */
  physical=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0;
  latch=*(const volatile BYTE *)(uintptr_t)ESC_BATCH_LATCH;
  if(escape_samples<ESC_BATCH_LIMIT &&
     (physical!=escape_last_physical || latch!=escape_last_latch)){
    snprintf(line,sizeof(line),"ESC-BATCH-SAMPLE: flip=%ld poll=%u os-down=%d latch=%02X",
      flip_index,escape_poll,physical,latch);
    if(write_log)write_log(line);
    ++escape_samples;
    if(escape_samples==ESC_BATCH_LIMIT && write_log)
      write_log("ESC-BATCH-SAMPLE: report cap reached");
  }
  escape_last_physical=physical;escape_last_latch=latch;
  escape_previous_flip=flip_index;
}
void SpiderDI8Retry_RequestEscapeBatchTrace(int enable) {
  escape_requested=enable!=0;
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

unsigned SpiderDI8Retry_KeyboardRecoveryAttempts(void) {
  /* Read-only count of original DIERR_NOTACQUIRED keyboard paths that
   * entered the game's existing Acquire/retry. This counter increments
   * BEFORE Acquire, so it is a hint, NOT proof of successful Acquire. */
  LONG attempts=snapshot(&retries_keyboard);
  return attempts<0?0u:(unsigned)attempts;
}
void SpiderDI8Retry_OnFlip(long flip_index) {
  if(snapshot(&requested)!=1) return;
  /* The original game calls Flip on its render thread. Delayed install
   * avoids patching keyboard/mouse while startup devices are being created. */
  if(flip_index==120) try_install();
  if(flip_index==120) install_escape_batch_trace();
  escape_trace_on_flip(flip_index);
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
