# ChatGPT development workspace

This directory stores **internal development tooling** for SpiderMan2k1Fix. It is distinct from the public game installation and the project's maintained [technical documentation](../docs/README.md).

- [Regression tests](tests/) — static/source checks, C helper runners, packaging checks and [INI fixtures](tests/fixtures/)
- [Developer utilities](tools/) — optional PowerShell inspection and exit-verification scripts
- [Temporary and external debugger diagnostics](diagnostics/) — WinDbg/CDB experiments and explanatory notes

## Running the tests

From the repository root, run:

```bash
python3 -m pytest -q ChatGPT/tests
```

The CI workflows use this same path. The two C test runners compile against headers at the **repository root**. Build scripts remain at the repository root:

```bash
./build.sh
```

**Distribution:** these files are tracked for developers and CI, but `.gitattributes` marks `ChatGPT/` as `export-ignore` for future GitHub source archives. Binary artifacts and official binary releases independently package just the two DLLs and the public README/CHANGELOG. None of the tooling is needed to install or run the game.

**Scope:** maintain original historical diagnostic findings in `docs/`; new scratch experiments belong here. Some historic PR comments and previously published commands will still mention older paths, but their immutable history is not altered.
