"""Exact-exception fclose(NULL) Win11 crash guard: conservative opt-in."""
from pathlib import Path
S=(Path(__file__).resolve().parents[1]/"spiderman2k1fix.c").read_text()
DOC=(Path(__file__).resolve().parents[1]/"WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()

def test_disabled_unless_exact_env_set_and_verified_game():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD"' in S
    assert "if(n!=1 || env[0]!='1') return;" in S
    assert "memcmp(base+0x12A7FC,signature,sizeof(signature))" in S
    assert S.index("if (!supported_exe())") < S.index("install_null_fclose_crash_test();")

def test_tight_exception_and_caller_filter():
    for p in ["ex->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION",
              "ex->ExceptionInformation[1]!=0xC",
              "(uintptr_t)ctx->Eip!=base+0x12A805",
              "ctx->Esi!=0", "ctx->Edi!=0xFFFFFFFFUL",
              "*(DWORD*)slot!=(DWORD)(base+0x1186F8)",
              "*(DWORD*)(slot+4)!=0", "mb.State!=MEM_COMMIT",
              "PAGE_NOACCESS|PAGE_GUARD"]:
        assert p in S

def test_recovery_is_original_fclose_epilogue_not_manual_stack_edit():
    assert "ctx->Eip=(DWORD)(base+0x12A828)" in S
    assert "return EXCEPTION_CONTINUE_EXECUTION;" in S
    assert "AddVectoredExceptionHandler(1,guarded_null_fclose_veh)" in S
    assert "report_null_fclose_recovery();" in S
    assert "RemoveVectoredExceptionHandler" not in S # process-lifetime opt-in

def test_diagnostic_instructions_and_independent_switches():
    assert "$env:SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD='1'" in DOC
    assert "$env:SPIDERMAN2K1FIX_D3D_INPLACE='1'" in DOC
    assert "fclose(NULL)" in DOC
