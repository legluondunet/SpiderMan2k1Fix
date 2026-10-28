/*
 * Experimental read-only native Joystick Configuration screen.
 * This module is deliberately separate from gamepad_ui_profile.c:
 * the DS4/DirectInput bridge never edits original game code or vtables.
 *
 * The installer is shared by two exclusively identified profiles:
 * Sony DS4+PS1 preview (manually opt-in) or Xbox360-compatible
 * (automatic after the original config bootstrap). Both use the same
 * immutable, SHA-pinned joystick-only capture lock. Never installs
 * twice or changes keyboard editing. No on-disk EXE, gameplay
 * mapping, or user INI preferences are modified by this module.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "ps1_joystick_readonly_menu.h"
#include "sdl3_controller_setup.h"

static void (*ps1_menu_logger)(const char *)=NULL;
/* The native machine-code patch is immutable during this process.
 * 1 = Sony/PS1 labels, 2 = Xbox A/B with numeric fallback. */
static unsigned readonly_installed_mode=0;
static unsigned readonly_label_mode=0;
static unsigned readonly_xbox_rt_alias=0;
void SpiderXbox360_SetRTAlias(int enabled) {
  readonly_xbox_rt_alias=enabled?1u:0u;
}
static void report(const char *msg) {
  if(ps1_menu_logger)ps1_menu_logger(msg);
}
/* Original callsite 0x50D4A9 invokes the zero-argument void native
 * PCINPUT_RestoreDefaultControllerSettings(). Replacing CALL with five
 * NOPs made it impossible to determine whether the menu actually took
 * this path. Use a normal cdecl void no-op instead, preserving the
 * original caller's CALL/RET control flow. No native mappings are
 * restored and the on-disk joystick preferences remain untouched.
 * Rate-limit logging because this may be reached repeatedly if the
 * original menu is stuck awaiting a selection release. */
static void __cdecl spider_joystick_restore_denied(void) {
  static LONG blocked=0;
  /* This exact callsite is inside original Joystick Configuration.
   * In the experimental user-wizard mode it becomes a safe menu command,
   * NEVER the original restore-defaults action. Native seven-action
   * assignments stay locked until SDL3 gameplay bridge is validated. */
  if(SpiderSDL3ControllerSetup_Enabled() &&
     *(const volatile DWORD *)(uintptr_t)0x00AC129Cu!=0u) {
    SpiderSDL3ControllerSetup_Open();
    return;
  }
  LONG n=InterlockedIncrement(&blocked);
  if(n<=8 || n%64==0) {
    char line[195];
    snprintf(line,sizeof(line),
      "GAMEPAD-READONLY: RESTORE-DEFAULT intercepted count=%ld at original 0x50D4A9; no controller bindings changed [CALL/RET preserved]",
      (long)n);
    report(line);
  }
}

/* Native initActionMaps (0x50D0F0) appends "restore default settings"
 * after 14 action rows. Replacing the native Restore handler with a no-op
 * leaves that selectable row and still enters the game's refresh path.
 * Remove the ROW instead, only for Joystick; Keyboard stays untouched.
 *
 * x86 ABI: original CMenu::AddEntry is thiscall (this in ECX and one
 * stack argument, callee pops 4 bytes). A __fastcall function with one
 * dummy EDX argument matches that ABI, including RET 4. */
typedef void (__thiscall *SpiderCMenuAddEntry)(void *,const char *);
static void __fastcall spider_joystick_restore_row_addentry(
    void *native_menu,void *unused_edx,const char *label) {
  (void)unused_edx;
  if(*(const volatile DWORD *)(uintptr_t)0x00AC129Cu!=0u) {
    static LONG suppressed=0;
    if(SpiderSDL3ControllerSetup_Enabled()) {
      /* Keep left/right CMenu rows aligned; no altered action indexes.
       * The original RestoreDefaults call is already replaced above. */
      const void *left=*(void *const volatile *)(uintptr_t)0x00AC11B4u;
      const char *text=native_menu==left?
                         "SDL3 TEST (PREVIEW)":" ";
      ((SpiderCMenuAddEntry)(uintptr_t)0x0043FFF0u)(native_menu,text);
      if(InterlockedIncrement(&suppressed)==1)
        report("SDL3-SETUP: native Joystick preview row replaces Restore Default (paired columns; keyboard menu unchanged)");
    } else {
      if(InterlockedIncrement(&suppressed)==1)
        report("GAMEPAD-READONLY: Restore Default ROW OMITTED in Joystick (and matching placeholder); 14 action entries kept, Keyboard restore preserved");
    }
    return;
  }
  ((SpiderCMenuAddEntry)(uintptr_t)0x0043FFF0u)(native_menu,label);
}

