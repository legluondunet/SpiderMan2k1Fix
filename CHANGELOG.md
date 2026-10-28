## Unreleased — experimental SDL3-selected DirectInput owner (DRAFT PR #80, NOT hardware-validated)
- **Simplified controller menu/media setup (INI v2 unchanged):** ten UI rows (eight 3D actions + Confirm / Skip + Back / Cancel). An explicit single Confirm / Skip capture atomically synchronizes the four v2 keys MenuConfirm, SkipVideo, NextStill and SkipCutscene, preserving native-context ownership and original game's timing gates; Back remains separate. Changing a conflicting Confirm/Back button swaps the assignments, keeping unified bindings unified. Existing v2 profiles with different context buttons are shown as MIXED and preserved without silent edits until explicit capture; existing v1 and v2 profiles remain supported, without a v3 migration. New controller defaults use South/A/Cross across all four Confirm / Skip roles. **Live menu/movie/cutscene input routing is not yet implemented**; this change is to UI/configuration only. Wine hardware validation pending.

- **SDL3 controller settings v2 (draft):** five new configurable menu/media bindings in [MenuBindings] (MenuConfirm, MenuBack, SkipVideo, NextStill, SkipCutscene) are now editable in the existing mouse-free wizard, separately from the original eight 3D actions. Sharing a button between different contexts is supported; menu Confirm/Back swap if conflicting. Legacy Version=1 profiles load losslessly, automatically gain five defaults and upgrade to Version=2 only on the next explicit save, with original exact bytes backed up as .v1.bak. Contextual runtime input interception is not installed or hardware-validated; native video timing, game UI and cinematic skip gates remain unchanged.

- **SDL3 custom 3D controls: phase-1 read-only gameplay bridge preflight** (`[Debug] SDL3GameplayPlan=1` hidden OFF by default): evaluate already persisted per-model semantic actions, button and LT/RT trigger edges on the selected SDL3 device, gated by the actual original game-owned DirectInput device identity and foreground unpaused 3D. Require a new neutral sample after focus loss, hot-swap, media, or pause. Up to 64 bounded diagnostic records `SDL3-3D-PLAN` with exact action masks; no native table/COM writes, extra DI polling, key injection or duplicate Swing activation. This is **not yet live gameplay remapping**, because the native game consumes physical DirectInput button numbers rather than SDL3 triggers. Read-only physical acceptance on Wine/Windows required before designing a single-owner native input-reader adapter.


- **SDL3 per-controller persistent profiles (draft, Windows/Wine physical acceptance pending):** add strict portable versioned C99 INI serializer/parser and bounded Win32 profile storage, auto-load on SDL3 model discovery, atomic temp/flush/replace, corruption/VID:PID/duplicate-binding rejection and preservation of malformed files as `.invalid.bak` before explicit overwrite. Profiles stored as `SpiderMan2k1Fix/controllers/%04X-%04X.ini` beside the game executable when writable, otherwise `%LOCALAPPDATA%\\SpiderMan2k1Fix\\controllers\\` with local overrides taking precedence. The setup saves every accepted remap and per-model reset before adopting it in memory; on save failure the previous profile remains active and the UI reports `SAVE FAILED`. Both mouse-free controller/keyboard navigation and native game DirectInput remain unchanged. **Persistent semantic mappings do not yet control 3D gameplay**; original game Joystick table stays locked. More Wine/Windows validation required before merging.

- **SDL3 setup Wine crash mitigation (NOT root-cause confirmed):** the second Wine log (`SpiderMan2k1Fix(20261028-224257).log`, `v1.1.0-097d29f`) shows three successful in-memory captures followed by `in-memory model restored to defaults`, then abruptly ends **without** `preview closed` or an exception address. The optional wizard previously called synchronous `SendMessage(WM_CLOSE)` from the SDL3 polling `WM_TIMER` callback when controller B/Circle was pressed. This could destroy the modal HWND/controls reentrantly on the active timer stack. Queue closure with `PostMessage` **after physical release** (bounded 2s, keyboard Esc always possible), and also handle detachment while close is pending. Use deferred keyboard and Close-button requests, disarm timer and input in `WM_CLOSE`, clear user data in `WM_DESTROY`, and add bounded lifecycle diagnostics around Reset → Close → modal exit. No controller profile file persistence or native 3D action changes. Manual Windows/Wine repro still required; collect a crash dump if it persists.


