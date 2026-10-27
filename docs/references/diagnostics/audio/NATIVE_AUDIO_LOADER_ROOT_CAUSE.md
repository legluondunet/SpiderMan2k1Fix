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

## 2nd disassembly: loader truncation confirmed in the native reader

The user provided `spiderman-audio-root-disassembly(1).txt` from Windows
`llvm-objdump`, exact original `SpideyPC.exe` SHA256 verified. The newly
covered reader function at **VA `0x005051D0`** establishes the causal chain:

```asm
; caller: 0x00503CC9-0x00503CD8
cdq
and edx,0x7ff
add eax,edx
sar eax,0xb     ; blocks = floor(WAV file bytes / 2048)
push eax
push esi
call 0x005051D0

; reader: 0x005051D4-0x0050522B
mov edx,[esp+8] ; number of blocks requested
shl edx,0xb     ; requested byte count = blocks * 2048
; clamp requested bytes to resource end
; ... compare (cursor + byte count) vs resource length ...
rep movsd       ; copy full dwords
rep movsb       ; copy 0..3 leftover bytes
; update the resource cursor by the bytes copied
```

The reader **does** clamp to the resource's physical end and can copy a
partial 2048-byte block when requested. However the caller never
requests the final block: it truncates the file length to whole sectors
before the call, then allocates and copies the WAV header/PCM as if the
entire file had been loaded. DirectSound's native template-buffer
`CreateSoundBuffer`, `Lock`, `rep movs`, `Unlock`, and later
`DuplicateSoundBuffer` occur only **after** this incomplete resource
read. This directly explains all three observed last-WAV-sector lengths,
with optional two bytes of padding in the subsequent aligned PCM
buffer. The conclusion is about this specific defect, **not** the two
outlier original `Heli1.wav` PCM samples, which remain a separate
source-asset problem.

**Confirmed root cause:** the original game erroneously rounds the
requested PKR/WAV transfer **down** to a multiple of 2048 bytes. A
native code correction is possible without replacing DirectSound.

## Experimental loader-level fix — default OFF, not yet game-validated

The draft PR now contains `pkr_audio_loader.c/.h` and the INI key:

```ini
[Debug]
ExperimentalAudioLoaderTail=0
```

The new implementation is a conservative, fail-open root fix. It hooks
ONLY the two exact calls in the original WAV loader:

- `0x00503CB4` calls `0x00505170` to obtain the **actual WAV byte
  length**, used immediately for game allocation. This value is
  captured per-thread, not guessed from a WAV header.
- `0x00503CD8` calls `0x005051D0` to read the full sectors. The hook
  calls the original routine exactly as before, then considers an
  optional remainder only when `blocks == length / 2048` and the
  remainder is in `1..2047`.
- The in-memory staging WAV must exactly identify **one** independently
  decompressed original PCM16 mono WAV with a 44-byte RIFF header and
  **matching full-sector PCM prefix**, from our CRC32-checked
  `data.pkr` cache. Unknown or ambiguous sources remain untouched.
- Only then, `0x005051D0` is called with **one additional sector into
  a private 2048-byte scratch buffer** (not into the game's WAV staging
  allocation). If and only if the returned remainder exactly matches
  the known CRC32-valid original WAV bytes, the hook writes precisely
  those `length % 2048` bytes at the end of the already allocated WAV.
  No round-up, no overrun, no reimplementation of game ZIP/PKR codecs.
- Existing native Play-time PCM sector-tail restore and the separate
  original Heli1 two-sample declick remain unmodified fallback paths.
  All DirectSound calls, buffer duplication, mixing and game music
  remain native. Signature mismatches refuse installation.

With `[Debug] Log=1` an applied correction will emit e.g.
`PKR-AUDIO: LOADER-TAIL-RESTORED ... remainder=1132 ...`.
Refused/ambiguous sources can emit `LOADER-SKIP-NO-UNIQUE-SOURCE`
or `LOADER-SKIP-REMAINDER-MISMATCH`, capped at 96 messages
per category. These are fail-open, not evidence that game playback
was fixed. Static tests and CI compilation alone **cannot prove**
runtime safety or successful audio playback.

**First runtime A/B test, only after building this experiment**:

```ini
[Debug]
Log=1
ExperimentalAudioLoaderTail=1
ExperimentalAudioPKRTailRestore=1
ExperimentalAudioHeli1Declick=1
```

Test a new gameplay session with 3D cinematics, `Heli1` (helicopter)
and fight effects; compare logs to the known-good baseline. Expect
`LOADER-TAIL-RESTORED` for uniquely matched sounds and fewer
Play-time `TAIL-RESTORED` events for the same assets, as their
template buffers should already have original complete audio.
`Heli1Declick` must remain enabled because its two wrong PCM
samples originate inside the original WAV.

Do not disable the fallback on the first run. If the new hook fails,
remove only `ExperimentalAudioLoaderTail=1` (set to `0`) and the
pre-existing native restore/declick remain in place. Test one
noncritical session and inspect logs before asserting success.
No merge or default activation without explicit maintainer approval.

## First Windows gameplay log for the loader root fix (27 Oct 2026, 10:13)

Original submitted game log `SpiderMan2k1Fix(20261027-101351).log`,
running `SpiderMan2k1Fix v1.0.0-6d6584f` (the GitHub PR build's
temporary test-merge SHA; the PR source was `528f5e3`).
This is a **real Windows run, not merely a simulated test**:

- The exact supported game SHA-256 and INI validation both succeeded.
  656/656 original PKR audio source records were CRC-validated, zero CRC errors.
  Native `LOADER-HOOK ARMED` and legacy source-index and Play hooks installed.
- **64** `PKR-AUDIO: LOADER-TAIL-RESTORED` lines (62 distinct file
  names plus `missing.wav` and `s_burn.wav` loaded again on menu
  return). Every logged native extra-sector sample check succeeded,
  with **no** `LOADER-SKIP-REMAINDER-MISMATCH`.
