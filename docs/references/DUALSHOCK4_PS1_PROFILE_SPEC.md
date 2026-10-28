# DualShock 4 automatic PS1-style profile — agreed design

Status: **experimental opt-in implementation on PR #74, not yet a
released automatic controller profile**. Seven PS1 gameplay actions,
verified DS4 acquisition, virtual Spidey.cfg preservation and SHARE/Escape
pause have been exercised on the maintainer's Linux/X11/Wine setup.
Native Windows, other DS4 revisions/transports and failure paths remain
unverified.
Dependency: draft PR #73 `fix/native-save-dialog-title-spacing` still being
tested. This branch does not modify PR #73 or its test artifact.

Author: legluondunet assisted by ChatGPT (OpenAI)

## Mouse controls — confirmed menu-only; disabled by default (2026-10-28)

The original Spider-Man **PC 2001** manual lists gameplay actions only for
keyboard and joystick, and its main/options menu can be fully operated via
arrows, Enter and Escape. The game's own Windows README likewise says menus
can use either the mouse or keyboard. The PC developer explicitly described
mouse functionality as **menus only** in a GameSpot interview, not as camera,
aiming or combat controls. Do **not** confuse this 2001 game with Spider-Man:
The Movie (2002), which supports gameplay mouse control.

Sources:
- Original PC 2001 manual:
  https://oldgamesdownload.com/wp-content/uploads/Spider_Man_Manual_Win_EN.pdf
- Original Windows README:
  https://oldgamesdownload.com/readme/spider-man-windows-readme-english/
- Developer interview:
  https://www.gamespot.com/articles/spider-man-preview/1100-2798115/

The maintainer approved **game-internal mouse disable by default** for every
user, including **keyboard-only** configurations. `[Input] MouseEnabled=0`
is now written into newly generated INIs, and is the default even if absent
from an older INI. To opt back into original mouse-driven menu selection:

```ini
[Input]
MouseEnabled=1
```

Add the key inside the **existing** `[Input]` section only; do not duplicate
the section or the key. The original native `gRenderTest & 0x10` mouse gate
is still verified against its exact three original instruction signatures
and writable single game-owned flag before any change; if verification fails,
the original mouse remains enabled. Keyboard input and controller gameplay
are unaffected. The fix never hides/hooks/repositions the Windows/X11 desktop
mouse. During loss of game foreground it releases **only the bit owned by the
fix**, and reapplies suppression when the game regains the foreground.

The old `ControllerAutoDisableMouse=1` setting is **retired and ignored**,
but recognized by the INI validator to avoid deleting settings on upgrade.
`MouseEnabled=1` unconditionally selects original game menu mouse behavior
even if an old INI contains `ControllerAutoDisableMouse=1`. This global
policy is independent of Sony DS4 detection, controller profile selection,
hotplugging and DirectInput polling.

This is a code+CI candidate until the maintainer validates real keyboard-only
gameplay, 2D menus, Alt-Tab and mouse opt-in under Wine and Windows.

## Controller consolidation checkpoint (2026-10-28, verified where noted)

The maintainer confirmed **PS4 OPTIONS/BUTTON9 alone does nothing** in the
original 2D main menu while PS1 preview is active. The runtime log
`SpiderMan2k1Fix(20261028-103357).log` (`v1.1.0-b6e22e7`)
also confirms unique Sony DS4 `054C:05C4`, successful original
`Spidey.cfg` bootstrap, Cross-only preview activation, SHA-checked
native Start/BUTTON11 unbinding, SHARE → native pause, Cross → Enter,
native request-7 Quit/Yes teardown, and a blocked inherited Cross
edge on the first returned 2D Flip (85 ms after the last 3D sample).
**The origin of that delayed Cross state remains unknown.** The
original Start/BUTTON11 physical-button no-op has not been separately
demonstrated by this particular log. No conclusion is drawn about
native Windows or DS4 v2/Bluetooth layouts.

For regular **opt-in PS1 testing**, keep the following values inside
the **existing** `[Input]` section of `SpiderMan2k1Fix.ini`
(do **not** append another section with the same name):

```ini
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1ReadOnlyJoystick=1
ControllerPS1DeepStartProbe=1
ControllerPS1DisableNativeStart=1
MouseEnabled=0
ControllerPS1TransitionEdgeOwnership=1
ControllerPS1PostQuitGuard=0
ControllerPS1OptionsSkip=0
ControllerPS1QuitTrace=0
ControllerPS1InputFlightRecorder=0
```

Cross/BUTTON1 is always the sole **synthetic** Enter source in PS1
preview, irrespective of the legacy `ControllerEnterButtons`
setting. Circle backs out; SHARE opens/closes the 3D pause.
OPTIONS/BUTTON9 neither confirms in 2D nor requests 3D script skip
while `ControllerPS1OptionsSkip=0`. The **different** original
Start/BUTTON11 is unbound through a strictly checked, opt-in,
RAM-only patch; both switches must not be confused.

