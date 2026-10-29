"""Offline tests for reproducible SDL3 gamepad runtime release packaging.

These tests do not download executables; CI integration downloads the
pinned official release and checks its SHA-256 before extracting it.
"""
from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import struct
import zipfile

import pytest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/package_sdl3_x86.py"

spec = importlib.util.spec_from_file_location("spider_sdl3_bundle", SCRIPT)
assert spec and spec.loader
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def fake_pe(machine=0x14C, magic=0x10B):
    pe = bytearray(1024)
    pe[:2] = b"MZ"
    struct.pack_into("<I", pe, 0x3C, 128)
    pe[128:132] = b"PE\x00\x00"
    struct.pack_into("<H", pe, 132, machine)
    struct.pack_into("<H", pe, 152, magic)
    return bytes(pe)


def archive_with(name, data):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(name, data)
    return stream.getvalue()


def test_pinned_upstream_version_url_sha_and_original_license():
    src = SCRIPT.read_text(encoding="utf-8")
    assert module.VERSION == "3.4.18"
    assert module.ASSET == "SDL3-3.4.18-win32-x86.zip"
    assert module.URL == (
        "https://github.com/libsdl-org/SDL/releases/download/"
        "release-3.4.18/SDL3-3.4.18-win32-x86.zip"
    )
    assert module.SHA256 == (
        "99f2ee6e126d828c5e5b3d20201930f3d16a8c6e147d1de90a59f4ae6df138c9"
    )
    assert module.ASSET_SIZE == 994224
    assert "check_pe32_x86(data)" in src
    assert "data[pe_at:pe_at + 4] != b\"PE\\x00\\x00\"" in src
    assert "expected: str = SHA256" in src
    assert "LICENSE" in src
    license = (ROOT / "third_party/SDL3-LICENSE.txt").read_text(encoding="utf-8")
    assert "Copyright (C) 1997-2026 Sam Lantinga" in license
    assert "redistribute it" in license
    assert "This notice may not be removed or altered" in license


def test_upstream_sha_before_pe_validation_and_strict_machine():
    content = fake_pe()
    archive = archive_with("SDL3.dll", content)
    digest = hashlib.sha256(archive).hexdigest()
    assert module.extract_verified_runtime(archive, digest) == content
    with pytest.raises(ValueError, match="SHA-256 mismatch"):
        module.extract_verified_runtime(archive, "0" * 64)
    for machine, magic in [(0x8664, 0x20B), (0x14C, 0x20B), (0xAA64, 0x20B)]:
        bad = archive_with("SDL3.dll", fake_pe(machine, magic))
        with pytest.raises(ValueError, match="PE32 i386"):
            module.extract_verified_runtime(bad, hashlib.sha256(bad).hexdigest())


def test_fail_closed_for_zip_slip_duplicate_dll_wrong_name_and_bad_pe():
    for filename in ("../SDL3.dll", "/SDL3.dll", "x\\SDL3.dll"):
        blob = archive_with(filename, fake_pe())
        with pytest.raises(ValueError, match="unsafe path"):
            module.extract_verified_runtime(blob, hashlib.sha256(blob).hexdigest())
    blob = archive_with("SDL3-x64.dll", fake_pe())
    with pytest.raises(ValueError, match="exactly one SDL3.dll"):
        module.extract_verified_runtime(blob, hashlib.sha256(blob).hexdigest())
    blob = archive_with("SDL3.dll", b"not a DLL")
    with pytest.raises(ValueError, match="decompressed size"):
        module.extract_verified_runtime(blob, hashlib.sha256(blob).hexdigest())
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        z.writestr("SDL3.dll", fake_pe())
        z.writestr("elsewhere/SDL3.dll", fake_pe())
    blob = buf.getvalue()
    with pytest.raises(ValueError, match="exactly one"):
        module.extract_verified_runtime(blob, hashlib.sha256(blob).hexdigest())


def test_build_artifact_includes_game_folder_dll_license_and_release_zip():
    ci = (ROOT / ".github/workflows/spiderman2k1fix-windows-x86.yml").read_text()
    assert "Bundle pinned upstream SDL3 Windows x86 runtime and license" in ci
    assert "curl --fail --location --retry 4 --retry-all-errors" in ci
    assert "python3 scripts/package_sdl3_x86.py dist/package/SpiderMan2k1Fix" in ci
    assert "test -s dist/package/SpiderMan2k1Fix/SDL3.dll" in ci
    assert "cmp third_party/SDL3-LICENSE.txt" in ci
    assert "SpiderMan2k1Fix/SDL3.dll" in ci
    assert "SpiderMan2k1Fix/SDL3-LICENSE.txt" in ci
    assert "zip -q -9" in ci
    assert "sha256sum" in ci
    assert "unzip -Z -1" in ci
    assert "Upload release-ready ZIP without creating a GitHub Release" in ci
    assert "dist/release/*.zip" in ci
    assert "dist/release/*.zip.sha256" in ci
    assert ci.count("uses: actions/upload-artifact@v4") == 1
    assert ci.count("name: SpiderMan2k1Fix-Release-ZIP") == 1
    assert "name: SpiderMan2k1Fix-Windows-x86" not in ci
    assert "gh release create" not in ci
    # INI/log/user profiles are still created at runtime, never bundled.
    assert "SpiderMan2k1Fix.ini" not in ci