- Exactly **27** `LOADER-SKIP-NO-UNIQUE-SOURCE` guard refusals.
  They require separate classification (noncanonical RIFF variants,
  unsupported resources, or ambiguous identifiers), and **must not**
  be force-fixed without PCM identity proof.
- The three source-forensics benchmarks repaired **at load time**:
  `Heli1.wav` remainder **1132**, `ThugHit2.wav` remainder **846**,
  `traffic2.wav` remainder **1526**. At Play, the helicopter
  `FULL-PCM IDENTICAL` and `TAIL-SKIP already-identical` prove its
  final PCM sector reached the native DirectSound secondary buffer
  completely. Its *separate original sample outlier* is still
  handled by `HELI1-DECLICK` (two PCM16 samples).
- Only **12** `PKR-AUDIO: TAIL-RESTORED` Play-time fallback repairs
  remain. Full list:
  `PickConf.wav`, `achum.wav`, `Crawl1.wav`, `Crawl2.wav`,
  `Swing5.wav`, `WebShot10.wav`, `WebShot3.wav`,
  `WebShot11.wav`, `tingle2.wav`, `h_hitfloor.wav`,
  `Pistol1.wav`, `Pistol2.wav`.
  **None of these twelve names appears in the loader-hook successful
  or skipped events.** Their exact predicted WAV size modulo 2048
  also matches **none** of the 27 logged skip remainders. Strong
  **working hypothesis**: these assets are populated through another
  native WAV-loading callsite or path, rather than the one-time,
  64-entry initialization function `0x00503B40`.
  This is not yet confirmed by a distinct callsite disassembly.
- The run progressed from intro through 3D gameplay and native
  return-to-menu cleanup; no reported process crash or PKR-audio
  warnings/errors. **Actual listening/auditory quality was not
  reported by the user with this log.** A clean log alone is not
  subjective sound validation.

In comparison, the *earlier* user-tested log
`SpiderMan2k1Fix(20261027-080359).log` (without loader hook)
contained **66 Play-time `TAIL-RESTORED`**. The new test has 12;
the precise gameplay/replay scope differs, so 66→12 is a
**per-session count comparison**, not a controlled reduction rate.

**Next step**: Find any other direct native `E8 rel32` callers of
`0x00505170` (PKR resource byte length) and `0x005051D0`
(2048-byte resource read), as well as callers of `0x00503B40`.
Do not hook by source filename or blindly patch an unknown WAV
reader: first confirm its exact callsite, source buffer allocation
and read contract. The current loader fix stays opt-in and the
Play-time fallback stays on for the twelve unresolved assets.

## Original executable cross-references: a second candidate resource path (27 Oct 2026)

The user supplied `spiderman-audio-loader-xrefs.txt` from the
**exact SHA256-verified** original Windows binary. The read-only raw
x86 `E8 rel32` census produced:

| Target original VA | Callsite VAs | Result |
| --- | --- | --- |
| `0x005051D0` (2048-byte resource reader) | `0x00430B23`, `0x00430B52`, `0x00430B9E`, `0x00503CD8` | 4 raw near-call candidates |
| `0x00505170` (resource length query) | `0x00430A21`, `0x00503CB4` | 2 raw near-call candidates |
| `0x00503B40` (known 64-entry native WAV loader) | `0x00471822` | 1 raw near-call candidate |

The additional **three sector-reader references are clustered within
`0x00430B00–0x00430BA0`**, with a resource length call at
`0x00430A21`. They could form a second resource-loading routine,
possibly used by the 12 WAVs that still need late Play-time repair.
**The report alone does not establish function boundaries, caller
arguments or even that these calls handle WAV/DirectSound at all.**
Do not install a hook at these candidate addresses until this is
verified from their actual native instructions.

The PowerShell and Linux read-only tools now produce a **third static
disassembly window**, `0x00430800–0x00430DC0`, in addition to the
existing sound-loader/reader windows, to resolve those questions.
All source hooks and prior CRC-backed runtime repair behaviors remain
unchanged. No game launch, new runtime patch, or PR merge is involved
in this stage.

The user reports **no audible parasites on the current Windows PC**,
while deliberately reserving final audio validation for a separate
Linux/Wine PC. Do not claim Linux compatibility or overall auditory
perfection from a single Windows listening session.

## Second native disassembly: `0x00430A40` **already handles the short final sector** (27 Oct 2026)

The user provided the original game's `llvm-objdump` of
`0x00430800–0x00430DC0` in
`spiderman-audio-second-path.txt`. This resolves the raw xrefs
identified above as **real instructions** and changes our earlier
working hypothesis. The second path is a **generic resource-transfer
routine**, not yet established as a DirectSound/WAV template
loader. **It does not show the same omitted-tail defect as
`0x00503B40`.**

`0x00430940` opens a game resource via `0x00504CB0`, and
`0x00430A21` queries its byte length via `0x00505170`.
`0x00430A40` receives an output buffer and a resource byte count;
it also contains a branch via `0x00458130` (verify this routine
separately if needed) and uses a reusable 2048-byte scratch buffer at
`0x0057AD30`.

The crucial native instruction paths:

| VA | Real native behavior |
| --- | --- |
| `0x00430AF2` | compare the requested remaining byte length to `0x800` |
| `0x00430B06` | check 32-byte destination alignment to decide whether to copy full sectors directly |
| `0x00430B0B–0x00430B23` | on the aligned fast path, truncate the **full-sector** count, and pass that count to native `0x005051D0` |
| `0x00430B28–0x00430B39` | subtract full-sector bytes from remaining length, advance the destination pointer |
| `0x00430B43–0x00430B87` | for additional full sectors, read **one 2048-byte sector** into scratch and copy its 2048 bytes to the destination |
| `0x00430B89–0x00430BA8` | when the remaining byte count is nonzero and at most 2048, request **one more sector** into scratch via `0x005051D0` |
| `0x00430BCC–0x00430BDB` | copy **exactly the remaining requested bytes** from scratch, one byte at a time |

