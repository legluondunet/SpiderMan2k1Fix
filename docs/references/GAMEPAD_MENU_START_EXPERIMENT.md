# DualShock 4 automatic UI profile — draft PR #73

**Status:** DualShock 4 detection and 2D menu Enter are user-confirmed
under Wine. The user subsequently observed a non-skippable cinematic
AFTER the main menu, while Triangle natively skips 3D cinematics and
a camera-related controller button opens the in-game ESC menu.
Circle/Back, post-menu Bink skipping and new Options/3D Pause still
require separate user tests; Windows support is not yet verified.
The published v1.1.0 is unchanged.

## User-reported behavior and evidence

The original *Spider-Man (2001)* PC game reads DualShock 4 gameplay
buttons and menu directions but, on this system, cannot validate a
menu selection or bypass normal movies from the pad. The user verified
that **Enter** (keyboard) works for menu selection, for movie skipping,
AND for the static interactive title-screen image; **Space does not**
work on the final still page.

The first test `v1.1.0-a807941` had zero `GAMEPAD-START` lines.
The second `v1.1.0-10aff89` log recorded:

```
SPIDEY-CFG: BOOTSTRAP game loader returned: ... [READ CONFIRMED]
GAMEPAD-START: REQUESTED BUTTON8 after native game configuration bootstrap
GAMEPAD-START: SKIPPED unsupported PE header layout
```

Thus **neither native Start-table experiment reached the target**.
`BUTTON8` is DirectInput button index eight (often DS4 **SHARE**);
it is **not the F8 keyboard key**. Rather than keep guessing the
game's writable PE layout and reloading its legacy Start mapping,
the PR removes those source files entirely.

## Current design: profile tied to identified controller

### Automatically recognized

The profile is selected by **DirectInput8 product GUID** with Sony
VID `0x054C`, PID `0x05C4` (DS4 v1), `0x09CC` (DS4 v2), or
`0x0BA0` (official Sony wireless adapter).

For safety it must be the **one and only attached DirectInput gamepad**,
and the Sony device must be unambiguous. Other gamepads are not changed.
A generic `Wireless Controller` name alone is never trusted.

This prototype creates a **separate** DirectInput8 device in
non-exclusive foreground mode; it does **not** intercept or modify the
game's DI8 object or original seven gameplay bindings. Another program
or the original game might deny simultaneous Acquire; logs explain
that failure rather than forcing any changes.

### Default UI bindings

| Controller button | Standard DS4 DirectInput index | 2D UI action | 3D gameplay |
|---|---|---|---|
| ✕ Cross | BUTTON1 | Enter / validate | Unchanged (game native) |
| ◯ Circle | BUTTON2 | Escape / Back / Cancel (experimental) | Unchanged |
| OPTIONS | BUTTON9 | Enter / validate / attempt native Bink movie skip | **Pause/Escape ONLY when BUTTON9 is free of native gameplay mappings** |
| SHARE | BUTTON8 | Optional Escape if configured | Unchanged |
| △ Triangle | BUTTON3 | Native submenu confirm, unchanged | **Native 3D cinematic skip remains intact** |
| Other | Original device mapping | Unchanged | Unchanged |

The **two** Enter candidates are deliberate: user may refine which
physical button should validate menus, enter the title screen or skip
movies. The standard indices are a starting hypothesis under Wine,
not a validated description of this user's controller.

`[Input]` factory defaults for newly generated INIs:

```ini
ControllerProfile=Auto
ControllerEnterButtons=Both
ControllerBackButton=Circle
ControllerPauseButton=Options
```

Existing INIs missing these keys also use these defaults.
`ControllerProfile=Off` disables only the added 2D UI bridge.
`ControllerProfile=DualShock4` explicitly selects the same
VID/PID-verified profile and still refuses unknown devices.
`ControllerEnterButtons` accepts `Cross`, `Options` or `Both`.
`ControllerBackButton` accepts `Circle` (default), `Share`, or
`Off`. These are only mapped while the original game is in 2D UI
or media presentation. The option is available to existing INIs without
forcing any reset of the user's original controls.

**Confirmed follow-up:** the user observed that **Triangle also performs
native Enter/confirm inside some submenus**, making it redundant with the
new Cross bridge. We deliberately leave Triangle's native mapping intact:
injecting Escape on Triangle *without suppressing the original confirm*
would risk activating a menu selection and then backing out in the same
press. The safe first iteration uses the otherwise unassigned **Circle**
button for Back/Escape. If Triangle should instead become Back, first
identify and guard the original submenu controller handler so its native
confirm can be suppressed *only* in the required 2D contexts.

