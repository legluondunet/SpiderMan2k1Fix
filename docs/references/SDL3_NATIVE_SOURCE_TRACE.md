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
