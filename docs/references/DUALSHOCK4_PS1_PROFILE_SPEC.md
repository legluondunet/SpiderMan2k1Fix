# DualShock 4 automatic PS1-style profile — agreed design

Status: **experimental first implementation on PR #74, manually opt-in,
NOT ready as an automatic full controller profile**. Seven-action PS1 mappings,
early Sony DS4 acquisition guard, in-memory virtual Spidey.cfg overlay,
original-user-settings preservation and contextual SHARE/Escape pulse
are now implemented. Real Wine/Windows validation remains outstanding.
Dependency: draft PR #73 `fix/native-save-dialog-title-spacing` still being
tested. This branch does not modify PR #73 or its test artifact.

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

## Prototype now implemented on this branch (not enabled by default)

To test just the first candidate, keep the rest of the original INI and add:

```ini
[Debug]
Log=1

[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
```

**This is a deliberately hidden, opt-in experiment**, not the final
`ControllerProfile=Auto` fixed profile. Without
`ControllerPS1Preview=1`, the previous PR #73 behaves as before.
The INI validator accepts the new boolean option, but the first-run INI
generator does not enable it.

The branch adds:

- `ps1_gamepad_mapping.h`: seven native action offsets 0x50..0x68
  mapped to BUTTON1,2,0,3,4,5,7 (Jump, Kick, Punch, Web, Aim,
  Zipline, Swing), independent of Windows libraries and host-tested.
- `SpiderGamepadUI_PreparePS1`: only with a real focused game HWND
  and **one unique Sony VID/PID verified DS4**, try an early separate
  DirectInput8 nonexclusive Acquire before the original Spidey.cfg
  bootstrap. If it fails, do not modify gameplay mapping.
- `SpiderCfg_EnablePS1Preview`: memory-only overlay in the existing
  virtual Spidey.cfg. The PS1 UI preview is marked active only if
  the game's original CRT loader actually reads the virtual config
  successfully in its verified bootstrap. If that check fails,
  the original gamepad UI bridge remains unchanged.
- `save_settings`: temporary buttons are **replaced with the
  previous user settings in the serialized 252-byte RawHex**. The
  seven named [Joystick] settings are likewise saved from their
  pre-preview snapshot. No original stored controls are overwritten.
- `SpiderGamepadUI_OnFlip`: when the preview is active, SHARE emits
  Escape in playable 3D or the game-owned pause menu. The 3D Options
  experiment from PR #73 is bypassed, so the original BUTTON9
  tyrolienne collision does not produce a second synthetic pause.
  Inside pause options, Cross/Enter confirms and Circle/Escape returns,
  using original verified native pause-context state.
- No global controller hooks, no edits to the original executable,
  no persistent user mapping changes, no silent release update.

**Known unimplemented blockers for the full profile:**

1. The original game's *Joystick Configuration* screen is **not yet
   disabled or read-only**. Do not open or change that screen in the
   prototype; its native in-memory action table is not yet locked.
