#ifndef SPIDERMAN2K1FIX_GAMEPAD_START_NATIVE_H
#define SPIDERMAN2K1FIX_GAMEPAD_START_NATIVE_H
/* Call only after supported_exe() SHA-256 verification and INI validation. */
void SpiderGamepadStart_Init(const char *ini,void (*logger)(const char *));
#endif
