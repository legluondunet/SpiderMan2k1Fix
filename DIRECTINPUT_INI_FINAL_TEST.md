# Windows 11: final INI-only DirectInput recovery check

> PR #7 test only. Do not merge without explicit user approval.

This is the INI integration check after the confirmed menu, movie and 3D
success of the previous combination: `PauseOnFocusLoss=1`,
`PauseKey=Any` and native DirectInput8 retry enabled via an environment
variable. Only **the activation mechanism** changes in this test.

## Settings for this test

Edit the **existing** `SpiderMan2k1Fix.ini` (do not replace your audio,
keyboard, joystick or resolution settings):

```ini
[Debug]
Log=1

[Input]
DirectInputReacquire=1
PauseOnFocusLoss=1
PauseKey=Any
RestoreCursorOnFocusLoss=1
ExperimentalFocusTrace=0
ExperimentalDI8EntryTrace=0
```

Two **obsolete unsafe options** (`ExperimentalAltTabReacquire` and
`ExperimentalDI8CreateDeviceTrace`) are no longer included in factory
INI files. If present in an old personal INI, they remain accepted to
avoid destructive INI repair, but their code paths are forcibly disabled.

## Launch under ProcDump with no legacy environment variable

Run from PowerShell in the game's directory:

```powershell
Remove-Item Env:\SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force .\crash-dumps | Out-Null
& '.\ProcDump\procdump.exe' -accepteula -ma -e -x '.\crash-dumps' '.\SpideyPC.exe'
```

No x32dbg interactive breakpoints. If ProcDump is installed elsewhere,
adjust its path. The `-e` option captures unhandled exceptions; hangs
without exceptions may not produce a dump.

## Verify the log first

After one launch, the non-cumulative log should show:

```text
DI8-RETRY: EXPERIMENTAL requested source=INI; deferred branch install at Flip #120
DI8-RETRY: EXPERIMENTAL active at Flip #120
FOCUS-PAUSE: persistent Flip-boundary pause ENABLED (Any key consumed on release)
```

A retry may later show `DI8-RETRY: redirected DIERR_NOTACQUIRED to game's
Acquire: keyboard=N mouse=M` after Alt+Tab. This line is conditional on
the game's actual error return.

If the log says `source=environment` or `source=INI+environment`,
the environment variable still exists, so this is **not** an INI-only test.

## Test path

1. In the main menu, Alt+Tab away and back. Press and release Enter to wake;
   it must **not** select the current menu entry. Then use Enter normally.
2. Repeat with Space and navigation keys.
3. Repeat once during a cinematic; normal skip should work only on a
   **subsequent** key after waking.
4. Repeat during 3D gameplay; keyboard, mouse and gamepad should resume,
   with no action from the wake key.
5. Close normally, provide only the new single-run `.log` and `.ini`.

If a crash occurs, keep the ProcDump `.dmp` privately and share it only
through a suitable private channel (dumps can contain process-memory data).
If focus gets stuck, restore the last validated INI and binaries.

## Safe rollback

Set `DirectInputReacquire=0` to disable the native branch retry, and
`PauseKey=F9` to use the previous manual wake behavior. The new DLLs
do not modify `SpideyPC.exe` on disk.

---
legluondunet assisted by ChatGPT (OpenAI)
