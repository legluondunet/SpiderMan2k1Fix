#ifndef SPIDERMAN2K1FIX_KEYBOARD_NAMES_H
#define SPIDERMAN2K1FIX_KEYBOARD_NAMES_H
#include <windows.h>
#include "keyboard_layout.h"

/* Retail-game key-name table correction, in RAM only after EXE SHA check. */
int SpiderKeyNames_Install(SpiderKeyboardLayout mode,HKL hkl,
                           const unsigned char letter_scans[26],int map_valid,
                           void (*logger)(const char *));
#endif
