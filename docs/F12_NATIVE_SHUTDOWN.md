## 2026-10-22 live build #488 trace: mode global is a request, not scene ownership

The user supplied `SpiderMan2k1Fix(20261022-172349).log`, comprising a normal startup, the first real menu, a native comic cover, actual 3D gameplay, and a return toward the 2D menu. There is **no F12 press** in that run. Analysis:

- `0x0060CFA4` remained **0** throughout Bink movies, the idle menu, comic, and the real 3D gameplay (renderer reached gameplay at flip ~900). It changed to **7** only at the transition back toward a 2D menu (flip 1859). The global's exact writer xrefs include `0x0047BE6F`, `0x0044108A`, etc., setting **next top-level dispatch requests** while mode owners are still running. It is **not** a reliable continuously updated "active scene" enum.
- The `0x006B4C9C` field is a countdown, not an async scene identifier: native `0x004E5C10` subtracts `0x10000` while the high word is nonzero, then decrements the low word, giving observed progression `0x1C005A` → `0x1A005A` → ... → `0x00000059` → 0. It produced unhelpful once-per-frame log spam with the earlier trace trigger.
- `0x0054A510` remained **20** in the samples; it is not sufficient as a phase marker.
- `menu_seen=1` is an irreversible "menu has occurred" latch. It is not a signal that the menu is **currently** active.
- Observed graphical transitions in the SAME log are stronger: native `BORDERLESS: MENU exited; windowed gameplay presentation active`, `COMIC-NATIVE-FIX`, and `HUD-ADAPTIVE` messages distinguish entry to comics and 3D. These are renderer hints only, not proof that a scene's cleanup is safe.

Therefore F12 for comic/3D **must not** be enabled using the guessed `0x60CFA4` values alone. New read-only tracer reports the current **renderer2d** flag (existing `menu_upscale_active`), **menu_recent** and **movie_recent** based on already-owned timestamps, and throttles the countdown to active/inactive transitions and five-second heartbeats. Diagnostic labels changed from `mode`/`async` to `mode_request`/`countdown` to prevent incorrect conclusions.

# F12 native engine shutdown: mode-aware design and evidence

**Status: investigation / draft PR #26. Do not merge until explicit user approval and Windows runtime tests.**

This investigation uses ONLY the original, SHA-gated *Spider-Man (2001)* Windows executable:

```text
SpideyPC.exe size: 1,507,328 bytes
SHA-256: 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
Format: PE32 i386
```

All listed virtual addresses (VA) are for this executable. The numeric meanings
of many engine states are **not yet validated**; source-level or disassembly
evidence must not be presented as a runtime result.

## Confirmed Windows 11 behavior

| Game context | Tested behavior | Decision |
|---|---|---|
| Actual interactive main menu | Build #471: F12 activates native Shell_MainMenu Quit (item 3, command 18). User confirmed `SpideyPC.exe` absent in Task Manager | **Keep the existing proven menu hook unchanged** |
| Four opening Bink movies | Build #476: F12 skips the clips, reaching a static pre-menu screen; no full shutdown | Preserve game-owned movie cleanup, fix shutdown handoff separately |
| Two static intro screens | User confirmed two screens after Bink; second requires Enter. Build #483 still stops on first despite logging native Quit flag set | Locate actual screen controller, no simulated Enter or forced window close |
| Comic pages | Not tested for a safe shutdown path | Only observe state |
| 3D gameplay | Not tested for a safe shutdown path | Only observe state |

An earlier direct `WM_CLOSE` destroyed the HWND without killing the rendering/audio
process. It is unsafe to equate **window disappearance** or **last decoded Bink
frame** with a clean engine exit. Only a verified stopped process, preferably
exit code 0 and no Windows crash event, validates closure.

## Verified original binary flow

### Opening and native Bink cleanup

- `0x00455C90`: main startup/controller. Four native movie calls
  `CALL 0x00470750` at `0x00455CF1`, `0x00455CFE`, `0x00455D0C`,
  `0x00455D1A`.
- `0x0047081D` and `0x0047085F`: native Bink tick
  `CALL 0x0050B5A0`.
- `0x00470871`: movie owner executes native Bink close at
  `0x0050B790`; the owner later clears `0x006151F8`. A 0-result
  from the native tick allows this cleanup on the game thread.