/* Original initActionMaps assembly validated from 5296368.bin:
 *   0x50D2BB mov ecx,[0x54B7B4]   (pointer to restore string)
 *   0x50D2C1 push ecx
 *   0x50D2C2 mov ecx,[0xAC11B4]   (left menu)
 *   0x50D2C8 call 0x43FFF0       (restore row)
 *   0x50D2CD mov ecx,[0xAC12A0]   (right menu)
 *   0x50D2D3 push 0x56EB54       (placeholder string)
 *   0x50D2D8 call 0x43FFF0       (matching placeholder row)
 * Both sites have exact SHA-pinned 5-byte instruction signatures in
 * ps1_joystick_readonly_menu.h and run through all-site preflight. */
/* Called instead of just the one original joystick-only sprintf at
 * 0x50D28C. The native CMenu::AddEntry that follows keeps ownership of
 * its own text and drawing, so we do not need a custom renderer. */
static int __cdecl spider_ps1_joystick_name(char *destination,
                                            const char *original_format,
                                            unsigned button) {
  const char *name=readonly_label_mode==2u?
    spider_xbox360_native_button_label(button):
    spider_ps1_native_button_label(button);
  (void)original_format;
  if(!destination)return 0;
  /* The original game binds Swing to physical BUTTON9 (R3), not Xbox
   * RT. When our independent RT-axis -> keyboard Swing experiment is
   * verified and active, list the shortcut alongside the real binding.
   * Never silently claim that the game's native BUTTON9 became RT. */
  if(readonly_label_mode==2u && button==9u && readonly_xbox_rt_alias)
    return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,
                    "R3 + RT");
  if(name)return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,
                          "%s",name);
  if(button==0x4000u)return snprintf(destination,
    SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,"NONE");
  return snprintf(destination,SPIDER_PS1_JOYSTICK_LABEL_CAPACITY,
                  "button %u",button);
}

/* Native processControllerScreen at 0x50D2F0 has two distinct
 * joystick-assignment branches and an original per-frame cleanup at
 * 0x50D852. Jumping to the original cleanup at the two *post-poll*
 * assignment-entry sites makes controller key presses inert, but does
 * not change the menu's existing normal navigation and Escape/Back.
 *
 * The verified restore-defaults site is replaced with a cdecl no-op
 * CALL (retaining the native CALL/RET shape and logging activation);
 * the original setter is NOP'd as defense in depth. The 7-entry virtual profile and
 * persistent user mapping are independent of these menu-only edits.
 * All sites are verified BEFORE any code memory is written. */
