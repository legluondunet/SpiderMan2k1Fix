# Native Windows menu-to-gameplay PRIMARY reuse diagnostic

Windows 11 test: native device-vtable bypass ON and experimental DirectInput
reacquire OFF allow the game to reach the menu. Starting a new game attempts to
create another DirectDraw PRIMARY and fails with 0x88760234, a duplicate primary.

The patch retains an independent reference to the first windowed PRIMARY, then
returns an additional caller-owned reference to that surface at the second
flipping-primary request. The existing offscreen backbuffer is reused. It
checks backbuffer dimensions and fails explicitly on mismatch.

## Test

Keep `[Debug] Log=1`, set the following in the INI:

```ini
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
```

Then from PowerShell in the game directory:

```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE='1'
Start-Process .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE
```

Choose New Game, then send the complete log. The expected transition line is
`BORDERLESS: REUSE windowed PRIMARY=`.

This is an experimental fix to the menu/gameplay transition. The native
D3D7 device-table and DirectInput startup problems remain separately unresolved.
Do not merge into the stable branch pending real Windows testing.
