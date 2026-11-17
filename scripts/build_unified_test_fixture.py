#!/usr/bin/env python3
"""Create synthetic-only unified installer source. Never commit fixture EXEs."""
from pathlib import Path
import hashlib
source=Path("installer/spiderman2k1fix_installer.c")
output=Path("installer/spiderman2k1fix_installer.fixture.c")
text=source.read_text(encoding="utf-8")
retail="0a11a49f3f63d4bb15650f322d654553fc47234b01885b852a058ff074689dea"
fake=b"MZ"+b"A"*(1_507_328-2)
assert text.count(retail)==1
text=text.replace(retail,hashlib.sha256(fake).hexdigest())
site='''       !combine(dst,SPIDER_INSTALLER_PATH,launcher_dir,publish[i]) ||
       GetFileAttributesW(dst)!=INVALID_FILE_ATTRIBUTES ||
       !MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)) {'''
assert text.count(site)==1, "unified publication loop must be unique"
text=text.replace(site,'''       !combine(dst,SPIDER_INSTALLER_PATH,launcher_dir,publish[i]) ||
       GetFileAttributesW(dst)!=INVALID_FILE_ATTRIBUTES ||
       (i==2u && GetEnvironmentVariableW(L"SPIDER_UNIFIED_TEST_FAIL",NULL,0)>0) ||
       !MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)) {''')
# Induce failure in the upgrade loop AFTER fix.dll has been replaced,
# but BEFORE the next changed resource can be committed. Test-only path.
site2='''    if(!MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)){
      message(L"UPGRADE: cannot publish %ls (Win32=%lu)",'''
assert text.count(site2)==1
text=text.replace(site2,'''    if(i==1u && GetEnvironmentVariableW(
        L"SPIDER_UNIFIED_TEST_UPGRADE_FAIL",NULL,0)>0)break;
    if(!MoveFileExW(src,dst,MOVEFILE_WRITE_THROUGH)){
      message(L"UPGRADE: cannot publish %ls (Win32=%lu)",''')
output.write_text(text,encoding="utf-8")
print("SYNTHETIC ONLY fixture prepared; no original game bytes or files.")
