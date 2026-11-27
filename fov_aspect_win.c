#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "fov_aspect.h"
#include "fov_aspect_win.h"
#include "wide_culling.h"
#include "world_quad_projection.h"

static int active=0;
static int horplus_mode=0;
static volatile LONG horplus_stage=0;
static volatile LONG gameplay_scalar_ready=0;
static float horplus_value=1.f;
static unsigned world_quad_gameplay_width=0;
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
  world_quad_gameplay_width=0;
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
  world_quad_gameplay_width=0;
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
  if(gameplay) {
    world_quad_gameplay_width=screen_w;
    InterlockedExchange(&gameplay_scalar_ready,1);
  }
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

/* Issue #181. Exact SHA-pinned PC source: 0x4730B2 constructs
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

int SpiderFOV_InstallWideCulling(void) {
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
    say("WIDE-CULL: correction refused; original signature mismatch");return 0;
  }
  site=base+0x73204;
  /* x86 rel32 arithmetic is modulo 2^32; all process addresses are 32-bit. */
  rel=(DWORD)((uintptr_t)cull_load_vector-(uintptr_t)(site+5));
  replacement[0]=0xe8;memcpy(replacement+1,&rel,sizeof(rel));
  if(!VirtualProtect(site,sizeof(replacement),PAGE_EXECUTE_READWRITE,&old)){
    say("WIDE-CULL: correction refused; cannot protect original CALL");return 0;
  }
  cull_original_load=(SpiderCullLoadVector)(uintptr_t)(base+0x6D8A0);
  memcpy(site,replacement,sizeof(replacement));
  FlushInstructionCache(GetCurrentProcess(),site,sizeof(replacement));
  if(!VirtualProtect(site,sizeof(replacement),old,&restored))
    say("WIDE-CULL: warning: original code-page protection restoration failed");
  say("WIDE-CULL: horizontal visibility-plane correction installed; no distance or vertical change");
  return 1;
}


/* #147: two exact native world-quad submissions from 0x4097E0.
 * 0x46DBC0 constructs integer screen X with focal length and half
 * width 0x54F040; no read of 0x550064. The caller converts to pixels
 * by 0x568154 / 0x61B5FC (40A021,40A221).
 * 0x508550 receives four consecutive {x,y,depth,u,v,color} records
 * as 24 stack DWORDs. Its original clipping and rendering stay intact.
 * The wrapper changes only four X arguments. Bitwise forwarding of
 * every other argument, no float conversion of colors/depth/UV. */
