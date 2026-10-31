# Spider-Man PC 2001 — original joystick event and action dataflow

**Status: new, genuine offline x86 disassembly of the user's original EXE,
1 November 2026, read-only.** This is a research report, NOT an installed
input hook or proof of generic controller support.

- Input file: the maintainer-supplied `SpideyPC(9).exe`, 1,507,328 bytes.
- SHA-256: `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
- PE32 x86, image base `0x00400000`, four sections.
- Static disassembly: `llvm-objdump --disassemble --no-show-raw-insn
  --start-address=0x501e40 --stop-address=0x501fb0 <local-exe>`;
  additionally inspected `0x0050A190..0x0050A400` and
  `0x00505720..0x00505750`, and searched the entire executable
  disassembly for direct call references.
- The original copyrighted executable and full disassembly are **not
  copied into GitHub**. Only addresses, interpretations and short
  identifying byte signatures are recorded here.

## Main finding — the engine reads *buffered* DirectInput events

```text
original input updating code, many call sites
  0x00505720 (input-frame routine)
    0x00505738: call 0x0050A190
       |
       +- 0x0050A190: keyboard action accumulator
       |    and joystick action/directional accumulator
       |      0x0050A2AD: call 0x00501E50
       |        |
       |        +- 0x00501E79: IDirectInputDevice8::Poll
       |        +- 0x00501EA1: IDirectInputDevice8::Acquire
       |        +- 0x00501EF4: IDirectInputDevice8::Poll (retry)
       |        +- 0x00501F0E: IDirectInputDevice8::GetDeviceData
       |        +- 0x00501F15..0x00501F27: clear previous edge bit
       |        +- 0x00501F4D..0x00501F9E: dispatch buffered events
       |                    |
       |                    +- button events -> 0x006B7A34..0x006B7A53
       |                    +- lX -> 0x00AC0A10
       |                    +- lY -> 0x00AC0A14
       |                    +- POV -> 0x00568394
       |      |
       |      +- 0x0050A2D2..0x0050A371: joystick axes/POV to
       |      |    directional **bitmask** for this consumer
       |      +- 0x0050A377..0x0050A3F3: 14-action native
       |           binding lookup using 0x00568408 and
       |           0x00501FB0 (latched joystick button getter)
       |
       +- 0x00505740 onward: consumes action/directional results