**Experiment/diagnostic cleanup:** The legacy eight-Flip
`ControllerPS1PostQuitGuard` is now an optional **fallback**, not the
recommended active protection. `ControllerPS1TransitionEdgeOwnership`
is the tested no-timeout alternative and stays explicitly opted-in.
`ControllerPS1QuitTrace` and `ControllerPS1InputFlightRecorder`
are diagnostic-only and can be left at 0 for routine play.
`ControllerPS1DeepStartProbe` remains a safety precondition for
Start unbinding: do not delete or bypass it yet. These keys remain
recognized for compatibility and further A/B testing; do not
automatically enable opt-in native patches on untested Windows
configurations.

**Remaining acceptance matrix:** Check the seven physical gameplay
actions, 3D scripted cinematic skip by native Cross, repeated menu
Quit/Yes cycles without diagnostic logging, physical old Start/BUTTON11,
original keyboard Joystick screen isolation, disconnect/reconnect or
GetDeviceState failure, independent game mouse restoration on focus
loss, DS4 v2 and USB/Bluetooth mapping differences, and native Windows.
CI regression passes do not substitute for real hardware validation.

The 10:33 game launch also repaired one invalid `SpiderMan2k1Fix.ini`
entry and backed up the original as `SpiderMan2k1Fix.ini.old`.
The log does **not** identify the invalid key or duplicate; compare
the old and regenerated file instead of assuming the cause.

## Maintainer acceptance: read-only Joystick menu (2026-10-28 02:53)

Maintainer tested `v1.1.0-84ec4fd` with original DS4 `054C:05C4`
and explicitly confirmed that the original **Joystick Configuration
screen is truly frozen/non-editable**, still accessible, and shows the
seven intended PS1 controller labels correctly. Runtime log confirms
`JOYSTICK-READONLY EXPERIMENT ACTIVE`, original Start unbound,
and two original game settings saves with personal joystick bindings
preserved. **Validated on this controller/setup only; leave opt-in.**

### New separate user request: disable the GAME'S mouse while using DS4

An original-game design issue appears when using controller-confirm
with a mouse pointer left hovering over a different menu row: the
mouse-highlighted row wins over the controller-selected row. Merely
hiding the Windows cursor **does not prevent that selection error**.

The original `PCShell.cpp` and `PCInput.cpp` provide a better native
solution: `gRenderTest & 0x10` is used by the exact functions
`PCSHELL_DrawMouseCursor` (VA `0x0050C470`),
`PCSHELL_UpdateMouse` (flag load at VA `0x0050C4CF`), and
`PCSHELL_IsMouseOver` (VA `0x0050C5F0`). All three read the same
`gRenderTest` word at original VA `0x02E0988C`. The original
`PCINPUT_Initialize` uses the same flag to skip mouse-device setup,
although this optional fix is intentionally applied **later** so
that it does not change device initialization.

The opt-in `[Input] ControllerAutoDisableMouse=1` is **OFF by
default**, and only activates when `ControllerPS1Preview=1` and
a unique authentic DS4 has been acquired and the native game config
bootstrap confirmed. It sets only the existing original game-owned
bit `0x10`, after verifying the untouched original x86 signatures
and writable native flag. This simultaneously hides the game-owned
mouse sprite, stops the game's own mouse-position polling and ensures
its menu hit-test returns false. The keyboard and UI controller input
remain active. No Windows `ShowCursor`, no global mouse hook, no
DirectInput COM hook, no cursor warping.

On losing game foreground, hidden window, missing DS4 or failed pad
reading, the module removes **only the bit it originally set** and
returns the game's mouse controls. This is a game-level change only;
the desktop cursor remains owned by the existing Alt+Tab focus recovery.
When the option is absent, `0` or the PS1 profile is inactive,
original mouse functionality is unchanged. Full game restart also
resets the volatile experiment.

