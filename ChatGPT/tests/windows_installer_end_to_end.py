#!/usr/bin/env python3
"""Execute the real PE32 installer on Windows with synthetic ISO/CUE fixtures.

Never uses/distributes original commercial game bytes. A separate, clearly
marked TEST-ONLY build accepts the deterministic fake 1,507,328-byte EXE.
The production release executable is independently checked to refuse it.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile

BLOCK = 2048
ORIGINAL_BYTES = 1_507_328
FAKE_EXE = b"MZ" + b"A" * (ORIGINAL_BYTES - 2)
FILES = {
    "SpideyPC.exe": FAKE_EXE,
    "data.pkr": b"synthetic-data.pkr" * 17,
    "media.pkr": b"synthetic-media.pkr" * 13,
    "texture.dat": b"synthetic-texture",
    "binkw32.dll": b"synthetic-Bink-placeholder",
    "Anet.inf": b"synthetic-ini",
    "SpideyPC.ico": b"synthetic-icon",
    "Spider-Man Setup.exe": b"synthetic-display-selector",
}
OWNED = [
    "winmm.dll",
    "SpiderMan2k1Fix/SpiderMan2k1Fix.dll",
    "SpiderMan2k1Fix/SDL3.dll",
    "SpiderMan2k1Fix/SDL3-LICENSE.txt",
    "SpiderMan2k1Fix/linux/spiderman-focus-helper",
    "SpiderMan2k1Fix/README.md",
    "SpiderMan2k1Fix/CHANGELOG.md",
]


def directory_record(name: bytes, extent: int, size: int, directory: bool) -> bytes:
    length = 33 + len(name) + (len(name) % 2 == 0)
    assert 34 <= length <= 255
    record = bytearray(length)
    record[0] = length
    struct.pack_into("<I", record, 2, extent)
    struct.pack_into(">I", record, 6, extent)
    struct.pack_into("<I", record, 10, size)
    struct.pack_into(">I", record, 14, size)
    record[25] = 2 if directory else 0
    struct.pack_into("<H", record, 28, 1)
    struct.pack_into(">H", record, 30, 1)
    record[32] = len(name)
    record[33:33 + len(name)] = name
    return bytes(record)


def pack_directory(records: list[bytes]) -> bytes:
    blob = b"".join(records)
    assert len(blob) < BLOCK
    return blob.ljust(BLOCK, b"\x00")


def make_iso(*, missing: str = "", duplicate: bool = False,
             bad_extent: bool = False, bad_pvd: bool = False) -> bytes:
    """A minimal ISO9660 image exercising the installer's real C parser."""
    selected = {name: value for name, value in FILES.items() if name != missing}
    locations: dict[str, tuple[int, int]] = {}
    next_block = 22
    for name, payload in selected.items():
        locations[name] = (next_block, len(payload))
        next_block += (len(payload) + BLOCK - 1) // BLOCK
    total_blocks = next_block + 1
    out = bytearray(total_blocks * BLOCK)
    primary = bytearray(BLOCK)
    primary[0] = 1
    primary[1:6] = b"CD001"
    primary[6] = 1
    primary[40:72] = b"SPIDERMAN".ljust(32, b" ")
    struct.pack_into("<I", primary, 80, total_blocks)
    struct.pack_into(">I", primary, 84, total_blocks)
    struct.pack_into("<H", primary, 128, BLOCK)
    struct.pack_into(">H", primary, 130, BLOCK)
    root = directory_record(b"\x00", 20, BLOCK, True)
    primary[156:156 + len(root)] = root
    if bad_pvd:
        primary[1:6] = b"BROKE"
    out[16 * BLOCK:17 * BLOCK] = primary
    term = bytearray(BLOCK)
    term[0] = 255
    term[1:6] = b"CD001"
    term[6] = 1
    out[17 * BLOCK:18 * BLOCK] = term

    root_dir = [
        directory_record(b"\x00", 20, BLOCK, True),
        directory_record(b"\x01", 20, BLOCK, True),
        directory_record(b"Setup", 21, BLOCK, True),
    ]
    out[20 * BLOCK:21 * BLOCK] = pack_directory(root_dir)
    entries = [
        directory_record(b"\x00", 21, BLOCK, True),
        directory_record(b"\x01", 20, BLOCK, True),
    ]
    for name, payload in selected.items():
        extent, length = locations[name]
        record_extent = total_blocks + 10 if bad_extent and name == "data.pkr" else extent
        entries.append(directory_record((name + ";1").encode("ascii"),
                                        record_extent, length, False))
        out[extent * BLOCK:extent * BLOCK + length] = payload
    if duplicate and "data.pkr" in locations:
        lba, size = locations["data.pkr"]
        entries.append(directory_record(b"DATA.PKR;1", lba, size, False))
    out[21 * BLOCK:22 * BLOCK] = pack_directory(entries)
    return bytes(out)


