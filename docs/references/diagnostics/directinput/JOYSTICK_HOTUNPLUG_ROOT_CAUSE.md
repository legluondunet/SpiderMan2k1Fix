# Native joystick unplug termination — SHA-verified root cause (2026-10-28)

**Status:** offline x86 disassembly completed against the user-supplied
**unchanged retail `SpideyPC.exe`**, size **1,507,328 bytes**, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
No game/EXE memory patch was applied by this investigation. All VAs below
are for the original image base **`0x00400000`**.

## Critical correction to earlier evidence

Wine's user-supplied exit relay logged

```text
0160:Call KERNEL32.ExitProcess(80040209) ret=0052a1c6
```

**`0x80040209` is `DIERR_UNPLUGGED` in DirectInput's `dinput.h`.**
The value is also reused as DirectShow's `VFW_E_NOT_CONNECTED`; this
collision previously made attribution ambiguous. The device-detach timing,
original DirectInput joystick poll/reacquire callsites and *the original
EXE's embedded `DIERR_UNPLUGGED` error string* establish DirectInput as
the relevant interpretation. `0x80040209` does **not** occur as a
literal DWORD in the original binary: it is dynamically returned by an API.

Source definitions:
- https://chromium.googlesource.com/chromium/deps/perl/+/master/c/i686-w64-mingw32/include/dinput.h
- https://sources.debian.org/src/wine/1.0.1-1/include/dinput.h
- https://www.winehq.org/pipermail/wine-devel/2022-January/204481.html

## The precise native joystick failure path

The original joystick interface pointer is stored at **`0x006B7A2C`**.
The native polling function begins at **`0x00501E50`**.

| VA | Exact original bytes | Explanation |
| --- | --- | --- |
| `0x00501E79` | `FF 51 64` | COM `IDirectInputDevice8::Poll` (vtable +0x64) |
| `0x00501E7C` | `3D 1E 00 07 80` | Compare with `DIERR_INPUTLOST=0x8007001E` |
| `0x00501E83` | `3D 0C 00 07 80` | Compare with `DIERR_NOTACQUIRED=0x8007000C` |
| `0x00501E9F` | `8B 10 FF 52 1C` | COM `IDirectInputDevice8::Acquire` (vtable +0x1c) |
| `0x00501EA6` | `81 FE 05 00 07 80` | `E_ACCESSDENIED`, alias `DIERR_OTHERAPPHASPRIO`, is handled nonfatally |
| `0x00501EC6` | `85 F6 74 22` | Test `Acquire`'s HRESULT in ESI |
| `0x00501EDD` | `85 F6 7D 0B` | Only **negative** error results take fatal branch |
| `0x00501EE6` | `56` | Push the exact returned HRESULT for exit |
| `0x00501EE7` | `E8 15 82 02 00` | `CALL 0x0052A101` — native CRT exit wrapper |
| `0x0052A101` | `6A 00 6A 00` | CRT `exit` wrapper accepts status arg |
| `0x0052A1C0` | `FF 15 CC B0 53 00` | IAT `KERNEL32.ExitProcess` |
| `0x0052A1C6` | `5F C3` | **Return address observed in Wine relay** |

The negative `Acquire` HRESULT is also sent to the game's DirectInput
error-message formatter at **`0x004FC240`**, with embedded historical
source path **`C:\backup\SpideyPC\SpideyPC\D3d\DXInput.cpp`**
and source line **`0x2EA = 746`**. The original source files themselves
have not been recovered.

Interpretation: on a detached pad, Wine returns `DIERR_INPUTLOST`
from polling, then **`DIERR_UNPLUGGED=0x80040209` from the native
`Acquire`**. Original Spider-Man incorrectly treats this as a fatal
error, calling the standard CRT exit path and finally
`ExitProcess(0x80040209)` (shell exit status `9`).

The combined Wine `+dinput` trace and binary control flow make this
a **very strong root-cause reconstruction**, but no CPU-level stack
trace has yet independently recorded that `0x00501EE7` was executed
during the physical unplug. The other `ExitProcess` error branches
in the executable remain theoretically possible. Do not overstate
that last increment of certainty.

## Recovery design (not implemented in this analysis)

The proper fix is **not** intercepting all `ExitProcess` calls or
returning forged success to `IDirectInputDevice8::Acquire`.