typedef void (__cdecl *SpiderQuadSubmit)(
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,
    uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static SpiderQuadSubmit const quad_original=(SpiderQuadSubmit)(uintptr_t)0x00508550u;
/* 0x409F48 reads quad+60; 0x409F51 reads its +2 word;
 * 0x409F8C passes that index plus the native mode to 0x506440.
 * The next instructions overwrite EAX and ECX. Preserve both args
 * and original return; only observe after the original selector. */
typedef DWORD (__cdecl *SpiderNativeTextureSelector)(DWORD,DWORD);
static SpiderNativeTextureSelector const shadow_selector_original=(SpiderNativeTextureSelector)(uintptr_t)0x00506440u;
static void (*shadow_selector_observer)(unsigned)=NULL;
void SpiderFOV_SetShadowSelectorObserver(void (*observer)(unsigned)) {shadow_selector_observer=observer;}
static DWORD __cdecl shadow_selector(DWORD index,DWORD mode) {
  DWORD result=shadow_selector_original(index,mode);
  if(shadow_selector_observer)shadow_selector_observer(index);
  return result;
}
int SpiderFOV_InstallShadowSelector(void) {
  static int installed;
  static const BYTE source[]={0x8b,0x46,0x60,0x85,0xc0,0x74,0x08,0x33,0xd2,0x66,0x8b,0x50,0x02};
  static const BYTE call[]={0x50,0x52,0xe8,0xaf,0xc4,0x0f,0x00,0x33,0xc0,0x8b,0x0d,0x14,0x86,0x62,0x00};
  BYTE *base=(BYTE*)GetModuleHandleA(NULL),*site;
  BYTE replacement[5]={0xe8,0,0,0,0};
  DWORD rel,old,restored;
  if(installed)return 1;
  if(!shadow_selector_observer || (uintptr_t)base!=0x400000u ||
     memcmp(base+0x9f48,source,sizeof(source)) ||
     memcmp(base+0x9f8a,call,sizeof(call))){
    say("SHADOW-HD: selector refused; native signature mismatch");return 0;
  }
  site=base+0x9f8c;
  rel=(DWORD)((uintptr_t)shadow_selector-(uintptr_t)(site+5));
  memcpy(replacement+1,&rel,4);
  if(!VirtualProtect(site,5,PAGE_EXECUTE_READWRITE,&old))return 0;
  memcpy(site,replacement,5);FlushInstructionCache(GetCurrentProcess(),site,5);
  if(!VirtualProtect(site,5,old,&restored))say("SHADOW-HD: selector page protection restore warning");
  installed=1;say("SHADOW-HD: native texture selector observation installed");
  return 1;
}
/* Both original queue builders pass the selected index on the stack
 * and store the converter's EAX return at record+4. The following
 * instructions overwrite ECX/EDX. Keep the original CALL/return ABI. */
typedef DWORD (__cdecl *SpiderNativeTextureResolve)(DWORD);
static SpiderNativeTextureResolve const shadow_resolve_original=(SpiderNativeTextureResolve)(uintptr_t)0x0050F3C0u;
static void (*shadow_resolve_observer)(unsigned,DWORD)=NULL;
void SpiderFOV_SetShadowResolveObserver(void (*observer)(unsigned,DWORD)) {
  shadow_resolve_observer=observer;
}
static DWORD __cdecl shadow_resolve(DWORD index) {
  DWORD result=shadow_resolve_original(index);
  if(shadow_resolve_observer)shadow_resolve_observer(index,result);
  return result;
}
int SpiderFOV_InstallShadowResolver(void) {
  static int installed;
  static const BYTE first[]={0x50,0xe8,0xec,0x7b,0x00,0x00,0x83,0xc4,0x04,0x89,0x45,0x04,0x66,0x8b,0x0d,0x20,0xc7,0x71,0x00};
  static const BYTE second[]={0x50,0xe8,0x3b,0x65,0x00,0x00,0x83,0xc4,0x04,0x89,0x43,0x04,0x66,0x8b,0x0d,0x20,0xc7,0x71,0x00};
  BYTE *base=(BYTE*)GetModuleHandleA(NULL),*sites[2];
  BYTE replacement[2][5]={{0xe8,0,0,0,0},{0xe8,0,0,0,0}};
  DWORD old[2],restored,rel;
  unsigned i;
  if(installed)return 1;
  if(!shadow_resolve_observer || (uintptr_t)base!=0x400000u ||
     memcmp(base+0x1077ce,first,sizeof(first)) ||
     memcmp(base+0x108e7f,second,sizeof(second))){
    say("SHADOW-HD: resolver refused; native signature mismatch");return 0;
  }
  sites[0]=base+0x1077cf;sites[1]=base+0x108e80;
  for(i=0;i<2;i++){
    rel=(DWORD)((uintptr_t)shadow_resolve-(uintptr_t)(sites[i]+5));
    memcpy(replacement[i]+1,&rel,4);
  }
  if(!VirtualProtect(sites[0],5,PAGE_EXECUTE_READWRITE,&old[0]))return 0;
  if(!VirtualProtect(sites[1],5,PAGE_EXECUTE_READWRITE,&old[1])){
    VirtualProtect(sites[0],5,old[0],&restored);return 0;
  }
  for(i=0;i<2;i++){
    memcpy(sites[i],replacement[i],5);
    FlushInstructionCache(GetCurrentProcess(),sites[i],5);
    if(!VirtualProtect(sites[i],5,old[i],&restored))
      say("SHADOW-HD: resolver page protection restore warning");
  }
  installed=1;say("SHADOW-HD: both native index-to-surface CALLs installed");
  return 1;
}
static unsigned quad_logged,quad_refused;
static void (*world_quad_observer)(int)=NULL;
void SpiderFOV_SetWorldQuadObserver(void (*observer)(int)) { world_quad_observer=observer; }
static void __cdecl quad_submit(
    uint32_t x0,uint32_t y0,uint32_t z0,uint32_t u0,uint32_t v0,uint32_t c0,
    uint32_t x1,uint32_t y1,uint32_t z1,uint32_t u1,uint32_t v1,uint32_t c1,
    uint32_t x2,uint32_t y2,uint32_t z2,uint32_t u2,uint32_t v2,uint32_t c2,
    uint32_t x3,uint32_t y3,uint32_t z3,uint32_t u3,uint32_t v3,uint32_t c3) {
  if(SpiderFOV_GameplayReady() && horplus_value>=0.3f && horplus_value<1.f &&
     *(const volatile DWORD *)(uintptr_t)0x006151F8u==0u &&
     *(const volatile DWORD *)(uintptr_t)0x00682950u==0u){
    unsigned width=*(const volatile DWORD *)(uintptr_t)0x00568154u;
    unsigned logical=*(const volatile DWORD *)(uintptr_t)0x0061B5FCu;
    unsigned native_center_width=*(const volatile DWORD *)(uintptr_t)0x0054F040u;
    uint32_t raw[4]={x0,x1,x2,x3};
    float source[4],mapped[4];
    memcpy(source,raw,sizeof(source));
    if(width==world_quad_gameplay_width && logical==512u &&
       native_center_width==logical &&
       SpiderWorldQuad_MapX(source,width,horplus_value,mapped)){
      memcpy(raw,mapped,sizeof(raw));
      x0=raw[0];x1=raw[1];x2=raw[2];x3=raw[3];
      if(quad_logged<8u){
        char line[256];
        ++quad_logged;
        snprintf(line,sizeof(line),
          "WORLD-QUAD-FOV: width=%u scalar=%.5f x=(%.2f,%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f,%.2f)",
          width,(double)horplus_value,
          (double)source[0],(double)source[1],(double)source[2],(double)source[3],
          (double)mapped[0],(double)mapped[1],(double)mapped[2],(double)mapped[3]);
        say(line);
      }
    }else if(!quad_refused){
      char line[180];
      quad_refused=1;
      snprintf(line,sizeof(line),
        "WORLD-QUAD-FOV: original preserved; width=%u expected=%u logical=%u center-width=%u or invalid quad",
        width,world_quad_gameplay_width,logical,native_center_width);
      say(line);
    }
  }
  if(world_quad_observer)world_quad_observer(0);
  quad_original(x0,y0,z0,u0,v0,c0,x1,y1,z1,u1,v1,c1,
                x2,y2,z2,u2,v2,c2,x3,y3,z3,u3,v3,c3);
  if(world_quad_observer)world_quad_observer(1);
}

int SpiderFOV_InstallWorldQuadTrial(void) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  BYTE *first,*second;
  BYTE replacement[2][5]={{0xe8,0,0,0,0},{0xe8,0,0,0,0}};
  DWORD old,restored,rel;
  static int installed=0;
  static const BYTE projection1[]={
    0xdb,0x05,0x54,0x81,0x56,0x00,0xd9,0x5c,0x24,0x14,
    0xdf,0xac,0x24,0xa8,0x00,0x00,0x00,0xd9,0x5c,0x24,0x1c,
    0xd9,0x44,0x24,0x14,0xd8,0x74,0x24,0x1c
  };
  static const BYTE projection2[]={
    0xdb,0x05,0x54,0x81,0x56,0x00,0x0f,0xbf,0x50,0x0a,
    0xd9,0x5c,0x24,0x14,0xdf,0xac,0x24,0x94,0x00,0x00,0x00,
    0x68,0xa4,0x70,0x7d,0x3f,0x68,0xa4,0x70,0x7d,0x3f,0x53,
    0xd9,0x5c,0x24,0x38,0xd9,0x44,0x24,0x20,0xd8,0x74,0x24,0x38
  };
  static const BYTE submit1[]={0x50,0x51,0x52,0xe8,0xa2,0xe3,0x0f,0x00,
    0xdb,0x05,0x58,0x81,0x56,0x00,0xa1,0x14,0x86,0x62,0x00,0x83,0xc4,0x68};
  static const BYTE submit2[]={0x50,0x51,0x52,0xe8,0xe4,0xe1,0x0f,0x00,
    0x8b,0x76,0x08,0x83,0xc4,0x60,0x85,0xf6};
  static const BYTE callee[]={0x81,0xec,0x40,0x01,0x00,0x00,0x53,0x55,0x56,0x57,
    0x8b,0xac,0x24,0x6c,0x01,0x00,0x00,0x8b,0x84,0x24,0x5c,0x01,0x00,0x00};
  static const BYTE fixed[]={0x8b,0x3d,0x3c,0xf0,0x54,0x00,0x8b,0xc7,0x0f,0xaf,0xc2,
    0x99,0xf7,0xfe,0x8b,0xd8,0xa1,0x40,0xf0,0x54,0x00,0x99,0x2b,0xc2,0xd1,0xf8,0x03,0xd8};
  if(installed)return 1;
  if(!active || (uintptr_t)base!=0x400000u ||
     memcmp(base+0xa021,projection1,sizeof(projection1)) ||
     memcmp(base+0xa221,projection2,sizeof(projection2)) ||
     memcmp(base+0xa1a6,submit1,sizeof(submit1)) ||
     memcmp(base+0xa364,submit2,sizeof(submit2)) ||
     memcmp(base+0x108550,callee,sizeof(callee)) ||
     memcmp(base+0x6dc8d,fixed,sizeof(fixed))){
    say("WORLD-QUAD-FOV: trial refused; original signature mismatch");return 0;
  }
  first=base+0xa1a9;second=base+0xa367;
  rel=(DWORD)((uintptr_t)quad_submit-(uintptr_t)(first+5));
  memcpy(replacement[0]+1,&rel,4);
  rel=(DWORD)((uintptr_t)quad_submit-(uintptr_t)(second+5));
  memcpy(replacement[1]+1,&rel,4);
  /* Both CALLs are on the same native 4 KiB page. Preflight both,
   * make the complete span writable once, then publish both. */
  if(!VirtualProtect(first,(SIZE_T)(second+5-first),PAGE_EXECUTE_READWRITE,&old)){
    say("WORLD-QUAD-FOV: trial refused; cannot protect original CALLs");return 0;
  }
  memcpy(first,replacement[0],5);
  memcpy(second,replacement[1],5);
  FlushInstructionCache(GetCurrentProcess(),first,(SIZE_T)(second+5-first));
  if(!VirtualProtect(first,(SIZE_T)(second+5-first),old,&restored))
    say("WORLD-QUAD-FOV: warning: native page protection restoration failed");
  installed=1;
  say("WORLD-QUAD-FOV: opt-in native world-quad projection trial installed; two exact calls, X only");
  return 1;
}
