#ifndef SPIDERMAN2K1FIX_ALT_LATCH_RECOVERY_H
#define SPIDERMAN2K1FIX_ALT_LATCH_RECOVERY_H
/* One-byte, experimental native Left Alt latch reconciliation.
 * Only the original SHA-verified retail EXE is supported. Never hooks COM,
 * sends Win32 input, calls DirectInput, modifies keyboard bindings, or
 * touches joystick/mouse/global key state. ON by default, explicit INI=0 disables.
 *
 * This is deliberately separate from SDL3 diagnostics and gameplay.
 * Its only possible write is the game's own DIK_LMENU latch at 0x006B7964:
 * 0x7F (stale held) -> 0x80 (the game's original released-edge encoding).
 * Called only on the game's existing Flip thread, after original input.
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "alt_latch_recovery_policy.h"

#define SPIDER_ALT_LATCH_ADDR ((uintptr_t)0x006B7964u)
static SpiderAltLatchRecoveryPolicy spider_alt_latch_policy;
static int spider_alt_latch_enabled=0;
static DWORD spider_alt_latch_return_tick=0;
static void (*spider_alt_latch_logger)(const char *)=NULL;

static int spider_alt_latch_preflight(void) {
  MEMORY_BASIC_INFORMATION page;
  DWORD protect;
  static const unsigned char key_reader[]={
    0x8a,0x98,0x2c,0x79,0x6b,0x00
  };
  static const unsigned char key_getdata[]={
    0xff,0x51,0x28,0x3d,0x1e,0x00,0x07,0x80
  };
  if(sizeof(void*)!=4u ||
     (uintptr_t)GetModuleHandleA(NULL)!=0x00400000u ||
     memcmp((const void *)(uintptr_t)0x00501C56u,
            key_reader,sizeof(key_reader))!=0 ||
     memcmp((const void *)(uintptr_t)0x00501BC3u,
            key_getdata,sizeof(key_getdata))!=0 ||
     VirtualQuery((const void *)SPIDER_ALT_LATCH_ADDR,
                  &page,sizeof(page))!=sizeof(page) ||
     page.State!=MEM_COMMIT ||
     (page.Protect&PAGE_GUARD)!=0 ||
     (page.Protect&PAGE_NOACCESS)!=0)return 0;
  protect=page.Protect&0xffu;
  return protect==PAGE_READWRITE||protect==PAGE_WRITECOPY||
         protect==PAGE_EXECUTE_READWRITE||protect==PAGE_EXECUTE_WRITECOPY;
}
static void SpiderAltLatch_Init(const char *ini,
                                void (*logger)(const char *)) {
  memset(&spider_alt_latch_policy,0,sizeof(spider_alt_latch_policy));
  spider_alt_latch_logger=logger;
  (void)ini; /* Permanent functionality; no INI switch or opt-out. */
  spider_alt_latch_enabled=1;
  if(!spider_alt_latch_preflight()) {
    spider_alt_latch_enabled=0;
    if(logger)logger("ALT-LATCH: REFUSED original executable/input opcode or writable latch preflight");
    return;
  }
  if(logger)logger("ALT-LATCH: ARMED built-in Left Alt-only recovery after confirmed focus pause");
}
/* Called ONLY when the existing pause gate positively confirms another
 * application kept foreground for >=150 ms. Bink and modal eligibility
 * transitions alone MUST NEVER call this. */
static void SpiderAltLatch_OnConfirmedExternalLoss(void) {
  if(!spider_alt_latch_enabled)return;
  spider_alt_latch_policy_loss(&spider_alt_latch_policy);
}
/* Called when the existing focus-pause wait has finished. A spurious wake
 * outside of foreground cannot arm a repair. */
