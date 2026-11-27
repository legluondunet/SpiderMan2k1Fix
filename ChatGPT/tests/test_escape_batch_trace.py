"""Bounded read-only observation of the original buffered Escape path."""
from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]

def test_native_escape_batch_classifier(tmp_path):
    if not shutil.which("gcc"):
        pytest.skip("host C compiler missing")
    src=tmp_path/"test.c"
    src.write_text(r'''
#include <assert.h>
#include "escape_batch_trace.h"
int main(void) {
  uint32_t events[80]={0};
  SpiderEscapeBatch v={0,0};
  events[0]=1; events[1]=0x80;
  events[5]=0x39; events[6]=0x80;
  events[10]=1; events[11]=0;
  assert(SpiderEscapeBatch_Decode(events,3,&v));
  assert(v.down==1 && v.up==4 && SpiderEscapeBatch_DownOverwritten(v));
  events[15]=1; events[16]=0x80;
  assert(SpiderEscapeBatch_Decode(events,4,&v));
  assert(v.down==9 && v.up==4 && !SpiderEscapeBatch_DownOverwritten(v));
  assert(!SpiderEscapeBatch_Decode(events,17,&v));
  assert(!SpiderEscapeBatch_Decode(events,0,&v));
  assert(v.down==0 && v.up==0);
  assert(!SpiderEscapeBatch_Decode(events+5,1,&v));
  events[75]=1; events[76]=0;
  assert(SpiderEscapeBatch_Decode(events,16,&v));
  assert(v.up==0x8004 && SpiderEscapeBatch_DownOverwritten(v));
  assert(!SpiderEscapeBatch_Decode(0,0,&v));
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_escape_observation_exact_epilogue_and_independent_retry():
    s=(ROOT/"input_notacquired_retry.c").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    assert "#define ESC_BATCH_SITE 0x00501C9Bu" in s
    assert "0x5b,0x8b,0xc6,0x5f,0x5e,0x81,0xc4,0x44,0x01,0x00,0x00,0xc3" in s
    assert "0x8b,0x44,0x24,0x30,0x8d,0x4c,0x24,0x34,0x50,0x51" in s
    assert "0x83,0xc4,0x08,0x61,0x9d" in s
    assert "memcpy(p,native_exit,5);p+=5;" in s
    assert "emit_jmp(&p,site+5);" in s
    assert "memcmp(site,native_exit,sizeof(native_exit))" in s
    assert "0x00501C6Cu,native_loop,sizeof(native_loop)" in s
    assert "try_install();install_escape_batch_trace();" in s
    assert 'GetPrivateProfileIntA("Debug","EscapeBatchTrace",0,escape_ini)==1' in m
    assert "if(debug_log_enabled && spider_path_ini_a(escape_ini,sizeof(escape_ini)))" in m
    observe=s.split("static void __cdecl escape_observe(",1)[1].split(
        "static void install_escape_batch_trace(",1)[0]
    assert "count>16u" in observe
    assert "escape_used>=ESC_BATCH_CAP" in observe
    assert "const volatile BYTE" in observe
    for forbidden in ("write_log(", "GetDeviceData(", "SendInput(", "keybd_event(", "VirtualProtect("):
        assert forbidden not in observe
    assert "ESC-BATCH: report cap reached" in s
