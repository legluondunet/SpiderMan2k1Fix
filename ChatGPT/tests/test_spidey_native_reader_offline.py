"""Read-only regression tests for the offline native reader inspector.

No original or synthetic executable is distributed in CI.
"""
import importlib.util
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "ChatGPT" / "tools" / "inspect_spidey_native_reader.py"


def load_tool():
    spec = importlib.util.spec_from_file_location("offline_native_reader", TOOL)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_reject_unknown_content_and_malformed_binary():
    mod = load_tool()
    with pytest.raises(mod.InvalidExecutable):
        mod.parse_pe(b"not a Windows executable")
    with pytest.raises(mod.InvalidExecutable, match="exact SHA-256"):
        mod.inspect(b"not a game binary")


def test_pattern_match_is_explicitly_unverified():
    mod = load_tool()
    samples = bytes([255, 80, 36, 255, 81, 40])
    matches = mod.indirect_call_patterns(samples, 0x501E6D)
    assert [entry["possible_method"] for entry in matches] == [
        "GetDeviceState", "GetDeviceData"
    ]
    assert all(entry["status"] == "unverified_byte_pattern_not_disassembly"
               for entry in matches)


def test_cli_failure_does_not_modify_file(tmp_path):
    source = tmp_path / "sample.bin"
    original = b"invalid input data"
    source.write_bytes(original)
    result = subprocess.run([sys.executable, str(TOOL), str(source)],
                            capture_output=True, text=True)
    assert result.returncode == 2
    assert source.read_bytes() == original
    assert not list(tmp_path.glob("*.json"))


def test_no_runtime_mutation_or_external_modules():
    script = TOOL.read_text(encoding="utf-8")
    assert "EXPECTED_SHA256" in script
    assert "unsupported EXE: exact SHA-256" in script
    mod = load_tool()
    assert len(mod.KNOWN_NATIVE_SIGNATURES) == 10
    assert mod.KNOWN_NATIVE_SIGNATURES[0] == (
        0x00501E79, "ff 51 64", "Poll indirect COM call")
    assert mod.KNOWN_NATIVE_SIGNATURES[3] == (
        0x00501E9F, "8b 10 ff 52 1c",
        "Acquire indirect COM call sequence")
    assert "source-confirmed native reader signature differs" in script
    for forbidden in ("ctypes", "subprocess", "urllib", "WriteProcessMemory"):
        assert forbidden not in script
