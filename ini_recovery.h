#ifndef SPIDERMAN2K1FIX_INI_RECOVERY_H
#define SPIDERMAN2K1FIX_INI_RECOVERY_H
#include "ini_validation.h"

/* Recover the FIRST VALID value of each supported key, across duplicate
 * sections/keys. Malformed, unknown and binary input never becomes a setting.
 * This independent parser does not rely on Win32's ambiguous duplicate lookup.
 * Parse first; the calling repair routine then backs up the original
 * before installing any replacement.
 */
#define SPIDER_INI_RECOVER_MAX 192
#define SPIDER_INI_RECOVER_VALUE 520
typedef struct {
  char section[32],key[64],value[SPIDER_INI_RECOVER_VALUE];
} SpiderIniRecoveredEntry;
typedef struct {
  SpiderIniRecoveredEntry entries[SPIDER_INI_RECOVER_MAX];
  unsigned count;
} SpiderIniRecovered;
static int SpiderIni_Recover(const char *path,SpiderIniRecovered *out) {
  FILE *file; char line[SPIDER_INI_MAX_LINE],section[32]="";
  unsigned line_number=0;
  int too_long=0,has_nul=0,known_section=0;
  if(!out)return 0;
  memset(out,0,sizeof(*out));
  file=fopen(path,"rb");if(!file)return 0;
  while(spider_ini_readline(file,line,&too_long,&has_nul)) {
    char *s,*sep,*end;unsigned i;size_t len;
    if(++line_number>SPIDER_INI_MAX_LINES)break;
    if(too_long||has_nul) {section[0]=0;known_section=0;continue;}
    len=strlen(line);
    if(line_number==1 && len>=3 && (unsigned char)line[0]==0xEF &&
       (unsigned char)line[1]==0xBB && (unsigned char)line[2]==0xBF)
      memmove(line,line+3,len-2);
    if((unsigned char)line[0]==0xFF || (unsigned char)line[0]==0xFE)break;
    s=spider_ini_trim(line);
    if(!*s||*s==';')continue;
    if(*s=='[') {
      end=strchr(s,']');
      if(!end || *spider_ini_trim(end+1)) {section[0]=0;known_section=0;continue;}
      *end=0;s=spider_ini_trim(s+1);
      if(!*s||strlen(s)>=sizeof(section)){section[0]=0;known_section=0;continue;}
      strcpy(section,s);known_section=SPIDER_INI_IN(section,spider_ini_sections);
      continue;
    }
    if(!known_section)continue;
    sep=strchr(s,'=');if(!sep)continue;
    *sep=0;s=spider_ini_trim(s);sep=spider_ini_trim(sep+1);
    if(!*s||strlen(s)>=64||strlen(sep)>=SPIDER_INI_RECOVER_VALUE)continue;
    if(!spider_ini_key_known(section,s)||!spider_ini_value_valid(section,s,sep))continue;
    for(i=0;i<out->count;i++)if(!spider_ini_icmp(out->entries[i].section,section)&&
                                !spider_ini_icmp(out->entries[i].key,s))break;
    if(i<out->count)continue; /* First valid occurrence wins. */
    if(out->count>=SPIDER_INI_RECOVER_MAX)break;
    strcpy(out->entries[out->count].section,section);
    strcpy(out->entries[out->count].key,s);
    strcpy(out->entries[out->count].value,sep);
    out->count++;
  }
  if(ferror(file)){fclose(file);return 0;}
  fclose(file);return 1;
}

/* Repair overlays recovered values onto fresh defaults. Resolve the legacy
 * shadow opt-in before the new factory 0 can silently mask it. The parser
 * has already removed invalid values and duplicate entries. */
static inline int SpiderIni_RecoveredEnhancedShadows(const SpiderIniRecovered *values) {
  unsigned i;
  int feature=-1,legacy=0;
  if(!values)return 0;
  for(i=0;i<values->count;i++) {
    const SpiderIniRecoveredEntry *e=&values->entries[i];
    if(!spider_ini_icmp(e->section,"Features") &&
       !spider_ini_icmp(e->key,"EnhancedShadows"))
      feature=strtoul(e->value,NULL,10)==1;
    else if(!spider_ini_icmp(e->section,"Debug") &&
            !spider_ini_icmp(e->key,"ExperimentalHDShadows"))
      legacy=strtoul(e->value,NULL,10)==1;
  }
  return feature<0?legacy:feature;
}

#endif
