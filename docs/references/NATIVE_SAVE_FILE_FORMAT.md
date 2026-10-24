# Spider-Man (2001) PC: native progress save file (`SPIDRMAN.DAT`)

> **Research / documentation only — 2026-10-24.** This page describes
> observed bytes in **one original PC game save**, cross-checked with the
> public partial decompilation and save editor by krystalgamer. The game
> binary and save file were **not changed**. No personal save file or
> proprietary game data is committed to this repository.
>
> **Confidence labels:** **Observed** = checked in the submitted binary;
> **Reverse-engineered** = supported by the linked code; **Inferred** =
> plausible but not independently demonstrated with multiple PC saves.
> This is **not yet a SpiderMan2k1Fix feature**: the patch does not hook,
> migrate, back up or repair native progress saves.

## 1. Native save mechanism and location

The original game uses a single file named **`SPIDRMAN.DAT`**, storing
**eight game-progress slots**. It is independent of the 252-byte
`Spidey.cfg` settings and of the patch's `SpiderMan2k1Fix.ini`.
The partially decompiled game uses:

- `pcdcBkup.cpp::buInit()`: `GetCurrentDirectoryA()`, appends
  `\save\`, then calls `CreateDirectoryA()`.
  **Reverse-engineered implication:** the save directory is relative to the
  process's **working directory**, not necessarily the executable directory.
  Different launchers / working directories can show seemingly missing
  saves without deleting the underlying data.
- `ps2card.cpp::Card_Load` and `Card_Write`: load/write a backup-image
  named `SPIDRMAN.DAT`.
- `pcdcBkup.cpp::buLoadFile` / `buSaveFile`: ordinary C file I/O using
  `rb` and `wb` on the constructed path.
- `shell.cpp::Shell_AddGameSlots`: list eight slots as a saved name,
  `---- empty ----` or `read error` according to the saved and computed
  checksum. The `read error` UI therefore **does not by itself prove**
  an OS/disk failure; it is also the game's checksum-mismatch label.

**Paths under Windows:** running with `Program Files` as the working
directory can interact with Windows permissions and UAC file
virtualization; do not assume that the game always uses a single shared
`save` directory or that a missing slot was erased.

## 2. Observed whole-file layout

The analyzed example has **3,072 bytes (`0xC00`)** on disk.
Its first four bytes are a little-endian internal image size
**`0x00000A24` (2,596 bytes)**. The four-byte size prefix is **not**
part of the embedded image. The image begins at file offset `0x0004`.

| Absolute file offset | Length | Contents / confidence |
| --- | ---: | --- |
| `0x0000..0x0003` | `0x4` (4) | Little-endian image length `0xA24` — **observed** |
| `0x0004..0x0203` | `0x200` (512) | `SCardHead`, signature `SC`, type `0x11`, entry `0x01`, title `SPIDRMAN.DAT`, icon/palette — **observed + reverse-engineered** |
| `0x0204..0x07E3` | `8 * 0xBC` (1,504) | Eight consecutive `SSaveGame` slots — **observed + reverse-engineered** |
| `0x07E4..0x0A27` | `0x244` (580) | Additional game data, **not conclusively decoded** |
| `0x0A28..0x0BFF` | `0x1D8` (472) | Zero padding to six 512-byte sectors — **observed** |

The auxiliary 580-byte region is **close to** the decompiled
`SRecords` structure size `0x242` (578 bytes), but the offset, two
remaining bytes and meaning of individual records have **not been
demonstrated**. Only **20 bytes were nonzero** in the sample. A valid
header and checksums do not certify the semantics of this additional
region.

### Slot boundaries (absolute offsets)

Each slot occupies `0xBC` bytes (188). Offsets are calculated as
`0x0204 + index * 0xBC`, with `index = 0..7`.

| Slot | Absolute start | Observed status |
| ---: | --- | --- |
| 1 | `0x0204` | Occupied, checksum valid |
| 2 | `0x02C0` | Entirely zero (empty) |
| 3 | `0x037C` | Entirely zero (empty) |
| 4 | `0x0438` | Entirely zero (empty) |
| 5 | `0x04F4` | Entirely zero (empty) |
| 6 | `0x05B0` | Entirely zero (empty) |
| 7 | `0x066C` | Entirely zero (empty) |
| 8 | `0x0728` | Entirely zero (empty) |

An all-zero slot is **empty**, not a damaged nonzero save. Its stored
checksum is zero, while the algorithm below would return `1` for 184
zero data bytes. Skip checksum validation on *completely empty* slots.

## 3. Decoded fields in a nonempty slot

Offsets in this table are **relative to the start of the slot**, not the
start of the file. The submitted sample has **one occupied slot**.
The name below is user-supplied in game; it is *not* a verified completion
percentage.

| Relative slot offset | Length | Meaning / sample | Confidence |
| --- | ---: | --- | --- |
| `+0x00` | 4 | Stored checksum, sample `0x6D478F4B`; calculation matches | Observed + source-confirmed |
| `+0x04` | 7 | Level ID, `l1a1_t` = area 1, mission 1, `Get to the Bank!` | Observed; level-name mapping in `spidey-tools/save_editor/levels.h` |
| `+0x0D` | up to 50 | Restart-point string field in `SSaveGame`, sample includes `e_Start_Death`; exact gameplay interpretation not established | Structure known; semantics **not** established |
| `+0x3F` | 9 | Slot display name, sample `100 SAVE` | Observed + editor mapping |
| `+0x54` | 1 | Difficulty value `3` = Hard (`0` Kid Mode, `1` Easy, `2` Normal, `3` Hard) | Observed + source-confirmed |
| `+0x55` | 1 | `playAgain` enabled (`1`) | Observed + editor mapping |
| `+0x56` | 35 | Progression flags: all **34 listed missions** are nonzero, with one `2` rather than `1`; the 35th byte is also nonzero. Value `2` is **unexplained** | Observed; exact semantics partly unknown |
| `+0x7C` | 4 | Current costume index `0` = classic Spider-Man | Observed + editor mapping |
| `+0x80` | 4 | Unlocked-costume bitmask `0x000003FF` = 10 of 10 costume bits | Observed + editor mapping |

The sample's eight slots are **one occupied + seven empty**, consistent
with a heavily completed/replay-enabled first slot. It **does not prove**
100% completion of every collectible, challenge, or hidden statistic.
Offsets outside the identified fields must not be overwritten.

### Slot checksum (exact native algorithm)

`shell.cpp::Shell_CalculateGameChecksum()` processes **46 32-bit
little-endian words** beginning at relative offset `+0x04` (i.e. all
remaining 184 bytes), rotating the accumulator left by one and adding
each word with 32-bit wraparound. The final accumulator has bit 0 forced
to `1`. The equivalent pseudocode:

```text
sum = 0
for word in 46_le_u32_words(slot[0x04:0xBC]):
    sum = ROTL32(sum, 1)
    sum = (sum + word) & 0xFFFFFFFF
