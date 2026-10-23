## 2026-10-23 — F10 consolidation and in-level cinematic protection

The active physical shortcut is F10, not F12. The original
F12 WM_CLOSE call remains safely patched out. Earlier
F12-named INI flags remain accepted only as compatibility
aliases; no old flag makes the physical F12 key active.

The user validated Wine build #687, whose log
`SpiderMan2k1Fix(20261023-192206).log` shows:
- two original independent CutSceneScript auto-skips ending
  with lifecycle 6 and native auto-stage 7 at +1306ms,
  original manager idle and native skip request cleared;
- a later F10 accepted during 3D gameplay;
- original request 7 dispatched on the native game thread,
  followed by `NATIVE-3D-CLEANUP completed`;
- controller-7 completed with
  `bit4=1 status=-1 outcome=NATIVE-FINAL-CLEANUP`.

The log does not include OS process exit code. User visual
validation is positive; other levels and cinematic owners
are still untested.

New `[Debug] F10VerboseTrace=0` defaults to COMPACT logging,
hiding only noisy passive probe messages. It never changes
native gameplay, script handling or the probes themselves.
Original script-lifecycle, auto-skip and native cleanup
messages remain visible. Enable F10VerboseTrace=1 to recover
the old detailed troubleshooting snapshots.

New fail-closed native status `debug_live_cutscene_active`
is updated every original 3D game-input owner frame:
- -1: invalid/unreadable owner (exit forbidden)
- 1: context or script-active byte set (exit forbidden)
- 0: both original script bytes idle (needed to permit exit)

The F10 background watcher now requires current status 0,
in addition to lifecycle 6 + native skip stage 7 and all
existing focus/video/pause/owner-frame guards. The game
thread repeats this guard and directly checks the verified
original script manager's two idle bytes before writing
native request 7. This protects later in-level scenes
running through the same original CutSceneScript mechanism.
It does NOT establish that every kind of cinematic uses
that manager. No new hook sites or forced process quit.

Testing checklist: `docs/F10_VALIDATION_MATRIX.md`.
PR #26 remains draft/unmerged pending user approval.

---

## 2026-10-23 — New experimental F10 in-game native quit (strictly opt-in)

With the F10 intro-skip confirmed by the user in Wine, the
next requested feature is to quit safely **from playable 3D**
by pressing F10 again. The original 2001 game's request 7
at 0x60CFA4 enters its own 3D owner cleanup (0x455BF3),
followed by controller-7 cleanup and optional native FINAL
shutdown at 0x45619D. Existing code already owns a gated
hook for this, but previously failed closed in ALL the
two-cinematic New Game scenes.

**NEW extra [Debug] F10GameplayExit=1 (default OFF)**,
strict boolean validated and handled by INI auto repair,
requires Log=1 and F10TestMode=1. During this first
conservative experiment it ONLY works after a New Game
comic F10 with F10Auto3DScripts=1, which makes:
- both distinct game-owned CutSceneScript lifetimes end
  via original native cleanup (lifecycle state 6);
- both game-owned skip request bytes are consumed
  (native auto-skip stage 7);
- the manager's actual cutscene context +0x1AC
  and active-script flag +0x1B4 are both ZERO.
The watcher also insists on current original 3D
input-owner and renderer frames, no video, no pause,
no pending skip request, mode=0, and user F10
fresh key edge with the game foreground.
The background watcher queues ONLY an intent;
the NATIVE GAME THREAD verifies manager memory
using VirtualQuery before setting the existing
mode 7 request. The native 3D cleanup must return
before controller 7 can set final shutdown bit.
Unsolicited ORIGINAL mode7 requests, such as the
one observed ~11 seconds into earlier Wine logs,
cannot trigger this final shutdown because
game3d_exit_state is not 2 for those original
game-generated requests.

A new F10 on either cinematic remains blocked;
no unsafe timers, actor state 5/3, SPACE counts,
graphics HUD appearances, or fake Space injection.
New Game manual scripted skips and unclassified
other game modes also fail closed for this initial
feature. A second physical F10 press is required
after the previously validated opening F10.
F12's original unsafe WM_CLOSE stays patched away.

**Next LIVE Wine test (not yet performed):**
```ini
[Debug]
Log=1
F10TestMode=1
F10Auto3DScripts=1
F10GameplayExit=1
```
Start New Game, F10 once on L1COV.bmp, allow
both original 3D scripts to auto-skip, verify
actual Spider-Man movement and SPACE jump,
RELEASE then press F10 again during playable
3D and check native 3D cleanup, original
controller 7 final teardown and no lingering
SpideyPC.exe process. If exit blocked, send
full log. For errors, disable F10GameplayExit
or restore previous known-good F10 build.
PR #26 remains DRAFT/unmerged pending validation.

---

## 2026-10-23 — Shortcut reassigned from F12 to F10 for Linux MangoHud

The physical shortcut is **F10 exclusively**, to avoid MangoHud's
Linux F11/F12 bindings. This is NOT a request to enable the original
F12 WndProc command: static x86 disassembly of SHA-verified
SpideyPC.exe shows that original WndProc 0x5152B0 is registered
through RegisterClassA at 0x515A18, WM_KEYDOWN wParam 0x7B (F12)
selects the PostMessageA(WM_CLOSE) path at 0x515373. Keep the
existing exact-signature patch which replaces ONLY the original
PostMessageA call with stack cleanup. It does not have to be
reinstalled for F10 and does not interfere with MangoHud's key.

The F10 polling site in the existing background input watcher
uses GetAsyncKeyState(VK_F10); no GetAsyncKeyState(VK_F12) remains.
All existing original menu, comic, Bink and native script paths
are unchanged; the proven Wine #681 native two-script auto-skip
now starts on a single F10 during the original New Game comic.

New canonical INI debug keys: F10TestMode and
F10Auto3DScripts, both default 0, require the same Log/opt-in
safety gates as their old names. New canonical System flags:
QuickExitF10, ExperimentalQuickExitF10In3D,
ExperimentalQuickExitF10InNewGameMovie,
ExperimentalQuickExitF10InNewGameComic and
ExperimentalQuickExitF10InIntroStills, all default 0.
The corresponding old F12-named keys REMAIN VALID and
are interpreted as config aliases for F10; if either alias
or canonical version is enabled, the corresponding F10
behavior may be enabled. No legacy option can make F12
an active shortcut again. This avoids INI auto-repair
deleting old settings and preserves saved controls.
The existing internal C function names and historical
QUICK-EXIT-F12 log prefix are kept for compatibility.

No automatic F10 gameplay exit is authorized; controller-7
exit still fails closed during unclassified New Game 3D.
PR remains DRAFT/UNMERGED pending Wine test/approval.

---

## 2026-10-23 — Fix: auto-skip debug setting rejected by INI validation

After Windows x86 build #678, the user reported
`[Debug] F12Auto3DScripts=1` was NOT accepted
by the game's INI preflight. Confirmed cause:
`quick_exit_f12.c` loads that exact key with
`GetPrivateProfileIntA`, but the
`ini_validation.h` `spider_ini_debug[]`
allowlist still contained only `Log` and
`F12TestMode`. `SpiderCfg_ValidateCurrentIni`
treats an unknown key as a warning and invokes
automatic canonical INI repair, where
`SpiderIni_Recover` drops it because it is
not considered supported. Thus the option
disappeared and the prototype stayed off
before testing even started.

Fix: add `F12Auto3DScripts` to the STRICT
[Debug] allowlist (values ONLY `0` or `1`),
also add a documented `F12Auto3DScripts=0`
default to the actual Win32 generated-template
literal in `virtual_spidey_cfg.c`, the
checked-in generated example and the
checked-in base INI. Keep enabled only via
user's explicit edit; do NOT activate by
default or on legacy INI files. Use existing
C99 validator/recovery regression harnesses
to verify enabled and disabled options are
accepted without warnings, invalid values
still fail validation, valid `1` survives
an INI repair alongside display and keyboard
preferences, and the factory template
continues to default to `0`.

This change does NOT alter the previously
unvalidated native 3D script skip implementation,
its two one-shot requests, or full-game
F12 shutdown guard. PR #26 stays DRAFT,
UNMERGED without user approval.

---

## 2026-10-23 — Wine 18:01:30: full six-step native cutscene lifecycle and deliberate gameplay jump confirmed

The user sent `SpiderMan2k1Fix(20261023-180130).log` from build #677.
The precise original CutSceneScript lifecycle diagnostic now emits
**ALL SIX states** in the expected original game-owned order:
FIRST_SCRIPT_ACTIVE +0ms; FIRST_SKIP_ALLOWED +68ms;
FIRST_SCRIPT_ENDED +1489ms; SECOND_SCRIPT_ACTIVE
+1502ms (different script bytecode address);
SECOND_SKIP_ALLOWED **+2635ms**; SECOND_SCRIPT_ENDED
+2669ms. The first Space press occurred around +1484ms;
the second physical Space press at **+2369ms**, notably
**266ms BEFORE** the second script's native
`CutSceneSkipAllow` becomes 1, yet that script
ends only at +2669ms. This proves any automation
must WAIT for each original script's independent
skip permission, rather than assume physical
edge count or issue back-to-back skips immediately.
The third Space at +4269ms is a deliberate gameplay
JUMP: both native context and active remain ZERO,
and the lifecycle state correctly stays stage 6.
Original comic/Bink #4 completion remains **1ms**.
Native unrelated request-7 at ~+8.6s still has an
unconfirmed writer and MUST NOT be treated as
successful F12 gameplay exit.

