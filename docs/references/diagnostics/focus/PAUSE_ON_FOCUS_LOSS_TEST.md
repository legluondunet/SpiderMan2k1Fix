# Windows 11 persistent pause prototype (experimental)

> **CURRENT SAFETY STATUS (2026-10-21):** Windows 11 verified the experimental
> `IDirectInput8::CreateDevice` COM interception returns `E_INVALIDARG`
> (0x80070057) and prevents game startup. Both
> `ExperimentalDI8CreateDeviceTrace` and legacy
> `ExperimentalAltTabReacquire` are now **quarantined in code**, regardless
> of their INI values, with diagnostic warnings. Do not use the old
> CreateDevice test procedure below; it is retained as historical context.
> `ExperimentalDI8EntryTrace=1` remains a separate, working factory-only
> probe. The menu still loses keyboard and mouse after Alt+Tab; this fix
> prevents a new startup regression, not the original input issue.

Branch: `experiment/win11-manual-persistent-pause-20261021`

This new experiment changes the old focus-loss presentation-loop pause: it
latches the paused state, and regaining focus does NOT resume the game.
Only a new press of the F9 key while the game is foreground
can resume. Pressing it while the game is in background has no effect.
The same key can request manual pause while the game is active.

The experiment is limited to the DirectDraw Flip presentation loop: it does
NOT yet guarantee suspension of Bink video or sound, and it does not draw a
PAUSE banner. Do not treat this as a complete game pause.

## Setup on native Windows 11

1. Download the Windows x86 artifact from this branch's CI run if available.
2. Back up the existing DLLs. Install the new DLLs alongside SpideyPC.exe.
3. In SpiderMan2k1Fix.ini set:

```ini
[Debug]
Log=1
[Input]
PauseOnFocusLoss=1
ExperimentalAltTabReacquire=0
ExperimentalFocusTrace=0
RestoreCursorOnFocusLoss=1
```

4. Launch the game and press F9 during gameplay. It should stop at
   the next DirectDraw Flip and stay stopped. Press F9 again to resume.
5. Repeat with Alt+Tab during gameplay. Returning focus must NOT resume the
   game; press F9 to resume.
6. Test menus and cutscenes separately, noting that video/audio may still
   continue in this first experiment.
7. Repeat Alt+Tab at least 20 times; check input recovery and crash behavior.
8. Quit normally, keep SpiderMan2k1Fix.log and note the game's exit code.

### Diagnostics

Look for `FOCUS-PAUSE: focus lost; pause latched until manual resume`,
`FOCUS-PAUSE: waiting for foreground and explicit F9 key`, and
`FOCUS-PAUSE: manual resume requested` in SpiderMan2k1Fix.log.

This prototype must not be merged until real Windows 11 tests are validated.

---

legluondunet assisted by ChatGPT (OpenAI)

## Focus-loss filtering: left-click regression test

This revision requires a foreign foreground process to remain active for at
least 150 ms before latching a pause. A transient NULL foreground or another
window belonging to the game process is ignored. A click inside the already
focused Spider-Man window must not trigger a pause.

Test on Windows 11: in menus and gameplay, click the left mouse button
repeatedly without leaving the game, then Alt+Tab to a different app and back.
The click must not pause; Alt+Tab must pause, and F9 must resume once the game
is foreground. Confirm logs contain 'confirmed external focus loss' only for
true application switches. Audio/movie synchronization remains out of scope.

## Windows 11 regression: safe focus trace

With ExperimentalAltTabReacquire=0 and ExperimentalFocusTrace=1,
the tracer now observes window events and Flip snapshots without modifying
the DirectInput8 import or COM vtables. This intentionally does not collect
Acquire/GetDeviceState device-level calls; do not interpret zero tracked
DirectInput devices as proof that the game is not reading input.

First confirm that the game reaches the intro and menu with this combination.
Then reproduce Alt+Tab, return, resume with the configured pause key, and
record the resulting log. If a startup crash persists, switch
ExperimentalFocusTrace back to 0. Keep reacquisition disabled.


## Windows 11: isolated DirectInput8 factory probe (experimental)

The previously crashing DirectInput8 COM-vtable tracer remains **off**. For a
single controlled experiment, set these options in the user INI:

```ini
[Debug]
Log=1
[Input]
PauseOnFocusLoss=1
PauseKey=F9
ExperimentalFocusTrace=1
ExperimentalAltTabReacquire=0
ExperimentalDI8EntryTrace=1
RestoreCursorOnFocusLoss=1
```

The new `ExperimentalDI8EntryTrace` option defaults to **0** and requires
`ExperimentalFocusTrace=1`. It intercepts only the EXE's supported
`DirectInput8Create` import, records the native return code and interface
pointer, and returns the COM object unmodified. It does **not** inspect
`CreateDevice`, `Acquire`, `GetDeviceState`, or `GetDeviceData`; it
cannot repair lost controls by itself. A new startup crash would implicate
the factory/IAT observation independently of the disabled COM instrumentation.

Check for `INPUT-DI8: ENTRY-ONLY PROBE enabled`,
`INPUT-DI8: DirectInput8Create IAT hooked`, and
`INPUT-DI8: DirectInput8Create version=... hr=...`. Reach the main menu,
Alt+Tab once, return, resume with F9, try keyboard/mouse, and save the log.
If the game fails to start, set `ExperimentalDI8EntryTrace=0` immediately.
Do not enable `ExperimentalAltTabReacquire`.



