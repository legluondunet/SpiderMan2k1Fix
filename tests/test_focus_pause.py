"""PR #2: focus pause is isolated from the base graphics and stability fixes."""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
INI=(ROOT/"SpiderMan2k1Fix.ini").read_text()
GEN=(ROOT/"virtual_spidey_cfg.c").read_text()
VALID=(ROOT/"ini_validation.h").read_text()

def test_pause_is_opt_in_and_configured_consistently():
    assert "PauseOnFocusLoss=0" in INI
    assert '"PauseOnFocusLoss=0\\r\\n"' in GEN
    assert '"PauseOnFocusLoss"' in VALID
    assert 'GetPrivateProfileIntA("Input","PauseOnFocusLoss",0,path)==1' in CORE

def test_pause_only_at_ddraw_flip_boundary():
    assert "static void pause_at_flip_if_background(void)" in CORE
    assert CORE.count("pause_at_flip_if_background();")==1
    start=CORE.index("static HRESULT WINAPI hooked_flip(")
    end=CORE.index("SpiderDI8Retry_OnFlip(count);",start)
    flip=CORE[start:end]
    assert flip.count("pause_at_flip_if_background();")==1
    assert flip.count("SpiderInputFocus_OnFlip(count);")==2
    assert flip.index("SpiderInputFocus_OnFlip(count);") < flip.index("pause_at_flip_if_background();")
    assert "if(was_background &&" in flip
    assert flip.rindex("SpiderInputFocus_OnFlip(count);") > flip.index("pause_at_flip_if_background();")
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)") : start]
    for needle in ["GetForegroundWindow()==hwnd", "PeekMessageA", "PostQuitMessage",
                   "TranslateMessage", "DispatchMessageA", "MsgWaitForMultipleObjectsEx"]:
        assert needle in pause
    assert "SuspendThread" not in pause

def test_no_native_input_hook_is_enabled_by_default():
    assert "ExperimentalAltTabReacquire=0" in INI
    assert '"ExperimentalAltTabReacquire=0\\r\\n"' in GEN

def test_persistent_pause_requires_manual_foreground_key():
    pause=CORE[CORE.index("static volatile LONG pause_on_focus_loss=0;"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "GetAsyncKeyState(pause_key_vk)" in pause
    assert "GetForegroundWindow()!=hwnd" in pause
    assert "pause_latched" in pause
    assert "pause_key_down" in pause
    assert 'while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)' in pause
    assert "while(IsWindow(hwnd) && GetForegroundWindow()!=hwnd)" not in pause
    assert "InterlockedExchange(&pause_latched,1)" in pause
    assert "PostQuitMessage" in pause
    assert "SuspendThread" not in pause

def test_focus_loss_filters_click_activation_glitches():
    focus=CORE[CORE.index("static BOOL pause_confirmed_focus_loss(HWND hwnd)"):
               CORE.index("static void pause_poll_key(HWND hwnd)")]
    assert "GetWindowThreadProcessId(hwnd,&game_pid)" in focus
    assert "GetWindowThreadProcessId(fg,&fg_pid)" in focus
    assert "fg_pid==game_pid" in focus
    assert "pause_background_since=0" in focus
    assert "(DWORD)(now-pause_background_since)>=150" in focus
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "pause_confirmed_focus_loss(hwnd)" in pause
    assert "GetAsyncKeyState(VK_LBUTTON)" not in pause

def test_pause_key_configurable_with_safe_default():
    assert "PauseKey=F9" in INI
    assert '"PauseKey"' in VALID
    assert "GetPrivateProfileStringA(\"Input\",\"PauseKey\",\"F9\"" in CORE
    assert 'lstrcmpiA(key,"Space")==0' in CORE
    assert "VK_SPACE:VK_F9" in CORE

def test_focus_trace_only_does_not_hook_directinput():
    trace=(ROOT/"input_focus_trace.c").read_text()
    safe=trace.index("if(want_trace && !want_fix)")
    hook=trace.index("slot=(void**)(base+0x13b01c)")
    assert safe < hook
    assert "InterlockedExchange(&diagnostic_active,1)" in trace[safe:hook]
    assert "return;" in trace[safe:hook]
    assert "DirectInput8 IAT and COM vtables untouched" in trace

def test_pause_key_ini_value_validation_precedes_boolean_input_rules():
    specific=VALID.index('if(!spider_ini_icmp(section,"Input") && !spider_ini_icmp(key,"PauseKey"))')
    boolean=VALID.index('if(!spider_ini_icmp(section,"Debug")||!spider_ini_icmp(section,"Input")||')
    assert specific < boolean
    assert 'spider_ini_icmp(value,"F9")' in VALID[specific:boolean]
    assert 'spider_ini_icmp(value,"Space")' in VALID[specific:boolean]
    assert '"PauseKey=F9\\r\\n"' in GEN
