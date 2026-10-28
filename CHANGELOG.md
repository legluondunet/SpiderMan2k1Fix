# Changelog

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

- Fixed crackling and distorted sound effects caused by a bug in the original game's audio loading.
- The audio fix is enabled automatically — no configuration or extra files required.

[Technical details and testing notes](docs/references/diagnostics/audio/PKR_NATIVE_AUDIO_REPAIR.md)

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
