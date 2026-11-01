# Experimental Left Alt latch reconciliation (PR #88)

**Status: research-only, strictly opt-in. First Windows 11 + DS4
hardware validation confirms the targeted one-byte repair; full
keyboard/control regression and Wine/Proton remain UNTESTED.** Requires the exact supported
`SpideyPC.exe` SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Background: what the Windows 11 logs actually prove

The maintainer reproduced 3D Alt+Tab focus loss, with the original
DirectInput keyboard latch still registering Left Alt held while Win32
`GetAsyncKeyState(VK_LMENU)` reports it released. At least one later
**physical foreground Left Alt press/release** cleared the stale
original latch. The suspect mechanism is a missing buffered
`GetDeviceData` release event during background; this precise event
loss has **not** been intercepted and proved. The logs do not prove
the whole Alt+Tab keyboard/controller lockout is caused by Left Alt.

See `SDL3_NATIVE_SOURCE_TRACE.md` for event numbers and nuances.

## Opt-in, off by default

Add under the **existing** `[Input]` section:

```ini
[Input]
ExperimentalAltLatchRecovery=1
```

`[Debug] Log=1` is recommended to observe the experiment. The key is
recognized by INI validation, but it is **never** added to default
generated INIs. This experiment does **not** require SDL3,
`SDL3NativeSourceTrace`, a controller, or any generic-input bridge.
Disable by removing the key or setting it to `0`.

## Safety contract

- The main EXE must have passed the existing exact SHA check. A separate
  signature/layout preflight checks the image base at `0x00400000`,
  original keyboard reader code at `0x00501C56`, buffered
  `GetDeviceData` call/compare at `0x00501BC3`, x86 process, and
  the target byte's committed writable memory protection.
- **Only** the existing `FOCUS-PAUSE` code, upon confirming another
  application's foreground window for at least 150 ms, can arm a
  loss. A successful *manual* foreground wake arms a **single**
  subsequent recovery window. SDL3 `sampling=SUSPENDED`, Bink
  changes and native in-game pause alone cannot arm it.
- Each eligible sample occurs on the original Flip thread, strictly
  while the game HWND is foreground and menu or 3D is active; Bink
  and paused game states are excluded. A return window expires
  after 5 seconds.
- The original Left Alt latch must be **exactly `0x7F`**
  (stable held, not `0xFF` fresh press or `0x80` fresh release).
  Win32 must report Left Alt, generic Alt, Right Alt and Tab **all
  released** on at least **six consecutive eligible foreground
  Flip samples spanning at least 180 ms of genuinely clean key
  observations**; the original Right Alt and Tab latches must
  also be released. This timer starts on the FIRST clean
  observation, not on focus wake; intervening Bink/pause,
  physical Alt/Tab presses or original Right Alt/Tab activity
  reset the clean period.
- Final foreground, **all four Win32 modifier keys** (Left Alt,
  generic Alt, Right Alt, Tab), **both other original DirectInput
  latches** (Right Alt and Tab), and the native Left Alt byte are
  **re-read immediately before writing**. This rejects an intervening
  Tab/AltGr press after the six-frame policy decision. If all pass,
  the **only native write possible** is `0x006B7964` (original keyboard
  `0x006B792C + DIK_LMENU 0x38`), changing `0x7F -> 0x80`.
  `0x80` is the original game keyboard reader's normal *released
  edge* representation. The operation is **one-shot per confirmed
  loss/return**, even if the stale state returns later.
- Never clears the other 255 keyboard latch bytes, never touches
  mouse/joystick buffers, never calls DirectInput Acquire, never
  patches the executable, never changes keyboard mappings, and
  never sends a Win32 `SendInput`/synthetic global key up.
- If an observable fresh physical Alt press already cleared the
  original latch or if a Bink/pause makes the window ineligible,
  the policy fails closed. A physical key held during the sampling
  window must **not** be released automatically.

