# ChatGPT development workspace

This directory contains development-only tooling for **SpiderMan2k1Fix**, separate from its [maintained documentation](../docs/README.md) and [technical research synthesis](../docs/DEVELOPMENT.md).

- [Automated regression tests](tests/) — source checks, C helper runners and packaging checks
- [INI test fixtures](tests/fixtures/) — historical configuration examples used only by tests
- [Developer utilities](tools/) — PowerShell inspection and exit-verification scripts
- [External debugger experiment](diagnostics/WINDBG_DI8_TRACE.md) — read-only WinDbg/CDB tracing of DirectInput, with the associated [scripts](diagnostics/)

## Run the regression tests

From the repository root:

```bash
python3 -m pytest -q ChatGPT/tests
```

Both GitHub Actions workflows use this same command. The game DLL build scripts remain at the repository root:

```bash
./build.sh
```

## Where the explanatory documents went

All Markdown reference reports previously stored in `ChatGPT/diagnostics/` or `ChatGPT/tests/reports/` are now gathered under **[docs/references/](../docs/README.md#detailed-references-docsreferences)**. The main `docs/README.md` lists them by topic; `docs/DEVELOPMENT.md` provides the broader synthesis.

The WinDbg prototype remains here because it is directly paired with its executable diagnostic scripts. `ChatGPT/` and `docs/` are still omitted from future GitHub source archives by `.gitattributes`. Neither is part of the binary releases.
