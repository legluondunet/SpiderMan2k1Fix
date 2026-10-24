# SpideyPC.exe — reverse-engineering development notes

> Living technical reference for the **original Spider-Man (2001) PC executable**, prepared from the project's static analyses, instrumentation and documented runtime tests. This is not a complete decompilation or a replacement for the original binary. **Addresses are executable-specific.** Historical research documents may describe superseded experiments; this document separates observations from hypotheses.

## 1. Supported binary identity

| Property | Value |
| --- | --- |
| Executable | `SpideyPC.exe` (original retail PC build) |
| Architecture | PE32, Intel x86 |
| Image base | `0x00400000` |
| File size | 1,507,328 bytes |
| PE timestamp | 2001-08-15 08:23:03 UTC |
| SHA-256 | `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea` |

All virtual addresses (VA) in this document assume this exact binary and image base. Where a relative virtual address (RVA) is needed, calculate `RVA = VA - 0x00400000`. **Do not equate a VA/RVA with a raw file offset.** Check instruction bytes before using any patch site. Other releases or modified executables have not been established as equivalent.

## 2. Loading and patch architecture

- The fix loads through a local `winmm.dll` proxy, which loads `SpiderMan2k1Fix.dll`. The original game executable is not rewritten on disk.
- The supported early in-memory No-CD patch bypasses the original disc-check routine at **RVA `0x001163E0`**; unlike a passive tracing hook, this is a **signature-checked function-entry replacement** installed by `winmm_proxy.c` on process attach (see §2.1).
- Display compatibility uses borderless fullscreen at desktop resolution, with widescreen/4:3 modes. Game-native and DLL-owned settings are consolidated in `SpiderMan2k1Fix.ini`.
- A default 30 FPS cap preserves game timing; historical Scorpion encounters motivated avoiding high frame rates.
- The project has investigated original DirectDraw/Direct3D 7 presentation, Bink movies, native input ownership and script cleanup. The game is not fully decompiled.

### 2.1 Original disc check: discovered callsites, entry and early No-CD patch

The supported original `SpideyPC.exe` invokes a **disc-check function** at **VA `0x005163E0` / RVA `0x001163E0`**, with at least the following two SHA-pinned original **direct CALL instructions**. Their destination is verified both by the bytes in the project and by x86 `E8 rel32` target arithmetic:

| Item | VA | RVA | Original instruction bytes and meaning |
| --- | --- | --- | --- |
| Disc-check entry | `0x005163E0` | `0x001163E0` | Entry begins `81 EC 20 02 00 00 56 8B B4 24 28 02 00 00`: `sub esp,0x220; push esi; mov esi,[esp+0x228]` |
| First known direct caller | `0x005159AC` | `0x001159AC` | `E8 2F 0A 00 00` — `call 0x005163E0` |
| Second known direct caller | `0x00515D85` | `0x00115D85` | `E8 56 06 00 00` — `call 0x005163E0` |

**What the original routine does, at the available level of proof:** it is the game disc-check entry used by these call paths; the original function has a stack frame and reads a pointer argument as its prologue begins. With the unmodified check bypassed, the game's CD requirement no longer blocks normal startup in validated historical runs. **The full unpatched body is not reproduced or exhaustively interpreted here**: the specific media/drive/file probes, individual failure branches, potential message-dialog path and precise meaning of every original return value are **not established by this repository source alone**. Do not infer that this address is a general-purpose program-exit routine.

**How the current fix works (patch-owned logic, not original game code):**

1. The local `winmm.dll` proxy is loaded with the game. Its `DllMain(DLL_PROCESS_ATTACH)` invokes `patch_early()` **before** creating the worker thread that loads `SpiderMan2k1Fix.dll`.
2. The patch checks the in-memory PE DOS/NT headers, the original image base `0x00400000`, the image size, the **14-byte original function-entry signature**, and **both** original 5-byte CALL signatures above. A mismatch aborts the patch without intentionally changing the target bytes.
3. It temporarily changes the code page protection using `VirtualProtect`, replaces **only the first three bytes** of the function entry with `31 C0 C3` (`xor eax,eax; ret`), flushes the instruction cache and restores the previous protection. The remainder of the function body and both original CALL instructions remain unchanged. The original function is no longer executed past that three-byte early return on those calls.
4. The proxy records status `1` when applied; other internal statuses indicate load, PE, signature or protection failures. During main DLL initialization, `spiderman2k1fix.c` verifies the three-byte post-patch signature and can log `OK: early No-CD memory patch present at RVA 0x1163E0`, or a diagnostic failure.
5. **No on-disk EXE modification** and no separate No-CD user option are required. This is deliberately tied to the exact supported binary layout. The proxy's early patch is **distinct** from a generic `MessageBoxA` or CD-warning interception and from any later menu/video/3D exit hooks.

The patch changes the disc-check function's *return behavior* but does **not** change scene owners, clean up Bink resources, close the game window or implement a general exit. Its correct position in the game lifecycle is an **early startup prerequisite**, not an exit point for the menu or a cinematic.

