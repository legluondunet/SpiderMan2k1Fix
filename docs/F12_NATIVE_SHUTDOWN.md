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

### Native in-game pause menu: first proven game-owned route out of 3D

Further disassembly of the supported `SpideyPC.exe` identified the actual
in-game pause option routine. This is more precise than a generic
`mode_request=7` heuristic.

1. `0x00455400` is a game frame/tick function; it updates other
   world/game input state and invokes `0x00440EF0` at `0x0045553B`.
2. It reads `[0x00682950]` at `0x00455540`; **only if nonzero** does
   it call `0x0047BC40` at `0x00455549`. This gates execution of
   the game's actual pause option handler, on the current game thread.
3. `0x0047BC40` creates/updates the pause menu object
   `[0x00682968]`; the menu text is loaded from original game
   pointers `[0x0054BA94]`, `[0x0054BA98]`, `[0x0054BA9C]`:
   **"retry", "quit to training", "quit to main menu"**.
4. After the normal input confirmation (`0x0047BDAE` or
   `0x0047BE1B` branches), `0x0047BE62` reads the native option
   index byte from `[menu+0x14]`, and dispatches:

| Native option index | User-visible menu text | Instruction | Transition request at `0x0060CFA4` |
|---|---|---|---|
| 0 | retry | `0x0047BE87` | 8 |
| 1 | quit to training | `0x0047BE7B` | 10 |
| 2 | **quit to main menu** | `0x0047BE6F` | **7** |

5. **The same game-owned routine** then destroys both menu objects
   when present (virtual destructors at `0x0047BEA0` and
   `0x0047BEB6`), clears pointers `0x00682960` and
   `0x00682968`, and returns naturally.
6. The top-level controller processes request 7 at `0x00456185`:
   it calls native `0x00443AD0(0)` and `0x0048A820`,
   checks `[0x02E0988C]&4` and signed `[0x00568FB0]`,
   then **either** loops or exits. Importantly, **"quit to main
   menu" is not identical to "terminate the process"**.

**Design implication:** The game already offers a real, mode-owned
mechanism to leave 3D gameplay and perform its normal pause/transition
cleanup. Future F12 can potentially use this *owner* and its genuine
cleanup sequence, then request a separate native top-level shutdown.
But F12 must not merely write the global request 7 or call the
menu's destructor out of context. Native mode exit and final program
exit are distinct phases; both need Windows validation.

**Diagnostic probe update after Windows build #496:** The uploaded
`SpiderMan2k1Fix(20261022-175550).log` shows genuine 3D gameplay
(renderer2d=0 and HUD), return to menu rendering, native pending
request `7` during that transition, followed by a successful call
to the already proven native menu Quit action 18. Yet
`pause_ctx=0`, `pause_options=0`, `pause_restart=0` at **every
DirectDraw Flip snapshot**, including the 3D frames. These
snapshots **cannot confirm or rule out** that a pause selection occurred:
the native owner can create/use/free its objects between Flip calls;
moreover, request 7 has several separate writer sites.

To resolve this ambiguity, the new diagnostic installs a
**signature-checked pass-through CALL wrapper** at `0x00455549`
(original `E8 F2 66 02 00`) on the *game's own thread*.
The wrapper invokes the original `0x0047BC40` exactly once and
observes **only** entry, native request before/after, and object
presence before/after. It never chooses the menu item, writes the
transition request, sends keyboard input, or invokes native
destructors itself.

Expected log markers **only if the actual native pause owner executes**:

```text
QUICK-EXIT-F12: NATIVE-PAUSE owner ENTER n=1 ...
QUICK-EXIT-F12: NATIVE-PAUSE owner EXIT n=... native request 7 from pause; ctx=0 options=0
```

If these are absent despite the menu visually appearing, the
`0x0047BC40` owner was not reached, the game uses another pause
path, or the logger missed the call. The values are diagnostic,
**not proof of full-process shutdown**. F12 during 3D remains disabled
pending separate user tests. Unlike the Flip sampler, this test
does add one signature-verified native instruction patch and
therefore must be regression-tested before merging.

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

## Build #500 Windows diagnostic: pause owner was NOT observed

User log `SpiderMan2k1Fix(20261022-180341).log`, on the current
Windows x86 build, shows native 3D rendering with HUD, a transition
`mode_request=7` and then the validated Shell_MainMenu Quit action
18 after F12. **There are no NATIVE-PAUSE owner ENTER or EXIT markers**,
even though the signature-gated pause-handler call probe was installed.
The three passive pointer-presence indicators also remained zero.

This rules out the particular instrumented
`0x00455549 -> 0x0047BC40` caller as the observed source of
transition 7 for this run. It does **not** prove that no pause menu
was shown, nor that every pause implementation is unused. The
controller transition request 7 has multiple distinct native writers.

### Second, independently identified game-input owner

Disassembly of the SHA-verified executable shows the per-tick sequence:

```text
0x00455536 CALL 0x00430880
0x0045553B CALL 0x00440EF0    ; E8 B0 B9 FE FF
0x00455540 MOV  EAX,[0x00682950]
0x00455545 TEST EAX,EAX
0x00455547 JE   0x0045554E
0x00455549 CALL 0x0047BC40    ; E8 F2 66 02 00
0x0045554E CALL 0x00458940
```

`0x00440EF0` includes a **different writer** at
`0x0044108A` of native controller request `7`. That writer
is guarded by bytes `[0x006611F0]`, `[0x006611E0]` and a
`>0x78` comparison of the elapsed game tick counter relative to
`[0x005FAEDC]` (with arming state at `[0x005FAED8]`).
This makes it a plausible but **not yet confirmed** source of
the user-observed return to the menu.

