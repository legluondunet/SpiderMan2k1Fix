# Separate original keyboard from native joystick for future generic SDL3

**Status:** source-guided read-only design and portable C99 tests on the
research-only PR #88. Not activated in the DLL. The same original
SpideyPC.exe SHA-256 gate applies:
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Verified binary branch boundaries

A fresh offline `llvm-objdump -d --x86-asm-syntax=intel` of the
user-supplied exact retail executable establishes:

| x86 address | Instruction / role |
|---|---|
| `0x0050A190` | Entry to the keyboard + joystick semantic aggregator |
| `0x0050A19F` | Zero HELD output before reading any input |
| `0x0050A1A5` | Zero fresh input output |
| `0x0050A1AC` | Keyboard-state reading `call 0x00501B80` |
| `0x0050A1B3` | Negative keyboard-input status can branch ahead to controller stage |
| `0x0050A24B` | Keyboard action bit OR into HELD output |
| `0x0050A27C` | Fresh keyboard action bit OR into FRESH output |
| `0x0050A29E` | End of keyboard region, begin physical joystick section |
| `0x0050A2AD` | Native joystick reader `call 0x00501E50` |
| `0x0050A2B5` | If reader returns zero, skip native additions (keyboard remains) |
| `0x0050A371` | Native directions ORed into already-populated HELD output |
| `0x0050A3B8` | Native joystick action bit OR into HELD output |
| `0x0050A3DF` | Native fresh action bit OR into FRESH output |
| `0x0050A3F4` | Common return (no input event injection needed for keyboard retention) |

These are **function segments, not independent callable entry points**.
For example, jumping to `0x0050A3F4` without preserving
the original stack/register context would be unsafe, and
the `0x0050A29E` boundary is not automatically a valid hook site.
The joystick reader may have side effects even when the returned
buttons/directions are zero; do not simply skip it until those
effects are understood.

The original combines keyboard and DirectInput:
`HELD = keyboard | physical_native_controller`.
An eventual generic SDL3 owner would need
`HELD = original_keyboard | normalized_sdl3_controller`,
but **never** OR the generic controller on top of the still-active
physical DirectInput device.

## Pure C99 safety-contract preview

The standalone header `sdl3_generic_source_isolation.h` models
these two combinations with explicit input-source provenance.
Its tests cover the original path and fail-closed generic preview
unless **all** conditions hold:

- explicit opt-in and exact EXE/callsite validation;
- foreground, playable 3D, no 2D menu/Bink/pause;
- selected SDL3 instance, verified identity/profile, fresh neutral
  sample after owner/focus/profile change;
- **independently authenticated isolation** of the old DirectInput
  device contribution (not the keyboard);
- normalized SDL3 action/direction mask restricted to the
  presently established `0x7FF` semantic bit range.

When any condition fails the preview returns the **original**
keyboard + native controller combination. In the eligible case it
keeps every original keyboard bit (including `0x1000` and other
special flags), replaces the original controller contribution with
one generic SDL3 mask, and never mixes two gamepads.

**The crucial `old_di_source_isolated` flag is intentionally
false in current runtime. No code has been written to satisfy
it yet**, and the test C99 simply simulates that hypothetical
proof. Consequently this is a proposed contract, not a working
controller bridge or a safe injection-point demonstration.

## Integration blockers

1. Prove at runtime whether the original joystick reader has
   maintenance/event side effects necessary to scripts/hotplug
   and whether an isolated controller mask can be substituted
   without suppressing keyboard or changing call count.
2. Define an atomic same-thread owner transaction that disables
   only the original controller contribution, with rollback and
   exact signature/mode checks. Do not intercept every one of
   the 56 service callsites.
3. Validate native menu/cinematic/3D context and avoid double
   Steam Input virtual+physical controller samples.
4. Compare action state transitions and directional/centering
   behavior with verified DS4/Xbox hardware before enabling
   any unverified DS3/Steam Deck device.

No generic gameplay changes or EXE bytes committed to GitHub.
PR #80 retains verified Xbox/DS4 ownership; PR #88 is a draft
stacked on PR #80, no merge to main.

legluondunet assisted by ChatGPT (OpenAI)
