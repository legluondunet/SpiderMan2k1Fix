# DualShock 4 automatic PS1-style profile — agreed design

Status: **design specification only**. Not yet implemented or validated.
Dependency: draft PR #73 `fix/native-save-dialog-title-spacing`, still being
tested. This proposal must not modify, merge, or invalidate its test build.

Author: legluondunet assisted by ChatGPT (OpenAI)

## Objective

With a recognized, unique Sony DualShock 4, play **all** of the original
Spider-Man PC (2001) with a controller: initial videos and still, title and
option menus, in-game 3D cinematics, full gameplay, and pause/options menu.
Adopt the **PlayStation 1 gameplay arrangement** with more intuitive,
consistent navigation: **Cross confirms**, **Circle backs out**, and
**SHARE opens/closes the in-game menu**. No keyboard required for normal use.

These are the maintainer-approved *target* mappings, not claims of existing
behavior. Input gating and renderer/media detection require real-game tests.

## Activation / compatibility

- `[Input] ControllerProfile=Auto`: detect Sony DS4 only by trusted VID/PID
  and an unambiguous device. If no recognized device, the patch's profile
  stays **Off** and the original game's controller support is unaffected.
- `ControllerProfile=Off`: fully restore original input handling, including
  original gamepad configuration screen and the player's stored mappings.
- Activation should be **atomic**: all context-specific bindings must become
  consistent together, never half apply a fixed gameplay map and old menu
  bindings. An unsupported input layout must fail safely to Off.
- No forced rewriting of `[Joystick]`, `[Internal] RawHex`, or the user's
  original preferences. Keep existing saves/other controller mappings.
- Prefer the Sony DS4 v1/v2 mappings observed by DirectInput8 on Wine:
  `054C:05C4`, `054C:09CC`, possibly original `054C:0BA0` adapter.
  Verify USB/Bluetooth layouts individually; never guess from device name.
- No hooks into non-game processes or generic system/global hotkeys.

## Contextual UI actions

| Context | Movement/selection | Cross (BUTTON1) | Circle (BUTTON2) | OPTIONS (BUTTON9, PS4 Start) | SHARE (BUTTON8) |
| --- | --- | --- | --- | --- | --- |
| Opening Bink videos | — | Skip when original permits | — | Skip when original permits | — |
| Interactive static image before main menu | — | Advance/Enter | — | Advance/Enter | — |
| Main menu and nested Options/Joystick-disabled menu | D-pad + left analog native | Confirm/Enter | Back/Escape | Optional Enter only where safe for startup | No assigned menu command |
| Post-menu Bink movies | — | Skip when original permits | — | Skip when original permits | — |
| Scripted 3D cinematics | Native scene state | Skip | No injected Back | Skip | No pause during scripted scene |
| Active playable 3D | Native movement | **Jump** | **Kick** | No gameplay action by default | **Open pause/menu** |
| In-game pause/options | D-pad + left analog | Confirm/Enter | Back/Escape | Optional resume where native behavior allows | **Close/resume** |

One meaning per context; never inject contradictory Enter+Escape, never send
a movie-skip event as a gameplay button, and never open pause during an
unskippable native scripted scene. Maintain the original movie time gates.

On all menus (including pause/options), Cross must confirm and Circle must
return. The previous suggestion to use Circle to *validate* a pause menu is
**superseded** by the explicit new decision: **Circle always returns in UI**.

## Fixed gameplay actions (PS1-inspired)

| Native PC action | DS4 physical button | DirectInput zero-based index |
| --- | --- | --- |
| Jump | Cross (✕) | `BUTTON1` |
| Kick | Circle (◯) | `BUTTON2` |
| Punch | Square (□) | `BUTTON0` |
| Web | Triangle (△) | `BUTTON3` |
| Aim / camera | L1 | `BUTTON4` |
| Zipline | R1 | `BUTTON5` |
| Swing | R2 | `BUTTON7` |
| Pause / native menu activation | SHARE | `BUTTON8` |

Indices are consistent with the Sony 054C:05C4 layout used in PR #73,
but **only Cross, Circle, OPTIONS and SHARE indices are presently
instrumented in the experimental UI bridge**. Independently validate L1,
R1, R2, Square, Triangle and native Start/SHARE behavior. The current
factory virtual cfg has different gameplay bindings: Jump 3, Kick 4,
Punch 0, Web 1, Aim 6, Zipline 9, Swing 7.

