#ifndef SPIDER_AUDIO_DS4_ROUTE_H
#define SPIDER_AUDIO_DS4_ROUTE_H

/* Best-effort game-local DirectSound8 output selection for Windows.
 * No global endpoint changes, no controller configuration, no COM vtable hooks.
 * Install only after the exact supported EXE hash and INI validation. */
void SpiderAudioDS4Route_Install(const char *ini, void (*logger)(const char *));

#endif