The 2048-byte integer division at `0x00430B0B` is **not**
the same bug. The short-sector branch at `0x00430B9E`,
followed by `0x00430BCC–0x00430BDB`, explicitly consumes
the remainder (and the native reader itself clamps to the resource
end). On an unaligned destination, the code simply bypasses the
fast full-sector copy and still performs the scratch-based reads.
This is strong instruction-level evidence that the routine
**does not systematically drop the trailing resource bytes**.

**Updated diagnosis:** do **not** extend the loader-tail patch to the
three `0x00430Bxx` callsites. They are valid calls but do not exhibit
our specific truncation defect. The twelve remaining late
`TAIL-RESTORED` events could arise from **another WAV-to-DirectSound
loading/transfer route** or corruption **after** a resource was read
correctly. We must inspect **where their DirectSound template buffers
are populated**, rather than conclude that the generic
`0x00430A40` reader needs fixing. No in-game audio or DLL
modifications were made during this investigation.

Known Windows listening result: user reported no audible parasites,
with final validation still reserved for Linux/Wine.

## Forensics for 12 remaining Play-time restores: all begin at exact sector boundary

Reanalysis of the **real** Windows
`SpiderMan2k1Fix(20261027-101351).log` confirms
**every one of the 12 remaining late repairs** has
`first_diff == sector_tail_offset`. No late restoration starts
in another region of PCM. These are in addition to the **64**
already repaired at native load time.

| Original WAV | First corrupt PCM offset | Bytes restored |
| --- | ---: | ---: |
| `PickConf.wav` | 42964 | 784 |
| `achum.wav` | 32724 | 236 |
| `Crawl1.wav` | 22484 | 576 |
| `Crawl2.wav` | 18388 | 868 |
| `Swing5.wav` | 30676 | 1956 |
| `WebShot10.wav` | 18388 | 376 |
| `WebShot3.wav` | 14292 | 1304 |
| `WebShot11.wav` | 12244 | 1608 |
| `tingle2.wav` | 55252 | 1676 |
| `h_hitfloor.wav` | 18388 | 192 |
| `Pistol1.wav` | 28628 | 1296 |
| `Pistol2.wav` | 22484 | 4 |

All offsets are PCM offsets **after** the 44-byte WAV header:
`sector_tail_offset = floor((44+PCM_size)/2048)*2048 - 44`.
For several PCM sizes ending in `2 mod 4`, the game's DirectSound
buffer is padded with 2 bytes; those padding bytes are not the
source-file tail and remain a separate issue.

These 12 share the **exact same file-sector truncation signature**
seen in the initially broken assets, even though the independently
disassembled `0x00430A40` generic reader is capable of consuming
partial sectors. The next causality test must therefore distinguish
(1) a *caller* passing a prematurely rounded-down byte count,
(2) another WAV-to-DirectSound construction routine dropping the
remainder, or (3) data lost downstream of the generic resource read.
Do not infer which one is true until code-level evidence exists.

A revised SHA-gated **read-only offline** inspector now outputs:
- `0x00470F00–0x00472A50`: game audio manager, including
  `0x00471822` (known WAV template-loading call) and
  `0x004727F9` (original DirectSound voice duplication call);
- `0x004FBD60–0x004FC050`: native bank/resource lookup helper
  near `0x004FBDC0`, used by known native loader;
- direct `E8` call candidates into `0x00430A40`, `0x00430940`
  and other resource/DirectSound routines;
- candidate references to the original DirectSound object global
  `0x006B7920`, template-buffer table `0x006BBAD4`,
  and 32 active-voice descriptors `0x006BBD50`.

**Next user evidence**: run the updated PowerShell diagnostic
on the same SHA-verified EXE and send its new report, preferably
`spiderman-audio-buffer-population.txt`. Code and default game
behavior are completely unchanged; the native loader repair stays
opt-in and legacy pre-Play restore stays as a safety net. Do not
assert perfect Linux audio until a Linux/Wine listening run confirms it.

## Native DS buffer xrefs and sound-bank manager confirmed in full Windows report

User supplied `spiderman-audio-buffer-population.txt`, a **5155-line**
SHA256-verified static disassembly/cross-reference report of original
`SpideyPC.exe` on 27 Oct 2026. It confirms:

| Original VA | Evidence and interpretation |
| --- | --- |
| `0x00471550` / `0x00471580` | Game audio manager opens and uses the generic, partial-sector-aware resource reader to populate a temporary block used in sound-bank preparation |
| `0x00471822` | **Only direct `E8` reference** to original `0x00503B40`, the 64-reference-WAV DirectSound buffer creator |
| `0x00503B40` / `0x00503EBD` / `0x00503EFF` / `0x00503F83` / `0x00503FCD` | Native WAV load → `IDirectSound::CreateSoundBuffer` → `Lock` → memory copy → `Unlock`, whose incomplete final WAV file sector is fixed by the current opt-in loader hook |
| `0x004727F9` | **Only direct `E8` caller** of native `0x00504110`, which duplicates an original reference buffer into one of 32 game voices |
| `0x004FBDC0` | Native sound-bank selection helper; direct references at `0x00503B5D` and `0x005040AE` |
| `0x004711CB` / `0x004711E7` | Another audio-manager path also calls the generic resource open/transfer routines. No proof this path directly builds a DirectSound PCM template. |

Across the **whole original .text** byte stream, raw exact
4-byte VA occurrence counts were:

- **112** occurrences of `0x006B7920` (game's DirectSound
  object pointer), heavily concentrated in `0x004FD800–0x00500D00`;
- **5** occurrences of `0x006BBAD4` (template/reference table),
  all in the known native DirectSound module
  `0x005039F0–0x00504135`;
