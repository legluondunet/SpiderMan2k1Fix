/* Keyboard-layout policy for user-facing INI letter names. Pure C; no
 * Windows/DirectInput dependencies, suitable for host regression tests.
 *
 * The original game consumes physical DirectInput DIK scan codes. In physical
 * legacy mode ASCII labels denote US key positions, not international glyphs.
 * An explicit AZERTY mode supports French physical letter positions without
 * requiring a French Windows layout in Wine. Auto mode uses the host's active
 * MapVirtualKeyEx() VK->scan result; invalid/unmapped inputs preserve legacy.
 */
#ifndef SPIDERMAN2K1FIX_KEYBOARD_LAYOUT_H
#define SPIDERMAN2K1FIX_KEYBOARD_LAYOUT_H

typedef enum {
  SPIDER_KEYBOARD_PHYSICAL=0,
  SPIDER_KEYBOARD_QWERTY=1,
  SPIDER_KEYBOARD_AZERTY=2,
  SPIDER_KEYBOARD_AUTO=3
} SpiderKeyboardLayout;

static unsigned SpiderKeyboard_MapLetter(SpiderKeyboardLayout mode,
                                         char uppercase_letter,
                                         unsigned physical_dik,
                                         unsigned host_scan) {
  if(mode==SPIDER_KEYBOARD_AZERTY) {
    switch(uppercase_letter) {
      case 'A': return 0x10u; /* French A at original DIK_Q */
      case 'Q': return 0x1Eu; /* French Q at original DIK_A */
      case 'Z': return 0x11u; /* French Z at original DIK_W */
      case 'W': return 0x2Cu; /* French W at original DIK_Z */
      case 'M': return 0x27u; /* French M at original DIK_SEMICOLON */
      default: return physical_dik;
    }
  }
  if(mode==SPIDER_KEYBOARD_AUTO && host_scan>0 && host_scan<0x80u)
    return host_scan;
  return physical_dik;
}
#endif
