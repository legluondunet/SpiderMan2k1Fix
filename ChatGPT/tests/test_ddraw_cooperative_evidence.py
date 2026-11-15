"""Read-only Windows DirectDraw cooperative-level startup evidence contracts.

Two native WER crashes proved E_FAIL came from SetCooperativeLevel before
DirectSound initialized. Preserve the original method and HRESULT. Snapshot
the HWND *before* the native method to avoid inferring its earlier state from
a handle that could have changed on failure. Compare success/failure
backend modules and top-level/window-thread constraints without retries.
"""
from pathlib import Path

SOURCE = (Path(__file__).resolve().parents[2] / "spiderman2k1fix.c").read_text(
    encoding="utf-8")
COOP = SOURCE.split("static HRESULT WINAPI hooked_coop(", 1)[1].split(
    "static HRESULT WINAPI hooked_mode(", 1)[0]


def test_original_coop_called_exactly_once_with_normal_and_original_hresult():
    assert COOP.count("hr=original_coop(self,hwnd,DDSCL_NORMAL);") == 1
    assert COOP.count("return hr;") == 1
    assert "DDSCL_EXCLUSIVE" not in COOP
    assert "Sleep(" not in COOP
    assert "MessageBox" not in COOP
    assert "hr=DD_OK" not in COOP


def test_window_evidence_is_snapshotted_before_native_coop():
    call = COOP.index("hr=original_coop(self,hwnd,DDSCL_NORMAL);")
    for token in (
        "pre_valid=(hwnd && IsWindow(hwnd))?TRUE:FALSE;",
        "pre_caller_tid=GetCurrentThreadId();",
        "pre_owner_tid=GetWindowThreadProcessId(hwnd,&pre_owner_pid);",
        "pre_style=GetWindowLongPtrA(hwnd,GWL_STYLE);",
        "pre_root=GetAncestor(hwnd,GA_ROOT);",
        "pre_owned=GetWindow(hwnd,GW_OWNER)!=NULL;",
    ):
        assert COOP.index(token) < call
    # If a COM call invalidates the HWND, we retain the actual pre-call
    # evidence rather than silently using post-call window attributes.
    assert COOP.index("COOP-SUCCESS-PRE") > call
    assert COOP.index("COOP-FAIL-PRE") > call
    assert COOP.count("pre_valid && pre_root==hwnd && !(pre_style&WS_CHILD)") == 2
    assert COOP.count("pre_owner_tid && pre_owner_tid==pre_caller_tid") == 2


def test_success_and_failure_context_include_actual_vtable_backend():
    assert "COOP-SUCCESS-CONTEXT" in COOP
    assert "COOP-FAIL-CONTEXT" in COOP
    assert "COOP-SUCCESS-PRE" in COOP
    assert "COOP-FAIL-PRE" in COOP
    assert "COOP-SUCCESS-BACKEND module=%s" in COOP
    assert "COOP-FAIL-BACKEND module=%s" in COOP
    assert COOP.count("VirtualQuery((LPCVOID)original_coop") == 2
    assert COOP.count("GetModuleFileNameA((HMODULE)mem.AllocationBase") == 2
    assert COOP.index("COOP-SUCCESS-BACKEND") < COOP.index(
        "borderless_window_apply();")
    assert COOP.index("COOP-FAIL-PRE") < COOP.index("COOP-FAIL-BACKEND")


def test_original_vtable_diagnostics_run_before_any_substitution():
    create = SOURCE.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "static void install_ddraw_hook(", 1)[0]
    assert create.index("void **vtable=*object;") < create.index(
        "ddraw_log_original_vtable(vtable);")
    assert create.index("ddraw_log_original_vtable(vtable);") < create.index(
        "*object=patched_ddraw_vtable;")
    # Four original COM method owners, not addresses or private DLL paths.
    origin = SOURCE.split("static void ddraw_native_method_module(", 1)[1].split(
        "static HRESULT WINAPI hooked_create(", 1)[0]
    for item in ("vtable[0]", "vtable[6]", "vtable[20]", "vtable[21]"):
        assert item in origin
    assert "DDRAW-STARTUP: COM-ORIGINS" in origin
    assert "GetModuleFileNameA" in origin
    assert "strrchr(module_path" in origin
    assert 'GetModuleHandleA("apphelp.dll")!=NULL' in origin
    assert 'GetEnvironmentVariableA("__COMPAT_LAYER",compat_probe' in origin
    assert "attempt>4" in origin
    assert "LoadLibrary" not in origin
    assert "VirtualProtect" not in origin
    assert "SetWindowLong" not in origin
    assert "original_coop(" not in origin


