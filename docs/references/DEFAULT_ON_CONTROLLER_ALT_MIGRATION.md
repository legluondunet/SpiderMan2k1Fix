# SDL3 and original Left Alt recovery: default-on staging plan

**Status: PR #92, DRAFT, not released, no main merge.** This document
describes the new integration built on top of the current `main` release,
with the tested PR #91 combined runtime as its second merge parent.

## What changes from the opt-in test binary?

All these keys are now generated with `=1` and have **1 as their
runtime fallback when absent**. A user-specified `=0` remains an
explicit opt-out, and cannot be silently overwritten.

```ini
[Input]
ExperimentalSDL3Probe=1
ExperimentalSDL3NativeOwner=1
ExperimentalSDL3ControllerSetup=1
ExperimentalSDL3ContextBridge=1
ExperimentalSDL3GameplayBindings=1
ExperimentalSDL3XboxRTSwing=1
ExperimentalNativeJoystickUnplugGuard=1
ExperimentalNativeJoystickReconnect=1
ExperimentalNativeJoystickMappingSwap=1
ExperimentalAltLatchRecovery=1
```

- **SDL3 Probe:** discovers mapped pads only when the official x86
  `SDL3.dll` exists, matching original device identity. An unknown,
  unverified, ambiguous or disconnected controller does not gain
  game-owned input ownership automatically.
- **Native owner:** still depends on exact original EXE/memory
  verification and the three native safety prerequisites
  (unplug-guard, reconnect and in-memory mapping swap).
  If even one prerequisite is explicitly disabled or refuses
  activation, the SDL3 native-owner handoff fails closed.
- **Gamepad menu/Bink/UI and 3D bindings:** supported original Sony
  DS4 and Xbox 360-compatible controllers retain strict identity,
  neutral-state, mapping and game-state gates. Unsupported custom
  button semantics cannot modify game-owned native mappings.
  The Xbox RT Swing path retains its physical trigger down/up
  guards; no research-only generic DS3/Steam Deck mapper is enabled.
- **Original Left Alt recovery:** still only checks one verified
  native game-owned byte after confirmed external loss and
  foreground return, and only after six stable release frames,
  with both OS and original game's modifier states clear. No
  keyboard injection, external-window focus steal or user-key
  replacement is added.
- **Not promoted:** `[Debug] Log=1`, controller trigger/trace
  probes, read-only reverse-engineering experiments, legacy
  `ControllerPS1Preview` and `ControllerXbox360Preview`, and
  independent Sony audio-output routing PR #86.

## Upgrading an existing INI: important distinction

The new defaults apply to *missing* keys, and to newly generated
`SpiderMan2k1Fix.ini` files. **An older INI may already have
`ExperimentalNativeJoystickUnplugGuard=0`,
`ExperimentalNativeJoystickReconnect=0`, or
`ExperimentalNativeJoystickMappingSwap=0`** because
an earlier version generated them as off. Those **saved zeros
take precedence over the new default 1** and may intentionally
keep SDL3 native control disabled. This avoids silently
overwriting user configuration.

To enable the new defaults on an older installation, with the game
closed, back up the INI and change those explicit `=0` values
to `=1`, or remove the related keys so the new runtime fallback
applies. **Edit each existing key rather than appending duplicates**:
a prior Windows test caused correct INI repair after a second
obsolete `MouseEnabled` key was accidentally appended.
Never edit the 504-character `[Internal] RawHex`.

For a deliberate opt-out, set `ExperimentalSDL3Probe=0` to
disable the SDL3 device-selection path, or
`ExperimentalAltLatchRecovery=0` for the original Left Alt
repair alone. More granular `=0` flags remain accepted, but
must be tested together with the safety dependency chain.
The keyboard remains usable without gamepads.

## Wine/Proton evidence and movie-to-first-3D handoff follow-up

The maintainer's real **Manjaro 7.1.13-2, Wine 11.0 /
GE-Proton11-11** run `SpiderMan2k1Fix(20261101-124923).log`
using the GitHub PR #92 test merge `6a13eb7` verified
**four focus-loss/resume cycles**, **3/3 Bink pause/resume
pairs**, **12/12 active DirectSound voices** stopped and
restored once, **four verified DS4/Xbox original DirectInput
rebinds**, a **balanced Xbox RT Swing down/up** and 3D-to-menu
return. The maintainer additionally confirmed **all native
3D controls on both controllers worked correctly after each
Xbox ↔ DS4 transition**. Nine diagnostic refusals of optional
SDL3 custom seven-action remapping had **no user-observed
control failure**; preserve their fail-closed behavior.

