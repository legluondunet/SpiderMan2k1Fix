#ifndef SPIDERMAN2K1FIX_INPUT_NOTACQUIRED_RETRY_H
#define SPIDERMAN2K1FIX_INPUT_NOTACQUIRED_RETRY_H

/* Exact-EXE x86 DirectInput error-path experiment.
 * OFF unless SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1.
 * Called on game render thread after SHA-256 verification. */
void SpiderDI8Retry_Init(void (*log_fn)(const char *));
void SpiderDI8Retry_OnFlip(long flip_index);

#endif
