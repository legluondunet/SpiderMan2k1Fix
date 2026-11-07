"""SDL3 recovery path for synchronous focus-PAUSED screen (PR #80).

These are source-contract checks, not substitutes for native Windows/Wine
hardware acceptance. Verify fail-closed ownership and no input injection.
"""
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]

def read(path):
    return (ROOT/path).read_text(encoding="utf-8")

def test_sdl3_wake_uses_selected_owner_and_same_game_thread():
    p=read("sdl3_gamepad_probe.c")
    h=read("sdl3_gamepad_probe.h")
    begin=p.split("int SpiderSDL3Probe_PauseWake_Begin(void) {",1)[1].split(
        "unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd)",1)[0]
    poll=p.split("unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd) {",1)[1].split(
        "void SpiderSDL3Probe_PauseWake_End(void)",1)[0]
    end=p.split("void SpiderSDL3Probe_PauseWake_End(void) {",1)[1].split(
        "/* Optional semantic preview",1)[0]
    assert 'GetProcAddress(module,"SDL_GamepadConnected")' in p
    assert "SDL3Probe_PauseWake_Begin" in h
    assert "SDL3Probe_PauseWake_Poll" in h
    assert "SDL3Probe_PauseWake_End" in h
    assert "sdl3_game_thread!=GetCurrentThreadId()" in begin
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" in begin
    assert "SpiderSDL3Probe_SelectedIdentity(&vid,&pid,&id)" in begin
    assert "SpiderNativeReconnect_SDL3SelectionReady(" not in begin
    assert "SpiderSDL3Probe_SelectedMappedSample(" in begin
    assert "sdl_connected(pads[i].handle)" in begin
    assert "GetForegroundWindow()!=hwnd" in poll
    assert poll.index("GetForegroundWindow()!=hwnd") < poll.index("sdl_pump();")
    assert "sdl3_game_thread!=GetCurrentThreadId()" in poll
    assert "id!=sdl_pause_id || vid!=sdl_pause_vid || pid!=sdl_pause_pid" in poll
    assert "sdl_pump();sdl_update();" in poll
    assert "sdl_connected(handle)" in poll
    assert "SPIDER_SDL3_BUTTON_START" in poll
    assert "SPIDER_SDL3_BUTTON_GUIDE" in poll
    assert "spider_pause_pad_wake_step(&sdl_pause_wake,buttons)" in poll
    assert "memset(&sdl_pause_wake,0,sizeof(sdl_pause_wake))" in poll
    assert "sdl_pause_active=0;" in poll
    assert "spider_sdl3_selection_focus_lost(&selection)" in end
    for forbidden in ("SendInput(", "IDirectInputDevice8_Acquire(",
                      "WriteProcessMemory(", "sdl_open(", "sdl_close(",
                      "rescan(", "begin_scancode("):
        assert forbidden not in begin+poll+end

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


def test_selected_universal_ps3_wake_is_strictly_digital_and_keeps_native_path():
    p=read("sdl3_gamepad_probe.c")
    begin=p.split("int SpiderSDL3Probe_PauseWake_Begin(void) {",1)[1].split(
        "unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd)",1)[0]
    poll=p.split("unsigned SpiderSDL3Probe_PauseWake_Poll(HWND hwnd) {",1)[1].split(
        "void SpiderSDL3Probe_PauseWake_End(void)",1)[0]
    assert "if(universal_requested)" in begin
    assert "SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)" in begin
    assert "sdl_pause_universal=1;" in begin
    assert "else if(!SpiderSDL3Probe_NativeOwnerEnabled()" in begin
    assert "SpiderNativeReconnect_SDL3SelectionReady(" not in begin
    assert "SpiderSDL3Probe_SelectedMappedSample(" in begin
    assert "sdl_pause_id=id;sdl_pause_vid=vid;sdl_pause_pid=pid;" in begin
    assert "selected universal mapped pad armed" in begin
    assert "GetForegroundWindow()!=hwnd" in poll
    assert poll.index("GetForegroundWindow()!=hwnd") < poll.index(
        "if(sdl_pause_universal)")
    assert "if(sdl_pause_universal)" in poll
    generic=poll.split("if(sdl_pause_universal) {",1)[1].split(
        "} else {",1)[0]
    assert "sdl_pump();sdl_update();" in generic
    assert "SpiderSDL3Probe_SelectedMappedSample(&id,&vid,&pid,&sample)" in generic
    assert "id!=sdl_pause_id || vid!=sdl_pause_vid || pid!=sdl_pause_pid" in generic
    assert "sample.buttons&(1u<<j)" in generic
    assert "SPIDER_SDL3_BUTTON_GUIDE" in generic
    assert "SPIDER_SDL3_BUTTON_START" in generic
    assert "spider_pause_pad_wake_step(&sdl_pause_wake,buttons)" in poll
    assert "sdl_pause_universal=0;" in poll
    for forbidden in ("SendInput(", "IDirectInputDevice8_Acquire(",
                      "WriteProcessMemory(", "sdl_open(", "sdl_close(",
                      "rescan(", "begin_scancode("):
        assert forbidden not in begin+poll
    # No change to keyboard Any-key wake or legacy native Sony/Xbox fallback.
    main=read("spiderman2k1fix.c")
    assert "pause_gamepad_available=pause_sdl3_available?1:SpiderGamepadUI_PauseWake_Begin();" in main
    assert "SpiderPauseAny_Completed()" in main


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
