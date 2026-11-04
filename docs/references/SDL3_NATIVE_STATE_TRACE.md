# SDL3 native action-latch trace — optional, passive, research only

Issue #87 / draft PR #88, stacked on the validated DS4/Xbox draft PR #80.

The authenticated original retail `SpideyPC.exe` SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`
holds 20 native action-state records, each 16 bytes, starting at
`0x00661100`. This experimental trace can **read** their first three
bytes after the existing game Flip, without calling the original input
service again and without changing game memory.

## Enable only when collecting diagnostic data

```ini
[Input]
ExperimentalSDL3Probe=1
[Debug]
Log=1
SDL3NativeStateTrace=1
```

Both experimental keys default to **0**; the debug trace does **not**
depend on a verified DS4/Xbox native controller owner or any gameplay
remapping option. It cannot itself make a DualShock 3 or Steam Deck work.

A passive read of each original record captures:

- byte +0: original engine HELD state,
- byte +1: engine event indicator, **not necessarily one-frame**,
- byte +2: engine early-press indicator.

The sample maps these to three independent 20-bit bitsets. A fourth
field ORs the known game-action mask for any held slot. Some original
slots intentionally share identical action bits, so the slot bitset
is more informative than the aggregate mask.

### Safety and instrumentation boundaries

- The original game's SHA-256 and version are already checked by
  SpiderMan2k1Fix's standard loader.
- This trace additionally checks three exact x86 opcode signatures
  in the original keyboard/joystick aggregator and 16-byte latch
  consumer before enabling game-state reads.
- It is called only through the existing same-thread Flip hook,
  **after** the native reconnect handler and the existing DS4/Xbox
  gameplay module, and before the optional SDL3 3D semantic logger.
- It reads only at foreground, and refuses during Bink and pause.
  Menu and playable 3D are labelled explicitly; any other
  foreground state is labelled OTHER.
- It is capped at **64 changed or context-transition samples**.
  There is no new thread, input poll, COM call, keyboard event
  generation, native detour or writing to game memory.
- When the opt-in is absent, it reads no original game-state
  addresses. The old DS4/Xbox mappings and owners remain unchanged.

Logs begin with `SDL3-NATIVE-STATE:` and end with
`[READ ONLY, SOURCE UNKNOWN]`. **Critically, these game states
are already merged**: a held bit cannot yet be attributed uniquely
to the original keyboard, the original joystick, or both.
The trace only helps correlate game state with another selected
controller's separate SDL3 sample *when* tests are performed.

This trace does **not** classify the 46 unclassified direct input
call sites into game modes and does not prove it is safe to replace
any native input pipeline. Those are still research blockers.

## Acceptance before a future bridge

First compare the passive native logs against a previously verified
Xbox/DS4 using isolated keyboard-only and joystick-only button
presses (no simultaneous buttons). Then test owner handoffs,
Alt+Tab, menu/3D and Bink skip. Hardware tests with DS3 or
Steam Deck will follow only after a real, disabled-by-default
source-isolated bridge exists and preserves keyboard control.

No merge into `main` without explicit maintainer approval.

legluondunet assisted by ChatGPT (OpenAI)