The original game accepts in-game configuration for only 7 actions
(`[Joystick]` `Jump,Kick,Punch,Web,Aim,Zipline,Swing`). The native
Start/Pause mapping is distinct, observed near `BUTTON11` on the
supported executable, and is not one of these 7 editable entries.

**Do not conflate PS1 Circle = Kick while playing with Circle = Back while
in menus.** Context must be checked at the input-action owner, not by
reusing a generic renderer 2D/3D flag.

## Fixed profile and game's original joystick menu

The maintainer explicitly requests **no user customization while the
automatic profile is On**. The original game's *Joystick Configuration*
screen must therefore be unavailable or genuinely read-only, with a
clear explanatory message directing the player to `ControllerProfile=Off`
to restore customization.

Do not merely overwrite the INI on next boot while temporarily letting
the user change buttons in memory: this can create inconsistent mappings,
incorrect UI labels and accidental loss of custom settings. If a safe
native menu lock cannot be proven against the supported EXE, **do not
claim full fixed-profile support**. Delay activation of that feature.

Always restore previous personal controls when returning to Off; never
overwrite original `[Joystick]` values or `RawHex` for that purpose.
Saved keyboard and audio settings must remain independent and writable.

## Input pipeline / guardrail requirements

1. Read/poll the unique verified pad independently until safe signatures
   for the game's native action inputs are identified. Never hijack an
   unknown DirectInput COM vtable. Preserve controller vibration.
2. Classify actual engine contexts separately: Bink via verified native
   `GameFMV_Active`; title intro still; menus; gameplay 3D; scripted 3D
   cutscenes; paused in-game menus. The presentation mode
   `menu_upscale_active` is NOT a sufficient gameplay/cutscene marker,
   and `gameplay_4x3_active` is false in widescreen Auto.
3. Implement fixed gameplay mapping at a **verified native game action
   interface or guarded controller mapping table**, not by sending fake
   keyboard keypresses for continuous movement/combat. Maintain combos.
4. Carefully replace/suppress the original native Start button
   (`BUTTON11` observed) only when the new SHARE assignment has passed
   in-game validation. Avoid camera actions also opening pause.
5. Video and interactive-still advance must use the original engine's
   input/skip checks; a successful synthetic Return by itself proves
   only the request was sent, not that it is consumed by the scene.
6. On entering/leaving any context or losing focus, release synthetic
   keys. Debounce button edges and prevent repeated menu transitions.
7. Context classification failure, unsupported executable SHA or
   ambiguous controller -> no input surgery; retain original controls.
8. Preserve existing focus-loss pause, 30 FPS, widescreen and
   virtual `Spidey.cfg` functionality.

## Required tests before enabling by default

- Cold boot with DS4, opening four videos, static Enter-only still,
  main menu, every submenu, and non-destructive Escape navigation.
- Choose New Game, identify the type of the post-menu cinematic
  (Bink versus static comic versus scripted 3D), and verify Cross and
  OPTIONS skips only where the original allows.
- During scripted 3D, verify both Cross and OPTIONS can skip, with no
  accidental Web/Triangle or gameplay commands.
- In playable 3D verify all seven actions and combo actions, aim,
  zipline, swing, D-pad/analog and no camera-induced pause.
- Press SHARE to pause, navigate with analog/D-pad, Cross to confirm,
  Circle to back out, SHARE to resume. OPTIONS must not fire Zipline.
- Check the original joystick-configuration menu is actually disabled
  or read-only while the fixed profile is active.
- Turn profile Off and confirm saved original mappings, normal Joystick
  menu, keyboard and audio settings are unchanged.
- Alt+Tab during video, menus, paused gameplay; USB unplug/replug;
  multiple controllers; unsupported controller; different resolutions.
- Confirm under Wine and native Windows with logs; build/test CI green.

## Non-goals for this design-only PR

- No code changes, no release, no user control remapping yet.
- No changes to draft PR #73 during the user's present runtime test.
- No merge until explicit maintainer validation.
