/* Win11 Spider-Man 2001 native DirectInput8 polling diagnostics and
 * opt-in repair, for the EXACT SHA-256-validated SpideyPC.exe only.
 *
 * Disassembly of the original EXE:
 *   00501590  GUID_SysKeyboard -> global 006B7A5C
 *   00501710  GUID_SysMouse    -> global 006B7A64
 *   00501B80  keyboard GetDeviceData()
 *   00501CC0  mouse GetDeviceData()
 *
 * Game behavior:
 *   DIERR_INPUTLOST    (0x8007001E) -> Acquire -> retry GetDeviceData
 *   DIERR_NOTACQUIRED  (0x8007000C) -> no Acquire; return with no events
 *
 * In the trace-only mode the existing conditional control-flow is preserved.
 * In the repair mode only DIERR_NOTACQUIRED is additionally redirected into
 * the game's EXISTING Acquire-and-retry block. No COM patching, DirectInput8
 * IAT interception, fake events, window changes, or new Acquire implementation.
 * On any signature mismatch, keep the original EXE unchanged.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "input_callsite_guard.h"

#define SPIDEY_EXE_BASE 0x00400000u
#define DIERR_INPUTLOST_VALUE   0x8007001Eu
#define DIERR_NOTACQUIRED_VALUE 0x8007000Cu
#define KEY_BRANCH_RVA   0x00101BCBu
#define MOUSE_BRANCH_RVA 0x00101D0Au

typedef struct {
  volatile LONG calls;
  volatile LONG notacquired;
  volatile LONG inputlost;
  volatile DWORD last_hr;
  LONG last_reported_calls;
  LONG last_reported_notacquired;
  LONG last_reported_inputlost;
} PollSite;

static PollSite sites[2];
static void (*diag_log)(const char *)=NULL;
static volatile LONG guard_installed=0;
static volatile LONG retry_notacquired=0;
static void *allocated_stubs=NULL;

/* Only the exact 11 original bytes: cmp eax,INPUTLOST; jne +0x80 */
static const BYTE expected_branch[11]={
  0x3D,0x1E,0x00,0x07,0x80, 0x0F,0x85,0x80,0x00,0x00,0x00
};
static const BYTE original_jne[6]={0x0F,0x85,0x80,0x00,0x00,0x00};

