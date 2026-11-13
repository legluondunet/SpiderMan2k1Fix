# SpiderMan2k1Fix 2.0.0 — standalone installer design and evidence gate

The maintainer requested one executable beside their own retail ISO, on the
same model as Rayman2Fix. This is a **release blocker**, not a promise that the
original disc is already supported by the first bootstrap executable.

## Required user-facing layout

```text
SpiderMan2k1Fix.exe
Spider-Man.iso
SpiderMan2k1Fix_installer.log      # installer/update/verification
SpiderMan2k1Fix/
  SpideyPC.exe                     # SHA256-confirmed original retail game
  winmm.dll                        # built-in proxy from project
  data.pkr, media.pkr, binkw32.dll # originals from user's media
  SpiderMan2k1Fix.log             # game/runtime events, never installer output
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
- Deploy project-owned/redistributable resources from embedded RCDATA.
  Use temporary files and preserve at most the immediately previous DLL as
  `.bak`, without touching existing saves or user configuration.
- Game working directory is the install directory. On Wine the game child
  receives `winmm=n,b` without altering the user's system registry.
- The existing game's `SpiderMan2k1Fix.log` stays separate. The launcher
  resets only `SpiderMan2k1Fix_installer.log` at its own startup.
- During the bootstrap phase a genuinely packed retail InstallShield disc
  must be **refused with a diagnostic**, rather than being declared
  installed after a raw ISO copy. Do not merge an incomplete prototype.

## Critical missing CD evidence

The maintainer's current Lutris recipe extracts ISO to a temporary folder,
runs `Autorun.exe`, **then explicitly copies** `Setup/media.pkr` into the
game directory. That does not establish whether the original installer
expands CAB archives, changes file names, creates essential empty folders,
sets registry values, or selects source assets by install component.

To claim original-CD installation complete, inspect the authentic user's ISO
and its setup manifest, record installed tree with sizes/hash baselines,
reconstruct those choices without executing any 16-bit installer, and run
end-to-end install + gameplay tests under both Windows 11 and Wine 11.x.
Do not guess from an Internet repack, and do not redistribute game binaries.

## Release gate

1. ISO source identification and required file map proven against original CD.
2. Synthetic ISO tests (valid/Joliet, malformed record/extent, bad hash,
   multiple images, tampered embedded resources, failed stage/rollback).
3. Built-in update mode preserves settings and save games; one `.bak` only.
4. Actual one-EXE Windows x86 CI artifact with verified embedded resources.
5. Independent Windows 11 and Wine installation, normal launch and shutdown.
6. Only then merge the installer and publish tag `v2.0.0`.

