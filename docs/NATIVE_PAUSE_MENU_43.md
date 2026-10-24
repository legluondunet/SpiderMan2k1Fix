## After build #780 — 16:9 pause tint gutter regression and experimental restoration (2026-10-24)

Pixel evidence from the user-provided native 1920x1080 captures:
- `scrn0005(3).png` and `scrn0006(2).png`: original **full-screen violet/blue dimming** before motif correction.
- `scrn0007(2).png` and `scrn0008(2).png`: orange/blue motif aspect **looks correct**, original center panel and selector remain correct, but the full-screen **dimming is absent in the 240-pixel gutters** x=0..239 and x=1680..1919. The margins show normal untinted world graphics, while x=240..1679 is darkened.
- Direct pixel comparison against normal gameplay `scrn0004(3).png`: at y=10..30 and y=1020..1050, the build #780 gutters are **pixel-identical** to normal gameplay, unlike the previous dimmed version. This confirms the aspect transformation also moved the full-screen tint into the 4:3 interior. There is no need to retune the violet panel, cursor, glyphs, or 3D backdrop.

**Candidate fix**: Retain build #780 exact, opt-in native alpha=0x80, no-texture 2D quad classifier and X-only motif transform. For each *positively classified* transformed native 4-vertex triangle fan at 1920x1080 Auto only, replay its **two original triangles**, Sutherland-Hodgman-clipped independently to the **left (x<=240)** and **right (x>=1680)** gutters. Replay uses the **original D3D7 draw**, the game's **current render state and blending**, the same source **Z/RHW/UV/diffuse**, and interpolated edge colors. It draws **nothing in x=240..1680**, so there is no double tint over the center, no guessed purple color, and no modifications to original comic, HUD, world, ribbon, text or cursor.
Only the original alpha UI quads actually intersecting the gutters are re-rendered, minimizing extra GPU work. Guards and opt-in remain `SPIDERMAN2K1FIX_NATIVE_ESC_43=1` + `SPIDERMAN2K1FIX_NATIVE_MOTIF_43=1` (no new INI option).
This is an experimental graphics correction, **not yet validated in Wine**.

Look for `NATIVE-MOTIF-43: GUTTER-REPLAY` in `Log=1`. On the next Wine test take F9 screenshots in 3D normal, ESC, and REALLY QUIT?; verify both 240px gutters match the previous **uniform full-screen violet tint**, motif remains 4:3, banner/selector and HUD stay sharp, and no colored motif extends improperly beyond x=240..1680. If unexpected artifacts occur, unset `NATIVE_MOTIF_43` to revert **only the motif experiment**, keeping other validated patches. Keep PR #28 draft.

# Native ESC pause menu: centered 4:3 diagnosis

## Current experiment: Wine build after #770 (2026-10-24)

