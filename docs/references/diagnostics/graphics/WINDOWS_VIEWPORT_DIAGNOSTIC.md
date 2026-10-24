# PR #1 consolidation — Windows D3D7 and shutdown fixes

**Current PR #1 behavior (2026-10-20):** The native Direct3D7 device
vtable is now patched **in place by default** (four methods; original
COM vtable pointer and remaining entries preserved). The narrowly guarded
`fclose(NULL)` exception recovery is also armed by default **only for
the exact SHA-256-verified game executable and matching machine-code
signature**. The early process DPI-awareness fix is included here.
The experimental `PauseOnFocusLoss` option belongs exclusively to the stacked PR #2.

There is **no PowerShell environment variables requirement** for the
consolidated graphics and fclose fixes. For a clean native-Windows test,
remove old diagnostic switches from the current terminal, then run:

```powershell
'SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE',
'SPIDERMAN2K1FIX_D3D_INPLACE',
'SPIDERMAN2K1FIX_D3D_NATIVE_VIEWPORT',
'SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD' |
    ForEach-Object { Remove-Item "Env:$_" -ErrorAction SilentlyContinue }
$p = Start-Process -FilePath .\SpideyPC.exe -Wait -PassThru
"Exit code: $($p.ExitCode)"
```

Expected debug-log entries (with `[Debug] Log=1`):
- `DPI-BOOT: SetProcessDPIAware result=OK`
- `D3D7: native device vtable patched in place (default; all four hooks ON)`
- `FCLOSE-GUARD: exact-signature fclose(NULL) recovery armed`

An exit code of `0` on one test confirms a normal process termination,
**not** that the fclose guard necessarily fired; its recovery count is
reported only if a subsequent Flip occurs.

For a diagnostic A/B test, `SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD=0`
disables only the targeted exception recovery.
`SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE=1` bypasses the device graphics
hooks and can distort the display; **do not use it for regular play**.

The older A/B experiments below are retained as historical records.
Commands setting `D3D_INPLACE=1` and `FCLOSE_NULL_GUARD=1` are no
longer necessary; those modes now run by default. None of these PR
changes should be merged before native-Windows and Wine validation.

---

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

## Test G — Windows desktop cursor recovery (automatic) and Z-order A/B

Windows 11 repeatedly hid the system pointer after Alt+Tab away from
Spider-Man. Native logs captured `cursor-showing=0` on focus loss.
The validated experimental fix restored it with bounded `ShowCursor(TRUE)`
calls **before** the optional Flip-boundary focus pause and balanced the
same count on return. Eleven Alt+Tab cycles were reported as successful
with the explicit test variables.

From PR #3 onward, `[Input] RestoreCursorOnFocusLoss=1` enables the
cursor repair by default **without any environment variables**.
Older INIs missing the key use the same default. To disable and compare,
set `RestoreCursorOnFocusLoss=0`. Normal cursor-only tracking does not
install DirectInput or WndProc hooks, nor does it change the game window
Z-order. It only performs bounded cursor counter changes on actual
foreground transitions observed at DirectDraw Flip.

`SPIDERMAN2K1FIX_FOCUS_PASSIVE=1` explicitly enables *additional*
WndProc/focus event diagnostics without invasive DirectInput hooks.
`SPIDERMAN2K1FIX_ZORDER_TEST=1` is a **separate, optional experiment**
that requests `HWND_TOP` only if the game is already foreground; it
remains disabled by default. Do not enable this Z-order experiment
during cursor validation.

Example final native test (from the game directory):

```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_FOCUS_PASSIVE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_ZORDER_TEST -ErrorAction SilentlyContinue
$p=Start-Process .\SpideyPC.exe -Wait -PassThru
"Code de sortie : $($p.ExitCode)"
Select-String .\SpiderMan2k1Fix.log -Pattern 'INPUT-CURSOR|CURSOR-TEST'
```

The old `SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST` variable is no longer
needed or interpreted. For the tested pause sequence, use
`PauseOnFocusLoss=1`, `ExperimentalAltTabReacquire=0`,
`ExperimentalFocusTrace=0` and `RestoreCursorOnFocusLoss=1`.
See [cursor visibility tests](../focus/CURSOR_FOCUS_TEST.md) for both normal and opt-out procedures.

## Test H — fclose(NULL) at 0052A805, exact Windows crash dump

ProcDump 2026-10-19 16:10:36 local:
- Exception C0000005, read address 0000000C, EIP 0052A805.
- Statically linked MSVC fclose(FILE*) starts at game RVA 0x12A7FC.
- ESI=0, EDI=FFFFFFFF, caller return 005186F8, file argument NULL.
- The call at 005186F8 belongs to original SpideyPC.exe, not Direct3D,
  DirectDraw, input_focus_trace or proxy code. Root cause of NULL FILE*
  remains unknown; the crash may occur during menu/intro focus transitions.
- First-chance exception handling can verify this diagnosis without
  altering any valid FILE* handling or the rendering path.

