#ifndef SPIDERMAN_WINE_FOCUS_AUTOSTART_H
#define SPIDERMAN_WINE_FOCUS_AUTOSTART_H

/* Returns 1 when an installed native X11 helper was successfully launched
 * through Wine's own Unix process-spawn interface. Otherwise 0.
 * On native Windows the Wine exports are absent: a no-op.
 * Call from the game Flip thread, never under the loader lock. */
int SpiderWineFocusAutostart_TryStart(void (*log_line)(const char *));
/* Polls without blocking the game frame; enables bridge only after a
 * genuinely fresh GAME/OTHER state from the native observer. */
int SpiderWineFocusAutostart_PollReady(void (*log_line)(const char *));

#endif