- **11** occurrences of `0x006BBD50` (secondary-voice table),
  again in the known native DirectSound module
  `0x005039F0–0x00504436`.

These are **raw immediate patterns**, not automatically established
decoded references or alternative sound buffers; the 112-pointer
cluster *could be* an additional DirectSound consumer or repeated
methods on the same object. It must be disassembled before assigning
any audio-loader functionality to it.

**Next investigation tool now included in the PR:** the read-only
PowerShell and Python inspectors emit a raw x86 `FF /2`
register-vtable-call census for possible
`CreateSoundBuffer (+0x0C)`, `DuplicateSoundBuffer (+0x14)`,
`Lock (+0x2C)`, and `Unlock (+0x4C)`. Those offsets are **not
sufficient to identify DirectSound without object provenance**, so
the tools also disassemble representative functions around
`0x004FD800–0x004FDAC0`,
`0x004FEF70–0x004FF260`,
`0x005000B0–0x00500260`, and
`0x00500700–0x00500940` to inspect actual call contexts.

Important: **no runtime DLL, hook, WAV asset, or PKR change** in this
forensic stage; existing 64 loader-tail repairs and 12 remaining
legacy pre-Play fixes are unchanged. Do not disable legacy
`ExperimentalAudioPKRTailRestore` or assume the other consumer is
the culprit absent instruction-level proof. Windows listening
reported no audible parasites; independent Linux/Wine listening
is still pending.

## COM-vtable census resolves repeated DirectSound references to cleanup paths (27 Oct 2026)

The user delivered `spiderman-audio-dsound-callers.txt`, a **6144-line**
SHA256-verified full native x86 disassembly/census from Windows.
The generic raw `FF /2` short-disp register vtable call
census found:

| Potential interface vtable offset | Raw opcode candidates | Confirmed example |
| --- | ---: | --- |
| `+0x0C` (might mean `IDirectSound::CreateSoundBuffer`) | 97 | `0x00503EBD` known game WAV creation |
| `+0x14` (might mean `IDirectSound::DuplicateSoundBuffer`) | 30 | `0x00504152` known game voice duplication |
| `+0x2C` (might mean `IDirectSoundBuffer::Lock`) | 7 | `0x00503EFF` known WAV fill; `0x00503F53` retry |
| `+0x4C` (might mean `IDirectSoundBuffer::Unlock`) | 2 | `0x00503FCD` known WAV Unlock |

**Caution:** the counts represent **raw matching byte patterns
within `.text`**, not confirmed instruction boundaries or
DirectSound interfaces. They only recognize the 3-byte
`FF 50–53 XX` form, not indirect calls through other registers,
longer displacements or wrappers.

Four representative disassembled windows near
`0x004FD800`, `0x004FEF70`, `0x005000B0`, and
`0x00500700` **resolve the large
`0x006B7920` reference cluster**: many are repeated error
cleanup blocks, not WAV buffer constructors.
A representative sequence is:

```asm
4fd8a0: call 0x503af0         ; game audio cleanup
4fd8a5: mov eax,[0x6b7920]   ; DirectSound object
4fd8ae: mov edx,[eax]
4fd8b0: push eax
4fd8b1: call [edx+0x08]      ; IUnknown::Release
...
4fd8dc: mov [0x6b7920],ebx   ; set pointer NULL
```

The pattern also repeats at `0x004FDA41`,
`0x004FEFB4`, `0x004FF0A1`,
`0x005000F2`, `0x005001C0`,
`0x005007C1` and other sites. In contrast, one of the
`+0x14` candidates at `0x004FD972` operates on a COM
object from **`0x006B7904`**, not `0x006B7920`
(`0x004FD9B7` acts on `0x006B7908`); an identical offset
is therefore **not evidence of DirectSound duplication**.
There is **no newly proven PCM/DirectSound loader** in these
four windows.

The remaining concentrated potential method sets are
`+0x0C` at `0x005015A8`, `0x00501728`,
`0x00501AFB`, `0x00501B31` and
`+0x2C` at `0x00501629`, `0x005017A6`,
`0x00501903`, `0x00502A0B`, with
additional `+0x14` near `0x005005AC`,
`0x005005E8`, `0x005028FE`, and
`0x005029D3`.
**These are not yet decoded into proven game sound calls.**

**Next read-only experiment:** add disassembly windows
`0x00501480–0x00501C20`,
`0x005027F0–0x00502BB0`, and
`0x005004C0–0x00500640`, on the same hash-verified EXE.
Classify the *underlying COM object pointers and call argument
layout* before deciding whether any of these operations creates
or fills WAV buffers. **No additional runtime hook, default
change, or workaround is justified yet.** Keep all 12 Play-time
tail restores and the verified loader fix intact.

The user heard no artifacts on their Windows session;
Linux/Wine listening validation is still planned.

## DirectInput/graphics vtable false positives eliminated; new 44-byte skip correlation

The user supplied the original, SHA-256 verified
`spiderman-audio-com-interfaces.txt` (~7,271 lines) on 27 Oct 2026.
The three latest windows give actual decoded instruction and
argument context for the apparent second DirectSound candidate set:

- **`0x005015A8` and `0x00501728`** call
  `[ecx+0x0c]` on global object **`0x006B7A30`**, passing
  4 arguments (object, device GUID
  `0x0053C9E8`/`0x0053C9F8`, output pointer, NULL):
  consistent with **`IDirectInput::CreateDevice`**.
  Outputs go to **`0x006B7A5C`** and
  **`0x006B7A64`**, not DirectSound templates.
- **`0x00501629`, `0x005017A6`, and `0x00501903`**
  call `[vtable+0x2c]` on these input devices with **one**
  data-format pointer as input besides the object: this is
  **`IDirectInputDevice::SetDataFormat`**, NOT the eight-argument
  `IDirectSoundBuffer::Lock`. Confirming adjacent calls are
  `SetCooperativeLevel (+0x34)`,
  `Acquire (+0x1c)`, and an `EnumDevices (+0x10)`
  callback at `0x005018AA` (enumeration routine
  `0x00501AE0`).
