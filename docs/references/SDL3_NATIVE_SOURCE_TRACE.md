# PR #88 — passive original keyboard/joystick source correlation

**Research only.** This optional trace observes independently maintained
original DirectInput latches without changing them. It is **not** a
generic SDL3 controller bridge, and does not provide DS3 or Steam Deck
gameplay support. No added Poll, GetDeviceData, Acquire or input hooks.

## Evidence and motivation

In the maintainer's three Windows 11 logs (keyboard with idle DS4,
Xbox 360, DualShock 4), the earlier `SDL3-NATIVE-STATE` observer
recorded 64/64 changes in each run. Those 20 original engine action
records are already **merged** across keyboard and native joystick;
the logs cannot uniquely attribute actions to one physical source.
A selected SDL3 device is not necessarily the same original DI owner.

## New, separately enabled passive diagnostic

```ini
[Input]
ExperimentalSDL3Probe=1

[Debug]
Log=1
SDL3NativeStateTrace=1
SDL3NativeSourceTrace=1
```

`SDL3NativeSourceTrace` defaults **OFF**, regardless of whether the
previous native-state observer is on. Log marker: `SDL3-NATIVE-SOURCE`.

* `kb-held`, `kb-fresh`, `kb-released`: **counts only** across the
  authenticated 256-byte original keyboard DirectInput latch at
  `0x006B792C`; individual key codes and typed sequences are **not
  recorded**.
* `joy-held`, `joy-fresh`, `joy-released`: 32-bit bitsets across the
  original joystick's 32-button latch at `0x006B7A34`. Bit positions
  represent **native** button indices, not SDL3 semantic button numbers.
* `merged-actions`: an already-merged original game HELD action mask
  read from the twenty existing records; **not source attribution**.
  Changes to this mask now also trigger a row, even if no physical button
  latch changes (for example a stick/POV direction). This alone does not
  prove a continuous analog source or device ownership.
* `alt-tab`: only the original DirectInput held states for Left Alt
  (bit 1), Right Alt (bit 2), Tab (bit 4); **no ordinary keycodes or
  typed characters**. Because the game stores key latches, a set bit
  is not necessarily proof that the physical key is still depressed.
* `os-alt-tab`: an independent, foreground-only Win32 `GetAsyncKeyState`
  high-bit snapshot using **only** `VK_LMENU`, `VK_RMENU`, and `VK_TAB`,
  with exactly the same bit layout as `alt-tab`. It is a separate API
  observation, **not** proof by itself that the user is physically pressing
  a key (focus/layout, injection, keyboard hooks and system semantics can
  differ). When native `alt-tab=1` remains 1 but Win32 `os-alt-tab=0`,
  it is strong evidence of a stale original DirectInput keyboard latch.
  OS changes to these three bits also count as an observed change.
* `postfocus=1`: one of up to **four consecutive eligible Flip snapshots**
  after an observed suspended-to-active transition **once the normal
  96-change cap is exhausted**. Up to **eight** such snapshots total
  may be recorded (e.g. four after each of two returns), including
  unchanged states, to inspect whether an Alt/Tab latch clears.
* `ctx=MENU/3D`, `sampling=ACTIVE/SUSPENDED`: context and eligibility
  transitions that actually occur while the original game Flip runs.

The supported retail EXE identity is already verified by the
SpiderMan2k1Fix loader. Before reading either source, this trace also
checks exact original x86 instruction signatures at `0x00501C56`
(keyboard latch reader), `0x00501F13` (joystick event maintenance),
and `0x00479540` (engine action-state updater). Any mismatch refuses
the new diagnostic. The trace runs only on the existing game Flip
thread, after the existing Xbox/DS4 gameplay logic. It reads no native
input state during Bink, pause, or when not foreground.

The independent budget is **96 source/game-mask changes**, plus **8
reserved postfocus snapshots** after that cap and **12 status/focus
transitions**. The status budget and reserved recovery snapshots remain
usable after 96 changes. The retained postfocus samples are consecutive
per-Flip snapshots, not only the first changed state. Any
observed eligible-to-ineligible Flip transition resets the comparison
baseline, but does **not** reset either cap. If Flip does not run in
the background, the trace **cannot** see that background interval;
therefore the absence of a `SUSPENDED` record is not proof that
Alt+Tab did not occur. The old native-state trace keeps its own 64
samples and is not modified.

## New Windows 11 evidence (1 November)

