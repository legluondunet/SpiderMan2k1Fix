# Native DirectSound primary-buffer NULL cleanup crash — issue #160

## Scope and independently confirmed evidence (14 November 2026)

Two native Windows 11 WER dumps of the same supported retail game executable
demonstrate an identical crash, **both before and after** the independent
`spiderman2001/` directory migration in PR #159.

| Field | 16:52, old location | 21:05, new location |
|---|---|---|
| `SpideyPC.exe` exception | `0xC0000005` | `0xC0000005` |
| faulting VA/EIP | `0x00503AF7` | `0x00503AF7` |
| read address / EAX | `0x00000000` | `0x00000000` |
| original `[0x006BBF1C]` primary buffer | 0 | 0 |
| original `[0x006B7920]` DirectSound object | 0 | 0 |
| EDI | `0x80004005` (`E_FAIL`) | same |
| immediate original caller | `0x004FF9CA` | same |

Dumps contain private process memory and are **not committed** to this
repository. The fault site and original 14 NOP bytes were verified against
each dump and an earlier SHA-verified original-game disassembly report.

The original function at `0x00503AF0` unconditionally invokes
`IDirectSoundBuffer::Stop` on the game-owned primary buffer:

```asm
00503AF0  mov eax,[006BBF1C]  ; NULL in both dumps
00503AF5  push esi
00503AF6  push eax
00503AF7  mov ecx,[eax]       ; read NULL -> exception
00503AF9  call [ecx+48h]     ; native Stop, only valid for non-NULL
00503AFC  mov esi,eax        ; native HRESULT handling
...
00503B24  push 1
00503B26  push 0
00503B28  call 00504050     ; native cleanup, guards each released pointer
00503B2D  add esp,8
00503B30  pop esi
00503B31  ret
00503B32-00503B3F  14 NOP bytes
```

The original `0x00504050` final-cleanup disassembly checks each referenced
sound buffer for NULL before `Release`. This is the existing native
cleanup path used by the repair; no alternative shutdown is invented.

## Protection (experimental until accepted on original hardware)

The early `winmm.dll` proxy runs under the original game's DLL loader lock,
before the game can enter its early sound failure/teardown code. After the
existing original PE image + No-CD opcode checks, the code verifies *every*
original instruction affected and all 14 NOP bytes, plus the final cleanup
callee signature. If any check fails, it makes **no DirectSound change**.

Only the 5 bytes at `0x00503AF7` change in **process memory** to
`E9 36 00 00 00` (jump to `0x00503B32`). The in-memory trampoline bytes are:

```asm
00503B32  85 C0         test eax,eax
00503B34  74 07         jz 00503B3D
00503B36  8B 08         mov ecx,[eax]       ; original, when non-NULL
00503B38  FF 51 48      call [ecx+48h]      ; original Stop
00503B3B  EB BF         jmp 00503AFC        ; original HRESULT path
00503B3D  58            pop eax              ; discard pushed NULL; preserve ESI
00503B3E  EB E4         jmp 00503B24        ; original cleanup
```

The code cave is published and flushed before redirecting the fault site.
Only Win32 memory-protection and cache-flush APIs are used at DLL attach.
The original `SpideyPC.exe` file remains byte-identical on disk.

The NULL path retains the original caller return address and saved ESI.
The non-NULL path preserves the full original `Stop` call, HRESULT, and
all original later logic. This is deliberately **not** an attempt to make
DirectSound initialization succeed, hide `E_FAIL`, or retry a failing launch.

The original patch's game-image SHA verification still occurs in the core
DLL at initialization. The loader-lock guard itself uses several exact
native opcode/PE signatures, as does the existing early No-CD and
DirectDraw startup gate; it does **not** read or hash an EXE from disk
inside the loader lock.

## Validation boundaries

- Source and exact opcode/branch/stack tests:
  `ChatGPT/tests/test_dsound_primary_null_cleanup.py`.
