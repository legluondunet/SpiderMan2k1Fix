# Changelog

## Unreleased — frozen DirectDraw pause frame (PR #18 experiment)

- Replace the former Wine-fragile desktop alpha popups, normal child windows and dither-grid simulation with **one composited DirectDraw7 game frame**. At the confirmed focus-loss Flip boundary, save the game's backbuffer through its native surface DC, apply the configured **55% true alpha darkening, 16px square grid, and centered English comic PAUSE panel**, and present the result once using the existing validated windowed-primary `Blt`.
- The resulting image is part of the game window's OWN pixel surface, not an HWND overlay. Its normal desktop Z-order follows Spider-Man when Thunar/Firefox or another app is active. Nothing separately floats over the desktop; foreground/background styling is identical.
- Preserve the original backbuffer in an independent compatible bitmap, and **restore it before resuming** gameplay and Bink/DirectSound. Fail safely if surface `GetDC`, snapshot, GDI alpha blend or primary `Blt` is unavailable, without affecting the real pause or changing game surfaces permanently.
- Keep the existing INI pause options, Any-key-only resume and Alt+Tab passthrough. Retire popup show/hide switching and stop compiling `pause_overlay.c` and `pause_dim.c`; link the surface renderer against `msimg32` for `AlphaBlend`.
- **Experimental**: requires direct Wine/DXVK and Windows 11 runtime testing of surface `GetDC` support, scene restoration, menu/cutscene/3D rendering and media synchronization. The earlier unmerged #14–#17 visual experiments are superseded.

## Unreleased — full background game-bound grid + shade (PR #17 revision)

- Follow-up to the game-bound PAUSE banner: draw the **16px square grid over the entire paused Spider-Man window even when it is not foreground**, retaining normal Xfce/Thunar stacking.
- Add a nonlayered `WS_CHILD|WS_CLIPSIBLINGS` backdrop under the game's child banner. Its Win32 region contains the grid strokes plus a **distributed opaque-pixel dither** approximating the requested `PauseDimOpacity` without unsupported legacy layered-child alpha. At the default 55%, the background approximation covers **4/8 rows (50% area)**; visual texture differs deliberately from the true foreground alpha blend.
- Preserve the **original exact 55% alpha blend and 16px grid in the foreground**, with no changes to audio/video/input and no new INI options.
- Failure to create a background grid logs a warning and leaves the pause and message functional. The background grid follows the game as a child, **never floating above other desktop applications**.
- Needs native Wine / Windows 11 screenshots, especially Thunar partially obscuring the gridded game, before merging.

## Unreleased — background-visible PAUSE message (PR #17)

- Keep the PAUSE instruction **visible inside Spider-Man even after Alt+Tab**, without drawing anything above Thunar/Firefox or another desktop window. While focus is lost, use a real **nonlayered, game-parented `WS_CHILD`** with the same small muted-plum rounded comic frame and fully white `PAUSE / PRESS ANY KEY TO RESUME` lettering.
- On returning to the game, replace the background GDI child with the **approved per-pixel-alpha foreground banner** and show the configured 55% dark/16px grid shade again. On further Alt+Tab the shade and floating banner disappear and the game-bound child returns. The background banner uses an **opaque** panel because layered child alpha is not reliable with the original unmanifested 2001 EXE.
- Parent clipping and child Z-order avoid independently floating windows that could cover unrelated desktop applications. No new INI settings or changes to Any-key press/release, Bink, DirectSound, DirectInput or the game's EXE.
- Wine/Linux and Windows 11 testing remains required for foreground/background transition, appearance and audio synchronization. This PR supersedes earlier unmerged visual drafts #14–#16.

## Unreleased — one-piece atomic comic PAUSE banner (PR #16)

- After native Wine/Xfce screenshot feedback, replace the fragile shaped PAUSE child and additional center-alpha popup with **one per-pixel-alpha, game-owned nonactivating comic banner**. Compose the muted translucent purple/black panel, thin muted red 2px outline and **fully opaque white lettering** in one 32bpp premultiplied BGRA bitmap via `UpdateLayeredWindow(ULW_ALPHA)`. A failed ULW operation is safe and leaves no opaque fallback.
- Remove `GetWindowRgn` / glyph-path masks, and exclude the **entire rounded panel rectangle** from the separate, approved 55%-dark 16px-grid backdrop. No font holes, stale child pixels or scattered black remnants should remain when the game loses focus.
- Use an active-window **AND** foreground-window gate; the popup is not shown while another application is active. Existing pause loop continues to hide both popups on background focus, resume and WM_QUIT.
- Keep the English `PAUSE / PRESS ANY KEY TO RESUME` text, Any-key-only wake and Alt+Tab passthrough. No DirectDraw or game EXE changes.
- Requires actual **Wine/Linux and Windows 11** screenshot and input/audio checks before merging. Supersedes #14 and #15, both remain unmerged.

## Unreleased — Wine focus-only pause UI and solid lettering (PR #15)