```

The game performs **GetDeviceData** via the actual game-owned
`IDirectInputDevice8A *` at `0x006B7A2C`. The exact call at
`0x00501F0E` is `FF 52 28` (`call [edx+0x28]`),
i.e. device vtable slot 10. The call explicitly passes a
**20-byte `DIDEVICEOBJECTDATA` size**, a **16-record bound**
and zero flags. This is **not** a `GetDeviceState` sample.

Earlier confirmed method sites are also visible in this binary:
`0x00501E79: FF 51 64` = `Poll` (vtable +0x64);
`0x00501EA1: FF 52 1C` = `Acquire` (+0x1C);
`0x00501EF4` = second `Poll` after an Acquire path.
The current native joystick reader contains **no identified
GetDeviceState +0x24 call**, which does **not** imply that no
other function/game context calls it.

## Buffered event decoding (verified static instructions)

At `0x00501F15..0x00501F27`, the game walks **32 bytes**
at `0x006B7A34..0x006B7A53` and strips bit `0x80`.
Subsequent event records drive their held/fresh state:

| `DIDEVICEOBJECTDATA.dwOfs` | Branch | Native result |
| --- | --- | --- |
| `0x30 <= dwOfs < 0x30 + dwButtons` | `0x00501F4F..0x00501F77` | Button `rgbButtons` event, transformed into 0xFF (pressed) or 0x80 (released), stored at the corresponding byte of the game's button latch |
| `0` | `0x00501F79..0x00501F82` | `DIJOYSTATE.lX` event data written through first output pointer, `0x00AC0A10` |
| `4` | `0x00501F84..0x00501F8E` | `DIJOYSTATE.lY` event data written through second output pointer, `0x00AC0A14` |
| `0x20` | `0x00501F90..0x00501F98` | **`DIJOYSTATE.rgdwPOV[0]`**, not `lZ`, written through third output pointer, `0x00568394` |

The output argument addresses are independently visible at
`0x0050A29E..0x0050A2AD` immediately before calling the native
reader. `dwOfs` is expressed in the **80-byte `DIJOYSTATE` layout**
used by the retail executable.

The button getter at `0x00501FB0` masks an index to `0xFF`
and reads `0x006B7A34[index]`. The action accumulator calls it at
`0x0050A3A1` and `0x0050A3C5` while iterating the
**14×8-byte (action-mask, native-button-index)** table at
`0x00568408`. Unbound entries carry `0x4000`, and
the terminator mask is `0x8000`.

## Axis/POV handling and implications for an SDL3 bridge

At `0x0050A2F9..0x0050A32D`, the aggregator derives directional
bit flags from the X/Y globals using **±250** comparisons; it also
translates the POV value using quadrant boundaries at **9000,
18000, 27000** hundredths of a degree, with `-1` meaning no POV.
The same kind of axis-to-direction translation is exposed at
`0x0050A9D0..0x0050AA5A` in another callable routine.

**Important:** these paths clearly produce *digital direction masks*
from the physical analog stick. They **do not yet establish** whether
other parts of the original engine consume a continuous analog
magnitude or smoothing states, and no general claim should be made
about stick fidelity. Further xref/control-flow verification is
required before choosing a generic injection boundary.

At `0x00505738`, a caller invokes `0x0050A190` with two
stack-allocated output masks, and uses the result in a larger input
frame routine. Many code sites call `0x00505720` (including
`0x00427989`, `0x00440F4F`, `0x0047C0E9`). This identifies a
shared input processing boundary but **not** a validated point to
inject new controller states.

## Exact on-disk signatures validated on the uploaded EXE

The following byte sequences were independently confirmed against
the authenticated original image and now fail closed in the offline
inspector (`ChatGPT/tools/inspect_spidey_native_reader.py`):

| Address | Bytes | Purpose |
| --- | --- | --- |
| `0x00501F0E` | `FF 52 28` | `GetDeviceData` |
| `0x00501F13` | `B1 7F` | Clear previous edge flag |
| `0x00501F4F` | `83 F8 30` | Button event range starts at 0x30 |
| `0x00501F54` | `8B 1D 58 7A 6B 00` | Physical button count |
| `0x00501F7D` / `0x00501F89` / `0x00501F95` | `8B 41 04` | Read `dwData` for lX/lY/POV |
| `0x0050A2AD` | `E8 9E 7B FF FF` | Aggregator calls native reader |
| `0x00505738` | `E8 53 4A 00 00` | Frame routine calls aggregator |
| `0x0050A3A1` | `E8 0A 7C FF FF` | Aggregator reads button latch |

All 20 known signatures (10 inherited from the old unplug guard, 10
newly confirmed from the binary) match. The offline tool is SHA-gated
and read-only and produces a structured JSON report, with its
heuristic, undisassembled candidates clearly distinguished from
real known instructions.

## Another important finding — **two distinct masks** (held vs fresh press)

Disassembly of `0x0050A190` shows **two DWORD outputs**, zeroed at
`0x0050A19F` and `0x0050A1A5`, passed by the caller
`0x00505720` at `0x00505736..0x00505738`.

The first output accumulates **currently held** input:
- Keyboard: `0x0050A24B` ORs an action mask if a key has
  `state & 0x7F` set;
- Joystick: `0x0050A3B8` ORs an action mask when the
  `0x00501FB0` latch has `state & 0x7F` set;
- Stick and POV: `0x0050A375` ORs direction masks into this
  **held-only** output.

The second output accumulates **fresh digital press events**:
- Keyboard: `0x0050A27C..0x0050A283` ORs a mask when key
  latch is exactly `0xFF`;
- Joystick: `0x0050A3CF..0x0050A3E1` ORs a mask when the
  `0x00501FB0` button latch is exactly `0xFF`;
- The X/Y and POV direction conversion in the identified branch
  does **not** populate the fresh-mask output.

This matters for a future universal SDL3 bridge: writing only an
all-buttons-held mask would omit genuine press-edge semantics for
menus/actions. Conversely, synthesizing fresh events every frame
would double-trigger actions. A safe producer needs **held** and
**fresh** masks, a release/focus/owner-change policy, and a definition
of how it samples multiple button transitions within one frame.

The standalone C99 preview therefore also offers
`spider_sdl3_generic_engine_preview_edges()`. It computes
`held_mask` and `fresh_mask` from the existing saved semantic
profile and previous SDL3 action state; analog and D-pad directional
bits remain held-only in this preview. **It cannot prove strict parity
with native buffered DirectInput press events**: multiple transitions
between samples, rebind generation changes, focus and context
ownership require separate research and tests before wiring it up.

## Direct input mask consumer: shared **held** state and native event latches

A deeper offline disassembly of the exact original EXE pinpoints
how the output of `0x0050A190` is used.

`0x00505720` allocates **two DWORD locals** (`sub esp,8`).
At `0x00505729` it takes the address of the **first** local
(`lea ecx,[esp]`), and at `0x00505732` it takes the address
of the **second** (`lea eax,[esp+4]`), then passes them to
`0x0050A190` at `0x00505738`. Thus the earlier HELD/FRESH
distinction inside the aggregator remains correct.

**Important refinement**: in this particular caller, after preserving
`esi` and `edi`, the game reads the **first local** at
`0x00505759: mov edx,[esp+8]`, then feeds individual HELD bits
to `0x00479540`. Subsequent stack-offset loads track the same
local while argument pushes change `esp`. This consumer does **not
read the second/FRESH local** before returning. The second mask
still exists in the aggregator, but its presence **does not
automatically mean an external SDL3 bridge must write both masks**
at this consumer boundary.

The executable has **one direct `call 0x0050A190`**
(`0x00505738`), and the `0x00505720` routine is directly called
at **56** sites across the executable. This is a **shared engine
input service**, not a 3D-only function by mere location.
Do not patch its inputs indiscriminately in a menu, movie, pause or
scripted cutscene.

### The original engine already updates button transitions

The caller sends individual HELD-mask bits to
`0x00479540` at **20** direct call sites
(`0x00505785..0x005058E3`), then invokes `0x004796A0`
at `0x005058F5`.

The `0x00479540` helper operates on a **16-byte per-action
state record**, not on the DirectInput device: it reads the
previous active state byte at offset `+0`, increments/clears
frame counters `+4`, `+8`, `+12` and updates event bytes
`+1`/`+2`. Among verified instructions:

| VA | x86 | Observation |
| --- | --- | --- |
| `0x00479548` | `8B 48 0C` | Read internal frame/release counter |
| `0x0047954B` | `8A 18` | Read previous HELD state byte |
| `0x0047955D` | `83 F9 0A` | Threshold against 10 frames |
| `0x00479560` | `C6 40 01 01` | Set event indicator `+1` |
| `0x0047956F` | `88 50 02` | Clear event indicator `+2` |
| `0x0047957C` | `C6 00 01` | Mark action held |
| `0x00479581` | `88 10` | Mark action released |

This is better evidence for a potential future high-level integration
point: supply a **neutral-gated HELD semantic mask** to the
existing game action-state updater, rather than invent raw DirectInput
button indexes or synthesize new fresh edges at every Flip. The
existing engine already derives some transitions from a held value.

**Still unproven and blocking activation**: exact safe hook boundary
and calling convention, the helper's full per-action state semantics,
how the native joystick's physical state is excluded to prevent
duplicate commands, continuous analog expectations, and which of
the 56 callers execute during each gameplay/menu/movie mode.

An exact-hash offline reader inspector now validates **17 additional
instruction signatures** for this chain and includes the consumer
layout in its JSON output. Their signatures have been checked on
the genuine original EXE supplied in this session.

### Neutral-gated independent frame model

`sdl3_generic_frame_preview.h` is an additional **pure portable C99
test-only** step. It uses the previously established per-instance
sample/profile snapshots, requires a neutral sample after focus,
profile/instance change or context loss, and computes a HELD and
diagnostic FRESH mask without touching game memory. Tests cover a DS3
default semantic profile, two identical VID/PID devices with
distinct SDL3 instance IDs, profile remapping while held, loss of
foreground, D-pad held-only directions and a bad trigger sample.

It is not wired to any runtime C file. The FRESH mask in this test
model is **diagnostic only**, not a proposed second native write.

## Portable semantic-mask prototype (phase-1 research, NOT gameplay)

A new strictly non-runtime C99 module,
`sdl3_generic_engine_preview.h`, independently previews what an SDL3
normalized controller would request from the **identified** native
input mask consumer:

- Uses existing per-model SDL3 `Jump, Kick, Punch, Web, Aim, Zipline,
  Swing` semantic profile assignments rather than physical
  DirectInput button indexes.
- Maps seven semantic action bits to the verified native masks
  `0x10,0x20,0x40,0x80,0x100,0x200,0x400`, independent of
  DS3/DS4/Xbox hardware identity.
- Estimates the original ±250 threshold on a ±1000 signed axis
  using an **uncalibrated** ±8192 normalized SDL3 stick threshold.
  D-pad inputs use SDL3 buttons 11–14; an explicit D-pad direction
  overrides the corresponding analog axis, and contradictory
  directional buttons cancel.
- Applies the existing 11000/5000 trigger press/release hysteresis.
  Never forwards Pause as a fictional eighth game-native action;
  menu/focus/Pause ownership remains separate.
- Is not included by any runtime C source and **cannot alter
  DirectInput state, patch code, inject events or make any controller
  work in the actual game**. The older profile validity requirement
  still needs a safe migration for zero VID/PID devices.

`ChatGPT/tests/test_sdl3_generic_engine_preview.py` tests these
purely computational rules with DS3 semantic defaults and the user's
Xbox custom B/X and Y/A swaps. The new masks are only a **candidate
for future verified integration**. No live hardware mapping, output
transaction, or original action-mask consumer contract has been
accepted for mutation yet.

## Architectural conclusion and next step

**We have evidence for a possible higher-level input boundary**, not
yet evidence to edit one safely. SDL3 normalization could feed
**actions + directional masks** without mapping every pad to physical
DirectInput BUTTON indices, if the engine's actual consumption of
these masks and the analog behavior are fully understood.

Before implementing a bridge:
1. Trace `0x00505720` outputs and all consumers, including native
   menu, playable 3D and cutscenes; verify which readers still demand
   the game-owned DirectInput COM device.
2. Compare SDL3 samples and post-aggregator masks in **read-only**
   bounded logs with the existing verified Xbox/DS4 owners.
3. Only then propose an opt-in, exact-signature-guarded single-owner
   integration plan that does not double-drive the original game or
   confuse Steam Input virtual and physical gamepads.
4. Test DS3 and Proton/Steam Deck separately. These are **NOT
   hardware validated** by offline disassembly or green CI.

Do not call this a working generic gamepad fix. PR #80 remains the
hardware-validated DS4/Xbox baseline, PR #88 remains research-only;
**no merge** without explicit user authorization.

legluondunet assisted by ChatGPT (OpenAI)
