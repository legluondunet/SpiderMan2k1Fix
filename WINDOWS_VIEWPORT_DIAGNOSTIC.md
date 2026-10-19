# Windows D3D7 crash — viewport diagnostic

This is a temporary diagnostic on pull request #1. It makes no default
rendering changes, does not modify the EXE, and preserves the menu-version
and native WinMM audio exports.

The Windows Event Log showed `0xC0000005`, fault offset `0`, and the last
SpiderMan2k1Fix log entry was inside `hooked_set_viewport`, immediately before
the real Direct3D 7 `SetViewport` call. This suggests an invalid indirect
call or a graphics-driver fault but **does not prove either**.

## Test A — normal behavior

Ensure `[Debug] Log=1` in SpiderMan2k1Fix.ini. From the game's PowerShell
directory, run:

```powershell
Remove-Item Env:\SPIDERMAN2K1FIX_VIEWPORT_LOGICAL -ErrorAction SilentlyContinue
.\SpideyPC.exe
```

Save the log. A `D3D7-VIEWPORT-DIAG: BEFORE` line with no corresponding
`AFTER` means the process failed during the real native SetViewport call
or before it could return. `AFTER` proves that particular call returned.

## Test B — original logical viewport

Exit the game and start it again from the **same PowerShell window**:

```powershell
$env:SPIDERMAN2K1FIX_VIEWPORT_LOGICAL='1'
.\SpideyPC.exe
Remove-Item Env:\SPIDERMAN2K1FIX_VIEWPORT_LOGICAL
```

This keeps the original 640×480 D3D7 viewport for the menu stage rather
than remapping it to 1600×1200. The view may look wrong; it is only an
A/B diagnostic to isolate the crash. Please send both logs and whether a
menu appears.

Do not merge this diagnostic code until its purpose is complete.