1. Under strict original-binary signature and user-config opt-in,
   treat only `DIERR_UNPLUGGED` from the original joystick Acquire
   as a recoverable *device missing* event, without entering CRT exit.
   The game's existing `E_ACCESSDENIED` case is a **proven example
   of graceful return with `EAX=0`** (`0x00501EBB`).
2. Ensure all joystick actions, buffered button state and synthetic
   keys are released so a disconnected device cannot leave movement
   or menu confirmations stuck. Preserve keyboard/mouse and Alt+Tab.
3. Re-enumerate and bind a **new game-owned DirectInput joystick**
   after the physical device has returned or been replaced. PR #76's
   secondary DS4 device discovery alone cannot make the original
   engine's pointer `[0x006B7A2C]` refer to a newly plugged controller.
4. Build regression checks for the exact x86 bytes and branch jump
   bounds, keep the guard disabled by default until genuine Wine and
   Windows hardware tests. All changes through a draft PR.

Do not simply jump to the original successful-data path after
`DIERR_UNPLUGGED`: `GetDeviceData` and gameplay state should remain
invalid until a genuinely acquired new native device is available.
Preserve the game's other fatal error handling.

**Conclusion:** Original game-native joystick recovery, not
the patch's secondary DS4 DirectInput device, is the first blocking
dependency for hotplug. PR #76 remains experimental and unmerged.

legluondunet assisted by ChatGPT (OpenAI)


## Experimental stage 1 implemented (opt-in, PR #76 only)

`native_joystick_unplug.c` adds a **single** original-game x86 guard at
`0x00501EA6`. The original bytes at that site are
`81 FE 05 00 07 80`, the `cmp esi,DIERR_OTHERAPPHASPRIO`
immediately following native `IDirectInputDevice8::Acquire`.
The guard allocates its own RX stub, replaces **only those 6 bytes**
with a relative JMP and NOP, and restores the original compare inside
the stub for every other HRESULT.

The stub handles **only `DIERR_UNPLUGGED=0x80040209`**, clears exactly
the original buffered joystick-button bytes
`0x006B7A34..0x006B7A53` (32 bytes) to avoid held face buttons,
increments a counter, then jumps to the game's own nonfatal
`0x00501EBB` return, which sets `EAX=0` and restores the
original stack. No successful acquisition is forged; an unplugged
game-owned DirectInput interface remains detached. All other
HRESULTs follow the unchanged original `cmp esi,0x80070005`
and its native branch/formatter/exit handling. The patch checks the
original PE image base, 32-bit architecture and **10 exact byte
signatures** and is only requested after the existing full retail
SHA-256 verification. It installs on the game render thread at
Flip #120 (not under loader lock), using a permanent executable
allocation. Failure to meet any gate preserves the original behavior.

Add to the installed `SpiderMan2k1Fix.ini`:

```ini
[Input]
ControllerProfile=Off
ExperimentalNativeJoystickUnplugGuard=1

[Debug]
Log=1
```

`ControllerProfile=Off` isolates the native bug and avoids the
independent DS4 UI bridge on this first test; `Off` does NOT disable
the original game's own joystick. The new guard option **defaults to
0** and is repaired/validated with the other INI boolean settings.
On startup, confirm the log shows
`NATIVE-UNPLUG: experimental guard requested` and, after 120
frames, `NATIVE-UNPLUG: ACTIVE at Flip #120`. If the second
entry is missing, the guard is not installed and detaching the pad
can still intentionally exit the retail game. The next Flip after
a suppressed error should log
`NATIVE-UNPLUG: suppressed fatal native DIERR_UNPLUGGED`.
A *successful* first test means the game remains open and keyboard
controls still work when disconnecting the DS4 from the main menu.
Do **not** test repeated unplugs or live device replacement until
this is verified.

**Limitations and risks:** (1) native DirectInput device
re-enumeration/rebinding has **not** been implemented, so a newly
plugged controller will likely not recover movement; (2) the original
32-button buffer was verified statically but other analog/action caches
may need separate clearing; (3) intercepting a native code path is
experimental, even when SHA/signatures match. Physical Wine and
Windows tests remain pending; CI compilation and unit tests do not
validate runtime stability. Disable the option again if there is
any instability. Never merge PR #76 before explicit maintainer approval.

The native executable remains unchanged on disk. No global
`ExitProcess` patch, COM vtable replacement or fabricated HRESULT
is used.

legluondunet assisted by ChatGPT (OpenAI)
