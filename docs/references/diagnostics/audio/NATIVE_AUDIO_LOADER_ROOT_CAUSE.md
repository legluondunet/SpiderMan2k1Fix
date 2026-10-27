# Root-cause investigation: original DirectSound WAV loader (27 Oct 2026)

## Status: analysis in progress; no root fix asserted

The **user-approved audible repairs** (generic CRC-backed native PCM sector-tail
restoration and the two isolated original `Heli1.wav` outliers) remain unchanged.
This document starts a **separate, read-only forensic phase**, without changing
DirectSound implementation, creating new game hooks, or writing game assets.

The current native loader defect is strongly indicated by the exact onset
of the damaged last partial RIFF/WAV file sector:

```text
(first mismatched PCM byte + 44-byte RIFF/WAV header) % 2048 == 0
```

This was observed in many distinct audio sources from the **supported exact
SpideyPC.exe**, including `Heli1.wav`, `ThugHit2.wav` and `traffic2.wav`.
The original 656/656 source WAVs in `data.pkr` passed CRC32, while game-owned
native DirectSound secondary buffers were defective **before** `DXSOUND_Play`.
The PCM buffer sizes are sometimes 4-byte aligned, adding one silent 16-bit
sample when the original length is `4k+2`. Both features suggest a native
size/copy path issue, but **do not by themselves prove** the precise bad
instruction or that the API `Lock`, `Unlock`, `DuplicateSoundBuffer`, or
the Wine DirectSound implementation caused it.

A separate source-asset defect exists in the original `Heli1.wav` at
PCM samples 12095 and 12109; the game runtime cannot be blamed for those
two already-bad input samples. These are two distinct bugs.

## Verified code landmarks in original executable

| Original game VA | Evidence | Known meaning |
|---|---|---|
| `0x005039F0` | SHA-bound native signature in `focus_dsound_pause.c` | 32-slot voice initialization |
| `0x00504110` | signature `8B 0D 20 79 6B 00 53 56 57` | `DXSOUND_Open` / voice duplication path |
| `0x00504230` | existing native Play hook | `DXSOUND_Play` |
| `0x005042A0` | signature in `focus_dsound_pause.c` | `DXSOUND_Stop` |
| `0x00472870` | `E8 BB 19 09 00` | game's native callsite into Play |

The independent `krystalgamer/spidey-decomp` source implements
`DXSOUND_Open` with `IDirectSound::DuplicateSoundBuffer`, followed by
the native `DXSOUND_Play` using `IDirectSoundBuffer::Play`. However,
`DXSOUND_CreateDSBuffer`, `loadWAV`, and `ParseWavHeader` are marked
TODO or are placeholders in that decompilation. The decompilation
is **not proof** of those missing native functions' actual behavior.
The unknown loader addresses are deliberately left **unasserted**.

## Static, reproducible next step under Linux

The branch contains a standard-library Python 3 probe that:

1. Refuses any executable whose size or SHA256 differs from the supported
   `SpideyPC.exe` (size **1,507,328**, SHA256
   `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`).
2. Validates the PE32 i386 section mapping and previously checked native
   site bytes.
3. Disassembles the native audio code neighborhood
   `0x005039D0–0x00504340` via `objdump`, typically installed as
   `binutils` on Linux.
4. Reports nearby raw immediate matches for `0x800`, `0x7FF`,
   `0xFFFFF800`, `0x400`, and `0x7FC`.
5. Enumerates candidate x86 near-call (`E8 rel32`) references from
   the executable's `.text` into that range.
6. Produces a UTF-8 report. **Raw byte matches and call signatures are
   candidates**, not verified instruction boundaries or confirmed
   functional identifications.

From a checked-out branch on Linux, run:

```bash
python3 ChatGPT/tools/inspect_audio_load_x86.py \
  "/path/to/SpideyPC.exe" \
  -o spiderman-audio-root-disassembly.txt
```

Or download the single standalone Python script from the PR branch first.
The probe does **not** run the game or Wine, alter memory, modify the
executable or archive, or invoke any network operation. Its output is
an offline disassembly of only the known audio neighborhood, plus
bounded candidate call references.

## Windows PowerShell (no Python needed)

For users working directly on Windows, the equivalent read-only PowerShell
5.1+ script is
[`Inspect-SpiderManAudioLoader.ps1`](../../../../ChatGPT/tools/Inspect-SpiderManAudioLoader.ps1).
It applies the **same exact SHA-256/size gate**, parses PE32/i386, checks
the previously verified original signatures, scans the same 2,048-byte
constant patterns and raw `E8 rel32` call candidates, and writes the
same kind of UTF-8 investigation report.

From PowerShell in the game directory (adjust the EXE path if necessary):

```powershell
Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/legluondunet/SpiderMan2k1Fix/experiment/sdl3-audio-probe/ChatGPT/tools/Inspect-SpiderManAudioLoader.ps1' -OutFile '.\Inspect-SpiderManAudioLoader.ps1'
Unblock-File '.\Inspect-SpiderManAudioLoader.ps1'
.\Inspect-SpiderManAudioLoader.ps1 -ExePath '.\SpideyPC.exe' -OutputPath '.\spiderman-audio-root-disassembly.txt'
```

The script does **not** require Python, and generates a partial static
report even without an external disassembler. To include the actual
x86 instructions, install an `objdump.exe` or `llvm-objdump.exe` binary
on PATH, or supply it explicitly:

```powershell
.\Inspect-SpiderManAudioLoader.ps1 -ExePath '.\SpideyPC.exe' -ObjdumpPath 'C:\LLVM\bin\llvm-objdump.exe'
```

