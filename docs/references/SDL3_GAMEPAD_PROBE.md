# SDL3 read-only gamepad discovery (draft stacked PR)

This PR branches from `feat/xbox360-native-hotplug` (PR #77), which
already includes the Sony DS4 / Xbox360 DirectInput hotplug work and 40/60/40
factory audio levels. **No merge or release is authorized.**

## Current scope: passive diagnostic, not an input backend

* `[Input] ExperimentalSDL3Probe=1` opts into SDL3 3.2+ enumeration and
  isolated one-player selection. **Default is 0 (off)**. This option is
  intentionally absent from the generated user INI.
* Manually place the **official Windows x86 (32-bit)** `SDL3.dll` beside
  `SpideyPC.exe`. This experimental build does NOT bundle SDL3.dll and does
  NOT link the DLL at load time. Never install a 64-bit SDL3.dll in the game.
* Enable `[Debug] Log=1` to inspect `SDL3-PROBE` messages.
* SDL3 is initialized only on the game Flip thread, after Flip #120, while
  the Spider-Man window has focus; only GAMEPAD/JOYSTICK/EVENTS subsystems are
  requested. No SDL video/audio subsystem or new threads are created.
* SDL3 enumerates all joysticks and mapped gamepads independently. We log
  counts, gamepad instance IDs, attach/detach, active ID, and the independent
  normalized LT/RT axes. A joystick without a supported mapping is counted
  but not opened as a Gamepad.
* Selection is by **SDL joystick instance ID**, never by a VID/PID shortlist
  or device index. One mapped pad is selected automatically. If multiple
  pads are present, the existing selection is retained; when selection is
  empty, a neutral -> fresh Start press claims one. Other pads remain silent.
  Unplugging the active gamepad clears ownership and allows re-selection.
* **The SDL3 input is NEVER sent to the game.** The validated DirectInput
  menu bridges, 3D gameplay, native unplug guard, native rebind, and keyboard
  paths remain untouched. This means two real controllers may still affect
  native gameplay while this passive probe runs; single-controller ownership
  is only a PROPOSED policy pending the future input adapter.

## Safety and validation

Run `python3 -m pytest -q ChatGPT/tests` and build both x86 MSVCRT DLLs
with `bash ./build-native-msvcrt.sh`. An opt-out or absent SDL3.dll must
preserve identical gameplay and dependencies. The dynamic SDL3 ABI subset
matches SDL 3.2+; no SDL headers or binaries are vendored.

On Windows and Wine, physically test no pad, one pad, two mapped pads,
mapped plus unmapped, two unmapped; hold Start while attaching; unplug active
during 3D; switch DS4 -> Xbox -> DS4; ALT+TAB. Compare the unchanged
legacy gameplay and `SDL3-PROBE` logs. SDL3 enumeration may be imperfect
across Wine drivers, so no universal compatibility claim yet.

## Next separate milestones

1. Optional external `gamecontrollerdb.txt` mapping loading with validated
   SDL3 API and library-compatible license/provenance.
2. Explicitly exclusive input owner: safely bridge a single normalized SDL3
   gamepad into the original game input at its verified read boundary;
   completely prevent concurrent native DirectInput input duplication.
3. Verify latency (no extra frame), stick precision, trigger polarity,
   cinematic/menu actions and all established hotplug/focus safety cases.
4. Only after real hardware validation consider promoting SDL3 to default
   and retiring model-specific DirectInput branches.

Documentation reference: https://wiki.libsdl.org/SDL3/SDL_GetGamepads

## Multi-controller hardware smoke test (2026-10-28)

Windows log `SpiderMan2k1Fix(20261028-205126).log` confirms SDL3
`physical-joysticks=2 mapped-gamepads=2 opened=2 active=0` at startup:
DS4 instance 1 plus Xbox360 instance 2. When one pad was unplugged, SDL3
automatically selected the sole remaining mapped pad; reattached DS4 was
instance 3 and reattached Xbox was instance 5. The previously validated
native DirectInput unplug guard and native rebind also recovered.

**Regression fixed:** first prototype retained empty slots after detaching
a pad, repeatedly emitting `SDL3-PROBE: detached id=...` (164 duplicates
in one test log) and invoking `SDL_CloseGamepad(NULL)` on later scans.
The slot lifecycle now closes each non-NULL device once and compacts the
live array before each discovery. A portable C99 test performs 100 repeat
scans and confirms no repeated close or detachment event.

Hardware **has not yet revalidated the fix**; SDL3 remains read-only and
the legacy game will still respond only to the independently selected
DirectInput device. The two-pad Start-to-select pathway was not exercised
in the supplied test (active=0 until a single pad remained).
