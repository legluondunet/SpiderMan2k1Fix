"""PR #88: two independent original DirectInput latch observations.

Portable policy/static regression tests, not hardware validation.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap
import pytest

ROOT=Path(__file__).resolve().parents[2]

@pytest.mark.skipif(not (shutil.which("gcc") or shutil.which("cc")),
                    reason="C99 compiler unavailable")
def test_keyboard_counts_joystick_bitsets_and_reserved_focus_events(tmp_path):
    source=tmp_path/"sources.c"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_native_source_observer.h"
      int main(void) {
        uint8_t keyboard[256]={0},joystick[32]={0};
        SpiderNativeSourceSnapshot s={0};
        SpiderNativeSourceObserver o={0};
        keyboard[30]=0xff; keyboard[31]=0x01;
        keyboard[42]=0x80;
        joystick[0]=0xff; joystick[12]=0x02;
        joystick[31]=0x80;
        spider_native_source_decode(keyboard,joystick,&s);
        assert(s.keyboard_held==2&&s.keyboard_fresh==1);
        assert(s.keyboard_released==1);
        assert(s.joystick_held==((1u<<0)|(1u<<12)));
        assert(s.joystick_fresh==1u&&s.joystick_released==(1u<<31));
        assert(spider_native_source_changed(&o,&s,2u));
        assert(!spider_native_source_changed(&o,&s,2u));
        assert(spider_native_source_changed(&o,&s,1u));
        assert(o.emitted==2u);
        assert(spider_native_source_focus_step(&o,1));
        assert(!spider_native_source_focus_step(&o,1));
        assert(spider_native_source_focus_step(&o,0));
        assert(!o.have_previous);
        assert(spider_native_source_focus_step(&o,1));
        assert(spider_native_source_changed(&o,&s,2u));
        o.emitted=SPIDER_SOURCE_MAX_CHANGES;
        assert(!spider_native_source_changed(&o,&s,3u));
        /* Focus status is still logged at the ordinary sample cap. */
        assert(spider_native_source_focus_step(&o,0));
        assert(spider_native_source_focus_step(&o,1));
        assert(o.emitted==SPIDER_SOURCE_MAX_CHANGES);
        while(o.focus_events<SPIDER_SOURCE_MAX_FOCUS_EVENTS)
          spider_native_source_focus_step(&o,!o.was_active);
        assert(!spider_native_source_focus_step(&o,!o.was_active));
        assert(!spider_native_source_focus_step(0,0));
        assert(!spider_native_source_changed(0,&s,0));
        assert(!spider_native_source_changed(&o,0,0));
        spider_native_source_decode(0,joystick,&s);
        assert(s.keyboard_held==0&&s.joystick_held==0);
        spider_native_source_reset(&o);
        assert(!o.emitted&&!o.focus_events&&!o.have_focus);
        return 0;
      }
    """),encoding="utf-8")
    binary=tmp_path/"sources"
    subprocess.run([shutil.which("gcc") or shutil.which("cc"),
                    "-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(source),"-o",str(binary)],check=True,
                   capture_output=True,text=True)
    subprocess.run([str(binary)],check=True)

def test_runtime_is_read_only_fail_closed_and_distinctly_opted_in():
    trace=(ROOT/"sdl3_native_source_trace.h").read_text(encoding="utf-8")
    model=(ROOT/"sdl3_native_source_observer.h").read_text(encoding="utf-8")
    assert 'GetPrivateProfileIntA("Debug","SDL3NativeSourceTrace",0,ini)==1' in trace
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in trace
    assert 'spider_source_trace_binary_ok()' in trace
    assert '0x00501C56u' in trace and '0x00501F13u' in trace
    assert '0x00479540u' in trace
    assert 'GetForegroundWindow()==hwnd' in trace
    assert '!bink&&!paused' in trace
    assert 'spider_native_source_focus_step(' in trace
    assert 'spider_source_trace_observer.emitted>=SPIDER_SOURCE_MAX_CHANGES' in trace
    assert 'const volatile uint8_t *slot=' in trace
    assert 'const volatile uint8_t *)(uintptr_t)0x006B792Cu' in trace
    assert 'const volatile uint8_t *)(uintptr_t)0x006B7A34u' in trace
    assert 'CORRELATION NOT PROVENANCE' in trace
    assert 'keyboard_held++' in model and 'keyboard_fresh++' in model
    assert 'kb-code=' not in trace  # do not log individual scan codes
    for forbidden in ("WriteProcessMemory","VirtualProtect","SendInput",
                      "GetDeviceData(","Poll(","Acquire(","CreateThread(",
                      "IDirectInputDevice8_", "SetWindowsHookEx", "keycode="):
        assert forbidden not in trace and forbidden not in model
    for forbidden in ("windows.h","0x006B792C","0x006B7A34"):
        assert forbidden not in model

def test_wiring_is_independent_of_existing_gameplay_and_defaults_off():
    runtime=(ROOT/"sdl3_gameplay_plan_probe.c").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert '#include "sdl3_native_source_trace.h"' in runtime
    assert 'spider_source_trace_init(ini,logger);' in runtime
    assert 'spider_source_trace_flip(hwnd,menu2d,gameplay3d,bink,pause);' in runtime
    assert runtime.index('SpiderSDL3GameplayLive_OnFlip(hwnd,') < runtime.index(
        'spider_source_trace_flip(hwnd,') < runtime.index('if(!requested)return;')
    assert '"SDL3NativeSourceTrace"' in ini
    for path in ROOT.glob("*.ini"):
        assert 'SDL3NativeSourceTrace=1' not in path.read_text(
            encoding="utf-8",errors="replace")