static int ps1_patch_readonly_site(BYTE *address,const BYTE *replacement,
                                   unsigned n,BYTE *saved) {
  DWORD old,ignored;
  if(!address || !replacement || n<5u || n>6u)return 0;
  if(!VirtualProtect(address,n,PAGE_EXECUTE_READWRITE,&old))return 0;
  memcpy(saved,address,n);
  memcpy(address,replacement,n);
  FlushInstructionCache(GetCurrentProcess(),address,n);
  if(!VirtualProtect(address,n,old,&ignored)){
    /* Even a protection-restore failure must not leave an unknown
     * partial joystick menu patch installed. */
    memcpy(address,saved,n);
    FlushInstructionCache(GetCurrentProcess(),address,n);
    VirtualProtect(address,n,old,&ignored);
    return 0;
  }
  return 1;
}
static int spider_install_readonly_joystick(unsigned mode,
                                           void (*logger)(const char *)) {
  ps1_menu_logger=logger;
  if(mode!=1u && mode!=2u)return 0;
  if(readonly_installed_mode) {
    /* Sony and Xbox share the SAME validated eight native patch sites;
     * only the on-screen physical button names differ. A pad swap may
     * change the display profile without re-patching machine code or
     * modifying the current game's seven action bindings. */
    if(readonly_label_mode!=mode) {
      readonly_label_mode=mode;
      report(mode==1u?
        "GAMEPAD-READONLY: live controller labels changed to DS4 (native action mapping unchanged)":
        "GAMEPAD-READONLY: live controller labels changed to Xbox360 (native action mapping unchanged)");
    }
    return 1;
  }
  static const BYTE nop5[5]={0x90u,0x90u,0x90u,0x90u,0x90u};
  SpiderPS1JoystickReadOnlySite live_sites[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT];
  BYTE replacements[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT][6]={{0}};
  BYTE backups[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT][6]={{0}};
  BYTE *base=(BYTE *)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  unsigned i,installed=0;
  BYTE *capture_thunk=NULL;
  char line[220];
  if(!base || (uintptr_t)base!=0x00400000u)return 0;
  dos=(IMAGE_DOS_HEADER *)(void *)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE ||
     dos->e_lfanew<0x40 || dos->e_lfanew>0x1000)return 0;
  nt=(IMAGE_NT_HEADERS32 *)(void *)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x00400000u ||
     nt->OptionalHeader.SizeOfImage<0x00110000u ||
     nt->OptionalHeader.SizeOfImage>0x10000000u)return 0;
  memcpy(live_sites,spider_ps1_readonly_sites,sizeof(live_sites));

  /* Verify every exact original machine instruction and writable text
   * window, including the original input branch/callsite signatures.
   * This strict preflight prevents patches on unknown binaries. */
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&live_sites[i];
    const BYTE *location=(const BYTE *)(uintptr_t)p->virtual_address;
    MEMORY_BASIC_INFORMATION mbi;
    uintptr_t addr=(uintptr_t)location,region;
    DWORD prot;
    if(p->virtual_address<0x00400000u ||
       p->virtual_address-0x00400000u>nt->OptionalHeader.SizeOfImage-p->size ||
       VirtualQuery(location,&mbi,sizeof(mbi))!=sizeof(mbi) ||
       mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD))) {
      report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED invalid native instruction memory");
      return 0;
    }
    prot=mbi.Protect&0xffu;
    region=(uintptr_t)mbi.BaseAddress;
    if(addr<region || addr-region>=mbi.RegionSize ||
       mbi.RegionSize-(addr-region)<p->size ||
       !(prot==PAGE_EXECUTE || prot==PAGE_EXECUTE_READ ||
         prot==PAGE_EXECUTE_READWRITE || prot==PAGE_EXECUTE_WRITECOPY) ||
       memcmp(location,p->original,p->size)!=0) {
      snprintf(line,sizeof(line),
        "GAMEPAD-PS1: JOYSTICK-READONLY REFUSED unexpected original bytes/protection at VA=0x%08lX",
        (unsigned long)p->virtual_address);
      report(line);
      return 0;
    }
  }

  /* The original controller screen has a pre-capture transition
   * at 0x50D3FA (before its PRESS-A-BUTTON prompt). Merely neutralizing
   * the mapping setter still allows that prompt to appear.
   *
   * This tiny original-x86 thunk checks native gActionMapRelated at
   * 0xAC129C. A nonzero value means Joystick Configuration: jump to
   * 0x50D872 (the ORIGINAL function cleanup/return). When zero
   * (Keyboard Configuration), replay the two overwritten original
   * PUSH instructions and jump back to 0x50D400. We never change
   * keyboard remapping, existing CMenu navigation or exit ownership.
   *
   * Assembly: push eax; mov eax,[0xAC129C]; test eax,eax; pop eax;
   *           jnz game_end; push ebp; push 0x2000; jmp game_resume.
   * The binary is static except the two verified rel32 displacements.
   */
  {
    static const BYTE code[]={
      0x50u,0xA1u,0x9Cu,0x12u,0xACu,0x00u,
      0x85u,0xC0u,0x58u,
      0x0Fu,0x85u,0u,0u,0u,0u,
      0x55u,0x68u,0x00u,0x20u,0x00u,0x00u,
      0xE9u,0u,0u,0u,0u};
    DWORD old,unused;
    uint32_t joystick_end,keyboard_continue;
    capture_thunk=(BYTE *)VirtualAlloc(NULL,sizeof(code),
                        MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!capture_thunk){
      report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED could not allocate native capture gate");
      return 0;
    }
    memcpy(capture_thunk,code,sizeof(code));
    joystick_end=0x0050D872u-
       ((uint32_t)(uintptr_t)capture_thunk+15u);
    keyboard_continue=0x0050D400u-
       ((uint32_t)(uintptr_t)capture_thunk+sizeof(code));
    memcpy(capture_thunk+11,&joystick_end,sizeof(joystick_end));
    memcpy(capture_thunk+22,&keyboard_continue,sizeof(keyboard_continue));
    if(!VirtualProtect(capture_thunk,sizeof(code),PAGE_EXECUTE_READ,&old)){
      VirtualFree(capture_thunk,0,MEM_RELEASE);
      report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED cannot protect capture gate executable memory");
      return 0;
    }
    FlushInstructionCache(GetCurrentProcess(),capture_thunk,sizeof(code));
    (void)unused;
  }
  replacements[1][0]=0xE8u; /* original CALL/RET; safe no-op logger */
  {
    uint32_t rel=(uint32_t)(uintptr_t)&spider_joystick_restore_denied-
       (spider_ps1_readonly_sites[1].virtual_address+5u);
    memcpy(&replacements[1][1],&rel,sizeof(rel));
  }
  memcpy(replacements[4],nop5,5u); /* native controller action setter */
  replacements[0][0]=0xE8u; /* CALL joystick-only name formatter */
  {
    uint32_t rel=(uint32_t)(uintptr_t)&spider_ps1_joystick_name-
                 (spider_ps1_readonly_sites[0].virtual_address+5u);
    memcpy(&replacements[0][1],&rel,sizeof(rel));
  }
  /* CRITICAL KEYBOARD REGRESSION FIX: original processControllerScreen
   * 0x50D51C is inside the KEYBOARD-only branch (the native test at
   * 0x50D4D7 jumps to JOYSTICK branch 0x50D683). Our earlier unconditional
   * JMP at 0x50D51C prevented keyboard capture from completing.
   * Keep this original instruction UNCHANGED (verified in the 8-site
   * preflight), while the guarded 0x50D3FA capture gate still blocks
   * Joystick and permits Keyboard's normal PRESS A KEY process. */
  memcpy(replacements[2],live_sites[2].original,live_sites[2].size);
  {
    const SpiderPS1JoystickReadOnlySite *p=&live_sites[3];
    uint32_t rel=0x0050D852u-(p->virtual_address+5u);
    replacements[3][0]=0xE9u; /* joystick-only mapping branch */
    memcpy(&replacements[3][1],&rel,sizeof(rel));
    if(p->size==6u)replacements[3][5]=0x90u;
  }
  {
    /* The last site replaces the entire six-byte capture-mode entry
     * with a jump to our checked mode-specific thunk. */
    const SpiderPS1JoystickReadOnlySite *p=&live_sites[5u];
    uint32_t rel=(uint32_t)(uintptr_t)capture_thunk-
      (p->virtual_address+5u);
    BYTE *replacement=replacements[5u];
    replacement[0]=0xE9u;
    memcpy(replacement+1,&rel,sizeof(rel));
    replacement[5]=0x90u;
  }
  /* The game adds TWO final CMenu rows, one Restore Default entry
   * and a corresponding empty value-column placeholder. Suppress both
   * when Joystick is active so indices/counts remain synchronized. */
  for(i=SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT;
      i<=SPIDER_PS1_JOYSTICK_EMPTY_ROW_SLOT;i++) {
    const SpiderPS1JoystickReadOnlySite *p=&live_sites[i];
    uint32_t rel=(uint32_t)(uintptr_t)&spider_joystick_restore_row_addentry-
      (p->virtual_address+5u);
    replacements[i][0]=0xE8u;
    memcpy(&replacements[i][1],&rel,sizeof(rel));
  }
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&live_sites[i];
    BYTE *location=(BYTE *)(uintptr_t)p->virtual_address;
    if(!ps1_patch_readonly_site(location,replacements[i],p->size,
                               backups[i]))break;
    installed++;
  }
  if(installed!=SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT){
    while(installed>0u){
      const SpiderPS1JoystickReadOnlySite *p=&live_sites[--installed];
      BYTE *location=(BYTE *)(uintptr_t)p->virtual_address;
      DWORD old,ignored;
      if(VirtualProtect(location,p->size,PAGE_EXECUTE_READWRITE,&old)){
        memcpy(location,backups[installed],p->size);
        FlushInstructionCache(GetCurrentProcess(),location,p->size);
        VirtualProtect(location,p->size,old,&ignored);
      }
    }
    report("GAMEPAD-PS1: JOYSTICK-READONLY REFUSED install failed; rollback attempted, original menu remains editable");
    return 0;
  }
  readonly_label_mode=mode;
  readonly_installed_mode=mode;
  snprintf(line,sizeof(line),
    SpiderSDL3ControllerSetup_Enabled()?
      "SDL3-SETUP: Joystick Restore Default replaced by preview row; native left AddEntry VA=0x%08lX; Keyboard untouched":
      "GAMEPAD-READONLY: Restore Default and matching placeholder rows disabled in Joystick only; native left AddEntry VA=0x%08lX; Keyboard restore preserved",
    (unsigned long)live_sites[SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT].virtual_address);
  report(line);
  if(mode==2u)
    report("GAMEPAD-XBOX360: JOYSTICK-READONLY ACTIVE native joystick rebind/restore and press-a-button capture disabled; Keyboard editable; A/B labels, remaining buttons numeric; bindings untouched");
  else
    report("GAMEPAD-PS1: JOYSTICK-READONLY EXPERIMENT ACTIVE native capture entry blocked for Joystick only, Keyboard editable; native CMenu intact; seven PS1 button names displayed, game rebind/restore calls blocked; RAM-only, requires in-game validation");
  return 1;
}
int SpiderPS1_InstallReadOnlyJoystick(void (*logger)(const char *)) {
  return spider_install_readonly_joystick(1u,logger);
}
int SpiderXbox360_InstallReadOnlyJoystick(void (*logger)(const char *)) {
  return spider_install_readonly_joystick(2u,logger);
}
