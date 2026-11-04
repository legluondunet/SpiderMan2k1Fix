# HUD 3D cinematic / aiming / level-2 native ESC — combined visual fix

Status: **staged diagnostic + first narrowly scoped compass correction**.
Do not claim all three issue classes are fixed until the runtime capture
and regression test below have been reviewed.

## Player reports (2026-11-04)

Reference images supplied by the project maintainer, all 1920×1080:

- **HUD-01 (compass):** second 3D cutscene and aiming sequences show
  horizontally stretched compass artwork/indicator. Do not alter the already
  correctly full-width 3D cinematic letterbox bars.
- **HUD-02 (aim):** green and red aiming crosshairs/rings have a wider-than-tall
  outline (screenshots `scrn0008.png`, `scrn0009.png`).
- **HUD-03 (native pause):** the level-2 `PAUSED` ribbon, red/blue motif,
  title and selector do not all show the same restored 4:3 presentation
  previously validated in other gameplay; see `scrn0010.png`.

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
  The image does not reveal whether the mismatch is caused by an altered
  D3D7 caller, ribbon geometry, depth/RHW, bound texture or stage state.
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
   `PAUSED`, red/blue motif and selector are deformed. Collect
   `HUD-VISUAL: PAUSE-RIBBON` and existing `PAUSE-NATIVE-43`,
   `NATIVE-MOTIF-43`, `NATIVE-CURSOR-43` lines.
5. Return to 3D and quit normally; provide the single log and representative
   F9 captures. A known-good gameplay HUD and 4:3 mode must not regress.
   On 1920×1200 Auto, check the same types where practical.

**Next patch within this PR:** use the matched runtime caller/texture/Z/RHW
and stage evidence to add exact reticle and level-2 ESC transformations with
focused portable regression tests. Until then, those two families are
**diagnosed only, not fixed**. Do not merge the PR based solely on CI.

legluondunet assisted by ChatGPT (OpenAI)