## Windows 11: CreateDevice-only second-stage diagnostic

The 2026-10-21 entry-only test confirms successful
`DirectInput8Create(...)=0x00000000` and a valid returned interface while the
game enters the main menu. The log also confirms window focus returns on F9,
but it ends with a subsequent pause, so it cannot establish control recovery.

Use the latest compiled Windows x86 artifact, and retain the existing
settings from the entry-only test. Set the second opt-in:

```ini
[Input]
ExperimentalFocusTrace=1
ExperimentalDI8EntryTrace=1
ExperimentalDI8CreateDeviceTrace=1
ExperimentalAltTabReacquire=0
```

This stage replaces only the `IDirectInput8::CreateDevice` entry in a private
copy of the returned interface vtable. It logs the native result, forwards
output without altering device vtables, and never calls `Acquire`. It is
nonetheless more invasive than the validated factory-only test and could
regress startup. The options default to zero in factory-generated INIs.

If the game does not start, immediately revert only
`ExperimentalDI8CreateDeviceTrace=0` while keeping the known-working factory
probe. If the game starts, Alt+Tab **once** from the main menu, return, resume
with F9, test navigation and provide the new log. Look for
`INPUT-DI8: IDirectInput8::CreateDevice hr=...`. Do not enable automatic
reacquisition yet.



## Windows 11: opt-in Any-key focus-pause resume (experimental)

New selection in `[Input]`:

```ini
[Debug]
Log=1
[Input]
PauseOnFocusLoss=1
PauseKey=Any
ExperimentalAltTabReacquire=0
ExperimentalDI8CreateDeviceTrace=0
```

For the independently validated menu input reacquisition, launch the game with
`SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1` (still controlled separately from
the INI). Retain the existing ProcDump launch command.

**Behavior:** After a real focus loss, the game pauses at its next
DirectDraw `Flip`. Switching back to the game does not auto-resume. Once
the Spider-Man window is foreground, pressing **any keyboard key** starts a
single-use resume sequence. The experimental Win32 `WH_KEYBOARD_LL` hook
suppresses foreground Win32 keydown, repeat, and keyup events; the pause
releases **only after all intercepted keys have been released**. This avoids
the first wake key remaining held during the next game frame. `PauseKey=Any`
**does not** let ordinary gameplay keys manually trigger a pause. The default
remains `PauseKey=F9`; `PauseKey=Space` is still accepted.

The hook is installed only while this focus-loss pause is active and removed
on resume/quit. It always chains events when another application is
foreground. If installation fails it logs an error and falls back to F9 for
that process. Do not treat the hook as complete interception of
DirectInput/Raw Input; that part **requires validation on native Windows 11**.
Likewise the pause still only blocks Flip, not asynchronous media/audio.

**First test:** from the menu, Alt+Tab away and return, use a direction key,
Enter and then Space in separate sessions to resume. Check whether the wake
key causes any unexpected menu navigation or selection. Inspect
`FOCUS-PAUSE: Any keyboard resume ARMED` and
`FOCUS-PAUSE: Any-key resume swallowed virtual-key=...`. Then test
cinematics and 3D gameplay with ProcDump. If any wake key leaks into gameplay
or Alt+Tab becomes unreliable, revert `PauseKey=F9` in the INI and keep PR #7
unmerged. Do not test system-reserved combinations as wake keys.

This trial is not a release-ready guarantee of native DirectInput suppression.


## Regression investigation: PAUSED pad wake after Alt+Tab (2026-10-28)

The maintainer reports that the digital controller buttons do **not**
resume our custom focus-loss PAUSED banner. The runtime log
`SpiderMan2k1Fix(20261028-173412).log` is from the older
`v1.1.0-14e56bb` DLL, starting with one Sony DualShock 4.
The game entered PAUSED on focus loss and armed digital pad wake.
On return to the exact game HWND, the **first secondary DirectInput
GetDeviceState failed**, causing the earlier implementation to
permanently disable the pad wake path for that PAUSED session.
The keyboard SPACE press/release resumed normally, followed by
secondary DirectInput failures `0x8007000C` (DIERR_NOTACQUIRED)
in the resumed Flip loop. The **PAUSED read's HRESULT is not
present** in that older log, so its exact value remains unproven.

Candidate fix: the next experimental build allows exactly one
`IDirectInputDevice8_Acquire` attempt **inside PAUSED**, but only
when the already-verified secondary device's GetDeviceState returned
`DIERR_NOTACQUIRED` **and the exact game HWND is foreground**.
A successful Acquire does not wake immediately: neutral input then
a *new digital button press + full release* remains required.
`DIERR_INPUTLOST`, `DIERR_UNPLUGGED` and all other errors
still fail closed to keyboard wake, with a precise HRESULT now logged.
No native joystick COM pointer, input mapping or SendInput is touched.

Test: Alt+Tab away from the main menu, return, release any buttons,
press and fully release Cross/Circle, verify the banner disappears
and the same physical press is **not** delivered to the game menu.
Confirm a deliberate fresh press still navigates the native menu.
Repeat from playable 3D. Verify the log contains
`FOCUS-PAUSE-GAMEPAD: foreground secondary DIERR_NOTACQUIRED...`
or the exact error which prevented wake. If a physical unplug occurs
while PAUSED, use keyboard to resume; leave native rebind to the
normal subsequent foreground Flip.
