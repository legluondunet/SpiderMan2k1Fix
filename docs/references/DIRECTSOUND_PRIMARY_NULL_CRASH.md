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
- The original graphics initialization returned `E_FAIL` from
  `IDirectDraw7::SetCooperativeLevel` before native DirectSound was
  initialized. The reason the Windows backend rejected `DDSCL_NORMAL`
  remains unknown; a NULL DirectSound buffer is a consequence, not the
  upstream cause.

### DirectDraw root error: confirmed by uploaded Linux original executable

On 15 November 2026 the maintainer supplied a read-only
`--startup-crash-only` disassembly and the original supported
`SpideyPC.exe`, matching the **exact pinned SHA-256**. The previously
missing original caller `0x004FF9CA` is now proven to be in an
**error branch of DirectDraw7::SetCooperativeLevel**, *before* any
native DirectSound initialization. This corrects the earlier
uncertain "audio initialization failure" interpretation.

The exact original x86 bytes:

~~~asm
004FF983  6A 11          push 0x11          ; DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN
004FF985  56             push esi           ; original HWND
004FF986  50             push eax           ; original IDirectDraw7*
004FF987  FF 52 50       call [edx+0x50]    ; IDirectDraw7::SetCooperativeLevel
004FF98A  8B F8          mov edi,eax        ; HRESULT from that exact method
004FF98C  81 FF 45 02 76 88  cmp edi,0x88760245
004FF992  0F 84 B7 00 00 00  je 0x004FFA4F
004FF998  3B FB          cmp edi,ebx       ; EBX == 0 in both dumps
004FF99A  0F 84 AF 00 00 00  je 0x004FFA4F
004FF9A0  ...            native error reporting
004FF9B3  3B FB          cmp edi,ebx
004FF9B5  0F 8D 94 00 00 00  jge 0x004FFA4F
004FF9BB  E8 E0 2D 00 00  call 0x005027A0 ; graphics cleanup
004FF9C0  6A 01          push 1
004FF9C2  E8 89 DE FF FF  call 0x004FD850 ; further graphics cleanup
004FF9C7  83 C4 04       add esp,4
004FF9CA  E8 21 41 00 00  call 0x00503AF0 ; NULL-unsafe primary Stop
004FF9CF  ...            release original DirectSound object, if any
~~~

Both independent Windows WER dumps captured **`EDI=0x80004005`**
at EIP `0x00503AF7`, along with `EBX=0` and the original
primary sound buffer `[0x006BBF1C]=0`. The x86 caller holds the
SetCooperativeLevel result in the *callee-saved* EDI register through
native emergency cleanup, so the two context values identify the
**failed COM method** and not merely a coincidental HRESULT.
E_FAIL is negative and differs from the two accepted values; the
original error branch therefore reaches `0x004FF9CA` exactly as
the two dump return addresses record. The special accepted value
`0x88760245` is `DDERR_EXCLUSIVEMODEALREADYSET` (an existing exclusive
owner), not the error captured in these WER dumps.

The official Microsoft DirectDraw7 cooperative-level contract requires
`SetCooperativeLevel` to be called from the **window-creating thread**,
with a top-level HWND. It allows `DDSCL_NORMAL` without an HWND,
whereas original fullscreen/exclusive `0x11` requires the paired
flags. Accordingly, the diagnostic branch records the HWND's owning
thread and the calling thread on a first successful call and on every
failure; it does not force or retry exclusive mode. Reference:
https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdraw7-setcooperativelevel

**Established:** `IDirectDraw7::SetCooperativeLevel` returned
`0x80004005` in both crashes. The graphics initialization error path
invoked native audio shutdown before a primary DirectSound buffer existed,
then the unconditional native `Stop` dereferenced NULL. The game originally
requests exclusive/full-screen flags `0x11`; our `hooked_coop` redirects
these requests to `DDSCL_NORMAL`.

### Newly recovered from both raw Windows minidumps: the hook WAS entered

Earlier analysis could only confirm that `ddraw_vtable_ready=2`. A deeper
read of the **original DLL's captured, initialized data** and the Win32
`CONTEXT` resolves this uncertainty. A CI-built, unstripped PE32 DLL
contains COFF symbols `_ddraw_vtable_ready`,
`_patched_ddraw_vtable`, `_original_coop`, and `_game_hwnd`.
The original `spiderman2k1fix.c` has **exactly one assignment**
`game_hwnd=hwnd`, inside `hooked_coop` before calling
`original_coop(self,hwnd,DDSCL_NORMAL)`.

