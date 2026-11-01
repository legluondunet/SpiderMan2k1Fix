# Combined Xbox / mouse-off / Alt-latch runtime acceptance

**Status:** Draft PR #91 — test-only. **Do not merge.** Base PR #80,
opt-in Left Alt recovery from PR #88, permanent game-mouse removal from
PR #90. This document does **not** authorize changing protected `main`
or altering the individual draft PRs.

## Verified artifact and composition (1 November 2026)

- GitHub integration commit: `1159b11e170bfc95b95fbde8b0016b52b7f7c954`.
- Full [Windows x86 CI run #41293775028](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/41293775028)
  **PASS**: **741 Python regressions**, PowerShell audio probe,
  native x86 MSVCRT DLL compilation, packaging, bundled SDL3 and
  SHA-256 output.
- [Installable ZIP artifact](https://github.com/legluondunet/SpiderMan2k1Fix/actions/runs/41293775028/artifacts/13079045133):
  `SpiderMan2k1Fix-Windows-x86`, GitHub artifact ID
  `13079045133`. This is **not** a release.
- Source tree audit: **286** non-overlapping files identical to
  latest base PR #80; **40** identical to PR #88; **8** identical
  to PR #90. No unexpected file blob substitutions; only
  `ini_validation.h` is consciously reconciled to retain BOTH
  `ExperimentalAltLatchRecovery` and the obsolete/ignored
  `MouseEnabled` settings. Added only
  `ChatGPT/tests/test_integration_mouse_alt_latch.py`.
- Tested separate DLLs **do not establish hardware acceptance**
  for this new combined binary. Full original
  `SpideyPC.exe` SHA-256 is
  `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Installation and diagnostic configuration

1. With the game **closed**, back up existing `winmm.dll`,
   `SpiderMan2k1Fix.dll` and `SpiderMan2k1Fix.ini`.
   Extract the Windows x86 artifact, install the two DLLs
   **together in their supplied layout**, preserving the existing
   INI where possible. Do not combine DLLs from different runs.
2. For diagnostic runs, use `[Debug] Log=1`.
   The startup log shows `SpiderMan2k1Fix v1.1.0-...`;
   a GitHub PR workflow can build a **temporary test merge**
   with a *different* SHA from the branch head. Verify the
   GitHub commit parents of that SHA rather than assuming
   an exact prefix.
3. On **native Windows 11**, to test the already-hardened
   one-byte Left Alt fix, deliberately opt in:

   ```ini
   [Input]
   ExperimentalAltLatchRecovery=1
   ```

   Without this key (or with `=0`), it stays **OFF**.
   Confirm `ALT-LATCH: ARMED` at startup when opted in;
   do not infer `REPAIRED` must always occur.
4. To test legacy configuration compatibility,
   the same `[Input]` section may contain
   `MouseEnabled=1`. The game mouse **must still remain
   disabled**; expect a log saying the obsolete setting is
   ignored. **No new generated INI** offers this old toggle.
5. Set `SDL3GameplayTrace=0` and `Xbox360TriggerProbe=0`
   in `[Debug]` for ordinary sessions; the 64-sample
   controller trace and raw trigger probes are for targeted
   tests only.

## Windows 11 acceptance checklist (strict focus regression)

- **Menu, intro, playable 3D, game Pause, back to menu** work
  with keyboard alone; normal audio, videos, game rendering,
  30-fps behavior, and full-screen borderless remain intact.
- Test an actual **external application Alt+Tab** while 3D:
  the existing focus handler should enter its custom
  PAUSED state when it confirms real external focus loss;
  the game resumes on the intended input, without a
  crash, frozen controls, stuck keyboard Alt, or stuck
  controller buttons. Verify in-game keyboard and
  controller actions **after** return.
- Opt-in Alt experiment: a `REPAIRED ... 0x7F->0x80`
  record occurs **only if** the original game actually has
  a stale native Left Alt latch after an eligible confirmed
  focus loss and return. `SKIPPED`, `EXPIRED` or
  **no repair** can be expected for clean returns and
  are **not** themselves a failure. Must never neutralize
  physical Alt, Tab or AltGr.
- Test supported **Sony DS4** and **Xbox 360** separately.
  Ensure Xbox **RT triggers Swing and releases cleanly**,
  LT does not trigger Swing, Back opens normal game
  Pause, Start/selection does not leak to the other
  controller, and mapped buttons perform intended
  actions. Simultaneous DS4+Xbox handoffs are a
  **separate**, more demanding test and not proven by
  single-controller acceptance.
- In-game mouse cursor, mouse movement and clicks
  remain **nonfunctional**, even with old
  `[Input] MouseEnabled=1`. The **Windows desktop
  cursor must remain visible/usable after Alt+Tab**.
- Attach full `SpiderMan2k1Fix(...).log` and note
  observed physical behavior, not just detected HID IDs.
  One or more successful manual cycles is needed before
  considering an approved combined build.

## Wine/Proton acceptance checklist (tolerate cosmetic focus loss)

- Validate keyboard, Xbox 360 and/or DS4 gameplay, normal
  game Pause, mouse permanently OFF in game, and visible
  OS pointer **outside** the game.
- After changing applications, check whether the **game
  still accepts keyboard/controller commands, sound and
  graphics remain normal, and no crash/hang occurs**.
  **A missing automatic background `PAUSED` overlay is
  explicitly NONBLOCKING if the return is functionally
  stable.** Do not add forced focus, speculative hooks or
  input injection just to display a ribbon.
- If Wine does not report a real background transition,
  **do not count it as an Alt-latch recovery test**:
  `ExperimentalAltLatchRecovery` depends on the original
  confirmed-pause/return path, and must not mutate keys
  on unobserved or spurious focus changes.
- Reopen this issue only if a reproducible *functional*
  regression occurs: keyboard/controller stuck,
  missing audio, broken cursor on desktop, graphics
  lost, or crash. Capture the full log and actual
  reproduction conditions.

## Evidence boundaries

Previously confirmed **separately**:
- PR #88 Windows 11: **three successful Left Alt repairs
  after true 3D external focus returns** on a hardened
  code revision, with original game keyboard states
  observed to clear; the experiment stayed default-OFF.
- PR #90 Wine 11.0: in-game mouse suppressed, old
  `MouseEnabled=1` ignored, normal desktop cursor
  after Alt+Tab, including a background interval over
  four minutes, no visible game instability.
- PR #80 Wine 11.0: Xbox 360 physical gameplay
  confirmed by maintainer, native SDL3/DI identity,
  trigger RT down/up, game Back/Pause and exit.
  A missing custom focus PAUSED overlay was accepted
  by the maintainer because the game continued normally.

**Combined PR #91 has now passed maintainer-observed Windows 11 native tests as well as prior Wine/Proton runtime tests; see the new dated acceptance report below.** CI and identical source blobs alone were not considered sufficient.

legluondunet assisted by ChatGPT (OpenAI)


## 1 November 2026 — combined PR #91 Windows 11 native acceptance PASS

**Maintainer first-hand result: 20/20 user-observed checks PASS across all
six prescribed test scenarios**: main menu, four intro videos, 3D gameplay
focus loss, Xbox 360 alone, Sony DualShock 4 alone, and both controllers
connected with selection handoff. Normal pause menu, return input,
the visible/useable **Windows desktop mouse**, absent in-game mouse
control, rendering and audio after return were all personally confirmed.
These real-world observations supplement, rather than derive from,
the instrumentation below.

### Exact test artifact, host and engine

- Full runtime: `SpiderMan2k1Fix(20261101-103438).log`, **2,646
  lines**; runtime version **`v1.1.0-72e2f2a`**, the previously
  confirmed GitHub test-merge artifact from PR #91 head `f14006f6`.
- **Windows native**, Win32 version **10.0 build 26200**, explicitly
  **not Wine or Proton**; Ryzen 7 PRO 8840U / Radeon 780M,
  one 1920×1200 60 Hz screen (**16:10**, meaningful aspect coverage).
- Exact supported original `SpideyPC.exe` SHA-256 verified,
  **0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea**.
- Permanent in-game mouse gate armed and `GAME MOUSE OFF (permanent)`
  (**362–363**); obsolete `MouseEnabled` parsed but ignored
  (**44–45**). Desktop cursor visible on all nine external
  background snapshots; maintainer confirmed its physical usability.

### Focus, native Left Alt and audio

- **Nine real, confirmed external focus-loss cycles,
  nine explicit controller press/release-driven resumes**:
  confirmed loss lines **542,722,826,1057,1680,1714,1747,1834,1945**;
  resumption lines **560,739,844,1073,1698,1732,1765,1852,1961**.
  Duration as measured by `paused-after-ms`, respectively:
  **9,578 / 2,828 / 2,110 / 27,281 / 3,531 / 12,562 /
  28,828 / 1,656 / 1,547 ms**. Every cycle logged
  custom `FOCUS-SURFACE` presentation and wake.
- **Six `ALT-LATCH: REPAIRED original Left Alt 0x7F->0x80`**
  at lines **573,802,857,1087,1862,1974**,
  after an eligible confirmed focus return, with
  six stable foreground frames and OS modifiers released.
  All repairs are **actual one-byte native latch changes**,
  not `SKIPPED` observations. The maintainer confirms no
  trapped keyboard Alt or other key afterward.
- **Nine DirectSound pause/resume calls with failed=0**.
  Four had actual playing voices stopped and restored:
  **7/7**, **9/9**, **7/7**, **14/14**; **37/37
  total**. Five other cycles had no actively playing voice
  to restore (0/0, not failures).
- **Seven Bink `pause-result=1` / `resume-result=0`
  pairs**. The maintainer verified appropriate video
  freeze/resume including the four introductory clips.
- Normal native **3D owner entered and exited three
  times**, with original `request=7` teardown
  completed and main menu restored at the end.
  No fatal/error marker in the supplied log.

### Both hardware controllers and original mapping

- Both **Sony DualShock 4 `054C:09CC`** and
  **Xbox 360 `045E:028E`** present in one
  native-Windows runtime. Successful selected native
  DirectInput rebound **Sony id=1** (**458**)
  and subsequent **Xbox id=2** (**2345**).
  Exact RAM-only canonical 7-action profile swap
  **Xbox360→DS4** (**457**) and back **DS4→Xbox360**
  (**2344**) verified.
- Xbox **two RT-SWING DOWN/UP pairs**
  (**2463/2469** and **2526/2529**) with
  SDL3 RT=32767 and DirectInput `lZ=128`.
  `Back -> Escape` game-pause markers also present;
  physical button/joystick correctness and no
  competing inputs are confirmed separately by the
  maintainer's 20/20 checklist.
- Note: device enumeration in this **single** log
  shows both physically connected for the recorded
  runtime. Standalone controller test results are
  separately **reported by the maintainer**, not
  independently established by an unambiguous
  single-connected-device log.

### INI auto-repair cause identified — resolved; no code change

The user provided the **exact backed-up** `SpiderMan2k1Fix.ini(3).old`
(120 physical lines) and regenerated/current
`SpiderMan2k1Fix(20261101-104154).ini` (118 lines).

The previous INI includes **two conflicting instances of the
obsolete, permanently ignored** `[Input] MouseEnabled`:
**line 34 `MouseEnabled=0`** (left from an older generated
template with obsolete mouse-restoration comments) and
**line 79 `MouseEnabled=1`** (appended during intentional
negative migration testing). The exact C validator
`ini_validation.h` lines 282–287 rejects any duplicate
section+key with **one hard error**. Both values are
individually valid booleans; the duplicate alone explains
`INI-REPAIR: fixed 1 error(s), 0 unknown` at the 10:34
startup. The former file was backed up; 54 recognized
settings recovered. The repair generated one
`MouseEnabled=0`, preserving the **first** value and
removing the conflicting later key; the runtime disables
the game mouse regardless of either value.

A **new native Windows session**
`SpiderMan2k1Fix(20261101-103731).log` (747 lines)
explicitly reports **`INI-CHECK: valid SpiderMan2k1Fix.ini`**
at line 12. This verifies the duplicate error is no longer
present; **no change to the strict duplicate-key validation
is warranted**. The earlier suggestion to append an old
`MouseEnabled=1` without removing the preexisting key
caused this test-configuration conflict; future negative
tests should **replace the existing line or append it
only if not already present**.

The current user INI also differs from the backup in
**`MusicSFXLevel=68→49`, `Stereo=0→1`,
and four joystick action values (Jump, Kick, Punch,
Swing)**, plus corresponding **RawHex**. The files
alone **do not date or attribute** those later writes
to the auto-repair versus in-game configuration edits,
so do not claim all previous user settings stayed
byte-for-byte identical or silently revert current
controller bindings.

#### Supplemental 10:37 native Windows input smoke test

This later log is an **intro/menu-only** test; **five
confirmed focus-pauses, five resumes** in foreground,
**four Bink pause/resume pairs**, one real safe
`ALT-LATCH: REPAIRED ... 0x7F->0x80` at line 681,
permanent game mouse OFF, and OS cursor visible on
background. **DSOUND had zero active voices** in
these pauses and thus cannot independently add to
the previously validated 37/37 playing-voice restores.

Hardware events: original **DS4 `054C:09CC`**
initially alone; the **Xbox 360 `045E:028E`**
appeared and was selected by SDL3 while still in
intro/menus; native owner switched DS4→Xbox at
**605–606**. Later one secondary DirectInput
`INPUTLOST` (HRESULT `0x8007001E`, **698–702**)
was handled without unsafe Acquire: the stale
secondary device was released, SDL3 detached Xbox,
**native owner rebound to DS4 at 709–710**, and
Xbox was rediscovered as SDL3 ID 3 at **718–720**.
This is useful **recovery telemetry**, not by itself
proof of flawless gamepad physical behavior after
all hotplugs; the log ends in the menu, without 3D
owner evidence or a normal process exit.

### Status

**Windows 11 native combination acceptance PASS** for
the hardware and 16:10 setup tested, together with
prior Wine/Proton practical acceptance. No functional
failure or reason to change runtime/gameplay code
emerged from these tests. Windows focus-paused behavior
is strictly verified here; the independently approved
harmless missing-PAUSED-ribbon tolerance under Wine
remains unchanged. No claim of exhaustive GPU, multiple
controller vendors, aspect ratios or configurations.

**PR #91 remains DRAFT and unmerged; #80, #88, #90
remain separate. A merge still requires explicit
maintainer approval.**

legluondunet assisted by ChatGPT (OpenAI)