**Experimental test plan:** enable only this additional option next
to the previously successful PS1 read-only/controller profile. With
the game's menu showing, move its mouse cursor over option B while
selecting option A using the gamepad D-pad; press Cross and verify
A is selected, not B. The red native game cursor should not be drawn
while DS4 is active. Test both main and Options/Joystick menus, real
mouse movement and clicks (should not affect game's menu), normal
controller navigation and Cross/Circle, controller disconnect/read
failure if safely possible, and Alt+Tab back to desktop (desktop
mouse must continue working). With the option disabled and game
restarted, normal mouse selection must return.

Sample targeted INI:

```ini
[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1ReadOnlyJoystick=1
ControllerPS1DeepStartProbe=1
ControllerPS1DisableNativeStart=1
ControllerAutoDisableMouse=1
ControllerPS1OptionsSkip=0

[Debug]
Log=1
```

Expected log: `GAMEPAD-MOUSE: ARMED verified native mouse`, then
`GAMEPAD-MOUSE: GAME MOUSE OFF (DS4 active)`. Focus/device failure
should produce `GAMEPAD-MOUSE: GAME MOUSE RESTORED`. Neither
proves correct selection behavior until the maintainer checks it
in-game. Do not promote or merge PR #74 automatically.

## New in-game Quit → Yes regression report (Oct 28, 2026)

The maintainer **confirmed the experimental** `ControllerAutoDisableMouse=1`
**works**: the game cursor and stale mouse hover no longer override
DualShock 4 selection.

**Separate unresolved bug:** In playable 3D, pressing SHARE opens
native pause; selecting **Quit** shows a **Yes/No confirmation**;
confirming **Yes** unexpectedly launches a new game rather than
returning as intended. The user reports this may also happen when the
game mouse is enabled; **causality not yet established**. There is no
log of this specific new incident attached yet. Do not mark it fixed.

### What the verified original code shows

- Original `PCSHELL_IsMouseOverText` (`0x0050C6C0`) checks
  `gRenderTest & 0x10` and immediately returns zero when mouse is
  disabled. This is **in addition to** the already checked original
  cursor/mouse-update/rectangle-hover gates. Therefore a lingering
  hit-test in this specific helper is unlikely to explain the bug
  with `ControllerAutoDisableMouse=1`.
- Original handler `PShell_EndTrainingUpdate` (`0x0047BC40`) has a
  selection index at the *verified menu object* `0x682968 + 0x14`:
  index 0 requests engine code 8, 1 requests code 10,
  2 requests code 7 (return to main menu). **Important:** this
  handler is named EndTrainingUpdate; do not assume every main-game
  pause/confirmation uses the same menu object or mapping.
  A **separate** original `0x440EF0` gameplay input owner can
  request 7. Thus code inspection alone is not proof which branch
  produced the user's new-game launch.
- Previous game logs already contain genuine transitions 7 and
  successful normal teardown; they do not record the new regression.
  Our current gamepad bridge synthesizes a **single Enter on a Cross
  edge** in recognized 2D/native-pause contexts. We must establish
  whether the wrong transition happens **before** a return to menu
  or whether a subsequent gamepad confirmation is misrouted.

### Conservative diagnostic candidate (OFF by default)

Enable `[Input] ControllerPS1QuitTrace=1` (and `[Debug] Log=1`)
while retaining the otherwise functional DS4 profile. The new
`GAMEPAD-PS1: QUIT-TRACE` messages sample the original
`0x682968` confirmation owner, `0x682960` auxiliary owner, their
readable selection bytes at `+0x14`, original `0x682950`
pause context, `0x60CFA4` engine transition request,
mouse-off bit, and **separate physical Cross/Circle/SHARE press edges**.
The pointer reads are VirtualQuery-guarded; at most 130 changed-state
samples are logged; no original game values are written. Native
owners may live for less than a Flip, so a `choice=-1` does not
prove an absent choice.

On the **same save/level** and preferably with a fresh launch for
each run, compare:

1. Mouse disabled 1, confirm **Quit → Yes** with DS4 Cross.
2. Mouse disabled 1, use **keyboard Enter** for the final Yes (not
   Cross); avoid pressing another controller button until transition.
3. Mouse disabled 0, repeat Cross confirmation.
4. When the bad outcome occurs, distinguish **return to title menu
   followed by an automatic New Game**, from **direct loading
   of gameplay without reaching title**. These look similar but
   require different fixes.

Never patch the original mode request directly to 7 or force a Quit
before the native menu owner is identified and the player's exact
choice verified. Keep existing joystick read-only mapping and
SHARE/Start safeguards unchanged. PR #74 remains draft.

## Maintainer decision: Cross-only confirmation in the PS1 profile (2026-10-28)

The maintainer decided that PS4 OPTIONS/START is redundant with Cross for menu
confirmation. The PS1 preview therefore **always uses Cross/BUTTON1 as the
single synthetic Enter source** in native 2D, Bink, and paused 3D menus.
The legacy `ControllerEnterButtons=Both` setting remains available and
unchanged outside this opt-in PS1 profile, but does not re-enable OPTIONS
for menu confirmation while PS1 preview is active. Circle still backs out
and SHARE still opens/closes native pause.

This is separate from the original game's **Start/BUTTON11**, which the
already existing opt-in `ControllerPS1DisableNativeStart=1` can unbind
in validated original-game RAM (log `NATIVE-START-UNBIND EXPERIMENT ACTIVE`).
For an entirely inactive PS4 OPTIONS button, leave the independent
experimental `ControllerPS1OptionsSkip=0` (current/default setting).
Do not change gameplay controls or the legacy non-PS1 scheme.

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
| Main menu and nested Options/Joystick read-only screen | D-pad + left analog native | Confirm/Enter | Back/Escape | Optional Enter only where safe for startup | No assigned menu command |
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

## Fixed profile and game's original joystick menu — maintainer decision (Oct 28)

**Keep the original *Joystick Configuration* screen accessible and
make it a genuine read-only reference for the ACTIVE DualShock 4
PS1-style profile. Do not hide or remove the menu.** This replaces
earlier fallback language allowing the menu to be unavailable.

When the verified Sony DS4 profile is enabled, show the real runtime
fixed bindings in the existing seven gameplay rows (not the original
saved/factory values):

| Original joystick menu action | Physical DS4 control | Active native button |
| --- | --- | --- |
| Jump | Cross (✕) | BUTTON1 |
| Kick | Circle (◯) | BUTTON2 |
| Punch | Square (□) | BUTTON0 |
| Web | Triangle (△) | BUTTON3 |
| Aim / Camera | L1 | BUTTON4 |
| Zipline | R1 | BUTTON5 |
| Swing | R2 | BUTTON7 |

If the game's native text path can safely represent readable names,
prefer e.g. `CROSS (BUTTON1)`, `CIRCLE (BUTTON2)` etc. Otherwise
retain exact native `BUTTONn` values with a clear controller legend
using the existing safe UI presentation. Do not invent an eighth
gameplay setting inside the seven-row native screen: `SHARE
(BUTTON8) = pause/menu` may be described as a separate, non-editable
hint only if the actual native layout supports it. In menus, keep
Cross=Confirm and Circle=Back, irrespective of their gameplay actions.

**Read-only is behavioral, not just a save policy.** The menu must
remain navigable and closable with Cross/Circle, but selecting a
binding must not arm any native key/button-capture or reassignment
state, mutate any of the seven native live action slots, or accept
a replacement button. Displayed values must always agree with the
actual active PS1 mapping. A non-intrusive message such as
`DualShock 4 profile active — controls are fixed` may be used.

When `ControllerProfile=Off` or no unambiguous verified DS4 is
present, restore the original editable Joystick Configuration menu
and the user's actual original custom mappings. Never overwrite
original `[Joystick]` values or `RawHex` for the fixed profile.
Saved keyboard and audio settings must remain independent and
writable.

Do not merely overwrite the INI on the next boot while allowing
temporary native reassignment in RAM. If safe signature-gated native
UI input/capture prevention cannot be established against the
supported EXE, **do not claim full read-only support**. Keep this
as an outstanding requirement until verified in-game.

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
- Check the original joystick-configuration menu stays accessible, is truly read-only while the fixed profile is active, shows the seven actual DS4 bindings, and cannot arm a reassignment even temporarily.
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

## Runtime report: 2026-10-28 01:38:39, original v1.1.0-ce217d3

The user supplied `SpiderMan2k1Fix(20261028-013839).log`
(1007 lines). This logs one verified Sony DS4 (054C:05C4), original
EXE hash verification, seven correct PS1 overlay bindings, verified
native CRT config read, five distinct SHARE/Escape attempts during 3D
and native ESC ribbon draw. It confirms that the game returned to the
native main menu. There are no explicit ERROR or WARNING entries.

Three post-menu Bink Return injections are present; an injection log
is **not** proof that the original media gate accepted a skip. The native
script lifecycle records first/second SkipAllow but
`ControllerPS1OptionsSkip=0`: **OPTIONS 3D skipping was not tested**.

**Start diagnostic:** exactly one 14-action Start candidate at
`RVA 0x00168408`, with `Start=BUTTON11`. Critically, the
probe also says `truncated-regions=1`. The 1 MiB limit makes the
observed candidate count **incomplete**, not proof that a second table
does not exist. This justifies a separate bounded *read-only* deep
scan before attempting any native modification.

The log does NOT include a virtual `Spidey.cfg` settings write or save,
so it does not verify restoration of saved player preferences. The
Joystick configuration menu is still **not locked** and the original
Start input is still **not disabled**.

### Extended native Start table diagnosis (new, opt-in)

The next build adds `[Input] ControllerPS1DeepStartProbe=1`.
With the Sony DS4 PS1 preview active, it scans original executable's
writable `.data` by committed readable memory regions. The default
probe remains capped at 1 MiB; the optional extended diagnostic scans
up to 64 MiB per section, with `VirtualQuery` bounds and NO writes.
Results include `deep=1`, number of queried regions, truncation count,
candidate addresses and total matches.

This diagnostic **never** patches Start; even two matching read-only
tables are not sufficient to conclude a safe native remap. It may add
a one-time startup delay during the explicitly requested deeper scan.
Leave this option absent/Off during normal play. This is separate from
`ControllerPS1OptionsSkip=1`, which remains an unvalidated native
script-skip modification and should not be combined with a first
deep-scan test.

Suggested diagnostic INI:
```ini
[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1DeepStartProbe=1
ControllerPS1OptionsSkip=0

[Debug]
Log=1
```

After the read-only scan, check `GAMEPAD-PS1: START-TABLE` lines
and report no/one/multiple candidates with `truncated-regions`.
Compare against the user-validated previous runtime behavior; do NOT
change the joystick menu or persistent game settings during this test.

## Native Start/BUTTON11 disable experiment — October 28

Following the first **complete** read-only DS4 runtime scan
(`v1.1.0-1da610c`), the original verified retail `.data` contains
**one** 14-action candidate at RVA `0x00168408` (Start=BUTTON11);
`truncated-regions=0`, `deep=1`, `queried-regions=1`.
The earlier two-table assumption is not supported by this scan.

The **next build** adds the manually opted-in **experimental**
`ControllerPS1DisableNativeStart=1`. This is **not** enabled in
`ControllerProfile=Auto`, and **not** validated in-game. This
option must **also** have `ControllerPS1Preview=1` and
`ControllerPS1DeepStartProbe=1`; otherwise it fails closed.

Even after a complete read-only scan, the original Start binding is
modified **only if** all of the following are true:

- One and only one exact original **14-action sequence** is present
  in one fully scanned writable `.data` section; no truncated or
  unreadable regions; only one matching **strict factory** table.
- All original action masks are correct; unused action slots have
  native `0x4000` sentinel, Start originally has `BUTTON11`, final
  action slot is 0, and other native button indices are valid.
- A **single aligned writable DWORD** at
  `RVA 0x00168408 + (11 * 8 + 4) = 0x00168464`
  remains mapped and Start still equals `11` during the checked,
  atomic compare-and-swap.

If and only if those checks succeed, the **one original Start button
index** changes in **process RAM only** to the already used native
unbound sentinel `0x4000`. No executable file, DirectInput COM
object, joystick save or unrelated action changes. Closing and
restarting the game discards this RAM-only change. SHARE continues
to synthesize Escape while the PS1 preview is active.

**Important risk/limitation:** the code's strict factory check is
derived from earlier native mapping research and might fail for the
user's real table. That is intended: no write should occur if any
assumption is wrong. Even a successful atomic RAM write does **not**
prove that this is the game-owned *live* Start table; the real runtime
test must show that the old camera/R3/BUTTON11 no longer opens the
pause menu, while SHARE still works. In addition, the original game's
separate Joystick Configuration screen is **not yet disabled**.

### Single-variable A/B test (only after the optional build is ready)

Start with the same INI and game install as the successful prior run.
For a conservative test first leave `ControllerPS1DisableNativeStart=0`
and check `START-TABLE ... strict=1`. Then, in a separate launch:

```ini
[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1DeepStartProbe=1
ControllerPS1DisableNativeStart=1
ControllerPS1OptionsSkip=0

[Debug]
Log=1
```

Find `GAMEPAD-PS1: NATIVE-START-UNBIND EXPERIMENT ACTIVE` or
`... REFUSED`. Only when ACTIVE: open the in-game menu with SHARE,
try the **old camera/R3** button and report whether it still opens
the menu. Do not edit the Joystick Configuration menu during this
test, and do not enable the unrelated unvalidated OPTIONS cinematic
skip experiment at the same time.

The experiment is not approved for default enablement or release.
PR #74 remains a dependent draft and cannot be merged until explicit
maintainer validation.

## Native Start unbinding: real runtime result, 2026-10-28 02:05

The user tested `SpiderMan2k1Fix(20261028-020502).log`
(`v1.1.0-d667441`) with
`ControllerPS1DisableNativeStart=1`. Exact original EXE SHA-256
and one Sony DS4 054C:05C4 were verified.

**Memory modification succeeded**:

```text
GAMEPAD-PS1: START-TABLE probe data-sections=1 candidates=1 strict=1 truncated-regions=0 ... deep=1 queried-regions=1 cap-MiB=64
GAMEPAD-PS1: NATIVE-START-UNBIND EXPERIMENT ACTIVE RVA=0x00168464 Start BUTTON11 -> native unused 0x4000
```

SHARE produced four `PS1-PREVIEW SHARE -> Escape` messages in
3D/pause, and the original ESC ribbon was drawn. The game continued
playing and returned to native menus using original cleanup without
an explicit ERROR or WARNING. **This confirms the RAM write, NOT
yet that physical BUTTON11/R3 stopped invoking native Start.** The
log has no native physical BUTTON11 polling trace and the user has
not explicitly reported whether the old button opened pause.

A stale string `native Start button still unchanged` appeared next
to SHARE presses even after the successful Start table update. This
was an *outdated diagnostics message*, not a contradictory memory
read. The follow-up build removes the misleading text, records whether
the RAM patch was actually accepted, and logs up to 12 separate
`ORIGINAL-BUTTON11 PHYSICAL PRESS` edges (with native-pause-at-edge),
without altering any joystick event. Even `native-pause-current=0`
at the instant of the press is not conclusive: the player should also
observe whether the menu opens.

**Next focused test:** In playable 3D with the experimental
Start-unbind option enabled, first ensure SHARE opens/closes native
pause. With the menu closed, press the old physical camera/R3 button
**by itself**, not together with SHARE, and report whether the menu
remains closed. Look for `Start-RAM-unbound=1` in the new
`ORIGINAL-BUTTON11 PHYSICAL PRESS` entry. If there is no entry,
the physical button may not actually be DS4 DirectInput BUTTON11
on this hardware/transport. To isolate the experiment, leave
`ControllerPS1OptionsSkip=0`. Do not change gameplay bindings.

The 02:05 log **did not exercise** original settings saves
(`SPIDEY-CFG: virtual WriteFile` absent), so it doesn't add
additional persistence evidence beyond the previous runtime test.

## Native Joystick Configuration read-only candidate (PR #74, new opt-in)

The approved design keeps the **original** two-column Joystick
Configuration screen accessible. A new manually enabled prototype now
implements this approach using the game's own native PCShell routines,
rather than an overlaid dialog.

Source research: [krystalgamer/spidey-decomp
`PCShell.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/PCShell.cpp)
describes `initActionMaps()`, `processControllerScreen()`,
`displayControllerScreen()` and the original eleven-row UI;
the accompanying `tools/names.json` and archived x86 function
bytes identify their original addresses in the 2001 PC executable.
The repository's own `PCShell.cpp` reconstruction still labels
`processControllerScreen` as unfinished, so this prototype used
the **original function's x86 instruction paths** to locate the
exact input handlers rather than assuming the C++ stub was executable.

The experiment requires a **unique verified Sony DS4**, an active
PS1 virtual config bootstrap and the new key below. It is absent/Off
by default:

```ini
[Input]
ControllerProfile=Auto
ControllerPS1Preview=1
ControllerPS1ReadOnlyJoystick=1
```

On successful signature checks, it applies **six guarded, restart-reversible,
RAM-only native code edits**, without modifying game assets or DLL
DirectInput COM objects:

1. At original VA `0x50D28C`, redirect only the joystick name
   formatting call used by `initActionMaps` to display `CROSS`,
   `CIRCLE`, `SQUARE`, `TRIANGLE`, `L1`, `R1`, or `R2`
   according to the **actual native button index** before the
   game's original second menu column is built. Unknown button
   indices keep `button N`, and the existing menu draw/scroll
   routines remain untouched.
2. At `0x50D3FA`, redirect *the original rebind-capture
   entry itself* through a tiny guarded original-x86 trampoline.
   It checks the original `gActionMapRelated` at `0xAC129C`.
   In **Joystick Configuration** it jumps to the game's own
   `0x50D872` cleanup before any capture prompt can be armed.
   In **Keyboard Configuration**, it replays the two original
   instructions `push ebp; push 0x2000` and jumps straight
   back to `0x50D400`, retaining normal keyboard editing.
3. At `0x50D51C` and `0x50D6A6`, redirect the original
   key/controller reassignment-acceptance branches to their
   existing `0x50D852` cleanup/return as additional safeguards
   against any preexisting capture state.
4. At `0x50D4A9` and `0x50D7AD`, neutralize only the
   original controller-restore-default and controller-action
   setter call sites as extra protection.
5. Verify the PE32 base, section memory, executable page
   protections and **every original instruction byte** before
   any write. On a failed edit, attempt to restore all previously
   modified sites. No changes are attempted if any original
   byte differs. The patch is installed only when the new
   option is enabled and the native virtual config load succeeded.

Look for:
```text
GAMEPAD-PS1: JOYSTICK-READONLY EXPERIMENT ACTIVE
```

If instead the log says `JOYSTICK-READONLY REFUSED` or
`JOYSTICK-READONLY inactive`, **no read-only behavior is
guaranteed**. Do not modify joystick bindings in that state.

**Not yet validated:** the gameplay/menu interaction with all
button inputs, mouse clicks, arrow navigation, Cross/Enter and
Circle/Escape, the original Restore Default Settings row, whether
every menu transition remains accessible, and any original key-capture
prompt reached before the verified assignment branch. It is
also not yet proven under Wine or native Windows for the new
code patch. A guarded conditional trampoline is used specifically
to preserve the original **editable Keyboard Configuration** menu. The host regression suite and CI compilation do
not substitute for a physical in-game trial. If any control can
still be changed or a stale prompt appears, leave the option Off
and use the previously validated PS1 controller build.

When the option is absent/Off, all these native code locations are
left untouched; the original editable Joystick screen is retained.
The game's saved personal `[Joystick]` and 252-byte `RawHex`
remain preserved independently by the existing virtual config
save/restore system.

## Next phase after prototype validation

- Complete native joystick-menu lock, native Start suppression, scene
  classifier and fixed profile default are **future work**, not already
  functioning features.
- No changes to draft PR #73 during the user's present runtime test.
- No merge until explicit maintainer validation.

## Reproduced post-Quit synthetic Enter race — October 28, 2026

Three real-game logs using the same `v1.1.0-ccc2f51` build distinguish:

- **Trace ON** `SpiderMan2k1Fix(20261028-075753).log`: original native request 7, clean 3D teardown, main menu displayed; maintainer confirms no unexpected New Game.
- **Trace OFF** `SpiderMan2k1Fix(20261028-080648).log`: synthetic Return #3 occurs immediately at the first 2D Flip (350) after native request 7, and New Game comic `L1COV.bmp` is drawn on that same Flip. A later return repeats this pattern.
- **Trace OFF** `SpiderMan2k1Fix(20261028-081250).log`: a synthetic Return occurs on Flip 734, in which `renderer2d=1, mode_request=7`, then the New Game comic is drawn on the **same** Flip; this repeats on Flip 853. The original 3D teardown is successful each time.

The premature synthetic Enter is a strongly supported immediate cause of unwanted New Game, while **why** the independent pad reader reports a Cross edge on this specific transition is not yet proven. Enabling the read-only QuitTrace changes timing, not semantics.

New opt-in guard under `[Input]`:

```ini
ControllerPS1QuitTrace=0
ControllerPS1PostQuitGuard=1
```

The guard applies only when the verified PS1 preview is active. It remembers valid playable 3D and looks for the first 2D frame with original `0x60CFA4` request equal to 7. On that transition it blocks **only synthetic UI Enter** for eight 2D Flip calls, consuming the physical Cross/Options edge normally. It never sends a new key, edits the original native game state, changes the original Joystick mappings, suppresses the user's real keyboard, or alters the engine's request 7. Other menus, movies, initial boot and non-request-7 transitions are unaffected. The option is off by default. Its effect has now been confirmed on the maintainer's original DS4 in the 2026-10-28 runtime A/B described below; broader validation is still needed before considering any default change or release.

Look for `GAMEPAD-PS1: POST-QUIT-GUARD suppressed synthetic Return on first 2D menu frames after native request 7` in the new test log. If the test fails, preserve the log and revert the opt-in instead of speculating about the game's native input owners.

### Maintainer validation: post-Quit guard works — 2026-10-28 08:24

The maintainer explicitly reported **“cela fonctionne à présent normalement”** after the guarded build and supplied `SpiderMan2k1Fix(20261028-082416).log`. The running game reports `v1.1.0-89f2ef7`, the SHA-verified original `SpideyPC.exe` and Sony DS4 `054C:05C4`.

Two independent native 3D `Quit → Yes` transitions were observed:
1. First return: native `NATIVE REQUEST 7`, `NATIVE-3D-CLEANUP completed` and `CONTROLLER-7 native teardown COMPLETED` were followed by 2D `Flip=296`, `mode_request=7`. The PS1 guard logged `POST-QUIT-GUARD suppressed synthetic Return on first 2D menu frames after native request 7`, and the original `Shell_MainMenu` callback was reached.
2. Second return: same successful request/teardown, 2D `Flip=1046`, second guard suppression and second `Shell_MainMenu` return. There was **no immediate automatic New Game on either 2D transition**.
3. Normal later synthetic `Return` and `Escape` actions were still logged after the guard window. A later 3D game entry occurred only after additional menu activity, not as an immediate request-7 rebound.

This confirms the **behavioral fix in this tested setup**, not the as-yet-unresolved underlying cause of the spurious physical Cross edge. The feature remains **OFF by default** and PS1-preview-only pending broader regression coverage; **PR #74 remains draft and unmerged** until explicit authorization.

legluondunet assisted by ChatGPT (OpenAI)

## Root-cause investigation: bounded raw DI8 flight recorder (2026-10-28)

**Behavioral status:** `ControllerPS1PostQuitGuard=1` is independently
validated by the maintainer for two real native returns (`Flip 296` and
`Flip 1046`); preserve it unchanged while investigating why the independent
DS4 reader reports a new Cross rising edge on the first 2D frame.

**Source code inspection:** `previous_buttons` is the previous successful
raw `DIJOYSTATE2` Cross/Options mask. The code resets it to zero on lost
foreground or a failed `GetDeviceState`. Normal menu transitions do **not**
explicitly reset it. The earlier logs show no diagnostic failures, but did
not record every raw button state, nor each foreground read. Therefore they
cannot distinguish a physical release/repress, lost-input reset, or transient
DirectInput release/repress across the original game's native device teardown.
Do not claim any one as the proven root cause yet.

New **OFF-by-default** read-only diagnostic:
```ini
[Input]
ControllerPS1Preview=1
ControllerPS1QuitTrace=0
ControllerPS1PostQuitGuard=1
ControllerPS1InputFlightRecorder=1

[Debug]
Log=1
```

When enabled, a pure C99 96-element ring holds the raw Sony Cross, Options,
Circle and SHARE button bits from the independently acquired DirectInput8
device; the pre-read synthetic-confirmation mask; 2D/3D/movie/native-pause
classification; original engine `request`; milliseconds; and separate
events for a missed device, lost foreground or failed `GetDeviceState`.
Normal sampling performs no text formatting or file writes. Only when the
**existing, already validated post-Quit guard suppresses a synthetic Return**
does it print the buffered state transitions and last eight snapshots under
`GAMEPAD-PS1: FLIGHT-RECORDER`.

The recorder requires **all three**: authenticated PS1 preview, the already
validated post-Quit guard enabled, and this explicit recorder key. With its
key absent or 0, it does not read the clock or store histories, print
messages or alter the game input path. It does not replace the working guard.
`ControllerPS1QuitTrace` must remain 0 in this A/B to avoid its earlier
measured timing effect.

Interpret the raw state carefully: a DirectInput rising edge alone does
**not** prove a physical new button press. Look for whether Cross was held,
became 0, or suffered a failed read/reset just before the first 2D frame.
Do not attempt to remove the eight-frame guard until a replacement based on
verified menu/input ownership passes an independent real-game test.

**This investigation has no root-cause runtime evidence yet.** PR #74 stays
draft and unmerged.

legluondunet assisted by ChatGPT (OpenAI)

## Raw DI8 input evidence and new button ownership candidate (2026-10-28 08:44)

Trace `SpiderMan2k1Fix(20261028-084402).log` (`v1.1.0-aed5f9d`)
was collected with `ControllerPS1QuitTrace=0`,
`ControllerPS1PostQuitGuard=1`,
`ControllerPS1InputFlightRecorder=1` on original Sony DS4.

The second successful original native `Quit/Yes` transition emitted the
flight recorder when its first 2D `Flip=587` arrived:
- `seq=567`: `2d=0,3d=1,pause=0,CROSS=1,previous-mask=0`.
- `seq=568`: `pause=1,CROSS=1,previous-mask=1`.
- `seq=572`: `pause=1,CROSS=0,previous-mask=1`.
- `seq=573`: `pause=1,CROSS=0,previous-mask=0`.
- `seq=586`: `pause=1,CROSS=0,previous-mask=0`, successful DI8 read.
- `seq=587`: `2d=1,3d=0,pause=0,request=7,CROSS=1,previous-mask=0`,
  successful DI8 read and **guarded** synthetic Enter suppression.
- No failed DI8 reads or focus transitions in the 96-sample window preceding
  this return. There **was** a separate focus loss and DI8 re-acquire after
  the **earlier** successful Quit; that does not explain the second return.

This excludes the hypothesis that `previous_buttons` was reset to zero
because of a DI8 polling error or focus loss in the **last 3D → 2D
transition**. It supports an **input ownership/routing** problem:
the native game's Quit/Yes menu consumed an input before our independent
per-Flip DirectInput8-to-keyboard adapter saw the state and misinterpreted
it as a new Enter for the returned main menu.

Do **not** claim hardware/physical presses are proven by sampled bits.
A person could have pressed/released/repressed between samples; our logger
cannot see the exact moment when the original game interpreted Yes.

### No-timer replacement candidate, OFF by default

New `[Input] ControllerPS1TransitionEdgeOwnership=1` runs a pure C99
state machine: it first records that genuine gameplay 3D was observed.
On the **first** subsequent 2D main-menu Flip with original native
`request=7`, any Cross/Options confirm bits already depressed are marked
as **owned by the old native Quit menu** and not re-sent as synthetic Return.
Those inherited bits remain consumed while held, then re-arm immediately
when the raw DI8 bit is observed released. If no button is held at return,
no action is swallowed. No arbitrary eight-Flip timer; no native menu
writes or input hooks.

The previously real-game-validated `ControllerPS1PostQuitGuard=1`
remains untouched and available as a fallback. To test whether ownership
fix alone solves the bug, use the **alternative**, not both:

```ini
[Input]
ControllerPS1Preview=1
ControllerPS1QuitTrace=0
ControllerPS1PostQuitGuard=0
ControllerPS1TransitionEdgeOwnership=1
ControllerPS1InputFlightRecorder=1

[Debug]
Log=1
```

The flight recorder now works with either the old guard or the new owner,
and prints its buffered raw DI8 history when that selected mechanism
suppresses a duplicate Enter. Check
`GAMEPAD-PS1: TRANSITION-OWNERSHIP consumed inherited Cross/Options`,
then `GAMEPAD-PS1: FLIGHT-RECORDER`, plus correct native request 7
and original `Shell_MainMenu`. Also test an immediate intentional fresh
press after releasing Cross on the returned main menu. If original New Game
still starts involuntarily, immediately restore the old working setting;
do not change other input mappings.

**Only the original 8-Flip guard is user validated so far.** Do not
merge PR #74 or publish a release before the new experiment is verified.

legluondunet assisted by ChatGPT (OpenAI)
