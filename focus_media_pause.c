/* Spider-Man PC 2001 permanent native Bink focus pause.
 *
 * Static analysis of the supported original EXE: BinkPause(handle, pause)
 * callsite 0x0050BDE5, IAT slot 0x0053B210, cinematic handle 0x00AC0BA4,
 * and audio-only Bink music handle 0x00AC1020. The original game's native
 * pause routine at 0x0050BDD0 calls the same API.
 *
 * Keep Bink playback synchronized at the focus-pause Flip boundary.
 * No Bink loader patch, COM vtable replacement, or synthetic input.
 * Enabled with the fix; calls are guarded by the known original executable.
 * Movie handles may be managed on other threads, so retain handle checks.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "focus_media_pause.h"

#define SPIDER_BINK_CALLSITE ((const BYTE *)(uintptr_t)0x0050BDE5u)
#define SPIDER_BINK_IMPORT ((void * const *)(uintptr_t)0x0053B210u)
#define SPIDER_MOVIE_A ((void * const *)(uintptr_t)0x00AC0BA4u)
#define SPIDER_MOVIE_B ((void * const *)(uintptr_t)0x00AC1020u)
#define SPIDER_BINK_GAME_PAUSE ((const BYTE *)(uintptr_t)0x00AC101Du)

typedef int (WINAPI *BinkPauseFn)(void *movie,int pause);
static BinkPauseFn native_bink_pause=NULL;
static void *paused[2]={NULL,NULL}; /* movie/video plus Bink soundtrack */
static volatile LONG enabled=0;
static volatile LONG in_media_pause=0;
static void (*logger)(const char *)=NULL;
static const BYTE bink_signature[6]={0xFF,0x15,0x10,0xB2,0x53,0x00};

static void diag(const char *message) {
  if(logger) logger(message);
}
/* Read-only game DirectSound device and primary-buffer candidates. This
 * diagnostic is distinct from the optional 32 secondary voice controllers
 * in focus_dsound_pause.c. Never mutate sound interfaces here. */
static void diag_dsound(const char *phase) {
  char line[170];
  void *device=*(void * const *)(uintptr_t)0x006B7920u;
  void *primary=*(void * const *)(uintptr_t)0x006B7924u;
  snprintf(line,sizeof(line),
    "FOCUS-AUDIO: %s DSOUND device=%p primary-candidate=%p; read-only Bink-module probe",
    phase,device,primary);
  diag(line);
}

static void log_movie(const char *phase,int index,void *handle,int result) {
  char line[160];
  snprintf(line,sizeof(line),
    "FOCUS-MEDIA: Bink %s handle[%d]=%p BinkPause-result=%d",
    phase,index,handle,result);
  diag(line);
}
void SpiderFocusMedia_Init(void (*log_fn)(const char *)) {
  HMODULE bink;
  FARPROC exported;
  void *imported=NULL;
  logger=log_fn;
  /* Permanent synchronized focus pause. The retail Bink callsite and
   * native module/export/import checks below still fail closed. */
  /* Caller has already verified supported EXE SHA-256. Compare a genuine
   * native callsite and the decorated stdcall export before using a fixed VA.
   * Do not initialize/hook Bink ourselves or load alternate versions. */
  bink=GetModuleHandleA("binkw32.dll");
  if(!bink || memcmp(SPIDER_BINK_CALLSITE,bink_signature,sizeof(bink_signature))) {
    diag("FOCUS-MEDIA: DISABLED (no Bink module or native callsite mismatch)");
    return;
  }
  exported=GetProcAddress(bink,"_BinkPause@8");
  imported=*SPIDER_BINK_IMPORT;
  if(!exported || imported!=(void *)exported) {
    diag("FOCUS-MEDIA: DISABLED (BinkPause import does not match native export)");
    return;
  }
  memcpy(&native_bink_pause,&exported,sizeof(exported));
  InterlockedExchange(&enabled,1);
  diag("FOCUS-MEDIA: guarded native BinkPause enabled; secondary DirectSound voice handling is separate");
}
void SpiderFocusMedia_Pause(void) {
  void *handles[2],*handle;
  int i,j,duplicate;
  if(InterlockedCompareExchange(&enabled,0,0)!=1 ||
     InterlockedCompareExchange(&in_media_pause,1,0)!=0) return;
  diag_dsound("pause");
  handles[0]=*SPIDER_MOVIE_A;
  handles[1]=*SPIDER_MOVIE_B;
  for(i=0;i<2;i++) {
    handle=handles[i];
    duplicate=0;
    for(j=0;j<i;j++)if(handles[j]==handle)duplicate=1;
    /* Audio-only Bink music has an independent game pause state. Do not unpause
     * a movie the game already paused for its own reasons. */
    if(!handle || duplicate ||
       (i==1 && *SPIDER_BINK_GAME_PAUSE!=0)) continue;
    paused[i]=handle;
    log_movie("pause",i,handle,native_bink_pause(handle,1));
  }
}
void SpiderFocusMedia_Resume(void) {
  void *now[2];
  int i;
  if(InterlockedCompareExchange(&in_media_pause,0,1)!=1) return;
  diag_dsound("resume");
  now[0]=*SPIDER_MOVIE_A;
  now[1]=*SPIDER_MOVIE_B;
  for(i=0;i<2;i++) {
    if(paused[i]) {
      if(now[i]==paused[i] &&
         (i!=1 || *SPIDER_BINK_GAME_PAUSE==0))
        log_movie("resume",i,paused[i],native_bink_pause(paused[i],0));
      else diag("FOCUS-MEDIA: movie replaced or game-native pause active; skip Bink resume");
      paused[i]=NULL;
    }
  }
}
