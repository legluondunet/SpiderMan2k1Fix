"""Regression model for SDL3 modal closing/Win32 focus ownership.

The Win32 API's actual ability to foreground a window is hardware tested.
This tests only whether a modal close is *authorized* to try: never steal
an explicitly external foreground; do not mislabel post-DestroyWindow's
automatic Explorer activation as an Alt+Tab if B/ESC closed an owned modal.
"""
from pathlib import Path
import shutil
import subprocess
import textwrap

ROOT=Path(__file__).resolve().parents[2]


def test_wm_close_owns_snapshot_before_destroy_and_guarded_restore():
    src=(ROOT/"sdl3_controller_setup.c").read_text(encoding="utf-8")
    assert '#include "sdl3_setup_focus_policy.h"' in src
    handler=src.split("case WM_CLOSE:",1)[1].split("case WM_DESTROY:",1)[0]
    assert handler.index("ui->closing_foreground=GetForegroundWindow();") < handler.index(
        "DestroyWindow(hwnd)")
    assert "ui->closing_foreground_captured=1u;" in handler
    outer=src.split("if(setup_report)setup_report(\"SDL3-SETUP: modal message loop EXIT",1)[1]
    assert outer.index("prior_foreground=GetForegroundWindow();") < outer.index(
        "restore_allowed=spider_sdl3_setup_restore_focus(")
    assert outer.index("restore_allowed=spider_sdl3_setup_restore_focus(") < outer.index(
        "SetForegroundWindow(owner)")
    assert "if(restore_allowed && IsWindow(owner) && IsWindowVisible(owner))" in outer
    assert "if(GetForegroundWindow()==owner)SetFocus(owner);" in outer
    assert "preclose=%p" in outer
    for banned in ("AttachThreadInput", "AllowSetForegroundWindow",
                   "SetWindowPos", "BringWindowToTop", "Sleep(",
                   "GetAsyncKeyState"):
        assert banned not in outer


def test_c99_owned_dialog_close_but_explorer_after_destroy_is_authorized(tmp_path):
    cc=shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")
    if not cc:return
    file=tmp_path/"focus.c"
    exe=tmp_path/"focus"
    file.write_text(textwrap.dedent(r"""
        #include <assert.h>
        #include "sdl3_setup_focus_policy.h"
        #define ALLOW(quit,captured,before_owned,after_owned,before_null,after_null) \
          spider_sdl3_setup_restore_focus(quit,captured,before_owned,after_owned,before_null,after_null)
        int main(void) {
          /* B closes our foreground modal. Explorer becomes foreground
             during DestroyWindow; this is NOT an intentional Alt+Tab. */
          assert(ALLOW(0,1,1,0,0,0)==1);
          /* Closing while our owner is foreground is also permitted. */
          assert(ALLOW(0,1,1,1,0,0)==1);
          /* The user DID Alt+Tab before WM_CLOSE. Never steal focus. */
          assert(ALLOW(0,1,0,0,0,0)==0);
          /* Even if game subsequently foreground, the prior event was
             external, so no forced SetForegroundWindow is justified. */
          assert(ALLOW(0,1,0,1,0,0)==0);
          /* No owned WM_CLOSE captured, including abnormal termination. */
          assert(ALLOW(0,0,0,1,0,0)==0);
          assert(ALLOW(0,0,1,1,0,0)==0);
          assert(ALLOW(1,1,1,0,0,0)==0);
          /* Wine NULL on both sides of an owned modal close is allowed;
             NULL before but foreign after is ambiguous and denied. */
          assert(ALLOW(0,1,0,0,1,1)==1);
          assert(ALLOW(0,1,0,1,1,0)==1);
          assert(ALLOW(0,1,0,0,1,0)==0);
          /* Explicit external focus on WM_CLOSE cannot be rescued by
             a NULL foreground following destruction. */
          assert(ALLOW(0,1,0,0,0,1)==0);
          return 0;
        }
    """),encoding="utf-8")
    subprocess.run([cc,"-std=c99","-Wall","-Wextra","-Werror",
                    "-I",str(ROOT),str(file),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
