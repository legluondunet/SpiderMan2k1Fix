"""SDL3 read-only multi-pad selection: compile portable policy on CI host."""
from pathlib import Path
import shutil
import subprocess
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not shutil.which('gcc'),reason='gcc required for C99 policy test')
def test_selection_compiles_and_handles_multi_pad_hotplug(tmp_path):
    src=tmp_path/'selection.c'
    src.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "sdl3_gamepad_selection.h"
int main(void) {
    SpiderSDL3Selection s={0};
    const uint32_t one[]={11}, two[]={11,22}, three[]={11,22,33};
    const uint32_t other[]={22,33}, singleton[]={33}, dup[]={33,33};
    spider_sdl3_selection_sync(&s,NULL,0);
    assert(s.count==0 && s.active==0);
    spider_sdl3_selection_sync(&s,one,1);
    assert(s.count==1 && s.active==11);
    spider_sdl3_selection_sync(&s,two,2);
    assert(s.count==2 && s.active==11); /* don't steal focus */
    /* A fresh Start on another pad explicitly takes ownership. */
    spider_sdl3_selection_start(&s,22,0);
    assert(spider_sdl3_selection_start(&s,22,1)); /* 11 -> 22 */
    assert(s.active==22);
    assert(!spider_sdl3_selection_start(&s,22,1)); /* no held-button repeat */
    assert(!spider_sdl3_selection_start(&s,11,1)); /* held while unarmed */
    assert(!spider_sdl3_selection_start(&s,11,0)); /* neutral arms */
    assert(spider_sdl3_selection_start(&s,11,1));  /* 22 -> 11 */
    assert(s.active==11);
    spider_sdl3_selection_sync(&s,three,3);
    assert(s.active==11);
    spider_sdl3_selection_sync(&s,other,2); /* active 11 unplugged */
    assert(s.active==0);
    assert(!spider_sdl3_selection_start(&s,22,1)); /* held Start can't claim */
    assert(!spider_sdl3_selection_start(&s,22,0));
    assert(spider_sdl3_selection_start(&s,22,1));
    assert(s.active==22);
    spider_sdl3_selection_sync(&s,singleton,1);
    assert(s.count==1 && s.active==33); /* auto fallback */
    spider_sdl3_selection_sync(&s,dup,2);
    assert(s.count==1 && s.active==33); /* duplicate ignored */
    spider_sdl3_selection_sync(&s,NULL,0);
    assert(s.active==0 && s.count==0);
    /* Fresh multi-pad session must release Start before it can claim. */
    memset(&s,0,sizeof(s));
    spider_sdl3_selection_sync(&s,two,2);
    assert(!spider_sdl3_selection_start(&s,11,1));
    assert(!spider_sdl3_selection_start(&s,11,0));
    assert(spider_sdl3_selection_start(&s,11,1));
    assert(s.active==11);
    return 0;
}
''')
    exe=tmp_path/'selection'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT),str(src),'-o',str(exe)],
                   capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)

def test_sdl3_probe_is_optional_read_only_and_exe_relative():
    code=(ROOT/'sdl3_gamepad_probe.c').read_text()
    main=(ROOT/'spiderman2k1fix.c').read_text()
    shell=(ROOT/'build.sh').read_text()
    ini=(ROOT/'ini_validation.h').read_text()
    assert 'ExperimentalSDL3Probe' in ini
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in code
    assert 'SpiderSDL3Probe_OnFlip(count,game_hwnd);' in main
    assert 'SpiderSDL3Probe_Init(ini,log_line);' in main
    assert 'sdl3_gamepad_probe.c' in shell
    assert 'SDL_GetGamepads' in code and 'SDL_GetJoysticks' in code
    assert 'SDL_GetGamepadButton' in code and 'SDL_GetGamepadAxis' in code
    assert 'SDL_GetGamepadVendorForID' in code
    assert 'SDL_GetGamepadProductForID' in code
    assert 'selected-VID:PID=%04X:%04X' in code
    assert 'sdl3_trace_native_owner(now);' in code
    assert 'SPIDER_NATIVE_JOYSTICK_PTR 0x006B7A2Cu' in code
    assert 'NATIVE-OWNER physical-opened=' in code
    assert '(read-only; NOT bridged to SDL3)' in code
    # The SDL3 probe must not replace the original DirectInput pointer.
    assert 'InterlockedExchangePointer(' not in code
    assert 'spider_sdl3_drop_absent(pads,&num_pads,ids,n,' in code
    assert 'sdl_close,sdl3_detached)' in code
    assert 'strcpy(slash+1,"SDL3.dll");' in code
    assert 'LoadLibraryA(path)' in code
    assert 'SpiderGamepadUI_OnFlip' in main
    for forbidden in ('SendInput(', 'WriteProcessMemory(', 'SPIDER_NATIVE_ACTION_TABLE',
                      'SetWindowLong', 'IDirectInputDevice8_'):
        assert forbidden not in code

@pytest.mark.skipif(not shutil.which('gcc'),reason='gcc required for C99 slot lifecycle test')
def test_sdl3_detach_closes_exactly_once_and_compacts_slots(tmp_path):
    """Regression: repeated rescan after unplug must not re-close stale SDL handle."""
    src=tmp_path/'slots.c'
    src.write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include "sdl3_gamepad_slots.h"
static unsigned closes=0,notices=0;
static uint32_t last_detached=0;
static void close_device(void *p) {assert(p);closes++;}
static void report_detach(uint32_t id) {notices++;last_detached=id;}
int main(void) {
    unsigned count=2;
    int tokens[3]={0};
    const uint32_t two[]={11,22},one[]={22},next[]={22,33};
    SpiderSDL3Opened pads[SPIDER_SDL3_MAX_GAMEPADS]={{0}};
    pads[0].id=11;pads[0].handle=&tokens[0];
    pads[1].id=22;pads[1].handle=&tokens[1];
    assert(spider_sdl3_drop_absent(pads,&count,two,2,close_device,report_detach)==0);
    assert(count==2 && closes==0 && notices==0);
    assert(spider_sdl3_drop_absent(pads,&count,one,1,close_device,report_detach)==1);
    assert(count==1 && pads[0].id==22 && pads[0].handle==&tokens[1]);
    assert(pads[1].id==0 && pads[1].handle==0);
    assert(closes==1 && notices==1 && last_detached==11);
    /* The old loop reported detached=11 on EVERY scan. */
    for(unsigned i=0;i<100;i++) {
      assert(spider_sdl3_drop_absent(pads,&count,one,1,close_device,report_detach)==0);
      assert(count==1 && closes==1 && notices==1);
    }
    pads[1].id=33;pads[1].handle=&tokens[2];count=2;
    assert(spider_sdl3_drop_absent(pads,&count,next,2,close_device,report_detach)==0);
    assert(count==2 && pads[0].id==22 && pads[1].id==33);
    assert(spider_sdl3_drop_absent(pads,&count,NULL,0,close_device,report_detach)==2);
    assert(count==0 && closes==3 && notices==3 && last_detached==33);
    assert(spider_sdl3_drop_absent(pads,&count,NULL,0,close_device,report_detach)==0);
    assert(closes==3);
    /* An already-empty slot is pruned without a null close. */
    count=2;pads[0].id=0;pads[0].handle=NULL;
    pads[1].id=55;pads[1].handle=&tokens[0];
    const uint32_t fiftyfive[]={55};
    assert(spider_sdl3_drop_absent(pads,&count,fiftyfive,1,close_device,report_detach)==0);
    assert(count==1 && pads[0].id==55 && pads[0].handle==&tokens[0]);
    assert(closes==3 && notices==3);
    return 0;
}
''')
    exe=tmp_path/'slots'
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror',
                    '-I',str(ROOT),str(src),'-o',str(exe)],
                   capture_output=True,text=True,check=True)
    subprocess.run([str(exe)],check=True)
