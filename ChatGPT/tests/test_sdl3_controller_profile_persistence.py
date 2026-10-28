"""Per-model SDL3 storage: native portable INI codec and safety contracts.

Windows filesystem write behavior needs Wine/Windows physical acceptance.
These tests prove schema, reject corruption and prevent native engine writes.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_profile_ini_codec_roundtrip_and_fail_closed(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src=tmp_path/"ini_codec.c"
    exe=tmp_path/"ini_codec"
    src.write_text(textwrap.dedent(r"""
      #include <assert.h>
      #include <stdio.h>
      #include <string.h>
      #include "sdl3_controller_profile_ini.h"
      int main(void) {
        SpiderSDL3ControllerProfile original,decoded,unchanged,trial;
        char buffer[SPIDER_SDL3_PROFILE_INI_MAX+1],tampered[2300];
        size_t len=0;
        SpiderSDL3Binding binding;
        const char *start,*end;
        assert(spider_sdl3_profile_default(&original,0x054c,0x09cc));
        assert(spider_sdl3_ini_encode(&original,buffer,sizeof(buffer),&len));
        assert(len>90 && len<400);
        assert(strstr(buffer,"Vendor=0x054C\r\n"));
        assert(strstr(buffer,"Product=0x09CC\r\n"));
        assert(strstr(buffer,"Swing=trigger:5\r\n"));
        assert(spider_sdl3_ini_decode(buffer,len,0x054c,0x09cc,&decoded));
        assert(!memcmp(&original,&decoded,sizeof(original)));
        assert(spider_sdl3_profile_assign(&original,SPIDER_SDL3_ACTION_JUMP,
                 spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2)));
        assert(spider_sdl3_ini_encode(&original,buffer,sizeof(buffer),&len));
        assert(spider_sdl3_ini_decode(buffer,len,0x054c,0x09cc,&decoded));
        assert(!memcmp(&original,&decoded,sizeof(original)));
        assert(!spider_sdl3_ini_decode(buffer,len,0x045e,0x028e,&decoded));
        assert(!spider_sdl3_ini_encode(&original,buffer,12,&len));
        assert(spider_sdl3_ini_encode(&original,buffer,sizeof(buffer),&len));
        assert(!spider_sdl3_ini_decode(buffer,0,0x054c,0x09cc,&decoded));
        assert(!spider_sdl3_ini_decode(buffer,SPIDER_SDL3_PROFILE_INI_MAX+1u,
                                       0x054c,0x09cc,&decoded));
        unchanged=decoded;
        memcpy(tampered,buffer,len);
        tampered[len]=0;
        /* Version changes are incompatible, not silently accepted. */
        start=strstr(tampered,"Version=1");
        assert(start);
        tampered[start-tampered+8]='2';
        assert(!spider_sdl3_ini_decode(tampered,len,0x054c,0x09cc,&decoded));
        assert(!memcmp(&decoded,&unchanged,sizeof(decoded)));
        memcpy(tampered,buffer,len);tampered[len]=0;
        start=strstr(tampered,"Jump=button:");
        assert(start);
        tampered[start-tampered+12]='6'; /* reserved START button */
        assert(!spider_sdl3_ini_decode(tampered,len,0x054c,0x09cc,&decoded));
        memcpy(tampered,buffer,len);tampered[len]=0;
        start=strstr(tampered,"Swing=trigger:5");
        assert(start);
        tampered[start-tampered+14]='3'; /* not a trigger */
        assert(!spider_sdl3_ini_decode(tampered,len,0x054c,0x09cc,&decoded));
        memcpy(tampered,buffer,len);tampered[len]=0;
        start=strstr(tampered,"Product=0x09CC");
        assert(start);
        tampered[start-tampered+10]='A'; /* wrong VID/PID */
        assert(!spider_sdl3_ini_decode(tampered,len,0x054c,0x09cc,&decoded));
        /* Duplicate action, unknown field and missing binding. */
        snprintf(tampered,sizeof(tampered),"%s",buffer);
        end=strstr(tampered,"Kick=button:1");
        assert(end);memcpy((char *)end,"Kick=button:0",13);
        assert(!spider_sdl3_ini_decode(tampered,strlen(tampered),
                                        0x054c,0x09cc,&decoded));
        snprintf(tampered,sizeof(tampered),"%s",buffer);
        strcat(tampered,"NotAnAction=button:8\r\n");
        assert(!spider_sdl3_ini_decode(tampered,strlen(tampered),
                                        0x054c,0x09cc,&decoded));
        snprintf(tampered,sizeof(tampered),"%s",buffer);
        start=strstr(tampered,"Pause=button:4");
        assert(start);
        memmove((char *)start,(char *)start+strlen("Pause=button:4\r\n"),
                strlen(start+strlen("Pause=button:4\r\n"))+1);
        assert(!spider_sdl3_ini_decode(tampered,strlen(tampered),
                                        0x054c,0x09cc,&decoded));
        /* duplicate serialized section/key */
        snprintf(tampered,sizeof(tampered),"%s",buffer);
        strcat(tampered,"Jump=button:0\r\n");
        assert(!spider_sdl3_ini_decode(tampered,strlen(tampered),
                                        0x054c,0x09cc,&decoded));
        assert(!spider_sdl3_ini_binding("button:06",&binding));
        assert(!spider_sdl3_ini_binding("button:6",&binding));
        assert(!spider_sdl3_ini_binding("trigger:3",&binding));
        assert(!spider_sdl3_ini_binding("trigger:-1",&binding));
        assert(spider_sdl3_ini_binding("none",&binding));
        assert(binding.kind==SPIDER_SDL3_BIND_NONE);
        /* Pause may be unassigned; other actions may not. */
        trial=original;
        trial.actions[SPIDER_SDL3_ACTION_PAUSE]=spider_sdl3_bind(0,0);
        assert(spider_sdl3_ini_encode(&trial,buffer,sizeof(buffer),&len));
        assert(spider_sdl3_ini_decode(buffer,len,0x054c,0x09cc,&decoded));
        assert(decoded.actions[SPIDER_SDL3_ACTION_PAUSE].kind==0);
        return 0;
      }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(src),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)


