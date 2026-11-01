## Binary-verified independent axis suppression and POV mechanics

The actual 2001 retail EXE's input path has **two distinct consumers**
of the same native joystick globals:

* `0x0050A190` (aggregator): reads `0x00AC0A18` at
  `0x0050A2BD`; while that byte is nonzero, left-stick X/Y are
  suppressed. Both axes must be **strictly** within (-250,+250)
  before it clears the suppression flag at `0x0050A2F4`.
  On that same sample, a centered axis yields no direction.
* `0x0050A9D0` (standalone directional reader): checks the same
  flag but **does not** clear it, even when axes have centered.
  It can be called outside the main action aggregation.
* `0x0050A9B0` resets the gate to 1 **and** clears the POV
  reading at `0x00568394` to -1. It is invoked by the
  input-condition function `0x0050C180` at `0x0050C2AE`,
  `0x0050C2C8`, `0x0050C2E2` and `0x0050C2FC` after
  selected directional events are consumed, depending on flags.
  This associates the gate with **event consumption**, not just
  controller disconnection or a single menu transition.

Native X/Y use threshold values **strictly** less than -250
or greater than +250. POV angles are hundredths of a degree;
`-1` is neutral, `0` up, `4500` up+right, `9000`
right, `18000` down, `27000` left, `31500` up+left.
The native code OR-combines stick and POV directional bits.

`sdl3_native_direction_research.h` models these original
**native-coordinate** behaviors in standalone portable C99,
including independent `peek` and `aggregator` operations.
Its tests verify the deadzone boundaries, the reset trigger,
and contradictory stick/POV directions. It does not run in the
DLL. The SDL3 signed-axis conversion (±32767 → ±1000) and
device-specific noise calibration are **not established** by
this original-binary analysis.

A cautious future bridge should not just copy an SDL3 stick
threshold into the original engine. It must preserve:
(1) the reset-on-consumption action, (2) both readers'
interaction with the same gate, and (3) the keyboard + physical
joystick + generic SDL3 source-isolation policy.
