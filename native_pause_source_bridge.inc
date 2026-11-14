/* SHA-pinned original ESC renderer observation, independent of 3D level.
 * This code is included after native_pause43_init() by spiderman2k1fix.c.
 * It never modifies the game-owned memory, renderer state or input directly.
 */
static volatile LONG native_pause_source_flip=-100000;
static volatile LONG native_pause_source_space=SPIDER_PAUSE_SOURCE_NONE;
static volatile LONG native_pause_source_top=0,native_pause_source_bottom=0;
static volatile LONG native_pause_source_reports=0,native_pause_source_remaps=0;

static int native_pause_source_visible_near_flip(LONG flip) {
  LONG last=InterlockedCompareExchange(&native_pause_source_flip,0,0);
  return InterlockedCompareExchange(&menu_upscale_active,0,0)==0 &&
         InterlockedCompareExchange(&fov_game_height,0,0)>0 &&
         /* Detection is independent of transform eligibility. In native
          * physical 4:3, space=NONE is expected, but Pause still exists. */
         last>=0 && flip>=last && flip-last<=1;
}
static void native_pause_source_reset(void) {
  InterlockedExchange(&native_pause_source_flip,-100000);
  InterlockedExchange(&native_pause_source_space,SPIDER_PAUSE_SOURCE_NONE);
}
static void native_pause_source_observe_ribbon(
    void *device,D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
    DWORD count,void *caller) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  float left,right,top,bottom;
  DWORD rva=0;
  unsigned i,logical_w,logical_h,space;
  IDirectDrawSurface7 *texture=NULL;
  if(!device || !v || count!=4u || type!=D3DPT_TRIANGLEFAN ||
     fvf!=0x144UL ||
     InterlockedCompareExchange(&menu_upscale_active,0,0)!=0 ||
     InterlockedCompareExchange(&fov_game_height,0,0)<=0 ||
     /* Original GameFMV_Active: cinematic owner must never claim Pause. */
     (*(const volatile DWORD *)(uintptr_t)0x006151F8u)!=0u ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL ||
     desktop_w<640u || desktop_h<480u)return;
  if(!SpiderPauseSource_RibbonMaterial(v[0].z,v[0].rhw,v[0].diffuse) &&
     !SpiderNativePause43_RibbonLayer(v[0].z,v[0].rhw))return;
  left=right=v[0].x;top=bottom=v[0].y;
  for(i=0u;i<4u;i++){
    if(!SpiderPauseSource_ValidVertex(v[i].x,v[i].y,v[i].z,v[i].rhw,
        (unsigned)desktop_w,(unsigned)desktop_h) ||
       !(SpiderPauseSource_Abs(v[i].z-v[0].z)<0.00010f) ||
       !(SpiderPauseSource_Abs(v[i].rhw-v[0].rhw)<0.30f))return;
    if(v[i].x<left)left=v[i].x;
    if(v[i].x>right)right=v[i].x;
    if(v[i].y<top)top=v[i].y;
    if(v[i].y>bottom)bottom=v[i].y;
  }
  logical_h=(unsigned)desktop_h;
  logical_w=(unsigned)((uint64_t)logical_h*4u/3u);
  if(InterlockedCompareExchange(&gameplay_4x3_active,0,0)==1) {
    logical_h=(unsigned)game_logical_h;
    logical_w=(unsigned)game_logical_w;
  }
  space=SpiderPauseSource_Space(left,right,top,bottom,
    logical_w,logical_h,(unsigned)desktop_w,(unsigned)desktop_h);
  /* When the physical monitor itself is genuine 4:3, there is no
   * physical/logical transform to distinguish. Still recognize the
   * source-verified Pause menu for SDL3, but never remap its sprites. */
  if(space==SPIDER_PAUSE_SOURCE_NONE &&
     !((uint64_t)desktop_w*3u==(uint64_t)desktop_h*4u &&
       SpiderPauseSource_RibbonBounds(left,right,top,bottom,
         (unsigned)desktop_w,(unsigned)desktop_h)))return;
  /* The original Pause ribbon is a textureless fan. */
  if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,
                                        &texture)))return;
  if(texture) { IDirectDrawSurface7_Release(texture);return; }
  {
    LONG old=InterlockedCompareExchange(&native_pause_source_space,0,0);
    LONG flip=InterlockedCompareExchange(&flip_count,0,0);
    InterlockedExchange(&native_pause_source_space,(LONG)space);
    InterlockedExchange(&native_pause_source_flip,flip);
    InterlockedExchange(&native_pause_source_top,(LONG)top);
    InterlockedExchange(&native_pause_source_bottom,(LONG)bottom);
    if(debug_log_enabled &&
       (old!=(LONG)space || InterlockedIncrement(&native_pause_source_reports)<=4)) {
      char msg[260];
      snprintf(msg,sizeof(msg),
        "PAUSE-SOURCE: original ribbon flip=%ld source=%s logical=%ux%u physical=%lux%lu bounds=(%.1f..%.1f,%.1f..%.1f) [READ-ONLY; ALL LEVELS]",
        (long)flip,space==SPIDER_PAUSE_SOURCE_PHYSICAL?"physical":
        space==SPIDER_PAUSE_SOURCE_LOGICAL?"logical":"native-4:3",
        logical_w,logical_h,(unsigned long)desktop_w,(unsigned long)desktop_h,
        (double)left,(double)right,(double)top,(double)bottom);
      log_line(msg);
    }
  }
}
/* A source-verified physical-width ESC overlay in a forced 4:3 game
 * must NOT receive the generic game_map logical offset. Instead map the
 * original physical X to the same centered 4:3 canvas as its ribbon.
 * Logical-source Pause, the HUD, normal 3D and all unknown materials
 * keep the existing renderer. No level/RHW look-up table is used.
 */
