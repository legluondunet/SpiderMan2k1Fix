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
