# DualShock 4 automatic UI profile — draft PR #73

**Status:** the user has confirmed that the automatic DualShock 4
Enter mapping works for menus/videos under their Wine setup. The newly
added Escape/Back mapping remains to be tested; native Windows support is
not yet verified. The already-released v1.1.0 remains unchanged.

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
| ◯ Circle | BUTTON2 | **Escape / Back / Cancel** (new) | Unchanged (game native) |
| OPTIONS | BUTTON9 | Enter / validate / movie skip | Unchanged (game native) |
| SHARE | BUTTON8 | Optional Escape if configured | Unchanged |
| △ Triangle | BUTTON3 | **Original game already confirms some submenus**; NOT remapped | Unchanged (game native) |
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

**Delete the old `ExperimentalGamepadStart=BUTTON8` entry**:
this setting is now retired/ignored (still accepted by the INI
validator to preserve other saved settings).

### UI Enter route

The original PC menu/movie/still accept a keyboard Enter
(`DIK_RETURN=0x1C`). The user confirms the profile makes that work
under Wine. Escape/Back is newly added as `DIK_ESCAPE=0x01`, triggered
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

legluondunet assisted by ChatGPT (OpenAI)
