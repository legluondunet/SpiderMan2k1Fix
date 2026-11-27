/* Experimental Shadow2 provenance + opt-in sampling trial. No texture
 * replacement, extra draw, world/UV/alpha changes. Fail closed if unknown. */
static int shadow_material_soft;
static DWORD shadow_material_thread;
static unsigned shadow_material_nest,shadow_material_before,shadow_material_submit_logs;
static LONG shadow_material_frame;
static unsigned shadow_material_draw_logs,shadow_material_match_logs,shadow_material_linear_logs;
static unsigned shadow_descriptor_match_logs,shadow_descriptor_miss_logs;
typedef struct ShadowMaterialRecord {
  LONG stamp;
  DWORD material;
  SpiderMenuVertex verts[4];
} ShadowMaterialRecord;
static ShadowMaterialRecord shadow_material_tags[SPIDER_MENUBG43_RECORD_CAP];

static void shadow_material_observe_submit(int phase) {
  DWORD after,i;
  LONG flip;
  if(!shadow_trace_enabled)return;
  if(!shadow_material_thread)shadow_material_thread=GetCurrentThreadId();
  if(shadow_material_thread!=GetCurrentThreadId())return;
  if(!phase) {
    if(shadow_material_nest++!=0u)return;
    shadow_material_before=SPIDER_MENUBG43_RECORD_CAP+1u;
    if(!SpiderFOV_GameplayReady() ||
       *(const volatile DWORD *)(uintptr_t)0x006151F8u ||
       *(const volatile DWORD *)(uintptr_t)0x00682950u)return;
    shadow_material_frame=InterlockedCompareExchange(&flip_count,0,0);
    if(!shadow_trace_read(SPIDER_MENUBG43_QUEUE_COUNT,&shadow_material_before,sizeof(DWORD)))
      shadow_material_before=SPIDER_MENUBG43_RECORD_CAP+1u;
    return;
  }
  if(!shadow_material_nest || --shadow_material_nest!=0u)return;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(shadow_material_before>SPIDER_MENUBG43_RECORD_CAP || flip!=shadow_material_frame ||
     !shadow_trace_read(SPIDER_MENUBG43_QUEUE_COUNT,&after,sizeof(after)) ||
     after<shadow_material_before || after>SPIDER_MENUBG43_RECORD_CAP ||
     after-shadow_material_before>32u)return;
  if(shadow_material_submit_logs<16u){
    char msg[180];++shadow_material_submit_logs;
    snprintf(msg,sizeof(msg),"SHADOW-MATERIAL: submit flip=%ld queue=%u->%lu",
      (long)flip,shadow_material_before,(unsigned long)after);
    log_line(msg);
  }
  for(i=shadow_material_before;i<after;i++){
    uintptr_t record=SPIDER_MENUBG43_RECORD_BASE+(uintptr_t)i*SPIDER_MENUBG43_RECORD_STRIDE;
    ShadowMaterialRecord *tag=&shadow_material_tags[i];
    tag->stamp=0;
    if(shadow_trace_read(record+4u,&tag->material,sizeof(tag->material)) &&
       shadow_trace_read(record+16u,tag->verts,sizeof(tag->verts)))
      tag->stamp=flip+1;
  }
}

