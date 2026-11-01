#ifndef SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#define SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#include <windows.h>
/* Called after SDL_InitSubSystem(SDL_INIT_GAMEPAD), before initial scan.
 * Loads approved user GUID mappings into SDL. Never modifies game input. */
void SpiderSDL3Raw_Init(HMODULE sdl,void (*logger)(const char *));
/* Modal, keyboard-operated raw joystick recognition for currently
 * unmapped SDL joysticks. Must run on the existing SDL/Flip thread. */
void SpiderSDL3Raw_Recognize(HWND owner);
unsigned SpiderSDL3Raw_UnmappedCount(void);
#endif