**New safety gate for 3D Pause:** the game's original `[Joystick]
Zipline=BUTTON9` occupies DS4 OPTIONS. The implementation will NOT
send Escape from OPTIONS in 3D until the saved INI confirms that none
of the seven native gameplay actions uses BUTTON9. To activate
OPTIONS = Pause, first reassign Zipline via the *original game's*
Joystick menu to a currently unassigned button — **R1 / BUTTON5** is
a suggested trial — then leave the game options so the new value
saves. Verify other game commands remain intact. The bridge rechecks
the INI on each Options press rather than rewriting any mapping itself.
A guard `GAMEPAD-UI: PAUSE BLOCKED` identifies conflicts.

The original game binds native Start to BUTTON11 (often the right
thumbstick click/R3 on this DS4 layout); the user's description
"camera button" **has not been physically verified**. We do not disable
that original native Start/pause button. As such, the profile can expose
both OPTIONS and the original pause button until a separate,
signature-verified, non-conflicting remap is tested.

**Delete the old `ExperimentalGamepadStart=BUTTON8` entry**:
this setting is now retired/ignored (still accepted by the INI
validator to preserve other saved settings).

### UI Enter route

The original PC menu/movie/still accept a keyboard Enter
(`DIK_RETURN=0x1C`). The user confirms the profile makes that work
in main menus under Wine. After the main menu, some Bink movies run
with `menu_upscale_active=0`; the first prototype therefore did NOT
inject Enter while those were playing. The newer version also permits
Enter **only when original game-owned `GameFMV_Active` is nonzero**
(at fixed VA `0x006151F8` in the verified original executable).
This explicitly does not claim to skip static comic sequences or
scripted 3D scenes: Triangle remains the original 3D skip command,
and Bink controls remain subject to their native frame-gate.

Escape/Back is newly added as `DIK_ESCAPE=0x01`, triggered
by Circle (or SHARE if explicitly configured). Both synthetic keys use
paired scan-code down/up via `SendInput`, on distinct controller press
edges, with key-up on the *next* DirectDraw Flip.
When Enter and Escape are pressed on the same Flip, **Escape wins**:
the bridge never injects contradictory menu keys in the same frame.
It runs on the already hooked game's Flip thread.

Very strict context guards:
- The game window must be visible and the actual foreground HWND.
- The renderer must be in the original **2D menu/video mode**;
  widescreen 3D gameplay is explicitly excluded.
- Only a transition from released to pressed triggers a single
  Enter pulse. Held controller buttons never auto-repeat.
- Synthetic Enter is released before any possible Alt+Tab focus
  pause, to avoid leaving a pressed key behind.
- A failed separate controller read or unsupported GUID causes
  **no synthetic keyboard input**.
- Never call the original game's DXInput setters, alter its
  DirectInput COM/vtable, or write to its game-owned keyboard bytes.
- The original Bink/movie skip timing remains in charge.
  The bridge never skips a video on its own.

**Limitation:** even a successful `SendInput` can fail to register
as an original DirectInput keyboard event under some Wine or
Windows environments. Only an actual in-game test proves that.
In that case we will inspect the game-specific verified
`PCSHELL_CheckTriggers(0x0050C180)` behavior and design a guarded
native return-path hook rather than claiming the feature works.

## Debug messages

With `[Debug] Log=1`, inspect:

- `GAMEPAD-UI: config profile=Auto confirm=Both ...`
- `GAMEPAD-UI: DISCOVERY gamepads=... SonyDualShock4=...`
- `GAMEPAD-UI: PROFILE DualShock4 ACTIVE ...`
- `GAMEPAD-UI: SYNTHETIC Return ...` on Cross/Options press
- `GAMEPAD-UI: SYNTHETIC Escape ...` on Circle/Share press
- `GAMEPAD-UI: PAUSE BLOCKED ...` if OPTIONs is still assigned to any gameplay action
- `GAMEPAD-UI: 3D PAUSE REQUEST OPTIONS -> Escape` after conflict-free mapping
- `GAMEPAD-UI: unclassified engine stage` if a controller Enter press cannot safely be routed to 2D menu or native Bink
- `GAMEPAD-UI: independent DirectInput read hr=...` if polling fails
- `GAMEPAD-UI: ... inactive ...` if the device cannot be identified

A `SYNTHETIC Return` line means the Win32 request was submitted,
**not** that the game accepted it.

## Required tests to build the final profile together

1. The maintainer already confirmed ✕/OPTIONS Enter works in the tested
   Wine configuration. Ensure that remains true.
2. Open a submenu using ✕, press ◯ once to go back, and test cancellation
   of NO/YES confirmations. Triangle still confirms natively; avoid
   reassigning it to Escape without first suppressing the original route.
3. Play a regular Bink video and press ✕ / OPTIONS *after* the original
   skippable interval; ensure exactly one advance.
4. Enter 3D: prove that both buttons still perform only their
   original gameplay functions, including movement/attacks.
5. Verify focus Alt+Tab never leaves Enter pressed. Test under
   both Wine and native Windows; evaluate log outputs.
6. Choose `Cross`, `Options` or `Both` as the default based on
   actual results. Consider different DS4 transport layouts separately.

This record intentionally preserves the two failed Start-table tests
and the unverified state of our new approach.

## Proposed next gamepad acceptance matrix (October 28)

1. In the startup still and main menu: Cross confirms, Circle returns,
   OPTIONS confirms only per existing `ControllerEnterButtons=Both`.
2. After *New Game* begins, press Cross then OPTIONS in the video.
   Compare log `GAMEPAD-UI: SYNTHETIC Return` and native Bink status.
   A static comic panel may intentionally require the original timer
   and is NOT included in this video bridge.
3. In original 3D scripted cutscenes: Triangle must still skip; X must
   not emit Enter unless `GameFMV_Active` identifies a native Bink.
4. In playable 3D with original Zipline=BUTTON9: OPTIONS must NOT
   inject Escape or activate both pause and Zipline; log must say
   `PAUSE BLOCKED`. The existing camera-button native pause stays.
5. If the player elects to move Zipline to an unused native button,
   e.g. R1/BUTTON5, confirm OPTIONS alone opens and closes native ESC
   menu and the newly mapped Zipline works.
6. Restart with another attached controller, lose foreground via
   Alt+Tab, change 16:9/4:3 mode, and inspect input regressions.

The exact meaning of "camera button" and whether the after-menu
sequence is Bink, a static comic, or scripted 3D are **not yet proven
by a new runtime log**. Keep the two safeguards above even if a
particular scene cannot be skipped with Enter.

legluondunet assisted by ChatGPT (OpenAI)
