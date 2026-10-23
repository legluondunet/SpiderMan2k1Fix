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
| Any of the four opening Bink movies | Finish game-owned Bink/video cleanup, then leave the intro through its native timed and input-wait pages to the normal main menu |
| First timed pre-menu still | Keep the game's original timer, automatically complete the native input-wait page, then show the menu |
| Second pre-menu `Press Enter` still | Complete the native `0x4A1896` input-wait page normally, then show the menu |
| Main menu | F12 ignored (no Quit) |
| New Game comic or Bink cinematic | F12 ignored to avoid unsupported scene interruption |
| Active 3D gameplay | Native game-thread request 7 and controller-7 final cleanup; quit the whole game |

In the first three cases, F12 **does not** select the native menu's
Quit action. Original cleanup happens before normal Shell_MainMenu is
entered. The native timed page's programmed duration is deliberately
respected, so a pre-menu skip is not instantaneous.

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
