"""SDL3 setup Win32 single-cursor and explicit remap action invariants."""
from pathlib import Path
import shutil
import subprocess
import tempfile

import pytest

ROOT=Path(__file__).resolve().parents[2]
UI=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
FOCUS=(ROOT/"sdl3_controller_setup_focus.h").read_text(encoding="utf-8")


def test_only_one_selection_cursor_and_use_is_not_action_capture():
    assert "static void setup_sync_visual_selection(" in UI
    assert "SpiderMan2k1FixSDL3ActionView" in UI
    assert "LBS_OWNERDRAWFIXED" not in UI
    assert "SPIDER_SETUP_BUTTON_USE" in FOCUS
    assert "SPIDER_SETUP_BUTTON_COUNT=6" in FOCUS
    assert "SPIDER_UI_USE;" in UI
    assert "SPIDER_SETUP_BUTTON_RAW+LOWORD(wp)-SPIDER_UI_RAW" in UI
    assert "HIWORD(wp)==BN_SETFOCUS" in UI
    assert UI.count("WS_TABSTOP|BS_PUSHBUTTON|BS_NOTIFY")>=3
    selector=UI.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;" in selector
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in selector
    assert "SetFocus(ui->actions)" in selector
    assert "EXPLICIT-SELECT already queued; focus returned to actions" in selector
    assert "ui->start_owner_requested=1u;" in selector
    assert "spider_sdl3_capture_start(" not in selector
    nav=UI.split("static void sample_device(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    # Deliberate Cross on the selected action enters remapping, after
    # which a fresh neutral is mandatory before taking Circle or Square.
    action_only=nav.split("} else if(current_profile(ui)) {",1)[1].split(
        "} else if(edge==(1u<<SPIDER_SETUP_NAV_EAST))",1)[0]
    assert "spider_sdl3_capture_start(&ui->capture);" in action_only
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in action_only
    assert "ACTION-ROW CROSS CAPTURE ARMED" in action_only
    assert "neutral required" in action_only
    assert "Circle allowed" in action_only
    keyboard=UI.split("static int handle_setup_keyboard(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "Ctrl+Shift+C" in UI
    assert "if(vk=='C'" in keyboard
    assert "if(focused==ui->actions)" in keyboard
    assert "if(focused==ui->actions && current_profile(ui))" not in keyboard
    # No direct remapping by Enter/Space unless an explicit Capture button
    # (or Ctrl+Shift+C) was invoked.
    assert "if(ui->focus.column!=SPIDER_SETUP_FOCUS_ACTIONS)" in keyboard
    assert "Action selected. Press Enter to capture" not in UI


@pytest.mark.skipif(not (shutil.which("cc") or shutil.which("gcc")),
                    reason="No C compiler")
def test_six_button_focus_navigation_in_real_portable_policy():
    source=r"""
#include <assert.h>
#include "sdl3_controller_setup_focus.h"
int main(void) {
  SpiderSDL3SetupFocus f={0};
  unsigned i;
  assert(SPIDER_SETUP_BUTTON_COUNT==6);
  spider_sdl3_setup_focus_reset(&f);
  assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_RIGHT));
  assert(f.column==SPIDER_SETUP_FOCUS_BUTTONS);
  for(i=0;i<5;i++)
    assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_DOWN));
  assert(f.button==SPIDER_SETUP_BUTTON_USE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_DOWN));
  assert(f.button==SPIDER_SETUP_BUTTON_CAPTURE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_UP));
  assert(f.button==SPIDER_SETUP_BUTTON_USE);
  assert(spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_LEFT));
  assert(f.column==SPIDER_SETUP_FOCUS_ACTIONS);
  assert(!spider_sdl3_setup_focus_step(&f,1u<<SPIDER_SETUP_NAV_LEFT));
  return 0;
}
"""
    with tempfile.TemporaryDirectory() as tmp:
        src=Path(tmp)/"focus.c"
        exe=Path(tmp)/"focus"
        src.write_text(source,encoding="utf-8")
        subprocess.run([shutil.which("cc") or shutil.which("gcc"),
            "-std=c11","-Wall","-Wextra","-Werror",
            "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)


def test_ps3_cross_on_explicit_capture_button_arms_only_neutral_gated_rebind():
    """After single-focus fix, allow true controller-only remapping.
    Safety: both a deliberate action-row Cross and a genuinely focused
    Capture button can enter remapping; never synthesize a Win32 click."""
    nav=UI.split("static void sample_device(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    south=nav.split("edge==(1u<<SPIDER_SETUP_NAV_SOUTH)",1)[1].split(
        "} else if(edge==(1u<<SPIDER_SETUP_NAV_EAST))",1)[0]
    assert "id==SPIDER_UI_CAPTURE" in south
    assert "GetFocus()==GetDlgItem(ui->hwnd,SPIDER_UI_CAPTURE)" in south
    assert "current_profile(ui)" in south
    assert "spider_sdl3_capture_start(&ui->capture);" in south
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in south
    assert "ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;" in south
    assert "SetFocus(ui->actions)" in south
    assert "CROSS-CAPTURE ARMED" in south
    assert "neutral required; no immediate binding" in south
    assert "id==SPIDER_UI_RESET" in south
    assert "CROSS-RESET defaults SAVED" in south
    assert "GetFocus()==GetDlgItem(ui->hwnd,SPIDER_UI_RESET)" in south
    assert "ACTION-ROW CROSS CAPTURE ARMED" in south
    # The focused button used to cause duplicate synthetic clicks in Proton.
    assert "MAKEWPARAM(id,BN_CLICKED)" not in south.split(
        "if(id==SPIDER_UI_CAPTURE)",1)[1].split(
        "} else if(id==SPIDER_UI_RESET)",1)[0]
    cmds=UI.split("case WM_COMMAND:",1)[1].split("case WM_TIMER:",1)[0]
    assert "LOWORD(wp)==SPIDER_UI_CAPTURE && HIWORD(wp)==BN_CLICKED" in cmds
    assert "setup_physical_south_held(ui)" in cmds
    assert "SYNTHETIC CROSS CAPTURE CLICK SUPPRESSED" in cmds
    assert "current_profile(ui) && !ui->capture.active" in cmds
    assert "spider_sdl3_capture_start(&ui->capture);" in cmds
    keyboard=UI.split("static int handle_setup_keyboard(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "if(vk=='C'" in keyboard
    assert "setup_physical_south_held(ui)" in keyboard
    assert "Ctrl+Shift+C" in UI
    assert "Cross/A on action row OR Capture: remap; Circle allowed" in UI
    assert "F9/F10 unchanged" not in UI.split(
        "case WM_CREATE:",1)[1].split("case WM_COMMAND:",1)[0]


def test_redundant_use_button_restores_single_cursor_again():
    selector=UI.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    queued=selector.split("if(ui->start_owner_requested &&",1)[1].split(
        "ui->start_owner_id=id;",1)[0]
    assert "ui->focus.column=SPIDER_SETUP_FOCUS_ACTIONS;" in queued
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in queued
    assert "SetFocus(ui->actions)" in queued
    assert "setup_sync_visual_selection(ui);" in queued
    assert "EXPLICIT-SELECT already queued; focus returned to actions" in queued
    assert "return 1;" in queued


def test_single_action_selection_uses_no_native_listbox_caret():
    """Regression: screenshot 20261102-005854 showed Jump and Restore focus."""
    sync=UI.split("static void setup_sync_visual_selection(",1)[1].split(
        "static LRESULT CALLBACK setup_action_view_proc(",1)[0]
    view=UI.split("static LRESULT CALLBACK setup_action_view_proc(",1)[1].split(
        "static void setup_normalize_default_buttons(",1)[0]
    assert "InvalidateRect(ui->actions,NULL,TRUE)" in sync
    assert "LB_SETCARETINDEX" not in sync
    assert "LB_SETCURSEL" not in sync
    assert "WM_PAINT" in view and "BeginPaint(hwnd,&ps)" in view
    assert "GetFocus()==hwnd && ui->action==i" in view
    assert "ui->focus.column==SPIDER_SETUP_FOCUS_ACTIONS" in view
    assert "FillRect(dc,&cell,GetSysColorBrush(COLOR_HIGHLIGHT))" in view
    assert "DrawFocusRect" not in view
    assert "setup_sync_visual_selection(ui);" in UI.split(
        "static void refresh_actions(",1)[1].split(
        "static int commit_profile(",1)[0]
    assert "setup_sync_visual_selection(ui);" in UI.split(
        "static void setup_apply_focus(",1)[1].split(
        "static void setup_close_from_pad(",1)[0]


def test_proton_synthesized_arrow_cannot_be_processed_twice_by_win32_and_sdl3():
    """When the physical PS3 stick/D-pad moves, only SDL3 owns navigation;
    keyboard arrows remain usable when the hardware is neutral."""
    helper=UI.split("static int setup_suppress_duplicate_arrow(",1)[1].split(
        "/* One keyboard path independent",1)[0]
    assert "SpiderSDL3Probe_SetupPump()" in helper
    assert "SpiderSDL3Probe_SetupRead(" in helper
    assert "id!=ui->instance || vid!=ui->vid || pid!=ui->pid" in helper
    assert "SPIDER_SDL3_AXIS_LEFTX" in helper
    assert "SPIDER_SDL3_AXIS_LEFTY" in helper
    for button in ("UP","DOWN","LEFT","RIGHT"):
        assert "SPIDER_SDL3_BUTTON_DPAD_"+button in helper
    assert "ui->suppressed_analog_vk|=mask" in helper
    assert "ui->suppressed_analog_vk&=~mask" in helper
    assert "SDL3-SETUP: DUPLICATE-WIN32-ARROW SUPPRESSED" in helper
    assert "return 0;" in helper
    loop=UI.split("while(IsWindow(window)) {",1)[1].split(
        'SDL3-SETUP: modal message loop EXIT',1)[0]
    assert "setup_suppress_duplicate_arrow(&ui,&msg)" in loop
    assert loop.index("setup_suppress_duplicate_arrow(&ui,&msg)") < loop.index(
        "handle_setup_keyboard(&ui,(UINT)msg.wParam)")
    assert loop.index("setup_suppress_duplicate_arrow(&ui,&msg)") < loop.index(
        "IsDialogMessageA(window,&msg)")
    # Never intercept F9/F10 or the raw physical Escape handling.
    assert "vk<VK_LEFT || vk>VK_DOWN" in UI
    assert "ui->capture.active || ui->test_mode" in helper
    # No stuck blocked keyboard key if Proton omits synthesized KEYUP.
    assert UI.count("ui->suppressed_analog_vk=0u;") >= 4
    assert "Some Proton implementations do not deliver paired synthetic KEYUP" in UI
    assert "new profile invalidates prior claim" in UI


def test_previously_explicit_ps3_is_previewed_without_native_ownership():
    """PS3 054C:0268 stays the displayed profile on reopening SDL3 setup."""
    refresh=UI.split("static void refresh_pads(",1)[1].split(
        "static int modal_start_handoff(",1)[0]
    assert "SpiderSDL3Probe_SelectedIdentity(" in refresh
    assert "SpiderSDL3Probe_GenericSelectedSample(" in refresh
    assert refresh.index("SpiderSDL3Probe_SelectedIdentity(") < refresh.index(
        "SpiderSDL3Probe_GenericSelectedSample(")
    assert "previous-explicit-generic-selection" in refresh
    assert "id==gameplay_id" in refresh
    assert "vid==gameplay_vid && pid==gameplay_pid" in refresh
    assert "SELECT-UNCONFIRMED multiple pads" in refresh
    # A preview cannot claim game-owned DirectInput.
    assert "SpiderSDL3Probe_SetupModalReturn(" not in refresh
    assert "NATIVE-OWNER" not in refresh


def test_start_selection_is_visible_in_native_modal_instructions():
    modal=UI.split("case WM_CREATE:",1)[1].split("case WM_COMMAND:",1)[0]
    assert "Press START on the gamepad you want to SELECT" in modal
    start=UI.split("static int modal_start_handoff(",1)[1].split(
        "static int setup_select_explicit_pad(",1)[0]
    assert "START SELECTED controller" in start
    assert "status(ui,line);" in start
    assert "ui->start_owner_requested=1u;" in start


def test_only_actual_win32_button_has_a_visible_default_border():
    """Do not leave a heavy default border on Use while Close has focus."""
    helper=UI.split("static void setup_normalize_default_buttons(",1)[1].split(
        "static void setup_apply_focus(",1)[0]
    assert "GWL_STYLE" in helper
    assert "(style&BS_TYPEMASK)==BS_DEFPUSHBUTTON" in helper
    assert "BM_SETSTYLE,BS_PUSHBUTTON|BS_NOTIFY,TRUE" in helper
    assert "SPIDER_UI_CLOSE" in helper and "SPIDER_UI_USE" in helper
    assert "STALE DEFAULT BUTTON BORDER removed" in helper
    focus=UI.split("static void setup_apply_focus(",1)[1].split(
        "static void setup_close_from_pad(",1)[0]
    assert "SetFocus(target);" in focus
    assert "setup_normalize_default_buttons(ui);" in focus
    assert "setup_normalize_default_buttons(ui);" in UI.split(
        "case WM_COMMAND:",1)[1].split("case WM_TIMER:",1)[0]
    loop=UI.split("while(IsWindow(window)) {",1)[1].split(
        "SDL3-SETUP: modal message loop EXIT",1)[0]
    assert "if(IsDialogMessageA(window,&msg))" in loop
    assert "setup_normalize_default_buttons(&ui);" in loop
    # Win32 keyboard/Tab/Space semantics still go through the native path.
    assert "TranslateMessage(&msg);DispatchMessageA(&msg);" in loop


def test_custom_view_is_mouse_keyboard_and_gamepad_accessible():
    """The 3D-action view must not reintroduce a Win32 selection/caret."""
    view=UI.split("static LRESULT CALLBACK setup_action_view_proc(",1)[1].split(
        "static void setup_normalize_default_buttons(",1)[0]
    create=UI.split("case WM_CREATE:",1)[1].split("case WM_COMMAND:",1)[0]
    wndproc=UI.split("static LRESULT CALLBACK setup_wndproc(",1)[1].split(
        "void SpiderSDL3ControllerSetup_Open(",1)[0]
    refresh=UI.split("static void refresh_actions(",1)[1].split(
        "static int commit_profile(",1)[0]
    assert 'CreateWindowExA(WS_EX_CLIENTEDGE,' in create
    assert '"SpiderMan2k1FixSDL3ActionView",""' in create
    assert "WS_CHILD|WS_VISIBLE|WS_TABSTOP" in create
    assert '"LISTBOX"' not in create
    assert "LBS_" not in create
    assert "LB_" not in create
    assert "case WM_LBUTTONDOWN:" in view
    assert "SetFocus(hwnd);" in view
    assert "ui->action=idx;" in view
    assert "idx>=ui->action_count" in view
    assert "MAKEWPARAM(SPIDER_UI_ACTION,LBN_SELCHANGE)" in view
    assert "case WM_SETFOCUS:" in view
    assert "case WM_KILLFOCUS:" in view
    assert "WM_GETDLGCODE:" in view
    assert "case WM_PAINT:" in view
    assert "case WM_SETFONT:" in view
    assert "DrawTextA(" in view
    assert "ui->action_labels[i]" in view
    assert "ui->action_count++;" in refresh
    assert "snprintf(ui->action_labels[i]" in refresh
    assert "HIWORD(wp)==LBN_SETFOCUS" in wndproc
    assert "HIWORD(wp)==LBN_KILLFOCUS" in wndproc
    assert "LBN_SELCHANGE" in wndproc
    assert "HIWORD(wp)==BN_SETFOCUS" in wndproc
    assert "SetFocus(target);" in UI
    assert "if(IsDialogMessageA(window,&msg))continue;" in UI
    assert "SendInput(" not in UI


def test_circle_can_be_captured_without_triggering_proton_escape_or_close():
    """PS3 054C:0268 repro 2026-11-02: action mapped to Square
    must be reassignable to Circle, swapping the old 3D binding."""
    nav=UI.split("static void sample_device(",1)[1].split(
        "static int setup_suppress_duplicate_arrow(",1)[0]
    assert "if(!ui->capture.active) {" in nav
    assert "edge==(1u<<SPIDER_SETUP_NAV_EAST)" in nav
    capture=nav.split("rc=spider_sdl3_capture_feed(",1)[1]
    assert "candidate.index==1u" in capture
    assert "ui->last_captured_east_ms=GetTickCount()" in capture
    assert "spider_sdl3_profile_assign_visible(&trial,ui->action,candidate)" in capture
    helper=UI.split("static int setup_suppress_synthetic_east_escape(",1)[1].split(
        "static int handle_setup_keyboard(",1)[0]
    assert "msg->wParam!=VK_ESCAPE" in helper
    assert "ui->capture.active" in helper
    assert "sample.buttons&(1u<<1)" in helper
    assert "ui->last_captured_east_ms" in helper
    assert "elapsed<350u" in helper
    assert "PROTON SYNTHETIC ESC SUPPRESSED" in helper
    loop=UI.split("while(IsWindow(window)) {",1)[1].split(
        "SDL3-SETUP: modal message loop EXIT",1)[0]
    assert "setup_suppress_synthetic_east_escape(&ui,&msg)" in loop
    assert loop.index("setup_suppress_synthetic_east_escape(&ui,&msg)") < loop.index(
        "handle_setup_keyboard(&ui,(UINT)msg.wParam)")
    assert loop.index("setup_suppress_synthetic_east_escape(&ui,&msg)") < loop.index(
        "IsDialogMessageA(window,&msg)")
    assert "VK_ESCAPE" in UI


def test_ps3_cross_restores_default_profile_from_actual_focused_reset_button():
    """GE-Proton hardware log 20261102-013108:
    Cross on Restore defaults was blocked, F5 alone worked."""
    nav=UI.split("static void sample_device(",1)[1].split(
        "static int setup_suppress_duplicate_arrow(",1)[0]
    south=nav.split("edge==(1u<<SPIDER_SETUP_NAV_SOUTH)",1)[1].split(
        "} else if(edge==(1u<<SPIDER_SETUP_NAV_EAST))",1)[0]
    reset=south.split("} else if(id==SPIDER_UI_RESET)",1)[1].split(
        "} else {",1)[0]
    assert "GetFocus()==GetDlgItem(ui->hwnd,SPIDER_UI_RESET)" in reset
    assert "current_profile(ui)" in reset
    assert "spider_sdl3_profile_default(&trial,ui->vid,ui->pid)" in reset
    assert 'commit_profile(ui,&trial,"Defaults")' in reset
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in reset
    assert "CROSS-RESET defaults SAVED" in reset
    assert "CROSS-RESET FAILED" in reset
    assert "CROSS-RESET REFUSED" in reset
    assert "PostMessageA(ui->hwnd,WM_COMMAND" not in reset
    assert "SendMessageA(ui->hwnd,WM_COMMAND" not in reset
    commands=UI.split("case WM_COMMAND:",1)[1].split(
        "case WM_TIMER:",1)[0]
    win32=commands.split(
        "LOWORD(wp)==SPIDER_UI_RESET && HIWORD(wp)==BN_CLICKED",1)[1].split(
        "LOWORD(wp)==SPIDER_UI_RAW && HIWORD(wp)==BN_CLICKED",1)[0]
    assert "if(setup_physical_south_held(ui))" in win32
    assert "SYNTHETIC CROSS RESET CLICK SUPPRESSED" in win32
    assert "spider_sdl3_profile_default(&trial,ui->vid,ui->pid)" in win32
    assert 'commit_profile(ui,&trial,"Defaults")' in win32
    keyboard=UI.split("static int handle_setup_keyboard(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "VK_F5" in keyboard
    assert 'commit_profile(ui,&trial,"Defaults")' in keyboard
    enter=keyboard.split("if(vk==VK_RETURN)",1)[1].split(
        "if(vk==VK_F5)",1)[0]
    assert "SPIDER_UI_RESET" in enter and "setup_physical_south_held(ui)" in enter
    assert "MAKEWPARAM(id,BN_CLICKED)" in enter
