#ifndef SPIDERMAN2K1FIX_KEYBOARD_NAMES_H
#define SPIDERMAN2K1FIX_KEYBOARD_NAMES_H
#include <windows.h>
#include "keyboard_layout.h"

/* Update only the original game's DIK-to-name table, after EXE SHA validation.
 * This does not intercept DirectInput, change raw scan codes, or rewrite the EXE.
 * Returns 1 only when a signature-checked French AZERTY mapping was installed.
 */
int SpiderKeyNames_Install(SpiderKeyboardLayout layout, HKL hkl,
                           void (*logger)(const char *));
#endif