- **SDL3 configuration preview: mouse-free navigation after first Wine test:** the 2026-10-28 hardware screenshot/log confirmed opening from the original Joystick menu, detecting the Xbox360, capturing a semantic binding, restoring defaults, and returning to the game, but the Win32 preview needed mouse interaction while the original game uses `MouseEnabled=0`. Add keyboard-first focus + Up/Down action, Enter capture, PageUp/PageDown controller, F5 reset, Escape cancel/close, F1 help, retained Tab/Space/button activation; plus neutral-gated SDL3 gamepad D-pad selection, South capture, East close, LB/RB controller switch and North restore. When capture is active the pad is NOT used for navigation; controller switch/focus loss/release resets ownership edges. Replace non-ASCII labels in Win32 ANSI dialog to avoid Wine mojibake. This remains opt-in, temporary **in-memory preview** with NO saved profiles or native 3D gameplay remapping. New mouse-free behavior requires physical validation.


- **First keyboard-accessible SDL3 controller setup preview in the original game menu (opt-in only):** `[Input] ExperimentalSDL3ControllerSetup=1` together with `ExperimentalSDL3Probe=1` installs a SHA-verified `SDL3 TEST (PREVIEW)` row in the native **Joystick Configuration** screen, replacing only the originally intercepted Restore Default row and preserving paired menu columns. Native joystick capture/reassignment stays locked; Keyboard Configuration stays editable and has its normal Restore Default. A modal owned Win32 dialog allows keyboard users (including those with no recognized Sony/Xbox pad) to select any mapped SDL3 gamepad, view eight actions, capture a neutral-first physical button or LT/RT, atomically swap conflicting bindings and restore per-model defaults. The original game thread is paused in its native menu call; the already initialized SDL3 subsystem is pumped on that same thread. **This is a temporary in-memory visualization and capture prototype, not yet persistent and not applied to the game's DirectInput actions.** Default OFF; no game files or keys injected; no native mapping changes. Windows/Wine physical validation required before an actual user-facing release.


- **Foundation for future user-configurable SDL3 controllers (NOT an unlocked in-game menu yet):** add pure C99 semantic profiles for eight actions, independent per-model VID:PID bank, atomic binding swap, reserved Start/Guide, distinct RT/LT with hysteresis, optional Pause, fail-closed validation, and portable compile-time/runtime regression tests. New opt-in `[Debug] SDL3BindingTrace=1` (default OFF, requires SDL3 probe) logs only the selected mapped controller's read-only semantic action edges after neutral; never injects keys, modifies original DirectInput gameplay, writes profiles, or lifts the native joystick menu lock. Document the proposed accessible controller-setup wizard, unmapped-joystick recovery path, future per-model persistence and single-owner SDL3 gameplay bridge in `docs/references/SDL3_CONTROLLER_SETUP_DESIGN.md`. Hardware behavior unvalidated and PR stays DRAFT.

- **Xbox RT Swing regression (user hardware report):** after successful DS4/Xbox hot switching and unplug/replug, the Xbox Swing trigger no longer activates. Add a strictly selected, verified-native-Xbox **SDL3 normalized RT** fallback alongside the previously tested DirectInput shared `lZ` negative-trigger path, keeping old single-controller behavior and opt-in `ControllerXbox360RTSwing=1` unchanged. No remapping of native action table, extra DirectInput reads, device ownership changes or nonselected-pad inputs. Add source/blocked diagnostics and host-compiled hysteresis tests; awaiting hardware validation.

- **First real dual-pad hardware log** (`SpiderMan2k1Fix(20261028-214736).log`): both controllers connected, three successful original game-owned DirectInput handoffs DS4 → Xbox360 → DS4 and 2D/Bink synthetic controls; still **no in-game 3D action validation**. Intro Alt+Tab and keyboard resume observed. INI auto-repair found three errors (original saved as `.ini.old`).
- Add **optional read-only 3D physical input trace** via `[Debug] SDL3GameplayTrace=1` (default OFF, requires explicitly requested SDL3 native owner): first 16 physical button edge mask, raw lX/lY/lZ and POV, plus selected instance and profile, max 64 entries, no native DirectInput polling, mappings or input injection. Enables correlation of actual 3D visual effects with hardware state after handoffs.


