/* Spider-Man 2001 / Wine Alt+Tab focus trace and guarded input recovery.
 * [Input] ExperimentalFocusTrace=1 observes DirectInput8 calls.
 * [Input] ExperimentalAltTabReacquire=1 enables a tightly guarded optional
 * reacquisition only after a confirmed focus loss/return, on a failed read.
 * Never modifies cooperative levels, window activation, or input events.
 */
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "input_focus_trace.h"

typedef HRESULT (WINAPI *DICreate8Fn)(HINSTANCE,DWORD,REFIID,LPVOID *,LPUNKNOWN);
typedef HRESULT (WINAPI *DIInterfaceCreateDeviceFn)(void *,REFGUID,void **,LPUNKNOWN);
typedef HRESULT (WINAPI *DIGetStateFn)(void *,DWORD,LPVOID);
typedef HRESULT (WINAPI *DIGetDataFn)(void *,DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD);
typedef HRESULT (WINAPI *DISetCoopFn)(void *,HWND,DWORD);
typedef HRESULT (WINAPI *DIOneArgFn)(void *);
typedef ULONG (WINAPI *DIReleaseFn)(void *);

#define FOCUS_MAX_DEVICES 16
#define FOCUS_DIDEVICE_SLOTS 32

typedef struct {
  void *object;
  void *vt[FOCUS_DIDEVICE_SLOTS];
  DIReleaseFn release;
  DIOneArgFn acquire,unacquire,poll;
  DIGetStateFn state;
  DIGetDataFn data;
  DISetCoopFn coop;
  char label[20];
  LONG acquires,unacquires,state_calls,data_calls,errors,polls;
  DWORD last_state_hr,last_data_hr;
  LONG last_report_epoch;
  LONG last_acquire_log_epoch,last_unacquire_log_epoch;
  LONG fix_busy,fix_generation,fix_attempts,fix_successes,fix_retry_ok;
  DWORD fix_last_tick,fix_last_acquire_hr;
  LONG explicit_unacquire_generation;
} FocusDevice;

static void (*log_msg)(const char *);
static volatile LONG diagnostic_active=0;
static volatile LONG reacquire_enabled=0;
/* Only the diagnostic snapshot needs text formatting; input recovery stays active. */
static int input_debug_logging=0;
/* Tracks WM_SETFOCUS after a real WM_KILLFOCUS, not mere foreground guesses. */
static volatile LONG focus_return_generation=0;
static volatile LONG focus_has_returned=0;
static volatile LONG focus_lost_once=0;
static DICreate8Fn original_directinput8_create=NULL;
static DIInterfaceCreateDeviceFn original_interface_create=NULL;
static void *interface_vtable[11];
static FocusDevice devices[FOCUS_MAX_DEVICES];
static LONG devices_used=0;
static HWND tracked_window=NULL;
static WNDPROC previous_proc=NULL;
static LONG focus_epoch=0;
static LONG foreground_events=0,background_events=0;
static LONG focus_key_messages=0,focus_mouse_messages=0;
static volatile LONG last_focus_foreground=-1;
static DWORD last_winmsg_sample=0;
/* Desktop cursor recovery defaults ON (configurable in [Input]).
 * Taskbar Z-order experiment remains strictly opt-in.
 * Normal cursor-only tracking does not subclass WndProc or hook DirectInput. */
static volatile LONG cursor_visibility_test=0, zorder_test=0;
static volatile LONG cursor_only_mode=0;
static LONG cursor_display_increments=0;

