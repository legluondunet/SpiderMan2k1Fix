# Spider iris — atomic offscreen compositing contract (after V9 visual rejection)

## Reference and evidence

User-supplied 2026-11-06 04:42 screenshots and a 1.45-second 60fps
video show clearly separated black "legs" even when V9 is active.
The complete Wine log `SpiderMan2k1Fix(20261106-034518).log`
confirms `v1.1.0-d64ba08`, `UI-TRANSITION-TOPOLOGY: TRIAL ON`,
`UI-TRANSITION-TOPOLOGY: mapped=...` and no GPU reported error.
This invalidates V9's *visual* topology claim. Dreamcast footage
shows the spider legs attached to the black outside body; preserve
that relationship over the entire scene-opening wipe.

## Non-negotiable rendering invariant

**Treat the entire source mask as ONE object and ONE composition.**
The black exterior matte, spider body and all its legs are not
independent elements to be moved, extended, cropped or reconciled.
Let the original game drive the animation: no replacement animation
timer or special-case per-level placement. After source rendering,
use **ONE uniform similarity transform** (one scale, same X and Y)
of the resulting atomic mask image around its center. The mask grows
until its black shape is outside the physical screen.

Preserve the original relative geometry and pixel connectivity of
the SOURCE mask (no non-linear remap, per-tooth connectors, pinned
screen edge, or separately clipped bands). A narrow black screen
fringe is acceptable only if it belongs to the SOURCE whole-mask
raster and follows the same transform; do not synthesize independent
moving outer strips. The viewport crop may not be used to silently
discard parts of the spider body while teeth remain visible.

## Required implementation architecture

1. **Native source identification first:** capture the game-owned
   native D3D7 black draw families across a whole frame, including
   both `00000000` and `FF000000` where verified, not merely the
   zero-alpha inner teeth. The new opt-in `UI-ATOMIC-PROBE` records
   raw source vertices and caller RVAs from the real game, before
   canvas/HD/ESC transformations, in bounded selected frames.
   Do NOT include cinematic bars, text, HUD, ESC, world sprites or
   any surface without positively verified transition ownership.
2. **Atomic intermediate mask:** collect the complete transition
   in a single isolated source-aspect offscreen RGBA render target
   (or a CPU-generated alpha mask if D3D7 render-to-texture is not
   available). Black coverage becomes alpha=1, revealed world
   becomes alpha=0. The original native source must be rasterized
   **without any per-quad position correction**. Preserve draw
   ordering and all mask components in one texture before display.
3. **ONE final texture draw:** overlay the already-composed mask
   above the gameplay scene using one uniformly scaled textured
   quad. The game owns animation progress; do not invent a second
   speed curve. Save/restore D3D7 render target, viewport, texture
   stages, alpha, depth, blend and state-blocks. Never let temporary
   mask resources affect the normal 3D scene or native ESC overlay.
4. **Safe failure:** if source family identification is incomplete
   or render-to-texture is unsupported, fall back to the last
   completely known-good pipeline with a bounded log explanation.
   Do not leave half of the frame masked or mix old/new paths.
   The new renderer must be a separate **explicit opt-in flag**.
5. **Validation:** at least one city and industrial opening under
   Wine with complete logs + 60fps end-frame footage, 4:3 and 16:9
   (16:10/ultrawide if practical), original full-frame black start,
   **no detached spider teeth**, zero masked gutter exposure,
   no distorted body, no terminal flicker and unchanged HUD/ESC
   rendering and keyboard/gamepad functionality. Windows hardware
   and explicit maintainer approval still required for release.

## Explicitly rejected approaches

- V6 source-edge clipping and separate extension rectangles.
- V7 uniform *per-quad* fullscreen cover (vertical cropping).
- V8 rectangular per-tooth side seals.
- V9 non-linear horizontal warp *per polygon*.

A smooth coordinate function applied only to four vertices of a
polygon is **not** the same as a unified image transformation:
rasterized internal edges remain straight and neighboring source
primitives may come from different material families. Claiming
topology preservation from test geometry alone was insufficient.

Do not merge #115 based solely on CI and do not advertise another
finished graphics fix until real hardware video proves it.

legluondunet assisted by ChatGPT (OpenAI)