**STATIC native chain confirmed in supplied original
SpideyPC.exe**:
- `0x4C6654/0x4C665C` check native cutscene context
  and original skip permission.
- `0x4C66A6` sets exactly BYTE `0x0060CFC4=1`
  after additional original native input conditions.
- `0x4BC3E0` reads this byte. If nonzero, original
  `0x4BC3EF` clears active script +0x1B4, and
  iterates original pending-script action list for
  full native cleanup.
- `0x4BC17E` clears original request byte when
  initializing the next script. Do not alter the
  cinematic manager's own permission byte or
  press-space DirectInput buffer.

**First controlled automated-skip experiment**:
This commit adds NEW explicit opt-in INI setting
`[Debug] F12Auto3DScripts=1`, off by default
(also requires `F12TestMode=1` AND `Log=1`).
Only the already-supported New Game F12 on comic
`L1COV.bmp` arms this sequence. On the verified
post-Bink original 3D game thread, a separate
fail-closed state machine observes first active
with permission=0, then independently waits
permission=1 before it writes the exact original
one-byte skip REQUEST at 0x60CFC4 **ONCE**.
It waits for original manager context+active
both becoming zero (first script fully ended),
requires a second distinct script with
permission reset to 0, waits that script's
own permission=1, issues exactly ONE more
request, and observes its original end.
It NEVER forges Space, alters action 0x10,
overrides cutscene permission, bypasses
the original script cleanup, or authorizes
F12 gameplay exit.

The experimental state aborts if original
manager data become unreadable, owner changes,
native game mode/movie unexpectedly changes,
native skip request is already pending, or
the sequence takes over 15s. A failure keeps
native Space fallback intact and never writes
a late request. The original READ-ONLY
six-step tracker remains independently active
whether this optional mutation is enabled
or not. **Neither the two-auto-skip logic
nor gameplay safety is runtime validated yet.**
It is an isolated, explicitly opted-in
test under draft PR #26.

To test: set `[Debug] F12TestMode=1`,
`Log=1`, `F12Auto3DScripts=1` in
SpiderMan2k1Fix.ini. New Game -> F12
ONCE on L1COV.bmp, then do NOT press
Space during either intro. Check that
both original 3D intros end normally and
Spider-Man can move/jump afterwards;
send whole debug log, especially
`DEBUG-3D-AUTO-NATIVE`. No F12 gameplay
exit is enabled. If native owner does
not process either request, switch
`F12Auto3DScripts=0` and use manual
Space until this experimental path is
fixed. No merge without explicit approval.

---

## 2026-10-23 — Wine 17:50:21: BOTH New Game 3D intros positively identified as original CutSceneScript

The user provided `SpiderMan2k1Fix(20261023-175021).log`,
from Windows x86 build #676 under Wine. The new direct
CutSceneScript/SkipAllow/EndScript field probe is decisive:
the original script manager at **[0x6A9038]** (runtime
object 0x059E7B80) executes TWO separate skippable 3D
script lifetimes during exactly the user's two manual
cinematic skips. This is no longer an attribution based
merely on physical keyboard events, HUD or actor state.

**First script / Stan Lee 3D intro**:
- +0ms: context=1, script_active=1, skip_allowed=0,
  script_ip=0x05B64582.
- +68ms: skip_allowed=1, script_ip=0x05B64594.
- +2540ms: first SPACE physical keydown; +2545ms:
  context=0, script_active=0, skip_allowed=1.
  This directly documents the end of that script.
- +2568ms: new context=1, script_active=1,
  skip_allowed=0, script_ip=**0x05B6624E** —
  a DIFFERENT original script. The original game
  cleaned up and transitioned between scripts
  without forced program termination.

**Second script / following 3D intro**:
- +2702ms: context=1, script_active=1,
  skip_allowed=0, script step=0x60.
- +3702ms: skip_allowed=1, active/context still 1.
- +4402ms: second manual SPACE event and
  context=0, script_active=0. This is the SECOND
  script's original end, after which both remain
  zero through at least **+10702ms** (and HUD renders).
  The underlying Space edge logger reports +4419ms
  because it samples asynchronously to the original
  game-thread input; do NOT confuse the logger's
  edge timestamp with the native handler's frame.

Physical Space uses action 0x10, which also means
JUMP in playable gameplay. The script-manager's
native context and active bytes differentiate
the two real cinematic lifetimes. `skip_allowed`
indicates original permission, not a complete
safe shortcut by itself. The record does not
show a later intentional gameplay SPACE jump:
this particular run contains precisely TWO
physical Space presses. Earlier user-verified
runs establish that a third gameplay Space
performs Spider-Man's jump, not a third scene.

**New conservative life-cycle-only inference**:
the existing original 3D game-thread probe now
logs `DEBUG-3D-CUTSCENE-LIFECYCLE` stages:
FIRST_SCRIPT_ACTIVE, FIRST_SKIP_ALLOWED,
FIRST_SCRIPT_ENDED, SECOND_SCRIPT_ACTIVE,
SECOND_SKIP_ALLOWED, SECOND_SCRIPT_ENDED_CANDIDATE_GAMEPLAY.
It requires original context=1 / script_active=1,
native explicit allow=1, true context=0 /
active=0 end, and a DIFFERENT second script IP.
The original script pointers can advance during
a script, so the last first script IP is retained
to compare with the new script's initial address.
It uses neither elapsed time nor physical Space
edge counts as the stage predicates. It is
**ONLY LOGGING**, NOT an F12 exit permission.
This state machine is specific to the first
New Game intro sequence and must not be assumed
valid during other levels, arbitrary scripted
gameplay, or an interrupted/restarted campaign.

Still missing for requested automatic one-F12
cinematic skips: positively identify the exact
ORIGINAL engine's native skip-request handler,
and trigger it at the moment it accepts skipping
without overwriting global DIK input or bypassing
the game's script end/cleanup. Do not merely
toggle skip_allowed from 0 to 1: the script
itself sets that permission and the physical
SPACE performs an additional original action.
F12's playable-only native EXIT also stays
unarmed pending positive controller/context
review. PR #26 remains DRAFT and UNMERGED.

---

## 2026-10-23 — Original 3D cinematic script engine identified after Wine #675

User log `SpiderMan2k1Fix(20261023-172652).log`,
from build #675, confirms original F12 native comic
`L1COV.bmp` plus Bink movie #4 teardown in **2 ms**.
Three original 3D physical SPACE edges: **+1303 ms**
(Stan Lee introduction skip), **+2407 ms** (second
3D intro skip), **+3923 ms** (deliberate playable
Spider-Man jump). All three successfully reach the
original DIK_SPACE buffer and game action bit 0x10.
Actor member state +0x2A0 again briefly 5->3->5
after SPACE #1 and persistently 5->3 after SPACE #2.
It is still NOT a proven native playable-ready flag.

The newly read first TWELVE nodes of the original
event-dispatch list ALL show `event_id=0`,
`triggered=0`, `pending=0`, with identical
hash `0x4AF9FE53` before/during/after both
cinematic skips and during the gameplay jump.
`unreadable=0`, `cycle=0`, but `truncated=1`;
thus only this bounded 12-node PREFIX is ruled
out as a native scene discriminator. Don't infer
that the full linked list is inert.

**DIRECT static native cutscene breakthrough**:
The SHA-verified original `SpideyPC.exe` contains
specific commands `CutSceneScript`,
`CutSceneSkipAllow(%i)`, and `CutSceneEndScript`;
it also names `SkipLib.txt`. These are actual
interpreter cases, not speculation:

- At **0x4E1485**, the original script dispatches
  `CutSceneScript` via **0x4BC1A0**, setting
  `[0x6A9038]+0x1AC=1` (scene context),
  `+0x1B4=1` (script active), `+0x1B8` to
  script instruction pointer, and `+0x1A4=0`
  (initial skip permission).
- At **0x4E14A5**, the original interpreter executes
  `CutSceneSkipAllow(%i)`, setting the native
  **`+0x1A4`** byte to 1/0 depending on argument.
- Original **0x4C6654 / 0x4C665C** directly test
  these `+0x1AC / +0x1A4` bytes in the scene
  update function, conditionally changing other
  cinematic/scene-related engine fields. This
  proves real game logic uses those two flags.
- `CutSceneEndScript` at **0x4BC4A2** clears
  **`+0x1B4`** at 0x4BC4AF.

This is the first confirmed, explicit *cinematic
script mechanism* found through disassembly rather
than generic input/actor correlation. But it
does NOT YET establish that either of the TWO
specific New Game 3D intros executes that script
path or that `+0x1A4` is sufficient to skip the
active scene. In particular the script loader may
use level-supplied external data.

**Focused next read-only runtime test**:
The EXISTING 3D native-thread observation now
records a compact, bounded `DEBUG-3D-CUTSCENE-SCRIPT`
line with original `0x6A9038` manager pointer,
`+0x1AC` scene context, `+0x1A4` skip flag,
`+0x1B4` script active, and `+0x1A8/+0x1B8`
script progress/pointer. It verifies committed
readable manager memory before dereferencing any
fields. Log changes across the TWO user-confirmed
intro SPACE skips and the intentional gameplay
jump; the full owner is never written, and these
fields NEVER authorize a synthetic Space or F12
exit. This test should confirm or disprove whether
this particular explicit native cutscene owner is
indeed live during the user's intro sequence.

The requested one-F12 native 3D skip and positively
verified gameplay-only EXIT are still unimplemented.
PR #26 remains DRAFT and UNMERGED pending approval.

