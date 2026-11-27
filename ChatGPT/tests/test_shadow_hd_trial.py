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
    assert "shadow_resolve_original(index)" in f
    assert "sites[0]=base+0x1077cf;sites[1]=base+0x108e80;" in f
    assert "SpiderShadowResolve_Match(&shadow_hd_scope,tag->material)" in s
    assert "shadow_hd_current_index(shadow_material_frame)" in s
    assert 'GetPrivateProfileIntA("Debug","ExperimentalHDShadows",0,escape_ini)==1' in m
    assert "ExperimentalHDShadows=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
    assert h.count("original_draw_primitive(")==1

def test_index_and_surface_provenance(tmp_path):
    import shutil, subprocess, pytest
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    source=tmp_path/"test.c"
    source.write_text(r'''
#include <assert.h>
#include "shadow_resolve_scope.h"
int main(void){
  SpiderShadowResolveScope s;
  SpiderShadowResolve_Begin(&s,240);
  assert(!SpiderShadowResolve_Match(&s,240));
  assert(!SpiderShadowResolve_Match(&s,0x07d2bf68));
  SpiderShadowResolve_Observe(&s,240,0x07d2bf68);
  assert(SpiderShadowResolve_Match(&s,0x07d2bf68));
  assert(!SpiderShadowResolve_Match(&s,240));
  SpiderShadowResolve_Observe(&s,240,0x07d2bf68);
  assert(SpiderShadowResolve_Match(&s,0x07d2bf68));
  SpiderShadowResolve_Observe(&s,241,0x07d2bf68);
  assert(!SpiderShadowResolve_Match(&s,0x07d2bf68));
  SpiderShadowResolve_Begin(&s,240);
  SpiderShadowResolve_Observe(&s,240,0x07d2bf68);
  SpiderShadowResolve_Observe(&s,240,0x07d2bf70);
  assert(!SpiderShadowResolve_Match(&s,0x07d2bf68));
  SpiderShadowResolve_Begin(&s,240);
  SpiderShadowResolve_Observe(&s,240,0);
  assert(!SpiderShadowResolve_Match(&s,0));
  SpiderShadowResolve_Begin(&s,0);
  SpiderShadowResolve_Observe(&s,240,0x07d2bf68);
  assert(!SpiderShadowResolve_Match(&s,0x07d2bf68));
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
