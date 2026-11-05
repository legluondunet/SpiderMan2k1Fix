# HUD 3D cinematic / aiming / level-2 native ESC — combined visual fix

Status: **staged compass correction, level-2 native ESC depth signature fix, and reticle diagnostics**.
Do not claim all three issue classes are fixed until the runtime capture
and regression test below have been reviewed. The native Pause correction
is only source/CI tested so far.

## Player reports (2026-11-04)

Reference images supplied by the project maintainer, all 1920×1080:

- **HUD-01 (compass):** second 3D cutscene and aiming sequences show
  horizontally stretched compass artwork/indicator. Do not alter the already
  correctly full-width 3D cinematic letterbox bars.
- **HUD-02 (aim):** green and red aiming crosshairs/rings have a wider-than-tall
  outline (screenshots `scrn0008.png`, `scrn0009.png`).
- **HUD-03 (native pause):** the level-2 `PAUSED` ribbon, red/blue motif,
  title and selector do not all show the same restored 4:3 presentation
  previously validated in other gameplay; see `scrn0020.png`. The
  maintainer provided the Dreamcast 4:3 reference
  `Capture d’écran_2026-11-01_10-46-44.png`.

## Code investigation

- `hud_widescreen.c` repairs the player life/web icon, bars and text with
  known CALL-site signatures. It does **not** implement generic reticle
  rendering.
- `hud_adaptive.c` matches the composite compass using its known original
  D3D7 Z/RHW and mirrored 64×64 texture halves; the previous right/bottom
  position cutoff (min Y > 70%, center X > 75%) excluded the observed
  higher/left-shifted cinematic/aim presentations. This branch widens **only
  the positional window**, retaining the exact content/depth/texture/UV
  signatures and right-edge X mapping. This is a code-level correction,
  not yet proven in the user's 3D cutscene.
- The native ESC overlay is a different renderer path in
  `spiderman2k1fix.c`. Its same-Flip wide untextured ribbon signature
  unlocks the border/title matcher; motif and cursor have separate strict
  conditions. Level 2 is **not excluded explicitly** in the source.
  **Verified full-resolution image evidence (scrn0020 and scrn0021):**
  the level-2 purple panel's left-edge border is y=398..681 at 1920×1080,
  and x=0..1919 across the full screen. The earlier y=342..585 estimate
  resulted from measuring a *scaled image preview*, NOT actual pixels.
  The ribbon bottom is ~63.1% of screen height; it ALREADY satisfied
  the original >=55% threshold. The replacement >=50% threshold did not
  fix this defect and has been reverted for safety. The Dreamcast reference
  instead shows a narrower, centered panel (~1480px wide).
  The persistent failure is likely one of the independently verified
  D3D7 draw conditions: object owner/caller, Z/RHW, bound texture,
  stage/HUD hook initialization or same-frame ribbon classification.
  **Do not loosen these guards without runtime evidence.**
  The new `native_pause_43_policy.h` derives a centered 4:3 X mapping
  from actual W/H for all safe widescreen outputs, and the associated
  border/glyph/cursor bounds scale with display size. The 3D scene and
  cinematic bars are not changed. **scrn0021 visually fails this acceptance
  check** despite a green CI; the new model is not yet reaching the banner.
  An altered caller, Z/RHW, texture or stage could still reject the fix;
  if so the same log's `HUD-VISUAL: PAUSE-RIBBON` samples will tell us why.
- Red/green reticle signatures have **not been measured**. They must not
  be corrected just because a quad is colored or elliptical: the shared
  D3D7 caller draws unrelated 3D primitives as well.

## Level-2 trace resolution (SpiderMan2k1Fix 2026-11-04 22:09–22:10)

The full `SpiderMan2k1Fix(20261104-211836).log` from GE-Proton11-12
contains five `HUD-VISUAL: PAUSE-RIBBON` samples. The strongest are:

- Flip **826**: native original caller `0x00102C72`,
  quad **x=-7.5..1923.8, y=400.5..675.0**,
  `Z=0.00122`, **`RHW=8.9615`**, diffuse `7F3232A0`,
  **no bound texture**. Repeats at Flip **1024**.
- Flip **934**: same owner and original depth, alternate animated band
  y=463.5..607.5, **same RHW=8.9615**.
- The active native ribbon matcher expected `RHW≈11.176` (tolerance
  ±0.25), so the **depth signature rejected** this complete level-2
  variant; the viewport, color and geometry did not explain the failure.
