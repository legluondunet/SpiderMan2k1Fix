"""World-quad X-only math and exact native submission contract."""
from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]

def test_world_quad_projection_math(tmp_path):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler missing")
    src=tmp_path/"test.c"
    src.write_text(r'''
#include <assert.h>
#include <math.h>
#include <string.h>
#include "world_quad_projection.h"
int main(void){
    float in[4]={0.f,960.f,1920.f,-100.f},out[4],saved[4];
    assert(SpiderWorldQuad_MapX(in,1920,0.75f,out));
    assert(out[0]==240.f && out[1]==960.f && out[2]==1680.f && out[3]==165.f);
    /* Observed left-side shadow near x=219 in a 1920 image:
       mapped center is ~404, consistent with the pickup position. */
    in[0]=219.f; in[1]=243.f; in[2]=2100.f; in[3]=1900.f;
    assert(SpiderWorldQuad_MapX(in,1920,0.75f,out));
    assert(out[0]==404.25f && out[1]-out[0]==18.f && out[2]==1815.f);
    memcpy(saved,out,sizeof(out));
    assert(!SpiderWorldQuad_MapX(in,1920,1.f,out));
    assert(!SpiderWorldQuad_MapX(in,1920,NAN,out));
    assert(!SpiderWorldQuad_MapX(in,0,0.75f,out));
    in[2]=NAN;
    assert(!SpiderWorldQuad_MapX(in,1920,0.75f,out));
    assert(!memcmp(saved,out,sizeof(out)));
    in[2]=4000.f; in[3]=-1000.f;
    assert(SpiderWorldQuad_MapX(in,2560,0.5f,in));
    assert(in[2]==2640.f && in[3]==140.f);
    return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_world_quad_trial_scope_and_native_abi():
    c=(ROOT/"fov_aspect_win.c").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    hook=c.split("static void __cdecl quad_submit(",1)[1].split(
        "int SpiderFOV_InstallWorldQuadTrial(",1)[0]
    assert "SpiderFOV_GameplayReady()" in hook
    assert "width==world_quad_gameplay_width" in hook
    assert "logical==512u" in hook and "native_center_width==logical" in hook
    assert "x0=raw[0];x1=raw[1];x2=raw[2];x3=raw[3];" in hook
    assert hook.count("quad_original(")==1
    assert "quad_original(x0,y0,z0,u0,v0,c0,x1,y1,z1,u1,v1,c1," in hook
    assert "x2,y2,z2,u2,v2,c2,x3,y3,z3,u3,v3,c3);" in hook
    assert "0x006151F8u==0u" in hook and "0x00682950u==0u" in hook
    assert "quad_logged<8u" in hook
    assert "first=base+0xa1a9;second=base+0xa367;" in c
    assert "memcmp(base+0xa021,projection1" in c
    assert "memcmp(base+0xa221,projection2" in c
    assert "memcmp(base+0xa1a6,submit1" in c
    assert "memcmp(base+0xa364,submit2" in c
    assert "memcmp(base+0x108550,callee" in c
    assert "memcmp(base+0x6dc8d,fixed" in c
    assert 'GetPrivateProfileIntA("Debug","ExperimentalWorldQuadFOV",0,ini)==1' in m
    assert "ExperimentalWorldQuadFOV=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
