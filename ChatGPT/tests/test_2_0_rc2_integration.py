"""2.0 RC2 must contain BOTH SDL3 improvements, not only a version bump.
CI source regression does not replace live game hardware acceptance.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]


def test_combined_rc2_contains_save_confirmation_and_duplicate_instances():
    ver=(ROOT/"version_info.h").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    universal=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    policy=(ROOT/"sdl3_universal_policy.h").read_text(encoding="utf-8")
    identity=(ROOT/"sdl3_gamepad_identity.h").read_text(encoding="utf-8")
    assert '#define SPIDERMAN2K1FIX_VERSION "2.0.0-rc.2"' in ver
    assert "spider_sdl3_selected_instance_valid(" in identity
    assert "spider_sdl3_selected_model_unique(" in identity
    assert "spider_sdl3_start_claim_add(" in probe
    assert "start_claims.count==1u" in probe
    assert "spider_sdl3_selected_instance_valid(opened,num_pads," in probe
    assert "native_shell_menu=native_shell_menu_context(" in universal
    assert "if(native_shell_menu && context==SPIDER_UNIVERSAL_CTX_3D)" in universal
    assert "spider_sdl3_native_shell_menu_candidate(" in policy
    assert "spider_sdl3_menu_confirm_defer_keyup(" in policy
    assert 'pulse(0x1cu,"menu Confirm")' in universal
