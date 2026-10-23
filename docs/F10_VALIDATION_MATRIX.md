# F10 validation matrix — SpiderMan2k1Fix

Scope: the SHA-matched original Spider-Man 2001 PC executable,
with SpiderMan2k1Fix running under Wine unless explicitly noted.
F10 is the only physical shortcut; F11/F12 are reserved for
MangoHud on Linux. Legacy F12 INI names are compatibility aliases.

## Test configuration

```ini
[Debug]
Log=1
F10TestMode=1
F10Auto3DScripts=1
F10GameplayExit=1
F10VerboseTrace=0
```

**Important:** all F10 switches are disabled by default.
For troubleshooting, set `F10VerboseTrace=1` to restore
the detailed read-only reverse-engineering log messages.
Do not assume arbitrary cinematics are supported. The legacy
`F10GameplayExit=1` option now requests return to the main menu,
NOT process shutdown.

## Confirmed baseline

| Scenario | Evidence | Status |
| --- | --- | --- |
| First F10 on New Game comic | Original L1COV.bmp timer branch, Bink movie #4 owner and native teardown | Validated in Wine |
| Automatic Stan Lee 3D introduction skip | Original first CutSceneScript skip permission, request, native end | Validated in Wine |
| Automatic second 3D introduction skip | Distinct original CutSceneScript, independently allowed/requested/ended | Validated in Wine |
| Earlier F10 process-quit prototype | Native request 7, original 3D cleanup and final-exit bit | Old mode validated in Wine #687; superseded |
| New F10 during playable 3D | Native request 7, game-owned cleanup and controller-7 return to original menu (no final-exit bit write) | Pending Wine and Windows 11 runtime validation |
| Game PID stays alive after gameplay F10 | Verify original Shell_MainMenu returns and a new game can launch | Pending runtime validation |

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
time. With `F10GameplayExit=1`, F10 during verified playable 3D now
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

For any unexpected behavior, disable `F10GameplayExit=0`
and retain the last known working build. Attach the complete
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