- The original motif also differs: repeated untextured alpha-0x80
  samples at **`Z=0.00146`, `RHW=7.8952`**, e.g. quad
  x=1245.0..1511.2, y=904.5..1053.0, as opposed to legacy
  `RHW≈9.988`. The broad AIM sampler mislabels these motif candidates;
  they do not prove any actual red/green crosshair identity.

The PR now accepts **both exact observed RHW families** via the pure
`SpiderNativePause43_RibbonLayer` and
`SpiderNativePause43_MotifLayer` helpers, only after the original
caller, primitive/FVF, bounding box, texture, Z, alpha and stage guards.
No intermediate RHW values are accepted. All other game rendering is
unchanged.

A new read-only `PAUSE-NATIVE-DETAIL` log is emitted **only after a
verified ribbon** for up to **80** original font/border/selector
candidates. It reports their original quad coordinates, depth,
texture, format and whether the existing hook corrected them. This is
needed because the log contains *no* previous
`PAUSE-NATIVE-43: ribbon hit` and the new session's
text/selector Z/RHW signatures are not yet known.

**Runtime status:** pending visual verification with a new CI build.
The user's previous screenshot `scrn0021.png` belongs to the older,
now diagnosed build and is therefore still a genuine failed test.

## Second validation: scrn0023.png and 22:31:52–22:33:20 log

The maintainer's next 1920×1080 screenshot `scrn0023.png` is an
**intermediate result**, not acceptance: the violet **fill** now occupies
the middle 4:3 region, but the brighter horizontal and vertical **border
strokes** still span the full 16:9 width. The native lettering also
remains wide. The red/blue motif is now caught by the new level-2 matcher.

The accompanying `SpiderMan2k1Fix(20261104-213332).log` establishes:

- `PAUSE-NATIVE-43: ribbon hit=1` on Flip **776**, then repeated hits
  through at least Flip **2046**, `scale=0.75000`, Z=`0.00122`,
  RHW=`8.9615`. **The banner correction is working.**
- Numerous `NATIVE-MOTIF-43: QUAD-MATCH` records use the verified
  **RHW `7.8952`** signature. The motif correction is also active.
- **Zero** `PAUSE-NATIVE-43: glyph` or `border` hits, and no
  `NATIVE-CURSOR-43: QUAD-MATCH` while these screenshots were taken.
  Do not infer selector failure from a screenshot that lacks a
  red moving selector.
- All 80 `PAUSE-NATIVE-DETAIL` samples reported `corrected=0`.
  The real foreground/shadow text quads have distinct exact pairs:
  **Z=`0.00122`, RHW=`8.9615`**, and
  **Z=`0.00098`, RHW=`10.3609`**, with a 16/32x32 font atlas.
  Previous code exclusively allowed the legacy
  **11.176 / 12.686** pairs and therefore never reached its 4:3
  text scaling.
- The detail probe's original 15–85% central X filter excluded
  the left/right 8x8 borders. Their actual RHW is still unknown.
  It would be incorrect to claim that the text samples also prove
  the border's exact depth.

**Follow-up in the same PR:** add a separate pure, host-tested native
`SpiderNativePause43_TextLayer` accepting only the original and
observed pairs, without mixing Z/RHW combinations, and update the strict
font pass. The 8x8 border pass additionally permits the independently
verified new ribbon Z/RHW pair, while keeping its existing caller,
texture, geometry, stage and same-Flip checks. A new **bounded 40-sample**
`PAUSE-NATIVE-BORDER-PROBE` records edge-only 8x8 primitives after the
known-good ribbon if the border remains unmatched. It does not alter
vertices or state. Original 3D, movie, comic and gameplay HUD paths
remain unchanged.

**Acceptance remains pending**: collect one new F9 of native Pause and
its matching `[Debug] Log=1` journal. Confirm no full-width border
and native text proportions against the Dreamcast reference. The
red/green reticle and cinematic-compass tests are still pending.

## Third validation: exact Dreamcast menu-panel extent (2026-11-04)

The next captured `scrn0024.png` and
`SpiderMan2k1Fix(20261104-214925).log` confirm all strict native
Pause rendering paths now match the *actual* level-2 game drawing:

- `PAUSE-NATIVE-43: ribbon`: recognized from Flip **1353** with
  scale **0.75000** and exact level-2 Z/RHW.
