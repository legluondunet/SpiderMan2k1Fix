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
- The supported in-memory No-CD patch avoids the game disc check. Historical diagnostic records identify a CheckCD tracing hook at **RVA `0x1163E0`**; this must not be confused with a universal patch point for arbitrary EXEs.
- Display compatibility uses borderless fullscreen at desktop resolution, with widescreen/4:3 modes. Game-native and DLL-owned settings are consolidated in `SpiderMan2k1Fix.ini`.
- A default 30 FPS cap preserves game timing; historical Scorpion encounters motivated avoiding high frame rates.
- The project has investigated original DirectDraw/Direct3D 7 presentation, Bink movies, native input ownership and script cleanup. The game is not fully decompiled.

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

The original window procedure assigns the activation flag at `0x005152F8`, in the `WM_ACTIVATE = 0x0006` case. The static findings therefore suggest a **possible** failure to reacquire on `DIERR_NOTACQUIRED` after Alt+Tab. This is **not proof** that this is the failing HRESULT in every real incident, that the menu continues polling, or that a particular retry fixes the bug.

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


## 8. Known validation and open questions

| Area | Evidence | Boundary |
| --- | --- | --- |
| Original DirectInput assembly branches | Static audit of exact EXE | Runtime Alt+Tab HRESULT and menu polling remain open |
| Two New Game CutSceneScript lifecycles | Wine native traces | Later level-specific engines may differ |
| F10 movie/intro/gameplay path | Wine build #713 (project validation log) | Not a complete native Windows 11 certification |
| Physical Enter/Space release safeguard | Wine build #713 | Root cause of stale Space remains unknown |
| Movie-originated yellow window | Comparative Windows 11 exit observations | Exact graphics surface and timing not determined |
| No-CD, borderless, aspect handling | Compatibility project implementation and historical tests | Never apply to unmatched EXE builds |

Automated tests confirm signatures and code properties; **they do not prove game runtime behavior**. Keep Wine and Windows 11 regressions distinct, and record exact commit/build, INI, original EXE hash, diagnostic log, steps to reproduce and whether the process actually exits or returns to the menu.

## 9. Research references and next steps

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
