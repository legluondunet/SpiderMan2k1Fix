/*
 * Spider-Man 2001: avoid routing the game's DirectSound8 device to the
 * DualShock 4 audio endpoint on native Windows when another playback
 * device exists.  0 (default) = avoid DS4 output; 1 = original behaviour.
 *
 * The verified game's initialization uses DirectSoundCreate8 with
 * DSDEVID_DefaultPlayback.  PCMovie and PCMusic pass this same DS8 instance
 * to BinkOpenDirectSound, so selecting the device at creation covers native
 * effects and Bink without changing the original audio engine.
 *
 * Deliberately NOT a global Windows audio policy change.  Never disable a
 * device, reset an audio session, patch COM vtables or forcibly reinitialize
 * sound during controller hotplug.  If the selection fails, use the game's
 * original GUID and return the original DirectSound result.
 *
 * Windows hardware validation PENDING.  Wine keeps its original selection.
 */
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "audio_ds4_route.h"

typedef HRESULT (WINAPI *SpiderDSCreate8)(LPCGUID, LPDIRECTSOUND8 *, LPUNKNOWN);
typedef HRESULT (WINAPI *SpiderDSEnumerateA)(LPDSENUMCALLBACKA, LPVOID);

static SpiderDSCreate8 real_create8;
static SpiderDSEnumerateA enumerate_devices;
static void (*route_log)(const char *);
static volatile LONG route_enabled;
static volatile LONG route_attempts;

typedef struct {
  GUID fallback_guid;
  char fallback_name[128];
  int have_fallback;
  int saw_ds4;
  unsigned physical_count;
} SpiderAudioChoices;

static void report(const char *msg) {if(route_log)route_log(msg);}

/* Endpoint display names are not stable hardware identifiers.  Do NOT match
 * generic "headphones" or "controller": these are intentionally strict
 * indications of a Sony PS4 wireless audio endpoint. */
static int contains_ascii_ci(const char *text,const char *needle) {
  const unsigned char *p=(const unsigned char*)text;
  size_t i,n;
  if(!text || !needle || !(n=strlen(needle)))return 0;
  for(;*p;p++) {
    for(i=0;i<n && p[i] &&
        tolower((unsigned char)p[i])==tolower((unsigned char)needle[i]);i++){}
    if(i==n)return 1;
  }
  return 0;
}

static int is_ds4_audio(const char *name) {
  return contains_ascii_ci(name,"Wireless Controller") ||
         contains_ascii_ci(name,"DUALSHOCK 4") ||
         contains_ascii_ci(name,"PS4 Controller");
}

static BOOL CALLBACK collect_outputs(LPGUID guid,LPCSTR name,LPCSTR driver,
                                      LPVOID opaque) {
  SpiderAudioChoices *choices=(SpiderAudioChoices*)opaque;
  (void)driver;
  /* NULL is the synthetic "Primary Sound Driver": skip it. */
  if(!guid)return TRUE;
  choices->physical_count++;
  if(is_ds4_audio(name)) {
    choices->saw_ds4=1;
    return TRUE;
  }
  /* DirectSound enumerates the actual default endpoint before others.
   * Preserve that choice if it isn't the DS4 audio endpoint. */
  if(!choices->have_fallback) {
    choices->fallback_guid=*guid;
    if(name) {
      strncpy(choices->fallback_name,name,sizeof(choices->fallback_name)-1);
      choices->fallback_name[sizeof(choices->fallback_name)-1]=0;
    }
    choices->have_fallback=1;
  }
  return TRUE;
}

static HRESULT WINAPI routed_create8(LPCGUID requested,LPDIRECTSOUND8 *out,
                                      LPUNKNOWN outer) {
  SpiderAudioChoices choices={0};
  HRESULT hr,enumeration;
  char line[300];
  if(!real_create8)return DSERR_NODRIVER;
  if(InterlockedCompareExchange(&route_enabled,0,0)!=1 ||
     (requested && !IsEqualGUID(requested,&DSDEVID_DefaultPlayback)))
    return real_create8(requested,out,outer);
  if(!enumerate_devices) {
    report("AUDIO-ROUTING: no DirectSoundEnumerateA; original output preserved");
    return real_create8(requested,out,outer);
  }
  enumeration=enumerate_devices(collect_outputs,&choices);
  if(FAILED(enumeration) || !choices.saw_ds4 || !choices.have_fallback) {
    if(InterlockedIncrement(&route_attempts)==1) {
      snprintf(line,sizeof(line),
        "AUDIO-ROUTING: default output retained (enumeration=0x%08lX, devices=%u, ds4=%d, alternative=%d)",
        (unsigned long)enumeration,choices.physical_count,
        choices.saw_ds4,choices.have_fallback);
      report(line);
    }
    return real_create8(requested,out,outer);
  }
  /* Explicitly select the non-DS4 GUID only for this game process.
   * Windows per-application audio preferences can still supersede this;
   * the hardware test must verify audible output. */
  hr=real_create8(&choices.fallback_guid,out,outer);
  snprintf(line,sizeof(line),
    "AUDIO-ROUTING: selected non-DS4 DirectSound device '%s' hr=0x%08lX (Windows audio policy not changed)",
    choices.fallback_name,(unsigned long)hr);
  report(line);
  if(SUCCEEDED(hr))return hr;
  report("AUDIO-ROUTING: device selection failed; retrying game's original default");
  return real_create8(requested,out,outer);
}

