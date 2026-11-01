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
- **2026-11-01 second launch: saved mapping RELOAD CONFIRMED.**
  User log `SpiderMan2k1Fix(20261101-163128).log` shows
  `SDL3-MAPPINGS: persisted per-GUID user mappings loaded=1`,
  followed by PS3 `054C:0268` detection. However read-only
  `Test SDL3 inputs` toggled ON/OFF while controller buttons interacted
  with enabled native Win32 dialog widgets. The diagnostic was not
  usable. New diagnostic implementation locks **all** Win32 controls,
  ignores synthetic keyboard events and clicks, exposes keyboard-only
  Ctrl+Shift+T (stop) and Ctrl+Shift+Q (close), and logs both stick axes;
  the subsequently uploaded 17:35 log confirms the isolation + keyboard
  Escape work on the actual hardware (see next bullet).
- **2026-11-01 hardware acceptance: physical keyboard Escape and isolated
  SDL3 diagnostic CONFIRMED under GE-Proton 11-11.** In user log
  `SpiderMan2k1Fix(20261101-173528).log`, build
  `v1.1.0-77d9938`, both DS4 `054C:09CC` (SDL id=1)
  and calibrated PS3 `054C:0268` (SDL id=2) activate the read-only test.
  Both runs report `physical keyboard Escape via WM_INPUT ARMED`,
  `PHYSICAL-ESC keyboard stopped normalized input test`, test OFF,
  and successful restoration of the original game-window focus.
  The PS3's live mask changes across face, shoulder and other inputs,
  and both analog triggers reach `32767`; SDL mapping reloads with
  `loaded=1`. The user reports both Escape and controller input test OK.
  The PS3 log shows some left-stick axis changes but **RX/RY remain 0
  in all recorded snapshots**; right stick range/assignment needs a
  separate explicit physical check before full analog validation.
  The bypass of the original Joystick list is a **later experimental
  build** and is NOT validated by this hardware log.
- **Game ownership still refused for this PS3** even after calibration:
  `SDL3-OWNER: START REFUSED unsupported native controller` and
  `SDL3-PROBE: SETUP RETURN REFUSED stale/ambiguous modal owner`.
  The existing native DS4/Xbox-only bridge remains unchanged.
- The F9 screenshot and F10 in-game location shortcuts remain reserved.

## Experimental direct Joystick menu entry (awaiting hardware test)

Selecting **Joystick** in the retail game's options should open the SDL3
configuration window **directly**, without the now-redundant native list
of seven controller actions or a second `SDL3 TEST (PREVIEW)` selection.
The `Keyboard` entry must still invoke the original PCShell configuration
without any UI or binding modification.

The verified decompilation symbol index
`krystalgamer/spidey-decomp/tools/names.json` identifies
`PCSHELL_DoControllerConfig(bool)` at the original game VA `0x0050CE70`.
Instead of patching this shared function entry, the experimental redirect
finds executable-section x86 direct CALL instructions targeting it and
replaces only their relative CALL destinations. A cdecl dispatcher
passes through keyboard configuration to the original unmodified function,
but opens SDL3 immediately for joystick configuration. It checks PE
layout and the original CALL targets; if discovery or patching fails, it
preserves the older native Joystick submenu with its manual preview row.
There is no gameplay or DirectInput data mutation and no changes to F9/F10.

**2026-11-01 hardware regression and exact PE-size fix:** In
`SpiderMan2k1Fix(20261101-182610).log` (version
`v1.1.0-5ef7ce9`, GE-Proton11-11), the direct-entry installer
traced `REFUSED PE header magic=010B base=00400000
size=02A0D000 sections=4`. The loose `SizeOfImage <= 0x02000000`
preflight was incorrect: **the exact SHA-verified retail executable
uses `0x02A0D000`**. The installer now checks this known size exactly
and keeps the remaining PE, executable-section, original-call and
rollback checks. Runtime original-call matching and entry behavior still
need a fresh hardware test; if no verified caller matches, preserve
the old list + preview fallback, with a precise log message.

**2026-11-01 follow-up with runtime evidence:** The next user log,
`SpiderMan2k1Fix(20261101-184301).log`, confirms the PE fix and
native CALL redirection install **work**:
`SDL3-DIRECT-ENTRY: ACTIVE ... CALL(s)=2`. Selecting Joystick then
reaches the OLD Joystick screen and logs
`SDL3-DIRECT-ENTRY: Keyboard configuration forwarded to original game`.
This proves our assumed mode flag was **reversed**: on the actual retail
game's caller, native `mode=0` selects Joystick, `mode=1` selects
Keyboard. The dispatcher now branches on `native_mode==0` for SDL3
and passes the original, unchanged `native_mode` to the PCShell
implementation for Keyboard and on fail-closed fallback. It logs
`SDL3-DIRECT-ENTRY: INVOKED native-mode=... interpreted=...` so
future tests can confirm each selection. Opening SDL3 directly, returning
to the parent Options menu, and preserving Keyboard all **still need
explicit hardware validation**.

