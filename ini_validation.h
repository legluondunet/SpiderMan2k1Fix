#ifndef SPIDERMAN2K1FIX_INI_VALIDATION_H
#define SPIDERMAN2K1FIX_INI_VALIDATION_H
/* Read-only INI preflight. Standalone C99: it does not rewrite the user's file.
 * Built into SpiderMan2k1Fix.dll via virtual_spidey_cfg.c; testable on Linux.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#define SPIDER_INI_MAX_LINES 1024
#define SPIDER_INI_MAX_KEYS 192
#define SPIDER_INI_MAX_LINE 4096
#define SPIDER_INI_MAX_REPORT 96

typedef struct {
  unsigned errors, warnings;
  unsigned reported;
  int safe_to_save;
} SpiderIniResult;
typedef void (*SpiderIniIssue)(void *context, unsigned line, int error, const char *text);
typedef struct { char section[32], key[64]; unsigned line; } SpiderIniSeenKey;

static int spider_ini_icmp(const char *a,const char *b) {
  unsigned char x,y;
  do {
    x=(unsigned char)*a++; y=(unsigned char)*b++;
    x=(unsigned char)tolower(x); y=(unsigned char)tolower(y);
    if(x!=y)return x<y?-1:1;
  } while(x && y);
  return 0;
}
static char *spider_ini_trim(char *s) {
  char *end;
  while(*s==' '||*s=='\t'||*s=='\r'||*s=='\n')++s;
  end=s+strlen(s);
  while(end>s && (end[-1]==' '||end[-1]=='\t'||end[-1]=='\r'||end[-1]=='\n'))--end;
  *end=0;
  return s;
}
static int spider_ini_in(const char *name,const char *const *items,size_t count) {
  size_t i;
  for(i=0;i<count;i++)if(!spider_ini_icmp(name,items[i]))return 1;
  return 0;
}
#define SPIDER_INI_IN(value,array) spider_ini_in(value,array,sizeof(array)/sizeof((array)[0]))
static const char *const spider_ini_sections[]={"Debug","System","Display","HUD","Input","Audio","Keyboard","Joystick","Internal"};
/* Native two-cutscene F12 auto-skip opt-in: never silently discard it
 * during INI preflight/repair. The generic Debug validator below
 * already enforces strict boolean values (0/1). */
/* F10 is the hotkey. F12-named keys are legacy INI aliases ONLY. */
static const char *const spider_ini_debug[]={"Log","F10Shortcut","F10TestMode","F10Auto3DScripts","F10GameplayExit","F10VerboseTrace","F12TestMode","F12Auto3DScripts"};
/* Keep all pre-existing INIs valid without regenerating saved controls. */
static const char *const spider_ini_system[]={"QuickExitF10","ExperimentalQuickExitF10In3D","ExperimentalQuickExitF10InNewGameMovie","ExperimentalQuickExitF10InNewGameComic","ExperimentalQuickExitF10InIntroStills","QuickExitF12","ExperimentalQuickExitF12In3D","ExperimentalQuickExitF12InNewGameMovie","ExperimentalQuickExitF12InNewGameComic","ExperimentalQuickExitF12InIntroStills"};
static const char *const spider_ini_display[]={
  "Resolution","AspectRatio","Brightness","FPSLimit","FOV",
  "ExperimentalFOVAspect","ExperimentalFOVHorPlus",
  "ExperimentalMenu4x3","ExperimentalMediaAspect","ExperimentalMediaBlt4x3",
  "ExperimentalMediaBltKnownSizes","ExperimentalIntroSourceAspect","ExperimentalMediaBltAnySize",
  "ExperimentalComicNativeAspect",
  "ExperimentalDisplayMenuLock"
};
/* Hidden debug opt-outs for the native ESC 4:3 children.
 * All are default ON under the HUDWidescreenFix master, but must
 * validate and survive INI recovery if explicitly set to 0.
 * Never emit these keys in a generated/default INI. */
static const char *const spider_ini_hud[]={
  "HUDWidescreenFix","NativeESC4x3","NativeMotif4x3","NativeCursor4x3"
};
/* Retain two obsolete COM keys for backward compatibility: old user INIs
 * must NOT be treated as invalid and regenerated, losing saved controls.
 * The runtime refuses both unsafe features regardless of their values.
 * Neither obsolete key is generated or documented as an active option. */
