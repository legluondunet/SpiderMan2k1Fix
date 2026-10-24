# External DirectInput8 diagnosis — Windows 11 main-menu Alt+Tab

Status: investigation only. No game/DLL COM instrumentation changes.

## Why this test

The focus-only logs establish that Windows restores game keyboard focus after
Alt+Tab/F9, but the main menu no longer responds. The factory-only
`DirectInput8Create` hook succeeded. Our own `IDirectInput8::CreateDevice`
COM-vtable replacement changed startup behavior: `E_INVALIDARG 0x80070057`
and no device. Do not revive that wrapper or infer that native DirectInput8
itself returns `E_INVALIDARG`.

**Goal:** observe the native game's `CreateDevice`, `Acquire`,
`Unacquire`, `GetDeviceState`, `GetDeviceData` results before/after the
first Alt+Tab, not force any input or alter returned HRESULT values.

## Safe baseline

Use the last known-booting PR #7 build and back up the installed DLLs. In the
existing `SpiderMan2k1Fix.ini` use:

```ini
[Debug]
Log=1

[Input]
PauseOnFocusLoss=1
PauseKey=F9
ExperimentalFocusTrace=1
ExperimentalDI8EntryTrace=0
ExperimentalDI8CreateDeviceTrace=0
ExperimentalAltTabReacquire=0
RestoreCursorOnFocusLoss=1
```

The externally attached debugger is the only DirectInput8 observer. Turning
off the factory-only probe avoids duplicate hook behavior while inspecting
the native interface.

## Phase A — capture an unmodified native CreateDevice result

1. Launch `SpideyPC.exe` from **x32dbg** (not x64dbg: game is 32-bit).
   Start before the game creates DirectInput devices, rather than attaching
   after the menu is already displayed.
2. At the debugger's initial pause, use Symbols/Modules to locate
   `dinput8.dll`. Break on the exported `DirectInput8Create` function
   when it is loaded (or set a deferred symbolic breakpoint).
3. At the first call, record `version`, `riid`, `ppvOut`, and the caller's
   return address. Under x86 stdcall, the five arguments at function entry
   begin at `[ESP+4]`, `[ESP+8]`, `[ESP+0xC]`, `[ESP+0x10]`,
   `[ESP+0x14]` respectively. `[ESP+0x10]` is the **address of the
   output pointer**, not the interface pointer itself.
4. Run until the native factory returns. Note `EAX` (HRESULT) and
   `*ppvOut`. An HRESULT of zero means success. Read the pointer to its COM
   vtable from `*ppvOut`. Do not edit memory.
5. The **IDirectInput8 CreateDevice method is vtable slot 3** (byte offset
   `0x0C` for x86). Set a normal breakpoint on the function pointer
   stored there. Log the `rguid` device GUID, `ppvOut`, and returned
   HRESULT; do not patch or redirect the method.
6. If the native method returns a device object, note its address and
   vtable. For `IDirectInputDevice8`, these methods use zero-based vtable
   indices: `Acquire=7` (`+0x1C`), `Unacquire=8` (`+0x20`),
   `GetDeviceState=9` (`+0x24`), `GetDeviceData=10` (`+0x28`).
   The offsets are **relative to the returned vtable pointer** (not to the
   device object itself). Verify the pointers reside in executable memory
   before using them as breakpoint addresses.

## Phase B — compare before and after one Alt+Tab in the main menu

1. Let the game run to its main menu. Test that keyboard/mouse work before
   switching apps.
2. With debugger *log-only, auto-continue* breakpoints on the native device
   method addresses, collect call counts and HRESULT return values for a few
   seconds with game foreground. Ensure recording overhead stays manageable;
   sample/limit calls rather than logging every polling call at 30–60 FPS.
   x32dbg's conditional-breakpoint editor supports log text with a break
   condition `0`, so logged hits need not pause the game.
3. Perform **one** Alt+Tab away and back, then F9 to resume. Do not
   repeatedly cycle apps or force a new DirectInput hook.
4. Compare call counts and statuses. Specifically note if
   `GetDeviceState`/`GetDeviceData` stop being called; or continue to
   return `DIERR_INPUTLOST`/`DIERR_NOTACQUIRED`; or return `DI_OK`
   while the menu is inert.
5. Save the debugger log and the fix's normal text log. Also note whether
   the same game build remains interactive in the first 3D gameplay scene.
   If the debugger itself changes behavior or no COM device is created,
   record that fact instead of guessing at a fix.

Avoid enabling `ExperimentalDI8CreateDeviceTrace=1` or
`ExperimentalAltTabReacquire=1`: their old COM hooks remain quarantined.
Do not use `SetFocus`, `SetForegroundWindow`, injected key presses, or a
forced `Acquire` to mask the bug during observation.

## Interpreting the result

- Calls stop after Alt+Tab: inspect game's own menu input polling/activation
  state, not the DirectInput API implementation.
- Reads continue with `DIERR_INPUTLOST` or `DIERR_NOTACQUIRED`: inspect
  the game's reacquisition path and native `Acquire` calls.
- Reads return `DI_OK` but menu is inert: inspect game-state/UI processing.
- Devices never created in native mode: validate module selection and
  whether the menu uses an alternative input path.

All code changes must be proposed within PR #7; never merge without explicit
user approval. PR descriptions must contain
`legluondunet assisted by ChatGPT (OpenAI)`.
