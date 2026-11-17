#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>
#include <stdint.h>
#include <string.h>
#include "unified_import_alias.h"

/* The original retail game's image base and one DDraw IAT slot were derived
 * from the original supported 1,507,328-byte PE32, not guessed at runtime. */
#define SPIDER_IMAGE_BASE ((uintptr_t)0x00400000u)
#define SPIDER_DDRAW_CREATE_IAT_RVA 0x0013b010u

static int remote_read(HANDLE process,uintptr_t addr,void *dst,SIZE_T size) {
  SIZE_T got=0;
  return ReadProcessMemory(process,(LPCVOID)addr,dst,size,&got) && got==size;
}
static int rva_range(DWORD offset,DWORD length,DWORD image_size) {
  return offset>0 && offset<image_size && length<=image_size-offset;
}
static int remote_write_verified(HANDLE process,uintptr_t addr,
                                  const void *old_bytes,const void *new_bytes,
                                  SIZE_T size) {
  BYTE verify[32];
  DWORD previous,ignored;
  SIZE_T written=0;
  int success;
  if(!process || !old_bytes || !new_bytes || !size || size>sizeof(verify))
    return 0;
  if(!VirtualProtectEx(process,(LPVOID)addr,size,
                       PAGE_EXECUTE_READWRITE,&previous))return 0;
  success=WriteProcessMemory(process,(LPVOID)addr,new_bytes,size,&written) &&
          written==size &&
          FlushInstructionCache(process,(LPCVOID)addr,size) &&
          remote_read(process,addr,verify,size) &&
          memcmp(verify,new_bytes,size)==0;
  if(!success) {
    /* The parent must still terminate the suspended child on any failure.
     * Restore bytes best-effort; never resume partly patched code. */
    SIZE_T restored=0;
    WriteProcessMemory(process,(LPVOID)addr,old_bytes,size,&restored);
    FlushInstructionCache(process,(LPCVOID)addr,size);
  }
  if(!VirtualProtectEx(process,(LPVOID)addr,size,previous,&ignored))
    success=0;
  return success;
}
static int remote_supported_pe32(HANDLE process,IMAGE_NT_HEADERS32 *nt) {
  IMAGE_DOS_HEADER dos;
  uintptr_t nt_addr;
  if(!remote_read(process,SPIDER_IMAGE_BASE,&dos,sizeof(dos)) ||
     dos.e_magic!=IMAGE_DOS_SIGNATURE || dos.e_lfanew<0x40 ||
     dos.e_lfanew>0x1000)return 0;
  nt_addr=SPIDER_IMAGE_BASE+(uintptr_t)dos.e_lfanew;
  if(!remote_read(process,nt_addr,nt,sizeof(*nt)) ||
     nt->Signature!=IMAGE_NT_SIGNATURE ||
     nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
     nt->OptionalHeader.ImageBase!=SPIDER_IMAGE_BASE ||
     nt->OptionalHeader.SizeOfImage<=SPIDER_DDRAW_CREATE_IAT_RVA+4u ||
     nt->OptionalHeader.SizeOfImage>0x02000000u ||
     nt->OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT)
    return 0;
  return 1;
}
/* All table lookups are bounded by the supported PE32 image. The name must
 * belong to an actual import descriptor for the exact import thunk, not to a
 * coincidental ASCII substring elsewhere in game data. */
