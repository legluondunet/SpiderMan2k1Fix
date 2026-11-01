/* Spider-Man 2001 native joystick unplug safety experiment.
 *
 * The unmodified game calls exit(HRESULT) if its native joystick's
 * IDirectInputDevice8::Acquire returns a negative error (other than
 * DIERR_OTHERAPPHASPRIO). Wine returns DIERR_UNPLUGGED (0x80040209)
 * after a physical DS4 detach, so the entire game terminates.
 *
 * Opt-in and fail-closed: [Input] ExperimentalNativeJoystickUnplugGuard=1.
 * Only the exact SHA-verified 2001 x86 EXE and ALL known native callsite
 * signatures can be patched. The source EXE on disk stays untouched.
 *
 * This patches the six-byte CMP ESI,0x80070005 at 0x00501EA6 to a
 * five-byte JMP + NOP. The RX stub checks ESI==DIERR_UNPLUGGED;
 * on match it clears only the original 32-byte game-native joystick
 * button-state array [0x006B7A34..0x006B7A53], increments a diagnostic
 * counter and branches to the game's proven balanced nonfatal return
 * at 0x00501EBB (which sets EAX=0). On any other HRESULT it replays
 * exactly the original CMP and branches to 0x00501EAC.
 *
 * No fabricated successful Acquire, no hooks on game-owned COM objects,
 * no global ExitProcess interception, no native controller rebind yet.
 * Movement axes outside that 32-byte buffer may require separate
 * investigation, and a reconnected controller may remain unusable.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "native_joystick_unplug.h"
#include "native_joystick_unplug_policy.h"

#define SPIDER_EXE_BASE 0x00400000u
#define SPIDER_SITE_CMP 0x00501EA6u
#define SPIDER_ORIGINAL_BRANCH 0x00501EACu
#define SPIDER_GRACEFUL_RETURN 0x00501EBBu
#define SPIDER_GAME_BUTTON_STATES 0x006B7A34u
#define SPIDER_MAX_STUB 128u

typedef struct SpiderSiteSignature {
  uintptr_t addr;
  unsigned char bytes[6];
  unsigned length;
} SpiderSiteSignature;

static const SpiderSiteSignature signatures[] = {
  {0x00501E79u,{0xFF,0x51,0x64},3},      /* original Poll COM call */
  {0x00501E7Cu,{0x3D,0x1E,0x00,0x07,0x80},5},
  {0x00501E83u,{0x3D,0x0C,0x00,0x07,0x80},5},
  {0x00501E9Fu,{0x8B,0x10,0xFF,0x52,0x1C},5},
  {SPIDER_SITE_CMP,{0x81,0xFE,0x05,0x00,0x07,0x80},6},
  {SPIDER_ORIGINAL_BRANCH,{0x75,0x18},2},
  {SPIDER_GRACEFUL_RETURN,{0x33,0xC0,0x5F},3},
  {0x00501ECAu,{0x68,0xEA,0x02,0x00,0x00},5},
  {0x00501EDDu,{0x85,0xF6,0x7D,0x0B},4},
  {0x00501EE7u,{0xE8,0x15,0x82,0x02,0x00},5},
};
static const unsigned char original_cmp[6]={0x81,0xFE,0x05,0x00,0x07,0x80};
static volatile LONG requested=0,installed=0,detached_events=0;
static LONG reported_events=0;
static void (*write_log)(const char *)=NULL;
static BYTE *retained_stub=NULL;

