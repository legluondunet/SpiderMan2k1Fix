#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "display_policy.h"
#include "display_menu_lock.h"

/* Applies to SHA-256 0a11a49f...7489dea only (checked by caller).
 * PE ImageBase 0x400000 is fixed/relocations stripped. All four original
 * instruction sequences must match before any writing. Never change the
 * binary on disk. The menu's brightness path (selection=2) remains intact.
 * This does NOT modify the renderer's surface pixel formats.
 */
static volatile LONG shown_width=640, shown_height=480, shown_depth=32;
static int installed=0;
static void (*logger)(const char *);

typedef struct {
  DWORD rva;
  BYTE expected[23];
  BYTE replacement[23];
  unsigned length;
} Patch;

static BOOL write_code(BYTE *addr,const BYTE *bytes,unsigned len) {
  DWORD old,ignored;
  if(!VirtualProtect(addr,len,PAGE_EXECUTE_READWRITE,&old)) return FALSE;
  memcpy(addr,bytes,len);
  FlushInstructionCache(GetCurrentProcess(),addr,len);
  VirtualProtect(addr,len,old,&ignored);
  return TRUE;
}

static void set_immediate_address(BYTE *out,unsigned offset,const void *ptr) {
  DWORD value=(DWORD)(uintptr_t)ptr;
  memcpy(out+offset,&value,sizeof(value));
}

static void report(const char *line) { if(logger) logger(line); }

void SpiderDisplayMenu_SetTarget(DWORD width,DWORD height) {
  char msg[160];
  if(width<640 || height<480 || width>16384 || height>16384) return;
  InterlockedExchange(&shown_width,(LONG)width);
  InterlockedExchange(&shown_height,(LONG)height);
  if(installed) {
    snprintf(msg,sizeof(msg),"DISPLAY-MENU-LOCK: selected resolution label=%lux%lu, depth=32 [MENU UI ONLY]",
             (unsigned long)width,(unsigned long)height);
    report(msg);
  }
}

BOOL SpiderDisplayMenu_Install(const char *setting,const char *aspect,void (*log_fn)(const char *)) {
  /* Original instructions, identified in the verified SpideyPC.exe.
   * 0x50dd19: option selection 0=screen size 1=color depth 2=brightness.
   * 0x50db3f/44/baa: three print-time field loads ONLY.
   */
  static const BYTE original_choice[23]={
    0x33,0xC0,0x8A,0x47,0x14,0x2B,0xC3,0x0F,0x84,0x03,0x01,0x00,
    0x00,0x48,0x74,0x4E,0x48,0x0F,0x85,0x74,0x01,0x00,0x00
  };
  static const BYTE original_height[5]={0xA1,0x0C,0x97,0xE0,0x02};
  static const BYTE original_width[6]={0x8B,0x0D,0xF8,0x96,0xE0,0x02};
  static const BYTE original_depth[6]={0x8B,0x0D,0xE4,0x98,0xE0,0x02};
  BYTE *base=(BYTE*)GetModuleHandleA(NULL);
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  Patch patches[4];
  DWORD native_width=640,native_height=480;
  unsigned target_w,target_h;
  DEVMODEA mode;
  char msg[220];
  unsigned i;
  logger=log_fn;
  if(installed) return TRUE;
  if(!base) goto fail;
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 || dos->e_lfanew>0x1000) goto fail;
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     (uintptr_t)base!=0x400000 || nt->OptionalHeader.SizeOfImage<0x10df00) goto fail;
  memset(patches,0,sizeof(patches));
  patches[0].rva=0x0010dd19;patches[0].length=23;
  memcpy(patches[0].expected,original_choice,23);
  /* cmp byte ptr [edi+14h],2; jne 0x50dea4; jmp 0x50dd30.
   * Selection 0/1: no action, next menu frame. Selection 2: original
   * brightness controls; all menu navigation and exit handlers untouched. */
  {
    const BYTE code[]={0x80,0x7F,0x14,0x02,0x0F,0x85,0x81,0x01,0x00,0x00,
                       0xE9,0x08,0x00,0x00,0x00};
    memcpy(patches[0].replacement,code,sizeof(code));
    memset(patches[0].replacement+sizeof(code),0x90,23-sizeof(code));
  }
  patches[1].rva=0x0010db3f;patches[1].length=5;
  memcpy(patches[1].expected,original_height,5);
  memcpy(patches[1].replacement,original_height,5);
  set_immediate_address(patches[1].replacement,1,(const void*)&shown_height);
  patches[2].rva=0x0010db44;patches[2].length=6;
  memcpy(patches[2].expected,original_width,6);
  memcpy(patches[2].replacement,original_width,6);
  set_immediate_address(patches[2].replacement,2,(const void*)&shown_width);
  patches[3].rva=0x0010dbaa;patches[3].length=6;
  memcpy(patches[3].expected,original_depth,6);
  memcpy(patches[3].replacement,original_depth,6);
  set_immediate_address(patches[3].replacement,2,(const void*)&shown_depth);
  for(i=0;i<4;i++) {
    if(memcmp(base+patches[i].rva,patches[i].expected,patches[i].length)) {
      snprintf(msg,sizeof(msg),"DISPLAY-MENU-LOCK: ERROR original bytes mismatch at RVA 0x%08lX; no patch applied",
        (unsigned long)patches[i].rva);
      report(msg);return FALSE;
    }
  }
  memset(&mode,0,sizeof(mode));mode.dmSize=sizeof(mode);
  if(EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&mode)) {
    native_width=mode.dmPelsWidth;native_height=mode.dmPelsHeight;
  }
  if(SpiderDisplayPolicy_ResolveRuntime(setting,aspect,native_width,native_height,&target_w,&target_h))
    SpiderDisplayMenu_SetTarget(target_w,target_h);
  else report("DISPLAY-MENU-LOCK: WARNING resolution invalid; temporary label 640x480 until monitor probe");
  for(i=0;i<4;i++) {
    if(!write_code(base+patches[i].rva,patches[i].replacement,patches[i].length)) {
      unsigned j;
      for(j=0;j<i;j++) write_code(base+patches[j].rva,patches[j].expected,patches[j].length);
      report("DISPLAY-MENU-LOCK: ERROR VirtualProtect; previous changes rolled back");
      return FALSE;
    }
  }
  installed=1;
  snprintf(msg,sizeof(msg),"DISPLAY-MENU-LOCK: ACTIVE screen size=%ldx%ld (INI), color=32; option 0/1 ignored, brightness untouched",
    (long)shown_width,(long)shown_height);
  report(msg);
  return TRUE;
fail:
  report("DISPLAY-MENU-LOCK: ERROR unsupported executable layout; no changes");
  return FALSE;
}