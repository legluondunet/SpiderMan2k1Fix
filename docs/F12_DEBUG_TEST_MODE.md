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
