"""Narrow fclose(NULL) exception recovery defaults to enabled for verified EXE."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
S = (ROOT / "spiderman2k1fix.c").read_text()
DOC = (ROOT / "docs/references/diagnostics/graphics/WINDOWS_VIEWPORT_DIAGNOSTIC.md").read_text()


def test_default_guard_with_diagnostic_opt_out_and_verified_game():
    assert 'GetEnvironmentVariableA("SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD"' in S
    assert "if(n==1 && env[0]=='0')" in S
    assert 'FCLOSE-GUARD: disabled by diagnostic override' in S
    assert "memcmp(base+0x12A7FC,signature,sizeof(signature))" in S
    assert S.index("if (!supported_exe())") < S.index("install_null_fclose_guard();")


def test_tight_exception_and_caller_filter():
    for p in [
        "ex->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION",
        "ex->ExceptionInformation[1]!=0xC",
        "(uintptr_t)ctx->Eip!=base+0x12A805",
        "ctx->Esi!=0", "ctx->Edi!=0xFFFFFFFFUL",
        "*(DWORD*)slot!=(DWORD)(base+0x1186F8)",
        "*(DWORD*)(slot+4)!=0", "mb.State!=MEM_COMMIT",
        "PAGE_NOACCESS|PAGE_GUARD",
    ]:
        assert p in S


def test_recovery_uses_original_fclose_epilogue():
    assert "ctx->Eip=(DWORD)(base+0x12A828)" in S
    assert "return EXCEPTION_CONTINUE_EXECUTION;" in S
    assert "AddVectoredExceptionHandler(1,guarded_null_fclose_veh)" in S
    assert "report_null_fclose_recovery();" in S
    assert "RemoveVectoredExceptionHandler" not in S  # process-lifetime guard


def test_default_native_vtable_and_diagnostic_instructions():
    assert "SPIDERMAN2K1FIX_D3D_INPLACE" not in S
    assert "vt[13]=(void*)hooked_set_viewport;" in S
    assert "vt[20]=(void*)hooked_set_render_state;" in S
    assert "vt[25]=(void*)hooked_draw_primitive;" in S
    assert "vt[26]=(void*)hooked_draw_indexed_primitive;" in S
    assert "if(device_ready==2 && vt[13]==(void*)original_set_viewport" in S
    assert "*obj=patched_device_vtable;" not in S
    assert "SPIDERMAN2K1FIX_FCLOSE_NULL_GUARD=0" in DOC
    assert "no PowerShell environment variables" in DOC
