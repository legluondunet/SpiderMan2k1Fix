# Spider-Man (2000/2001) original installer — Wise-script evidence

## Provenance and scope

This is a read-only forensic analysis of **owner-supplied Windows installer
files**, performed on November 13, 2026. No old Windows installer was run.
These files are evidence, **not redistributable game assets**, and their bytes
must never be committed to or embedded in SpiderMan2k1Fix.

| Uploaded filename | Bytes | SHA-256 | Identification |
| --- | ---: | --- | --- |
| `Autorun.exe` | 118,221 | `e66e1ae72e68ada982eef013f7b2688a4abf7164b2b2e6a0d556080433d8b40f` | PE32 Wise front-end |
| `Setup(2).exe` | 5,174,358 | `20bea9fb79f10a2f693fb7f103fa6097d572866d6353b92d94accb080e6e9f16` | PE32 Wise **Spider-Man Installation** |
| `Setup(1).exe` | 77,824 | `b671a65958fefcbaedabfb8f1c1a83e489028afa8e05a74d123915631cfc825e` | PE32 DemoShield launcher, not the game installer |
| `Spider-Man Setup.exe` | 45,056 | `2640f40c4ef46906ade2d26c6337e9ded28864b00f014e5a4eb8a293356c8b8c` | PE32 **display adapter selector**, not the installer |

All four uploaded EXEs have 32-bit PE headers. **Do not claim the original
installer is 16-bit** based on the age of the game. The Win11 compatibility
of this Wise package still requires a real test.

The separate uploaded `Setup(3).ini` is a **DemoShield** configuration
for `demo32.exe`/`Spidey.dbd` (`CopyFiles=0`), and the uploaded
`Spidey.ini` is a historical Wise/DemoShield registration state file,
not a verified required runtime configuration file. Uploaded `anet.inf`
declares `Run=SpideyPC.exe` and `ProductID=spideypc`.

## Wise scripts recovered without executing Windows code

- `Autorun.exe`: a raw-DEFLATE Wise script (2,784 bytes expanded)
  contains `Spiderman PC`, `%INST%\\Setup.exe` and `%INST%`.
  The CD frontend is a Wise wrapper and **invokes a separate Setup.exe**.
- `Setup(2).exe`: PE overlay begins at file offset `0x3A00`,
  contains a 38,646-byte decompressed Wise script, and retains about
  5 MB of setup payload. This is *not* an InstallShield CAB.
  Extracted script SHA-256: `4dfbfc4b07e1dd656656baec60437e6832dec014c390d8ca52d0250a32986cb6`.
- Both Wise scripts were inflated from original overlay data using
  standard raw DEFLATE (`zlib`, `wbits=-15`); the scripts were
  inspected as **data**, not interpreted or executed.

## Game-file installation from exact original script strings

The main Wise script contains **two game-copy lists** following a
`Select Install Size` interface. A compact first block around offset
`0x197C` omits `media.pkr`; the second block around offset
`0x4F12` adds `media.pkr`. The precise branch predicates have **not**
been decoded; it is reasonable to treat the second, superset block as
the intended complete installation **subject to original ISO verification**.

| Source in Wise script | Original destination | Proposed self-contained destination | Policy |
| --- | --- | --- | --- |
| `%INST%\\SpideyPC.exe` | `%MAINDIR%\\SpideyPC.exe` | `SpiderMan2k1Fix/SpideyPC.exe` | Essential; SHA-256 locked |
| `%INST%\\data.pkr` | `%MAINDIR%\\data.pkr` | `SpiderMan2k1Fix/data.pkr` | Essential |
| `%INST%\\media.pkr` | `%MAINDIR%\\media.pkr` | `SpiderMan2k1Fix/media.pkr` | Include by default; second copy list |
| `%INST%\\texture.dat` | `%MAINDIR%\\texture.dat` | `SpiderMan2k1Fix/texture.dat` | Essential candidate; don't omit |
| `%INST%\\binkw32.dll` | `%MAINDIR%\\binkw32.dll` | `SpiderMan2k1Fix/binkw32.dll` | Essential video codec |
| `%INST%\\Anet.inf` | `%MAINDIR%\\Anet.inf` | `SpiderMan2k1Fix/Anet.inf` | Preserve original file |
| `%INST%\\SpideyPC.ico` | `%MAINDIR%\\SpideyPC.ico` | `SpiderMan2k1Fix/SpideyPC.ico` | Preserve original icon |
| `%INST%\\Spider-Man Setup.exe` | `%MAINDIR%\\Spider-Man Setup.exe` | `SpiderMan2k1Fix/Spider-Man Setup.exe` | Preserve; **never auto-launch** |
| `%INST%\\Spidey.ini` | `%WIN%\\Spidey.ini` | none by default | Legacy system-directory write; must investigate |

