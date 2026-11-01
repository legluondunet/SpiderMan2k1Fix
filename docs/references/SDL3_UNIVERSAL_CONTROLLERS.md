# SDL3 universal controller support — implementation roadmap

## User contract

With **keyboard only**, a user opens the SDL3 Controller Setup screen,
chooses a recognized pad or a raw joystick, maps the hardware once, saves,
and can play without the source code knowing its manufacturer or VID/PID.
The keyboard remains the recovery route throughout.

This is the **target**, not a description of the current release.

## Baseline and safety rules

1. **Detection:** `SDL_GetJoysticks` sees raw physical joysticks;
   `SDL_GetGamepads` lists those with SDL-standard mappings. An unknown
   raw joystick must be visibly labelled **unmapped**, not silently omitted
   or described as gameplay-compatible.
2. **Hardware calibration:** on the game's existing foreground UI thread,
   open a specifically user-selected raw joystick; capture buttons, hats,
   positive/negative axes and triggers; verify neutral and release edges;
   let the user correct mistakes with the keyboard. Obtain a stable SDL
   joystick GUID, build and validate an SDL gamepad mapping string, apply it
   via `SDL_AddGamepadMapping` and persist it transactionally. Reload on
   startup, then re-enumerate as a standard SDL gamepad.
3. **Action configuration:** once SDL3 maps a device, use its normalized
   buttons, triggers and axes to assign gameplay actions. Profiles are
   model-specific data, not model-specific code.
4. **Universal game bridge:** convert selected SDL3 *action state*, not
   vendor-specific DirectInput `rgbButtons` indexes, into the original
   game command reader. Investigate the retail x86 consumer and establish
   that every action, movement, pause and release can be delivered and
   restored safely. Never generate duplicate native and SDL3 actions.
5. **Fail closed:** no blanket memory writes or unverified DI COM hooks;
   disarm on focus loss, unplug, profile change or pause. Retain the
   original keyboard and a configurable legacy-input fallback.
6. **Compatibility:** use Sony DS4, Xbox 360, DualShock 3 and a third,
   previously unknown raw joystick as test devices; model-specific
   workarounds should be narrowly documented exceptions.

## Current implementation on this branch

- `sdl3_gamepad_probe.c` now logs previously unseen SDL raw joysticks
  lacking a standardized SDL gamepad mapping. It does **not** yet open or
  configure them.
- `sdl3_universal_actions.h` implements a portable normalized
  button/trigger action frame: held, pressed, released; identity switch,
  neutral rearming, focus and gameplay gates, trigger hysteresis. It does
  **not** yet connect to the original game input consumer.
- `sdl3_raw_mapping.h` validates a user-calibrated standard SDL mapping\n  string (GUID, safe name, button/hat/axis identifiers, required inputs)\n  without applying it to SDL yet. The keyboard capture UI remains missing.\n- A local `SDL3-gamecontrollerdb.txt` can be loaded via SDL's existing\n  `SDL_AddGamepadMappingsFromFile` ABI at startup; this is an optional\n  source of externally supplied mapping entries, not auto-calibration.\n- Portable regression coverage exercises unknown VID/PID semantics,
  remapping, no inherited held keys and hysteresis.

## Not yet implemented

- The raw joystick identification/capture wizard.
- SDL mapping creation, validation and persistence.
- A verified hardware-neutral injection path into the original retail
  Spider-Man 3D engine.
- End-to-end user acceptance testing and compatibility across devices.

Do not describe any unsupported raw joystick as playable yet. Do not
replace the current verified DS4/Xbox behavior until the universal path
has passed full native Windows and Wine tests.

legluondunet assisted by ChatGPT (OpenAI)
