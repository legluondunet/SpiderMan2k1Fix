from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_hd_no_longer_requires_logging_or_pickup_snapshots():
    m=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"shadow_hd_trial.inc").read_text()
    s=(ROOT/"shadow_material_trial.inc").read_text()
    assert 'GetPrivateProfileIntA("Features","EnhancedShadows",2,escape_ini)' in m
    assert "shadow_hd_enabled=feature==2?legacy_hd:feature==1;" in m
    assert "if(shadow_hd_enabled && !shadow_hd_registry_init())shadow_hd_enabled=0;" in m
    assert 'shadow_hd_audit=debug_log_enabled && GetPrivateProfileIntA("Debug","ShadowDiagnostics",0,escape_ini)==1;' in m
    assert "shadow_hd_registry_ready=sizeof(void*)==4" in h
    assert "shadow_hd_registry_ready=shadow_trace_enabled" not in h
    assert "if(shadow_hd_audit)shadow_hd_probe_draw(self,type,fvf,verts,count,0,0," in m
    assert "if(shadow_hd_audit)shadow_hd_probe_draw(self,type,fvf,verts,count,icount,1," in m
    assert "SpiderFOV_InstallShadowSelector" not in m
    assert "SpiderFOV_InstallShadowResolver" not in m
    assert "shadow_material_draw(" not in m
    assert 'GetPrivateProfileIntA("Debug","ExperimentalProceduralShadows"' not in m
    assert "material!=shadow_hd_registry.surface)return 0;" in s
    assert "SHADOW-HD-CAPTURE-AUDIT: off" in h

def test_explicit_feature_is_known_but_not_forced_on():
    ini=(ROOT/"ini_validation.h").read_text()
    assert '"F10SkipCinematics","EnhancedShadows"' in ini
    assert '"ExperimentalProceduralShadows","ShadowDiagnostics"' in ini
    assert "EnhancedShadows=0" in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()


def test_repaired_log_is_loaded_before_shadow_audit():
    m=(ROOT/"spiderman2k1fix.c").read_text()
    start=m.index("SpiderCfg_ValidateCurrentIni(log_line);")
    end=m.index("shadow_hd_audit=debug_log_enabled",start)
    assert "debug_log_load_setting();" in m[start:end]
    assert "if(first_run_log_enabled)debug_log_enabled=1;" in m

def test_recovery_preserves_shadow_opt_in_before_installing_replacement(tmp_path):
    import shutil, subprocess, pytest
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    source=tmp_path/"test.c"
    source.write_text(r'''
#include <stdio.h>
#include "ini_recovery.h"
int main(int argc,char **argv){
  SpiderIniRecovered *v;
  (void)SpiderIni_Validate;
  if(argc!=2)return 2;
  v=malloc(sizeof(*v));if(!v)return 3;
  if(!SpiderIni_Recover(argv[1],v)){free(v);return 4;}
  printf("%d",SpiderIni_RecoveredEnhancedShadows(v));
  free(v);return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    cases=[
        ("[Debug]\\nLog=0\\n",0),
        ("[Debug]\\nLog=0\\nExperimentalHDShadows=1\\n",1),
        ("[Debug]\\nExperimentalHDShadows=0\\n[Features]\\nEnhancedShadows=1\\n",1),
        ("[Debug]\\nExperimentalHDShadows=1\\n[Features]\\nEnhancedShadows=0\\n",0),
        ("[Features]\\nEnhancedShadows=invalid\\n[Debug]\\nExperimentalHDShadows=1\\n",1),
        ("[features]\\nEnhancedShadows=bad\\nEnhancedShadows=0\\nEnhancedShadows=1\\n[Debug]\\nExperimentalHDShadows=1\\n",0),
        ("[Debug]\\nExperimentalHDShadows=0\\nExperimentalHDShadows=1\\n",0),
        ("[Debug]\\nExperimentalHDShadows=01\\n[Unknown]\\nEnhancedShadows=0\\n",1),
    ]
    for data,expected in cases:
        ini=tmp_path/"test.ini"
        ini.write_text(data.replace("\\n","\n"))
        assert subprocess.check_output([str(exe),str(ini)],text=True)==str(expected)
    g=(ROOT/"virtual_spidey_cfg.c").read_text()
    assert g.index("SpiderIni_Recover(ini,values)") < g.index("SpiderIni_RecoveredEnhancedShadows(values)")
    assert g.index("SpiderIni_RecoveredEnhancedShadows(values)") < g.index("SpiderIni_Validate(tmp,NULL,NULL)")