static void focus_log(const char *msg) { if(log_msg) log_msg(msg); }
static FocusDevice *focus_device(void *self) {
  LONG i;
  for(i=0;i<devices_used;i++) if(devices[i].object==self) return &devices[i];
  return NULL;
}
static const char *focus_hr(HRESULT hr) {
  if(hr==DI_OK) return "DI_OK";
  if(hr==DIERR_INPUTLOST) return "DIERR_INPUTLOST";
  if(hr==DIERR_NOTACQUIRED) return "DIERR_NOTACQUIRED";
  if(hr==DIERR_OTHERAPPHASPRIO) return "DIERR_OTHERAPPHASPRIO";
  return "other";
}
static const char *focus_message(UINT m) {
  switch(m) {
    case WM_ACTIVATEAPP:return "WM_ACTIVATEAPP";
    case WM_ACTIVATE:return "WM_ACTIVATE";
    case WM_SETFOCUS:return "WM_SETFOCUS";
    case WM_KILLFOCUS:return "WM_KILLFOCUS";
    case WM_NCACTIVATE:return "WM_NCACTIVATE";
    case WM_SYSKEYDOWN:return "WM_SYSKEYDOWN";
    case WM_KEYDOWN:return "WM_KEYDOWN";
    case WM_MOUSEMOVE:return "WM_MOUSEMOVE";
    case WM_LBUTTONDOWN:return "WM_LBUTTONDOWN";
    case WM_RBUTTONDOWN:return "WM_RBUTTONDOWN";
    case WM_SIZE:return "WM_SIZE";
    case WM_DISPLAYCHANGE:return "WM_DISPLAYCHANGE";
    case WM_WINDOWPOSCHANGED:return "WM_WINDOWPOSCHANGED";
    default:return "other";
  }
}
static void focus_report_window(const char *tag) {
  char line[300];HWND fg=GetForegroundWindow();
  HWND window=tracked_window;
  DWORD tid=window?GetWindowThreadProcessId(window,NULL):0;
  snprintf(line,sizeof(line),
    "INPUT-FOCUS: %s hwnd=%p foreground=%p active=%p game-is-foreground=%d focus-on-game-thread=%p window-thread=%lu epoch=%ld",
    tag,(void*)window,(void*)fg,(void*)GetActiveWindow(),fg==window,
    (void*)GetFocus(),(unsigned long)tid,(long)focus_epoch);
  focus_log(line);
}
static LRESULT CALLBACK focus_window_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  if(msg==WM_ACTIVATEAPP || msg==WM_ACTIVATE || msg==WM_SETFOCUS ||
     msg==WM_KILLFOCUS || msg==WM_NCACTIVATE) {
    char line[240];
    InterlockedIncrement(&focus_epoch);
    if(msg==WM_KILLFOCUS || (msg==WM_ACTIVATEAPP && wp==FALSE)) {
      InterlockedExchange(&focus_lost_once,1);
      InterlockedExchange(&focus_has_returned,0);
    }
    if(msg==WM_SETFOCUS && InterlockedCompareExchange(&focus_lost_once,0,0)==1) {
      InterlockedIncrement(&focus_return_generation);
      InterlockedExchange(&focus_has_returned,1);
    }
    snprintf(line,sizeof(line),"INPUT-FOCUS: event=%s wp=0x%08lX lp=0x%08lX fg=%p hwnd=%p epoch=%ld",
      focus_message(msg),(unsigned long)wp,(unsigned long)lp,
      (void*)GetForegroundWindow(),(void*)hwnd,(long)focus_epoch);
    focus_log(line);
    focus_report_window("window-event");
  } else if(msg==WM_SIZE || msg==WM_DISPLAYCHANGE || msg==WM_WINDOWPOSCHANGED) {
    /* Passive snapshot around Alt+Tab; no window size or focus changes
     * are performed by this diagnostic observer. */
    RECT window={0},client={0};
    char line[260];
    GetWindowRect(hwnd,&window);
    GetClientRect(hwnd,&client);
    snprintf(line,sizeof(line),
      "INPUT-FOCUS: window-geometry event=%s wp=0x%08lX screen-rect=(%ld,%ld)+%ldx%ld client=%ldx%ld iconic=%d foreground=%d",
      focus_message(msg),(unsigned long)wp,
      (long)window.left,(long)window.top,
      (long)(window.right-window.left),(long)(window.bottom-window.top),
      (long)(client.right-client.left),(long)(client.bottom-client.top),
      (int)IsIconic(hwnd),GetForegroundWindow()==hwnd);
    focus_log(line);
  } else if(msg==WM_SYSKEYDOWN || msg==WM_KEYDOWN ||
            msg==WM_MOUSEMOVE || msg==WM_LBUTTONDOWN || msg==WM_RBUTTONDOWN) {
    LONG n;DWORD now=GetTickCount();
    if(msg==WM_MOUSEMOVE) n=InterlockedIncrement(&focus_mouse_messages);
    else n=InterlockedIncrement(&focus_key_messages);
    /* Message samples are bounded; mouse movement is never logged at 30 Hz. */
    if(n<=4 || (now-last_winmsg_sample)>=10000UL) {
      char line[180];
      last_winmsg_sample=now;
      snprintf(line,sizeof(line),"INPUT-FOCUS: window-input event=%s value=0x%lX count=%ld foreground=%d",
        focus_message(msg),(unsigned long)wp,(long)n,GetForegroundWindow()==hwnd);
      focus_log(line);
    }
  }
  return previous_proc ? CallWindowProcA(previous_proc,hwnd,msg,wp,lp)
                       : DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderInputFocus_AttachWindow(HWND hwnd) {
  WNDPROC old;char line[180];DWORD error;
  if(!diagnostic_active || !hwnd || !IsWindow(hwnd)) return;
  if(InterlockedCompareExchange(&cursor_only_mode,0,0)==1) {
    if(tracked_window!=hwnd) {
      tracked_window=hwnd;
      focus_log("INPUT-CURSOR: tracking game HWND only (no WndProc subclass or DirectInput hooks)");
    }
    return;
  }
  if(tracked_window==hwnd && previous_proc) return;
  if(tracked_window && tracked_window!=hwnd && IsWindow(tracked_window) &&
     previous_proc && (WNDPROC)GetWindowLongPtrA(tracked_window,GWLP_WNDPROC)==focus_window_proc) {
    SetWindowLongPtrA(tracked_window,GWLP_WNDPROC,(LONG_PTR)previous_proc);
  }
  tracked_window=hwnd;
  previous_proc=NULL;
  SetLastError(0);
  old=(WNDPROC)SetWindowLongPtrA(hwnd,GWLP_WNDPROC,(LONG_PTR)focus_window_proc);
  error=GetLastError();
  if(!old && error) {
    snprintf(line,sizeof(line),"INPUT-FOCUS: WARNING WndProc subclass failed error=%lu",(unsigned long)error);
    focus_log(line);return;
  }
  previous_proc=old;
  snprintf(line,sizeof(line),"INPUT-FOCUS: window subclass installed hwnd=%p oldproc=%p",
    (void*)hwnd,(void*)old);
  focus_log(line);
  focus_report_window("window-attached");
}

