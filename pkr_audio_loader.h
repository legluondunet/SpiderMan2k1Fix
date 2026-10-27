#ifndef SPIDERMAN2K1FIX_PKR_AUDIO_LOADER_H
#define SPIDERMAN2K1FIX_PKR_AUDIO_LOADER_H

/* Experimental: repair the original WAV resource's missing final
 * partial 2048-byte block before the game creates DirectSound buffers.
 * Call after supported_exe() SHA256, INI validation, and PKR index init.
 * OFF by default, signature-gated, transparent on any guard failure. */
void SpiderPKRAudioLoader_Init(void (*logger)(const char *),int enabled);

#endif
