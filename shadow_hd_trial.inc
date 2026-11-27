/* Opt-in shadow-only replacement, never changes original resource or vertices.
 * The native selector must receive a current live Shadow2 index from an
 * authenticated pickup descriptor. Its resulting current material, tagged
 * queue record and actual stage-0 surface must agree. No size-only matches. */
#include "shadow_alpha_reconstruct.h"
#include "shadow_alpha_procedural.h"
#include "shadow_registry.h"
#include "shadow_draw_paths.h"
static int shadow_hd_enabled;
static int shadow_hd_procedural=1;
static int shadow_hd_procedural_active;
static int shadow_hd_registry_ready;
static DWORD shadow_hd_registry_thread;
static LONG shadow_hd_registry_flip=-1;
static SpiderShadowRegistry shadow_hd_registry;
static unsigned shadow_hd_registry_logs,shadow_hd_registry_misses;
static unsigned shadow_hd_frame_candidates,shadow_hd_frame_draws,shadow_hd_frame_fallback;
static unsigned shadow_hd_total_candidates,shadow_hd_total_draws,shadow_hd_total_fallback;
static unsigned shadow_hd_frames,shadow_hd_full_frames,shadow_hd_partial_frames;
static LONG shadow_hd_last_completed=-1;
static unsigned shadow_hd_last_candidates,shadow_hd_last_draws,shadow_hd_last_fallback;
/* All hooked native primitive calls are observed before HUD / queue gates.
 * Counters are independent of whether the replacement is authorized. */
