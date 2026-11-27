from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_shadow_hd_is_opt_in_and_bound_to_native_provenance():
    h=(ROOT/"shadow_hd_trial.inc").read_text()
    s=(ROOT/"shadow_material_trial.inc").read_text()
    f=(ROOT/"fov_aspect_win.c").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    for gate in ("shadow_descriptor_words[i][0]>>16","live==index","flip!=shadow_hd_selected_flip",
                 "current!=shadow_hd_selected","0x00568170u","dwRGBAlphaBitMask!=0xf000u",
                 "(pixel&0x0fffu)!=rgb","lo==hi","SpiderShadow_Alpha4To8",
                 "IDirect3DDevice7_SetTexture(device,0,shadow_hd_surface)",
                 "IDirect3DDevice7_SetTexture(device,0,source)"):
        assert gate in h
    assert "material==tag->hd_material" in s and "bound_texture==(uintptr_t)material" in s
    assert "memcmp(source,tag->verts" in s
    assert "site=base+0x9f8c" in f
    assert "shadow_selector_original(index,mode)" in f
    assert 'GetPrivateProfileIntA("Debug","ExperimentalHDShadows",0,escape_ini)==1' in m
    assert "ExperimentalHDShadows=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
    assert h.count("original_draw_primitive(")==1