- New hidden `[Input] ExperimentalSDL3NativeOwner=1` is **OFF by default**, additionally requires SDL3 probe and all three existing native unplug/reconnect/runtime-mapping opt-ins. If dependencies are missing, SDL3 remains passive and the existing single-controller behaviour is preserved.
- SDL3 selected gamepad identity is bound to **exactly one** known matching native DirectInput instance, reusing the original game's factory and verified joystick pointer. Duplicate VID/PID, unknown products, active FFB effects, non-neutral buttons or unsafe native state fail closed.
- Disarm the old game-owned controller during Start selection handoffs; publish the newly acquired, neutral controller only after verification. The separate menu/Bink DirectInput UI adapter follows the same selected controller, releasing old synthetic keys rather than combining two controllers' inputs.
- For this explicit experiment only, permit canonical DS4/Xbox action-map switching from the untouched original retail seven-action button preset. Keep custom joystick mappings, original executable, keyboard, mouse and saved INI unchanged.
- Added CI source-contract regression tests and `docs/references/SDL3_NATIVE_OWNER_EXPERIMENT.md` with Windows 11/Wine smoke tests, rollback and limitations. **No claim of validated simultaneous physical gamepad gameplay, no merge or release.**

## Unreleased — optional SDL3 read-only gamepad detection (draft stacked PR)

- Hardware-confirmed limitation: SDL3 selection by Start does not yet provide game input with two pads connected at startup. The existing DirectInput UI bridge intentionally refuses ambiguous devices; do not claim single-logical-pad gameplay before a real input adapter exists.
- Add passive, validated native joystick pointer telemetry for multi-pad sessions; no game memory or COM device mutations.

- Fix SDL3 hot-unplug lifecycle: compact disconnected slots, close their SDL handles exactly once, and prevent repetitive detach messages/NULL handle close calls (tested with a portable 100-rescan C99 regression).
- Add an **OFF-by-default** `ExperimentalSDL3Probe=1` diagnostic. Dynamically loads an optional executable-relative Windows x86 SDL3.dll; gameplay remains exclusively on the validated native DirectInput backend. No SDL input is injected and no SDL/audio/video replacement occurs.
- Enumerate mapped gamepads separately from all joysticks, track SDL3 instance IDs, and log attach/detach, normalized LT/RT and a neutral-Start one-player selection experiment; preserve the existing owner when more pads arrive.
- Portable C99 multi-gamepad selection tests and CI build; no claims of production-ready SDL3 gameplay, input deduplication, or hardware validation yet. This experimental PR is stacked on #77 and must remain draft/unmerged.

# Changelog

## Unreleased — PAUSED cursor visibility (PR #74)

- Fixed the mouse cursor appearing over the custom PAUSED screen after Alt+Tab, while keeping normal desktop mouse behavior and `MouseEnabled=1` unchanged. Pending in-game retest.

## Unreleased — context-aware PAUSED instruction (PR #74)

- The custom Alt+Tab PAUSED screen shows `PRESS ANY KEY TO RESUME` if **no verified Sony DS4 wake device** was acquired at startup (including keyboard-only play).
- When a Sony DS4 is already detected/acquired and its temporary wake handler is armed, show `PRESS KEY OR GAMEPAD BUTTON TO RESUME`. Snapshot once at custom pause entry; do not query DirectInput from the drawing code or let the prompt change mid-pause.
- Use the same verified-device availability for the displayed instruction **and** the temporary pause gamepad poll; keyboard resume always works. This is not USB/Bluetooth hotplugging: detaching a pad during the frozen pause is still a separate future task, and does not change the already-drawn frame.

## Unreleased — DS4 can dismiss the custom Alt+Tab PAUSED screen (PR #74)

- Fix controller wake for the **custom focus-loss PAUSED DirectDraw screen**, not the game's original Escape/3D pause. The gamepad UI bridge normally only polls during Flip, which is deliberately blocked while this pause waits for keyboard input.
- Poll the **already discovered Sony DS4 secondary nonexclusive DirectInput device** inside the 20 ms message loop, only after Spider-Man regains foreground. Accept a fresh press of any of DS4's first 12 digital buttons followed by complete release; ignore axes/analog drift and buttons held on focus return. Reset incomplete events on foreground loss or read failure, and retry independent device acquisition without modifying the game's native DirectInput state.
- Resume on controller button release with **no injected Return/Escape**, preventing unintended original menu confirmation or gameplay actions on resume. Preserve the existing any-key press/release keyboard hook and native quit/WM_QUIT/audio cleanup paths; update the paused UI hint to `PRESS KEY OR GAMEPAD BUTTON TO RESUME`.
- Add pure portable C99 wake-policy regression tests and source-level integration checks. **Hardware validation under Wine/Windows required**; this is not USB hotplugging and does not activate unknown controllers. No merge or release authorized.