int SpiderUnified_AliasSuspendedDDraw(HANDLE process) {
  IMAGE_NT_HEADERS32 nt;
  IMAGE_DATA_DIRECTORY table;
  DWORD i,found_name_rva=0,found=0,table_terminated=0;
  static const char replacement[10]={'f','i','x','.','d','l','l',0,0,0};
  char original[10];
  if(!remote_supported_pe32(process,&nt))return 0;
  table=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
  if(!rva_range(table.VirtualAddress,table.Size,nt.OptionalHeader.SizeOfImage) ||
     table.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR))return 0;
  for(i=0;i<128 && (i+1u)*sizeof(IMAGE_IMPORT_DESCRIPTOR)<=table.Size;i++){
    IMAGE_IMPORT_DESCRIPTOR desc;
    char name[10];
    DWORD j,matched=0,thunks_terminated=0;
    DWORD desc_rva=table.VirtualAddress+i*sizeof(desc);
    if(!remote_read(process,SPIDER_IMAGE_BASE+desc_rva,&desc,sizeof(desc)))
      return 0;
    if(desc.Name==0 && desc.FirstThunk==0){table_terminated=1;break;}
    if(!rva_range(desc.Name,sizeof(name),nt.OptionalHeader.SizeOfImage))
      return 0;
    if(!remote_read(process,SPIDER_IMAGE_BASE+desc.Name,name,sizeof(name)))
      return 0;
    if(name[9]!=0 || _stricmp(name,"ddraw.dll")!=0)continue;
    if(!desc.OriginalFirstThunk || !desc.FirstThunk)return 0;
    for(j=0;j<128;j++){
      IMAGE_THUNK_DATA32 thunk;
      DWORD rva=desc.OriginalFirstThunk+j*sizeof(DWORD);
      char symbol[sizeof("DirectDrawCreateEx")];
      if(!rva_range(rva,sizeof(thunk),nt.OptionalHeader.SizeOfImage) ||
         !rva_range(desc.FirstThunk+j*sizeof(DWORD),sizeof(DWORD),
                    nt.OptionalHeader.SizeOfImage) ||
         !remote_read(process,SPIDER_IMAGE_BASE+rva,&thunk,sizeof(thunk)))
        return 0;
      if(thunk.u1.AddressOfData==0){thunks_terminated=1;break;}
      /* fix.dll exports only DirectDrawCreateEx. Never alias a descriptor
       * containing a second method/ordinal that would fail PE loading. */
      if(IMAGE_SNAP_BY_ORDINAL32(thunk.u1.Ordinal))return 0;
      if(!rva_range(thunk.u1.AddressOfData+2u,sizeof(symbol),
                    nt.OptionalHeader.SizeOfImage) ||
         !remote_read(process,SPIDER_IMAGE_BASE+
           thunk.u1.AddressOfData+2u,symbol,sizeof(symbol)))
        return 0;
      if(memcmp(symbol,"DirectDrawCreateEx",sizeof(symbol))!=0 ||
         desc.FirstThunk+j*sizeof(DWORD)!=SPIDER_DDRAW_CREATE_IAT_RVA)
        return 0;
      matched++;
    }
    if(matched!=1 || !thunks_terminated)return 0;
    found++;
    found_name_rva=desc.Name;
  }
  if(found!=1 || !table_terminated ||
     !rva_range(found_name_rva,sizeof(original),
                            nt.OptionalHeader.SizeOfImage) ||
     !remote_read(process,SPIDER_IMAGE_BASE+found_name_rva,
                  original,sizeof(original)) ||
     original[9]!=0 || _stricmp(original,"ddraw.dll")!=0)return 0;
  return remote_write_verified(process,SPIDER_IMAGE_BASE+found_name_rva,
                               original,replacement,sizeof(replacement));
}
int SpiderUnified_PatchSuspendedNoCD(HANDLE process) {
  IMAGE_NT_HEADERS32 nt;
  BYTE old[14],call1[5],call2[5];
  static const BYTE original[14]={
    0x81,0xEC,0x20,0x02,0x00,0x00,0x56,
    0x8B,0xB4,0x24,0x28,0x02,0x00,0x00
  };
  static const BYTE expected1[5]={0xE8,0x2F,0x0A,0x00,0x00};
  static const BYTE expected2[5]={0xE8,0x56,0x06,0x00,0x00};
  static const BYTE bypass[3]={0x31,0xC0,0xC3};
  if(!remote_supported_pe32(process,&nt) ||
     !rva_range(0x001163E0u,sizeof(old),nt.OptionalHeader.SizeOfImage) ||
     !rva_range(0x001159ACu,sizeof(call1),nt.OptionalHeader.SizeOfImage) ||
     !rva_range(0x00115D85u,sizeof(call2),nt.OptionalHeader.SizeOfImage) ||
     !remote_read(process,SPIDER_IMAGE_BASE+0x001163E0u,old,sizeof(old)) ||
     !remote_read(process,SPIDER_IMAGE_BASE+0x001159ACu,call1,sizeof(call1)) ||
     !remote_read(process,SPIDER_IMAGE_BASE+0x00115D85u,call2,sizeof(call2)) ||
     memcmp(old,original,sizeof(old)) ||
     memcmp(call1,expected1,sizeof(call1)) ||
     memcmp(call2,expected2,sizeof(call2)))return 0;
  return remote_write_verified(process,SPIDER_IMAGE_BASE+0x001163E0u,
                               old,bypass,sizeof(bypass));
}
