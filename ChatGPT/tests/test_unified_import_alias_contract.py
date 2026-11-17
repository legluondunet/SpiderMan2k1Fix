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
                  '"DirectDrawCreateEx"','"DirectDrawEnumerateExA"',
                  'found!=1','matched!=3u',
                  'ReadProcessMemory','WriteProcessMemory','VirtualProtectEx',
                  'FlushInstructionCache','return remote_write_verified('):
        assert token in source or token in (ROOT/"installer/unified_import_alias.h").read_text()
    assert "SPIDER_RETAIL_SIZE_OF_IMAGE 0x02A0D000u" in source
    assert "SPIDER_DDRAW_ENUM_EXA_IAT_RVA 0x0013b014u" in source
    assert "nt->OptionalHeader.SizeOfImage!=SPIDER_RETAIL_SIZE_OF_IMAGE" in source
    assert 'strcmp(symbol,"DirectDrawEnumerateExA")==0' in source
    assert "matched|=bit;" in source
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


def test_primary_null_stop_guard_matches_original_machine_code_and_fails_closed():
    source=(ROOT/"installer/unified_import_alias.c").read_text()
    header=(ROOT/"installer/unified_import_alias.h").read_text()
    installer=(ROOT/"installer/spiderman2k1fix_installer.c").read_text()
    for fragment in (
        "SpiderUnified_PatchSuspendedPrimaryNullStop(HANDLE process)",
        "0xA1,0x1C,0xBF,0x6B,0x00,0x56,0x50,0x8B",
        "0x08,0xFF,0x51,0x48,0x8B,0xF0,0x85,0xF6",
        "0x6A,0x01,0x6A,0x00,0xE8,0x23,0x05,0x00,0x00",
        "0x8B,0x44,0x24,0x08,0x53,0x56,0x57,0x85,0xC0,0x74,0x4E",
        "0x85,0xC0", "0x74,0x07", "0x8B,0x08", "0xFF,0x51,0x48",
        "0xEB,0xBF", "0x58", "0xEB,0xE4",
        "0xE9,0x36,0x00,0x00,0x00",
        "0x00103AF7u", "0x00103B32u", "0x00103B24u",
        "0x00104050u", "memcmp(unused,nops,sizeof(nops))",
        "remote_write_verified(process,cave,nops,cave_program,sizeof(nops))",
        "remote_write_verified(process,site,original+7,branch,sizeof(branch))",
    ):
        assert fragment in source, fragment
    assert "SpiderUnified_PatchSuspendedPrimaryNullStop(HANDLE process);" in header
    assert "SpiderUnified_PatchSuspendedPrimaryNullStop(pi.hProcess)" in installer
    assert "DDSCL_EXCLUSIVE" not in source
    # Do not manipulate the cooperative level in code (comments may name it).
    assert "original_coop(" not in source
    assert "DDSCL_NORMAL" not in source
