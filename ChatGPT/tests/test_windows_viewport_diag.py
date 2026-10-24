"""Regression checks for Windows Direct3D7 viewport crash diagnostics."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text()
DOC = (ROOT / "docs/references/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_logs_boundary_around_original_setviewport():
    assert 'D3D7-VIEWPORT-DIAG: BEFORE' in CORE
    assert 'D3D7-VIEWPORT-DIAG: AFTER' in CORE
    assert 'hr=original_set_viewport(self,view)' in CORE
    assert 'if(!original_set_viewport)' in CORE
    assert 'VirtualQuery(' in CORE

def test_diagnostic_is_opt_in_only_and_leaves_default_mapping():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_VIEWPORT_LOGICAL"' in CORE
    assert 'nbytes==1 && diagnostic[0]==\'1\'' in CORE
    assert 'call_original_set_viewport_traced(self,&physical,n,"menu-mapped")' in CORE
    assert 'call_original_set_viewport_traced(self,view,n,"menu-logical-ab")' in CORE
    assert 'Remove-Item Env:' in DOC

def test_existing_features_remain_present():
    assert '#include "menu_version.h"' in CORE
    assert "SpiderInputFocus_OnFlip(count);" in CORE
    assert "SpiderFrameLimiter_Wait(&g_frame_limiter);" in CORE
    assert 'SpiderMenuVersion_DrawToSurface(borderless_back' in CORE
