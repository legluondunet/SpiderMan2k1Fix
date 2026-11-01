"""Opt-in Left Alt-only native recovery policy and wiring guards.

No hardware validation here. The executable memory is NEVER accessed by
these tests. A failing model or guard must stop enabling the experiment.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_c99_policy_focus_and_one_shot_guards(tmp_path):
    c=tmp_path/"alt_latch.c"
    c.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "alt_latch_recovery_policy.h"
      static int step(SpiderAltLatchRecoveryPolicy *p,unsigned eligible,
          unsigned left,unsigned win_up,unsigned other_up,unsigned age) {
        return spider_alt_latch_policy_step(p,eligible,(uint8_t)left,
                                            win_up,other_up,age);
      }
      int main(void) {
        SpiderAltLatchRecoveryPolicy p={0};
        unsigned k;
        /* No actual confirmed focus loss -> no authorization. */
        spider_alt_latch_policy_return(&p,1);
        assert(!p.pending);
        assert(step(&p,1,0x7f,1,1,1000)==0);
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,0);
        assert(!p.pending);
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(p.pending);
        /* Ineligible Bink/pause/background cannot progress the streak. */
        for(k=0;k<40;k++)assert(step(&p,0,0x7f,1,1,100)==0);
        assert(p.stable_up_frames==0);
        /* Real physical Alt or Tab/RAlt held means never write. */
        for(k=0;k<12;k++)assert(step(&p,1,0x7f,0,1,110)==0);
        assert(p.stable_up_frames==0);
        for(k=0;k<12;k++)assert(step(&p,1,0x7f,1,0,130)==0);
        assert(p.stable_up_frames==0);
        /* Six stable foreground frames AND >=180ms of actual clean
         * observations, not just >=180ms elapsed after the return. */
        for(k=0;k<5;k++)assert(step(&p,1,0x7f,1,1,160)==0);
        assert(step(&p,1,0x7f,1,1,170)==0);
        assert(step(&p,1,0x7f,1,1,190)==0); /* 30ms clean only */
        assert(p.pending);
        assert(step(&p,1,0x7f,1,1,339)==0);
        assert(step(&p,1,0x7f,1,1,340)==1);
        assert(!p.pending);
        for(k=0;k<20;k++)assert(step(&p,1,0x7f,1,1,1000)==0);
        /* A fresh native down (0xff) cannot be rewritten as an up. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,1,0xff,1,1,100)==2);
        assert(!p.pending);
        /* A native key that cleared naturally should never be touched. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,1,0x80,1,1,100)==2);
        assert(!p.pending);
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,1,0x00,1,1,100)==2);
        assert(!p.pending);
        /* Return limited to 5s; never reset by a movie or pause. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,0,0x7f,1,1,4900)==0);
        assert(step(&p,1,0x7f,1,1,5001)==3);
        assert(!p.pending);
        /* A real physical Tab/Alt event interrupts the clean
         * observation interval; the 180ms clock must restart. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        for(k=0;k<6;k++)
          assert(step(&p,1,0x7f,1,1,100+30*k)==0);
        assert(step(&p,1,0x7f,0,1,280)==0);
        assert(!p.stable_up_started&&p.stable_up_frames==0);
        for(k=0;k<6;k++)
          assert(step(&p,1,0x7f,1,1,300+30*k)==0);
        assert(step(&p,1,0x7f,1,1,479)==0);
        assert(step(&p,1,0x7f,1,1,480)==1);
        assert(!p.pending);
        /* A late first clean sample cannot borrow time from the
         * foreground wake or the previous ineligible window. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,0,0x7f,1,1,3600)==0);
        for(k=0;k<6;k++)
          assert(step(&p,1,0x7f,1,1,4000+30*k)==0);
        assert(step(&p,1,0x7f,1,1,4180)==1);
        assert(!p.pending);
        /* Another loss cancels any previous return window. */
        spider_alt_latch_policy_loss(&p);
        spider_alt_latch_policy_return(&p,1);
        assert(step(&p,1,0x7f,1,1,100)==0);
        spider_alt_latch_policy_loss(&p);
        assert(!p.pending && p.stable_up_frames==0);
        assert(step(&p,1,0x7f,1,1,300)==0);
        spider_alt_latch_policy_return(&p,1);
        assert(p.pending && p.stable_up_frames==0);
        assert(step(0,1,0x7f,1,1,1000)==0);
        spider_alt_latch_policy_loss(0);
        spider_alt_latch_policy_return(0,1);
        return 0;
      }
    """),encoding="utf-8")
    exe=tmp_path/"alt_latch"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(c),"-o",str(exe)],check=True,
                   text=True,capture_output=True)
    subprocess.run([str(exe)],check=True)