The script **never writes the game executable**, starts the game,
modifies DirectSound, or modifies `data.pkr`; its only output is the
explicit report path. The PowerShell syntax is also checked in CI,
independently of the Linux Python helper.

## Major finding from first actual Windows LLVM disassembly (27 Oct 2026)

The user ran the PowerShell probe with `llvm-objdump.exe` and supplied
`spiderman-audio-root-disassembly.txt` (SHA-verified supported original
EXE, full native assembly for VA `0x005039D0–0x00504340`).

We can now identify the **native loader and template buffer writer**
at `0x00503B40`, rather than only the `DXSOUND_Open` voice duplicator:

| Native VA | Original instruction/behavior | Meaning |
| --- | --- | --- |
| `0x00503C9C` | `call 0x00504CB0` | opens/accesses original WAV resource, precise contract still to verify |
| `0x00503CB4` | `call 0x00505170` | writes a length used for allocation and read |
| `0x00503CBE` | `call 0x0052A227` | allocates memory of that length |
| `0x00503CC9–0x00503CD3` | `cdq; and edx,0x7ff; add eax,edx; sar eax,0xb` | **signed integer division by 2,048, truncated toward zero** |
| `0x00503CD8` | `call 0x005051D0` | invokes a **suspected sector-unit read routine** with the truncated quotient and allocated memory destination |
| `0x00503EBD` | `call [ecx+0x0C]` | native DirectSound `CreateSoundBuffer` for the original/reference/template voice |
| `0x00503EFF` | `call [ecx+0x2C]` | DirectSound `Lock` |
| `0x00503F83–0x00503FB2` | `rep movsd; rep movsb` | copies a specified byte length, including any remainder, into one or two locked buffer regions |
| `0x00503FCD` | `call [eax+0x4C]` | DirectSound `Unlock` |
| `0x00504152` | `call [eax+0x14]` | `DuplicateSoundBuffer` into game voice |

Crucial distinction: `cdq / and 0x7ff / add / sar 11` is **signed
division truncated toward zero**, **not ceiling division**. For a positive
WAV length, the result equals `floor(length / 2048)`. Stack tracking
shows the same length used immediately for allocation is loaded from
`[esp+0x30]` before division. The quotient is then pushed as the
second argument to `0x005051D0`, alongside the allocated destination
and the resource handle.

This strongly suggests an **incomplete last-block read in the
game's original WAV resource path**. Independent byte-level forensic
evidence matches the arithmetic exactly:

| Original WAV | File size (44-byte header + PCM) | `size % 2048` | Byte count restored in native buffer |
| --- | ---: | ---: | ---: |
| `Heli1.wav` | 60,524 | **1,132** | **1,132** |
| `ThugHit2.wav` | 15,182 | **846** | **848** (846 original PCM + 2 alignment bytes) |
| `traffic2.wav` | 79,350 | **1,526** | **1,528** (1,526 original PCM + 2 alignment bytes) |

The last two examples have a native 4-byte-aligned allocation
(`PCM+2`) and are therefore expected to differ from the original
WAV-file remainder by two padding bytes. This explains a previously
unresolved detail in the data.

**Confidence boundary:** `0x005051D0` has **not yet been disassembled**.
The 2048 divisor and the observed missing final sector are a very
strong connection, but do not prove the callee silently ignores the
partial sector or that the root cause is specifically this callsite
until its exact read semantics are checked. No root patch, blind count
round-up, or runtime change has been made.

The **PowerShell tool has been updated** to append a *second*
read-only disassembly of `0x00504C80–0x00505300`, covering precisely
the three newly identified callees `0x00504CB0`,
`0x00505170` and `0x005051D0`. Repeat it on the original EXE and
inspect the new second section to confirm whether the third
argument truly represents a block count and how the remainder is
handled. **Blindly replacing `floor` by `ceil` is not safe**:
it could make the callee write beyond the allocated buffer, which
currently holds the original file length, and introduce a heap
overflow. The safe fix must be based on the actual callee semantics.

## What will settle the true origin

After examining the exact native instruction sequence, identify and
verify `DXSOUND_CreateDSBuffer` / `loadWAV` / `ParseWavHeader`
entry points and the effective 2,048-byte transfer loop.

Only then add an **off-by-default, signature-gated, read-only native
timeline** around the actual loader. For the selected CRC-verified
WAVs `Heli1.wav`, `ThugHit2.wav` and `traffic2.wav`, compare:

1. PCM source immediately after PKR decompression;
2. PCM data and length passed into the native DirectSound write;
3. original/unduplicated DirectSound template buffer **after
   the game's final Unlock**, before `DuplicateSoundBuffer`;
4. secondary game voice **after duplication**, before native Play.

A corruption present before step 2 implicates the WAV/parser/copy
preparation. A defect first visible in step 3 implicates the transfer
to DirectSound (native length, offset, lock regions, or API behavior).
A defect only at step 4 focuses attention on the duplicate/share path.
**No diagnosis of the actual responsible instruction is possible from
the Play logs alone.**

Do **not** patch arbitrary addresses or assume
`DuplicateSoundBuffer` produces an independent PCM copy: DirectSound
can share sample memory between duplicated buffers. Do not modify
any template or duplicate that may be active; retain the validated
user-approved pre-Play repair as a fallback until a new low-level
repair is independently tested in Wine and Windows.

No merge of draft PR #67 without explicit user approval.

legluondunet assisted by ChatGPT (OpenAI)
