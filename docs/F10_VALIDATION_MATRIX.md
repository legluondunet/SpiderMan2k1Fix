# F10 validation matrix — SpiderMan2k1Fix

Scope: the SHA-matched original Spider-Man 2001 PC executable,
with SpiderMan2k1Fix running under Wine unless explicitly noted.
F10 is the only physical shortcut; F11/F12 are reserved for
MangoHud on Linux. Legacy F12 INI names are compatibility aliases.

## Test configuration

```ini
[Debug]
Log=1
F10Shortcut=1
F10VerboseTrace=0
```

**Important:** `F10Shortcut` is not generated, and its absent
value defaults to 0. Add `F10Shortcut=1` manually to `[Debug]`;
`Log=1` is optional, for diagnostics only. Older F10TestMode,
F10Auto3DScripts and F10GameplayExit keys stay accepted in existing
INIs under their original requirements. Set `F10VerboseTrace=1`
for detailed read-only diagnostics. Unverified later in-level
cinematics are outside the validated New Game behavior.

## Confirmed baseline

| Scenario | Evidence | Status |
| --- | --- | --- |
| First F10 on New Game comic | Original L1COV.bmp timer branch, Bink movie #4 owner and native teardown | Validated in Wine |
| F10 in New Game Bink movie #4 | Native movie stop then two independent native 3D auto-skips | Validated in Wine #705 |
| F10 during first introductory 3D script | Native permission, first skip and second skip to gameplay | Validated in Wine build #700 |
| F10 during second introductory 3D script | Native permission then second skip to gameplay | Validated in Wine #705 |
| Automatic Stan Lee 3D introduction skip | Original first CutSceneScript skip permission, request, native end | Validated in Wine |
| Automatic second 3D introduction skip | Distinct original CutSceneScript, independently allowed/requested/ended | Validated in Wine |
| Earlier F10 process-quit prototype | Native request 7, original 3D cleanup and final-exit bit | Old mode validated in Wine #687; superseded |
| New F10 during playable 3D | Native request 7, game-owned cleanup and controller-7 return to original menu (no final-exit bit write) | Validated in Wine #705; Windows 11 pending |
| Game PID stays alive after gameplay F10 | Original Shell_MainMenu returns and a new game can launch | Validated in Wine #705 |

The #687 log recorded native completion (relative to entering 3D):
`SECOND_SCRIPT_ENDED_CANDIDATE_GAMEPLAY stage=6` and
`TWO_SCRIPTS_ENDED_NO_GAMEPLAY_EXIT auto_stage=7` at +1306ms,
then `DEBUG-F10-GAMEPLAY-EXIT F10 accepted`,
`NATIVE-3D-CLEANUP completed`, and
`CONTROLLER-7 ... outcome=NATIVE-FINAL-CLEANUP`.
This validates the engine's shutdown path, not every possible
Wine/Windows process outcome.

## Regression: two consecutive New Games in the same process

**Reported reproduction (2026-10-23):** New Game -> F10 on the
L1COV.bmp comic -> playable 3D -> Esc -> Pause menu `Exit`
-> `Yes` -> original main menu -> New Game -> comic.
Before the fix, F10 on the second comic and subsequent video did
not respond.

**Root cause in source:** `debug_newgame_progress` stayed 3 after
the first movie-4/3D session, although the exact 0x4279B8
comic-clock original hook requires it to be **0**. The F10 input
worker also requires `input_native_invocations == 0` to classify
a new L1COV comic. This monotonic counter never reset on returning
to the menu, so second-session F10 failed its detection.
The remembered lifecycle, postmovie and auto-skip states also
required resetting before the next introductory pair.

**Fix:** use the EXISTING original `Shell_MainMenu` game-thread
callback at 0x493DD6 / `SpiderQuickExitF12_ConsumeRealMenuRequest`.
Only after returning to that verified native menu with no Bink
active, reset the DLL's per-game-session latches, script generations,
F10 native exit intent and stale comic candidate. Keep actual
game-owned memory, permanent instruction hooks, opt-ins and config
unchanged. The reset is idempotent across subsequent menu frames.

**Test to perform under Wine:** start process once, F10 on
first New Game comic -> verify 2 native intro skips and player
controls -> Esc -> Exit -> Yes -> New Game -> F10 again
on second L1COV.bmp -> verify Bink #4 and both scripts again.
Repeat a third time if practical. Look for one
`DEBUG-F10-SESSION-RESET` after every return to the
main menu, and fresh `DEBUG-3D-AUTO-NATIVE ARMED` each
time. With `F10Shortcut=1`, F10 during verified playable 3D now
requests normal owner cleanup and a native RETURN TO MENU rather
than the old process exit. Test the new behavior separately.