The maintainer's follow-up DS4 log `SpiderMan2k1Fix(20261101-014645).log`
verifies the refined previous build: 96 ordinary source samples, with
78 MENU and 18 3D, followed by **one additional reserved** postfocus
sample at original log line 1579. Native `INPUT-FOCUS` confirms the
foreground went away and returned; the prior game's focus pause stopped
and resumed 11 DirectSound voices without errors, and the known DS4
seven-button mapping was restored then applied. Later physical SDL3
Cross and RT inputs are observed; the log does not by itself prove every
original game action responded. During the first reserved postfocus
sample `kb-held=1`, `joy-held=0`, `merged-actions=0`. The identity and
persistence of that one keyboard latch are **unknown**. The Alt/Tab-only
flags and short consecutive postfocus window are a follow-on read-only
experiment to distinguish a transient residual modifier from persistent
latch state, not a declared game fix.

## DS4 follow-up: independent modifier comparison

The second maintainer Windows 11 DS4 log `SpiderMan2k1Fix(20261101-015535).log`
shows 96 ordinary changes plus all eight reserved samples. Original
`alt-tab=1` (left Alt) first appears at log line 894 **immediately
before** confirmed focus loss at line 898, persists through foreground
resumption at 913–926, and is repeatedly observed during 3D gameplay
until the ordinary sample cap (including in combination with new native
joystick button changes). A second independently confirmed focus loss
occurs at line 1721; the first four postfocus snapshots at 1747 and
1753–55 all have `alt-tab=1, kb-held=1, joy-held=0, merged-actions=0`.
The remaining four reserved snapshots at 1782–88 instead follow an
**ineligible Bink/movie interval**, not a proven second Alt+Tab: all
have `alt-tab=1, kb-held=2, joy-held=0, merged-actions=0`. A third
`sampling=SUSPENDED` at 1824 corresponds to game pause; the trace
exhausted its 8-frame reserve by that point. Audio pause/resume at
line 918 restored 8 secondary DirectSound voices without error.

**Finding:** the original game's DirectInput keyboard latch retains
left Alt through multiple foreground Flips and sessions. This is **not
proof** of whether Windows still regards the physical left Alt as
held, nor of a gameplay failure. An additional **read-only**
`os-alt-tab` field uses foreground Win32 async key-down state to compare
an independent representation. This experiment is off by default and
no native or Win32 input state is ever changed by it. Do not apply
any automated keyboard-latch clearing until cross-source evidence and
real hardware tests justify it.

## Test protocol and boundaries

Use the new x86 CI artifact only after its checks pass; keep a backup
of the previously working DLLs. Start with idle controller and keyboard
actions only, then no keyboard and verified Xbox-only, then DS4-only.
In each run navigate the native menu, enter 3D, release all inputs,
and try Alt+Tab/return. Send complete logs including any
`SDL3-NATIVE-SOURCE: REFUSED` or focus/sampling status lines.

Original keyboard and joystick states are genuinely independent, but
a snapshot *after* Flip is not a synchronized proof of the origin of
the engine's merged actions. No raw key logging, no game-memory writes,
no keyboard disabling, no generic input injection, no new controller
ownership; this is only a research instrument.

PR #88 stays **DRAFT**, stacked on #80, and must not be merged without
explicit maintainer acceptance and Windows/Wine hardware regression
tests.

legluondunet assisted by ChatGPT (OpenAI)

## Windows 11 evidence: independent OS-vs-original-DI Alt state (2:07 AM log)

Full maintainer log `SpiderMan2k1Fix(20261101-020705).log`:
1,467 lines, original executable SHA authenticated, Sony DS4 `054C:09CC`,
`SDL3-NATIVE-SOURCE` armed, and **the `os-alt-tab` field is present**.
The preexisting startup version string is not used to establish build
provenance. The actual optional comparator is present in the loaded DLL.

**All 85 eligible native source snapshots** (38 MENU, 47 3D):

| Original DI Left Alt / Win32 Left Alt | Count |
| --- | ---: |
| 1 / 0 — original keyboard says held, Win32 says up | **77** |
| 1 / 1 — both held | 4 |
| 0 / 0 — both released | 3 |
| 0 / 1 — OS first observed pressed | 1 |

The key transition starts with both released (lines 458–461);
Win32 detects Left Alt pressed before the native latch (line 464:
`alt-tab=0 os-alt-tab=1`); both then report pressed (465–466).
The first independently logged foreground loss is at **469** and the
return at **483**. At **496**, the original keyboard reports
`alt-tab=1, kb-held=1` while Win32 reports `os-alt-tab=0`.
That difference persists in menu and 3D, including when `joy-held=0`
and `merged-actions=0`, as well as during later native joystick
and engine action changes.

The second focus loss and return are independently recorded at
**509 / 523** (still MENU). The existing guarded
`DI8-RETRY` patch reports keyboard `DIERR_NOTACQUIRED` → original
Acquire path at lines **503** and **558**. These messages report a
reacquire *attempt*, **not** proof of a successful buffered keyboard
read, nor of actual release events. A quick-check at the end shows
the native Left Alt latch still set in MENU (line 1465) while
Win32 reports the key up. The later 3D run contains no further
independently confirmed focus transition.