- Fix a Wine/Linux regression where the pause's owned alpha popups could remain visible on top of unrelated desktop windows (e.g. Thunar) while Spider-Man was not foreground. While focus pause is latched, **Bink/DirectSound and the game still pause immediately**, but no pause windows are shown until the *game* becomes foreground; they are destroyed again within the 20 ms pause-pump interval if foreground changes.
- Preserve the approved 55% black and 16 px grid outside the banner, but **clip the entire centered banner rectangle out of all grid-line painting** instead of relying on per-glyph window-region masking. Keep the additional 35%-alpha black rounded panel inside it (about 71% effective darkening) with a precise glyph/frame cutout.
- Strengthen lettering with an enlarged **25 px upright Arial Black** second line, a 3 px dark GDI outline, and expanded font-path cutouts, to prevent the grid or dimmer from visually eating white strokes under Wine's GDI/compositor.
- Keep **PAUSE / PRESS ANY KEY TO RESUME**, the exclusive ordinary-key press-and-release wake, original binary, DirectDraw/DirectInput handling and media resume ordering. Pass Alt/Windows desktop-switch modifiers through the keyboard hook rather than consuming Alt+Tab while the foreground pause banner is shown. The PR #14 visual build is superseded and must not be merged alone.
- **Windows 11 and Wine runtime testing is still required**: automated tests cannot prove window-manager Z-order and GDI layered-composition behavior.

## Unreleased — clearer English focus-pause indicator and Any-key-only wake (PR #14)

- Change the center message to **PAUSE / PRESS ANY KEY TO RESUME** in English, with a larger (23px) white bold-italic hint, 4px rounded red outline and slightly larger centered box. Preserve the shaped child HWND and its pixel-precise cutout through the surrounding full-screen grid overlay.
- Keep the validated global **55% darkening and 16px grid** intact; add a smaller, **optional** owned, nonactivating constant-alpha dark panel clipped inside the rounded PAUSE frame. A second exact red/white glyph cutout avoids shading the foreground text. The additional 35% tint yields roughly 71% combined black where panel and regular shade overlap; panel failures are logged but are nonfatal.
- Retire distinct F9 and Space pause/resume modes and the `PauseKey` INI choice. Always resume by a foreground Any-key press/release (consumed by the existing WH_KEYBOARD_LL hook); any retained older `PauseKey` entry is accepted by validation only to prevent unnecessary INI repair, but ignored at runtime.
- Install the Any-key hook **before** pausing Bink/DirectSound. If Windows refuses the hook, decline the pause immediately instead of freezing or falling back to F9; preserve cleanup of input/audio/video on release and window destruction.
- No changes to game `SpideyPC.exe`, DirectDraw surface hooks, 4:3 layout, source video, FPS limiter or game control mappings. Native Windows 11 pause/menu/3D presentation remains to be confirmed before merging.

## Unreleased — comic-style main-menu version label (PR #13)

- Restyle the small bottom-right `SpiderMan2k1Fix v<version>-<commit>` footer with system **Arial Black Bold Italic**, the game's own **inactive menu violet `RGB(138,120,214)`** (sampled from the `OPTIONS`, `TRAINING` and `GALLERY` lettering), and a fine dark violet 8-direction glyph outline. The initial lighter-lavender version was visually too bright; shrink text around **20%** (25 to 20 px at 1200p menu-content height) and soften the outline.
- Keep the real version/commit text, menu-only classification, fitted 4:3 content rectangle, scaling and both existing DirectDraw backbuffer and GDI window fallback paths. Preserve DC settings and GDI font lifetime; no changes to menu items, input, audio or the game executable.
- Native Windows 11 menu screenshots still required to approve aesthetics before merging; automated checks only verify structure and build.

## Unreleased — darker gridded focus-pause presentation (PR #12 update)

- Increase the original 40% smooth darkening to **55% black** by default, retaining the full-brightness red/white PAUSE banner in the shaped cutout.
- Overlay a subtle **16 px** square grid, painted by GDI directly in the existing alpha popup rather than in another floating window.
- Add validated `[Input] PauseDimOpacity=55` (range 0–90) and `[Input] PauseGrid=1` (0/1); keep `PauseDim=0` as the single visual rollback. Log the active opacity, alpha and grid state.
- All focus, keyboard/mouse, Bink and DirectSound pause/resume hooks remain unchanged. Windows 11 runtime rendering must be tested before merging PR #12.

## Unreleased — dimmed pause scene (PR #12)

- Start from a uniformly alpha-blended black tint across the *game client area* only during a confirmed focus pause; the latest revision uses 55%. The underlying scene remains visible; the previous white comic-style `PAUSE` message with red frame stays centered above it at full brightness.
- Native log exposed repeated `FOCUS-DIM: creation failed` from the layered child. Replace it with a **non-topmost owned layered popup**, supported for unmanifested 2001 executables, and exclude the validated shaped child banner's *exact* red/white pixels from the popup region. The popup uses constant alpha (`LWA_ALPHA`), not the previously unreliable color key, and is skipped if creation, region masking or alpha setup fails. Each failure now logs its Win32 code.
- Add `[Input] PauseDim=1` (default on for active focus pauses; `0` disables only dimming). Focus pause itself remains opt-in. Destroy dim child on explicit resume and `WM_QUIT`, before DirectSound/Bink resume; no changes to input hooks, audio, timing, rendering surfaces or the game EXE.
- Runtime visual validation is required on native Windows 11 and DirectDraw; CI alone does not prove transparent compositor behavior.

