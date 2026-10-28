# Changelog

## Unreleased — opt-in native Quit → main-menu transition guard (PR #74)

- Real-game A/B on 2026-10-28: with `ControllerPS1QuitTrace=0` the Sony DS4 can inject synthetic Return on **the first returned 2D Flip** following native engine request 7, immediately opening New Game. In `SpiderMan2k1Fix(20261028-081250).log`, this occurs on flips 734 and 853; original native teardown itself is correct. The read-only `QuitTrace=1` had masked this timing-sensitive problem in an earlier test.
- New **OFF-by-default** `[Input] ControllerPS1PostQuitGuard=1` experimental PS1-preview-only protection: suppress synthetic Cross/Options-to-Return for the first **eight 2D Flips** after the confirmed original 3D → menu `request=7` transition. Existing physical button state is still consumed; the original menu, native controller input, quit operation and keyboard inputs remain untouched.
- This experimental fix is **not runtime-validated yet**. Test with `ControllerPS1QuitTrace=0`, change no other setting. Do not merge PR #74 or publish a release until maintainer approves the in-game A/B.


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
