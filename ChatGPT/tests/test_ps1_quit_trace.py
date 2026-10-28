"""Regression tests for passive Quit->Yes native 3D selection tracing.

The original PC game has several native transition owners. We must
never force Quit, menu cursor, restart, engine requests, or mouse state
to 'fix' an unconfirmed user-report cause.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
PAD=(ROOT/"gamepad_ui_profile.c").read_text()
INI=(ROOT/"ini_validation.h").read_text()
CFG=(ROOT/"virtual_spidey_cfg.c").read_text()


def test_telemetry_is_explicit_and_never_enabled_by_default():
    assert '"ControllerPS1QuitTrace"' in INI
    assert 'GetPrivateProfileIntA("Input","ControllerPS1QuitTrace",0,ini)==1' in PAD
    assert "ps1_quit_trace_requested=0" in PAD
    assert "ControllerPS1QuitTrace=1" not in CFG
    assert "if(!ps1_preview_active || !ps1_quit_trace_requested ||" in PAD
    assert "ps1_quit_trace_count>=130u" in PAD


def test_native_pause_choices_read_only_and_page_bounded():
    assert "0x00682968u" in PAD   # original confirmation object pointer
    assert "0x00682960u" in PAD   # original secondary pause object
    assert "0x00682950u" in PAD   # original native pause context
    assert "0x0060CFA4u" in PAD   # original transition REQUEST (READ ONLY)
    assert "0x02E0988Cu" in PAD   # original game's mouse-off bit
    assert "VirtualQuery((LPCVOID)va,&mbi,sizeof(mbi))" in PAD
    assert "mbi.State!=MEM_COMMIT" in PAD
    assert "(mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS))" in PAD
    assert "mbi.RegionSize-(va-base)<0x15u" in PAD
    assert "const volatile BYTE *)(va+0x14u)" in PAD
    assert "*selection=(int)" in PAD
    assert "SPIDER_CONFIRM_CROSS" in PAD
    assert "SPIDER_BACK_CIRCLE" in PAD
    assert "GAMEPAD-PS1: QUIT-TRACE" in PAD
    assert "mouse-off=%d CROSS-edge=%d CIRCLE-edge=%d SHARE-edge=%d" in PAD
    assert "PASSIVE PER-FLIP" in PAD


def test_no_native_pause_or_menu_writes_in_trace_functions():
    start=PAD.index("static int ps1_read_native_selection(")
    stop=PAD.index("void SpiderGamepadUI_OnFlip(",start)
    block=PAD[start:stop]
    for forbidden in (
        "VirtualProtect(", "InterlockedExchange(", "InterlockedCompareExchange(",
        "SendInput(", "begin_scancode(", "memcpy(", "WriteProcessMemory(",
        "PostMessage(", "SendMessage(", "SetCursorPos(", "ShowCursor(",
    ):
        assert forbidden not in block
    assert "*(volatile DWORD *)(uintptr_t)0x0060CFA4u=" not in block
    assert "*(volatile BYTE *)(uintptr_t)0x00682968u=" not in block
    assert "ps1_trace_native_quit(menu_2d,gameplay_3d,native_pause_active" in PAD
