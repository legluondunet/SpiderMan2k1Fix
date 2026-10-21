# Windows 11 external DirectInput8 diagnostics (x86)

This is an EXPERIMENTAL diagnostic prototype, not a fix. It starts the original
Spider-Man PC game under Microsoft CDB (x86), observes native DirectInput8
functions via debugger breakpoints and leaves game binaries, fix DLLs,
COM vtables, cooperative levels and input state untouched. Debugger-owned
software breakpoints temporarily modify executable instruction bytes and may
disturb timing: this is not a non-invasive attach.

## Requirements

- Use the stable PR #7 game DLLs and set both
  ExperimentalDI8CreateDeviceTrace=0 and ExperimentalAltTabReacquire=0.
  These COM hooks were quarantined after confirmed startup failures.
- Prefer ExperimentalDI8EntryTrace=0 so only the external debugger observes
  DirectInput8Create. The F9 pause option may remain enabled.
- Install Microsoft Debugging Tools for Windows, x86, from the Windows SDK.
  The launcher looks for Windows Kits\10\Debuggers\x86\cdb.exe.
- Have run_directinput8_trace.ps1 and trace_directinput8_windbg.js next to
  one another in the PR #7 diagnostics directory. Node.js is NOT required.

## Run once on Windows 11

Open PowerShell in your clone of the PR #7 branch and execute this command:

    powershell -NoProfile -ExecutionPolicy Bypass -File .\diagnostics\run_directinput8_trace.ps1 -GameDir 'C:\Users\saccor\Downloads\Spider-Man'

If necessary, add -CdbPath 'C:\full\path\to\x86\cdb.exe'.

The launcher starts SpideyPC.exe *under CDB* and captures a distinct log at
%TEMP%\SpiderDI8Trace\SpiderDI8-WinDbg-*.log.

Wait for the MAIN MENU without Alt+Tab during introductory movies.
Verify keyboard and mouse work initially; Alt+Tab ONCE, return, press F9 to
resume, then test both inputs. Do not repeat when the menu is unresponsive.

At the end, use Ctrl+Break in the debugger and enter:

    dx @$scriptContents.di8Summary()

Then g resumes execution. The qd command quits the debugger by detaching.
If Spider-Man cannot close normally, Windows Task Manager may be necessary.
Provide BOTH the new CDB log and the usual SpiderMan2k1Fix log.

## Expected markers

- SPIDEY-DI8: DirectInput8Create hr=0x0 interface=...
- SPIDEY-DI8: CreateDevice guid0=0x6f1d2b61 ... (keyboard)
- SPIDEY-DI8: CreateDevice guid0=0x6f1d2b60 ... (mouse)
- SPIDEY-DI8: keyboard GetDeviceState count=... hr=...
- SPIDEY-DI8: mouse GetDeviceData count=... hr=...

The script reads native IDirectInput8::CreateDevice (vtable slot 3) and
device methods Acquire (slot 7), Unacquire (slot 8), GetDeviceState (slot 9),
GetDeviceData (slot 10). It records their native return HRESULTs without
calling these methods itself. It logs initial calls, changes and periodic
samples to limit output.

If no factory breakpoint hits, this does NOT prove DirectInput8 is unused.
Debugger setup, script limitations, or a different entrypoint may be involved.

Important: the JavaScript is syntax-checked and has been mocked on Linux,
but the live CDB x86 execution is NOT yet validated under Windows 11.
The first test may expose debugger-script problems. In that case stop and
send the CDB log; do not interpret missing input records as proof.

Microsoft references:
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/debugging-a-user-mode-process-using-cdb
https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/javascript-debugger-scripting
https://learn.microsoft.com/en-us/windows-hardware/drivers/debuggercmds/bp--bu--bm--set-breakpoint-