def scenario(base: Path, name: str, binary: Path) -> tuple[Path, Path]:
    directory = base / name
    directory.mkdir()
    exe = directory / binary.name
    shutil.copy2(binary, exe)
    return directory, exe


def execute(exe: Path, *args: str, success: bool,
            environment: dict[str, str] | None = None) -> str:
    env = os.environ.copy()
    if environment:
        env.update(environment)
    outcome = subprocess.run(
        [str(exe), *args], cwd=exe.parent, env=env,
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        timeout=90, check=False,
    )
    expected = 0 if success else 1
    assert outcome.returncode == expected, (
        f"{exe.name} args={args} rc={outcome.returncode} expected={expected}\n"
        f"stdout={outcome.stdout[-3000:]}\nstderr={outcome.stderr[-3000:]}"
    )
    report = exe.parent / (
        "SpiderMan2k1Fix_inspect.log" if args and args[0] == "--inspect"
        else "SpiderMan2k1Fix_installer.log"
    )
    assert report.exists(), f"missing log: {report}"
    return report.read_text(encoding="utf-8-sig", errors="replace")


def no_partial_install(directory: Path) -> None:
    assert not (directory / "SpiderMan2k1Fix").exists()
    assert not (directory / "SpiderMan2k1Fix.installing").exists()
    assert not (directory / "SpiderMan2k1Fix.updating").exists()


def negative_media_cases(base: Path, production: Path) -> None:
    valid = make_iso()
    directory, exe = scenario(base, "inspect_valid_iso", production)
    (directory / "original.iso").write_bytes(valid)
    log = execute(exe, "--inspect", "original.iso", success=True)
    assert "INSPECT_RESULT=COMPLETE" in log
    no_partial_install(directory)

    directory, exe = scenario(base, "production_rejects_fake_exe", production)
    (directory / "original.iso").write_bytes(valid)
    log = execute(exe, "--install-only", success=False)
    assert "unsupported original game hash" in log
    no_partial_install(directory)

    cases = {
        "missing_required_file": make_iso(missing="data.pkr"),
        "duplicate_file": make_iso(duplicate=True),
        "unsafe_file_extent": make_iso(bad_extent=True),
        "bad_iso_descriptor": make_iso(bad_pvd=True),
        "truncated_iso": valid[:30 * BLOCK],
    }
    for name, data in cases.items():
        directory, exe = scenario(base, name, production)
        (directory / "original.iso").write_bytes(data)
        execute(exe, "--install-only", success=False)
        no_partial_install(directory)

    directory, exe = scenario(base, "ambiguous_images", production)
    (directory / "first.iso").write_bytes(valid)
    (directory / "second.iso").write_bytes(valid)
    report = execute(exe, "--install-only", success=False)
    assert "none or multiple" in report
    no_partial_install(directory)

    directory, exe = scenario(base, "cue_missing_track", production)
    (directory / "original.cue").write_text(
        'FILE "missing.bin" BINARY\n  TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n',
        encoding="ascii",
    )
    execute(exe, "--install-only", success=False)
    no_partial_install(directory)

    directory, exe = scenario(base, "cue_unsafe_parent_path", production)
    (directory / "original.cue").write_text(
        'FILE "../outside.bin" BINARY\n  TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n',
        encoding="ascii",
    )
    execute(exe, "--install-only", success=False)
    no_partial_install(directory)

    directory, exe = scenario(base, "valid_raw_cue", production)
    raw = bytearray()
    for at in range(0, len(valid), BLOCK):
        raw += b"\x00" * 16 + valid[at:at + BLOCK] + b"\x00" * 288
    (directory / "track.bin").write_bytes(raw)
    (directory / "original.cue").write_text(
        'FILE "track.bin" BINARY\n  TRACK 01 MODE1/2352\n  INDEX 01 00:00:00\n',
        encoding="ascii",
    )
    log = execute(exe, "--inspect", "original.cue", success=True)
    assert "INSPECT_RESULT=COMPLETE" in log
    no_partial_install(directory)