static void focus_log_device_call(FocusDevice *d,const char *action,HRESULT hr,LONG count) {
  char line[240];
  snprintf(line,sizeof(line),"INPUT-DI8: device=%s object=%p %s #%ld hr=0x%08lX (%s) fg=%d epoch=%ld",
    d->label,d->object,action,(long)count,(unsigned long)hr,focus_hr(hr),
    GetForegroundWindow()==tracked_window,(long)focus_epoch);
  focus_log(line);
}
static ULONG WINAPI focus_release(void *self) {
  FocusDevice *d=focus_device(self);ULONG refs;
  if(!d || !d->release) return 0;
  refs=d->release(self);
  if(!refs) {
    char line[160];
    snprintf(line,sizeof(line),"INPUT-DI8: device=%s Release final (object=%p)",d->label,self);
    focus_log(line);
    d->object=NULL; /* tracker can be reused after COM destruction */
  }
  return refs;
}
static HRESULT WINAPI focus_acquire(void *self) {
  FocusDevice *d=focus_device(self);HRESULT hr;
  if(!d || !d->acquire) return E_FAIL;
  hr=d->acquire(self);
  { LONG n=InterlockedIncrement(&d->acquires);
    if(n<=10 || n%120==0 || focus_epoch!=d->last_acquire_log_epoch)
      focus_log_device_call(d,"Acquire",hr,n);
    d->last_acquire_log_epoch=focus_epoch;
  }
  return hr;
}
static HRESULT WINAPI focus_unacquire(void *self) {
  FocusDevice *d=focus_device(self);HRESULT hr;
  if(!d || !d->unacquire) return E_FAIL;
  hr=d->unacquire(self);
  /* Respect any explicit game Unacquire in this focus generation. */
  d->explicit_unacquire_generation=InterlockedCompareExchange(&focus_return_generation,0,0);
  { LONG n=InterlockedIncrement(&d->unacquires);
    if(n<=10 || n%120==0 || focus_epoch!=d->last_unacquire_log_epoch)
      focus_log_device_call(d,"Unacquire",hr,n);
    d->last_unacquire_log_epoch=focus_epoch;
  }
  return hr;
}
static HRESULT WINAPI focus_set_coop(void *self,HWND hwnd,DWORD flags) {
  FocusDevice *d=focus_device(self);HRESULT hr;char line[220];
  if(!d || !d->coop) return E_FAIL;
  hr=d->coop(self,hwnd,flags);
  snprintf(line,sizeof(line),
    "INPUT-DI8: device=%s SetCooperativeLevel hwnd=%p flags=0x%08lX foreground=%d exclusive=%d hr=0x%08lX (%s)",
    d->label,(void*)hwnd,(unsigned long)flags,!!(flags&DISCL_FOREGROUND),
    !!(flags&DISCL_EXCLUSIVE),(unsigned long)hr,focus_hr(hr));
  focus_log(line);
  return hr;
}
/* Only retry a genuinely failed read after WM_SETFOCUS following a loss.
 * One initial attempt per device/return; if the driver temporarily refuses,
 * at most 5 retries with >= 700 ms delay. Never Acquire in the background.
 * This runs at the game's own DirectInput polling site, not WndProc/Flip.
 */