- **`0x00501AFB`** calls the input-device creation method
  on `0x006B7A30` again; **`0x00501B31`**
  uses `+0x0c` on a **created input device**
  `0x006B7A2C` (capability query), not on
  `IDirectSound`.
- **`0x005005AC`, `0x005005E8`, `0x005029D3`**
  use graphics-related COM objects `0x006B7904`,
  `0x006B7908`, and **six** stack arguments including
  `0x01000400`/`0x01000000`. They cannot be
  `IDirectSound::DuplicateSoundBuffer`, which uses two
  parameters plus the object. Similarly `0x00502A0B`
  uses `0x006B7904` with **two** params plus the object;
  this is not the eight-parameter
  `IDirectSoundBuffer::Lock`. The exact graphics
  interface subtype is not asserted.
- `0x005028FE` acts on another graphics object
  `0x006B791C` with a single object argument.
  It is not the native sound buffer duplication path.
  
**Result:** NONE of the new COM candidates establishes a second
WAV-to-DirectSound population route. The prior raw
`FF 51 0c`/`FF 51 2c` offset-only heuristic caused false
positives from DirectInput. Do not add a hook based on these sites.

### Stronger independent correlation using 27 Oct 10:13 game audio log

The *already uploaded* original
`SpiderMan2k1Fix(20261027-101351).log` was
cross-correlated with this disassembly. It contains
**64 `LOADER-TAIL-RESTORED` events** (62 unique file names),
**27 `LOADER-SKIP-NO-UNIQUE-SOURCE` events** (no file name),
and **12 distinct legacy `TAIL-RESTORED` sound names**.
There is **zero filename overlap** between the latter 12
and the 64 loader fixes.

For all 12 late-restored sounds, the exact native PCM corruption
starts at the final original-WAV sector boundary. For **10/12**,
there is a unique *value* of an earlier loader SKIP remainder
equal to `(44 + PCM_bytes + 44) % 2048`, i.e.
**the original WAV file-sector remainder plus precisely 44 bytes**:

| Late Play-restored WAV | Original WAV remainder `(PCM+44)%2048` | Earlier loader-SKIP remainder `(PCM+88)%2048` |
| --- | ---: | ---: |
| PickConf.wav | 782 | 826 |
| Crawl1.wav | 574 | 618 |
| Crawl2.wav | 866 | 910 |
| Swing5.wav | 1956 | 2000 |
| WebShot10.wav | 374 | 418 |
| WebShot3.wav | 1304 | 1348 |
| WebShot11.wav | 1608 | 1652 |
| h_hitfloor.wav | 190 | 234 |
| Pistol1.wav | 1294 | 1338 |
| Pistol2.wav | 2 | 46 |

`achum.wav` (original remainder 236) and
`tingle2.wav` (1674) do **not** match any of
the 27 earlier skipped remainders at `+44`.

For comparison, **17/17 named sources** appearing in
both `LOADER-TAIL-RESTORED` and Play-time
`MATCH original=...` log entries have exact ordinary
`(PCM+44)%2048` equality (i.e. no 44-byte shift).

The **10/12 numerical alignment is striking** and more
actionable than any COM-offset raw-xref. But note:
**skipped loader events have no source identifiers**,
so this is NOT yet an established file-level match or
proof of a second 44-byte header. It may indicate
an extra 44 bytes of resource data, a WAV-container
difference, or another loader-length discrepancy.
`achum`/`tingle2` need independent investigation.
No file contents or archive bytes have been modified.

**Next decisive diagnostic (not another huge COM scan):**
The existing, strictly **opt-in and x86-signature-gated**
native loader now logs an additional bounded, **read-only**
`PKR-AUDIO: LOADER-SKIP-DETAIL` line, but ONLY after
`LOADER-SKIP-NO-UNIQUE-SOURCE` and ONLY for the already
successfully read in-bounds full WAV file sector bytes.
No new hook or default option is required. This record
includes the original native resource `total_bytes`,
`complete_bytes`, `remainder`, `riff_at_0`,
`riff_at_44`, `riff_declared_bytes`,
`data_chunk_bytes`, and `riff_excess_bytes` =
actual resource length minus the RIFF-declared length
(if a bounded RIFF header can be recognized).
It does not allocate, seek, issue extra sector reads,
touch the source archive, or modify the input bytes.

**Decisive next test:** use the newly built Windows x86 DLL
(with `ExperimentalAudioLoaderTail=1` as previously tested)
and upload one complete new game log after playing
the same sequence. If the ten numeric skip matches
show `riff_excess_bytes=44` and actual `total_bytes`
equal to the original-source `PCM_bytes+88` for
each uniquely matching sound, we will have substantially
stronger evidence for an extra 44 bytes *outside*
the RIFF-declared WAV data. If `riff_at_0` is false
or `riff_excess_bytes` differs, revisit the hypothesis.
Do not assign a source name on modulo-2048 match alone;
absolute lengths, headers and source PCM should agree.

Crucially, do not copy any unverified remainder, do not
modify `data.pkr`, and keep existing loader and Play-time
repairs until identity is proven and validated on
Windows and Linux/Wine.

## Confirmed: skipped native WAVs have valid RIFF trailers (Windows 27 Oct 11:45 log)

The user retested with the **first read-only RIFF header telemetry**
(`SpiderMan2k1Fix(20261027-114503).log`). We now have an
unambiguous structural explanation for the strict-loader SKIPs.

**Measured:**
- The SHA-verified game successfully armed both original audio fixes.
- **64** `LOADER-TAIL-RESTORED` events as before; strict old matcher
  rejects **27** other WAV loads.