return sum | 1
```

The sample produced **`0x6D478F4B`**, exactly matching the saved checksum.
Seven empty slots must not be treated as invalid merely because the
calculated checksum for an all-zero *nonempty* slot would be `1`.

A matching checksum validates this **particular internal integrity
test**. It cannot exclude a wrong-but-self-consistent level ID, unknown
corruption in the surrounding container, or a previously edited save
whose checksum was recomputed.

## 4. Known code risks and not-yet-reproduced failure modes

These are properties visible in the **partial decompilation**; they are
not claims of a reproduced bug on every Windows or Wine installation.

| Area | Observed original code | Consequence / limit |
| --- | --- | --- |
| Relative path | `buInit` reads process working directory and appends `\save\` | Different launch working directories may use different save files |
| Overwrite | `buSaveFile` opens the file with `fopen(path, "wb")` | An existing file is truncated before the new backup-image is written; all eight slots share that file |
| Short write | `fwrite(data, 0x200, sector_count, stream)` has no checked return and `fclose` is unchecked | Short writes / close failures may go undetected by that low-level function |
| Short read | `fread(buffer, 0x200, sector_count, stream)` has no checked return | Returned success is not by itself evidence that every sector was read |
| Bad slot checksum | `Shell_AddGameSlots` compares the stored checksum against `Shell_CalculateGameChecksum` | The game displays `read error` for the affected slot, including after edits without checksum updates |
| Write permissions | Legacy relative directories on modern Windows | Write denial or redirection via UAC `VirtualStore` is possible; unverified in the submitted sample |

Do **not** conflate these low-level code weaknesses with a proven global
game-save bug. The submitted file's active slot checksum is valid, and
no corrupt slot was observed.

## 5. Future patch design — investigation only

A potential independent SpiderMan2k1Fix contribution should first:

1. Log the **effective native save path** without changing it; verify
   Windows 11 and Wine launches from different current directories.
2. Test loading, creating, overwriting each of the eight slots, and
   handling a missing/short file; preserve original error behavior.
3. Implement a **read-only** verifier for declared image size, signature,
   slot boundaries and checksums; report but never silently mutate
   unknown bytes or auto-"fix" mismatched checksums.
4. Design **transactional persistence** or a verified `.old` backup
   *before* the game's truncating write, including error propagation,
   rollback, file locking and process-interruption tests. Simply copying
   the old file **after** writing is not a protection.
5. Repeat tests under Windows and Wine with read-only directories,
   insufficient disk space, truncated sectors and both valid and
   damaged saves. Keep a backup of test data before experiments.

**Current project status:** there are **no save-system hooks, repairs,
backups or automated save-file validation** in the patch. This document
does not authorize changing a player's `SPIDRMAN.DAT` silently.

## 6. Public reverse-engineering sources

- [krystalgamer/spidey-decomp: `pcdcBkup.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/pcdcBkup.cpp)
  (`buInit`, `buLoadFile`, `buSaveFile`).
- [`ps2card.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/ps2card.cpp)
  (`Card_Load`, `Card_Write`, `SPIDRMAN.DAT`, `0xA24` image).
- [`ps2card.h`](https://github.com/krystalgamer/spidey-decomp/blob/master/ps2card.h)
  (`SCardHead` and `SSaveFile`).
- [`shell.cpp`](https://github.com/krystalgamer/spidey-decomp/blob/master/shell.cpp)
  (eight slots, checksum, `read error` menu behavior).
- [`shell.h`](https://github.com/krystalgamer/spidey-decomp/blob/master/shell.h)
  (`SSaveGame` relative offsets and `0xBC` size).
- [krystalgamer/spidey-tools: `save_editor/save.c`](https://github.com/krystalgamer/spidey-tools/blob/master/save_editor/save.c)
  (specific progress, costume, `playAgain` field interpretations).
- [`save_editor/levels.h`](https://github.com/krystalgamer/spidey-tools/blob/master/save_editor/levels.h)
  (mission and costume name tables).

The format is reverse engineered; the game's proprietary binary or saves
are **not distributed** with SpiderMan2k1Fix.
