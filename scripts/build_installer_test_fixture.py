#!/usr/bin/env python3
"""Generate a TEST-ONLY installer source, never a release binary.

Only CI uses this fixture source. It replaces the supported retail EXE
hash with a hash of deterministic synthetic bytes and injects a rollback
failure point. The production source and public EXE are untouched.
"""
from __future__ import annotations

import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "installer/spiderman2k1fix_installer.c"
TARGET = ROOT / "installer/spiderman2k1fix_installer.fixture.c"
EXE_BYTES = 1_507_328
FAKE_EXE = b"MZ" + b"A" * (EXE_BYTES - 2)
ORIGINAL = "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
FAIL_SITE = "if(!MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)){"
FAIL_PATCH = (
    'if((i==3 && GetEnvironmentVariableW(L"SPIDER_FIXTURE_FAIL_PUBLISH",NULL,0)>0) ||\n'
    '       !MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)){'
)

def main() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    assert source.count(ORIGINAL) == 1, "production retail hash must be unique"
    assert source.count(FAIL_SITE) == 1, "upgrade rollback site changed"
    assert "SPIDER_FIXTURE_FAIL_PUBLISH" not in source, "test injection leaked into production"
    text = source.replace(ORIGINAL, hashlib.sha256(FAKE_EXE).hexdigest())
    text = text.replace(FAIL_SITE, FAIL_PATCH)
    assert text != source
    TARGET.write_text(text, encoding="utf-8")
    print(f"TEST-ONLY source: {TARGET.name}; production retail SHA unchanged")
    print(f"Synthetic executable bytes={EXE_BYTES} sha256={hashlib.sha256(FAKE_EXE).hexdigest()}")

if __name__ == "__main__":
    main()
