"""Optional universal SDL3 gameplay bridge: C99 semantics and fail-closed ownership.

These are portable policy/source tests; they DO NOT replace hardware
validation of original game-owned DirectInput pointer suspension or of
SendInput in retail 3D on Windows and Wine.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_universal_profile_actions_axes_dpad_and_pause_edges(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"universal.c"
    exe=tmp_path/"universal"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        SpiderSDL3UniversalState state={0};
        SpiderSDL3ControllerProfile profile;
        SpiderSDL3ControllerSample sample={0};
        uint32_t desired=999u;
        unsigned pause=999u;
        assert(spider_sdl3_profile_default(&profile,0xfffeu,0x1234u));
        /* Physical SDL3 south is Jump regardless of manufacturer. */
        sample.buttons=1u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==0);
        assert(desired==0u && pause==0u);
        sample.buttons=0;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==0);
        assert(state.armed && !desired);
        sample.buttons=1u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_JUMP) && pause==0);
        sample.buttons=0;
        sample.axis[SPIDER_SDL3_AXIS_LEFTX]=22000;
        sample.axis[SPIDER_SDL3_AXIS_LEFTY]=-21000;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_RIGHT |
                         1u<<SPIDER_UNIVERSAL_UP));
        /* D-pad must be generic SDL button 11..14, not physical DI index. */
        sample.axis[SPIDER_SDL3_AXIS_LEFTX]=0;
        sample.axis[SPIDER_SDL3_AXIS_LEFTY]=0;
        sample.buttons=(1u<<SPIDER_UNIVERSAL_DPAD_DOWN) |
                       (1u<<SPIDER_UNIVERSAL_DPAD_LEFT);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_DOWN |
                         1u<<SPIDER_UNIVERSAL_LEFT));
        /* Pause is a separate rising edge and never a held gameplay key. */
        sample.buttons=1u<<4;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(desired==0u && pause==1u);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,1,
                                         &desired,&pause)==1);
        assert(!desired && !pause);
        sample.buttons=0u;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        sample.buttons=1u<<4;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,17,0,
                                         &desired,&pause)==1);
        assert(pause==1u);
        /* A different SDL3 instance must never inherit held buttons. */
        assert(spider_sdl3_universal_step(&state,&profile,&sample,18,0,
                                         &desired,&pause)==0);
        assert(!desired && !pause && !state.armed);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_universal_runtime_is_opt_in_and_mutually_exclusive():
    game=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    live=(ROOT/"sdl3_gameplay_live.c").read_text(encoding="utf-8")
    ui=(ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    cfg=(ROOT/"virtual_spidey_cfg.c").read_text(encoding="utf-8")
    ini=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert 'ExperimentalSDL3UniversalGameplay=0' in cfg
    assert '"ExperimentalSDL3UniversalGameplay"' in ini
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)==1' in backend
    assert '"ExperimentalSDL3UniversalGameplay",0,ini)!=1' in live
    assert 'sdl3_universal_gameplay.c' in build
    assert '#include "sdl3_universal_gameplay.h"' in game
    assert 'SpiderSDL3Universal_BeforeUI(game_hwnd);' in game
    assert game.index("SpiderSDL3Probe_OnFlip(count,game_hwnd);") < game.index(
        "SpiderSDL3Universal_BeforeUI(game_hwnd);") < game.index(
        "SpiderGamepadUI_OnFlip(game_hwnd")
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in game
    assert "SpiderSDL3Universal_Enabled()" in native
    assert "SpiderSDL3Universal_Enabled()" in ui
    # PR #96 retains its validated SDL3 mapped-device panel. The raw
    # unmapped GUID wizard from #94 is deliberately NOT merged here.
    assert "SpiderSDL3Probe_GenericSelectedSample" in probe
    assert "if(generic_selected_id)" in probe
    assert "universal_requested" in probe
    assert "SDL3-UNIVERSAL: START selected generic" in probe
    assert "SpiderSDL3Probe_SelectedMappedSample" in probe
    assert "InterlockedCompareExchangePointer(SPIDER_UNIVERSAL_DI_SLOT" in backend
    assert "restored" in backend.lower()
    assert "release_all()" in backend
    assert "SpiderCfg_NativeConfigLoaded()" in backend
    assert "spider_sdl3_universal_step" in backend
    assert "SPIDER_UNIVERSAL_DI_SLOT" in backend
    assert "KEYEVENTF_SCANCODE" in backend
    assert "KEYEVENTF_EXTENDEDKEY" in backend
    for forbidden in ("VirtualProtect(", "WriteProcessMemory(",
                      "IDirectInputDevice8_Release(", "IDirectInputDevice8_Acquire(",
                      "SetWindowsHookEx(", "CreateThread("):
        assert forbidden not in backend


def test_universal_native_di_is_neutral_gated_and_modal_safe():
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    header=(ROOT/"sdl3_universal_gameplay.h").read_text(encoding="utf-8")
    modal=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert "void SpiderSDL3Universal_BeforeModal(HWND game_hwnd)" in header
    assert "void SpiderSDL3Universal_BeforeModal(HWND game_hwnd) {" in backend
    assert 'restore_original("opening controller configuration modal");' in backend
    assert "SpiderSDL3Universal_BeforeModal(owner);" in modal
    assert modal.index("SpiderSDL3Universal_BeforeModal(owner);") < modal.index(
        "if(!SpiderSDL3Probe_SetupPump()) {",modal.index(
            "void SpiderSDL3ControllerSetup_Open(void)"))
    assert "if(!suspended && !spider_sdl3_universal_neutral(&sample))" in backend
    assert backend.index("if(!suspended && !spider_sdl3_universal_neutral(&sample))") < (
        backend.index("if(!suspend_original())"))
    assert "SendInput failure; releasing and restoring native owner" in backend
    assert 'restore_original("SendInput failed");' in backend
    assert 'restore_original("menu SendInput failed");' in backend
    assert "native DI slot already NULL; no pointer overwritten" in backend or (
        "original game-owned DI slot already NULL; no pointer overwritten" in backend)
    assert "game-owned native DI pointer SUSPENDED (experimental; lifetime unproven)" in backend
    # Event telemetry is evidence of OS acceptance, never falsely claims
    # the original game actually received these synthetic keystrokes.
    assert "SendInput KEYDOWN action=" in backend
    assert "SendInput KEYUP action=" in backend
    assert "OS accepted, game reception unverified" in backend
    assert "if(key_transition_reports++<48u)" in backend
    assert "if(report && trace++<100u)report(msg);" in backend


def test_no_gameplay_actions_or_pause_on_non_neutral_start(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"universal_neutral.c"
    exe=tmp_path/"universal_neutral"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        SpiderSDL3UniversalState state={0};
        SpiderSDL3ControllerProfile p;
        SpiderSDL3ControllerSample sample={0};
        uint32_t desired=0xffu;
        unsigned pause=1;
        assert(spider_sdl3_profile_default(&p,0x054c,0x0268));
        /* Holding Circle (menu Back) or an action cannot arm 3D. */
        sample.buttons=1u<<1;
        assert(spider_sdl3_universal_step(&state,&p,&sample,2,0,
                                         &desired,&pause)==0);
        assert(!desired && !pause && !state.armed);
        sample.buttons=0;
        assert(spider_sdl3_universal_step(&state,&p,&sample,2,0,
                                         &desired,&pause)==0);
        assert(state.armed && !desired);
        sample.buttons=1u<<1;
        assert(spider_sdl3_universal_step(&state,&p,&sample,2,0,
                                         &desired,&pause)==1);
        assert((desired&(1u<<SPIDER_UNIVERSAL_KICK))!=0u);
        spider_sdl3_universal_reset(&state);
        sample.buttons=0;
        sample.axis[SPIDER_SDL3_AXIS_LEFTX]=32000;
        assert(spider_sdl3_universal_step(&state,&p,&sample,2,0,
                                         &desired,&pause)==0);
        assert(!state.armed && !desired);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_universal_hardware_logs_explain_why_gameplay_adapter_abstains():
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "SDL3-UNIVERSAL: waiting for selected SDL3 mapped pad" in backend
    assert "SDL3-UNIVERSAL: REFUSED no valid normalized profile" in backend
    assert "SDL3-UNIVERSAL: REFUSED unverified writable native joystick slot" in backend
    assert "SDL3-UNIVERSAL: REFUSED original keyboard table invalid" in backend
    assert "SDL3-UNIVERSAL: EXIT reason=%s" in backend
    assert "refusal_reports++<6u" in backend
    assert "no selected mapped controller" in backend
    assert 'restore_original("native DI slot not proven");' in backend
    assert 'restore_original("game keyboard table not proven");' in backend
    # Existing button-assignment model files stay owned by SDL3 setup;
    # the bridge must not mutate them or bypass a failed validation.
    assert "SpiderSDL3Probe_SetupCommit(" not in backend
    assert "spider_sdl3_profile_assign(" not in backend
    assert "return;" in backend


def test_gameplay_scan_filter_rejects_system_keys_and_f9_screenshot(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"scan_gate.c"
    exe=tmp_path/"scan_gate"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        static const unsigned rejected[]={
          0,1,0x0e,0x0f,0x1c,0x1d,0x2a,0x36,0x38,
          0x3a,0x43,0x44,0x45,0x46,0x57,0x58,
          0x80,0x9c,0x9d,0xb7,0xb8,0xc5,0xd4
        };
        static const unsigned accepted[]={
          0x11, /* physical Z */
          0x2d, /* physical X */
          0x2e, /* physical C */
          0x2f, /* physical V */
          0x20, /* physical D */
          0x21, /* physical F */
          0x22, /* physical G */
          0x39, /* Space */
          0xc8,0xd0,0xcb,0xcd /* extended arrows */
        };
        unsigned i;
        for(i=0;i<sizeof(rejected)/sizeof(rejected[0]);i++)
          assert(!spider_sdl3_universal_scan_safe(rejected[i]));
        for(i=0;i<sizeof(accepted)/sizeof(accepted[0]);i++)
          assert(spider_sdl3_universal_scan_safe(accepted[i]));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "spider_sdl3_universal_scan_safe(scan)" in backend
    assert "read_scans(" in backend


def test_integrated_panel_preserved_and_generic_owner_is_explicit_only():
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    game=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    assert '"SpiderMan2k1FixSDL3ActionView"' in ui
    assert '"LISTBOX"' not in ui
    assert "CROSS-RESET defaults SAVED" in ui
    assert "ACTION-ROW CROSS CAPTURE ARMED" in ui
    assert "SpiderSDL3Universal_BeforeModal(owner);" in ui
    assert "generic_selected_id" in probe
    assert "selection.active=previous;" in probe
    assert "universal_requested && vid && pid && matches==1u" in probe
    assert "spider_reconnect_kind(vid,pid)==SPIDER_RECONNECT_NONE" in probe
    assert "if(generic_selected_id)" in probe
    assert "return SpiderSDL3Probe_GenericSelectedSample(" in probe
    assert "return SpiderSDL3Probe_SelectedSample(" in probe
    assert "SpiderSDL3Universal_BeforeUI(game_hwnd);" in game
    assert "SpiderSDL3Universal_OnFlip(game_hwnd," in game
    assert "SpiderSDL3Universal_Enabled()" in (
        ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    assert "spider_sdl3_generic_ui_reset(&generic_ui);" in (
        ROOT/"gamepad_ui_profile.c").read_text(encoding="utf-8")


def test_raw_unmapped_enumeration_boundaries_do_not_use_other_loop_index():
    """Catch Clang's uninitialized j warning: raw fallback used j in jj loops."""
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    assert "for(jj=0;jj<(unsigned)mapped;jj++)" in probe
    assert "for(jj=0;jj<raw_last_n;jj++)" in probe
    assert "for(jj=0;j<(unsigned)mapped;jj++)" not in probe
    assert "for(jj=0;j<raw_last_n;jj++)" not in probe
    assert "if(jj==raw_last_n)" in probe


