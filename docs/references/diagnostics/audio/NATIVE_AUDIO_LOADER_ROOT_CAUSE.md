# Root-cause investigation: original DirectSound WAV loader (27 Oct 2026)

## Status: analysis in progress; no root fix asserted

The **user-approved audible repairs** (generic CRC-backed native PCM sector-tail
restoration and the two isolated original \`Heli1.wav\` outliers) remain unchanged.
This document starts a **separate, read-only forensic phase**, without changing
DirectSound implementation, creating new game hooks, or writing game assets.

The current native loader defect is strongly indicated by the exact onset
of the damaged last partial RIFF/WAV file sector:

\`\`\`text
(first mismatched PCM byte + 44-byte RIFF/WAV header) % 2048 == 0
\`\`\`

This was observed in many distinct audio sources from the **supported exact
SpideyPC.exe**, including \`Heli1.wav\`, \`ThugHit2.wav\` and \`traffic2.wav\`.
The original 656/656 source WAVs in \`data.pkr\` passed CRC32, while game-owned
native DirectSound secondary buffers were defective **before** \`DXSOUND_Play\`.
The PCM buffer sizes are sometimes 4-byte aligned, adding one silent 16-bit
sample when the original length is \`4k+2\`. Both features suggest a native
size/copy path issue, but **do not by themselves prove** the precise bad
instruction or that the API \`Lock\`, \`Unlock\`, \`DuplicateSoundBuffer\`, or
the Wine DirectSound implementation caused it.

A separate source-asset defect exists in the original \`Heli1.wav\` at
PCM samples 12095 and 12109; the game runtime cannot be blamed for those
two already-bad input samples. These are two distinct bugs.

## Verified code landmarks in original executable

| Original game VA | Evidence | Known meaning |
|---|---|---|
| \`0x005039F0\` | SHA-bound native signature in \`focus_dsound_pause.c\` | 32-slot voice initialization |
| \`0x00504110\` | signature \`8B 0D 20 79 6B 00 53 56 57\` | \`DXSOUND_Open\` / voice duplication path |
| \`0x00504230\` | existing native Play hook | \`DXSOUND_Play\` |
| \`0x005042A0\` | signature in \`focus_dsound_pause.c\` | \`DXSOUND_Stop\` |
| \`0x00472870\` | \`E8 BB 19 09 00\` | game's native callsite into Play |

The independent \`krystalgamer/spidey-decomp\` source implements
\`DXSOUND_Open\` with \`IDirectSound::DuplicateSoundBuffer\`, followed by
the native \`DXSOUND_Play\` using \`IDirectSoundBuffer::Play\`. However,
\`DXSOUND_CreateDSBuffer\`, \`loadWAV\`, and \`ParseWavHeader\` are marked
TODO or are placeholders in that decompilation. The decompilation
is **not proof** of those missing native functions' actual behavior.
The unknown loader addresses are deliberately left **unasserted**.

## Static, reproducible next step under Linux

The branch contains a standard-library Python 3 probe that:

1. Refuses any executable whose size or SHA256 differs from the supported
   \`SpideyPC.exe\` (size **1,507,328**, SHA256
   \`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea\`).
2. Validates the PE32 i386 section mapping and previously checked native
   site bytes.
3. Disassembles the native audio code neighborhood
   \`0x005039D0–0x00504340\` via \`objdump\`, typically installed as
   \`binutils\` on Linux.
4. Reports nearby raw immediate matches for \`0x800\`, \`0x7FF\`,
   \`0xFFFFF800\`, \`0x400\`, and \`0x7FC\`.
5. Enumerates candidate x86 near-call (\`E8 rel32\`) references from
   the executable's \`.text\` into that range.
6. Produces a UTF-8 report. **Raw byte matches and call signatures are
   candidates**, not verified instruction boundaries or confirmed
   functional identifications.

From a checked-out branch on Linux, run:

\`\`\`bash
python3 ChatGPT/tools/inspect_audio_load_x86.py \
  "/path/to/SpideyPC.exe" \
  -o spiderman-audio-root-disassembly.txt
\`\`\`

Or download the single standalone Python script from the PR branch first.
The probe does **not** run the game or Wine, alter memory, modify the
executable or archive, or invoke any network operation. Its output is
an offline disassembly of only the known audio neighborhood, plus
bounded candidate call references.

## What will settle the true origin

After examining the exact native instruction sequence, identify and
verify \`DXSOUND_CreateDSBuffer\` / \`loadWAV\` / \`ParseWavHeader\`
entry points and the effective 2,048-byte transfer loop.

Only then add an **off-by-default, signature-gated, read-only native
timeline** around the actual loader. For the selected CRC-verified
WAVs \`Heli1.wav\`, \`ThugHit2.wav\` and \`traffic2.wav\`, compare:

1. PCM source immediately after PKR decompression;
2. PCM data and length passed into the native DirectSound write;
3. original/unduplicated DirectSound template buffer **after
   the game's final Unlock**, before \`DuplicateSoundBuffer\`;
4. secondary game voice **after duplication**, before native Play.

A corruption present before step 2 implicates the WAV/parser/copy
preparation. A defect first visible in step 3 implicates the transfer
to DirectSound (native length, offset, lock regions, or API behavior).
A defect only at step 4 focuses attention on the duplicate/share path.
**No diagnosis of the actual responsible instruction is possible from
the Play logs alone.**

Do **not** patch arbitrary addresses or assume
\`DuplicateSoundBuffer\` produces an independent PCM copy: DirectSound
can share sample memory between duplicated buffers. Do not modify
any template or duplicate that may be active; retain the validated
user-approved pre-Play repair as a fallback until a new low-level
repair is independently tested in Wine and Windows.

No merge of draft PR #67 without explicit user approval.

legluondunet assisted by ChatGPT (OpenAI)
