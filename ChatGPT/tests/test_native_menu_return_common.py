"""The native menu boundary is a shared engine concern, not a pad mapping.

F10-independent read-only instrumentation must preserve the exact original
Shell_MainMenu tick and must never dispatch a Quit or synthesize a key.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]
NATIVE=(ROOT/"native_menu_return_gate.h").read_text(encoding="utf-8")
SONY=(ROOT/"ps1_transition_edge_ownership.h").read_text(encoding="utf-8")
UI=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
MENU=(ROOT/"quick_exit_f12.c").read_text(encoding="utf-8")
INI=(ROOT/"ini_validation.h").read_text(encoding="utf-8")


def test_native_menu_gate_is_shared_with_xbox_and_legacy_sony_adapter():
    assert "SpiderNativeMenuReturnGate" in NATIVE
    assert "native_request==7u" in NATIVE
    assert "owner->inherited_buttons &= pressed_confirm_mask;" in NATIVE
    assert '#include "native_menu_return_gate.h"' in SONY
    assert "typedef SpiderNativeMenuReturnGate SpiderPS1TransitionEdgeOwnership;" in SONY
    assert "return spider_native_menu_return_gate_flip(" in SONY
    assert "static SpiderNativeMenuReturnGate xbox_transition_ownership={0};" in UI
    assert "spider_native_menu_return_gate_flip(" in UI
    assert "spider_ps1_transition_edge_ownership_flip(" in UI
    for forbidden in ("SendInput", "Sleep(", "GetTickCount(", "VirtualProtect",
                      "WriteProcessMemory", "DIJOYSTATE", "DWORD", "GUID"):
        assert forbidden not in NATIVE


def test_shared_native_gate_works_for_separate_physical_confirmation_indices(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"test_native_gate.c"
    exe=tmp_path/"test_native_gate"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "native_menu_return_gate.h"
      #include "ps1_transition_edge_ownership.h"
      int main(void) {
        SpiderNativeMenuReturnGate xbox={0};
        SpiderPS1TransitionEdgeOwnership sony={0};
        unsigned i;
        assert(!spider_native_menu_return_gate_flip(&xbox,1,0,1,0,0,1));
        assert(!spider_ps1_transition_edge_ownership_flip(&sony,1,0,1,0,0,2));
        assert(spider_native_menu_return_gate_flip(&xbox,1,1,0,0,7,1)==1);
        assert(spider_ps1_transition_edge_ownership_flip(&sony,1,1,0,0,7,2)==2);
        /* No fixed 8-frame limit. Distinct controllers share semantics. */
        for(i=0;i<240;i++) {
          assert(spider_native_menu_return_gate_flip(&xbox,1,1,0,0,7,1)==1);
          assert(spider_ps1_transition_edge_ownership_flip(&sony,1,1,0,0,7,2)==2);
        }
        assert(spider_native_menu_return_gate_flip(&xbox,1,1,0,0,7,0)==0);
        assert(spider_ps1_transition_edge_ownership_flip(&sony,1,1,0,0,7,0)==0);
        assert(!spider_native_menu_return_gate_flip(&xbox,1,1,0,0,7,1));
        assert(!spider_ps1_transition_edge_ownership_flip(&sony,1,1,0,0,7,2));
        /* Do not suppress on unrelated native transition or Bink. */
        assert(!spider_native_menu_return_gate_flip(&xbox,1,0,1,0,0,1));
        assert(!spider_native_menu_return_gate_flip(&xbox,1,1,0,0,8,1));
        assert(!spider_native_menu_return_gate_flip(&xbox,1,0,1,1,0,1));
        assert(!spider_native_menu_return_gate_flip(&xbox,1,1,0,1,7,1));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


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
