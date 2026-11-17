/* Experimental unified DirectDraw entry for a mandatory launcher alias.
 * This file is compiled ONLY with SPIDER_UNIFIED_FIX; the existing WinMM
 * architecture stays unchanged while the new installer is developed.
 *
 * The PE loader loads game/fix.dll from the game's import alias. DllMain in
 * the core must stay inert in unified mode. On the game's FIRST actual
 * DirectDrawCreateEx call (after loader lock), resolve the operating
 * system's genuine DirectDraw implementation and run the authenticated
 * core initialization synchronously. No third-party ddraw.dll is modified.
 */
#ifndef SPIDER_UNIFIED_FIX
#error Unified DirectDraw bridge requires SPIDER_UNIFIED_FIX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>
#include <unknwn.h>
#include <wchar.h>

typedef HRESULT (WINAPI *SpiderSystemDDrawCreateEx)(
    GUID *, void **, REFIID, IUnknown *);
extern HRESULT __cdecl SpiderMan2k1Fix_DirectDrawGate(
    GUID *, void **, REFIID, IUnknown *, void *);

static INIT_ONCE spider_system_ddraw_once=INIT_ONCE_STATIC_INIT;
static SpiderSystemDDrawCreateEx spider_system_ddraw_create=NULL;
/* Do not unload the provider: COM objects can retain methods in this DLL
 * after the factory call returns. */
static HMODULE spider_system_ddraw_module=NULL;

static BOOL CALLBACK spider_load_system_ddraw(
    PINIT_ONCE once, PVOID param, PVOID *context) {
  wchar_t path[MAX_PATH];
  UINT n;
  FARPROC symbol;
  (void)once; (void)param; (void)context;
  n=GetSystemDirectoryW(path,MAX_PATH);
  if(!n || n>=MAX_PATH-11)return TRUE;
  wcscpy(path+n,L"\\ddraw.dll");
  spider_system_ddraw_module=LoadLibraryW(path);
  if(!spider_system_ddraw_module)return TRUE;
  symbol=GetProcAddress(spider_system_ddraw_module,"DirectDrawCreateEx");
  if(symbol)spider_system_ddraw_create=(SpiderSystemDDrawCreateEx)(void*)symbol;
  return TRUE;
}

/* Exported by unified_directdraw.def under the ORIGINAL undecorated API
 * name. The installed retail game calls this factory from its IAT. */
HRESULT WINAPI SpiderUnified_DirectDrawCreateEx(
    GUID *guid, void **out, REFIID iid, IUnknown *outer) {
  if(out)*out=NULL;
  if(!InitOnceExecuteOnce(&spider_system_ddraw_once,
       spider_load_system_ddraw,NULL,NULL) || !spider_system_ddraw_create)
    return DDERR_UNSUPPORTED;
  return SpiderMan2k1Fix_DirectDrawGate(
    guid,out,iid,outer,(void*)spider_system_ddraw_create);
}
