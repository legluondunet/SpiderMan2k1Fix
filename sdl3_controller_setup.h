#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_SETUP_H
/* In-game fixed PS1-style controller guide, drawn on the original
 * Joystick menu backbuffer. No modal popup and no configurable actions. */
#include <windows.h>
#include <ddraw.h>
void SpiderSDL3ControllerSetup_Init(const char *ini,
                                    void (*logger)(const char *));
int SpiderSDL3ControllerSetup_Enabled(void);
void SpiderSDL3ControllerSetup_OnNativeEnter(void);
void SpiderSDL3ControllerSetup_OnNativeExit(void);
int SpiderSDL3ControllerSetup_IsNativeActive(void);
void SpiderSDL3ControllerSetup_OnNativeFrame(void);
/* One snapshot for both native text and GDI artwork on the same frame. */
void SpiderSDL3ControllerSetup_RefreshGuide(void);
unsigned SpiderSDL3ControllerSetup_GuideFamily(void);
const char *SpiderSDL3ControllerSetup_GuideButton(unsigned position);
int SpiderSDL3ControllerSetup_DrawToSurface(
    IDirectDrawSurface7 *surface,const RECT *content);
/* Deprecated ABI kept nonoperational to avoid an accidental popup. */
void SpiderSDL3ControllerSetup_Open(void);
#endif
