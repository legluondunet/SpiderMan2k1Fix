# SDL3 universal controllers — experimental plan

Status: DRAFT, not merged and not validated on physical hardware.

## Stage 1: recognize SDL3-unmapped joysticks (PR #94)

Open the game's Joystick Configuration with the keyboard, then choose
Recognize unknown controller (keyboard). The wizard lists raw SDL3 joysticks
that are not yet SDL3 gamepads, and records 20 logical controls, including
face buttons, shoulders, Back, Start, movement axes and optional D-pad.
It is keyboard-accessible; Escape cancels, and Space skips optional controls.

The guided wizard creates a GUID-based SDL_AddGamepadMapping entry. Its
mapping is stored in SpiderMan2k1Fix/SDL3-gamepad-mappings.txt or the
writable LOCALAPPDATA counterpart. Saved mapping lines load before SDL3
enumerates mapped gamepads on later launches. The wizard does not alter
Spider-Man's native DirectInput gameplay or original game executable.

## Stage 2: experimental manufacturer-independent gameplay (PR #95)

PR #95 builds on PR #94. Only enable on an isolated test install after
reviewing the risks below. The generated default is OFF:

    [Input]
    ExperimentalSDL3UniversalGameplay=1

The new adapter translates SDL3 normalized face/shoulder/trigger bindings,
left stick movement, D-pad and Pause to the original game's eleven
keyboard actions. It reads the live game action key table, respecting
custom keyboard bindings instead of embedding US keyboard key codes.
The selected SDL3 device ID is the input owner; no DS4/Xbox button mapping
table is consulted. Devices lacking VID/PID can receive a provisional
GUID-derived synthetic profile key reserved for storage, not native DI.

To avoid double input, the prototype temporarily suspends the one original
game-owned DirectInput joystick pointer. It attempts to restore the pointer
and release synthetic keyboard keys on focus loss or device changes. The
older model-specific gamepad input bridge is disabled in universal mode.

The November 2 hardening requires a full neutral SDL3 controller frame before
temporarily suspending that original pointer. It restores game ownership and
releases synthetic keys immediately BEFORE opening the modal controller setup,
because that dialog blocks the original render/Flip thread. Failures to inject
keys restore the native pointer. Capped diagnostics distinguish an unselected
gamepad, invalid controller profile, unverifiable native slot, and invalid
original keyboard table. `SendInput KEYDOWN/KEYUP` means only that the
operating system accepted the input: it does NOT prove the game received it.

**Safety limits:** The original 2001 executable has not been exhaustively
proven safe for a NULL game-owned joystick pointer during all game modes
or startup/level changes. SDL3-to-SendInput held keyboard actions also
need verification under Windows and Wine. The backend remains OFF by
default, as neither a successful DLL build nor static regression tests
demonstrate hardware compatibility. GUID-based 16-bit profile hashes can
collide; a stronger profile identity scheme will be needed for release.

## Hardware acceptance before switching default on

- Verify unknown raw joystick can complete the keyboard-guided wizard.
- Restart and verify saved mapping automatically loads as SDL3 gamepad.
- In a separate test install enable the experimental universal backend.
- Verify stick directions, D-pad, seven customizable gameplay commands,
  menu confirmation/back, Pause toggle and Bink/intro skipping.
- Confirm no stuck synthetic keys, missing events or duplicate actions.
- Exercise focus loss while holding an action, Alt+Tab, pause/focus return,
  disconnect/reconnect and device Start selection with two pads.
- Retest existing DS4 and Xbox 360 controls, plus at least one unrelated
  generic/third-party gamepad on Windows and Wine/Proton.
- Disable the experimental INI key and keep the keyboard/native fallback
  if any original game mode fails a safety check.

Draft PRs #94 and #95 must not merge into main, PR #92, or a release
without explicit maintainer approval and successful hardware validation.

legluondunet assisted by ChatGPT (OpenAI)


## First opt-in GE-Proton / Windows 11 gameplay smoke test

**This prototype remains isolated in draft PR #95** (on top of raw-mapping
draft PR #94), not in the physically validated controller-panel PR #96.
The branches have diverged. Do not replace the stable setup DLL from PR #96
on a daily-play installation, and do not assume their UI fixes are present
in the PR #95 build. A dedicated test copy of the game is required.

1. Back up the existing `SpiderMan2k1Fix.ini` and per-model controller
   mappings. Copy the whole GitHub Actions `SpiderMan2k1Fix-Windows-x86`
   artifact from **PR #95 only** to an isolated test installation.
2. Enable `[Input] ExperimentalSDL3UniversalGameplay=1`. The
   default remains `0`; there is no hidden auto-enabling via a detected PS3.
3. In a neutral main menu, press **Start** on the intended PS3 gamepad
   (SDL3 VID:PID `054C:0268`). Verify the log records
   `SDL3-PROBE: START selected logical id=...`; no unsupported-native
   refusal should occur in this opt-in branch.
4. Confirm `SDL3-UNIVERSAL: ACTIVE selected-id=... model=054C:0268`.
   If instead a REFUSED/waiting line appears, stop and attach the log:
   investigate selection, mapping, keyboard key-table and DI slot verification.
5. Start a *new* game with the real keyboard if required. Release every
   physical gamepad button/stick before first 3D input, then test movement,
   Jump and Kick individually. Each `SendInput KEYDOWN/KEYUP action=...`
   proves only OS acceptance; compare it with Spider-Man's actual response.
6. Test Pause and Bink/menu handoff, then Alt+Tab while holding a 3D action.
   On losing focus, verify a release and
   `SDL3-UNIVERSAL: original game-owned DirectInput pointer RESTORED`
   (only if a non-NULL pointer had actually been borrowed). Do not proceed
   if keys remain held, menu behavior breaks, or the game crashes.
7. Open the Joystick Configuration panel and verify a
   `SDL3-UNIVERSAL: setup modal entered` release/restoration trace before
   interacting with the modal, then close it and retest gameplay.

**Immediate rollback:** set `ExperimentalSDL3UniversalGameplay=0` and
restart the game. The adapter is not promoted to default, and its draft PR
must not merge before actual Windows and Proton hardware acceptance.

The PR #96 setup UX has separately verified Cross capture, Circle
reassignment, single-focus navigation and Cross Restore defaults. These
fixes must be reconciled with PR #95 in a subsequent integration branch;
they are not implied by PR #95's successful CI build.
