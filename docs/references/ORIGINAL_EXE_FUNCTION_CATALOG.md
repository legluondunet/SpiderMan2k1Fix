# Recovered original `SpideyPC.exe` function catalog

**Purpose:** a concise, searchable index of identified **original-game** routines and instruction sites, recovered during SpiderMan2k1Fix reverse engineering. This is **not** a complete decompilation or source reconstruction. The symbolic names below are **descriptive analyst labels**, not claimed retail symbol names or proven C prototypes.

## Evidence rules and address scope

- **Binary:** unchanged retail Windows PC `SpideyPC.exe`, PE32 x86, 1,507,328 bytes, image base `0x00400000`; SHA-256 `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
- Unless specified as an **RVA**, every address below is an original executable **VA**. `RVA = VA - 0x00400000`, but **file offsets differ from RVAs**.
- **S** = original x86 instruction/disassembly or source-described static signature; **R** = behavior also observed in recorded Windows 11 or Wine runtime diagnostics; **I** = reasonable interpretation only. These markers apply to the cited **role**, not a full recovered function body.
- The native code has not been independently disassembled anew for this audit. This index is consolidated from existing historical diagnostics and the source's exact-signature guards. Some archived notes describe experiments **superseded** by current behavior.
- Original executable functions are distinct from `SpiderMan2k1Fix`'s newly written hook/proxy/INI functions. Do **not** copy original game code or assets into the repository.

Primary sources: [DEVELOPMENT.md](../DEVELOPMENT.md), [original native exit disassembly and process tests](F12_NATIVE_SHUTDOWN.md), [historical game-script/input research](F12_DEBUG_TEST_MODE.md), [DirectInput static audit](diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md), [original ESC 4:3 analysis](NATIVE_PAUSE_MENU_43.md).

## 1. Startup, disc check and real main menu

| Original VA / callsite | Analyst-described role | Evidence / limits |
| --- | --- | --- |
| `0x005163E0` | Original game disc-check **function entry**. Callers `0x005159AC` and `0x00515D85`. | S, original entry/CALL signatures verified; **internal drive/file probes and return-value semantics not exhaustively decoded**. The proxy's early 3-byte return patch is **our code**, not a recovered native function. |
| `0x00455C90` | Native startup/main-controller entry. | S+R. Owns four startup calls to shared movie player at `0x00455CF1`, `0x00455CFE`, `0x00455D0C`, `0x00455D1A`. |
| `0x00443AD0` | Game-owned post-movie/transition routine; invoked from startup at `0x00455D23`. Can play an additional movie at `0x00443BE7`, from pending ID `[0x0054F570]`. | S. **Not** a universal `CleanupEverything()`; caller and argument matter. |
| `0x00455F24`, `0x00455F56` | Native read/dispatch of **pending transition request** `[0x0060CFA4]` using jump table `0x00456274` for requests 1–11. | S. A request value is **not a continuously updated scene ID**. |
| `0x00455E50` | Original quit-byte test on `[0x0060CF88]`. | S. Startup branches at `0x00455DC8` / `0x00455DD0` **can bypass the test**; writing the byte alone cannot promise termination. |
| `0x00493990` | Original interactive `Shell_MainMenu` owner. | S+R, actual callback `0x00493DD6` observed. Presence of a menu-context pointer does not alone prove interactive menu entry. |
| `0x00493DD6 → 0x00505720` | Native **per-main-menu frame callback**, after original input polling. | S+R; used as a safe observation/session-reset boundary by the fix, not an invented menu function. |
| `0x004941D5` | Native **menu Quit action 18**, option slot 3, normal menu resource/selection cleanup. | S+R, historical Windows process-exit tests. **Only valid within the real menu owner**; not a callable process-exit API. |
| `0x004E5D60` | Original game-clock wait routine, checking clock `[0x006B4CA0]`. | S. Its calls at `0x0049415B`, `0x0049416F`, `0x00494209` explain menu-loop timing; all ~4s stalls cannot conclusively be attributed to this alone. |
| `0x004A1930` | Native **timed opening still-page** loop and its object/SEH cleanup; called at `0x004A1FE7`. | S + Wine stage correlation. Uses clock `[0x006B4CA0]`, frame counter `[0x0060CFA8]`, one-time marker `[0x006A7785]`. |
| `0x004A17A0` | Native **input-wait opening still-page** loop and its object/SEH cleanup; called at `0x004A2006`. | S+R. Original input call at `0x004A1896 → 0x0050C180`; accepted completion reaches native cleanup at `0x004A18AE`. The displayed-image identity is correlated, not extracted from code. |

## 2. New Game comic, Bink video and scripted cinematics

| Original VA / callsite | Analyst-described role | Evidence / limits |
| --- | --- | --- |
| `0x004E0461 → 0x00427910` | Scene interpreter invokes the New Game comic/image owner. | S, correlated with New Game runtime traces. |
| `0x0042796D → 0x004151D0` | Passes original literal **`L1COV.bmp`** at `0x00549214` to image setup. | S. The exact signature identifies this cover, not every game image. |
| `0x00427983`, `0x004279B8` | Comic's original initial-clock save / later clock read. | S+R. At `0x004279BE` / `0x004279C0` / `0x004279C6`, elapsed **300 native ticks** determines transition, not 300 guaranteed milliseconds/frames. |
| `0x004279FB → 0x00470750` | New Game comic owner starts native **Bink movie ID 4** (`push 4` at `0x004279F9`), returns at `0x00427A00`. | S+R. The large value in an early movie-ID log was an overwritten **float temporary**, **not** the movie ID. |
| `0x00470750` | **Shared native Bink playback owner**; original script audit found **30 direct CALLs**, including four startup callers, optional `0x00443BE7`, UI caller `0x0049D654`, and 24 scripted callsites in `0x004279FB`–`0x00427D8A`. | S+R. **Not unique to New Game** and a Bink tick is **not** itself a process exit. |
| `0x0050B5A0` | Bink frame-tick routine called at `0x0047081D` and `0x0047085F` inside the video owner. | S+R. Early video skip can occur **between** those tick callsites without a tick returning finished. |
| `0x0050C180` | Original game-wide input-mask query, used for Bink skip and other menu/script conditions. | S. Video owner calls at `0x00470853` with **`0x07000000`** mask = Enter/Escape/Space, only after caller's allow-skip flag and at least **60 iterations**. |
| `0x0050A650` | Native DIK-key query used by `0x0050C180` to test keys. | S. Do not confuse DirectInput scan codes with Windows virtual keys. |
| `0x00470871 → 0x0050B790` | Native **video cleanup**: closes game-owned Bink handle `[0x00AC0BA4]`, releases related resources and clears it. | S+R. Natural-end and allowed-key skip converge here; cleanup continues before the caller resumes. |
| `0x00440920` | Original post-video transition called after Bink teardown at `0x0047089F`. | S, precise broader role not recovered. |
| `0x004E0466`, `0x004E2B3F` | Original scene-interpreter operand advance / next-opcode dispatch **after Bink returns**. | S. Explains why simply ending Bink resumes the **next scene**, not a clean process quit. |
| `0x004E1485 → 0x004BC1A0` | Interpreter start of original **`CutSceneScript`**: establishes manager context, active flag, instruction pointer and resets skip permission. | S+R, positively observed in two **separate** New Game 3D scripts. |
| `0x004E14A5` (also `0x004E14C3`, `0x004E14D9`) | Original **`CutSceneSkipAllow`** script events; update manager permission at offset `+0x1A4`. | S+R. The two introductory scripts independently grant/deny permission. |
| `0x004C6654`, `0x004C665C` | Script-manager context and skip-permission checks. | S; these are **instruction sites**, not separately identified functions. |
| `0x004C66A6 → [0x0060CFC4]` | Game-owned **one-byte script-skip request** writer. | S+R. `0x004BC3E0` consumes request; `0x004BC3EF` clears active script. |
| `0x004BC4A2`, `0x004BC4AF` | Native **`CutSceneEndScript`** flow; `0x004BC17E` clears skip request for the next script. | S+R. **No permission to force unrelated scripted engines** from these addresses. |
| `0x004DE890`, `0x004DE8B0`, `0x004DE8D0` | Original event-list **clear/test/search** routines (head `[0x006B4708]`, hashed buckets `[0x006B4214]`). | S. Event-node layout was partially recovered; **the full event list and every event semantic were not**. |

## 3. DirectInput, gameplay update, ESC and exit

| Original VA / callsite | Analyst-described role | Evidence / limits |
| --- | --- | --- |
| `0x0051672C` / `0x004FE0E4` | Original `DirectInput8Create` import stub / caller; IAT `0x0053B01C`. | S, original imported code is external `dinput8.dll`, **not a game function**. |
| `0x005015A8`, `0x00501728` | Original keyboard/mouse `CreateDevice` **call sites**, pointers `[0x006B7A5C]` and `[0x006B7A64]`. | S; device GUIDs and COM method slots mapped in [DirectInput audit](diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md). |
| `0x00501BC3`, `0x00501D02` | Keyboard/mouse native `GetDeviceData` **call sites**. | S + Windows 11 WinDbg observed **`DIERR_NOTACQUIRED`** in both. The game explicitly handles `DIERR_INPUTLOST`; an initial `NOTACQUIRED` may bypass native `Acquire`. This is not proof the optional retry fixes all Alt+Tab failures. |
| `0x00501BD9`, `0x00501D18` | Native keyboard/mouse **`Acquire` call sites**. | S. They are COM calls, not distinct recovered x86 `Acquire` implementations. |
| `0x00501CB0` | Original buffered keyboard reader, with DIK state table `[0x006B792C+DIK]`. | S+R; key-down/up buffer writes at `0x00501C87`/`0x00501C8F`. |
| `0x00505720 → 0x0050A190` | Original per-frame input/action update and key-to-action conversion; mapping array `0x00568398`. | S+R. |
| `0x005058A5 → 0x00479540` | Original **action `0x10`** handling; in a tested keymap Space maps to that action (jump in gameplay, may skip active scripts). | S+R. Action `0x10` alone **does not prove cinematic ownership**. |
| `0x00440EF0` | Original **3D game/input update** routine, called at `0x0045553B` from native frame `0x00455400`. | S+R; observed its execution produced a request-7 transition in Windows 11. Attribution to its specific internal writer instruction `0x0044108A` remained **unproven** in the observed run. |
| `0x00455400` | Original 3D frame/update owner, called at `0x00455A8B`. | S+R. It also checks pause context `[0x00682950]` before native ESC handler. |
| `0x0047BC40` | Original **ESC pause option handler**, called at `0x00455549` if context is active; menu `[0x00682968]`. | S. Option index in menu object `+0x14` maps **0 retry / 1 training / 2 main menu**, producing controller requests **8 / 10 / 7** respectively. |
| `0x0047BEA0`, `0x0047BEB6` | Original pause menu **virtual destructor call sites**, releasing menu objects. | S. Do not call out-of-context from a patch thread. |
| `0x004302D0` | Original **3D per-mode cleanup checkpoint**, called at `0x00455BF9` after nonzero native request triggers owner exit at `0x00455BF3`. | S+R Windows. Further original owner cleanup occurs at `0x00455C12`, `0x00455C2F`, `0x00455C34` before return at `0x00455C3D`. |
| `0x00456185` | Top-level **controller request-7** branch after 3D ownership ends. | S+R. **May return to controller OR exit process**, depending on native flags/status; request 7 is **not** synonymous with Quit. |
| `0x0048A820` | Original controller **post-mode cleanup/finalization** called at `0x00456198`. | S+R; native state tested at `0x0045619D`, `0x004561A6` can choose loop `0x00455D98` or final `0x0045620C`. |
| `0x0045620C` | Game's **final process cleanup path**. | S+R historical process-level observations. A patch must never jump directly here from a nested Bink/script callback. |
| `0x005152B0` / `0x00515373` | Original registered WndProc / its unsafe **F12 → `PostMessageA(WM_CLOSE)`** call site. | S. Current fix suppresses the original call; historical F12 diagnostic experiments **do not** mean physical F12 is currently enabled. |

## 4. Rendering, screenshots and original game settings

| Original VA / RVA | Analyst-described role | Evidence / limits |
| --- | --- | --- |
| **Caller RVA** `0x00102C72` (VA `0x00502C72`) | Shared original D3D7 primitive-call path through which game-world sprites, HUD, comic and native ESC elements pass. | S+R. **Not an exclusive HUD function**: different texture, depth, caller and state signatures are essential. |
| **Caller RVA** `0x00103389` (VA `0x00503389`) | Original menu/title D3D7 render call context. | S. Distinct from gameplay shared caller. |
| `0x00507910` | Native sprite/rectangle drawing **callee** from multiple scoped player HUD callsites. | S+R, [HUD callsite inventory](../DEVELOPMENT.md#71-original-player-hud-call-site-inventory-16-sha-pinned-hooks); **not** a unique player-HUD routine. |
| `0x00507470` | Native shared glyph/bar draw **callee**. | S+R; semantics known from the original targeted CALLs and original rendering signatures, not a full source reconstruction. |
| `0x00462FB0`, `0x00462D60` | Native rectangle and bar **routines** used from specific HUD callsites. | S+R; internal drawing outside the bounded HUD scope must remain unchanged. |
| `0x0050DD19` | Original **Display option-selection branch site** (screen size, color depth, brightness). | S. A code site, **not** a proven function entry; the patch's 32-bpp policy is new behavior. |
| **RVA** `0x00150064` (VA `0x00550064`) | Game-owned **aspect/projection float** originally `1.0f`. | S, **data**, not a `SetFOV(degrees)` function. Complete native camera/FOV calculation and degree calibration remain unproven. |
| `0x005033E0` | Original screenshot **BMP writer**. | S + observed magenta PRIMARY-surface failure on Windows. The patch redirects its original calls to a different PNG implementation. |
| **RVA** `0x0010A1DC`, `0x0010A62B` | Original **F9 screenshot call sites** (VAs `0x0050A1DC` / `0x0050A62B`). | S. They are code sites, not two different screenshot functions. See [F9 analysis](F9_PNG_CAPTURE.md). |
| `Spidey.cfg` raw **252-byte layout** | Original resolution/brightness, eleven DIK bindings, seven joystick button bindings, audio and stereo fields. | S + project INI translation. See [recovered byte offsets](../DEVELOPMENT.md#86-recovered-binary-layout-of-spideycfg-252-bytes). **Joystick directional bytes `0x40`–`0x4C` are undecoded**; don't assign guessed meanings. |

## Unresolved reverse-engineering questions

1. The original **disc-check function's complete body**, every original status/failure path, and any other retail-EXE variant are not reconstructed.
2. Full **Bink/movie owners outside the identified 30 callsites**, later in-level cinematic engines, all scripts, and complete event-list semantics are not mapped.
3. The original **FOV/camera pipeline** is not recovered as a calibrated `SetFOV` function.
4. Game input state `[0x0060CFA4]` is a **request**, not a reliable live-scene identifier; not every one of its writers has a recovered owner.
5. The **yellow window** during some historical movie-originated process exits was observed, but its exact DirectDraw/Direct3D frame producer remains unidentified.

For reliable working notes, use [full reverse-engineering synthesis](../DEVELOPMENT.md) and the [F12 exit preservation summary](F12_EXIT_ARCHIVE_SUMMARY.md). Keep symbolic labels, instruction callsites, runtime observations and inferences separate.
