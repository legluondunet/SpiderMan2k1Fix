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
Do not enable the experimental shutdown in an otherwise
untested game session or assume arbitrary cinematics are supported.

## Confirmed baseline

| Scenario | Evidence | Status |
| --- | --- | --- |
| First F10 on New Game comic | Original L1COV.bmp timer branch, Bink movie #4 owner and native teardown | Validated in Wine |
| Automatic Stan Lee 3D introduction skip | Original first CutSceneScript skip permission, request, native end | Validated in Wine |
| Automatic second 3D introduction skip | Distinct original CutSceneScript, independently allowed/requested/ended | Validated in Wine |
| Later F10 during initial playable New Game level | Native request 7, game-owned 3D cleanup and controller-7 final-exit flag | Validated in Wine #687 |
| Process exit code / no orphan process | Not contained in the supplied #687 log itself | Confirm separately when desired |

The #687 log recorded native completion (relative to entering 3D):
`SECOND_SCRIPT_ENDED_CANDIDATE_GAMEPLAY stage=6` and
`TWO_SCRIPTS_ENDED_NO_GAMEPLAY_EXIT auto_stage=7` at +1306ms,
then `DEBUG-F10-GAMEPLAY-EXIT F10 accepted`,
`NATIVE-3D-CLEANUP completed`, and
`CONTROLLER-7 ... outcome=NATIVE-FINAL-CLEANUP`.
This validates the engine's shutdown path, not every possible
Wine/Windows process outcome.

## Still to be tested on real gameplay

| Test | Procedure | Expected safe behavior |
| --- | --- | --- |
| F10 during first introductory 3D script | Press F10 before that script has ended, rather than waiting for automatic completion | No controller-7 shutdown; observe a blocked/ignored F10 |
| F10 during second introductory 3D script | Press F10 while original second CutSceneScript remains active | No controller-7 shutdown |
| F10 during a *later in-level* scripted cinematic | Progress to another native CutSceneScript; press F10 during playback | `DEBUG-F10-CUTSCENE-STATUS native_context=1`; `DEBUG-F10-GAMEPLAY-EXIT BLOCKED`; no cleanup request 7 |
| F10 after the later cinematic finishes | Wait for original script manager to report `native_context=0`; press F10 during playable 3D | Normal native cleanup is expected, but not yet runtime validated in this context |
| Different cutscene engine | Find a scene that does not use CutSceneScript | Treat as unverified; do not assume safety from this guard alone |
| Pause screen | Press F10 while paused | No new native exit request |
| Alt+Tab / focus loss | Press F10 while game is not foreground; return and test | Focus handling must prevent an off-focus quit |
| Restart New Game | Complete a run, restart a new campaign, repeat F10 | Prior script stage must not authorize a new premature exit |
| Windows 11 native | Repeat these actions without Wine | Not yet validated for this F10 consolidated build |

For any unexpected behavior, disable `F10GameplayExit=0`
and retain the last known working build. Attach the complete
`SpiderMan2k1Fix*.log`; native script lifecycle and exit
decisions are retained even in compact log mode. Do not use
`taskkill`, `TerminateProcess` or synthetic Space as
a substitute for original native engine cleanup.

This matrix records actual observed/verified behavior separately
from proposed tests. It does not imply the pending cases have
already passed.

PR #26: https://github.com/legluondunet/SpiderMan2k1Fix/pull/26

Contributor: legluondunet assisted by ChatGPT (OpenAI)
