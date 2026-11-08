# SpiderMan2k1Fix v2.0.0-rc.1 — release candidate validation

Status: **RC build only; v2.0.0 is not published**. This checklist does not
authorize creating a tag or release until the real-game tests below succeed.

## Provenance and package

- Base tree: tested and merged `main` commit `c746080dd5f9263d6eb2e063255c46af3290a7bc`.
  The earlier accepted Windows artifact identified itself as `v1.1.0-b66c6c4`;
  GitHub compare reported zero changed files relative to that merged tree.
- This RC changes only the version stamp, public docs and regression tests.
  It must not change original-game hooks, graphics, controller policies,
  audio paths, SDL3 runtime pin or focus recovery implementation.
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

## One consolidated real Windows 11 test

- [ ] Launch and verify the menu version/log prefix contains **v2.0.0-rc.1**.
- [ ] Start-select the DS4 during the opening movie; confirm movie skip,
  main menu, Pause, GAME OVER and normal 3D gameplay buttons.
- [ ] Alt+Tab from a movie and 3D gameplay; resume with no frozen window,
  stuck motion/Alt key, audio loss or missing desktop cursor.
- [ ] Switch DS4 ↔ Xbox 360, unplug/replug and confirm only the selected pad
  drives the game; verify keyboard-only fallback.
- [ ] Confirm original menu/comic/video 4:3, gameplay widescreen HUD, scene
  iris and native ESC 4:3 menu at the tested monitor resolution.
- [ ] Exit cleanly. Attach the `SpiderMan2k1Fix.log` with `[Debug] Log=1`
  if any problem appears (do not post private user paths unnecessarily).

## One consolidated real Linux Wine/Proton test

- [ ] Launch the identical RC package with `WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe`
  or an already validated Proton launch configuration.
- [ ] Validate early Start, movie skip, menu controls, normal gameplay and
  Pause/return using an SDL3 mapped controller.
- [ ] Alt+Tab during video and gameplay; verify working input, audio and
  display on return. The background PAUSED overlay may be delayed under
  Wine/Proton, provided it does not break normal behavior.
- [ ] Disconnect/reconnect or change the controller once; confirm no
  spontaneous second-controller activation or stuck actions.
- [ ] Confirm widescreen/4:3 placement, usable audio and a clean exit.

## Separate follow-up, not silently claimed as complete

Steam Deck/Steam Input hardware, real analog-axis preservation, identical
virtual controller identities and untested gamepads remain subjects of
[issue #120](https://github.com/legluondunet/SpiderMan2k1Fix/issues/120),
which must remain open. Other existing issues must remain unchanged.

## Release decision

Only after the RC's exact-head CI is green **and** both Windows and Wine
hardware test reports are accepted: bump the version to `2.0.0` in a new
PR, rerun CI, inspect the final direct-extraction archive + SHA-256, then
create the official `v2.0.0` tag and GitHub release. Never auto-publish
from this RC branch. See the [project README](../../README.md).

legluondunet assisted by ChatGPT (OpenAI)
