"""Windows native DirectDraw primary reuse checks."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
S=(ROOT/"spiderman2k1fix.c").read_text()

def test_menu_to_gameplay_reuses_primary():
    assert "BORDERLESS: REUSE windowed PRIMARY=" in S
    assert "IDirectDrawSurface7_AddRef(borderless_primary);" in S
    assert "*out=borderless_primary;" in S

def test_primary_remains_alive_after_menu_cleanup():
    assert "IDirectDrawSurface7_AddRef(primary);" in S
    assert "BORDERLESS: ERROR incomplete PRIMARY/BACKBUFFER ownership" in S

def test_reuse_requires_valid_backbuffer_size():
    assert "IDirectDrawSurface7_GetSurfaceDesc(borderless_back,&existing)" in S
    assert "existing.dwWidth!=desktop_w" in S
    assert "existing.dwHeight!=desktop_h" in S
