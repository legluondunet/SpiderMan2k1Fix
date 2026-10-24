# Automatic keyboard layout: implementation and original-game analysis

**Feature:** `[Input] KeyboardLayout=Auto|Off`  
**Scope:** original Windows x86 *Spider-Man* (2001) PC executable and SpiderMan2k1Fix.  
**Runtime user testing:** accepted on Windows 11 and Linux/Wine on 2026-10-24, using a French AZERTY keyboard. CI separately verifies source regressions and the Windows x86 DLL build; it does **not** emulate gameplay or certify every international layout.

This note records the actual implementation, its supported binary signatures, Windows functions, failure policy and verification. The [configuration guide](CONFIGURATION.md#keyboard-layout-auto-or-off) is the user-facing reference; [development notes](../DEVELOPMENT.md) describe the original game and the virtual configuration.

## 1. Why there are two independent paths

The original game treats keys as **physical DirectInput 8 DIK scan codes**. Its 11 user-configurable gameplay actions are stored as DWORD codes in its 252-byte `Spidey.cfg` record. Separately, the original keyboard setup UI and `CHEATS` text entry look up **names** for those scan codes in a fixed US/QWERTY table.

On a French AZERTY keyboard, this distinction matters. Before the fix, pressing the physical `M` key in CHEATS did not enter M, while pressing physical `,` did enter M. The original table associated the US M position, DIK `0x32`, with the literal `M`, not the physical French M position, DIK `0x27`.

The patch fixes **both** user-facing layers, without substituting keyboard events or changing the original DirectInput device state:

1. Map letter names in `[Keyboard]` to physical DIK codes during virtual-config loading and back to names when saving.
2. Correct the game's **in-memory** scan-code-to-name table for CHEATS and keyboard-menu text labels.

Other controls, mouse, joystick and focus/Alt+Tab handling remain separate.

## 2. Configuration and compatibility

```ini
[Input]
KeyboardLayout=Auto
```

| Setting | Effect |
| --- | --- |
| `Auto` | Resolve the active OS keyboard layout to physical A–Z scan codes at game startup, then use one validated mapping in both layers. This is written into a **newly generated** INI. |
| `Off` | Do not translate letters. The original physical US/QWERTY mapping and original key-name table are retained. |

The parser in [`ini_validation.h`](../../ini_validation.h) accepts both current values, case-insensitively. Older INIs **missing** this field behave as `Off` and are not silently remapped. Historical aliases remain accepted only for migration by [`load_keyboard_layout()`](../../virtual_spidey_cfg.c): `AZERTY` → `Auto`; `QWERTY`, `Physical` and `Legacy` → `Off`. The canonical value is written using `WritePrivateProfileStringA` when migration is needed. Existing action values, joystick buttons and `[Internal] RawHex` are not replaced merely because the option changes.

Important distinction: migrating a previously forced `AZERTY` option to `Auto` now **depends on the layout reported by Windows/Wine**. If the operating system reports the wrong active layout, detection cannot identify the letters actually printed on the keyboard. The application makes no claim to infer keyboard hardware or engraved legends.

## 3. Startup and Windows API calls

The runtime path is in [`virtual_spidey_cfg.c`](../../virtual_spidey_cfg.c):

```text
SpiderCfg_EnsureDefaultIni / SpiderCfg_ValidateCurrentIni
  -> SpiderCfg_Install / load_settings()
       -> load_keyboard_layout()
            -> GetPrivateProfileStringA("Input", "KeyboardLayout", "Off", ...)
            -> GetKeyboardLayout(0)
            -> MapVirtualKeyExA('A' ... 'Z', MAPVK_VK_TO_VSC, hkl)
            -> SpiderKeyboard_ValidateMap(...)
            -> SpiderKeyNames_Install(...)
       -> load_keyboard()
       -> load_joystick()
       -> load_audio()
```

The order is intentional: `load_keyboard_layout()` resolves the map **before** `load_keyboard()` interprets any INI letter names. The map is sampled **once per game start**, not whenever the layout changes. Switching the OS keyboard layout while playing requires a restart.

**Relevant functions and their roles:**

| Function / constant | Implementation role |
| --- | --- |
| `GetPrivateProfileStringA` | Read the INI's keyboard mode and individual action names. |
| `WritePrivateProfileStringA` | Canonicalize historical modes and save action names. |
| `GetKeyboardLayout(0)` | Acquire the layout handle (`HKL`) for the calling thread as observed by Windows/Wine. |
| `MapVirtualKeyExA(VK_A..VK_Z, MAPVK_VK_TO_VSC, hkl)` | Convert each letter's virtual-key value into the corresponding nonextended physical scan code in that layout. |
| `SpiderKeyboard_ValidateMap` | Reject any mapping with zero, extended/out-of-range (`>=0x80`) or duplicated letter scan codes. |
| `SpiderKeyboard_MapLetter` | Return the validated physical code for `Auto`, or the original DIK code for `Off` / invalid map. |
| `decode_key` / `encode_key` | Round-trip an INI key name through the same cached mapping. |
| `GetKeyboardLayout` and `MapVirtualKeyExA` via Wine | Use Wine's Windows-compatible keyboard-layout API, not a separate Linux keyboard hook or an X11-only implementation. |

The validation logic is in [`keyboard_layout.h`](../../keyboard_layout.h) and is pure C, independent of Windows headers. Its `spider_us_letter_scans[26]` array is the reference US DIK mapping. The result is cached as `keyboard_scans[26]` and `keyboard_scans_valid`. On an incomplete/ambiguous layout **all** letter bindings fall back to their original US physical mapping: no partial mix of layouts is deliberately applied.

Example of the user-observed French A–Z mappings:

| Printed letter | Original US position (DIK) | Detected French position (DIK) |
| --- | --- | --- |
| A | `0x1E` | `0x10` |
| Q | `0x10` | `0x1E` |
| Z | `0x2C` | `0x11` |
| W | `0x11` | `0x2C` |
| M | `0x32` | `0x27` |

The game's original numeric mapping is not changed globally; the selected 11 actions still store the resolved **physical** DIK values. Explicit hexadecimal `0xNN` values, arrow keys, SPACE and other named non-letter keys retain their original physical interpretation. The game does not use a modern Windows text-input framework for CHEATS.

## 4. Retail CHEATS and keyboard-menu label fix

Source: [`keyboard_names.c`](../../keyboard_names.c), [`keyboard_names.h`](../../keyboard_names.h). The original executable was verified against SHA-256:

```text
0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea
```

The following addresses belong to **that particular original PE32 image**, not arbitrary versions or game ports:

| VA | Original-game function or data |
| --- | --- |
| `0x0049E5A0` | `Shell_Cheats` entry (calls name-input routine). |
| `0x0048DC10` | `Shell_InputName` entry; text entry used by CHEATS. |
| `0x0048E048` | Within `Shell_InputName`, relative CALL to the key-name helper; exact 5-byte opcode is checked. |
| `0x00501550` | `DXINPUT_GetKeyName`; original routine indexes a string-pointer table from a DIK scan code. |
| `0x00562FF4` | Original QWERTY key-name pointer table. |

The sequence is `Shell_Cheats` → `Shell_InputName` → `DXINPUT_GetKeyName` → original name table. The game's keyboard configuration menu also uses the same lookup.

**`SpiderKeyNames_Install()` procedure:**

1. Refuse any action for `Off`, no `HKL`, invalid A–Z map, or a map identical to original US QWERTY.
2. `original_text_path_verified()` checks original instructions using `memcmp` at `0x0048E048` and `0x00501550`, checks selected table pointers, then validates that all 26 original US letter entries point to their expected one-character strings.
3. Copy the first `0x80` (`128`) entries of the native table into local `original` and `replacement` arrays. Mark all scan-code positions occupied by letters in the detected layout.
4. For displaced old US letter positions, call `native_nonletter_name()` to attempt a printable punctuation name, by calling `MapVirtualKeyExA(scan, MAPVK_VSC_TO_VK, hkl)` and `ToUnicodeEx`. Reuse original-game pointers for common punctuation (`COMMA`, `PERIOD`, `COLON`, etc.) or use the neutral DLL-owned label `KEY` when no matching one-character punctuation is supported. This avoids a duplicate valid CHEATS letter at the abandoned US position.
5. For all 26 letters, assign `replacement[letter_scans[i]] = original[spider_us_letter_scans[i]]`; the **letter string stays the same**, while its DIK index changes to the detected physical location.
6. Use `VirtualProtect(..., PAGE_READWRITE, ...)`, copy the replacement table into **process memory**, then request restoration of its previous protection. The original `SpideyPC.exe` file on disk is untouched; no new DirectInput factory/device hook is installed.

In the user's validated French AZERTY case, the physical M key (DIK `0x27`) now resolves to `M` in CHEATS. The physical comma key (DIK `0x32`) no longer yields M. The same fix updates key legends in the keyboard configuration screen.

The original PC game's `Shell_InputName` has only been partially recovered in the external `krystalgamer/spidey-decomp` project; SpiderMan2k1Fix **does not** replace or claim to have recompiled that function. It modifies the established key-name data it reads. The same internal table mechanism does not prove support for non-Latin input, dead keys, IME composition or every international punctuation layout.

## 5. Safety boundaries and diagnostics

- **Supported binary:** patch initialization is behind the supported executable SHA-256 gate; the name-table modification also has its own opcode and pointer-data checks. If signatures disagree, CHEATS name correction is skipped rather than patching unknown code/data.
- **No DirectInput event interception:** no `DirectInput8Create`, `IDirectInputDevice8`, `GetDeviceData` or COM-vtable wrapper is added for this feature. The existing Alt+Tab and input-reacquisition features are independent.
- **No permanent EXE changes:** only process memory is changed using `VirtualProtect`; no new game assets are distributed.
- **Two-layer fallback:** an invalid OS letter map reverts INI interpretation to physical codes and skips native name remapping. A **native table signature mismatch** skips only the name-table stage; INI action mapping can still work, so the two stages may differ if the original binary unexpectedly changes.
- **Logging:** set `[Debug] Log=1` and relaunch. `SPIDEY-CFG: keyboard letter layout=Auto HKL=... valid=1 A=... Z=... W=... M=...` reports the selected layout and physical codes. `KEY-NAMES: Auto keyboard layout applied...` confirms table patch application; a mismatch or `VirtualProtect` failure is logged as a skip.
- **Configuration recovery:** invalid INIs are backed up and reconstructed by the standard validator; old aliases are permitted for one-way canonicalization without discarding other recognized fields.

## 6. Test evidence and limitations

**Automated regression tests:** [`ChatGPT/tests/test_keyboard_layout.py`](../../ChatGPT/tests/test_keyboard_layout.py) compiles the pure-C mapping policy with the host C compiler, checks US identity, French A/Q–Z/W–M, a QWERTZ example, duplicate/out-of-range/zero fallback, and statically checks that the CHEATS table patch is signature-guarded without broad DirectInput interception. [`test_ini_validation.py`](../../ChatGPT/tests/test_ini_validation.py) checks accepted old aliases and the current modes. The Windows x86 build is produced by `build.sh` through CI.

**Manual acceptance (2026-10-24):** the user reported successful testing of the `Auto|Off` revision on **Windows 11** and subsequently **Linux/Wine**. An earlier Windows 11 test demonstrated the uncorrected comma→M CHEATS problem; the final implementation was accepted after the auto-detection and table fix.

**Not established by these tests:** every Windows locale, keyboard hardware engraving, non-Latin input, arbitrary Wine keyboard-layout misconfiguration, runtime layout switching without restart, or all possible game binaries. Do not extrapolate the verified 2001 executable signatures to another release without a separate disassembly and SHA check.

## 7. File/function index

| File | Important symbols |
| --- | --- |
| [`keyboard_layout.h`](../../keyboard_layout.h) | `SpiderKeyboardLayout`, `SPIDER_KEYBOARD_AUTO`, `SPIDER_KEYBOARD_OFF`, `spider_us_letter_scans`, `SpiderKeyboard_ValidateMap`, `SpiderKeyboard_MapLetter` |
| [`virtual_spidey_cfg.c`](../../virtual_spidey_cfg.c) | `load_keyboard_layout`, `letter_scan`, `decode_key`, `encode_key`, `load_keyboard`, `save_keyboard`, `load_settings` |
| [`keyboard_names.c`](../../keyboard_names.c) | `original_text_path_verified`, `native_nonletter_name`, `SpiderKeyNames_Install` |
| [`ini_validation.h`](../../ini_validation.h) | Strict allowed values and legacy-mode acceptance |
| [`ChatGPT/tests/test_keyboard_layout.py`](../../ChatGPT/tests/test_keyboard_layout.py) | Map validation, runtime-path assertions and original-table-guard regression tests |

For reverse-engineering background, see the [original function catalog](ORIGINAL_EXE_FUNCTION_CATALOG.md) and the [DirectInput static analysis](diagnostics/directinput/DIRECTINPUT_STATIC_ANALYSIS.md).