---

## 2026-10-23 — Wine 17:13:51 (#674): first event node inert, original list is multi-node

The user log SpiderMan2k1Fix(20261023-171351).log confirms native
comic L1COV.bmp + Bink movie #4 skip in **1 ms**. Following
native 3D entry, SPACE key events at **+1990 ms** and
**+3717 ms** correlate with the two original introductory
3D cinematic skips, followed by the separately requested
deliberate gameplay jump at **+5121 ms**.
The game's native 0x661200 action 0x10 activates at all
three presses.

Original actor/controller state at [0x56F3B8]+0x2A0 again
goes **5->3->5** on the first cutscene SPACE, then
**5->3 persistently** on the second, and remains 3 on the
actual gameplay jump. Its 3 transient state during
the first introduction disqualifies unguarded
controller_state==3 as an automatic playable predicate.

The new FIRST-NODE diagnostic was conclusive only about
that particular node: the dispatch pointer stays
0x059E8AC0, first node event_id=0, triggered=0,
pending=0, next=0x059E8B20 for ALL three events and
the sampled gameplay. Static disassembly proves that
original 0x4DE890 / 0x4DE8B0 operate on the COMPLETE
singly linked dispatch list at node +0x14, while
0x4DE8D0 also searches event bucket chains at +0x10
starting from hash table 0x6B4214. Reading only the
first node is NOT evidence that the rest of the
game-owned dispatcher is inert or has no script event
information.

New read-only, game-thread-only DEBUG-3D-DISPATCH-CHAIN
probe in the existing original 3D owner:
- Traverse a maximum of 12 original event list nodes
  following the actual +0x14 link. Check EACH node's
  entire 0x18-byte span with Windows VirtualQuery
  (committed, readable, non-guard region) first.
- Detect cycles and unreadable pointers without
  dereferencing either one. Log bounds/truncation.
- Read original node event ID +0x0C, triggered +4,
  pending +5, and link +0x14. Emit bounded per-session
  fingerprints and compact node snapshots on genuine
  changes, physical SPACE state transitions and a
  sparse heartbeat, at most 65 lines.
- The probe DOES NOT install a new native hook, modify
  game bytes, forge action 0x10, walk unbounded
  pointers, or let dispatcher data authorize F12 exit.

Neither the original cinematic script owner nor
the positive gameplay-ready boundary is yet proven.
If even the bounded chain contains no meaningful
difference, stop adding generic keyboard or actor
heuristics: reverse-engineer original script/camera
director and its scene progression directly. The
full one-F12 skip of both 3D cinematic sequences
and playable-only exit remain unimplemented.
PR #26 remains draft/unmerged without user approval.

---

## 2026-10-23 — Analysis of user Wine 16:25:54 log, native dispatcher observation

Build #673 log SpiderMan2k1Fix(20261023-162554).log shows
two original 3D cinematic SPACE presses at +2070ms and +3269ms,
then a deliberate gameplay jump at +4903ms. Original cover/Bink
teardown still completes in 2ms.

The original controller object at 0x56F3B8 and member +0x2A0
starts in value 5, changes 5 -> 3 -> 5 during the FIRST intro
press, then 5 -> 3 persistently at the SECOND cinematic skip,
remaining 3 during the gameplay jump. This is a promising
correlation, but 3 appears transiently even during the first
cinematic. Static EXE disassembly shows this is an actor-state
field with writers in 0x4162xx/0x4164xx and a generic setter
0x4167F0. Thus treating value 3 as native playable-ready
would be unsafe. Neither an elapsed-time threshold nor
SPACE event counts can prove scene completion.

The original dispatcher head at 0x6B4708 remains 0x059E8AC0
through all three presses. The exact disassembly shows this
is a linked list: original routine 0x4DE890 clears node +4,
0x4DE8B0 tests node +5, and 0x4DE8D0 uses a DWORD event ID
at node +0x0C, following its next pointer at +0x14.
Therefore constant HEAD does not imply constant event data.

The existing original game-thread diagnostic now PASSIVELY
records the first node's game-owned event flag bytes +4/+5,
event ID +0x0C, and link +0x14 in bounded
DEBUG-3D-DISPATCH-NODE lines, correlated to physical SPACE
and elapsed time. No additional instruction hook or
native input/scene/clock mutation is introduced. These
generic event flags remain unproven as cinematic detectors.
PR #26 remains draft and unmerged; F12 auto-3D-skip and
playable-only native exit stay disabled pending proof.

---

## 2026-10-23 — Wine 16:17:50: action 0x10 is indistinguishable for 3D skips and gameplay jump

The user supplied complete build #672 log
`SpiderMan2k1Fix(20261023-161750).log`, with
three physically distinct Space presses after the
native Bink #4 New Game transition. The test requested
two original 3D cinematic skips followed by one
deliberate gameplay jump:

- **+1879 ms**, first Space event: native
  `0x661200` action **0x10** held=1, then
  just_pressed=1 and early=1 at +1902 ms.
- **+3152 ms**, second Space edge: action 0x10
  held=1 and just_pressed=1 at nearby +3135 ms,
  then again just_pressed=1 / early=1 +3202 ms.
- **+4336 ms**, third Space edge (requested
  gameplay jump): action 0x10 held=1, native
  keyboard byte `0x7F`; the very short
  just_pressed window for this third event was
  not captured by the bounded change logger.

All three original keyboard events are received and
translated by the game. The original action bit is
**the same 0x10 for BOTH 3D scene skips and actual
gameplay jump**, so none of `held`, `just_pressed`,
`early`, DIK 0x39, key-edge count, or HUD instruction
draw alone identifies an original cutscene stage or
proves a playable frame. Native cover+Bink movie #4
completion remains **2 ms**. Native `0x5FAECC` starts
changing only at +7.9–9 s in this run, **well after**
the action events, so it is NOT either scene boundary.
A later native request 7 still has unconfirmed writer.

**Original executable disassembly-guided next
read-only attribution**: The exact original 3D update
at `0x455582` queries a controller object pointer at
`0x56F3B8` and tests its member DWORD at `+0x2A0`
against `0xD`. Independently the original dispatch
helpers `0x4DE890` / `0x4DE840` reference the
game-owned dispatch linked-list head `0x6B4708`.
Neither pointer nor controller state is established
as a cutscene marker. The EXISTING native game-thread
checkpoint now logs bounded
`DEBUG-3D-SCENE-CONTEXT` messages with actual
controller pointer/state, dispatch head, original
`0x2E0988C` flags, physical SPACE and original
action held/just-pressed state on any changes
(and sparse heartbeat). This can reveal which
game-owned object changes when a scripted 3D intro
ends, *without* altering the game or treating a
UI draw as a scene signal.

No new hooked game instructions, no synthetic
key or native scene changes, and no controller-7
exit permission. PR #26 remains draft/unmerged,
with single-F12 two-3D-cinematic auto-skip and
positive gameplay-only exit boundary still pending
runtime correlation.

---

## 2026-10-23 — Wine 16:04:45; original Jump action-state handling is now located