- `PAUSE-NATIVE-43: border`: recognized repeatedly, including both
  horizontal `8x8` strokes and left/right caps. The edge
  `PAUSE-NATIVE-BORDER-PROBE` records `corrected=1`.
- `PAUSE-NATIVE-43: glyph`: 16/32/64x32 original menu characters
  recognized and transformed in both text layers; central probe
  records `corrected=1`.
- `NATIVE-MOTIF-43: QUAD-MATCH`: original translucent motif matched.
  The native red selector was **not shown** during this level's
  screenshots; absence of a selector hit is not evidence of a defect.

Both original PNGs have an actual **1920x1080** image size. Bright
violet outer horizontal border pixels were measured at approximately:

| Original screenshot | Left border | Right border | Visible width |
| --- | ---: | ---: | ---: |
| Dreamcast reference `Capture d’écran_2026-11-01_10-46-44(1).png` | x=223 | x=1702 | 1480 |
| Fix before this calibration `scrn0024.png` | x=241 | x=1679 | 1439 |

Thus the **previous empirical 1439px calibration actually made the
native Pause border 41px too narrow** compared with the Dreamcast
reference; the initial assumption that a 1439px border was correct was
reversed after comparing the original two full-size captures.

The source 8x8 border's real extreme X coords (`-22.5` and `1950.0`)
mapped with the **standard centered 4:3 factor** give
`x=223.125` and `x=1702.5`: the Dreamcast target without any special
pixel correction. Therefore remove both **border-specific** factor
`1439/1479` and **ribbon-specific** `1439/1448.5` from the
1920x1080 native renderer. Keep the already verified menu **glyph
mapping unchanged**, keep all original strict caller/texture/depth
guards, leave the translucent motif and the rest of the game untouched.
Portable C-level geometry regression checks cover the 1920x1080 native
border reference in addition to earlier 4:3, 16:10 and 16:9 cases.

The Dreamcast capture is from a *different level*; its bottom-right
compass is not necessarily shown during another level's Pause. The
Dreamcast `A SELECT` prompt is intentionally postponed to a later
version. **Neither is a PR #109 acceptance criterion.**

**Acceptance of this final 41px geometry adjustment awaits an
in-game screenshot**. Automated tests can assert the calculated
coordinate mapping, but not the resulting pixel appearance.

## Fourth validation: aiming compass PASS, red/green reticles FAIL

The maintainer has **visually accepted the compass in active aiming** as
well as in 3D cinematic #2 (2026-11-04), and **confirmed both red and green
aiming circles remain stretched**. Three original lossless 1920x1080 PNGs
were supplied:

| Image | Ring color | Approx. colored outline | Width / height |
| --- | --- | --- | --- |
| Dreamcast `Capture d’écran_2026-11-01_10-48-50(1).png` | green | x=877..1039, y=556..717 → 163×162 | 1.006 |
| Wine `scrn0029.png` | green | x=932..1025, y=348..417 → 94×70 | 1.343 |
| Wine `scrn0028.png` | red | x=569..721, y=682..794 → 153×113 | 1.354 |

The Dreamcast image and the Wine images are **different game views**.
The sizes and scene positions are not meant to match each other. The
**circle aspect ratio**, however, should remain 1:1 regardless of each
reticle's size or on-screen location.

A local source-agnostic geometry function
`SpiderHudAim_MapX(x, object_center_x, width, height)` predicts the
inverse of the 4:3→16:9 stretch: a 0.75 width factor around the ring's
**own center**, not the screen center. It predicts:

- Green: 94×70 → 70.5×70, ratio 1.007
- Red: 153×113 → 114.75×113, ratio 1.015

This geometry rule was initially host-tested without altering any D3D7
draw. **It was subsequently connected to the strict live reticle hook**
only after the matching gameplay log recorded the original caller, FVF,
texture, UV, Z/RHW and tint for both screenshot moments. Other world
quads must never be rescaled by appearance alone.

The read-only `HUD-VISUAL: AIM` candidate sampler includes the
smaller 94×70 green reticle and excludes prior known player-HUD and
native Pause false positives. **The matching Log=1 journal has now been
received and identified both rings**, and the conditional live D3D7
correction is staged in the PR. Its next gate is visual acceptance.

## Fifth validation: precise red/green aim signatures and active correction

Received original `SpiderMan2k1Fix(20261104-224539).log` from the **same**
GE-Proton11-12 session that produced both supplied 1920x1080 F9 captures.

