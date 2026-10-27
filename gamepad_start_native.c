/*
 * Experimental original-game Start binding for DirectInput gamepads.
 * No XInput conversion, keyboard events or unsafe DI8 COM detours.
 *
 * Partial community decompilation identifies PCINPUT_GetMappedStates:
 * native action mask 0x1000 (Start) originally maps to controller button
 * 11 and gets passed through Pad_Button(&gSControl[0].Start, v3 & 0x1000).
 *
 * Supported retail binary is SHA-256 gated by initialize_body() BEFORE
 * this initializer is called. We further require exactly TWO tables with
 * the original 14-action layout in writable PE .data (active + factory).
 * If the retail code differs, refuse to modify anything.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "gamepad_start_mapping.h"
#include "gamepad_start_native.h"

static int parse_button(const char *s,uint32_t *button) {
  unsigned x=0,i=6;
  if(!s || !button || _strnicmp(s,"BUTTON",6)!=0 ||
     s[6]<'0' || s[6]>'9')return 0;
  while(s[i]>='0' && s[i]<='9') {
    x=x*10u+(unsigned)(s[i]-'0');
    if(x>31u)return 0;
    ++i;
  }
  if(s[i]!=0)return 0;
  *button=(uint32_t)x;
  return 1;
}
static int writable_mapping_slot(BYTE *slot) {
  MEMORY_BASIC_INFORMATION info;
  DWORD protection;
  if(!slot || ((uintptr_t)slot & 3u) ||
     !VirtualQuery(slot,&info,sizeof(info)) ||
     info.State!=MEM_COMMIT)return 0;
  protection=info.Protect;
  if(protection&(PAGE_GUARD|PAGE_NOACCESS))return 0;
  protection&=0xFFu;
  return protection==PAGE_READWRITE || protection==PAGE_WRITECOPY ||
         protection==PAGE_EXECUTE_READWRITE || protection==PAGE_EXECUTE_WRITECOPY;
}
/* First DirectDrawCreateEx is a one-shot, game-thread bootstrap point. */
static volatile LONG g_start_init_attempted=0;
void SpiderGamepadStart_Init(const char *ini,void (*logger)(const char *)) {
  BYTE *base;
  IMAGE_DOS_HEADER *dos;
  IMAGE_NT_HEADERS32 *nt;
  IMAGE_SECTION_HEADER *sections;
  BYTE *matches[3]={0};
  unsigned found=0,i,sections_seen=0;
  uint32_t button=0;
  char choice[40],line[240];

  if(InterlockedCompareExchange(&g_start_init_attempted,1,0)!=0)return;
  if(!ini || !ini[0]) {
    if(logger)logger("GAMEPAD-START: SKIPPED INI filename not available");
    return;
  }
  GetPrivateProfileStringA("Input","ExperimentalGamepadStart","",
                           choice,sizeof(choice),ini);
  if(!choice[0]) {
    if(logger)logger("GAMEPAD-START: OFF [Input] ExperimentalGamepadStart absent; no controller mapping attempted");
    return;
  }
  if(!parse_button(choice,&button)) {
    if(logger)logger("GAMEPAD-START: invalid opt-in value (use BUTTON0..BUTTON31); original controls unchanged");
    return;
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-START: REQUESTED %s after native game configuration bootstrap (Start factory BUTTON11)",
    choice);
  if(logger)logger(line);
  base=(BYTE*)GetModuleHandleA(NULL);
  if(!base || (uintptr_t)base!=0x00400000u) {
    if(logger)logger("GAMEPAD-START: SKIPPED unsupported loaded image base");
    return;
  }
  dos=(IMAGE_DOS_HEADER*)base;
  if(dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<=0 ||
     dos->e_lfanew>0x1000) {
    if(logger)logger("GAMEPAD-START: SKIPPED invalid DOS/PE header offset");
    return;
  }
  nt=(IMAGE_NT_HEADERS32*)(base+dos->e_lfanew);
  if(nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=0x00400000u ||
     nt->OptionalHeader.SizeOfImage>0x2000000u ||
     nt->OptionalHeader.SizeOfImage<0x140000u ||
     nt->FileHeader.NumberOfSections==0 ||
     nt->FileHeader.NumberOfSections>32u) {
    if(logger)logger("GAMEPAD-START: SKIPPED unsupported PE header layout");
    return;
  }
  sections=IMAGE_FIRST_SECTION(nt);
  for(i=0;i<(unsigned)nt->FileHeader.NumberOfSections;i++) {
    IMAGE_SECTION_HEADER *sec=&sections[i];
    DWORD span=sec->Misc.VirtualSize,offset;
    BYTE *data;
    if(memcmp(sec->Name,".data",5)!=0 ||
       !(sec->Characteristics & IMAGE_SCN_MEM_WRITE) ||
       span<SPIDER_START_MAP_BYTES ||
       sec->VirtualAddress>=nt->OptionalHeader.SizeOfImage ||
       span>nt->OptionalHeader.SizeOfImage-sec->VirtualAddress)continue;
    ++sections_seen;
    data=base+sec->VirtualAddress;
    for(offset=0;offset<=span-SPIDER_START_MAP_BYTES;offset+=4u) {
      if(!SpiderGamepad_MappingRecognized(data+offset,span-offset))continue;
      if(found<3u)matches[found]=data+offset;
      ++found;
    }
  }
  snprintf(line,sizeof(line),
    "GAMEPAD-START: SCAN writable-data-sections=%u recognized-action-tables=%u",
    sections_seen,found);
  if(logger)logger(line);
  if(found!=2u || !matches[0] || !matches[1]) {
    snprintf(line,sizeof(line),
      "GAMEPAD-START: expected two verified controller maps; found=%u; no patch (read-only fail-closed)",
      found);
    if(logger)logger(line);
    return;
  }
  if(!SpiderGamepad_StartButtonAvailable(matches[0],SPIDER_START_MAP_BYTES,button) ||
     !SpiderGamepad_StartButtonAvailable(matches[1],SPIDER_START_MAP_BYTES,button) ||
     !writable_mapping_slot(matches[0]+SPIDER_START_MAP_BUTTON_OFFSET) ||
     !writable_mapping_slot(matches[1]+SPIDER_START_MAP_BUTTON_OFFSET)) {
    if(logger)logger("GAMEPAD-START: conflicting gameplay button or non-writable map; original mappings retained");
    return;
  }
  /* Both tables are known-shaped, stable, writable game-owned .data.
   * Mutate ONLY the one 32-bit Start index, atomically, on the game's
   * startup worker thread before DirectDraw is initialized. No other
   * action, key state or COM interface is modified.
   */
  InterlockedExchange((volatile LONG*)(void*)
      (matches[0]+SPIDER_START_MAP_BUTTON_OFFSET),(LONG)button);
  InterlockedExchange((volatile LONG*)(void*)
      (matches[1]+SPIDER_START_MAP_BUTTON_OFFSET),(LONG)button);
  snprintf(line,sizeof(line),
    "GAMEPAD-START: EXPERIMENTAL mapping Start from BUTTON11 to BUTTON%u in two verified original tables; check menu, Bink and gameplay under Wine/Windows",
    (unsigned)button);
  if(logger)logger(line);
}
