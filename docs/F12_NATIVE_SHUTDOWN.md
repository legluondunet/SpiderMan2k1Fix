## 2026-10-23 — Opt-in, one-shot runtime attribution of New Game native Bink parent

The verified original EXE contains **30 different CALL sites** for
native `0x470750`. Static inspection cannot uniquely attribute the
post-menu `New Game` cinematic to one of them. To resolve this without
30 additional hooks, a **strictly opt-in diagnostic** now uses the
two *already installed* native Bink frame-tick wrappers at
`0x47081D` and `0x47085F`, once after the interactive menu.

Enable only for the Windows test in PowerShell, with
`[System] QuickExitF12=1` and `[Debug] Log=1`:

```powershell
$env:SPIDERMAN2K1FIX_F12_PARENT_TRACE = "1"
.\SpideyPC.exe
```

Then select `New Game`, let the comic still disappear, press Space
during the cinematic, and (if native 3D works normally) F12 in
3D. Inspect `POST-SHELL-BINK-PARENT` once, especially
`native_tick_return`, `parent_return`,
`known_original_site`, `movie_id` and `allow_skip`.

The x86 owner at `0x470750` saves ESI and EBX before both
frame-tick CALL instructions. The instrumented wrapper uses
GCC `__builtin_frame_address(0)` to obtain *its own*
frame pointer on entry (not an unwinder). It first checks the
expected original tick-return address `0x470822` or
`0x470864`; **only if matched** does it read the video
owner's caller return address and first two arguments
from the same already-established stack. The parent return
is compared to the complete static whitelist of **30 exact
return addresses**. All numbers are observations; no return
address or game-global is overwritten. If the layout is
unexpected, no deeper stack read occurs.

This one-time diagnostic is **off by default**, requires a
separate environment switch and Debug Log=1 in addition to
the existing QuickExitF12 option, and makes **no new game
hook, native input invocation, Bink tick invocation or F12
exit action**. A whitelisted caller still does *not* mean
we have a safe post-video exit checkpoint. Only real
Windows/Wine execution can validate the traced stack layout;
signature gate and passive runtime log provide safe failure
without inferring scene ownership.

To clear the environment switch:
`Remove-Item Env:SPIDERMAN2K1FIX_F12_PARENT_TRACE`.

---

## 2026-10-23 — Native callers and top-level dispatcher: static control-flow audit