- Windows x86 compile and full repo regression suite run in GitHub CI.
- Windows executable synthetic disc/installer tests also run in CI.
- **Not yet validated on an actual crashing retail game.** A signature
  mismatch is logged as an inactive guard; a successful install logs
  `DSOUND-PRIMARY: early native NULL-cleanup guard status=1` when
  `[Debug] Log=1`.
- If startup still fails, inspect the preserved runtime log and WER event
  to distinguish a graceful original error path from another exception.
  Do not auto-restart or claim the missing original primary buffer exists.
- The upstream reason DirectSound initialization was incomplete and
  `E_FAIL` was present remains a separate investigation.

### Upstream startup failure hypothesis (not yet confirmed)

The public source reconstruction in `krystalgamer/spidey-decomp`,
`DXinit.cpp`: `DXINIT_DirectX8()` calls `initDirectDraw7()`
and `initDirect3D7()` **before** `initDirectSound8()`. Its
`DXINIT_ShutDown()` always calls `shutdownDirectSound8()`;
`DXsound.cpp` marks `DXSOUND_ShutDown()` matching the original and
unconditionally invokes `g_pDSBuffer->Stop()`.

Thus the zero native sound-device/primary-buffer pointers in **both**
crashes may be caused by an **earlier graphics error**, followed by
generic shutdown, rather than by a DirectSound-specific API failure.
`EDI=0x80004005` in both dumps is consistent with `E_FAIL`, but
does **not** identify which API returned it. Do not report this as a
proven DirectSound initialization failure.

The WinMM proxy intentionally returns `E_FAIL` if its early
`DirectDrawCreateEx` gate cannot load/resolve the core fix DLL;
the core gate returns `DDERR_UNSUPPORTED` if core initialization
fails. These are **candidate** upstream failure paths, not confirmed
participants in either crash.

The branch now logs the exact DirectDrawCreateEx result on the
existing unsuccessful/non-DDraw7 path; it does not alter HRESULT
or graphics/audio APIs. The existing **SHA-256 + size gated** original
game forensic inspectors, Python and PowerShell, have both been
extended to disassemble the newly proven caller `0x004FF9CA`
and surrounding error handling, plus the possible deeper upstream
stack address `0x004FDED9` (not a confirmed unwound frame).
Only another original-EXE static analysis can classify the actual
pre-shutdown branch conclusively.

## Linux: focused original-EXE analysis (no Wine / no game launch)

The normal Windows/Wine runtime guard is unchanged. To determine
what called native audio cleanup while no primary buffer existed,
run the original-game inspector with the focused Linux option:

~~~bash
# Locate the original SpideyPC.exe under your installed games.
find /mnt/LUTRIS/Games -type f -iname 'SpideyPC.exe' -print 2>/dev/null

# From a checkout of PR #161 (branch fix/dsound-null-primary-160):
python3 ChatGPT/tools/inspect_audio_load_x86.py \
  "/absolute/path/to/SpideyPC.exe" \
  --startup-crash-only \
  --output "$HOME/spiderman-dsound-startup.txt"
~~~

Requirements: Python 3 (standard library only) and objdump or
llvm-objdump (check with: command -v objdump llvm-objdump).
The inspector rejects any file whose original size, SHA-256, image
base, signature-checked native caller, Stop call or cleanup bytes
differ from the supported retail EXE.

The focused report contains only five bounded disassembly windows around
native caller 0x004FF9CA, the possible upstream return address near
0x004FDED9, original shutdown and graphics initialization. It contains
no game assets and does not write anything to the game folder unless
the caller explicitly chooses an output path there. It refuses an output
file that aliases the original EXE, including symlinks and hard links.
The original-game executable and its memory are never changed.

Do not treat EDI=E_FAIL in the dumps as proof of a failed DirectSound
API. We still need to identify the prior graphics or audio error
from the actual x86 caller branches. The guard prevents the secondary
NULL dereference; it does not repair the initial error by itself.

## Integration

The draft fix in PR #161 is stacked atop the unmerged directory migration
PR #159 to make its standalone Windows artifact directly usable against the
maintainer's existing `spiderman2001/` directory. Keep the release unchanged
until native Windows and Wine runtime validation is accepted.