- `0x00455D23`: `CALL 0x00443AD0`, the native post-movie transition.
  The function itself contains a conditional call to `0x00470750`,
  so “four calls completed” must not be equated to “all media and
  startup states finished” without an actual runtime checkpoint.

### Top-level controller and why build #483 misses the exit

- `0x0060CF88`: original engine Quit byte.
- `0x00455E50`: `CMP BYTE PTR [0x0060CF88], BL`,
  `JNE 0x004561F2`; `0x004561F2` calls `0x00443AD0(3)`,
  then branches to `0x0045620C` final cleanup.
- **Key discovery:** upstream `JNE 0x00455E5E` branches at both
  `0x00455DC8` and `0x00455DD0` **bypass the native Quit-byte check**
  at `0x00455E50`. Thus the correct log message “Quit flag set”
  in build #483 does **not** prove that the conditional Quit branch
  was ever executed.
- `0x00455F24`: `MOV EAX,[0x0060CFA4]` reads a **pending controller transition request**, not a continuously updated active-scene identifier.
  `0x00455F4C` decrements it; `0x00455F56` jumps via
  `0x00456274`, a table of **11 modes**. The jump destinations are:

| Mode value | Dispatch VA | Label |
|---|---|---|
| 1 | `0x00456100` | Unknown pending runtime correlation |
| 2 | `0x00455F5D` | Unknown |
| 3 | `0x00456045` | Unknown |
| 4 | `0x00456118` | Unknown |
| 5 | `0x00456118` | Unknown |
| 6 | `0x00455FE0` | Unknown |
| 7 | `0x00456185` | Unknown |
| 8 | `0x00455FB0` | Unknown |
| 9 | `0x00455F5D` | Unknown |
| 10 | `0x00456156` | Unknown |
| 11 | `0x00456177` | Unknown |

The table is extracted from the verified executable's `.text` data, not
deduced from screenshots. **These are controller action codes, not
one-to-one scene IDs.** Do not guess which action owns 3D, comic
images, splash screens, or the real menu. A value of zero occurs
throughout several very different scenes.

