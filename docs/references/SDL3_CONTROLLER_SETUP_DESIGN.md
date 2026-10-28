# SDL3 controller setup — staged design (draft PR #80)

## User goal and recovery flow

A person installs the fix, plugs a controller in, starts the game and
should **not** need an INI, Windows DirectInput button numbering, SDL3
configuration files or a log.

1. **A gamepad already has an SDL3 mapping and native gameplay works.**
   It gets a sensible semantic default: Jump/South, Kick/East,
   Punch/West, Web/North, Aim/LB, Zipline/RB, Swing/RT, Pause/Back.
   START claims the active controller. These are *proposed* SDL3
   defaults, not yet a production replacement for native mappings.
2. **SDL3 detects a mapped gamepad, but game actions do not work.**
   Future in-game `Options > Controller Setup` displays the selected
   device, a live **read-only test** and a guided assignment sequence.
   A person can use the **keyboard** even if no gamepad works.
   If a new profile cannot be applied safely, show that explicitly and
   leave current keyboard/native input intact.
3. **SDL3 sees a joystick but has no gamepad mapping.**
   Future wizard distinguishes the *raw joystick discovery* from an
   SDL3-standardized gamepad. Offer a supported joystick mapping flow
   only if raw input and a safe SDL3 mapping API are available. Do not
   tell the user that the pad is completely absent.
4. **Neither SDL3 nor the OS exposes the device.**
   Explain that it is not detected and invite reconnection/system checks;
   don't make up assignments or consume another pad's input.
5. **A profile is misconfigured.** Future `Restore Defaults` operates
   on **this model only**, leaves other controllers and the original
   `[Joystick]` preferences untouched, and offers a keyboard path.

## Existing game engine constraints

- Spider-Man 2001 has a seven-action native DirectInput table. The
  current guarded native-owner experiment already swaps **canonical
  DS4 and Xbox button indices**, after Start selection, on the
  *original game's device*. The custom original menu is patched
  **read-only** because unrestricted reassignment conflicts with
  canonical hot-swap and the virtual `Spidey.cfg` snapshot.
- The original game also owns analog movement, pause, keyboard and
  menu/Bink input. SDL3 selection is **not** by itself an SDL3 gameplay
  backend. Re-enabling the old joystick configuration menu without
  replacing the underlying mapping ownership would be unsafe.
- The active 3D gamepad must have **one authoritative gameplay
  input source**. Never forward all SDL3 actions as synthetic keys
  while leaving the original native joystick actions active: that
  causes duplicate and possibly held actions. A future SDL3 gameplay
  bridge must explicitly isolate native gameplay consumption, preserve
  focus/hotplug cancellation, and rollback atomically if validation
  fails. Do not read the original game's buffered DirectInput events
  from a second consumer.

## Implemented in this increment (foundation, NO playable remapping)

`sdl3_controller_profile.h` provides an independent pure C99,
portable, side-effect-free policy:

- Eight semantic actions: Jump, Kick, Punch, Web, Aim, Zipline, Swing,
  Pause; standard SDL3 buttons with normalized LT/RT trigger input;
  the **START** button is reserved for changing the selected pad and
  **GUIDE** is reserved for system handling.
- One in-memory profile per `VID:PID` gamepad model, 16 slots maximum.
  A reattached instance of the same model accesses the same profile,
  while DS4 and Xbox settings remain distinct. Two physically distinct
  pads with identical VID:PID are **not** distinguishable and must not
  be claimed as separately configurable yet.
- Atomic rebinding: selecting an input already used by another action
  swaps the two assignments instead of silently doubling actions.
  Mandatory gameplay actions cannot be unbound; Pause is optional.
  Invalid buttons, nontrigger analog axes, START/GUIDE and invalid
  loaded data are rejected. Defaults can be recreated per model.
- Pure SDL3 sample evaluation returns a semantic action **bitmask**,
  never DirectInput actions or keyboard events. LT and RT are independent,
  with threshold hysteresis (trigger 11000 to press, 5000 to release).
  A fresh neutral state is required after focus loss/selection before
  read-only observation resumes.
- `[Debug] SDL3BindingTrace=1` (default **OFF**) uses the already
  opened selected SDL3 gamepad **after** the normal event pump to log
  semantic action edges. Requires `[Input] ExperimentalSDL3Probe=1`
  and `[Debug] Log=1` to receive useful output. Up to 48 bounded lines
  with `SDL3-BINDINGS: ARMED` and
  `SDL3-BINDINGS: selected-id=... action-held=... pressed=...`.
  The sample is **read-only**: no native DirectInput reads/writes,
  synthesized keyboard, UI unlocking, file writes or gameplay effect.

Action bits in the read-only preview: 0 Jump, 1 Kick, 2 Punch, 3 Web,
4 Aim, 5 Zipline, 6 Swing, 7 Pause. Example: `pressed=0x40`
means the selected SDL3 controller physically requested Swing.
It **does not** prove that the game performed Swing.

## First opt-in in-game menu prototype (experimental Windows owner window)

The supported original game's **Options > Joystick Configuration**
screen now gains a `SDL3 TEST (PREVIEW)` row **only when explicitly opted
in**. Its previous `Restore Default` action remains intercepted, and the
original button-assignment capture/setter stays guarded and read-only.
Keyboard Configuration and its Restore Default are untouched.
The two original menu columns still gain exactly one matching row.

