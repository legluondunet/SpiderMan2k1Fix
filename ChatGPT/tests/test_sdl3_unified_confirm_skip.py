"""Single configurable SDL3 Confirm/Skip UI action, v2 files unchanged."""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT = Path(__file__).resolve().parents[2]


def test_confirm_skip_uses_one_visible_binding_and_preserves_legacy_mixed(tmp_path):
    cc = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:
        return
    src = tmp_path / "unified.c"
    exe = tmp_path / "unified"
    src.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include <string.h>
        #include "sdl3_controller_profile_ini.h"
        #define C SPIDER_SDL3_CONTEXT_CONFIRM
        #define B SPIDER_SDL3_CONTEXT_BACK
        #define V SPIDER_SDL3_CONTEXT_SKIP_VIDEO
        #define S SPIDER_SDL3_CONTEXT_NEXT_STILL
        #define K SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE
        int main(void) {
          SpiderSDL3ControllerProfile p,old,decoded;
          SpiderSDL3Binding shared,a,b,x;
          size_t len=0;
          char txt[SPIDER_SDL3_PROFILE_INI_MAX+1];
          assert(SPIDER_SDL3_ACTION_COUNT==8);
          assert(SPIDER_SDL3_CONTEXT_COUNT==5);
          assert(SPIDER_SDL3_SETUP_ACTION_COUNT==13); /* unchanged v2 */
          assert(SPIDER_SDL3_VISIBLE_ACTION_COUNT==10);
          a=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,0);
          b=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,1);
          x=spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,2);
          assert(spider_sdl3_profile_default(&p,0x054c,0x09cc));
          assert(spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          assert(spider_sdl3_binding_equal(shared,a));
          assert(spider_sdl3_binding_equal(p.context[K],a));
          /* An older v2 with North for scripted skip remains MIXED.
             Reading/encoding has no implicit migration or edits. */
          assert(spider_sdl3_profile_assign_context(&p,K,
                      spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,3)));
          old=p;
          assert(!spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          assert(spider_sdl3_ini_encode(&p,txt,sizeof(txt),&len));
          assert(strstr(txt,"SkipCutscene=button:3\r\n"));
          assert(spider_sdl3_ini_decode(txt,len,0x054c,0x09cc,&decoded));
          assert(!memcmp(&decoded,&old,sizeof(old)));
          /* Moving Back on mixed profile must not silently homogenize
             the three non-menu keys. */
          assert(spider_sdl3_profile_assign_back(&p,a));
          assert(spider_sdl3_binding_equal(p.context[C],b));
          assert(p.context[K].index==3);
          assert(!spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          /* Single Confirm/Skip capture replaces FOUR keys at once,
             while keeping 3D Jump and other 3D actions untouched. */
          old=p;
          assert(spider_sdl3_profile_assign_visible(&p,8,x));
          assert(spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          assert(spider_sdl3_binding_equal(shared,x));
          assert(spider_sdl3_binding_equal(p.context[C],x));
          assert(spider_sdl3_binding_equal(p.context[V],x));
          assert(spider_sdl3_binding_equal(p.context[S],x));
          assert(spider_sdl3_binding_equal(p.context[K],x));
          assert(!memcmp(old.actions,p.actions,sizeof(p.actions)));
          assert(spider_sdl3_binding_equal(p.context[B],a));
          /* Bind Confirm/Skip to Back key: previous menu confirm becomes
             Back, and all four linked keys remain synchronized. */
          assert(spider_sdl3_profile_assign_visible(&p,8,a));
          assert(spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          assert(spider_sdl3_binding_equal(shared,a));
          assert(spider_sdl3_binding_equal(p.context[B],x));
          /* Bind Back to Confirm/Skip key: swap both as one group. */
          assert(spider_sdl3_profile_assign_visible(&p,9,a));
          assert(spider_sdl3_profile_shared_confirm_binding(&p,&shared));
          assert(spider_sdl3_binding_equal(shared,x));
          assert(spider_sdl3_binding_equal(p.context[B],a));
          assert(!memcmp(old.actions,p.actions,sizeof(p.actions)));
          assert(!spider_sdl3_profile_assign_visible(&p,10,a));
          assert(!spider_sdl3_profile_assign_visible(&p,8,
              spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,6)));
          assert(!spider_sdl3_profile_assign_visible(&p,9,
              spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,5)));
          assert(spider_sdl3_profile_valid(&p));
          /* Changing a 3D binding never changes UI/menu defaults. */
          old=p;
          assert(spider_sdl3_profile_assign_visible(&p,0,
                      spider_sdl3_bind(SPIDER_SDL3_BIND_BUTTON,4)));
          assert(!memcmp(old.context,p.context,sizeof(p.context)));
          assert(spider_sdl3_ini_encode(&p,txt,sizeof(txt),&len));
          assert(spider_sdl3_ini_decode(txt,len,0x054c,0x09cc,&decoded));
          assert(!memcmp(&decoded,&p,sizeof(p)));
          return 0;
        }
    """), encoding="utf-8")
    subprocess.run([cc, "-std=c99", "-Wall", "-Wextra", "-Werror",
                    "-I", str(ROOT), str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)


def test_wizard_is_ten_rows_and_does_not_touch_game_native_input():
    ui = (ROOT / "sdl3_controller_setup.c").read_text(encoding="utf-8")
    model = (ROOT / "sdl3_controller_profile.h").read_text(encoding="utf-8")
    ini = (ROOT / "sdl3_controller_profile_ini.h").read_text(encoding="utf-8")
    assert "for(i=0;i<SPIDER_SDL3_VISIBLE_ACTION_COUNT;i++)" in ui
    assert "SPIDER_SDL3_VISIBLE_ACTION_COUNT=SPIDER_SDL3_ACTION_COUNT+2" in model
    assert "spider_sdl3_profile_shared_confirm_binding(p,&shared)" in ui
    assert "MIXED (Capture to unify)" in ui
    assert "spider_sdl3_profile_assign_visible(&trial,ui->action,candidate)" in ui
    assert "Confirm / Skip" in ui and "Back / Cancel" in ui
    assert '"[MenuBindings]' in ini and 'Version=2' in ini
    assert "SPIDER_SDL3_CONTEXT_SKIP_CUTSCENE" in model
    assert "spider_sdl3_profile_assign_context(" in model
    assert "SPIDER_SDL3_SETUP_ACTION_COUNT" not in ui
    for denied in ("SendInput(", "VirtualProtect(", "WriteProcessMemory(",
                   "IDirectInputDevice8_", "CreateThread("):
        assert denied not in ui
