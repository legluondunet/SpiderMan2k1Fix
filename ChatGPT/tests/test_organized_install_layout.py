"""Single runtime data folder: proxy loading, INI migration, PNG and CI layout.

These are cross-platform source contracts; they do not claim hardware Wine
or Windows path/permissions testing without a native runtime.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(name):
    return (ROOT / name).read_text(encoding="utf-8")


def test_proxy_keeps_root_winmm_but_uses_nested_core_and_early_diagnostics():
    proxy=read("winmm_proxy.c")
    paths=read("spider_fix_paths.h")
    assert 'spider_path_code_asset_a("SpiderMan2k1Fix.dll",path,sizeof(path))' in proxy
    assert 'spider_path_log_a(path,sizeof(path))' in proxy
    assert 'spider_path_ini_a(ini,sizeof(ini))' in proxy
    assert 'GetPrivateProfileIntA("Debug","Log",0,ini)!=1' in proxy
    assert 'InitOnceExecuteOnce(&log_reset_once,reset_log_on_first_write,path,NULL)' in proxy
    assert 'f=fopen((const char*)arg,"w");' in proxy
    assert '"SpiderMan2k1Fix"' in paths
    assert 'spider_path_code_asset_a(' in paths
    assert 'spider_path_file_a(preferred)' in paths
    assert 'spider_path_file_a(legacy)' in paths
    # Expensive or blocking filesystem operations must not happen in DllMain.
    dllmain=proxy.split("BOOL WINAPI DllMain(",1)[1]
    assert "patch_early();" in dllmain
    for unsafe in ("CreateDirectoryA(", "CopyFileA(", "LoadLibraryA(",
                   "spider_path_ini_a(", "spider_path_log_a("):
        assert unsafe not in dllmain


def test_log_defaults_to_executable_directory_and_keeps_readonly_fallback():
    paths=read("spider_fix_paths.h")
    resolver=paths.split("static inline int spider_path_log_a(",1)[1].split(
        "static inline int spider_path_code_asset_a(",1)[0]
    assert 'spider_path_exe_dir_a(exe,sizeof(exe))' in resolver
    assert 'spider_path_join_a(target,sizeof(target),exe,"SpiderMan2k1Fix.log")' in resolver
    assert 'spider_path_roots_a(game,sizeof(game),local,sizeof(local))' in resolver
    assert 'spider_path_join_a(target,sizeof(target),local,"SpiderMan2k1Fix.log")' in resolver
    assert 'spider_path_writable_a(exe)' in resolver
    assert 'spider_path_writable_a(local)' in resolver
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in resolver
    assert "FILE_ATTRIBUTE_READONLY" in resolver
    assert "spider_path_ini_a(" not in resolver
    # Both startup proxy and main core write to the one shared resolver.
    proxy=read("winmm_proxy.c")
    core=read("spiderman2k1fix.c")
    assert 'spider_path_log_a(path,sizeof(path))' in proxy
    assert 'spider_path_log_a(path,sizeof(path))' in core
    assert "reset_log_on_first_write" in proxy


def test_core_uses_same_resolver_for_every_main_ini_subsystem():
    core=read("spiderman2k1fix.c")
    paths=read("spider_fix_paths.h")
    for module in ("virtual_spidey_cfg.c", "pause_surface.c",
                   "quick_exit_f12.c", "sdl3_gamepad_probe.c"):
        assert '#include "spider_fix_paths.h"' in read(module), module
    assert '#include "spider_fix_paths.h"' in core
    assert "spider_path_ini_a(path,sizeof(path))" in core
    assert "spider_path_log_a(path,sizeof(path))" in core
    assert "spider_path_ini_a(input_ini,sizeof(input_ini))" in core
    assert "spider_path_ini_a(ini,sizeof(ini))" in core
    assert "spider_path_ini_a(ini_path,sizeof(ini_path))" in core
    assert "return spider_path_ini_a(ini,cap);" in read("virtual_spidey_cfg.c")
    assert "spider_path_code_asset_a(\"SDL3.dll\",path,sizeof(path))" in read("sdl3_gamepad_probe.c")
    assert "SpiderCfg_EnsureDefaultIni(log_line);" in core
    # Native game resources still use their original executable paths.
    assert "SPIDER_SDL3" not in paths


def test_ini_migration_is_lossless_atomic_and_fallback_safe():
    p=read("spider_fix_paths.h")
    assert 'spider_path_join_a(legacy,sizeof(legacy),exe_dir,"SpiderMan2k1Fix.ini")' in p
    assert 'spider_path_migrate_ini_a(legacy,target)' in p
    assert "CopyFileA(legacy,temp,TRUE)" in p
    assert "MoveFileExA(temp,dest,MOVEFILE_WRITE_THROUGH)" in p
    assert "DeleteFileA(temp);" in p
    assert "MoveFileExA(temp,dest,MOVEFILE_REPLACE_EXISTING" not in p
    assert "DeleteFileA(legacy)" not in p
    assert "spider_path_dir_a(game,1)" in p
    assert "spider_path_dir_a(local,1)" in p
    assert 'GetEnvironmentVariableA("LOCALAPPDATA"' in p
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in p
    assert "spider_path_writable_a(local)" in p
    assert "spider_path_writable_a(game)" in p
    assert "FILE_FLAG_DELETE_ON_CLOSE" in p
    assert "return spider_path_file_a(dest);" in p


def test_f9_atomic_pngs_stay_together_in_screenshots_folder():
    png=read("f9_png.c")
    paths=read("spider_fix_paths.h")
    assert 'spider_path_screenshots_w(exe,MAX_PATH)' in png
    assert 'L"%ls\\\\screenshots"' in paths
    assert "spider_path_writable_w(out)" in paths
    assert "GetEnvironmentVariableW(L\"LOCALAPPDATA\"" in paths
    assert 'L"%ls\\\\scrn%04u.png"' in png
    assert 'L"%ls\\\\SpiderF9PNG-%lu-%ld.tmp"' in png
    assert "MoveFileExW(temp,dest,MOVEFILE_WRITE_THROUGH)" in png
    assert "spider_png_verify(dest)" in png
    assert "SpiderF9Toast_PublishSavedPath(dest)" in png
    assert "SpiderF9Toast_PublishSavedPathComic(dest)" in png


def test_artifact_contains_one_root_proxy_and_one_nested_core():
    ci=read(".github/workflows/spiderman2k1fix-windows-x86.yml")
    assert "mkdir -p dist/package/SpiderMan2k1Fix" in ci
    assert "cp winmm.dll dist/package/" in ci
    assert "cp SpiderMan2k1Fix.dll README.md CHANGELOG.md dist/package/SpiderMan2k1Fix/" in ci
    assert "cp SDL3.dll" not in ci
    assert "SpiderMan2k1Fix.ini" not in ci
    assert "path: dist/package/" in ci
    assert ci.count("uses: actions/upload-artifact@v4") == 1
    upload=ci.split("      - name: Upload installation files as one ZIP (no nested ZIP)",1)[1]
    assert "dist/release/" not in upload
    assert "name: SpiderMan2k1Fix-Windows-x86" in upload
    assert "cmp README.md dist/package/SpiderMan2k1Fix/README.md" in ci
