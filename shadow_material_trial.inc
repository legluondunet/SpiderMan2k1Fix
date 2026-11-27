/* Enhanced shadows: verified named registry, guarded drawing, optional audit. */
#include "shadow_hd_trial.inc"
/* Observe the actual stage-0 texture on every hooked native draw, before
 * the existing UI, topology or queue checks. Unknown sites are read-only.
 * Use the per-frame registry cache to keep the common non-shadow path cheap;
 * a positive match revalidates the live descriptor and table entry. */
static void shadow_hd_probe_draw(void *self,D3DPRIMITIVETYPE type,DWORD fvf,
    LPVOID source,DWORD count,DWORD icount,int indexed,void *caller){
  DWORD rva=0,material=0,record_count=0,named;
  uint32_t idx;
  LONG flip;
  unsigned path,i,queue_match=0;
  IDirectDrawSurface7 *tex=NULL;
  IDirect3DDevice7 *device=(IDirect3DDevice7*)self;
  if(!shadow_hd_audit || !shadow_hd_registry_ready || !shadow_hd_enabled ||
     !SpiderFOV_GameplayReady() ||
     *(const volatile DWORD *)(uintptr_t)0x006151F8u ||
     *(const volatile DWORD *)(uintptr_t)0x00682950u ||
     !geom_rva(caller,&rva))return;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  if(shadow_hd_registry_flip!=flip)shadow_hd_registry_surface(flip);
  if(shadow_hd_registry_thread!=GetCurrentThreadId() || !shadow_hd_registry.surface)return;
  named=shadow_hd_registry.surface;
  if(FAILED(IDirect3DDevice7_GetTexture(device,0,&tex))){
    ++shadow_probe_frame.texture_query_fail;++shadow_probe_total.texture_query_fail;
    if(tex)IDirectDrawSurface7_Release(tex);
    return;
  }
  if(!tex)return;
  if((uintptr_t)tex!=(uintptr_t)named){IDirectDrawSurface7_Release(tex);return;}
  IDirectDrawSurface7_Release(tex);
  if(shadow_hd_registry_surface(flip)!=named)return;
  path=SpiderShadowDraw_Path(rva,indexed);
  ++shadow_probe_frame.named[path];++shadow_probe_total.named[path];
  if(source && SpiderMenubg43_RecordIndex((uintptr_t)source,&idx) &&
     shadow_trace_read((uintptr_t)source-12u,&material,sizeof(material)) &&
     shadow_trace_read((uintptr_t)source-4u,&record_count,sizeof(record_count)) &&
     material==named)queue_match=1;
  if(queue_match && SpiderShadowDraw_Eligible(path,type,fvf,count,record_count,material,named)){
    ++shadow_probe_frame.eligible[path];++shadow_probe_total.eligible[path];
  }
  for(i=0;i<shadow_probe_shape_count;i++){
    ShadowDrawProbeShape *p=&shadow_probe_shapes[i];
    if(p->rva==rva && p->indexed==(unsigned)indexed && p->type==(DWORD)type &&
       p->fvf==fvf && p->count==count && p->icount==icount &&
       p->record_count==record_count && p->queue_match==queue_match){
      ++p->hits;return;
    }
  }
  if(shadow_probe_shape_count>=32u){++shadow_probe_overflow;return;}
  {
    char msg[260];
    ShadowDrawProbeShape *p=&shadow_probe_shapes[shadow_probe_shape_count++];
    p->rva=rva;p->indexed=(unsigned)indexed;p->type=(DWORD)type;p->fvf=fvf;
    p->count=count;p->icount=icount;p->record_count=record_count;
    p->queue_match=queue_match;p->hits=1;
    snprintf(msg,sizeof(msg),"SHADOW-PATH-NEW: flip=%ld rva=%08lX indexed=%d type=%lu fvf=%08lX vertices=%lu indices=%lu record-count=%lu queue-match=%u",
      (long)flip,(unsigned long)rva,indexed,(unsigned long)type,(unsigned long)fvf,
      (unsigned long)count,(unsigned long)icount,(unsigned long)record_count,queue_match);
    log_line(msg);
  }
}

/* Independent from bounded pickup snapshots and scoped converter logs.
 * Same native flush, exact queue record, live named material and bound
 * surface. Preserve the vertices selected by the existing draw path,
 * including its prior aspect corrections. Never classify by size. */
static int shadow_hd_registry_draw(void *self,D3DPRIMITIVETYPE type,DWORD fvf,
    LPVOID source,LPVOID drawn,DWORD count,DWORD flags,void *caller,HRESULT *result){
  DWORD rva=0,material=0,named=0,record_count=0;
  unsigned path;
  uint32_t idx;
  LONG flip;
  IDirectDrawSurface7 *tex=NULL;
  IDirect3DDevice7 *device=(IDirect3DDevice7*)self;
  int replaced=0;
  if(!shadow_hd_registry_ready || !shadow_hd_enabled || !source || !drawn ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL || count<3u || count>8u ||
     !SpiderFOV_GameplayReady() ||
     *(const volatile DWORD *)(uintptr_t)0x006151F8u ||
     *(const volatile DWORD *)(uintptr_t)0x00682950u ||
     !geom_rva(caller,&rva) || (rva!=0x00102C72UL && rva!=0x00103389UL) ||
     !SpiderMenubg43_RecordIndex((uintptr_t)source,&idx) ||
     !shadow_trace_read((uintptr_t)source-12u,&material,sizeof(material)) || !material ||
     !shadow_trace_read((uintptr_t)source-4u,&record_count,sizeof(record_count)))return 0;
  flip=InterlockedCompareExchange(&flip_count,0,0);
  /* Resolve once per Flip; reject unrelated queue materials before any
   * repeated descriptor/table read or COM GetTexture. Positive matches
   * still revalidate against the live native descriptor and table. */
  if(shadow_hd_registry_flip!=flip)shadow_hd_registry_surface(flip);
  if(shadow_hd_registry_thread!=GetCurrentThreadId() ||
     material!=shadow_hd_registry.surface)return 0;
  named=shadow_hd_registry_surface(flip);
  path=SpiderShadowDraw_Path(rva,0);
  if(!named || material!=named ||
     !SpiderShadowDraw_Eligible(path,type,fvf,count,record_count,material,named))return 0;
  ++shadow_hd_frame_candidates;++shadow_hd_total_candidates;
  if(SUCCEEDED(IDirect3DDevice7_GetTexture(device,0,&tex)) && tex &&
     (uintptr_t)tex==(uintptr_t)named)
    replaced=shadow_hd_draw(device,tex,type,fvf,drawn,count,flags,result);
  if(tex)IDirectDrawSurface7_Release(tex);
  if(replaced && SUCCEEDED(*result) && shadow_hd_enabled){
    ++shadow_hd_frame_draws;++shadow_hd_total_draws;
    if(shadow_hd_audit){++shadow_probe_frame.hd[path];++shadow_probe_total.hd[path];}
  }else{
    ++shadow_hd_frame_fallback;++shadow_hd_total_fallback;
  }
  return replaced;
}
