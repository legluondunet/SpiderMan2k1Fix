"""SDL3 controller profiles v2: independent UI/media bindings and v1 migration."""
from pathlib import Path
import subprocess
import shutil
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_profile_v2_backward_compatible_contexts_and_corruption(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    source=tmp_path/"v2.c"
    exe=tmp_path/"v2"
    source.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <string.h>
      #include <stdio.h>
      #include "sdl3_controller_profile_ini.h"
      int main(void) {
        SpiderSDL3ControllerProfile profile,loaded,legacy_loaded,saved;
        SpiderSDL3Binding a,b;
        char out[SPIDER_SDL3_PROFILE_INI_MAX+1];
        char legacy[SPIDER_SDL3_PROFILE_INI_MAX+1];
        char bad[SPIDER_SDL3_PROFILE_INI_MAX+1];
        char *p;
        size_t length=0;
        assert(spider_sdl3_profile_default(&profile,0x054c,0x09cc));
        assert(SPIDER_SDL3_ACTION_COUNT==8);
        assert(SPIDER_SDL3_CONTEXT_COUNT==5);
        assert(SPIDER_SDL3_SETUP_ACTION_COUNT==13);
        a=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,0);
        b=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,1);
        /* Reuse A for 3D Jump, menu Confirm, movie Skip and NextStill. */
        assert(spider_sdl3_binding_equal(profile.actions[0],a));
        assert(spider_sdl3_binding_equal(profile.context[0],a));
        assert(spider_sdl3_binding_equal(profile.context[2],a));
        assert(spider_sdl3_binding_equal(profile.context[3],a));
        assert(spider_sdl3_profile_valid(&profile));
        /* Same-context menu confirm/back must swap instead of conflicting. */
        assert(spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_BACK,a));
        assert(spider_sdl3_binding_equal(profile.context[0],b));
        assert(spider_sdl3_binding_equal(profile.context[1],a));
        assert(spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_BACK,b));
        assert(spider_sdl3_binding_equal(profile.context[0],a));
        assert(spider_sdl3_binding_equal(profile.context[1],b));
        /* Movie skip and fixed second screen may be disabled independently. */
        assert(spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_SKIP_VIDEO,
                    spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0)));
        assert(spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_NEXT_STILL,
                    spider_sdl3_bind(SPIDER_SDL3_BIND_NONE,0)));
        assert(!spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_CONFIRM,
                    spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6)));
        assert(!spider_sdl3_profile_assign_context(&profile,
                    SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE,
                    spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,5)));
        assert(!spider_sdl3_profile_assign_context(&profile,5,a));
        assert(!spider_sdl3_profile_assign_setup(&profile,13,a));
        assert(spider_sdl3_profile_setup_binding(&profile,13).kind==0);
        assert(spider_sdl3_profile_assign_setup(&profile,12,
                    spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        assert(profile.context[4].index==2);
        assert(spider_sdl3_ini_encode(&profile,out,sizeof(out),&length));
        assert(length>210 && length<SPIDER_SDL3_PROFILE_INI_MAX);
        assert(strstr(out,"Version=2\r\n"));
        assert(strstr(out,"[MenuBindings]\r\n"));
        assert(strstr(out,"SkipVideo=none\r\n"));
        assert(strstr(out,"NextStill=none\r\n"));
        assert(strstr(out,"SkipCutscene=button:2\r\n"));
        assert(spider_sdl3_ini_decode(out,length,0x054c,0x09cc,&loaded));
        assert(!memcmp(&profile,&loaded,sizeof(profile)));
        /* Form a byte-exact v1 file from the v2's original 8-action block.
         * Loading must preserve its 8 action bindings and populate five
         * default context keys, without touching disk or a new file. */
        memcpy(legacy,out,length+1);
        p=strstr(legacy,"[MenuBindings]\r\n");
        assert(p);
        *p=0;
        p=strstr(legacy,"Version=2");
        assert(p);
        p[8]='1';
        assert(spider_sdl3_ini_decode(legacy,strlen(legacy),
                   0x054c,0x09cc,&legacy_loaded));
        assert(!memcmp(profile.actions,legacy_loaded.actions,
                       sizeof(profile.actions)));
        assert(legacy_loaded.context[SPIDER_SDL3_CONTEXT_CONFIRM].index==0);
        assert(legacy_loaded.context[SPIDER_SDL3_CONTEXT_BACK].index==1);
        assert(legacy_loaded.context[SPIDER_SDL3_CONTEXT_SKIP_VIDEO].kind==
               SPIDER_SDL3_BIND_BUTTON);
        assert(legacy_loaded.context[SPIDER_SDL3_CONTEXT_NEXT_STILL].index==0);
        assert(legacy_loaded.context[SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE].index==0);
        assert(spider_sdl3_ini_encode(&legacy_loaded,out,sizeof(out),&length));
        assert(strstr(out,"Version=2\r\n"));
        assert(spider_sdl3_ini_decode(out,length,0x054c,0x09cc,&saved));
        assert(!memcmp(&legacy_loaded,&saved,sizeof(saved)));
        /* Fail closed: v2 requires all five keys, exactly once, in the
         * correct section. Invalid files cannot partially mutate output. */
        loaded=saved;
        memcpy(bad,out,length+1);
        p=strstr(bad,"NextStill=button:0\r\n");
        assert(p);
        memmove(p,p+strlen("NextStill=button:0\r\n"),
                strlen(p+strlen("NextStill=button:0\r\n"))+1);
        assert(!spider_sdl3_ini_decode(bad,strlen(bad),
                                        0x054c,0x09cc,&loaded));
        assert(!memcmp(&loaded,&saved,sizeof(saved)));
        memcpy(bad,out,length+1);
        strcat(bad,"MenuConfirm=button:0\r\n");
        assert(!spider_sdl3_ini_decode(bad,strlen(bad),
                                        0x054c,0x09cc,&loaded));
        memcpy(bad,out,length+1);
        p=strstr(bad,"MenuBack=button:1");
        assert(p);
        p[strlen("MenuBack=button:")]='0';
        assert(!spider_sdl3_ini_decode(bad,strlen(bad),
                                        0x054c,0x09cc,&loaded));
        memcpy(bad,out,length+1);
        p=strstr(bad,"Version=2");
        assert(p);
        p[8]='1'; /* v1 must not contain a v2-only section */
        assert(!spider_sdl3_ini_decode(bad,strlen(bad),
                                        0x054c,0x09cc,&loaded));
        return 0;
      }
    """), encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(source),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_ui_context_remapping_and_v1_backup_contract():
    header=(ROOT/"sdl3_controller_profile.h").read_text(encoding="utf-8")
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    store=(ROOT/"sdl3_controller_profile_store.c").read_text(encoding="utf-8")
    codec=(ROOT/"sdl3_controller_profile_ini.h").read_text(encoding="utf-8")
    assert "SPIDER_SDL3_VISIBLE_ACTION_COUNT" in ui
    assert "SPIDER_SDL3_SETUP_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT+SPIDER_SDL3_CONTEXT_COUNT" in header
    assert "spider_sdl3_profile_shared_confirm_binding(p,&shared)" not in ui
    assert "spider_sdl3_profile_assign_visible(&trial,ui->action,candidate)" in ui
    assert "Cross/A Confirm, Circle/B Back fixed" in ui
    assert "Back / Cancel" not in ui
    assert "MIXED (Capture to unify)" not in ui
    assert "SPIDER_SDL3_VISIBLE_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT," in header
    assert "spider_sdl3_context_names[i]" not in ui
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" in header
    assert "spider_sdl3_profile_assign_setup(" in header
    assert '"[MenuBindings]' in codec
    assert 'Version=2' in codec
    assert 'version==1u && section==2' in codec
    assert 'version==2u && section==3' in codec
    assert '"%s.v1.bak"' in store
    assert 'GetPrivateProfileIntA("Profile","Version",0,path)!=1' in store
    assert 'CopyFileA(path,backup,TRUE)!=0' in store
    for illegal in ("SendInput(", "VirtualProtect(", "WriteProcessMemory(",
                    "IDirectInputDevice8_", "WritePrivateProfileString("):
        assert illegal not in ui and illegal not in header and illegal not in codec
