"""Retired scoped sampling and resolver experiments must not return to runtime."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_retired_material_paths():
    s=(ROOT/"shadow_material_trial.inc").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    for retired in ("shadow_material_soft","shadow_material_tags","shadow_hd_scope","shadow_material_draw("):
        assert retired not in s and retired not in m
    assert "shadow_hd_registry_draw" in s and "shadow_hd_probe_draw" in s
    assert "SetTextureStageState(" not in s
    assert "VirtualProtect(" not in s
    assert 'GetPrivateProfileIntA("Debug","ExperimentalSoftShadows"' not in m
