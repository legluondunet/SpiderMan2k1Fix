"""Linux-host regression tests for opt-in PKR loose-file path and hook wiring."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]
CORE=(ROOT/"spiderman2k1fix.c").read_text()
LOADER=(ROOT/"loose_files.c").read_text()
BUILD=(ROOT/"build.sh").read_text()

def test_opt_in_load_after_config_and_native_allocator():
    assert "SpiderLoose_Init(log_line)" in CORE
    assert CORE.index("SpiderCfg_ValidateCurrentIni(log_line)") < CORE.index("SpiderLoose_Init(log_line)")
    assert "loose_files.c" in BUILD
    assert "0x00519194u" in LOADER
    assert "0x00529BA2u" in LOADER
    assert "0x00529BB0u" in LOADER
    assert "memcmp(target,pkr_prologue,sizeof(pkr_prologue))" in LOADER
    assert 'GetPrivateProfileIntA("Modding","LooseFiles",0,ini)' in LOADER
    assert "return original_read?original_read(" in LOADER
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in LOADER

@pytest.mark.skipif(not shutil.which("gcc"),reason="gcc required")
def test_relative_paths_only_and_optional_data_prefix(tmp_path):
    test=tmp_path/"path.c"
    test.write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "loose_files_path.h"
static void yes(const char *d,const char *f,const char *expected){
  char b[260];
  assert(spider_loose_path(b,sizeof(b),"C:\\Games\\Spider-Man\\",d,f));
  assert(strcmp(b,expected)==0);
}
static void no(const char *d,const char *f){
  char b[260];
  assert(!spider_loose_path(b,sizeof(b),"C:\\Games\\Spider-Man\\",d,f));
}
int main(void){
  yes("data\\audio\\","wind2.wav","C:\\Games\\Spider-Man\\data\\audio\\wind2.wav");
  yes("AUDIO\\","wind2.wav","C:\\Games\\Spider-Man\\data\\AUDIO\\wind2.wav");
  yes("data\\movie\\","intro.bik","C:\\Games\\Spider-Man\\data\\movie\\intro.bik");
  yes("data\\","wind2.wav","C:\\Games\\Spider-Man\\data\\wind2.wav");
  yes("","wind2.wav","C:\\Games\\Spider-Man\\data\\wind2.wav");
  no("..\\","wind2.wav");no("data\\..\\audio","wind2.wav");
  no("C:\\system\\","wind2.wav");no("\\\\server","wind2.wav");
  no("data\\audio\\","..\\wind2.wav");no("data\\audio\\","CON.wav");
  no("data\\audio\\","NUL");no("data\\audio\\","hello:world");
  no("data\\audio\\","a//b.wav");no("data\\audio\\","a*?.wav");
  no("data\\audio\\","wind2.wav\\");no("data\\audio\\","");
  puts("loose path checks passed");return 0;
}
''')
    exe=tmp_path/"loose-path-test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror",
        "-I",str(ROOT),str(test),"-o",str(exe)],check=True,capture_output=True,text=True)
    result=subprocess.run([str(exe)],capture_output=True,text=True)
    assert result.returncode==0,result.stderr
