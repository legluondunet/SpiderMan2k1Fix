"""The PE32 resource audit must accept exact payloads and reject altered bytes."""
from importlib.util import module_from_spec, spec_from_file_location
from pathlib import Path
import struct

ROOT = Path(__file__).resolve().parents[2]
SPEC = spec_from_file_location(
    "resource_audit", ROOT / "scripts/verify_spiderman_installer_resources.py"
)
AUDIT = module_from_spec(SPEC)
SPEC.loader.exec_module(AUDIT)


def _fixture(tmp_path):
    package = tmp_path / "package"
    package.mkdir()
    pe = bytearray(8192)
    pe[:2] = b"MZ"
    struct.pack_into("<I", pe, 0x3C, 0x80)
    pe[0x80:0x84] = b"PE\x00\x00"
    struct.pack_into("<HHIIIHH", pe, 0x84, 0x14C, 1, 0, 0, 0, 0xE0, 0)
    optional = 0x80 + 24
    struct.pack_into("<H", pe, optional, 0x10B)
    section = optional + 0xE0
    struct.pack_into("<8sIIII", pe, section, b".rsrc\x00\x00\x00", 0x1600, 0x1000, 0x1800, 0x200)
    base = 0x200
    struct.pack_into("<II", pe, optional + 96 + 2 * 8, 0x1000, 0x1600)

    def directory_entry(parent, idx, name, child, is_directory=True):
        struct.pack_into("<II", pe, base + parent + 16 + idx * 8,
                         name, child | (0x80000000 if is_directory else 0))

    struct.pack_into("<HH", pe, base + 12, 0, 1)
    directory_entry(0, 0, 10, 0x20)  # IMAGE_RESOURCE_DATA_IS_DIRECTORY / RCDATA
    struct.pack_into("<HH", pe, base + 0x20 + 12, 0, 7)
    for idx, (resource_id, filename) in enumerate(AUDIT.PAYLOADS.items()):
        payload = (f"fixture-{resource_id}".encode() * 4)
        destination = package / filename
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(payload)
        language_dir = 0x70 + idx * 0x18
        data_entry = 0x200 + idx * 0x10
        raw = 0x300 + idx * 0x80
        directory_entry(0x20, idx, resource_id, language_dir)
        struct.pack_into("<HH", pe, base + language_dir + 12, 0, 1)
        directory_entry(language_dir, 0, 1033, data_entry, False)
        struct.pack_into("<IIII", pe, base + data_entry, 0x1000 + raw, len(payload), 0, 0)
        pe[base + raw:base + raw + len(payload)] = payload
    path = tmp_path / "SpiderMan2k1Fix.exe"
    path.write_bytes(pe)
    return path, package


def test_pe32_rcdata_exact_match(tmp_path):
    exe, package = _fixture(tmp_path)
    AUDIT.verify(exe, package)


def test_pe32_rcdata_mutation_fails_closed(tmp_path):
    import pytest
    exe, package = _fixture(tmp_path)
    corrupt = bytearray(exe.read_bytes())
    corrupt[0x200 + 0x300] ^= 0x40
    exe.write_bytes(corrupt)
    with pytest.raises(ValueError, match="mismatches staged"):
        AUDIT.verify(exe, package)


def test_pe32_missing_rcdata_fails_closed(tmp_path):
    import pytest
    exe, package = _fixture(tmp_path)
    corrupt = bytearray(exe.read_bytes())
    struct.pack_into("<HH", corrupt, 0x200 + 0x20 + 12, 0, 6)
    exe.write_bytes(corrupt)
    with pytest.raises(ValueError, match="missing RCDATA IDs"):
        AUDIT.verify(exe, package)