**Source of record:** [`winmm_proxy.c`](../winmm_proxy.c) (`patch_early()`, original CALL signatures and `DllMain`), [`spiderman2k1fix.c`](../spiderman2k1fix.c) (post-patch signature/status log), [README No-CD feature](../README.md). The [external disc-check research](https://krystalgamer.github.io/spidey-breaking/index.html) is background material; it was **not** used to infer undocumented original instruction behavior here.

## 3. DirectInput 8: static findings

The detailed source of record is [DIRECTINPUT_STATIC_ANALYSIS.md](../DIRECTINPUT_STATIC_ANALYSIS.md) (2026-10-21). These findings come from **offline static examination** of the SHA-matched EXE.

| Item | Game VA / detail |
| --- | --- |
| `DINPUT8.dll!DirectInput8Create` IAT | `0x0053B01C` |
| Import stub | `0x0051672C` |
| Call into stub | `0x004FE0E4` |
| COM `CreateDevice` call | `0x004FE2CC`, vtable offset `+0x0C` |
| Keyboard `CreateDevice` | `0x005015A8` |
| Mouse `CreateDevice` | `0x00501728` |
| Keyboard device pointer | `0x006B7A5C` |
| Mouse device pointer | `0x006B7A64` |
| Keyboard GUID | `6f1d2b61-d5a0-11cf-bfc7-444553540000` |
| Mouse GUID | `6f1d2b60-d5a0-11cf-bfc7-444553540000` |
| Activation flag | `0x006B78F0` |

The GUIDs were decoded from embedded data at `0x0053C9E8` and `0x0053C9F8`. The game calls the DirectInput device `Acquire` function via COM vtable offset `+0x1C`; `GetDeviceData` uses `+0x28`.

### 3.1 Keyboard reacquisition logic

At `0x00501BC3`, the game calls `GetDeviceData`. At `0x00501BC6` it checks specifically for `DIERR_INPUTLOST = 0x8007001E`. On that HRESULT it invokes native `Acquire` at `0x00501BD9` and retries `GetDeviceData` at `0x00501C3B`.

However, an initial `DIERR_NOTACQUIRED = 0x8007000C` follows the alternate branch at `0x00501BCB`, bypassing that acquire/retry sequence. The later result is checked near `0x00501C3E–0x00501C50`.

### 3.2 Mouse reacquisition logic

Analogous locations: `GetDeviceData` at `0x00501D02`, comparison at `0x00501D05`, native `Acquire` at `0x00501D18`, retry at `0x00501D7A`, branch at `0x00501D0A`, and subsequent check at `0x00501D7D`.

### 3.3 Activation and Alt+Tab

The original window procedure assigns the activation flag at `0x005152F8`, in the `WM_ACTIVATE = 0x0006` case. The static findings therefore suggest a **possible** failure to reacquire on `DIERR_NOTACQUIRED` after Alt+Tab. This alone does not prove the failing runtime HRESULT. **Separately, native Windows 11 WinDbg did observe persistent `DIERR_NOTACQUIRED (0x8007000C)` at both keyboard `0x00501BC3` and mouse `0x00501D02` GetDeviceData sites**; see [Windows viewport/WinDbg diagnostic](../WINDOWS_VIEWPORT_DIAGNOSTIC.md). That capture did **not** timestamp each error against foreground/background focus or prove that any proposed retry fixed the main menu.

An experimental branch-native `DIERR_NOTACQUIRED` retry exists in `input_notacquired_retry.c`. Historical analysis describes it as **off by default**, opt-in using `SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY=1`; it patches only signature-matched branches and attempts to reuse the game's own Acquire/retry code. It was not thereby validated as a general solution. An earlier experimental COM vtable wrapper produced `E_INVALIDARG` under Windows 11. **Do not enable either experiment by default or infer runtime success from static analysis.**

## 4. Original window procedure and shortcut handling

Static disassembly recorded the native WndProc at `0x005152B0`, registered via `RegisterClassA` at `0x00515A18`. An original `WM_KEYDOWN` with `wParam=0x7B` (F12) reaches a `PostMessageA(WM_CLOSE)` call at `0x00515373`. The fix suppresses that unsafe original call, preserving appropriate stack behavior.

**Current development behavior:** physical F10 is the opt-in debug shortcut, to avoid MangoHud's F11/F12 bindings. Old F12-named configuration keys are historical aliases, not evidence that physical F12 still triggers the debug action. See [F10 validation matrix](F10_VALIDATION_MATRIX.md) and [historical F12 research](F12_DEBUG_TEST_MODE.md).

## 5. Opening movies, New Game comic and 3D cutscenes

- The four opening publisher/developer videos and later New Game Bink movie are separate lifecycle contexts. Historical measured content includes intro videos at 640×480, 320×192 and 320×240, and in-game cinematic material at 320×192. Presentation must preserve intended aspect rather than indiscriminately stretch to widescreen.
- The New Game comic was identified in tracing as `L1COV.bmp`. Its original clock/input-related hook is documented at `0x004279B8`.
- The native movie clean-up call `0x00443AD0` is associated with the four opening Bink calls in historical shutdown traces.
- The original script engine uses a `CutSceneScript` lifecycle, with per-script `CutSceneSkipAllow` permission. Recorded checks near `0x004C6654`/`0x004C665C` and skip-request logic near `0x004C66A6` must be understood within the **original game-owned script state**, not used as unconditional jump targets.
- The **first and second** introductory 3D scripts are distinct. Their skip permissions must be checked independently. Historical Wine traces show that a physical Space press may precede permission to skip the second script; synthetic Space or an immediate second request is therefore unsafe.
- The original script manager's live context and active-script fields (documented offsets `+0x1AC` and `+0x1B4`) were used for conservative guards. These guards cannot establish that **all** later cinematics use this same manager.

### 5.1 Repeated New Game and per-session state

A documented regression occurred when debug latches remained set across a normal return to menu. The game-owned `Shell_MainMenu` callback at `0x00493DD6` is used as the point to reset **DLL-owned per-session markers only**, provided no Bink is active. Do **not** reset arbitrary game memory or reinstall permanent hooks for every session.

### 5.2 Bink-to-first-3D key handoff

Wine tests found an unexpected physical Space-down state on reaching the first 3D cutscene after movie #4, sometimes causing an unintended skip. A bounded wait for actual Enter/Space key release before the original first 3D input update was validated in the documented Wine build #713; it injects no synthetic key and does not rewrite game-owned input buffers. **The origin of the transient Space state remains unproven.** Do not treat Windows 11 results as established from those Wine runs.

## 6. Game-owned 3D cleanup and shutdown

The historical reverse-engineering and native tracing identified:

| Native element | Address / interpretation |
| --- | --- |
| Game 3D cleanup checkpoint | `0x00455BF3`, native request 7 |
| Request field | `0x0060CFA4` |
| Original final-exit handling | `0x0045619D` |
| Original menu callback | `0x00493DD6` |
| Menu-native Quit | Action 18, slot 3; cleanup branch `0x004941D5` |
| Top-level native Quit check | `0x00455E50` |
| Quit byte | `0x0060CF88` |
| Four-opening-movie native cleanup | `0x00443AD0` |

An original startup branch can **bypass** the top-level quit test:
```asm
00455DC1: test al,0x4
00455DC8: jne 0x455E5E
00455DCE: cmp eax,ebp
00455DD0: jne 0x455E5E
...
00455E50: cmp byte [0x60CF88],bl
00455E56: jne 0x4561F2
```
Consequently, setting the Quit byte does **not** guarantee the native check runs on every startup path. Skipping across native stack/SEH cleanup, forcing process termination or closing an active HWND is not an acceptable substitute for the original owner-driven cleanup.

Historical F12 shutdown experiments achieved specific Windows 11 process exits, but F12 was subsequently repurposed to F10 debug behavior. In the current opt-in F10 gameplay mode, the documented intended behavior is a **native return to the main menu**, preserving the process and *not* forcing the final-exit flag. Do not confuse old shutdown trials with the current interface.

## 7. Display and rendering research

- Original game requests included low-resolution 640×480 modes that caused trouble on modern displays. The compatibility approach avoids exclusive fullscreen and keeps desktop display mode unchanged.
- Widescreen 3D uses aspect-dependent framing/FOV; menus, intro media, comic artwork and other 2D content require appropriate 4:3 treatment. There is an optional full-game 4:3 pillarbox mode.
- DirectDraw/Direct3D 7 state differs among Bink playback, menus, 2D artwork and playable 3D. A historically observed short **yellow window** on some native Windows movie-originated exits was absent in a control test exiting directly from 3D. That comparison **does not identify** the offending surface, draw call or teardown operation.
- The game HUD has **separately identified native player-HUD CALL sites** and **D3D7 draw-primitive signatures** for instructions, compass and boss UI. These are documented below; do not assume every 2D primitive is HUD.

### 7.1 Original player-HUD CALL-site inventory (16 SHA-pinned hooks)

The earlier `player_hud(2).gdb` prototype isolated a group of original x86 `CALL rel32` instructions handling the upper-left Spider-Man portrait, health/web gauges, rectangular bars, HUD counters and text. Their exact target addresses are checked by `hud_widescreen.c` before any in-memory detour is installed. This is a **verified hook inventory**, not a claim that we possess fully reconstructed source or semantic names for all original routines.

| Hook kind | Original game CALL sites (VA) | Expected original callee (VA) |
| --- | --- | --- |
| Six player-HUD sprite calls | `0x00465D08`, `0x00465F46`, `0x00466176`, `0x004663AA`, `0x004665D1`, `0x004667F6` | `0x00507910` |
| HUD/Webcart text scope | `0x00465A83` | `0x00458700` |
| Glyph draw sink | `0x005068CB` | `0x00507470` |
| Three rectangle scope calls | `0x0046687B`, `0x00466931`, `0x004669C7` | `0x00462FB0` |
| Three bar scope calls | `0x004668D1`, `0x00466A1B`, `0x00466A65` | `0x00462D60` |
| Bar draw sink | `0x00462EB3` | `0x00507470` |
| Rectangle draw sink | `0x004631A4` | `0x00507910` |

The original player HUD appears near the **top-left** (e.g. approximately X=56–461 at 1920×1080 in historical traces). The hook transforms sprite-quad X arguments at byte offsets `+0,+20,+40,+60`, and scoped bar/glyph X and width arguments at `+0,+8`. It tracks thread-local scope entry/exit so that shared text/bar/rectangle sinks outside a HUD scope are not indiscriminately changed.

The original resolution globals read by this module are `0x00568154` (width) and `0x00568158` (height). For any aspect wider than 4:3, the correction factor is:

```text
scale = (4 * screen_height) / (3 * screen_width)
player-HUD left anchor: x' = x * scale
1920x1080: scale = 0.75
1920x1200: scale = 5/6
```

Only horizontal geometry is corrected; vertical coordinates stay unchanged. Original 4:3 is unchanged. These hooks preserve CPU registers, EFLAGS and legacy x87 state, and require the exact verified EXE, CALL targets and initialization before rendering. An earlier GDB prototype reported corrected thousands of sprites, rectangles and glyphs; **instrumentation counts alone do not prove every HUD asset was individually classified**.

### 7.2 Direct3D7 path shared by compass, mission instructions and boss UI

The more targeted adaptive filter in [`hud_adaptive.c`](../hud_adaptive.c) recognizes original game **four-vertex triangle fans**, primitive type `6`, FVF `0x144`, from the caller **RVA `0x00102C72`** (VA `0x00502C72`). This is a **shared draw path**, not an exclusive HUD function: unrelated 3D world particles also reach it.

Consequently, recognition additionally checks original per-vertex screen bounds, **Z, RHW, diffuse color, texture dimensions and UVs**. The DLL works on **copies** of four original vertices and leaves `Y`, `Z`, `RHW`, colors and UV coordinates bit-preserved. Unrecognized primitives are passed through unchanged. The filter returns region identifiers `INSTRUCTIONS=1`, `COMPASS=2`, `BOSS=3` or `NONE=0`.

| Identified object | Representative original signatures | Anchor and behavior |
| --- | --- | --- |
| Bottom-right compass | Two mirrored **64×64** textured halves, `Z≈0`, `RHW≈27.6000`, opposite U orientation; untextured direction pointer `RHW≈26.0678` | Right anchored; measured at 1920×1080: halves X≈1428.8–1620 and X≈1616.2–1807.5, Y≈823.5–1003.5 |
| Bottom-left/center instructions | Solid translucent backdrop `0xA0000060` / `0xA0600000`; shadow glyphs `Z≈0.0012`, `RHW≈11.1764`, `0xFF000000`; light lavender glyphs `Z≈0.0010`, `RHW≈12.6862`, `0xFFC0C0FF` | Left anchored; panel examples X≈123.8–1173.8 at Y≈864–931.5 and X≈11.2–667.5 at Y≈774–891 |
| Boss portrait | **64×64** atlas texture, `Z≈0.0002`, opaque white, `RHW≈19–26` | Upper-right, right anchored |
| Boss bar frame | **32×32** tiles, same approximate foreground depth range | Upper-right, right anchored |
| Boss health fill | Untextured, opaque gradient, `Z≈0.0010`, `RHW≈10–17`; width varies with HP | Upper-right, right anchored; one row for Venom, two rows for Scorpion / J. Jonah Jameson |
| Boss missing-health mask | Opaque black `0xFF000000`, untextured, `Z≈0.0007`, `RHW≈13–19`; dynamically changing width | Must use *same* right anchor as the bar/frame; a former narrow-only match left a detached black rectangle when Venom was damaged |

Instruction glyph atlases measured in the source use **16×8, 16×32, 32×32 and 64×32** textures. Both dark shadow and bright foreground passes must be transformed identically or glyphs detach from their shadows/backdrops.

For compass and boss right-edge UI, the adaptive horizontal transform is:

```text
x' = screen_width + (x - screen_width) * scale
```

The mission-instruction region uses the left-edge transform `x' = x * scale`. The original 4:3 mode produces no adaptive transform. At 1920×1200 the boss scale is **5/6**; at 1920×1080 compass/instructions use **0.75**. These specific coordinates are **observed samples**, not absolute thresholds that apply unchanged to every resolution.

**Evidence:** Wine 1920×1080 compass/instruction traces and native Windows 11 1920×1200 boss traces, with regression cases in [`tests/test_hud_adaptive.py`](../tests/test_hud_adaptive.py). Tests additionally reject mismatched caller addresses, textures, colors, depth/RHW or region; especially **world particles** (e.g. `Z≈0.9983`, `RHW≈0.0229`) must not be modified. The observed and unit-tested signatures do not prove all bosses, levels or resolutions are covered.

### 7.3 In-game ESC/pause menu: separate native 2D primitives

The **original gameplay ESC overlay** is distinct from the normal HUD, although its fixes use the same `[HUD] HUDWidescreenFix` master option. It contains:

- Purple options ribbon, border tiles and menu labels; original 4:3 target width approximately **1440 pixels at 1920×1080**.
- Orange/blue translucent geometric motif, with **`Z≈0.00146`**, **`RHW≈9.988`**, alpha `0x80`, drawn over the 3D scene. Its X geometry is scaled **0.75 around display center**.
- Violet full-screen darkening; after shrinking the motif, the two outer strips (**X 0–240 and 1680–1920**) are rendered via clipped original geometry to retain the tint across the entire 16:9 screen, rather than painting a second central overlay.
- Red spider-shaped selector: original **96×72 quad** from a **32×32 texture**, `Z≈0`, `RHW≈27.519`. Scale its X width 0.75 **around its own center** to produce 72×72 without changing its position.

These signatures operate on copied Direct3D7 vertices with gameplay/menu and resolution guards. They were **visually validated under Wine, 1920×1080 Auto, on 2026-10-24**, both for the ESC menu and **REALLY QUIT? → NO / YES**. The background game world and normal HUD remain unaffected in those tests. The motif and cursor-specific matches have **not been validated at arbitrary resolutions or on native Windows graphics drivers**. Detailed findings: [`docs/NATIVE_PAUSE_MENU_43.md`](NATIVE_PAUSE_MENU_43.md).

### 7.4 Activation, validation and remaining gaps

- `[HUD] HUDWidescreenFix=1` is the **current default** for the exact supported EXE. `HUDWidescreenFix=0` explicitly disables the full player/adaptive HUD transforms and the three dependent ESC corrections. Original display aspect `4:3` also disables widescreen transforms.
- Hidden optional `[HUD]` keys `NativeESC4x3`, `NativeMotif4x3`, `NativeCursor4x3` default to 1 and can individually be set to 0 for troubleshooting; they are **not** normally generated in the INI. `NativeESC4x3=0` also disables its motif/cursor children.
- Scoped hook installation alone (used experimentally to exclude native HUD drawing from Hor+ projection diagnostics) does **not** enable geometry correction.
- The fixed-screen player HUD and adaptive lower-screen/boss HUD use **different native interception strategies**; do not conflate the 16 `CALL` sites with the shared D3D7 `DrawPrimitive` caller.
- Runtime Wine captures support portrait, life/web bars, compass, instructions and boss-layout work; Windows 11 native boss traces support representative signatures. Some other HUD forms, rare in-level prompts, dynamic bosses, unusual aspect ratios and multiple display drivers still need separate screenshots and runtime verification. No full original HUD source reconstruction is claimed.

Primary sources: [`hud_widescreen.c`](../hud_widescreen.c), [`hud_widescreen.h`](../hud_widescreen.h), [`hud_adaptive.c`](../hud_adaptive.c), [`hud_adaptive.h`](../hud_adaptive.h), [HUD tests](../tests/test_hud_adaptive.py), [native ESC analysis](NATIVE_PAUSE_MENU_43.md). Historical development: [HUD PR #21](https://github.com/legluondunet/SpiderMan2k1Fix/pull/21) and [native ESC PR #28](https://github.com/legluondunet/SpiderMan2k1Fix/pull/28).


### 7.5 Original graphics capabilities and exclusive fullscreen limits

The stock executable uses **DirectDraw 7** to create/present video surfaces and **Direct3D 7** to render 3D and a substantial part of the **2D UI**; its early-2000s original display path used **exclusive fullscreen**, not a borderless desktop-size window. These are **original-game behaviors** as reconstructed from the source-level interceptions and original runtime mode requests.

| Original game behavior / evidence | What is established, and what is not |
| --- | --- |
| **Original fullscreen ownership** | Original code requests a DirectDraw **exclusive/fullscreen cooperative mode**, switches the **physical video mode** with `IDirectDraw7::SetDisplayMode`, and sets up a flipping primary surface with `DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP` and a backbuffer. The patch intercepts these native COM calls; their **exact original call instructions/VA**, and a complete enumeration of device capabilities, are not yet disassembled here |
| **Original menu's logical render mode** | The game issues a **640×480, 16-bpp** mode request for menus/early 2D. Its own menu/2D drawing space is 640×480. This was **observed directly at the DirectDraw boundary** and was a modern-monitor black-screen trigger under original exclusive handling |
| **Original game configuration** | The retail `Spidey.cfg` factory record starts with **width 640, height 480, color depth 16** (little-endian DWORDs `0x00/0x04/0x08`); the game's native Display submenu also exposes changing resolution, color depth and brightness (§8.6–8.7) |
| **Gameplay mode / depth** | Gameplay calls the same underlying mode-switching APIs with a game-configured logical width/height; our fix **forces 32-bpp** and skips physical mode switches. **Do not retroactively describe the retail game's 32-bpp support as its sole original bit depth** |
| **Original resolution menu** | The native menu can select a mode, but **the complete original selectable-mode table or display-driver enumeration has not been recovered**. Only **640×480** is securely established as a default/legacy original request in these records. Historical 1400×1050, 1440×1080 or 1920×1080 displays under our compatibility layer are **not proof** of those modes appearing in the retail selection list |
| **UI aspect / pixels** | Menus and overlays use 4:3-origin coordinates, while videos include observed source frames **640×480**, **320×240** and **320×192**. Those are **media source/frame dimensions**, not selectable 3D display modes |

**Native exclusive-mode limitations:** the original engine assumes it owns the physical display's resolution/format and a flipping-primary presentation arrangement. Under modern monitors/Wine this produced an unsupported 640×480/16-bpp display, stretching of 4:3 art, and an exclusive-fullscreen Alt+Tab crash in prior observations. These are observed compatibility problems, **not evidence that Direct3D7 cannot draw higher-resolution imagery**. Rendering menu sprites at 640×480 and changing monitor mode are separate concerns.

**Our patch-owned replacement, not native functionality:** `spiderman2k1fix.c` intercepts `SetCooperativeLevel` and forwards **`DDSCL_NORMAL`**, acknowledges the game's legacy `SetDisplayMode` without changing the monitor, substitutes a **windowed PRIMARY plus offscreen render/backbuffer** for the original exclusive flip chain, and presents by **Blt** inside a borderless desktop-size window. The UI's 640×480 logical coordinates map into the centered 4:3 region. `display_policy.c` accepts a broad range of user-supplied logical resolutions for the **fix** (not a proven stock-mode list), locks game settings to 32 bits and supports an internal centered 4:3 canvas. The original game EXE is unchanged on disk.

**Source references:** [`spiderman2k1fix.c`](../spiderman2k1fix.c) (`hooked_set_cooperative_level`, `hooked_set_display_mode`, `hooked_create_surface`, `hooked_flip`), [`spidey_cfg_template.h`](../spidey_cfg_template.h), [`display_menu_lock.c`](../display_menu_lock.c), [`display_policy.c`](../display_policy.c). Precise original resolution-enumeration routine/addresses remain a future reverse-engineering task.

### 7.6 Original ESC menu: purple ribbon, bitmap-textured border and red selector

The **actual ESC gameplay pause UI** is a D3D7 overlay created **by the original game**, different from our optional **Alt+Tab PAUSED screen** drawn with Win32 GDI in `pause_surface.c`. The original ESC scene contains **separate draw calls** for banner, border, text, motif/tint and cursor; a single global screen bitmap does not account for the traced geometry.

| Original ESC element | Specific recovered graphical evidence |
| --- | --- |
| **Violet-blue main ribbon** | **Untextured**, four-vertex D3D7 triangle fan `FVF=0x144` (28-byte XYZRHW/diffuse/UV vertex); diffuse **ARGB `0x7F3232A0`** (alpha 0x7F; R=0x32 G=0x32 B=0xA0). Original representative bounding box at **1440×1080**: **X −5.6..1442.8, Y 400.5..675.0** |
| **Really Quit? confirmation ribbon** | Same general UI mechanism, but different vertical extent, approximately **Y 432..634** in the measured 1920×1080 Auto recording |
| **Decorative frame** | **Eight textured quadrilaterals:** two horizontal strokes, two vertical sides and four corners. The renderer binds native **8×8 textures**. A representative draw sequence is **3173–3180**, but draw sequence numbers are per capture, **not stable original addresses**. Color/detail pixels inside the original 8×8 textures and corresponding precise file names **have not been recovered** |
| **Text** | Separate quads for bitmap-font glyphs, mostly texture dimensions **16×32, 32×32 or 64×32**, and characteristic `Z≈0.00098,\ RHW≈12.686` (foreground) or `Z≈0.00122,\ RHW≈11.176` (related pass). Samples include **PAUSED** near Y≈171, **REALLY QUIT?** near Y≈283 and native option labels near Y≈436/508/580 or Y≈468/540; these are **measured output geometry**, not source-line numbers |
| **Red spider selector** | Original **32×32 bound texture**, drawn as a **96×72** sprite quad at **Z≈0, RHW≈27.519**, diffuse white `0xFFFFFFFF`; its position legitimately moves anywhere across the screen. The patch corrects width to 72 at 1920×1080 by transforming only X around the quad's center. Historical archive candidate: **`lti/cursor.bmp`**, but that filename's direct connection to the live 32×32 ESC texture has **not been proven** |
| **Native dim/tint** | Additional original untextured translucent geometry, potentially spanning beyond the central ESC UI. The patch clips/replays the original geometry in the 240-pixel lateral margins at 1920×1080 to preserve screen-wide tint |

**Distinguish bitmap resources from GPU textures:** eight small **8×8 GPU-bound textures** were observed at the original border draws; their source **might** be BMP files or data packed inside an archive/resource, but no specific BMP↔border-texture mapping is established. Likewise, the 32×32 ESC selector texture size does not prove the on-disk filename. The fix's **independent** Alt+Tab GDI pause ribbon only **approximates** the original border using colored lines, with `Arial Black` fonts and the original icon extracted from `SpideyPC.exe`—these are not the retail ESC artwork files.

Original D3D7 identification is guarded on the native draw **caller RVA `0x00102C72`** (VA `0x00502C72`), `D3DPT_TRIANGLEFAN`, four vertices, FVF `0x144`, texture presence/dimensions, depth, RHW, same-Flip banner ownership and native game mode. At 1920×1080 the goal is original centered **1440×1080** art with **240-pixel margins**; screen X maps about display center `x' = 960+(x-960)*0.75`. The panel/border use **small experimentally calibrated** corrections at 1920×1080 after that mapping. Preserve Y, depth, UV and game memory. [Native ESC study](NATIVE_PAUSE_MENU_43.md).

### 7.7 How the original orange-and-blue spider-shaped backdrop is drawn

The motif is **not a large BMP pasted behind the ESC menu**. Previous original D3D7 tracing identified **eight distinct large, symmetric/mirrored, untextured quadrilaterals**, with original vertex winding/ordering variations. A representative original diffuse **ARGB `0x80868620`** (alpha `0x80`) and the recovered rendering-state sample `source blend=1, destination blend=4` are characteristic of that observed pass. That blend state is **not ordinary SRCALPHA/INVSRCALPHA blending**; together with the colored primitives it produces a translucent/multiplicative-looking overlay. A blue/darker companion and native full-screen dimming use related geometry; individual pass ownership must not be guessed from a color alone.

**Exact executable draw signature:** `caller RVA 0x00102C72` (`VA 0x00502C72`), non-indexed **four-vertex** `D3DPT_TRIANGLEFAN`, `FVF 0x144`, **no stage-0 texture**, `Z≈0.00146`, `RHW≈9.988`, vertex diffuse alpha `0x80`. Motif quads must also have plausible 2D on-screen bounds and must **not** be classified as 3D scenery (commonly caller `RVA 0x00103389` with different textures/depth), thin HUD rules, the purple band or cursor.

**Vertex layout and exact coordinate status:**

```c
// Original D3D7 28-byte transformed/lit vertex interpretation
struct SpiderMenuVertex {
    float x;          // +0x00 : screen X
    float y;          // +0x04 : screen Y
    float z;          // +0x08 : normalized depth
    float rhw;        // +0x0C : reciprocal homogeneous W
    uint32_t diffuse; // +0x10 : ARGB
    float u;          // +0x14 : texture U (unused by motif)
    float v;          // +0x18 : texture V (unused by motif)
};
```

The eight quads' **complete original per-vertex `(x,y)` coordinates are not preserved in the current repository or its summarized development notes**. Historical capture discussions confirm that coordinates were collected in a separate diagnostic log, but those original logs/vertices are **not present in the GitHub project snapshot audited**. Therefore, **do not invent 32 point pairs** or treat the patch's *recognition bounds* as the eight original vertex lists. Original panel bounds **X −5.6..1442.8/Y 400.5..675** (§7.6) are **the banner**, not the spider motif's vertex coordinates.

What **is** exact in our patch: on a **1920×1080 Auto** image the original motif's matching four vertices are **copied, then only X changes** with `x' = 960 + (x-960) * 0.75`; Y, Z, RHW, diffuse and UV remain identical. Its native full-width tint is preserved by splitting the original triangle fans into triangles and clipping against **X=240** and **X=1680**, interpolating original Z, RHW, color and UV in the two outer gutters **X 0..240** and **1680..1920**. No new large spider bitmap was generated, and the world is left alone. The live classifier is in [`spiderman2k1fix.c`](../spiderman2k1fix.c), `maybe_correct_native_motif43()` and `native_motif43_restore_gutters()`. The original individual vertex coordinates can be reliably recovered **only by rerunning an original-vertex trace before our transformation** and recording all four XY pairs for each of eight unique quads, with their full diffuse/color/render-state and draw phase.

### 7.8 Original game fonts, known bitmap resource paths and confidence

**Font machinery recovered in the [partial PC decompilation](https://github.com/krystalgamer/spidey-decomp):**

- [`mess.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/mess.cpp) implements **`Mess_LoadFont` → `FontManager::LoadFont`**, **`Mess_SetCurrentFont` → `FontManager::GetFont`**, **`Mess_DrawText` → `Font::draw`**. The selected font, drawing scale, justification, top/bottom tint and text/shadow colors are stateful, not a call to Windows `TextOut`.
- [`FontTools.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/FontTools.cpp) decodes a **font's character count, per-glyph width/height/baseline/slice width, 16-color palette (CLUT) and glyph image data** from the font buffer. It constructs per-character **`SlicedImage2`** graphics, then packs them into the game’s texture system. The glyphs are therefore **bitmap/texture-based**, rather than ordinary TrueType glyph rendering. Some unrelated `Font::draw` portions are not reconstructed in that partial decompilation.
- [`pshell.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/pshell.cpp) selects `sp_fnt00.fnt` for normal text, `sp_fnt02.fnt` for small/instructional text and `font_big.fnt` for big text. [`shell.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/shell.cpp) loads `font_big.fnt`, `sp_fnt02.fnt`, `sp_fnt03.fnt` during shell initialization. The original [`front.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/front.cpp) contains `PrintPaused(): Mess_SetScale(256); Mess_SetRGB(0x7F,0x19,0x21,0); Mess_DrawText(256,50,"Paused",0,0x1000)`. This gives an **original game-level text rendering function**, not proof that this call renders every menu glyph in precisely the same context.
- Original 1920×1080 ESC traces show **textured quad glyphs** with differing atlas dimensions and depth/RHW (§7.6). Both the per-character texture work and drawn glyph passes need their X-coordinate adjustment to preserve 4:3 typography in widescreen. The existing [`hud_widescreen.c`](../hud_widescreen.c) and native-ESC matcher implement scoped glyph fixes, rather than changing `.fnt` files.

**Resource container vs directories:** previous project archive investigations identified original assets **inside the game's `data.pkr` archive**, with reported *internal* paths/names such as:

| Earlier archive inventory / candidate | Interpreted content | Confidence / limitations |
| --- | --- | --- |
| `data/sp_fnt00.bmp` … `data/sp_fnt03.bmp` | Related bitmap font artwork candidates | Reported from earlier asset inventory, **not reopened/byte-verified during this audit**; cannot assume these BMPs are loaded instead of the actual `.fnt` glyph data |
| `sp_fnt00.fnt`, `sp_fnt02.fnt`, `sp_fnt03.fnt`, `font_big.fnt` | Fonts *explicitly named and loaded* by the PC code | **Source-confirmed filenames**; precise inner paths must be checked against the original archive |
| `data/shell_bg.bmp` | Shell/menu background image candidate | Archive-inventory finding, **not** a proven source for the ESC purple banner, spider motif or 8×8 border |
| `data/shell.psx`, `data/Icons.PSX` | Other shell/icon resources | Candidate archive contents, **not direct evidence for an ESC BMP frame** |
| `lti/cursor.bmp` | Historical red-menu-cursor candidate | Archive-path attribution from prior research; the actual original ESC 32×32 bound texture has **not been individually file-matched** |

The **outer** `data.pkr` archive should be examined or extracted to inspect these file names/resources; `data/sp_fnt00.bmp` or `lti/cursor.bmp` are reported **archive-internal paths**, *not necessarily loose files adjacent to `SpideyPC.exe`*. The project's [partial decompilation `pkr.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/pkr.cpp) documents directory/file lookup and reading from a `.pkr` archive, but **neither the retail archive nor the relevant source textures are committed to our fix repository**. To identify the *specific ESC border BMPs*, one still needs a verified archive index and a runtime texture-content/caller correlation. **No border-file basename, eight-quad motif vertex table or complete retail resolution list is presently proven.**

## 8. Additional recovered native functions and structures — full repository audit

This section inventories **previously missing positively identified functions, branches and data layouts** in the SHA-verified original EXE. It is based on repository code and historical static/runtime records, **not on a new independent disassembly of the copyrighted EXE**. All addresses below are **VA**, except where expressly marked RVA. Experimental patch code and observed game code are kept distinct.

### 8.1 Main controller, request dispatch and original cleanup

| Game address | Recovered original function |
| --- | --- |
| `0x00455C90` | Main startup controller; calls video owner `0x00470750` four times at `0x00455CF1`, `0x00455CFE`, `0x00455D0C`, `0x00455D1A` |
| `0x00455D23 → 0x00443AD0` | Original post-movie transition/cleanup; the callee optionally launches a further movie at `0x00443BE7` using pending ID `[0x0054F570]`, then clears it |
| `[0x0060CFA4]` | **Next transition request**, not a continuous current-scene identifier |
| `0x00455F24` / `0x00455F56` | Reads/dispatches transition request via original **11-entry** jump table `0x00456274` |
| `0x00456185` / `0x00456198 → 0x0048A820` | Request-7 controller path and native controller finalization |
| `[0x02E0988C]` / `[0x00568FB0]` | Game flags and status consulted during controller-7 exit; neither is by itself a universal quit signal |
| `0x00455E50` / `[0x0060CF88]` | Native Quit-byte test; can be bypassed by upstream branches at `0x00455DC8` and `0x00455DD0` |
| `0x00455D98` / `0x0045620C` | Controller loop continuation / final cleanup path |
| `0x004E5C10` / `[0x006B4C9C]` | Countdown processing, **not** an active scene identifier |

Confirmed **writers** to the request field include `0x0044108A` (7), `0x0044174E` (8), `0x00441A92` (6), `0x00441D00` (3), `0x0047BE6F`/`0x0047BE7B`/`0x0047BE87` (7/10/8), `0x004B28B3` (2), `0x004B297B` (8), `0x004BD2DC` (7), `0x004BE027` (2) and `0x004E28BE`/`0x004E28CA` (9/3). The **write action** is known, but most writers' higher-level scene identities are **not**. Do not relabel all 11 dispatch actions as scenes. Detailed disassembly and transition table: [F12 native shutdown research](F12_NATIVE_SHUTDOWN.md).

### 8.2 Shell_MainMenu and the two original startup still screens

| Game address | Recovered original behavior |
| --- | --- |
| `0x00493990` | Native `Shell_MainMenu` entry; native frame callback `0x00493DD6` calls `0x00505720` |
| `0x004E5D60` | **Clock busy-wait**: polls game clock `[0x006B4CA0]` until increment has elapsed |
| `0x0049415B`, `0x0049416F` | Conditional and unconditional calls to that clock wait in the original menu loop |
| `0x00494209` | Additional native clock wait as the menu processes a selection / exits |
| `0x004941D5` | Original menu Quit/selection cleanup, action 18 in slot 3 |
| `0x004A1FE7 → 0x004A1930` | Timed opening still page, using native clock, frame counter `[0x0060CFA8]`, one-time marker `[0x006A7785]` and original object/SEH cleanup |
| `0x004A2006 → 0x004A17A0` | Subsequent native **input-wait** page; polls `0x0050C180` at `0x004A1896`, proceeds to native page cleanup at `0x004A18AE` |
| `[0x0054D38C]` | Startup transition variable; 1→0 observed between still-page stages |

Read-only menu fields `[0x00682948]` (context), `[0x006828D0]` (stage) and `[0x0068293C]` (dispatcher) were identified, but **none alone identifies which still is actually visible**. A 3.659-second gap between menu frame callbacks was observed; the specific clock-stall cause was **not conclusively established**. The initial-menu wait workaround was tested in a debug configuration, not independently validated across every release. Source: [native startup research](F12_NATIVE_SHUTDOWN.md) and [historical debug menu timing](F12_DEBUG_TEST_MODE.md).

### 8.3 New Game comic and native Bink playback

| Game address | Recovered original behavior |
| --- | --- |
| `0x004E0461 → 0x00427910` | Script interpreter invokes the New Game comic owner |
| `0x0042796D → 0x004151D0` | Supplies literal **L1COV.bmp** (`0x00549214`) to the original image setup |
| `0x00427983` / `0x004279B8` | Saves initial game clock / reads current clock |
| `0x004279BE` / `0x004279C0` / `0x004279C6` | Subtracts start, compares with **`0x12C = 300` clock ticks**, branches on elapsed time |
| `0x004279F9 → 0x004279FB` | Pushes **Bink video ID 4** and calls native `0x00470750`; return site `0x00427A00` |
| `0x00470750` | Shared game-native Bink video owner; **30 direct callsites** across startup and scripted sequences, not a dedicated New Game method |
| `0x0047081D`, `0x0047085F` | Calls to Bink frame tick `0x0050B5A0` |
| `0x00470835` / `0x0047084E` | Native input/frame update, then skippable movie input `0x0050C180` (after 60 iterations and only if skip allowed); mask **`0x07000000` = Enter/Esc/Space** |
| `0x00470871 → 0x0050B790` | Original Bink cleanup and handoff; playback exit **does not imply process exit** |
| `[0x006151F8]` / `[0x00AC0BA4]` | Movie-active flag / Bink video handle, not universally sufficient to identify owner |

The **original script interpreter resumes after Bink**: `0x004E0466` advances its operand and `0x004E2B3F` dispatches the next scene opcode. Static caller grouping also identifies startup calls, optional nested `0x00443BE7`, UI/sequence caller `0x0049D654` and 24 scripted callers `0x004279FB`–`0x00427D8A`. **Do not assume every caller has the same owner or exit cleanup.** An earlier trace's apparent huge movie ID was an overwritten floating-point temporary; the real New Game movie argument is **4**.

Sources: [Bink assembly and Windows 11 parent trace](F12_NATIVE_SHUTDOWN.md), [script debugging](F12_DEBUG_TEST_MODE.md).

### 8.4 DIK input path, 3D action mapping and real CutSceneScript interpreter

| Game address | Recovered original function or field |
| --- | --- |
| `0x0045553B → 0x00440EF0` | Native 3D game/input update; shared by scripted intro and playable gameplay |
| `0x00440F34`, `0x00440F77`, `0x00440F8E`, `0x00440FAF`, `0x00441606` | Original `0x0050C180` input calls with different native masks; not five separate scene types |
| `0x0050A650` / `0x00501CB0` | Original DIK polling helper and raw buffered keyboard reader |
| `[0x006B792C+DIK]` / `[0x006B7965]` | Game's keyboard-state buffer and `DIK_SPACE=0x39` slot; `0x00501C87` writes 0xFF on key-down; `0x00501C8F` writes 0x80 on key-up |
| `0x00505720 → 0x0050A190` | Game frame update and key-to-action converter; mapping array `0x00568398` |
| `0x005058A5 → 0x00479540` | Original **action 0x10** transition, state at `[0x00661200]`: held, just-pressed, early and counters. In tested keymap, Space maps to 0x10 |
| `0x00455427`, `0x00455444` | Original direct Space release/press gate; guarded by bit 0x100 at `[0x02E0988C]`. **Not** proven to own the two New Game scene skips |
| `0x004E1485 → 0x004BC1A0` | `CutSceneScript` interpreter: sets manager `+0x1AC` context, `+0x1B4` active, `+0x1B8` script IP and resets `+0x1A4` skip permission |
| `0x004E14A5` (related cases `0x004E14C3`/`0x004E14D9`) | `CutSceneSkipAllow(%i)` updates original manager `+0x1A4` |
| `0x004C6654`, `0x004C665C` | Original scene update checks context and skip permission |
| `0x004C66A6` / `[0x0060CFC4]` | Sends original one-byte native skip request; `0x004BC3E0` consumes it and performs scene cleanup |
| `0x004BC3EF`, `0x004BC4A2`/`0x004BC4AF` | Original script-active clearing and `CutSceneEndScript`; `0x004BC17E` clears skip request for next script |
| `[0x006A9038]` | Pointer to the original cinematic script manager; observed in **two distinct** New Game 3D script lifetimes |
| `0x004DE890`, `0x004DE8B0`, `0x004DE8D0` | Original event list clear/test/search; head `[0x006B4708]`, hash buckets `0x006B4214`, node fields `+4` triggered, `+5` pending, `+0x0C` ID, `+0x14` next |

Live Wine traces positively distinguish **first Stan Lee CutSceneScript**, **second separate CutSceneScript**, and **playable 3D**. The original skip permission is granted independently during each script. Space/action 0x10 can skip a cutscene **or perform Jump in gameplay**; it does not prove script ownership. Actor controller `[0x0056F3B8]+0x2A0`, phase `[0x005FAECC]`, the generic event list and HUD appearance were considered **insufficient as scene discriminators**. The inspected first 12 event nodes were unchanged; this does not prove the full list is inert.

Source: [six-stage lifecycle and input disassembly](F12_DEBUG_TEST_MODE.md), [F10 validation](F10_VALIDATION_MATRIX.md).

### 8.5 Original gameplay ESC/pause and 3D cleanup owners

- `0x00455549 → 0x0047BC40`: native per-game-frame **ESC pause-option handler**, with context `[0x00682950]` and option menu `[0x00682968]`. Its original action choices are **0 retry**, **1 quit to training**, **2 quit to main menu**; an additional menu pointer is at `[0x00682960]`.
- `0x00455A8B → 0x00455400`: original 3D tick. When the pending controller request triggers owner cleanup, it reaches `0x00455BF3` and `0x00455BF9 → 0x004302D0`.
- Top-level controller-7 completion at `0x00456198 → 0x0048A820` must check native exit conditions. Request **7 is not a universal “quit now” command**.
- Original in-game ESC rendering and its primitives are separately cataloged in **§7.3**. Our **focus-loss pause experiments** are not part of the game's original pause-menu mechanism, and past stability problems preclude treating them as validated.

### 8.6 Recovered binary layout of Spidey.cfg (252 bytes)

[`virtual_spidey_cfg.c`](../virtual_spidey_cfg.c) records the original 252-byte configuration layout recovered from factory data and game setting changes. The **INI and virtualized file I/O are our fix**, not original executable features.

| Original byte offsets | Meaning |
| --- | --- |
| `0x00`, `0x04`, `0x08` | Little-endian 32-bit resolution width, height, color depth |
| `0x0C` | Brightness 0..9 |
| `0x14` through `0x3C` step 4 | **11 DIK keyboard bindings**: Up, Down, Left, Right, Jump, Kick, Punch, Web, Aim, Zipline, Swing |
| `0x50` through `0x68` step 4 | **7 joystick button bindings**: Jump, Kick, Punch, Web, Aim, Zipline, Swing |
| `0xEC` | Music/SFX volume, 0..`0x3FFF` |
| `0xF0`, `0xF4` | Movie volume / voice volume, 0..255 |
| `0xF8` | Mono=1 / stereo=0 |

Joystick movement bytes **`0x40`–`0x4C` remain undecoded**; unknown fields should be preserved verbatim through `[Internal] RawHex`, not relabeled.

### 8.7 Original Display menu branches and a tentative FOV scalar

The game's **Display option-selection** code at **RVA `0x0010DD19`** (VA `0x0050DD19`) handles **0 screen size, 1 color depth, 2 brightness**. The **original print-time field reads** are **RVA `0x0010DB3F` (height)**, **`0x0010DB44` (width)**, **`0x0010DBAA` (depth)**; all four are exact-signature checked in [`display_menu_lock.c`](../display_menu_lock.c), without changing the real renderer's surface pixel format. The 32bpp policy is a **patch decision**, not an original hardware limitation.

The aspect-related [`fov_aspect_win.c`](../fov_aspect_win.c) experiment uses a **game-owned four-byte float** at **RVA `0x00150064` (VA `0x00550064`)**, whose original value is `1.0f` (IEEE-754 `0x3F800000`). This is a recovered **projection/aspect scaling variable**, **not a decoded native `SetFOV(degrees)` function or a calibrated vertical/horizontal camera angle**. Its exact original code cross-references and complete original camera pipeline are not reconstructed here.

**Changing it in the supported patch:**
- [`fov_aspect.c`](../fov_aspect.c) `SpiderFOV_AspectScalar(width,height,&value)` computes `value=(4.0/3.0)/(width/height) = 4*height/(3*width)`, valid only for screened geometries at least as wide as 4:3 (with limits/validation). Typical values: **4:3 → 1.0**, **16:10 → 0.833333**, **16:9 → 0.75**.
- `SpiderFOV_Init()` configures an **Auto-only** runtime experiment from `[Display] FOV` (default `Auto`), `ExperimentalFOVAspect` (1 to opt in) and optional `ExperimentalFOVHorPlus` (1 to opt in). The distributed `SpiderMan2k1Fix.ini` currently uses `ExperimentalFOVAspect=1`. At **forced logical 4:3**, startup explicitly selects automatic scaling with factor **1.0**.
- Game stage selection is observed from the game's **mode/display transitions**: `SpiderFOV_SetStage(0,...)` restores **1.0** for menu / 640×480 legacy mode; `SpiderFOV_SetStage(1,w,h)` applies the aspect scalar at the gameplay resolution. See [`spiderman2k1fix.c`](../spiderman2k1fix.c), `hooked_set_display_mode()`.
- `update_scalar()` checks PE32 and fixed image base **`0x00400000`**, verifies current 4 bytes match the **last known expected value**, temporarily sets `PAGE_READWRITE` using `VirtualProtect`, writes the new `float` bit pattern and restores the protection. A mismatch fails closed and disables the experiment. No original EXE is modified on disk.
- **Optional Hor+ experimental path:** changing the scalar also affects native projected on-screen vertical geometry. To counter that, `SpiderFOV_RestoreVertical()` acts only on candidate **depth-tested world quads**, using **`y' = mid_y + (y-mid_y)/scalar`**. Guardrails require an active Z-buffer and recognized HUD scopes; otherwise the code declines to transform. The D3D7 wrapper copies vertices and must avoid moving the 2D HUD, comic, menus and text. **Hor+ and the degree calibration are not established for all scenes/drivers.**
- Although `fov_aspect.c` parses numbers in **10–170**, [`fov_aspect_win.c`](../fov_aspect_win.c) explicitly logs **`manual FOV NOT APPLIED (game camera baseline not established)`**. It does **not** convert these numbers into native camera degrees. Do not document `FOV=90` as an effective supported adjustment.

An illustrative existing configuration (not a guaranteed arbitrary-FOV control):

```ini
[Display]
AspectRatio=Auto
FOV=Auto
ExperimentalFOVAspect=1
; ExperimentalFOVHorPlus=1   ; separate unverified mode; leave disabled normally
```

Original menu/title rendering uses D3D7 **caller RVA `0x00103389`**, distinct from gameplay/HUD/comic **caller RVA `0x00102C72`**.

The game-owned 2D comic art includes a **512×256 texture**, displayed with V span **0..240/256**. This signature supports the [`comic_native.c`](../comic_native.c) centered 4:3 correction, rather than proving every comic/image source uses that format.

### 8.8 Bink and DirectSound audio internals (focus research, not a stable pause claim)

| Native function/data | Understood meaning |
| --- | --- |
| `0x0050BDD0`, `0x0050BDE5`, IAT `0x0053B210` | Original game `_BinkPause@8` call |
| `[0x00AC0BA4]`, `[0x00AC1020]`, `[0x00AC101D]` | Video handle, separate audio-only Bink music handle, and game's existing audio pause state |
| `0x005039F0`, `[0x006BBD50]` | Initializes original **32 DirectSound voice records of 12 bytes** |
| `0x00504110` | Duplicates game-owned secondary voice buffers |
| `0x00504230`, `0x005042A0` | Native voice play and stop |
| `[0x006BBF1C]` | Original primary sound buffer, excluded from individual-voice manipulation |
| `[0x006B7920]` / `[0x006B7924]` | Read-only DirectSound device/primary candidates in diagnostics |

The patch's optional synchronized focus-pausing reused these **original APIs and data structures**. The original engine ownership is mapped, but the focus pause experiment was **not proven generally reliable**. Sources: [focus audio/video diagnostics](../FOCUS_AUDIO_VIDEO_SYNC_TEST.md), [Bink focus module](../focus_media_pause.c), [DirectSound voice module](../focus_dsound_pause.c).

### 8.9 Original F9 screenshot path and statically linked CRT fault

- Native F9 already polls **`DIK_F9=0x43`**, with approximately **500 ms debounce**, then calls its **BMP writer** at **RVA `0x0010A1DC` / `0x0010A62B`** (VA `0x0050A1DC` / `0x0050A62B`). The fix's PNG dispatch replaces only these verified call instructions; it does not create the original keyboard binding.
- Original screenshot readback from DirectDraw **PRIMARY** uses **Lock caller RVA `0x001034D4`** and **Unlock caller RVA `0x00103537`**. The fix distinguishes the native primary and the completed backbuffer for borderless frame capture; that is a targeted screenshot observation, not a rule for all DirectDraw operations.
- An original statically linked CRT **`fclose(FILE*)`** starts at **RVA `0x0012A7FC`**, according to the Windows crash/disassembly audit. The actual fault at **VA `0x0052A805`** received a **NULL `FILE*`** from game caller **`0x005186F8`**. The invalid call is documented, but **what caused NULL remains unknown**. The defensive VEH workaround was experimental. See [Windows crash research](../WINDOWS_VIEWPORT_DIAGNOSTIC.md).

### 8.10 Original media/audio owners and the multi-component focus-pause mechanism

The 2001 game has **separate presentation, Bink video, Bink soundtrack and DirectSound voice paths**. An Alt+Tab pause of the **gameplay/render thread** is **not** sufficient to stop all of them. This distinction was discovered and exercised in the [focus audio/video Windows test](../FOCUS_AUDIO_VIDEO_SYNC_TEST.md), the original native structures cataloged in §8.8, and the current source.

**Original game-owned sites and objects relevant to pausing:**

| Component | Original function / address | Interpretation and safe mechanism |
| --- | --- | --- |
| 3D / 2D frame presentation | Game calls `IDirectDrawSurface7::Flip` through its DirectDraw path (COM method, not one universal known EXE VA) | The fix intercepts the **DirectDraw Flip boundary** in `spiderman2k1fix.c::hooked_flip` and blocks further progression of the **calling render/game thread** inside `pause_at_flip_if_background()`. This is **not a recovered original engine-wide `PauseGame()` function** and does **not** suspend all asynchronous threads |
| Native Bink movie video | Movie object `[0x00AC0BA4]`; `_BinkPause@8` original import **IAT VA `0x0053B210`**, call at **VA `0x0050BDE5`**, within game routine `0x0050BDD0` | The fix calls the **same exported Bink API** to pause/resume a currently existing movie handle, without substituting a dummy decoder, forcing movie end or bypassing its owner |
| Bink audio-only music | Separate handle `[0x00AC1020]`; native pause state `[0x00AC101D]` | Also pauses through Bink's API when applicable; **do not resume** a replaced handle or audio already considered paused by the game |
| Native DirectSound effects / voices | **32 records × 12 bytes**, base `0x006BBD50`; init `0x005039F0`, duplicate `0x00504110`, play `0x00504230`, stop `0x005042A0` | Inspect game-owned slots via `IDirectSoundBuffer::GetStatus`, retain each actually playing **secondary** buffer and its looping flag, `Stop` while backgrounded, then `Play` only if the exact same slot/buffer remains valid and stopped |
| Primary DirectSound buffer | `[0x006BBF1C]` | Excluded: stopping primary audio is not equivalent to independently pausing the game's 32 voices |
| Window activation/keyboard return | Native `WM_ACTIVATE` state `[0x006B78F0]`; original DirectInput devices and polling (§3) | Focus restoration and input reacquisition are **separate** from stopping Flip/Bink/sounds; the patch's temporary `WH_KEYBOARD_LL` resume-key handling is not a native engine feature |

**Current optional pause sequence, from [`spiderman2k1fix.c`](../spiderman2k1fix.c):**

1. With `[Input] PauseOnFocusLoss=1` (factory default **0**), `hooked_flip()` observes foreground state. The game must **previously have been foreground** and another process's window must remain foreground for at least **150 ms**. A transient NULL foreground or same-process window is ignored. Windows 11 sometimes reported a **`Ghost`** helper window, spuriously satisfying this test; that case remains diagnostic/experimental (see [unexpected focus pause](../UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md)).
2. Upon the next Flip, `pause_at_flip_if_background()` latches the pause and first installs a **temporary** keyboard wake hook via [`pause_any_key.c`](../pause_any_key.c), `SetWindowsHookExA(WH_KEYBOARD_LL,...)`. The hook only swallows an ordinary key **when the game is foreground**. It deliberately preserves OS Alt/Win/Tab navigation. If hook installation fails, the code **abandons** this pause rather than risk a stuck game.
3. It calls **`SpiderFocusMedia_Pause()`** ([`focus_media_pause.c`](../focus_media_pause.c)) and **`SpiderFocusDSound_Pause()`** ([`focus_dsound_pause.c`](../focus_dsound_pause.c)) **separately**. Both audio modules are **opt-in independently**, default **0** (`PauseBinkOnFocusLoss`, `PauseDirectSoundOnFocusLoss`). A Flip pause without those options does **not** prove Bink/music/effects stopped.
4. It paints a pause display **inside the existing DirectDraw backbuffer**, via [`pause_surface.c`](../pause_surface.c): saves its pixels using GDI, dims/renders a native-ESC-inspired violet ribbon and original game icon, performs **one** `Blt` to the PRIMARY and preserves the original backbuffer until resuming. There are **no separate overlay HWNDs**; failure to draw the label does not imply failure to pause. Content bounds respect the 4:3/pillarbox region.
5. The game **stays blocked at Flip** while a loop **keeps Win32 messages pumping** with `PeekMessageA`, `TranslateMessage`, `DispatchMessageA` and `MsgWaitForMultipleObjectsEx(...,20,...)`. It does **not** freeze the entire process, suspend Bink worker threads, or guarantee that no non-render task continues elsewhere.
6. Back in the foreground, the user must **press and release an ordinary key**; the `WH_KEYBOARD_LL` hook discards the press/release so the game cannot accidentally jump/skip a cutscene with that same Win32 key. Then the fix removes the hook, **restores the original backbuffer**, resumes only retained/eligible DirectSound voices and Bink handles, and returns to the original Flip flow. The code is explicit that Win32 hook suppression alone is **not proof** that DirectInput/Raw Input cannot observe every hardware key event. Reacquiring native DirectInput devices requires its **own** guarded input path (§3).

The user-visible optional settings and boundaries are:

```ini
[Input]
PauseOnFocusLoss=1
PauseBinkOnFocusLoss=1
PauseDirectSoundOnFocusLoss=1
; DirectInputReacquire=1  ; independent input recovery; validate for target OS/build
```

**Runtime evidence (2026-10-21 Windows 11):** [`FOCUS_AUDIO_VIDEO_SYNC_TEST.md`](../FOCUS_AUDIO_VIDEO_SYNC_TEST.md) records one tester successfully pausing/resuming Bink video and music and stopping/resuming **11**, then **9**, playing DirectSound secondary voices in two separate 3D focus pauses, with reported `failed=0`. It does **not** establish unconditional correctness in all sequences, Windows/Wine configurations, driver versions, sound devices or input-recovery paths. The later [`UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md`](../UNEXPECTED_FOCUS_PAUSE_DIAGNOSTIC.md) documents spontaneous pauses associated with Windows `Ghost` foreground windows; this is a **distinct unresolved reliability issue**. Focus pausing and the **game's original ESC pause menu (§8.5 / §9)** are different operations.

**Research lesson:** a real simultaneous pause requires separate treatment of **frame production**, **Bink decoding/playback**, **Bink music**, **DirectSound secondary effects**, **foreground/input recovery** and **visible paused frame**. The fix uses those known original APIs/structures, but has **not** recovered a single global original engine pause switch.

### 8.11 Explicit unknowns and coverage boundary

Do not turn the following into “understood functions”: most 11-mode dispatch action *scene labels*, all event-node semantic meanings, all actor states, unobserved level-specific cutscene systems, complete 3D projection/FOV in degrees, the underlying reason for `fclose(NULL)`, why Alt+Tab loses input, missing joystick-axis bytes, exact cause of the temporary yellow frame, or as-yet-unclassified HUD primitives.

**Completeness means:** all **functionally identified findings located in the current repository's source and research documents** were compared during this audit, with the major previously missing address/data groups now indexed here. It **does not** mean the game has been fully decompiled or every unpublished past GDB observation can be recovered. The retail EXE is not included in the repository, so this audit did not re-disassemble it byte for byte.

## 9. Native exit points, transition ownership and cleanup by game phase

**Do not conflate three different operations:** (1) ending the current screen/movie/script, (2) leaving the current gameplay owner to return to a main-menu/controller loop, and (3) terminating `SpideyPC.exe` after final program cleanup. The game reuses several low-level routines, and the **same request value can end in a controller loop or a process shutdown** depending on native state.

This is a **flow/exit-point map** for the exact supported original 2001 binary. “Normal” paths are original game-owned logic verified statically and/or through logs; experimental F10/F12 behavior is described separately. Locations are absolute game virtual addresses (VA); data addresses are enclosed in brackets. The movie, comic and gameplay code must execute its own owner/SEH/resource teardown before another owner is entered.

### 9.1 At-a-glance phase transition map

The early disc-check call gate (VA `0x005163E0`, §2.1) is a **separate prerequisite**, not a screen/movie/script exit; its exact execution order relative to all startup substeps is not inferred merely from its two CALL addresses.

```text
STARTUP / MAIN CONTROLLER   0x00455C90
   |
   +-- Four original opening Bink calls to 0x00470750
   |      each exits via movie-owner cleanup 0x00470871 -> 0x0050B790
   +-- Original post-intro handoff 0x00455D23 -> 0x00443AD0
          |
          +-- Timed opening still 0x004A1930 (clock deadline)
          +-- Input-wait opening still 0x004A17A0 (native input accepted)
          |
          v
MAIN MENU  Shell_MainMenu 0x00493990
   |    +-- Quit (item 3 / action 18) -> 0x004941D5 -> original final exit
   |    +-- New Game -> original scene-script execution
   v
NEW GAME COMIC / BD COVER  L1COV.bmp, owner 0x00427910
   |    +-- after 300 native clock ticks: 0x004279C6 -> 0x004279F0
   v
NEW GAME BINK MOVIE #4  0x004279FB -> shared owner 0x00470750
   |    +-- natural end OR allowed Enter/Esc/Space skip
   |    +-- 0x00470871 -> 0x0050B790 -> original movie/script return
   v
FIRST 3D CutSceneScript (Stan Lee)
   |    +-- normal CutSceneEndScript OR allowed native skip
   |    +-- 0x004BC3E0 / 0x004BC3EF / 0x004BC4AF (script cleanup)
   v
SECOND DISTINCT 3D CutSceneScript
   |    +-- independent permission and native end/skip
   v
PLAYABLE 3D GAMEPLAY
   |    +-- ESC -> native pause choice 'quit to main menu'
   |    |       0x00455549 -> 0x0047BC40 -> 0x0047BE6F
   |    +-- native request 7 at [0x0060CFA4]
   |    +-- 3D cleanup 0x00455BF3 -> 0x004302D0
   |    +-- top-level controller 7 at 0x00456185
   |             +-- RETURN TO CONTROLLER -> original main menu
   |             +-- FINAL CLEANUP -> terminate (only when guards permit)
   |
   +-- Later levels/cutscenes may use other owners: not all decoded
```

The line from **Main Menu → New Game** is an observed scene progression; the entire intervening menu-option-to-script call chain has **not** been reconstructed. Likewise, the diagram does **not** claim that all later game chapters reuse exactly the two introductory CutSceneScript instances.

### 9.2 Exit-point reference table

| Phase being left | Original exit trigger / decision | Original point of exit and cleanup | Destination and confidence |
| --- | --- | --- | --- |
| **Startup disc check (not an end-of-scene)** | Original callers `0x005159AC` and `0x00515D85` invoke function `0x005163E0` | Original full disc-verification logic is **not exhaustively decoded**; current proxy signature-checks and replaces its first three bytes with `xor eax,eax; ret` before the normal game loader thread starts | Returns 0 immediately to the original caller instead of proceeding through its disc check. **Not** a menu/game process exit; its precise original status-code semantics remain unproven |
| **Each of the four opening videos** | Natural Bink end, or keyboard skip only if the video's caller permits it; shared playback loop uses frame ticks `0x0047081D`, `0x0047085F` | `0x00470750` movie owner; normal/skip paths converge at `0x00470871 → 0x0050B790` video teardown. The **four** startup CALLs are `0x00455CF1`, `0x00455CFE`, `0x00455D0C`, `0x00455D1A` | Back to the startup controller; after the four calls `0x00455D23 → 0x00443AD0` performs further transition cleanup. **Static recovery + runtime traces**, not an immediate jump to the menu |
| **First startup static screen (timed)** | The original clock deadline inside `0x004A1930`; code includes `0x004A1A26` clock read and `0x004A1A2B` compare / `0x004A1A2D` branch to native exit `0x004A1A36` | The **timed screen's own** loop, object release and SEH epilogue | Then the original input-wait screen `0x004A17A0`; **static code + Wine phase correlation** |
| **Second startup static screen (Press Enter / input-wait)** | `0x004A1896 → 0x0050C180` polls original input; success proceeds via `0x004A18AE` | `0x004A17A0` native input-screen cleanup, object/SEH epilogue | Genuine interactive `Shell_MainMenu` callback `0x00493DD6`; **static + Wine**. The exact displayed pixel content was not inferred from code alone |
| **Interactive main menu — Quit** | Original menu entry **3**, action **18**, table `0x00552AA8` | Game-owned selection and shutdown branch at `0x004941D5` from `Shell_MainMenu` (`0x00493990`) | **Program termination**, validated in historical Windows testing. Do not invoke that branch outside the menu owner |
| **Interactive main menu — New Game** | User selects New Game through the game's native menu options | `Shell_MainMenu` returns through its own selection/cleanup logic; later the scene-script interpreter calls `0x00427910` from `0x004E0461` | New Game cover; **runtime sequence confirmed, intermediate call chain not fully identified** |
| **New Game comic / BD cover `L1COV.bmp`** | Its native clock reaches **300 ticks (`0x12C`)** after the initial timestamp stored at `0x00427983`; clock reread `0x004279B8`, subtract `0x004279BE`, compare `0x004279C0`, branch at `0x004279C6` | Original comic loop ends through `0x004279F0`; immediately pushes movie ID **4** at `0x004279F9` and calls `0x00470750` at `0x004279FB` | Bink movie #4; **SHA-matched disassembly + Windows/Wine traces**. This is *comic timer completion*, **not** release of a Bink or termination of the process |
| **New Game Bink movie #4** | Natural completion when tick indicates finished; allowed key skip requires caller's allow flag and **60 iterations** before `0x0047084E → 0x0050C180(0x07000000)` recognizes Enter/Esc/Space | `0x00470871 → 0x0050B790` Bink/video cleanup; `0x0047089F → 0x00440920` post-video transition; returns to comic owner at `0x00427A00`, which returns to the scene interpreter | Scene script advances at `0x004E0466` and resumes opcode dispatch at `0x004E2B3F`, eventually entering 3D. **Static + Windows owner trace**. A skipped Bink frame does **not** itself clean up 3D scenes |
| **First New Game 3D scripted cinematic** | Original script completion, or a **single** skip request only once `CutSceneSkipAllow` for this script becomes true | Manager pointer at `[0x006A9038]`; active/context/permission at offsets `+0x1B4/+0x1AC/+0x1A4`. Native skip byte `[0x0060CFC4]` is consumed at `0x004BC3E0`, active script cleared at `0x004BC3EF`; `CutSceneEndScript` clears it at `0x004BC4AF` | Next **distinct** 3D CutSceneScript, whose skip permission starts disabled; **positive Wine two-script lifecycle traces** |
| **Second New Game 3D scripted cinematic** | Own native end or independently permitted native skip; cannot borrow first script's permission | Same CutSceneScript cleanup machinery; `0x004BC17E` clears skip-request byte for a new script | Manager becomes idle (context and active zero), playable 3D follows; **positive Wine trace**, not proof for every other level |
| **Gameplay → native pause menu** | Player opens ESC; original gameplay frame checks pause context `[0x00682950]` at `0x00455540` | `0x00455549 → 0x0047BC40` enters **native pause option owner** on game thread | Pause UI, **not** a game termination or a generic transition to a new engine mode |
| **Gameplay ESC → Retry / Training / Main Menu** | Native menu choice byte `[menu+0x14]`: **0 retry, 1 training, 2 main menu** | Branches **`0x0047BE87`→request 8**, **`0x0047BE7B`→request 10**, **`0x0047BE6F`→request 7** respectively; pause objects destroyed at `0x0047BEA0` / `0x0047BEB6`; pointers cleared | Original top-level controller handles the pending request; **static mapping**. Only the *return-to-main-menu* branch is covered as a safe exit in the cited traces |
| **Active 3D gameplay owner** | Native controller request becomes nonzero (e.g., request 7 after genuine pause/menu or input-owner logic); original 3D tick `0x00455A8B → 0x00455400` returns | `0x00455A90` checks `[0x0060CFA4]`; `0x00455A96` goes to owner cleanup `0x00455BF3`, calls `0x004302D0` at `0x00455BF9`, then later `0x004CB0B0`, `0x00472340`, `0x00479E30`; `0x00455C3D` returns | Returns to top-level dispatcher, **not necessarily to the menu or process exit**; static and Windows traces |
| **Controller request 7 after leaving 3D** | Dispatcher `0x00455F24` / `0x00455F56` selects table `0x00456274` entry `0x00456185`; original owner-specific cleanup has happened | Calls `0x00443AD0(0)` and `0x0048A820` at `0x00456198`, then evaluates **bit 4** of `[0x02E0988C]` and signed `[0x00568FB0]` | When bit4=0 and status=-1, observed **RETURN-TO-CONTROLLER** / original main-menu continuation. With separate exit conditions it can reach final cleanup `0x0045620C`. **Both outcomes observed in historical tests**; request 7 alone does not choose them |
| **Final game process exit** | Original main-menu Quit route or native controller final-exit conditions | Full game-owned cleanup, conditional Quit test `0x00455E50` where reachable, and final path `0x0045620C` | Process termination is confirmed only by OS-side verification (process absent/exit code); **window close, movie end or HUD disappearance do not prove this** |

### 9.3 Distinct termination mechanisms and historical development shortcuts

**Normal, game-owned actions:** leaving a Bink player via `0x00470871`; ending a BD cover via its native 300-tick condition; ending a 3D script through the original script manager; leaving gameplay via its pause owner and native 3D cleanup; quitting the **real** main menu by item 3/action 18. They are different scopes of cleanup and **cannot be substituted** for one another.

**Historical F12 process-exit experiments (superseded):** Windows 11 tests established process termination from a New Game comic and movie #4 after deferring the request through original movie/script return and native controller-7 finalization. A ~1-second yellow window appeared in both movie-origin cases. Active 3D F12 finalization did **not** show that yellow window. Another historical test exited with one F12 during an opening video by progressing through original opening-Bink cleanup, the native timed/input-wait screens and real menu Quit; this may briefly flash the main menu. These are **historical experimental** routes, not currently enabled physical F12 shortcuts. See [native shutdown test records](F12_NATIVE_SHUTDOWN.md); none proves the precise graphics cause of the yellow artifact.

**Current project F10 development shortcut:** physical F10 is opt-in via `[Debug] F10Shortcut=1` and is **not** process Quit. Documented Wine results show: opening Bink → original main menu; New Game comic/movie → remaining native Bink and two permission-gated 3D script skips → gameplay; first/second New Game 3D script → only the remaining validated native skips → gameplay; playable 3D → owner cleanup/request 7/controller **RETURN-TO-CONTROLLER** → responsive main menu with the **same PID** and without forcing final-exit bit. Later in-level CutSceneScript is handled only when that script's native permission is present, one script at a time. These tests **do not cover all possible future level-specific script owners**, and the corresponding Windows 11 consolidated F10 behavior remained unverified in the cited matrix. See [F10 validation matrix](F10_VALIDATION_MATRIX.md) and [F10 development history](F12_DEBUG_TEST_MODE.md).

### 9.4 Safety and regression requirements for any new exit hook

1. **Identify the owner** (movie number/caller return, real `Shell_MainMenu`, comic owner, live script context, original 3D tick or gameplay pause owner). A global `mode_request`, HUD appearing, Bink handle existing or elapsed wall-clock time alone is not enough.
2. **Check eligibility**: native skip permission, owner-local timer, video allow-skip flag, actual menu input selection, or native controller return/final-exit conditions. Do not manufacture a universal exit by writing `[0x0060CFA4]=7`.
3. **Let the original code clean up**: Bink `0x0050B790`, startup page object+SEH epilogues, cinematic `CutSceneEndScript`, pause menu destructors, 3D `0x00455BF3` cleanup and the original controller finalizer.
4. **Do not jump between unrelated contexts**: no direct call to menu Quit from a Bink callback; no shortcut from comic draw to an end-of-3D branch; no off-thread native destructors; no fabricated Space, forced `WM_CLOSE` or `TerminateProcess` used as a substitute.
5. **Test transitions distinctly**: each of the four startup movies; first/second still; actual menu; BD cover; movie #4; **each** of the two introductory scripts; later scripted scenes; gameplay → ESC → Retry/Training/Main Menu; gameplay F10 → menu and a second New Game; and true final process exit. Capture game log, validated EXE hash, build/INI, OS, visual rendering, game PID and OS exit status when relevant.

**Research provenance:** [`docs/F12_NATIVE_SHUTDOWN.md`](F12_NATIVE_SHUTDOWN.md), [`docs/F12_DEBUG_TEST_MODE.md`](F12_DEBUG_TEST_MODE.md), [`docs/F10_VALIDATION_MATRIX.md`](F10_VALIDATION_MATRIX.md), [`quick_exit_f12.c`](../quick_exit_f12.c). **Unresolved:** universal exit behavior for all later levels/other cinematic systems, exact visual-frame transition timing, and whether the yellow window comes from a particular DirectDraw/Direct3D surface.

## 10. Known validation and open questions

| Area | Evidence | Boundary |
| --- | --- | --- |
| Original DirectInput assembly branches | Static audit of exact EXE | Runtime Alt+Tab HRESULT and menu polling remain open |
| Two New Game CutSceneScript lifecycles | Wine native traces | Later level-specific engines may differ |
| F10 movie/intro/gameplay path | Wine build #713 (project validation log) | Not a complete native Windows 11 certification |
| Physical Enter/Space release safeguard | Wine build #713 | Root cause of stale Space remains unknown |
| Movie-originated yellow window | Comparative Windows 11 exit observations | Exact graphics surface and timing not determined |
| No-CD, borderless, aspect handling | Compatibility project implementation and historical tests | Never apply to unmatched EXE builds |

Automated tests confirm signatures and code properties; **they do not prove game runtime behavior**. Keep Wine and Windows 11 regressions distinct, and record exact commit/build, INI, original EXE hash, diagnostic log, steps to reproduce and whether the process actually exits or returns to the menu.

## 11. Research references and next steps

Repository sources:
- [DirectInput static audit](../DIRECTINPUT_STATIC_ANALYSIS.md)
- [F10 validation matrix](F10_VALIDATION_MATRIX.md)
- [F12 debugging research (historical)](F12_DEBUG_TEST_MODE.md)
- [Native shutdown and Windows test records](F12_NATIVE_SHUTDOWN.md)
- [Project README](../README.md)
- [External partial decompilation: krystalgamer/spidey-decomp](https://github.com/krystalgamer/spidey-decomp)
- [External CD-check research](https://krystalgamer.github.io/spidey-breaking/index.html)

When adding new reverse-engineering findings:
1. Record precise EXE hash, VA/RVA and relevant instruction bytes or disassembly.
2. Identify whether evidence is **static**, **logged at runtime**, **visually validated**, or merely **hypothesized**.
3. Link the exact test, source file, issue/PR or supporting log.
4. Describe lifecycle owner, preconditions and original cleanup before proposing a hook.
5. Never silently turn an experimental patch or historical result into a currently supported behavior.

---
Research documentation maintained by **legluondunet assisted by ChatGPT (OpenAI)**.
