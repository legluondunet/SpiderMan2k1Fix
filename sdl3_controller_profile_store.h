#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_STORE_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_STORE_H
#include "sdl3_controller_profile.h"
/* Uses only Win32 filesystem calls, independent of the game's virtual
 * Spidey.cfg, native joystick table, SDL input and main INI repair.
 * One file per validated non-zero model VID/PID.
 *
 * 1=valid, 0=not found, -1=corrupted/unsafe, -2=I/O failure.
 * Caller uses defaults on all non-success results, without overwriting
 * the file. Save is transactional: update live memory ONLY if return 1. */
void SpiderSDL3ProfileStore_Init(int enabled,void (*report)(const char *));
int SpiderSDL3ProfileStore_Load(uint16_t vid,uint16_t pid,
                                SpiderSDL3ControllerProfile *out);
int SpiderSDL3ProfileStore_Save(const SpiderSDL3ControllerProfile *profile);
#endif