def test_original_write_is_one_byte_only_and_fail_closed():
    impl=(ROOT/"alt_latch_recovery.h").read_text(encoding="utf-8")
    model=(ROOT/"alt_latch_recovery_policy.h").read_text(encoding="utf-8")
    assert 'GetPrivateProfileIntA("Input","ExperimentalAltLatchRecovery",1,ini)==1' in impl
    assert 'spider_alt_latch_preflight()' in impl
    assert '0x00501C56u' in impl and '0x00501BC3u' in impl
    assert '0x006B7964u' in impl
    assert 'GetModuleHandleA(NULL)!=0x00400000u' in impl
    assert 'VirtualQuery(' in impl and 'page.State!=MEM_COMMIT' in impl
    assert '!bink&&!paused' in impl
    assert 'GetForegroundWindow()==hwnd' in impl
    assert 'GetAsyncKeyState(VK_LMENU)' in impl
    assert 'GetAsyncKeyState(VK_MENU)' in impl
    assert 'GetAsyncKeyState(VK_RMENU)' in impl
    assert 'GetAsyncKeyState(VK_TAB)' in impl
    assert 'key_state[0xb8]' in impl and 'key_state[0x0f]' in impl
    assert 'spider_alt_latch_policy_step(' in impl
    assert '*left==0x7fu' in impl
    assert impl.count('*left=0x80u;')==1
    assert 'SPIDER_ALT_LATCH_MIN_UP_FRAMES = 6' in model
    assert 'SPIDER_ALT_LATCH_MIN_UP_MS = 180' in model
    assert 'SPIDER_ALT_LATCH_MAX_AFTER_RETURN_MS = 5000' in model
    assert 'stable_up_since_ms' in model and 'stable_up_started' in model
    assert 'ms_since_return-p->stable_up_since_ms' in model
    assert 'p->pending=0u;' in model
    # A passing policy check does not permit a stale/cached modifier
    # reading to authorize a write if Tab, AltGr or original keyboard
    # state changed in the same Flip. All checks must be LIVE, inside
    # the final if(result==1) guard before the only native-byte write.
    final_guard=impl[impl.index('if(result==1) {'):
                     impl.index('*left=0x80u;')]
    assert 'GetForegroundWindow()==hwnd' in final_guard
    for key in ('VK_LMENU','VK_MENU','VK_RMENU','VK_TAB'):
        assert f'GetAsyncKeyState({key})' in final_guard
        assert impl.count(f'GetAsyncKeyState({key})')==2
    for code in ('0xb8','0x0f'):
        assert f'key_state[{code}]' in final_guard
        assert impl.count(f'key_state[{code}]')==2
    assert '*left==0x7fu' in final_guard
    assert 'os_modifiers_up && original_other_up' not in final_guard
    for forbidden in ("SendInput(", "keybd_event(", "VirtualProtect(",
                      "CreateThread(", "GetDeviceData(", "Acquire(",
                      "Unacquire(", "SetWindowsHookEx(", "IDirectInputDevice8_",
                      "memset(left", "memset(key_state", "GetDeviceState("):
        assert forbidden not in impl and forbidden not in model

def test_wired_to_confirmed_focus_pause_not_sdl3_sampling():
    core=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    validator=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert '#include "alt_latch_recovery.h"' in core
    assert '"ExperimentalAltLatchRecovery"' in validator
    assert 'SpiderAltLatch_Init(ini,log_line);' in core
    assert 'SpiderAltLatch_Init(NULL,log_line);' in core
    assert core.count('SpiderAltLatch_OnConfirmedExternalLoss();')==1
    focus=core[core.index('static void pause_at_flip_if_background(void)'):
               core.index('static HRESULT WINAPI hooked_flip(')]
    assert focus.index('pause_confirmed_focus_loss(hwnd)') < focus.index(
        'SpiderAltLatch_OnConfirmedExternalLoss();')
    assert focus.index('SpiderAltLatch_OnConfirmedExternalLoss();') < focus.index(
        'SpiderAltLatch_OnPauseReturn(hwnd);')
    assert 'SpiderAltLatch_OnFlip(game_hwnd,' in core
    flip=core[core.index('static HRESULT WINAPI hooked_flip('):]
    assert flip.index('pause_at_flip_if_background();') < flip.index(
        'SpiderAltLatch_OnFlip(game_hwnd,')
    assert flip.index('SpiderAltLatch_OnFlip(game_hwnd,') < flip.index(
        'SpiderSDL3GameplayPlan_OnFlip(game_hwnd,')
    source=(ROOT/"sdl3_native_source_trace.h").read_text(encoding="utf-8")
    assert 'SpiderAltLatch' not in source
    for path in ROOT.glob("*.ini"):
        ini=path.read_text(encoding="utf-8",errors="replace")
        assert 'ExperimentalAltLatchRecovery=1' not in ini