## Still to be tested on real gameplay

| Test | Procedure | Expected safe behavior |
| --- | --- | --- |
| F10 during first introductory 3D script | Let comic/movie run, press F10 mid-Stan Lee intro | `FIRST_INTRO_ARMED_SKIP_BOTH`; skips BOTH scripts after permission to gameplay |
| F10 during second introductory 3D script | Let first intro finish, press F10 mid-second script | `SECOND_INTRO_ARMED_SKIP_REMAINING`; skips remaining script to gameplay |
| Observe a later 3D script before skipping | Native tracker reports `DEBUG-F10-CUTSCENE-STATUS native_context=1` | This is script-active evidence; no controller-7 menu return while active |
| F10 during later in-level native CutSceneScript | Press F10 in another skippable script | `LATER_NATIVE_SKIP_REQUESTED` only when `CutSceneSkipAllow` permits; one native script at a time, no request 7 |
| F10 during unknown/unskippable cinematic | Press F10 when native permission is missing | Do not force scene or controller state; pending request expires or cancels |
| F10 after the later cinematic finishes | Wait for original script manager to report `native_context=0`; press F10 during playable 3D | Normal native cleanup is expected, but not yet runtime validated in this context |
| Different cutscene engine | Find a scene that does not use CutSceneScript | Treat as unverified; do not assume safety from this guard alone |
| Pause screen | Press F10 while paused | No new native exit request |
| Alt+Tab / focus loss | Press F10 while game is not foreground; return and test | Focus handling must prevent an off-focus quit |
| Restart New Game in the same process | Esc → Exit → Yes → New Game → press F10 on second comic | Session markers reset in original menu; F10 auto-skips two scripts — validated in Wine #694 |
| Return to menu with F10 during playable 3D | Press F10 only after both initial scripts ended | Controller-7 says RETURN-TO-CONTROLLER; real main menu responds; same PID; no process final-exit flag |
| Repeat gameplay F10 after return | New Game → F10 on comic → F10 in gameplay → New Game again | F10 works in the next comic and gameplay, with menu reset between sessions |
| Windows 11 native | Repeat these actions without Wine | Not yet validated for this F10 consolidated build |

For unexpected behavior, remove `F10Shortcut=1`
or set it to 0, and retain the last known working build. Attach the complete
`SpiderMan2k1Fix*.log`; native script lifecycle and exit
decisions are retained even in compact log mode. Do not use
`taskkill`, `TerminateProcess` or synthetic Space as
a substitute for original native engine cleanup.

This matrix records actual observed/verified behavior separately
from proposed tests. It does not imply the pending cases have
already passed. The existing process-exit PowerShell watcher is NOT
appropriate for the new in-game F10 return-to-menu behavior.

PR #26: https://github.com/legluondunet/SpiderMan2k1Fix/pull/26

Contributor: legluondunet assisted by ChatGPT (OpenAI)


### Bink Enter-to-first-3D handoff (#704 experimental)

When the user does NOT press F10 during the New Game comic or Bink,
pressing Enter on the movie should only close the movie and allow the
FIRST 3D cutscene to play. Wine #703 observed `f10_origin=0`,
`auto_stage=0`, first script skip allowed after 68 ms and ended after
131 ms: not an F10 auto-skip. A bounded **physical Enter release**
barrier now runs before the first original 3D input-owner call, only
without F10 intent. It changes no game-owned key buffer or script
flags. Check `DEBUG-BINK-ENTER-HANDOFF` and the first-script duration.
Try a quick tap and a longer Enter hold; also verify F10 still skips
comic / movie / both intro scripts as before. This remains a runtime
hypothesis pending testing under Wine and Windows 11.


### Build #705 — Bink Enter/Space physical-release investigation

Build #704 log `20261023-211310`: at the **first** original 3D owner,
`enter_before=0`, but **`physical_space=1`** and game-owned buffered
`raw_space_before=0x7F`. First CutSceneScript became skip-allowed at
68ms and ended at 131ms, despite `f10_origin=0` and `auto_stage=0`.
The earlier Enter-only barrier did not run because VK_RETURN had
already been released. Build #705 now waits for **both VK_RETURN and
VK_SPACE physical key-up** before the first original 3D input update,
bounded to 1500ms. It does not inject keys, write DirectInput memory,
or request a native cutscene skip. New diagnostics show both keys'
before/after states and raw-space byte. This is still a hypothesis:
physical Space may be pressed or interpreted unexpectedly under Wine;
the new test log must confirm whether key-up precedes the first scene.