static HRESULT shadow_material_draw(void *self,D3DPRIMITIVETYPE type,DWORD fvf,
    LPVOID source,LPVOID drawn,DWORD count,DWORD flags,void *caller) {
  IDirect3DDevice7 *device=(IDirect3DDevice7 *)self;
  IDirectDrawSurface7 *tex=NULL;
  DDSURFACEDESC2 desc;
  DWORD rva=0,material=0,mag=0,min=0,blend=0,atest=0;
  uint32_t idx;
  LONG flip;
  int matched,known,mag_changed=0,min_changed=0,restored=1;
  int descriptor_material,descriptor_texture;
  uintptr_t bound_texture;
  HRESULT hr;
  ShadowMaterialRecord *tag;
  if(!shadow_trace_enabled || shadow_material_thread!=GetCurrentThreadId() ||
     !source || source!=drawn || type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count!=4u ||
     !SpiderFOV_GameplayReady() ||
     *(const volatile DWORD *)(uintptr_t)0x006151F8u ||
     *(const volatile DWORD *)(uintptr_t)0x00682950u ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL ||
     !SpiderMenubg43_RecordIndex((uintptr_t)source,&idx))
    return original_draw_primitive(self,type,fvf,drawn,count,flags);
  flip=InterlockedCompareExchange(&flip_count,0,0);
  tag=&shadow_material_tags[idx];
  if(tag->stamp!=flip+1 || !tag->material ||
     !shadow_trace_read((uintptr_t)source-12u,&material,sizeof(material)) ||
     material!=tag->material || memcmp(source,tag->verts,sizeof(tag->verts)))
    return original_draw_primitive(self,type,fvf,drawn,count,flags);
  matched=shadow_trace_material_match(material,flip);
  descriptor_material=shadow_descriptor_offset(material,flip);
  if(!matched && descriptor_material<0 && shadow_material_draw_logs>=32u &&
     (shadow_trace_material_count==0 || shadow_descriptor_miss_logs>=64u))
    return original_draw_primitive(self,type,fvf,drawn,count,flags);
  memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
  if(FAILED(IDirect3DDevice7_GetTexture(device,0,&tex)) || !tex){
    if(tex)IDirectDrawSurface7_Release(tex);
    return original_draw_primitive(self,type,fvf,drawn,count,flags);
  }
  bound_texture=(uintptr_t)tex;
  descriptor_texture=shadow_descriptor_offset((uint32_t)bound_texture,flip);
  known=SUCCEEDED(IDirectDrawSurface7_GetSurfaceDesc(tex,&desc));
  IDirectDrawSurface7_Release(tex);
  if(shadow_trace_material_count &&
     ((descriptor_material>=0 || descriptor_texture>=0) ? shadow_descriptor_match_logs<32u : shadow_descriptor_miss_logs<64u)){
    char msg[300];
    if(descriptor_material>=0 || descriptor_texture>=0)++shadow_descriptor_match_logs;
    else ++shadow_descriptor_miss_logs;
    snprintf(msg,sizeof(msg),"SHADOW-DESC-LINK: flip=%ld record=%u queue-material=%08lX bound-texture=%08lX desc-material-offset=%d desc-texture-offset=%d [candidate only]",
      (long)flip,(unsigned)idx,(unsigned long)material,(unsigned long)bound_texture,descriptor_material,descriptor_texture);
    log_line(msg);
  }
  known=known &&
    SUCCEEDED(IDirect3DDevice7_GetTextureStageState(device,0,D3DTSS_MAGFILTER,&mag)) &&
    SUCCEEDED(IDirect3DDevice7_GetTextureStageState(device,0,D3DTSS_MINFILTER,&min)) &&
    SUCCEEDED(IDirect3DDevice7_GetRenderState(device,D3DRENDERSTATE_ALPHABLENDENABLE,&blend)) &&
    SUCCEEDED(IDirect3DDevice7_GetRenderState(device,D3DRENDERSTATE_ALPHATESTENABLE,&atest));
  if((matched && shadow_material_match_logs<16u) || (!matched && shadow_material_draw_logs<32u)){
    char msg[350];
    if(matched)++shadow_material_match_logs;else ++shadow_material_draw_logs;
    snprintf(msg,sizeof(msg),
      "SHADOW-MATERIAL: draw flip=%ld record=%u material=%08lX match=%d known=%d texture=%lux%lu pf=%08lX bits=%lu alpha=%08lX mag=%lu min=%lu blend=%lu atest=%lu",
      (long)flip,(unsigned)idx,(unsigned long)material,matched,known,
      (unsigned long)desc.dwWidth,(unsigned long)desc.dwHeight,
      (unsigned long)desc.ddpfPixelFormat.dwFlags,(unsigned long)desc.ddpfPixelFormat.dwRGBBitCount,
      (unsigned long)desc.ddpfPixelFormat.dwRGBAlphaBitMask,(unsigned long)mag,
      (unsigned long)min,(unsigned long)blend,(unsigned long)atest);
    log_line(msg);
  }
  if(!shadow_material_soft || !matched || !known || !blend || atest ||
     (desc.ddpfPixelFormat.dwFlags&(DDPF_RGB|DDPF_ALPHAPIXELS))!=(DDPF_RGB|DDPF_ALPHAPIXELS) ||
     !desc.ddpfPixelFormat.dwRGBAlphaBitMask ||
     (desc.dwFlags&DDSD_CKSRCBLT) || (desc.dwFlags&DDSD_CKSRCOVERLAY) ||
     (mag!=D3DTFG_POINT && mag!=D3DTFG_LINEAR) ||
     (min!=D3DTFN_POINT && min!=D3DTFN_LINEAR))
    return original_draw_primitive(self,type,fvf,drawn,count,flags);
  if(mag==D3DTFG_POINT){
    if(FAILED(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_MAGFILTER,D3DTFG_LINEAR)))
      return original_draw_primitive(self,type,fvf,drawn,count,flags);
    mag_changed=1;
  }
  if(min==D3DTFN_POINT){
    if(SUCCEEDED(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_MINFILTER,D3DTFN_LINEAR)))
      min_changed=1;
    else {
      if(mag_changed && FAILED(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_MAGFILTER,mag))){
        shadow_material_soft=0;log_line("SHADOW-MATERIAL: MAG restore failed; trial disabled");
      }
      return original_draw_primitive(self,type,fvf,drawn,count,flags);
    }
  }
  hr=original_draw_primitive(self,type,fvf,drawn,count,flags);
  if(min_changed && FAILED(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_MINFILTER,min)))restored=0;
  if(mag_changed && FAILED(IDirect3DDevice7_SetTextureStageState(device,0,D3DTSS_MAGFILTER,mag)))restored=0;
  if(!restored){
    shadow_material_soft=0;log_line("SHADOW-MATERIAL: state restoration failed; trial disabled");
  }else if((mag_changed || min_changed) && shadow_material_linear_logs<16u){
    ++shadow_material_linear_logs;
    log_line("SHADOW-MATERIAL: scoped linear sampling applied; original filters restored");
  }
  return hr;
}
