/* Opt-in shadow-only replacement, never changes original resource or vertices.
 * The native selector must receive a current live Shadow2 index from an
 * authenticated pickup descriptor. Its resulting current material, tagged
 * queue record and actual stage-0 surface must agree. No size-only matches. */
#include "shadow_alpha_reconstruct.h"
static int shadow_hd_enabled;
static DWORD shadow_hd_selected;
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
      if(shadow_trace_read(0x00568170u,&candidate,sizeof(candidate)) && candidate){
        shadow_hd_selected=candidate;shadow_hd_selected_flip=flip;
        if(shadow_hd_selector_logs<24u){
          char msg[180];++shadow_hd_selector_logs;
          snprintf(msg,sizeof(msg),"SHADOW-HD-SELECT: flip=%ld index=%u descriptor=%08X material=%08X",
            (long)flip,index,(unsigned)shadow_trace_materials[i],(unsigned)candidate);
          log_line(msg);
        }
      }
      break;
    }
  }
}
static DWORD shadow_hd_current_material(LONG flip) {
  DWORD current=0;
  if(!shadow_hd_enabled || !shadow_hd_selected || flip!=shadow_hd_selected_flip ||
     !shadow_trace_read(0x00568170u,&current,sizeof(current)) ||
     current!=shadow_hd_selected)return 0;
  return current;
}
static void shadow_hd_clear(void) {
  if(shadow_hd_surface)IDirectDrawSurface7_Release(shadow_hd_surface);
  if(shadow_hd_source)IDirectDrawSurface7_Release(shadow_hd_source);
  shadow_hd_surface=NULL;shadow_hd_source=NULL;
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
  int ok=1;
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
  if(!ok || lo==hi || !SpiderShadow_Alpha4To8(shadow_hd_a4,shadow_hd_a8)){
    shadow_hd_refuse("source is not a uniform-RGB varying-alpha mask");return 0;
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
  shadow_hd_surface=out;
  log_line("SHADOW-HD: generated 256x256 A8R8G8B8 from original 64x64 A4 mask");
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
