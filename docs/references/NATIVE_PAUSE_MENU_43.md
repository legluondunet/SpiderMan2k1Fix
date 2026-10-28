# Native gameplay ESC menu: 4:3 presentation in widescreen

The original **Spider-Man PC (2001)** 3D gameplay ESC screen has a
full-screen translucent orange/blue geometric motif, a purple options ribbon,
text, and a red spider-shaped selector. These are 2D Direct3D7 primitives
drawn over the 3D world. In 1920×1080 Auto mode the original 4:3 artwork
was stretched horizontally.

The corrections were **visually validated under Wine on 2026-10-24**:
the ribbon, its outline and labels, translucent motif, background violet tint
(including both screen edges), and cursor keep their intended proportions.
Both the main ESC choices and the **REALLY QUIT? → NO / YES** dialog were
tested. The normal gameplay HUD, compass and 3D scene are unaffected.

## Activation / supported scope

The player INI no longer exposes a HUD switch. Widescreen HUD and the three
native ESC 4:3 corrections are **permanently requested**, subject to the
verified executable, the game stage and the successful full native HUD patch.
The original `4:3` gameplay presentation retains its original geometry; the
widescreen transform is not needed in that case.

Historical development toggles `HUDWidescreenFix`, `NativeESC4x3`,
`NativeMotif4x3`, and `NativeCursor4x3` are accepted only so existing INIs
can be migrated safely; **they no longer disable these features**. The old
`SPIDERMAN2K1FIX_NATIVE_ESC_43`, `SPIDERMAN2K1FIX_NATIVE_MOTIF_43`, and
`SPIDERMAN2K1FIX_NATIVE_CURSOR_43` environment overrides are likewise inert.
The code still refuses unsafe changes if the native HUD hook or original
D3D7 geometry signatures do not match.

The motif and selector signatures were visually calibrated at
**1920×1080 Auto** (borderless) under Wine. The patch also recognizes
**1920×1200 Auto (16:10)** using the same native owner/FVF/Z/RHW and
texture signatures. User-supplied native Windows screenshots and log
(2026-10-24) now confirm that the new 16:10 branches execute and render;
the maintainer also reports successful Windows and Wine tests with no
regressions observed. These reports supplement, rather than replace, the
specific Windows screenshots and logs below. Other resolutions are deliberately left unchanged
by the motif and selector correction until the signatures are measured.
Both are disabled in main menus and classic 4:3 gameplay.

## Implementation details / safety

The hooks edit **copies of D3D7 screen-space vertices**, never engine-owned
vertex data or input state. Native `DrawPrimitive` and exact owner RVA
`0x00102C72`, FVF `0x144`, 4-vertex triangle fans, texture dimensions,
Z/RHW and current render mode distinguish the ESC UI from the world and HUD.

- **Ribbon:** Match the wide untextured pause band, then consider only
  exact 8x8 border textures and text glyphs in the *same frame*.
  Empirical outline calibration gives the original 4:3 width
  (approximately 1440px at 1920×1080).
- **Motif:** Match *untextured* 2D alpha-0x80 primitives at Z≈0.00146,
  RHW≈9.988. Scale X around display center by **(4H/3W)**, preserving Y:
  0.75 at 1920×1080 and 0.833333 at 1920×1200. Keep original 3D
  world vertices untouched.
- **Violet tint:** Compressing motif primitives would otherwise narrow the
  native full-screen darkening. Replay original triangles **only** in the
  two computed side gutters. At 1920×1080 those are x=0..240 and
  x=1680..1920; at 1920×1200 they are x=0..160 and x=1760..1920.
  Clip and interpolate Z, RHW, diffuse colors and UV under the game's
  original blend state. Never overdraw the central 4:3 region.
- **Red selector:** Exact game-owner/texture/Z/RHW/white-color/same-frame
  matching remains mandatory. Its 1920×1080 quad is **96×72** and is
  narrowed to 72×72 around its own center. At 1920×1200, recognize either
  **96×80** (height-scaled) or **96×72** (legacy-height) and transform its
  X coordinates by **actual quad height / width**. Thus the visible square
  remains 80×80 or 72×72 without shifting vertically. This is scoped to
  the ESC native cursor, not the gameplay HUD or arbitrary textured quads.

Diagnostics from research builds were intentionally removed before merging.
When troubleshooting, enable `[Debug] Log=1` to obtain limited native
ESC matching and gutter replay records; F9 captures PNG screenshots.

## Regression checks

**Baseline:** Check in Auto 1920×1080:

1. Normal 3D play shows no overlay; the HUD and compass retain their 4:3
   proportions and widescreen world rendering.
2. Open **ESC**: violet darkening reaches both physical screen edges, orange
   motif and purple panel are centered and not horizontally stretched.