typedef struct ShadowDrawProbeCounts {
  unsigned named[4],eligible[4],hd[4],texture_query_fail;
} ShadowDrawProbeCounts;
static ShadowDrawProbeCounts shadow_probe_frame,shadow_probe_last,shadow_probe_total;
typedef struct ShadowDrawProbeShape {
  DWORD rva,fvf,count,icount,type,record_count;
  unsigned indexed,queue_match,hits;
} ShadowDrawProbeShape;
static ShadowDrawProbeShape shadow_probe_shapes[32];
static unsigned shadow_probe_shape_count,shadow_probe_overflow;
static unsigned shadow_probe_frames,shadow_probe_empty_frames;
static void shadow_probe_summary(const char *label,LONG flip,const ShadowDrawProbeCounts *p){
  char msg[360];
  snprintf(msg,sizeof(msg),
    "SHADOW-PATHS: %s flip=%ld named(primary/companion/other/indexed)=%u/%u/%u/%u eligible=%u/%u/%u/%u hd=%u/%u/%u/%u texture-query-fail=%u",
    label,(long)flip,p->named[1],p->named[2],p->named[0],p->named[3],
    p->eligible[1],p->eligible[2],p->eligible[0],p->eligible[3],
    p->hd[1],p->hd[2],p->hd[0],p->hd[3],p->texture_query_fail);
  log_line(msg);
}
static unsigned shadow_probe_count(const ShadowDrawProbeCounts *p){
  return p->named[0]+p->named[1]+p->named[2]+p->named[3];
}
static int shadow_hd_registry_read(uintptr_t address,void *out,size_t bytes){
  return shadow_trace_read(address,out,(SIZE_T)bytes);
}
static int shadow_hd_registry_init(void){
  static const BYTE names[]={0xbf,0xb8,0x90,0x6a,0x00,0x8d,0x74,0x24,0x10,0x8d,0x47,0x01};
  static const BYTE loop[]={0x83,0xc7,0x28,0x45,0x81,0xff,0xb8,0xb8,0x6a,0x00,0x7c,0xb7};
  static const BYTE key[]={0x8d,0x44,0xad,0x00,0x8b,0x0c,0xc5,0xdc,0x90,0x6a,0x00,0x8b,0xd1,0x81,0xe2,0xff,0x01,0x00,0x00,0x8b,0x04,0x95,0x34,0xb9,0x6a,0x00};
  static const BYTE chain[]={0x39,0x48,0x14,0x74,0x09,0x8b,0x40,0x20,0x85,0xc0,0x75,0xf4};
  static const BYTE resolver[]={0x8b,0x44,0x24,0x04,0x8d,0x0c,0x40,0x8d,0x14,0x88,0x8b,0x04,0xd5,0xf4,0x12,0xac,0x00,0xc3};
  shadow_hd_registry_ready=shadow_trace_enabled &&
    shadow_trace_signature(0x004c94f1u,names,sizeof(names)) &&
    shadow_trace_signature(0x004c9533u,loop,sizeof(loop)) &&
    shadow_trace_signature(0x004c955bu,key,sizeof(key)) &&
    shadow_trace_signature(0x004c9579u,chain,sizeof(chain)) &&
    shadow_trace_signature(0x0050f3c0u,resolver,sizeof(resolver));
  log_line(shadow_hd_registry_ready?"SHADOW-HD: live named registry verified; continuous mode":
    "SHADOW-HD: named registry signature refused; previous scoped trial only");
  return shadow_hd_registry_ready;
}
static DWORD shadow_hd_registry_surface(LONG flip){
  uint32_t surface=0;
  uint16_t index=0;
  if(!shadow_hd_registry_ready || !shadow_hd_enabled)return 0;
  if(!shadow_hd_registry_thread)shadow_hd_registry_thread=GetCurrentThreadId();
  if(shadow_hd_registry_thread!=GetCurrentThreadId())return 0;
  if(shadow_hd_registry_flip!=flip){
    shadow_hd_registry_flip=flip;
    if(SpiderShadowRegistry_Find(shadow_hd_registry_read,&shadow_hd_registry)){
      if(shadow_hd_registry_logs<8u){
        char msg[180];++shadow_hd_registry_logs;
        snprintf(msg,sizeof(msg),"SHADOW-HD-REGISTRY: flip=%ld descriptor=%08X index=%u surface=%08X",
          (long)flip,(unsigned)shadow_hd_registry.descriptor,(unsigned)shadow_hd_registry.index,(unsigned)shadow_hd_registry.surface);
        log_line(msg);
      }
    }else if(shadow_hd_registry_misses<8u){
      ++shadow_hd_registry_misses;
      log_line("SHADOW-HD-REGISTRY: exact live Shadow2 absent; original preserved");
    }
  }
  if(!shadow_hd_registry.surface ||
     !shadow_trace_read(shadow_hd_registry.descriptor+2u,&index,sizeof(index)) ||
     index!=shadow_hd_registry.index ||
     !shadow_trace_read(SPIDER_SHADOW_SURFACES+(uintptr_t)index*SPIDER_SHADOW_SURFACE_STRIDE,&surface,sizeof(surface)) ||
     surface!=shadow_hd_registry.surface)return 0;
  return surface;
}
static void shadow_hd_finish_frame(LONG next_flip){
  char msg[300];
  if(!shadow_hd_registry_ready ||
     (shadow_hd_registry_thread && shadow_hd_registry_thread!=GetCurrentThreadId()))return;
  shadow_hd_last_completed=next_flip-1;
  shadow_hd_last_candidates=shadow_hd_frame_candidates;
  shadow_hd_last_draws=shadow_hd_frame_draws;
  shadow_hd_last_fallback=shadow_hd_frame_fallback;
  shadow_probe_last=shadow_probe_frame;
  if(shadow_probe_count(&shadow_probe_frame)){
    ++shadow_probe_frames;
    if(shadow_probe_frames<=8u || shadow_probe_frames%120u==0u){
      shadow_probe_summary("frame",next_flip-1,&shadow_probe_frame);
      shadow_probe_summary("session",next_flip-1,&shadow_probe_total);
    }
  }else if(shadow_hd_registry_thread && SpiderFOV_GameplayReady())
    ++shadow_probe_empty_frames;
  memset(&shadow_probe_frame,0,sizeof(shadow_probe_frame));
  if(shadow_hd_frame_candidates){
    ++shadow_hd_frames;
    if(shadow_hd_frame_draws==shadow_hd_frame_candidates)++shadow_hd_full_frames;
    else ++shadow_hd_partial_frames;
    if(shadow_hd_frames<=8u || shadow_hd_frames%120u==0u){
      snprintf(msg,sizeof(msg),"SHADOW-HD-COVERAGE: render-flip=%ld candidates=%u hd=%u fallback=%u total=%u/%u fallback-total=%u frames=%u full=%u partial=%u",
        (long)shadow_hd_last_completed,shadow_hd_frame_candidates,shadow_hd_frame_draws,shadow_hd_frame_fallback,
        shadow_hd_total_draws,shadow_hd_total_candidates,shadow_hd_total_fallback,
        shadow_hd_frames,shadow_hd_full_frames,shadow_hd_partial_frames);
      log_line(msg);
    }
  }
  shadow_hd_frame_candidates=shadow_hd_frame_draws=shadow_hd_frame_fallback=0;
}
static void shadow_hd_capture_report(LONG flip){
  char msg[320];
  if(!shadow_hd_registry_ready || !shadow_hd_registry_thread ||
     shadow_hd_registry_thread!=GetCurrentThreadId())return;
  snprintf(msg,sizeof(msg),"SHADOW-HD-CAPTURE: flip=%ld current=%u/%u fallback=%u last-completed-flip=%ld last=%u/%u fallback=%u session=%u/%u frames=%u full=%u partial=%u",
    (long)flip,shadow_hd_frame_draws,shadow_hd_frame_candidates,shadow_hd_frame_fallback,
    (long)shadow_hd_last_completed,shadow_hd_last_draws,shadow_hd_last_candidates,shadow_hd_last_fallback,
    shadow_hd_total_draws,shadow_hd_total_candidates,shadow_hd_frames,shadow_hd_full_frames,shadow_hd_partial_frames);
  log_line(msg);
  log_line(shadow_hd_procedural_active?"SHADOW-HD-CAPTURE-PROFILE: procedural":"SHADOW-HD-CAPTURE-PROFILE: cubic or not allocated");
  shadow_probe_summary("capture-current",flip,&shadow_probe_frame);
  shadow_probe_summary("capture-last",shadow_hd_last_completed,&shadow_probe_last);
  shadow_probe_summary("capture-session",flip,&shadow_probe_total);
  snprintf(msg,sizeof(msg),"SHADOW-PATHS-FRAMES: named=%u empty-gameplay=%u unique-shapes=%u overflow=%u",
    shadow_probe_frames,shadow_probe_empty_frames,shadow_probe_shape_count,shadow_probe_overflow);
  log_line(msg);
  {
    unsigned i;
    for(i=0;i<shadow_probe_shape_count;i++){
      const ShadowDrawProbeShape *s=&shadow_probe_shapes[i];
      snprintf(msg,sizeof(msg),"SHADOW-PATH-SHAPE: rva=%08lX indexed=%u type=%lu fvf=%08lX vertices=%lu indices=%lu record-count=%lu queue-match=%u total=%u",
        (unsigned long)s->rva,s->indexed,(unsigned long)s->type,(unsigned long)s->fvf,
        (unsigned long)s->count,(unsigned long)s->icount,(unsigned long)s->record_count,s->queue_match,s->hits);
      log_line(msg);
    }
  }
}
static DWORD shadow_hd_selected; /* index, never a COM pointer */
static LONG shadow_hd_selected_flip=-1;
static unsigned shadow_hd_selector_logs,shadow_hd_refusal_logs,shadow_hd_draw_logs;
static IDirectDrawSurface7 *shadow_hd_source,*shadow_hd_surface;
static uint8_t shadow_hd_a4[64u*64u],shadow_hd_a8[256u*256u];