**Important:** Check `SDL3-DIRECT-ENTRY: ACTIVE`, then select Keyboard
to confirm original editing works, and select Joystick to confirm direct
SDL3 opening. A wrong selection or a rejected hook is a reason to revert
to the previous tested DLL; never merge before these observations.

## 2026-11-01 direct-menu hardware acceptance and generic gameplay groundwork

User explicitly validated direct **Options -> Joystick -> SDL3 Setup**, and the
`SpiderMan2k1Fix(20261101-185042).log` supports it: the original
`PCSHELL_DoControllerConfig` hook is `ACTIVE` with two callsites,
`native-mode=1` opens the original Keyboard configuration unchanged,
and `native-mode=0` opens SDL3 directly, including three repeated
modal-open/close/focus-return cycles. This UX milestone is **accepted**.

**Next stage (implementation in this draft; no live game injection):**
The native game refuses PS3 `054C:0268` ownership because DirectInput
button mappings cannot be inferred from SDL's normalized mappings.
Instead of loosening the native owner check, a separate explicit
**generic SDL3 selection** has been added. A PS3 player can choose their
device in SDL3 Setup, **press the physical controller Start** to claim
the setup-only selected pad, then close the modal. On the next foreground
game Flip, the request is checked against the still-connected,
uniquely identified SDL3 instance; for an otherwise unsupported native
model it becomes `SDL3-GENERIC: SELECTED ...`, with **no native
DirectInput owner handoff**. For Sony DS4/Xbox360, the prior verified
native route remains unchanged.

The newly exposed `SpiderSDL3Probe_GenericSelectedSample()` returns
only the selected connected instance, with **all four stick axes** and
both triggers, on the game thread. The neutral-gated
`sdl3_universal_actions.h` evaluator now emits a bounded read-only
`SDL3-GENERIC: SEMANTIC ... pressed=... released=...` trace from the
stored eight-action model profile. Foreground loss, model/profile
changes and invalid snapshots reset the gate. No keyboard synthesis,
native DirectInput read/write or other gameplay action is performed.

**2026-11-01 new PS3 hardware log** `SpiderMan2k1Fix(20261101-192102).log`,
build `v1.1.0-679e04b`: `SDL3-SETUP: START-SAMPLE id=2 slot=1
armed=1 previous=0 capture=1` proves Start was pressed **while the
seven-action binding capture was active**. The original Start-claim
code correctly refuses changing controller ownership during a rebind.
After keyboard Escape, it logs
`SDL3-PROBE: SETUP RETURN Start takeover disarmed until neutral`
and later `SDL3-OWNER: START REFUSED unsupported native controller`,
which concerns **native DirectInput ownership**, not a hardware SDL3
failure. To avoid requiring Start to double as both binding and source
selection, SDL3 Setup now exposes a dedicated **Use selected SDL3
controller** button (keyboard `Ctrl+Shift+U`), which queues the exact
currently selected GUID/instance model only if no action capture is
underway. On next safe foreground Flip, the existing unique-ID +
VID/PID check accepts unsupported DI models into the **read-only generic
semantic adapter**, and leaves verified DS4/Xbox native code untouched.
Hardware needs to verify the new explicit button/return path.

**Hardware test needed before gameplay bridging:** with PS3 selected,
observe `SDL3-GENERIC: SELECTED explicit modal Start`, then
`SDL3-GENERIC: SEMANTIC ARMED` after releasing every button,
then action `pressed/released` changes and LX/LY/RX/RY motion. Existing
SDL3 diagnostic, Keyboard menu and DS4/Xbox360 native paths must still
operate normally. Passing these traces does **NOT** mean the PS3 can
already move Spider-Man, navigate in-game menus or skip videos.

## Outstanding

- Hardware retest the isolated SDL3 normalized input diagnostic
  (mapping reload itself has been confirmed as `loaded=1`).
  Confirm physical button/axis telemetry without unintended UI actions.
- Improve analog-axis polarity, physical trigger neutrality and UI recovery.
- Implement and verify the **universal 3D + menu + video + cutscene +
  still/Enter** context-aware bridge.
- Cross-device physical Windows and Wine testing and explicit user sign-off.

No declaration of "universal controller support" before every context above
passes real-hardware acceptance. Keep this draft outside protected main
until the maintainer authorizes integration.

legluondunet assisted by ChatGPT (OpenAI)


### 2026-11-01 follow-up: two-step safe selection when capture is active

User log `SpiderMan2k1Fix(20261101-195948).log` shows the correct PS3
`054C:0268` chosen in the dropdown (slot=1, id=3), but
`EXPLICIT-SELECT refused during action capture`, then pressing Escape
closed the dialog; no `EXPLICIT-SELECT QUEUED` or generic frame followed.
To prevent a synthesized Proton Return/click from accidentally claiming a
controller, the **first** Use selected click during an active action
rebind now **cancels just that capture** and moves focus to the inert
modal window; a **second explicit click** or `Ctrl+Shift+U` is needed
to queue the verified PS3 identity. Synthetic Return on the inert window
cannot reopen remapping: Enter requires the action-list focus.
Controller selection still needs explicit manual choice if multiple
pads are attached, and queued identity is revalidated on the next Flip.
This is still a read-only generic-semantic experiment, not native
DirectInput gameplay support.
