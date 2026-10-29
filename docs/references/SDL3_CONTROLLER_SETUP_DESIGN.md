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

## Experimental live menu/Bink context bridge — PR #80

The first optional runtime consumer of saved model-specific `[MenuBindings]` is
now available. It is **OFF by default**. In the main `SpiderMan2k1Fix.ini`:

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3NativeOwner=1
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1
ExperimentalNativeJoystickMappingSwap=1
ExperimentalSDL3ControllerSetup=1
ExperimentalSDL3ContextBridge=1
```

Only the selected SDL3 pad, after the actual original game-owned DirectInput
device is verified, may request synthetic **Return** (`MenuConfirm` in the
2D menu or `SkipVideo` in Bink) and **Escape** (`MenuBack`). The
legacy 2D controller bridge is suppressed for those same frames so inputs
cannot be doubled. An initial neutral SDL3 sample is mandatory after focus
loss, controller handoff, profile edit, menu/movie transition or a failing
native owner. START remains reserved for pad selection. These synthetic
keys go through the existing guarded per-Flip SendInput scancode lifecycle;
the original Bink/movie cleanup and skip eligibility gates are not bypassed.

**Not implemented:** `NextStill`, `SkipCutscene` and the eight semantic
3D bindings. The exact original still-page/cutscene owner and safe native 3D
buffer adapter have not been validated. The original 3D DirectInput actions,
analog axes and custom game joystick table remain unchanged. Disabling
`ExperimentalSDL3ContextBridge` restores the previous native/legacy menu
behavior. This opt-in mode needs a physical Windows and Wine test before
publication.

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
5. **No mouse is needed.** On opening the preview, focus starts on
   the actions list. The independent keyboard shortcuts work even with
   `[Input] MouseEnabled=0`:
   `Up/Down` change the selected action; `Enter` starts its capture;
   `PageUp/PageDown` cycle between SDL3 gamepads; `F5` restores
   defaults for the current model; `Escape` cancels capture (or closes
   the window if not capturing); `F1` repeats the help text.
   `Tab` and `Space` continue to operate the native Win32 controls,
   and `Enter` activates focused Restore/Close buttons.
   A mapped gamepad can navigate too: **D-pad Up/Down** changes the
   action, **South (A/Cross)** starts capture, **East (B/Circle)**
   closes the preview, **LB/RB (L1/R1)** switches between connected
   mapped gamepads, and **North (Y/Triangle)** restores the current
   model defaults. Physical navigation requires a first neutral
   sample, is edge-triggered (no repeated action on held buttons),
   discards simultaneous/Start/Guide presses, and suspends while a
   button is being captured. A keyboard is always a fallback.
6. The window pumps only the **already initialized** SDL3 subsystem
   on the **same game thread**, while the normal Flip loop is stopped.
   It never consumes game-owned DirectInput events, claims SDL native
   ownership, writes game memory, creates threads or injects keys.
   It stops reading controls if the dialog loses focus, and discards
   pending captures on device changes or disconnection.

**Wine accessibility retest:** The first hardware capture
`SpiderMan2k1Fix(20261028-223017).log` confirms that the native
menu entry opens, the Xbox 360 is detected (`045E:028E`), a semantic
binding changes in memory, restoring defaults succeeds, and closing
returns to the original game. The maintainer's screenshot also shows
the original game mouse disabled, so the new keyboard/gamepad
navigation is being introduced specifically to remove the dialog's
mouse dependency. The **new shortcuts themselves are not yet physically
tested**; verify them with the next Windows x86 artifact. The title
and help now use ASCII-only Win32 labels to avoid Wine ANSI mojibake.

**2026-10-28 Wine crash candidate guard:** One physical test of the keyboard/gamepad navigable build (`v1.1.0-097d29f`) succeeded through several in-memory assignments and a model reset, but the process stopped without logging dialog close. This is **not yet a diagnosed exception**: no dump/stack was provided. The first mitigation removes reentrant `DestroyWindow` from the SDL3 timer's B/Circle path: it schedules close only after button release using a posted WM_CLOSE. Keyboard Escape and the Close button also queue close. `WM_CLOSE` disarms the timer and captures before destruction; the dialog logs Reset begin/result, close request, `WM_CLOSE`, `WM_DESTROY`, modal-loop exit and owner restoration. If the next Wine test still crashes, capture exception code, faulting module/stack and action used at the moment of failure, instead of assuming the same cause.

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

## One Confirm / Skip button plus Back / Cancel — INI v2

**Draft PR #80: editor and persistence implemented; engine runtime routing
is NOT yet connected.** This change replaces five visible menu/media
rows with just **two** in the no-mouse SDL3 setup wizard:

- **Confirm / Skip** maps one physical gamepad button to menu
  confirmation, Bink video skip, the second fixed screen awaiting Enter,
  and native 3D scripted cutscene skip (subject to the original game's
  permission checks). The same physical button may also remain assigned
  to Jump in the independent 3D gameplay section.
- **Back / Cancel** controls menu back/cancel separately.

The wizard now has **10 visible rows**: eight 3D actions and these two
menu/media actions. It works with keyboard and gamepad navigation as
before. Start and Guide remain reserved, and native SDL3 LT/RT capture
is supported. When editing Confirm / Skip, all four corresponding v2
keys are changed together **in memory as one transaction**, atomically
saved, and then committed to the live profile. The five actual on-disk
keys are retained for backward compatibility:

```ini
[MenuBindings]
MenuConfirm=button:0
MenuBack=button:1
SkipVideo=button:0
NextStill=button:0
SkipCutscene=button:0
```

The original eight `[Bindings]` actions are unchanged. We do NOT
introduce a Version=3 file. The v1->v2 migration still works and
preserves existing 3D settings and original .v1.bak before first
explicit user save. Defaults for new or v1-upgraded controllers use
South (Cross/A) consistently across all four Confirm / Skip roles.
Back uses East (Circle/B). The same physical button cannot be both
Confirm and Back in the same menu: assigning one to the other's button
**swaps** the two; for an already unified Confirm/Skip assignment the
whole four-key group stays unified after the swap.

**Crucial existing v2 migration policy:** profiles saved by the older
five-row wizard may have different buttons for MenuConfirm, SkipVideo,
NextStill and SkipCutscene. The new editor displays
`MIXED (Capture to unify)`, reads them without rewriting or deleting
anything, and only makes them equal when the user explicitly captures
a new Confirm / Skip binding. Changing Back on a mixed profile also
preserves unrelated saved video/still/cutscene assignments.

The five context keys are still validated independently on disk,
including user-hand-edited values. This simplified editor is not a
change to the INI parser, fallback directory, or original game-owned
input tables.

**Runtime boundary:** no Bink, native menu, fixed-screen or scripted
cinematic input route has been replaced by the new settings. The
existing legacy DirectInput/Sony/Xbox confirm/back behavior remains
active for now. Later context-specific routing must select the one
correct original-game owner, gate native cutscene skipping on
`CutSceneSkipAllow`, wait for release across transitions, and avoid
double-confirmation. A successful save is NOT evidence that this
button already skips every video/cutscene.

## Per-model profile persistence (experimental, draft PR #80)

When `ExperimentalSDL3Probe=1` and
`ExperimentalSDL3ControllerSetup=1`, SDL3 automatically loads each
valid model profile when it first discovers that gamepad. The primary
storage location is next to `SpideyPC.exe`:

```text
Spider-Man/
  SpideyPC.exe
  SpiderMan2k1Fix.ini
  SpiderMan2k1Fix/
    controllers/
      054C-09CC.ini       # Sony DualShock 4
      045E-028E.ini       # Xbox 360