def test_win32_controller_storage_is_atomic_isolated_and_fallback_safe():
    store=(ROOT/"sdl3_controller_profile_store.c").read_text(encoding="utf-8")
    probe=(ROOT/"sdl3_gamepad_probe.c").read_text(encoding="utf-8")
    ui=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    build=(ROOT/"build.sh").read_text(encoding="utf-8")
    assert "sdl3_controller_profile_store.c" in build
    assert '"SpiderMan2k1Fix"' in store
    assert r'controllers\\%04X-%04X.ini' in store
    assert '"LOCALAPPDATA"' in store
    assert "GetModuleFileNameA(NULL,exe,sizeof(exe))" in store
    assert "MOVEFILE_REPLACE_EXISTING" in store
    assert "MOVEFILE_WRITE_THROUGH" in store
    assert "FlushFileBuffers(file)" in store
    assert "CREATE_NEW" in store
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in store
    assert "CopyFileA(path,backup,TRUE)" in store
    assert "SpiderSDL3ProfileStore_Load(" in store
    assert "SpiderSDL3ProfileStore_Save(" in store
    for forbidden in ("SendInput(", "VirtualProtect(", "IDirectInput",
                      "Spidey.cfg", "WritePrivateProfileString"):
        # references to untouched Spidey.cfg in comment are okay
        if forbidden=="Spidey.cfg":
            assert '"Spidey.cfg"' not in store
        else:
            assert forbidden not in store
    assert "SpiderSDL3ProfileStore_Init(requested && ini &&" in probe
    assert "bindings_loaded[slot]" in probe
    assert "SpiderSDL3ProfileStore_Load(vid,pid,&trial)" in probe
    assert "SpiderSDL3ProfileStore_Save(candidate)" in probe
    assert "if(!profile || !SpiderSDL3ProfileStore_Save(candidate))return 0;" in probe
    assert "spider_sdl3_get_profile(sdl_vendor(pads[k].id)," in probe
    assert "commit_profile(ui,&trial," in ui
    assert "SDL3-SETUP: SAVE FAILED;" in ui
    assert "Profile" in ui and "SAVED:" in ui
    assert "spider_sdl3_profile_default(&trial,ui->vid,ui->pid)" in ui
    assert "spider_sdl3_profile_assign(&trial,ui->action,candidate)" in ui
    assert "no native gameplay change" in probe or "game-owned DirectInput" in probe
