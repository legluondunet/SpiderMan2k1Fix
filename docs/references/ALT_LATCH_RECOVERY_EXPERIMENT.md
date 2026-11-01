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

## Windows 11 hardware validation: Sony DS4, 1 November 2026 07:59

User-provided **complete 1,557-line native Windows 11 log**
`SpiderMan2k1Fix(20261101-075920).log`; original retail
`SpideyPC.exe` SHA-256 confirmed, Sony DS4
`VID:PID=054C:09CC` selected. The exact build commit is **not**
proven by the static startup version string
`v1.1.0-cd6de56`, but the new runtime emits
`ALT-LATCH: ARMED`, `return confirmed`, and `REPAIRED`, proving
the optional recovery implementation is actually loaded and running.

### Observed first real focus-loss recovery

- Log **60**: opt-in `ALT-LATCH: ARMED`; original SHA and preflight
  pass. This is **not** a default-on feature.
- **721**: in the 3D game, original keyboard and Win32 both observe
  Left Alt held: `alt-tab=1 os-alt-tab=1`.
- **724**: `INPUT-FOCUS: flip-background`;
  **731–732**: the existing cross-process foreground-loss detector
  confirms an external foreground after 172 ms (`explorer.exe`),
  and latches the PAUSED wait.
- **739**: `flip-foreground`, **743–748**: manual Return keyboard
  press/release wake, eight DirectSound secondary voices resumed
  (`failed=0`), and verified foreground state. **749**:
  `ALT-LATCH: return confirmed`.
- **753**: actual original DI/Win32 divergence remains after waking:
  `kb-held=1 alt-tab=1 os-alt-tab=0 merged-actions=0000`.
- **765**: `ALT-LATCH: REPAIRED original Left Alt 0x7F->0x80;
  6 stable foreground frames; OS modifiers up`.
- **766**: the independent observer immediately sees
  `kb-held=0 kb-released=1 alt-tab=0 os-alt-tab=0`;
  **767**: the release edge is cleared naturally by subsequent
  original game state progression (`kb-released=0`).
  This validates the **actual one-byte native write and the
  original game latch's resulting transition** on Windows 11.

### Bounded outcomes and things NOT yet validated

- Across **100** source snapshots (81 in 3D, 19 in MENU):
  **98** with `alt-tab=0 os-alt-tab=0`,
  **1** with `1/1` (before switching focus) and
  **1** with `1/0` (stale original latch immediately after return).
  **96** regular + **4** reserved snapshots; the reserved samples
  at **1540–1545** follow in-game pause/menu eligibility transitions,
  not a second proven desktop Alt+Tab.
- Precisely **one** confirmed cross-process foreground loss/return
  and **one** `ALT-LATCH: REPAIRED`. No unexpected second repair,
  automatic whole-keyboard clearing or crash/error indicator is
  visible. Subsequent physical DS4 Cross/RT button edges, axis
  movements, and nonzero game action masks appear in the log.
- The game's original controller-7 transition at **1520–1525**
  completes a return to the main/controller menu, with normal
  2D presentation afterward. This is telemetry, not an assertion
  that every controller gameplay action was successful.
- **Unverified:** the user's own observation of *all keyboard
  controls* after recovery, a **new physical Left Alt press/release**
  following a repair, keyboard-only and Xbox tests, repeated 3D
  Alt+Tabs in a single run, a negative test with the option disabled,
  and Wine/Proton. The original broad Alt+Tab control-lockout may
  involve additional input state not captured here. Do not enable
  by default or merge on this single successful test.

**Conclusion:** first **runtime hardware validation succeeded
for the precise stale Left Alt byte repair**, not a blanket proof
of complete Alt+Tab input recovery.
