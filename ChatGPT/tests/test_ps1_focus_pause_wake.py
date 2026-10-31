"""Host-compiled regression for gamepad wake of custom focus PAUSED screen.

A manual DS4 test is still required: this unit tests digital press/release
ownership and guards that the paused Flip actually polls the pad.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[2]
CORE = (ROOT / "spiderman2k1fix.c").read_text(encoding="utf-8")
PAD = (ROOT / "gamepad_ui_profile.c").read_text(encoding="utf-8")
HDR = (ROOT / "gamepad_ui_profile.h").read_text(encoding="utf-8")
PURE = (ROOT / "ps1_focus_pause_wake.h").read_text(encoding="utf-8")
UI = (ROOT / "pause_surface.c").read_text(encoding="utf-8")

def pause_fn():
    return CORE[CORE.index("static void pause_at_flip_if_background(void)"):
                CORE.index("static HRESULT WINAPI hooked_flip(")]

def test_focus_only_pad_wake_and_no_native_menu_shortcut():
    p = pause_fn()
    assert "SpiderGamepadUI_PauseWake_Begin();" in p
    assert "SpiderGamepadUI_PauseWake_Poll(hwnd)" in p
    assert p.count("SpiderGamepadUI_PauseWake_End();") == 2
    assert p.index("SpiderPauseAny_Begin(hwnd)") < p.index("SpiderGamepadUI_PauseWake_Begin();")
    assert p.index("SpiderGamepadUI_PauseWake_Poll(hwnd)") < p.index("MsgWaitForMultipleObjectsEx(")
    assert "SpiderPauseAny_Completed()" in p
    assert "FOCUS-PAUSE: gamepad digital press/release" in p
    assert "SpiderGamepadUI_PauseWake_Poll" in HDR
    assert "PRESS KEY OR GAMEPAD BUTTON TO RESUME" in UI
    assert "PRESS ANY KEY TO RESUME" in UI
    assert "int SpiderGamepadUI_PauseWake_Begin(void)" in PAD
    assert "int available=configured && detected && device!=NULL;" in PAD
    assert "pause_wake_active=available;" in PAD
    assert "return available;" in PAD
    assert "pause_sdl3_available=SpiderSDL3Probe_PauseWake_Begin();" in p
    assert "pause_gamepad_available=pause_sdl3_available?1:SpiderGamepadUI_PauseWake_Begin();" in p
    assert "SpiderSDL3Probe_PauseWake_Poll(hwnd)" in p
    assert p.count("SpiderSDL3Probe_PauseWake_End();") == 2
    assert p.index("SpiderSDL3Probe_PauseWake_Begin();") < p.index(
        "SpiderGamepadUI_PauseWake_Begin();")
    assert p.index("SpiderSDL3Probe_PauseWake_Poll(hwnd)") < p.index(
        "MsgWaitForMultipleObjectsEx(")
    assert "no verified gamepad wake device; PAUSED hint remains keyboard-only" in p
    assert "(SpiderPauseBltFn)original_blt,\n                             pause_gamepad_available);" in p
    assert "if(pause_gamepad_available)" in p
    assert "no verified gamepad wake device; PAUSED hint remains keyboard-only" in p
    assert "begin_scancode(" not in p
    assert "SendInput(" not in p

def test_verified_ds4_secondary_device_only():
    assert '#include "ps1_focus_pause_wake.h"' in PAD
    assert "if(!configured || !detected || !device)" in PAD
    assert "GetForegroundWindow()!=hwnd" in PAD
    assert "IsWindowVisible(hwnd)" in PAD
    assert "IDirectInputDevice8_GetDeviceState(device,sizeof(state),&state)" in PAD
    assert "IDirectInputDevice8_Acquire(device)" in PAD
    assert "spider_pause_pad_wake_step(&pause_wake," in PAD
    assert "SpiderGamepadUI_PauseWake_End" in PAD
    assert "unsigned i,buttons=0;" in PAD
    assert "for(i=0;i<12u;i++)" in PAD
    assert "SpiderGamepadUI_OnFlip" in PAD
    assert "DI8DEVCLASS_GAMECTRL" in PAD
    for verboten in ("SendInput(", "GetAsyncKeyState(", "XInputSetState(",
                     "VirtualProtect(", "WriteProcessMemory(", "Sleep("):
        assert verboten not in PURE

def test_compiled_no_ghost_press_repeats_or_axis_wake(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"wake.c"
    binary=tmp_path/"wake"
    source.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "ps1_focus_pause_wake.h"
        int main(void) {
          SpiderPausePadWake w={0},reset={0};
          unsigned i;
          assert(!spider_pause_pad_wake_step(&w,1u)); /* held on focus return */
          assert(!spider_pause_pad_wake_step(&w,1u)); /* not yet neutral */
          assert(!spider_pause_pad_wake_step(&w,0u)); /* now armed */
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,2u)); /* fresh Circle down */
          for(i=0;i<20;i++)assert(!spider_pause_pad_wake_step(&w,2u));
          assert(!spider_pause_pad_wake_step(&w,3u)); /* new Cross while held */
          assert(spider_pause_pad_wake_step(&w,0u)==2u); /* all released */
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,1u));
          assert(spider_pause_pad_wake_step(&w,0u)==1u);
          /* Only DS4 digital 0..11, never any analog state/other bits. */
          memset(&w,0,sizeof(w));
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,0x1000u));
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,0x800u));
          assert(spider_pause_pad_wake_step(&w,0u)==0x800u);
          /* Failure/background clears incomplete wake sequence. */
          memset(&w,0,sizeof(w));
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,1u));
          memset(&w,0,sizeof(w)); /* simulated failed GetDeviceState */
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&w,0u));
          assert(!spider_pause_pad_wake_step(&reset,1u));
          assert(!spider_pause_pad_wake_step(&reset,0u));
          assert(!spider_pause_pad_wake_step(0,1u));
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),
                    str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_focus_gamepad_reacquire_exactly_once_only_after_foreground_notacquired():
    # A foreground DirectInput cooperative-level loss is not a hot-unplug.
    # A frozen Flip cannot call normal secondary UI OnFlip recovery, so a
    # single foreground-only Acquire on DIERR_NOTACQUIRED is necessary.
    p=PAD.split("unsigned SpiderGamepadUI_PauseWake_Poll(HWND hwnd) {",1)[1].split(
        "void SpiderGamepadUI_PauseWake_End(void)",1)[0]
    assert "GetForegroundWindow()!=hwnd" in p
    assert p.index("GetForegroundWindow()!=hwnd") < p.index("IDirectInputDevice8_Acquire(device)")
    assert "hr==DIERR_NOTACQUIRED && pause_wake_notacquired_attempts==0u" in p
    assert "pause_wake_notacquired_attempts=1u;" in p
    assert "acquire_hr=IDirectInputDevice8_Acquire(device);" in p
    assert "if(SUCCEEDED(acquire_hr))return 0;" in p
    assert "pause_wake_active=0;" in p
    assert "spider_pause_pad_wake_step(&pause_wake,buttons)" in p
    assert "if(FAILED(hr)) {" in p
    # Any physically lost peripheral (INPUTLOST/UNPLUGGED) is never
    # reacquired during PAUSED; no simultaneous device release/enumeration.
    assert "hr==DIERR_INPUTLOST" not in p
    assert "hr==DIERR_UNPLUGGED" not in p
    for bad in ("detect_device(", "disable_profile(", "SendInput(", 
                "SpiderCfg_Enable", "GetAsyncKeyState("):
        assert bad not in p
    assert 'pause_wake_notacquired_attempts=0;' in PAD
    assert 'single guarded Acquire=0x%08lX' in PAD
    assert 'secondary controller read unavailable hr=0x%08lX' in PAD
