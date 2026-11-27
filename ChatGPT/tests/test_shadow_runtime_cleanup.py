from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_hd_no_longer_requires_logging_or_pickup_snapshots():
    m=(ROOT/"spiderman2k1fix.c").read_text()
    h=(ROOT/"shadow_hd_trial.inc").read_text()
    s=(ROOT/"shadow_material_trial.inc").read_text()
    assert 'GetPrivateProfileIntA("Features","EnhancedShadows",2,escape_ini)' in m
    assert "shadow_hd_enabled=feature==2?legacy_hd:feature==1;" in m
    assert "if(shadow_hd_enabled)shadow_hd_registry_init();" in m
    assert 'shadow_hd_audit=debug_log_enabled && GetPrivateProfileIntA("Debug","ShadowDiagnostics",0,escape_ini)==1;' in m
    assert "shadow_hd_registry_ready=sizeof(void*)==4" in h
    assert "shadow_hd_registry_ready=shadow_trace_enabled" not in h
    assert "if(shadow_hd_audit)shadow_hd_probe_draw(self,type,fvf,verts,count,0,0," in m
    assert "if(shadow_hd_audit)shadow_hd_probe_draw(self,type,fvf,verts,count,icount,1," in m
    assert "if(shadow_hd_enabled && shadow_hd_audit && !shadow_hd_registry_ready)" in m
    assert "material!=shadow_hd_registry.surface)return 0;" in s
    assert "SHADOW-HD-CAPTURE-AUDIT: off" in h

def test_explicit_feature_is_known_but_not_forced_on():
    ini=(ROOT/"ini_validation.h").read_text()
    assert '"F10SkipCinematics","EnhancedShadows"' in ini
    assert '"ExperimentalProceduralShadows","ShadowDiagnostics"' in ini
    assert "EnhancedShadows=1" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