The 85 snapshot total is **below** the 96-change ordinary cap,
so there is no `postfocus=1` reserved sample in this session;
**that is expected**, not a regression.

**Interpretation and safety boundary:** the game's original keyboard
`0x006B792C + 0x38` latch and Win32 key-down observation
**repeatedly disagree** after actual loss/return of foreground.
Because the engine uses *buffered* DirectInput GetDeviceData events
to maintain a 0x7F held-state, a key-up event lost while the game is
background is a plausible explanation. This is an inference, **not**
a captured GetDeviceData release result. Native action masks can
remain zero while the stale Alt latch is held; the log does not prove
that every gameplay keyboard command or the DS4 is broken.

A future opt-in repair should remain separate from passive source
logging and must **not** globally zero the 256-byte keyboard state,
synthetically inject an Alt key-up or seize the DirectInput device.
Candidate criteria for a small independent experiment: focus
loss/return actually witnessed, matching original EXE/opcodes,
foreground eligible gameplay/menu, stable Win32 `VK_LMENU` *and*
`VK_MENU` both released for multiple engine frames, original Alt
latch specifically in its old held value (not a fresh press), and
an unambiguous game-thread native buffered input resumption.
A later physical Alt press must work normally. Do not assert these
criteria are currently proven or enable a native mutation from this
read-only trace alone.

## 1 November 2026 — physical Left Alt release **clears** stale original DI latch

Complete subsequent Windows 11 DS4 log: `SpiderMan2k1Fix(20261101-021319).log`, 1,710 lines, authenticated retail SHA, selected DS4 `054C:09CC`, native Windows. **104 source records**: 96 ordinary (20 MENU, 76 3D among ordinary records) + eight reserved, total 20 MENU and 84 3D; all have the OS comparator. The state-pair breakdown is **97** `alt-tab=0 os-alt-tab=0`, **5** `1/0` (stale original Left Alt), **2** `1/1` (both held). No `0/1` samples.

The test is decisive for *event/latch recovery*, though not by itself for all gameplay keyboard symptoms:

1. Line **1024**, in 3D immediately before a real `INPUT-FOCUS: flip-background` at **1030**: `alt-tab=1 os-alt-tab=1` (physical Alt down, original DI agrees).
2. Actual `flip-foreground` at **1046**, native DS4 wake and game resume at **1050–55**, and first eligible source observation **1059**: `alt-tab=1 os-alt-tab=0` and `kb-held=1, merged-actions=0`. The original game **missed or did not account for the background release**.
3. While still in foreground, line **1087** shows `alt-tab=1 os-alt-tab=1` after another actual Alt key-down; at **1092**, **both become zero**, `kb-held=0`, after release. This strongly supports that the original buffered keyboard reader is alive and can process *fresh foreground key-release events*; a new physical press+release clears the previous stale held latch. The diagnostic counts do **not** prove whether a fresh `0xFF` down event was received while the latch was already held.
4. **Second genuine 3D focus loss/return** at **1575/1589**, with four consecutive `postfocus=1` source records at **1601 and 1609–11**, all `alt-tab=1 os-alt-tab=0 kb-held=1 merged-actions=0`. Thus the phenomenon **recurs** after another Alt+Tab, not just once.
5. Four further reserved postfocus samples at **1668, 1706, 1709–10** follow a **native in-game pause/menu context transition**, not a third observed desktop Alt+Tab; they all show `alt-tab=0 os-alt-tab=0`. There is no directly observed physical Alt key release during the second postfocus interval; do not claim it cleared spontaneously. The ordinary cap was reached at line **1483** before the second focus episode; source logs are bounded and can omit later transitions.

No `DI8-RETRY: redirected` counter line occurs in this new log; the patch was installed at Flip #120, but no `DIERR_NOTACQUIRED` branch intervention is reported here. The native DS4 profile is restored on focus loss and reapplied on return; media/audio pause/resume logs indicate no failure. No crash is indicated.

**Conclusion:** a reproducible *stale original DirectInput Left Alt latch after genuine 3D Alt+Tab*, and recovery after a *new physical foreground Alt release*, are **observed**. Missing buffered background release is still an inference rather than a captured `GetDeviceData` return/event. The log does not prove the entire input-lockout bug is caused solely by Left Alt. Any future trial repair must remain default OFF, SHA/opcode and focus-gated, only ever alter the original Left Alt state (and only if an independently sampled OS release is stable), not all 256 keyboard bytes, controller state, or external Windows key state; hardware tests must verify no lost real Alt keypress or new gameplay regression.
