# SpiderMan2k1Fix v2.0.0-rc.2 — release candidate validation

Status: **historical RC2 acceptance record**. The maintainer accepted the final
clean-exit and Windows 11 package tests on November 12, 2026, after earlier
Wine/Proton, Pause, Save Game, controller and focus validations. PR #136 was
merged into `main` at `0d5ae792`. The unchecked boxes below preserve the
original test requests and are not an assertion of missing approval. The
stable 2.0.0 version-bump PR has independent CI and package gates; this RC
history does not create a tag or GitHub release.

## Provenance and package

- Integrated baseline: `main` commit `a5e13c66bf4b5e5c47c452758f5669124d166d96`,
  containing the native Save Game controller fix (#138), validated with
  PS3 Cross on GE-Proton11-12 and Xbox 360 A on native Windows 11,
  and the trigger-drift fix (#139, issue #60) first validated by the
  reporter on Windows 10 with a mapped `045E:02E0` controller.
- RC2 includes the **already merged PR #137** for SDL3 duplicate-VID:PID
  keyboard-only selection, plus version/documentation/CI work. No new
  graphics/audio/media/focus patch, SDL3 runtime or game EXE patch is intended.
- Real Windows 11 tests (November 9, `v2.0.0-rc.2-2d73879`) confirmed
  two Xbox-compatible `045E:028E` devices alternating through Start without
  cross-talk and SAVE GAME / YES via controller A.
- Real Wine/GE-Proton tests (November 12, `v2.0.0-rc.2-aee01fc`)
  logged controller selection 4 → 3 → 1 → 4 → 3: two identical-model Xbox
  devices plus a PS4 controller, with proper owner exit and activation.
  The runtime source blobs matched the later CI head `02de842`; later changes
  concerned documentation/CI, not the game runtime.
- **These focused hardware results are accepted.** Do not interpret them
  as proof of untested simultaneous Start edges or the entire release smoke
  test. Do not publish v2.0.0 solely on CI.
- Obtain the exact RC-head artifact from
  [Windows x86 GitHub Actions](https://github.com/legluondunet/SpiderMan2k1Fix/actions/workflows/spiderman2k1fix-windows-x86.yml).
  Confirm the workflow succeeded for the RC commit before testing.
- Unzip the **single Actions artifact**, keeping `winmm.dll` alongside
  `SpideyPC.exe` and the `SpiderMan2k1Fix/` subdirectory intact.
  Required files include `SpiderMan2k1Fix.dll`, `SDL3.dll`,
  `SDL3-LICENSE.txt`, `README.md`, `CHANGELOG.md`, and the optional
  `linux/spiderman-focus-helper`. Do not flatten the directories.
- The game must be the supported original `SpideyPC.exe` with SHA-256
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
  Keep existing user settings and campaign saves.

## Already accepted hardware evidence

- [x] **Windows 11**: two `045E:028E` controllers selected alternately
  with Start without interfering, and native SAVE GAME / YES confirmed with A.
- [x] **Wine/GE-Proton**: two `045E:028E` controllers and one PS4 controller
  selected in sequence with verified SDL3 owner handoffs (November 12 log).
- [ ] **Not covered by those logs**: deliberately simultaneous Start claims,
  complete game smoke tests on the final packaged artifact, Bink/Pause/Alt+Tab
  and clean exit across all requested combinations.

**CI provenance:** post-PR-#137 main CI passed ([run #43263574600](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/43263574600)); the reconciled RC branch at `46d3e59` also passed ([run #43264265433](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/43264265433)). Documentation was then updated; accept a distributable RC artifact only from a new successful run on the exact subsequent release-branch head. Previous artifacts remain useful for comparison but are not the final package.

## One consolidated real Windows 11 test

- [ ] Launch and verify the menu version/log prefix contains **v2.0.0-rc.2**.
- [ ] Start-select the DS4 during the opening movie; confirm movie skip,
  main menu, Pause, GAME OVER and normal 3D gameplay buttons.
- [ ] Alt+Tab from a movie and 3D gameplay; resume with no frozen window,
  stuck motion/Alt key, audio loss or missing desktop cursor.
- [ ] Switch DS4 ↔ Xbox 360, unplug/replug and confirm only the selected pad
  drives the game; verify keyboard-only fallback.
- [ ] Enter **SAVE GAME**, select a slot and accept the **YES** confirmation
  with controller South/A/Cross, without using physical keyboard Enter.
- [ ] If two `045E:028E` devices coexist, press Start on each one
  separately from neutral, confirm only the selected SDL3 instance controls
  the game, then try an ambiguous simultaneous Start (must fail closed).
- [ ] Recheck issue #60 after a Bink cutscene and native Pause: LT/RT
  in the 5000..10999 idle range must not block context rearming;
  genuine trigger presses (>=11000) must not auto-trigger or stick;
  the initial Start/DirectInput handoff must remain strictly neutral-gated.
- [ ] Confirm original menu/comic/video 4:3, gameplay widescreen HUD, scene
  iris and native ESC 4:3 menu at the tested monitor resolution.
- [ ] Exit cleanly. Attach the `SpiderMan2k1Fix.log` with `[Debug] Log=1`
  if any problem appears (do not post private user paths unnecessarily).

## One consolidated real Linux Wine/Proton test

- [ ] Launch the identical RC package with `WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe`
  or an already validated Proton launch configuration.
- [ ] Validate early Start, movie skip, menu controls, normal gameplay and
  Pause/return using an SDL3 mapped controller.
- [ ] Verify **SAVE GAME / YES** with South/Cross and the log line
  `SDL3-NATIVE-SHELL: ACTIVE` while the game still renders the menu in 3D.
- [ ] Verify duplicate-VID:PID per-instance Start selection, safe handoff
  and fail-closed simultaneous claims; note separately if one physical
  device is mirrored into multiple SDL handles.
- [ ] Alt+Tab during video and gameplay; verify working input, audio and
  display on return. The background PAUSED overlay may be delayed under
  Wine/Proton, provided it does not break normal behavior.
- [ ] Disconnect/reconnect or change the controller once; confirm no
  spontaneous second-controller activation or stuck actions.
- [ ] Validate neutral-first SDL3 rearm after Bink and Pause with partially
  depressed LT/RT; verify no unwanted injected action or stuck keys.
- [ ] Confirm widescreen/4:3 placement, usable audio and a clean exit.

## Separate follow-up, not silently claimed as complete

Steam Deck/Steam Input hardware, real analog-axis preservation, ambiguous
mirrored controller identities and untested gamepads remain subjects of
[issue #120](https://github.com/legluondunet/SpiderMan2k1Fix/issues/120),
which must remain open. Other existing issues must remain unchanged.

## Release decision

Only after the RC's exact-head CI is green **and** both Windows and Wine
hardware test reports are accepted: bump the version to `2.0.0` in a new
PR, rerun CI, inspect the final direct-extraction archive + SHA-256, then
create the official `v2.0.0` tag and GitHub release. Never auto-publish
from this RC branch. See the [project README](../../README.md).

legluondunet assisted by ChatGPT (OpenAI)
