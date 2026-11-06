# Native spider transition (V25)

## Scope

SpiderMan2k1Fix keeps the original PC game's Direct3D 7 spider-iris transition. The
game still owns the animation, vertex topology, and draw ordering. The patch
changes **only the horizontal projection** of the verified original iris
polygons into a centered 4:3 canvas and adds independent black strips outside
the currently visible silhouette. It does not load a sprite, generate spider
legs, recompose a texture, or resize the top/bottom cinematic bars.

The implementation is in:
- `ui_transition_native43.h` — guarded 4:3 X mapping (including off-screen
  original vertices);
- `ui_transition_native43_bounds25.h` — screen-Y-clipped original tooth
  envelope, outer-matte exclusion, independently shrinking side strips and a
  1-physical-pixel anti-seam overlap;
- `ui_transition_native43_sync.h` — current-scene source collection and
  deferred draw. The shrink/hold headers are **internal fallback geometry**,
  not separate user-selectable rendering modes.
- `spiderman2k1fix.c` — game caller/material guards, Direct3D 7 BeginScene
  and EndScene integration, COM state save/restore and diagnostic logging.

The original `SpideyPC.exe` identity SHA-256 remains mandatory. Only original
triangle fans with the known caller RVA, FVF, colour, Z/RHW, alpha-state and
untextured material can be mapped. If verification fails, the original game
DrawPrimitive is called unchanged. Fullscreen black bars and all other draw
families are left alone.

## Test activation

This rendering change is still **opt-in** until native Windows and additional
widescreen resolutions have been validated on hardware. With the existing
widescreen HUD enabled, set:

```text
SPIDERMAN2K1FIX_UI_CANVAS_TEST=1
SPIDERMAN2K1FIX_UI_TRANSITION_NATIVE43_BOUNDS_TEST=1
```

These are process environment variables (not INI keys). No V6–V24 transition
variables are required or recognized. EndScene instrumentation is automatically
enabled when V25 is active, even if the separately optional pause compositor
is disabled.

To restore the existing stable behaviour, unset the second variable (and the
canvas trial variable if not otherwise testing that UI feature).

## Lifecycle and failure containment

On BeginScene the per-frame original-iris prototype and shape envelope are
reset. Original draws are rendered exactly once through the game API. At
EndScene, **after all original polygons of the frame have been observed**, the
patch draws at most two untextured, black physical gutter rectangles. It
restores D3D7 render states and the bound texture. There are no game assets,
frame-count delays, per-level constants, or calls to replay old spider polygons.
A source family with no verified visible tooth uses the prior, bounded
two-strip fallback; if the device refuses the supplemental rendering, the
original spider remains unaffected.

At the first scene with no verified native iris, the two stored envelope widths
reset for the next animation. This prevents black margins from reappearing
within an animation when original polygons are culled outside the screen.

## Validation and remaining limits

- Video comparison under Wine/Proton at **1920 × 1080** on 6 November 2026:
  the black vertical seam was not visible and the eight original limbs reached
  the physical display edges before being clipped.
- C geometry and monotonicity tests cover 4:3, 16:10, 16:9 and ultrawide,
  including 3440 × 1440 and 5120 × 1440.
- Native Windows **and non-1920×1080 visual behaviour remain to be validated**.
  Passing math and cross-compilation tests does not replace that validation.
- Debug logging: `[Debug] Log=1`, then search for
  `UI-TRANSITION-BOUNDS25: V25 ON`, `FRAME=`, `RELEASE`,
  `NATIVE-UNCHANGED` and `END-SCENE D3D7 DRAW/STATE ERROR`.

Older V6–V24 experimental renderer implementations and their environment flags
were deleted from the source, not merely hidden behind disabled switches. The
underlying native-width helper used for conservative fallback remains tested.