static HRESULT focus_recover_read(FocusDevice *d,void *self,HRESULT failed,
                                  int data_call,DWORD cb,void *buffer,
                                  LPDWORD elements,DWORD flags,
                                  DWORD requested_elements) {
  LONG gen,attempt;DWORD now;HRESULT acq,next;char line[300];
  if(InterlockedCompareExchange(&reacquire_enabled,0,0)!=1 ||
     (failed!=DIERR_NOTACQUIRED && failed!=DIERR_INPUTLOST) ||
     InterlockedCompareExchange(&focus_has_returned,0,0)!=1 ||
     !tracked_window || !IsWindow(tracked_window) ||
     GetForegroundWindow()!=tracked_window ||
     (strcmp(d->label,"keyboard")!=0 && strcmp(d->label,"mouse")!=0)) return failed;
  gen=InterlockedCompareExchange(&focus_return_generation,0,0);
  if(gen<=0 || d->explicit_unacquire_generation==gen) return failed;
  if(InterlockedCompareExchange(&d->fix_busy,1,0)!=0) return failed;
  now=GetTickCount();
  if(d->fix_generation!=gen) {
    d->fix_generation=gen;d->fix_attempts=0;d->fix_last_tick=0;
  }
  if(d->fix_attempts>=5 || (d->fix_attempts>0 &&
     (DWORD)(now-d->fix_last_tick)<700UL)) {
    InterlockedExchange(&d->fix_busy,0);return failed;
  }
  attempt=++d->fix_attempts;
  d->fix_last_tick=now;
  /* Guard against an Alt+Tab happening between the eligibility check and call. */
  if(GetForegroundWindow()!=tracked_window ||
     InterlockedCompareExchange(&focus_has_returned,0,0)!=1) {
    InterlockedExchange(&d->fix_busy,0);return failed;
  }
  acq=d->acquire(self);
  d->fix_last_acquire_hr=(DWORD)acq;
  if(SUCCEEDED(acq)) {
    InterlockedIncrement(&d->fix_successes);
    /* A failed GetDeviceData may change its in/out count. Restore the caller's
       original buffer capacity before retrying the very same read. */
    if(data_call && elements) *elements=requested_elements;
    next=data_call?d->data(self,cb,(LPDIDEVICEOBJECTDATA)buffer,elements,flags):
                   d->state(self,cb,buffer);
    if(SUCCEEDED(next)) InterlockedIncrement(&d->fix_retry_ok);
  } else next=failed;
  snprintf(line,sizeof(line),
    "INPUT-FIX: reacquire device=%s generation=%ld attempt=%ld original=%s acquire=0x%08lX (%s) retry=0x%08lX (%s) foreground=%d",
    d->label,(long)gen,(long)attempt,focus_hr(failed),
    (unsigned long)acq,focus_hr(acq),(unsigned long)next,focus_hr(next),
    GetForegroundWindow()==tracked_window);
  focus_log(line);
  InterlockedExchange(&d->fix_busy,0);
  return next;
}
static HRESULT WINAPI focus_get_state(void *self,DWORD cb,LPVOID data) {
  FocusDevice *d=focus_device(self);HRESULT hr;LONG count,epoch;
  if(!d || !d->state) return E_FAIL;
  hr=d->state(self,cb,data);
  hr=focus_recover_read(d,self,hr,0,cb,data,NULL,0,0);
  count=InterlockedIncrement(&d->state_calls);epoch=focus_epoch;
  d->last_state_hr=(DWORD)hr;
  if(FAILED(hr)) InterlockedIncrement(&d->errors);
  if(count<=3 || (FAILED(hr) && (count<=12 || count%6000==0)) ||
      (epoch!=d->last_report_epoch && GetForegroundWindow()==tracked_window)) {
    focus_log_device_call(d,"GetDeviceState",hr,count);
    d->last_report_epoch=epoch;
  }
  return hr;
}
static HRESULT WINAPI focus_get_data(void *self,DWORD cb,LPDIDEVICEOBJECTDATA data,LPDWORD elements,DWORD flags) {
  FocusDevice *d=focus_device(self);HRESULT hr;LONG count,epoch;
  DWORD requested_elements=elements?*elements:0;
  if(!d || !d->data) return E_FAIL;
  hr=d->data(self,cb,data,elements,flags);
  hr=focus_recover_read(d,self,hr,1,cb,data,elements,flags,requested_elements);
  count=InterlockedIncrement(&d->data_calls);epoch=focus_epoch;
  d->last_data_hr=(DWORD)hr;
  if(FAILED(hr)) InterlockedIncrement(&d->errors);
  if(count<=3 || (FAILED(hr) && (count<=12 || count%6000==0)) ||
      (epoch!=d->last_report_epoch && GetForegroundWindow()==tracked_window)) {
    focus_log_device_call(d,"GetDeviceData",hr,count);
    d->last_report_epoch=epoch;
  }
  return hr;
}
static HRESULT WINAPI focus_poll(void *self) {
  FocusDevice *d=focus_device(self);HRESULT hr;LONG count;
  if(!d || !d->poll) return E_FAIL;
  hr=d->poll(self);
  count=InterlockedIncrement(&d->polls);
  if(count<=2 || (FAILED(hr) && (count<=12 || count%60==0)))
    focus_log_device_call(d,"Poll",hr,count);
  return hr;
}
static void focus_instrument_device(void *obj,REFGUID guid) {
  void ***object=(void ***)obj;void **vt;FocusDevice *d;char line[190];
  if(!obj || !object || !*object) return;
  vt=*object;
  {
    LONG k;
    for(k=0;k<devices_used;k++) {
      if(devices[k].object==obj) return; /* already instrumented */
      if(!devices[k].object) break; /* recycled released slot */
    }
    if(k==FOCUS_MAX_DEVICES) {
      focus_log("INPUT-DI8: WARNING device tracker limit reached, subsequent devices forwarded unchanged");
      return;
    }
    if(k==devices_used) ++devices_used;
    d=&devices[k];
    memset(d,0,sizeof(*d));
  }
  d->object=obj;
  if(guid && IsEqualGUID(guid,&GUID_SysKeyboard)) strcpy(d->label,"keyboard");
  else if(guid && IsEqualGUID(guid,&GUID_SysMouse)) strcpy(d->label,"mouse");
  else snprintf(d->label,sizeof(d->label),"other-%ld",(long)devices_used);
  d->release=(DIReleaseFn)vt[2];
  d->acquire=(DIOneArgFn)vt[7];d->unacquire=(DIOneArgFn)vt[8];
  d->state=(DIGetStateFn)vt[9];d->data=(DIGetDataFn)vt[10];
  d->coop=(DISetCoopFn)vt[13];d->poll=(DIOneArgFn)vt[25];
  if(!d->release || !d->acquire || !d->unacquire || !d->state || !d->data || !d->coop) {
    focus_log("INPUT-DI8: WARNING invalid device vtable, no instrumentation");return;
  }
  memcpy(d->vt,vt,sizeof(d->vt));
  d->vt[2]=(void*)focus_release;
  d->vt[7]=(void*)focus_acquire;d->vt[8]=(void*)focus_unacquire;
  d->vt[9]=(void*)focus_get_state;d->vt[10]=(void*)focus_get_data;
  d->vt[13]=(void*)focus_set_coop;
  if(d->poll) d->vt[25]=(void*)focus_poll;
  *object=d->vt;
  snprintf(line,sizeof(line),"INPUT-DI8: hooked device=%s object=%p (observe only)",
    d->label,obj);
  focus_log(line);
}
static HRESULT WINAPI focus_create_device(void *self,REFGUID guid,void **out,LPUNKNOWN outer) {
  HRESULT hr;char line[200];
  if(!original_interface_create) return E_FAIL;
  hr=original_interface_create(self,guid,out,outer);
  snprintf(line,sizeof(line),"INPUT-DI8: IDirectInput8::CreateDevice hr=0x%08lX object=%p",
    (unsigned long)hr,(void*)(out?*out:NULL));
  focus_log(line);
  if(SUCCEEDED(hr) && out && *out) focus_instrument_device(*out,guid);
  return hr;
}
static HRESULT WINAPI focus_directinput8_create(HINSTANCE h,DWORD version,REFIID iid,LPVOID *out,LPUNKNOWN outer) {
  HRESULT hr;char line[200];
  if(!original_directinput8_create) return E_FAIL;
  hr=original_directinput8_create(h,version,iid,out,outer);
  snprintf(line,sizeof(line),"INPUT-DI8: DirectInput8Create version=0x%08lX hr=0x%08lX object=%p",
    (unsigned long)version,(unsigned long)hr,(void*)(out?*out:NULL));
  focus_log(line);
  if(SUCCEEDED(hr) && out && *out) {
    void ***object=(void ***)*out;void **vt=*object;
    if(!original_interface_create) {
      memcpy(interface_vtable,vt,sizeof(interface_vtable));
      original_interface_create=(DIInterfaceCreateDeviceFn)vt[3];
      interface_vtable[3]=(void*)focus_create_device;
    }
    if(vt[3]==(void*)original_interface_create) *object=interface_vtable;
    else focus_log("INPUT-DI8: WARNING unexpected IDirectInput8 vtable; not instrumented");
  }
  return hr;
}
void SpiderInputFocus_Install(void (*log_fn)(const char *)) {
  char exe[MAX_PATH],ini[MAX_PATH],*sep,line[200];DWORD n;
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;IMAGE_NT_HEADERS32 *nt;
  void **slot;HMODULE dll;FARPROC target;DWORD old,ignored;LONG want_trace,want_fix,want_cursor;
  log_msg=log_fn;
  n=GetModuleFileNameA(NULL,exe,sizeof(exe));
  if(!n || n>=sizeof(exe) || !(sep=strrchr(exe,'\\'))) return;
  strcpy(ini,exe);strcpy(ini+(sep-exe)+1,"SpiderMan2k1Fix.ini");
  input_debug_logging=GetPrivateProfileIntA("Debug","Log",0,ini)==1;
  want_trace=GetPrivateProfileIntA("Input","ExperimentalFocusTrace",0,ini)==1;
  want_fix=GetPrivateProfileIntA("Input","ExperimentalAltTabReacquire",0,ini)==1;
  /* Default 1 also covers pre-existing user INIs with no such key. */
  want_cursor=GetPrivateProfileIntA("Input","RestoreCursorOnFocusLoss",1,ini)==1;
  InterlockedExchange(&cursor_visibility_test,want_cursor);
  /* This explicit diagnostic switch still provides WndProc focus/geometry
   * snapshots without patching the DirectInput8 import or COM vtables.
   * Cursor visibility follows the INI even in this diagnostic mode;
   * SPIDERMAN2K1FIX_ZORDER_TEST stays separate and opt-in.
   */
  {
    char passive[4]={0},zorder[4]={0};
    DWORD nbytes=GetEnvironmentVariableA("SPIDERMAN2K1FIX_FOCUS_PASSIVE",
                                          passive,sizeof(passive));
    if(nbytes==1 && passive[0]=='1') {
      DWORD nz=GetEnvironmentVariableA("SPIDERMAN2K1FIX_ZORDER_TEST",
                                        zorder,sizeof(zorder));
      InterlockedExchange(&zorder_test,nz==1 && zorder[0]=='1');
      InterlockedExchange(&reacquire_enabled,0);
      InterlockedExchange(&diagnostic_active,1);
      focus_log("INPUT-FOCUS: PASSIVE ONLY - WndProc events/Flip snapshots active; DirectInput8 hook NOT installed");
      snprintf(line,sizeof(line),"INPUT-WINDOW: settings cursor=%ld zorder=%ld (DirectInput hooks OFF)",
        (long)cursor_visibility_test,(long)zorder_test);
      focus_log(line);
      return;
    }
  }
  /* Safe normal path: use the game's existing DirectDraw Flip boundary
   * to detect background/foreground. No WndProc subclass, DirectInput
   * hook, focus events, synthetic input, or window Z-order modification. */
  if(!want_trace && !want_fix && want_cursor) {
    InterlockedExchange(&reacquire_enabled,0);
    InterlockedExchange(&cursor_only_mode,1);
    InterlockedExchange(&diagnostic_active,1);
    focus_log("INPUT-CURSOR: automatic desktop cursor restore ENABLED (INI; Flip-only, no input hooks)");
    return;
  }
  if(!want_trace && !want_fix) {
    focus_log("INPUT-CURSOR: RestoreCursorOnFocusLoss=0; automatic cursor restore disabled");
    return;
  }
  InterlockedExchange(&reacquire_enabled,want_fix);
  snprintf(line,sizeof(line),
    "INPUT-FIX: ExperimentalAltTabReacquire=%ld ExperimentalFocusTrace=%ld (guarded recovery after lost/returned focus)",
    (long)want_fix,(long)want_trace);
  focus_log(line);
  if(!base) return;
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 || dos->e_lfanew>0x1000) return;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     (uintptr_t)base!=0x400000 || nt->OptionalHeader.ImageBase!=0x400000 ||
     nt->OptionalHeader.SizeOfImage<=0x13b020) {
    focus_log("INPUT-FOCUS: unsupported loaded EXE layout; diagnostic disabled");return;
  }
  InterlockedExchange(&diagnostic_active,1);
  slot=(void**)(base+0x13b01c); /* Supported EXE's verified DirectInput8Create IAT RVA. */
  dll=GetModuleHandleA("dinput8.dll");
  target=dll?GetProcAddress(dll,"DirectInput8Create"):NULL;
  if(!target || *slot!=(void*)target) {
    focus_log("INPUT-DI8: WARNING IAT target missing/mismatch; focus messages only");return;
  }
  original_directinput8_create=(DICreate8Fn)*slot;
  if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old)) {
    focus_log("INPUT-DI8: WARNING IAT VirtualProtect failed; focus messages only");return;
  }
  *slot=(void*)focus_directinput8_create;
  VirtualProtect(slot,sizeof(*slot),old,&ignored);
  snprintf(line,sizeof(line),"INPUT-DI8: DirectInput8Create IAT hooked rva=0x13B01C (reacquire=%ld; no synthetic input)",(long)want_fix);
  focus_log(line);
}
/* Passive samples on actual foreground transitions: distinguish visible
 * cursor from hidden cursor and focus from Z-order without forced activation.
 * Cursor repair follows RestoreCursorOnFocusLoss (on by default).
 * Z-order modification remains an explicit diagnostic A/B experiment.
 */