def fixture_integrity_and_enrollment(base: Path, fixture: Path) -> None:
    directory, exe = scenario(base, "fixture_integrity", fixture)
    (directory / "original.iso").write_bytes(make_iso())
    log = execute(exe, "--install-only", success=True)
    assert "complete; game was not launched" in log
    game = directory / "SpiderMan2k1Fix"
    receipt = game / "SpiderMan2k1Fix/installed-game.sha256"
    assert receipt.is_file()
    assert "SpiderMan2k1Fix-Game-v1" in receipt.read_text(encoding="ascii")
    execute(exe, "--verify", success=True)

    archive = game / "data.pkr"
    prior = archive.read_bytes()
    archive.write_bytes(prior + b"corrupted")
    log = execute(exe, "--verify", success=False)
    assert "original game file missing/altered" in log
    assert archive.read_bytes().endswith(b"corrupted")  # no silent repair
    archive.write_bytes(prior)
    execute(exe, "--verify", success=True)

    receipt.unlink()  # simulate previous 2.0 prerelease installation
    execute(exe, "--verify", success=False)
    archive.write_bytes(prior + b"corrupted")
    log = execute(exe, "--install-only", success=False)
    assert "ENROLL: original file mismatch" in log
    assert not receipt.exists()
    archive.write_bytes(prior)
    log = execute(exe, "--install-only", success=True)
    assert "ENROLL: OK eight original game files" in log
    assert receipt.exists()
    execute(exe, "--verify", success=True)

    receipt.write_bytes(receipt.read_bytes() + b"malicious edit\n")
    prior_receipt = receipt.read_bytes()
    execute(exe, "--verify", success=False)
    log = execute(exe, "--install-only", success=False)
    assert "existing game receipt invalid; refusing replacement" in log
    assert receipt.read_bytes() == prior_receipt


def fixture_rollback_upgrade(base: Path, fixture: Path) -> None:
    directory, exe = scenario(base, "fixture_upgrade_rollback", fixture)
    (directory / "original.iso").write_bytes(make_iso())
    execute(exe, "--install-only", success=True)
    game = directory / "SpiderMan2k1Fix"
    receipt = game / "SpiderMan2k1Fix/installed-resources.sha256"
    # Make a formerly known runtime look like an older but receipt-authenticated
    # bundled version. This is an isolated test fixture, not a user install.
    winmm = game / OWNED[0]
    winmm.write_bytes(winmm.read_bytes() + b"old-version-fixture")
    lines = receipt.read_text(encoding="ascii").splitlines()
    assert lines[0] == "SpiderMan2k1Fix-Resources-v1" and len(lines) == 8
    lines[1] = hashlib.sha256(winmm.read_bytes()).hexdigest()
    receipt.write_bytes(("\n".join(lines) + "\n").encode("ascii"))  # exact LF on Windows
    snapshot = {
        item: (game / item).read_bytes() for item in OWNED
    }
    snapshot["receipt"] = receipt.read_bytes()

    log = execute(
        exe, "--install-only", success=False,
        environment={"SPIDER_FIXTURE_FAIL_PUBLISH": "1"},
    )
    assert "UPGRADE: cannot publish" in log, log
    for item in OWNED:
        assert (game / item).read_bytes() == snapshot[item], item
        assert not (game / (item + ".bak")).exists(), item
    assert receipt.read_bytes() == snapshot["receipt"]
    assert not (directory / "SpiderMan2k1Fix.updating").exists()
    assert (game / "data.pkr").read_bytes() == FILES["data.pkr"]

    log = execute(exe, "--install-only", success=True)
    assert "authenticated bundled components updated" in log
    execute(exe, "--verify", success=True)
    assert (game / "data.pkr").read_bytes() == FILES["data.pkr"]


def main() -> None:
    if os.name != "nt":
        raise SystemExit("Windows-only executable integration harness")
    if len(sys.argv) != 3:
        raise SystemExit("usage: windows_installer_end_to_end.py release.exe TEST-ONLY.exe")
    production, fixture = (Path(a).resolve() for a in sys.argv[1:])
    assert production.exists() and fixture.exists()
    assert "TEST-ONLY" in fixture.name
    assert hashlib.sha256(production.read_bytes()).digest() != hashlib.sha256(fixture.read_bytes()).digest()
    with tempfile.TemporaryDirectory(prefix="spider-installer-negative-") as root:
        base = Path(root)
        negative_media_cases(base, production)
        print("PASS: production malformed ISO/CUE and rejected retail-signature paths")
        fixture_integrity_and_enrollment(base, fixture)
        print("PASS: eight-file receipt, read-only verify and ISO-based legacy enrollment")
        fixture_rollback_upgrade(base, fixture)
        print("PASS: injected mid-publish failure restores all authenticated resources")
    print("PASS: all Windows PE32 executable negative-path tests")


if __name__ == "__main__":
    main()