The user's full log `SpiderMan2k1Fix(20261023-160445).log`
(build #671) records only TWO physical SPACE presses in the
native 3D owner: **+2156 ms** (first intro, per prior user
clarification) and **+4003 ms** (second intro). Its first
HUD instruction appears immediately AFTER the second
press in this run. The original DirectInput keyboard byte
at `0x6B7965` changes with each press, the native keyboard
device stays active, and the game-owned 13-entry runtime
keymap again maps SPACE DIK `0x39` to exactly **action 0x10**.
The prior native F12 cover+movie-4 jump remains **2 ms**.
The game's `0x5FAECC` phase-state changes at +~14–16 s,
long after both 3D SPACE presses, and cannot distinguish
the two intro skip events. A native request-7 return
occurs still later but its specific initiating input
is unverified.

**New disassembly result:** original `0x505720` calls
`0x50A190` at `0x505738` to convert DIK presses
into game action bits. For the first action-button
slot, `0x5058A5` passes native keyboard action
`0x10` to `0x479540` with state destination
**`0x661200`**. Original `0x479540` stores a
held state at `+0`, a newly-pressed state at
`+1`, early-press state at `+2`, and counters
at `+4/+8/+12`. The game's post-Bink 3D checkpoint
now samples that original structure in read-only mode,
logging bounded `DEBUG-3D-SPACE-ACTION` on state
changes. Each message includes original held/new/early
and frame counters, DIK raw keyboard state, physical
Space and the time from native 3D entry.

**Caution:** action 0x10 is ALSO the real player JUMP
after both 3D cinematics, as the user directly verified
in the earlier 15:57:30 test. Neither an action bit
nor physical edge nor HUD draw can by itself identify
the original scene-completion boundary. This probe
will confirm the native action pipeline and whether
scripted scenes consume/clear the same signal, not
enable new F12 functionality without positive
game-owned scene/playable proof. No additional native
site is patched; no original action value is modified;
the F12 3D guard remains fail-closed.

---

## 2026-10-23 — User-confirmed interpretation of Wine 15:57:30 log (build #669)

**Crucial first-hand correction:** of the THREE physical SPACE edges in
`SpiderMan2k1Fix(20261023-155730).log`, ONLY the first
TWO skipped the original 3D intro cinematics:

- +**1684 ms**: original Stan Lee 3D narration -> second cinematic.
- +**2713 ms**: second cinematic -> playable gameplay.
- +**4573 ms**: an **accidental extra SPACE press in gameplay** which
  made Spider-Man **jump**. This MUST NOT be counted as a third
  cinematic, third scripted skip or a gameplay-ready transition.

The game-owned `0x006B7965` DirectInput SPACE byte
records the physical keyboard changes across all three events.
The first `HUD-ADAPTIVE region=instructions hit=1` line happens
after the third event in this particular run; this is merely
render diagnostic timing. Combined with the user's visual confirmation,
it establishes that first HUD-draw time and physical press count
are **NOT** valid detectors of native cinematic completion.

**Key new positive finding:** the game's actual `0x568398`
runtime action map contains **13 entries**, and exactly **one**
DIK_SPACE (`0x39`) entry mapped to **action bit `0x00000010`**.
The user confirms SPACE makes the playable Spider-Man jump.
Thus the same physical SPACE key has a gameplay action; its
use during scripted 3D cinematics must be mediated by a separate
original scene/skip state or that action in a different context.
The runtime keymap itself does **not** prove which native routine
ends either cinematic. `0x50A190` action conversion and
`0x50C180` action-mask handling merit static follow-up,
but the next F12 skip must not blindly synthesize jump
input twice or assume gameplay becomes ready on edge #2.

The diagnostic SPACE-event string previously mentioned “TWO
ORIGINAL SPACE-SKIPPABLE SCENES” on *every* raw physical
edge, which was misleading when edge #3 was a jump. This
has been amended to `RAW PHYSICAL PRESS ONLY; MAY BE GAMEPLAY
JUMP; NO SCENE OR EXIT AUTHORITY`. No game hook behavior
was altered; the event count remains raw telemetry and does
not authorize F12 native exit. The validated F12-on-comic
original native Bink #4 completion remains **1 ms**.

**Classification is per this user-confirmed run only.**
Subsequent runs must identify ORIGINAL game-owned scene
completion and positive playable state, not simply rely on
press-count thresholds. The user's desired one-F12
completion of both 3D intros and safe gameplay-only
F12 shutdown are still pending.

---

## 2026-10-23 — 15:42:54 Wine: original DirectInput SPACE byte observed; audit live action mapping

The user-supplied log `SpiderMan2k1Fix(20261023-154254).log`
confirms two original game DirectInput SPACE transitions
(+1202 ms native `0x00 -> 0xFF`, then `0x7F`,
release `0x00`; +2502 ms `0x00 -> 0x7F`,
release `0x80 -> 0x00`), with the original keyboard
device present. The first HUD instructions appear
after the second SPACE press. Native cover + Bink #4
completion remains **2 ms**. These observations
do not prove original cinematic ownership or readiness
to accept a gameplay F12 exit.

The prior raw reader probes cover `0x50A650` only.
Original game frame routine `0x505720` calls the
keyboard-action converter `0x50A190` at `0x505738`.
That code directly reads raw keys at `0x50A231`
and `0x50A258` while scanning the original
`0x568398` table of `(action,DIK)` pairs.
The untouched original on-disk table has NO
DIK_SPACE `0x39` default mapping, but the
live settings may differ. A new `DEBUG-3D-KEYMAP`
one-time snapshot of this live table in the
existing native game-thread input owner counts
SPACE mappings, prints their action mask and
checks the original `0x8000` sentinel.
No new code hook, native memory write, keyboard
injection or F12 request-7 exit. If live SPACE
is not bound to an action, the original cinematic
skip mechanism must use a separate input/scene
path instead. The 3D F12 safety guard remains
fail-closed pending exact script/cinematic ownership.

---

## 2026-10-23 — 15:29:27 Wine log: game-owned DirectInput SPACE buffer audit

The user-provided log `SpiderMan2k1Fix(20261023-152927).log`
confirms the original native F12 cover+movie completion
within **1 ms**, again reaching Stan Lee's 3D scene.
Three physical SPACE presses at **+1878**, **+2774**,
**+3813 ms** were recorded. First HUD instructions
appeared after the **third** press in this session;
other Wine sessions showed HUD immediately after
the second. Physical press count is not a reliable
number of completed native cutscene transitions.

The newly added `DEBUG-3D-RAW-SPACE` probe was installed
but logged no Space-key reads, and the 0x455427/
0x455444 gate flag bit-0x100 stayed clear.
This does NOT prove the engine never receives SPACE:
those probes observe only specific polling functions.

The SHA-matched EXE at original `0x501CB0` directly
reads its buffered DirectInput keyboard-state byte at
`0x006B792C + DIK`; for SPACE (DIK `0x39`)
that is exactly `0x006B7965`. Buffered event handler
`0x501C87` stores `0xFF` on keydown;
`0x501C8F` stores `0x80` on keyup.
The already installed original 3D input checkpoint now
READ-ONLY logs `DEBUG-3D-DINPUT-SPACE-BYTE`
whenever this byte changes, along with physical
Space, native keyboard-device presence, helper poll
counts, and native 3D relative elapsed time.
Original `0x50A650` read wrappers also maintain
diagnostic counts of *all* raw-helper invocations vs
the subset reading Space, to disambiguate lack of
key polling from a particular key not being polled.
No new game hook, synthetic key, scene skip, game
global write or controller-7 exit has been added.

F12 remains fail-closed after verified Bink movie #4:
a positively identified original scene/completion owner
and playable-ready gate are still required before
the user's automatic two-3D-cinematic skip can be
safely implemented. PR remains DRAFT/unmerged.

---

## 2026-10-23 — Wine 15:19:58 log: prior Space gate disproved, central key source traced

Uploaded `SpiderMan2k1Fix(20261023-151958).log` (Windows
x86 build #666 under Wine) confirms the original F12-on-comic
300-tick expiry + Bink movie #4 native shutdown in **1 ms**.
Physical SPACE events in original 3D occurred at **+1278 ms**
and **+2643 ms**. HUD instruction geometry appeared immediately
after the second event. The `space_gate_bit100` raw signal
`0x02E0988C & 0x100` was continuously **0** throughout the
observed 3D session. No `DEBUG-3D-SPACE-GATE` line was produced;
the original `0x455427/0x455444` gate is **NOT the routine
used to skip these two intros** in this run. Likewise the
five earlier instrumented `0x50C180` callsite masks returned
0 across both Space presses. Later player-like mode transitions
(`0x5FAECC` to 1 and then 3) occur **long after the key
events** and cannot label either intro.

**Next passive attribution:** The SHA-matched original EXE's
`0x50A650` key helper calls `0x501CB0` in two separate
branches, at `0x50A65D` (original event mode) and
`0x50A674` (original level mode). Unlike the previous
`0x455427/0x455444` guard, these are the low-level
keyboard helper's CENTRAL original raw-read sites and may
be reached from many game and script owners. Signature-verified
call wrappers return the original raw read exactly once
without any key injection. **ONLY while inside the confirmed
post-Bink New Game 3D owner** and with Debug Log enabled,
the wrapper records calls where DIK=0x39 (SPACE), their
original caller return VA, original mode, key result,
physical SPACE status and 3D-relative time, bounded
to 100 lines. The actual original caller identity will
help locate whichever scene/script routine really reads
Space; absence of records would instead falsify that
source path. The x86 original `0x50A650` body has no
prologue or stack adjustment before these CALLs, allowing
the wrapper to observe the caller return address through
the unchanged stack, without dereferencing game pointers.

**This remains strictly diagnostic**. The exact two 3D
cinematic skip mechanisms and a verifiable playable-ready
signal are not yet mapped; F12 after movie #4 still fails
closed and is NOT equivalent to SPACE yet. New log key:
`DEBUG-3D-RAW-SPACE` (`origin`, `internal`, `mode`,
`raw_code`, `physical_space`). Suggested next Wine
test: F12 on New Game cover, manually press/release SPACE
once in Stan Lee intro and once in the following cinematic,
then control Spider-Man briefly and upload the entire log.

---

## 2026-10-23 — Wine 15:01:05 log + exact original DIK_SPACE gate found

The log `SpiderMan2k1Fix(20261023-150105).log` validates native
comic + Bink #4 completion again in **1 ms**, with direct physical
Space edges during 3D at **+2478 ms** and **+4401 ms** and HUD
instructions immediately after the second edge. The five
`0x50C180` masks probed in prior CI #664 returned 0 on both
physical Space key events. They are **not confirmed** as the
cinematic Space-skip input handler. Later `0x5FAECC` values changed
from 0 to 1 at ~+19.9 s and 1 to 3 at ~+21.9 s,
**well after** both Space presses; treating them as the two
cinematic scene boundaries would be a false inference.

More precise static inspection of the supplied original executable:
`0x50C180` tests mask bit `0x04000000` at `0x50C442`;
the positive branch calls `0x50A650(DIK_SPACE=0x39,...)`
at `0x50C44B`. None of the five earlier probed 3D mask sites
passes this mask bit. Separately, inside ORIGINAL `0x455400`:
`0x455400` checks game flags `0x02E0988C`;
`0x455415` tests bit `0x100`; ONLY when that bit is set
does the game enter a direct Space-release/Space-press gate:
`0x455427: call 0x50A650(0x39,0)` (repeat while held),
then `0x455444: call 0x50A650(0x39,0)` (wait for press
or bit-clear), followed by original `0x5111D0` resume.
This is **real native SPACE polling** but its relationship to
Stan Lee / the second 3D cinematic is NOT yet confirmed.

A second **passive** exact-signature pair of 5-byte CALL hooks
now probes both `0x455427` and `0x455444` only in
`[Debug] F12TestMode=1` + `Log=1`.
Each wrapper calls original `0x50A650(0x39,0)`
**exactly once** and returns its result unmodified.
Bounded `DEBUG-3D-SPACE-GATE` logs report which original
site, real keyboard result, physical key state, native bit
`0x100`, key-down edge count and 3D elapsed time.
Periodic read-only 3D phase snapshots now include
`space_gate_bit100`. A signature mismatch leaves both
original calls untouched. No synthetic input, forced
scene progression, native memory writes or gameplay
exit gate reactivation.

The final one-F12 two-3D-scene skip is **still unimplemented**
pending confirmation that these exact original Space reads
occur at the two original scene boundaries and a separate
positive playable-ready discriminator. Requested validation:
New Game F12 on comic, one real Space press/release in Stan Lee
narration, one real Space press/release in second cinematic,
briefly play, send complete log with
`DEBUG-3D-SPACE-GATE`, `DEBUG-NEWGAME-3D-PHASE`.
PR #26 remains draft/unmerged.

---

## 2026-10-23 — Native 3D original-input attribution before enabling F12 cutscene skip

The supplied SHA-verified original executable shows that the native 3D
gameplay tick `0x45553B` calls `0x440EF0`. That game routine
consults the ORIGINAL `0x50C180` input handler at the following
SHA-confirmed callsites: `0x440F34` (mask `0x40040`),
`0x440F77` (dynamic `0x40000`/`0x40040`),
`0x440F8E` (mask `0x10010`), `0x440FAF` (mask
`0x20220`) and `0x441606` (mask `0x100`).

**Important:** a static mask and a physical SPACE key press have not
been proven equivalent. Nor does the common 3D tick identify either
of the two cinematic scenes or the first playable game frame.

The new **read-only** native-input diagnostic is installed only with
`[Debug] F12TestMode=1` AND `Log=1`, after all five exact instruction
signatures are verified. It wraps all five calls with original ABI
and invokes `0x50C180(mask,1,1)` **once per call**, returning the
unaltered native result. It samples physical Space-down, native mask,
actual input result, the game's own `0x5FAECC` phase mode,
`0x5FAED0` phase object presence, `0x6B468C` script callback slot
presence, `0x66126C` event marker and original engine flags.
The existing periodic `DEBUG-NEWGAME-3D-PHASE` snapshot also
records these extra game-owned scalar variables.

This hook does **not** emit SPACE, alter game memory, activate any
scene, increment any native skip counter, or unlock full-game F12.
In the known post-Bink New Game 3D session, the existing
fail-closed F12 EXIT guard remains untouched.

**Next validation:** compile Windows x86; run New Game, press F12
on the comic, then once during Stan Lee narration press/release
physical SPACE, then another press/release during the second 3D
cinematic, and finally briefly play the level. Provide log
`DEBUG-3D-NATIVE-INPUT` plus `DEBUG-NEWGAME-3D-PHASE`.
We will compare original mask results and phase object transitions
at each boundary before injecting native completion or unlocking
the gameplay EXIT action. Auto-skipping the two 3D scenes and the
positive playable-ready boundary are **not yet implemented**.

---

## 2026-10-23 — Original executable disassembled: direct F12 comic timer expiry

The user supplied the unchanged original `SpideyPC.exe` (1,507,328
bytes, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`).
The original code itself resolves the previously unknown first New
Game comic owner: original `0x427910`, reached from the native
scene-script interpreter at `0x4E0461`.

In that owner, `0x42796D: push 0x549214` supplies the literal
`L1COV.bmp` to original `0x4151D0`; `0x427983` saves the initial
native clock in ESI and the code loops through original
`0x505720` frame update + original `0x50C180` input polling.
The initial clock reading for each later wait check is precisely
`0x4279B8: mov edx,[0x6B4CA0]` (6 bytes).
Original `0x4279BE: sub edx,esi`, `0x4279C0: cmp edx,0x12c`,
`0x4279C6: jae 0x4279f0` is the true native timer-expired exit.
`0x4279F9: push 4` then invokes native Bink movie #4 at
`0x4279FB`. No separate comic-specific cleanup is jumped over:
the hook returns into the ORIGINAL threshold compare.

**New branch code:** only with `[Debug] F12TestMode=1`, verify exact
bytes of the L1 cover owner (`68 14 92 54 00`), the Bink #4
call (`6A 04 E8 50 8D 04 00`), and the original clock-read
(`8B 15 A0 4C 6B 00`). Replace ONLY that original six-byte
MOV with CALL rel32 + NOP, whose naked wrapper normally returns
the actual game clock in EDX. When F12 has been explicitly latched
for an active comic (postmovie_state=4), with main menu seen,
no Bink video and no prior movie #4 owner, the wrapper returns
ESI+0x12c in EDX. Game-owned SUB/CMP/JAE then handles normal
timeout-to-video continuation. The global clock remains untouched.
The same latched F12 subsequently completes exact Bink owner #4
through the already validated native tick path.

If any signature fails, the hook is not installed and F12 retains
its older fail-safe deferred-movie behavior, rather than patching an
unknown 2001 executable. This still needs runtime validation on
Windows 11 and Wine. The separate **two SPACE-skippable 3D
cinematics** remain protected from controller-7 F12 exit until the
original playable-ready gate has been positively identified.

**Proposed test:** F12 soon after first L1 comic appears. Expect
`DEBUG-NEWGAME-COMIC F12 armed`, then the Bink owner and
`DEBUG-NEWGAME-COMIC-FINISHED` with elapsed time far under the
previous ~4,041 ms. Movie #4 should be skipped, with Stan Lee's
3D scene then shown. Use Space twice to pass both distinct 3D
intros and confirm normal playable input. F12 still must NOT
quit during either unidentified 3D scene. Full log required.

---

## 2026-10-23 — Corrected New Game scene sequence and native SPACE diagnostics

**User-confirmed original sequence:**

1. New Game comic cover: a timed static page, not skippable with Enter.
2. Bink cinematic: **Enter** can skip this movie.
3. First 3D scene: Stan Lee's narrated introduction, **Space** skips it.
4. Second 3D cinematic: **Space** skips it independently.
5. First level becomes playable **only after both 3D scenes**.

This replaces the earlier mistaken claim that Enter skips Stan Lee's
3D narration. A *single* requested F12 must ultimately skip both 3D
scenes by their ORIGINAL native Space mechanisms; a single observed
Space press cannot be treated as proof of reaching gameplay.

**Now implemented:** the read-only 3D owner diagnostic samples
`GetAsyncKeyState(VK_SPACE)` rather than `VK_RETURN`, records
`space_down` and a bounded passive Space key-down edge count, and
logs `DEBUG-NEWGAME-3D-SPACE-EVENT` from the independent 25-ms F12
polling thread. This latter trace remains useful even if original
3D game-input callbacks are absent during cinematic playback.
Neither the number of physical Space presses nor the passage of time
is sufficient authorization to unlock F12's native gameplay shutdown.
The existing post-Bink unknown-3D guard continues to fail closed.
No synthetic input is injected and no scene resources are modified.

**Test sequence:** F12 on the comic or Bink; during the first
Stan Lee 3D introduction **press Space once, release it**, then
during the SECOND 3D cinematic **press Space again, release it**,
then wait for the first level to be demonstrably playable.
Press F12 one last time to verify that this experimental build
does not accidentally exit during an unidentified stage. Provide
the entire log and note the visual screen/state around each press.
Identifying the actual original Space handlers and native
playable-ready signal remains the next implementation step.

---
## 2026-10-23 — New Game F12: native movie skip + fail-closed 3D phase tracing

**Current code, partial implementation (F12TestMode=1 only):**

- Identifies original New Game Bink movie #4 by original tick return
  0x470822/0x470864, movie-owner return 0x427A00 and allow-skip=1.
- F12 on the comic-render candidate keeps the comic timer and defers
  a one-shot skip until movie #4 begins. F12 during movie #4 requests
  the same owner-verified Bink completion and game-owned teardown
  (no controller request 7 or process exit).
- Movie #4 completion is observed even if the user pressed Enter rather
  than F12. After that movie the game plays **two distinct 3D scenes**,
  each skippable with **Space**. A throttled read-only trace samples
  the native mode/task/async, input flags, timer and pause-context,
  along with Space presses and their observed key-down edges.
- Crucial conflict guard: after movie #4, the Stan Lee 3D narration,
  the SECOND 3D cinematic and actual playable-level state are **not yet distinguishable**
  using a positively verified scene flag. Therefore F12 **fails closed**
  rather than running native request 7, for *both* this introduction and
  later gameplay in the same unclassified New Game session.
  This is deliberate: F12 cannot accidentally quit during narration.
- No synthetic keys or DirectInput/WM_CLOSE injections. The existing
  native gameplay-exit code is retained for future re-enablement once
  a genuine game-owned playable-ready condition is identified.
- A comic may be detected only if the native comic candidate renderer
  is instrumented; if not observed, F12 is not force-applied globally.

**Test next on Windows or Wine:** enable `[Debug] F12TestMode=1`
and `Log=1`, start New Game, press F12 during the comic or Bink,
then **press Space during Stan Lee's 3D narration, release it, and
press Space AGAIN during the next 3D cinematic**. After the first
level is clearly playable, press F12 separately to confirm the
unclassified guard does not exit; attach the entire log.
The `DEBUG-NEWGAME-3D-PHASE` traces are needed to compare original
3D state around BOTH Space presses and select a defensible native
playable-ready condition. Until then, this is *not* a completed
one-press skip-to-gameplay feature.

This change is only on draft PR #26; user validation is required and
nothing is merged.

---

## 2026-10-23 — Decision: restore original menu clock waits

The first-menu clock-wait bypass experiment and its INI switch were
removed from the current branch at the user's request. With
`[Debug] F12TestMode=1`, F12 still advances pre-menu videos and native
static pages to the main menu, but calls to original `0x4E5D60(1)`
are never skipped. A 3–4 second first-menu wait may therefore occur.
The separate first *timed still* acceleration remains enabled in test
mode. Passive clock-call diagnostics still invoke the original once.

Historical investigations below refer to previous, withdrawn
experiments and are not current INI instructions.

---

## 2026-10-23 — New Game submenu freezes 3–4 seconds after clicking: passive selection-exit wait trace

After validation of automatic `F12SkipInitialMenuWait` with
`F12TestMode=1`, the user reported a **second ~3–4-second
freeze** when selecting **New Game** from the now-responsive
main menu, *before* a submenu appears and becomes responsive.
No fresh runtime log for this second transition has yet
been submitted.

**Source-supported first hypothesis:** the accepted main-menu
fix intentionally stops bypassing its clock waits permanently
as soon as the native clock progresses once. Its bypass is
restricted to the two per-frame menu callsites
`0x49415B` and `0x49416F`. Disassembly of the exact
SHA-verified original 2001 `SpideyPC.exe` shows a THIRD
call to the SAME original `0x4E5D60(1)` wait, but
at `0x494209`, inside the **native
`Shell_MainMenu` selection/cleanup/return** path:

```asm
004941D5: ... original menu item selection path
00494202: call 0x4437E0       ; original menu transition housekeeping
00494207: push 1
00494209: call 0x4E5D60      ; original one-tick wait
0049420E: ... rest of original selected-action cleanup
00494256: ... return native menu action
```

Because the current first-menu bypass state is
permanently `2` after restoration and because
`0x494209` was NOT hooked by the earlier bypass,
it is plausible that a **fresh native clock stall**
on menu selection causes the reported second freeze.
This is a hypothesis; no timing measurement yet shows
a slow `0x494209` call. If its elapsed time is near
zero, the delay must be sought in another transition
function or the next submenu's per-frame waits.

**New diagnostic-only change (new Windows build):**
install a third **exact-opcode-verified** call wrapper
at `0x494209`, expected bytes `E8 52 1B 05 00`,
when `[Debug] F12TestMode=1, Log=1` and the earlier
two probes install. The wrapper ALWAYS calls original
`0x4E5D60(ticks)` exactly once, and reports:

```text
QUICK-EXIT-F12: DEBUG-MENU-EXIT-CLOCK-WAIT site=0x00494209 call=... elapsed_ms=... wait_arg=1 native_clock_before=... native_clock_after=... init_bypass_state=... [ORIGINAL WAIT CALLED; READ-ONLY; SELECTED ACTION UNKNOWN]
```

Logging is bounded to the first 8 such calls, plus
any later call exceeding 100ms. There is **NO**
skip of this third wait, modification to native clock,
input injection, resource-lifetime bypass, F12 action
change, or assumption about which particular menu
item was selected. Signature mismatch leaves the
original `0x494209` call untouched and does not
disable the already validated F12 behavior.
Its restoration is independent of the original two hooks.

**Next test:** install the updated DLLs while keeping
the same INI (`F12TestMode=1`, no active
`F12SkipInitialMenuWait=0`, `Log=1`). Press F12
during an opening video, let the main menu appear,
then click **New Game exactly once**, observe the
second pause and upload the complete log. Compare
the new `DEBUG-MENU-EXIT-CLOCK-WAIT elapsed_ms`
with the reported 3–4s. Do not change/bypass
the selected-action transition until its native
blocking site is confirmed.

This remains within draft, **unmerged PR #26**.
The full native F12 exit and the pre-clock-workaround
debug mode are independently backed up.

---

## 2026-10-23 — F12TestMode automatically enables the initial-menu wait fix (explicit opt-out)

**Updated configuration semantics:** `F12TestMode` is OFF by default.
If enabled, the previously validated `F12SkipInitialMenuWait`
workaround is **ON automatically** unless the user's actual
`SpiderMan2k1Fix.ini` contains an **active**
`F12SkipInitialMenuWait=0` line in `[Debug]`.
An absent key uses the default value `1` only inside the
`F12TestMode=1` initialization branch.

```ini
[Debug]
F12TestMode=1
Log=1
;F12SkipInitialMenuWait=0  ; uncomment to opt out
```

The effective truth table:

| `F12TestMode` | `F12SkipInitialMenuWait` INI | Initial-menu acceleration |
| --- | --- | --- |
| Missing or `0` | Any or missing | OFF |
| `1` | Missing | ON automatically |
| `1` | `1` | ON explicitly |
| `1` | `0` | OFF explicitly |

Both the generated example INI and the virtual template now
**comment out** the override. This distinction matters: a
previously generated INI with active `F12SkipInitialMenuWait=0`
will retain that opt-out until its line is commented out or
removed. `Log=1` is optional for behavior but recommended
for reproducible test logs.

The original native wait, 4.5-second safety fallback, native
clock restoration, opening Bink cleanup, and 3D F12 exit
code are unchanged by this configuration adjustment.
The workaround was previously user-validated with build #645;
**automatic enabling is a new configuration change** and
should be verified in a fresh run. The PR remains draft
and unmerged.

---

## 2026-10-23 — Fast initial menu wait workaround: user validation on build #645

The user explicitly confirmed **"test validé !"** after
testing Windows-x86 artifact **build #645**, introducing
the separately opt-in `[Debug] F12SkipInitialMenuWait=1`
workaround (with `F12TestMode=1`). Record the fix as
**user-validated in the tested configuration**: skipping
an opening Bink video with F12 reaches a usable main
menu without the previously reported ~3–4-second
unresponsive interval.

The validation follows the requested end-to-end check
of **F12 during opening video → main menu → New Game
comic → New Game Bink → 3D → F12 native exit**.
There is **no newly uploaded diagnostic log** accompanying
this acceptance message; do not claim measured bypass
duration, a specific `NATIVE-CLOCK-ADVANCED` log marker,
frame-perfect behavior, or a new cross-platform test
from this message alone. The previously attached log
`SpiderMan2k1Fix(20261023-121645).log` belongs to
the pre-workaround diagnostic build #637; it proves the
source stall, not the success of #645.

**Default safety behavior is preserved:** the generated
INI sets `F12TestMode=0` and contains only a *commented*
`;F12SkipInitialMenuWait=0` example, so the workaround
remains OFF until debug mode is enabled. The old full F12-exit implementation
is frozen on `backup/f12-native-exit-validated-20261023`
(`4adf16aa13a867a3256f7a2f7ce506cc53b9b82a`).
The immediately preceding non-bypassing F12TestMode
implementation is also frozen on
`backup/f12-debug-menu-wait-before-experiment-20261023`
(`f00fe26f02724c924e368960c52ea47ac3aff378`).

This update only records validation. It does not
alter F12 runtime behavior, enable experimental
options for other users, or merge PR #26. PR stays
**draft/open/unmerged** until explicit approval.

---

## 2026-10-23 — Original clock wait confirmed: optional fast-menu experiment

The user's new diagnostic log
`SpiderMan2k1Fix(20261023-121645).log`
(patch `v1.0.0-c6bec84`, original executable SHA matched)
**confirms** the precise native cause of the initial menu freeze:

```text
DEBUG-MENU-TICK-TIMING frame=1 native_0x505720_ms=0
DEBUG-MENU-CLOCK-WAIT site=0x0049415B frame=1 wait_arg=1 elapsed_ms=3100 native_clock_before=429 native_clock_after=430
DEBUG-MENU-CLOCK-WAIT site=0x0049416F frame=1 wait_arg=1 elapsed_ms=0 native_clock_before=430 native_clock_after=431
DEBUG-MENU-TICK-TIMING frame=2 since_last_exit_ms=3143 native_0x505720_ms=1
DEBUG-MENU-CLOCK-WAIT site=0x0049415B frame=2 wait_arg=1 elapsed_ms=4 native_clock_before=431 native_clock_after=432
```

The ORIGINAL `0x4E5D60(1)` busy-wait invoked at the
CONDITIONAL `0x49415B` menu site consumes **3,100 ms**
of the **3,143 ms** pause between frames 1 and 2.
The next original wait returns immediately; subsequent
frames regain normal 30 FPS. This is the confirmed
blocking call. Why the *independent clock source*
does not advance for ~3 seconds before that first
increment remains under investigation. Neither the
native menu input gate nor a late DI8 hook installs
accounts for the stall.

**Initial experiment configuration (historical)** in
`SpiderMan2k1Fix.ini`:

```ini
[Debug]
F12TestMode=1
F12SkipInitialMenuWait=1
Log=1
```

This separate experiment was later **validated by the user
on build #645**. Its configuration has since been simplified:
`F12TestMode=1` now enables the same mechanism automatically,
unless `F12SkipInitialMenuWait=0` explicitly disables it.

While F12TestMode and the effective menu wait workaround are active, *only after* the
original four-movie + timed-page + native input-wait
cleanup has completed AND the actual main menu has
been entered following a debug F12 request, two
SHA-signature-checked call wrappers (`0x49415B`
and `0x49416F`) may **omit the original wait** for
the initial clock stall. The original menu, input
processing, rendering and existing DirectDraw Flip FPS
cap continue to execute normally.

- First original menu tick: snapshot native clock.
- If the clock **is still at that same value** and
  fewer than **4,500 ms** have elapsed, bypass BOTH
  original one-tick waits (else the unconditional
  wait would simply inherit the stall).
- As soon as the original clock advances on its own,
  stop bypassing and restore normal calls to
  `0x4E5D60(1)` permanently for the rest of the
  process.
- Safety fallback: after 4,500 ms with no native
  clock progress, revert to original blocking waits
  and log `4500MS-SAFETY-TIMEOUT`.
- All native shared clocks, game timers, Bink,
  DirectInput, engine flags and controller-7 exit
  state remain untouched. F12 after the main menu
  and any subsequent menus are not fast-skipped.

New diagnostic markers:
`DEBUG-MENU-FAST-WAIT armed`,
`DEBUG-MENU-FAST-WAIT bypassing stalled native sync`,
and `DEBUG-MENU-FAST-WAIT original synchronization
RESTORED reason=NATIVE-CLOCK-ADVANCED`.

**Important limitation:** this is a deliberately
reversible experiment to keep input processing
responsive *while the original timer catches up*.
It does not prove the clock source's ~3s startup
latency can be eliminated, and starting a game during
that interval may reveal additional timing-dependent
issues. Test New Game comic → Bink → 3D after F12,
as well as F12 native 3D exit. If any fault occurs,
immediately disable `F12SkipInitialMenuWait`; the
original clock wait remains intact in the old path.

**Rollback references:**
- Complete previously validated legacy F12 exit:
  `backup/f12-native-exit-validated-20261023`,
  commit `4adf16aa13a867a3256f7a2f7ce506cc53b9b82a`.
- Snapshot of the working F12TestMode **before this
  new clock bypass**:
  `backup/f12-debug-menu-wait-before-experiment-20261023`,
  commit `f00fe26f02724c924e368960c52ea47ac3aff378`.

This section records the original investigation.
Subsequent user acceptance confirmed the workaround on
build #645; the revised automatic INI behavior remains
subject to a new test. The PR is still draft/unmerged.

---

# Debug F12 fast-test mode (experimental)

**Default: disabled.** Enable in `SpiderMan2k1Fix.ini`:

```ini
[Debug]
F12TestMode=1
Log=1
```

`Log=1` is recommended to capture evidence, but is not required
by `F12TestMode`. The new single flag takes precedence over the
older experimental F12 controls in `[System]`.

## Behaviour

| When F12 is pressed | Debug action |
| --- | --- |
| Any of the four opening Bink movies | Finish game-owned Bink/video cleanup, accelerate the first timed still's original timeout condition, complete the input-wait page, and enter the normal main menu |
| First timed pre-menu still | Make the game's native timed-loop deadline expire on its next check, complete the native page cleanup and input-wait page, then show the menu |
| Second pre-menu `Press Enter` still | Complete the native `0x4A1896` input-wait page normally, then show the menu |
| Main menu | F12 ignored (no Quit) |
| New Game comic or Bink cinematic | F12 ignored to avoid unsupported scene interruption |
| Active 3D gameplay | Native game-thread request 7 and controller-7 final cleanup; quit the whole game |

In the first three cases, F12 **does not** select the native menu's
Quit action. Original cleanup happens before normal Shell_MainMenu is
entered. **New experimental acceleration:** when F12 was pressed before the
main menu, the first fixed image no longer waits several seconds.
The hook replaces only the five-byte original read of the native clock
at `0x4A1A26`. On a guarded debug F12 request, the hook returns the
*existing native deadline in EDI plus one*, so the game's unchanged
`CMP EAX,EDI; JA 0x4A1A36` takes its original timed-page destructor
and SEH return. The shared game clock, native timer setup, resource
pointers and page objects remain untouched. Without a debug F12
request, the clock read is unmodified. A hook-signature mismatch
**disables the debug mode safely**, rather than skipping cleanup.
The first page may appear for a single frame; immediate completion
without any rendering or game-thread cleanup is not attempted.

This is a test-specific behaviour, not a replacement of the previous
validated F12 shutdown implementation. If this INI switch is `0`,
the pre-existing System F12 options keep their legacy meanings.
Every F12 option, including this switch, defaults to `0` in the
generated INI. The game's known unsafe native F12 `WM_CLOSE` is still
suppressed when the legacy F12 feature is off.

## Restore the earlier F12 implementation

A frozen GitHub branch preserves **all source, options, test cases and
documents from before this experiment**:

- **Branch:** `backup/f12-native-exit-validated-20261023`
- **Pinned SHA:** `4adf16aa13a867a3256f7a2f7ce506cc53b9b82a`
- **Browse:** https://github.com/legluondunet/SpiderMan2k1Fix/tree/backup/f12-native-exit-validated-20261023

To simply run the prior code without changing the active PR:

```sh
git fetch origin backup/f12-native-exit-validated-20261023
git switch --detach 4adf16aa13a867a3256f7a2f7ce506cc53b9b82a
```

To restore only the old F12 implementation to a development branch,
after checking for local uncommitted changes:

```sh
git switch feature/f12-quick-exit
git restore --source=backup/f12-native-exit-validated-20261023 -- quick_exit_f12.c
git add quick_exit_f12.c
git commit -m "Restore previously validated F12 native-exit implementation"
```

For a **complete** return to the prior PR source tree, use the
frozen branch as the source for a fresh branch or the exact pinned commit.
Do not reset/force-push the PR until the user explicitly approves it.

## New Game Bink regression found after intro F12 skip

A Linux/Wine trial with patch `v1.0.0-aef9397` and
`F12TestMode=1` (`SpiderMan2k1Fix(20261023-112854).log`)
validated the new fast pre-menu skip: the native
`0x4A1A26` deadline hook and `0x4A1896` input-wait
completion both ran, and the actual menu remained open.
However, **New Game then showed the comic and skipped its Bink
cinematic**, returning through native controller request 7.

The cause was an **unconsumed pre-menu F12 skip request**:
`intro_quit_pending=1` remained set after the real menu
was reached. The game's `0x470750` Bink tick is used for both
the four opening movies and later New Game movie #4. The
hook saw this stale flag and returned 0 (movie finished)
before calling the native Bink frame tick for New Game.

**Fix (not yet runtime-validated):** the `spider_after_intro_cleanup`
wrapper now consumes `intro_quit_pending` (and resets the
early short-circuit diagnostic) in **debug F12TestMode only**
AFTER calling original native intro transition cleanup and
AFTER issuing the separate pending input-wait-page request.
The former F12 Quit path is unchanged. No game Bink object,
shared playback pointer, native timer, or 3D controller
logic is modified.

Expected successful regression:
`DEBUG-MENU-SKIP opening Bink request CONSUMED at native
post-intro cleanup; movie #4 tick restored for future New Game
playback` appears in log; after selecting New Game, the comic
is followed by the actual Bink cinematic before 3D gameplay,
without pressing F12 again. As usual, this must be confirmed
with a real Wine/Windows test; source assertions and cross
compilation do not prove native runtime behavior.

## Delayed menu input after pre-menu F12 (diagnostic)

A further user test confirms that with `F12TestMode=1` the
introductory videos, accelerated timed still and input-wait still
all complete correctly, and the real menu **appears**, but menu
keyboard/mouse control becomes responsive only **approximately
4–5 seconds later**. User log:
`SpiderMan2k1Fix(20261023-114209).log`
(`v1.0.0-56a5b31`).

The uploaded log confirms:
- F12 intent consumed at original `0x443AD0` cleanup; later
  movie #4 works normally
- `DEBUG-MENU-SKIP first timed-page deadline accelerated`
- `DEBUG-MENU-SKIP original 0x4A1896 input-wait TRUE completion`
- `REAL Shell_MainMenu frame callback observed`
- `DI8-RETRY: EXPERIMENTAL active at Flip #120`
- `INPUT-FOCUS` still shows the game as foreground, with no
  recorded focus loss
- New Game comic, Bink and subsequent 3D run and exit normally

**Two distinct hypotheses, not yet demonstrated causes:**
1. The original game's `Shell_MainMenu` draws before a native
   condition allows input: the exact SHA-verified x86 at
   `0x493DDB` reads DWORD `[0x54D38C]` and at `0x493DE2`
   branches to `0x4941C0` (return **before** menu input reads
   at `0x493E1F`) while the DWORD is nonzero.
   The flag is known to transition during startup and might
   already be zero when the real menu begins. Do NOT alter it.
2. The optional `[Input] DirectInputReacquire` native patch
   is deliberately installed only at Flip **#120**, about four
   seconds from the first Flip under FPSLimit=30. That timing
   may correlate with input recovery, but there is **no actual
   DirectInput error/reacquisition observed in this log**.
   Do NOT equate installation with proof that a blocked key
   read was repaired.

The new diagnostic is **read-only** and only activates after the
debug F12 shortcut has completed the original pre-menu pages.
It uses the already installed native Shell_MainMenu callback.
There is **no new executable patch**, DirectInput call, synthetic
keypress or modified engine state. It prints a bounded
`DEBUG-MENU-INPUT-PROBE` line at the first menu tick, each
native transition or game-active change, and every 15 native
menu frames up to frame 150; final checkpoint at frame 210.
Fields:
- `ms_since_first_menu`: independent wall-clock elapsed time
- `ms_since_intro_F12_latch`: elapsed from the saved request latch
- `native_transition`: original `[0x54D38C]` DWORD
- `native_menu_input_branch=GATED|ELIGIBLE`: exactly the
  original 0x493DE2 condition, **NOT proof of keyboard readiness**
- `game_input_active`: original DirectInput context-active DWORD
  `[0x6B78F0]`
- `menu_ctx` and `native_clock`: read-only supporting context

**Next comparative run:** same INI/build except updated DLLs, press
F12 during one of the four opening movies, do not Alt+Tab, then
repeatedly try moving the main-menu highlight with arrow keys.
Note roughly when it first reacts and upload the log. If
`native_menu_input_branch=GATED` persists for the delay,
the original menu condition is strongly implicated. If it is
`ELIGIBLE` throughout, investigate actual DirectInput data
availability/Acquire separately instead of bypassing any timer.
The `DI8-RETRY` activation at Flip #120 is a time marker,
not a proven input-responsiveness cause.

The frozen old F12 implementation on
`backup/f12-native-exit-validated-20261023` remains untouched.
No functional F12 or native menu changes are made for this
diagnostic. PR #26 remains draft/unmerged.

## 2026-10-23 — ~4-second visible menu input latency: original native menu tick vs frame-gap timing

The user confirmed a persistent **~4-second** input delay once the
main menu is visually displayed after pressing F12 during an opening
Bink movie (`[Debug] F12TestMode=1`). Log:
`SpiderMan2k1Fix(20261023-115419).log`, patch
`v1.0.0-56b5e4f`, exact supported game EXE SHA verified.

The preceding read-only native `0x493DD6` callback reported:

```text
DEBUG-MENU-INPUT-PROBE frame=1 ms_since_first_menu=0 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=15 ms_since_first_menu=3768 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=30 ms_since_first_menu=4282 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
DEBUG-MENU-INPUT-PROBE frame=45 ms_since_first_menu=4796 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
```

A 30-FPS loop would take ~467ms for 14 frames;
the observed first 14 inter-frame intervals took
**3,768ms**, an excess of approximately **3,301ms**.
Afterwards 15 frames took 514ms, essentially the
expected 500ms. The original menu gate `[0x54D38C]`
was **already zero at the first observed menu frame**
and remained so, with `[0x6B78F0]=1`. This rules
out a persistent nonzero `0x54D38C` menu gate
as the source of that delay. It does **not** prove
all DirectInput calls/keypresses were accepted.

The optional DirectInputRetry patch installed at
Flip #120 AFTER this first unusually slow segment.
No DIERR_NOTACQUIRED or successful native Acquire
retry was observed. Its installation cannot by
itself be identified as the causal fix or cause.

**Diagnostic-only code in build #634**: augment
the *already installed* original-game call wrapper
at `0x493DD6`. A passive entry timestamp
immediately precedes the original
`0x505720` native menu tick call, and the
existing post-tick hook collects a second timestamp.
A bounded `DEBUG-MENU-TICK-TIMING` trace for
first 20 menu ticks (and selected subsequent
ones up to 65) reports:

- `native_0x505720_ms`: time INSIDE the original
  `0x505720` native menu tick
- `since_last_exit_ms`: time BETWEEN the previous
  native menu tick's return and this tick's entry
- `wallclock_ms`: monotonic `GetTickCount` sample

This partitions the ~3.3-second excess into
**inside-native-tick** versus **between-ticks**
causes. It does not diagnose deeper internals
of `0x505720` yet; do not guess an input API
or driver root cause from frame timing alone.
All helper calls occur on the same original
render/menu thread within existing register/flags
save/restore and do not change game state.
No new EXE CALL site, DirectInput hook, timing
override, input injection, async callback, or
extra normal-game F12 behavior is introduced.

**Next runtime regression:** test build
[#634](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/39949796958)
with EXACT same INI including
`[Debug] F12TestMode=1,Log=1` and original
`[Input] DirectInputReacquire`. Press F12 in an
opening Bink, immediately try moving menu
selection after menu appears, and upload the log.
The largest `DEBUG-MENU-TICK-TIMING` entry should
locate the side of the native `0x505720` call
responsible for the wall-time gap. Once the
location is known, inspect that native game
caller's path before considering a correction.
F12 shortcut, accelerated still, restored New
Game Bink playback, native 3D exit remain
untouched; previous native F12 full-exit branch
`backup/f12-native-exit-validated-20261023`
is preserved.

## 2026-10-23 — First-menu 3,659ms gap: game-native clock busy wait identified

The user supplied `SpiderMan2k1Fix(20261023-120205).log`
(patch `v1.0.0-40136fd`, supported original
`SpideyPC.exe` SHA256 verified). The extra passive
menu-tick timing shows **exactly where** the delay occurs:

```text
DEBUG-MENU-TICK-TIMING frame=1 since_last_exit_ms=0 native_0x505720_ms=0
DEBUG-MENU-TICK-TIMING frame=2 since_last_exit_ms=3659 native_0x505720_ms=1
DEBUG-MENU-TICK-TIMING frame=3 since_last_exit_ms=30 native_0x505720_ms=0
DEBUG-MENU-TICK-TIMING frame=4 since_last_exit_ms=32 native_0x505720_ms=1
DEBUG-MENU-TICK-TIMING frame=5 since_last_exit_ms=32 native_0x505720_ms=1
DEBUG-MENU-INPUT-PROBE frame=15 ms_since_first_menu=4098 native_transition=0x00000000 native_menu_input_branch=ELIGIBLE game_input_active=1
```

**Confirmed:** the missing 3.659 seconds are **BETWEEN**
return of the first original `0x505720` menu tick and
entry of the next one, not inside `0x505720`.
The native menu gate `[0x54D38C]` is already zero,
and `[0x6B78F0]` reports game input active.
Normal 30-FPS pacing (30–45ms/tick) resumes afterward.

**New SHA-verified original x86 code finding:** the
single original menu loop `Shell_MainMenu`, after the
`0x493DD6 call 0x505720`, does frame processing and:

```asm
00494150 mov eax,[0x6B4CA0]   ; original game tick
00494155 cmp eax,ecx
00494157 jne 0x494163
00494159 push 1
0049415B call 0x4E5D60      ; conditional native clock wait
00494160 add esp,4
00494163 mov dword [0x5598B8],0
0049416D push 1
0049416F call 0x4E5D60      ; unconditional native clock wait
...
004941AA call 0x50DF80
004941BB jmp 0x493B82      ; next original menu iteration
```

The exact body of the original `0x4E5D60` is a
**busy wait on the original clock**, for one tick:

```asm
004E5D60 mov eax,[0x6B4CA0]
004E5D65 mov ecx,[esp+4]
004E5D69 add eax,ecx
004E5D6B mov ecx,[0x6B4CA0]
004E5D71 cmp ecx,eax
004E5D73 jae 0x4E5D7F
004E5D75 mov ecx,[0x6B4CA0]
004E5D7B cmp ecx,eax
004E5D7D jb 0x4E5D75
004E5D7F ret
```

**Hypothesis, NOT YET CONFIRMED:** after accelerating
the first still and reaching the real menu quickly,
the original game clock may temporarily stop advancing
and cause one of these two native one-tick waits to
consume the ~3.659s. An alternative is another caller
between the two menu ticks. No clock values were
sampled during the stalled interval in previous logs.
**Do not bypass or alter `0x4E5D60` yet.**

**Diagnostic-only build #637:** two exact-five-byte
CALL replacements at the original menu-only locations
`0x49415B` and `0x49416F`, enabled ONLY when
`[Debug] F12TestMode=1` and `Log=1`:
- Each replacement calls the ORIGINAL
  `0x4E5D60(ticks)` function once, with the original
  `ticks=1` cdecl parameter and normal return
- Logs `DEBUG-MENU-CLOCK-WAIT site=... frame=...`,
  `elapsed_ms`, `native_clock_before` and
  `native_clock_after` for the first few menu
  iterations after a debug F12 intro skip
- Exact native call bytes must match before patch
  insertion; optional probe fails **softly** on mismatch
  without disabling original F12
- No acceleration, shortening, clock writes,
  additional game-loop changes, input injections,
  DirectInput API calls or process termination
- The earlier frozen backup branch remains untouched

CI [#637](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/39951176590):
**295 tests passed**, Windows x86 DLL compiled and
artifact [#12500312978](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/39951176590/artifacts/12500312978)
uploaded.

**Next runtime control:** same INI, install #637 DLLs,
press F12 during an original opening video,
avoid Alt+Tab, test responsiveness when the
main menu first appears and upload the new log.
A ~3659ms `DEBUG-MENU-CLOCK-WAIT` record will
confirm the one-tick wait mechanism as the blocking
location. If both waits finish quickly, inspect
the original menu loop's other calls between
`0x493DD6` and the next frame; do not invent
a game clock failure.

PR #26 stays draft/open/unmerged pending explicit
approval and in-game validation.

## Regression test plan

1. On Windows 11, press F12 **once** during the first intro Bink movie.
   Expect native Bink cleanup, the timed still to complete normally,
   then the interactive main menu (do not press F12 again).
2. Repeat from the `Press Enter` page. Expect the main menu, no
   `SpideyPC.exe` process exit.
3. From real interactive 3D gameplay, press F12 once. Verify the
   window disappears and no residual `SpideyPC.exe` remains.
4. Optionally verify F12 does nothing in the main menu and during
   the New Game comic or its Bink cinematic.

All runtime changes live on draft PR #26. They have **not** been merged
and require native Windows/Wine testing before final validation.
