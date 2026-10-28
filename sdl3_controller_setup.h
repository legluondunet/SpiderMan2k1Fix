#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_H
/* Experimental menu-launched Win32 SDL3 mapping PREVIEW.
 * Does not edit original game's joystick/keyboard mapping or persist. */
void SpiderSDL3ControllerSetup_Init(const char *ini,
                                    void (*logger)(const char *));
int SpiderSDL3ControllerSetup_Enabled(void);
/* Called ONLY from SHA-verified native Joystick "restore" callsite. */
void SpiderSDL3ControllerSetup_Open(void);
#endif