**Direct3D7 ring matches, verified against screenshot timestamps:**

| Input capture | Original log rectangle | ARGB diffuse | Z | RHW | Bound texture |
| --- | --- | --- | --- | --- | --- |
| `scrn0028.png` red (23:27:19) | x=566.2..723.8; y=679.5..796.5 | `B0FF4040` | `0.00146` | `9.9878` | 64x64 |
| `scrn0029.png` green (23:27:27) | x=930.0..1027.5; y=346.5..418.5 | `B040FF40` | `0.00146` | `9.9878` | 64x64 |

Both also share the original D3D7 DrawPrimitive caller `00102C72`,
triangle-fan FVF `0x144`, and initial UV `(0,0)`. The log explicitly
records each corresponding **lossless F9 PNG saved**. The red sprite
moved to other coordinates while retaining the same renderer signature,
so it cannot be corrected using a fixed screen position.

The earlier `hud_aim_geometry.h` factor is now enabled **only** after
matching the exact source-backed textured/tinted Z/RHW/game-stage
signature in `hud_aim_reticle.h`. The game's original source vertices
remain untouched; their X is copied to a scratch quad and compressed
about the **object's own center**. The 64x64 bound texture, tint, layer,
caller, primitive type and measured sprite aspect must all agree.
Compass/motif/pause/instructions and other game geometry cannot pass
these collective strict predicates.

Three precautions are intentional:

1. We preserve the ring's center and Y coordinates; correcting around
   the screen center would move the actual target.
2. Native 4:3 is a strict no-op, and 16:10 uses its derived 5/6 scale.
3. The code preserves the validated native Pause / compass path order,
   requires the SHA-pinned HUD patch, and never changes texture or
   D3D7 render state.

New host-compiled exact-signature regression tests verify both
colors, both screenshot bounding boxes, movable red aiming position,
uniform-depth/color checks and false-positive exclusions.
All non-X attributes (Y, Z, RHW, tint and UV) remain byte-identical.

**Initial visual test:** level 1 accepted; level 2 still failed.
The 2026-11-05 follow-up below identifies the new RHW=7.8952 variant
and stages a targeted revision. Only level-2 visual acceptance remains.

## Sixth validation: level-dependent red/green ring depth (2026-11-05)

The maintainer tested the first source-verified live fix and supplied
`scrn0030.png` (green, level 1) and `scrn0034.png` (red, level 2)
with the **same session's** `SpiderMan2k1Fix(20261104-235727).log`.

**Result:** level-1 aiming rings visually **PASS** (green is round);
level-2 rings **FAIL** (red remains horizontally stretched).
This is a distinct rendering-depth variant, not a regression in the
0.75 X-scale factor.

From the matching Log=1 and screenshot moments:

| | Level 1: F9 scrn0030 at 00:55:19 | Level 2: F9 scrn0034 at 00:56:55 |
| --- | --- | --- |
| Red/green caller | `00102C72` | `00102C72` |
| Primitive / FVF | triangle fan / `0x144` | same |
| Bound texture | `64x64` | `64x64` |
| Z | `0.00146` | `0.00146` |
| RHW | **`9.9878`** | **`7.8952`** |
| Colors | red `B0FF4040`, green `B040FF40` | same |

The log records 13 `HUD-AIM:` correction events for **RHW
9.9878** but none for **7.8952**, while read-only
`HUD-VISUAL: AIM` records show both **red** and **green** ring
candidates on the 7.8952 layer (samples 25–39, Flips 3875–4687).
Specifically:

- Green at Flip 3875: x=832.5..1080.0, y=310.5..495.0,
  Z 0.00146, RHW 7.8952, diffuse B040FF40, texture 64x64.
- Red at Flip 4687, immediately preceding F9 `scrn0034`:
  x=1110.0..1357.5, y=265.5..450.0,
  Z 0.00146, RHW 7.8952, diffuse B0FF4040, texture 64x64.

The existing exact prefilter accepted only RHW ~9.9878, and therefore
rejected **all** second-level rings. We now recognize the **two
discrete, independently verified signatures** with narrow tolerances,
not a broad 7.9..10.0 interval. The validated tint, UV, texture,
primitive, Z, original caller, gameplay gate, per-vertex agreement,
relative bounds and shape guard remain unchanged; native Pause motifs
also sometimes have RHW 7.8952 but are excluded by their entirely
different color/texture signature.

