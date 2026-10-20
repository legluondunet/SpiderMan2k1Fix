# Windows 11 — original EXE icon in the taskbar

The game currently displays a generic exclamation icon in the running
Windows taskbar. This PR sets the original icon **embedded in the verified
`SpideyPC.exe`** on the game's actual window. No extra `SpideyPC.ico`
file is required.

The fix retrieves the **first grouped icon from the game EXE** via Windows
`ExtractIconExW` (Shell32), obtains both small and large icon sizes,
applies them using `WM_SETICON`, and verifies the window icons using
`WM_GETICON`. No resource ID is assumed. Extraction failure leaves the
existing window icon unchanged, without altering the EXE on disk.

The fix is on by default. For an A/B test only, set
`SPIDERMAN2K1FIX_WINDOW_ICON=0` to disable it.

It does **not** modify the process icon resources, game input,
DirectDraw/Direct3D7, mouse cursor, window position, or Z-order.
The old `SPIDERMAN2K1FIX_WINDOW_ICON_TEST=1` switch is no longer used.

## Native Windows test

Install **both 32-bit DLLs from this PR's successful Actions artifact**
next to `SpideyPC.exe` and set `[Debug] Log=1` in the normal
`SpiderMan2k1Fix.ini`. No external `.ico` is required.

From PowerShell in the game folder:

```powershell
Remove-Item Env:\SPIDERMAN2K1FIX_WINDOW_ICON -ErrorAction SilentlyContinue
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
$p=Start-Process -FilePath .\SpideyPC.exe -Wait -PassThru
"Code de sortie : $($p.ExitCode)"
```

While playing, check the icon in the taskbar for the **running game
window** (not a separately pinned shortcut, which Windows may cache).
Test menu → gameplay → menu and at least a few Alt+Tab cycles.
In `SpiderMan2k1Fix.log`, look for:

```text
WINDOW-ICON: embedded SpideyPC.exe icon loaded small+big
WINDOW-ICON: applied embedded SpideyPC.exe icon hwnd=... small=1 big=1
```

If you see `WINDOW-ICON: EXE embedded icon unavailable`, please send
the entire log; the EXE may have no standard grouped icon or its
resource extraction may be failing.

For a control run without the fix:

```powershell
$env:SPIDERMAN2K1FIX_WINDOW_ICON='0'
Start-Process -FilePath .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_WINDOW_ICON -ErrorAction SilentlyContinue
```

Compare the taskbar icon in both runs and report the result.
An `WM_GETICON` success does not guarantee that a separately pinned
taskbar shortcut's icon cache has been updated.

**Do not merge until native Windows validation.** Wine testing is
deferred; the cursor and taskbar overlay bugs are separate and not
modified by this PR.

---

legluondunet assisted by ChatGPT (OpenAI)