```

If the game folder is not writable, for example under
`C:\\Program Files (x86)`, the store creates a **per-user fallback** in
`%LOCALAPPDATA%\\SpiderMan2k1Fix\\controllers\\`. Once a local
override exists, it takes precedence over the same game's model file.
No elevation/admin permissions are needed for the fallback. Portable
game folders remain self-contained when writable.

One versioned strict INI file is generated **only after the user
changes a binding or restores defaults**, not merely upon discovery:

```ini
[Profile]
Version=1
Vendor=0x054C
Product=0x09CC
[Bindings]
Jump=button:0
Kick=button:1
Punch=button:2
Web=button:3
Aim=button:9
Zipline=button:10
Swing=trigger:5
Pause=button:4
```

These are SDL3-normalized button/trigger indices, not DirectInput
indices. On load, every required field is checked: schema version,
exact model VID/PID, all eight action names exactly once, valid button
or trigger indices, reserved Start/Guide exclusions, mandatory bindings
and action uniqueness. Invalid, oversized, unreadable or unknown schema
data is never partially installed; the model retains safe defaults and
the problem is logged. A malformed prior file is copied to
`<VID>-<PID>.ini.invalid.bak` before the first subsequent explicit
save; the backup is never overwritten. File writes use a unique
same-directory temporary file, flush, then atomic replacement. If a
save fails, **the new binding is rejected and the previous in-memory
profile stays unchanged**; the user sees `SAVE FAILED` and can still
navigate the original game.

Changing a binding, swapping two bindings, pressing F5/Y or clicking
Restore Defaults **saves immediately** and affects only the selected
model. No controller file is generated for unknown/zero VID/PID or when
the experimental setup is disabled. The main
`SpiderMan2k1Fix.ini`, its auto-repair, and virtual
`Spidey.cfg` continue independently.

**Important scope:** Persisting these SDL3 *semantic* mappings does
**not** yet apply them to the game's seven native 3D DirectInput
actions. The window remains a setup preview, although the profile is
durably saved. The separate native gameplay-owner adapter still needs
design, implementation, and hardware validation.

**Physical acceptance requested under Wine and Windows:** on a
non-admin install in `Program Files (x86)`, customize Swing and Jump,
quit the game, relaunch, verify the same values loaded, check the
fallback folder and that no game executable or original config was
rewritten. Repeat with a writable portable installation and verify the
game-local directory. Test reset, separate DS4 and Xbox profiles,
hot-swap, and malformed file recovery. Keep mouse disabled and ensure
the dialog still closes without a crash.

## Explicitly NOT implemented yet

- A first **opt-in Win32 owner-window preview** is accessible from the native Joystick menu; a complete production-ready, renderer-integrated `Controller Setup` screen is still outstanding.
- Per-model settings are now saved and reloaded; there is still **no**
  native DirectInput 3D gameplay adapter applying those semantic bindings.
- No replacement of native DirectInput by SDL3 for 3D actions; existing
  Sony/Xbox controls and native hot-swap continue as before.
- No automatic fixing of the logged Xbox RT -> Swing keyboard-scan
  refusal. That is a separate verified bug in the legacy keyboard gate.
- No support for configuring a joystick that SDL3 does not identify as
  a mapped gamepad. Raw device mapping is a later explicit phase.
- No promise of per-unit configuration for duplicate VID/PID devices.

## SDL3 3D action adapter — first preflight (read-only, no injection)

To avoid corrupting the original input engine, the first stage of the
real gameplay adapter is a **diagnostic preflight** that evaluates the
**saved SDL3 profiles** only when all of these conditions hold:

- `[Debug] SDL3GameplayPlan=1` is explicitly set, **and**
  `[Input] ExperimentalSDL3Probe=1` and
  `ExperimentalSDL3NativeOwner=1` have been enabled;
- the original game's foreground window is active in actual **3D**
  (not a menu, Bink cinematic or native pause screen);
- the current selected SDL3 VID/PID and **live original native
  DirectInput device** have been verified to match after native
  handoff; otherwise the adapter resets and refuses;
- the pad has produced a **fresh neutral state** after starting 3D,
  reconnecting, hot-swapping, or Alt+Tab;
- all eight semantic bindings pass strict validation and normalized
  LT and RT axes are valid. Hysteresis protects long holds.

On qualifying controller edges, up to 64 log lines prefixed
`SDL3-3D-PLAN:` show the selected model and semantic bitmasks,
`pressed=...`, `released=...`, `held=...`, and LT/RT values.
Action bit 0=Jump, 1=Kick, 2=Punch, 3=Web, 4=Aim, 5=Zipline,
6=Swing, 7=Pause. A default RT Swing press should log
`pressed=0x40` **even if the game's original Swing action still
fails**. Remap Swing to LT in the persistent setup, return to 3D,
and it should change to LT-only.

**This is NOT a live gameplay patch yet**: no game-owned buffers,
native 14-slot table, COM objects, keyboard events, or file mappings
are touched by the probe. SDL3 physical buttons and analog triggers
cannot simply be copied to the original `0x568408` DirectInput button
indices. A live solution needs an exact-signature-checked original
buffered input-reader interception, preserving its analog stick path
and disabling duplicate native button dispatch. Do not enable a
second simultaneous keyboard-synthesis path for the same Swing action.
An observed semantic bitmask is evidence of physical normalization,
**not** evidence of a Spider-Man 3D action.

Next hardware observation: with both pads, enter real 3D; press each
face button and separate LT/RT, return to menu, Start-handoff to
second pad, restart 3D, check the second profile, Alt+Tab and resume.
Confirm `SDL3-3D-PLAN: ARMED`, correct action edges and no crash,
and that original gameplay continues unchanged. The preflight must
remain OFF by default.

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
3. **Persistent profiles (implemented, physical acceptance pending):**
   versioned `SpiderMan2k1Fix/controllers/VID-PID.ini` files,
   validated load, atomic write, per-model defaults and LOCALAPPDATA
   fallback without modifying the main INI or virtual Spidey.cfg.
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
