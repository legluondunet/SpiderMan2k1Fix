# SDL3 universal controller — hands-on hardware test (draft PR #96)

This protocol verifies **SDL3 normalization, raw recognition and persistent
hardware mapping**, NOT universal 3D gameplay input. The game's original
DirectInput and the existing DS4/Xbox integration are intentionally unchanged.

**Reserved shortcuts:** F9 remains the game's screenshot shortcut; F10 remains a game teleport/location shortcut. Neither is assigned to SDL3 diagnostics. The SDL3 tools are buttons reachable with Tab + Enter.

## Test A — existing mapped pad (Sony DS4 or Xbox 360)

1. Download the **Windows x86** artifact from the newest successful GitHub
   Actions run for PR #96. Unzip the game installation files as usual.
2. Start Spider-Man with one controller connected. Use the **keyboard** to
   open the original Joystick Configuration entry; it opens the SDL3 setup.
3. Use **Tab** to focus **Test SDL3 inputs**, then press **Enter**.
   The status becomes `SDL3 TEST ON`. **All dialog controls, including
   the Test button, dropdown, action list and Close, become disabled**;
   focus moves to the inert parent window, so Wine's gamepad-to-keyboard
   events cannot click them. Press each face button, SHARE/BACK, START,
   L3, R3, and both sticks and triggers. Note the
   `SDL3 TEST: id=... buttons=... LX=... LY=... RX=... RY=... LT=... RT=...` lines.
   Check that the `id` matches the **selected PS3 or other controller**.
4. On the tested Sony `054C:09CC` under Wine, earlier independent logs
   observed physical SHARE as SDL bit `0x000010` and L3 as
   `0x000100`. Use these as comparison only, **not** as a universal
   required mapping: other drivers might differ and that is the point.
5. **Do not use the controller or click disabled buttons to leave
   test mode.** Press the **physical keyboard Escape** to stop diagnostic
   capture and re-enable the dialog controls. Because Proton sometimes
   emulates Escape when Circle/B is pressed, this new shortcut is accepted
   only from Windows **keyboard Raw Input** (`WM_INPUT`), not an ordinary
   synthetic `VK_ESCAPE`. If the raw keyboard API is unavailable or already
   owned by another component, use the retained fallback **Ctrl+Shift+T**
   to stop, or **Ctrl+Shift+Q** to close the setup.
6. Confirm the game still responds to the original keyboard and controller
   controls, including a subsequent Alt+Tab and return.

## Test B — calibrate an already-mapped or unmapped SDL joystick

1. Connect the SDL joystick you want to calibrate (including an already SDL-mapped device such as the PS3 054C:0268). Open SDL3 Controller Setup with
   the keyboard, use **Tab** to select **Calibrate SDL3 joystick**, then **Enter**. The wizard lists **all** SDL joysticks with their names and either `Mapped - recalibrate` or `Unmapped - recognize`.
   If no devices appear, SDL3 is not exposing a joystick at all; the log will show the device inventory.
2. Select the intended physical joystick by **name** using the keyboard; press **Enter**. Already-mapped devices can be recalibrated; the existing mapping is not replaced until the wizard completes successfully.
3. The assistant walks through **20** SDL-standard controls. As soon
   as capture starts, the device dropdown **and all three bottom buttons**
   (Start/Restart, Skip, Cancel/Close) should become **disabled/grayed out**.
   Their Win32 click notifications are also rejected while SDL3 captures.
   Keyboard focus moves to the inert wizard window. Release each button
   before pressing the next. Verify that **B/Circle** and **X/Square**
   advance the wizard WITHOUT opening the device list or activating
   any of the three bottom buttons. Also test left-stick directions when
   the wizard reaches steps 15/16: they must not change keyboard focus.
