#!/usr/bin/env python3
"""Windows PE32 synthetic-ISO installation, rollback and integrity checks.

Never uses/distributes original commercial data. The TEST-ONLY fixture accepts
a deterministic non-runnable fake original game executable; the production
experimental launcher must reject that fake hash.
"""
from pathlib import Path
import tempfile, sys
from windows_installer_end_to_end import make_iso, execute, scenario, FILES

DIRS=("game","docs","linux","state")
OWNED=("game/fix.dll","game/SpideyPC.exe.manifest","game/SDL3.dll",
       "docs/SDL3-LICENSE.txt","linux/spiderman-focus-helper",
       "docs/README.md","docs/CHANGELOG.md","docs/LICENSE")
def empty(root):
    assert all(not (root/x).exists() for x in DIRS)
    assert not (root/"spiderman2k1fix.unified.installing").exists()

def install(root,fixture,name="successful_install"):
    d,e=scenario(root,name,fixture)
    (d/"original.iso").write_bytes(make_iso())
    log=execute(e,"--install-only",success=True)
    assert "UNIFIED: installed game/" in log,log
    for item in (*OWNED,*("game/"+x for x in FILES)):
        assert (d/item).is_file(),item
    for item in ("installed-game.sha256","installed-resources.sha256"):
        assert (d/"state"/item).is_file(),item
    assert not (d/"spiderman2k1fix.unified.installing").exists()
    assert not (d/"third_party").exists()
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
        rejected=execute(e,"--install-only",success=False)
        assert "RECEIPT: unexpected trailing data" in rejected
        assert receipt.read_bytes()==data+b"tampered"
        receipt.write_bytes(data)
        execute(e,"--verify",success=True)
        lic=d/"docs/SDL3-LICENSE.txt"; license_bytes=lic.read_bytes()
        lic.write_bytes(license_bytes+b"user-change")
        execute(e,"--verify",success=False)
        rejected=execute(e,"--install-only",success=False)
        assert "RECEIPT: non-DLL resource hash mismatch: docs\\SDL3-LICENSE.txt" in rejected
        assert lic.read_bytes().endswith(b"user-change")
        lic.write_bytes(license_bytes)
        execute(e,"--verify",success=True)
        # The reported incident: a missing DLL alone is repairable, but
        # a later non-DLL mismatch must refuse all writes and explain why.
        sdl=d/"game/SDL3.dll"; sdl_bytes=sdl.read_bytes()
        sdl.unlink()
        lic.write_bytes(license_bytes+b"user-change")
        rejected=execute(e,success=False)
        assert "missing installer-owned runtime DLL game\\SDL3.dll; eligible for repair if all receipts pass" in rejected
        assert "RECEIPT: non-DLL resource hash mismatch: docs\\SDL3-LICENSE.txt" in rejected
        assert not sdl.exists() and receipt.read_bytes()==data
        assert lic.read_bytes().endswith(b"user-change")
        assert not (d/"SpiderMan2k1Fix.updating").exists()
        lic.write_bytes(license_bytes)
        sdl.write_bytes(sdl_bytes)
        # Separate missing receipt and malformed header failures.
        receipt.unlink()
        rejected=execute(e,"--install-only",success=False)
        assert "RECEIPT: resource receipt missing or unsafe" in rejected
        assert not receipt.exists()
        receipt.write_bytes(b"invalid-header\n"+data.split(b"\n",1)[1])
        rejected=execute(e,"--install-only",success=False)
        assert "RECEIPT: invalid resource receipt header" in rejected
        receipt.write_bytes(data)
        execute(e,"--verify",success=True)
        # Authenticated old unified seven-hash receipt: --verify rejects
        # missing manifest, but --install-only upgrades atomically to eight.
        d,e=install(base,fixture,"seven_receipt_upgrade")
        receipt=d/"state/installed-resources.sha256"
        manifest=d/"game/SpideyPC.exe.manifest"
        original_manifest=manifest.read_bytes()
        old=receipt.read_bytes().splitlines(keepends=True)
        assert len(old)==9, len(old)  # header plus eight managed SHA-256
        receipt.write_bytes(b"".join(old[:-1]))
        manifest.unlink()
        execute(e,"--verify",success=False)
        upgraded=execute(e,"--install-only",success=True)
        assert "UPGRADE:" in upgraded
        assert manifest.read_bytes()==original_manifest
        execute(e,"--verify",success=True)
        assert len(receipt.read_bytes().splitlines())==9
        # An unknown manifest accompanying a seven-hash receipt is NEVER
        # silently adopted/overwritten.
        d,e=install(base,fixture,"unknown_manifest_refused")
        receipt=d/"state/installed-resources.sha256"
        manifest=d/"game/SpideyPC.exe.manifest"
        old=receipt.read_bytes().splitlines(keepends=True)
        receipt.write_bytes(b"".join(old[:-1]))
        manifest.write_bytes(b"FOREIGN-PRESERVE-ME")
        execute(e,"--install-only",success=False)
        assert manifest.read_bytes()==b"FOREIGN-PRESERVE-ME"
        # Plain launch auto-repairs an installer-owned fix.dll that differs
        # from the embedded version. Never create persistent fix.dll.bak.
        d,e=install(base,fixture,"auto_replace_fix")
        fix=d/"game/fix.dll"; authentic=fix.read_bytes()
        fix.write_bytes(b"UNKNOWN-OUTDATED-FIX")
        assert not (d/"game/fix.dll.bak").exists()
        execute(e,"--verify",success=False)  # read-only, always
        assert fix.read_bytes()==b"UNKNOWN-OUTDATED-FIX"
        repaired=execute(e,success=False)  # fake EXE cannot run
        assert "replacing automatically without .bak" in repaired
        assert "UPGRADE: replacing game\\fix.dll;" in repaired
        assert fix.read_bytes()==authentic
        assert not (d/"game/fix.dll.bak").exists()
        assert not (d/"SpiderMan2k1Fix.updating").exists()
        execute(e,"--verify",success=True)
        # An already-existing .bak is NOT destroyed (possibly user-owned).
        (d/"game/fix.dll.bak").write_bytes(b"PREEXISTING-BACKUP")
        fix.write_bytes(b"OUTDATED-AGAIN")
        execute(e,success=False)
        assert fix.read_bytes()==authentic
        assert (d/"game/fix.dll.bak").read_bytes()==b"PREEXISTING-BACKUP"
        execute(e,"--verify",success=True)
        (d/"game/fix.dll.bak").unlink()
        # Missing fix.dll with a valid receipt is also restored silently.
        fix.unlink()
        execute(e,success=False)
        assert fix.read_bytes()==authentic
        assert not (d/"game/fix.dll.bak").exists()
        execute(e,"--verify",success=True)
        # A missing or mismatched SDL3.dll alone now uses the same transaction.
        d,e=install(base,fixture,"auto_repair_sdl3")
        fix=d/"game/fix.dll"; sdl=d/"game/SDL3.dll"
        receipt=d/"state/installed-resources.sha256"
        game_receipt=d/"state/installed-game.sha256"
        fix_bytes=fix.read_bytes(); sdl_bytes=sdl.read_bytes()
        original_receipt=receipt.read_bytes()
        original_game_receipt=game_receipt.read_bytes()
        backup=d/"game/SDL3.dll.bak"
        backup.write_bytes(b"KEEP-EXISTING-BACKUP")
        sdl.unlink()
        execute(e,"--verify",success=False)
        assert not sdl.exists()
        denied=execute(e,success=False,
                       environment={"SPIDER_UNIFIED_TEST_UPGRADE_FAIL":"1"})
        assert "automatic SDL3.dll repair refused" in denied
        assert not sdl.exists() and receipt.read_bytes()==original_receipt
        assert backup.read_bytes()==b"KEEP-EXISTING-BACKUP"
        assert not (d/"SpiderMan2k1Fix.updating").exists()
        repaired=execute(e,success=False)  # synthetic game cannot start
        assert "SDL3.dll differs from embedded version; repairing automatically" in repaired
        assert sdl.read_bytes()==sdl_bytes
        assert backup.read_bytes()==b"KEEP-EXISTING-BACKUP"
        assert fix.read_bytes()==fix_bytes
        execute(e,"--verify",success=True)
        sdl.write_bytes(b"UNKNOWN-SDL3")
        execute(e,success=False)
        assert sdl.read_bytes()==sdl_bytes
        assert backup.read_bytes()==b"UNKNOWN-SDL3"
        assert fix.read_bytes()==fix_bytes
        assert not (d/"game/fix.dll.bak").exists()
        assert receipt.read_bytes()==original_receipt
        assert game_receipt.read_bytes()==original_game_receipt
        execute(e,"--verify",success=True)
        # If a later component fails in the staged transaction, restore the
        # previous fix.dll bytes and the unmodified receipt; no backup.
        d,e=install(base,fixture,"auto_replace_fix_rollback")
        fix=d/"game/fix.dll"; sdl=d/"game/SDL3.dll"
        receipt=d/"state/installed-resources.sha256"
        before_receipt=receipt.read_bytes()
        fix.write_bytes(b"OLD-FIX-MUST-ROLL-BACK")
        sdl.write_bytes(b"OLD-SDL-MUST-ROLL-BACK")
        denied=execute(e,success=False,
                       environment={"SPIDER_UNIFIED_TEST_UPGRADE_FAIL":"1"})
        assert "automatic fix.dll replacement refused" in denied
        assert fix.read_bytes()==b"OLD-FIX-MUST-ROLL-BACK"
        assert sdl.read_bytes()==b"OLD-SDL-MUST-ROLL-BACK"
        assert receipt.read_bytes()==before_receipt
        assert not (d/"game/fix.dll.bak").exists()
        assert not (d/"SpiderMan2k1Fix.updating").exists()
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