The original DirectInput keyboard latch can still reflect a genuine
key being held while the Windows async API temporarily differs.
That is why the feature is **optional**, relies on repeated independent
readings, and is not enabled for general users.

## Windows test protocol (not performed yet)

1. Preserve the previous `SpiderMan2k1Fix.dll` and `winmm.dll`.
   Install the PR #88 Windows x86 CI artifact, verify the game loads
   the DLL from the nested `SpiderMan2k1Fix` directory (see
   `DLL-LOAD: target` in log).
2. Enable `ExperimentalAltLatchRecovery=1` and `Log=1`; retain
   `ExperimentalSDL3Probe=1` and `SDL3NativeSourceTrace=1` if
   you want independent `alt-tab` / `os-alt-tab` diagnostics.
3. In native 3D, verify physical Left Alt and Tab work normally
   before any focus switch. Alt+Tab away, release both keys outside,
   Alt+Tab/foreground back, wake with the DS4 (or keyboard).
4. Expect `ALT-LATCH: return confirmed` and, **only** when the
   original latch remains stale and Windows shows no modifiers down
   through six eligible Flip samples, `ALT-LATCH: REPAIRED ...`.
   The passive trace should then show original Left Alt no longer
   held. Test keyboard arrows/Space and DS4 actions after recovery.
5. Repeat with **Alt physically held on focus return**; it must not
   be forcibly released. Repeat with **no focus loss** and with a
   Bink-only/pause-only eligibility transition; those must not
   trigger repair. Test manual Alt press/release immediately after
   a recovery, ensuring the original key continues to work.
6. Re-run with feature `=0` and confirm there is no `ALT-LATCH:
   REPAIRED` line. Check both Xbox and DS4 and Wine before any
   broader enablement.

If the game freezes/crashes, Alt becomes unresponsive, or a
held modifier is cleared unexpectedly, disable the experiment and
report the full log. No merges without maintainer review.

Signature: legluondunet assisted by ChatGPT (OpenAI)

## Windows 11 hardware validation: Sony DS4, 1 November 2026 07:59

User-provided **complete 1,557-line native Windows 11 log**
`SpiderMan2k1Fix(20261101-075920).log`; original retail
`SpideyPC.exe` SHA-256 confirmed, Sony DS4
`VID:PID=054C:09CC` selected. The exact build commit is **not**
proven by the static startup version string
`v1.1.0-cd6de56`, but the new runtime emits
`ALT-LATCH: ARMED`, `return confirmed`, and `REPAIRED`, proving
the optional recovery implementation is actually loaded and running.

### Observed first real focus-loss recovery

- Log **60**: opt-in `ALT-LATCH: ARMED`; original SHA and preflight
  pass. This is **not** a default-on feature.
- **721**: in the 3D game, original keyboard and Win32 both observe
  Left Alt held: `alt-tab=1 os-alt-tab=1`.
- **724**: `INPUT-FOCUS: flip-background`;
  **731–732**: the existing cross-process foreground-loss detector
  confirms an external foreground after 172 ms (`explorer.exe`),
  and latches the PAUSED wait.
- **739**: `flip-foreground`, **743–748**: manual Return keyboard
  press/release wake, eight DirectSound secondary voices resumed
  (`failed=0`), and verified foreground state. **749**:
  `ALT-LATCH: return confirmed`.
- **753**: actual original DI/Win32 divergence remains after waking:
  `kb-held=1 alt-tab=1 os-alt-tab=0 merged-actions=0000`.
- **765**: `ALT-LATCH: REPAIRED original Left Alt 0x7F->0x80;
  6 stable foreground frames; OS modifiers up`.
- **766**: the independent observer immediately sees
  `kb-held=0 kb-released=1 alt-tab=0 os-alt-tab=0`;
  **767**: the release edge is cleared naturally by subsequent
  original game state progression (`kb-released=0`).
  This validates the **actual one-byte native write and the
  original game latch's resulting transition** on Windows 11.

