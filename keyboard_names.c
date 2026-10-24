/* Layout-aware names for retail CHEATS text entry and keyboard options.
 * Shell_InputName (0x0048DC10) calls DXINPUT_GetKeyName (0x00501550)
 * through the fixed QWERTY string-pointer table at 0x00562FF4.
 * Change only its ASCII-letter positions after signature validation.
 * No DirectInput hooks, and no change to the SpideyPC.exe on disk.
 *
 * All addresses apply ONLY to the SHA-verified 2001 retail executable.
 */
#include <windows.h>
#include <string.h>
#include <stdio.h>
#include "keyboard_names.h"

#define SPIDER_DIK_NAME_TABLE ((DWORD *)(ULONG_PTR)0x00562FF4u)
#define SPIDER_NAME_COUNT 0x80

static const char unmapped_key_name[]="KEY";

static int original_text_path_verified(void) {
  static const BYTE call_signature[]={0xE8,0x03,0x35,0x07,0x00};
  static const BYTE name_signature[]={
    0x8B,0x44,0x24,0x04,0x56,0x25,0xFF,0x00,0x00,0x00,0x57,
    0x83,0xC9,0xFF,0x8B,0x3C,0x85,0xF4,0x2F,0x56,0x00
  };
  const DWORD *table=SPIDER_DIK_NAME_TABLE;
  unsigned i;
  if(sizeof(void*)!=4 ||
     memcmp((const void *)(ULONG_PTR)0x0048E048u,call_signature,
            sizeof(call_signature))!=0 ||
     memcmp((const void *)(ULONG_PTR)0x00501550u,name_signature,
            sizeof(name_signature))!=0 ||
     table[0x10]!=0x00563AB8u ||
     table[0x1E]!=0x00563A70u ||
     table[0x27]!=0x00563A4Cu ||
     table[0x32]!=0x00563A14u ||
     table[0x33]!=0x00563A0Cu)
    return 0;
  /* Every original letter must have the expected single-letter name.
   * The strings are in the supported original executable's image. */
  for(i=0;i<26;i++) {
    DWORD ptr=table[spider_us_letter_scans[i]];
    if(ptr<0x00400000u || ptr>=0x00600000u ||
       ((const char *)(ULONG_PTR)ptr)[0]!=(char)('A'+i) ||
       ((const char *)(ULONG_PTR)ptr)[1]!='\0')
      return 0;
  }
  return 1;
}

/* A letter moved away from its US position must not remain as a duplicate
 * valid CHEATS letter on a French comma key or a similar physical position.
 * Reuse native punctuation labels when the layout exposes an ASCII symbol.
 */
static DWORD native_nonletter_name(unsigned scan,HKL hkl,const DWORD *original) {
  BYTE keys[256]={0};
  WCHAR chars[4]={0};
  UINT vk=MapVirtualKeyExA(scan,MAPVK_VSC_TO_VK,hkl);
  int n=vk?ToUnicodeEx(vk,scan,keys,chars,4,0,hkl):0;
  if(n==1) {
    switch(chars[0]) {
      case ',':return original[0x33]; /* COMMA */
      case '.':return original[0x34]; /* PERIOD */
      case '/':case '?':return original[0x35]; /* SLASH */
      case ';':case ':':return original[0x27]; /* COLON */
      case '-':case '_':return original[0x0C]; /* DASH */
      case '=':case '+':return original[0x0D]; /* EQUAL */
      case '\'':case '"':return original[0x28]; /* QUOTE */
      case '`':case '~':return original[0x29]; /* TILDE */
      default:break;
    }
  }
  return (DWORD)(ULONG_PTR)unmapped_key_name;
}

int SpiderKeyNames_Install(SpiderKeyboardLayout mode,HKL hkl,
                           const unsigned char letter_scans[26],int map_valid,
                           void (*logger)(const char *)) {
  static int installed=0;
  DWORD *table=SPIDER_DIK_NAME_TABLE;
  DWORD original[SPIDER_NAME_COUNT],replacement[SPIDER_NAME_COUNT];
  BYTE has_letter[SPIDER_NAME_COUNT]={0};
  DWORD old_protect=0,unused=0;
  unsigned i;
  int change=0;
  if(installed)return 1;
  if(mode!=SPIDER_KEYBOARD_AUTO || !map_valid || !hkl)return 0;
  for(i=0;i<26;i++) {
    if(letter_scans[i]!=spider_us_letter_scans[i])change=1;
    has_letter[letter_scans[i]]=1;
  }
  if(!change)return 0; /* QWERTY is already correct; no write needed. */
  if(!original_text_path_verified()) {
    if(logger)logger("KEY-NAMES: original CHEATS key-name signature mismatch; skipped");
    return 0;
  }
  memcpy(original,table,sizeof(original));
  memcpy(replacement,original,sizeof(replacement));

  for(i=0;i<26;i++) {
    unsigned us=spider_us_letter_scans[i];
    if(!has_letter[us])
      replacement[us]=native_nonletter_name(us,hkl,original);
  }
  for(i=0;i<26;i++)
    replacement[letter_scans[i]]=original[spider_us_letter_scans[i]];

  if(!VirtualProtect(table,sizeof(replacement),PAGE_READWRITE,&old_protect)) {
    if(logger)logger("KEY-NAMES: original table protection change failed; skipped");
    return 0;
  }
  memcpy(table,replacement,sizeof(replacement));
  VirtualProtect(table,sizeof(replacement),old_protect,&unused);
  installed=1;
  if(logger)logger("KEY-NAMES: Auto keyboard layout applied to CHEATS and original keyboard labels; no DirectInput hooks");
  return 1;
}