**The actual red gameplay ESC selector was independently identified** in
user screenshot \`scrn0007(1).png\` and
\`SpiderMan2k1Fix(20261024-084004).log\`, which ran
\`SpiderMan2k1Fix v1.0.0-ee64819\`. The stable Direct3D7
\`SELECTOR-SHAPE\` draw is:

- Caller RVA \`0x00102C72\`, non-indexed \`D3DPT_TRIANGLEFAN\`,
  \`FVF=0x144\`, exactly four vertices.
- \`32x32\` bound texture, \`diffuse=0xFFFFFFFF\`, pixel format
  flags \`0x41\`, \`z=0.00000\`, \`rhw=27.519\`.
- 96x72 quad at \`(982,526)-(1078,598)\` (matches the
  screenshot's red pixel bounds within transparent sprite margins),
  and the **same texture and signature** at
  \`(772,495)-(868,567)\` in another menu-selection state.
- This quad occurs *after* the independently validated wide, untextured
  purple native ESC ribbon in the **same Flip**.
- Font glyphs may use the same 32x32 texture dimensions, but have
  a completely different size, z and rhw. Sprite pointer
  \`04e1a168\` is only meaningful during this recorded Wine run and
  **must not** be embedded as an object identity.

The previous \`PCSHELL_DrawMouseCursor\` stack-owner hypothesis proved
incorrect/unusable for this particular render: Wine reported
\`depth=0\`; 758-DWORD x86 fallback scans never matched the supposed
owner. The old stack walker is **now replaced** by a strict quad
signature check. The replacement remains **OFF by default**.

On this first experimental version, the adjustment is restricted to
**1920x1080 Auto**, with \`SPIDERMAN2K1FIX_NATIVE_ESC_43=1\`
and \`SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1\` both set.
Only the selector's X vertex coordinates are compressed around
**that sprite's existing center** by 0.75 (96 -> 72 px); Y, Z,
RHW, UV, colors, other UI, world and game input are untouched.
The positional center is preserved for the first controlled visual
test; other menu text continues to use its pre-existing correction.
No promise is made yet about behavior at other resolutions or on
native Windows.

Next Wine test (fresh matched winmm proxy + main DLL from the
same CI artifact):

1. Set \`[Debug] Log=1\` in INI.
2. Enable \`SPIDERMAN2K1FIX_NATIVE_ESC_43=1\` and
   \`SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1\`. The read-only
   \`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1\` is optional.
3. Open native in-game ESC menu, then REALY QUIT? / YES and NO;
   take F9 lossless PNG screenshots at multiple selector positions.
4. Verify \`NATIVE-CURSOR-43: QUAD-MATCH\` entries, expected
   \`output-width=72.0\`, and check that red cursor height,
   text, panel, HUD, input and gameplay all remain unchanged.
   On any mismatch, disable just the cursor opt-in and preserve
   screenshots + log.

**Do not merge PR #28 on the basis of logging or compilation alone.**
It requires visual Wine and native Windows validation.

## Next visual calibration — ESC outline and backdrop motif (24 October 2026)

Two original unscaled **1920x1080** PNGs were compared:

- Original 4:3 pillarboxed `scrn0002(3).png`: violet horizontal
  outline runs from **x=241 to x=1679**, inclusive, a 1439-pixel row.
- Corrected Auto `scrn0000(4).png`: outline spans
  **x=224 to x=1702**, a 1479-pixel row.
- Both were measured from purple border pixels at screenshot y=398–402
  and y=679–681. The original and new menus have identical
  vertical extents. The difference is about 40 horizontal pixels.

The previous 0.75 aspect transform made the **border/corner geometry**
slightly wider than the reference, while the untextured violet fill
was already substantially closer. Therefore this update does **not**
apply the same 2.7% additional scaling to the entire menu.

For the **already recognized native ESC ribbon and 8x8 borders**
in **1920x1080 Auto only**, transform X as before to the 4:3
coordinate system, then separately calibrate final X against the
original full-resolution PNG:

- Outer border/corner pieces: `960+(X-963)*(1439/1479)`
- Untextured fill: `960+(X-958.6)*(1439/1448.5)`
- All menu text glyphs, cursor sprite, Y, depth, RHW, textures, mouse
  coordinates, HUD and 3D world stay **unchanged** from the previous
  experimentally validated rendering. Other resolutions keep the old
  transform until measured individually.

The **large translucent orange/blue background motif is not yet
identified** as belonging either to the native pause UI or to
the projected 3D scene. Some early 128x128-textured projected world
quads in older logs have extremely large screen rectangles, e.g.
`caller_rva=0x00103389` with non-UI Z/RHW. Scaling them
blindly would distort the background and gameplay geometry.

Instead this build adds a **read-only, opt-in**
`PAUSE-NATIVE-PROBE: MOTIF-CANDIDATE` line.
During the existing F9-triggered short diagnostics, it reports a
bounded sample (at most **28 per sampled frame**) of large triangle
draws with caller RVA, vertex format, texture dimensions, diffuse
color, source rectangle, Z and RHW. Samples may be **world/scene
geometry**; their presence is not proof of identity and triggers
**no rendering change**. Any motif correction must wait for
correlation across normal gameplay, paused gameplay, and 4:3 screenshots.

### Wine test

Keep `[Debug] Log=1` in the existing INI. Launch with:

```bash
SPIDERMAN2K1FIX_NATIVE_ESC_43=1 SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1 SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1 WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

Open the native ESC menu. Capture F9 at CONTINUE and at
REALLY QUIT? / NO/YES; leave each unchanged for a second.
Capture the same scene once **without ESC** as a geometry
reference. Save the three PNGs and the complete log. Check the
violet border x=241..1679; check that the text and 72-pixel
red selector remain unaffected. Compare `MOTIF-CANDIDATE`
entries to decide whether the motif is a distinct native
2D overlay or part of the projected world. This PR remains a
draft until visual and platform validation.

## Build after #777 — opt-in native ESC translucent motif X-correction (2026-10-24)

Evidence from exact **1920x1080** F9 images:
- `scrn0004(3).png`: same roof, Spidey and background in **normal gameplay**;
  orange/blue large translucent shapes are **absent**.
- `scrn0005(3).png`: ESC panel active; translucent geometric shapes
  appear while background 3D remains the same scene.
- `scrn0006(2).png`: REALY QUIT? / YES/NO uses the same shapes.
- `SpiderMan2k1Fix(20261024-093324).log`: native ESC panel
  x=241..1679 (1439 px) exactly matches original `scrn0002(3).png`.
  Already validated red selector and menu glyph transformations remain unchanged.

The #777 passive `MOTIF-CANDIDATE` group was **too broad** and
sampled early **3D world triangles**: `caller_rva=0x00103389`,
3 vertices, 128x128 textured, variable `z≈0.02..0.04`,
`rhw≈0.5..1.3`. Those **must not be rescaled**.

