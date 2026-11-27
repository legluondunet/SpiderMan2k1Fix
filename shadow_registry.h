#ifndef SPIDER_SHADOW_REGISTRY_H
#define SPIDER_SHADOW_REGISTRY_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
/* Read-only transcription of the SHA-pinned native named lookup, excluding
 * its error/fallback resource. 0x50F3C0 uses index * 13 * 8, NOT *60. */
#define SPIDER_SHADOW_NAMES 0x006a90b8u
#define SPIDER_SHADOW_BUCKETS 0x006ab934u
#define SPIDER_SHADOW_SURFACES 0x00ac12f4u
#define SPIDER_SHADOW_NAME_COUNT 256u
#define SPIDER_SHADOW_NAME_STRIDE 40u
#define SPIDER_SHADOW_SURFACE_STRIDE 104u
typedef int (*SpiderShadowRead)(uintptr_t,void*,size_t);
typedef struct SpiderShadowRegistry {
  uint32_t descriptor,index,surface;
} SpiderShadowRegistry;
static inline uint32_t SpiderShadowRegistry_U32(const unsigned char *p){
  uint32_t v;memcpy(&v,p,4);return v;
}
static inline int SpiderShadowRegistry_Name(const unsigned char *entry){
  static const char name[]="shadow2";
  unsigned i;
  if(!entry[0])return 0;
  for(i=0;i<7u;i++){
    unsigned ch=entry[i+1];
    if(ch>='A' && ch<='Z')ch+='a'-'A';
    if(ch!=(unsigned char)name[i])return 0;
  }
  return entry[8]==0;
}
static inline int SpiderShadowRegistry_Find(SpiderShadowRead read,SpiderShadowRegistry *out){
  unsigned char names[SPIDER_SHADOW_NAME_COUNT*SPIDER_SHADOW_NAME_STRIDE],node[36];
  unsigned i,hops;
  uint32_t key=0,p=0,surface=0;
  uint16_t index=0;
  if(!read || !out)return 0;
  memset(out,0,sizeof(*out));
  if(!read(SPIDER_SHADOW_NAMES,names,sizeof(names)))return 0;
  for(i=0;i<SPIDER_SHADOW_NAME_COUNT;i++)
    if(SpiderShadowRegistry_Name(names+i*SPIDER_SHADOW_NAME_STRIDE)){
      key=SpiderShadowRegistry_U32(names+i*SPIDER_SHADOW_NAME_STRIDE+36u);
      break; /* same first active exact name as native lookup */
    }
  if(i==SPIDER_SHADOW_NAME_COUNT)return 0;
  if(!read(SPIDER_SHADOW_BUCKETS+(key&511u)*4u,&p,4))return 0;
  for(hops=0;p && hops<512u;hops++){
    if(!read(p,node,sizeof(node)))return 0;
    if(SpiderShadowRegistry_U32(node+20)==key){
      memcpy(&index,node+2,2);
      if(index<=2u || index>=1024u ||
         !read(SPIDER_SHADOW_SURFACES+(uintptr_t)index*SPIDER_SHADOW_SURFACE_STRIDE,&surface,4) ||
         !surface)return 0;
      out->descriptor=p;out->index=index;out->surface=surface;return 1;
    }
    {
      uint32_t next=SpiderShadowRegistry_U32(node+32);
      if(next==p)return 0;
      p=next;
    }
  }
  return 0;
}
#endif