2. Original Start assigned to BUTTON11 (camera-related controller
   button according to the maintainer's observations) is not suppressed.
   The old button may continue to open the pause menu alongside SHARE.
3. Cross/OPTIONS replacement for native scripted 3D cinematic skipping
   is not yet implemented. Triangle's original scene behavior is
   unchanged and must be observed with the newly assigned Web action.
4. R1/R2/L1 physical DirectInput indices still need real hardware tests.
5. Disconnect/reconnect and original gamepad device acquisition
   coexistence need real Windows/Wine validation.
6. If `Spidey.cfg` was read before the bootstrap, or the early Sony
   Acquire/CRT probe fails, the new fixed mapping cannot safely start;
   read the log before assuming it is active.

Only after these are resolved and proven should the user-visible
automatic full profile be enabled by default.

## Second prototype: context guards, optional native cinematic skip, read-only Start probe

**Important:** These changes are on PR #74 only; they have not yet been
verified under Wine/Windows. The user-confirmed prior build was
`v1.1.0-480acea`. Keep that artifact for A/B comparisons.

1. **Pause-menu recognition:** the user log shows the purple PAUSED
   ribbon, confirmed by native D3D7 drawing, even when the original
   `0x682950` pause-context pointer is zero on a sampled Flip.
   The new classification therefore combines the old positive native
   pointer with a 0–1-frame-old exact **widescreen native ESC ribbon**
   signature (`native_pause43_banner_flip`), guarded to validated
   1920×1080/1920×1200 geometry. This should allow Cross=Enter and
   Circle=Escape in the visible pause UI, without firing Jump/Kick;
   test both normal pause and the confirmation dialog. Classic 4:3
   still needs separate menu-owner verification.
2. **OPTIONS for 3D scripts is a separate manual experiment:** add
   `ControllerPS1OptionsSkip=1` to the existing `[Input]` section
   **only if testing this particular change**. It is absent/Off by
   default and only works alongside `ControllerPS1Preview=1`.
   On a fresh OPTIONS press during actual 3D, it verifies a readable
   and game-owned CutSceneScript object (original pointer
   `0x6A9038`), context=1, script active=1, original
   CutSceneSkipAllow=1, nonzero script IP, and unchanged
   `0x60CFC4` request byte=0. Only then it emits the original
   native `0x60CFC4=1` **single skip request**, which the original
   `0x4BC3E0` script cleanup consumes. No fake key or game exit.
   If inactive/unreadable/unskippable/paused/Bink, it refuses with a
   diagnostic. This native write is **unvalidated in this code path**
   even though the earlier debug F10 experiment used the same
   request; leave the option Off for general use.
3. **Cross for 3D scripts:** native action Jump (`0x10`) was
   previously bound to Triangle under the default PC gamepad map,
   and the existing native cinematic input research found Jump also
   participates in script skipping. The PS1 overlay now binds Jump
   to Cross, so **Cross might already skip scripted 3D normally**.
   Confirm this in-game before adding any further synthetic action.
4. **Joystick configuration protection:** game-owned virtual
   `Spidey.cfg` writes now immediately reassert the seven PS1
   in-memory action entries. Any settings save writes back the
   original seven mappings, never the preview mapping. If the
   original CRT bootstrap did not consume the virtual config, the
   overlay is rolled back. **This is NOT a UI lock:** the original
   Joystick Configuration screen is still accessible and might alter
   the game's separate live button/action tables until next reload.
   Do not edit it while testing; further exact-native-menu research
   is required to actually disable or make the screen read-only.
5. **Start/BUTTON11 research:** the historical native table remapper
   was rejected by its PE header check **before table discovery**.
   PR #74 introduces a one-shot, **read-only** scan of writable
   `.data` for the original 14-action mapping sequence, with a
   less arbitrary PE SizeOfImage bound, VirtualQuery memory guards,
   and a 1 MiB scan bound per data section. New log messages
   `GAMEPAD-PS1: START-TABLE probe ...` and
   `GAMEPAD-PS1: START-TABLE candidate ...` will tell us if the
   previously assumed table layout is present. **NO original Start
   button is disabled or remapped in this build.**

Second-candidate acceptance checks: compare the same loaded save on
the old successful build and new one; check SHARE and both in-game
PAUSED/REALLY QUIT dialogs; press Cross in EACH native scripted 3D
cinematic to verify its natural Jump action; with explicit skip opt-in,
press OPTIONS **once after SkipAllow becomes true**; check legacy
camera-related Start remains (expected pending native-remap); then
inspect all `GAMEPAD-PS1` and `GAMEPAD-UI` logs.

### INI for the optional 3D cinematic experiment

```ini
[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1OptionsSkip=1

[Debug]
Log=1
```

Do not enable the last line in `[Input]` in the normal profile; it is
for controlled testing until the runtime confirms exact behavior.

## Next phase after prototype validation

- Complete native joystick-menu lock, native Start suppression, scene
  classifier and fixed profile default are **future work**, not already
  functioning features.
- No changes to draft PR #73 during the user's present runtime test.
- No merge until explicit maintainer validation.