## Unreleased — game-only mouse disabled by default (PR #74)

- Original PC 2001 manual, game README and period developer interview confirm the mouse is **only used for menus**. Gameplay and all menu commands work with keyboard/joystick; do not conflate with the 2002 Movie game.
- Add `[Input] MouseEnabled=0` to the generated configuration and default to off for pre-existing INIs with no key. Set `MouseEnabled=1` to restore original mouse navigation in menus. This policy applies with **keyboard alone**, any gamepad, or `ControllerProfile=Off`; it no longer depends on DS4 availability or `ControllerPS1Preview`.
- Reuse the previously validated, original game-owned `gRenderTest & 0x10` gate with strict three-site native instruction verification, one atomic RAM flag that stays disabled across focus loss. Never touch the Windows/Linux desktop cursor, joystick mapping, or keyboard input. The game mouse remains unchanged if native validation fails.
- Retire but continue accepting the old controller-specific `ControllerAutoDisableMouse` INI key. It no longer changes behavior; `MouseEnabled=1` takes precedence and always restores the original menu mouse. The user's existing previous test with a DS4 remains evidence for the native gate; keyboard-only and native Windows require runtime validation.

## Unreleased — controller cleanup and accepted Cross-only PS1 navigation (PR #74)

- Maintainer **visually validated** that OPTIONS/BUTTON9 alone does nothing in the native 2D title menu under Linux/X11/Wine with a Sony `054C:05C4` DS4. The separate 10:33 game log confirms Cross-only PS1 preview activation, SHARE/native pause, original Quit request 7, native 3D cleanup and successful interception of one inherited Cross edge (85 ms gap) on return to 2D. The physical source of the delayed Cross signal remains unproven.
- Preserve **Cross as the only synthetic Enter button** in PS1 preview (even when legacy `ControllerEnterButtons=Both` is configured). Preserve all legacy settings and menu input behavior outside preview. Keep OPTIONS-only native 3D script skip disabled by default.
- Refactor the menu-confirm policy into pure C99 helpers with host-compiled button/edge/owner regression cases; update legacy-versus-PS1 discovery and INI comments to avoid wrongly claiming that OPTIONS confirms in the PS1 profile. Clarify that the Joystick read-only protection is separately gated.
- For routine opt-in testing, leave diagnostic `ControllerPS1QuitTrace=0` and `ControllerPS1InputFlightRecorder=0`, and favor `ControllerPS1TransitionEdgeOwnership=1` over the old optional eight-Flip `ControllerPS1PostQuitGuard=0`. Retain both options in code as fallbacks until broader hardware/Windows verification; **no default activation, release or merge**.

## Unreleased — DS4 native confirmation ownership experiment (PR #74)

- Read-only `ControllerPS1InputFlightRecorder=1` trace `SpiderMan2k1Fix(20261028-084402).log` isolates the apparent duplicate Cross action: last successful 3D poll `seq=586` reports `Cross=0 previous-mask=0`; first returned 2D poll `seq=587` reports `Cross=1 previous-mask=0 request=7`. The game has already processed the native Quit/Yes. No focus loss, missing device or DirectInput failure occurs between these two polls. This is a **new observed raw input edge on the first 2D frame**, not a proven spontaneous hardware signal or reset. The exact physical press moment is unobserved.
- Added **OFF-by-default**, alternative `[Input] ControllerPS1TransitionEdgeOwnership=1`: on the first 2D frame after a verified 3D session ended through native request 7, treat any currently depressed Cross/Options confirmation as already owned by the previous native menu. Do not forward a second synthetic Return to the new main menu. Suppression persists **only while the inherited button remains held** and immediately re-arms after the physical release. There is **no fixed number of frames** and no per-frame timer.
- Preserve the existing *runtime-validated* `ControllerPS1PostQuitGuard=1` option unchanged, as a fall-back. To independently A/B the new ownership model, test `ControllerPS1PostQuitGuard=0`, `ControllerPS1TransitionEdgeOwnership=1`, `ControllerPS1QuitTrace=0`, `ControllerPS1InputFlightRecorder=1` with the same original Sony DS4; then confirm normal menu navigation, including fast Cross release/re-press after Quit. Neither alternative is enabled by default.
- The ownership model is **NOT runtime-validated** yet. The original game's correct native Quit request is left untouched; this fix affects only the separate PS1 keyboard-synthesis adapter. PR #74 stays DRAFT, unmerged and unreleased until maintainer test and explicit approval.

