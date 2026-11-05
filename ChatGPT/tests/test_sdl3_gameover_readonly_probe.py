"""Fail-closed, bounded, read-only original GAME OVER state correlation.

No GAME OVER detector or altered SDL3 controls are authorized by this probe.
The original 3D runtime may show a full-screen GAME OVER/RETRY/QUIT menu
while the 3D renderer remains active.  Hardware evidence is required to
identify a unique original engine state before routing South to Return.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "sdl3_universal_gameplay.c").read_text(encoding="utf-8")
POLICY = (ROOT / "sdl3_universal_policy.h").read_text(encoding="utf-8")


def test_gameover_probe_is_bounded_and_has_its_own_budget():
    assert "static void gameover_probe_snapshot(" in SOURCE
    assert "GetAsyncKeyState(VK_F9)" in SOURCE
    assert "gameover_probe_confirm_reports<12u" in SOURCE
    assert "gameover_probe_f9_reports<8u" in SOURCE
    assert "gameover_probe_confirm_edges%16u==0u" in SOURCE
    # The first Confirm immediately following each F9 must be sampled,
    # even after arbitrary normal 3D combat button presses.
    assert "gameover_probe_post_f9_confirm=1u;" in SOURCE
    assert "gameover_probe_post_f9_confirm=0;" in SOURCE
    assert "confirm_after_f9 ||" in SOURCE
    assert "!f9_edge && confirm_edge && gameover_probe_post_f9_confirm" in SOURCE
    assert "after-f9=%u" in SOURCE
    assert "SDL3-GAMEOVER-PROBE:" in SOURCE
    assert "[READ-ONLY; NOT A GAME-OVER DETECTOR]" in SOURCE
    assert "gameover_probe_snapshot(id,context,menu,gameplay,bink,native_pause," in SOURCE
    assert "gameover_probe_confirm_prev=gameover_probe_f9_prev=0;" in SOURCE
    assert "gameover_probe_owner!=owner" in SOURCE
    # The regular SDL3 event logger is only allowed 100 reports: the
    # late GAME OVER screen must still have an independent small budget.
    assert "if(report && trace++<100u)report(msg);" in SOURCE
    block = SOURCE.split("static void gameover_probe_snapshot(", 1)[1].split(
        "static void pulse(", 1
    )[0]
    assert "report(line);" in block


def test_probe_cannot_guess_gameover_or_change_menu_routing():
    assert "if(native_pause)return SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE;" in POLICY
    assert "if(menu || !gameplay)return SPIDER_UNIVERSAL_CTX_MENU;" in POLICY
    assert "return SPIDER_UNIVERSAL_CTX_3D;" in POLICY
    assert "context=spider_sdl3_universal_route_context(" in SOURCE
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in SOURCE
    assert "pulse(0x1cu,\"menu Confirm\")" in SOURCE
    assert "if(pause_edge)pulse(0x01u,\"3D Pause\");" in SOURCE
    block = SOURCE.split("static int gameover_probe_read_u32(", 1)[1].split(
        "static void pulse(", 1
    )[0]
    for prohibited in (
        "SendInput(", "send_scan(", "VirtualProtect(",
        "WriteProcessMemory(", "InterlockedExchangePointer(",
        "spider_sdl3_universal_step(", "update_keys(",
    ):
        assert prohibited not in block, prohibited
    # Read only retail documented scalar addresses; original pointer
    # fields are interpreted as presence, never dereferenced.
    for known in (
        "0x0060CFA4u", "0x0054A510u", "0x006B78F0u",
        "0x00682950u", "0x00682948u",
        "0x0068293Cu", "0x0054D38Cu",
    ):
        assert known in block
    assert "VirtualQuery(" in block
    assert "memcpy(out,(const void *)address,sizeof(*out));" in block
