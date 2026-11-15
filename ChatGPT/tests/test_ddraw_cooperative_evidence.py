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