Add the following in `SpiderMan2k1Fix.ini` with the game closed:

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3ControllerSetup=1

[Debug]
Log=1
```

`ExperimentalSDL3ControllerSetup` is **OFF by default**, must never be
added to the generated user INI, and has no effect without the original
`ExperimentalSDL3Probe=1` opt-in and a supported 32-bit `SDL3.dll`
beside the game EXE.

In-game workflow:

1. Navigate with the **keyboard** to `Options > Joystick Configuration`.
   Even if no Sony/Xbox device works, this experimental mode attempts to
   install the SHA-verified **generic read-only** menu patch to expose
   the new entry. If the exact native signatures cannot be validated,
   the patch fails closed; no unsupported game code is modified.
2. Select `SDL3 TEST (PREVIEW)`. The game-thread native menu handler
   opens an **owned Win32 dialog**. This is a prototype window on top
   of the game, **not yet drawn by the original 3D renderer**. The
   original menu tick is temporarily stopped on its call stack.
3. Select any mapped SDL3 gamepad from the drop-down. Its actions appear
   in a list. Use the keyboard to select an action, choose **Capture
   selected action**, release **all** buttons/triggers to neutral, then
   press **exactly one** new button or LT/RT. Invalid, reserved,
   simultaneous or inherited presses cannot assign a binding.
4. **Restore defaults (this model)** resets only that model's
   in-memory preview, and **Close** resumes the original game.
   Esc cancels capture first; a second Esc closes the dialog.
   There is no special gamepad navigation requirement; keyboard works.
5. The window pumps only the **already initialized** SDL3 subsystem
   on the **same game thread**, while the normal Flip loop is stopped.
   It never consumes game-owned DirectInput events, claims SDL native
   ownership, writes game memory, creates threads or injects keys.
   It stops reading controls if the dialog loses focus, and discards
   pending captures on device changes or disconnection.

**Limitations:** The preview's assignments are **not yet persisted** and
**do not yet modify gameplay**. The native game continues to own 3D
actions, including the currently unresolved Xbox RT Swing scan refusal.
An SDL3 device listed as a raw joystick but not as a mapped gamepad
is not yet configurable. Separate physical controllers with the same
VID/PID currently share a model profile. Do not mistake successful
dialog capture for a working end-to-end gamepad mapping.

**Manual acceptance:** verify launching from native Joystick via
keyboard with (a) no pad, (b) both DS4/Xbox, (c) an SDL3-mapped third
pad, capture South and RT separately, reserved START/Guide, LT-only,
simultaneous presses, unplug during capture, Alt+Tab during capture,
Escape/cancel/close and return to menu/3D, including Ctrl/Enter input.
After returning from the modal dialog, the game must still receive
keyboard/gamepad actions and native Start switching without duplicate
inputs or stuck focus. Keep this OFF if Windows/Wine behavior differs.

## Explicitly NOT implemented yet

- A first **opt-in Win32 owner-window preview** is accessible from the native Joystick menu; a complete production-ready, renderer-integrated `Controller Setup` screen is still outstanding.
- No new saved per-model controller settings. Today's profile bank is
  **in-memory and read-only**, not a persistence format.
- No replacement of native DirectInput by SDL3 for 3D actions; existing
  Sony/Xbox controls and native hot-swap continue as before.
- No automatic fixing of the logged Xbox RT -> Swing keyboard-scan
  refusal. That is a separate verified bug in the legacy keyboard gate.
- No support for configuring a joystick that SDL3 does not identify as
  a mapped gamepad. Raw device mapping is a later explicit phase.
- No promise of per-unit configuration for duplicate VID/PID devices.

## Next implementation gates

1. **Source/identity validation:** real Windows 11 and Wine logs for
   the selected DS4 and Xbox. Confirm press, release, LT-only, RT-only,
   focus loss, detach/reconnect, no inherited Start/held presses, and
   unchanged original game actions. The debug probe may be left OFF.
2. **In-game interface:** add an accessible title-menu entry and an
   independent, native-version-signature-checked setup view. Keyboard
   must fully control that view. Capture one button/trigger at a time
   with explicit confirm/cancel, visible conflicts, reset and preview.
   Never silently accept a physical input held before the capture.
3. **Persistent profiles:** write separate, schema-versioned controller
   settings (e.g. `SpiderMan2k1Fix.controllers.ini`) to avoid the
   existing main INI validator/repair and original 252-byte Spidey.cfg.
   Validate every loaded binding; use atomic file replacement and
   per-model restore. Unknown/invalid settings revert to a safe
   default without replacing the main INI.
4. **Exactly one gameplay owner:** prototype the guarded SDL3 action
   adapter with full safety and single-authority tests. During
   transition, no duplication with native DirectInput, keyboard or
   Bink/menu bridge. Keep an immediate rollback to original native
   input on failure/Alt+Tab/unplug. Old custom bindings are preserved.
5. **Physical acceptance + release:** verify switching Sony ↔ Xbox
   and at least one otherwise unsupported SDL3-mapped pad, both
   startup orders, and Windows/Wine. Only then consider enabling the
   setup UI by default or removing the native joystick read-only lock.

This design is for the ongoing draft. It does not authorize merging
PR #79/#80 or publishing a release.
