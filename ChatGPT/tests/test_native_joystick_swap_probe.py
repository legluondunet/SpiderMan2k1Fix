"""Legacy native joystick table probes are retired with the rebind backend."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_only_sdl3_controller_profiles_are_mutable():
    for obsolete in ("native_joystick_swap_probe.h",
                     "native_joystick_reconnect_policy.h",
                     "native_joystick_reconnect.c"):
        assert not (ROOT/obsolete).exists()
    controller=(ROOT/"sdl3_controller_profile_store.c").read_text(encoding="utf-8")
    adapter=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "SpiderSDL3ProfileStore_Save" in controller
    assert "SpiderSDL3ProfileStore_Load" in controller
    assert "spider_sdl3_universal_step" in adapter
    assert "spider_native_swap_read(" not in adapter
    assert "SPIDER_NATIVE_ACTION_TABLE" not in adapter
    assert "IDirectInput8_CreateDevice(" not in adapter


def test_no_native_reconnect_in_compilation_or_game_loop():
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert "native_joystick_reconnect.c" not in build
    assert "SpiderNativeReconnect" not in core + probe
    assert "native_joystick_unplug.c" in build  # crash guard remains
