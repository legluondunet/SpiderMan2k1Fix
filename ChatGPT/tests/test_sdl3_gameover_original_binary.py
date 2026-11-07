"""Static source-level GAME OVER proof, without shipping the original EXE.

The maintainer's SHA-verified original binary was independently inspected
offline. CI uses pure code/structure tests only, and never downloads or
distributes proprietary executable bytes.
"""
from __future__ import annotations

import runpy
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[2]
INSPECTOR = ROOT / "ChatGPT/tools/inspect_spidey_gameover_owner.py"


def module():
    return runpy.run_path(str(INSPECTOR), run_name="spider_gameover_research")


def test_original_binary_audit_is_sha_and_pe32_gated():
    m = module()
    assert m["RETAIL_SIZE"] == 1507328
    assert m["RETAIL_SHA256"] == (
        "0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
    )
    assert m["IMAGE_BASE"] == 0x00400000
    assert m["SOURCE_PTR_ENTRY"] == 0x0054B748
    assert m["SOURCE_TEXT_VA"] == 0x0054D008
    with pytest.raises(m["InvalidExecutable"], match="size or SHA-256"):
        m["inspect"](b"MZ" + b"\0" * 200)
    src = INSPECTOR.read_text(encoding="utf-8")
    for forbidden in ("WriteProcessMemory(", "VirtualProtect(", "SendInput("):
        assert forbidden not in src


def test_original_owner_call_and_text_sites_are_explicit_and_verified():
    m = module()
    sig = m["SIGNATURES"]
    assert sig["native_gameover_screen_owner_call"] == (
        0x00455F91, "e8 9a 78 02 00"
    )
    assert sig["native_gameover_screen_seh_function_entry"][0] == 0x0047D830
    assert sig["controller_mode_two_sets_task_20"][0] == 0x00455F5E
    assert sig["mode_two_gameover_screen_draws_source_text"][0] == 0x0047D93A
    assert sig["mode_two_gameover_screen_polls_native_input"][0] == 0x0047D9B8
    assert sig["gameover_title_signed_hp_nonpositive"][0] == 0x00440BA5
    assert sig["gameover_title_phase_must_equal_one"][0] == 0x00440BAF
    assert sig["gameover_3d_death_requests_controller_mode_two"][0] == 0x004B28B3
    writers = [
        addr for name, (addr, _) in sig.items()
        if "mode_two_writer" in name or name == "gameover_3d_death_requests_controller_mode_two"
    ]
    assert set(writers) == {0x004459BD, 0x00455BE9, 0x004B28B3, 0x004BE027}
    assert len(sig) >= 20
    assert len({v[0] for v in sig.values()}) == len(sig)


def test_disassembly_does_not_claim_all_request_two_paths_are_game_over():
    source = (ROOT / "docs/references/SDL3_GAMEOVER_NATIVE_OWNER.md").read_text(
        encoding="utf-8")
    assert "request 2 is NOT a scene ID" in source
    assert "That CALL's execution is the ownership marker" in source
    assert "PASS" not in source.split("### Current acceptance status")[-1][:30]
    assert "experimental" in source.lower()