/* IAT-only, original-executable-only.  Never patch a guessed instruction or
 * another DLL's imports.  The caller validated exact game EXE SHA-256. */
static int patch_ds8_import(void) {
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  IMAGE_IMPORT_DESCRIPTOR *imp;
  DWORD image_size,imports_rva,imports_size,old,ignored;
  if(!base)return 0;
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
     dos->e_lfanew>0x1000)return 0;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     (uintptr_t)base!=0x00400000u)return 0;
  image_size=nt->OptionalHeader.SizeOfImage;
  imports_rva=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
  imports_size=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
  if(!imports_rva || imports_rva>=image_size ||
     imports_size<sizeof(IMAGE_IMPORT_DESCRIPTOR) ||
     imports_size>image_size-imports_rva)return 0;
  imp=(IMAGE_IMPORT_DESCRIPTOR*)(base+imports_rva);
  for(; (BYTE*)(imp+1)<=base+imports_rva+imports_size && imp->Name;imp++) {
    IMAGE_THUNK_DATA32 *slot,*name_thunk;
    const char *lib;
    if(imp->Name>=image_size || !imp->OriginalFirstThunk ||
       !imp->FirstThunk || imp->OriginalFirstThunk>=image_size ||
       imp->FirstThunk>=image_size)continue;
    lib=(const char*)(base+imp->Name);
    if(lstrcmpiA(lib,"dsound.dll"))continue;
    slot=(IMAGE_THUNK_DATA32*)(base+imp->FirstThunk);
    name_thunk=(IMAGE_THUNK_DATA32*)(base+imp->OriginalFirstThunk);
    for(; (BYTE*)(name_thunk+1)<=base+image_size &&
          (BYTE*)(slot+1)<=base+image_size && name_thunk->u1.AddressOfData;
          name_thunk++,slot++) {
      IMAGE_IMPORT_BY_NAME *entry;
      SpiderDSCreate8 existing;
      DWORD func;
      if(IMAGE_SNAP_BY_ORDINAL32(name_thunk->u1.Ordinal) ||
         name_thunk->u1.AddressOfData>=image_size)continue;
      entry=(IMAGE_IMPORT_BY_NAME*)(base+name_thunk->u1.AddressOfData);
      if((BYTE*)entry->Name+sizeof("DirectSoundCreate8")>base+image_size ||
         strcmp((const char*)entry->Name,"DirectSoundCreate8"))continue;
      func=slot->u1.Function;
      if(!func || func==(DWORD)(uintptr_t)&routed_create8)return 0;
      existing=(SpiderDSCreate8)(uintptr_t)func;
      if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&old))return 0;
      real_create8=existing;
      slot->u1.Function=(DWORD)(uintptr_t)&routed_create8;
      if(!VirtualProtect(slot,sizeof(*slot),old,&ignored))
        report("AUDIO-ROUTING: WARNING IAT original page protection could not be restored");
      return 1;
    }
  }
  return 0;
}

void SpiderAudioDS4Route_Install(const char *ini,void (*logger)(const char *)) {
  HMODULE dsound;
  int setting;
  route_log=logger;
  setting=ini?GetPrivateProfileIntA("Audio","DualShock4AudioOutput",0,ini):0;
  if(setting!=0) {
    report("AUDIO-ROUTING: DualShock4AudioOutput=1; original game output kept");
    return;
  }
  dsound=GetModuleHandleA("dsound.dll");
  if(GetProcAddress(GetModuleHandleA("ntdll.dll"),"wine_get_version")) {
    report("AUDIO-ROUTING: Wine detected; DS4 output policy not applied (native audio unchanged)");
    return;
  }
  if(!dsound) {
    report("AUDIO-ROUTING: dsound.dll not imported yet; original output preserved");
    return;
  }
  enumerate_devices=(SpiderDSEnumerateA)(void*)GetProcAddress(dsound,"DirectSoundEnumerateA");
  if(!enumerate_devices) {
    report("AUDIO-ROUTING: enumeration API unavailable; original output preserved");
    return;
  }
  /* Enabled before the hook is published; the actual original DS8 API is
   * never called from initialization and remains responsible for all output. */
  InterlockedExchange(&route_enabled,1);
  if(!patch_ds8_import()) {
    InterlockedExchange(&route_enabled,0);
    report("AUDIO-ROUTING: no verified DirectSoundCreate8 import; original output preserved");
    return;
  }
  report("AUDIO-ROUTING: DualShock4AudioOutput=0; game-only DS4 avoidance armed (native Windows)");
}