def test_in_game_native_pause_routes_to_menu_not_3d_keyboard(tmp_path):
    """Physical PS3 passed 3D and Bink; Pause stayed inert at in-game Flip.
    Original Pause is drawn on the 3D renderer and still reports gameplay=1.
    Native Pause must take precedence so Cross/Arrow/Circle reach the UI."""
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if cc:
        src=tmp_path/"pause_context.c"
        exe=tmp_path/"pause_context"
        src.write_text(textwrap.dedent(r"""
            #include <assert.h>
            #include "sdl3_universal_policy.h"
            int main(void) {
              assert(spider_sdl3_universal_route_context(0,1,0,0)
                     ==SPIDER_UNIVERSAL_CTX_3D);
              assert(spider_sdl3_universal_route_context(0,1,0,1)
                     ==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE);
              assert(spider_sdl3_universal_route_context(1,1,0,1)
                     ==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE);
              assert(spider_sdl3_universal_route_context(0,0,0,1)
                     ==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE);
              assert(spider_sdl3_universal_route_context(0,1,1,1)
                     ==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE);
              assert(spider_sdl3_universal_route_context(0,0,1,0)
                     ==SPIDER_UNIVERSAL_CTX_BINK);
              assert(spider_sdl3_universal_route_context(1,0,0,0)
                     ==SPIDER_UNIVERSAL_CTX_MENU);
              return 0;
            }
        """),encoding="utf-8")
        subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                        "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    game=(ROOT/"spiderman2k1fix.c").read_text(encoding="utf-8")
    assert "context=spider_sdl3_universal_route_context(" in backend
    assert "menu,gameplay,bink,native_pause" in backend
    assert "if(context!=SPIDER_UNIVERSAL_CTX_3D)" in backend
    assert "spider_sdl3_universal_reset(&state);" in backend
    assert "menu_arm=0;menu_previous=0;" in backend
    assert "SDL3-UNIVERSAL: PAUSE-MENU context transition" in backend
    assert "SDL3-UNIVERSAL: PAUSE-MENU INPUT held=" in backend
    assert "context==SPIDER_UNIVERSAL_CTX_NATIVE_PAUSE &&" in backend
    assert "binding_held(profile->actions[SPIDER_SDL3_ACTION_PAUSE],&sample)" in backend
    assert "if(back || resume)menu_buttons|=1u<<5;" in backend
    assert "if(confirm)menu_buttons|=1u<<4;" in backend
    assert "update_keys(menu_buttons&0x0fu,scans)" in backend
    assert 'pulse(0x01u,"menu Back/Pause resume")' in backend
    assert 'pulse(0x1cu,"menu Confirm")' in backend
    # The original 3D-native Pause ribbon detector is sampled on the
    # actual guarded Flip path. It works even when the renderer says 3D.
    caller=game.split("SpiderSDL3Universal_OnFlip(game_hwnd,",1)[1].split(
        "/* Passive, bounded post-hotplug heartbeat",1)[0]
    assert "native_pause43_visible_near_flip(count)" in caller
    assert "0x00682950u" in caller


