# F12 native engine shutdown: mode-aware design and evidence

**Status: investigation / draft PR #26. Do not merge until explicit user approval and Windows runtime tests.**

This investigation uses ONLY the original, SHA-gated *Spider-Man (2001)* Windows executable:

```text
SpideyPC.exe size: 1,507,328 bytes
SHA-256: 0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
Format: PE32 i386
```

All listed virtual addresses (VA) are for this executable. The numeric meanings
of many engine states are **not yet validated**; source-level or disassembly
evidence must not be presented as a runtime result.

## Confirmed Windows 11 behavior

| Game context | Tested behavior | Decision |
|---|---|---|
| Actual interactive main menu | Build #471: F12 activates native Shell_MainMenu Quit (item 3, command 18). User confirmed `SpideyPC.exe` absent in Task Manager | **Keep the existing proven menu hook unchanged** |
| Four opening Bink movies | Build #476: F12 skips the clips, reaching a static pre-menu screen; no full shutdown | Preserve game-owned movie cleanup, fix shutdown handoff separately |
| Two static intro screens | User confirmed two screens after Bink; second requires Enter. Build #483 still stops on first despite logging native Quit flag set | Locate actual screen controller, no simulated Enter or forced window close |
| Comic pages | Not tested for a safe shutdown path | Only observe state |
| 3D gameplay | Not tested for a safe shutdown path | Only observe state |

An earlier direct `WM_CLOSE` destroyed the HWND without killing the rendering/audio
process. It is unsafe to equate **window disappearance** or **last decoded Bink
frame** with a clean engine exit. Only a verified stopped process, preferably
exit code 0 and no Windows crash event, validates closure.

## Verified original binary flow

### Opening and native Bink cleanup

- `0x00455C90`: main startup/controller. Four native movie calls
  `CALL 0x00470750` at `0x00455CF1`, `0x00455CFE`, `0x00455D0C`,
  `0x00455D1A`.
- `0x0047081D` and `0x0047085F`: native Bink tick
  `CALL 0x0050B5A0`.
- `0x00470871`: movie owner executes native Bink close at
  `0x0050B790`; the owner later clears `0x006151F8`. A 0-result
  from the native tick allows this cleanup on the game thread.
- `0x00455D23`: `CALL 0x00443AD0`, the native post-movie transition.
  The function itself contains a conditional call to `0x00470750`,
  so “four calls completed” must not be equated to “all media and
  startup states finished” without an actual runtime checkpoint.

### Top-level controller and why build #483 misses the exit

- `0x0060CF88`: original engine Quit byte.
- `0x00455E50`: `CMP BYTE PTR [0x0060CF88], BL`,
  `JNE 0x004561F2`; `0x004561F2` calls `0x00443AD0(3)`,
  then branches to `0x0045620C` final cleanup.
- **Key discovery:** upstream `JNE 0x00455E5E` branches at both
  `0x00455DC8` and `0x00455DD0` **bypass the native Quit-byte check**
  at `0x00455E50`. Thus the correct log message “Quit flag set”
  in build #483 does **not** prove that the conditional Quit branch
  was ever executed.
- `0x00455F24`: `MOV EAX,[0x0060CFA4]` reads current engine mode.
  `0x00455F4C` decrements it; `0x00455F56` jumps via
  `0x00456274`, a table of **11 modes**. The jump destinations are:

| Mode value | Dispatch VA | Label |
|---|---|---|
| 1 | `0x00456100` | Unknown pending runtime correlation |
| 2 | `0x00455F5D` | Unknown |
| 3 | `0x00456045` | Unknown |
| 4 | `0x00456118` | Unknown |
| 5 | `0x00456118` | Unknown |
| 6 | `0x00455FE0` | Unknown |
| 7 | `0x00456185` | Unknown |
| 8 | `0x00455FB0` | Unknown |
| 9 | `0x00455F5D` | Unknown |
| 10 | `0x00456156` | Unknown |
| 11 | `0x00456177` | Unknown |

The table is extracted from the verified executable's `.text` data, not
deduced from screenshots. **Do not guess which numeric mode is 3D, comics,
static startup image, or menu before tracing the real game.**

### Main menu (already user-validated)

- Actual `Shell_MainMenu` at `0x00493990`.
- Native per-frame call `0x00493DD6`, selected Quit item **3** in table
  `0x00552AA8`, corresponding to command **18**.
- Existing F12 hook runs on the menu thread and takes the native
  selection/cleanup branch `0x004941D5`.
- This success does **not** imply that calling menu cleanup while in 3D,
  Bink or a static splash is safe.

## Agreed architecture

1. Capture F12 once while the real game is foreground and latch an
   **intent**, not a Windows destroy or immediate native destructor call.
2. Locate the **active native mode** (start movies, static screens, actual
   menu, comic pages, gameplay). Do not infer mode from time since last
   decoded frame alone.
3. The mode owner performs its **own** normal exit and resource cleanup
   on the **owning game thread**.
4. Control returns to the top-level engine controller. Only at a verified
   owner-safe point does the engine take its normal final shutdown.
5. A state never safely identified **must fail closed**: ignore F12 with a
   debug log rather than leave a headless renderer or crash.
6. Keep the validated main-menu action exactly as working in build #471.

No `WM_CLOSE`, `DestroyWindow`, `ExitProcess`, `TerminateProcess`,
simulated Enter/menu navigation, or off-thread DirectDraw/DirectSound/Bink
cleanup should be used as the F12 shutdown implementation.

## Diagnostic phase: read-only engine-state instrumentation

The existing `IDirectDrawSurface7::Flip` hook now calls
`SpiderQuickExitF12_ObserveEngineState(count)` when
`[System] QuickExitF12=1`. It **reads only**:

- `0x0060CFA4`: native engine mode (for dispatch mapping).
- `0x0060CF88`: native Quit byte.
- `0x006B4C9C`: transition/async state, observed in native spin loop.
- `0x0054A510`: native task/status variable used by the controller.
- Existing patch-local observations: whether main-menu callback was
  observed, intro F12 intent and whether post-movie forwarding executed.

It logs only when mode/quit/async state changes, or after five seconds
without a snapshot. It does **not** install a new executable machine-code
hook, mutate game-state pointers, or trigger a shutdown.

Example expected log shape (example values intentionally omitted):

```text
QUICK-EXIT-F12: ENGINE-STATE flip=... mode=... async=... task=... quit=... menu_seen=... intro_request=... intro_forwarded=...
```

Collect **separate log files** while viewing (a) the first static screen
after movies, (b) the second “Press Enter” screen, (c) the interactive
main menu, (d) comic pages, and (e) actual 3D gameplay. The original
F12 during unsupported modes need not be pressed; passive snapshots
alone are enough to establish the mode/state mapping.

Only after mapping the mode values to scene types should separate,
signature-gated **mode-owned exit hooks** be implemented and tested
individually on Windows 11.

## Validation gates

- Native mode resource cleanup executes on the correct thread and never
  while COM/Bink callbacks are live.
- Menu F12 user-regression test remains successful.
- Videos stop and audio ends before final engine shutdown.
- Program actually exits; no `SpideyPC.exe` remains.
- External process watcher records an ordinary exit code and checks for
  Windows Error Reporting crash events.
- Comic, static-image and 3D pathways pass **separate** live tests.
- PR remains a draft and is merged only after user approval.

Credit: **legluondunet assisted by ChatGPT (OpenAI)**.