Under Wine, `ALT-LATCH: ARMED` was reported but the two
sampled focus returns found the original Left Alt naturally
cleared (`SKIPPED`); native Windows 11 tests did require
actual `0x7F→0x80` repairs. The fix remains default-on for
both platforms, subject to the original safety conditions.

**Bink timing follow-up, not yet physically retested:**
the Wine log shows `GAMEPAD-UI: SYNTHETIC ... Return DOWN`,
then **`DEBUG-BINK-ENTER-HANDOFF timeout=1 waited_ms=1509`**,
and only afterwards the synthetic `KEYUP`. The safety
barrier waits for Enter/Space release on the first game-owned
3D update, while the UI layer previously scheduled the
synthetic Return release on the **next DirectDraw Flip**.
Blocking the same game thread prevents that Flip from
arriving, producing a bounded 1.5-second wait.

The staging correction now releases **only its own pending,
game-thread-owned synthetic Return** at the movie #4
handoff, with verified foreground, visible game window
and exact Return scan, **before** the original physical
Enter/Space release check. It never resets real-key
state, clears the game's input table, skips a script,
injects a new key-down, or disables the 1500-ms
safety timeout for a genuinely held key. This is
covered by `ChatGPT/tests/test_bink_sdl3_return_handoff.py`
in addition to the original movie-4/F10 regression
tests. A new release binary must be checked with
the same Bink-skip-to-3D scenario under Wine and
native Windows, and should show the dedicated
`GAMEPAD-UI: BINK-4-HANDOFF pending synthetic Return released`
line, with `timeout=0` **when no physical key
remains held**. No shorter wait is guaranteed if the
player is physically holding Enter or Space.

**INI provenance remains a separate question.** The
maintainer confirmed each platform test started with
a **fresh log**, which makes evidence independent;
this does **not by itself establish** a factory-new
`SpiderMan2k1Fix.ini` for the first-launch migration
checks below.

## 1 November Wine/Windows evidence and Bink handoff fix

The maintainer's separate, freshly generated **log files** establish a
positive hardware result on both native Windows 11 and
Manjaro/Wine 11.0 with a GE-Proton11-11 environment hint:
the PR #92 test-merge binary `v1.1.0-6a13eb7` ran with SDL3
and Left Alt recovery armed by default, and supported Sony DS4
and Xbox 360 controllers functioned in playable 3D, including
after physical ownership swaps. The maintainer explicitly confirms
**no failed controller actions under Wine**. The Wine test had
4/4 focus resumes, 3/3 Bink resumes, 12/12 playing DirectSound
voices restored, and one balanced Xbox RT Swing press/release.
Left Alt required actual safe memory repairs on native Windows 11,
while it cleared naturally under Wine.

**A fresh log is not the same as a fresh INI.** These checks
do not prove that the config file was absent at launch. The
**first run with a newly generated INI** and an **existing INI
with explicit `=0`** are still distinct pending checks.

Wine's 12:49 log also revealed the precise timing hazard
near movie #4: our selected SDL3 gamepad's **synthetic
Return KEYDOWN** was emitted on a Flip; the native first-3D
Enter/Space release barrier then blocked the game thread
for ~1,500 ms, **before the next Flip could send KEYUP**.
The new staging change makes the verified movie #4 handoff
release **only our own pending synthetic Return**, on its
recorded injection thread, before the original `GetAsyncKeyState`
checks. The real physical Enter and Space release wait
remains in place, with the same bounded timeout, and the
actual original game input reader is still called once.
No game-owned keyboard byte or native skip request is written.

Look for the new `GAMEPAD-UI: BINK-4-HANDOFF pending
synthetic Return released before physical Enter/Space wait`
diagnostic and a `DEBUG-BINK-ENTER-HANDOFF` report
with `timeout=0` when no physical Enter/Space is held.
A real held key, missing foreground verification, or
failed Windows `SendInput` may still cause the protective
wait to reach `timeout=1`. **This code adjustment has
passed static/source and Windows x86 build checks, but
must be tested in the game on the exact new artifact.**

## 1 Nov: release blocker — long-held Cross/A skips the Stan Lee intro