static void shadow_hd_observe_selector(unsigned index) {
  unsigned i;
  LONG flip=InterlockedCompareExchange(&flip_count,0,0);
  uint32_t candidate=0;
  if(!shadow_hd_enabled)return;
  shadow_hd_selected=0;shadow_hd_selected_flip=-1;
  if(!SpiderFOV_GameplayReady() || !shadow_trace_enabled ||
     shadow_trace_thread!=GetCurrentThreadId() ||
     shadow_trace_material_flip<0 || flip<shadow_trace_material_flip ||
     flip-shadow_trace_material_flip>60 || index>65535u)return;
  for(i=0;i<shadow_trace_material_count;i++){
    uint16_t live=0;
    if(shadow_descriptor_valid[i] &&
       (shadow_descriptor_words[i][0]>>16)==index &&
       shadow_trace_read((uintptr_t)shadow_trace_materials[i]+2u,&live,sizeof(live)) &&
       live==index){
      if(shadow_trace_read(0x00568170u,&candidate,sizeof(candidate)) && candidate==index && candidate>2u){
        shadow_hd_selected=candidate;shadow_hd_selected_flip=flip;
        if(shadow_hd_selector_logs<24u){
          char msg[180];++shadow_hd_selector_logs;
          snprintf(msg,sizeof(msg),"SHADOW-HD-SELECT: flip=%ld index=%u descriptor=%08X selected-index=%08X",
            (long)flip,index,(unsigned)shadow_trace_materials[i],(unsigned)candidate);
          log_line(msg);
        }
      }
      break;
    }
  }
}
static DWORD shadow_hd_current_index(LONG flip) {
  DWORD current=0;
  if(!shadow_hd_enabled || !shadow_hd_selected || flip!=shadow_hd_selected_flip ||
     !shadow_trace_read(0x00568170u,&current,sizeof(current)) ||
     current!=shadow_hd_selected)return 0;
  return current;
}
static void shadow_hd_clear(void) {
  if(shadow_hd_surface)IDirectDrawSurface7_Release(shadow_hd_surface);
  if(shadow_hd_source)IDirectDrawSurface7_Release(shadow_hd_source);
  shadow_hd_surface=NULL;shadow_hd_source=NULL;shadow_hd_procedural_active=0;
}
static void shadow_hd_refuse(const char *reason) {
  if(shadow_hd_refusal_logs<12u){
    char msg[200];++shadow_hd_refusal_logs;
    snprintf(msg,sizeof(msg),"SHADOW-HD-REFUSED: %s; original preserved",reason);
    log_line(msg);
  }
}
static int shadow_hd_make(IDirectDrawSurface7 *source) {
  DDSURFACEDESC2 d,lock;
  IDirectDraw7 *dd=NULL;
  IUnknown *owner=NULL;
  IDirectDrawSurface7 *out=NULL;
  unsigned x,y,lo=15,hi=0;
  uint16_t rgb=0;
  int ok=1,procedural=0;
  SpiderShadowFit fit={0};
  HRESULT hr;
  if(source==shadow_hd_source){
    if(!shadow_hd_surface)return 0; /* a failed attempt is not retried per draw */
    if(IDirectDrawSurface7_IsLost(shadow_hd_surface)==DD_OK &&
       IDirectDrawSurface7_IsLost(source)==DD_OK)return 1;
  }
  shadow_hd_clear();
  shadow_hd_source=source;IDirectDrawSurface7_AddRef(source);
  memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
  if(FAILED(IDirectDrawSurface7_GetSurfaceDesc(source,&d)) ||
     d.dwWidth!=64u || d.dwHeight!=64u ||
     d.ddpfPixelFormat.dwFlags!=(DDPF_RGB|DDPF_ALPHAPIXELS) ||
     d.ddpfPixelFormat.dwRGBBitCount!=16u ||
     d.ddpfPixelFormat.dwRGBAlphaBitMask!=0xf000u ||
     d.ddpfPixelFormat.dwRBitMask!=0x0f00u ||
     d.ddpfPixelFormat.dwGBitMask!=0x00f0u ||
     d.ddpfPixelFormat.dwBBitMask!=0x000fu ||
     (d.dwFlags&(DDSD_CKSRCBLT|DDSD_CKSRCOVERLAY)) ||
     (d.ddsCaps.dwCaps&(DDSCAPS_MIPMAP|DDSCAPS_COMPLEX))){
    shadow_hd_refuse("unsupported source format");return 0;
  }
  memset(&lock,0,sizeof(lock));lock.dwSize=sizeof(lock);
  if(FAILED(IDirectDrawSurface7_Lock(source,NULL,&lock,DDLOCK_READONLY|DDLOCK_WAIT,NULL))){
    shadow_hd_refuse("source lock failed");return 0;
  }
  if(!lock.lpSurface || lock.lPitch<128 || lock.lPitch>65536)ok=0;
  if(ok){
    memcpy(&rgb,lock.lpSurface,2);rgb&=0x0fffu;
    for(y=0;y<64u;y++)for(x=0;x<64u;x++){
      uint16_t pixel;unsigned alpha;
      memcpy(&pixel,(BYTE*)lock.lpSurface+(size_t)y*(size_t)lock.lPitch+x*2u,2);
      alpha=pixel>>12;
      if((pixel&0x0fffu)!=rgb)ok=0;
      if(alpha<lo)lo=alpha;
      if(alpha>hi)hi=alpha;
      shadow_hd_a4[y*64u+x]=(uint8_t)alpha;
    }
  }
  if(FAILED(IDirectDrawSurface7_Unlock(source,NULL)))ok=0;
  if(!ok || lo==hi){
    shadow_hd_refuse("source is not a uniform-RGB varying-alpha mask");return 0;
  }
  if(shadow_hd_procedural){
    char msg[320];
    procedural=SpiderShadow_Procedural(shadow_hd_a4,shadow_hd_a8,&fit);
    snprintf(msg,sizeof(msg),"SHADOW-HD-PROFILE: mode=%s reason=%u bbox=%u,%u-%u,%u peak-a4=%u source-sum-a4=%u output-sum-a8=%u shape-error-permille=%u profile=%u mix-permille=%u",
      procedural?"procedural":"cubic-fallback",fit.reason,fit.left,fit.top,fit.right,fit.bottom,
      fit.peak,fit.source_sum,fit.output_sum,fit.error_permille,fit.profile,fit.mix_permille);
    log_line(msg);
  }
  if(!procedural && !SpiderShadow_Alpha4To8(shadow_hd_a4,shadow_hd_a8)){
    shadow_hd_refuse("alpha reconstruction failed");return 0;
  }
  if(FAILED(IDirectDrawSurface7_GetDDInterface(source,(void**)&owner)) || !owner){
    if(owner)owner->lpVtbl->Release(owner);
    shadow_hd_refuse("cannot obtain owning DirectDraw");return 0;
  }
  hr=owner->lpVtbl->QueryInterface(owner,&IID_IDirectDraw7,(void**)&dd);
  owner->lpVtbl->Release(owner);
  if(FAILED(hr) || !dd){if(dd)IDirectDraw7_Release(dd);shadow_hd_refuse("DirectDraw7 unavailable");return 0;}
  memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
  d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
  d.dwWidth=256;d.dwHeight=256;d.ddsCaps.dwCaps=DDSCAPS_TEXTURE;
  d.ddpfPixelFormat.dwSize=sizeof(d.ddpfPixelFormat);
  d.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;
  d.ddpfPixelFormat.dwRGBBitCount=32;
  d.ddpfPixelFormat.dwRBitMask=0x00ff0000u;
  d.ddpfPixelFormat.dwGBitMask=0x0000ff00u;
  d.ddpfPixelFormat.dwBBitMask=0x000000ffu;
  d.ddpfPixelFormat.dwRGBAlphaBitMask=0xff000000u;
  hr=IDirectDraw7_CreateSurface(dd,&d,&out,NULL);
  IDirectDraw7_Release(dd);
  if(FAILED(hr) || !out){if(out)IDirectDrawSurface7_Release(out);shadow_hd_refuse("A8R8G8B8 allocation failed");return 0;}
  memset(&lock,0,sizeof(lock));lock.dwSize=sizeof(lock);
  hr=IDirectDrawSurface7_Lock(out,NULL,&lock,DDLOCK_WAIT|DDLOCK_WRITEONLY,NULL);
  if(FAILED(hr)){IDirectDrawSurface7_Release(out);shadow_hd_refuse("replacement lock failed");return 0;}
  ok=lock.lpSurface && lock.lPitch>=1024 && lock.lPitch<=65536 &&
     lock.ddpfPixelFormat.dwRGBBitCount==32u &&
     lock.ddpfPixelFormat.dwRGBAlphaBitMask==0xff000000u &&
     lock.ddpfPixelFormat.dwRBitMask==0x00ff0000u &&
     lock.ddpfPixelFormat.dwGBitMask==0x0000ff00u &&
     lock.ddpfPixelFormat.dwBBitMask==0x000000ffu;
  if(ok){
    DWORD rgb8=(((DWORD)rgb>>8)&15u)*17u<<16 |
               (((DWORD)rgb>>4)&15u)*17u<<8 | ((DWORD)rgb&15u)*17u;
    for(y=0;y<256u;y++)for(x=0;x<256u;x++){
      DWORD pixel=((DWORD)shadow_hd_a8[y*256u+x]<<24)|rgb8;
      memcpy((BYTE*)lock.lpSurface+(size_t)y*(size_t)lock.lPitch+x*4u,&pixel,4);
    }
  }
  if(FAILED(IDirectDrawSurface7_Unlock(out,NULL)))ok=0;
  if(!ok){IDirectDrawSurface7_Release(out);shadow_hd_refuse("replacement format or pitch mismatch");return 0;}
  shadow_hd_surface=out;shadow_hd_procedural_active=procedural;
  log_line(procedural?"SHADOW-HD: generated 256x256 procedural A8R8G8B8 from calibrated original mask":
    "SHADOW-HD: generated 256x256 A8R8G8B8 from original 64x64 A4 mask (cubic)");
  return 1;
}
static int shadow_hd_draw(IDirect3DDevice7 *device,IDirectDrawSurface7 *source,
    D3DPRIMITIVETYPE type,DWORD fvf,LPVOID vertices,DWORD count,DWORD flags,HRESULT *result) {
  DWORD blend=0,atest=0,mag=0,min=0;
  if(!shadow_hd_enabled || !result ||
     FAILED(IDirect3DDevice7_GetRenderState(device,D3DRENDERSTATE_ALPHABLENDENABLE,&blend)) || !blend ||
     FAILED(IDirect3DDevice7_GetRenderState(device,D3DRENDERSTATE_ALPHATESTENABLE,&atest)) || atest ||
     FAILED(IDirect3DDevice7_GetTextureStageState(device,0,D3DTSS_MAGFILTER,&mag)) || mag!=D3DTFG_LINEAR ||
     FAILED(IDirect3DDevice7_GetTextureStageState(device,0,D3DTSS_MINFILTER,&min)) || min!=D3DTFN_LINEAR ||
     !shadow_hd_make(source))return 0;
  if(FAILED(IDirect3DDevice7_SetTexture(device,0,shadow_hd_surface))){
    shadow_hd_refuse("replacement binding failed");shadow_hd_enabled=0;return 0;
  }
  *result=original_draw_primitive(device,type,fvf,vertices,count,flags);
  if(FAILED(IDirect3DDevice7_SetTexture(device,0,source))){
    shadow_hd_enabled=0;log_line("SHADOW-HD: original binding restore FAILED; trial disabled");
  }else if(shadow_hd_draw_logs<24u){
    ++shadow_hd_draw_logs;log_line("SHADOW-HD-DRAW: replacement drawn; original texture restored");
  }
  return 1;
}
