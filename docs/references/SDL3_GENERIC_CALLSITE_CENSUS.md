# SDL3 generic input research — exact original 56-call census

Status: **offline static audit**, 1 November 2026. No game launch, live
runtime trace, game input modification, or claim of DS3/Steam Deck support.

Input: user-supplied exact retail `SpideyPC(9).exe`,
SHA-256 `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
Addresses were confirmed by `llvm-objdump` and by a bounded
`.text`-only x86 `E8 rel32` search.

## Direct-call census

| Target VA | Direct callsites | Evidence / role |
| --- | ---: | --- |
| `0x0050A190` | **1** | Aggregator for keyboard + joystick held/fresh masks; called at `0x00505738` |
| `0x00505720` | **56** | Shared input service; all 56 addresses recorded and checked |
| `0x00479540` | **20** | Stateful 16-byte per-action latch updater; 20 calls confined to `0x00505785..0x005058E3` |
| `0x004796A0` | **1** | Called at `0x005058F5`, follow-up state maintenance (exact function semantics still need mapping) |

Callsite locations for `0x00505720`, grouped **by address only**:

| Region | Number | Evidence |
| --- | ---: | --- |
| `0x0042xxxx` | 10 | Ten calls in `0x00427989..0x00427E9F`; neighboring timing checks/calls to `0x0050C180` |
| `0x0044xxxx` | 2 | `0x00440F4F`, `0x00443F3B` |
| `0x0045xxxx` | 1 | `0x00455477` |
| `0x0047xxxx` | 5 | `0x00470835`, `0x004793B9`, `0x0047C0E9`, `0x0047D849`, `0x0047D985` |
| `0x0048xxxx` | 2 | `0x0048DFE5`, `0x0048EC85` |
| `0x0049xxxx` | 27 | Numerous scattered calls `0x004935BB..0x0049FFFA` |
| `0x004Axxxx` | 6 | `0x004A0CDC..0x004A1A21` |
| `0x0050xxxx` | 3 | `0x00509883`, `0x0050D31E`, `0x0050DC72` |
| **Total** | **56** | Static *direct* references only; virtual/indirect call paths are out of scope |

**These region groupings are not game mode labels.** The ten nearby
`0x427xxx` calls are adjacent to repeated elapsed-time checks
and `0x0050C180`; this suggests event/wait-loop usage but does
**not** prove whether they run during movies, menus or gameplay.
The other clusters have no verified scene classification.
A simple address heuristic must never be used as the future
`menu2d` / `gameplay3d` / `Bink` gate.

## Implications for an eventual SDL3 bridge

The original shared service `0x00505720` increments game global
`0x0066129C`, can reset it after `0x0050C4B0`, updates 20
game-native 16-byte per-action state records through `0x00479540`,
then invokes `0x004796A0`. It has **side effects** independent
of a gamepad: hooking it or calling it an extra time can
affect game timing, action states and repeat logic.

The aggregator `0x0050A190` merges **keyboard AND physical
DirectInput joystick** into its first HELD mask. A generic
SDL3 output cannot blindly replace that mask: it could discard
keyboard controls. Conversely simply ORing SDL3 into the held
mask without disabling the previously game-owned physical joystick
could double-activate controller actions or allow a different
controller to influence the game. An eventual single-owner
transaction must explicitly preserve keyboard and isolate
the original joystick source.

Independent direct references to `lX/lY` globals
`0x00AC0A10/0x00AC0A14` occur in the aggregator and a second
path near `0x0050A9D0`. This supports two **known direct
static readers**, not a claim that all continuous analog uses
have been exhaustively excluded: aliased pointer accesses and
indirect callers remain possible.

The tested portable SDL3 frame/semantic previews continue to be
standalone and are **not wired to runtime**. User-visible
Windows/Proton tests are not appropriate until a bounded passive
game-mode and owner-invariant probe exists.

## Offline audit command

The companion code `ChatGPT/tools/inspect_spidey_input_callsites.py`
performs a read-only exact-hash PE32/source-signature preflight,
checks **all 56 exact call addresses** and the 20 state-update
calls, groups them structurally by 4 KiB address range and reports
short local SHA fingerprints, **not** copyrighted function bytes.

```bash
python ChatGPT/tools/inspect_spidey_input_callsites.py /path/to/SpideyPC.exe > input-callsites.json
```

It cannot identify dynamic menu/3D/Bink modes and does not install
hooks. The original EXE stays outside the GitHub repository.

### Remaining blocker

Map the call contexts by **real game-state evidence**, including
the input mode transitions and possible calls during movies;
resolve the keyboard-vs-native-controller combination semantics;
then design an authenticated optional read-only tracing step.
Only after those validations could an opt-in generic SDL3 game
bridge be proposed for DS3/Steam Deck.

PR #88 remains draft/stacked on PR #80, no merge into `main`.
Signature: legluondunet assisted by ChatGPT (OpenAI).
