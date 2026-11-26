#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "fov_aspect.h"
#include "fov_aspect_win.h"
#include "wide_culling.h"

static int active=0;
static int horplus_mode=0;
static volatile LONG horplus_stage=0;
static volatile LONG gameplay_scalar_ready=0;
static float horplus_value=1.f;
static SpiderFOVSetting requested=SPIDER_FOV_AUTO;
static double manual_degrees=0;
static void (*log_fn)(const char *);
static const DWORD scalar_rva=0x00150064;
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

/* Issue #181 trial. Exact SHA-pinned PC source: 0x4730B2 constructs
 * the positive horizontal normal, 0x4730C6 the negative counterpart.
 * The loop at 0x4731E5..0x473235 rotates six camera-space planes;
 * its second native vector-load CALL takes 0x65CED0 (vertical),
 * 0x65CED8 (horizontal +X), 0x65CEE0 (horizontal -X).
 * Replace only those two arguments with temporary normalized vectors.
 * Native storage and all subsequent transforms/tests remain original. */
typedef void (__cdecl *SpiderCullLoadVector)(const int16_t *);
static SpiderCullLoadVector cull_original_load=NULL;
static unsigned cull_logged=0,cull_rejected_logged=0;
static void __cdecl cull_load_vector(const int16_t *src) {
  int16_t mapped[3];
  uintptr_t address=(uintptr_t)src;
  if(SpiderFOV_GameplayReady() && horplus_value>=0.3f && horplus_value<1.f &&
     (address==0x0065CED8u || address==0x0065CEE0u)){
    const int16_t *positive=(const int16_t *)(uintptr_t)0x0065CED8u;
    const int16_t *negative=(const int16_t *)(uintptr_t)0x0065CEE0u;
    if(SpiderCull_WidenHorizontalPair(positive,negative,horplus_value,
                                   address==0x0065CEE0u,mapped)){
      if(cull_logged<8u){
        char line[180];
        ++cull_logged;
        snprintf(line,sizeof(line),
          "WIDE-CULL: horizontal normal at %08lX scalar=%.5f (%d,%d,%d)->(%d,%d,%d)",
          (unsigned long)address,(double)horplus_value,(int)src[0],(int)src[1],(int)src[2],
          (int)mapped[0],(int)mapped[1],(int)mapped[2]);
        say(line);
      }
      cull_original_load(mapped);
      return;
    }
    if(!cull_rejected_logged){
      char line[180];
      cull_rejected_logged=1;
      snprintf(line,sizeof(line),
        "WIDE-CULL: unexpected horizontal pair; original preserved +(%d,%d,%d) -(%d,%d,%d)",
        (int)positive[0],(int)positive[1],(int)positive[2],
        (int)negative[0],(int)negative[1],(int)negative[2]);
      say(line);
    }
  }
  cull_original_load(src);
}

int SpiderFOV_InstallWideCullingTrial(void) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL),*site;
  DWORD old,restored,rel;
  BYTE replacement[5];
  /* All addresses below belong to the already authenticated retail EXE.
   * Check exact original construction, call context and callee bytes. */
  static const BYTE construction[]={
    0x66,0xa3,0xd8,0xce,0x65,0x00,0x8b,0xc7,0xf7,0xf1,
    0x8b,0x15,0xd8,0xce,0x65,0x00,0x33,0xff,0xf7,0xda,
    0x66,0x89,0x15,0xe0,0xce,0x65,0x00,0x66,0xa3,0xdc,0xce,0x65,0x00,
    0x66,0xa3,0xe4,0xce,0x65,0x00
  };
  static const BYTE context[]={
    0x55,0xe8,0x97,0xa6,0xff,0xff,0xe8,0xe2,0xab,0xff,0xff,
    0x8b,0x54,0x24,0x20,0x8d,0x82,0x20,0x86,0x62,0x00,0x50
  };
  static const BYTE loop[]={
    0x8b,0x4c,0x24,0x24,0x83,0xc5,0x08,0x83,0xc4,0x10,
    0x83,0xc1,0x06,0x81,0xfd,0xe8,0xce,0x65,0x00,
    0x89,0x4c,0x24,0x14,0x7c,0xae
  };
  static const BYTE load[]={
    0x8b,0x44,0x24,0x04,0x0f,0xbf,0x08,0x89,0x0d,0xb0,0x0b,0x61,0x00,
    0x0f,0xbf,0x50,0x02,0x89,0x15,0xb4,0x0b,0x61,0x00,
    0x0f,0xbf,0x40,0x04,0xa3,0xb8,0x0b,0x61,0x00,0xc3
  };
  static const BYTE consume[]={0x68,0x20,0x86,0x62,0x00,0xe8,0x40,0x9e,0xff,0xff};
  if(cull_original_load)return 1;
  if(!active || (uintptr_t)base!=0x400000u ||
     memcmp(base+0x730B2,construction,sizeof(construction))||
     memcmp(base+0x73203,context,sizeof(context))||
     memcmp(base+0x7321E,loop,sizeof(loop))||
     memcmp(base+0x6D8A0,load,sizeof(load))||
     memcmp(base+0x739C6,consume,sizeof(consume))){
    say("WIDE-CULL: trial refused; original signature mismatch");return 0;
  }
  site=base+0x73204;
  /* x86 rel32 arithmetic is modulo 2^32; all process addresses are 32-bit. */
  rel=(DWORD)((uintptr_t)cull_load_vector-(uintptr_t)(site+5));
  replacement[0]=0xe8;memcpy(replacement+1,&rel,sizeof(rel));
  if(!VirtualProtect(site,sizeof(replacement),PAGE_EXECUTE_READWRITE,&old)){
    say("WIDE-CULL: trial refused; cannot protect original CALL");return 0;
  }
  cull_original_load=(SpiderCullLoadVector)(uintptr_t)(base+0x6D8A0);
  memcpy(site,replacement,sizeof(replacement));
  FlushInstructionCache(GetCurrentProcess(),site,sizeof(replacement));
  if(!VirtualProtect(site,sizeof(replacement),old,&restored))
    say("WIDE-CULL: warning: original code-page protection restoration failed");
  say("WIDE-CULL: opt-in horizontal visibility-plane trial installed; no distance or vertical change");
  return 1;
}