Portable host tests now check the level-2 red and green original
quads, a preserved circle center, rejection of intermediate RHW=8.9,
the original 9.9878 source layer, incorrect per-corner depth, native
Pause tint, and untouched original 4:3 output.

**Status:** updated fix in PR #109, **pending one visual level-2
acceptance check**. Level-1, compass and Pause menus remain validated
and need no repetitive regression playthrough.

## Seventh validation: Rhino boss reticle, native Pause AND boss life UI

The next maintainer test uses **Spidey vs Rhino** in the same 1920×1080
GE-Proton11-12 run. All screenshots and logs are original files:

- `image(20261105-000716).png`: Dreamcast reference, corrected
  aspect ratio for the **Rhino life bar, round Rhino portrait, red ring**.
- `scrn0035.png`: Wine **native Pause** still full-width.
- `scrn0036.png`: Wine **red aiming reticle** and **Rhino life bar and
  portrait** stretched sideways.
- `SpiderMan2k1Fix(20261105-000954).log`: same run, beginning at
  2026-11-05 01:00:02, captures scrn0035 at **01:01:09** and
  scrn0036 at **01:01:23**.

The native draw signatures changed AGAIN while retaining the original
source caller `00102C72`, texture, color and geometries:

| Native HUD object | Z | Rhino RHW | Known earlier RHW |
| --- | ---: | ---: | ---: |
| Pause purple ribbon `7F3232A0`, no texture | `0.00122` | **`6.8041`** | `11.176` / `8.9615` |
| Pause motif (alpha `80`), no texture | `0.00146` | **`5.9130`** | `9.988` / `7.8952` |
| Red aim ring `B0FF4040`, texture 64×64 | `0.00146` | **`5.9130`** | `9.9878` / `7.8952` |
| Rhino boss **32×32 frame** `FFFFFFFF` | `0.00024` | **`17.1292`** | other bosses `21..22` |

Exact Rhino red ring samples in the log:
- Flip 2051: x=877.5..1035, y=679.5..796.5;
- Flip 2096 (shortly before scrn0036): x=637.5..832.5,
  y=652.5..796.5. Both have Z/RHW `0.00146/5.9130`,
  red diffuse `B0FF4040`, bound 64×64 texture, original UV0=(0,0).
The existing strict `hud_aim_reticle.h` rule rejected this third RHW
family. The PR now adds the exact **5.9130** case alongside the
previous two, with identical unchanged color/UV/texture/shape/owner
checks and X-only compression around the **individual reticle center**.

The fullscreen Pause ribbon is observed at Flips 1399, 1489 and 1579:
x=-7.5..1923.8, typical y=400.5..675.0, Z/RHW
`0.00122/6.8041`, no texture. The PR likewise adds this third
exact untextured ribbon signature, and the matching motif `5.9130`.
Since original 8×8 border segments have the same ribbon depth in
earlier levels, the exact 6.8041 depth is also allowed for borders
and dark text. **The brighter foreground text depth in Rhino
is NOT captured yet** and must be confirmed from the existing
`PAUSE-NATIVE-DETAIL` probe once the new ribbon matches. In-game
Pause is NOT yet considered repaired.

### Rhino life bar and portrait — a separate composite HUD bug

The Dreamcast and Wine original pictures also reveal that Rhino's
**health bar and portrait are stretched horizontally**:

| Element | Dreamcast approx. X | Wine original approx. X |
| --- | --- | --- |
| Boss bar | 1037..1508 (~471px) | 1062..1694 (~632px) |
| Boss portrait | 1505..1626 (~121px) | 1684..1848 (~164px) |

Their heights are close; the portrait is round only in the Dreamcast
reference. The desired *entire group* mapping, including position,
is the standard screen-centered 4:3 transform:
`x_corrected = 960 + (x_original - 960) × 0.75`.
This maps the Wine boss bar to ~1037..1511 and the portrait to
~1503..1626, almost exactly the reference. **This is not the
reticle's object-centered mapping.**

The already installed `hud_adaptive.c` boss branch uses an older
right-edge-anchored transform for earlier boss scenes, with strict
portrait/frame `RHW=19..26`. In Rhino, the original **32×32
frame piece** was logged at x=1590..1687.5, y=117..189,
Z=`0.00024`, **RHW=`17.1292`**, tex=32×32,
diffuse `FFFFFFFF`, and therefore is rejected by its old
prefilter. The *original fill and large portrait primitives* were
not covered by the earlier broad HUD-VISUAL sampler, so their
exact original Z/RHW/texture signatures cannot yet be asserted.