### Bounded outcomes and things NOT yet validated

- Across **100** source snapshots (81 in 3D, 19 in MENU):
  **98** with `alt-tab=0 os-alt-tab=0`,
  **1** with `1/1` (before switching focus) and
  **1** with `1/0` (stale original latch immediately after return).
  **96** regular + **4** reserved snapshots; the reserved samples
  at **1540–1545** follow in-game pause/menu eligibility transitions,
  not a second proven desktop Alt+Tab.
- Precisely **one** confirmed cross-process foreground loss/return
  and **one** `ALT-LATCH: REPAIRED`. No unexpected second repair,
  automatic whole-keyboard clearing or crash/error indicator is
  visible. Subsequent physical DS4 Cross/RT button edges, axis
  movements, and nonzero game action masks appear in the log.
- The game's original controller-7 transition at **1515–1519**
  completes a return to the controller/menu path, with normal
  2D presentation afterward at **1520–1525**. This is telemetry, not an assertion
  that every controller gameplay action was successful.
- **Unverified:** the user's own observation of *all keyboard
  controls* after recovery, a **new physical Left Alt press/release**
  following a repair, keyboard-only and Xbox tests, repeated 3D
  Alt+Tabs in a single run, a negative test with the option disabled,
  and Wine/Proton. The original broad Alt+Tab control-lockout may
  involve additional input state not captured here. Do not enable
  by default or merge on this single successful test.

**Conclusion:** first **runtime hardware validation succeeded
for the precise stale Left Alt byte repair**, not a blanket proof
of complete Alt+Tab input recovery.

### Maintainer's gameplay confirmation after the 07:59 test

The maintainer explicitly confirmed: **only the DualShock 4 was used**, and
**the controller worked normally after the Alt+Tab recovery**. This is
first-hand hardware feedback, distinct from the log-only correlation above.
The physical keyboard's in-game movements/actions **were not tested** in
this run. Do not mark keyboard input recovery, normal fresh physical Alt
press/release after automatic repair, or a complete fix of the historical
Alt+Tab keyboard+mouse lockout as validated. The manual wake's logged
virtual-key Return is not evidence of a user gameplay keyboard test.

## Follow-up Windows 11 keyboard + DS4 verification: 08:08 log

Complete maintainer log `SpiderMan2k1Fix(20261101-080817).log`
(**1,636 lines**), Windows 11 native, supported original SHA,
Sony DualShock 4 VID:PID `054C:09CC`, opt-in
`ExperimentalAltLatchRecovery=1` explicitly ARMED at line **60**.

**User's first-hand gameplay report:** *"rien à signaler avec les
touches du clavier, même après un alt tab"* — no keyboard-control
anomaly experienced, including after Alt+Tab. The previous hardware
trial had independently confirmed DS4 gameplay worked after Alt+Tab.
The present test adds **keyboard gameplay validation by the
maintainer**, not merely a log-only assertion.

### Two independent actual 3D Alt+Tabs and one-shot repairs

| Event | First Alt+Tab | Second Alt+Tab |
| --- | ---: | ---: |
| Original/OS see Left Alt key down | 1157 | 1225 |
| Genuine `INPUT-FOCUS: flip-background` | 1161 | 1227 |
| Confirmed external loss and focus pause | 1169 | 1233 |
| `INPUT-FOCUS: flip-foreground` | 1177 | 1241 |
| Manual keyboard wake: virtual key | 1181: `0x26` (Up) | 1245: `0x20` (Space) |
| `ALT-LATCH: return confirmed` | 1187 | 1251 |
| Old DI Left Alt held, OS released `1/0` | 1191 | 1255 |
| `ALT-LATCH: REPAIRED ... 0x7F->0x80` | **1200** | **1266** |
| Native/OS states synchronized `0/0` | **1201–1202** | **1267–1268** |

