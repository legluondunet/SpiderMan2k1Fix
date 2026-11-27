"""Fixed native path windows + standalone alpha reconstruction tests."""
from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]

def test_native_path_only_observes_authenticated_process():
    h=(ROOT/"pickup_shadow_trace.h").read_text()
    c=(ROOT/"spiderman2k1fix.c").read_text()
    s=(ROOT/"shadow_material_trial.inc").read_text()
    for addr in ("0x00409144u","0x004C94B0u","0x004097E0u","0x00508550u",
                 "0x00507680u","0x00502B20u"):
        assert addr in h
    assert "if(!shadow_trace_enabled || !shadow_trace_log)return;" in h
    assert "shadow_trace_native_code();" in c
    assert c.index("shadow_trace_native_code();") < c.index("SpiderFOV_SetWorldQuadObserver(shadow_material_observe_submit)")
    assert "SHADOW-IN-SUBMIT:" in s and "SHADOW-SUBMIT-STATE:" in s
    assert "if(!shadow_material_soft || !matched || !known" in s
    assert "shadow_alpha_reconstruct.h" not in c
    assert "SpiderShadow_Alpha4To8" not in s

def test_monotone_shadow_alpha_reconstruction(tmp_path):
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    source=tmp_path/"test.c"
    source.write_text(r'''
#include <assert.h>
#include <string.h>
#include "shadow_alpha_reconstruct.h"
static uint8_t src[4096], dst[65536];
int main(void){
  unsigned x,y,lo,hi,intermediate=0;
  memset(src,7,sizeof(src));
  assert(SpiderShadow_Alpha4To8(src,dst));
  for(x=0;x<65536;x++)assert(dst[x]==119);
  memset(src,0,sizeof(src));
  assert(SpiderShadow_Alpha4To8(src,dst));
  for(x=0;x<65536;x++)assert(dst[x]==0);
  for(y=0;y<64;y++)for(x=0;x<64;x++)src[y*64+x]=x<32?0:15;
  assert(SpiderShadow_Alpha4To8(src,dst));
  for(x=0;x<256;x++){
    if(x)assert(dst[x]>=dst[x-1]);
    if(dst[x]>0&&dst[x]<255)++intermediate;
    for(y=1;y<256;y++)assert(dst[y*256+x]==dst[x]);
  }
  assert(dst[0]==0&&dst[255]==255&&intermediate>0);
  for(y=0;y<64;y++)for(x=0;x<64;x++){
    int dx=(int)(2*x)-63,dy=(int)(2*y)-63;
    unsigned d=(unsigned)(dx*dx+dy*dy);
    src[y*64+x]=(uint8_t)(d>=3600?0:(3600-d)*15/3600);
  }
  assert(SpiderShadow_Alpha4To8(src,dst));
  lo=255;hi=0;
  for(y=0;y<256;y++)for(x=0;x<256;x++){
    unsigned a=dst[y*256+x],b=dst[y*256+255-x],c=dst[(255-y)*256+x];
    assert((a>b?a-b:b-a)<=1 && (a>c?a-c:c-a)<=1);
    if(a<lo)lo=a;if(a>hi)hi=a;
  }
  assert(lo==0&&hi>200&&dst[0]==0);
  src[0]=16;
  assert(!SpiderShadow_Alpha4To8(src,dst));
  assert(!SpiderShadow_Alpha4To8(0,dst));
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
                    "-Wno-misleading-indentation","-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