All rows above are literal source/destination pairs found in the supplied
**compressed Wise script**. `%INST%` denotes the original Wise installer's
source directory. The uploaded files do **not** establish what physical
ISO folder contains those sources (root vs `Setup/`). Never infer a
source prefix from uploaded renames `Setup(1).exe`/`Setup(2).exe`.

The Wise script also enumerates an optional `Help Files` component
including `Docs/readme.txt`, `Docs/SpiderMan.pdf`, HTML help, vendor
pages and `Getinfo.dll`/`sysinfo.exe`/`Sysinv.dll`/registration tools.
Some content is stored in the multi-megabyte Wise setup payload; the
gameplay-critical source files above are referenced as loose
`%INST%\\...` files, not as members of a CAB archive. **No requirement
to decode full help/documentation Wise content has been demonstrated for
gameplay**, but this should be checked in the authentic installation.

## Other original actions — intentionally not recreated yet

The script references:

- `Software\\Activision\\Spiderman`, including a demo/full-version
  check (look for `Version=Demo`) and an executable path.
- `Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Spider-man`
  and an `Unwise.exe` uninstaller.
- Start-menu/desktop shortcuts, original DirectX installer
  (`%INST%\\..\\DirectX\\dsetup.dll`) and Electronic Registration.
- `%WIN%\\SPIDEY.INI` with paths to `sysinfo.exe`, `SpideyPC.exe`,
  Unwise and other legacy launcher values.
- The bundled `Spider-Man Setup.exe` utility queries DirectX/display
  adapters and references `Software\\Activision\\SpiderMan`.

Our standalone fix already controls native display startup and avoids
the old graphics selector. We should **not** reproduce legacy system
writes, DirectX installs, registration, autorun or uninstaller until a
specific game runtime dependency is proven. Registry independence under
Windows 11 and Wine requires a clean-prefix smoke test, not inference.

## Historical 2.0 implementation plan (before receiving the retail ISO)

1. Detect original ISO and discover the directory containing a **single,
   consistent set** of the above source files. Reject duplicates, ambiguous
   root/`Setup/` layouts, missing `texture.dat`/`media.pkr`, symlinks,
   unsafe names and invalid extents.
2. Require supported `SpideyPC.exe` length 1,507,328 bytes, SHA-256
   `0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.
   Do not accept repacks merely because their filenames match.
3. Stage original game files with the bundled project DLLs/SDL3, publish
   once, verify and launch. No future update/repair, preserve INI/saves.
4. Keep BOTH logs beside the launcher:
   `SpiderMan2k1Fix_installer.log` and `SpiderMan2k1Fix.log`.
5. Obtain **actual original Spider-Man CD ISO or read-only `--inspect`
   inventory** to confirm the source prefix, file sizes/headers and the
   `media.pkr` version. This evidence has **not** yet been supplied.
6. Test a clean Windows 11 and Wine prefix, with no legacy registry keys,
   under original game operation. Keep PR #150 a **draft**, do not
   merge/release before that test.

This document began as a **derived installer-script manifest**. The six
items above preserve the original November 13 research plan as history:
they are **not** the current release blockers. No original commercial game
files are committed or redistributed.

## Subsequent retail-ISO evidence — November 14, 2026

An authentic owner-supplied `SPIDERMAN` ISO (694,661,120 bytes) confirmed
the original source directory **`Setup/`** and all eight Wise full-install
files, including `media.pkr` and `texture.dat`. Installations were
observed under Wine-staging 11.17 and native Windows 11, including gameplay.
The original supported `SpideyPC.exe` SHA-256 matched.

The merged standalone installer (PR #150) now records two separate receipts:
the seven project-owned resources and, after PR #154, **all eight original
game files** with size and SHA-256. Legacy installations may enroll original
data only by comparing each installed file with the unchanged owner's ISO,
using explicit `--install-only`. PR #155 subsequently added executable
Windows PE32 ISO/CUE corruption and rollback tests with wholly synthetic
images. The final remaining acceptance is a brief hardware smoke test of
the newest receipt-enabled build against the original ISO before publishing
v2.0.0; it is not an obstacle to retaining this original forensic record.
