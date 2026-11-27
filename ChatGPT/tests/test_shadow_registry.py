from pathlib import Path
import shutil, subprocess, pytest
ROOT=Path(__file__).resolve().parents[2]

def test_registry_exact_name_chain_index_stride(tmp_path):
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    source=tmp_path/"test.c"
    source.write_text(r'''
#include <assert.h>
#include "shadow_registry.h"
static unsigned char names[10240],node1[36],node2[36];
static uint32_t head=0x1000,surface=0x07d6aae8;
static void put32(unsigned char *p,uint32_t v){memcpy(p,&v,4);}
static int readmem(uintptr_t a,void *p,size_t n){
  if(a==SPIDER_SHADOW_NAMES && n==sizeof(names))memcpy(p,names,n);
  else if(a==SPIDER_SHADOW_BUCKETS+(0x1234&511)*4 && n==4)memcpy(p,&head,4);
  else if(a==0x1000 && n==36)memcpy(p,node1,n);
  else if(a==0x2000 && n==36)memcpy(p,node2,n);
  else if(a==SPIDER_SHADOW_SURFACES+240*104 && n==4)memcpy(p,&surface,4);
  else return 0;
  return 1;
}
int main(void){
  SpiderShadowRegistry o;
  unsigned char *e=names+40*7;
  e[0]=1;memcpy(e+1,"Shadow2",8);put32(e+36,0x1234);
  put32(node1+20,0x9999);put32(node1+32,0x2000);
  put32(node2+20,0x1234);node2[2]=240;
  assert(SpiderShadowRegistry_Find(readmem,&o));
  assert(o.descriptor==0x2000 && o.index==240 && o.surface==surface);
  e[8]='x';assert(!SpiderShadowRegistry_Find(readmem,&o));e[8]=0;
  e[0]=0;assert(!SpiderShadowRegistry_Find(readmem,&o));e[0]=1;
  put32(node1+32,0x1000);assert(!SpiderShadowRegistry_Find(readmem,&o));put32(node1+32,0x2000);
  node2[2]=2;assert(!SpiderShadowRegistry_Find(readmem,&o));node2[2]=240;
  surface=0;assert(!SpiderShadowRegistry_Find(readmem,&o));assert(!o.surface);
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_continuous_path_is_independent_and_guarded():
    h=(ROOT/"shadow_hd_trial.inc").read_text()
    s=(ROOT/"shadow_material_trial.inc").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    for gate in ("shadow_trace_signature(0x0050f3c0u","SpiderShadowRegistry_Find","index!=shadow_hd_registry.index",
                 "surface!=shadow_hd_registry.surface","SHADOW-HD-COVERAGE:","SHADOW-HD-CAPTURE:"):
        assert gate in h
    start=s.index("static int shadow_hd_registry_draw")
    end=s.index("static HRESULT shadow_material_draw",start)
    scope=s[start:end]
    for gate in ("rva!=0x00102C72UL","SpiderMenubg43_RecordIndex","material!=named",
                 "(uintptr_t)tex==(uintptr_t)named","shadow_hd_draw","SpiderFOV_GameplayReady"):
        assert gate in scope
    assert "shadow_trace_material_flip" not in scope and "shadow_material_tags" not in scope
    assert "shadow_hd_finish_frame(count)" in m
    assert "shadow_hd_capture_report(" in m
    assert "!native_esc_primitive && !verified_hud_canvas" in m
