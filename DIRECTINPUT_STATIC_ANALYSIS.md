# Native input path: static audit of the supported Spider-Man PC executable

Analysis date: 2026-10-21. This report analyzes the user's unchanged retail
`SpideyPC.exe` **offline**; it does not add the copyrighted binary to GitHub,
run the game, or imply the menu bug is solved.

## Binary identity

- PE32, Intel x86; fixed image base: `0x00400000`
- PE timestamp: 2001-08-15 08:23:03 UTC
- Size: 1,507,328 bytes
- SHA-256: `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`
- Imported `DINPUT8.dll!DirectInput8Create` via IAT VA `0x0053B01C`.
  Stub at `0x0051672C`, called at `0x004FE0E4`.
- Native interface `CreateDevice` vtable slot `+0x0C` is used at
  `0x004FE2CC`. **Do not** replace the COM vtable for diagnosis:
  the previous experimental wrapper introduced `E_INVALIDARG` on Windows 11.

## Specific keyboard and mouse objects

The game's own second DirectInput initialization path calls an interface's
`CreateDevice` (zero-based vtable slot 3, `+0x0C`) with these GUIDs:

| Device | GUID | `CreateDevice` call | Game-owned pointer location |
| --- | --- | --- | --- |
| Keyboard | `6f1d2b61-d5a0-11cf-bfc7-444553540000` | `0x005015A8` | `0x006B7A5C` |
| Mouse | `6f1d2b60-d5a0-11cf-bfc7-444553540000` | `0x00501728` | `0x006B7A64` |

The GUIDs were decoded from the actual embedded data at VAs
`0x0053C9E8`/`0x0053C9F8` as little-endian Windows GUIDs. Both devices
have `Acquire` at vtable slot 7 / byte offset `+0x1C`.

The keyboard and mouse each configure their cooperative level/format and call
their native `Acquire` during initialization (near `0x005016C3` and
`0x00501838`).

## The crucial lost-input branch already in the game

**Keyboard**, `0x00501BC3`: game calls
`IDirectInputDevice8::GetDeviceData` via vtable `+0x28`.
At `0x00501BC6` it compares the HRESULT with `0x8007001E`
(`DIERR_INPUTLOST`). Only if equal does it call `Acquire` at
`0x00501BD9` and retry `GetDeviceData` at `0x00501C3B`.

After that retry, if HRESULT is `0x8007000C` (`DIERR_NOTACQUIRED`),
the method returns early (`0x00501C3E` – `0x00501C50`).
A **first** read returning `DIERR_NOTACQUIRED` follows the direct
`jne 0x00501C51` path, **skipping the Acquire attempt**.

**Mouse** has an analogous path:
`GetDeviceData` `0x00501D02`, compare to `DIERR_INPUTLOST`
`0x00501D05`, native `Acquire` `0x00501D18`,
retry `0x00501D7A`, then test for `DIERR_NOTACQUIRED`
`0x00501D7D`.

This is evidence for a **possible** missed recovery path after Alt+Tab, not
proof that the actual failing HRESULT is `DIERR_NOTACQUIRED`. The menu
may also stop calling its polling path or ignore correctly returned events.
We must not report one hypothetical error code as observed.

## Narrow, testable hypothesis and proposal

The next potential experiment should be confined to the supported binary,
to the keyboard/mouse, and to a **real** lost/returned focus event. It should
preserve the native COM objects and native error values. A one-shot opt-in
`Acquire` on the two known device objects (only if still valid, while the
game window is foreground, and ideally after menu focus return) is safer to
investigate than replacing factory/device COM vtables. This is not yet
implemented or validated; it must be disabled by default, log HRESULTs, and
include a clean rollback path before user testing.

Prefer a safer observation of actual HRESULTs before any recovery if possible.
No in-debugger interactive breaks while a borderless foreground window owns
the screen: the earlier x32dbg session forced Windows logout.

Other major stages of the game may use different input logic. Static binary
analysis alone cannot establish whether a specific path executes in the menu
after Alt+Tab.

Signature: legluondunet assisted by ChatGPT (OpenAI)
