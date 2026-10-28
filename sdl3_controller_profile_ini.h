/* Portable, strict v2 SDL3 model-profile serializer/parser.
 * Reads legacy version 1 without changing its eight gameplay bindings;
 * initializes five new, independently contextual menu/media defaults.
 * Writes v2 on the next explicitly confirmed binding change/reset.
 * No disk I/O, Windows APIs or input injection. All versions fail closed
 * on corruption, unknown fields, duplicates and incorrect VID/PID. */
#ifndef SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_INI_H
#define SPIDERMAN2K1FIX_SDL3_CONTROLLER_PROFILE_INI_H
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "sdl3_controller_profile.h"
#define SPIDER_SDL3_PROFILE_INI_MAX 2048u
static inline int spider_sdl3_ini_hex(char c) {
  if(c>='0'&&c<='9')return c-'0';
  if(c>='a'&&c<='f')return c-'a'+10;
  if(c>='A'&&c<='F')return c-'A'+10;
  return -1;
}
static inline int spider_sdl3_ini_hex4(const char *s,uint16_t *out) {
  unsigned i,v=0;
  if(!s || !out || strlen(s)!=6u || s[0]!='0' ||
     (s[1]!='x'&&s[1]!='X'))return 0;
  for(i=2;i<6;i++) {
    int h=spider_sdl3_ini_hex(s[i]);
    if(h<0)return 0;
    v=(v<<4)|(unsigned)h;
  }
  *out=(uint16_t)v;return 1;
}
static inline int spider_sdl3_ini_binding(const char *s,
                                          SpiderSDL3Binding *out) {
  unsigned n=0,i;
  const char *p;
  SpiderSDL3Binding b;
  if(!s || !out)return 0;
  if(!strcmp(s,"none")) {
    b=spider_sdl3_bind(0,0);
    *out=b;return 1;
  }
  if(!strncmp(s,"button:",7)) {b.kind=SPIDER_SDL3_BIND_BUTTON;p=s+7;}
  else if(!strncmp(s,"trigger:",8)) {
    b.kind=SPIDER_SDL3_BIND_TRIGGER;p=s+8;
  } else return 0;
  if(!*p || (p[0]=='0' && p[1]))return 0;
  for(i=0;p[i];i++) {
    if(i>2 || p[i]<'0'||p[i]>'9')return 0;
    n=n*10u+(unsigned)(p[i]-'0');
    if(n>255u)return 0;
  }
  b.index=(uint8_t)n;
  if(!spider_sdl3_binding_valid(b))return 0;
  *out=b;
  return 1;
}
static inline int spider_sdl3_ini_encode(
    const SpiderSDL3ControllerProfile *p,char *out,size_t cap,
    size_t *written) {
  size_t used=0;
  unsigned i;
  int n;
  if(written)*written=0;
  if(!out || !cap || !spider_sdl3_profile_valid(p))return 0;
  n=snprintf(out,cap,"[Profile]\r\nVersion=2\r\nVendor=0x%04X\r\nProduct=0x%04X\r\n[Bindings]\r\n",
      (unsigned)p->vendor,(unsigned)p->product);
  if(n<0 || (size_t)n>=cap)return 0;
  used=(size_t)n;
  for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++) {
    SpiderSDL3Binding b=p->actions[i];
    const char *kind=b.kind==SPIDER_SDL3_BIND_BUTTON?"button":
                     b.kind==SPIDER_SDL3_BIND_TRIGGER?"trigger":"none";
    n=b.kind==SPIDER_SDL3_BIND_NONE?
      snprintf(out+used,cap-used,"%s=none\r\n",spider_sdl3_action_names[i]):
      snprintf(out+used,cap-used,"%s=%s:%u\r\n",
        spider_sdl3_action_names[i],kind,(unsigned)b.index);
    if(n<0 || (size_t)n>=cap-used)return 0;
    used+=(size_t)n;
  }
  n=snprintf(out+used,cap-used,"[MenuBindings]\\r\\n");
  if(n<0 || (size_t)n>=cap-used)return 0;
  used+=(size_t)n;
  for(i=0;i<SPIDER_SDL3_CONTEXT_COUNT;i++) {
    SpiderSDL3Binding b=p->context[i];
    const char *kind=b.kind==SPIDER_SDL3_BIND_BUTTON?"button":
                     b.kind==SPIDER_SDL3_BIND_TRIGGER?"trigger":"none";
    n=b.kind==SPIDER_SDL3_BIND_NONE?
      snprintf(out+used,cap-used,"%s=none\\r\\n",spider_sdl3_context_names[i]):
      snprintf(out+used,cap-used,"%s=%s:%u\\r\\n",
        spider_sdl3_context_names[i],kind,(unsigned)b.index);
    if(n<0 || (size_t)n>=cap-used)return 0;
    used+=(size_t)n;
  }
  if(used>SPIDER_SDL3_PROFILE_INI_MAX)return 0;
  if(written)*written=used;
  return 1;
}
static inline int spider_sdl3_ini_decode(
    const char *data,size_t len,uint16_t vid,uint16_t pid,
    SpiderSDL3ControllerProfile *result) {
  char copy[SPIDER_SDL3_PROFILE_INI_MAX+1],*cursor,*end;
  SpiderSDL3ControllerProfile trial;
  unsigned seen=0,section=0,version=0;
  const unsigned gameplay_mask=(1u<<(SPIDER_SDL3_ACTION_COUNT+3u))-1u;
  const unsigned v2_mask=(1u<<(SPIDER_SDL3_SETUP_ACTION_COUNT+3u))-1u;
  uint16_t found_vid=0,found_pid=0;
  unsigned i;
  if(!data || !result || !vid || !pid ||
     len==0 || len>SPIDER_SDL3_PROFILE_INI_MAX ||
     memchr(data,0,len)!=NULL)return 0;
  memcpy(copy,data,len);copy[len]=0;
  if(!spider_sdl3_profile_default(&trial,vid,pid))return 0;
  cursor=copy;
  while(*cursor) {
    char *line=cursor,*eq;
    end=strchr(cursor,'\n');
    if(end){*end=0;cursor=end+1;}else cursor+=strlen(cursor);
    end=line+strlen(line);
    if(end>line && end[-1]=='\r')*--end=0;
    if(!*line || line[0]==';' || line[0]=='#')continue;
    if(!strcmp(line,"[Profile]")) {
      if(section || (seen&7u))return 0;
      section=1;continue;
    }
    if(!strcmp(line,"[Bindings]")) {
      if(section!=1 || (seen&7u)!=7u)return 0;
      section=2;continue;
    }
    if(!strcmp(line,"[MenuBindings]")) {
      if(version!=2u || section!=2 || seen!=gameplay_mask)return 0;
      section=3;continue;
    }
    eq=strchr(line,'=');
    if(!eq || strchr(eq+1,'=') || eq==line || !eq[1])return 0;
    *eq++=0;
    if(section==1) {
      unsigned bit=0;
      if(!strcmp(line,"Version")) {
        bit=1u;
        if(!strcmp(eq,"1"))version=1u;
        else if(!strcmp(eq,"2"))version=2u;
        else return 0;
      } else if(!strcmp(line,"Vendor")) {
        bit=2u;if(!spider_sdl3_ini_hex4(eq,&found_vid) ||
                  found_vid!=vid)return 0;
      } else if(!strcmp(line,"Product")) {
        bit=4u;if(!spider_sdl3_ini_hex4(eq,&found_pid) ||
                  found_pid!=pid)return 0;
      } else return 0;
      if(seen&bit)return 0;
      seen|=bit;
    } else if(section==2) {
      unsigned index=SPIDER_SDL3_ACTION_COUNT;
      SpiderSDL3Binding bind;
      for(i=0;i<SPIDER_SDL3_ACTION_COUNT;i++)
        if(!strcmp(line,spider_sdl3_action_names[i])) {
          index=i;break;
        }
      if(index==SPIDER_SDL3_ACTION_COUNT ||
         (seen&(1u<<(index+3u))) ||
         !spider_sdl3_ini_binding(eq,&bind))return 0;
      trial.actions[index]=bind;
      seen|=1u<<(index+3u);
    } else if(section==3) {
      unsigned index=SPIDER_SDL3_CONTEXT_COUNT;
      SpiderSDL3Binding bind;
      for(i=0;i<SPIDER_SDL3_CONTEXT_COUNT;i++)
        if(!strcmp(line,spider_sdl3_context_names[i])) {
          index=i;break;
        }
      if(index==SPIDER_SDL3_CONTEXT_COUNT ||
         (seen&(1u<<(index+SPIDER_SDL3_ACTION_COUNT+3u))) ||
         !spider_sdl3_ini_binding(eq,&bind))return 0;
      trial.context[index]=bind;
      seen|=1u<<(index+SPIDER_SDL3_ACTION_COUNT+3u);
    } else return 0;
  }
  if(!((version==1u && section==2 && seen==gameplay_mask) ||
       (version==2u && section==3 && seen==v2_mask)) ||
     !spider_sdl3_profile_valid(&trial))return 0;
  *result=trial;
  return 1;
}
#endif
