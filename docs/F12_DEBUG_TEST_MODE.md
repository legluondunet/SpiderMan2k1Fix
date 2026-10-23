# Debug F12 fast-test mode (experimental)

**Default: disabled.** Enable in `SpiderMan2k1Fix.ini`:

```ini
[Debug]
F12TestMode=1
Log=1
```

`Log=1` is recommended to capture evidence, but is not required
by `F12TestMode`. The new single flag takes precedence over the
older experimental F12 controls in `[System]`.

## Behaviour

| When F12 is pressed | Debug action |
| --- | --- |
| Any of the four opening Bink movies | Finish game-owned Bink/video cleanup, accelerate the first timed still's original timeout condition, complete the input-wait page, and enter the normal main menu |
| First timed pre-menu still | Make the game's native timed-loop deadline expire on its next check, complete the native page cleanup and input-wait page, then show the menu |
| Second pre-menu `Press Enter` still | Complete the native `0x4A1896` input-wait page normally, then show the menu |
| Main menu | F12 ignored (no Quit) |
| New Game comic or Bink cinematic | F12 ignored to avoid unsupported scene interruption |
| Active 3D gameplay | Native game-thread request 7 and controller-7 final cleanup; quit the whole game |

In the first three cases, F12 **does not** select the native menu's
Quit action. Original cleanup happens before normal Shell_MainMenu is
entered. **New experimental acceleration:** when F12 was pressed before the
main menu, the first fixed image no longer waits several seconds.
The hook replaces only the five-byte original read of the native clock
at `0x4A1A26`. On a guarded debug F12 request, the hook returns the
*existing native deadline in EDI plus one*, so the game's unchanged
`CMP EAX,EDI; JA 0x4A1A36` takes its original timed-page destructor
and SEH return. The shared game clock, native timer setup, resource
pointers and page objects remain untouched. Without a debug F12
request, the clock read is unmodified. A hook-signature mismatch
**disables the debug mode safely**, rather than skipping cleanup.
The first page may appear for a single frame; immediate completion
without any rendering or game-thread cleanup is not attempted.

This is a test-specific behaviour, not a replacement of the previous
validated F12 shutdown implementation. If this INI switch is `0`,
the pre-existing System F12 options keep their legacy meanings.
Every F12 option, including this switch, defaults to `0` in the
generated INI. The game's known unsafe native F12 `WM_CLOSE` is still
suppressed when the legacy F12 feature is off.

## Restore the earlier F12 implementation

A frozen GitHub branch preserves **all source, options, test cases and
documents from before this experiment**:

- **Branch:** `backup/f12-native-exit-validated-20261023`
- **Pinned SHA:** `4adf16aa13a867a3256f7a2f7ce506cc53b9b82a`
- **Browse:** https://github.com/legluondunet/SpiderMan2k1Fix/tree/backup/f12-native-exit-validated-20261023

To simply run the prior code without changing the active PR:

```sh
git fetch origin backup/f12-native-exit-validated-20261023
git switch --detach 4adf16aa13a867a3256f7a2f7ce506cc53b9b82a
```

To restore only the old F12 implementation to a development branch,
after checking for local uncommitted changes:

```sh
git switch feature/f12-quick-exit
git restore --source=backup/f12-native-exit-validated-20261023 -- quick_exit_f12.c
git add quick_exit_f12.c
git commit -m "Restore previously validated F12 native-exit implementation"
```

For a **complete** return to the prior PR source tree, use the
frozen branch as the source for a fresh branch or the exact pinned commit.
Do not reset/force-push the PR until the user explicitly approves it.

## New Game Bink regression found after intro F12 skip

A Linux/Wine trial with patch `v1.0.0-aef9397` and
`F12TestMode=1` (`SpiderMan2k1Fix(20261023-112854).log`)
validated the new fast pre-menu skip: the native
`0x4A1A26` deadline hook and `0x4A1896` input-wait
completion both ran, and the actual menu remained open.
However, **New Game then showed the comic and skipped its Bink
cinematic**, returning through native controller request 7.

The cause was an **unconsumed pre-menu F12 skip request**:
`intro_quit_pending=1` remained set after the real menu
was reached. The game's `0x470750` Bink tick is used for both
the four opening movies and later New Game movie #4. The
hook saw this stale flag and returned 0 (movie finished)
before calling the native Bink frame tick for New Game.