The new opt-in diagnostic intercepts **only the existing original
gameplay CALL** at `0x0045553B`, after exact signature verification.
Its wrapper calls `0x00440EF0` exactly once with the original
no-argument cdecl ABI. It logs first entry and when the original
call itself changes the transition request from another value to
7. It also records, read-only, the arming state and two input flags
at the call boundary. No game state or native input is changed.

Expected evidence for this distinct route:

```text
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner ENTER n=1 ...
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner EXIT n=... NATIVE REQUEST 7 produced by 0x440EF0 ...
```

If a request-7 transition occurs with neither native owner marker,
we must investigate further native writer sites rather than
assuming that menu selection invoked one of the monitored owners.
This remains a **diagnostic only**; F12 gameplay exit is still
disabled, and the previously validated main-menu F12 is unchanged.

## Windows build #503 validation: actual 3D input owner confirmed

The user supplied `SpiderMan2k1Fix(20261022-181331).log`.
It explicitly confirms the *game-owned* `0x00440EF0` routine
generated `mode_request=7` during native 3D gameplay:

```text
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner ENTER n=1 request=0 timer_active=0 flag1=0 flag2=1
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner EXIT n=358 NATIVE REQUEST 7 produced by 0x440EF0; timer_before=0 flag1=0 flag2=1
QUICK-EXIT-F12: ENGINE-STATE flip=635 renderer2d=1 mode_request=7 countdown=1704026 ...
QUICK-EXIT-F12: F12 queued for REAL Shell_MainMenu (0x493DD6)
QUICK-EXIT-F12: native Shell_MainMenu Quit activated: slot=3 action=18 ...
```

No `NATIVE-PAUSE` owner marker appeared. This identifies **the
observed 3D-to-menu transition's origin**, not an untested game-wide
F12 shutdown path.

### Verified native 3D loop handoff and cleanup

The relevant original executable control flow is:

```text
0x00455A8B CALL 0x00455400    ; original 3D update/frame
0x00455A90 CMP  [0x0060CFA4], 0
0x00455A96 JNE  0x00455BF3    ; native 3D-owner cleanup on pending request
... otherwise normal gameplay loop ...
0x00455BF3 MOV  [0x0056FC74], 0
0x00455BF9 CALL 0x004302D0    ; original per-mode cleanup checkpoint
... conditional native audio/engine cleanup ...
0x00455C12 CALL 0x004CB0B0
0x00455C17 MOV  EAX,[0x0060CFA4]
0x00455C2F CALL 0x00472340
0x00455C34 CALL 0x00479E30
0x00455C3D RET                 ; hands control back to top-level dispatcher
```

The **actual, game-owned** input function `0x00440EF0` writes
controller request 7 at `0x0044108A`, only after:

- Both game-owned input flags `[0x006611F0]` and `[0x006611E0]`
  are nonzero when checked inside this native function.
- If inactive, it arms `[0x005FAED8]` and records tick counter
  `[0x006B4CA0]` into `[0x005FAEDC]`.
- It later compares elapsed ticks with `0x78` (**120 ticks**), and
  requests 7 when elapsed ticks are strictly greater than 120.
  The input owner also updates a stack-local transition marker.
- The first "enter" snapshot showed flag1=0, flag2=1.
  The later request-7 snapshot reports those same **pre-call**
  values; native flags can change *during* `0x440EF0`, so the
  pre-call zeros cannot disprove the conditional gate.

**Critical safety constraint:** We must **not** assume writing
`mode_request=7` from an F12 worker thread is equivalent to
activating this native path. Its owner also manages elapsed-tick
state and an internal transition marker. The 3D loop's own
native cleanup must be observed before any F12 extension is tried.

### Additional opt-in, PASS-THROUGH diagnostic (no F12-in-3D yet)

At `0x00455BF9`, the original 3D owner calls `0x004302D0`
with the exact CALL bytes `E8 D2 A6 FD FF`. The new
signature-gated wrapper executes that original function **exactly
once** on the game thread and logs entry / successful return:

```text
QUICK-EXIT-F12: NATIVE-3D-CLEANUP begin n=1 request=7
QUICK-EXIT-F12: NATIVE-3D-CLEANUP completed n=1 request=7
```

The game-input observer additionally reports **post-call**
values of both input flags and timer arming state when the
native request-7 transition occurs, to verify its actual
conditional gate. It never mutates either flag, the timer,
or the pending controller request.

Passing these diagnostics still does **not** prove that invoking
native Quit during live 3D gameplay is safe, or that program
shutdown can omit the real main menu. We must validate the
game owner's native cleanup first.

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
- Three game-owned pause-menu object pointers by **presence only**:
  `[0x00682950]` (pause context), `[0x00682968]` (option menu),
  and `[0x00682960]` (restart/selection menu), with log labels
  `pause_ctx`, `pause_options`, `pause_restart`.
- Existing patch-local observations: intro F12 intent and whether
  post-movie forwarding executed.

It logs only when the pending action, Quit byte, **countdown active/inactive state**, or 2D rendering policy changes, or after five seconds without a snapshot. It does **not** install a new executable machine-code
hook, mutate game-state pointers, or trigger a shutdown.

Example expected log shape (example values intentionally omitted):

```text
QUICK-EXIT-F12: ENGINE-STATE flip=... renderer2d=... mode_request=... countdown=... countdown_active=... task=... quit=... pause_ctx=... pause_options=... pause_restart=... menu_recent=... movie_recent=... menu_seen=... intro_request=... intro_forwarded=...
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
