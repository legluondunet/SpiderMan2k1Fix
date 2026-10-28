/*
 * Optional game-owned mouse/cursor disable during Sony DS4 PS1 play.
 *
 * Original PCShell.cpp/PCInput.cpp use gRenderTest & 0x10 to:
 *   (1) skip drawing the IN-GAME cursor sprite (0x50C470);
 *   (2) suppress original mouse movement/button polling (0x50C4CF);
 *   (3) return false from original menu mouse hit-testing (0x50C5F0).
 * This addresses *selection precedence*, not merely pointer visibility.
 *
 * No Win32 ShowCursor, SetCursorPos, ClipCursor, SetWindowLong or DirectInput
 * hooks; the OS desktop cursor and existing Alt-Tab recovery are untouched.
 * The only live change is game-owned gRenderTest bit 0x10. Restore only the
 * bit we installed, on focus loss or an independent pad read failure.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include "ps1_mouse_mode.h"

#define SPIDER_GAME_RENDERTEST_VA 0x02E0988Cu
#define SPIDER_GAME_MOUSE_DISABLED 0x10u

static volatile LONG *render_flags=NULL;
static void (*mouse_logger)(const char *)=NULL;
static int armed=0,owns_bit=0,disabled=0;
static void log_mouse(const char *message){
  if(mouse_logger)mouse_logger(message);
}
typedef struct {
  uintptr_t va;
  unsigned char code[11];
  unsigned length;
} OriginalMouseGate;

static const OriginalMouseGate mouse_gates[]={
  /* PCShell::DrawMouseCursor: TEST byte ptr [gRenderTest], 0x10; JNE */
  {0x0050C470u,{0xF6u,0x05u,0x8Cu,0x98u,0xE0u,0x02u,0x10u,0x75u},8u},
  /* PCShell::UpdateMouse: original game's disable-mouse test */
  {0x0050C4CFu,{0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0xA8u,0x10u,0x0Fu,0x85u},9u},
  /* PCShell::IsMouseOver: return zero if mouse-disable bit is on */
  {0x0050C5F0u,{0xA0u,0x8Cu,0x98u,0xE0u,0x02u,0x83u,0xECu,0x18u,0xA8u,0x10u,0x74u},11u}
};
static int writable_original_dword(const void *address){
  MEMORY_BASIC_INFORMATION mbi;
  uintptr_t a=(uintptr_t)address,b;
  DWORD protection;
  if((a&3u) || VirtualQuery(address,&mbi,sizeof(mbi))!=sizeof(mbi) ||
     mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD)))
    return 0;
  b=(uintptr_t)mbi.BaseAddress;
  protection=mbi.Protect&0xffu;
  return a>=b && a-b<mbi.RegionSize &&
    mbi.RegionSize-(a-b)>=sizeof(LONG) &&
    (protection==PAGE_READWRITE || protection==PAGE_EXECUTE_READWRITE ||
     protection==PAGE_WRITECOPY || protection==PAGE_EXECUTE_WRITECOPY);
}
int SpiderPS1Mouse_Arm(void (*logger)(const char *)){
  BYTE *base=(BYTE *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  unsigned i;
  mouse_logger=logger;
  if(armed)return 1;
  if(!base || (uintptr_t)base!=0x00400000u)return 0;
  dos=(IMAGE_DOS_HEADER *)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE ||
     dos->e_lfanew<0x40 || dos->e_lfanew>0x1000){
    log_mouse("GAMEPAD-MOUSE: REFUSED original game DOS header mismatch");
    return 0;
  }
  nt=(IMAGE_NT_HEADERS32 *)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x00400000u ||
     nt->OptionalHeader.SizeOfImage<=
         SPIDER_GAME_RENDERTEST_VA-0x00400000u+sizeof(LONG) ||
     nt->OptionalHeader.SizeOfImage>0x10000000u){
    log_mouse("GAMEPAD-MOUSE: REFUSED unsupported original game PE image");
    return 0;
  }
  /* Exact x86 gates are all checked before ANY writable data is changed.
   * They prove the same global drives drawing, polling and hit testing. */
  for(i=0;i<sizeof(mouse_gates)/sizeof(mouse_gates[0]);i++){
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t at=mouse_gates[i].va,start;
    DWORD prot;
    if(VirtualQuery((LPCVOID)at,&mbi,sizeof(mbi))!=sizeof(mbi) ||
       mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD))){
      log_mouse("GAMEPAD-MOUSE: REFUSED original mouse instruction unavailable");
      return 0;
    }
    start=(uintptr_t)mbi.BaseAddress;
    prot=mbi.Protect&0xffu;
    if(at<start || at-start>=mbi.RegionSize ||
       mbi.RegionSize-(at-start)<mouse_gates[i].length ||
       !(prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
         prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY) ||
       memcmp((const void *)at,mouse_gates[i].code,mouse_gates[i].length)){
      log_mouse("GAMEPAD-MOUSE: REFUSED original mouse logic signature mismatch");
      return 0;
    }
  }
  render_flags=(volatile LONG *)(uintptr_t)SPIDER_GAME_RENDERTEST_VA;
  if(!writable_original_dword((const void *)render_flags)){
    render_flags=NULL;
    log_mouse("GAMEPAD-MOUSE: REFUSED original game mouse flag not writable");
    return 0;
  }
  armed=1;
  log_mouse("GAMEPAD-MOUSE: ARMED verified native mouse cursor/draw, input poll and menu hit-test gates; desktop mouse untouched");
  return 1;
}
void SpiderPS1Mouse_SetPadActive(int active){
  LONG before,desired;
  unsigned attempts;
  if(!armed || !render_flags)return;
  if(active) {
    for(attempts=0;attempts<8u;attempts++){
      before=InterlockedCompareExchange(render_flags,0,0);
      if(((unsigned long)before&SPIDER_GAME_MOUSE_DISABLED)!=0u){
        if(!disabled) {
          /* If another owner disabled it first, do not claim its bit. */
          disabled=1;
          if(!owns_bit)
            log_mouse("GAMEPAD-MOUSE: native mouse already disabled by game; leaving original flag ownership untouched");
        }
        return;
      }
      desired=(LONG)((unsigned long)before|SPIDER_GAME_MOUSE_DISABLED);
      if(InterlockedCompareExchange(render_flags,desired,before)==before){
        owns_bit=1;
        disabled=1;
        log_mouse("GAMEPAD-MOUSE: GAME MOUSE OFF (DS4 active); original native sprite/poll/hit testing disabled, desktop untouched");
        return;
      }
    }
  }else{
    if(!owns_bit){
      disabled=0;
      return;
    }
    for(attempts=0;attempts<8u;attempts++){
      before=InterlockedCompareExchange(render_flags,0,0);
      if(!((unsigned long)before&SPIDER_GAME_MOUSE_DISABLED)){
        owns_bit=0;disabled=0;
        return;
      }
      desired=(LONG)((unsigned long)before&~SPIDER_GAME_MOUSE_DISABLED);
      if(InterlockedCompareExchange(render_flags,desired,before)==before){
        owns_bit=0;disabled=0;
        log_mouse("GAMEPAD-MOUSE: GAME MOUSE RESTORED (focus lost/pad unavailable); Win32 desktop pointer not modified");
        return;
      }
    }
  }
}
