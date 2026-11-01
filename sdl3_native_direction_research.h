#ifndef SPIDERMAN2K1FIX_SDL3_NATIVE_DIRECTION_RESEARCH_H
#define SPIDERMAN2K1FIX_SDL3_NATIVE_DIRECTION_RESEARCH_H
/*
 * ORIGINAL x86 INPUT BEHAVIOR, pure C99 research reference ONLY.
 *
 * SHA-verified SpideyPC.exe 0x0050A2BD..0x0050A375:
 *  - 0x00AC0A18 stores a "block axes until centered" latch.
 *  - while the latch is set, both lX and lY MUST lie strictly inside
 *    (-250,+250) to clear it; this sample can then use the axes.
 *  - X/Y outside the latch map to held directions at <-250 and >+250.
 *  - POV[0] (0x00568394) ORs directional flags even when axes oppose.
 *  - 0x0050A9B0 enables latch and resets POV to 0xFFFFFFFF.
 *  - 0x0050A9D0 reads latch and POV, but DOES NOT clear the latch.
 * This model uses native [-1000,+1000] coordinates and [0,36000]
 * POV units. No SDL normalization can be declared calibrated from it.
 *
 * Not included by the runtime and NEVER writes game memory.
 */
#include <stdint.h>
enum {
 SPIDER_NATIVE_DIRECTION_UP=1u,SPIDER_NATIVE_DIRECTION_DOWN=2u,
 SPIDER_NATIVE_DIRECTION_LEFT=4u,SPIDER_NATIVE_DIRECTION_RIGHT=8u
};
typedef struct {
 unsigned block_axes_until_center;
 int32_t pov;
} SpiderNativeDirectionResearch;

static inline void spider_native_direction_research_reset(
    SpiderNativeDirectionResearch *state) {
  if(state) {
    state->block_axes_until_center=1u;
    state->pov=-1;
  }
}
static inline unsigned spider_native_direction_research_pov(int32_t angle) {
  unsigned directions=0u;
  if(angle==-1)return 0u;
  if(angle>18000)directions|=SPIDER_NATIVE_DIRECTION_LEFT;
  if(angle>0 && angle<18000)directions|=SPIDER_NATIVE_DIRECTION_RIGHT;
  if(angle<9000 || angle>27000)directions|=SPIDER_NATIVE_DIRECTION_UP;
  if(angle>9000 && angle<27000)directions|=SPIDER_NATIVE_DIRECTION_DOWN;
  return directions;
}
/* Side-effect-free peek matches the independent 0x0050A9D0 reader,
 * which NEVER clears the global 0x00AC0A18 suppression flag. */
static inline unsigned spider_native_direction_research_peek(
    const SpiderNativeDirectionResearch *state,int32_t x,int32_t y) {
  unsigned result=0u;
  if(!state)return 0u;
  if(!state->block_axes_until_center) {
    if(x< -250)result|=SPIDER_NATIVE_DIRECTION_LEFT;
    if(x>250)result|=SPIDER_NATIVE_DIRECTION_RIGHT;
    if(y< -250)result|=SPIDER_NATIVE_DIRECTION_UP;
    if(y>250)result|=SPIDER_NATIVE_DIRECTION_DOWN;
  }
  return result|spider_native_direction_research_pov(state->pov);
}
/* The main 0x0050A190 aggregator is allowed to clear the latch when
 * BOTH axes move STRICTLY inside the deadzone. This operation never
 * clears POV, which remains an independent directional input. */
static inline unsigned spider_native_direction_research_aggregator(
    SpiderNativeDirectionResearch *state,int32_t x,int32_t y) {
  if(!state)return 0u;
  if(state->block_axes_until_center &&
     x> -250 && x<250 && y> -250 && y<250)
    state->block_axes_until_center=0u;
  return spider_native_direction_research_peek(state,x,y);
}
#endif