A separate read-only `HUD-BOSS-PROBE` now records up to 18 draws
per kind: **fill / 32×32 frame / 64×64 portrait**, reporting
original caller, coordinates, texture, color, Z/RHW, UV and whether
the prior adaptive hook transformed it. This diagnostic changes
**no vertices or D3D7 states** and is deliberately bounded.
The boss health bar and portrait correction will NOT be enabled
until these three layers are observed and safely classified,
to avoid breaking the previously supported Venom/Scorpion HUD.

**Next useful check:** run **just Rhino** with the next CI ZIP,
take one F9 screenshot with the red ring and life bar, then one
with ESC Pause, and send **one Log=1** journal. The new reticle
and menu banner signatures can be assessed visually while the
separate boss probe captures the missing composite parts. Earlier
validated levels and the boussole do not require repetitive tests.

## Eighth validation: full Rhino boss HUD sources identified (2026-11-05)

Original maintainer `scrn0042.png` (1920×1080) and same-run
`SpiderMan2k1Fix(20261105-003640).log` now show the **red crosshair
circular** at the Rhino fight. The active `HUD-AIM: red` draw hits
compress x from (877.5..1038.8) to (897.7..1018.6), scale=0.75000,
Z/RHW `0.00146/5.9130`, while leaving the target center unchanged.
The red crosshair is **visually PASS** at this scene.

**The life bar and portrait remain uncorrected in this test.**
`HUD-BOSS-PROBE` captured **18 independent samples for each of
fill/frame/portrait** (54 observations total), all `corrected=0`.
Original source D3D7 caller is `00102C72`, fan/FVF `0x144`.

| Layer | Original 1920×1080 quad | Z / RHW | Texture / first color |
| --- | --- | --- | --- |
| Colored fill (yellow half) | x=1383.8..1691.2, y=130.5..166.5 | 0.00098 / **8.0113** | none / FFFFFF00 |
| Border tile | x=1590..1687.5, y=108..180 | 0.00024 / **17.1292** | 32×32 / FFFFFFFF |
| Rhino portrait | x=1680..1848.8, y=72..211.5 | 0.00024 / **17.1292** | 64×64 / FFFFFFFF |

No older boss `HUD-ADAPTIVE: region=boss` draw matched, because
those original Venom/Scorpion source layers differ. The new separate
`hud_boss_native.h` strict classifier/renderer is now installed
*after* the old adaptive HUD so the previous boss behaviors retain
priority. Each logged source family requires the exact original
D3D7 caller, 4-vertex fan/FVF, screen region, Z/RHW, texture size,
alpha/white portrait tint and per-corner layer agreement.

For this specific render variant, apply the **same screen-centered
mapping to every bar/portrait component**:

`Xout = screenWidth/2 + (Xin - screenWidth/2) × 4×screenHeight/(3×screenWidth)`.

For 1920×1080, that predicts:
- actual observed yellow fill right: **1277.85..1508.40**;
- frame tile: **1432.50..1505.625**;
- portrait: **1500..1626.6**.

This aligns with the earlier native Dreamcast reference (full
bar ~1037..1508, portrait ~1505..1626), unlike the old right-edge
boss mapping. Pixel Y, Z, RHW, texture, UV and color remain intact;
native 4:3 remains a no-op.

**Important partial-damage caveat:** the most recent sampled fill
was the opaque yellow half; the left green gradient and separate
black missing-HP mask were not individually captured by the old
single-fill-per-45-Flips diagnostic. The strict renderer recognizes
opaque green fill *only when its layer matches* the observed yellow
fill, but does **not** guess the unknown black damage mask's Z/RHW.
The read-only probe now keeps independent bounded buckets for
`fill-black`, `fill-green`, `fill-other`, frame and portrait.
If the missing-health segment detaches under gameplay, its new
precise draw signature will be available for a follow-up correction
without guessing or increasing world-draw exposure.

Tests compile the real portable C helper and verify all 3 original
quads, exact Dreamcast-projected X values, preservation of all
non-X bytes, green gradient, 16:10 mapping, 4:3 no-op and exclusion
of earlier boss/UI/world sprite signatures.

