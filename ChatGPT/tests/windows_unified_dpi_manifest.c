/* Win32 external-manifest activation check; NO original game files.
 * This test EXE is intentionally built with no embedded RT_MANIFEST.
 * Windows is expected to recognize its adjacent SpideyPC.exe.manifest.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
typedef HRESULT (WINAPI *GetProcessDpiAwarenessFn)(HANDLE,int *);
int wmain(void) {
  HMODULE mod=LoadLibraryW(L"shcore.dll");
  GetProcessDpiAwarenessFn get=NULL;
  int awareness=-1;
  HRESULT hr=E_FAIL;
  if(mod)get=(GetProcessDpiAwarenessFn)(void*)GetProcAddress(mod,"GetProcessDpiAwareness");
  if(get)hr=get(GetCurrentProcess(),&awareness);
  else {
    HMODULE user=GetModuleHandleW(L"user32.dll");
    typedef BOOL (WINAPI *IsProcessDPIAwareFn)(void);
    IsProcessDPIAwareFn aware=user?(IsProcessDPIAwareFn)(void*)GetProcAddress(user,"IsProcessDPIAware"):NULL;
    if(aware) {
      awareness=aware()?1:0;
      hr=S_OK;
    }
  }
  wprintf(L"DPI-MANIFEST-TEST: process-awareness=%d hr=0x%08lx\n",awareness,(unsigned long)hr);
  if(mod)FreeLibrary(mod);
  return SUCCEEDED(hr)&&awareness==1?0:1;
}