def test_native_pause_menu_must_rearm_after_release_and_not_autoresume():
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    body=backend.split("context=spider_sdl3_universal_route_context(",1)[1]
    transition=body.split("if(context!=SPIDER_UNIVERSAL_CTX_3D)",1)[0]
    assert "if(context!=previous_context)" in transition
    assert "menu_arm=0;menu_previous=0;" in transition
    assert "spider_sdl3_universal_reset(&state);" in transition
    assert "previous_context=context;" in transition
    menu=body.split("if(context!=SPIDER_UNIVERSAL_CTX_3D)",1)[1].split(
        "menu_arm=0;menu_previous=0;",1)[0]
    assert "if(spider_sdl3_universal_neutral(&sample))menu_arm=1;" in menu
    assert "menu_previous=0;" in menu
    assert "if(back || resume)" in menu


def test_first_universal_active_report_survives_prolonged_start_hold():
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    assert "if(owner_reports++<10u)" in backend
    assert "if(!active) {" in backend
    assert "if(!active && owner_reports++<10u)" not in backend
    assert "SDL3-UNIVERSAL: ACTIVE selected-id=" in backend


def test_saved_ps3_custom_swap_drives_exactly_one_3d_action(tmp_path):
    """Portable policy validation of the user's Square->Circle remapping.
    Menu Circle/Back must remain independent of swapped 3D Punch/Kick."""
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    src=tmp_path/"ps3_custom_3d.c"
    exe=tmp_path/"ps3_custom_3d"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include "sdl3_universal_policy.h"
      int main(void) {
        SpiderSDL3UniversalState state={0};
        SpiderSDL3ControllerProfile profile;
        SpiderSDL3ControllerSample sample={0};
        uint32_t desired=0;
        unsigned pause_edge=0;
        const uint32_t id=23;
        assert(spider_sdl3_profile_default(&profile,0x054c,0x0268));
        assert(spider_sdl3_profile_assign_visible(
          &profile,SPIDER_SDL3_ACTION_PUNCH,
          spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,1)));
        /* Circle (east) becomes Punch; Square (west) becomes Kick. */
        assert(profile.actions[SPIDER_SDL3_ACTION_PUNCH].index==1);
        assert(profile.actions[SPIDER_SDL3_ACTION_KICK].index==2);
        assert(profile.context[SPIDER_SDL3_CONTEXT_BACK].index==1);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==0);
        assert(state.armed && !desired);
        sample.buttons=(1u<<1);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_PUNCH) && !pause_edge);
        sample.buttons=(1u<<2);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_KICK) && !pause_edge);
        /* Remapping while the game is running is a live model profile
         * read: restoring defaults changes the next semantic action. */
        assert(spider_sdl3_profile_default(&profile,0x054c,0x0268));
        sample.buttons=(1u<<1);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_KICK) && !pause_edge);
        sample.buttons=0;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(!desired);
        /* A remapped analog trigger uses hysteresis. Swapping Swing and
         * Aim still produces exactly one original 3D action per input. */
        assert(spider_sdl3_profile_assign_visible(
          &profile,SPIDER_SDL3_ACTION_AIM,
          spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_RT)));
        assert(profile.actions[SPIDER_SDL3_ACTION_AIM].kind==
               SPIDER_SDL3_BIND_TRIGGER);
        assert(profile.actions[SPIDER_SDL3_ACTION_SWING].index==9);
        sample.axis[SPIDER_SDL3_AXIS_RT]=20000;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_AIM));
        sample.axis[SPIDER_SDL3_AXIS_RT]=7000;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_AIM));
        sample.axis[SPIDER_SDL3_AXIS_RT]=4000;
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(!desired);
        sample.buttons=(1u<<9);
        assert(spider_sdl3_universal_step(&state,&profile,&sample,id,0,
                                         &desired,&pause_edge)==1);
        assert(desired==(1u<<SPIDER_UNIVERSAL_SWING));
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_universal_fail_closed_reentry_after_failed_keyup():
    """A pending virtual key must be released before DI re-borrowed or
    a new SDL controller/foreground frame is accepted after Alt+Tab."""
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    onflip=backend.split("void SpiderSDL3Universal_OnFlip(",1)[1]
    preowner=onflip.split("if(!suspend_original())",1)[0]
    assert 'restore_original("game HWND not foreground");' in preowner
    assert 'restore_original("no selected mapped controller");' in preowner
    assert 'restore_original("SDL3 Start owner changed");' in preowner
    assert "if(!active && (sent_mask || pulse_scan))" in preowner
    assert "if(!release_all())" in preowner
    assert "REENTRY BLOCKED pending synthetic KEYUP" in preowner
    assert preowner.index("if(!active && (sent_mask || pulse_scan))") < (
        preowner.index("if(!suspended && !spider_sdl3_universal_neutral"))
    assert "if(active && pulse_scan)" in preowner
    assert 'restore_original("pulse KEYUP failed before next gameplay action");' in preowner
    assert "if(!suspended && !spider_sdl3_universal_neutral(&sample))" in preowner
    assert "if(!release_all())" in backend.split("static void restore_original(",1)[1].split(
        "static int suspend_original(",1)[0]
    beforeui=backend.split("void SpiderSDL3Universal_BeforeUI(",1)[1].split(
        "static int binding_held(",1)[0]
    assert 'restore_original("focus or window lost");' in beforeui
    # Identity cannot silently fall back to another SDL3 pad if the user
    # explicitly selected a generic PS3 that has just been unplugged.
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    selected=probe.split("int SpiderSDL3Probe_SelectedMappedSample(",1)[1].split(
        "/* The player may hold physical Cross",1)[0]
    assert "if(generic_selected_id)" in selected
    assert "return SpiderSDL3Probe_GenericSelectedSample(" in selected
    assert "return SpiderSDL3Probe_SelectedSample(" in selected


def test_neutral_rearm_release_failure_falls_back_to_original_di():
    backend=(ROOT/"sdl3_universal_gameplay.c").read_text(encoding="utf-8")
    onflip=backend.split("void SpiderSDL3Universal_OnFlip(",1)[1]
    assert "if(result!=1) {" in onflip
    unarmed=onflip.split("if(result!=1) {",1)[1].split(
        "if(!update_keys(desired,scans))",1)[0]
    assert "if(!update_keys(0u,scans))" in unarmed
    assert "neutral rearm KEYUP failed; restoring native owner" in unarmed
    assert 'restore_original("neutral rearm KEYUP failed");' in unarmed
    assert "(void)update_keys(0u,scans);" not in onflip