**Status:** active code patch staged in PR #109, waiting for one short
Rhino screenshot/Log=1 hardware check of both HP group and Pause.
The same recent run saved `scrn0041.png` for Pause but that PNG
was not among the user's new attachments; the Rhino Pause outcome
is **not** visually established in this round.

## Ninth validation: Rhino boss bar/portrait PASS; Pause face glyphs still FAIL (2026-11-05)

Two **original native 1920x1080 F9 PNGs**, saved in the same run as
`SpiderMan2k1Fix(20261105-010307).log`, verify the new composite:

- `scrn0045.png` — Rhino native ESC Pause, written **01:59:35**.
  The violet top border is **x=224..1702** at y=398..402,
  virtually exact Dreamcast x≈223..1702 (as previously measured).
  Ribbon, 8×8 border tiles and 4:3 orange/blue motif are matched.
  Text shows displaced black shadows because **only the shadow layer**
  was being transformed.
- `scrn0046.png` — Rhino gameplay, written **01:59:49**.
  The top-right boss life bar and Rhino portrait are now positioned
  at Dreamcast scale with a rounded portrait. Source-backed
  `HUD-BOSS-NATIVE:` records show **factor=0.75000** for fill,
  individual frame tiles and portrait; all **64** bounded
  `HUD-BOSS-PROBE` captures report **`corrected=1`**.
  The portrait's original x=1680..1848.8 is mapped to
  **x=1500.0..1626.6**; the fill's right edge x=1691.2 is mapped
  to **1508.4**. The original game vertex attributes beyond X
  remain unchanged. This validates the *shown full-health bar*,
  not the unknown black missing-HP overlay under partial damage.

**Crucial Pause text evidence from 80 original D3D7 detail samples:**

| Source face/colour | Z | RHW | Corrected |
| --- | --- | --- | --- |
| Black text shadow `FF000000` | .00122 | 6.8041 | **48 / 48 yes** |
| Title red face `FFFE3242` | .00098 | **8.0113** | **18 / 18 no** |
| Menu violet face `FF8A78D6` | .00098 | **8.0113** | **14 / 14 no** |

This is **not** a new level-specific letter shape or font: the
existing 16/32×32 original game glyphs at x≈791..1117,
y=171..639 already satisfy the native Pause glyph bounds, original
D3D7 caller `00102C72`, exact texture and *same-Flip verified
ribbon* gate. The old policy simply lacked the third **foreground**
depth `Z=.00098, RHW=8.0113`, while black shadows at 6.8041
were already mapped.

`SpiderNativePause43_TextLayer` now recognizes this discrete
third measured original foreground pair, alongside the already
validated 12.686 and 10.3609 families, without widening the intervals
or touching non-glyph geometry. Host-compiled regression asserts
the exact pair, rejects intermediate RHW 8.40 and incorrect Z.
**The post-fix PAUSED/title/choice visual acceptance is still
pending.**

There are **zero recorded `HUD-BOSS-PROBE: fill-black`**
samples in this run, so the damaged Rhino life-bar mask must not be
declared verified merely from the intact bar screenshot.
Neither screenshots nor log contain evidence of a graphics crash:
no `FATAL`, `ERROR:` or `CRASH` markers, but the session ends
after a focus change, not an explicit clean-exit signal.

## Tenth validation: later Mysterio native Pause source family (2026-11-05)

The maintainer visually **validated Rhino Pause** using original F9
`scrn0047.png`, and supplied `scrn0048.png` from the subsequent
Mysterio-level native ESC: the **entire purple banner remains
screen-wide** and the heading/options too broad at 1920×1080.
Both source screenshots are saved in one
`SpiderMan2k1Fix(20261105-012536).log` run:

- `scrn0047.png`: 02:23:00 F9 native PNG, Rhino Pause **PASS**.
  The matching `PAUSE-NATIVE-DETAIL` shows black shadow **and**
  newly integrated red/violet colored glyph faces
  (`Z=.00098, RHW=8.0113`) both **corrected=1**, confirming the
  Rhino title/label two-layer alignment.
- `scrn0048.png`: 02:23:39 F9 native PNG, Mysterio Pause **FAIL**.
  `HUD-VISUAL: PAUSE-RIBBON` in the immediately surrounding frames
  records the original caller `00102C72`, untextured quad
  x=-7.5..1923.8, y=400.5..675, color `7F3232A0`,
  **Z=0.00122, RHW=11.7760**; source differs from the previously
  verified ribbon RHW 11.176/8.9615/6.8041.
  No `PAUSE-NATIVE-43: ribbon` hit occurs after the Mysterio
  scene enters gameplay.
