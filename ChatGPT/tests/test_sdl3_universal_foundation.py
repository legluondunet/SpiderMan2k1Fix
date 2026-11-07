"""Universal SDL3 foundation: hardware-neutral semantics and raw-pad visibility.

No physical gamepad or Spider-Man executable is required for these tests.
A live 3D bridge and a raw-axis mapping wizard remain separate future stages.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_unmapped_joysticks_are_reported_without_unsafe_native_claim():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert '"SDL_GetJoysticks"' in probe
    assert '"SDL_AddGamepadMappingsFromFile"' in probe
    assert 'SDL3-gamecontrollerdb.txt' in probe
    assert "SDL3-MAPPINGS: external model database" in probe
    assert '"SDL_IsGamepad"' in probe
    assert '"SDL_GetJoystickNameForID"' in probe
    assert "SDL3-RAW: unmapped joystick" in probe
    assert "raw mapping wizard required" in probe
    assert "raw_last_ids" in probe
    assert '"SDL_OpenJoystick"' in probe
    assert '"SDL_AddGamepadMapping"' in probe
    assert "SDL3-mapping-%s.txt" in probe


def test_hardware_neutral_action_policy(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"universal.c"
    binary=tmp_path/"universal"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_universal_actions.h"
      int main(void) {
        SpiderSDL3ControllerProfile a,b;
        SpiderSDL3ControllerSample sample={0};
        SpiderSDL3UniversalActions state={0};
        SpiderSDL3UniversalFrame frame;
        /* Completely unknown USB IDs get standard SDL semantic defaults. */
        assert(spider_sdl3_profile_default(&a,0x1234u,0x9876u));
        assert(spider_sdl3_profile_default(&b,0x4567u,0x1122u));
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.valid && !frame.pressed && state.armed);
        sample.buttons=1u<<0u; /* SOUTH -> Jump on any model. */
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.pressed==1u && frame.held==1u);
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.held==1u && !frame.pressed);
        sample.buttons=0;
        frame=spider_sdl3_universal_step(&state,&a,&sample,51,1,1);
        assert(frame.released==1u && frame.held==0u);
        /* A profile binding swap changes semantics without VID filtering. */
        assert(spider_sdl3_profile_assign(&b,SPIDER_SDL3_ACTION_JUMP,
            spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.valid && state.armed && !frame.held);
        sample.buttons=1u<<2u;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.pressed==1u);
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,0,1);
        assert(!frame.valid && !frame.pressed && !state.armed);
        /* Returning with a held button never produces a ghost Jump. */
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.valid && !frame.pressed && !state.armed);
        sample.buttons=0;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(state.armed && !frame.pressed);
        /* Trigger hysteresis: RT Swing ON >= 11000; OFF below 5000. */
        sample.axis[SPIDER_SDL3_AXIS_RT]=15000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.pressed==(1u<<SPIDER_SDL3_ACTION_SWING));
        sample.axis[SPIDER_SDL3_AXIS_RT]=7000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.held&(1u<<SPIDER_SDL3_ACTION_SWING));
        sample.axis[SPIDER_SDL3_AXIS_RT]=4000;
        frame=spider_sdl3_universal_step(&state,&b,&sample,72,1,1);
        assert(frame.released==(1u<<SPIDER_SDL3_ACTION_SWING));
        return 0;
      }
    """), encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_raw_sdl_gamepad_mapping_builder(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"rawmap.c"
    binary=tmp_path/"rawmap"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_raw_mapping.h"
      int main(void) {
        const char *guid="341a3608000000000000504944564944";
        SpiderSDL3RawMapping map={0};
        char buf[512],small[12];
        map.input[SPIDER_RAW_MAP_A].kind=SPIDER_RAW_BUTTON;
        map.input[SPIDER_RAW_MAP_A].index=1;
        map.input[SPIDER_RAW_MAP_B].kind=SPIDER_RAW_BUTTON;
        map.input[SPIDER_RAW_MAP_B].index=2;
        map.input[SPIDER_RAW_MAP_LEFTX].kind=SPIDER_RAW_AXIS;
        map.input[SPIDER_RAW_MAP_LEFTX].index=0;
        map.input[SPIDER_RAW_MAP_LEFTY].kind=SPIDER_RAW_AXIS;
        map.input[SPIDER_RAW_MAP_LEFTY].index=1;
        map.input[SPIDER_RAW_MAP_DPUP].kind=SPIDER_RAW_HAT;
        map.input[SPIDER_RAW_MAP_DPUP].index=0;
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=1;
        assert(spider_sdl3_raw_build(guid,"Generic Gamepad",&map,buf,sizeof(buf)));
        assert(!strcmp(buf,
          "341a3608000000000000504944564944,Generic Gamepad,"
          "a:b1,b:b2,dpup:h0.1,leftx:a0,lefty:a1,"));
        assert(!spider_sdl3_raw_build(guid,"bad,name",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        assert(!spider_sdl3_raw_build("not-a-guid","Pad",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,small,sizeof(small)));
        assert(!small[0]);
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=3;
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,buf,sizeof(buf)));
        assert(!buf[0]);
        map.input[SPIDER_RAW_MAP_DPUP].hat_value=1;
        map.input[SPIDER_RAW_MAP_A].kind=SPIDER_RAW_NONE;
        map.input[SPIDER_RAW_MAP_A].index=0;
        assert(!spider_sdl3_raw_build(guid,"Pad",&map,buf,sizeof(buf)));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)


def test_keyboard_calibration_wizard_is_built_and_keeps_gameplay_unchanged():
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert "sdl3_raw_wizard.c" in build
    assert "SPIDER_UI_RAW=1008, SPIDER_UI_TEST=1009" in setup
    assert "SpiderSDL3RawWizard_Open(ui->hwnd,setup_report)" in setup
    assert "setup_toggle_sdl3_test(ui)" in setup
    assert "Test SDL3 inputs" in setup
    assert "Calibrate SDL3 joystick" in setup
    assert "F9/F10 unchanged" in setup
    assert "if(vk==VK_F9)" not in setup
    assert "if(vk==VK_F10)" not in setup
    assert "if(vk==VK_F8)" not in setup
    assert "SDL3 TEST: id=%lu buttons=" in setup
    assert "hardware recognition / recalibration" in wizard.lower()
    assert "SpiderSDL3Probe_RawListAll" in wizard
    assert "SpiderSDL3Probe_RawListAll" in probe
    assert "SpiderSDL3Probe_RawName" in wizard
    assert "spider_raw_ui_thread()" in probe
    assert "ALREADY MAPPED, recalibrating" in wizard
    assert "Mapped - recalibrate" in wizard
    assert "mapped devices CAN be recalibrated" in wizard
    assert "current-sdl-mapping=" in wizard
    assert "SDL3-RAW-WIZARD: step=" in wizard
    assert "n=SpiderSDL3Probe_RawListAll(ids,NULL" in probe
    assert "wizard_neutral" in wizard
    assert "wizard_finish(w)" in wizard
    assert "SDL3 CALIBRATION SAVED" in wizard
    assert "if(msg.wParam==VK_ESCAPE)" in wizard
    assert "if(msg.wParam=='S'" in wizard
    assert "SDL3-RAW-WIZARD: completed and persisted" in wizard
    assert "SpiderSDL3Probe_RawList" in probe
    assert "SpiderSDL3Probe_RawRead" in probe
    assert "SpiderSDL3Probe_RawRegisterAndSave" in probe
    assert "SDL3-mapping-%s.txt" in probe
    assert "spider_raw_local_path" in probe
    assert "LOCALAPPDATA" in probe
    assert "MoveFileExA(temp,file,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)" in probe
    assert "SDL_AddGamepadMapping refused mapping; nothing saved" in probe
    assert "SDL3-MAPPINGS: persisted per-GUID user mappings loaded=" in probe
    assert "sdl3-gamepad" not in wizard

def test_mapped_gamepad_can_enter_recalibration_without_changing_game_owner():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert "SpiderSDL3Probe_RawListAll(ids,NULL" in probe
    assert "SpiderSDL3Probe_RawListAll(w->ids,w->already_mapped,16u)" in wizard
    assert "current-sdl-mapping=%s" in wizard
    assert "Mapped - recalibrate" in wizard
    assert "Unmapped - recognize" in wizard
    assert "SPIDER_UI_RAW" in setup
    assert "universal 3D game bridge NOT active" in setup
    assert "Press F9 to exit" not in setup
    assert "SDL3-3D-LIVE" not in wizard


def test_wizard_start_does_not_restart_capture_and_context_scope_documented():
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    roadmap=(ROOT/"docs/references/SDL3_UNIVERSAL_CONTROLLERS.md").read_text(encoding="utf-8")
    assert "if(msg.wParam==VK_RETURN && !wizard_is_capturing(&w))" in wizard
    assert "synthesized controller keys ignored" in wizard
    assert "Start / Restart" in wizard
    assert "VK_RETURN" in wizard
    for action in ["MenuConfirm", "MenuBack", "SkipVideo", "NextStill", "SkipCutscene"]:
        assert action in roadmap
    for context in ["Main and pause menus", "Bink/videos", "Scripted/in-engine 3D cutscenes", "Fixed entry/continue image or still", "Gameplay 3D"]:
        assert context in roadmap
    assert "F9 screenshot and F10" in roadmap
    assert "release-blocking acceptance criteria" in roadmap


def test_raw_wizard_win32_focus_isolation_prevents_gamepad_combo_activation():
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    assert "EnableWindow(w->pads,capturing?FALSE:TRUE)" in wizard
    assert "SendMessageA(w->pads,CB_SHOWDROPDOWN,FALSE,0)" in wizard
    assert "SetFocus(w->window)" in wizard
    assert "wizard_capture_focus(w,1)" in wizard
    assert "wizard_capture_focus(w,0)" in wizard
    assert "synthesized controller keys ignored" in wizard
    assert "msg.message==WM_KEYDOWN || msg.message==WM_KEYUP" in wizard
    assert "msg.message==WM_CHAR || msg.message==WM_SYSCHAR" in wizard
    assert "if(IsDialogMessageA(hwnd,&msg))continue;" in wizard
    assert wizard.index("if(wizard_is_capturing(&w) &&") < wizard.index("if(IsDialogMessageA(hwnd,&msg))")
    assert "if(msg.wParam=='Q')" in wizard
    assert "if(msg.wParam=='S')" in wizard
    assert "if(msg.wParam=='R')" in wizard
    assert "GetKeyState(VK_CONTROL)&0x8000" in wizard
    assert "GetKeyState(VK_SHIFT)&0x8000" in wizard
    assert "Ctrl+Shift+Esc" not in wizard
    assert "SDL3-RAW-WIZARD: capture UI ISOLATED" in wizard
    assert "SDL3-RAW-WIZARD: suppressed Win32 keyboard" in wizard
    # The bottom Start/Restart, Skip and Close pushbuttons must ALL be
    # disabled during calibration; otherwise Wine routes X/Circle to their
    # current keyboard focus and triggers BN_CLICKED.
    assert "for(id=RAW_BEGIN;id<=RAW_CLOSE;id++)" in wizard
    assert "EnableWindow(button,capturing?FALSE:TRUE)" in wizard
    assert "wizard_is_capturing(w)) {" in wizard
    assert "SDL3-RAW-WIZARD: blocked Win32 control" in wizard
    assert wizard.index("if(wizard_is_capturing(w)) {", wizard.index("case WM_COMMAND:")) < wizard.index("if(HIWORD(wp)==BN_CLICKED)", wizard.index("case WM_COMMAND:"))
    assert "SetFocus(w->window)" in wizard



def test_sdl3_normalized_input_diagnostic_has_no_active_win32_controls():
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    # The tester is a temporary read-only SDL3 viewport, not an interactive
    # Win32 pushbutton while controller hardware is being sampled.
    assert "static void setup_sdl3_test_lock(" in setup
    assert "SendMessageA(ui->pads,CB_SHOWDROPDOWN,FALSE,0)" in setup
    assert "EnableWindow(ui->pads,lock?FALSE:TRUE)" in setup
    assert "EnableWindow(ui->actions,lock?FALSE:TRUE)" in setup
    assert "for(i=SPIDER_UI_CAPTURE;i<=SPIDER_UI_CLOSE;i++)" in setup
    assert "for(i=SPIDER_UI_RAW;i<=SPIDER_UI_USE;i++)" in setup
    assert "EnableWindow(button,lock?FALSE:TRUE)" in setup
    assert "if(lock)SetFocus(ui->hwnd)" in setup
    assert "if(ui->test_mode) {" in setup
    assert "SDL3-SETUP: TEST blocked Win32 control" in setup
    assert setup.index("if(ui->test_mode) {",setup.index("case WM_COMMAND:")) < setup.index("if(LOWORD(wp)==SPIDER_UI_PAD",setup.index("case WM_COMMAND:"))
    assert "if(ui.test_mode &&" in setup
    assert setup.index("if(ui.test_mode &&") < setup.index("if(IsDialogMessageA(window,&msg))continue;")
    assert "SDL3-SETUP: TEST suppressed Win32 keyboard" in setup
    assert "msg.wParam=='T'" in setup
    assert "msg.wParam=='Q'" in setup
    assert "GetKeyState(VK_CONTROL)&0x8000" in setup
    assert "GetKeyState(VK_SHIFT)&0x8000" in setup
    assert "Ctrl+Shift+T" in setup
    assert "Ctrl+Shift+Q" in setup
    assert "SDL3 TEST: id=%lu buttons=" in setup
    assert "SPIDER_SDL3_AXIS_RIGHTX" in setup
    assert "SPIDER_SDL3_AXIS_RIGHTY" in setup
    assert "if(!ui->test_mode && modal_start_handoff(ui))return;" in setup
    assert "if(vk==VK_F9)" not in setup
    assert "if(vk==VK_F10)" not in setup


def test_keyboard_escape_uses_raw_hid_input_and_not_controller_fake_escape():
    physical=(ROOT/"sdl3_physical_escape.h").read_text(encoding="utf-8")
    wizard=(ROOT/"sdl3_raw_wizard.c").read_text(encoding="utf-8")
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert "GetRegisteredRawInputDevices(NULL,&count" in physical
    assert "usUsagePage==0x01u && devices[i].usUsage==0x06u" in physical
    assert "RegisterRawInputDevices(&keyboard,1u,sizeof(keyboard))" in physical
    assert "RIDEV_REMOVE" in physical
    assert "GetRawInputData(input,RID_INPUT" in physical
    assert "packet.header.dwType!=RIM_TYPEKEYBOARD" in physical
    assert "packet.data.keyboard.VKey==VK_ESCAPE" in physical
    assert "packet.data.keyboard.Flags&RI_KEY_BREAK" in physical
    for dialog in [wizard,setup]:
        assert '#include "sdl3_physical_escape.h"' in dialog
        assert "spider_sdl3_escape_begin(" in dialog
        assert "spider_sdl3_escape_key((HRAWINPUT)msg.lParam)" in dialog
        assert "spider_sdl3_escape_end(" in dialog
        assert "msg.message==WM_INPUT" in dialog
        # Still keep the controller-synthesized Win32 Escape suppressed
        # while physically calibrating or recording normalized SDL3 input.
        assert "msg.message==WM_KEYDOWN || msg.message==WM_KEYUP" in dialog
        assert "if(IsDialogMessageA(" in dialog
        assert dialog.index("spider_sdl3_escape_key((HRAWINPUT)msg.lParam)") < dialog.index("if(IsDialogMessageA(")
        assert "Ctrl+Shift+Q" in dialog
    assert "PHYSICAL-ESC keyboard cancel" in wizard
    assert "PHYSICAL-ESC keyboard stopped normalized input test" in setup
    assert "if(vk==VK_F9)" not in setup
    assert "if(vk==VK_F10)" not in setup


def test_generic_sdl3_selection_is_explicit_unique_without_native_rebind():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    header=(ROOT/"sdl3_gamepad_probe.h").read_text(encoding="utf-8")
    assert "static uint32_t generic_selected_id=0;" in probe
    assert "static uint16_t generic_selected_vendor=0,generic_selected_product=0;" in probe
    assert "SDL3-GENERIC: SELECTED explicit modal Start" in probe
    assert "spider_sdl3_selected_model_unique(opened,num_pads," in probe
    pending=probe.split("if(setup_owner_pending) {",1)[1].split(
        "sdl3_trace_native_owner(now);",1)[0]
    assert "generic_selected_id=requested_id;" in pending
    assert "selection.active=requested_id;" not in pending
    assert "SpiderSDL3Probe_NativeOwnerEnabled()" not in pending
    assert "SETUP RETURN REFUSED stale/ambiguous" in pending
    assert "int SpiderSDL3Probe_GenericSelectedSample(" in header
    generic=probe.split("int SpiderSDL3Probe_GenericSelectedSample(",1)[1].split(
        "/* Universal SDL3 selection",1)[0]
    assert "sdl3_game_thread!=GetCurrentThreadId()" in generic
    assert "spider_sdl3_selected_model_unique(" in generic
    assert "sdl_connected(pads[i].handle)" in generic
    assert "sample->axis[j]=sdl_axis(pads[i].handle,(int)j);" in generic
    assert "SPIDER_SDL3_AXIS_RIGHTX" in (ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    for unsafe in ("SendInput(", "VirtualProtect(", "IDirectInputDevice8_",
                   "WriteProcessMemory(", "spider_reconnect_kind("):
        assert unsafe not in generic
    trace=probe.split("static void sdl3_generic_semantic_trace_on_flip(",1)[1].split(
        "void SpiderSDL3Probe_OnFlip(",1)[0]
    assert "spider_sdl3_universal_step(" in trace
    assert "spider_sdl3_profile_valid(profile)" in trace
    assert "generic_trace_lines>=48u" in trace
    assert "sdl3_generic_semantic_trace_on_flip();" in probe
    assert "SpiderSDL3Probe_SelectedIdentity" not in probe
    assert "SpiderNativeReconnect_" not in probe


def test_ps3_explicit_setup_selection_when_start_was_consumed_by_capture():
    setup=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert "SPIDER_UI_USE=1010" in setup
    assert '"Use selected SDL3 controller"' in setup
    assert "setup_select_explicit_pad(" in setup
    assert "SPIDER_UI_USE && HIWORD(wp)==BN_CLICKED" in setup
    assert "LOWORD(wp)==SPIDER_UI_USE" in setup
    assert "ui->start_owner_id=id;" in setup
    assert "ui->start_owner_vid=vid;" in setup
    assert "ui->start_owner_pid=pid;" in setup
    assert "ui->start_owner_requested=1u;" in setup
    assert "SDL3-SETUP: EXPLICIT-SELECT QUEUED" in setup
    assert "SDL3-SETUP: EXPLICIT-SELECT CAPTURE CANCELLED; no owner queued; click Use again" in setup
    assert "SDL3-SETUP: START-IGNORED action capture active" in setup
    assert "ui->capture.active || ui->close_pending" in setup
    assert "SpiderSDL3Probe_SetupRead(ui->slot,&id,&vid,&pid," in setup
    assert "id!=ui->instance || vid!=ui->vid || pid!=ui->pid" in setup
    assert "SpiderSDL3Probe_SetupModalReturn(" in setup
    assert "setup_owner_pending=requested_id;" in probe
    assert "SDL3-GENERIC: SELECTED explicit modal Start" in probe
    assert "generic_selected_id=requested_id;" in probe
    assert "selection.active=requested_id;" not in probe
    assert "sdl3_generic_semantic_trace_on_flip();" in probe
    assert "for(i=SPIDER_UI_RAW;i<=SPIDER_UI_USE;i++)" in setup
    assert "for(unsigned i=SPIDER_UI_RAW;i<=SPIDER_UI_USE;i++)" in setup
    assert "CW_USEDEFAULT,CW_USEDEFAULT,730,558," in setup
    assert "Ctrl+Shift+U" in setup
    assert "GetKeyState(VK_CONTROL)&0x8000" in setup
    assert "GetKeyState(VK_SHIFT)&0x8000" in setup
    # No DI rebind or synthetic key injection from modal helper.
    helper=setup.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    for forbidden in ("SendInput(", "VirtualProtect(", "IDirectInput", "WriteProcessMemory("):
        assert forbidden not in helper


def test_multiple_sdl3_pads_never_select_default_ds4_when_ps3_was_intended():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    # Real PS3+DS4 hardware log: auto-selected id=1 VID:PID=054C:09CC
    # while physical PS3 was id=3 VID:PID=054C:0268. Explicit Use
    # must not silently grant the preselected first device.
    assert "unsigned user_chose_controller;" in ui
    refresh=ui.split("static void refresh_pads(",1)[1].split(
        "static int modal_start_handoff(",1)[0]
    assert "ui->user_chose_controller=0;" in refresh
    assert "if(n>1u && !ui->user_chose_controller)" in refresh
    assert "SDL3-SETUP: SELECT-UNCONFIRMED multiple pads" in refresh
    assert "if(old_id && id==old_id)desired=i;" in refresh
    selector=ui.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "if(ui->count>1u && !ui->user_chose_controller)" in selector
    assert "EXPLICIT-SELECT REFUSED auto-preview" in selector
    assert selector.index("if(ui->count>1u && !ui->user_chose_controller)") < selector.index("ui->start_owner_id=id;")
    # Explicit dropdown, L1/R1, keyboard page browsing, or controller
    # Start are the only paths confirming a chosen model.
    assert "SDL3-SETUP: USER-CHOSEN slot=" in ui
    assert ui.count("ui->user_chose_controller=1u;") >= 4
    assert "SDL3-SETUP: EXPLICIT-SELECT QUEUED" in selector
    assert "generic_selected_id=requested_id;" in (ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")


def test_capture_cancelled_before_sdl3_owner_claim_requires_second_explicit_action():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    button=ui.split("static int setup_select_explicit_pad(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert "if(ui->count>1u && !ui->user_chose_controller)" in button
    assert "if(ui->capture.active) {" in button
    cancel=button.split("if(ui->capture.active) {",1)[1].split(
        "if(!ui->instance",1)[0]
    assert "spider_sdl3_capture_reset(&ui->capture);" in cancel
    assert "spider_sdl3_setup_nav_reset(&ui->nav);" in cancel
    assert "SetFocus(ui->hwnd)" in cancel
    assert "CAPTURE CANCELLED" in cancel
    assert "no owner queued; click Use again" in cancel
    assert "return 0;" in cancel
    # Controller ownership request must happen AFTER the cancellation
    # branch and AFTER exact SDL3 slot+VID/PID verification.
    assert button.index("if(ui->capture.active) {") < button.index(
        "SpiderSDL3Probe_SetupRead(") < button.index("ui->start_owner_id=id;")
    assert "ui->start_owner_requested=1u;" in button
    assert "SDL3-SETUP: EXPLICIT-SELECT QUEUED" in button
    key=ui.split("static int handle_setup_keyboard(",1)[1].split(
        "static void setup_toggle_sdl3_test(",1)[0]
    assert key.index("if(vk=='U'") < key.index("if(ui->capture.active)return 0;")
    # Avoid re-entering action capture if Wine synthesizes VK_RETURN
    # while keyboard focus is on inert dialog background.
    assert "if(focused==ui->actions)" in key
    assert "if(focused==ui->actions && current_profile(ui))" not in key
    assert "if(vk=='C'" in key  # explicit remap only
    assert "if(current_profile(ui)) {\n      spider_sdl3_capture_start" not in key
