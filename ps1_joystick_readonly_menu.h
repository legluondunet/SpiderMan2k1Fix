#ifndef SPIDERMAN2K1FIX_PS1_JOYSTICK_READONLY_MENU_H
#define SPIDERMAN2K1FIX_PS1_JOYSTICK_READONLY_MENU_H
/*
 * Original SpideyPC.exe SHA-256 0a11a49f... fixed 32-bit PCShell helpers.
 * Cross-referenced to krystalgamer/spidey-decomp PCShell.cpp / PCInput.cpp
 * and its tools/functions/{5296368,5296880}.bin.
 *
 * These are IN-MEMORY code call sites, not file offsets. Never patch a
 * different executable; perform strict byte checks before all writes.
 * The profile is an opt-in experiment until its native UI is tested.
 */
#include <stddef.h>
#include <stdint.h>

enum { SPIDER_PS1_JOYSTICK_LABEL_CAPACITY=32 };
typedef struct {
  uint32_t virtual_address;
  uint8_t size;
  uint8_t original[6];
} SpiderPS1JoystickReadOnlySite;

enum { SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT=8 };
enum { SPIDER_PS1_JOYSTICK_RESTORE_ROW_SLOT=6, SPIDER_PS1_JOYSTICK_EMPTY_ROW_SLOT=7 };
static const SpiderPS1JoystickReadOnlySite spider_ps1_readonly_sites[
    SPIDER_PS1_JOYSTICK_READONLY_SITE_COUNT]={
  /* initActionMaps: joystick branch formats gKeyNames[i] using sprintf;
   * replace the single CALL with our 3-argument label formatter, BEFORE
   * native CMenu::AddEntry copies the text. 0x50D28C -> 0x529F90. */
  {0x0050D28Cu,5u,{0xE8u,0xFFu,0xCCu,0x01u,0x00u,0}},
  /* processControllerScreen: native restore-defaults call (controller
   * path only); leave the menu row but make activation read-only. */
  {0x0050D4A9u,5u,{0xE8u,0x92u,0xD0u,0xFFu,0xFFu,0}},
  /* Native key/button reassignment begins; both branches jump to the
   * game's original per-frame cleanup at 0x50D852, not into a capture
   * routine. Cursor and Exit triggers remain original and unmodified. */
  {0x0050D51Cu,5u,{0x8Bu,0xC3u,0x89u,0x35u,0x48u,0x00u}},
  {0x0050D6A6u,6u,{0x8Bu,0xD3u,0x56u,0xC1u,0xE2u,0x05u}},
  /* Extra defense: the native game action setter cannot be called by
   * a stale capture state. Its call is controller-menu-local. */
  {0x0050D7ADu,5u,{0xE8u,0x2Eu,0xCDu,0xFFu,0xFFu,0}},
  /* Once the original native menu detects Enter on a binding row,
   * this branch would FIRST arm button capture and redraw a capture
   * prompt. A guarded 32-bit trampoline tests gActionMapRelated
   * (0xAC129C) and only skips that entire action on Joystick screens.
   * Keyboard Configuration must still execute the original two PUSHes
   * and continue at 0x50D400, keeping keyboard remapping editable. */
  {0x0050D3FAu,6u,{0x55u,0x68u,0x00u,0x20u,0x00u,0x00u}},
  /* initActionMaps real final two native rows: original first column
   * gControllerMenu (0xAC11B4) gets Restore Default through a string
   * POINTER at 0x54B7B4 (not PUSH literal!), while gControllerMenuTwo
   * (0xAC12A0) gets an empty placeholder via PUSH 0x56EB54.
   * Both must be omitted TOGETHER in joystick mode so CMenu entries
   * remain synchronized. Verified public original 501-byte function
   * 5296368.bin: 0x50D2C8 and 0x50D2D8 both CALL 0x43FFF0. */
  {0x0050D2C8u,5u,{0xE8u,0x23u,0x2Du,0xF3u,0xFFu,0}},
  {0x0050D2D8u,5u,{0xE8u,0x13u,0x2Du,0xF3u,0xFFu,0}}
};
/* Xbox-specific labels are intentionally limited to two physically
 * observed Holtek 045E:028E DirectInput indices. Unknown indices use
 * the original numeric fallback: do not invent X/Y/LT/RT mappings. */
static inline const char *spider_xbox360_native_button_label(uint32_t button) {
  switch(button) {
    case 0u: return "A";
    case 1u: return "B";
    default: return NULL;
  }
}
static const char *spider_ps1_native_button_label(uint32_t button) {
  switch(button) {
    case 0u: return "SQUARE";
    case 1u: return "CROSS";
    case 2u: return "CIRCLE";
    case 3u: return "TRIANGLE";
    case 4u: return "L1";
    case 5u: return "R1";
    case 7u: return "R2";
    default: return NULL;
  }
}
#endif
