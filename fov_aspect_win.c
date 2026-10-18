#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "fov_aspect.h"
#include "fov_aspect_win.h"

static int active=0;
static int horplus_mode=0;
static volatile LONG horplus_stage=0;
static volatile LONG gameplay_scalar_ready=0;
static float horplus_value=1.f;
static SpiderFOVSetting requested=SPIDER_FOV_AUTO;
static double manual_degrees=0;
static void (*log_fn)(const char *);
static const DWORD scalar_rva=0x00150064;
static DWORD original_bits=0x3F800000;
static DWORD last_bits=0x3F800000;

static void say(const char *s) {if(log_fn)log_fn(s);}
static int update_scalar(float new_value) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  DWORD old,unused,bits;
  if(!base || (uintptr_t)base!=0x400000) return 0;
  if(*(WORD*)base!=IMAGE_DOS_SIGNATURE) return 0;
  {
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)base;
    IMAGE_NT_HEADERS32 *nt;
    if(dos->e_lfanew<0 || dos->e_lfanew>0x1000) return 0;
    nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
       nt->OptionalHeader.SizeOfImage<scalar_rva+sizeof(bits))return 0;
  }
  memcpy(&bits,&new_value,sizeof(bits));
  if(memcmp(base+scalar_rva,&last_bits,4)!=0) return 0;
  if(!VirtualProtect(base+scalar_rva,4,PAGE_READWRITE,&old)) return 0;
  memcpy(base+scalar_rva,&bits,4);
  VirtualProtect(base+scalar_rva,4,old,&unused);
  last_bits=bits;
  return 1;
}

void SpiderFOV_Init(const char *setting,int experiment,int horplus,void (*logger)(const char *)) {
  char line[200];
  log_fn=logger;
  requested=SpiderFOV_Parse(setting,&manual_degrees);
  horplus_mode=(horplus==1 && requested==SPIDER_FOV_AUTO);
  active=(experiment==1 || horplus_mode) && requested==SPIDER_FOV_AUTO;
  InterlockedExchange(&horplus_stage,0);
  InterlockedExchange(&gameplay_scalar_ready,0);
  horplus_value=1.f;
  if(requested==SPIDER_FOV_INVALID) {
    say("FOV: invalid setting; original projection retained (valid: Auto, 10..170 degrees)");
  } else if(requested==SPIDER_FOV_DEGREES) {
    snprintf(line,sizeof(line),"FOV: %.4g degrees requested; manual FOV NOT APPLIED (game camera baseline not established)",manual_degrees);
    say(line);
  } else if(horplus_mode) {
    say("FOV-HOR+: experiment enabled; gameplay scalar plus Y-only geometry compensation (depth-test guarded) [NOT DEGREES]");
  } else if(active) {
    say("FOV: Auto aspect experiment enabled; 4:3-relative scalar will change in GAMEPLAY only [NOT DEGREES]");
  } else {
    say("FOV: Auto selected; camera unchanged (ExperimentalFOVAspect=0)");
  }
}

void SpiderFOV_SetStage(int gameplay,unsigned screen_w,unsigned screen_h) {
  float value=1.0f;
  char msg[180];
  if(!active) return;
  InterlockedExchange(&horplus_stage,0);
  InterlockedExchange(&gameplay_scalar_ready,0);
  if(gameplay && !SpiderFOV_AspectScalar(screen_w,screen_h,&value)) {
    say("FOV: Auto aspect SKIPPED (unsupported display geometry)");
    return;
  }
  if(!update_scalar(value)) {
    say("FOV: Auto aspect SKIPPED (scalar address/signature/protection mismatch)");
    active=0;
    horplus_mode=0;
    return;
  }
  horplus_value=value;
  if(gameplay) InterlockedExchange(&gameplay_scalar_ready,1);
  if(gameplay && horplus_mode) InterlockedExchange(&horplus_stage,1);
  snprintf(msg,sizeof(msg),"FOV: aspect scalar=%.6g stage=%s target=%ux%u [EXPERIMENTAL, not measured degrees]",
           (double)value,gameplay?"GAMEPLAY":"MENU",screen_w,screen_h);
  say(msg);
}

int SpiderFOV_GetHorPlusScalar(float *out) {
  if(!out || !horplus_mode ||
     InterlockedCompareExchange(&horplus_stage,0,0)!=1) return 0;
  *out=horplus_value;
  return *out>=0.3f && *out<1.f;
}

void SpiderFOV_DisableUnsafeHorPlus(void) {
  if(!horplus_mode) return;
  /* Only invoked during DLL initialization before game rendering begins. */
  active=0;
  horplus_mode=0;
  InterlockedExchange(&horplus_stage,0);
  say("FOV-HOR+: DISABLED safely: HUD scope tracking unavailable; original camera retained");
}

int SpiderFOV_GameplayReady(void) {
  return active && InterlockedCompareExchange(&gameplay_scalar_ready,0,0)==1;
}