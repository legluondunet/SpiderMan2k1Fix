"""One-shot Escape edge recovery; never changes the native latch."""
from pathlib import Path
import shutil
import subprocess
import pytest
ROOT=Path(__file__).resolve().parents[2]

def test_recovery_state_machine(tmp_path):
    if not shutil.which("gcc"): pytest.skip("host compiler absent")
    src=tmp_path/"test.c"
    src.write_text(r'''
#include <assert.h>
#include "escape_pause_edge.h"
int main(void){
  SpiderEscapePauseEdge p={0};
  SpiderEscapeBatch down={1,0}, up={0,1}, ambiguous={1,2};
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  assert(SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  assert(!SpiderEscapePauseEdge_Take(&p,100,10,255,1));
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  assert(!SpiderEscapePauseEdge_Take(&p,102,11,127,1));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  assert(!SpiderEscapePauseEdge_Take(&p,101,12,127,1));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,0));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,255);
  SpiderEscapePauseEdge_Batch(&p,up,101,11,128);
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  SpiderEscapePauseEdge_Batch(&p,ambiguous,100,10,255);
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  /* Actual failing sequence: close consumed native FF in poll 367,
   * next poll 368 sees 7F in gameplay. No reopening is allowed. */
  SpiderEscapePauseEdge_Batch(&p,down,367,316,255);
  assert(SpiderEscapePauseEdge_AckClose(&p));
  assert(!SpiderEscapePauseEdge_AckClose(&p));
  assert(!SpiderEscapePauseEdge_Take(&p,368,317,127,1));
  /* A fresh physical down remains eligible. */
  SpiderEscapePauseEdge_Batch(&p,down,400,349,255);
  assert(SpiderEscapePauseEdge_Take(&p,401,350,127,1));
  SpiderEscapePauseEdge_Batch(&p,down,100,10,127);
  assert(!SpiderEscapePauseEdge_Take(&p,101,11,127,1));
  return 0;
}
''')
    exe=tmp_path/"test"
    subprocess.run(["gcc","-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_recovery_is_exact_and_opt_in():
    q=(ROOT/"quick_exit_f12.c").read_text()
    i=(ROOT/"input_notacquired_retry.c").read_text()
    m=(ROOT/"spiderman2k1fix.c").read_text()
    for gate in ("0x00440F39u","mask==0x40040u","arg1==1u && arg2==1u",
                 "frame[1]==0x0050A662u","frame[3]==0x0050C23Fu",
                 "GetForegroundWindow()==hwnd","(*SPIDER_PAUSE_CONTEXT == 0u)",
                 "(*SPIDER_GAME_BINK_ACTIVE == 0u)","escape_pause_scope=previous"):
        assert gate in q
    assert "SpiderDI8Retry_TakeEscapePauseEdge((unsigned)result,1)" in q
    assert "(DWORD)frame[1]==0x00440F7Cu" in q
    assert "SpiderDI8Retry_AcknowledgeEscapePauseClose();" in q
    assert "SpiderEscapePauseEdge_AckClose(&escape_pause_edge)" in i
    assert "ESC-CLOSE-ACK:" in i
    assert "(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0" in i
    assert 'GetPrivateProfileIntA("Debug","ExperimentalEscapeEdgeRecovery",0,escape_ini)==1' in m
    assert "ExperimentalEscapeEdgeRecovery=" not in (ROOT/"ChatGPT/tests/fixtures/SpiderMan2k1Fix.generated-example.ini").read_text()

def test_shadow_descriptor_observation_not_authorization():
    s=(ROOT/"shadow_material_trial.inc").read_text()
    h=(ROOT/"pickup_shadow_trace.h").read_text()
    assert "shadow_descriptor_words[SPIDER_SHADOW_PICKUPS][16]" in h
    assert "SHADOW-DESC-LINK:" in s
    assert "candidate only" in s
    assert "matched=shadow_trace_material_match(material,flip)" in s
    assert "if(!shadow_material_soft || !matched || !known" in s