4. During capture, gamepad-emulated Win32 Enter/Escape/Space and
   arrow messages are discarded before dialog navigation. A **physical
   keyboard Escape** now cancels calibration (and does not save any
   unfinished mapping) **only if Raw Input keyboard verification succeeds**.
   The hardware-input fallback is **Ctrl+Shift+Q** to close; also use
   **Ctrl+Shift+S** to skip an optional mapping and **Ctrl+Shift+R**
   to restart. A and B and both left-stick axes remain mandatory;
   physical Start must not reset the wizard.

5. A successful capture displays `SDL3 CALIBRATION SAVED`. The log must
   contain `SDL3-RAW: MAPPING SAVED GUID=... location=GAME` or
   `location=LOCALAPPDATA`. If save fails, send the log; do not interpret
   an in-memory mapping as permanently saved.
6. Close the raw wizard and SDL3 setup, **restart the game**, reopen SDL3
   setup and check that the joystick appears as a *mapped* SDL3 gamepad.
   Its per-model action profile should be assignable if SDL exposes a
   valid vendor and product ID.
7. Re-enter Test A with this controller, if selectable, to verify normalized
   SDL3 button and trigger data. Do **not** assume it works in 3D yet.

## What to attach to the next report

- Full `SpiderMan2k1Fix(...).log`.
- Which test (A/B), device VID:PID (if known), Wine/Windows runtime.
- For Test A: whether the Test button and all controls become gray,
  whether the diagnostic stays **ON** during Cross/Circle and the sticks,
  whether the SDL button mask changes on SHARE/BACK/L3, whether the
  right/left stick readings change, whether Ctrl+Shift+T stops testing,
  and whether the keyboard regains control afterward.
- For Test B: specifically note whether **B/Circle**, **X/Square** or moving the
  **left stick** reactivates any disabled bottom button or opens the dropdown. Attach the
  `SDL3-RAW-WIZARD: capture UI ISOLATED` and
  `suppressed Win32 keyboard` log lines, the furthest captured step,
  whether the mapping saved and whether the game re-detected it after restart.

## Known limitations

- SDL must see the physical device as an SDL joystick. Unsupported hardware
  invisible to SDL cannot be calibrated with this wizard.
- The wizard currently samples up to 32 buttons, 16 axes and 4 hats.
- Only basic `bN`, `aN` and `hN.V` SDL mapping sources are supported;
  axis polarity/inversion and split-axis capture still need work.
- An SDL-mapped controller with no usable vendor/product identification
  might be recognized by SDL but rejected by the current model-profile UI.
- The fully hardware-neutral SDL3-to-retail-engine gameplay bridge remains
  unimplemented and disabled; **a successful mapping test does not imply
  universal 3D gameplay support**.

## Planned end-to-end acceptance once the universal engine bridge exists

A successful joystick calibration is **not** the end of the SDL3 task.
With only the newly configured controller (no keyboard after the initial
setup), test navigation/Confirm/Back in the main and pause menus; skip
startup Bink videos; skip authorized scripted 3D cutscenes; advance the
still screen that prompts for Enter; and play a 3D level with movement,
actions and Pause. Verify focus loss/hotplug, release boundaries and
that one press does not skip several successive screens. These scenarios
are **not implemented universally** in the current test build.

## Keyboard Escape safety (2026-11-01)

Under GE-Proton, physical Circle/B on a controller can create a synthetic
Win32 `VK_ESCAPE` that previously destroyed the calibration dialog or
toggled read-only tests. Do **not** handle ordinary `WM_KEYDOWN/Escape`
during capture/test. The optional shared `sdl3_physical_escape.h` registers
a foreground HID keyboard raw-input target for that modal only, refuses to
overwrite any pre-existing keyboard Raw Input registration, accepts only
the genuine `WM_INPUT` keyboard Escape key-down, then unregisters
on dialog exit. Real-hardware validation under Wine is still required.
Look for `physical keyboard Escape via WM_INPUT ARMED` or the fallback
`physical keyboard Escape unavailable` in the log. The reliable safety
chords **Ctrl+Shift+Q** and **Ctrl+Shift+T** remain in place.
