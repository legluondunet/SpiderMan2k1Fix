# Historical native F12 exits — preservation summary

**Why archived:** several experiment and `backup/f12-*` branches were kept during native shutdown research. This document preserves their **validated behavior, original engine owners and safety limits** before those branches are pruned. For detailed chronological logs, x86 snippets and exact Windows/Wine run records, consult the existing **[2,694-line F12 native shutdown journal](F12_NATIVE_SHUTDOWN.md)** and **[1,763-line F12/F10 investigation journal](F12_DEBUG_TEST_MODE.md)**. Both are already on `main`.

> **Not an active shortcut.** Historical physical **F12** process-exit experiments were superseded. The current patch suppresses the unsafe original `F12 → WM_CLOSE` path and uses optional **F10**, which normally **returns from gameplay to the main menu with the same PID**, rather than quitting the process. Do **not** re-enable an old F12 prototype, INI setting or state write by copying these steps.

## Historical validation matrix

All runs below refer to the exact SHA-verified 2001 PC `SpideyPC.exe`. “PASS” describes **one or more recorded trials in the specified phase/build**, not certification across every scene, driver or OS.

| Original context when F12 pressed | Historical test and outcome | Process / visual caveat |
| --- | --- | --- |
| Interactive `Shell_MainMenu` | Original **Quit slot 3 / action 18** at `0x004941D5`; Windows build #471 and later tests confirmed exit. | Correct only when **real menu-owner frame `0x00493DD6`** runs. |
| Four **opening Bink videos** | Windows 11 **build #605**, **one F12** caused normal Bink cleanup → timed/input-wait still-page completion → original menu Quit. | **No residual `SpideyPC.exe`** and no yellow window; **brief main-menu glimpse**. Does not independently test each video clip in every environment. |
| First **timed** pre-menu still | Windows 11 **build #589**, user tested F12 before automatic transition. | **No residual process**. Native timed-page loop and next page cleanup retained; timer duration not independently measured in that particular run. |
| Second **Press Enter** pre-menu still | Windows 11 **build #589**, user tested F12 directly. | Original input-wait owner `0x004A17A0`, input query `0x004A1896`, native cleanup branch `0x004A18AE`, then menu Quit; **no residual process**. |
| New Game **comic / BD `L1COV.bmp`** | Windows 11 **build #589**, intent **deferred** until native comic 300-tick completion and movie #4 original Bink cleanup; then 3D owner/controller-7 process exit. | **No residual process**, but **~1-second yellow window** observed. Exact surface/cause **unknown**; no earlier hard jump from comic draw. |
| New Game **Bink movie #4** | Windows 11 **build #589** and Wine build #552: native movie/script handoff → 3D cleanup → controller-7 final cleanup, exit. | **No residual process**, but Windows 11 again displayed ~1 s yellow. One movie log line was **truncated/interleaved**, not independent proof of parent identity; original x86 establishes caller `0x004279FB`. |
| Active **3D gameplay** | Windows 11 **build #520**, external watcher observed original 3D cleanup and native **final-controller** branch, OS exit code **0**, no residual PID, no matching Windows Application Error/WER event. Further Windows #589 control test. | **No yellow window** in the cited control. Does not prove that every COM object was individually released or every later level tested. |
| Later unknown cutscene engine / other phases | **Not generally validated** as a universal native process exit. | `CutSceneSkipAllow` and manager ownership may differ; ignore rather than forced teardown. |

**Primary evidence:** [Windows #605 opening-Bink report](F12_NATIVE_SHUTDOWN.md), [Windows #589 still/3D/movie/comic reports](F12_NATIVE_SHUTDOWN.md), [Windows #520 external process watcher result](F12_NATIVE_SHUTDOWN.md). Historical detailed commit/build numbers, logs and caveats are retained in those journals; this table is a **summary**, not new runtime verification.

## Why the clean shutdown worked when it worked

The original executable has **three different completion layers**, not one `Quit()` call:

1. **Finish the actual owner:** Bink `0x00470750` goes through **native `0x00470871 → 0x0050B790`** cleanup; fixed startup stills `0x004A1930` and `0x004A17A0` release their page/SEH state; the comic owner `0x00427910` waits for its original 300-tick deadline before it starts movie #4; `CutSceneScript` owners consume native skip only when permission allows it.
2. **Leave 3D cleanly:** original game/update function `0x00440EF0` and 3D frame `0x00455400` can lead to **request 7**. The 3D owner performs native checkpoint **`0x00455BF3 → 0x004302D0`** plus later cleanup and returns to the top-level controller. **Request 7 does not by itself terminate the game**.
3. **Make the native controller's final decision:** request-7 handler **`0x00456185 → 0x0048A820`** evaluates **`[0x02E0988C]&4`** and signed **`[0x00568FB0]`**; can return to controller at **`0x00455D98`** (e.g. flags 0 / status -1) or enter original final cleanup at **`0x0045620C`**. The **real menu Quit** instead uses its own option 3/action 18 owner cleanup at `0x004941D5`.

The **original F12 `WM_CLOSE`** from WndProc `0x005152B0` / call `0x00515373` is unrelated to safe shutdown. It could remove the window while audio/threads continued, so the patch intentionally disables it.

**Operational safety:** never call a native destructor on a background thread; never force a direct jump to `0x0045620C`; do not equate Bink closing with game closing; do not synthesize Space/Enter to fake native cleanup; and do not infer process termination from a vanished HWND. Earlier experiments guarded owner state and ran their final state changes **on the game's own thread**. Historical `F12_*` configurations remain **off or obsolete**, not instructions for users.

## Branch/content conservation findings

- `backup/f12-native-exit-validated-20261023`: archived native F12 result with Windows process checks, Bink/still/3D ownership and controller flow. Its corresponding large research journal is already present at `docs/references/F12_NATIVE_SHUTDOWN.md`.
- `backup/f12-debug-menu-wait-before-experiment-20261023` and `backup/f12-debug-menu-skip-first-3d-20261023`: intermediate debug-menu/clock experiments. Later interpretations and corrected findings already survive inside `docs/references/F12_DEBUG_TEST_MODE.md`. Their historical configuration flags are **not active best-practice instructions**.
- `feature/f12-quick-exit` / `integration/f10-main-20261024`: F12-to-F10 transition, key handoff and permitted 3D script skipping. The current source and [F10 validation matrix](F10_VALIDATION_MATRIX.md) supersede the earlier physical F12 semantics.
- `ChatGPT/tools/Verify-F12QuickExit.ps1` keeps a legacy filename and old log markers. It is **not a turnkey acceptance test for the current F10 process-exit behavior** because F10 gameplay returns to the menu rather than terminating.

**Related index:** [original EXE function catalog](ORIGINAL_EXE_FUNCTION_CATALOG.md) and [per-phase exit/cleanup map](../DEVELOPMENT.md#9-native-exit-points-transition-ownership-and-cleanup-by-game-phase).
