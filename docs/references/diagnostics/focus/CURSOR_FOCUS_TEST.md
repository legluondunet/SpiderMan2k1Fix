# Windows 11 — automatic desktop cursor recovery (PR #3)

**Bug:** the Windows pointer stays interactive but invisible on the desktop
after switching away from Spider-Man 2001 with Alt+Tab.

## Fix

The game can hide the Windows pointer using `ShowCursor`. On a real
foreground → background transition, the DirectDraw Flip hook observes
`GetCursorInfo` **before** any optional pause, and only if the pointer
is hidden, calls `ShowCursor(TRUE)` up to 16 times to make it visible.
At focus return, it undoes **exactly the increments it performed**
with `ShowCursor(FALSE)` and resets its balance counter. It does
not try to enforce visibility while Spider-Man has focus.

Previously, this required two diagnostic environment variables.
Now `[Input] RestoreCursorOnFocusLoss=1` enables it **by default**,
including for older INI files that lack the new key. Set `0` to disable.

The normal path only tracks the game HWND at its existing Flip boundary.
It **does not** subclass WndProc, install DirectInput hooks, reacquire
input devices, change clipping or set window Z-order. Detailed passive
WndProc traces remain available with
`SPIDERMAN2K1FIX_FOCUS_PASSIVE=1` as an explicit diagnostic. Z-order
remains strictly opt-in and unrelated.

## Final native Windows 11 test (no environment variables required)

Install **both compiled DLLs from this PR's latest successful Actions
artifact** alongside the original `SpideyPC.exe`.
Use this normal INI configuration:

```ini
[Input]
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
PauseOnFocusLoss=1
RestoreCursorOnFocusLoss=1

[Debug]
Log=1
```

The order of INI sections is irrelevant; do not add a duplicate section.
Remove the previous diagnostic switches in your PowerShell session:

```powershell
Remove-Item Env:\SPIDERMAN2K1FIX_FOCUS_PASSIVE -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_CURSOR_VISIBILITY_TEST -ErrorAction SilentlyContinue
Remove-Item Env:\SPIDERMAN2K1FIX_ZORDER_TEST -ErrorAction SilentlyContinue
Remove-Item .\SpiderMan2k1Fix.log -ErrorAction SilentlyContinue
$p = Start-Process .\SpideyPC.exe -Wait -PassThru
"Code de sortie : $($p.ExitCode)"
Select-String .\SpiderMan2k1Fix.log -Pattern 'INPUT-CURSOR|CURSOR-TEST|FOCUS-PAUSE|WINDOW-ICON'
```

Alt+Tab repeatedly while the game is in the menu **and** gameplay.
On the desktop, confirm the cursor is visible and can click things.
On return, check input and rendering, then exit normally (code 0).

Expected log on an initially hidden desktop pointer:

```text
INPUT-CURSOR: automatic desktop cursor restore ENABLED (INI; Flip-only, no input hooks)
INPUT-CURSOR: tracking game HWND only (no WndProc subclass or DirectInput hooks)
INPUT-WINDOW: CURSOR-TEST background ShowCursor(TRUE) calls=N final-count=0
INPUT-WINDOW: CURSOR-TEST foreground balanced ShowCursor(FALSE) calls=N final-count=...
```

`CURSOR-TEST` messages are logged **only when a counter adjustment is
needed**. If Windows already shows the pointer, none is required.

### Opt-out control

For an A/B check, change **only** this INI value to
`RestoreCursorOnFocusLoss=0`, restart the game, and check for:

```text
INPUT-CURSOR: RestoreCursorOnFocusLoss=0; automatic cursor restore disabled
```

With the opt-out enabled, the DLL must never call
`ShowCursor(TRUE)`/ `ShowCursor(FALSE)` via the cursor correction,
even during an Alt+Tab cycle. Return to `1` afterward.

An additional trial with `PauseOnFocusLoss=0` is useful to test
the stock INI default; it does **not** change the cursor option, but
the correction can run only if the game continues reaching a Flip
when focus changes.

**PR remains unmerged pending this final native Windows check.**
Wine testing deferred. The taskbar overlay bug is out of scope.

---

legluondunet assisted by ChatGPT (OpenAI)
