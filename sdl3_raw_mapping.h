#ifndef SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#define SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#include <windows.h>
#include <stdint.h>
/* Called after SDL_InitSubSystem(SDL_INIT_GAMEPAD), before initial scan.
 * Loads approved user GUID mappings into SDL. Never modifies game input. */
void SpiderSDL3Raw_Init(HMODULE sdl,void (*logger)(const char *));
/* Modal, keyboard-operated raw joystick recognition for currently
 * unmapped SDL joysticks. Must run on the existing SDL/Flip thread. */
void SpiderSDL3Raw_Recognize(HWND owner);
unsigned SpiderSDL3Raw_UnmappedCount(void);
/* Stable synthetic profile key from SDL GUID for VID/PID-less controllers.
 * Reserved pseudo vendor 0xFFFE is never used for native DI ownership. */
int SpiderSDL3Raw_ProfileKey(uint32_t instance_id,uint16_t vendor,
                             uint16_t product,uint16_t *key_vendor,
                             uint16_t *key_product);
#endif
