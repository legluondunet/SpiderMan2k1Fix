"""Fully synthetic Windows minidump tests for original Spider-Man DDraw triage.

No real user WER minidump, executable or private process memory is committed.
All data is generated from bytearrays inside this test module.
"""
from pathlib import Path
import importlib.util
import struct
import pytest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "ChatGPT/tools/inspect_spiderman_ddraw_dump.py"


def module():
    spec = importlib.util.spec_from_file_location("spider_wer_inspector", SOURCE)
    loaded = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(loaded)
    return loaded


def fake_dump(*, hwnd=True, ready=True, fault=0):
    b = bytearray(32 + 36)

    def append(raw):
        off = len(b)
        b.extend(raw)
        return off

    modules = [
        (0x00400000, 0x2A0D000, "C:\\Synthetic\\SpideyPC.exe"),
        (0x72B60000, 0x13E000, "C:\\Synthetic\\SpiderMan2k1Fix.dll"),
        (0x7ABD0000, 0xAF000, "C:\\Windows\\System32\\apphelp.dll"),
    ]
    m = append(bytes(4 + 108 * len(modules)))
    struct.pack_into("<I", b, m, len(modules))
    for i, (base, size, name) in enumerate(modules):
        encoded = name.encode("utf-16le")
        nrva = append(struct.pack("<I", len(encoded)) + encoded)
        struct.pack_into("<QIIII", b, m + 4 + 108 * i,
                         base, size, 0, 0, nrva)

    table_ready = 0x72C25100
    data = bytearray(0x900)
    off = 0x100
    struct.pack_into("<I", data, off, 2 if ready else 0)
    table = [0x7AC427D0] * 30
    for slot in (0, 6, 20, 21):
        table[slot] = 0x72B65000 + slot * 0x100
    struct.pack_into("<30I", data, off + 4, *table)
    struct.pack_into("<I", data, off + 4 + 30 * 4 + 8, 0x7AC428C0)
    if hwnd:
        struct.pack_into("<I", data, off + 0x3A8, 0x00100478)
    stack = bytearray(0x20)
    struct.pack_into("<I", stack, 8, 0x004FF9CF)
    memory = [
        (0x72C25000, bytes(data)),
        (0x001AF034, bytes(stack)),
        (0x00503AF7, bytes.fromhex("8B 08 FF 51 48")),
        (0x006BBF1C, bytes(4)),
        (0x006B7920, bytes(4)),
    ]
    mr = append(bytes(4 + 16 * len(memory)))
    struct.pack_into("<I", b, mr, len(memory))
    for i, (address, captured) in enumerate(memory):
        rva = append(captured)
        struct.pack_into("<QII", b, mr + 4 + 16 * i,
                         address, len(captured), rva)

    ex = append(bytes(168))
    ctx = bytearray(716)
    for at, val in (
        (0x9C, 0x80004005), (0xA0, 0x00100478),
        (0xB0, 0), (0xB8, 0x00503AF7), (0xC4, 0x001AF034),
    ):
        struct.pack_into("<I", ctx, at, val)
    ctxrva = append(ctx)
    struct.pack_into("<I", b, ex, 0x6AEC)
    struct.pack_into("<I", b, ex + 8, 0xC0000005)
    struct.pack_into("<Q", b, ex + 24, 0x00503AF7)
    struct.pack_into("<I", b, ex + 32, 2)
    struct.pack_into("<QQ", b, ex + 40, 0, fault)
    struct.pack_into("<II", b, ex + 160, len(ctx), ctxrva)

    b[:4] = b"MDMP"
    struct.pack_into("<II", b, 8, 3, 32)
    for i, (typ, offset, size) in enumerate(
        ((4, m, 4 + 108 * len(modules)),
         (5, mr, 4 + 16 * len(memory)), (6, ex, 168))
    ):
        struct.pack_into("<III", b, 32 + i * 12, typ, size, offset)
    return bytes(b)


def test_known_crash_hook_backend_and_hwnd_entered():
    mod = module()
    result = mod.inspect(fake_dump())
    assert result["known_crash"]
    assert result["primary"] == result["device"] == 0
    assert result["caller"] == 0x4FF9CF
    table = result["vtable"]
    assert table["ready"] == 0x72C25100
    assert table["hook"] == 0x72B66400
    assert table["original"] == 0x7AC428C0
    assert table["module"] == "apphelp.dll"
    assert table["hwnd_addrs"] == [0x72C254A8]
    assert table["mode_calls"] == 0
    output = mod.render(result)
    assert "entry observed" in output
    assert "module=apphelp.dll" in output
    assert "C:\\Synthetic" not in output


def test_no_false_positive_for_missing_native_hwnd_or_unarmed_hook():
    mod = module()
    x = mod.inspect(fake_dump(hwnd=False))
    assert x["known_crash"]
    assert not x["vtable"]["hwnd_addrs"]
    assert "not established" in mod.render(x)
    x = mod.inspect(fake_dump(ready=False))
    assert x["vtable"] is None
    assert "No known patched" in mod.render(x)


def test_no_root_cause_attribution_for_different_exception():
    mod = module()
    x = mod.inspect(fake_dump(fault=0xBAD0))
    assert not x["known_crash"]
    assert "NOT MATCHED" in mod.render(x)


def test_truncated_and_invalid_dump_refused():
    mod = module()
    with pytest.raises(mod.InvalidMiniDump):
        mod.inspect(b"this is not a minidump")
    with pytest.raises(mod.InvalidMiniDump):
        mod.inspect(fake_dump()[:80])


def test_offline_tool_does_not_write_private_files_or_run_windows():
    source = SOURCE.read_text(encoding="utf-8")
    assert "read_bytes()" in source
    assert ".write_bytes" not in source
    assert ".write_text" not in source
    assert "subprocess" not in source
    assert "win32api" not in source
    assert "apphelp.dll" not in source  # discovered from module table
    assert "0x394, 0x3A8" in source