Both repair events occur after **distinct, confirmed** background
losses. Exactly two repair events appear; not a duplicate intervention
within one return window. Each repair was immediately followed by
original native keyboard `kb-released=1`, then neutral `0`.
The existing DirectSound focus handling also resumed **8/8** and
**7/7** playing secondary voices respectively, `failed=0`.

### Subsequent physical Left Alt works normally

In uninterrupted foreground 3D (no intervening
`INPUT-FOCUS: flip-background`), source records
**1440–1442** show `alt-tab/os-alt-tab`:
`1/1 -> 1/0 -> 0/0`; **1461–1468** again show
`0/1 -> 1/1 -> 1/0 -> 0/0`. These independently demonstrate
fresh physical Left Alt press/release can still reach the original
game keyboard reader **after two auto repairs**. Neither sequence
causes an extra `ALT-LATCH: REPAIRED`.

**101** passive source snapshots: 96 ordinary + 5 reserved;
86 3D and 15 MENU; pairs: **90 × `0/0`**,
**4 × `1/1`**, **4 × `1/0`**, **3 × `0/1`**.
The five `postfocus=1` reserved snapshots later in the log
belong to a native game pause/menu-context sampling transition
(lines 1591 and 1630–1635), **not** a third desktop Alt+Tab.

A native controller-7 3D-to-menu path completes at
**1605–1629**, and the log ends normally after the game has
returned to MENU (**1636**). No crash or failed audio resume
is reported.

**Scope:** Windows 11 + supported original EXE + Sony DS4,
with the experimental option expressly on, **two actual Alt+Tab
cycles**, normal physical keyboard behavior according to the
maintainer and subsequent verified native physical Left Alt
down/up. The experimental one-byte latch repair has now passed
a valuable repeatability and keyboard-regression check. It
remains **default OFF**, with the PR **DRAFT** and unmerged.
Mouse-specific behavior, Xbox 360, keyboard-only configuration,
feature-OFF comparison and Wine/Proton remain unvalidated.


### Post-validation safety hardening (requires fresh Windows trial)

After the 08:08 hardware PASS, the PR tightened two fail-closed
conditions, without changing the option's default OFF state,
target address or native released-edge value:

1. The immediate final one-byte write gate now **re-reads** every
   independent OS modifier (Left Alt, generic Alt, Right Alt, Tab)
   and both original game's Right Alt/Tab latches. A key press
   arriving after the initial snapshot but before the actual
   release is not allowed through cached boolean values.
2. The minimum **180 ms** now means an uninterrupted interval
   of clean foreground observations **since the first eligible
   clean sample**. Previously, the timer counted from foreground
   wake and could include a period spent watching Bink/pause
   or waiting for a physical modifier to be released. At 30 FPS,
   this can delay a recovery by roughly a frame or more, and can
   prevent it if no continuous 180 ms clean window exists within
   the five-second expiration.

The policy unit tests cover an intervening Tab event resetting
the clean period and a late clean first observation. These
changes are **CI-only until another Windows trial with the new
artifact**. The earlier 07:59 and 08:08 tests validated the
previous guard revision; do not retroactively claim they
hardware-tested these additional checks.


## Hardened guard Windows 11 verification: 08:20 and 08:22 logs

**Two user-provided complete native Windows 11 logs**:
`SpiderMan2k1Fix(20261101-082040).log` (1,552 lines) and
`SpiderMan2k1Fix(20261101-082259).log` (1,532 lines).
Both are the authenticated original retail game
(`SpideyPC.exe` SHA-256 matches) with Sony DualShock 4
`054C:09CC`, `ALT-LATCH: ARMED`, and read-only
`SDL3-NATIVE-SOURCE` independent original/Win32 key comparisons.