**Fix (not yet runtime-validated):** the `spider_after_intro_cleanup`
wrapper now consumes `intro_quit_pending` (and resets the
early short-circuit diagnostic) in **debug F12TestMode only**
AFTER calling original native intro transition cleanup and
AFTER issuing the separate pending input-wait-page request.
The former F12 Quit path is unchanged. No game Bink object,
shared playback pointer, native timer, or 3D controller
logic is modified.

Expected successful regression:
`DEBUG-MENU-SKIP opening Bink request CONSUMED at native
post-intro cleanup; movie #4 tick restored for future New Game
playback` appears in log; after selecting New Game, the comic
is followed by the actual Bink cinematic before 3D gameplay,
without pressing F12 again. As usual, this must be confirmed
with a real Wine/Windows test; source assertions and cross
compilation do not prove native runtime behavior.

## Delayed menu input after pre-menu F12 (diagnostic)

A further user test confirms that with `F12TestMode=1` the
introductory videos, accelerated timed still and input-wait still
all complete correctly, and the real menu **appears**, but menu
keyboard/mouse control becomes responsive only **approximately
4–5 seconds later**. User log:
`SpiderMan2k1Fix(20261023-114209).log`
(`v1.0.0-56a5b31`).

The uploaded log confirms:
- F12 intent consumed at original `0x443AD0` cleanup; later
  movie #4 works normally
- `DEBUG-MENU-SKIP first timed-page deadline accelerated`
- `DEBUG-MENU-SKIP original 0x4A1896 input-wait TRUE completion`
- `REAL Shell_MainMenu frame callback observed`
- `DI8-RETRY: EXPERIMENTAL active at Flip #120`
- `INPUT-FOCUS` still shows the game as foreground, with no
  recorded focus loss
- New Game comic, Bink and subsequent 3D run and exit normally

**Two distinct hypotheses, not yet demonstrated causes:**
1. The original game's `Shell_MainMenu` draws before a native
   condition allows input: the exact SHA-verified x86 at
   `0x493DDB` reads DWORD `[0x54D38C]` and at `0x493DE2`
   branches to `0x4941C0` (return **before** menu input reads
   at `0x493E1F`) while the DWORD is nonzero.
   The flag is known to transition during startup and might
   already be zero when the real menu begins. Do NOT alter it.
2. The optional `[Input] DirectInputReacquire` native patch
   is deliberately installed only at Flip **#120**, about four
   seconds from the first Flip under FPSLimit=30. That timing
   may correlate with input recovery, but there is **no actual
   DirectInput error/reacquisition observed in this log**.
   Do NOT equate installation with proof that a blocked key
   read was repaired.

The new diagnostic is **read-only** and only activates after the
debug F12 shortcut has completed the original pre-menu pages.
It uses the already installed native Shell_MainMenu callback.
There is **no new executable patch**, DirectInput call, synthetic
keypress or modified engine state. It prints a bounded
`DEBUG-MENU-INPUT-PROBE` line at the first menu tick, each
native transition or game-active change, and every 15 native
menu frames up to frame 150; final checkpoint at frame 210.
Fields:
- `ms_since_first_menu`: independent wall-clock elapsed time
- `ms_since_intro_F12_latch`: elapsed from the saved request latch
- `native_transition`: original `[0x54D38C]` DWORD
- `native_menu_input_branch=GATED|ELIGIBLE`: exactly the
  original 0x493DE2 condition, **NOT proof of keyboard readiness**
- `game_input_active`: original DirectInput context-active DWORD
  `[0x6B78F0]`
- `menu_ctx` and `native_clock`: read-only supporting context

**Next comparative run:** same INI/build except updated DLLs, press
F12 during one of the four opening movies, do not Alt+Tab, then
repeatedly try moving the main-menu highlight with arrow keys.
Note roughly when it first reacts and upload the log. If
`native_menu_input_branch=GATED` persists for the delay,
the original menu condition is strongly implicated. If it is
`ELIGIBLE` throughout, investigate actual DirectInput data
availability/Acquire separately instead of bypassing any timer.
The `DI8-RETRY` activation at Flip #120 is a time marker,
not a proven input-responsiveness cause.

