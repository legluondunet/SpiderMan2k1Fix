#ifndef SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_POLICY_H
#define SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_POLICY_H
/* Portable SDL3 raw joystick -> normalized gamepad mapping policy.
 * No platform/DirectInput IDs, driver guesses, or Windows dependencies. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum { SPIDER_RAW_BIND_COUNT=20, SPIDER_RAW_BIND_CAP=20 };
enum { SPIDER_RAW_NONE=0, SPIDER_RAW_BUTTON=1,
       SPIDER_RAW_AXIS=2, SPIDER_RAW_HAT=3 };
typedef struct {
  unsigned char kind,index,hat_value;
} SpiderRawBinding;
static const char *const spider_raw_sdl_names[SPIDER_RAW_BIND_COUNT]={
  "a","b","x","y","leftshoulder","rightshoulder",
  "back","start","leftx","lefty","rightx","righty",
  "lefttrigger","righttrigger","dpup","dpdown","dpleft","dpright",
  "leftstick","rightstick"
};
static const char *const spider_raw_display_names[SPIDER_RAW_BIND_COUNT]={
  "SOUTH / A / Cross","EAST / B / Circle","WEST / X / Square",
  "NORTH / Y / Triangle","Left shoulder / L1","Right shoulder / R1",
  "Back / Share / Select","Start / Options",
  "Move LEFT STICK horizontally","Move LEFT STICK vertically",
  "Move RIGHT STICK horizontally","Move RIGHT STICK vertically",
  "Left trigger / L2","Right trigger / R2",
  "D-pad UP","D-pad DOWN","D-pad LEFT","D-pad RIGHT",
  "Click LEFT STICK / L3","Click RIGHT STICK / R3"
};
static inline int spider_raw_required(unsigned step) {
  /* All seven native gameplay actions, movement and Pause/selection. */
  /* All seven gameplay commands need their default SDL buttons:
   * four faces, both shoulders and RT. Back supplies Pause; Start
   * selects the pad and the left stick supplies movement. */
  return step<=9u || step==13u;
}
static inline int spider_raw_binding_valid(SpiderRawBinding b) {
  return b.kind==SPIDER_RAW_NONE ||
         (b.kind==SPIDER_RAW_BUTTON && b.index<64u) ||
         (b.kind==SPIDER_RAW_AXIS && b.index<16u) ||
         (b.kind==SPIDER_RAW_HAT && b.index<8u &&
          (b.hat_value==1u || b.hat_value==2u ||
           b.hat_value==4u || b.hat_value==8u));
}
static inline int spider_raw_binding_same(SpiderRawBinding a,
                                          SpiderRawBinding b) {
  return a.kind==b.kind && a.index==b.index &&
         (a.kind!=SPIDER_RAW_HAT || a.hat_value==b.hat_value);
}
static inline int spider_raw_bind_assign(SpiderRawBinding *slots,
                                         unsigned step,SpiderRawBinding input) {
  unsigned i;
  if(!slots || step>=SPIDER_RAW_BIND_COUNT ||
     !spider_raw_binding_valid(input))return 0;
  if(input.kind==SPIDER_RAW_NONE && spider_raw_required(step))return 0;
  if(input.kind!=SPIDER_RAW_NONE)for(i=0;i<SPIDER_RAW_BIND_COUNT;i++)
    if(i!=step && slots[i].kind!=SPIDER_RAW_NONE &&
       spider_raw_binding_same(slots[i],input))return 0;
  slots[step]=input;
  return 1;
}
static inline int spider_raw_mapping_text(const char *guid,const char *name,
    const SpiderRawBinding *slots,char *dst,size_t cap) {
  unsigned i;
  size_t len;
  int n;
  if(!guid || strlen(guid)!=32u || !slots || !dst || cap<70u)return 0;
  for(i=0;i<32u;i++)
    if(!((guid[i]>='0' && guid[i]<='9') ||
         (guid[i]>='a' && guid[i]<='f') ||
         (guid[i]>='A' && guid[i]<='F')))return 0;
  n=snprintf(dst,cap,"%s,",guid);
  if(n<=0 || (size_t)n>=cap)return 0;
  len=(size_t)n;
  if(!name || !*name)name="Custom SDL3 Controller";
  while(*name && len+2u<cap) {
    char c=*name++;
    /* Commas and newlines would alter SDL's mapping fields. */
    if(c==',' || c=='\r' || c=='\n' || c=='\\')c='_';
    dst[len++]=c;
  }
  if(*name)return 0;
  dst[len++]=',';
  dst[len]=0;
  for(i=0;i<SPIDER_RAW_BIND_COUNT;i++) {
    SpiderRawBinding b=slots[i];
    if(!spider_raw_binding_valid(b) ||
       (spider_raw_required(i) && b.kind==SPIDER_RAW_NONE))return 0;
    if(b.kind==SPIDER_RAW_NONE)continue;
    if(b.kind==SPIDER_RAW_BUTTON)
      n=snprintf(dst+len,cap-len,"%s:b%u,",spider_raw_sdl_names[i],b.index);
    else if(b.kind==SPIDER_RAW_AXIS)
      n=snprintf(dst+len,cap-len,"%s:a%u,",spider_raw_sdl_names[i],b.index);
    else
      n=snprintf(dst+len,cap-len,"%s:h%u.%u,",spider_raw_sdl_names[i],
                 b.index,b.hat_value);
    if(n<=0 || (size_t)n>=cap-len)return 0;
    len+=(size_t)n;
  }
  if(!len || dst[len-1]!=',')return 0;
  dst[len-1]=0; /* no trailing delimiter */
  return 1;
}
#endif