**Exact build provenance verified**, not inferred from a generic
banner: both print `v1.1.0-14009ea`. The GitHub commit
`14009ea91611044d9d82e6174c1c636fa67e07e3` is the
temporary PR test merge
**`fd8805a52096666beb4d078330aad33af7434c96` into
`0b675f1603c33a80860684d4e50e6bdbb631087f`**.
The second parent is exactly the hardened PR head whose Windows
x86 workflow #41288005297 completed successfully. Therefore
the new final modifier rechecks and 180-ms continuous-clean
window *were present in the tested binaries*.

### Three genuine externally confirmed 3D focus returns

| Log / cycle | Background | Confirmed loss | Foreground return | Return armed | Native stale 1/0 | One-byte repair | Original/Win32 neutral |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 08:20 #1 | 918 | 925 | 933 | 943 | 947 | **956** | **957–958** |
| 08:22 #1 | 720 | 727 | 735 | 745 | 749 | **759** | **760–761** |
| 08:22 #2 | 1093 | 1099 | 1107 | 1117 | 1121 | **1134** | **1135–1136** |

All three events are actual logged external focus-pause epochs,
not Bink/pause menu eligibility transitions; there are exactly
three repairs after three independent confirmed focus losses,
one repair per return. In each case the original DirectInput
Left Alt latch `0x7F` is still HELD while Win32 LeftAlt is UP
(`alt-tab=1 os-alt-tab=0`), followed by the explicit
`ALT-LATCH: REPAIRED ... 0x7F->0x80` and an independent
next-sample native `kb-released=1 alt-tab=0 os-alt-tab=0`.
The next sample clears the released edge naturally.

- Focus DirectSound paused/resumed **9/9** playing secondary
  voices (08:20), **8/8** then **9/9** (08:22), all `failed=0`.
- Original native 3D action masks change after corrections;
  joystick/source and gameplay state telemetry continue. No
  `ALT-LATCH: REFUSED`, `EXPIRED`, duplicate REPAIRED
  per focus return, or visible crash/error marker occurs in
  either bounded log. This does **not** constitute new explicit
  maintainer confirmation of subjective gameplay behavior.
- Read-only source snapshots: 08:20 **96 regular samples**
  (77 3D, 19 MENU), all `postfocus=0`, 87 `0/0`,
  3 `1/1`, 2 `0/1`, 2 `1/0`, 1 `0/4`, 1 `4/4`
  in `alt-tab/os-alt-tab` bitfield notation.
  08:22 **101 samples** (96 regular + 5 reserved;
  78 3D, 23 MENU), 88 `0/0`, 4 `1/1`,
  3 `1/0`, 3 `0/1`, 2 `4/4`, 1 `4/0`.
  `4` denotes **Tab**, not Alt Left. A transient `4/0`
  or `1/0` while the foreground player physically presses
  and releases keys can reflect asynchronous buffered/native
  update timing; these isolated events are NOT an extra
  confirmed focus-induced stuck-key and trigger no repair.
- After recovery, additional original/Win32 Left Alt and
  Tab physical transitions are observed in 3D (08:20
  lines 1141–1158; 08:22 lines 866–879 and 1254–1265).
  No extra repair occurred without a new confirmed
  cross-process loss.

**Result:** **PASS, narrowly scoped hardware validation of the
hardened 180-ms clean-period / final modifier-recheck version**
on native Windows 11 and Sony DS4 across 3 genuine 3D Alt+Tabs.
The log does not report the exact number of clean milliseconds
at the write itself; policy code and unit tests establish the
gating rule, and real runtime repairs demonstrate that at
least the rule allowed these three cases. Do not claim
independent timing measurement from these logs alone.

**Still outstanding before default activation or merging:**
mouse control is set **`MouseEnabled=0`** in these runs,
so mouse recovery is untested; Xbox 360, keyboard-only,
Wine/Proton, feature-OFF negative controls and varied
background durations remain unvalidated.
Keep **`ExperimentalAltLatchRecovery=0` by default**,
PR **DRAFT and unmerged**.
