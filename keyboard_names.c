/* French AZERTY labels for the retail game's keyboard settings and CHEATS
 * text entry. The original Shell_InputName (0x0048DC10) asks the original
 * DXINPUT_GetKeyName (0x00501550) for each physical DIK scan code; that
 * routine dereferences a fixed QWERTY string-pointer table at 0x00562FF4.
 *
 * We replace only six entries in that table in RAM, with pointers to the
 * game's own original strings. Gameplay DirectInput events remain untouched.
 *
 * All addresses below apply ONLY to the supported SHA-256 verified
 * SpideyPC.exe (0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea).
 */
#include <windows.h>
#include <string.h>
#include "keyboard_names.h"

#define SPIDER_DIK_NAME_TABLE ((DWORD *)(ULONG_PTR)0x00562FF4u)

static int original_text_path_verified(void) {
  /* Original Shell_InputName's CALL to DXINPUT_GetKeyName. */
  static const BYTE call_signature[]={0xE8,0x03,0x35,0x07,0x00};
  /* DXINPUT_GetKeyName: index&0xFF, reads [eax*4 + 0x562FF4]. */
  static const BYTE name_signature[]={
    0x8B,0x44,0x24,0x04,0x56,0x25,0xFF,0x00,0x00,0x00,0x57,
    0x83,0xC9,0xFF,0x8B,0x3C,0x85,0xF4,0x2F,0x56,0x00
  };
  const DWORD *table=SPIDER_DIK_NAME_TABLE;
  return sizeof(void*)==4 &&
         memcmp((const void *)(ULONG_PTR)0x0048E048u,
                call_signature,sizeof(call_signature))==0 &&
         memcmp((const void *)(ULONG_PTR)0x00501550u,
                name_signature,sizeof(name_signature))==0 &&
         table[0x10]==0x00563AB8u && /* Q */
         table[0x11]==0x00563AB4u && /* W */
         table[0x1E]==0x00563A70u && /* A */
         table[0x27]==0x00563A4Cu && /* COLON (US semicolon) */
         table[0x2C]==0x00563A28u && /* Z */
         table[0x32]==0x00563A14u && /* M (French comma) */
         table[0x33]==0x00563A0Cu;   /* COMMA */
}

static int native_french_azerty(SpiderKeyboardLayout mode,HKL hkl) {
  if(mode==SPIDER_KEYBOARD_AZERTY)return 1;
  if(mode!=SPIDER_KEYBOARD_AUTO || !hkl)return 0;
  /* Only patch a layout whose actual mapping has the five verified French
   * letter positions. Other international layouts retain their original
   * table until their text-entry mapping has been separately audited. */
  return MapVirtualKeyExA('A',MAPVK_VK_TO_VSC,hkl)==0x10 &&
         MapVirtualKeyExA('Q',MAPVK_VK_TO_VSC,hkl)==0x1E &&
         MapVirtualKeyExA('Z',MAPVK_VK_TO_VSC,hkl)==0x11 &&
         MapVirtualKeyExA('W',MAPVK_VK_TO_VSC,hkl)==0x2C &&
         MapVirtualKeyExA('M',MAPVK_VK_TO_VSC,hkl)==0x27;
}

int SpiderKeyNames_Install(SpiderKeyboardLayout mode,HKL hkl,
                           void (*logger)(const char *)) {
  static int installed=0;
  DWORD old_protect=0,ignored=0;
  DWORD *table=SPIDER_DIK_NAME_TABLE;
  DWORD old_q,old_w,old_a,old_z,old_m,old_comma;
  if(installed)return 1;
  if(!native_french_azerty(mode,hkl))return 0;
  if(!original_text_path_verified()) {
    if(logger)logger("KEY-NAMES: original CHEATS/key-name signatures mismatch; no patch applied");
    return 0;
  }
  if(!VirtualProtect(table+0x10,(0x33-0x10+1)*sizeof(DWORD),
                     PAGE_READWRITE,&old_protect)) {
    if(logger)logger("KEY-NAMES: could not unlock original key-name table; no patch applied");
    return 0;
  }
  old_q=table[0x10];old_w=table[0x11];old_a=table[0x1E];
  old_z=table[0x2C];old_m=table[0x32];old_comma=table[0x33];
  /* All new pointers reuse static original-game strings, not DLL allocations. */
  table[0x10]=old_a;     /* physical AZERTY A */
  table[0x1E]=old_q;     /* physical AZERTY Q */
  table[0x11]=old_z;     /* physical AZERTY Z */
  table[0x2C]=old_w;     /* physical AZERTY W */
  table[0x27]=old_m;     /* physical AZERTY M (US semicolon) */
  table[0x32]=old_comma; /* physical AZERTY comma (US M) */
  VirtualProtect(table+0x10,(0x33-0x10+1)*sizeof(DWORD),
                 old_protect,&ignored);
  installed=1;
  if(logger)logger("KEY-NAMES: French AZERTY CHEATS text and keyboard menu labels patched in RAM (A/Q Z/W M/comma); DirectInput untouched");
  return 1;
}
