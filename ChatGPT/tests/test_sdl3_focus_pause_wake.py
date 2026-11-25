"""SDL3 recovery path for synchronous focus-PAUSED screen (PR #80).

These are source-contract checks, not substitutes for native Windows/Wine
hardware acceptance. Verify fail-closed ownership and no input injection.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def read(path):
    return (ROOT/path).read_text(encoding="utf-8")

def test_sdl3_wake_uses_all_mapped_pads_and_same_game_thread():
    p=read("sdl3_gamepad_probe.c")
    begin=p.split("int SpiderSDL3Probe_PauseWake_Begin(void) {",1)[1].split("unsigned SpiderSDL3Probe_PauseWake_Poll",1)[0]
    poll=p.split("unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd) {",1)[1].split("void SpiderSDL3Probe_PauseWake_End(void)",1)[0]
    assert "sdl3_game_thread!=GetCurrentThreadId()" in begin
    assert "sdl3_game_thread!=GetCurrentThreadId()" in poll
    assert poll.index("GetForegroundWindow()!=hwnd") < poll.index("sdl_pump();")
    assert "spider_pause_pads_reset(&sdl_pause_pads)" in poll
    assert "rescan();" in poll
    assert "spider_sdl3_selected_instance_valid(opened,n,winner.id,winner.vid,winner.pid)" in poll
    assert "generic_selected_id=winner.id" in poll
    assert "activity_fresh_claim_id=0" in poll
    assert "SPIDER_SDL3_BUTTON_START" in poll and "SPIDER_SDL3_BUTTON_GUIDE" in poll
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id)" in begin
    assert "SpiderNativeReconnect_SDL3SelectionReady(vid,pid)" in begin
    for forbidden in ("SendInput(", "IDirectInputDevice8_Acquire(", "WriteProcessMemory("):
        assert forbidden not in begin+poll

def test_pause_prefers_sdl3_and_keeps_keyboard_and_di8_fallback():
    main=read("spiderman2k1fix.c")
    p=main.split("static void pause_at_flip_if_background(void) {",1)[1].split(
        "static HRESULT WINAPI hooked_flip(",1)[0]
    assert "SpiderPauseAny_Begin(hwnd)" in p
    assert "pause_sdl3_available=SpiderSDL3Probe_PauseWake_Begin();" in p
    assert "pause_gamepad_available=pause_sdl3_available?1:SpiderGamepadUI_PauseWake_Begin();" in p
    assert "pause_sdl3_available?" in p
    assert "SpiderSDL3Probe_PauseWake_Poll(hwnd)" in p
    assert "SpiderGamepadUI_PauseWake_Poll(hwnd)" in p
    assert p.count("SpiderSDL3Probe_PauseWake_End();")==2
    assert p.count("SpiderGamepadUI_PauseWake_End();")==2
    assert "SpiderPauseAny_Completed()" in p
    assert "pause_gamepad_available);" in p
    assert "SendInput(" not in p
    assert "IDirectInputDevice8_Acquire(" not in p


def test_focus_pause_composites_native_guide_and_resets_on_host_focus_loss():
    main=read("spiderman2k1fix.c")
    p=main.split("static void pause_at_flip_if_background(void) {",1)[1].split("static HRESULT WINAPI hooked_flip(",1)[0]
    assert p.index("SpiderSDL3ControllerSetup_DrawToSurface(borderless_back,&pause_content_rect)") < p.index("SpiderPauseSurface_Begin(")
    assert "SpiderSDL3Probe_PauseWake_Poll(NULL)" in p
    assert "x11_state!=1 && !fallback_wake_armed" in p

def test_universal_ps3_focus_pause_requires_neutral_fresh_press_and_release(tmp_path):
    import shutil
    import subprocess
    import textwrap
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"ps3_pause_wake.c"
    exe=tmp_path/"ps3_pause_wake"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "ps1_focus_pause_wake.h"
      int main(void) {
        SpiderPausePadWake w={0};
        /* Arrive with Cross held during Alt+Tab: never resume. */
        assert(!spider_pause_pad_wake_step(&w,1u));
        assert(!spider_pause_pad_wake_step(&w,1u));
        assert(!spider_pause_pad_wake_step(&w,0u));
        assert(w.seen_neutral);
        /* A new Cross press itself does not resume. */
        assert(!spider_pause_pad_wake_step(&w,1u));
        assert(!spider_pause_pad_wake_step(&w,1u));
        /* Only release after a fresh press emits the wake. */
        assert(spider_pause_pad_wake_step(&w,0u)==1u);
        /* Repeatable digital button control without injecting keys. */
        assert(!spider_pause_pad_wake_step(&w,1u<<2));
        assert(spider_pause_pad_wake_step(&w,0u)==(1u<<2));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_multi_pad_wake_handoff_hotplug_and_ambiguity(tmp_path):
    import shutil
    import subprocess
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"multi.c"
    exe=tmp_path/"multi"
    src.write_text(r"""
    #include <assert.h>
    #include "ps1_focus_pause_wake.h"
    int main(void) {
      SpiderPausePads s={0};
      SpiderPausePadSample p[2]={{1,1,1,0,1},{2,2,2,1,0}},w;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[1].buttons=0;p[1].neutral=1;
      assert(!spider_pause_pads_step(&s,p,2,&w)); /* inherited hold */
      p[1].buttons=2;p[1].neutral=0;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[1].buttons=0;p[1].neutral=1;
      assert(spider_pause_pads_step(&s,p,2,&w)==2 && w.id==2);
      spider_pause_pads_reset(&s);
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=1;p[0].neutral=0;p[1].buttons=2;p[1].neutral=0;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=0;p[0].neutral=1;p[1].buttons=0;p[1].neutral=1;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=1;p[0].neutral=0;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].id=3;p[0].buttons=0;p[0].neutral=1; /* hotplug cancels stale press */
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=4;p[0].neutral=0;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=0;p[0].neutral=1;
      assert(spider_pause_pads_step(&s,p,2,&w)==4 && w.id==3);
      spider_pause_pads_reset(&s); /* external focus resets pending */
      p[0].buttons=1;p[0].neutral=0;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      p[0].buttons=0;p[0].neutral=1;
      assert(!spider_pause_pads_step(&s,p,2,&w));
      return 0;
    }
    """,encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror","-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

def test_escape_diagnostic_is_passive_bounded_and_foreground_only():
    c=read("sdl3_universal_gameplay.c")
    f=c.split("void SpiderSDL3Universal_TraceEscape(",1)[1].split("/* Read-only candidate correlation",1)[0]
    assert f.index("GetForegroundWindow()!=hwnd") < f.index("GetAsyncKeyState(VK_ESCAPE)")
    assert "GetAsyncKeyState(VK_ESCAPE)&0x8000" in f
    assert "read_native_keyboard_latch(0x01u,&raw)" in f
    assert "reports>=8192u" in f and "tail=12u" in f
    assert "previous-pause=%d" in f
    assert "native_pause!=prior_pause" in f
    assert "raw==prior_raw" in f
    assert "diagnostic safety limit reached" in f
    for token in ("SendInput(", "send_scan(", "WriteProcessMemory(", "SetWindowsHookEx", "GetDeviceData(", "Acquire("):
        assert token not in f
    main=read("spiderman2k1fix.c")
    flip=main.split("static HRESULT WINAPI hooked_flip(void *self, IDirectDrawSurface7 *target, DWORD flags) {",1)[1]
    assert flip.index("SpiderSDL3Universal_TraceEscape(") < flip.index("SpiderSDL3Probe_OnFlip(")
    assert "if(debug_log_enabled)" in flip[:flip.index("SpiderSDL3Universal_TraceEscape(")]
    assert "pause_x11_bridge_state()==1" in flip[:flip.index("SpiderSDL3Probe_OnFlip(")]
