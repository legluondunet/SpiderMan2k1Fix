#ifndef SPIDERMAN2K1FIX_INPUT_NOTACQUIRED_RETRY_H
#define SPIDERMAN2K1FIX_INPUT_NOTACQUIRED_RETRY_H

/* Exact-EXE x86 DirectInput error-path experiment.
 * OFF unless SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1.
 * Called on game render thread after SHA-256 verification. */
void SpiderDI8Retry_Init(void (*log_fn)(const char *));
void SpiderDI8Retry_OnFlip(long flip_index);
/* Developer-only, read-only native Escape batch observation. */
void SpiderDI8Retry_RequestEscapeBatchTrace(int enable);
void SpiderDI8Retry_RequestEscapeEdgeRecovery(int enable);
/* Only from the exact scoped native Pause edge reader. */
int SpiderDI8Retry_TakeEscapePauseEdge(unsigned raw,int eligible);
/* Only valid on the authenticated game thread after native trace install. */
int SpiderDI8Retry_EscapeTraceActive(void);
int SpiderDI8Retry_EscapeReaderWindow(void);
void SpiderDI8Retry_ObserveEscapeReader(unsigned kind,unsigned site,
    unsigned arg1,unsigned arg2,unsigned arg3,unsigned result,unsigned before,
    unsigned context);
/* Diagnostic gating hint only: incremented before the game's native
 * keyboard Acquire. Zero when no retry was needed; never an ACK. */
unsigned SpiderDI8Retry_KeyboardRecoveryAttempts(void);

#endif
