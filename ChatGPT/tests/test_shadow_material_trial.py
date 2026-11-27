"""Guard the narrow native provenance and original render-state restoration contract."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_shadow_material_scope():
    s=(ROOT/"shadow_material_trial.inc").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    f=(ROOT/"fov_aspect_win.c").read_text()
    h=(ROOT/"pickup_shadow_trace.h").read_text()
    for gate in ("rva!=0x00102C72UL","source!=drawn","count!=4u","tag->stamp!=flip+1",
                 "material!=tag->material","memcmp(source,tag->verts","shadow_trace_material_match(material,flip)",
                 "!shadow_material_soft || !matched || !known","DDPF_ALPHAPIXELS",
                 "D3DTSS_MAGFILTER,mag","D3DTSS_MINFILTER,min","shadow_material_soft=0"):
        assert gate in s
    assert "shadow_trace_u32(quad+0x60)" in h
    assert "flip-shadow_trace_material_flip>60" in h
    assert "world_quad_observer(0)" in f and "world_quad_observer(1)" in f
    assert 'GetPrivateProfileIntA("Debug","ExperimentalSoftShadows",0,escape_ini)==1' in m
    assert "SpiderFOV_SetWorldQuadObserver(shadow_material_observe_submit)" in m
    assert "ExperimentalSoftShadows=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
    for forbidden in ("SetTexture(","->Lock(","WriteProcessMemory(","VirtualProtect("):
        assert forbidden not in s