static void focus_probe_window_system(const char *stage) {
  CURSORINFO ci;
  RECT clip={0,0,0,0};
  HWND above;
  char line[420];
  BOOL ok_cursor,ok_clip;
  LONG_PTR exstyle;
  if(!tracked_window || !IsWindow(tracked_window)) return;
  memset(&ci,0,sizeof(ci));ci.cbSize=sizeof(ci);
  ok_cursor=GetCursorInfo(&ci);
  ok_clip=GetClipCursor(&clip);
  above=GetWindow(tracked_window,GW_HWNDPREV);
  exstyle=GetWindowLongPtrA(tracked_window,GWL_EXSTYLE);
  snprintf(line,sizeof(line),
    "INPUT-WINDOW: %s fg=%d above=%p topmost=%d visible=%d iconic=%d cursor-ok=%d cursor-showing=%d cursor-handle=%p clip-ok=%d clip=(%ld,%ld)-(%ld,%ld) cursor-balance=%ld",
    stage,(int)(GetForegroundWindow()==tracked_window),(void*)above,
    (int)!!(exstyle&WS_EX_TOPMOST),(int)IsWindowVisible(tracked_window),
    (int)IsIconic(tracked_window),(int)ok_cursor,
    (int)(ok_cursor && !!(ci.flags&CURSOR_SHOWING)),
    ok_cursor?(void*)ci.hCursor:NULL,(int)ok_clip,
    (long)clip.left,(long)clip.top,(long)clip.right,(long)clip.bottom,
    (long)cursor_display_increments);
  focus_log(line);
}
static void focus_window_transition(int foreground) {
  LONG i;
  int result=0;
  char line[220];
  focus_probe_window_system(foreground?"foreground-before":"background-before");
  if(!foreground && InterlockedCompareExchange(&cursor_visibility_test,0,0)==1) {
    CURSORINFO ci;
    memset(&ci,0,sizeof(ci));ci.cbSize=sizeof(ci);
    if(cursor_display_increments==0 && GetCursorInfo(&ci) &&
       !(ci.flags&CURSOR_SHOWING)) {
      /* ShowCursor uses a display counter; balance each increment exactly.
       * A bounded 16-call limit avoids indefinite loops on unusual drivers.
       * Never change pointer visibility unless the real cursor is hidden.
       */
      for(i=0;i<16;i++) {
        result=ShowCursor(TRUE);
        ++cursor_display_increments;
        if(result>=0) break;
      }
      snprintf(line,sizeof(line),
        "INPUT-WINDOW: CURSOR-TEST background ShowCursor(TRUE) calls=%ld final-count=%d",
        cursor_display_increments,result);
      focus_log(line);
    }
  }
  if(foreground && cursor_display_increments>0) {
    LONG balanced=cursor_display_increments;
    for(i=0;i<balanced;i++) result=ShowCursor(FALSE);
    cursor_display_increments=0;
    snprintf(line,sizeof(line),
      "INPUT-WINDOW: CURSOR-TEST foreground balanced ShowCursor(FALSE) calls=%ld final-count=%d",
      balanced,result);
    focus_log(line);
  }
  if(foreground && InterlockedCompareExchange(&zorder_test,0,0)==1 &&
     GetForegroundWindow()==tracked_window && IsWindow(tracked_window) &&
     IsWindowVisible(tracked_window) && !IsIconic(tracked_window)) {
    BOOL ok=SetWindowPos(tracked_window,HWND_TOP,0,0,0,0,
      SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    snprintf(line,sizeof(line),
      "INPUT-WINDOW: ZORDER-TEST foreground HWND_TOP ok=%d Win32=%lu (no topmost, no background activation)",
      (int)ok,(unsigned long)(ok?0:GetLastError()));
    focus_log(line);
  }
  focus_probe_window_system(foreground?"foreground-after":"background-after");
}
void SpiderInputFocus_OnFlip(LONG flip_number) {
  HWND fg;LONG foreground,previous;char line[320];LONG i;
  if(!diagnostic_active || !tracked_window) return;
  fg=GetForegroundWindow();foreground=(fg==tracked_window)?1:0;
  previous=InterlockedExchange(&last_focus_foreground,foreground);
  if(foreground!=previous) {
    if(foreground) InterlockedIncrement(&foreground_events);
    else InterlockedIncrement(&background_events);
    focus_report_window(foreground?"flip-foreground":"flip-background");
    if(previous!=-1) focus_window_transition((int)foreground);
    else focus_probe_window_system("initial"); /* No startup mutation. */
  }
  if(!input_debug_logging || (flip_number>3 && flip_number%120!=0)) return;
  snprintf(line,sizeof(line),
    "INPUT-FOCUS: SNAPSHOT flip=%ld foreground=%d epoch=%ld focus-return=%ld focus-loss=%ld keyboard-messages=%ld mouse-messages=%ld tracked-devices=%ld",
    (long)flip_number,(int)foreground,(long)focus_epoch,
    (long)foreground_events,(long)background_events,
    (long)focus_key_messages,(long)focus_mouse_messages,(long)devices_used);
  focus_log(line);
  for(i=0;i<devices_used;i++) {
    FocusDevice *d=&devices[i];
    if(!d->object) continue;
    snprintf(line,sizeof(line),
      "INPUT-DI8: SNAPSHOT device=%s acquired=%ld unacquired=%ld GetDeviceState=%ld last=0x%08lX GetDeviceData=%ld last=0x%08lX Poll=%ld errors=%ld fix-attempts=%ld fix-acquire-ok=%ld fix-retry-ok=%ld",
      d->label,(long)d->acquires,(long)d->unacquires,(long)d->state_calls,
      (unsigned long)d->last_state_hr,(long)d->data_calls,(unsigned long)d->last_data_hr,
      (long)d->polls,(long)d->errors,(long)d->fix_attempts,
      (long)d->fix_successes,(long)d->fix_retry_ok);
    focus_log(line);
  }
}
