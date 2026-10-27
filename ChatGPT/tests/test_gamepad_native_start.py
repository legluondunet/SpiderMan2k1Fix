"""Tests for guarded, opt-in original-game controller Start binding.

The helper is compiled and executed (rather than only source-grepped).
Hardware interpretation of BUTTON8/9 and whether native Start completes
a particular menu/movie are intentionally left for Wine/Windows testing.
"""
from pathlib import Path
import ctypes
import shutil
import struct
import subprocess

import pytest

ROOT = Path(__file__).resolve().parents[2]
NATIVE = (ROOT / "gamepad_start_native.c").read_text(encoding="utf-8")
MAIN = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
BUILD = (ROOT / "build.sh").read_text(encoding="utf-8")
INI = (ROOT / "ini_validation.h").read_text(encoding="utf-8")

FACTORY = [0x4000] * 4 + [3, 4, 0, 1, 6, 9, 7, 11, 0x4000, 0]
ACTIONS = [1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100,
           0x200, 0x400, 0x1000, 0x2000, 0x8000]


def sample(buttons=FACTORY):
    return b"".join(struct.pack("<II", action, button)
                    for action, button in zip(ACTIONS, buttons))


@pytest.fixture(scope="module")
def mapping(tmp_path_factory):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler unavailable")
    folder = tmp_path_factory.mktemp("gamepad-map")
    src = folder / "probe.c"
    src.write_text('''
#include "gamepad_start_mapping.h"
int recognized(const unsigned char *buf,unsigned length) {
    return SpiderGamepad_MappingRecognized(buf,length);
}
int available(const unsigned char *buf,unsigned length,unsigned button) {
    return SpiderGamepad_StartButtonAvailable(buf,length,button);
}
''', encoding="utf-8")
    out = folder / "probe.so"
    subprocess.run(
        ["gcc", "-std=c11", "-Wall", "-Wextra", "-Werror",
         "-O2", "-fPIC", "-shared", "-I", str(ROOT),
         str(src), "-o", str(out)],
        check=True, capture_output=True, text=True,
    )
    dll = ctypes.CDLL(str(out))
    for name in ("recognized", "available"):
        fn = getattr(dll, name)
        fn.argtypes = [ctypes.POINTER(ctypes.c_ubyte),
                       ctypes.c_uint, ctypes.c_uint] if name == "available" else [
                           ctypes.POINTER(ctypes.c_ubyte), ctypes.c_uint]
        fn.restype = ctypes.c_int
    return dll


def call(fn, raw, *rest):
    buffer = (ctypes.c_ubyte * len(raw)).from_buffer_copy(raw)
    return fn(buffer, len(raw), *rest)


def test_original_input_layout_and_safe_unmapped_button(mapping):
    raw = sample()
    assert len(raw) == 112
    assert call(mapping.recognized, raw) == 1
    assert call(mapping.available, raw, 8) == 1
    assert call(mapping.available, raw, 9) == 0  # assigned to Zipline
    assert call(mapping.available, raw, 11) == 1  # factory Start
    assert call(mapping.available, raw, 32) == 0


def test_custom_gameplay_bindings_preserve_their_buttons(mapping):
    for index in range(4, 11):
        custom = FACTORY.copy()
        custom[index] = 8
        assert call(mapping.recognized, sample(custom)) == 1
        assert call(mapping.available, sample(custom), 8) == 0


def test_mismatched_action_table_or_start_is_rejected(mapping):
    different = ACTIONS.copy()
    different[11] = 0x800
    raw = b"".join(struct.pack("<II", a, b)
                   for a, b in zip(different, FACTORY))
    assert call(mapping.recognized, raw) == 0
    changed = FACTORY.copy()
    changed[11] = 8
    assert call(mapping.recognized, sample(changed)) == 0
    changed = FACTORY.copy()
    changed[12] = 3
    assert call(mapping.recognized, sample(changed)) == 0
    assert call(mapping.recognized, sample()[:100]) == 0


def test_runtime_opt_in_does_not_hook_directinput_or_spoof_keyboard():
    assert "ExperimentalGamepadStart" in NATIVE
    assert "ExperimentalGamepadStart" in INI
    assert "SpiderGamepadStart_Init(input_ini,log_line);" in MAIN
    # The original game's config reload restores joystick settings; touching
    # Start earlier in initialize_body() makes the change disappear.
    owner = MAIN.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "static void install_ddraw_hook(", 1
    )[0]
    assert owner.index("SpiderCfg_BootstrapGameRead();") < owner.index(
        "SpiderGamepadStart_Init(input_ini,log_line);"
    ) < owner.index("original_create(guid,out,iid,outer)")
    assert "SpiderGamepadStart_Init(" not in MAIN.split(
        "static DWORD initialize_body(", 1
    )[1].split("static BOOL CALLBACK core_initialize_once(", 1)[0]
    assert "gamepad_start_native.c" in BUILD
    # A fresh log must make every attempted Start override traceable;
    # otherwise a missing INI key looks exactly like an unsuccessful hook.
    for status in ("GAMEPAD-START: OFF", "GAMEPAD-START: REQUESTED",
                   "GAMEPAD-START: SCAN", "GAMEPAD-START: SKIPPED",
                   "GAMEPAD-START: expected two verified controller maps"):
        assert status in NATIVE
    assert "g_start_init_attempted" in NATIVE
    assert "found!=2u" in NATIVE
    assert "PAGE_GUARD" in NATIVE and "VirtualQuery" in NATIVE
    assert "supported_exe()" in MAIN
    for forbidden in ("SendInput(", "keybd_event(", "SetWindowsHookEx",
                      "SetKeyboardState(", "DirectInput8Create(", "XInputSetState("):
        assert forbidden not in NATIVE