However the same log repeatedly reveals a separate, native **2D**
family before the purple banner, seen in both pause states:
`caller_rva=0x00102C72`, **4-vertex triangle fan**,
`FVF=0x144`, **NO bound texture**, `z=0.00146`,
`rhw=9.988`, vertex **alpha=0x80**, e.g.
`diffuse=0x80101004 / 0x80868620 / 0x80208686`,
screen quads `(1331.2,693)..(1485.0,792)`,
`(281.2,585)..(528.8,711)`,
`(288.8,333)..(525,459)`,
`(435,693)..(588.8,792)`. This is a **strong**
pause-motif candidate, distinct from 3D and from
purple fill/borders (those have z=0.00122, rhw=11.176)
and text (z=0.00098, rhw=12.686).
The dynamic color animation changes RGB but maintains alpha=0x80.
Because these drawings occur **before** the banner, a same-Flip
banner marker cannot be used as a guard.

**Experimental opt-in** `SPIDERMAN2K1FIX_NATIVE_MOTIF_43=1`:
strictly in 1920x1080 Auto, and only when
`SPIDERMAN2K1FIX_NATIVE_ESC_43=1` is already enabled,
match ALL aforementioned 2D signature fields.
Scale **only X** about 960 by 0.75 for a 4:3 shape, without
changing Y, Z, RHW, texture, colors, game input or camera FOV.
Exclude fullscreen-sized rectangles (potential shading layer),
very thin or off-screen primitives. OFF by default.
This is a **first visual hypothesis**, not proven
until it produces `NATIVE-MOTIF-43: QUAD-MATCH` and the
new F9 screenshots reproduce the original 4:3 geometry.

Wine test command (with `[Debug] Log=1` in INI):

```bash
SPIDERMAN2K1FIX_NATIVE_ESC_43=1 SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1 SPIDERMAN2K1FIX_NATIVE_MOTIF_43=1 SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1 WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

Capture F9 in normal gameplay, ESC, and REALLY QUIT? states.
Compare original 4:3 motif extents and inspect all areas
outside the native 4:3 ribbon (world must not be cropped,
darkening must remain full-screen). If any visual regression,
omit `SPIDERMAN2K1FIX_NATIVE_MOTIF_43=1` and retain the
independently validated previous ESC/cursor/band corrections.
**PR #28 remains draft, no merge.**

## Desired visual behavior

Keep the widescreen 3D world and already validated gameplay HUD
(health, compass, instructions) unchanged. The native ESC pause UI
(red PAUSED title, purple ribbon, CONTINUE / RESTART LEVEL / QUIT
labels and red selection icon) should use the **same centered 4:3
reference** as the independently implemented Alt+Tab focus-pause panel.

At **1920×1080**, the 4:3 visual area is **1440×1080**, from
x=240 to x=1680, centered at x=960. The expected horizontal mapping
of native full-width screen-space vertices, when positively identified,
is:

```
  x_corrected = 960 + (x_original - 960) * 0.75