**Status: on-device regression reproduced; follow-up build NOT yet
physically retested.** The maintainer observed that holding **physical
DS4 Cross** somewhat longer than a quick tap while skipping the
**separate New Game Bink video** causes the **first scripted 3D
cinematic with Stan Lee to be skipped**. A new Wine 11.0 log,
`SpiderMan2k1Fix(20261101-132515).log` (test-merge
`a70b4b8`), shows the 1,500ms synthetic Return issue was fixed,
but this *separate* physical hold issue remains. The actual 3D script
skip is verified by the player's observation, **not directly by the
old log**, which only records native scene-mode frames.

The native game reads **SOUTH/Cross/A as a real 3D Jump/scene action**.
Our synthetic Return `KEYUP` does **not** release a physical
controller button. Even when `DEBUG-BINK-ENTER-HANDOFF timeout=0`,
an inherited held Cross may cause a real native cutscene skip.
`raw_enter_before=0x7F` is seen in both observed successful
and skipped runs; it does not establish independent causation.

**Staged candidate fix** in
`SpiderSDL3Probe_NewGameBinkWaitSouthRelease`: only at the
original positively verified New Game Bink-4 → first 3D input owner,
refresh SDL3 on its existing game thread, require the exact selected
DS4/Xbox VID:PID and verified game-owned DirectInput identity.
If SOUTH is held, wait for **its physical release** before the first
original 3D input update, then allow 40ms to settle. Neither the
original joystick's state nor script request byte or keyboard buffer
is rewritten. A **5s watchdog** prevents indefinite blocking, but
**a continuously held button at timeout can still cause a skip**:
do not describe this as guaranteed immunity to arbitrarily long
holds. Unsupported/unverified devices fail closed with no new
ownership or input patch.

**Mandatory regression test:** using the build generated *after*
this change, under Wine and Windows separately, select **New Game**,
hold **DS4 Cross** or **Xbox A** long enough to pass the movie and
continue holding into the scene boundary, then release. The log should
show `SDL3-BINK-4-SOUTH: HELD` followed by
`SDL3-BINK-4-SOUTH: RELEASED after <N> ms` and no timeout,
while the **first Stan Lee 3D cinematic plays normally**. Also
perform a normal short tap and verify the transition is not delayed.
If `TIMEOUT 5000ms` appears, re-evaluate the policy rather than
approving a release. The previous successful 0ms Bink retest does not
satisfy this newly discovered physical input scenario.

## Mandatory new physical smoke test before release

**This is new behavior**. Previous 20/20 Windows 11 checks were
performed with manually enabled opt-in settings. They demonstrate
the runtime implementation, **not** the fresh-INI default-on
activation after this change.

1. Install **both DLLs from the same PR #92 artifact** and
   the bundled Windows x86 `SDL3.dll`. Back up the current
   INI. For a *fresh factory-default trial only*, temporarily
   move the existing INI aside while the game is closed.
2. Run on **Windows 11 native**, with one supported DS4 and/or
   Xbox 360. Confirm the new factory INI contains all ten
   `=1` keys, no duplicate keys, and the log records verified
   original DirectInput ownership, permanent in-game mouse OFF,
   and **`ALT-LATCH: ARMED`**. Do not treat lack of
   `REPAIRED` as failure when Alt is naturally cleared.
3. Test menu/Bink keyboard and controller navigation, 3D
   gamepad actions (including **Xbox RT Swing release**),
   native Pause, save/quit return-to-menu and several Alt+Tab
   transitions with active effects audio. Ensure the
   Windows desktop pointer is usable outside the game.
4. Repeat a limited smoke run under **Wine/Proton** with the
   new factory defaults. The custom PAUSED banner may appear
   late/not at all in 3D and is **nonblocking if keyboard,
   gamepad, audio, graphics and stability remain correct**.
5. For an *existing INI*, use the original current file, verify
   that saved `=0` still disables only the desired option,
   and that restoring `=1` re-enables it **without
   automatically erasing custom joystick/volume bindings**.
6. Attach native Windows and Wine logs with the selected
   controller and **installed archive commit/hash**, then
   approve or report any functional regression. A CI PASS
   alone cannot satisfy this physical default-activation test.

**Integration boundary:** keep PR #92 as a draft against
`main`. Do not merge stacked PRs #80/#88/#90/#91 independently
or overwrite already published `RELEASE_NOTES_1.1.0.md`.
Decide release version, defaults and publication only after
the final branch CI, artifact comparison and smoke results.

legluondunet assisted by ChatGPT (OpenAI)