static LONG snapshot(volatile LONG *p) {
  return InterlockedCompareExchange(p,0,0);
}
static void append32(BYTE **p,uint32_t value) {
  memcpy(*p,&value,4);
  *p+=4;
}
static void rel32(BYTE *field,uintptr_t target) {
  uint32_t disp=(uint32_t)target - ((uint32_t)(uintptr_t)field+4u);
  memcpy(field,&disp,4);
}
static void jmp32(BYTE **p,uintptr_t target) {
  BYTE *field;
  *(*p)++=0xE9;
  field=*p;
  append32(p,0u);
  rel32(field,target);
}
static BOOL exact_supported_site(void) {
  unsigned i;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=SPIDER_EXE_BASE)
    return FALSE;
  for(i=0;i<sizeof(signatures)/sizeof(signatures[0]);i++) {
    if(memcmp((const void *)signatures[i].addr,
              signatures[i].bytes,signatures[i].length)!=0)
      return FALSE;
  }
  return TRUE;
}
static BOOL make_stub(BYTE *page,SIZE_T available) {
  BYTE *p=page,*je_field,*unplug_path;
  /* Fixed upper bound, checked before constructing the stub. */
  if(available<SPIDER_MAX_STUB) return FALSE;
  /* CMP ESI, DIERR_UNPLUGGED; JE detached. */
  *p++=0x81;*p++=0xFE;
  append32(&p,SPIDER_DIERR_UNPLUGGED);
  *p++=0x0F;*p++=0x84;
  je_field=p;append32(&p,0u);

  /* Everything except DIERR_UNPLUGGED follows original x86 logic. */
  memcpy(p,original_cmp,sizeof(original_cmp));p+=sizeof(original_cmp);
  jmp32(&p,SPIDER_ORIGINAL_BRANCH);

  unplug_path=p;
  rel32(je_field,(uintptr_t)unplug_path);
  /* Save EDI/ECX; keep ESI (returned HRESULT) and ESP untouched.
   * Clear only 8 DWORDs already proven to be the 32 native buttons.
   * No C calls from the patched game's stack. */
  *p++=0x57;*p++=0x51;                    /* push edi; push ecx */
  *p++=0x31;*p++=0xC0;                    /* xor eax,eax */
  *p++=0xBF;append32(&p,SPIDER_GAME_BUTTON_STATES);
  *p++=0xB9;append32(&p,SPIDER_NATIVE_JOYSTICK_BUTTON_BYTES/4u);
  *p++=0xFC;                              /* cld */
  *p++=0xF3;*p++=0xAB;                    /* rep stosd */
  *p++=0x59;*p++=0x5F;                    /* pop ecx; pop edi */
  *p++=0xF0;*p++=0xFF;*p++=0x05;          /* lock inc [counter] */
  append32(&p,(uint32_t)(uintptr_t)&detached_events);
  jmp32(&p,SPIDER_GRACEFUL_RETURN);
  return (SIZE_T)(p-page)<=available && (SIZE_T)(p-page)<=SPIDER_MAX_STUB;
}
static void install_guard(void) {
  BYTE *page;
  DWORD old,unused;
  BYTE patch[6]={0xE9,0,0,0,0x90};
  uint32_t displacement;
  if(snapshot(&installed) || retained_stub)return;
  if(!exact_supported_site()) {
    if(write_log)write_log("NATIVE-UNPLUG: DISABLED, unsupported x86 layout/signature; game unchanged");
    return;
  }
  page=(BYTE *)VirtualAlloc(NULL,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
  if(!page) {
    if(write_log)write_log("NATIVE-UNPLUG: DISABLED, executable stub allocation failed");
    return;
  }
  if(!make_stub(page,4096) ||
     !VirtualProtect(page,4096,PAGE_EXECUTE_READ,&old)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(write_log)write_log("NATIVE-UNPLUG: DISABLED, stub generation/RX protection failed");
    return;
  }
  FlushInstructionCache(GetCurrentProcess(),page,SPIDER_MAX_STUB);

  displacement=(uint32_t)(uintptr_t)page-(SPIDER_SITE_CMP+5u);
  memcpy(patch+1,&displacement,4);
  if(memcmp((void *)(uintptr_t)SPIDER_SITE_CMP,original_cmp,6)!=0 ||
     !VirtualProtect((void *)(uintptr_t)SPIDER_SITE_CMP,6,PAGE_EXECUTE_READWRITE,&old)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(write_log)write_log("NATIVE-UNPLUG: DISABLED, original site changed/protection failed");
    return;
  }
  memcpy((void *)(uintptr_t)SPIDER_SITE_CMP,patch,6);
  FlushInstructionCache(GetCurrentProcess(),(void *)(uintptr_t)SPIDER_SITE_CMP,6);
  if(!VirtualProtect((void *)(uintptr_t)SPIDER_SITE_CMP,6,old,&unused) && write_log)
    write_log("NATIVE-UNPLUG: WARNING, unable to restore original page protection");
  /* The live jump must never outlive its target allocation. */
  retained_stub=page;
  InterlockedExchange(&installed,1);
  if(write_log)
    write_log("NATIVE-UNPLUG: ACTIVE at Flip #120; strict original x86 signatures; only DIERR_UNPLUGGED returns nonfatally; native rebind NOT implemented");
}
long SpiderNativeUnplug_Events(void) { return snapshot(&detached_events); }
int SpiderNativeUnplug_Installed(void) { return snapshot(&installed)==1; }
void SpiderNativeUnplug_Init(const char *ini,void (*logger)(const char *)) {
  write_log=logger;
  InterlockedExchange(&requested,
    (ini && GetPrivateProfileIntA("Input",
      "ExperimentalNativeJoystickUnplugGuard",0,ini)==1)?1:0);
  if(write_log)write_log(snapshot(&requested)?
    "NATIVE-UNPLUG: experimental guard requested, will verify/install at Flip #120":
    "NATIVE-UNPLUG: guard OFF by default, original native joystick error handling preserved");
}
void SpiderNativeUnplug_OnFlip(long flip_index) {
  if(!snapshot(&requested))return;
  if(flip_index==120)install_guard();
  if(!snapshot(&installed) || (flip_index%120)!=0)return;
  {
    LONG count=snapshot(&detached_events);
    if(count!=reported_events) {
      char msg[180];
      snprintf(msg,sizeof(msg),
        "NATIVE-UNPLUG: suppressed fatal native DIERR_UNPLUGGED=%ld; cleared 32 button states; native device still stale",
        (long)count);
      if(write_log)write_log(msg);
      reported_events=count;
    }
  }
}
