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

static void (*ps1_menu_logger)(const char *)=NULL;
/* The native machine-code patch is immutable during this process.
 * 1 = Sony/PS1 labels, 2 = Xbox A/B with numeric fallback. */
static unsigned readonly_installed_mode=0;
static unsigned readonly_label_mode=0;
static void report(const char *msg) {
  if(ps1_menu_logger)ps1_menu_logger(msg);
}

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
 * The extra checked NOPs block native restore defaults and the
 * original setter as defense in depth. The 7-entry virtual profile and
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
    if(readonly_installed_mode==mode)return 1;
    report("GAMEPAD-READONLY: REFUSED another controller profile has already installed immutable native Joystick patches");
    return 0;
  }
  static const BYTE nop5[5]={0x90u,0x90u,0x90u,0x90u,0x90u};
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

  /* Verify every exact original machine instruction and writable text
   * window, including the original input branch/callsite signatures.
   * This strict preflight prevents patches on unknown binaries. */
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
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
  memcpy(replacements[1],nop5,5u); /* native restore-defaults */
  memcpy(replacements[4],nop5,5u); /* native controller action setter */
  replacements[0][0]=0xE8u; /* CALL joystick-only name formatter */
  {
    uint32_t rel=(uint32_t)(uintptr_t)&spider_ps1_joystick_name-
                 (spider_ps1_readonly_sites[0].virtual_address+5u);
    memcpy(&replacements[0][1],&rel,sizeof(rel));
  }
  for(i=2;i<=3u;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
    uint32_t rel=0x0050D852u-(p->virtual_address+5u);
    replacements[i][0]=0xE9u; /* JMP game's own cleaned return */
    memcpy(&replacements[i][1],&rel,sizeof(rel));
    if(p->size==6u)replacements[i][5]=0x90u;
  }
  {
    /* The last site replaces the entire six-byte capture-mode entry
     * with a jump to our checked mode-specific thunk. */
    const SpiderPS1JoystickReadOnlySite *p=
      &spider_ps1_readonly_sites[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT-1u];
    uint32_t rel=(uint32_t)(uintptr_t)capture_thunk-
      (p->virtual_address+5u);
    BYTE *replacement=replacements[SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT-1u];
    replacement[0]=0xE9u;
    memcpy(replacement+1,&rel,sizeof(rel));
    replacement[5]=0x90u;
  }
  for(i=0;i<SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT;i++){
    const SpiderPS1JoystickReadOnlySite *p=&spider_ps1_readonly_sites[i];
    BYTE *location=(BYTE *)(uintptr_t)p->virtual_address;
    if(!ps1_patch_readonly_site(location,replacements[i],p->size,
                               backups[i]))break;
    installed++;
  }
  if(installed!=SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT){
    while(installed>0u){
      const SpiderPS1JoystickReadOnlySite *p=
        &spider_ps1_readonly_sites[--installed];
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
