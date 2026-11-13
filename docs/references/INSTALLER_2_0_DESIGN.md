# SpiderMan2k1Fix 2.0.0 — standalone installer design and evidence gate

The maintainer requested one executable beside their own retail ISO, on the
same model as Rayman2Fix. This is a **release blocker**, not a promise that the
original disc is already supported by the first bootstrap executable.

## Required user-facing layout

```text
SpiderMan2k1Fix.exe
Spider-Man.iso
SpiderMan2k1Fix_installer.log      # one-time install/verification
SpiderMan2k1Fix.log                # game runtime log (same launcher directory)
SpiderMan2k1Fix/
  SpideyPC.exe                     # SHA256-confirmed original retail game
  winmm.dll                        # built-in proxy from project
  data.pkr, media.pkr, binkw32.dll # originals from user's media
  # game log is beside the launcher, NOT inside this folder
  SpiderMan2k1Fix/
    SpiderMan2k1Fix.dll            # built-in fix DLL
    SDL3.dll                       # verified official SDL redistributable
    SDL3-LICENSE.txt
    README.md, CHANGELOG.md
    SpiderMan2k1Fix.ini            # created by game, user-mutable
    linux/spiderman-focus-helper  # native observer; optional under Wine
```

The bundled payload must not contain `SpideyPC.exe`, `data.pkr`, `media.pkr`,
any disc assets, save data, or the ISO. The exact supported retail executable
is 1,507,328 bytes, SHA-256
`0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea`.

## First bootstrap scope (PR forthcoming)

- Native PE32 Windows/Wine console launcher, `--help`, `--version`, `--inspect`,
  `--verify`, and zero-argument automatic detection.
- Strictly bounded ISO9660/Joliet file inspection. Reject unknown/cyclic
  records, invalid file extents, unsafe relative names, multiple adjacent
  `.iso` files and unsupported raw-sector formats. Do not mount ISO or
  execute the CD `Autorun.exe` / `Setup.exe`.
- Exact retail executable SHA-256 check before adopting or launching any
  game folder; fail closed on all mismatches.
- Deploy project-owned/redistributable resources from embedded RCDATA
  **once**, inside the same private staging directory as the validated
  original CD files; publish the complete tree only after all assets pass.
  No DLL backup, upgrade, repair or overwrite of an existing installation.
  Existing installs are **verified read-only and launched**; missing or
  changed runtime files trigger an explicit refusal.
- Game working directory is the install directory. On Wine the game child
  receives `winmm=n,b` without altering the user's system registry.
- Both logs are beside `SpiderMan2k1Fix.exe`: launcher resets only
  `SpiderMan2k1Fix_installer.log`; the game child inherits the validated
  launcher directory through `SPIDERMAN2K1FIX_LAUNCHER_DIR`, and
  both the winmm proxy and core share `SpiderMan2k1Fix.log`.
  A direct legacy `SpideyPC.exe` run retains its original log fallback.
- During the bootstrap phase a genuinely packed retail InstallShield disc
  must be **refused with a diagnostic**, rather than being declared
  installed after a raw ISO copy. Do not merge an incomplete prototype.

## Critical missing CD evidence

The currently accessible inputs include the exact supported original
`SpideyPC.exe`, an inventory of 2,448 `data.pkr` resources, and
existing game/runtime logs. These are NOT the retail CD installation files.
No Spider-Man CD ISO, `Autorun.exe`, `Setup.exe` or InstallShield/CAB
container from this game is presently accessible in the workspace/library.
The installer is accordingly restricted to **inspection/fail-closed**
for unverified disc layouts.

The maintainer's historical Lutris recipe extracts the ISO to a temporary
folder, runs `Autorun.exe`, **then explicitly copies** `Setup/media.pkr`
into the game directory. This suggests extra installer work, but does not
establish whether the retail installer expands CAB archives, changes file
names, creates essential directories, sets registry keys, or selects assets
by install component. These facts need to be reconstructed from original
media, not guessed from an unpacked game or third-party archive.

To claim original-CD installation complete, inspect the authentic user's ISO
and its setup manifest, record installed tree with sizes/hash baselines,
reconstruct those choices without executing any 16-bit installer, and run
end-to-end install + gameplay tests under both Windows 11 and Wine 11.x.
Do not guess from an Internet repack, and do not redistribute game binaries.

## Release gate

1. ISO source identification and required file map proven against original CD.
2. Synthetic ISO tests (valid/Joliet, malformed record/extent, bad hash,
   multiple images, tampered embedded resources, failed stage/rollback).
3. One-time embedded payload deployment; existing installation is verified
   read-only and never updated/repaired. No DLL `.bak` is created.
4. Actual one-EXE Windows x86 CI artifact with verified embedded resources.
5. Independent Windows 11 and Wine installation, normal launch and shutdown.
6. Only then merge the installer and publish tag `v2.0.0`.