| Dump | ready=2 | saved hook `vtable[20]` | game ESI | patch `game_hwnd` |
|---|---|---|---|---|
| 16:52 (old folder) | `0x72C22AD8` | `0x72B65100` | `0x0018097C` | `0x0018097C` at `0x72C22E6C` |
| 21:05 (new folder) | `0x72C25AE0` | `0x72B65100` | `0x00100478` | `0x00100478` at `0x72C25E88` |

The matching, non-NULL game-window handle in the **single-writer**
core `game_hwnd` establishes that `hooked_coop` was actually entered
during *both* failing processes. Both core DLLs have
`borderless_legacy_mode_calls=0` and
`surface_create_count=0`: failure occurred before any patched
display-mode transition or surface creation. Neither dump has the
caller code page, but the uploaded SHA-verified original retail
`SpideyPC.exe` now supplies the exact native instructions.

Even more importantly, **both captured core DLLs stored**
`original_coop=0x7AC428C0`; the minidump ModuleList resolves
that address to **`C:\\Windows\\System32\\apphelp.dll`**,
not `ddraw.dll`. The saved original query-interface,
CreateSurface and SetDisplayMode methods also resolve there.
This identifies a Windows application-compatibility indirection
in the native COM vtable. **It does not establish that the
compatibility component is defective** or why the method returned E_FAIL.

Therefore the previous "missing early patch" hypothesis is superseded.
Our `hooked_coop` was entered, and the game received `E_FAIL`
from its `SetCooperativeLevel` call. Because a retained `game_hwnd`
could theoretically originate from an earlier invocation, dump state
alone cannot *mathematically exclude* a second unhooked call.
The zero mode/surface counters and original initialization sequence
make that possibility unlikely. Do **not** claim a specific invalid
HWND, thread mismatch or apphelp bug without a direct diagnostic.

The previous candidate `DirectDrawCreateEx` startup-gate error
cannot be substituted for this proven method result: this original
code path had a valid DirectDraw7 interface to call
`SetCooperativeLevel`. The branch's existing error logging remains
useful for other failures but is **not the first failing API in
these two dumps**.

**Next safe step:** no speculative change to DDSCL flags, window
styles or EXE control flow. Preserve the existing SHA/signature-
gated primary NULL guard and, if another Windows repro occurs,
compare same-process log lines for `DDRAW-STARTUP`,
`BORDERLESS: intercept SetCooperativeLevel` and
`BORDERLESS: ERROR DDSCL_NORMAL rejected`.
The added diagnostics now snapshot the HWND **before** the original
`SetCooperativeLevel` call, recording whether it is a valid top-level
window, whether it has `WS_CHILD` or an owner, and the window-owner and
calling thread IDs. A success baseline records these same attributes
once and identifies the actual method's containing module. On failure,
the previous post-call context and failure backend are also logged.
This distinguishes an invalid pre-call window from any changes caused
by the Windows backend; it does **not** prove which condition caused
the error. The native `DDSCL_NORMAL` call, HRESULT, and original
cleanup policy remain unchanged.

Compare the following lines from a **successful** and a **failed**
Windows 11 launch with `[Debug] Log=1`:

- `BORDERLESS: COOP-SUCCESS-PRE` and `BORDERLESS: COOP-FAIL-PRE`
  (valid, top-level, child/owned, owner thread versus caller thread).
- `BORDERLESS: COOP-SUCCESS-BACKEND` and `BORDERLESS: COOP-FAIL-BACKEND`
  (`apphelp.dll` versus `ddraw.dll` or another backend).
- `BORDERLESS: COOP-FAIL-CONTEXT` and
  `BORDERLESS: ERROR DDSCL_NORMAL rejected`.

The new invariant tests are
`ChatGPT/tests/test_ddraw_cooperative_evidence.py`; they also
ensure that no retry, fake HRESULT or exclusive fullscreen path was
introduced. Do not claim a permanent upstream E_FAIL resolution or
a release until Windows acceptance.

## Diagnostic revision after 15 November successful Windows baseline

The maintainer's real native Windows 11 `v2.0.0-be94bc9` runtime log
(`SpiderMan2k1Fix(20261115-114230).log`) provides a good reference:

- `COOP-SUCCESS-PRE valid=1 top-level=1 child=0 owned=0`; window
  owner thread equals the calling thread.
- `COOP-SUCCESS-BACKEND module=DDRAW.dll`, `HRESULT=0x00000000`.
- In contrast, the *two older failed processes* stored an
  `original_coop` address within `apphelp.dll` and an `E_FAIL`.
  The backend correlation is real; a malfunction in `apphelp.dll`
  is **not established**. Windows compatibility shim infrastructure
  can redirect API calls, but simply loading `apphelp.dll` proves
  neither an active shim nor causality.

