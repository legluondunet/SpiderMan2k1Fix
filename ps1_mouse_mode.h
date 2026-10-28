#ifndef SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
#define SPIDERMAN2K1FIX_PS1_MOUSE_MODE_H
/*
 * Original retail game mouse-disable flag, not the Win32 desktop cursor.
 * Requires the exact supported SpideyPC.exe, a unique verified DS4 and an
 * explicitly enabled controller mouse-auto-off option.
 */
int SpiderPS1Mouse_Arm(void (*logger)(const char *));
void SpiderPS1Mouse_SetPadActive(int active);
#endif