The uploaded original `SpideyPC.exe` (SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`)
was statically disassembled again with `objdump -d -M intel`.
There are exactly **30 direct `call 0x470750`** instructions
in the original `.text` image, grouped into:

| Caller sites, original VAs | Count | Caller behavior established |
|---|---:|---|
| `0x455CF1`, `0x455CFE`, `0x455D0C`, `0x455D1A` | 4 | Top-level startup's four pre-menu movies; then `0x455D23: call 0x443AD0` |
| `0x443BE7` | 1 | Nested inside `0x443AD0`; optional movie ID read from `[0x54F570]`, guarded against `0xFFFFFFFF` |
| `0x49D654` | 1 | UI/sequence loop; after the movie, `test al,al; sete byte ptr [esp+0x13]`, then branches back to its own control loop at `0x49D34E` |
| `0x4279FB` through `0x427D8A` | 24 | Switch/table-driven cinematic/event routines; use diverse numeric movie IDs and return to their own callers |

Specific parent evidence for `0x443BE7`:

```asm
00443BB9 mov eax,[0x54F570] ; pending video identifier
00443BC4 cmp eax,-1
00443BDB je  0x443BEF     ; no video requested
00443BDD push 0x3F800000
00443BE2 push 1
00443BE4 push 1
00443BE6 push eax
00443BE7 call 0x470750
00443BEC add  esp,0x10
00443BEF mov  dword ptr [0x54F570],0xFFFFFFFF
00443BF9 call 0x479B70
00443C01 ret
```

This establishes that `0x443AD0` **can** own optional video
playback and continues its own cleanup/state reset after the
movie owner returns. It does **not** prove that this is the
particular caller reached by New Game in the Windows log.
Other original callers also use `0x470750`, so a global
F12 shortcut applied to its return is not safe.

Important top-level engine control details verified from the
same EXE:

- `0x455F24` loads next-dispatch request from `[0x60CFA4]`;
  `0x455F56` dispatches through the native jump table at
  `0x456274` for requests 1–11.
- Request **7** maps to `0x456185` (the already investigated
  original engine teardown checkpoint), then executes
  `0x456198: call 0x48A820` and `0x45619D` condition/exit
  handling; only the existing validated 3D F12 approach
  currently establishes the prerequisite final-shutdown state.
- The top-level native Quit test `0x455E50` reads the quit
  byte `[0x60CF88]` and jumps to `0x4561F2` if set.
  Critically, this test is on the **startup/initialization
  entry path**, while the later dispatcher loop returns via
  `0x455F19` / `0x455D98`; simply setting the startup
  Quit byte while a *post-menu* cinematic is nested within
  the dispatcher does not establish that the engine will
  read it before starting 3D gameplay.
- The common final engine teardown enters at `0x45620C`,
  where there are additional game-owned resource releases.
  Jumping directly there from a nested video callback
  would bypass the caller's stack unwinding and cleanup,
  so is **not** an acceptable F12 strategy.

**Crucial correction to the previous static-disassembly entry:**
the second argument (native `allow-skip`) is `1` for **all
four startup movies as well as `0x443BE7`**, not `0` as
previously stated. The `0x470826` access to `[esp+0x10]`
occurs after `push ebx`, so it addresses the second
original C argument. This is established directly by the
original push instructions.

**Conclusion:** Native keyboard skip and native Bink cleanup
are verified, and the caller-dependent post-video flows
are mapped. **A clean F12 post-video Quit is not yet
proven or implemented.** Next diagnostic must establish
which of the 30 native callers actually owns the specific
New Game cinematic, and identify a game-thread exit
handoff **after the matching parent has run its cleanup**,
not via arbitrary `[0x60CFA4]` writes, direct BinkClose,
direct return-address rewrites, `ExitProcess`, or a
blind jump to `0x45620C`. No game behavior changed in
this documentation-only audit.

---

## 2026-10-23 — Verified native cinematic Space skip and Bink teardown (original EXE)

**Static disassembly of the user-provided original `SpideyPC.exe`:**
SHA-256 `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`,
PE32 x86, image base `0x00400000`, identical to the
supported EXE signature. The binary was **read-only**. Addresses
below are original **VAs**, not RVAs (subtract `0x00400000` for
an RVA).

The outer original playback routine starts at `0x00470750`.
The two existing frame-tick patch sites are
`0x0047081D` and `0x0047085F`, each normally calling
`0x0050B5A0`. Contrary to a simplistic "Bink returned
finished" interpretation, **the ordinary skip branch occurs
between these two calls**:

```asm
00470826  mov bl, [esp+0x10]        ; caller's allow-skip argument
0047082A  mov esi, 0x3c             ; 60 iterations
0047082F  inc dword ptr [0x6150F4] ; iteration counter
00470835  call 0x505720            ; input update, returns 0 in original routine
0047083E  test bl, bl
00470840  je   0x47085F            ; skip disabled for this video
00470842  cmp  dword ptr [0x6150F4],esi
00470848  jl   0x47085F            ; ignore skip until 60 iterations
0047084A  push 1
0047084C  push 1
0047084E  push 0x07000000         ; accepted keys
00470853  call 0x50C180           ; engine keyboard input query
0047085B  test al,al
0047085D  jne  0x47086A            ; native keyboard-driven skip
0047085F  call 0x50B5A0           ; original next Bink tick
00470864  test al,al
00470866  jne  0x47082F
00470868  jmp  0x470871           ; ordinary Bink completion
0047086A  mov  byte ptr [0x6611E1],0
00470871  call 0x50B790           ; original Bink/video teardown
00470895  mov  dword ptr [0x6151F8],0
0047089F  call 0x440920           ; original post-video transition
004708A5  mov  al,1               ; successful video owner return
004708A8  ret
```

The `0x50C180` input query implements the particular mask
`0x07000000` as OR-ed tests of DirectInput scan codes:

```asm
0050C418 test ebx,0x01000000
0050C421 push 0x1C             ; Enter
0050C423 call 0x50A650
0050C42D test ebx,0x02000000
0050C436 push 0x01             ; Escape
0050C438 call 0x50A650
0050C442 test ebx,0x04000000
0050C44B push 0x39             ; Space
0050C44D call 0x50A650
0050C457 xor eax,eax
0050C459 test esi,esi
0050C45D setne al             ; any accepted key -> AL=1
```

`0x0050B790` confirms real resource ownership: it checks
the game-owned Bink video handle at `0x00AC0BA4`,
calls the imported Bink close function via `[0x53B1F8]`,
releases related resources via the existing game code,
and clears `[0x00AC0BA4]`. `0x470871` then performs
additional original video/audio transition steps and
clears `[0x6151F8]`. Thus **the native Space-skip branch
and ordinary Bink-finished branch converge on the same
game-owned teardown at `0x470871`**.

The 60 threshold is measured in **loop iterations**, not
necessarily frames or elapsed seconds. The `allow-skip`
argument is visibly **1 at both** the non-startup `0x443BE7`
and **all four** startup movie call sites `0x455CF1`, `0x455CFE`,
`0x455D0C`, `0x455D1A`. The stack offset in `mov bl,[esp+0x10]`
at `0x470826` is relative to the additional `push ebx` inside
`0x470750`; it reads the **second argument** (1). An earlier
statement here incorrectly asserted that startup calls pass zero;
that assertion is now corrected. The flag only allows a skip
branch after the threshold; it does **not** demonstrate whether
the user actually skips any particular startup film.
Not all cinematics are proved to use the same immediate
call site, but `0x470750` is reused both before and after
the menu.

**Safety boundary still unresolved:** an F12 intent must not
merely simulate Space: doing so would clean up video then
continue into gameplay. The game-owned top-level quit path
at `0x455E50 → 0x4561F2 → 0x45620C` exists, but the
original post-video owner-to-dispatcher return path and the
proper **post-teardown, pre-gameplay** quit-injection point
for the New Game sequence remain to be established.
No cinematic F12 branch or shortcut to native destructors
is implemented. Preserve validated main-menu/startup/3D
behavior; keep the PR in draft pending Windows tests.

---

## 2026-10-23 — Static recovery of native post-menu Bink skip and cleanup (no runtime patch)

The Windows #530 log establishes that the New Game movie is driven by
the original Bink loop at `0x00470750`. Its last observed frame-tick
returned `1` while the user pressed Space, shortly before game-owned
3D controller entry. Thus a **different exit condition is likely**,
but **the precise input-handling instruction, native movie cleanup and
higher-level exit path have not been recovered**.

The supported `SpideyPC.exe` is not distributed in the repository.
To inspect the original game rather than guess x86 instructions,
`tools/Inspect-BinkNativeCode.ps1` performs **read-only** static PE
parsing on the user's own SHA-256-verified executable. It produces a
small textual hexadecimal report covering `0x00470700`–`0x00470A50`
(video owner and expected `0x00470871` close path) and
`0x00455C80`–`0x00455E80` (higher-level transition), plus
bounded neighborhoods of candidate x86 direct relative CALLs that
target `0x00470750`. Raw `E8 rel32` hits are **not proven instruction
boundaries** and must be disassembled before interpretation.

Windows PowerShell 5.1, launched from the game's directory after
placing the script there:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\Inspect-BinkNativeCode.ps1 -GameExe .\SpideyPC.exe
```

Upload only `SpiderMan2k1Fix-BinkNativeCode.txt` privately for the
next stage of analysis; **never commit the original EXE or the extracted
game code bytes** to this public PR. This script never launches the game,
writes to its memory, alters its executable or sends input. Its
conclusions are deliberately limited to validated PE mapping and
static raw code bytes; the actual branch and destructors remain
**unconfirmed** until disassembly is reviewed.

Do **not** activate F12 on the post-menu comic/cinematic solely because
the Bink tick wrapper is shared with the four opening videos. Existing
F12 menu, intro and 3D cleanup behavior stays untouched.

---

## 2026-10-23 — Windows #530: Space skip exits the Bink loop WITHOUT tick returning zero

User file `SpiderMan2k1Fix(20261023-033828).log`
(`v1.0.0-ba47032`) covers New Game → auto comic still →
long Bink cinematic → user presses Space → gameplay 3D → F12.

Observed:
- Comic quad at Flip 241; Bink video flag and video handle
  become present at Flip 243.
- The original post-menu Bink tick wrapper was invoked at least
  **883,403 times** before exiting the movie loop.
- **7,366 sampled tick-result logs ALL contain
  `native_result=1`**, including the final reported native tick.
  No `native_result=0` appears.
- The final tick-result trace at `n=883403` samples
  `space_down_sampled=1` while the Bink flag/handle are still
  present and the 3D owner has not started.
- The *next* log is `NATIVE-GAME-INPUT owner ENTER n=1`;
  the following `ENGINE-STATE flip=411` sees the movie handle
  and Bink flag cleared and the native 3D owner entered.
- F12 in 3D still reaches game-owned
  `CONTROLLER-7 ... bit4=1 ... NATIVE-FINAL-CLEANUP`.

**Inference, not proof of exact instructions:** the post-menu
cinematic can be skipped by the game without
`SPIDER_NATIVE_BINK_FRAME_TICK` returning 0. Space appears pressed
immediately before exit, so the original loop's input/branch
condition likely decides when to stop; the log does **not**
establish its instruction address or teardown sequence.
Do NOT reuse the existing F12 startup-video
`native_result=0` short-circuit for the New Game cinematic
until its own game-owned skip/cleanup/exit-control flow is
confirmed.

**Instrumentation problem found:** sampling all native ticks and
logging every 120 calls produced 7,366 near-identical lines
(about 1.65 MB of logs), with roughly 880k ticks in ~16 seconds.
The diagnostic is now throttled to first four samples, an
approximately 1-second heartbeat, original-return-value
transitions and sampled Space key-down edges. To reduce
interference, Space is sampled only every 128 native tick
calls (plus any result transition), typically far more often
than once per 10ms on this measured run. The original game
Bink call still executes exactly once per wrapper invocation,
its return value remains unchanged, and there is no F12
action or movie/global-state mutation. A short key tap
could be missed by sampled key polling; this is deliberately
a non-invasive observational tool.

Next investigation: pinpoint the original post-menu Bink
loop's **Space branch and native Bink-close cleanup** before
an explicitly opt-in F12 cinematic-exit prototype.
The validated F12 menu and 3D paths remain untouched.

---

## 2026-10-23 — Windows 11: New Game still → Bink → Space skip → 3D

The user provided `SpiderMan2k1Fix(20261023-033124).log`
(`v1.0.0-4c3f218`) and reports that pressing **Space** during the
long New Game cinematic skips it and starts playable 3D.

Observed diagnostic order:

```text
COMIC-RENDER-CANDIDATE draw=1 flip=304 caller_rva=0x00102C72 texture=512x256 ... native_3d_owner_calls=0
ENGINE-STATE flip=305 ... bink_flag=0 video_handle_present=0 native_3d_owner_seen=0
POST-SHELL-BINK-TICK-CALLSITE original 0x470750 tick observed [OWNER-IDENTITY NOT PROVEN]
ENGINE-STATE flip=306 ... bink_flag=1 video_handle_present=1 native_3d_owner_seen=0
ENGINE-STATE flip=381 ... bink_flag=1 video_handle_present=1 native_3d_owner_seen=0
NATIVE-GAME-INPUT owner ENTER n=1 request=0 ...
ENGINE-STATE flip=428 ... bink_flag=0 video_handle_present=0 native_3d_owner_seen=1
QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=1 bit4=1 status=-1 outcome=NATIVE-FINAL-CLEANUP
```

This is strong temporal evidence that the post-menu cinematic
uses the **existing original 0x470750 Bink loop** and its original
movie handle/flag. It does **not** establish whether the native
loop stopped because of Space, natural end-of-file, or another
game-owned control signal; the exact native skip input and
post-video cleanup have not been observed.

Added a **strictly passive** next diagnostic in the existing
`spider_f12_native_intro_tick` wrapper: after calling the original
`SPIDER_NATIVE_BINK_FRAME_TICK` **exactly once**, return the
unaltered native `BYTE` and log throttled
`POST-SHELL-BINK-TICK-RESULT` with the result, any sampled Space
keydown, original Bink movie flag and handle *presence*, and time
since comic draw. A sampled key state is not proof of the
game's true input handler. No new hook, extra native call,
native shutdown attempt, early Bink stop, or process exit.

Re-test `New Game`, allow still to disappear naturally, press
Space ~5s into the cinematic, then after playable 3D begins use
already validated F12 and upload the log. Only contemplate
**opt-in** cinematic F12 once the game-owned owner/cleanup/return
behavior is established. Do not change intro movie or 3D F12
paths while collecting this evidence.

---

## 2026-10-23 — Passive video-path diagnostics after New Game

The user's observed sequence is one automatically timed illustrated
still → one cinematic → gameplay 3D. Existing source knowledge supports
several separate, **read-only** indicators to compare during this sequence:

- The existing real native frame-tick wrapper within the original
  `0x470750` Bink loop (CALLs at `0x47081D` and `0x47085F`) can
  report a *single first post-Shell_MainMenu callback* if that same loop
  is reused; silence does not rule out Bink elsewhere.
- The already installed DirectDraw `Blt` recognition can report the
  first few successful **post-Shell** remapped movie-like presentations.
  These are **presentations only**, not proof of a Bink decode or
  native resource ownership.
- The preexisting `Flip` stage observer now logs edge transitions for
  the known game-owned video Bink handle slot `0x00AC0BA4`, soundtrack
  handle slot `0x00AC1020`, the existing
  `0x006151F8` movie-active flag, and first observed native
  3D-input-owner invocation. Non-null handles indicate presence
  only, **not** that playback is taking place.

All changes are in the **existing** video-tick, native draw and Flip
callbacks. No additional EXE hook, COM method call, playback pause,
F12 trigger, cleanup, global game-state write or scene-specific exit.
A post-New-Game cinematic may instead be *rendered in-engine*; the
results must be interpreted conservatively. The exact native
still→cinematic→3D owners remain unknown.

Windows test procedure: use the build containing these passive logs,
with `[System] QuickExitF12=1`,
`ExperimentalQuickExitF12In3D=1`,
`[Display] ExperimentalComicNativeAspect=1`, and `[Debug] Log=1`.
Start a new game; **let the still disappear automatically**; watch
the cinematic normally, without pressing F12 during the still or
cinematic; enter 3D and quit using the validated F12. Upload the log.
Compare `COMIC-RENDER-CANDIDATE`,
`POST-SHELL-BINK-TICK-CALLSITE`, `POST-SHELL-MEDIA-BLT`,
`ENGINE-STATE` handle edges and `NATIVE-GAME-INPUT owner ENTER`.
**No new F12 behavior is enabled in those unsupported scenes.**

---

## 2026-10-23 — User clarification: New Game comic is auto-timed, followed by a cinematic

**User-observed game sequence (not independently established by the
renderer trace):** select `New Game` in the interactive main menu
→ **one still comic-book image** appears for a few seconds and
disappears automatically → an **in-game cinematic** plays
→ interactive 3D gameplay begins.

Correction to the previous suggested experiment: do **not** ask the
user to hold a comic page on screen for 10–15 seconds or manually
advance multiple pages. In this observed sequence the still is
auto-timed, and the next stage is a cinematic. The lone
`COMIC-RENDER-CANDIDATE` in build #525 is consistent with this
description, but **does not prove** either the timing, cinematic
entry, cinematic decoder identity, or the native cleanup paths.

Revised read-only investigation: correlate the auto-timed
still→cinematic and cinematic→3D transitions in the original
game-owned scene/video control flow, including whether this
post-menu cinematic is Bink and how its lifetime differs from the
four startup videos. The existing F12 intro-video forwarding is
explicitly limited to the four pre-menu movies; do not reuse
that shutdown strategy for post-menu cinematics without
identifying and validating their separate native owner and cleanup.
Do not infer shutdown authority from DirectDraw Flip or comic
render geometry, and leave existing menu/3D F12 behavior untouched.

---

## 2026-10-23 — Windows build #525: passive comic renderer trace confirmed

User log `SpiderMan2k1Fix(20261023-031026).log` reports
`v1.0.0-23ce956` and confirms the new read-only instrumentation
fires exactly once for the recognized quad:

```text
QUICK-EXIT-F12: REAL Shell_MainMenu frame callback observed (0x493DD6)
BORDERLESS: MENU exited; windowed gameplay presentation active
QUICK-EXIT-F12: COMIC-RENDER-CANDIDATE draw=1 flip=314 caller_rva=0x00102C72 texture=512x256 mode_request=0 bink_active=0 menu_seen=1 native_3d_owner_calls=0 native_3d_owner_age_ms=0 [RENDER-ONLY; comic owner UNKNOWN]
COMIC-NATIVE-FIX: #1 stage=NATIVE rva=0x00102C72 texture=512x256
QUICK-EXIT-F12: ENGINE-STATE flip=315 renderer2d=0 mode_request=0
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner ENTER n=1 request=0 timer_active=0 flag1=0 flag2=1
QUICK-EXIT-F12: 3D game-thread frame completed; native request 7 queued for original 0x455BF3 cleanup
QUICK-EXIT-F12: NATIVE-3D-CLEANUP completed n=1 request=7
QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=1 bit4=1 status=-1 outcome=NATIVE-FINAL-CLEANUP
```

The comic-shaped draw occurs **before the first observed gameplay
input-owner invocation**; the 3D F12 path still chooses native final
cleanup. This is evidence of **draw/3D-owner ordering**, not of
native comic scene ownership, page advancement or resource cleanup.
Only **one matching quad** was observed. Flip numbers cannot be
translated into elapsed wall-clock time: an image can remain visible
with few/no additional Flip calls.

Next controlled diagnostic (existing build, no code change): during
New Game, **hold on the first illustrated page for 10–15 seconds**,
then manually advance through any remaining pages with deliberate
pauses, without pressing F12 on those pages. Enter actual 3D and
use the already-validated F12 native exit. A separate log from this
run can establish whether further matching
`COMIC-RENDER-CANDIDATE` events appear for page changes. If not,
the current geometry signature is insufficient to identify the
actual page controller. Do not implement F12 comic exit by
mutating `mode_request` or by inferring ownership from renderer
state; the native comic owner remains unknown.

---

## 2026-10-23 — first passive comic-stage correlation (Windows log)

User supplied `SpiderMan2k1Fix(20261023-030218).log` from
`v1.0.0-6668f82`, with `QuickExitF12=1` and
`ExperimentalQuickExitF12In3D=1` and **no F12 during the comic**.
The log contains:

```text
QUICK-EXIT-F12: REAL Shell_MainMenu frame callback observed (0x493DD6)
BORDERLESS: MENU exited; windowed gameplay presentation active
COMIC-NATIVE-FIX: #1 stage=NATIVE rva=0x00102C72 texture=512x256
QUICK-EXIT-F12: ENGINE-STATE flip=405 renderer2d=0 mode_request=0 ... menu_seen=1
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner ENTER n=1 request=0 timer_active=0 flag1=0 flag2=0
QUICK-EXIT-F12: NATIVE-GAME-INPUT owner EXIT n=255 NATIVE REQUEST 7 observed after 0x440EF0 (internal writer unconfirmed)
QUICK-EXIT-F12: NATIVE-3D-CLEANUP completed n=1 request=7
QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=1 bit4=0 status=-1 outcome=RETURN-TO-CONTROLLER
```

The **single logged** comic-render candidate appears between
the menu-to-gameplay rendering switch and first native game-input owner
entry. The log **does not** show page-by-page comic transitions or
a game-owned comic cleanup routine. In particular, the graphics caller
`0x00102C72` is NOT established to be a scene controller. The native
3D exit to main menu remains unchanged and still returns to the
controller rather than terminating the process.

Next instrumentation (diagnostic only): from the existing exact
D3D7 comic geometry/texture candidate, emit a throttled
`COMIC-RENDER-CANDIDATE` timestamp/flip, pending engine request,
Bink state, observed count/age of 3D owner ticks, without changing
any exit logic, game globals, input hooks, or COM cleanup. Because
the existing comic classifier is opt-in, this probe requires
`[Display] ExperimentalComicNativeAspect=1` plus
`[System] QuickExitF12=1`. **A draw event never authorizes an F12
comic shutdown.** Correlate further Windows runs with normal
comic-page advancement. The original executable's exact native
comic-owner/cleanup flow is still unidentified.

---

## 2026-10-23 — Windows 11: F12 disabled / original unsafe close suppressed (PASS)

User manually disabled both
`[System] QuickExitF12=0` and
`ExperimentalQuickExitF12In3D=0`, pressed F12 while in the real
main menu and 3D gameplay, and reported the key **inactive**.
The provided `SpiderMan2k1Fix(20261023-025443).log`, from
`v1.0.0-6668f82`, confirms initialization with:

```text
INI-CHECK: valid SpiderMan2k1Fix.ini
QUICK-EXIT-F12: OFF; unsafe original F12 WM_CLOSE suppressed
```

The log then reaches 3D gameplay (FOV/HUD rendering) and returns
to 2D/menu presentation, without any quick-exit dispatch. This
confirms the **expected disabled-key behavior in this live Windows 11
run**, while retaining suppression of the known unsafe original
F12→WM_CLOSE behavior.

Validated Windows 11 F12 functional scenarios so far:
- Direct 3D F12: original 3D cleanup → native final cleanup, watcher
  reports exit code 0, process absent and no matching WER crash event.
- Native 3D→menu then F12 from menu: watcher reports exit code 0,
  process absent and no matching crash event.
- Alt+Tab away: user confirms F12 in background leaves game running;
  after returning to the menu F12 passes watcher checks.
- F12 disabled: user confirms inert key in menu and 3D; above log
  confirms opt-out and suppression of original unsafe F12 path.

These are per-run validations, **not** repeated stress tests. Comic
covers and static intro screens still lack verified mode-owned native
exit paths. Maintain draft PR #26 and default-off opt-in; do not merge
without explicit user approval.

---

## 2026-10-23 — Windows 11 build #520: 3D F12 shutdown verified externally (PASS)

User ran the repository's read-only `tools/Verify-F12QuickExit.ps1`
with `-ExpectedMode 3D` against the Windows x86 build #520. Exact
PowerShell result:

```text
Process exited after 29.8 seconds, exit code 0
Observed peak working set: 157,2 MiB; peak handle count: 1452
OK: SpideyPC.exe process no longer exists; OS-owned process memory and handles are released.
Game log: SpiderMan2k1Fix.log; verified native F12 exit path=3D
PASS: native F12 3D exit logged, exit code 0, process gone, no matching crash event found.
Note: this does not prove that every native COM object was explicitly released before exit.
```

**Status: first full Windows 11 external verification PASSED for direct
F12 during 3D** (native 3D cleanup and controller final-exit trace, exit
code 0, OS process gone, no matching Application Error / WER event found
by the watcher). This evidence is from the reported single run and does
**not** establish exhaustive COM teardown or repeated-run stability.
The separately opted-in gameplay exit remains **default-off** until
remaining regressions (natural 3D→menu, menu F12, F12 disabled and
background, different 3D situations) are checked. Do not generalize this
result to intro static pages or comics; neither has a validated native
F12 shutdown. Keep PR #26 draft and do not merge without user approval.

---

## 2026-10-23 Windows 11: first direct F12 exit from 3D confirmed (build #516)

User-provided `SpiderMan2k1Fix(20261023-023052).log` running
`v1.0.0-278bd74` enabled the separate
`[System] ExperimentalQuickExitF12In3D=1` option and pressed F12
during actual gameplay. Observed, in order:

```text
QUICK-EXIT-F12: 3D native exit intent queued; waiting for original gameplay tick
QUICK-EXIT-F12: 3D game-thread frame completed; native request 7 queued for original 0x455BF3 cleanup
QUICK-EXIT-F12: NATIVE-3D-CLEANUP begin n=1 request=7
QUICK-EXIT-F12: 3D native cleanup checkpoint returned; awaiting controller-7 finalization
QUICK-EXIT-F12: NATIVE-3D-CLEANUP completed n=1 request=7
QUICK-EXIT-F12: CONTROLLER-7 native teardown ENTER n=1 bit4=0 status=-1 3D-checkpoint=1
QUICK-EXIT-F12: 3D owner+controller cleanup complete; engine native final-exit flag set at 0x45619D
QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=1 bit4=1 status=-1 outcome=NATIVE-FINAL-CLEANUP
```

The user separately confirmed `SpideyPC.exe` was absent from Windows
Task Manager after pressing F12. Thus this is a **first successful live
Windows functional test** of a direct 3D-to-final-cleanup exit, with no
intermediate main menu. It does **not** establish exit code 0, Windows
Error Reporting status, or repeated-run stability; keep the separate
opt-in disabled by default, the PR in draft, and do not merge.

**Diagnostic correction after reviewing the log:** The old line
`NATIVE REQUEST 7 produced by 0x440EF0` on this F12 run did not
identify a native internal writer: the wrapper inserted request 7
*after* `0x440EF0` returned. Preserve a pre-injection snapshot and
log native-owner transitions only from that value. The exact nested
native writer on normal 3D-to-menu transitions remains unproven.

For a follow-up Windows 11 direct-3D verification with explicit
exit-code and Windows crash-event checks, run this read-only PowerShell
watcher from the repository checkout (adjust `-GameExe` if needed):
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\Verify-F12QuickExit.ps1 -GameExe .\SpideyPC.exe -ExpectedMode 3D
```
It launches the game, waits for the user to press F12 during 3D, and
does not inject input or kill the process. A PASS requires all of:
3D request trace, 3D owner cleanup, controller native final-decision
trace, exit code 0, no residual process, and no matching Windows
Application Error/WER event accessible in the Windows event log.

Next Windows regression tests: F12 at different gameplay moments;
native exit-to-menu (without F12) remains unchanged; main-menu F12
still dispatches Quit (18); F12 off/background is inert; record the
exit code and Windows crash events externally. Comic covers and static
intro screens remain **unsupported** and need separate owner-safe
shutdown paths.

---

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

## Build #506 validation: 3D cleanup checkpoint executes, final controller choice still unknown

In user-uploaded `SpiderMan2k1Fix(20261022-182342).log`,
the following native sequence was observed:

```text
NATIVE-GAME-INPUT owner EXIT n=506 NATIVE REQUEST 7 produced by 0x440EF0;
 timer_before=0 timer_after=0 flag_before=0,1 flag_after=0,1 elapsed=2370
NATIVE-3D-CLEANUP begin n=1 request=7
NATIVE-3D-CLEANUP completed n=1 request=7
ENGINE-STATE flip=831 renderer2d=1 mode_request=7 countdown=1769562
QUICK-EXIT-F12: native Shell_MainMenu Quit activated: slot=3 action=18
```

This proves the true 3D update requested 7, and the original
`0x004302D0` call at the beginning of the 3D owner's exit cleanup
**returned normally**, before the original top-level controller
successfully re-entered menu rendering. The marker named
`NATIVE-3D-CLEANUP completed` verifies that **one cleanup
checkpoint** completed; there are further native owner cleanup
calls at `0x00455C12`, `0x00455C2F` and `0x00455C34` before
the owner returns at `0x00455C3D`.

### Correction: request-7 specific writer within 0x440EF0 not yet proven

Both the original game's input flags `0x006611F0` and
`0x006611E0` need to be nonzero *inside* the native function
to take its branch containing the direct write at `0x0044108A`.
Yet the trace recorded `flag1=0,flag2=1` immediately before
**and after** the wrapper call, with `timer_before=0` and
`timer_after=0`. A transient internal change is still possible,
as is a different nested writer. We can conclusively attribute
the observed request 7 to the **execution of 0x440EF0 as a
whole**, but not yet to direct instruction `0x0044108A`.
Do not simulate that instruction or its inferred timing in F12.

### NEW passive native controller-7 final-branch observation

After the native 3D owner finishes and returns, original request
7 is dispatched by the top-level switch at `0x00456185`:

```text
0x00456185 PUSH EBX
0x00456190 CALL 0x00443AD0   ; native mode transition cleanup (0)
0x00456198 CALL 0x0048A820   ; native post-mode cleanup
0x0045619D TEST BYTE [0x02E0988C],4
0x004561A4 JNE 0x0045620C  ; final process cleanup
0x004561A6 CMP DWORD [0x00568FB0],EBX
0x004561AC JGE 0x0045620C  ; final process cleanup
0x004561AE JMP 0x00455D98  ; resume controller loop
```

A new exact-signature-checked **pass-through** observer wraps
`CALL 0x0048A820` at `0x00456198`, preserving original
execution and normal CPU flags. It logs the *post-cleanup* exit
conditions (bit-4 and signed status) and whether the unmodified
branch will **return to the controller** or **enter native final
process cleanup**. No F12-in-3D behavior is enabled; the observer
never mutates either condition or calls a destructor itself.

Expected evidence:

```text
QUICK-EXIT-F12: CONTROLLER-7 native teardown ENTER n=1 ...
QUICK-EXIT-F12: CONTROLLER-7 native teardown COMPLETED n=1 ... outcome=RETURN-TO-CONTROLLER
```

If the controller instead takes `NATIVE-FINAL-CLEANUP`,
the game should exit normally; this has not yet been proved
during runtime. The established menu-quit action remains unchanged.

## Build #508 result and subsequent opt-in 3D native F12 experiment

User log `SpiderMan2k1Fix(20261022-183549).log` confirms:

```text
NATIVE-GAME-INPUT owner EXIT n=507 NATIVE REQUEST 7 produced by 0x440EF0
NATIVE-3D-CLEANUP begin n=1 request=7
NATIVE-3D-CLEANUP completed n=1 request=7
CONTROLLER-7 native teardown ENTER n=1 bit4=0 status=-1 3D-checkpoint=1
CONTROLLER-7 native teardown COMPLETED n=1 bit4=0 status=-1 outcome=RETURN-TO-CONTROLLER
```

After the original controller's native `0x48A820` call, `0x45619D`
tests `[0x02E0988C]&4`; if nonzero it enters the original final
program cleanup at `0x45620C`. If the bit is clear,
`0x4561A6` compares signed `[0x00568FB0]` against zero and
also exits when nonnegative. For the user-observed return to
menu, both conditions were false: bit 4 clear, status -1.

### New game-thread-only 3D shutdown prototype (strictly opt-in)

* Config: `[System] QuickExitF12=1` **and**
  `[System] ExperimentalQuickExitF12In3D=1`. Both are disabled by
  default in the generated and example INIs. The new key is accepted
  by `ini_validation.h`.
* F12 watcher must see recent native gameplay-owner callback
  `0x440EF0` AND recent non-menu DirectDraw Flip; it **only latches**
  the exit intent on the polling thread, never modifying native code,
  graphics objects, or game globals there.
* The instrumented, SHA/signature-checked native game-input CALL
  at `0x45553B` executes the **original `0x440EF0` exactly once**.
  After it returns, provided the owner is still in normal gameplay
  (`mode_request=0`, no active Bink), the original game thread sets
  `mode_request=7`, allowing the original `0x455A90` check to
  enter native 3D cleanup at `0x455BF3`.
* The original `0x455BF9 CALL 0x4302D0` returns; only then is a
  `game3d_cleanup_confirmed` latch set. The owner continues the rest
  of its **unchanged** native teardown and returns.
* The original request-7 controller then executes native
  `0x443AD0(0)` and original `0x48A820`. Only **after the latter
  returns** may our wrapper set bit 4 in `[0x02E0988C]`, retaining
  all unrelated bits. It is guarded by *all* conditions: opt-in flag,
  a matching pending 3D F12 state, actual request 7, and confirmed
  owner cleanup count beyond the pre-request baseline.
* The game itself tests the flag at `0x45619D` and jumps to
  `0x45620C`, the native **process final-cleanup** sequence.
  The patch does **not** call destructors, manipulate Windows
  HWND, synthesize Enter, or terminate the process.

This approach is **new and unvalidated on Windows**. It is a targeted
hypothesis informed by a verified native control-flow path, *not*
proof of full termination. Unlike the earlier read-only probes, it
intentionally updates two original game globals **on the owning native
game thread only**. The first test must check that F12 during
actual 3D closes the OS process (no audio leftovers or crash),
and that it does not regress the known-good main-menu F12.
If owner recognition fails, F12 must be ignored rather than
destroying the game window. The comic and fixed-splash paths remain
unsupported. **Do not merge PR #26 until user validation and explicit
approval.**

## Build #508 Windows validation — native controller return decision CONFIRMED

User-provided `SpiderMan2k1Fix(20261022-183549).log` proves the entire
observed mode transition, in this exact order:

```text
NATIVE-GAME-INPUT owner EXIT n=507 NATIVE REQUEST 7 produced by 0x440EF0; timer_before=0 timer_after=0 flag_before=0,1 flag_after=0,1 elapsed=2673
NATIVE-3D-CLEANUP begin n=1 request=7
NATIVE-3D-CLEANUP completed n=1 request=7
CONTROLLER-7 native teardown ENTER n=1 bit4=0 status=-1 3D-checkpoint=1
CONTROLLER-7 native teardown COMPLETED n=1 bit4=0 status=-1 outcome=RETURN-TO-CONTROLLER
QUICK-EXIT-F12: native Shell_MainMenu Quit activated: slot=3 action=18, native cleanup branch=0x4941D5
```

In the tested normal 3D-to-menu route, original controller request
7 performs its native teardown then **explicitly chooses to return
to the controller**, because `[0x02E0988C]&4 == 0` and signed
`[0x00568FB0] == -1`. These values were observed *after* the
original `0x0048A820` call on the original game thread.

This confirms that **a safe exit from 3D is not the same as
program termination** and explains why blindly writing request
7 cannot implement the desired universal F12 shutdown. It also
proves the native final-cleanup branch `0x0045620C` was
**not** taken during this normal return-to-menu test.

### Proposed next implementation / safety gates

- Retain the validated, unchanged interactive-menu F12 action.
- Capture a dedicated, one-shot **3D F12 intent** only when the
  true gameplay owner's tick is active; **no** off-thread writes
  to `0x0060CFA4`, `0x02E0988C`, or `0x00568FB0`.
- Locate a game-thread, native owner-approved *early exit*
  that reaches the existing `0x00455BF3` cleanup through the
  correct original conditions; do not assume `0x0044108A`
  was executed, since both wrapper snapshots show flag1=0.
- After verifying the **entire** 3D owner cleanup and returns,
  determine whether the engine's documented native process
  termination can be requested at the controller's actual
  `0x0045619D` decision safely, preferably reusing an
  existing command rather than changing the two unrelated
  global exit-condition variables. This is **not validated yet**.
- Separately identify native comic and static-startup owners;
  never infer their identity from render-only signals.
- Continue using the read-only external Windows watcher to
  verify exit code, audio stop and absence of a residual
  `SpideyPC.exe`.

**Status:** Evidence and design are validated through the
controller's `RETURN-TO-CONTROLLER` decision, not a native
direct F12 exit from gameplay. The PR must remain a draft;
no gameplay shutdown behavior should be enabled merely
because this diagnostic returned successfully.

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