## Unreleased — opt-in native Quit → main-menu transition guard (PR #74)

- Real-game A/B on 2026-10-28: with `ControllerPS1QuitTrace=0` the Sony DS4 can inject synthetic Return on **the first returned 2D Flip** following native engine request 7, immediately opening New Game. In `SpiderMan2k1Fix(20261028-081250).log`, this occurs on flips 734 and 853; original native teardown itself is correct. The read-only `QuitTrace=1` had masked this timing-sensitive problem in an earlier test.
- New **OFF-by-default** `[Input] ControllerPS1PostQuitGuard=1` experimental PS1-preview-only protection: suppress synthetic Cross/Options-to-Return for the first **eight 2D Flips** after the confirmed original 3D → menu `request=7` transition. Existing physical button state is still consumed; the original menu, native controller input, quit operation and keyboard inputs remain untouched.
- **Runtime validated by the maintainer (2026-10-28):** with `ControllerPS1QuitTrace=0` and `ControllerPS1PostQuitGuard=1`, the original Sony DS4 follows **SHARE → Quit → Yes → main menu** correctly. In `SpiderMan2k1Fix(20261028-082416).log` (`v1.1.0-89f2ef7`), native `request=7` and teardown succeed twice; the guard suppresses exactly one premature synthetic Return on each first returned 2D Flip (`296`, `1046`), and the original `Shell_MainMenu` is reached both times. Later synthetic Return/Back actions still work after settling. Verified for this DS4/game setup only; keep OFF by default and keep PR #74 draft pending broader validation and explicit merge approval.


## Unreleased — Quit/Yes native-selection investigation (PR #74, diagnostic only)

- The maintainer **confirmed** that `ControllerAutoDisableMouse=1` eliminates the original mouse cursor's stale hover overriding D-pad/DS4 Cross menu selection. This is accepted for their tested Sony DS4 setup only.
- New independent bug report: in the original 3D pause menu, **SHARE → Quit → Yes** unexpectedly launches a new game. The user suspects it might occur even with mouse enabled. **No cause or fix has been validated.**
- Add **OFF-by-default** `[Input] ControllerPS1QuitTrace=1`: bounded, passive, VirtualQuery-guarded monitoring of original game pause/confirmation pointers, selected item indices, engine transition request, native mouse-disable status and separate Cross/Circle/SHARE press edges. No game-control or native menu modifications. Use to distinguish an incorrect Yes result from a successful menu return followed by unintended New Game selection; compare the same Quit/Yes action using gamepad Cross and keyboard Enter with mouse on/off.

## Unreleased — game-native mouse suppression with DualShock 4 (PR #74, experimental)

- Add **opt-in** `[Input] ControllerAutoDisableMouse=1`, active only when the supported Sony DualShock 4 PS1 profile has been authenticated and its independent gamepad read succeeds. Default is Off; other input devices and `ControllerProfile=Off` are unchanged.
- Use the **original game's own** `gRenderTest & 0x10` no-mouse gate (not Win32 `ShowCursor`): original `PCSHELL_DrawMouseCursor`, `PCSHELL_UpdateMouse` and `PCSHELL_IsMouseOver` all use this same flag. This removes the in-game mouse sprite, stale pointer hover/selection and original mouse polling, so a controller Enter confirms the highlighted selection rather than an unrelated mouse-hover selection.
- Strictly verify the three original 32-bit x86 mouse-gate instruction signatures, game PE layout and writable original flag before changing a **single game-owned RAM bit**. Preserve other `gRenderTest` flags atomically; release only the bit owned by the fix on Alt+Tab, invisible window, missing controller or input read failure. The system/Windows mouse, existing focus recovery and physical keyboard are untouched.
- **Not yet tested in-game.** The existing original joystick menu read-only behavior was confirmed by the maintainer with a DualShock 4; this separate mouse-control experiment needs direct game testing. No automatic enablement, release or merge without explicit validation.

## Unreleased — native read-only DualShock 4 Joystick Configuration prototype (PR #74)

