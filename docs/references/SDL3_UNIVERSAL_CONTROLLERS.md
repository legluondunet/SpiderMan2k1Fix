# SDL3 universal controller support — implementation roadmap

## Acceptance contract

A user opens **Joystick Configuration** with the keyboard, selects any
SDL3-visible device (including one already mapped but incorrectly), calibrates
its physical controls if needed, assigns actions, and saves its profile. A
fully supported controller must control **the entire game**, not just its
3D avatar, without any vendor-specific code or switch back to the keyboard.

**Target behavior to implement and physically validate:**

| Original-game context | Controller acceptance criteria | Current state |
| --- | --- | --- |
| Main and pause menus, selection screens | D-pad/left stick moves selection; configurable Confirm/Back; reliable navigation and return | Partial Sony/Xbox-specific 2D/UI paths; NOT universal |
| Bink/videos (including startup) | Deliberate Confirm/Skip action skips a skippable video; never automatically skips multiple videos after held input | Partial existing physical Cross/A bridge; NOT universal |
| Scripted/in-engine 3D cutscenes | Skip supported cutscenes with the saved SDL3 action while respecting native skip rules | Existing game-native skip only; NOT universal |
| Fixed entry/continue image or still | Confirm/Next Still action advances the frozen screen that asks for Enter | Unbridged legacy still pages |
| Gameplay 3D | Movement, camera, seven gameplay actions, pause/resume, reconnect | Sony DS4/Xbox 360-specific native DirectInput path only |
| Focus loss, Alt+Tab, controller hotplug | Input disarmed on focus loss; no unintended skip, pause, movement or stale held action on return | Must be verified for all contexts |

The configuration must respect the **context**: the same hardware button
may correspond to different menu, video, still, cutscene and 3D actions.
For menu navigation, Confirm and Back must remain distinct; for videos and
stills, only a fresh intentional press should advance. Keep F9 screenshots,
F10 game location shortcuts, original keyboard controls, and a safe legacy
fallback unchanged. Avoid double-input from original DirectInput and SDL3.

These are **release-blocking acceptance criteria**, not a description of
the currently functioning implementation.

## Implementation plan

1. **Detection:** SDL's physical joystick list and mapped gamepad list are
   both necessary. A device may already be SDL-mapped yet have the wrong
   assignments; it must remain recalibratable.
2. **Hardware wizard:** on the foreground game/SDL3 thread, open the explicitly
   chosen joystick; capture buttons, hats, axes and triggers; reject ambiguous
   simultaneous input; require neutral/release edges; allow keyboard correction.
   Obtain SDL GUID, validate mapping, apply with SDL_AddGamepadMapping,
   persist per GUID transactionally, reload at startup.
3. **Semantic action profile:** let the player bind 3D actions and all
   context-specific commands (MenuConfirm, MenuBack, SkipVideo, NextStill,
   SkipCutscene) without hardcoded physical button numbers or VID/PID.
   User saves must survive restart and device reconnect.
4. **Context-aware adapter:** hook only verified original retail-engine input
   consumers. Translate SDL3 normalized actions and axes to appropriate
   game-native operations for each actual state: menus, Bink video,
   scripted cutscene, still/Enter screen and 3D gameplay. Use disjoint
   context routing and consume/arm press-release boundaries so one action
   cannot cross states.
5. **Safety:** never inject unrelated Win32 events globally, modify
   unsupported game memory or double-activate original DirectInput actions.
   On keyboard/focus/controller changes fall back safely; F9/F10 untouched.
6. **Acceptance tests:** on native Windows and Wine/Proton, verify
   DS4, Xbox 360, PS3 (`054C:0268`) and a previously unmapped controller,
   starting from startup videos through a complete level and return to menus.

## Current experimental state (2026-11-01)

- SDL3 discovers both mapped and raw joysticks. A mapped device can be
  recalibrated using **Calibrate SDL3 joystick**, reachable by Tab+Enter
  inside the game's SDL3 setup modal.
- An optional `SDL3-gamecontrollerdb.txt` extends SDL's mapping database.
  The wizard generates SDL mappings and saves them under the game installation
  or the user-writable LOCALAPPDATA fallback.
- `sdl3_universal_actions.h` provides a portable semantic action state
  (held/pressed/released, focus and neutral rearm, trigger hysteresis)
  **not yet connected** to all game consumers.
- The existing user-specific per-model semantic profile file can be saved,
  but this does not imply any in-game 3D/video/menu compatibility.
- Current native controller ownership still restricts live gameplay to the
  previously verified Sony DS4 / Xbox 360 adapters. Original game input
  cannot yet consume generic SDL3 input.
- **2026-11-01 hardware acceptance: raw calibration 20/20 CONFIRMED.**
  User validated on Manjaro + GE-Proton 11-11 / Wine 11.0 with the PS3
  `054C:0268`, exposed to SDL3 as `XInput Controller #1` (SDL ID=3).
  In `SpiderMan2k1Fix(20261101-162451).log`, wizard records **every
  step 1/20 through 20/20**, including face buttons, Back/Start,
  shoulders, stick clicks, four D-pad hats, four stick axes and both
  trigger axes. Exact GUID
  `0300fa674c0500006802000011817801` persists successfully as a
  per-GUID SDL mapping at `location=GAME`; log contains
  `SDL3-RAW-WIZARD: completed and persisted SDL3 normalized mapping`.
  The Win32 focus isolation (disabling both combo and bottom buttons)
  has passed this physical capture test. **Restart/reload of the newly
  saved mapping has not yet been tested**; initial `loaded=0` on that
  launch preceded mapping creation. This does **not** validate gameplay.
- **Game ownership still refused for this PS3** even after calibration:
  `SDL3-OWNER: START REFUSED unsupported native controller` and
  `SDL3-PROBE: SETUP RETURN REFUSED stale/ambiguous modal owner`.
  The existing native DS4/Xbox-only bridge remains unchanged.
- The F9 screenshot and F10 in-game location shortcuts remain reserved.

## Outstanding

- Re-launch the game and prove saved per-GUID mapping reloads (`SDL3-MAPPINGS: ... loaded=1`) and is usable for SDL3 normalized input after restart.
- Improve analog-axis polarity, physical trigger neutrality and UI recovery.
- Implement and verify the **universal 3D + menu + video + cutscene +
  still/Enter** context-aware bridge.
- Cross-device physical Windows and Wine testing and explicit user sign-off.

No declaration of "universal controller support" before every context above
passes real-hardware acceptance. Keep this draft outside protected main
until the maintainer authorizes integration.

legluondunet assisted by ChatGPT (OpenAI)
