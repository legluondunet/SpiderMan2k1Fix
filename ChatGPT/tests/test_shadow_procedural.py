from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]

def test_calibrated_procedural_alpha(tmp_path):
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    source=tmp_path/"test.c"
    source.write_text(r'''
#include <assert.h>
#include <string.h>
#include "shadow_alpha_procedural.h"
static uint8_t src[4096],copy[4096],dst[65536];
int main(void){
  SpiderShadowFit fit;
  unsigned x,y,n=0,seen[256]={0};
  for(y=0;y<64u;y++)for(x=0;x<64u;x++){
    double q=SpiderShadow_Radius2(x+0.5,y+0.5,32.0,32.0,27.0,23.0);
    src[y*64u+x]=(uint8_t)(12.0*SpiderShadow_Profile(q,4)+0.5);
  }
  memcpy(copy,src,sizeof(src));
  assert(SpiderShadow_Procedural(src,dst,&fit));
  assert(!memcmp(copy,src,sizeof(src)));
  assert(fit.peak==12u && fit.reason==0u && fit.error_permille<=300u);
  assert(dst[127*256+127]==204u && dst[0]==0u);
  for(y=0;y<256u;y++)for(x=0;x<256u;x++){
    unsigned v=dst[y*256u+x];
    assert(v<=204u && v==dst[y*256u+255u-x] && v==dst[(255u-y)*256u+x]);
    seen[v]=1;
  }
  for(x=0;x<256u;x++)n+=seen[x];
  assert(n>100u);
  for(x=1;x<128u;x++)assert(dst[127*256u+x]>=dst[127*256u+x-1u]);
  for(x=129;x<256u;x++)assert(dst[127*256u+x]<=dst[127*256u+x-1u]);
  assert(fit.output_sum*100ull>=fit.source_sum*272ull*97ull);
  assert(fit.output_sum*100ull<=fit.source_sum*272ull*103ull);
  memset(src,0,sizeof(src));
  assert(!SpiderShadow_Procedural(src,dst,&fit)&&fit.reason==2u);
  memset(src,15,sizeof(src));
  assert(!SpiderShadow_Procedural(src,dst,&fit)&&fit.reason==2u);
  memcpy(src,copy,sizeof(src));
  src[0]=16;
  assert(!SpiderShadow_Procedural(src,dst,&fit)&&fit.reason==1u);
  memcpy(src,copy,sizeof(src));
  for(y=10;y<54;y++)for(x=8;x<29;x++)src[y*64u+x]=0;
  assert(!SpiderShadow_Procedural(src,dst,&fit));
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_runtime_profile_and_fallback():
    h=(ROOT/"shadow_hd_trial.inc").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    assert "SpiderShadow_Procedural(shadow_hd_a4,shadow_hd_a8,&fit)" in h
    assert "if(!procedural && !SpiderShadow_Alpha4To8" in h
    assert "SHADOW-HD-PROFILE:" in h and "SHADOW-HD-CAPTURE-PROFILE:" in h
    assert 'GetPrivateProfileIntA("Debug","ExperimentalProceduralShadows",1,escape_ini)==1' in m
    assert "ExperimentalProceduralShadows=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()