## Unreleased — Windows Ghost-window false-focus experiment (PR #11)

- Diagnose repeated spontaneous 5–6-second menu/cinematic/3D pauses: external foreground window `class=Ghost` with the same helper PID/thread across all occurrences, indicating Windows' unresponsive-window surrogate, not a verified user Alt+Tab.
- Add `[Input] DisableWindowsGhosting=1` **opt-in** (default 0): calls Microsoft's `DisableProcessWindowsGhosting` after supported EXE/INI validation, before first game window; the change lasts for the process lifetime and removes the Windows ghost-window fallback.
- Preserve real focus-loss pausing, Any-key wake, Bink and DirectSound synchronization, windowed rendering, and diagnostic foreground-owner logs. Keep Windows' normal ghost-window behavior by default pending tests.
- Add regression tests, diagnostics and rollback instructions; this is a test experiment, **not yet a verified release fix**.

## Unreleased — centered transparent focus pause indicator

- Display `PAUSE` in the center of the game window, inside a red outline with no filled background and comic-inspired bold italic white text with a dark outline, while `PauseOnFocusLoss` is latched. Use a shaped child-window region to exclude all empty pixels (avoiding the black interior observed with DirectDraw and layered-window color keying).
- Display the appropriate French wake hint based on `PauseKey`: any key, Space, or F9.
- Draw the indicator in a nonactivating child window (no topmost desktop window, no replacement of the game's DirectDraw/3D drawing methods).
- Automatically hide it before Bink and DirectSound playback resumes and if the game quits while paused.
- Add `[Input] PauseOverlay=1` (default on for visible focus pauses; configurable to `0` for visual rollback); no change to focus/input/media pause behavior.

## Unreleased — synchronized focus pause for Bink and DirectSound (PR #9)

- Extend the optional Alt+Tab focus pause with `[Input] PauseBinkOnFocusLoss=1`: invoke the game's own Bink pause/resume mechanism for cinematics and audio-only Bink music.
- Add `[Input] PauseDirectSoundOnFocusLoss=1`: stop only active voices in the original game's 32-slot secondary DirectSound table; resume the same buffers with their original looping state and preserved playback position.
- Keep the two new options disabled by default and preserve the existing `DirectInputReacquire`, `PauseOnFocusLoss`, `PauseKey=Any`, and cursor-recovery controls.
- Preserve exact-executable signature guards, avoid COM vtable interception, update configuration templates, tests, and rollback documentation.
- **Windows 11 validation:** tester reports successful menu/cinematic/3D pausing with no audio desynchronization. In the recorded session, 11 and 9 playing secondary voices were stopped and successfully resumed on separate 3D focus pauses.
- **Known observation:** intermittent audio crackling occurred under ProcDump but was not reproduced on a normal game launch without ProcDump. Cause not established; investigation deferred.
- Not yet merged or released; final integration requires explicit approval.

## 1.0.0 — first stable release (2026-10-19)

- Renamed the project, main DLL, INI, logs, source file, build artifacts, and documentation to **SpiderMan2k1Fix**. The `winmm.dll` proxy retains its required name. Existing beta INI files need to be renamed manually to preserve custom settings.

- Retained the validated borderless fullscreen, desktop resolution, Auto/4:3 presentation, intro-video and comic artwork fixes, automatic FOV, No-CD patch, 30 FPS limiter, Alt+Tab input recovery, and integrated game settings.
- Retained automatic INI creation and numbered `.old` backups with recovery of known valid values.
- Default logging is disabled (`[Debug] Log=0`) and diagnostic string formatting on key graphics hot paths is skipped when logging is off.
- Removed obsolete sprite-entry trampoline and 31-callsite profiling, geometry/draw histograms, drawing-phase shortcuts, comic and intro-video tracing, and forwarding-only Lock, BltFast, projection and vertex-buffer diagnostic hooks.
- Removed unverified native-edge experiments: raster bias, culling/Z A/B tests, pixel readbacks, synthetic/original triangle witnesses, diagnostic Clear and EndScene hooks.
- Removed the corresponding obsolete INI keys from the validation allowlist. Existing invalid/obsolete keys are handled by the existing INI auto-repair and backed up.
- Added release-specific regression checks for hook preservation and removal of obsolete diagnostic paths.
- No alteration was made to the SHA-256 executable guard or to the existing virtual configuration and input recovery implementations.

The cleaned 1.0.0 source has been compiled and validated in-game by the maintainer. No measured FPS performance claim is made.
