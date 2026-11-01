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
3. Use **Tab** to focus **Test SDL3 inputs**, then press **Enter**. The status becomes `SDL3 TEST ON`. Press and release each
   face button, SHARE/BACK, START, L3, R3 and both triggers. Note the
   `SDL3 TEST: buttons=... LX=... LY=... LT=... RT=...` lines.
4. On the tested Sony `054C:09CC` under Wine, earlier independent logs
   observed physical SHARE as SDL bit `0x000010` and L3 as
   `0x000100`. Use these as comparison only, **not** as a universal
   required mapping: other drivers might differ and that is the point.
5. With the **Test SDL3 inputs** button focused, press **Enter** to exit test mode. Use Escape to close the SDL3 setup.
6. Confirm the game still responds to the original keyboard and controller
   controls, including a subsequent Alt+Tab and return.

## Test B — calibrate an already-mapped or unmapped SDL joystick

1. Connect the SDL joystick you want to calibrate (including an already SDL-mapped device such as the PS3 054C:0268). Open SDL3 Controller Setup with
   the keyboard, use **Tab** to select **Calibrate SDL3 joystick**, then **Enter**. The wizard lists **all** SDL joysticks with their names and either `Mapped - recalibrate` or `Unmapped - recognize`.
   If no devices appear, SDL3 is not exposing a joystick at all; the log will show the device inventory.
2. Select the intended physical joystick by **name** using the keyboard; press **Enter**. Already-mapped devices can be recalibrated; the existing mapping is not replaced until the wizard completes successfully.
3. The assistant walks through **20** SDL-standard controls. Release every
   control between steps. Press only the requested button, D-pad direction
   or move the requested stick/trigger. Press **S** to skip optional
   controls. The A and B buttons and the left stick X and Y axes are
   mandatory. Escape cancels; Enter restarts from scratch.
4. A successful capture displays `SDL3 CALIBRATION SAVED`. The log must
   contain `SDL3-RAW: MAPPING SAVED GUID=... location=GAME` or
   `location=LOCALAPPDATA`. If save fails, send the log; do not interpret
   an in-memory mapping as permanently saved.
5. Close the raw wizard and SDL3 setup, **restart the game**, reopen SDL3
   setup and check that the joystick appears as a *mapped* SDL3 gamepad.
   Its per-model action profile should be assignable if SDL exposes a
   valid vendor and product ID.
6. Re-enter Test A with this controller, if selectable, to verify normalized
   SDL3 button and trigger data. Do **not** assume it works in 3D yet.

## What to attach to the next report

- Full `SpiderMan2k1Fix(...).log`.
- Which test (A/B), device VID:PID (if known), Wine/Windows runtime.
- For Test A: whether the SDL button mask changes on SHARE/BACK and L3,
  and whether the keyboard regains control after closing the setup.
- For Test B: wizard opens, steps captured, whether a mapping was saved
  and whether the game re-detects the controller after restart.

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