- Introduce **disabled-by-default** `[Input] ControllerPS1ReadOnlyJoystick=1`, available only after an unambiguous Sony DualShock 4 is acquired and the PS1 gameplay overlay is successfully read by the original config loader.
- Keep the original Joystick Configuration two-column menu, but format its seven actual fixed native action button numbers as **CROSS, CIRCLE, SQUARE, TRIANGLE, L1, R1, R2**. Preserve original 2D rendering, cursor, scrolling, exit and controller gameplay.
- Based on original PCShell native code addresses, apply six transactional, original-bytes-verified, RAM-only joystick-menu-specific patches: intercept native button-label formatting, prevent entering the key/button reassignment prompt **only when the original menu mode indicates Joystick** (Keyboard Configuration remains editable), block the original two reassignment acceptance paths, neutralize native controller restore-default and setter call sites.
- Move all code-page changes into a dedicated `ps1_joystick_readonly_menu.c` module; the independent DirectInput gamepad UI bridge remains code-patch-free.
- **NOT YET VALIDATED IN GAME.** This feature will not be enabled automatically or released until physical controller, mouse and keyboard interactions verify a truly read-only Joystick menu and functioning editable Keyboard menu. Original personal `[Joystick]`/`RawHex` preferences remain preserved.

## Unreleased — native Start opt-in research (PR #74, UNVALIDATED)

- After a successful complete, read-only .data scan on the maintainer's original game, identify exactly one 14-action Start candidate at RVA `0x00168408` (BUTTON11), with no truncated memory.
- Add a disabled-by-default experiment `[Input] ControllerPS1DisableNativeStart=1` **requiring** `ControllerPS1Preview=1` and `ControllerPS1DeepStartProbe=1`. It refuses all writes unless the scanned native table matches the complete, strict original factory action/button layout and exactly one candidate exists. When permitted, atomically unbind only the single 32-bit Start button index in temporary process RAM by replacing BUTTON11 with the game's existing `0x4000` unused sentinel. Restarting the game restores original RAM; saved user joystick preferences are untouched.
- This is **unvalidated in-game**, not enabled automatically, not a proof of original runtime Start handler ownership, and **does not** yet disable the game-owned Joystick Configuration menu. Test with SHARE and the original camera/R3 button before claiming the new Start binding works.

## Unreleased — contextual DS4 preview follow-up (PR #74)

- Read the exact, SHA-gated native pause-menu **purple ribbon render signature** from the preceding Flip as a supplemental 3D pause indicator. The original pause pointer can be zero while the menu is displayed; this allows the experimental controller profile to distinguish the pause UI from gameplay when the exact measured widescreen renderer is active.
- Add manually opted-in `[Input] ControllerPS1OptionsSkip=1` for native scripted-3D cinematic skip on OPTIONS; write the original one-byte skip request only when the original CutSceneScript object is readable, active and explicitly allows skipping. Fail closed otherwise. This mode is **Off by default**, unvalidated and limited to the PS1 preview.
- Preserve original gamepad mappings on native config bootstrap failure via automatic rollback; reassert fixed seven-button virtual mappings if the game writes its virtual config, without persisting a customized preview mapping into the user's saved INI.
- Add one-shot **read-only** diagnostics for the native 14-action Start table. The historical native Start mapping hook never scanned the tables because of an overly restrictive executable SizeOfImage check; do not modify or disable the native BUTTON11 mapping until a real game log verifies the table shape.
- The in-game Joystick Configuration UI is **still not locked** and the previous native Start button is still active. Real Wine/Windows tests are required before these features can be claimed complete or enabled by default.

## Unreleased — PS1-inspired DualShock 4 profile preview (PR #74, opt-in only)

- Add a **disabled-by-default** `[Input] ControllerPS1Preview=1` test switch. The default `ControllerProfile=Auto` continues to use the existing experimental UI bridge unless manually opted in.
- Recognize exactly one original Sony DS4 using an early, foreground-only DirectInput8 acquisition before the verified game-native `Spidey.cfg` bootstrap. If detection or bootstrap fails, do not activate the fixed preview.
- Temporarily overlay seven virtual *in-memory* gamepad bindings: Cross=Jump, Circle=Kick, Square=Punch, Triangle=Web, L1=Aim, R1=Zipline and R2=Swing. The original user's gameplay bindings are snapshotted before the override.
- Preserve the original seven button bindings both in `[Joystick]` and the 252-byte `RawHex` whenever the game saves settings: the PS1 preview never persists or destroys existing user customizations.
- Route SHARE to the native Escape/pause command during gameplay and the original 3D pause menu. Cross=Enter and Circle=Escape in menus including the native pause menu. Stop the earlier 3D OPTIONS-pause synthesis in this preview.
- **Incomplete, unvalidated full-profile features**: the original in-game Joystick Configuration menu is not yet locked, original Start/BUTTON11 remains usable, native 3D scripted scene skipping via Cross/OPTIONS is not yet implemented, and the physical mappings require real gamepad testing on Wine and Windows. Do **not** enable this profile by default or release it as a finished feature.
- Add C99 host-compiled reversible-mapping test and structural regression tests; no merge until maintainer's validation.