3. Move the red icon near the center, the top of the screen, and near the
   right edge: width and height keep the same ratio everywhere.
4. Confirm **QUIT → REALLY QUIT?**: the **NO / YES** dialog, banner,
   motif and selector retain their proportions.
5. Return to the main menu and try 4:3 gameplay mode: neither should be
   affected by the experimental native ESC Auto fix.

**New 16:10 visual comparison:** repeat the same ESC and
**REALLY QUIT?** tests at **1920×1200 Auto**, particularly the orange
motif, edge tint, panel border, headings and red selector at center,
top and right edge. The logical 4:3 area should be **1600×1200** centered
at x=160..1760, while the physical window and 3D scene remain
1920×1200. Compare against 1920×1080 screenshots; do not assume that
passing static tests proves the on-screen motif/selector signatures match.

### Observed Windows 11 runtime test (2026-10-24)

Two game F9 captures from a **1920×1200 Auto** gameplay ESC menu
(`scrn0009` and `scrn0010`, including the **REALLY QUIT?** state),
together with `SpiderMan2k1Fix(20261024-202129).log`, verify:

- The same monitored screen uses 1920×1200 borderless output; the original
  main-menu request 640×480×16 is intercepted. The game viewport remains
  physical 1920×1200 for 3D.
- `PAUSE-NATIVE-43: ribbon/border/glyph` executes with the **0.83333**
  X-only 4:3 scale.
- `NATIVE-MOTIF-43: QUAD-MATCH` records the original translucent
  alpha-0x80 motif with Z≈0.00146 and RHW≈9.988; its
  `GUTTER-REPLAY` logs confirm dynamic clips
  `(0..160,1760..1920)`.
- `NATIVE-CURSOR-43: QUAD-MATCH` confirms an observed **96×80**
  native selector and an **80×80** output with scale 0.83333; successful
  matches occurred at center and near the display edges.
- The panel text and the QUIT confirmation remain centered and legible
  in the user screenshots; no visible 640×480 window or major border
  misalignment. The large orange/blue motif remains a prominent native
  design feature, so a screenshot alone is not a proof of the artist's
  intended geometry.

**Additional maintainer validation (2026-10-24):** Windows and Wine
runtime tests are reported **successful, with no regressions observed**.
This closes the previously requested Windows/Wine non-regression check by
maintainer report. The supplied screenshots and the detailed ESC draw log
specifically document Windows at 1920×1200; no separate Wine log or
screenshots were provided with this confirmation, so other resolutions,
DPI setups and hardware cannot be independently certified from the files.

**Merge status:** PR #47 remains unmerged pending explicit approval.

**Classic 4:3 regression:** verify no additional x-compression is applied
when `AspectRatio=4:3` is selected. Keep Bink/video/main-menu 4:3 and
the 3D HUD unchanged.

Automated source and clipping tests live in
`tests/test_native_pause_menu_probe.py`. The final F9 Wine visual checks
were performed by the project maintainer.

## Long save-confirmation title — regression under investigation

A new Wine screenshot dated 27 October 2026 shows the original
`SAVE GAME PROGRESS ?` heading rendered with very large gaps,
particularly between `SA` and `VE`, and immediately before `?`,
while the purple confirmation panel itself remains well proportioned.

**Likely cause from source inspection:** the earlier native ESC text-quad
classifier was limited to glyphs with `left > 25% W` and
`right < 75% W` (central 50%). Unlike the short previously tested
`PAUSED` and `REALLY QUIT?` titles, `SAVE GAME PROGRESS ?` reaches
beyond those X limits; outer font quads can remain at original 16:9
coordinates while middle ones are transformed to 4:3, creating exactly
such letter-spacing discontinuities. This is a strong static explanation,
**not yet a verified runtime diagnosis**.

Proposed correction on `fix/native-save-dialog-title-spacing`:
keep the existing central 50% for choices; for the title strip only
(`top >= 16% H`, `bottom <= 34% H`) extend glyph eligibility
to `left > 18% W`, `right < 82% W`. All native SHA-gated
stage/4:3 exclusions, exact original D3D7 caller, FVF, triangle
fan, four vertices, texture dimensions, Z/RHW, same-flip purple
ribbon, glyph size and the X-only vertex copy are unchanged.

A host-compiled geometry test covers the actual C helper at
1920x1080 and 1920x1200. Passing these tests **does not prove
in-game correction**; visual tests must revisit `SAVE GAME PROGRESS ?`,
`PAUSED`, `REALLY QUIT?`, all NO/YES choices, the red selector,
4:3 mode and the gameplay HUD on Windows/Wine before merging.

## Credits

legluondunet assisted by ChatGPT (OpenAI)
