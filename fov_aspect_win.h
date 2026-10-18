#ifndef SPIDERMAN2K1FIX_FOV_ASPECT_WIN_H
#define SPIDERMAN2K1FIX_FOV_ASPECT_WIN_H
#include <windows.h>
/* Observed at RVA 0x150064; affects projection-related drawing code,
 * but camera FOV in degrees is not yet established. Never changes EXE on disk. */
void SpiderFOV_Init(const char *setting,int experiment,int horplus,void (*logger)(const char *));
void SpiderFOV_SetStage(int gameplay,unsigned screen_w,unsigned screen_h);
/* 1 only after Hor+ native-stage scalar was successfully installed. */
int SpiderFOV_GetHorPlusScalar(float *out);
/* Fail-closed if the HUD scope safety classifier could not be installed
 * BEFORE the first native SetDisplayMode. Never apply a partial Hor+ fix. */
void SpiderFOV_DisableUnsafeHorPlus(void);
/* Confirm the protected camera update succeeded before pillarboxing. */
int SpiderFOV_GameplayReady(void);
#endif
