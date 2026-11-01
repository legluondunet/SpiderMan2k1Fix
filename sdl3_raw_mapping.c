/* SDL3 universal raw joystick recognition wizard. No model VID/PID table.
 * Runs in the existing SDL event-thread modal, keyboard-accessible.
 * Each verified raw GUID binding is persisted independently; no original
 * game DirectInput state or executable code is touched.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdl3_raw_mapping.h"
#include "sdl3_raw_mapping_policy.h"
#include "spider_fix_paths.h"

typedef struct { uint8_t data[16]; } SpiderSDLGuid;
typedef uint32_t *(__cdecl *IdsFn)(int *);
typedef bool (__cdecl *IsGamepadFn)(uint32_t);
typedef const char *(__cdecl *NameFn)(uint32_t);
typedef void *(__cdecl *OpenFn)(uint32_t);
typedef void (__cdecl *CloseFn)(void *);
typedef int (__cdecl *CountFn)(void *);
typedef bool (__cdecl *ConnectedFn)(void *);
typedef bool (__cdecl *ButtonFn)(void *,int);
typedef int16_t (__cdecl *AxisFn)(void *,int);
typedef uint8_t (__cdecl *HatFn)(void *,int);
typedef SpiderSDLGuid (__cdecl *GuidFn)(uint32_t);
typedef void (__cdecl *GuidStringFn)(SpiderSDLGuid,char *,int);
typedef int (__cdecl *AddMappingFn)(const char *);
typedef void (__cdecl *FreeFn)(void *);
typedef void (__cdecl *PumpFn)(void);

static IdsFn get_ids;
static IsGamepadFn is_gamepad;
static NameFn get_name;
static OpenFn open_joy;
static CloseFn close_joy;
static CountFn num_buttons,num_axes,num_hats;
static ConnectedFn connected;
static ButtonFn get_button;
static AxisFn get_axis;
static HatFn get_hat;
static GuidFn get_guid;
static GuidStringFn guid_string;
static AddMappingFn add_mapping;
static FreeFn free_sdl;
static PumpFn pump,update_joysticks;
static void (*logger)(const char *);
static int ready;
static char active_store[MAX_PATH];

static void note(const char *s){if(logger)logger(s);}
static int store_paths(char *game,char *local) {
  char g[MAX_PATH],l[MAX_PATH];
  if(!spider_path_roots_a(g,sizeof(g),l,sizeof(l)))return 0;
  if(!spider_path_join_a(game,MAX_PATH,g,"SDL3-gamepad-mappings.txt"))return 0;
  if(l[0] && spider_path_join_a(local,MAX_PATH,l,"SDL3-gamepad-mappings.txt"))
    return 1;
  local[0]=0;
  return 1;
}
static void choose_store(void) {
  char game[MAX_PATH],local[MAX_PATH],parent[MAX_PATH];
  const char *path=NULL;
  if(!store_paths(game,local))return;
  /* Never silently overwrite a pre-existing user mapping in a different
   * root: the same read path is always used for subsequent writes. */
  if(spider_path_file_a(game))path=game;
  else if(local[0] && spider_path_file_a(local))path=local;
  else {
    if(spider_path_roots_a(parent,sizeof(parent),NULL,0) &&
       spider_path_dir_a(parent,1) && spider_path_writable_a(parent))
      path=game;
    else {
      char scratch[MAX_PATH];
      if(spider_path_roots_a(scratch,sizeof(scratch),parent,sizeof(parent)) &&
         spider_path_dir_a(parent,1) && spider_path_writable_a(parent))
        path=local;
    }
  }
  if(path)snprintf(active_store,sizeof(active_store),"%s",path);
}
static int line_guid_valid(const char *line) {
  unsigned i;
  if(!line || strlen(line)<35u || line[32]!=',')return 0;
  for(i=0;i<32u;i++)
    if(!((line[i]>='0' && line[i]<='9') ||
         (line[i]>='a' && line[i]<='f') ||
         (line[i]>='A' && line[i]<='F')))return 0;
  return 1;
}
static int save_mapping(const char *mapping) {
  FILE *src=NULL,*dst=NULL;
  char tmp[MAX_PATH+64],line[1024];
  int rc=0,seen=0,n;
  if(!active_store[0] || !line_guid_valid(mapping))return 0;
  n=snprintf(tmp,sizeof(tmp),"%s-%lu.tmp",active_store,
             (unsigned long)GetCurrentProcessId());
  if(n<=0 || (size_t)n>=sizeof(tmp))return 0;
  src=fopen(active_store,"rb");
  dst=fopen(tmp,"wb");
  if(!dst){if(src)fclose(src);return 0;}
  if(src) {
    while(fgets(line,sizeof(line),src)) {
      size_t len=strlen(line);
      if(len && line[len-1]!='\n' && !feof(src))goto cleanup;
      if(!line_guid_valid(line))continue;
      if(!_strnicmp(line,mapping,32u)){seen=1;continue;}
      if(fputs(line,dst)==EOF)goto cleanup;
      if(len && line[len-1]!='\n' && fputc('\n',dst)==EOF)goto cleanup;
    }
    if(ferror(src))goto cleanup;
  }
  if(fputs(mapping,dst)==EOF || fputc('\n',dst)==EOF)goto cleanup;
  if(fflush(dst)!=0)goto cleanup;
  rc=1;
cleanup:
  if(src)fclose(src);
  if(fclose(dst)!=0)rc=0;
  if(rc)rc=MoveFileExA(tmp,active_store,
                      MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
  if(!rc)DeleteFileA(tmp);
  (void)seen;
  return rc;
}
void SpiderSDL3Raw_Init(HMODULE sdl,void (*report)(const char *)) {
  FILE *f;
  char line[1024];
  unsigned count=0;
  logger=report;ready=0;active_store[0]=0;
  if(!sdl)return;
#define SYM(field,sym) field=(void *)GetProcAddress(sdl,sym)
  SYM(get_ids,"SDL_GetJoysticks");
  SYM(is_gamepad,"SDL_IsGamepad");
  SYM(get_name,"SDL_GetJoystickNameForID");
  SYM(open_joy,"SDL_OpenJoystick");
  SYM(close_joy,"SDL_CloseJoystick");
  SYM(num_buttons,"SDL_GetNumJoystickButtons");
  SYM(num_axes,"SDL_GetNumJoystickAxes");
  SYM(num_hats,"SDL_GetNumJoystickHats");
  SYM(connected,"SDL_JoystickConnected");
  SYM(get_button,"SDL_GetJoystickButton");
  SYM(get_axis,"SDL_GetJoystickAxis");
  SYM(get_hat,"SDL_GetJoystickHat");
  SYM(get_guid,"SDL_GetJoystickGUIDForID");
  SYM(guid_string,"SDL_GUIDToString");
  SYM(add_mapping,"SDL_AddGamepadMapping");
  SYM(free_sdl,"SDL_free");
  SYM(pump,"SDL_PumpEvents");
  SYM(update_joysticks,"SDL_UpdateJoysticks");
#undef SYM
  if(!get_ids || !is_gamepad || !get_name || !open_joy ||
     !close_joy || !num_buttons || !num_axes || !num_hats ||
     !get_button || !get_axis || !get_hat || !get_guid ||
     !guid_string || !add_mapping || !free_sdl || !pump ||
     !update_joysticks || !connected) {
    note("SDL3-RAW: unavailable: SDL3.dll lacks one or more raw joystick mapping APIs");
    return;
  }
  ready=1;
  choose_store();
  if(!active_store[0]) {
    note("SDL3-RAW: hardware wizard available, persistent mapping path is read-only");
    return;
  }
  f=fopen(active_store,"rb");
  if(!f) {note("SDL3-RAW: keyboard wizard ready; no saved raw mappings");return;}
  while(fgets(line,sizeof(line),f)) {
    size_t len=strlen(line);
    if(len && line[len-1]!='\n' && !feof(f))break;
    while(len && (line[len-1]=='\n' || line[len-1]=='\r'))line[--len]=0;
    if(line_guid_valid(line) && add_mapping(line)>=0)count++;
  }
  fclose(f);
  {
    char info[160];
    snprintf(info,sizeof(info),
      "SDL3-RAW: loaded %u saved raw joystick GUID mappings into SDL3 before discovery",
      count);
    note(info);
  }
}
unsigned SpiderSDL3Raw_UnmappedCount(void) {
  uint32_t *ids;
  int n=0,i;
  unsigned count=0;
  if(!ready)return 0;
  ids=get_ids(&n);
  if(ids) {
    for(i=0;i<n;i++)if(ids[i] && !is_gamepad(ids[i]))count++;
    free_sdl(ids);
  }
  return count;
}

enum { W_PAD=300, W_BEGIN=301, W_SKIP=302, W_CANCEL=303,
       W_TIMER=1 };
typedef struct {
  HWND window,pads,status,prompt,button;
  uint32_t raw_ids[16],id;
  unsigned count,step,armed,complete;
  void *device;
  SpiderRawBinding slots[SPIDER_RAW_BIND_COUNT];
  int axes,buttons,hats;
  int16_t baseline[16];
  char guid[64],name[128];
} Wizard;
static void set_status(Wizard *w,const char *text) {
  if(w && w->status)SetWindowTextA(w->status,text);
}
static void prompt_step(Wizard *w) {
  char line[270];
  if(w->step>=SPIDER_RAW_BIND_COUNT)return;
  snprintf(line,sizeof(line),
    "Step %u / %u: %s%s\n\nRelease all controls, then press/move ONE input.\n"
    "Keyboard: ESC cancel; SPACE skip optional step.",
    w->step+1u,SPIDER_RAW_BIND_COUNT,spider_raw_display_names[w->step],
    spider_raw_required(w->step)?" (required)":" (optional)");
  SetWindowTextA(w->prompt,line);
  EnableWindow(GetDlgItem(w->window,W_SKIP),!spider_raw_required(w->step));
  w->armed=0;
}
static void close_device(Wizard *w) {
  if(w->device){close_joy(w->device);w->device=NULL;}
}
static int begin_capture(Wizard *w) {
  LRESULT index=SendMessageA(w->pads,CB_GETCURSEL,0,0);
  SpiderSDLGuid id;
  int i;
  if(index==CB_ERR || (unsigned)index>=w->count)return 0;
  w->id=w->raw_ids[index];
  if(!w->id || is_gamepad(w->id))return 0;
  w->device=open_joy(w->id);
  if(!w->device)return 0;
  w->buttons=num_buttons(w->device);
  w->axes=num_axes(w->device);
  w->hats=num_hats(w->device);
  if(w->buttons<4 || w->axes<2 || w->hats<0 ||
     w->buttons>256 || w->axes>256 || w->hats>256) {
    close_device(w);
    set_status(w,"Unusable joystick: needs at least 4 buttons and 2 axes.");
    return 0;
  }
  if(w->buttons>64)w->buttons=64;
  if(w->axes>16)w->axes=16;
  if(w->hats>8)w->hats=8;
  id=get_guid(w->id);
  memset(w->guid,0,sizeof(w->guid));
  guid_string(id,w->guid,sizeof(w->guid));
  if(strlen(w->guid)!=32u) {
    close_device(w);set_status(w,"Invalid SDL joystick GUID.");return 0;
  }
  snprintf(w->name,sizeof(w->name),"%s",
           get_name(w->id)?get_name(w->id):"Unknown SDL3 Joystick");
  for(i=0;i<w->axes;i++)w->baseline[i]=get_axis(w->device,i);
  w->step=0;w->complete=0;w->armed=0;
  memset(w->slots,0,sizeof(w->slots));
  EnableWindow(w->pads,FALSE);
  EnableWindow(GetDlgItem(w->window,W_BEGIN),FALSE);
  set_status(w,"Raw SDL3 capture active. Keyboard always remains available.");
  prompt_step(w);
  return 1;
}
static int neutral(Wizard *w) {
  int i;
  for(i=0;i<w->buttons;i++)if(get_button(w->device,i))return 0;
  for(i=0;i<w->hats;i++)if(get_hat(w->device,i))return 0;
  for(i=0;i<w->axes;i++) {
    int delta=(int)get_axis(w->device,i)-(int)w->baseline[i];
    if(delta>6000 || delta<-6000)return 0;
  }
  return 1;
}
static int try_capture(Wizard *w,SpiderRawBinding *b) {
  int i,delta;
  unsigned step=w->step;
  memset(b,0,sizeof(*b));
  /* Axis-target steps: prioritize a raw axis over buttons. */
  if((step>=8u && step<=13u)) {
    for(i=0;i<w->axes;i++) {
      delta=(int)get_axis(w->device,i)-(int)w->baseline[i];
      if(delta>=15000 || delta<=-15000) {
        b->kind=SPIDER_RAW_AXIS;b->index=(uint8_t)i;return 1;
      }
    }
    if(step>=8u && step<=11u)return 0;
  }
  if(step>=14u && step<=17u) {
    for(i=0;i<w->hats;i++) {
      unsigned value=get_hat(w->device,i);
      unsigned single=value==1u?1u:value==2u?2u:
                      value==4u?4u:value==8u?8u:0u;
      if(single) {
        b->kind=SPIDER_RAW_HAT;
        b->index=(uint8_t)i;b->hat_value=(uint8_t)single;
        return 1;
      }
    }
  }
  for(i=0;i<w->buttons;i++)if(get_button(w->device,i)) {
    b->kind=SPIDER_RAW_BUTTON;b->index=(uint8_t)i;return 1;
  }
  return 0;
}
static void finish(Wizard *w) {
  char mapping[1000],info[310];
  int rc,saved;
  if(!spider_raw_mapping_text(w->guid,w->name,w->slots,mapping,
                              sizeof(mapping))) {
    set_status(w,"Mapping validation failed; hardware remains unchanged.");
    return;
  }
  /* Add to SDL first, verify success; then persist exactly that mapping. */
  rc=add_mapping(mapping);
  if(rc<0) {
    set_status(w,"SDL_AddGamepadMapping rejected this mapping; not saved.");
    return;
  }
  saved=save_mapping(mapping);
  snprintf(info,sizeof(info),
    saved?"SDL3 recognized this joystick. Mapping SAVED for next launch.\n"
          "Close, then choose this gamepad in SDL3 Controller Setup.":
          "SDL3 recognized joystick for this run, but DISK SAVE FAILED.\n"
          "Check write permissions in SpiderMan2k1Fix and LOCALAPPDATA.");
  set_status(w,info);
  SetWindowTextA(w->prompt,"Recognition completed.\n"
                 "No changes to the original Spider-Man DirectInput device.");
  w->complete=1;
  close_device(w);
  {
    char log[200];
    snprintf(log,sizeof(log),
      "SDL3-RAW: GUID %.32s mapped by keyboard wizard SDL-result=%d saved=%d [no native gameplay injection]",
      w->guid,rc,saved);
    note(log);
  }
}
static void wizard_tick(Wizard *w) {
  SpiderRawBinding b;
  char info[180];
  if(!w->device || w->complete)return;
  pump();update_joysticks();
  if(!connected(w->device)) {
    close_device(w);set_status(w,"Controller disconnected: recognition cancelled.");
    return;
  }
  if(!w->armed) {
    if(neutral(w)){w->armed=1;set_status(w,"Ready: press / move the requested physical control.");}
    return;
  }
  if(!try_capture(w,&b))return;
  if(!spider_raw_bind_assign(w->slots,w->step,b)) {
    set_status(w,"Input already assigned or unsupported: release and try another.");
    w->armed=0;
    return;
  }
  snprintf(info,sizeof(info),"Assigned %s: raw %s %u%s.",
           spider_raw_display_names[w->step],
           b.kind==SPIDER_RAW_BUTTON?"button":
           b.kind==SPIDER_RAW_AXIS?"axis":"hat",
           (unsigned)b.index,b.kind==SPIDER_RAW_HAT?" (direction)":"");
  set_status(w,info);
  w->step++;
  if(w->step==SPIDER_RAW_BIND_COUNT)finish(w);
  else prompt_step(w);
}
static void skip_step(Wizard *w) {
  if(!w->device || w->complete || w->step>=SPIDER_RAW_BIND_COUNT ||
     spider_raw_required(w->step))return;
  w->step++;
  if(w->step==SPIDER_RAW_BIND_COUNT)finish(w);
  else prompt_step(w);
}
static LRESULT CALLBACK wizard_wndproc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
  Wizard *w=(Wizard *)(uintptr_t)GetWindowLongPtrA(hwnd,GWLP_USERDATA);
  switch(msg) {
    case WM_CREATE: {
      CREATESTRUCTA *cs=(CREATESTRUCTA *)lp;
      HFONT font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
      HWND h;
      uint32_t *ids;
      int n=0,i;
      w=(Wizard *)cs->lpCreateParams;
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,(LONG_PTR)(uintptr_t)w);
      w->window=hwnd;
      h=CreateWindowExA(0,"STATIC",
        "Unknown SDL3 joysticks (navigate with Tab and keyboard):",
        WS_CHILD|WS_VISIBLE,16,15,610,25,hwnd,NULL,NULL,NULL);
      SendMessageA(h,WM_SETFONT,(WPARAM)font,TRUE);
      w->pads=CreateWindowExA(0,"COMBOBOX","",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,
        16,44,590,240,hwnd,(HMENU)(uintptr_t)W_PAD,NULL,NULL);
      SendMessageA(w->pads,WM_SETFONT,(WPARAM)font,TRUE);
      w->prompt=CreateWindowExA(0,"STATIC",
        "Choose an unmapped device and select Begin. "
        "All game controls remain untouched.",
        WS_CHILD|WS_VISIBLE,16,95,590,110,hwnd,NULL,NULL,NULL);
      SendMessageA(w->prompt,WM_SETFONT,(WPARAM)font,TRUE);
      w->status=CreateWindowExA(0,"STATIC","",
        WS_CHILD|WS_VISIBLE,16,216,590,65,hwnd,NULL,NULL,NULL);
      SendMessageA(w->status,WM_SETFONT,(WPARAM)font,TRUE);
      h=CreateWindowExA(0,"BUTTON","Begin",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,16,297,180,38,
        hwnd,(HMENU)(uintptr_t)W_BEGIN,NULL,NULL);
      SendMessageA(h,WM_SETFONT,(WPARAM)font,TRUE);
      h=CreateWindowExA(0,"BUTTON","Skip optional (Space)",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,213,297,180,38,
        hwnd,(HMENU)(uintptr_t)W_SKIP,NULL,NULL);
      SendMessageA(h,WM_SETFONT,(WPARAM)font,TRUE);
      EnableWindow(h,FALSE);
      h=CreateWindowExA(0,"BUTTON","Cancel / Close",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP,410,297,195,38,
        hwnd,(HMENU)(uintptr_t)W_CANCEL,NULL,NULL);
      SendMessageA(h,WM_SETFONT,(WPARAM)font,TRUE);
      ids=get_ids(&n);
      if(ids) {
        for(i=0;i<n && w->count<16u;i++)if(ids[i] && !is_gamepad(ids[i])) {
          char label[256];
          const char *name=get_name(ids[i]);
          w->raw_ids[w->count++]=ids[i];
          snprintf(label,sizeof(label),"%s (SDL instance %lu)",
                   name?name:"Unnamed joystick",(unsigned long)ids[i]);
          SendMessageA(w->pads,CB_ADDSTRING,0,(LPARAM)label);
        }
        free_sdl(ids);
      }
      if(w->count) {
        SendMessageA(w->pads,CB_SETCURSEL,0,0);
        set_status(w,"Select Begin, release every control, then follow 20 guided steps.");
      } else {
        EnableWindow(GetDlgItem(hwnd,W_BEGIN),FALSE);
        set_status(w,"No unmapped SDL3 joystick detected. Existing SDL3 gamepads are already mapped.");
      }
      SetTimer(hwnd,W_TIMER,35,NULL);
      return 0;
    }
    case WM_TIMER:
      if(w && wp==W_TIMER)wizard_tick(w);
      return 0;
    case WM_COMMAND:
      if(!w || HIWORD(wp)!=BN_CLICKED)return 0;
      if(LOWORD(wp)==W_BEGIN) {
        if(!begin_capture(w))set_status(w,"Unable to open raw joystick. Select another device.");
      } else if(LOWORD(wp)==W_SKIP)skip_step(w);
      else if(LOWORD(wp)==W_CANCEL)PostMessageA(hwnd,WM_CLOSE,0,0);
      return 0;
    case WM_KEYDOWN:
      if(wp==VK_ESCAPE){PostMessageA(hwnd,WM_CLOSE,0,0);return 0;}
      if(wp==VK_SPACE && w){skip_step(w);return 0;}
      break;
    case WM_CLOSE:
      KillTimer(hwnd,W_TIMER);
      if(w)close_device(w);
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY:
      KillTimer(hwnd,W_TIMER);
      if(w)close_device(w);
      SetWindowLongPtrA(hwnd,GWLP_USERDATA,0);
      return 0;
  }
  return DefWindowProcA(hwnd,msg,wp,lp);
}
void SpiderSDL3Raw_Recognize(HWND owner) {
  static int registered=0,busy=0;
  static const char *cls="SpiderMan2k1FixSDL3RawWizard";
  WNDCLASSA wc;
  Wizard w;
  HWND hwnd;
  MSG msg;
  DWORD process=0;
  if(!ready || busy || !owner || !IsWindow(owner) ||
     GetWindowThreadProcessId(owner,&process)==0 ||
     process!=GetCurrentProcessId())return;
  if(!registered) {
    memset(&wc,0,sizeof(wc));
    wc.lpfnWndProc=wizard_wndproc;
    wc.hInstance=GetModuleHandleA(NULL);
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    wc.lpszClassName=cls;
    if(!RegisterClassA(&wc))return;
    registered=1;
  }
  memset(&w,0,sizeof(w));
  busy=1;
  hwnd=CreateWindowExA(WS_EX_DLGMODALFRAME,cls,
      "SDL3 - Recognize a new controller (raw joystick)",
      WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
      CW_USEDEFAULT,CW_USEDEFAULT,635,390,
      owner,NULL,GetModuleHandleA(NULL),&w);
  if(!hwnd){busy=0;return;}
  EnableWindow(owner,FALSE);
  ShowWindow(hwnd,SW_SHOW);
  SetForegroundWindow(hwnd);
  while(IsWindow(hwnd) && GetMessageA(&msg,NULL,0,0)>0) {
    if(!IsDialogMessageA(hwnd,&msg)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  }
  close_device(&w);
  EnableWindow(owner,TRUE);
  SetForegroundWindow(owner);
  busy=0;
}
