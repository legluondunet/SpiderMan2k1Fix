#!/usr/bin/env python3
"""Read-only original Spider-Man (2001) DirectDraw failure minidump triage.

The two private 2026-11-14 native Windows WER dumps show identical
SetCooperativeLevel E_FAIL -> original native audio Stop(NULL) failures.
This script reads standard MINIDUMP module, memory and x86 exception
streams using only Python 3 stdlib; it never publishes raw dump memory,
writes files, loads Wine, hooks processes or modifies any game binary.

Example:
  python3 ChatGPT/tools/inspect_spiderman_ddraw_dump.py SpideyPC.exe.24668.dmp

Only two exact known original DLL data layouts (ready->game_hwnd offsets
0x394 and 0x3A8) qualify as evidence of actual hooked_coop entry.
An unfamiliar build never receives a false affirmative.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import struct


class InvalidMiniDump(ValueError):
    pass


def u32(buf: bytes, off: int) -> int:
    if off < 0 or off + 4 > len(buf):
        raise InvalidMiniDump("truncated DWORD")
    return struct.unpack_from("<I", buf, off)[0]


def u64(buf: bytes, off: int) -> int:
    if off < 0 or off + 8 > len(buf):
        raise InvalidMiniDump("truncated QWORD")
    return struct.unpack_from("<Q", buf, off)[0]


class Dump:
    def __init__(self, b: bytes):
        self.b = b
        if len(b) < 32 or b[:4] != b"MDMP":
            raise InvalidMiniDump("invalid MDMP signature")
        count, directory = u32(b, 8), u32(b, 12)
        if not 1 <= count <= 512 or directory + 12 * count > len(b):
            raise InvalidMiniDump("invalid stream directory")
        streams = {}
        for i in range(count):
            kind, size, addr = struct.unpack_from("<III", b, directory + 12 * i)
            if kind and addr + size <= len(b):
                streams[kind] = (addr, size)
        if not all(k in streams for k in (4, 5, 6)):
            raise InvalidMiniDump("missing ModuleList, MemoryList or ExceptionStream")
        self.modules = []
        r, size = streams[4]
        n = u32(b, r)
        if n > 1024 or 4 + n * 108 > size:
            raise InvalidMiniDump("invalid ModuleList")
        for i in range(n):
            pos = r + 4 + i * 108
            base, nbytes, _, _, name_rva = struct.unpack_from("<QIIII", b, pos)
            length = u32(b, name_rva)
            if length > 4096 or length & 1 or name_rva + 4 + length > len(b):
                raise InvalidMiniDump("invalid module name")
            name = b[name_rva + 4:name_rva + 4 + length].decode("utf-16le")
            self.modules.append((base, base + nbytes,
                                 name.replace("/", "\\").rsplit("\\", 1)[-1]))
        r, size = streams[5]
        n = u32(b, r)
        if n > 100000 or 4 + n * 16 > size:
            raise InvalidMiniDump("invalid MemoryList")
        self.ranges = []
        for i in range(n):
            start, length, ptr = struct.unpack_from("<QII", b, r + 4 + i * 16)
            if ptr + length > len(b):
                raise InvalidMiniDump("invalid captured memory RVA")
            self.ranges.append((start, start + length, ptr))
        r, size = streams[6]
        if size < 168:
            raise InvalidMiniDump("truncated ExceptionStream")
        ctxlen, ctxoff = u32(b, r + 160), u32(b, r + 164)
        if ctxlen < 0xCC or ctxoff + ctxlen > len(b):
            raise InvalidMiniDump("unsupported x86 CONTEXT")
        ctx = b[ctxoff:ctxoff + ctxlen]
        self.exc = dict(
            thread=u32(b, r), code=u32(b, r + 8),
            address=u64(b, r + 24),
            fault=u64(b, r + 48) if u32(b, r + 32) > 1 else None,
            edi=u32(ctx, 0x9C), esi=u32(ctx, 0xA0),
            eax=u32(ctx, 0xB0), eip=u32(ctx, 0xB8),
            esp=u32(ctx, 0xC4))

    def get(self, address: int, size: int) -> bytes | None:
        for low, high, ptr in self.ranges:
            if low <= address and address + size <= high:
                off = ptr + address - low
                return self.b[off:off + size]
        return None

    def read32(self, address: int) -> int | None:
        buf = self.get(address, 4)
        return u32(buf, 0) if buf is not None else None

    def owner(self, address: int) -> str | None:
        return next((name for low, high, name in self.modules
                     if low <= address < high), None)


def inspect(data: bytes) -> dict:
    d = Dump(data)
    e = d.exc
    core = next(((lo, hi) for lo, hi, name in d.modules
                 if name.casefold() == "spiderman2k1fix.dll"), None)
    if core is None:
        raise InvalidMiniDump("SpiderMan2k1Fix.dll not loaded")
    caller = d.read32(e["esp"] + 8)
    match = (e["code"] == 0xC0000005 and e["address"] == e["eip"] == 0x00503AF7
             and e["eax"] == 0 and e["edi"] == 0x80004005 and e["fault"] == 0
             and caller == 0x004FF9CF and
             d.get(0x00503AF7, 5) == bytes.fromhex("8B 08 FF 51 48"))
    candidates = []
    for lo, hi, ptr in d.ranges:
        if not core[0] <= lo < core[1]:
            continue
        raw = d.b[ptr:ptr + hi - lo]
        for i in range(0, max(0, len(raw) - 140), 4):
            if u32(raw, i) != 2:  # native ddraw_vtable_ready sentinel
                continue
            table = struct.unpack_from("<30I", raw, i + 4)
            if not all(core[0] <= table[index] < core[1]
                       for index in (0, 6, 20, 21)):
                continue
            original = u32(raw, i + 4 + 30 * 4 + 8)
            module = d.owner(original)
            if module is None:
                continue
            # Match the exact supported layouts, not any coincidental HWND
            # stored in some other subsystem or a future unrelated build.
            hwnd_addrs = [lo + i + offset for offset in (0x394, 0x3A8)
                          if i + offset + 4 <= len(raw)
                          and u32(raw, i + offset) == e["esi"]]
            candidates.append(dict(
                ready=lo + i, hook=table[20], original=original,
                module=module, hwnd_addrs=hwnd_addrs,
                mode_calls=d.read32(lo + i + 4 + 30 * 4 + 16)))
    if len(candidates) > 1:
        raise InvalidMiniDump("ambiguous patched DirectDraw vtable")
    return dict(known_crash=match, exception=e,
                primary=d.read32(0x006BBF1C), device=d.read32(0x006B7920),
                caller=caller, vtable=candidates[0] if candidates else None)


def render(out: dict) -> str:
    e, v = out["exception"], out["vtable"]
    lines = [
        "Spider-Man 2001 DirectDraw/DirectSound WER (read-only, no private paths)",
        f"Exception code=0x{e['code']:08X} EIP=0x{e['eip']:08X} read_at={e['fault']}",
        f"EDI=0x{e['edi']:08X} ESI=0x{e['esi']:08X} EAX=0x{e['eax']:08X}",
        f"Primary DirectSound buffer={out['primary']}; DirectSound object={out['device']}",
        f"Original caller return={hex(out['caller']) if out['caller'] is not None else 'unavailable'}",
        f"Two-WER original startup crash signature={'MATCH' if out['known_crash'] else 'NOT MATCHED'}",
    ]
    if v:
        lines += [
            f"Patched DirectDraw7 table prepared: ready=2 at 0x{v['ready']:08X}",
            f"Hook SetCooperativeLevel=0x{v['hook']:08X}",
            f"Saved native SetCooperativeLevel=0x{v['original']:08X} module={v['module']}",
            f"Graphics mode calls before crash={v['mode_calls']}",
            "hooked_coop game_hwnd state: " +
            ("matching game HWND; entry observed" if v["hwnd_addrs"] else
             "not established by this build"),
        ]
    else:
        lines.append("No known patched DirectDraw vtable recovered")
    lines.append("Cause of native E_FAIL is still unknown; no repair is applied.")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, help="SpideyPC.exe Windows Minidump")
    args = parser.parse_args()
    try:
        result = inspect(args.dump.read_bytes())
    except (OSError, ValueError, struct.error) as exc:
        parser.error(str(exc))
    print(render(result))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