- **27/27** rejects have `riff_at_0=1`, `riff_at_44=0`,
  and `riff_declared_bytes == total_bytes`
  (`riff_excess_bytes=0`). Original RIFF files are
  **not oversized beyond the RIFF-declared boundary**.
- **25/27** have `total_bytes == data_chunk_bytes + 88`;
  **2/27** have `total_bytes == data_chunk_bytes + 234`.
  Crucially, `data` is still at standard WAV offset 36,
  so the PCM starts at offset 44. The original WAV contains
  **44 or 190 additional bytes following the PCM data within RIFF**.
  The archive reader already accepts extra RIFF chunks.
- **13/13** later Play-time `TAIL-RESTORED` sounds have a
  **distinct SKIP event with the exact same `data_chunk_bytes`**
  (11 with 44 trailing bytes, and two with 190).

| Late-restored WAV | PCM bytes | Native WAV total | Bytes after PCM |
| --- | ---: | ---: | ---: |
| `PickConf.wav` | 43746 | 43834 | 44 |
| `achum.wav` | 32960 | 33194 | 190 |
| `Crawl1.wav` | 23058 | 23146 | 44 |
| `Crawl2.wav` | 19254 | 19342 | 44 |
| `catland3.wav` | 11798 | 11886 | 44 |
| `Swing5.wav` | 32632 | 32720 | 44 |
| `WebShot10.wav` | 18762 | 18850 | 44 |
| `WebShot3.wav` | 15596 | 15684 | 44 |
| `WebShot11.wav` | 13852 | 13940 | 44 |
| `tingle2.wav` | 56926 | 57160 | 190 |
| `h_hitfloor.wav` | 18578 | 18666 | 44 |
| `Pistol1.wav` | 29922 | 30010 | 44 |
| `Pistol2.wav` | 22486 | 22574 | 44 |

This **corrects the previous hypothesis**: the extra 44 bytes
do not represent a second leading 44-byte WAV header or an
oversized RIFF file. They are bytes AFTER the data chunk,
inside the declared RIFF container. All sources were already
CRC-verified by the plugin's RIFF chunk parser; the overly strict
`SpiderPKRAudioIndex_ExpectedLoaderWavTail` had the outdated
assumption `total_bytes = 44 + PCM_length`.
This also explains **both previously unmatched** samples
(`achum` and `tingle2`) and the extra
`catland3` heard in the new session.

### Opt-in native-loader full-RIFF fix: Windows runtime validated 27 Oct 11:59

The branch now retains each original **complete** verified WAV
allocation instead of allocating and copying only its PCM.
`WAVSource.raw` owns the same CRC-verified allocation;
`WAVSource.pcm` aliases the verified data chunk, so
the independent PCM matching/Play-time fallback retain
their existing semantics. The original PCM cache byte counter
remains PCM-only; the 128-MiB cache cap instead includes all
retained RAW WAV bytes.

The native loader candidate is accepted ONLY if:
1. the game already read the full-sector prefix successfully,
   the original RIFF declares exactly the allocated native length,
   and the SHA/signature/opt-in guards still pass;
2. exactly ONE independent CRC-verified original WAV has the
   **same total RIFF length** and an **exact bytewise match of
   ALL complete 2048-byte sectors**, including WAV header,
   PCM prefix, and any metadata;
3. the PCM data end extends **beyond** the complete-sector boundary.
   We intentionally do NOT modify metadata-only last sectors;
4. the extra native PKR reader call for one private scratch
   sector succeeds and its remaining bytes exactly match the
   independent verified original WAV bytes, including metadata.

Only then copy the exact remaining bytes into the game's
already-allocated native WAV staging buffer. The native
reader and COM/DirectSound path remain unchanged.
**No changes** to `data.pkr`, native game binary, or the
separate stopped-DirectSound Play-time fallback; nothing
new is enabled by default.

**Windows runtime outcome (user-provided log
`SpiderMan2k1Fix(20261027-115910).log`, EXE SHA-verified):**
the new `FULL RIFF` loader correction works. This second
session reproduced the same intro, initial 3D and return to menu.

| Metric | Old Windows 11:45 log | New Windows 11:59 log |
| --- | ---: | ---: |
| Native `LOADER-TAIL-RESTORED` event count | 64 | **90** |
| Distinct named WAVs restored by native loader | 62 | **88** |
| `LOADER-SKIP-NO-UNIQUE-SOURCE` event count | 27 | **1** |
| Native `LOADER-SKIP-REMAINDER-MISMATCH` | 0 | **0** |
| Stopped-DirectSound pre-Play `TAIL-RESTORED` | 13 | **0** |
| `FULL-PCM IDENTICAL` checks | 40 | **40** |
| Indexed CRC-verified WAVs | 656 | **656, no CRC failures** |
| Separate `Heli1.wav` two-sample declick | enabled | **still applied** |

**26 of the previously 27 skipped original WAV loads**
were promoted to full-file verified native repairs. The
**13 distinct WAVs** previously repaired at Play are all now
named in successful native `LOADER-TAIL-RESTORED`
events: PickConf, achum, Crawl1, Crawl2, catland3,
Swing5, WebShot10, WebShot3, WebShot11, tingle2,
h_hitfloor, Pistol1, Pistol2. During this session the
previously damaged `achum.wav`, `Swing5.wav`,
`WebShot11.wav`, `WebShot3.wav` are explicitly
reported `FULL-PCM IDENTICAL`; other tested sounds
now generate `TAIL-SKIP reason=already-identical`,
with no new `TAIL-RESTORED` events.

**The single remaining native SKIP is correct by design:**
`total_bytes=18452`, `complete_bytes=18432`,
`remainder=20`, `data_chunk_bytes=18364`.
The original PCM ends at byte `44+18364=18408`,
**24 bytes BEFORE the complete-sector boundary**.
Thus the 20-byte unread remainder is metadata-only.
This sample corresponds to `BreakFree.wav` by
native PCM length / subsequent exact `MATCH`
and `FULL-PCM IDENTICAL` logs. We intentionally
do not request an extra sector for metadata-only
tails, because they cannot account for damaged audio.
This one remaining SKIP is **not a defect**.