static const char *const spider_ini_input[]={
  "KeyboardLayout","DirectInputReacquire","DisableWindowsGhosting","PauseOnFocusLoss","PauseBinkOnFocusLoss","PauseDirectSoundOnFocusLoss","PauseOverlay","PauseDim","PauseDimOpacity","PauseKey",
  "RestoreCursorOnFocusLoss","ExperimentalFocusTrace","ExperimentalDI8EntryTrace",
  "ExperimentalAltTabReacquire","ExperimentalDI8CreateDeviceTrace"
};
static const char *const spider_ini_audio[]={"MusicSFXLevel","VoiceLevel","MovieLevel","Stereo","ExperimentalMusicStutterFix"};
static const char *const spider_ini_keys[]={"Up","Down","Left","Right","Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const char *const spider_ini_joy[]={"Jump","Kick","Punch","Web","Aim","Zipline","Swing"};
static const char *const spider_ini_internal[]={"RawHex"};
static int spider_ini_key_known(const char *section,const char *key) {
  if(!spider_ini_icmp(section,"Debug"))return SPIDER_INI_IN(key,spider_ini_debug);
  if(!spider_ini_icmp(section,"System"))return SPIDER_INI_IN(key,spider_ini_system);
  if(!spider_ini_icmp(section,"Display"))return SPIDER_INI_IN(key,spider_ini_display);
  if(!spider_ini_icmp(section,"HUD"))return SPIDER_INI_IN(key,spider_ini_hud);
  if(!spider_ini_icmp(section,"Input"))return SPIDER_INI_IN(key,spider_ini_input);
  if(!spider_ini_icmp(section,"Audio"))return SPIDER_INI_IN(key,spider_ini_audio);
  if(!spider_ini_icmp(section,"Keyboard"))return SPIDER_INI_IN(key,spider_ini_keys);
  if(!spider_ini_icmp(section,"Joystick"))return SPIDER_INI_IN(key,spider_ini_joy);
  if(!spider_ini_icmp(section,"Internal"))return SPIDER_INI_IN(key,spider_ini_internal);
  return 0;
}
static int spider_ini_uint(const char *s,unsigned long min,unsigned long max) {
  char *end; unsigned long n;
  if(!*s || *s=='-' || *s=='+')return 0;
  errno=0; n=strtoul(s,&end,10);
  return !errno && *end==0 && end!=s && n>=min && n<=max;
}
static int spider_ini_hex32(const char *s) {
  unsigned long n; char *end;
  if(s[0]!='0' || (s[1]!='x' && s[1]!='X') || !isxdigit((unsigned char)s[2]))return 0;
  errno=0; n=strtoul(s+2,&end,16);
  return !errno && *end==0 && n<=0xFFFFFFFFUL;
}
static int spider_ini_keyboard(const char *value) {
  static const char *const extras[]={"UP","DOWN","LEFT","RIGHT","SPACE","ESC","ENTER","TAB",
    "BACKSPACE","LSHIFT","RSHIFT","LCTRL","RCTRL","LALT","RALT","HOME","END",
    "DELETE","INSERT","PAGEUP","PAGEDOWN"};
  size_t i;
  if(spider_ini_hex32(value)||SPIDER_INI_IN(value,extras))return 1;
  if(strlen(value)==1 && ((value[0]>='A'&&value[0]<='Z') ||
     (value[0]>='a'&&value[0]<='z') || (value[0]>='0'&&value[0]<='9')))return 1;
  for(i=1;i<=12;i++) {char f[5];snprintf(f,sizeof(f),"F%u",(unsigned)i);
    if(!spider_ini_icmp(value,f))return 1;
  }
  return 0;
}
static int spider_ini_joystick(const char *value) {
  if(spider_ini_hex32(value))return 1;
  if(strlen(value)<7)return 0;
  if(toupper((unsigned char)value[0])!='B'||toupper((unsigned char)value[1])!='U'||
     toupper((unsigned char)value[2])!='T'||toupper((unsigned char)value[3])!='T'||
     toupper((unsigned char)value[4])!='O'||toupper((unsigned char)value[5])!='N')return 0;
  return spider_ini_uint(value+6,0,127);
}
static int spider_ini_resolution(const char *value) {
  unsigned w,h; char trailing;
  if(!spider_ini_icmp(value,"Auto"))return 1;
  return sscanf(value,"%ux%u%c",&w,&h,&trailing)==2 &&
     w>=640 && h>=480 && w<=16384 && h<=16384;
}
static int spider_ini_value_valid(const char *section,const char *key,const char *value) {
  size_t i;
  if(!spider_ini_icmp(section,"Internal")) {
    if(spider_ini_icmp(key,"RawHex"))return 1;
    if(strlen(value)!=504)return 0;
    for(i=0;i<504;i++)if(!isxdigit((unsigned char)value[i]))return 0;
    return 1;
  }
  if(!spider_ini_icmp(section,"Keyboard"))return spider_ini_keyboard(value);
  if(!spider_ini_icmp(section,"Joystick"))return spider_ini_joystick(value);
  if(!spider_ini_icmp(section,"Audio"))return spider_ini_uint(value,0,
           (!spider_ini_icmp(key,"Stereo") || !spider_ini_icmp(key,"ExperimentalMusicStutterFix"))?1:100);
  /* Only Auto and Off are public modes. Accept older names solely so
   * the loader can migrate them without resetting saved controls/RawHex. */
  if(!spider_ini_icmp(section,"Input") && !spider_ini_icmp(key,"KeyboardLayout"))
    return !spider_ini_icmp(value,"Auto") ||
           !spider_ini_icmp(value,"Off") ||
           !spider_ini_icmp(value,"Legacy") ||
           !spider_ini_icmp(value,"Physical") ||
           !spider_ini_icmp(value,"QWERTY") ||
           !spider_ini_icmp(value,"AZERTY");
  if(!spider_ini_icmp(section,"Input") && !spider_ini_icmp(key,"PauseDimOpacity"))
    return spider_ini_uint(value,0,90);
  /* Deprecated key: continue accepting old INI values to avoid resetting
   * existing user bindings/configurations. Runtime ignores this entirely
   * and exclusively uses the consumed Any-key hook. */
  if(!spider_ini_icmp(section,"Input") && !spider_ini_icmp(key,"PauseKey"))
    return !spider_ini_icmp(value,"F9") || !spider_ini_icmp(value,"Space") ||
           !spider_ini_icmp(value,"Any");
  if(!spider_ini_icmp(section,"Debug")||!spider_ini_icmp(section,"System")||
     !spider_ini_icmp(section,"Input")||
     !spider_ini_icmp(section,"HUD"))return spider_ini_uint(value,0,1);
  if(!spider_ini_icmp(section,"Display")) {
    if(!spider_ini_icmp(key,"Resolution"))return spider_ini_resolution(value);
    if(!spider_ini_icmp(key,"AspectRatio"))return !spider_ini_icmp(value,"Auto")||!strcmp(value,"4:3");
    if(!spider_ini_icmp(key,"FOV"))return *value!=0; /* Legacy diagnostic: not a camera angle. */
    if(!spider_ini_icmp(key,"Brightness"))return spider_ini_uint(value,0,9);
    if(!spider_ini_icmp(key,"FPSLimit"))return spider_ini_uint(value,0,240);
    return spider_ini_uint(value,0,1);
  }
  return 1;
}
static void spider_ini_issue(SpiderIniResult *r,SpiderIniIssue cb,void *ctx,
                             unsigned line,int error,const char *message) {
  if(error) {r->errors++;r->safe_to_save=0;} else r->warnings++;
  if(r->reported<SPIDER_INI_MAX_REPORT) { if(cb)cb(ctx,line,error,message);r->reported++; }
}
static int spider_ini_readline(FILE *file,char *line,int *too_long,int *has_nul) {
  int c=0,any=0;size_t used=0;
  *too_long=0;*has_nul=0;
  while((c=fgetc(file))!=EOF) {
    any=1;
    if(c=='\n')break;
    if(c==0)*has_nul=1;
    if(used+1<SPIDER_INI_MAX_LINE)line[used++]=(char)c;
    else *too_long=1;
  }
  line[used]=0;
  return any;
}
static SpiderIniResult SpiderIni_Validate(const char *path,SpiderIniIssue cb,void *ctx) {
  FILE *file; SpiderIniResult r={0,0,0,1};
  SpiderIniSeenKey keys[SPIDER_INI_MAX_KEYS];
  char sections[32][32],section[32]="",line[SPIDER_INI_MAX_LINE];
  unsigned key_count=0,section_count=0,line_number=0,i;
  int known_section=0,too_long=0,has_nul=0;
  file=fopen(path,"rb");
  if(!file) {spider_ini_issue(&r,cb,ctx,0,1,"Cannot open SpiderMan2k1Fix.ini");return r;}
  while(spider_ini_readline(file,line,&too_long,&has_nul)) {
    char *s,*sep,*end; size_t len;
    line_number++;
    if(too_long) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"Line too long (maximum 4095 characters)");
      continue;
    }
    if(has_nul) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"Binary NUL byte in INI text");
      continue;
    }
    len=strlen(line);
    if(line_number==1 && len>=3 && (unsigned char)line[0]==0xEF &&
        (unsigned char)line[1]==0xBB && (unsigned char)line[2]==0xBF)
      memmove(line,line+3,len-2);
    if((unsigned char)line[0]==0xFF || (unsigned char)line[0]==0xFE) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"UTF-16 is unsupported; save as ANSI/UTF-8 text");break;
    }
    s=spider_ini_trim(line);
    if(!*s || *s==';')continue;
    if(*s=='[') {
      end=strchr(s,']');
      if(!end || *spider_ini_trim(end+1)) {
        spider_ini_issue(&r,cb,ctx,line_number,1,"Malformed section header");
        section[0]=0;known_section=0;continue;
      }
      *end=0;s=spider_ini_trim(s+1);
      if(!*s || strlen(s)>=sizeof(section)) {
        spider_ini_issue(&r,cb,ctx,line_number,1,"Empty or excessively long section name");
        section[0]=0;known_section=0;continue;
      }
      for(i=0;i<section_count;i++)if(!spider_ini_icmp(sections[i],s))break;
      if(i<section_count)spider_ini_issue(&r,cb,ctx,line_number,1,"Duplicate section header");
      else if(section_count<32)strcpy(sections[section_count++],s);
      else spider_ini_issue(&r,cb,ctx,line_number,1,"Too many sections");
      strcpy(section,s);
      known_section=SPIDER_INI_IN(section,spider_ini_sections);
      if(!known_section) {
        char message[170];snprintf(message,sizeof(message),"Unknown section [%s] ignored (kept on disk)",section);
        spider_ini_issue(&r,cb,ctx,line_number,0,message);
      }
      continue;
    }
    sep=strchr(s,'=');
    if(!sep || !section[0]) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"Malformed key/value or key outside a section");continue;
    }
    *sep=0;s=spider_ini_trim(s);sep=spider_ini_trim(sep+1);
    if(!*s || strlen(s)>=64) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"Empty or excessively long key");continue;
    }
    if(!spider_ini_icmp(s,"RawHex") && spider_ini_icmp(section,"Internal"))
      spider_ini_issue(&r,cb,ctx,line_number,1,"RawHex must appear only in [Internal]");
    for(i=0;i<key_count;i++)if(!spider_ini_icmp(keys[i].section,section) &&
       !spider_ini_icmp(keys[i].key,s))break;
    if(i<key_count) {
      char message[170];snprintf(message,sizeof(message),"Duplicate key [%s] %s (previous line %u)",section,s,keys[i].line);
      spider_ini_issue(&r,cb,ctx,line_number,1,message);
    } else if(key_count<SPIDER_INI_MAX_KEYS) {
      strcpy(keys[key_count].section,section);
      strcpy(keys[key_count].key,s);keys[key_count++].line=line_number;
    } else spider_ini_issue(&r,cb,ctx,line_number,1,"Too many keys");
    if(!known_section)continue;
    if(!spider_ini_key_known(section,s)) {
      char message[170];snprintf(message,sizeof(message),"Unknown key [%s] %s ignored (kept on disk)",section,s);
      spider_ini_issue(&r,cb,ctx,line_number,0,message);
    } else if(!spider_ini_value_valid(section,s,sep)) {
      char message[170];snprintf(message,sizeof(message),"Invalid value for [%s] %s (use a supported value)",section,s);
      spider_ini_issue(&r,cb,ctx,line_number,1,message);
    }
    if(line_number>=SPIDER_INI_MAX_LINES) {
      spider_ini_issue(&r,cb,ctx,line_number,1,"Too many lines in INI");break;
    }
  }
  if(ferror(file))spider_ini_issue(&r,cb,ctx,0,1,"Error reading SpiderMan2k1Fix.ini");
  fclose(file);
  return r;
}

#endif
