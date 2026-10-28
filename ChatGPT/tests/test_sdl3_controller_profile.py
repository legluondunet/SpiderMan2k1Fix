"""SDL3 controller profile policy: portable native C99 tests and safety contracts.

This stage has NO gameplay injection. Tests cover the future in-game
configuration wizard's model, source disambiguation, rollback and ownership.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_sdl3_controller_profiles_compile_and_behave(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"profile.c"
    exe=tmp_path/"profile"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include "sdl3_controller_profile.h"
      int main(void) {
        SpiderSDL3ProfileBank bank;
        SpiderSDL3ControllerProfile *sony,*xbox,*again,copy;
        SpiderSDL3ControllerSample s;
        uint32_t mask=0;
        unsigned i;
        spider_sdl3_bank_reset(&bank);
        assert(!spider_sdl3_bank_get(&bank,0,0x09cc));
        sony=spider_sdl3_bank_get(&bank,0x054c,0x09cc);
        xbox=spider_sdl3_bank_get(&bank,0x045e,0x028e);
        assert(sony && xbox && sony!=xbox && bank.used==2);
        assert(spider_sdl3_profile_valid(sony));
        assert(spider_sdl3_profile_valid(xbox));
        assert(sony->actions[SPIDER_SDL3_ACTION_SWING].kind==
               SPIDER_SDL3_BIND_TRIGGER);
        assert(sony->actions[SPIDER_SDL3_ACTION_SWING].index==
               SPIDER_SDL3_AXIS_RT);
        /* Different SDL3 model profiles persist independently within bank. */
        assert(spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_JUMP,
                spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        assert(sony->actions[SPIDER_SDL3_ACTION_JUMP].index==2);
        assert(sony->actions[SPIDER_SDL3_ACTION_PUNCH].index==0);
        assert(xbox->actions[SPIDER_SDL3_ACTION_JUMP].index==0);
        again=spider_sdl3_bank_get(&bank,0x054c,0x09cc);
        assert(again==sony && bank.used==2);
        copy=*sony;
        /* The Start switch button and OS Guide can never become actions. */
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_SWING,
                spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6)));
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_SWING,
                spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,5)));
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_SWING,
                spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,2)));
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_SWING,
                spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,21)));
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_JUMP,
                spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0)));
        assert(!memcmp(sony,&copy,sizeof(copy)));
        /* Optional pause may be unbound; other actions are mandatory. */
        assert(spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_PAUSE,
                spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0)));
        assert(spider_sdl3_profile_valid(sony));
        assert(!spider_sdl3_profile_assign(sony,SPIDER_SDL3_ACTION_PAUSE,
                spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,1)));
        /* Cleanly validate a corrupted or duplicate loaded profile. */
        copy=*xbox;
        copy.actions[SPIDER_SDL3_ACTION_WEB]=copy.actions[SPIDER_SDL3_ACTION_KICK];
        assert(!spider_sdl3_profile_valid(&copy));
        copy=*xbox;
        copy.actions[SPIDER_SDL3_ACTION_ZIPLINE].kind=9;
        assert(!spider_sdl3_profile_valid(&copy));
        /* Read-only translation of SDL3 button/trigger samples. */
        memset(&s,0,sizeof(s));
        assert(spider_sdl3_profile_neutral(&s));
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask)&&mask==0);
        s.buttons=1u<<SPIDER_SDL3_BUTTON_START;
        assert(!spider_sdl3_profile_neutral(&s));
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask)&&mask==0);
        s.buttons=(1u<<0)|(1u<<3)|(1u<<4);
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask));
        assert(mask==((1u<<SPIDER_SDL3_ACTION_JUMP)|
                       (1u<<SPIDER_SDL3_ACTION_WEB)|
                       (1u<<SPIDER_SDL3_ACTION_PAUSE)));
        /* RT only -> Swing; LT only -> NOT Swing. */
        memset(&s,0,sizeof(s));
        s.axis[SPIDER_SDL3_AXIS_LT]=32767;
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask)&&mask==0);
        s.axis[SPIDER_SDL3_AXIS_LT]=0;
        s.axis[SPIDER_SDL3_AXIS_RT]=32767;
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask));
        assert(mask==1u<<SPIDER_SDL3_ACTION_SWING);
        s.axis[SPIDER_SDL3_AXIS_RT]=6000;
        assert(spider_sdl3_profile_actions(xbox,&s,mask,&mask));
        assert(mask==1u<<SPIDER_SDL3_ACTION_SWING); /* hysteresis */
        s.axis[SPIDER_SDL3_AXIS_RT]=4999;
        assert(spider_sdl3_profile_actions(xbox,&s,mask,&mask));
        assert(mask==0);
        s.axis[SPIDER_SDL3_AXIS_RT]=-1;
        mask=999u;
        assert(!spider_sdl3_profile_actions(xbox,&s,0,&mask)&&mask==0);
        assert(!spider_sdl3_profile_neutral(&s));
        s.axis[SPIDER_SDL3_AXIS_RT]=0;
        /* Trigger remap: LT becomes Swing and RT stops being Swing. */
        assert(spider_sdl3_profile_assign(xbox,SPIDER_SDL3_ACTION_SWING,
                spider_sdl3_bind(SPIDER_SDL3_BIND_TRIGGER,SPIDER_SDL3_AXIS_LT)));
        s.axis[SPIDER_SDL3_AXIS_RT]=32767;
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask)&&mask==0);
        s.axis[SPIDER_SDL3_AXIS_RT]=0;
        s.axis[SPIDER_SDL3_AXIS_LT]=32767;
        assert(spider_sdl3_profile_actions(xbox,&s,0,&mask));
        assert(mask==1u<<SPIDER_SDL3_ACTION_SWING);
        /* Bank capacity and invalid IDs fail closed without overflow. */
        for(i=0;i<SPIDER_SDL3_PROFILE_BANK_CAPACITY-2;i++)
          assert(spider_sdl3_bank_get(&bank,0x4000,(uint16_t)(i+1)));
        assert(bank.used==SPIDER_SDL3_PROFILE_BANK_CAPACITY);
        assert(!spider_sdl3_bank_get(&bank,0x9000,0x9000));
        assert(spider_sdl3_bank_get(&bank,0x045e,0x028e)==xbox);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_profile_model_cannot_change_native_game_while_in_design_phase():
    h=(ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    assert "spider_sdl3_profile_assign(" in h
    assert "spider_sdl3_bank_get(" in h
    assert "spider_sdl3_profile_neutral(" in h
    assert "spider_sdl3_profile_actions(" in h
    assert "SPIDER_SDL3_BUTTON_START=6" in h
    assert "SPIDER_SDL3_AXIS_RT=5" in h
    assert "same action meaning for DS4/Xbox" in h
    for forbidden in (
        "#include <windows.h>", "VirtualProtect", "SendInput",
        "IDirectInput", "WritePrivateProfileString", "CreateFile",
        "SDL_UpdateGamepads", "SDL_PumpEvents",
    ):
        assert forbidden not in h
    native=(ROOT/"native_joystick_reconnect.c").read_text(encoding="utf-8")
    assert "NATIVE-SWAP: REFUSED CUSTOM joystick mapping" in native
    lock=(ROOT/"ps1_joystick_readonly_menu.c").read_text(encoding="utf-8")
    assert "joystick-only mapping branch" in lock


def test_readonly_selected_sdl3_profile_trace_is_opt_in_and_neutral_gated():
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    validator=(ROOT/"ini_validation.h").read_text(encoding="utf-8")
    assert '"SDL3BindingTrace"' in validator
    assert 'GetPrivateProfileIntA("Debug","SDL3BindingTrace",0,ini)==1' in probe
    assert 'GetPrivateProfileIntA("Input","ExperimentalSDL3Probe",0,ini)==1' in probe
    assert 'bindings_trace_lines>=48u' in probe
    assert 'spider_sdl3_bank_reset(&bindings_bank)' in probe
    assert 'spider_sdl3_bank_get(&bindings_bank,vid,pid)' in probe
    assert 'spider_sdl3_profile_neutral(&sample)' in probe
    assert 'spider_sdl3_profile_actions(profile,&sample,' in probe
    assert 'bindings_trace_armed=0;bindings_previous=0;' in probe
    assert 'sdl3_binding_trace_on_flip();' in probe
    trace=probe.split('static void sdl3_binding_trace_on_flip(void) {',1)[1].split(
        'void SpiderSDL3Probe_OnFlip(',1)[0]
    for unsafe in ('SendInput(', 'IDirectInput', 'VirtualProtect(', 
                   'WriteFile(', 'WritePrivateProfileString',
                   'sdl_pump(', 'sdl_update(', 'SDL_UpdateGamepads'):
        assert unsafe not in trace
    assert 'SDL3-BINDINGS: selected-id=' in trace
    assert 'READ-ONLY; NOT GAMEPLAY' in trace
    assert 'SPIDER_SDL3_BUTTON_COUNT' in trace
