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

## Test C — Direct original Joystick menu entry (experimental)

1. Save a copy of the current working DLL/ZIP first. Launch the new build.
2. **First select Keyboard in the original game's options menu.** It must
   open the *original* native keyboard binding screen and still allow
   normal actions without any SDL3 popup. Exit without editing.
3. Select **Joystick** from the same original options screen. The new
   **SpiderMan2k1Fix - Controller Setup** window should open **immediately**,
   with no intervening native seven-action joystick list.
4. Exit the SDL3 window using keyboard Esc; verify the original options
   menu still responds. Re-open and close a second time.
5. Check these log lines:
   `SDL3-DIRECT-ENTRY: ACTIVE intercepted original PCSHELL_DoControllerConfig CALL(s)=...`
   and `SDL3-DIRECT-ENTRY: Joystick selected; bypass original seven-action submenu and open SDL3 window`.
   The Keyboard path should log `SDL3-DIRECT-ENTRY: Keyboard configuration forwarded to original game`.
6. If the patch logs `REFUSED`, the old native action-list submenu is
   preserved, with the SDL3 preview option as a fallback; send the log.
   If **Keyboard** unexpectedly shows SDL3 or menus stop responding,
   stop testing and restore your prior working DLL, then send the log.

This is an experimental native entry redirect; CI verifies its build and
source contracts, but has not yet verified the runtime callsite signatures
or boolean semantics under actual game execution. The original F9/F10
shortcuts and native keyboard mappings must remain untouched.


## Test D — Explicit PS3 generic SDL3 source (after joystick calibration)

This protocol deliberately distinguishes an **action-rebind capture** from
**controller ownership**. A Start press while re-binding a visible action
is a valid captured input; it must NOT silently select a new game controller.
The 2026-11-01 log shows exactly that case:
`SDL3-SETUP: START-SAMPLE id=2 ... capture=1`, followed by no owner
request. `SDL3-OWNER: START REFUSED` still protects the original native
DirectInput owner and is **not** a failure of generic SDL3 sampling.

1. Download the CI artifact built after the **Use selected SDL3 controller**
   button was added. Backup the previously validated DLL.
2. In original game's **Options > Joystick**, enter the SDL3 window
   directly. **Explicitly choose** `XInput Controller #1 [054C:0268]`
   in the top dropdown (or use L1/R1 to change the displayed model).
   **Do not just leave the first auto-selected row:** with both DS4 and
   PS3 attached the default row was the DS4 (id=1) instead of PS3
   (id=3), as confirmed by the 19:47 GE-Proton hardware log. The new
   build refuses a multi-pad owner request until an intentional
   model selection is observed.
3. With the **PS3 explicitly selected** (log `USER-CHOSEN ...
   054C:0268`), click **Use selected SDL3 controller** with the mouse
   or keyboard Tab+Enter. Keyboard **Ctrl+Shift+U** also works.
   If an action remap capture was accidentally armed, the **first click
   cancels only that capture** and shows `CAPTURE CANCELLED`, never
   queues a controller. **Click Use a second time** to confirm the PS3.
   Alternatively, keyboard Escape once cancels an active capture; it
   closes the setup only when no capture remains active.
4. Expect `SDL3-SETUP: EXPLICIT-SELECT QUEUED id=...
   VID:PID=054C:0268` *before leaving* the dialog.
   Never accept a queued DS4 `054C:09CC` as PS3 success.
   No 3D gameplay action is sent to the original game yet.
5. Press keyboard **Escape** to close SDL3 Setup. Confirm focus returns
   to Spider-Man. The next foreground Flip should report
   `SDL3-PROBE: SETUP RETURN controller-initiated owner handoff PENDING`
   and then
   `SDL3-GENERIC: SELECTED explicit modal Start id=... VID:PID=054C:0268`.
   The historical log label mentions Start; it also covers the **new
   explicit button**, not just the physical Start key.
6. Release both analog triggers and all buttons, center both sticks,
   then press and release Cross/Circle/Square/Triangle, move both
   sticks in all directions and test L2/R2. Expect
   `SDL3-GENERIC: SEMANTIC ARMED` and
   `SDL3-GENERIC: SEMANTIC ... pressed=... released=... LX=... RX=...`.
7. Exit normally, send the log and a screenshot of the F9
   notification: **native 32×32 large SpideyPC.exe icon** beside a
   compact **SCREENSHOT SAVED** heading. The full Windows path must
   **not appear onscreen**. It remains intact in the PNG destination
   and diagnostic log. Verify the notification vanishes after about
   two seconds, F9 screenshot pixels are unaffected and keyboard/controller
   input continues to work.

**Fail closed:** If the SDL3 selection is refused or fails to arm,
do NOT enable synthetic keyboard injection, blindly patch DirectInput
or assume menus/3D gameplay work. Send the full log and keep the previous
validated build. Native Keyboard, verified Sony DS4/Xbox controls and
F9/F10 game shortcuts must still work.
