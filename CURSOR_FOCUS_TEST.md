# Windows 11 desktop cursor visibility — guarded A/B test

**Scope:** Mouse pointer becomes invisible on the Windows desktop after
Alt+Tab away from Spider-Man, even though mouse position/buttons still
work. The taskbar's Z-order is a separate problem and is not changed here.

## Suspected cause and change

With `[Input] PauseOnFocusLoss=1`, the DirectDraw Flip hook previously
blocked for focus restoration **before** calling the passive cursor
diagnostic. The code that could restore the desktop cursor therefore ran
only after returning to Spider-Man. The new ordering observes the
background transition **before** the pause, then observes focus again
after the pause to balance any temporary `ShowCursor(TRUE)` calls.

The existing cursor repair is **still opt-in** and is guarded by the
pre-existing `SPIDERMAN2K1FIX_FOCUS_PASSIVE=1` and
`SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST=1` switches. It only uses
`ShowCursor` when `GetCursorInfo` reports the pointer hidden.
The previous visibility counter is restored on focus return.

No new DirectInput hooks, no mouse input synthesis, no Z-order changes.
If `GetCursorInfo` reports the cursor **already visible**, this
experiment won't change it; the trace helps diagnose a different cause.

## Native Windows 11 trial

Use **both compiled DLLs from this PR's Actions artifact** in the game
folder. In `SpiderMan2k1Fix.ini`, use:

```ini
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
PauseOnFocusLoss=1
[Debug]
Log=1
```

In PowerShell in the folder of `SpideyPC.exe`:

```powershell
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_ZORDER_TEST -ErrorAction SilentlyContinue
$env:SPIDERMAN2K1FIX_FOCUS_PASSIVE='1'
$env:SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST='1'
$p=Start-Process -FilePath .\SpideyPC.exe -Wait -PassThru
"Code de sortie : $($p.ExitCode)"
Remove-Item Env:\SPIDERMAN2K1FIX_FOCUS_PASSIVE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST -ErrorAction SilentlyContinue
```

While playing, Alt+Tab to another application several times.
Check **desktop cursor visibility** while Spider-Man is paused in the
background and mouse clicking still works. Then return to the game and
check controls; repeat menu/gameplay and quit normally.

Expect (when triggered): `INPUT-WINDOW: CURSOR-TEST background
ShowCursor(TRUE)` and `foreground balanced ShowCursor(FALSE)`.
If neither appears, report the `INPUT-WINDOW: ... cursor-showing=...`
values; they distinguish a hidden pointer from a missing cursor image.

This PR is **experimental** pending manual validation on Windows.
Wine testing is deferred.

---

legluondunet assisted by ChatGPT (OpenAI)