def test_late_legacy_iat_hook_preserves_early_winmm_gate():
    legacy = SOURCE.split("static void install_ddraw_hook(", 1)[1].split(
        "static void install_player_hud(", 1)[0]
    assert 'GetModuleHandleA("winmm.dll")' in legacy
    assert "VirtualQuery(*slot,&mbi,sizeof(mbi))" in legacy
    assert "mbi.Type==MEM_IMAGE" in legacy
    assert "mbi.AllocationBase==(void*)winmm_proxy" in legacy
    assert "redundant legacy hook skipped (expected)" in legacy
    assert "WARNING unexpected DirectDrawCreateEx IAT owner" in legacy
    # Expected mismatch returns BEFORE the legacy IAT assignment.
    assert legacy.index("redundant legacy hook skipped (expected)") < legacy.index(
        "return;\n  }\n  original_create=(CreateExFn)*slot;")
    assert legacy.index("return;\n  }\n  original_create=(CreateExFn)*slot;") < legacy.index(
        "*slot=(void*)hooked_create;")
    assert "IAT target mismatch; hook not installed" not in legacy


def test_native_appcompat_layers_observation_is_read_only_and_private():
    block = SOURCE.split("static int ddraw_compat_layer_entry(", 1)[1].split(
        "static void ddraw_log_original_vtable(", 1)[0]
    for token in (
        'L"Software\\\\Microsoft\\\\Windows NT\\\\CurrentVersion\\\\AppCompatFlags\\\\Layers"',
        "RegOpenKeyExW(hive,key_path,0,KEY_QUERY_VALUE|view,&key)",
        "RegQueryValueExW(key,exe_path,NULL,&type,NULL,&bytes)",
        "RegCloseKey(key)",
        "GetModuleFileNameW(NULL,exe,MAX_PATH)",
        "HKEY_CURRENT_USER,0,exe",
        "KEY_WOW64_32KEY,exe",
        "KEY_WOW64_64KEY,exe",
        "DDRAW-STARTUP: APPCOMPAT-LAYERS",
        "DDRAW-STARTUP: APPCOMPAT-MODULES",
        "system SDB not covered",
    ):
        assert token in block
    for module in (
        "apphelp.dll", "AcLayers.dll", "AcGenral.dll",
        "AcXtrnal.dll", "ShimEng.dll",
    ):
        assert f'GetModuleHandleA("{module}")' in block
    assert "RegSetValue" not in block
    assert "RegCreateKey" not in block
    assert "RegDelete" not in block
    assert "RegQueryValueExW(key,exe_path,NULL,&type,NULL,&bytes)" in block
    assert 'log_line(exe' not in block
    assert "LogFlags" not in block
    assert "DisableEngine" not in block
    assert "ShimFlushCache" not in block
    assert "LoadLibrary" not in block
    assert "if(attempt==1)ddraw_log_compat_evidence();" in SOURCE


def test_appcompat_probe_runs_on_directdraw_create_not_under_loader_lock():
    origin = SOURCE.split("static void ddraw_log_original_vtable(", 1)[1].split(
        "static HRESULT WINAPI hooked_create(", 1)[0]
    create = SOURCE.split("static HRESULT WINAPI hooked_create(", 1)[1].split(
        "static void install_ddraw_hook(", 1)[0]
    dllmain = SOURCE.split("BOOL WINAPI DllMain(", 1)[1]
    assert origin.index("DDRAW-STARTUP: COM-ORIGINS") < origin.index(
        "if(attempt==1)ddraw_log_compat_evidence();")
    assert create.index("ddraw_log_original_vtable(vtable);") < create.index(
        "*object=patched_ddraw_vtable;")
    assert "ddraw_log_compat_evidence(" not in dllmain
    assert COOP.count("hr=original_coop(self,hwnd,DDSCL_NORMAL);") == 1
