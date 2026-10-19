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

## Test C — bypass our D3D7 device hooks entirely

If both prior SetViewport configurations crash inside the native Windows
Direct3D module, test whether it depends on our patched D3D7 device vtable.
This diagnostic is temporary, opt-in, and does **not** disable our winmm,
DirectDraw or No-CD hooks.

Leave `[Debug] Log=1` in the INI. In PowerShell, from the game's directory:

```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE='1'
Start-Process .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE
```

Expected diagnostic line:
`D3D7-DIAG: native device vtable, viewport/draw/render-state hooks bypassed`

Graphics and aspect ratio may be incorrect. Send the log and indicate whether
the game reaches the movies or main menu. If it still crashes, the cause is not
limited to our D3D7 device-vtable replacement.

## Test E — in-place D3D7 vtable, visual fixes ON

Background: the Windows 11 process crashed at EIP=0 inside d3dim700.dll
with our copied D3DDevice7 vtable. Running without device hooks reaches
the main menu and gameplay, but disables vertex/viewport visual fixes.
This experimental mode patches only the four device-method pointers in
the ORIGINAL vtable while preserving its address and other entries.

**Important:** turn OFF `SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE`; it overrides
this mode and would disable all rendering hooks again. Also keep DirectInput
reacquire OFF during this experiment.

In `SpiderMan2k1Fix.ini`:

```ini
[Debug]
Log=1
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
```

PowerShell in the game folder:

```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'
Start-Process .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_INPLACE
```

Look for:
`D3D7-DIAG: INPLACE native device vtable patched`.

Check that the menu and four intro videos respect 4:3, the version
label is visible, and gameplay renders at the native 1920x1200 output.
**Avoid Alt+Tab in this test** since native Windows input recovery has
not been fixed yet. Report any crash and attach the log.

This is an A/B experiment, not a production fix. If it crashes, return
to `SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE=1`.
 
## Test F — passive Alt+Tab tracing under native Windows 11

The D3D7 `SPIDERMAN2K1FIX_D3D_INPLACE=1` mode now renders movies,
menus, and gameplay correctly. Alt+Tab still crashes. The native Windows
DirectInput8 interception can itself prevent startup, so **do not** turn
`ExperimentalFocusTrace=1` or
`ExperimentalAltTabReacquire=1` back on for this test.

This test enables only an existing WndProc observer and Flip snapshots.
It does not patch the DirectInput8 IAT, intercept device COM vtables, or
reacquire keyboard/mouse.

INI (keep the rest unchanged):
```ini
[Debug]
Log=1
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
```

PowerShell in the game folder:
```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'
$env:SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'
Start-Process .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_INPLACE
Remove-Item Env:\SPIDERMAN2K1FIX_FOCUS_PASSIVE
```

Start a new game, wait a few seconds, Alt+Tab out once, then try
returning to the game. Save the full log whether or not it crashes.
Look for:
`INPUT-FOCUS: PASSIVE ONLY - WndProc events/Flip snapshots active; DirectInput8 hook NOT installed`

If the game crashes without further focus events, capture a ProcDump
minidump for this exact run and/or Windows Application Event 1000.
The log by itself cannot identify an exception that occurs in the original
game or Direct3D driver. Keep the graphical in-place test setting on.

This diagnostic remains opt-in and should be removed before merging the PR.

## Test G — native Windows cursor and Z-order A/B

Win11 multi-monitor tests show that Spider-Man survives repeated Alt+Tab
while keyboard focus, visible Z-order and system pointer diverge. The passive
trace confirms foreground transitions, but does not determine whether
ShowCursor's display counter, cursor shape or clipping is to blame.

When \`SPIDERMAN2K1FIX_FOCUS_PASSIVE=1\`, the new test logs actual cursor
visibility (\`GetCursorInfo\`), clipping (\`GetClipCursor\`), topmost
status and the next higher window in the Z-order.

Two independent opt-in repair switches:
- \`SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST=1\` only when leaving Spider-Man,
  and only if GetCursorInfo says the cursor is hidden, increment ShowCursor's
  count up to a cap of 16; exactly reverse the increments when foreground
  returns. Cannot cure hidden cursor shapes or unrelated drivers.
- \`SPIDERMAN2K1FIX_ZORDER_TEST=1\` only after the game is already foreground,
  request HWND_TOP in normal Z-order with SWP_NOACTIVATE. Never make topmost,
  never force activation over another foreground application.

Preserve the current DPI compatibility setting (Application), D3D_INPLACE,
and [Input] ExperimentalAltTabReacquire=0, ExperimentalFocusTrace=0.

From PowerShell in the game folder:

\`\`\`powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'
$env:SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'
$env:SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST='1'
$env:SPIDERMAN2K1FIX_ZORDER_TEST='1'
Start-Process .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_INPLACE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_FOCUS_PASSIVE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_ZORDER_TEST -ErrorAction SilentlyContinue
\`\`\`

Try Alt+Tab several times and report cursor and window separately, with log.
For isolation, remove either test switch in an additional run. Test code must
not be merged before native Windows/Wine validation.
