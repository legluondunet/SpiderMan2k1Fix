"""Source contracts for the suspended-process PE32 bootstrap.

These checks are static; Windows/Wine synthetic child tests are required
before enabling this code in the launcher.
"""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]

def test_alias_requires_unique_original_import_descriptor_and_exact_thunk():
    source=(ROOT/"installer/unified_import_alias.c").read_text()
    for token in ('CREATE_SUSPENDED','IMAGE_DOS_SIGNATURE','IMAGE_NT_SIGNATURE',
                  'IMAGE_NT_OPTIONAL_HDR32_MAGIC','IMAGE_DIRECTORY_ENTRY_IMPORT',
                  'SPIDER_DDRAW_CREATE_IAT_RVA 0x0013b010u',
                  '_stricmp(name,"ddraw.dll")',
                  '"DirectDrawCreateEx"','found!=1','matched!=1',
                  'ReadProcessMemory','WriteProcessMemory','VirtualProtectEx',
                  'FlushInstructionCache','return remote_write_verified('):
        assert token in source or token in (ROOT/"installer/unified_import_alias.h").read_text()
    assert "0x02000000u" in source  # bounded image size
    assert "ResumeThread" not in source  # only the verified parent may resume
    assert "CreateFile" not in source  # never write the original EXE

def test_original_early_nocd_bytes_remain_signature_gated():
    source=(ROOT/"installer/unified_import_alias.c").read_text()
    assert "0x001163E0u" in source
    assert "0x001159ACu" in source
    assert "0x00115D85u" in source
    assert "0x31,0xC0,0xC3" in source
    assert "memcmp(call1,expected1,sizeof(call1))" in source
    assert "memcmp(call2,expected2,sizeof(call2))" in source
    assert "PAGE_EXECUTE_READWRITE" in source
    assert "VirtualProtectEx(process,(LPVOID)addr,size,previous,&ignored)" in source