The game progressed into the second 3D script, active
gameplay, and returned to the original main menu with
the native request-7 cleanup. The log shows no new
audio patch failure or crash. `Heli1.wav` is
`FULL-PCM IDENTICAL` on first playback; later
four-byte source differences are the *intentional*
two-sample declick shared by duplicated voices,
not lost final sectors.

**Status:** native and pre-Play audio correctness is
**validated on this Windows runtime session**.
This does NOT establish that every level, audio path,
hardware, or Linux/Wine driver is artifact-free.
Await independent subjective listening evaluation
on user's Linux/Wine computer before promoting
the experiment. The PR remains **draft and unmerged**;
the two opt-in audio fallback options remain available
and unchanged.


## Linux/Wine extended gameplay trace — 27 Oct 2026, 18:45

The user supplied `SpiderMan2k1Fix(20261027-184558).log`
(1,775 physical lines; game build `v1.0.0-b100f84`,
a test merge commit that **does contain** the final
CRC-verified native-loader rootfix code from `40dd036`).

**CRITICAL interpretation:** The new code was **not
active in this Wine session**. The complete log has no
`PKR-AUDIO: LOADER-HOOK ARMED` entry, no
`LOADER-HOOK REFUSED` entry, and zero native
`LOADER-TAIL-RESTORED` events. In that code revision
`SpiderPKRAudioLoader_Init` returns without logging
when `[Debug] ExperimentalAudioLoaderTail` is not
`1`. The INI validator reported a valid configuration,
so the feature was almost certainly absent or set
to zero. This is an **opt-in configuration omission,
not evidence of a Wine-specific native-loader failure**.

The older **pre-Play DirectSound fallback did run**:

| Wine trace measurement | Observed |
| --- | ---: |
| Independent CRC-verified original PKR WAV sources | 656 of 656; 0 CRC errors |
| Pre-Play `TAIL-RESTORED` events | **100** |
| Distinct source WAV names repaired | **77** |
| Total PCM tail bytes restored (across all events) | **92,780 bytes** |
| `FULL-PCM DIFF` / `FULL-PCM IDENTICAL` messages | 43 / 24 |
| `TAIL-SKIP` with already-identical / non-tail-only differences | 24 / 3 |
| Read-only DirectSound Play traces | 256 complete before/after pairs (configured trace cap) |
| Failed HRESULTs among all four API-query HRESULTs across the 256 pairs | **0** |
| `Heli1.wav` declick | activated, including two-sample replacement |
| Native load-time `LOADER-TAIL-RESTORED` | **0 (loader opt-in not armed)** |

The source-level PCM differences still align precisely
with the missing final partial 2,048-byte WAV sector.
Common impacted files include `wind2.wav`, `wind3.wav`,
`Heli1.wav`, `ThugStp*.wav`, `ThugHit*.wav`,
`Crawl1.wav`, `Crawl2.wav`, `catland3.wav`,
and many later fight and ambience sounds. DirectSound
`Play` changed all 256 sampled buffers from stopped
status 0 to 0x1/0x5; all queried buffer capability,
format, status and position HRESULTs were zero.
After more than 14,000 DirectDraw flips and several
in-game scripts/levels, native controller-7 cleanup
returned to the main menu; no logged audio failure
or process crash. **This supports robustness of the
fallback under Wine, not auditory absence of crackles.**

**Necessary second Wine run, with no DLL rebuild
required for this same binary:** close the game and
set `ExperimentalAudioLoaderTail=1` under the existing
`[Debug]` section in `SpiderMan2k1Fix.ini`.
Keep `ExperimentalAudioPKRTailRestore=1` as a safety
fallback and `ExperimentalAudioHeli1Declick=1` for
the independent helicopter PCM discontinuities.
After launch, first require `PKR-AUDIO: LOADER-HOOK
ARMED`, then examine native `LOADER-TAIL-RESTORED`
and any remaining late `TAIL-RESTORED` events
(ideally none on the same paths). Exact counts will
depend on sound assets visited. Ask the tester for
subjective audio results to complete Wine validation.
Leave this PR draft/unmerged.

## Linux/Wine native-loader enabled run — 27 Oct 2026, 18:58

The user then supplied `SpiderMan2k1Fix(20261027-185801).log`,
the **same binary** `v1.0.0-b100f84` under Wine, with the
`ExperimentalAudioLoaderTail` opt-in now **active**.

**Result:** `PKR-AUDIO: LOADER-HOOK ARMED` occurred immediately during
initialization, with no hook-refusal message. This test finally exercises
the **native root-fix on Linux/Wine** rather than only the prior
DirectSound pre-Play fallback.

| Metric | Previous Wine run (loader off) | New Wine run (loader on) |
| --- | ---: | ---: |
| Native hook `LOADER-HOOK ARMED` | No | **Yes** |
| Logged native `LOADER-TAIL-RESTORED` | 0 | **96 entries** |
| Distinct source names among logged native restores | 0 | **94** |
| Logged original WAV tail bytes loaded at native stage | 0 | **92,040** |
| Late DirectSound `TAIL-RESTORED` | **100** | **0** |
| Read-only `FULL-PCM IDENTICAL` | 24 | **62** |
| Read-only `FULL-PCM DIFF` | 43 | **1** |
| Native `LOADER-SKIP-NO-UNIQUE-SOURCE` | n/a | **6** |
| `LOADER-SKIP-REMAINDER-MISMATCH` | n/a | **0** |
| CRC-valid WAV source records | 656/656 | **656/656** |
| Native 3D return-to-menu teardown | successful | **successful** |

