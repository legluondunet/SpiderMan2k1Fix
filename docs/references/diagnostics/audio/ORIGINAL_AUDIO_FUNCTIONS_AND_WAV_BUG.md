# Spider-Man (2001 PC) — original audio functions and proven WAV-sector bug

**Current technical reference (27 October 2026).** This is a navigable, maintained summary of the original x86 disassembly and the root cause of the audio crackling fixed in [PR #67](https://github.com/legluondunet/SpiderMan2k1Fix/pull/67) and enabled by default in [PR #70](https://github.com/legluondunet/SpiderMan2k1Fix/pull/70). The full *chronological* evidence, including hypotheses later rejected, is preserved separately in [NATIVE_AUDIO_LOADER_ROOT_CAUSE.md](NATIVE_AUDIO_LOADER_ROOT_CAUSE.md). The [implementation guide](PKR_NATIVE_AUDIO_REPAIR.md) covers the active patch.

**Binary identity and scope:** original `SpideyPC.exe`, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
**All addresses below are original executable virtual addresses (VA)**, not file offsets or RVA, unless explicitly labeled otherwise. These addresses are not portable to other editions. The retail executable and original sound assets are **not** included in this repository. This is a partial function and instruction map, **not** a complete decompilation.

## 1. Verified native function and instruction map

A *function* is distinguished from a callsite, an instruction range, a COM method call and a data address. Names such as “WAV template loader” describe observed behavior; they are not necessarily original binary symbols.

| Original VA / site | Type | Recovered role and evidence boundary |
| --- | --- | --- |
| `0x00470F00–0x00472A50` | Native code region | Game audio/sound-bank manager and 32-slot voice management. The full region was inspected statically; this is **not** one function. |
| `0x00471550` / `0x00471580` | Call contexts | Game audio manager invokes the *generic resource-transfer path* while preparing temporary sound-bank data. Not themselves established as DirectSound template-buffer loaders. |
| `0x00471822` | `CALL` site | Direct call to `0x00503B40`; the examined complete x86 near-call census found this to be its only direct `E8` reference. |
| **`0x00503B40`** | **Native function** | Loads/initializes a bank of **64 WAV reference/template buffers**. Opens WAV resources, obtains their byte lengths, allocates temporary file buffers, reads PKR sectors, parses/copies audio and populates original DirectSound template buffers. This is **the defective path**. |
| `0x00503C9C` → `0x00504CB0` | Native `CALL` and callee | Opens/accesses a PKR resource for the WAV loader. The call is verified; its full high-level open/handle contract is **not completely reconstructed**. |
| `0x00503CB4` → **`0x00505170`** | Native `CALL` and callee | Queries the complete resource **byte length**. The game uses that same length for its temporary allocation and passes it through the WAV read sequence. Original native call returns `1` for a successful length lookup in the guarded wrapper's observed convention. |
| `0x00503CBE` → `0x0052A227` | Native `CALL` | Allocates the temporary WAV-file staging buffer sized from the length query. Do not mistake this for a 2,048-byte allocation: it holds the **original file length**. |
| **`0x00503CC9–0x00503CD3`** | **Original instructions — defect** | Calculates **`floor(WAV_file_bytes / 2048)`**, not a ceiling. `0x00503CD3` contains `sar eax,0xb` at the end of the signed division sequence. |
| **`0x00503CD8` → `0x005051D0`** | **Native `CALL` and callee** | Passes that already-truncated full-sector count to the original PKR resource reader, with the destination buffer and resource handle. **The call omits the final partial sector.** |
| **`0x005051D0`** | **Native function** | Original PKR reader: converts its block count to bytes with `shl edx,0xb`, clamps at actual resource end, copies with `rep movsd` and `rep movsb`, and advances the resource cursor. **It can handle the short last sector when the caller requests one**; the defect is in the preceding WAV caller, not in this reader. Original wrapper observes `0` on successful sector reads. |
| `0x00503EBD` | COM callsite | `IDirectSound::CreateSoundBuffer` (vtable `+0x0C`) creates the game-owned reference/template sound buffer *after* the incomplete WAV staging read. |
| `0x00503EFF` / `0x00503F53` | COM callsites | `IDirectSoundBuffer::Lock` (vtable `+0x2C`), including the retry path. |
| `0x00503F83–0x00503FB2` | Original instruction range | `rep movsd; rep movsb` copy data from the staging buffer into the locked DirectSound regions. These instructions are downstream of the missing PKR bytes. |
| `0x00503FCD` | COM callsite | `IDirectSoundBuffer::Unlock` (vtable `+0x4C`). |
| `0x004FBDC0` | Native helper | Sound-bank selection/lookup helper; direct references examined around `0x00503B5D` and `0x005040AE`. Do not claim it is the PKR sector reader. |
| `0x005039F0` | Native function | Initializes the game's **32 secondary voice records**, each 12 bytes long. **32 active voices and 64 WAV templates are different collections.** |
| `0x004727F9` → **`0x00504110`** | Native `CALL` and callee | Native `DXSOUND_Open`/voice duplication into a game voice slot; the direct call census found this caller and the callee invokes `IDirectSound::DuplicateSoundBuffer` at `0x00504152` (vtable `+0x14`). |
| **`0x00504230`** / **`0x005042A0`** | Native functions | Original `DXSOUND_Play` and `DXSOUND_Stop` operations on game-owned voices. |
| `0x00472870` → `0x00504230` | Native `CALL` | Exact existing Play hook site (`E8 BB 19 09 00`). The current patch passes the call through to native Play, only offering separately gated read-only diagnostics and the optional two-sample `Heli1.wav` correction. |
| `0x005041C0` | Native function | Original secondary-voice buffer release, identified by the separate native voice lifecycle research ([historical PR #66](https://github.com/legluondunet/SpiderMan2k1Fix/pull/66)); not a WAV resource reader. |
| `0x00504420` | Native function | Native voice busy/status check, probed independently for the `DSBSTATUS_LOOPING` versus `DSBSTATUS_PLAYING` hypothesis ([historical PR #65](https://github.com/legluondunet/SpiderMan2k1Fix/pull/65)). **That separate hypothesis is not the demonstrated cause of the missing WAV sectors**. |

**Game data, not functions:**

- `[0x006B7920]`: game-owned DirectSound device/object pointer.
- `0x006BBAD4`: reference/template DirectSound buffer table location.
- `0x006BBD50`: 32 game-owned secondary-voice records, **12 bytes/record**.
- `[0x006BBF1C]`: primary DirectSound buffer, distinct from the 32 secondary voices.
- `0x0057AD30`: scratch storage observed in the **generic** resource transfer path.

**Cross-reference limitations:** raw occurrences of an absolute pointer or COM vtable offset alone do **not** establish an audio method. The investigation explicitly ruled out DirectInput/graphics COM false positives. “Only direct caller” refers to the scanned `E8 rel32` callsites, not a proof that indirect callers cannot exist.

## 2. Exact cause of the corrupted audio

The original WAV/template loader's order is:

1. Open the original WAV resource via `0x00504CB0`.
2. Query **`L` = full RIFF/WAV file length in bytes** at `0x00505170`.
3. Allocate the staging file buffer for **`L` bytes** at `0x0052A227`.
4. At `0x00503CC9–0x00503CD3` compute the number of full 2,048-byte sectors.
5. Call `0x005051D0` at `0x00503CD8` with **only that truncated block count**.
6. Continue parsing/populating a DirectSound buffer from the incompletely loaded WAV staging data; then duplicate/play voices.

**Instruction evidence** from the SHA-matched original EXE's Windows `llvm-objdump` report:

```asm
; caller: 0x00503CC9..0x00503CD8
cdq
and edx, 0x7ff
add eax, edx
sar eax, 0xb       ; positive byte count / 2048, truncated down
push eax           ; whole-block count, omits the last partial block
push esi
call 0x005051D0

; reader: 0x005051D4..0x0050522B
mov edx, [esp+8]  ; block count argument
shl edx, 0xb      ; blocks * 2048 bytes
; clamp to actual resource end, copy dwords then remaining bytes
rep movsd
rep movsb
; update resource cursor
```

For **positive** original file lengths:

- `full_blocks = L / 2048` using integer division;
- `read_bytes = full_blocks * 2048`;
- `missing_bytes = L % 2048`, from **0 to 2,047 bytes**.

Whenever the remainder is nonzero, the original WAV loading call simply **never requests the final partial block**. The next DirectSound stages consume a staging buffer whose full capacity exists but whose terminal original data has not been copied by the PKR read. This explains the noisy/crackling/stuttering sound effects observed from this defect. **DirectSound does not cause this particular loss**, and the game's resource archive is not thereby proven damaged.

Do **not** fix the `sar eax,0xb` by blindly rounding up the original block count: the reader scales it back to **2,048 bytes per block**, while the game's allocated destination may have space for only the exact `L` bytes. Rounding up risks a **buffer overrun**.

### Byte-level corroboration

| Original WAV | Original RIFF bytes | Last partial sector | Historical DirectSound PCM difference |
| --- | ---: | ---: | --- |
| `Heli1.wav` | 60,524 | 1,132 | 1,132 bytes |
| `ThugHit2.wav` | 15,182 | 846 | 848 bytes, including 2 native PCM alignment bytes |
| `traffic2.wav` | 79,350 | 1,526 | 1,528 bytes, including 2 native PCM alignment bytes |

The original WAV-file remainder is **not always equal** to the number of different bytes seen later in a DirectSound PCM buffer: its native allocation/copying can introduce **two alignment bytes**. The core fix restores the **original WAV-file bytes**, not invented padding.

A separate **source-asset issue** also exists: the original `Heli1.wav` contains two anomalous PCM samples at positions **12095** and **12109**. The optional `ExperimentalAudioHeli1Declick=1` affects only those original samples in a stopped secondary buffer and is **not** a PKR-sector fallback or evidence of another loader defect.

## 3. A different resource reader was examined — and cleared

Raw cross-references to `0x00505170`/`0x005051D0` initially suggested another defective WAV route. Follow-up native disassembly showed the path around `0x00430940` (resource open) and `0x00430A40` (generic resource transfer) **already processes a partial final block**.

| Native site | What the original instructions do |
| --- | --- |
| `0x00430A21` | Length query through `0x00505170` |
| `0x00430AF2` | Compare remaining requested bytes against `0x800` |
| `0x00430B06` | Check 32-byte output alignment |
| `0x00430B0B–0x00430B23` | Transfer **complete blocks** using the native sector reader |
| `0x00430B28–0x00430B39` | Advance output pointer; subtract full-block bytes |
| `0x00430B43–0x00430B87` | For another full block, read to private 2,048-byte scratch and copy all 2,048 bytes |
| `0x00430B89–0x00430BA8` | When 1–2,048 bytes remain, request another block into scratch |
| `0x00430BCC–0x00430BDB` | Copy **exactly** the residual requested byte count |

This path was **not established as a second WAV template-buffer constructor**, and the specific truncation error of `0x00503B40` **does not occur here**. The three `0x00430Bxx` references to the native sector reader must **not** be patched for this bug.

## 4. What the shipped source correction does

The built-in root-fix lives in:

- [`pkr_audio_loader.c`](../../../../pkr_audio_loader.c) / [`pkr_audio_loader.h`](../../../../pkr_audio_loader.h): guards, thread-local length/handle pair, two exact callsite wrappers, original sector read, private scratch sector and exact-size copy.
- [`pkr_audio_index.c`](../../../../pkr_audio_index.c) / [`pkr_audio_index.h`](../../../../pkr_audio_index.h): independent original `data.pkr` index, WAV decoding, RIFF boundaries, CRC32 and full-prefix matching, consensus across duplicate WAV aliases.
- [`pkr_audio_play_hook.c`](../../../../pkr_audio_play_hook.c): unchanged native Play plus optional read-only diagnostics and independently opt-in `Heli1.wav` declick; **no** late general DirectSound tail rewrite.
- [`spiderman2k1fix.c`](../../../../spiderman2k1fix.c): source index before loader hook, activation default and optional compatibility override.

**Safety checks and exact sequence:**

1. Support only the original **SHA-256-verified retail executable**, game image base `0x00400000` and original `CALL` bytes `E8 B7 14 00 00` at `0x00503CB4` and `E8 F3 14 00 00` at `0x00503CD8`.
2. Read original game-directory `data.pkr` **without modifying it**. Check PKR3 boundaries, resource lengths and compression flags (`-2` raw, `+2` zlib); decode valid WAV resources and validate their original **per-file CRC32**. The measured supported archive had **656/656 WAVs** verified.
3. Call the **original** length query and preserve the byte count and handle **per thread**. Use the game's actual allocation length, never an untrusted guessed capacity from a RIFF header.
4. Call the **original** sector reader normally first. Only if `blocks == floor(L/2048)`, `L` is within bounded limits, the handle agrees, and the remainder lies in **1..2047**, consider further work.
5. Require a valid RIFF/WAVE signature, exact declared RIFF length, **entire already-read prefix identical** to at least one independently CRC32-verified original, and a missing portion overlapping **actual PCM** rather than metadata alone.
6. When several original WAV resource names match the full prefix, require **all matching CRC-verified candidates have byte-identical complete remaining tails**. Otherwise refuse. Never select an arbitrary alias.
7. Read **one more sector** through original `0x005051D0` into a separate **2,048-byte scratch array**, then compare the **exact missing number of bytes** against the selected independently verified original remainder.
8. Only after all checks succeed, copy exactly `L % 2048` bytes to the end of the already allocated staging buffer. Keep native DirectSound `CreateSoundBuffer` / `Lock` / `Unlock` / `DuplicateSoundBuffer` / `Play` untouched.

All declined/uncertain cases leave the original read result and sound engine intact (**fail closed for correction**). Invalid signature, wrong EXE, unavailable resource index, absent CRC proof or inconsistent aliases must never permit a speculative write. The former `ExperimentalAudioPKRTailRestore` Play-time buffer mutation was **removed**, not silently retained.

**Current activation:** enabled automatically for the supported game; there is **no setting in the factory INI**. Advanced troubleshooting can explicitly disable it with `[Debug] ExperimentalAudioLoaderTail=0`. `ExperimentalAudioHeli1Declick` remains separately opt-in (default disabled).

## 5. Proven runtime result and boundaries

The user-provided **27 October 2026 21:25 Linux/Wine** run (`v1.0.0-62a9fc4`, with the obsolete Play-time tail fallback already removed) established:

- **656 / 656** original WAV sources indexed and CRC-verified;
- **95** logged `LOADER-TAIL-RESTORED` events, **91,406** original bytes restored;
- **three formerly unresolved alias restorations**, two **460-byte** tails and one **1,988-byte** tail; these **2,908 bytes are included in the 95**, not extra restorations;
- **49** `FULL-PCM IDENTICAL` checks, **zero** `FULL-PCM DIFF`;
- **no** pre-Play/late DirectSound tail-restoration events and **no** PCM-bearing native loader skips in that trace;
- one expected `BreakFree.wav` **20-byte non-PCM metadata-only tail** left alone;
- successful native 3D teardown/return to menu; the user confirmed **no audible parasites** in the fallback-free build.

**Scope:** the Linux/Wine listening and runtime evidence establishes resolution for the tested supported configuration. It does not certify every game sequence or source, nor native Windows behavior for the **final default-on version**. Windows x86 build and regression tests passed; native Windows gameplay verification is outstanding. Optional `Heli1.wav` declick was enabled but not exercised in the final fallback-free log.

## 6. Reproduce the static audit without distributing game files

Use the original **user-owned** `SpideyPC.exe` and the repository's **read-only** tools:

- [`ChatGPT/tools/Inspect-SpiderManAudioLoader.ps1`](../../../../ChatGPT/tools/Inspect-SpiderManAudioLoader.ps1) on Windows with an appropriate `llvm-objdump.exe`;
- [`ChatGPT/tools/inspect_audio_load_x86.py`](../../../../ChatGPT/tools/inspect_audio_load_x86.py) on Linux (also expects suitable x86 disassembly tools);
- [Full historical disassembly evidence and its boundaries](NATIVE_AUDIO_LOADER_ROOT_CAUSE.md);
- [General original-game function catalog](../../ORIGINAL_EXE_FUNCTION_CATALOG.md) and [development map](../../../DEVELOPMENT.md).

Representative read-only windows are `0x005039D0–0x00504340` (native DirectSound/WAV loader), `0x00504C80–0x00505300` (PKR open/length/read helpers), `0x00430800–0x00430DC0` (generic partial-sector-aware reader), `0x00470F00–0x00472A50` (game audio manager) and `0x004FBD60–0x004FC050` (sound-bank selection).

The chronological report cites the Windows `llvm-objdump` outputs `spiderman-audio-root-disassembly.txt`, `spiderman-audio-root-disassembly(1).txt`, `spiderman-audio-second-path.txt`, `spiderman-audio-loader-xrefs.txt` and `spiderman-audio-buffer-population.txt`. The repository preserves **the analysis and reproducible scripts**, not necessarily all user-produced raw text outputs or any copy of the retail game. The original tools and game are needed to regenerate raw assembly.

**Historical audio experimental PRs:** [#61](https://github.com/legluondunet/SpiderMan2k1Fix/pull/61), [#62](https://github.com/legluondunet/SpiderMan2k1Fix/pull/62), [#64](https://github.com/legluondunet/SpiderMan2k1Fix/pull/64), [#65](https://github.com/legluondunet/SpiderMan2k1Fix/pull/65), [#66](https://github.com/legluondunet/SpiderMan2k1Fix/pull/66) are closed and unmerged. Their **independent hypotheses** should not be presented as validated causes of this WAV truncation. The actual root correction is [#67](https://github.com/legluondunet/SpiderMan2k1Fix/pull/67), with default activation in [#70](https://github.com/legluondunet/SpiderMan2k1Fix/pull/70).

---

*Source-derived recovery summary by legluondunet assisted by ChatGPT (OpenAI). Native function roles and confidence statements are anchored in the historical Windows disassembly logs, not newly invented names or claims of complete original-source reconstruction.*