static int native_pause_source_remap_original_4x3(
    void *device,D3DPRIMITIVETYPE type,DWORD fvf,const void *source,
    DWORD count,void *caller,SpiderMenuVertex *copy) {
  const SpiderMenuVertex *v=(const SpiderMenuVertex*)source;
  LONG flip=InterlockedCompareExchange(&flip_count,0,0);
  DWORD rva=0;
  IDirectDrawSurface7 *texture=NULL;
  DDSURFACEDESC2 desc;
  unsigned width=0,height=0,i;
  int bound=0;
  LONG top,bottom;
  if(InterlockedCompareExchange(&gameplay_4x3_active,0,0)!=1 ||
     InterlockedCompareExchange(&native_pause_source_space,0,0)!=SPIDER_PAUSE_SOURCE_PHYSICAL ||
     !native_pause_source_visible_near_flip(flip) ||
     !device || !v || !copy || count!=4u ||
     type!=D3DPT_TRIANGLEFAN || fvf!=0x144UL ||
     !geom_rva(caller,&rva) || rva!=0x00102C72UL)return 0;
  if(FAILED(IDirect3DDevice7_GetTexture((IDirect3DDevice7*)device,0,
                                        &texture)))return 0;
  if(texture) {
    memset(&desc,0,sizeof(desc));desc.dwSize=sizeof(desc);
    if(FAILED(IDirectDrawSurface7_GetSurfaceDesc(texture,&desc))) {
      IDirectDrawSurface7_Release(texture);return 0;
    }
    width=(unsigned)desc.dwWidth;height=(unsigned)desc.dwHeight;bound=1;
    IDirectDrawSurface7_Release(texture);
  }
  if(!SpiderPauseSource_Material(v[0].z,v[0].rhw,v[0].diffuse,
                                 width,height,bound)){
    float l=v[0].x,r=v[0].x,t=v[0].y,bt=v[0].y;
    /* An inherited depth-family match alone is NOT authorization to
     * transform arbitrary untextured native 3D/HUD primitives. It may
     * select only the original full-width ribbon itself. */
    if(bound || !SpiderNativePause43_RibbonLayer(v[0].z,v[0].rhw))
      return 0;
    for(i=1u;i<4u;i++){
      if(v[i].x<l)l=v[i].x;if(v[i].x>r)r=v[i].x;
      if(v[i].y<t)t=v[i].y;if(v[i].y>bt)bt=v[i].y;
    }
    if(!SpiderPauseSource_RibbonBounds(l,r,t,bt,
       (unsigned)desktop_w,(unsigned)desktop_h))return 0;
  }
  top=InterlockedCompareExchange(&native_pause_source_top,0,0);
  bottom=InterlockedCompareExchange(&native_pause_source_bottom,0,0);
  /* Relative bounds from the observed original ribbon, never a level. */
  for(i=0u;i<4u;i++){
    if(!SpiderPauseSource_ValidVertex(v[i].x,v[i].y,v[i].z,v[i].rhw,
        (unsigned)desktop_w,(unsigned)desktop_h) ||
       !(SpiderPauseSource_Abs(v[i].z-v[0].z)<0.00010f) ||
       !(SpiderPauseSource_Abs(v[i].rhw-v[0].rhw)<0.30f) ||
       (bound &&
        (v[i].y<(float)top-(float)desktop_h*0.10f ||
         v[i].y>(float)bottom+(float)desktop_h*0.12f)))return 0;
  }
  memcpy(copy,v,4u*sizeof(*copy));
  for(i=0u;i<4u;i++)
    copy[i].x=SpiderPauseSource_MapPhysicalX(v[i].x,
      (unsigned)desktop_w,(unsigned)desktop_h);
  if(debug_log_enabled){
    LONG n=InterlockedIncrement(&native_pause_source_remaps);
    if(n<=8 || n%240==0){
      char msg[220];
      snprintf(msg,sizeof(msg),
        "PAUSE-SOURCE: original UI physical->4:3 remap n=%ld texture=%ux%u first-x=%.1f->%.1f [ALL LEVELS]",
        (long)n,width,height,(double)v[0].x,(double)copy[0].x);
      log_line(msg);
    }
  }
  return 1;
}