**Experimental workaround (OFF by default):**
\`SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD=1\` installs a VEH after game SHA-256
verification and checks the exact 13 opcode bytes of the embedded fclose.
For this one error only, and only when return address and the NULL argument
match the dump, it resumes at the original fclose epilogue returning EOF
(-1), without changing code bytes or other exception types.

Keep graphical mode, DPI and input configuration unchanged:

\`\`\`powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'
$env:SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'
# Cursor restoration is now controlled by [Input] RestoreCursorOnFocusLoss=1
$env:SPIDERMAN2K1FIX_ZORDER_TEST='1'
$env:SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD='1'
Start-Process .\SpideyPC.exe -Wait
'SPIDERMAN2K1FIX_D3D_INPLACE','SPIDERMAN2K1FIX_FOCUS_PASSIVE','SPIDERMAN2K1FIX_ZORDER_TEST','SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD' | ForEach-Object { Remove-Item -Path "Env:$_" -ErrorAction SilentlyContinue }
\`\`\`

Expected log: \`FCLOSE-GUARD: EXPERIMENTAL ... armed\`, and if
recovered rendering reaches another Flip, \`FCLOSE-GUARD: recovered 1
fclose(NULL) call(s)\`. If the guard triggers during termination
and no Flip follows, no recovery line will be written. Do not infer
that no recovery occurred in that case. Re-test under ProcDump to catch
other exceptions. If it crashes, send the complete log and minidump.

This is a narrowly controlled test, NOT a production solution. Examine the
call at game return VA 005186F8 in the exact SpideyPC.exe for the reason the
stream was NULL before considering a permanent fix. Never merge without
Windows and Wine verification.

## Withdrawn experiment — native DirectInput branch shims

The test-only DirectInput call-site patch introduced in commit 2682f955
regressed native Windows 11 startup: the verified EXE and D3D7 device
initialized, but no first DirectDraw Flip occurred. The trace printed
`INPUT-CALLSITE: TRACE ONLY active` before termination, and the log
contains no DirectInput error samples. That evidence is insufficient to
attribute the failure to `DIERR_NOTACQUIRED` or to choose a safe runtime
repair. The test has been fully removed from the build; the previous
working graphic, cursor, window Z-order and guarded fclose changes remain.

Do not activate `SPIDERMAN2K1FIX_DI8_POLL_TRACE` or
`SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY` on builds containing the
withdrawn experiment. The new build ignores both variables. Further
DirectInput investigation requires a live Win32 startup-crash dump or
a non-invasive mechanism validated on native Windows.


## WinDbg-confirmed DirectInput8 NOTACQUIRED error and opt-in retry

The native Windows 11 debug log measured persistent
\`DIERR_NOTACQUIRED (0x8007000C)\` from **both** keyboard and mouse
\`IDirectInputDevice8::GetDeviceData\` polling callsites
(\`0x00501BC3\`, \`0x00501D02\`). The game only retries
\`Acquire\` on \`DIERR_INPUTLOST (0x8007001E)\`, so its initial
\`DIERR_NOTACQUIRED\` exits the existing recovery path. The WinDbg
capture did not tag whether each error occurred while in the background;
therefore the experiment also checks the game's own active flag.

This revision replaces the withdrawn **incorrectly relocated**
branch-shim experiment. The old patch calculated a JMP offset relative
to a temporary local array; copying those bytes to the actual EXE JNE
produced an invalid jump and prevented startup. The new patch computes
the offset from the **actual game instruction address**, verifies the
exact 14-byte native signatures, and runs only at **Flip #120** on the
game's render thread, after successful game startup.

OFF by default, and supported only for the exact SHA-256-verified
\`SpideyPC.exe\`. The two 6-byte JNEs become long JMP+NOP to RX
trampolines that preserve all general-purpose registers and stack.
The original \`DIERR_INPUTLOST\` route and non-error route remain
intact. \`DIERR_NOTACQUIRED\` redirects to the ORIGINAL game
Acquire/retry code **only while its native game-active flag is 1**.
All other Windows/Wine graphics and input options are untouched.
No DirectInput8Create hook, no vtable replacement, no injected input
or synthetic focus events. Disabled or signature-mismatched means
the original EXE is unchanged.

First install the successful Windows x86 artifact from the PR's CI.
PowerShell from the game directory (keep previously validated switches):
\`\`\`powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_DI8_POLL_TRACE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_D3D_NATIVE_DEVICE -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'
$env:SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'
# Cursor restoration is now controlled by [Input] RestoreCursorOnFocusLoss=1
$env:SPIDERMAN2K1FIX_ZORDER_TEST='1'
$env:SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD='1'
$env:SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY='1'
Start-Process .\SpideyPC.exe
\`\`\`

Expect \`DI8-RETRY: EXPERIMENTAL requested\` at startup,
\`DI8-RETRY: EXPERIMENTAL active at Flip #120\` once armed,
then \`DI8-RETRY: redirected DIERR_NOTACQUIRED ... keyboard=N
mouse=N\` if triggered. Test repeated Alt+Tab with no debugger.
Send complete \`SpiderMan2k1Fix.log\` and any live hang or crash
dump. The experimental test remains **unmerged** until verified on
Windows 11 and Wine. To disable without replacing DLL files:
\`\`\`powershell
Remove-Item Env:\SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY -ErrorAction SilentlyContinue
Start-Process .\SpideyPC.exe
\`\`\`


## Windows game taskbar icon (PR #4, merged)

Native Windows 11 visually validated the game's original red Spider-Man
icon in the taskbar and its window preview. The fix extracts the first
grouped icon directly from the original `SpideyPC.exe` resources using
`ExtractIconExW`, applies small and large icons with `WM_SETICON` and
verifies them via `WM_GETICON`. A successful extraction can return
`2` for small+large icons; the previous erroneous `extracted==1`
check was corrected. No extra `.ico` file is needed.

On by default. Diagnostic opt-out:
`SPIDERMAN2K1FIX_WINDOW_ICON=0`. This does not change the taskbar
overlay or game Z-order.
