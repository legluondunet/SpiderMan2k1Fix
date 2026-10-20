# Windows 11 — original game icon in taskbar

The game currently displays a generic exclamation icon in the running
Windows taskbar. This PR makes the already-present `WM_SETICON`
compatibility fix active **by default**, but only if `SpideyPC.ico`
exists beside the original game `SpideyPC.exe`.

On the real game window, the fix loads the separate ICO as a small and
large `HICON` with Windows `LoadImageW` and sets them via `WM_SETICON`.
The result is verified using `WM_GETICON` and logged. If the ICO does
not exist or cannot be loaded, it preserves the original window icon.
No EXE resource, Z-order, focus, Direct3D or cursor modifications.

For isolated A/B tests, set `SPIDERMAN2K1FIX_WINDOW_ICON=0` to disable
the fix. The old experimental opt-in switch
`SPIDERMAN2K1FIX_WINDOW_ICON_TEST=1` is no longer required.

## Manual test on Windows 11

Install **both DLLs from this PR's Actions build** and enable
`[Debug] Log=1` in the INI. From the game folder:

```powershell
Test-Path .\SpideyPC.ico
Remove-Item Env:\SPIDERMAN2K1FIX_WINDOW_ICON -ErrorAction SilentlyContinue
$p=Start-Process -FilePath .\SpideyPC.exe -Wait -PassThru
"Code de sortie : $($p.ExitCode)"
```

Observe the icon of the **running game window** in the taskbar, not a
separately pinned shortcut (whose icon Windows may cache). In the log,
look for `WINDOW-ICON: applied SpideyPC.ico ... small=1 big=1`.
Test launch, menu->gameplay->menu and multiple Alt+Tab cycles.

If the taskbar icon still appears wrong, try the opt-out A/B run:

```powershell
$env:SPIDERMAN2K1FIX_WINDOW_ICON='0'
Start-Process -FilePath .\SpideyPC.exe -Wait
Remove-Item Env:\SPIDERMAN2K1FIX_WINDOW_ICON
```

Record the visual results for both runs. A successful `WM_GETICON`
verification does not guarantee that Windows has refreshed its
taskbar icon cache.

Experimental until validated on native Windows 11; Wine testing deferred.

---

legluondunet assisted by ChatGPT (OpenAI)
