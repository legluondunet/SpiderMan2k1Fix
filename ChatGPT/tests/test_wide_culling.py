"""Native horizontal-plane math and opt-in exact-site contract."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]

def test_wide_culling_math(tmp_path):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler missing")
    src=tmp_path/"test.c"
    src.write_text(r'''
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "wide_culling.h"
int main(void){
    int16_t p[3]={3277,0,2458},n[3]={-3277,0,2458};
    int16_t a[3]={11,22,33},b[3],before[3];
    assert(SpiderCull_WidenHorizontalPair(p,n,0.75f,0,a));
    assert(SpiderCull_WidenHorizontalPair(p,n,0.75f,1,b));
    assert(a[0]==-b[0] && a[1]==0 && b[1]==0 && a[2]==b[2]);
    assert(fabs((double)a[0]/a[2]-0.75*(double)p[0]/p[2])<0.001);
    assert(fabs(hypot(a[0],a[2])-hypot(p[0],p[2]))<1.0);
    /* A point newly inside the widened frustum, still outside old. */
    assert(-p[0]*9+p[2]*10<0);
    assert(-a[0]*9+a[2]*10>=0);
    /* Far outside still rejected. */
    assert(-a[0]*20+a[2]*10<0);
    assert(p[0]==3277 && p[2]==2458 && n[0]==-3277);
    memcpy(before,a,sizeof(a));
    assert(!SpiderCull_WidenHorizontalPair(p,n,1.0f,0,a));
    assert(!SpiderCull_WidenHorizontalPair(p,n,NAN,0,a));
    assert(!SpiderCull_WidenHorizontalPair(p,n,0.29f,0,a));
    assert(!SpiderCull_WidenHorizontalPair(p,n,0.75f,2,a));
    n[1]=1;assert(!SpiderCull_WidenHorizontalPair(p,n,0.75f,0,a));
    n[1]=0;n[2]=-1;assert(!SpiderCull_WidenHorizontalPair(p,n,0.75f,0,a));
    assert(!memcmp(before,a,sizeof(a)));
    return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-lm","-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_wide_culling_exact_site_and_scope():
    c=(ROOT/"fov_aspect_win.c").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    assert 'GetPrivateProfileIntA("Debug","ExperimentalWideCulling",0,ini)==1' in m
    assert 'if(debug_log_enabled && lstrcmpiA(aspect,"Auto")==0 &&' in m
    assert '"ExperimentalWideCulling"' in (ROOT/"ini_validation.h").read_text()
    hook=c.split("static void __cdecl cull_load_vector(",1)[1].split(
        "int SpiderFOV_InstallWideCullingTrial(",1)[0]
    assert "SpiderFOV_GameplayReady()" in hook
    assert "address==0x0065CED8u || address==0x0065CEE0u" in hook
    assert "cull_original_load(mapped);" in hook and "cull_original_load(src);" in hook
    assert "site=base+0x73204;" in c
    assert "memcmp(base+0x730B2,construction" in c
    assert "memcmp(base+0x73203,context" in c
    assert "memcmp(base+0x7321E,loop" in c
    assert "memcmp(base+0x6D8A0,load" in c
    assert "memcmp(base+0x739C6,consume" in c
