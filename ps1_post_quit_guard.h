#ifndef SPIDER_PS1_POST_QUIT_GUARD_H
#define SPIDER_PS1_POST_QUIT_GUARD_H
/*
 * Guard only the short native 3D -> main-menu transition (request 7).
 * Pure per-Flip state machine: no input injection, native writes or logging.
 * The original game owns the return; callers suppress *synthetic* Enter
 * while this guard is active, but still update physical button edge state.
 */
typedef struct {
  unsigned saw_3d;
  unsigned settle_flips;
} SpiderPS1PostQuitGuard;

#define SPIDER_PS1_POST_QUIT_SETTLE_FLIPS 8u

static int spider_ps1_post_quit_guard_flip(SpiderPS1PostQuitGuard *state,
    int enabled,int menu_2d,int gameplay_3d,int movie_active,
    unsigned native_request) {
  if(!state)return 0;
  if(!enabled) {
    state->saw_3d=0u;
    state->settle_flips=0u;
    return 0;
  }
  if(gameplay_3d && !menu_2d && !movie_active) {
    state->saw_3d=1u;
    state->settle_flips=0u;
    return 0;
  }
  if(menu_2d && !gameplay_3d && !movie_active) {
    if(state->saw_3d) {
      state->saw_3d=0u;
      if(native_request==7u)
        state->settle_flips=SPIDER_PS1_POST_QUIT_SETTLE_FLIPS;
    }
    if(state->settle_flips>0u) {
      state->settle_flips--;
      return 1;
    }
  }
  return 0;
}
#endif
