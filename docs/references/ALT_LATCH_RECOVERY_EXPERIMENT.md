# Experimental Left Alt latch reconciliation (PR #88)

**Status: research-only and strictly opt-in, NOT yet Windows/Wine
hardware-validated.** Requires the exact supported
`SpideyPC.exe` SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## Background: what the Windows 11 logs actually prove

The maintainer reproduced 3D Alt+Tab focus loss, with the original
DirectInput keyboard latch still registering Left Alt held while Win32
`GetAsyncKeyState(VK_LMENU)` reports it released. At least one later
**physical foreground Left Alt press/release** cleared the stale
original latch. The suspect mechanism is a missing buffered
`GetDeviceData` release event during background; this precise event
loss has **not** been intercepted and proved. The logs do not prove
the whole Alt+Tab keyboard/controller lockout is caused by Left Alt.

See `SDL3_NATIVE_SOURCE_TRACE.md` for event numbers and nuances.

## Opt-in, off by default

Add under the **existing** `[Input]` section:

```ini
[Input]
ExperimentalAltLatchRecovery=1
```

`[Debug] Log=1` is recommended to observe the experiment. The key is
recognized by INI validation, but it is **never** added to default
generated INIs. This experiment does **not** require SDL3,
`SDL3NativeSourceTrace`, a controller, or any generic-input bridge.
Disable by removing the key or setting it to `0`.

## Safety contract

- The main EXE must have passed the existing exact SHA check. A separate
  signature/layout preflight checks the image base at `0x00400000`,
  original keyboard reader code at `0x00501C56`, buffered
  `GetDeviceData` call/compare at `0x00501BC3`, x86 process, and
  the target byte's committed writable memory protection.
- **Only** the existing `FOCUS-PAUSE` code, upon confirming another
  application's foreground window for at least 150 ms, can arm a
  loss. A successful *manual* foreground wake arms a **single**
  subsequent recovery window. SDL3 `sampling=SUSPENDED`, Bink
  changes and native in-game pause alone cannot arm it.
- Each eligible sample occurs on the original Flip thread, strictly
  while the game HWND is foreground and menu or 3D is active; Bink
  and paused game states are excluded. A return window expires
  after 5 seconds.
- The original Left Alt latch must be **exactly `0x7F`**
  (stable held, not `0xFF` fresh press or `0x80` fresh release).
  Win32 must report Left Alt, generic Alt, Right Alt and Tab **all
  released** on **six consecutive** foreground Flip samples spanning
  at least 180 ms after return; the original Right Alt and Tab latches
  must also be released. New physical Alt/Tab presses reset the
  stable counter.
- Final foreground, OS modifier and native byte conditions are
  rechecked immediately before writing. If all pass, the **only
  native write possible** is `0x006B7964` (original keyboard
  `0x006B792C + DIK_LMENU 0x38`), changing `0x7F -> 0x80`.
  `0x80` is the original game keyboard reader's normal *released
  edge* representation. The operation is **one-shot per confirmed
  loss/return**, even if the stale state returns later.
- Never clears the other 255 keyboard latch bytes, never touches
  mouse/joystick buffers, never calls DirectInput Acquire, never
  patches the executable, never changes keyboard mappings, and
  never sends a Win32 `SendInput`/synthetic global key up.
- If an observable fresh physical Alt press already cleared the
  original latch or if a Bink/pause makes the window ineligible,
  the policy fails closed. A physical key held during the sampling
  window must **not** be released automatically.

The original DirectInput keyboard latch can still reflect a genuine
key being held while the Windows async API temporarily differs.
That is why the feature is **optional**, relies on repeated independent
readings, and is not enabled for general users.

## Windows test protocol (not performed yet)

1. Preserve the previous `SpiderMan2k1Fix.dll` and `winmm.dll`.
   Install the PR #88 Windows x86 CI artifact, verify the game loads
   the DLL from the nested `SpiderMan2k1Fix` directory (see
   `DLL-LOAD: target` in log).
2. Enable `ExperimentalAltLatchRecovery=1` and `Log=1`; retain
   `ExperimentalSDL3Probe=1` and `SDL3NativeSourceTrace=1` if
   you want independent `alt-tab` / `os-alt-tab` diagnostics.
3. In native 3D, verify physical Left Alt and Tab work normally
   before any focus switch. Alt+Tab away, release both keys outside,
   Alt+Tab/foreground back, wake with the DS4 (or keyboard).
4. Expect `ALT-LATCH: return confirmed` and, **only** when the
   original latch remains stale and Windows shows no modifiers down
   through six eligible Flip samples, `ALT-LATCH: REPAIRED ...`.
   The passive trace should then show original Left Alt no longer
   held. Test keyboard arrows/Space and DS4 actions after recovery.
5. Repeat with **Alt physically held on focus return**; it must not
   be forcibly released. Repeat with **no focus loss** and with a
   Bink-only/pause-only eligibility transition; those must not
   trigger repair. Test manual Alt press/release immediately after
   a recovery, ensuring the original key continues to work.
6. Re-run with feature `=0` and confirm there is no `ALT-LATCH:
   REPAIRED` line. Check both Xbox and DS4 and Wine before any
   broader enablement.

If the game freezes/crashes, Alt becomes unresponsive, or a
held modifier is cleared unexpectedly, disable the experiment and
report the full log. No merges without maintainer review.

Signature: legluondunet assisted by ChatGPT (OpenAI)
