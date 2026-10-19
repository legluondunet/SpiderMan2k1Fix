#ifndef SPIDER_DI8_CALLSITE_H
#define SPIDER_DI8_CALLSITE_H

/* Optional, SHA-gated native Win32 test. Neither an IAT hook nor a
 * DirectInput vtable replacement: only two original-game branch sites.
 * Logger is optional; never called directly from generated x86 stubs. */
void SpiderDI8Callsite_Init(void (*log_fn)(const char *));
void SpiderDI8Callsite_Report(long flip_index);

#endif