```

Do **not** alter Y, z, RHW, texture UVs, color or game-owned vertex
memory. In original 4:3 output the mapping is identity. The formula
is implemented only in an explicit **experimental opt-in**; it is not
yet a visually validated game patch.

## Why trace first?

The same Direct3D7 DrawPrimitive entry at caller RVA 0x00102C72
handles world particles, gameplay HUD, comic art and menus. It
cannot uniquely identify the ESC pause panel.

The SHA-gated game code already observes pointer **0x00682950**
as the native gameplay pause context (original pause handler
0x0047BC40); this is separate from the Alt+Tab focus pause.
Its nonzero presence starts passive sampling when available, but is not
alone sufficient to identify individual pause sprites. Under Wine, the
first combined build #728 successfully enabled the diagnostic but did not
record any BEGIN event, while sampled pause_ctx values were zero. The next successful Wine log
`SpiderMan2k1Fix(20261024-013833).log` showed ESC-EDGE at Flip 257,
but the first 160 matching triangles in frames 1–4 were nearly identical
world geometry from `caller_rva=0x00103389`. This is **not** a safe
pause-menu signature; the initial cap hid later primitive calls. The
diagnostic therefore also watches the physical ESC-down edge via the
high bit of GetAsyncKeyState on the existing Flip hook. No input is
consumed, inserted or rerouted; this is a short fallback observation
window, not a pause-detection claim.

The build #730 Wine log `SpiderMan2k1Fix(20261024-055402).log`
provides a useful temporal comparison: `ESC-EDGE` at Flip 204,
**3268** D3D candidates per frame in samples 1–4, then **3316**
candidates per frame from frame 5 through frame 20. That is **48 primitives**
appearing just after ESC. The final sampled primitives are four-vertex
2D quads from caller RVA `0x00102C72`, but the 1-in-64 sampling still
misses almost all of the new group. This count difference is evidence
of a new render batch **correlated with ESC**, not yet proof of which
quads are the native PAUSED ribbon/text/selector.

To test that hypothesis, the next diagnostic logs **every candidate
from draw 3140 through draw 3400**, along with the first eight and
every 64th candidate elsewhere, in frames 1, 5, 12 and 20. This
catches the last 48 candidates while preserving the same bounded
window, disabled-by-default opt-in and strict read-only operation.
No gameplay HUD or native pause vertices are transformed.

### New Wine evidence: build #737

The 24 October Wine trace `SpiderMan2k1Fix(20261024-063105).log`
recorded three ESC-triggered sessions at 1920x1080 Auto:

- Session 1: 3062 to 3111 eligible primitives as the overlay appeared.
- Session 2: 3244 candidates in frames 1–4, then 3292 from frame 5
  (48 additional late 2D primitives).
- Session 3: a stable 3292 candidates throughout.

In session 2, the new four-vertex glyph rows occupy native Y=436,
508 and 580. Their positions match CONTINUE, RESTART LEVEL and QUIT.
They share caller RVA 0x00102C72, a caller also used by HUD/world
sprites, so caller RVA alone is **not** a safe mapping gate.
A moving 96x72, 32x32-textured quad near the selection rows is a
candidate for the red spider cursor, but its texture identity is not
proved. The full-width purple panel has not been positively isolated.
**The NO/YES confirmation screen was not identified in the original
24-Flip windows**; do not treat these logs as validation of its glyphs.

The next probe remains strictly read-only and adds only:
- Up to three additional 24-Flip capture windows after Enter/Space
  key-down edges for up to 1800 Flips following each observed ESC
  (roughly one minute at 30 FPS). These are **observation triggers**,
  not assertions that QUIT or the confirmation menu is open.
- A bounded (maximum twelve per sampled frame) search for wide
  screen-space middle-of-screen quads, to help identify the ribbon.
- The same first 8, every 64th and indices 3140..3400, sampled on
  frames 1/5/12/20. No drawing/texture/input modification.

The first safe step is therefore read-only identification of
the panel/text/icon signatures: original caller RVAs, D3D primitive
types, FVF, source vertex rectangles, depth/RHW and bound texture
sizes. A follow-up change will add tightly gated 4:3 mapping only
to proven ESC pause components, after examining the logs.

## Experimental centered 4:3 mapping (build after #741)

Build #741's 24 October Wine log `SpiderMan2k1Fix(20261024-064205).log`
confirmed both native screens in 1920x1080 Auto:

- ESC at Flip 245: native purple ribbon is the **untextured quad
  #3166** at x=(-7.5..1923.8), y=(400.5..675.0), z=0.00122,
  RHW=11.176. Text at y=436 / 508 / 580 is the main choice group.
- Enter at Flip 383: the same purple quad #3166 at y=(432..634.5),
  followed by REALLY QUIT? text at y=283 and NO / YES at y=468 / 540.
- PAUSED remains at y=171. The instruction overlay is at y=877 and
  must never be transformed by the ESC menu correction.
- There is a **96x72** central textured sprite around x=(911..1007),
  y=(504..576) in both screens. Its function is not yet verified,
  so we **do not transform it**. In particular, do not assume this
  is the moving red spider selector merely because of its size.

An opt-in **PAUSE-NATIVE-43** experiment maps the positively
identified ribbon, 8x8-textured borders, and glyphs to a centered 4:3 width, preserving Y,
Z, RHW, colors and UVs. It arms only after the ribbon's strict
screen rectangle, x86 caller RVA and Z/RHW signature are observed in
the **same Flip**. The border pieces additionally require an exact
8x8 texture, matching Z/RHW, and geometry relative to the recorded
banner bounds. Glyphs separately require the observed font texture
dimensions and depth. The confirmed HUD and scene are not subject
to this transform.

This is **OFF by default**, not an INI setting. To test together
with the passive diagnostic, use:

```bash
SPIDERMAN2K1FIX_NATIVE_ESC_43=1 SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1 WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

Leave `[Debug] Log=1` in your existing INI. Check both the
CONTINUE / RESTART LEVEL / QUIT and REALLY QUIT? / NO / YES screens,
capture screenshots of each with F9, and save the log. At 1920x1080
the target correction is `x' = 960 + 0.75 * (x-960)`. There should be
**no** change to the background 3D world, bottom instructions, compass,
or gameplay health UI. This is a *visual verification build*, not
a validated fix.

To identify the red spider selector, the passive probe now samples
draws **3140 through 3400** (originally 3200..3400). Do not turn on
unidentified selector transforms until a distinct signature is proven.

### Next experimental border/selector test (after build #745)