static void SpiderAltLatch_OnPauseReturn(HWND hwnd) {
  if(!spider_alt_latch_enabled)return;
  spider_alt_latch_policy_return(&spider_alt_latch_policy,
                               hwnd&&GetForegroundWindow()==hwnd);
  if(spider_alt_latch_policy.pending) {
    spider_alt_latch_return_tick=GetTickCount();
    if(spider_alt_latch_logger)
      spider_alt_latch_logger("ALT-LATCH: return confirmed; monitoring original Left Alt and Win32 for max 5 seconds");
  }
}
static void SpiderAltLatch_OnFlip(HWND hwnd,int menu,int gameplay,
                                  int bink,int paused) {
  volatile uint8_t *const left=
      (volatile uint8_t *)SPIDER_ALT_LATCH_ADDR;
  volatile const uint8_t *const key_state=
      (volatile const uint8_t *)(uintptr_t)0x006B792Cu;
  unsigned eligible,os_modifiers_up,original_other_up;
  DWORD age;
  int result;
  if(!spider_alt_latch_enabled||!spider_alt_latch_policy.pending)return;
  eligible=hwnd&&GetForegroundWindow()==hwnd&&(menu||gameplay)&&
           !bink&&!paused;
  age=(DWORD)(GetTickCount()-spider_alt_latch_return_tick);
  /* Do not inspect original keyboard memory outside eligible foreground.
   * Pure policy still enforces expiry during non-gameplay Flips. */
  if(!eligible) {
    spider_alt_latch_policy_step(&spider_alt_latch_policy,0,0u,0u,0u,age);
    return;
  }
  /* A genuine new Alt press (or Tab/AltGr) MUST NOT be neutralized.
   * Compare independent async Windows key state, never inject a key event.
   * The original DI states for other modifiers must also be neutral. */
  os_modifiers_up=!(GetAsyncKeyState(VK_LMENU)&0x8000) &&
                  !(GetAsyncKeyState(VK_MENU)&0x8000) &&
                  !(GetAsyncKeyState(VK_RMENU)&0x8000) &&
                  !(GetAsyncKeyState(VK_TAB)&0x8000);
  original_other_up=!(key_state[0xb8]&0x7fu) &&
                    !(key_state[0x0f]&0x7fu);
  result=spider_alt_latch_policy_step(&spider_alt_latch_policy,1u,
            *left,os_modifiers_up,original_other_up,(unsigned)age);
  if(result==1) {
    /* Policy has consumed its one opportunity; a last live check rejects
     * a key press arriving between the earlier sample and this write. */
    if(GetForegroundWindow()==hwnd &&
       /* Re-read ALL four independent OS modifiers; do not trust
        * the earlier cached booleans if Tab/AltGr changes mid-frame. */
       !(GetAsyncKeyState(VK_LMENU)&0x8000) &&
       !(GetAsyncKeyState(VK_MENU)&0x8000) &&
       !(GetAsyncKeyState(VK_RMENU)&0x8000) &&
       !(GetAsyncKeyState(VK_TAB)&0x8000) &&
       /* Re-read native Right Alt and Tab, too: both were previously
        * sampled but could change after the policy decision. */
       !(key_state[0xb8]&0x7fu) &&
       !(key_state[0x0f]&0x7fu) &&
       *left==0x7fu) {
      *left=0x80u;  /* original game release EDGE, not global zeroing */
      if(spider_alt_latch_logger)
        spider_alt_latch_logger("ALT-LATCH: REPAIRED original Left Alt 0x7F->0x80; 6 stable foreground frames; OS modifiers up");
    } else if(spider_alt_latch_logger)
      spider_alt_latch_logger("ALT-LATCH: SKIPPED final recheck changed; original keyboard untouched");
  } else if(result==2 && spider_alt_latch_logger)
    spider_alt_latch_logger("ALT-LATCH: SKIPPED original Left Alt already changed/cleared naturally");
  else if(result==3 && spider_alt_latch_logger)
    spider_alt_latch_logger("ALT-LATCH: EXPIRED after 5 seconds; original keyboard untouched");
}
#endif