For the next test build, `hooked_create` records the original
`IDirectDraw7` vtable **before** replacing any method slot:

```text
DDRAW-STARTUP: COM-ORIGINS QueryInterface=... CreateSurface=... SetCooperativeLevel=... SetDisplayMode=... apphelp-loaded=0/1 compat-env-present=0/1
```

The first four created DirectDraw7 objects are sampled; source code uses
`VirtualQuery` and `GetModuleFileNameA` to report only module basenames,
not user paths or addresses. The environment indicator reports only whether
`__COMPAT_LAYER` is set, not its contents. All probes are read-only.
The late legacy IAT installer additionally recognizes an IAT slot already
pointing into the verified early WinMM proxy and explains that no second
hook is needed, replacing a misleading `IAT target mismatch` diagnostic;
the actual interception and error handling are unchanged.

On a *naturally occurring* startup failure, compare `COM-ORIGINS`,
`COOP-FAIL-PRE`, `COOP-FAIL-CONTEXT` and `COOP-FAIL-BACKEND` against
this successful baseline. Do **not** toggle compatibility settings,
bypass `apphelp.dll`, auto-retry or substitute a successful HRESULT
without establishing causality first. These diagnostics are test-only,
not a fix for the root DirectDraw failure.

Microsoft references:
- https://learn.microsoft.com/en-us/windows/win32/api/ddraw/nf-ddraw-idirectdraw7-setcooperativelevel
- https://learn.microsoft.com/en-us/windows/win32/devnotes/application-compatibility-database

### Read-only Windows compatibility attribution (new test build)

A separate observation from the COM method basenames is needed to avoid
mistaking the absence of `__COMPAT_LAYER` for proof that no Windows
compatibility fix is active. Microsoft documents both **per-executable**
layer entries and **system-wide .sdb matching**, which can apply
independently; `apphelp.dll` presence alone is insufficient to infer a
particular compatibility rule.

At the first intercepted native DirectDraw7 creation, after the game
executable identity was verified and outside loader lock, the patch now
queries **only the existence and type** of the full-path `SpideyPC.exe`
value under:

- `HKCU\\Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers`
- The analogous `HKLM` key in the **32-bit** view
- The analogous `HKLM` key in the **64-bit** view

The executable path is used for exact registry lookup but is **never
logged**. No registry contents, account paths, usernames, or compatibility
strings are logged or modified. The new diagnostics are:

```text
DDRAW-STARTUP: APPCOMPAT-LAYERS HKCU=... HKLM32=... HKLM64=... (0=absent 1=text 2=other -1=unreadable; system SDB not covered)
DDRAW-STARTUP: APPCOMPAT-MODULES apphelp=... AcLayers=... AcGenral=... AcXtrnal=... ShimEng=... (loaded presence only)
```

`0` means **that particular registry entry was absent**, not that
the compatibility engine was inactive. `-1` means a read error, an
unavailable registry view, or an unavailable executable path; it
is never reported as a proven absence. Module fields indicate
only *loaded presence* at that specific instant.

This evidence may distinguish Windows manual compatibility modes
from automatic database matching on future naturally failing launches.
It is safe under Wine because it only uses Win32 read operations.
**Do not disable the compatibility engine, change global machine
policies, force an exclusive display mode, or fake a successful HRESULT.**

References:
- https://learn.microsoft.com/en-us/windows/win32/devnotes/application-compatibility-database
- https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/d3d10-graphics-programming-guide-dxgi

## Offline minidump analysis (Linux or Windows, Python stdlib)

`python3 ChatGPT/tools/inspect_spiderman_ddraw_dump.py /path/to/SpideyPC.exe.24668.dmp`

The new read-only minidump analyzer parses only the standard Windows
ModuleList, MemoryList and x86 ExceptionStream. It verifies the native
fault signature, finds the core's **actual initialized DirectDraw
vtable**, locates the saved original COM method's containing DLL,
and checks the exact known `game_hwnd` layout against original ESI.
It emits only bounded addresses and DLL **basenames**, not private paths
or any process memory contents, and never modifies the input dump.
No proprietary binary/dump is included in the repository.
Synthetic-only tests create minimal fake MDMP streams to verify
successful recovery, fail-closed behavior and the report format.

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

PR #159 (verified game directory migration) has been merged into main.
PR #161 is now a draft directly targeting main and retains the current
`spiderman2001/` installation layout. Keep the release unchanged until
native Windows and Wine runtime validation is accepted.
