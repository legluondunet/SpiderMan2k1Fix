"""Passive native menu-return inspection is independent of controller presets.

The obsolete Sony/Xbox synthetic menu-return gate was removed with the
pre-SDL3 fixed physical button maps. Only the strictly read-only native
main-menu observer remains.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MENU=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
INI=(ROOT/"ini_validation.h").read_text(encoding="utf-8")


def test_readonly_native_probe_is_explicit_and_independent_from_f10():
    assert '"NativeMenuReturnProbe"' in INI
    assert 'GetPrivateProfileIntA("Debug","NativeMenuReturnProbe",0,ini)==1' in MENU
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)==1' in MENU
    assert "static volatile LONG native_menu_probe_only=0;" in MENU
    off=MENU.split('if(strcmp(val,"1")!=0) {',1)[1].split(
        '/* Shell_MainMenu has 8 entries.',1)[0]
    assert "install_real_menu_hook()" in off
    assert "install_debug_menu_wait_exit_probe()" in off
    assert "native_menu_probe_only,1" in off
    assert "SPIDER_SHELL_MENU_TICK,shell_menu_tick_original,5" in off
    assert "SPIDER_MENU_WAIT_EXIT_SITE,menu_wait_exit_original,5" in off
    assert "return;" in off  # F10 never enabled in passive-only path
    control=MENU.split("int __cdecl SpiderQuickExitF12_ConsumeRealMenuRequest(void) {",1)[1].split(
        "/* x86 stack at entry",1)[0]
    assert "if(InterlockedCompareExchange(&native_menu_probe_only,0,0)==1)return 0;" in control
    assert control.index("native_menu_probe_only,0,0)==1)return 0;") < control.index(
        "InterlockedCompareExchange(&pending,0,1)")
    assert "SPIDER_NATIVE_MENU_TICK" in MENU
    assert '"movl $0x00505720, %eax\\n\\t"' in MENU
    assert "NATIVE-MENU-RETURN-PROBE tick=" in control
    assert "selected action UNKNOWN" in control
    assert "native_menu_probe_reports,0,0)" in control
    assert "observed<128" in control


def test_native_menu_selected_action_probe_is_readonly_and_capped():
    observer=MENU.split("static void __cdecl spider_f12_debug_menu_wait(DWORD ticks,int which) {",1)[1].split(
        "static void __cdecl spider_f12_debug_menu_wait_cond",1)[0]
    assert "SPIDER_NATIVE_CLOCK_WAIT(ticks);" in observer
    assert "native_menu_probe_select_reports" in observer
    assert "<=64" in observer
    assert "SELECTED ACTION UNKNOWN" in observer
    for forbidden in ("SendInput", "TerminateProcess(", "ExitProcess(", "DestroyWindow(",
                      "SPIDER_GAME_QUIT_FLAG=", "SPIDER_ENGINE_MODE=", "PostMessageA("):
        assert forbidden not in observer
