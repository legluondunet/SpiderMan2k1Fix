# Manual Linux / Wine / Proton compatibility regression checklist

**Source/provenance:** recovered and **updated** from the unmerged historical branch `test/wine-compatibility-regression` ([closed PR #5](https://github.com/legluondunet/SpiderMan2k1Fix/pull/5), originally `WINE_REGRESSION_TEST.md` and `tests/wine-game-regression.sh`). The older document referred to a Wine CI smoke job and an abandoned stable build; **neither should be assumed current**. This version is a self-contained **manual** test plan that does not require the old scripts.

**No original game executable or assets are included in this project.** Use your own legally acquired 32-bit Windows copy. The Windows x86 GitHub Actions job compiles and runs source tests; **green CI is not proof that gameplay works on Wine or Proton**.

## 1. Prepare a reproducible test setup

Keep the Wine runner, prefix, GPU, session type, screen resolution, game data and matching DLL pair fixed while testing; document all changes. Verify the original game executable:

```bash
wine --version
file SpideyPC.exe winmm.dll SpiderMan2k1Fix.dll
sha256sum SpideyPC.exe
```

Expected `SpideyPC.exe` SHA-256:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

Place the **same-build** `winmm.dll` and `SpiderMan2k1Fix.dll` next to the game executable; back up any preexisting `winmm.dll`. Wine requires native-then-builtin override:

```bash
WINEDLLOVERRIDES="winmm=n,b" wine SpideyPC.exe
```

For a custom prefix set `WINEPREFIX` in that invocation. For Steam Proton, set the game's launch options to `WINEDLLOVERRIDES="winmm=n,b" %command%`. These are **Windows x86 DLLs used by Wine/Proton**, not native Linux binaries. To record a game log, set `[Debug] Log=1` in the automatically created `SpiderMan2k1Fix.ini` while the game is stopped; the log **overwrites on each launch**, so copy it before the next test.

## 2. Exercise the real game and record evidence

| Phase | Questions to answer on screen and in logs |
| --- | --- |
| Proxy/loading | Are both DLLs loaded without an EXE-hash rejection? Does the game reach its intended startup? |
| Four original intro videos | Are all videos visible at the intended centered 4:3 aspect and can they end normally? |
| Two pre-menu static pages | Do timed and Enter-wait pages complete naturally without input stalls? |
| Interactive menu | Do the keyboard and mouse respond before/after returning from another application? Is the native menu still responsive? |
| New Game comic and movie #4 | Are the cover and Bink cutscene in their intended aspect, with no accidental skip on Enter/Space release? |
| Two introductory 3D scripts | Do they play/complete independently without unwanted jumps? Does gameplay start correctly? |
| Gameplay / HUD / ESC | Is widescreen scene framing correct, compass/instructions/boss bars legible, ESC menu geometrically 4:3, Quit→Yes operational? |
| F9 PNG screenshot | Does the image save in game folder, show expected content and avoid black/magenta frames? |
| Sound | Are voice, Bink music, 3D music and effects present and synchronized? |
| Alt+Tab, default `PauseOnFocusLoss=0` | No crash, cursor remains visible on desktop, keyboard/mouse recover when game regains focus? |
| Optional focus pause `PauseOnFocusLoss=1` | Test only with a backed-up INI; if enabled, is the paused frame correct and does an ordinary key resume? Check for **Ghost** false foreground windows on Windows, and unintended audio playing in background. |
| 4:3 mode | Do pillarboxes, menus, videos and HUD retain the original aspect without changing desktop resolution? |
| Exit | Does normal game Quit close cleanly, without lingering process/audio or a crash? |
| Steam Proton (optional) | Repeat separately, recording Proton version, launch options and DLL override; do not infer compatibility merely from Wine success. |

**Test both first and second New Game sessions in the same process**, because earlier native script/shortcut markers were not always reset. If an optional F10 developer test is needed, consult [its actual current scope](F10_VALIDATION_MATRIX.md); F10 is off by default and **returns from gameplay to menu**, rather than imitating old physical F12 process-exit experiments.

## 3. Diagnose without confounding results

- When a run fails, save the **complete** `SpiderMan2k1Fix.log` before relaunching and, where possible, Wine stdout/stderr. For loader-only problems, a separate `WINEDEBUG=+loaddll` run can show imported DLL activity, but this can produce large logs.
- Verify the exact executable and paired DLL revisions; never combine `winmm.dll` from one build with `SpiderMan2k1Fix.dll` from another.
- A **successful process exit code**, DLL-load log or synthetic proxy-smoke test is *not* proof of correct native DirectDraw/Direct3D, Bink, input, sound, video timing or gameplay.
- If comparing builds, use two **known-good candidate development artifacts** with the same Wine environment. The old branch's instruction to retest against `v1.0.0` is **obsolete**: that publication was withdrawn as nonfunctional.
- When logs contain usernames/home paths, review or redact them before sharing.
- Do not automatically turn on historical `ExperimentalAltTabReacquire`, `ExperimentalFocusTrace`, or retired F12 options. Default/current flags are described in the [configuration guide](CONFIGURATION.md) and source.

## 4. Report format

Record: test date; Linux distribution; X11/Wayland; Wine/Wine-Staging/Proton version; runner and prefix; GPU/driver; desktop resolution; EXE SHA-256; two DLL revisions and their origin; INI options changed; each matrix row **pass/fail/not tested**; visual captures; game log/console log; whether PID really exited; and exact steps to reproduce any failures.

**Recovery decision:** the source in closed PR #5 also contains an unmerged `tests/wine-game-regression.sh`, a small Wine proxy smoke-host C program and CI workflow changes. Their **test methodology is preserved here**, but **their executable code has not been reintroduced or revalidated**. Keep that branch if the intention is to recover/review those tools as code; otherwise this checklist remains a usable manual plan after branch deletion.

See [original engine function catalog](ORIGINAL_EXE_FUNCTION_CATALOG.md) and [F12 native shutdown history](F12_EXIT_ARCHIVE_SUMMARY.md) when triaging real game-owner transitions.