- `HUD-VISUAL: AIM` in the same frames is a **generic candidate
  name**, not proof that a cursor or aiming element is drawn:
  its original call, alpha-80 colour `80041010` / `80208686`,
  untextured fan at **Z=0.00146, RHW=10.5646** closely matches the
  native translucent ESC motif family, distinct from earlier motif
  RHW=9.988/7.8952/5.9130.

**Strict source-backed changes:** add *only* discrete 11.7760
ribbon and 10.5646 motif families to the existing exact caller,
source geometry, texture-state and alpha/depth gates. Reuse the
canonical center-anchored 4:3 X mapping at any widescreen resolution,
not a Mysterio-specific display adjustment.

**Unresolved letter depths:** no original Mysterio glyph (shadow
or face) Z/RHW evidence is present in this run because the ribbon
did not match and `PAUSE-NATIVE-DETAIL` requires a verified
same-Flip ribbon. Do **not** invent these values or widen the glyph
depth intervals. Once the new ribbon is recognized, the bounded
read-only detail probe will sample letter layers and report
`corrected=0` until the additional exact source pair is observed.

**Important diagnostic sampling fix:** the original one-per-process
80-glyph limit was exhausted in Rhino before the user advanced to
Mysterio. To preserve a **single multi-level test session**,
the separate read-only second-family sampler has its own 80-draw
budget, keyed by the original *verified ribbon RHW* rather than
the level name. The log marks `family=1` versus earlier `family=0`.
No render behavior is changed by this sampling logic.

**Remaining validation:** build, run one Mysterio F9 screenshot
with the new artifact and `Log=1` journal, confirm centered
banner and motif, extract uncorrected letter signatures from
`PAUSE-NATIVE-DETAIL: family=1`, then make the exact text-depth
pair correction and revalidate. Rhino and earlier scenes do
not need to be repeated. Keep PR #109 DRAFT pending.

## Bounded read-only diagnostic

When `[Debug] Log=1`, only **uncorrected** original 4-vertex D3D7
triangle-fan candidates generate `HUD-VISUAL: AIM` or
`HUD-VISUAL: PAUSE-RIBBON` records. Each includes current Flip, caller RVA,
original screen rectangle, Z/RHW, color, texture dimensions and first UV.
Aim is sampled at most once per 45 Flips (200 samples max), pause once per
90 Flips (80 max), with independent budgets. Candidates are **heuristics**,
not proof of which object was drawn. No vertices or render state are changed
by this diagnostic; COM-acquired texture references are released.

The correct compass patch continues using
`HUD-ADAPTIVE: region=compass` records, as before.

## One combined maintainer acceptance run (GE-Proton/Wine)

1. Use the new PR CI artifact, launch with the game's normal 1920×1080
   Auto setup and `[Debug] Log=1`. Preserve the old INI/profile; the branch
   does not change controller mapping or settings.
2. Replay the second 3D cutscene: take an F9 screenshot showing the compass
   and full-width letterbox. Confirm the compass proportions; note whether
   `HUD-ADAPTIVE: region=compass` appears during the scene.
3. Show both green and red crosshairs (screenshots F9), holding each
   briefly; collect `HUD-VISUAL: AIM` candidate records from the same run.
4. Open the native level-2 ESC menu (F9) and confirm whether the ribbon,
   `PAUSED`, red/blue motif and selector are deformed. The provided
   `scrn0021.png` shows the ribbon is still full-width. **The next needed
   evidence is its corresponding SpiderMan2k1Fix.log with Log=1**, including
   `HUD-VISUAL: PAUSE-RIBBON` and `PAUSE-NATIVE-43`, `NATIVE-MOTIF-43`,
   `NATIVE-CURSOR-43` lines (plus startup HUD initialization entries).
5. Return to 3D and quit normally; provide the single log and representative
   F9 captures. A known-good gameplay HUD and 4:3 mode must not regress.
   On 1920×1200 Auto, check the same types where practical.

**Next patch within this PR:** validate the new native ESC rule against the
runtime log and screenshot, and use the matched reticle caller/texture/Z/RHW
and stage evidence for an exact aim correction. Reticles are **diagnosed only,
not fixed**. Do not merge the PR based solely on CI.

legluondunet assisted by ChatGPT (OpenAI)
