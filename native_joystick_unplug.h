#ifndef SPIDERMAN2K1FIX_NATIVE_JOYSTICK_UNPLUG_H
#define SPIDERMAN2K1FIX_NATIVE_JOYSTICK_UNPLUG_H

/* Stage 1 only: opt-in, exact-original-x86 patch for the game's fatal
 * joystick Acquire(DIERR_UNPLUGGED) branch. Does NOT rebind the native
 * device on reconnection. Installs on the render thread at Flip #120. */
void SpiderNativeUnplug_Init(const char *ini, void (*logger)(const char *));
void SpiderNativeUnplug_OnFlip(long flip_index);
/* Monotonic number of native Acquire(DIERR_UNPLUGGED) events for same-thread recovery. */
long SpiderNativeUnplug_Events(void);
int SpiderNativeUnplug_Installed(void);
#endif
