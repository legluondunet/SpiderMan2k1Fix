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
    assert "DirectInputReacquire=0" in INI
    assert '"DirectInputReacquire=0\\r\\n"' in GEN
    assert "ExperimentalAltTabReacquire=" not in INI
    assert '"ExperimentalAltTabReacquire=' not in GEN

def test_persistent_pause_requires_manual_foreground_key():
    pause=CORE[CORE.index("static volatile LONG pause_on_focus_loss=0;"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "SpiderPauseAny_Completed()" in pause
    assert "GetForegroundWindow()!=hwnd" in pause
    assert "pause_latched" in pause
    assert "pause_key_down" not in pause
    assert 'while(IsWindow(hwnd) && InterlockedCompareExchange(&pause_latched,0,0)==1)' in pause
    assert "while(IsWindow(hwnd) && GetForegroundWindow()!=hwnd)" not in pause
    assert "InterlockedExchange(&pause_latched,1)" in pause
    assert "PostQuitMessage" in pause
    assert "SuspendThread" not in pause

def test_focus_loss_filters_click_activation_glitches():
    focus=CORE[CORE.index("static BOOL pause_confirmed_focus_loss(HWND hwnd)"):
               CORE.index("/* Wake only with the foreground Any-key hook. */")]
    assert "GetWindowThreadProcessId(hwnd,&game_pid)" in focus
    assert "GetWindowThreadProcessId(fg,&fg_pid)" in focus
    assert "fg_pid==game_pid" in focus
    assert "pause_background_since=0" in focus
    assert "(DWORD)(now-pause_background_since)>=150" in focus
    pause=CORE[CORE.index("static void pause_at_flip_if_background(void)"):
               CORE.index("static HRESULT WINAPI hooked_flip(")]
    assert "pause_confirmed_focus_loss(hwnd)" in pause
    assert "GetAsyncKeyState(VK_LBUTTON)" not in pause

def test_only_any_key_resume_and_retired_ini_modes_inert():
    assert "PauseKey=" not in INI
    assert '"PauseKey"' in VALID  # legacy INI accepted but ignored
    assert "PauseKey" not in CORE.replace("Historical PauseKey selections in older INIs are ignored: Any only.","")
    assert 'SpiderPauseAny_Begin(hwnd)' in CORE
    assert 'SpiderPauseAny_Completed()' in CORE
    assert 'SpiderPauseAny_DiscardGameKeyMessage(&msg,hwnd)' in CORE
    assert 'VK_F9' not in CORE
    assert 'pause_poll_key' not in CORE


def test_focus_trace_only_does_not_hook_directinput():
    trace=(ROOT/"input_focus_trace.c").read_text()
    safe=trace.index("if(want_trace && !want_fix)")
    hook=trace.index("slot=(void**)(base+0x13b01c)")
    assert safe < hook
    assert "InterlockedExchange(&diagnostic_active,1)" in trace[safe:hook]
    assert "return;" in trace[safe:hook]
    assert "DirectInput8 IAT and COM vtables untouched" in trace

def test_legacy_pause_key_value_validation_keeps_old_inis_safe():
    specific=VALID.index('if(!spider_ini_icmp(section,"Input") && !spider_ini_icmp(key,"PauseKey"))')
    boolean=VALID.index('if(!spider_ini_icmp(section,"Debug")||!spider_ini_icmp(section,"Input")||')
    assert specific < boolean
    assert 'spider_ini_icmp(value,"F9")' in VALID[specific:boolean]
    assert 'spider_ini_icmp(value,"Space")' in VALID[specific:boolean]
    assert 'spider_ini_icmp(value,"Any")' in VALID[specific:boolean]
    assert '"PauseKey=' not in GEN


def test_resume_focus_diagnostic_does_not_mutate_focus():
    start=CORE.index("static void pause_log_resume_focus(HWND hwnd)")
    end=CORE.index("static void pause_at_flip_if_background",start)
    diagnostic=CORE[start:end]
    assert "GetGUIThreadInfo(tid,&gi)" in diagnostic
    assert "resume-focus hwnd=" in diagnostic
    assert "SetForegroundWindow" not in diagnostic
    assert "SetFocus(" not in diagnostic
    assert "Acquire(" not in diagnostic
    assert "pause_log_resume_focus(hwnd);" in CORE


def test_di8_entry_probe_is_opt_in_and_factory_only():
    trace=(ROOT/"input_focus_trace.c").read_text()
    assert "ExperimentalDI8EntryTrace=0" in INI
    assert '"ExperimentalDI8EntryTrace=0\\r\\n"' in GEN
    assert '"ExperimentalDI8EntryTrace"' in VALID
    assert 'GetPrivateProfileIntA("Input","ExperimentalDI8EntryTrace",0,ini)==1' in trace
    assert "if(!want_trace) want_probe=0;" in trace
    assert "if(!want_probe) {" in trace
    assert "ENTRY-ONLY PROBE enabled" in trace
    factory=trace[trace.index("static HRESULT WINAPI focus_directinput8_create("):
                  trace.index("void SpiderInputFocus_Install(")]
    assert "original_directinput8_create(h,version,iid,out,outer)" in factory
    assert "InterlockedCompareExchange(&reacquire_enabled,0,0)==1" in factory
    assert "if(SUCCEEDED(hr) && out && *out &&" in factory
    assert "focus_instrument_device" not in factory
    assert "d->acquire(self)" not in factory



def test_create_device_probe_requires_existing_factory_probe():
    trace=(ROOT/"input_focus_trace.c").read_text()
    # Obsolete COM-vtable probe remains accepted for old users, but not offered.
    assert "ExperimentalDI8CreateDeviceTrace=" not in INI
    assert '"ExperimentalDI8CreateDeviceTrace=' not in GEN
    assert '"ExperimentalDI8CreateDeviceTrace"' in VALID
    assert 'GetPrivateProfileIntA("Input","ExperimentalDI8CreateDeviceTrace",0,ini)==1' in trace
    assert "if(!want_probe) want_interface_probe=0;" in trace
    assert "CREATEDEVICE PROBE enabled" in trace
    assert "InterlockedCompareExchange(&interface_probe_enabled,0,0)==1" in trace

def test_create_device_probe_does_not_replace_device_methods():
    trace=(ROOT/"input_focus_trace.c").read_text()
    start=trace.index("static HRESULT WINAPI focus_create_device(")
    end=trace.index("static HRESULT WINAPI focus_directinput8_create(",start)
    wrapper=trace[start:end]
    assert "original_interface_create(self,guid,out,outer)" in wrapper
    assert "InterlockedCompareExchange(&reacquire_enabled,0,0)==1" in wrapper
    assert "focus_instrument_device(*out,guid)" in wrapper
    assert "SetCooperativeLevel" not in wrapper
    assert "->acquire(" not in wrapper


def test_unsafe_di8_com_paths_are_quarantined_before_hook_install():
    trace=(ROOT/"input_focus_trace.c").read_text()
    guard=trace.index("Windows 11 regression confirmed 2026-10-21")
    disable_probe=trace.index("want_interface_probe=0;",guard)
    disable_recovery=trace.index("want_fix=0;",disable_probe)
    mode=trace.index("if(want_trace && !want_fix)",disable_recovery)
    assert guard < disable_probe < disable_recovery < mode
    assert "verified startup failure E_INVALIDARG 0x80070057" in trace
    assert "legacy AltTabReacquire COM hook DISABLED" in trace
    assert "ENTRY-ONLY PROBE enabled" in trace