Build #745 screenshots of both ESC screens show the large violet ribbon and
text now occupy the centered 4:3 horizontal reference, while the thin
top/bottom lines and the small end-caps are still full-width. Its Wine log
`SpiderMan2k1Fix(20261024-065333).log` identifies all eight
**8x8-textured** border quads just after the untextured ribbon:

- top line: draw #3176, x=0..1920, y=396..414
- bottom line: draw #3174, x=0..1920, y=675..693
- left/right vertical strips: draws #3178 and #3173,
  x=-30..22.5 and x=1916.2..1968.8
- four corners: draws #3175, #3177, #3179 and #3180

The bounds above are from **one sampled frame** at 1920x1080. They
change with the ribbon animation and cannot be fixed pixel constants.
The next experimental correction applies the same center-based
X transform to these eight primitives only when an already verified
ribbon precedes them in the same Flip, with 8x8 texture, consistent
depth and geometric correspondence to that ribbon. It does not modify
vertical coordinates or make the 8x8 textures blurry through scaling
the source images; the GPU only changes the drawn X coordinates.

**The moving red spider selector is still unclassified and unmodified.**
The visible center 96x72 quad at x=911..1007 is not proof of the
selector's identity. To localize the selector in the next log,
the passive diagnostic re-arms a maximum of six additional 24-Flip
windows per ESC after *UP, DOWN, ENTER or SPACE* physical key edges,
within the same bounded 1800-Flip post-ESC period. All sampling is
read-only and opt-in, without virtual key injection or native menu
state modification.


### Targeted moving spider cursor diagnosis — after build #747

Build #747 showed the 4:3 **purple panel, corner pieces, top and bottom
borders, and both groups of texts properly centered**. The red
spider-shaped selection pointer is still separate and remains at its
uncorrected horizontal position. In
`SpiderMan2k1Fix(20261024-070917).log`, the native panel begins
around draw **1439**, not the older 3140–3400 range. As a result,
the previous sampler missed the relevant sprite even though UP/DOWN
key events and six follow-up sampling windows were recorded.

The new `PAUSE-NATIVE-PROBE: CURSOR-CANDIDATE` line samples by
**original screen geometry**, not a fixed draw number. It keeps the
original native 2D call site, a bounded depth/RHW range, and four
vertices in the middle-left panel region, but does **not** assume a
texture size or color. Each sampled candidate records its draw index,
screen rectangle, texture dimensions, **texture pointer ID** (only
comparable within the same process run), diffuse vertex color and
initial UV coordinates. At most **100** such candidates are logged
in each sampled frame (frames 1, 5, 12, 20) and no native vertices
are changed by the probe. The candidate label **does not mean the
red sprite has been identified**.

Run with both experimental variables as in the example above.
Open the ESC menu and leave the default selection visible for two
seconds, then press DOWN once and leave the next selection visible
for two seconds. Repeat for a third option. Open the QUIT confirmation
and record separate NO and YES selections, preferably including F9
captures. Send the entire new log so texture IDs, UVs and screen
rectangles can be correlated with each selection. Once the moving
selector has a consistent, distinct signature, the next patch can
safely add an X-only centered 4:3 transform for that one quad.

### Build #751 — detect the remaining red menu icon (read-only)

The build #750 Wine log `SpiderMan2k1Fix(20261024-072204).log`
confirms seven native pause-menu diagnostic windows and ~1,250
`CURSOR-CANDIDATE` records. It successfully detects font sprites,
but **no textured primitive matches the red icon** observed at
**x=502..597, y=494..558** in both 1920x1080 F9 screenshots.
In particular, the logged untextured pair at x=382.5..498.8,
y=450..585 is not evidence that it is the red icon. The red symbol
appears at the same location on both the regular ESC menu and the
REALLY QUIT? confirmation screen.

To avoid incorrectly scaling some other object, the opt-in
passive sampler now adds the label **`PAUSE-NATIVE-PROBE: ICON-REGION`**.
This independent geometric window includes nearby 4- or 6-vertex
draws (fan/list/strip, with both observed pre-transformed FVF formats)
from any caller; it records callsite, primitive type, source rectangle,
depth/RHW, texture identity/dimensions and diffuse color. It allows at
most 100 new region records per sampled frame, only within the existing
ESC and navigation-key 24-Flip windows and only if `[Debug] Log=1`
and the trace environment variable are enabled. This is **read-only**;
the accepted ribbon, borders, letters, gameplay HUD and F9 controls
are untouched. No red-icon transform is enabled.

The next F9 test should include the regular pause menu with selected
CONTINUE, RESTART and QUIT, and the confirmation with NO and YES.
Send both screenshots and the complete diagnostic log. Once a unique
textured, stable draw signature is confirmed against the visible
red icon, a narrowly gated X-only correction can be added.

### Build #757 — screenshot-correlated Wine verification

