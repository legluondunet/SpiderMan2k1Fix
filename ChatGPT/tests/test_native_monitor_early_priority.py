"""Native monitor priority: capture before game virtual config load.

These source-level invariants supplement in-game multi-monitor verification.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
MAIN=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
CFG=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
POLICY=(ROOT/"display_policy.c").read_text(encoding="utf-8")
MENU=(ROOT/"display_menu_lock.c").read_text(encoding="utf-8")


def test_native_game_window_monitor_is_captured_before_game_config_bootstrap():
    create=MAIN[MAIN.index("static HRESULT WINAPI hooked_create(GUID"):MAIN.index(
        "static void install_ddraw_hook(",MAIN.index("static HRESULT WINAPI hooked_create(GUID"))]
    assert "spider_find_game_window_before_ddraw()" in create
    assert "probe_desktop_mode(early);" in create
    assert "SpiderCfg_BootstrapGameRead();" in create
    assert create.index("probe_desktop_mode(early);") < create.index("SpiderCfg_BootstrapGameRead();")
    assert "EnumThreadWindows(GetCurrentThreadId(),spider_find_early_window" in MAIN
    assert "EnumWindows(spider_find_early_window,(LPARAM)(uintptr_t)&state)" in MAIN
    assert "GetWindowThreadProcessId(window,&pid)" in MAIN
    assert "pid!=GetCurrentProcessId()" in MAIN
    assert "GetActiveWindow()" in MAIN


def test_selected_monitor_resolution_not_guessed_from_primary_only():
    probe=MAIN[MAIN.index("static void probe_desktop_mode(HWND hwnd)"):MAIN.index(
        "static void log_monitor(HWND hwnd)")]
    assert "MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST)" in probe
    assert "GetMonitorInfoA(monitor,(MONITORINFO*)&info)" in probe
    assert "EnumDisplaySettingsA(info.szDevice,ENUM_CURRENT_SETTINGS,&mode)" in probe
    assert "mode.dmPelsWidth<640 || mode.dmPelsHeight<480" in probe
    assert "desktop_w=mapping.output_w;" in probe
    assert "desktop_h=mapping.output_h;" in probe
    assert "SpiderCfg_ObserveNativeMonitor(mode.dmPelsWidth,mode.dmPelsHeight)" in probe
    assert "SpiderDisplayMenu_SetTarget(selected_w,selected_h)" in probe
    assert "InterlockedCompareExchange(&borderless_legacy_mode_calls,0,0)==0" in probe
    assert "desktop_w!=mode.dmPelsWidth || desktop_h!=mode.dmPelsHeight" in probe
    assert "if(!SpiderMenu_MakeMapping(" in probe


def test_physical_mode_never_switched_and_borderless_monitor_used():
    coop=MAIN[MAIN.index("static HRESULT WINAPI hooked_coop("):MAIN.index(
        "static HRESULT WINAPI hooked_create(GUID")]
    assert "probe_desktop_mode(hwnd);" in coop
    assert "hr=original_coop(self,hwnd,DDSCL_NORMAL);" in coop
    assert "hr=DD_OK;" in coop
    assert "original_mode(self," not in coop
    assert "ChangeDisplaySettings" not in MAIN
    borderless=MAIN[MAIN.index("static void borderless_window_apply("):MAIN.index(
        "static HRESULT borderless_get_back(")]
    assert "MonitorFromWindow(game_hwnd,MONITOR_DEFAULTTONEAREST)" in borderless
    assert "r=mi.rcMonitor;" in borderless
    assert "SetWindowPos(game_hwnd,HWND_NOTOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top," in borderless


def test_classic_43_is_logical_pillarbox_not_physical_mode_change():
    probe=MAIN[MAIN.index("static void probe_desktop_mode(HWND hwnd)"):MAIN.index(
        "static void log_monitor(HWND hwnd)")]
    assert "game_output_w=physical_w;game_output_h=physical_h;" in probe
    assert "game_logical_w=selected_w;game_logical_h=selected_h;" in probe
    assert "SpiderMenu_MakeMapping((int)selected_w,(int)selected_h" in probe
    assert "InterlockedExchange(&gameplay_4x3_enabled,1);" in probe
    assert "return SpiderDisplayPolicy_ResolveAspect(resolution,aspect,native_width,native_height,width,height);" in POLICY
    assert 'strcpy(configured_resolution,"Auto");' in CFG
    assert '"Resolution=Auto\\r\\n"' not in CFG


def test_win11_wine_logs_and_original_game_mode_guard():
    assert "DISPLAY-PROBE: original monitor=" in MAIN
    assert "DISPLAY-POLICY: monitor=" in MAIN
    assert "DISPLAY-NATIVE: pre-bootstrap game HWND identified" in MAIN
    assert "DISPLAY-NATIVE: WARNING game HWND unavailable" in MAIN
    assert "DISPLAY-MENU-SAFE: BLOCKED physical 640x480x16" in MAIN
    assert "DISPLAY-MENU-SAFE: INTERCEPT 640x480x16" in MAIN
    assert "DISPLAY-MENU-LOCK:" in MENU
    assert "SpiderDisplayPolicy_Enforce(cfg,CFG_SIZE,policy_width,policy_height)" in CFG
    assert "SpiderCfg_BootstrapGameRead(void)" in CFG
