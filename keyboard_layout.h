/* Keyboard layout: only Auto or Off is user-facing.
 * Physical DIK positions remain the game's input representation. The host
 * mapping is validated as a complete injective A-Z map before use; a partial
 * or ambiguous Windows/Wine layout is never applied to saved controls.
 *
 * This helper has no Windows dependencies and is tested on the build host.
 */
#ifndef SPIDERMAN2K1FIX_KEYBOARD_LAYOUT_H
#define SPIDERMAN2K1FIX_KEYBOARD_LAYOUT_H

typedef enum {
  SPIDER_KEYBOARD_OFF=0,
  SPIDER_KEYBOARD_AUTO=1
} SpiderKeyboardLayout;

static const unsigned char spider_us_letter_scans[26]={
  0x1E,0x30,0x2E,0x20,0x12,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,
  0x31,0x18,0x19,0x10,0x13,0x1F,0x14,0x16,0x2F,0x11,0x2D,0x15,0x2C
};

static inline int SpiderKeyboard_ValidateMap(const unsigned host_scans[26],
                                       unsigned char output_scans[26]) {
  unsigned i,j;
  for(i=0;i<26;i++) {
    /* DirectInput positions for the ASCII letter keys must be nonextended. */
    if(host_scans[i]==0 || host_scans[i]>=0x80u)return 0;
    for(j=0;j<i;j++)if(host_scans[i]==host_scans[j])return 0;
  }
  for(i=0;i<26;i++)output_scans[i]=(unsigned char)host_scans[i];
  return 1;
}

static inline unsigned SpiderKeyboard_MapLetter(SpiderKeyboardLayout mode,
                                         char uppercase_letter,
                                         unsigned physical_dik,
                                         const unsigned char scanned_letters[26],
                                         int map_valid) {
  if(mode==SPIDER_KEYBOARD_AUTO && map_valid &&
     uppercase_letter>='A' && uppercase_letter<='Z')
    return scanned_letters[uppercase_letter-'A'];
  return physical_dik;
}
#endif