Two 1920×1080 F9 captures (`scrn0012.png`, `scrn0013.png`) and
`SpiderMan2k1Fix(20261024-074900).log` confirm that `F9-SNAPSHOT`
activates correctly at Flips **438** and **873**. The respective
24-Flip windows produced `SPIDER-POSE` candidate lines, but the sampled
sprites belong predominantly to the **text glyphs** (32×32 textures
around y=508 and y=540), not a positively identified red icon.
Do **not** add a texture-size-only correction to the existing 2D
menu pipeline: it would distort text.

Direct color-mask measurements of the user's unscaled PNGs:
- `scrn0012.png`, confirmation REALLY QUIT?: red cursor bounding
  box **(911,507) 96×67 pixels**
- `scrn0013.png`, regular ESC pause menu: red cursor bounding
  box **(900,552) 96×67 pixels**

The exact width/height stability supports X-only 0.75 as the
future **render correction target**, but the render identity
remains unverified.

**New source clue:** krystalgamer's partial **PC-port**
decompilation has `PCSHELL_DrawMouseCursor()`, which calls
`gCursorSprite->draw(gShellMouseX,gShellMouseY,0,0)`.
`PCSHELL_Initialize()` constructs this `Sprite2` from
`lti\\cursor.bmp` with a 32×32 mouse-bounds setup.
Reference:
https://github.com/krystalgamer/spidey-decomp/blob/master/PCShell.cpp
The accompanying `tools/names.json` suggests original
`PCSHELL_DrawMouseCursor` entry **0x0050C470**
(RVA **0x0010C470**), but its bytes must be validated against
the exact supported SHA-256 executable before using a native detour:
https://github.com/krystalgamer/spidey-decomp/blob/master/tools/names.json

**Next investigation:** verify the cursor asset/draw caller and
bound texture in the supported executable. Any eventual correction
must be scoped to the cursor's draw alone, preserving center/height
and mouse hit testing. **No game-code hook or cursor transform is
validated yet.**

### Build #755 — F9-correlated red spider sprite trace (24 Oct 2026)