## Unreleased — native save-dialog title spacing (awaiting runtime validation)

- Investigating widely separated characters in the original `SAVE GAME PROGRESS ?` confirmation heading under widescreen Auto.
- Expand the native ESC 4:3 font-glyph eligibility only in the heading row, preserving the original central bounds for NO/YES and other menu choices. Existing owner/texture/depth/same-frame guards and classic 4:3 behavior remain unchanged.
- Add host-compiled C geometry tests covering 16:9, 16:10 and unrelated HUD/edge cases. In-game Windows and Wine visual validation is still required; this change is **not yet released**.

## Unreleased — automatic DualShock 4 UI profile (experimental)

- Retire the unsuccessful `ExperimentalGamepadStart=BUTTONn` native-controller-table rewrite. The user's second log (`v1.1.0-10aff89`) confirmed it refused an unsupported PE header layout and **never activated**.
- Automatically recognize original Sony DualShock 4 VID/PID 054C:05C4 / 054C:09CC and the official 054C:0BA0 adapter, using a separate nonexclusive DirectInput8 gamepad device. Other controllers are left unchanged.
- Add `[Input] ControllerProfile=Auto|Off|DualShock4` and `ControllerEnterButtons=Cross|Options|Both` (default Auto and Both). When a unique DualShock 4 is detected, map ✕ and/or OPTIONS to **Enter** for interactive 2D menus, the pre-menu still image and skippable videos; keep all native gameplay controls unchanged.
- Trigger one debounced Enter scancode per button press using `SendInput`; release the key before focus-pause, never inject in 3D. Failure to read the separate controller or identify it unambiguously disables the bridge safely.
- Preserve the original game's cinematic timing and keyboard events. **DualShock 4 Enter is confirmed functional by the user under Wine**; native Windows validation remains pending.
- Add `[Input] ControllerBackButton=Circle|Share|Off` (default `Circle`) to send **Escape** during 2D menus and cancellation dialogs, without affecting 3D gameplay. Back takes priority over Enter on simultaneous presses, and its synthetic key is always released at the next Flip.
- Note the original Triangle button already confirms some submenus natively; do not remap Triangle to Escape until its original confirmation handler can be safely suppressed. Circle/Escape requires a new Wine/Windows acceptance test.
- Address a subsequent user report: a post-main-menu video no longer responds to pad skip, while Triangle natively skips 3D cinematics and a camera-related button accesses the in-game Escape menu.
- Extend Cross/Options Enter to native Bink playback even after the menu display mode exits, gated by the exact verified original GameFMV_Active flag instead of assuming all Bink movies run under the 2D menu viewport. Scripted 3D Triangle and native movie skip timers remain untouched.
- Correct the 3D stage check to use the game's display mode transition rather than the optional `gameplay_4x3_active` flag, which is disabled under Auto widescreen.
- Add guarded `[Input] ControllerPauseButton=Options|Off` (default Options) for the 3D Escape/pause key. The bridge **refuses** synthetic Escape if **any** native `[Joystick]` action still uses BUTTON9 (factory Zipline), logs `GAMEPAD-UI: PAUSE BLOCKED`, and never changes a player's gameplay bindings automatically. For a trial, the user may remap Zipline to a free gamepad button such as R1/BUTTON5 in the game's Joystick settings.
- Add new regression checks and a Wine/Windows test matrix. **Post-menu skip and OPTIONS pause are still unverified at runtime.**
- Add regression tests and research notes for profile choice, context isolation and the two previously failed controller experiments. The stable v1.1.0 release remains unchanged.

## 1.1.0 — October 27, 2026

