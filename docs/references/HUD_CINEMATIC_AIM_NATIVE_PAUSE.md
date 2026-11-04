# HUD 3D cinematic / aiming / level-2 native ESC — combined visual fix

Status: **staged compass correction, native ESC geometry fix and reticle diagnostics**.
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