**Verified action 7 path:** dispatch from `0x00455F56` to
`0x00456185`, calls `0x00443AD0(0)` and `0x0048A820`, then
tests a native bit at `0x02E0988C` and status at `0x00568FB0`.
Depending on those conditions it **either** continues the top-level
loop at `0x00455D98` **or** branches toward cleanup at
`0x0045620C`. Therefore request 7 (seen after gameplay in
build #488) is demonstrably **not a universal “Quit now” command**.
Do not write it as an F12 shortcut without verifying the owner
state and cleanup conditions.

### Where transition requests originate (additional verified x86 writer sites)

The following instructions **write** the global `0x0060CFA4`. They
are candidates for tracing what a scene is *requesting*, not for
invoking its teardown blindly:

| Writer VA | Request value(s) | Verified surrounding disassembly |
|---|---|---|
| `0x0044108A` | 7 | When a tick-counter difference from `[0x006B4CA0]` exceeds `0x78`; nearby owner-local state `[0x005FAED8]` is reset |
| `0x0044174E` | 8 | Conditional branch after a call to `0x004E6560`; no proven scene label |
| `0x00441A92` | 6 | Precedes `CALL 0x00471C50`; no proven shutdown semantics |
| `0x00441D00` | 3 | Follows checks/calls to `0x0043D8C0` and `0x0043D820` |
| `0x0047BE6F`, `0x0047BE7B`, `0x0047BE87` | 7, 10, 8 | Three branches of one routine, selected by comparing the byte at object offset `+0x14` |
| `0x004B28B3` | 2 | Checks `[0x006A7F98]` against `0x78`, writes request, calls `0x0043D800` |
| `0x004B297B` | 8 | Branch after an object method call at `0x004C1460` and a comparison involving offset `+0xE8E` |
| `0x004BD2DC` | 7 | Branch conditional on `[0x0068293C]`; no proven scene label |
| `0x004BE027` | 2 | One return path of an object method; no proven shutdown semantics |
| `0x004E28BE` / `0x004E28CA` | 9 / 3 | Two neighboring paths that lead to `CALL 0x004E65E0` |

Only verified code addresses may be used for future signature-gated hooks.

### Practical consequence for game-mode-specific F12

A **3D gameplay update** is not identified merely because the global
event request becomes 2, 7 or 8; these codes have multiple possible
writers. We must locate the genuine owner of the 3D loop and
observe its normal graceful exit, then separately trace the controller's
final cleanup. Likewise, Bink and comic render callbacks are **activity
markers**, not permission to release COM/video resources from a callback.


### Main menu (already user-validated)

- Actual `Shell_MainMenu` at `0x00493990`.
- Native per-frame call `0x00493DD6`, selected Quit item **3** in table
  `0x00552AA8`, corresponding to command **18**.
- Existing F12 hook runs on the menu thread and takes the native
  selection/cleanup branch `0x004941D5`.
- This success does **not** imply that calling menu cleanup while in 3D,
  Bink or a static splash is safe.

## Agreed architecture

1. Capture F12 once while the real game is foreground and latch an
   **intent**, not a Windows destroy or immediate native destructor call.
2. Locate the **active native mode** (start movies, static screens, actual
   menu, comic pages, gameplay). Do not infer mode from time since last
   decoded frame alone.
3. The mode owner performs its **own** normal exit and resource cleanup
   on the **owning game thread**.
4. Control returns to the top-level engine controller. Only at a verified
   owner-safe point does the engine take its normal final shutdown.
5. A state never safely identified **must fail closed**: ignore F12 with a
   debug log rather than leave a headless renderer or crash.
6. Keep the validated main-menu action exactly as working in build #471.

No `WM_CLOSE`, `DestroyWindow`, `ExitProcess`, `TerminateProcess`,
simulated Enter/menu navigation, or off-thread DirectDraw/DirectSound/Bink
cleanup should be used as the F12 shutdown implementation.

## Diagnostic phase: read-only engine-state instrumentation

The existing `IDirectDrawSurface7::Flip` hook now calls
`SpiderQuickExitF12_ObserveEngineState(count, renderer_menu)` when
`[System] QuickExitF12=1`. It **reads only**:

- `0x0060CFA4`: pending native controller action/request (for dispatch mapping, **not active scene**).
- `0x0060CF88`: native Quit byte.
- `0x006B4C9C`: two-part transition countdown (high-word then low-word decreasing).
- `0x0054A510`: native task/status variable used by the controller.
- The already-owned render policy (`renderer2d`), a recent real
  `Shell_MainMenu` frame (`menu_recent`), a recent decoded Bink
  presentation (`movie_recent`), and the lifetime latch `menu_seen`.
- Existing patch-local observations: intro F12 intent and whether
  post-movie forwarding executed.

It logs only when the pending action, Quit byte, **countdown active/inactive state**, or 2D rendering policy changes, or after five seconds without a snapshot. It does **not** install a new executable machine-code
hook, mutate game-state pointers, or trigger a shutdown.

Example expected log shape (example values intentionally omitted):

```text
QUICK-EXIT-F12: ENGINE-STATE flip=... renderer2d=... mode_request=... countdown=... countdown_active=... task=... quit=... menu_recent=... movie_recent=... menu_seen=... intro_request=... intro_forwarded=...
```

Collect **separate log files** while viewing (a) the first static screen
after movies, (b) the second “Press Enter” screen, (c) the interactive
main menu, (d) comic pages, and (e) actual 3D gameplay. The original
F12 during unsupported modes need not be pressed; passive snapshots
alone are enough to establish the mode/state mapping.

Only after identifying the actual native **scene owner** and its
safe exit/cleanup branch should separate, signature-gated
**mode-owned exit hooks** be implemented and tested individually
on Windows 11. It is specifically **not enough** to map values of
`mode_request` to the currently visible scene.

## Validation gates

- Native mode resource cleanup executes on the correct thread and never
  while COM/Bink callbacks are live.
- Menu F12 user-regression test remains successful.
- Videos stop and audio ends before final engine shutdown.
- Program actually exits; no `SpideyPC.exe` remains.
- External process watcher records an ordinary exit code and checks for
  Windows Error Reporting crash events.
- Comic, static-image and 3D pathways pass **separate** live tests.
- PR remains a draft and is merged only after user approval.

Credit: **legluondunet assisted by ChatGPT (OpenAI)**.
