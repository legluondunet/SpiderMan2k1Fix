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