static LONG snap(volatile LONG *v) {return InterlockedCompareExchange(v,0,0);}
static DWORD snap_hr(volatile DWORD *v) {
  return (DWORD)InterlockedCompareExchange((volatile LONG *)v,0,0);
}
static void emit_u32(BYTE **p,uint32_t value) {
  memcpy(*p,&value,4); *p+=4;
}
static void set_rel32(BYTE *field,const BYTE *target) {
  uint32_t offset=(uint32_t)((uintptr_t)target-((uintptr_t)field+4));
  memcpy(field,&offset,4);
}
static BYTE *emit_jmp(BYTE **p,const BYTE *target) {
  BYTE *field;
  *(*p)++=0xE9;field=*p;emit_u32(p,0);set_rel32(field,target);
  return field;
}
static BYTE *emit_jcc(BYTE **p,BYTE opcode) {
  BYTE *field;
  *(*p)++=0x0F;*(*p)++=opcode;
  field=*p;emit_u32(p,0);
  return field;
}
static void emit_lock_inc(BYTE **p,volatile LONG *ptr) {
  *(*p)++=0xF0;*(*p)++=0xFF;*(*p)++=0x05;
  emit_u32(p,(uint32_t)(uintptr_t)ptr);
}
static BYTE *build_stub(BYTE *space,PollSite *stats,const BYTE *acquire,
                        const BYTE *continue_normal,int allow_retry) {
  BYTE *p=space,*not_na,*not_lost;
  /* Recording volatile machine state doesn't call back into the DLL, alter
     the stack, or modify any general register. */
  *p++=0xA3; emit_u32(&p,(uint32_t)(uintptr_t)&stats->last_hr);
  emit_lock_inc(&p,&stats->calls);
  *p++=0x3D;emit_u32(&p,DIERR_NOTACQUIRED_VALUE);
  not_na=emit_jcc(&p,0x85); /* JNE to normal INPUTLOST decision */
  emit_lock_inc(&p,&stats->notacquired);
  if(allow_retry) emit_jmp(&p,acquire);
  set_rel32(not_na,p);
  *p++=0x3D;emit_u32(&p,DIERR_INPUTLOST_VALUE);
  not_lost=emit_jcc(&p,0x85);
  emit_lock_inc(&p,&stats->inputlost);
  emit_jmp(&p,acquire);
  set_rel32(not_lost,p);
  emit_jmp(&p,continue_normal);
  return p;
}
static BOOL change_original_six_bytes(BYTE *dst,const BYTE *src) {
  DWORD previous,ignored;
  if(!VirtualProtect(dst,6,PAGE_EXECUTE_READWRITE,&previous)) return FALSE;
  memcpy(dst,src,6);
  FlushInstructionCache(GetCurrentProcess(),dst,6);
  VirtualProtect(dst,6,previous,&ignored);
  return TRUE;
}
static int opt_in(const char *name) {
  char value[4]={0};
  DWORD n=GetEnvironmentVariableA(name,value,sizeof(value));
  return n==1 && value[0]=='1';
}
void SpiderDI8Callsite_Init(void (*log_fn)(const char *)) {
  BYTE *exe,*page,*keyboard_stub,*mouse_stub,*kb_branch,*ms_branch;
  BYTE *kb_end,*ms_end;
  BYTE jump[6];
  BOOL kb_installed=FALSE;
  DWORD protect,ignored;
  int trace=opt_in("SPIDERMAN2K1FIX_DI8_POLL_TRACE");
  int repair=opt_in("SPIDERMAN2K1FIX_DI8_NOTACQUIRED_RETRY");
  diag_log=log_fn;
  if(!trace && !repair) return;
  /* Caller already validated exact game SHA-256, but recheck image address
     and the precise original instructions before patching either location. */
  exe=(BYTE*)GetModuleHandleA(NULL);
  if(!exe || (uintptr_t)exe!=SPIDEY_EXE_BASE || sizeof(void*)!=4 ||
     memcmp(exe+KEY_BRANCH_RVA-5,expected_branch,sizeof(expected_branch))!=0 ||
     memcmp(exe+MOUSE_BRANCH_RVA-5,expected_branch,sizeof(expected_branch))!=0) {
    if(diag_log) diag_log("INPUT-CALLSITE: ERROR original EXE instructions mismatch; unchanged");
    return;
  }
  page=(BYTE*)VirtualAlloc(NULL,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
  if(!page) {
    if(diag_log) diag_log("INPUT-CALLSITE: ERROR stub allocation failed; unchanged");
    return;
  }
  kb_branch=exe+KEY_BRANCH_RVA;
  ms_branch=exe+MOUSE_BRANCH_RVA;
  keyboard_stub=page;
  kb_end=build_stub(keyboard_stub,&sites[0],exe+0x101BD1u,exe+0x101C51u,repair);
  mouse_stub=kb_end+16;
  ms_end=build_stub(mouse_stub,&sites[1],exe+0x101D10u,exe+0x101D90u,repair);
  if(ms_end>page+4096 ||
     !VirtualProtect(page,4096,PAGE_EXECUTE_READ,&protect)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(diag_log) diag_log("INPUT-CALLSITE: ERROR executable stub protection failed; unchanged");
    return;
  }
  FlushInstructionCache(GetCurrentProcess(),page,(SIZE_T)(ms_end-page));
  memcpy(jump,original_jne,6);
  jump[0]=0xE9;
  set_rel32(jump+1,keyboard_stub);
  jump[5]=0x90;
  if(!change_original_six_bytes(kb_branch,jump)) {
    VirtualFree(page,0,MEM_RELEASE);
    if(diag_log) diag_log("INPUT-CALLSITE: ERROR keyboard branch VirtualProtect failed; unchanged");
    return;
  }
  kb_installed=TRUE;
  set_rel32(jump+1,mouse_stub);
  if(!change_original_six_bytes(ms_branch,jump)) {
    if(kb_installed && change_original_six_bytes(kb_branch,original_jne))
      VirtualFree(page,0,MEM_RELEASE);
    /* In the highly unlikely event rollback fails, retain executable stub
       so the keyboard branch can never jump into deallocated memory. */
    if(diag_log) diag_log("INPUT-CALLSITE: ERROR mouse branch patch failed; keyboard rollback attempted");
    return;
  }
  /* Runtime lifetime is process lifetime: never free executable stubs while
     game's polling code can reach them. */
  allocated_stubs=page;
  (void)ignored;
  InterlockedExchange(&retry_notacquired,repair?1:0);
  InterlockedExchange(&guard_installed,1);
  if(diag_log) diag_log(repair?
    "INPUT-CALLSITE: TRACE+EXPERIMENTAL_RETRY active; keyboard/mouse DIERR_NOTACQUIRED uses original game Acquire/retry":
    "INPUT-CALLSITE: TRACE ONLY active; all original keyboard/mouse return paths preserved");
}
void SpiderDI8Callsite_Report(long flip_index) {
  unsigned i;
  if(snap(&guard_installed)!=1 || !diag_log) return;
  for(i=0;i<2;i++) {
    PollSite *s=&sites[i];
    LONG n=snap(&s->calls),na=snap(&s->notacquired),
         lost=snap(&s->inputlost);
    if(n==s->last_reported_calls) continue;
    if(flip_index>3 && (flip_index%120)!=0 &&
       na==s->last_reported_notacquired &&
       lost==s->last_reported_inputlost) continue;
    {
      char line[230];
      snprintf(line,sizeof(line),
        "INPUT-CALLSITE: %s GetDeviceData samples=%ld notacquired=%ld inputlost=%ld last-hr=0x%08lX retry-notacquired=%ld",
        i==0?"keyboard":"mouse",(long)n,(long)na,(long)lost,
        (unsigned long)snap_hr(&s->last_hr),(long)snap(&retry_notacquired));
      diag_log(line);
    }
    s->last_reported_calls=n;
    s->last_reported_notacquired=na;
    s->last_reported_inputlost=lost;
  }
}