**Logging cap:** `pkr_audio_loader.c` logs at most the first **96**
successful native load-time restorations via `MAX_LOADER_DIAG`.
Thus 96 is the number **visible**, not a confirmed exact cumulative
count for the whole session. There are 94 distinct names among these
logged entries; `grenade1.wav` and `gunbutt.wav` appear twice.
Likewise 92,040 is the sum of *logged* remainder sizes, not the
necessarily complete lifetime total. This session contains ~7,922
DirectDraw Flips, while the prior Wine run has >14,000, so counts
should not be mistaken for speed/performance benchmarks.

The sole `FULL-PCM DIFF` was **four bytes** in a duplicated
`Heli1.wav` voice at original PCM offsets 24190 and 24219:
it corresponds exactly to the two intentional Heli1 sample
replacement positions 12095 and 12109 (two 16-bit samples). It is
**not** a loader-tail regression. The independently opted-in Heli1
declick was applied twice in the run.

### Remaining six skipped native resources: important distinction

Three repeated skips involve a **20-byte final tail** with
`total_bytes=18452`, `complete_bytes=18432`,
`data_chunk_bytes=18364`. PCM ends at offset
`44+18364=18408`, **24 bytes BEFORE** the complete-sector limit,
so **only non-PCM metadata** is missing and skipping is safe as
far as audible content is concerned.

However, three newly encountered skip events **require further
investigation**:

- Two encounters of `total_bytes=59852`, `complete_bytes=59392`,
  `remainder=460`, `data_chunk_bytes=59808`.
  PCM ends at `44+59808=59852`, so all **460** skipped bytes
  could contain PCM, rather than trailer metadata.
- One encounter of `total_bytes=10180`, `complete_bytes=8192`,
  `remainder=1988`, `data_chunk_bytes=10136`.
  PCM ends at `44+10136=10180`, so all **1,988** skipped bytes
  could contain PCM.

Both have RIFF/WAVE at offset 0 and self-consistent RIFF length.
`SKIP-NO-UNIQUE-SOURCE` means there was **no unique exact
CRC-verified candidate in the current `data.pkr` WAV index**.
This could be an asset outside the indexed WAV sources or an
ambiguous candidate; the existing trace cannot distinguish those
cases or identify original filenames. The loader correctly
**refused to guess or patch bytes**. There were **zero** late
DirectSound repairs recorded, but this does not prove these
three loads never affected playback. An enhanced **read-only
source-identification probe** would be the safe follow-up.
Do not relax unique-source/memory-integrity gates without proof.

**Runtime:** The original game completed a 3D gameplay session
and returned through native controller-7 cleanup to the menu,
without any reported process crash or audio API failure.
**Audible subjective quality for this new Wine run has not
yet been reported by the user.** Their earlier confirmation
of zero crackles applies to the **old** fallback-only Wine
run, and must not be represented as a listening test
of the new loader code.

**Decision:** This run provides strong technical validation
of the native WAV root-fix **under Wine**. It does **not yet
prove complete coverage**, because of the two distinct
unidentified PCM-bearing WAV lengths and pending subjective
listening. Leave fallback enabled and PR #67 **draft/unmerged**.

## Next diagnostic: strictly read-only unmatched WAV fingerprints (28 Oct 2026 research follow-up)

After the successful and audibly clean Linux/Wine loader-enabled test, PR #67
adds **diagnostic logging only** for loader calls that were already rejected
by the existing strict source-identification guard. The audio fix,
stored WAV source CRC checks, native read behavior, memory writes,
source identity decisions, DirectSound, Heli1 declick, and fallback
remain **byte-for-byte behaviorally unchanged**.

When a `LOADER-SKIP-NO-UNIQUE-SOURCE` event is accompanied by
`LOADER-SKIP-DETAIL`, a new `LOADER-SKIP-FORENSIC` line reports:

- `pcm_tail_bytes`: how much of the unread RIFF remainder intersects
  the header-declared audio data for standard 44-byte-header WAVs
  (`0` on the identified 20-byte metadata-only tail);
- `wav_format`, `channels`, `hz`, `bits`: read-only format fields
  from an existing `fmt ` chunk at offset 12, when present;
- `head2048_fnv1a` and `last_loaded2048_fnv1a`: deterministic
  **noncryptographic** 32-bit FNV-1a fingerprints of the first and
  last already-loaded 2,048 bytes; no raw PCM/audio is logged;
- `indexed`: number of cached CRC-verified original `data.pkr` WAVs;
  `same_size`: cached originals having the same **entire RIFF file length**;
  `head2048`: cached originals with the same first 2,048 bytes
  (regardless of length); `full_prefix`: same-sized cached originals
  with an exact complete loaded prefix;
- `head_hint` / `full_hint`: a filename printed only if the
  corresponding candidate count is exactly one. A `head_hint`
  is **not** sufficient for source identity or repair.

These fields should distinguish an already-cached-but-ambiguous source
from a WAV unrepresented in the 656-source cache, and allow later
offline asset comparison. A zero candidate count does **not** establish
which external or unindexed asset was involved.

**Critical safety:** The new code never asks the game for an extra
sector, reads more than the already-loaded WAV staging prefix, changes
the correction decision or relaxes the current fail-closed matching.
Candidate scans are capped by the existing 656-source cache and skip
logging is capped by the existing `MAX_LOADER_DIAG` limit.
There is no new INI key: keep
`ExperimentalAudioLoaderTail=1` and the two independent fallback/declick
options for testing.

For the next Linux/Wine test on the new PR #67 artifact, inspect the
`LOADER-SKIP-FORENSIC` entries for `pcm_tail_bytes=460` (twice)
and `pcm_tail_bytes=1988` (once), then provide the resulting log.
The test will **identify candidates**, not automatically repair
unverified files. Keep the PR unmerged until both technical and
listening validation are complete.

## Archived investigation plan (before native root cause was identified)

The following was the former diagnostic plan, retained as historical context. The exact responsible instruction has since been established and the native loader correction validated in the Windows session above.

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
