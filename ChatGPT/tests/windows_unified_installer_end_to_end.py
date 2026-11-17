#!/usr/bin/env python3
"""Windows PE32 synthetic-ISO installation, rollback and integrity checks.

Never uses/distributes original commercial data. The TEST-ONLY fixture accepts
a deterministic non-runnable fake original game executable; the production
experimental launcher must reject that fake hash.
"""
from pathlib import Path
import tempfile, sys
from windows_installer_end_to_end import make_iso, execute, scenario, FILES

DIRS=("game","third_party","docs","linux","state")
OWNED=("game/fix.dll","third_party/SDL3/SDL3.dll",
       "third_party/SDL3/LICENSE.txt","linux/spiderman-focus-helper",
       "docs/README.md","docs/CHANGELOG.md","docs/LICENSE")
def empty(root):
    assert all(not (root/x).exists() for x in DIRS)
    assert not (root/"spiderman2k1fix.unified.installing").exists()

def install(root,fixture):
    d,e=scenario(root,"successful_install",fixture)
    (d/"original.iso").write_bytes(make_iso())
    log=execute(e,"--install-only",success=True)
    assert "UNIFIED: installed game/" in log,log
    for item in (*OWNED,*("game/"+x for x in FILES)):
        assert (d/item).is_file(),item
    for item in ("installed-game.sha256","installed-resources.sha256"):
        assert (d/"state"/item).is_file(),item
    assert not (d/"spiderman2k1fix.unified.installing").exists()
    execute(e,"--verify",success=True)
    return d,e

def main():
    if sys.platform!="win32":raise SystemExit("Windows only")
    if len(sys.argv)!=3:raise SystemExit("usage: script PRODUCTION.exe FIXTURE.exe")
    real,fixture=(Path(a).resolve() for a in sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="spider-unified-layout-") as td:
        base=Path(td)
        for case in ("no_game","unsigned_synthetic_original"):
            d,e=scenario(base,case,real)
            if case=="unsigned_synthetic_original":
                (d/"original.iso").write_bytes(make_iso())
            execute(e,"--install-only",success=False)
            empty(d)
        d,e=scenario(base,"incomplete_original",fixture)
        (d/"original.iso").write_bytes(make_iso(missing="data.pkr"))
        execute(e,"--install-only",success=False)
        empty(d)
        d,e=scenario(base,"occupied_docs",fixture)
        (d/"original.iso").write_bytes(make_iso())
        (d/"docs").mkdir()
        (d/"docs"/"do-not-delete").write_text("personal")
        execute(e,"--install-only",success=False)
        assert (d/"docs"/"do-not-delete").read_text()=="personal"
        assert not (d/"game").exists()
        d,e=scenario(base,"rollback_after_two_publishes",fixture)
        (d/"original.iso").write_bytes(make_iso())
        log=execute(e,"--install-only",success=False,
                    environment={"SPIDER_UNIFIED_TEST_FAIL":"1"})
        assert "rolling back" in log,log
        empty(d)
        d,e=scenario(base,"occupied_stage",fixture)
        (d/"original.iso").write_bytes(make_iso())
        (d/"spiderman2k1fix.unified.installing").mkdir()
        (d/"spiderman2k1fix.unified.installing"/"sentinel").write_text("keep")
        execute(e,"--install-only",success=False)
        assert (d/"spiderman2k1fix.unified.installing"/"sentinel").read_text()=="keep"
        assert not (d/"game").exists()
        d,e=install(base,fixture)
        # The synthetic retail EXE is deliberately NOT runnable, but the
        # launcher still must create the one-shot runtime startup log before
        # CreateProcessW. The default INI does not exist in this fixture.
        ini=d/"SpiderMan2k1Fix.ini"
        runtime_log=d/"SpiderMan2k1Fix.log"
        assert not ini.exists()
        launch=execute(e,success=False)
        assert "FIRST-RUN-LOG: first game session" in launch
        assert runtime_log.is_file()
        initial=runtime_log.read_bytes()
        assert b"FIRST-RUN-LOG: initial launcher startup" in initial
        assert not ini.exists(), "fixture has no real game to create an INI"
        # A user INI must suppress the automatic first-launch override,
        # regardless of whether the user selects 0 or 1. The fake game
        # cannot generate runtime logs itself; only the launcher is tested.
        for setting in (b"[Debug]\r\nLog=0\r\n",
                        b"[Debug]\r\nLog=1\r\n"):
            ini.write_bytes(setting)
            second=execute(e,success=False)
            assert "FIRST-RUN-LOG: first game session" not in second
            assert runtime_log.read_bytes()==initial
            assert ini.read_bytes()==setting
        # Do not append diagnostics through an unexpected directory/reparse
        # destination. The launcher must refuse this unsafe log path.
        preserved=d/"SpiderMan2k1Fix.log.preserved"
        runtime_log.rename(preserved)
        runtime_log.mkdir()
        refused=execute(e,success=False)
        assert "unsafe existing log path; refusing launch" in refused
        assert runtime_log.is_dir()
        runtime_log.rmdir()
        preserved.rename(runtime_log)
        assert runtime_log.read_bytes()==initial
        archive=d/"game/data.pkr"; original=archive.read_bytes()
        archive.write_bytes(original+b"corrupt")
        execute(e,"--verify",success=False)
        assert archive.read_bytes().endswith(b"corrupt")
        archive.write_bytes(original)
        execute(e,"--verify",success=True)
        receipt=d/"state/installed-resources.sha256"; data=receipt.read_bytes()
        receipt.write_bytes(data+b"tampered")
        execute(e,"--verify",success=False)
        execute(e,"--install-only",success=False)
        assert receipt.read_bytes()==data+b"tampered"
        receipt.write_bytes(data)
        execute(e,"--verify",success=True)
        lic=d/"third_party/SDL3/LICENSE.txt"; license_bytes=lic.read_bytes()
        lic.write_bytes(license_bytes+b"user-change")
        execute(e,"--verify",success=False)
        execute(e,"--install-only",success=False)
        assert lic.read_bytes().endswith(b"user-change")
        lic.write_bytes(license_bytes)
        execute(e,"--verify",success=True)
        d,e=scenario(base,"legacy_declined",fixture)
        (d/"original.iso").write_bytes(make_iso())
        legacy=d/"spiderman2001";legacy.mkdir()
        (legacy/"saves").write_text("preserve")
        execute(e,"--install-only",success=False)
        assert (legacy/"saves").read_text()=="preserve"
        assert not (d/"game").exists()
    print("PASS: unified layout receipts, rollback, corruption, foreign files and legacy refusal")
if __name__=="__main__":
    main()