The build #754 log `SpiderMan2k1Fix(20261024-073722).log`
recorded exactly **one ESC-EDGE at Flip #153**, collected one
24-Flip session (until #178), but **no post-ESC KEY-EDGE**.
The captured F9 screenshots occurred much later (~Flips #649, #865);
therefore the earlier diagnostic window did **not** cover the visible
icon positions. The key polling limitation is significant under Wine.

The user-supplied **full-size** screenshots, not the reduced chat
previews, show different poses:
- `scrn0010.png`: native ESC menu, selected RESTART LEVEL,
  red spider approx. **x=1121..1217, y=589..655**
- `scrn0011.png`: REALLY QUIT? selected NO,
  red spider approx. **x=937..1033, y=652..718**

Both red icons appear around **96×66** pixels; a 0.75 X-only
aspect correction would yield about **72×66**, subject to verifying
the exact native draw identity first. Previous statements that the icon
was at x=502..597 referred to an incorrectly scaled preview and
are **superseded** by these source PNG measurements.

When the opt-in probe is enabled, a *successful native F9 screenshot*
now triggers an independent **`PAUSE-NATIVE-PROBE: F9-SNAPSHOT`**
diagnostic session, capturing the *next* 24 Flips. This samples the
pose corresponding to the captured PNG (assuming the user keeps the
selection stationary) and does not require VK_UP/VK_DOWN detection.
Only the first four F9 captures after a detected ESC and within 1800
Flips are traced. The screenshot and all game input remain unchanged.

The supplementary **`PAUSE-NATIVE-PROBE: SPIDER-POSE`** records
3/4/6-vertex 2D draws in the original full-resolution icon region,
broadly across callers/FVF/triangle primitive types, with tight Z/RHW
gates and a 100-entry frame cap. Text sprites or unrelated geometry
can still appear and must not be treated as a proven selector.

**Test:** With `[Debug] Log=1`,
`SPIDERMAN2K1FIX_NATIVE_ESC_43=1`, and
`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1`, press ESC, take F9
at RESTART LEVEL then F9 at REALLY QUIT? / NO. Leave the same
selection highlighted for about one second after each F9.
The log should now contain **F9-SNAPSHOT**, **SAMPLE-FRAME**,
and **SPIDER-POSE** lines corresponding to the two captures.
Compare texture/callsite/rectangle/depth before attempting any
rendering fix.

### Experimental cursor 4:3 via verified native call stack (post-#757)

The decompilation's mouse cursor owner at **PCSHELL_DrawMouseCursor**
(VA `0x0050C470`, RVA `0x0010C470`, next function at RVA
`0x0010C4B0`) is a credible candidate for the red movable icon,
but direct comparison of function bytes against the SHA-verified game
executable has **not yet been completed**.

A **separate, OFF-by-default** visual experiment is now available:
`SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1`. It requires
`SPIDERMAN2K1FIX_NATIVE_ESC_43=1`, the already verified supported
SpideyPC.exe SHA-256, an executable memory page at the candidate RVA
and Windows' `RtlCaptureStackBackTrace` entry point. It records the
first 12 candidate function bytes **for follow-up verification**;
this alone is **not** an identity proof.

During a 3D game's native ESC ribbon draw frame, it checks the **entire
call stack** of bounded, appropriately sized 2D sprites. Only if a
return address is inside **0x0050C470–0x0050C4AF** does it modify
the quad's X coordinates. The specific correction compresses the
**sprite horizontally about its existing center by 0.75**. It does
not change Y, UV, mouse coordinates, gameplay hit testing or shared
texture surfaces. It never touches the generic 32×32 text glyphs based
on texture size alone. Maximum **180 candidates/Flip** are examined.

The prefix `NATIVE-CURSOR-43:` also logs a maximum of three early
`stack SAMPLE` call chains (including four return addresses, if available),
which can help diagnose failure to identify the native cursor owner.
The prefix `NATIVE-CURSOR-43:` logs initialization, unmatched candidate
caps and every early **owner MATCH**. If there are **no owner MATCH**
lines, the experiment intentionally applies **no correction**.
A successful compilation and static unit tests are not evidence that
the cursor owner is correct: only a native Wine/Windows screenshot
can validate this.

For the next Wine test:
```bash
SPIDERMAN2K1FIX_NATIVE_ESC_43=1 SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1 SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1 WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

Retain `[Debug] Log=1` in the existing INI, then capture the
regular ESC menu and REALLY QUIT? / NO / YES with F9, including
at least two cursor positions. Check the red sprite's width/height
and make sure **all text and the purple panel remain unchanged**.
Send both PNGs and the full log. If the candidate owner stack never
matches, stop and investigate call-stack availability or the original
Sprite2 draw owner; do **not** relax the guard based on texture size.

### Build #764 — comparison with the original 4:3 mouse cursor

The source PNGs supplied by the user on October 24, 2026 show a
**72x66 pixel red cursor on the MAIN MENU** (`scrn0000(2).png`),
and a **96x66 pixel cursor on the ESC and REALLY QUIT? screens**
(`scrn0003(1).png` and `scrn0004(2).png`). Measurements used
strict red-pixel connected components in the unscaled 1920x1080 PNGs.
This resolves the desired 4:3 sprite correction exactly: multiply
the **cursor X width by 0.75**, retain its center and unchanged height.
The main menu's 4:3 content must NEVER be modified by this experiment.

The Wine run `SpiderMan2k1Fix(20261024-081427).log` was on the
supported SpideyPC.exe SHA-256 and enabled all three opt-ins. The
native owner candidate at RVA 0x0010C470 is executable and begins
`F6 05 8C 98 E0 02 10 75 2F A1 A8 12`. Despite successful
initialization, the Windows stack walker returned **depth=0** on
all three sampled draws. No owner match occurred and, as expected,
the 96x66 cursor remained uncorrected.

The next opt-in experiment retains the original backtrace test but
adds a **bounded x86 stack return-address scan only when depth=0**.
It refuses to activate unless the supported executable hash, native
function opcode pattern, existing 4:3 ESC ribbon, sprite geometry,
and 32x32 cursor texture all pass. It scans at most 8192 bytes in
the COMMITTED current-thread stack region (checked via VirtualQuery),
without modifying the stack, and only accepts a return address
in the exact function interval **RVA 0x0010C470..0x0010C4AF**
if the preceding instruction resembles a machine **CALL**.
Missing matches always mean **NO RENDER MODIFICATION**.

Look for `NATIVE-CURSOR-43: x86 owner CALL-RETURN MATCH` (followed by
`owner MATCH`) or `fallback x86 stack scan ... owner-not-found`
in the next Wine log. This logic is experimental and relies on
native x86 return-address evidence, not a generic texture-size fix.
Even a positive match will still require two new visual screenshots,
mouse pointer movement and confirming the menu text remains intact
before merging PR #28.

### Build #767 — selector-shaped native draw sampler (read-only)

Wine test of CI #766, `SpiderMan2k1Fix(20261024-082835).log`:
the two new lossless PNGs are **still 96x65** red-pixel bounding boxes
(`scrn0005(2).png`, `scrn0006(1).png`); reference main-menu cursor
is **72x65** using the same red threshold. Neither matching method
identified the presumed `PCSHELL_DrawMouseCursor` frame: stack trace
`depth=0`, x86 fallback checked **758 DWORDs** three times and
returned `owner-not-found [NO CHANGE]`. The 60x50 DrawPrimitive
"candidates" in the log line up with textured menu **font glyphs**,
not the wider red icon. The decomp PCShell function is a *hypothesis* for
the gameplay ESC selector, not yet established by the drawing evidence.

Do NOT make cursor width changes from a guessed owner, texture size
or from the previous `CURSOR-CANDIDATE`/ `SPIDER-POSE` names.
Those labels never proved object identity. Continue to keep the owner
experiment disabled when collecting diagnostic frames.

The separate `SELECTOR-SHAPE` probe, enabled **only by the existing**
`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1` and `[Debug] Log=1`,
records source 2D quads with roughly **78–170px width** and
**52–130px height** in the central ESC menu region. Both ordinary
and indexed primitive calls and both supported vertex formats are
eligible. Crucially, the first **32 independent matches per frame**
are logged even when the previous 100 shared font candidates already
filled their quota. The metadata includes original quad bounding box,
native caller RVA, texture dimensions and COM identity, diffuse color,
UV0, and Z/RHW, on bounded F9-sampled frames 1/5/12/20.
No render vertices or input are changed by this sampler.

For next run, enable `SPIDERMAN2K1FIX_NATIVE_ESC_43=1` and
`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1` but **omit**
`SPIDERMAN2K1FIX_NATIVE_CURSOR_43=1` (the owner hypothesis is
currently unsupported). Take F9 screenshots of the same ESC and
REALLY QUIT? positions while leaving each state visible for seconds,
and provide the log. Look for `PAUSE-NATIVE-PROBE: SELECTOR-SHAPE`.
If no candidates match the screenshots, search a different draw path
(DirectDraw surface Blt or different 2D geometry); do not patch the
menu based on an unverified guess.

## Test the passive diagnostic (Wine or native Windows)

Use both DLLs from the same build on the diagnostic PR branch.
In `SpiderMan2k1Fix.ini`, enable:

```ini
[Debug]
Log=1
```

Set the game process environment variable
`SPIDERMAN2K1FIX_PAUSE_MENU_TRACE=1` (in Lutris, configure the
game's **System options → Environment variables**, or export the
variable before launching the game through the same environment).

1. Launch at 1920×1080, **AspectRatio=Auto**.
2. Begin a level and **hold Esc for about half a second** to make the
   physical key-down observable at a Flip. Let the game's original native
   CONTINUE / RESTART LEVEL / QUIT menu open; do not use F10 for this step.
3. Hold the CONTINUE / RESTART LEVEL / QUIT menu for two seconds.
   Press **DOWN** once, release it, then select another row with
   **DOWN**. Leave each new selection visible for a second.
   Move to QUIT and **hold Enter for half a second** to open the
   REALLY QUIT? / NO / YES confirmation menu; leave the confirmation
   visible for two seconds. Alternatively, hold Space if that is
   your actual confirmation key. Do not click YES to end the session
   before the samples have been recorded.
4. Press **DOWN** to move the red selector from NO to YES without
   confirming, then **UP** to return to NO. Leave each selection
   visible for a second, then choose NO to go back to gameplay.
5. Quit normally and preserve `SpiderMan2k1Fix*.log`.
6. Search for `PAUSE-NATIVE-PROBE: ESC-EDGE`, `KEY-EDGE`,
   `BEGIN`, `SAMPLE-FRAME`, `FRAME-END`, `caller_rva=` and `END`.
   If no ESC-EDGE is present, the physical
   ESC key was not observed in a Flip. If ESC-EDGE appears but BEGIN or
   candidate draws do not, the chosen D3D7 path/filter may be incorrect.

The diagnostic observes a maximum **24-Flip window** after each of
up to four ESC edges, plus at most **six** further short windows
per ESC from physically observed Enter/Space/Up/Down key-down edges occurring
within 1800 Flips. It counts eligible geometry candidates in the
first 20 frames of each window and logs `FRAME-END candidates=` for
each frame. These key edges are not proof that a menu was active.
It records bounded details at **sample frames 1, 5, 12 and 20**:
the first eight candidates and every 64th candidate, plus up to
**100 geometric CURSOR-CANDIDATE sprites per sampled frame**, up to
8192 eligible primitives. It no longer relies on the historical
3140–3400 draw indices. In addition, it reports up to twelve candidate
**wide** screen-space quads in the middle of each sampled frame,
even outside the numbered range. These are provisional panel candidates only. This reveals
potential end-of-frame menu primitives without logging every world
triangle. The early frame's x/y pairs may be **projected 3D scene
geometry, not menu positions**; do not apply a 4:3 transform from
these readings until the overlay has been independently identified.
It does not modify rendered coordinates, pause input, F10, F9,
the camera or texture contents. If no BEGIN appears,
the assumed native pause-context gate needs new passive research;
**do not enable a geometry rewrite based on guessing**.

Optional comparison: repeat once in **AspectRatio=4:3** and compare
the same native pause draw signatures with the full-width run. The
screenshots alone establish the desired visual reference, not the
individual reliable vertex signatures.

Finally, remove the environment variable; the diagnostic is
**OFF by default** and requires `Log=1`.