- Identified the root cause of audio crackling in the original game: its native WAV loader reads only complete 2,048-byte PKR resource sectors and omits the final partial sector.
- Added a **signature-guarded native loader correction** that restores only the missing RIFF/WAV remainder after matching the entire loaded prefix and verifying the original sources independently by CRC32.
- Validates each additional read through the original native PKR reader in a private 2,048-byte scratch buffer before writing only the exact missing remainder to the game's preallocated WAV buffer.
- Safely handles **duplicate original WAV sources** only if every matching source has identical remaining bytes. Leaves metadata-only remainders, ambiguous or unverified assets unchanged.
- Retained the game's original DirectSound playback engine and the separate opt-in two-sample `Heli1.wav` declick; removed the obsolete late DirectSound tail repair and its `ExperimentalAudioPKRTailRestore` INI option.
- Kept bounded read-only source/PCM diagnostics and optional DirectSound Play tracing; no SDL3 backend or external audio assets are required.
- **Linux/Wine validation (27 October 2026):** confirmed native repair of the previously ambiguous two 460-byte and one 1,988-byte PCM tails, unchanged full-PCM comparison results, and user-confirmed crackle-free playback **without** the fallback.
- **Enabled the native WAV loader correction by default** for the supported executable, without adding a setting to the generated INI. An existing manually configured `ExperimentalAudioLoaderTail=0` remains a hidden troubleshooting override; `=1` is no longer necessary.
- Preserved the original SHA-256-bound WAV/DirectSound function addresses, disassembly, failure mechanism and rejected hypotheses in the permanent technical research documentation (PR #71).
- **Verification:** Linux/Wine gameplay and listening confirmed no audible crackling with the late DirectSound fallback removed; Windows x86 compilation and static regression tests passed. Native Windows gameplay verification of the final default-on build remains pending.
- **Release:** minor version 1.1.0 introduces the built-in audio repair; only the supported original `SpideyPC.exe` is compatible.

## 1.0.0 — October 25, 2026

**First stable release of SpiderMan2k1Fix**, an unofficial compatibility patch for *Spider-Man (2001)* PC.

### Compatibility and Display

- Added an in-memory **No-CD patch**, without modifying the original executable.
- Replaced obsolete exclusive fullscreen modes with **borderless fullscreen** at the monitor's native resolution.
- Added automatic widescreen support with aspect-aware FOV adjustment and an optional centered **4:3 mode**.
- Preserved the original aspect ratios of menus, introductory videos, cinematics, and comic-book artwork.
- Corrected widescreen HUD elements, including health and web gauges, compass, objectives, boss health bars, and the in-game pause menu.
- Added multi-monitor support and Windows DPI scaling compatibility.
- Restored the original game icon in the Windows taskbar.

### Gameplay and Input

- Added a configurable **FPS limiter**, set to **30 FPS by default**, with support for 20 FPS in timing-sensitive sections.
- Fixed crashes and permanent keyboard/mouse input loss associated with Alt+Tab.
- Added automatic pausing when the game loses focus, with synchronized video and audio playback and keyboard-controlled resumption.
- Added automatic keyboard-layout detection, including support for French AZERTY controls, menu labels, and cheat-code entry.

### Configuration

- Replaced the original `Spidey.cfg` with a unified **`SpiderMan2k1Fix.ini`** configuration file.
- Preserved keyboard, gamepad, audio, and brightness settings.
- Added automatic validation and recovery of invalid or obsolete settings, with a single rolling `.ini.old` backup.
- Added configurable aspect ratio, frame-rate limit, keyboard-layout detection, and diagnostic logging.

### Fixes and Improvements

- Fixed black screens caused by unsupported legacy display modes.
- Fixed a crash that could occur when exiting the game.
- Fixed desktop mouse cursor visibility issues after switching applications.
- Added **F9 lossless PNG screenshots**.
- Added the patch version and source commit identifier to the game's main menu and diagnostic logs.
- Improved startup reliability, display initialization, and configuration recovery.
- Removed obsolete experimental code and development-only features from the stable build.

### Known Limitations

- Some bosses and levels still require a **20 FPS limit** to function correctly.
- Audio glitches inherited from the original game remain unresolved.
- Under Wine, automatic pausing after focus loss may occasionally be delayed.
- Only the supported original `SpideyPC.exe` executable is compatible with this release.

See the [README](README.md) for installation instructions, configuration, known issues, and possible future improvements.

### Release

[SpiderMan2k1Fix v1.0.0 — GitHub Releases](https://github.com/legluondunet/SpiderMan2k1Fix/releases/tag/v1.0.0)
