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

The independent budget is **96 source changes** plus **12 status/focus
transitions**. The status budget remains usable after 96 changes. Any
observed eligible-to-ineligible Flip transition resets the comparison
baseline, but does **not** reset either cap. If Flip does not run in
the background, the trace **cannot** see that background interval;
therefore the absence of a `SUSPENDED` record is not proof that
Alt+Tab did not occur. The old native-state trace keeps its own 64
samples and is not modified.

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