The frozen old F12 implementation on
`backup/f12-native-exit-validated-20261023` remains untouched.
No functional F12 or native menu changes are made for this
diagnostic. PR #26 remains draft/unmerged.

## 2026-10-23 — ~4-second visible menu input latency: original native menu tick vs frame-gap timing

The user confirmed a persistent **~4-second** input delay once the
main menu is visually displayed after pressing F12 during an opening
Bink movie (`[Debug] F12TestMode=1`). Log:
`SpiderMan2k1Fix(20261023-115419).log`, patch
`v1.0.0-56b5e4f`, exact supported game EXE SHA verified.

The preceding read-only native `0x493DD6` callback reported:

```text
DEBUG-MENU-INPUT-PROBE frame=1 ms_since_first_menu=0 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=15 ms_since_first_menu=3768 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=30 ms_since_first_menu=4282 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=45 ms_since_first_menu=4796 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
```

A 30-FPS loop would take ~467ms for 14 frames;
the observed first 14 inter-frame intervals took
**3,768ms**, an excess of approximately **3,301ms**.
Afterwards 15 frames took 514ms, essentially the
expected 500ms. The original menu gate `[0x54D38C]`
was **already zero at the first observed menu frame**
and remained so, with `[0x6B78F0]=1`. This rules
out a persistent nonzero `0x54D38C` menu gate
as the source of that delay. It does **not** prove
all DirectInput calls/keypresses were accepted.

The optional DirectInputRetry patch installed at
Flip #120 AFTER this first unusually slow segment.
No DIERR_NOTACQUIRED or successful native Acquire
retry was observed. Its installation cannot by
itself be identified as the causal fix or cause.

**Diagnostic-only code in build #634**: augment
the *already installed* original-game call wrapper
at `0x493DD6`. A passive entry timestamp
immediately precedes the original
`0x505720` native menu tick call, and the
existing post-tick hook collects a second timestamp.
A bounded `DEBUG-MENU-TICK-TIMING` trace for
first 20 menu ticks (and selected subsequent
ones up to 65) reports:

- `native_0x505720_ms`: time INSIDE the original
  `0x505720` native menu tick
- `since_last_exit_ms`: time BETWEEN the previous
  native menu tick's return and this tick's entry
- `wallclock_ms`: monotonic `GetTickCount` sample

This partitions the ~3.3-second excess into
**inside-native-tick** versus **between-ticks**
causes. It does not diagnose deeper internals
of `0x505720` yet; do not guess an input API
or driver root cause from frame timing alone.
All helper calls occur on the same original
render/menu thread within existing register/flags
save/restore and do not change game state.
No new EXE CALL site, DirectInput hook, timing
override, input injection, async callback, or
extra normal-game F12 behavior is introduced.

**Next runtime regression:** test build
[#634](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/39949796958)
with EXACT same INI including
`[Debug] F12TestMode=1,Log=1` and original
`[Input] DirectInputReacquire`. Press F12 in an
opening Bink, immediately try moving menu
selection after menu appears, and upload the log.
The largest `DEBUG-MENU-TICK-TIMING` entry should
locate the side of the native `0x505720` call
responsible for the wall-time gap. Once the
location is known, inspect that native game
caller's path before considering a correction.
F12 shortcut, accelerated still, restored New
Game Bink playback, native 3D exit remain
untouched; previous native F12 full-exit branch
`backup/f12-native-exit-validated-20261023`
is preserved.

## Regression test plan

1. On Windows 11, press F12 **once** during the first intro Bink movie.
   Expect native Bink cleanup, the timed still to complete normally,
   then the interactive main menu (do not press F12 again).
2. Repeat from the `Press Enter` page. Expect the main menu, no
   `SpideyPC.exe` process exit.
3. From real interactive 3D gameplay, press F12 once. Verify the
   window disappears and no residual `SpideyPC.exe` remains.
4. Optionally verify F12 does nothing in the main menu and during
   the New Game comic or its Bink cinematic.

All runtime changes live on draft PR #26. They have **not** been merged
and require native Windows/Wine testing before final validation.
