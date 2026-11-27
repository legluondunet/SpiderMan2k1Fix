#ifndef SPIDER_PICKUP_SHADOW_TRACE_H
#define SPIDER_PICKUP_SHADOW_TRACE_H
/* Developer-only, SHA-checked native game-thread read-only snapshots.
 * No native hooks, allocations, persistent pointer dereferences or writes.
 * See docs/references/diagnostics/PICKUP_SHADOW_WORLD.md. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SPIDER_SHADOW_PICKUP_LIST 0x0060FB94u
#define SPIDER_SHADOW_ACTIVE_LIST 0x0060DAB4u
#define SPIDER_SHADOW_PICKUP_VTABLE 0x0053BC5Cu
#define SPIDER_SHADOW_QUAD_VTABLE 0x0053B35Cu
#define SPIDER_SHADOW_NODES 128u
#define SPIDER_SHADOW_PICKUPS 16u
#define SPIDER_SHADOW_PERIODIC_MAX 120u
#define SPIDER_SHADOW_F9_MAX 12u
static int shadow_trace_enabled=0;
static unsigned shadow_trace_periodic=0,shadow_trace_f9=0;
static DWORD shadow_trace_thread=0;
static void (*shadow_trace_log)(const char *)=NULL;

static int shadow_trace_read(uintptr_t address,void *out,SIZE_T length) {
  SIZE_T received=0;
  if(!address || !out || !length || address>UINTPTR_MAX-length)return 0;
  return ReadProcessMemory(GetCurrentProcess(),(LPCVOID)address,out,length,&received)
    && received==length;
}
static uint32_t shadow_trace_u32(const BYTE *p) {
  uint32_t v;memcpy(&v,p,sizeof(v));return v;
}
static int32_t shadow_trace_i32(const BYTE *p) {
  int32_t v;memcpy(&v,p,sizeof(v));return v;
}
static uint16_t shadow_trace_u16(const BYTE *p) {
  uint16_t v;memcpy(&v,p,sizeof(v));return v;
}
static int shadow_trace_signature(uintptr_t address,const BYTE *bytes,SIZE_T n) {
  BYTE actual[32];
  return n<=sizeof(actual) && shadow_trace_read(address,actual,n) &&
      !memcmp(actual,bytes,n);
}
static void shadow_trace_init(int enabled,void (*logger)(const char *)) {
  static const BYTE owner[]={0xc7,0x06,0x5c,0xbc,0x53,0x00,0x66,0x89,0x46,0x38};
  static const BYTE link[]={0x68,0x94,0xfb,0x60,0x00,0x8b,0xce,0xe8,0x02,0x4f,0xff,0xff};
  static const BYTE texture[]={0x68,0x78,0x83,0x54,0x00,0xe8,0x62,0x03,0x0c,0x00,0x83,0xc4,0x04,0x89,0x46,0x60};
  static const BYTE build[]={0xe8,0x89,0x8d,0xfa,0xff};
  static const BYTE quad[]={0xc7,0x06,0x5c,0xb3,0x53,0x00};
  static const BYTE dispatch[]={0xf6,0x46,0x46,0x08,0x75,0x08,0x39,0xae,0xcc,0x00,0x00,0x00,0x74,0x07,0x8b,0xce,0xe8,0x59,0xf4,0xff,0xff};
  shadow_trace_log=logger;
  if(!enabled)return;
  if(sizeof(void*)!=4 || (uintptr_t)GetModuleHandleA(NULL)!=0x400000u ||
     !shadow_trace_signature(0x46B00Eu,owner,sizeof(owner)) ||
     !shadow_trace_signature(0x46B352u,link,sizeof(link)) ||
     !shadow_trace_signature(0x409144u,texture,sizeof(texture)) ||
     !shadow_trace_signature(0x460672u,build,sizeof(build)) ||
     !shadow_trace_signature(0x409005u,quad,sizeof(quad)) ||
     !shadow_trace_signature(0x461132u,dispatch,sizeof(dispatch))){
    if(logger)logger("SHADOW-WORLD: refused; original pickup/quad signatures mismatch");
    return;
  }
  shadow_trace_enabled=1;
  if(logger)logger("SHADOW-WORLD: active; periodic and F9 native world snapshots only, no game writes");
}
static void shadow_trace_snapshot(LONG flip,int f9) {
  static const uintptr_t heads[]={SPIDER_SHADOW_PICKUP_LIST,SPIDER_SHADOW_ACTIVE_LIST};
  static const unsigned offsets[]={0x10,0x3c,0x48,0x54};
  uintptr_t visited[2u*SPIDER_SHADOW_NODES];
  unsigned seen=0,found=0,reported=0,refused=0,limited=0,list,j,k;
  int32_t camera[3]={0,0,0};
  uint32_t p;
  char line[480];
  if(!shadow_trace_enabled || !shadow_trace_log || flip<1)return;
  if(!f9 && (flip%60!=0 || shadow_trace_periodic>=SPIDER_SHADOW_PERIODIC_MAX))return;
  if(f9 && shadow_trace_f9>=SPIDER_SHADOW_F9_MAX)return;
  if(!shadow_trace_thread)shadow_trace_thread=GetCurrentThreadId();
  if(shadow_trace_thread!=GetCurrentThreadId())return;
  if(f9)++shadow_trace_f9;else ++shadow_trace_periodic;
  if(shadow_trace_read(0x56F1B4u,camera,sizeof(camera))){
    snprintf(line,sizeof(line),"SHADOW-WORLD: snapshot flip=%ld trigger=%s camera-native=(%ld,%ld,%ld)",
      (long)flip,f9?"F9":"periodic",(long)camera[0],(long)camera[1],(long)camera[2]);
    shadow_trace_log(line);
  }else{
    snprintf(line,sizeof(line),"SHADOW-WORLD: snapshot flip=%ld trigger=%s camera-unavailable",
      (long)flip,f9?"F9":"periodic");
    shadow_trace_log(line);
  }
  for(list=0;list<2u;list++){
    if(!shadow_trace_read(heads[list],&p,sizeof(p))){++refused;continue;}
    for(j=0;p && j<SPIDER_SHADOW_NODES;j++){
      BYTE prefix[0x24],owner[0x138],quad[0x84];
      uint32_t next,child;
      int duplicate=0;
      for(k=0;k<seen;k++)if(visited[k]==(uintptr_t)p){duplicate=1;break;}
      if(duplicate){++limited;break;}
      visited[seen++]=(uintptr_t)p;
      if(!shadow_trace_read(p,prefix,sizeof(prefix))){++refused;break;}
      next=shadow_trace_u32(prefix+0x20);
      if(shadow_trace_u32(prefix)==SPIDER_SHADOW_PICKUP_VTABLE){
        ++found;
        if(reported>=SPIDER_SHADOW_PICKUPS){++limited;break;}
        if(!shadow_trace_read(p,owner,sizeof(owner)) ||
           shadow_trace_u32(owner)!=SPIDER_SHADOW_PICKUP_VTABLE ||
           shadow_trace_u32(owner+0x20)!=next){++refused;break;}
        child=shadow_trace_u32(owner+0xcc);
        if(child && shadow_trace_read(child,quad,sizeof(quad)) &&
           shadow_trace_u32(quad)==SPIDER_SHADOW_QUAD_VTABLE){
          int32_t xyz[4][3];
          int64_t d[3]={0,0,0};
          unsigned v,a;
          for(v=0;v<4u;v++)for(a=0;a<3u;a++){
            xyz[v][a]=shadow_trace_i32(quad+offsets[v]+4u*a);
            d[a]+=(int64_t)xyz[v][a];
          }
          for(a=0;a<3u;a++)d[a]-=4*(int64_t)shadow_trace_i32(owner+0xb8+4u*a);
          snprintf(line,sizeof(line),
            "SHADOW-WORLD: flip=%ld owner=%08X type=%u list=%u pos=(%ld,%ld,%ld) anchor=(%ld,%ld,%ld) ground=(%ld,%ld,%ld) quad=%08X size=%u color=%06X center-minus-ground-x4=(%lld,%lld,%lld)",
            (long)flip,(unsigned)p,(unsigned)shadow_trace_u16(owner+0x38),list,
            (long)shadow_trace_i32(owner+8),(long)shadow_trace_i32(owner+0xc),(long)shadow_trace_i32(owner+0x10),
            (long)shadow_trace_i32(owner+0x110),(long)shadow_trace_i32(owner+0x114),(long)shadow_trace_i32(owner+0x118),
            (long)shadow_trace_i32(owner+0xb8),(long)shadow_trace_i32(owner+0xbc),(long)shadow_trace_i32(owner+0xc0),
            (unsigned)child,(unsigned)shadow_trace_u16(owner+0xd0),(unsigned)(shadow_trace_u32(quad+0x6c)&0xffffffu),
            (long long)d[0],(long long)d[1],(long long)d[2]);
          shadow_trace_log(line);
          snprintf(line,sizeof(line),
            "SHADOW-WORLD-CORNERS: flip=%ld owner=%08X q0=(%ld,%ld,%ld) q1=(%ld,%ld,%ld) q2=(%ld,%ld,%ld) q3=(%ld,%ld,%ld)",
            (long)flip,(unsigned)p,(long)xyz[0][0],(long)xyz[0][1],(long)xyz[0][2],
            (long)xyz[1][0],(long)xyz[1][1],(long)xyz[1][2],(long)xyz[2][0],(long)xyz[2][1],
            (long)xyz[2][2],(long)xyz[3][0],(long)xyz[3][1],(long)xyz[3][2]);
          shadow_trace_log(line);
        }else{
          snprintf(line,sizeof(line),
            "SHADOW-WORLD: flip=%ld owner=%08X type=%u list=%u no-verified-quad=%s",
            (long)flip,(unsigned)p,(unsigned)shadow_trace_u16(owner+0x38),list,
            child?"unreadable-or-other-class":"absent");
          shadow_trace_log(line);
          if(child)++refused;
        }
        ++reported;
      }
      p=next;
    }
    if(p && j==SPIDER_SHADOW_NODES)++limited;
  }
  snprintf(line,sizeof(line),"SHADOW-WORLD: summary flip=%ld visited=%u pickups=%u reported=%u refused=%u limited=%u",
    (long)flip,seen,found,reported,refused,limited);
  shadow_trace_log(line);
}
#endif
