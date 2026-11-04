/* Portable SDL3 raw joystick -> standardized gamepad mapping builder.
 * This module has NO SDL or Windows dependencies. The future keyboard-driven
 * capture UI supplies observed button/axis/hat bindings. A GUID is necessary
 * so a user-supplied mapping cannot accidentally target other controllers.
 *
 * Output follows SDL_AddGamepadMapping "GUID,name,key:bN,key:aN,key:hN.Y".
 * Do not call SDL_AddGamepadMapping until the user verifies each binding.
 */
#ifndef SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#define SPIDERMAN2K1FIX_SDL3_RAW_MAPPING_H
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

enum {
  SPIDER_RAW_MAP_A=0,SPIDER_RAW_MAP_B,SPIDER_RAW_MAP_X,SPIDER_RAW_MAP_Y,
  SPIDER_RAW_MAP_BACK,SPIDER_RAW_MAP_START,
  SPIDER_RAW_MAP_LEFTSHOULDER,SPIDER_RAW_MAP_RIGHTSHOULDER,
  SPIDER_RAW_MAP_LEFTSTICK,SPIDER_RAW_MAP_RIGHTSTICK,
  SPIDER_RAW_MAP_DPUP,SPIDER_RAW_MAP_DPDOWN,
  SPIDER_RAW_MAP_DPLEFT,SPIDER_RAW_MAP_DPRIGHT,
  SPIDER_RAW_MAP_LEFTX,SPIDER_RAW_MAP_LEFTY,
  SPIDER_RAW_MAP_RIGHTX,SPIDER_RAW_MAP_RIGHTY,
  SPIDER_RAW_MAP_LEFTTRIGGER,SPIDER_RAW_MAP_RIGHTTRIGGER,
  SPIDER_RAW_MAP_COUNT
};
enum {
  SPIDER_RAW_NONE=0,SPIDER_RAW_BUTTON=1,SPIDER_RAW_AXIS=2,
  SPIDER_RAW_HAT=3
};
typedef struct {
  unsigned kind,index,hat_value;
} SpiderSDL3RawInput;
typedef struct {
  SpiderSDL3RawInput input[SPIDER_RAW_MAP_COUNT];
} SpiderSDL3RawMapping;
static const char *const spider_sdl3_raw_keys[SPIDER_RAW_MAP_COUNT]={
  "a","b","x","y","back","start","leftshoulder","rightshoulder",
  "leftstick","rightstick","dpup","dpdown","dpleft","dpright",
  "leftx","lefty","rightx","righty","lefttrigger","righttrigger"
};
static inline int spider_sdl3_raw_input_valid(
    SpiderSDL3RawInput v) {
  if(v.kind==SPIDER_RAW_NONE)return v.index==0 && v.hat_value==0;
  if(v.kind==SPIDER_RAW_BUTTON)
    return v.index<256u && !v.hat_value;
  if(v.kind==SPIDER_RAW_AXIS)
    return v.index<256u && !v.hat_value;
  if(v.kind==SPIDER_RAW_HAT)
    return v.index<16u && (v.hat_value==1u ||
      v.hat_value==2u || v.hat_value==4u || v.hat_value==8u);
  return 0;
}
static inline int spider_sdl3_guid_valid(const char *guid) {
  unsigned i;
  if(!guid || strlen(guid)!=32u)return 0;
  for(i=0;i<32u;i++)
    if(!isxdigit((unsigned char)guid[i]))return 0;
  return 1;
}
static inline int spider_sdl3_raw_name_safe(const char *name) {
  size_t i,n;
  if(!name || !(n=strlen(name)) || n>120u)return 0;
  for(i=0;i<n;i++)
    if((unsigned char)name[i]<32u || name[i]==',' ||
       name[i]=='\n' || name[i]=='\r')return 0;
  return 1;
}
static inline int spider_sdl3_raw_build(const char *guid,const char *name,
    const SpiderSDL3RawMapping *mapping,char *dst,size_t cap) {
  size_t length=0;
  unsigned i,count=0;
  int n;
  if(!dst || !cap)return 0;
  dst[0]=0;
  if(!mapping || !spider_sdl3_guid_valid(guid) ||
     !spider_sdl3_raw_name_safe(name))return 0;
  n=snprintf(dst,cap,"%s,%s,",guid,name);
  if(n<0 || (size_t)n>=cap) {dst[0]=0;return 0;}
  length=(size_t)n;
  for(i=0;i<SPIDER_RAW_MAP_COUNT;i++) {
    SpiderSDL3RawInput input=mapping->input[i];
    if(!spider_sdl3_raw_input_valid(input))goto fail;
    if(input.kind==SPIDER_RAW_NONE)continue;
    if(input.kind==SPIDER_RAW_BUTTON)
      n=snprintf(dst+length,cap-length,"%s:b%u,",
                  spider_sdl3_raw_keys[i],input.index);
    else if(input.kind==SPIDER_RAW_AXIS)
      n=snprintf(dst+length,cap-length,"%s:a%u,",
                  spider_sdl3_raw_keys[i],input.index);
    else
      n=snprintf(dst+length,cap-length,"%s:h%u.%u,",
                  spider_sdl3_raw_keys[i],input.index,input.hat_value);
    if(n<0 || (size_t)n>=cap-length)goto fail;
    length+=(size_t)n;
    count++;
  }
  /* A mapping without a usable action button and two axes is not
   * something we should present as a calibrated modern gamepad. */
  if(!count || mapping->input[SPIDER_RAW_MAP_A].kind==SPIDER_RAW_NONE ||
     mapping->input[SPIDER_RAW_MAP_B].kind==SPIDER_RAW_NONE ||
     mapping->input[SPIDER_RAW_MAP_LEFTX].kind!=SPIDER_RAW_AXIS ||
     mapping->input[SPIDER_RAW_MAP_LEFTY].kind!=SPIDER_RAW_AXIS)goto fail;
  return 1;
fail:
  dst[0]=0;
  return 0;
}
#endif
