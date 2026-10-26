#ifndef SPIDERMAN2K1FIX_LOOSE_FILES_PATH_H
#define SPIDERMAN2K1FIX_LOOSE_FILES_PATH_H
/* Portable path policy for original PKR resource names.
 * Win32 loader additionally denies directory junctions and symlinks.
 */
#include <stddef.h>
#include <string.h>

static int spider_loose_fold(char c) {
  return c>='A' && c<='Z' ? c-'A'+'a' : c;
}
static int spider_loose_device(const char *s,size_t n) {
  static const char *const names[]={"con","prn","aux","nul"};
  size_t i,j,stem=0;
  while(stem<n && s[stem]!='.')stem++;
  for(i=0;i<sizeof(names)/sizeof(names[0]);i++){
    for(j=0;j<stem && names[i][j] && spider_loose_fold(s[j])==names[i][j];j++);
    if(j==stem && !names[i][j])return 1;
  }
  if(stem==4 && s[3]>='1' && s[3]<='9' &&
     ((spider_loose_fold(s[0])=='c' && spider_loose_fold(s[1])=='o' &&
       spider_loose_fold(s[2])=='m') ||
      (spider_loose_fold(s[0])=='l' && spider_loose_fold(s[1])=='p' &&
       spider_loose_fold(s[2])=='t')))return 1;
  return 0;
}
static int spider_loose_append(char *out,size_t cap,size_t *used,
                               const char *component,int allow_trailing) {
  const char *p=component,*begin;
  if(!p)return 0;
  if(!*p)return allow_trailing;
  if(*p=='\\'||*p=='/')return 0;
  while(*p){
    size_t n;
    begin=p;
    while(*p && *p!='\\' && *p!='/'){
      unsigned char c=(unsigned char)*p;
      if(c<32 || c==127 || c==':' || c=='<' || c=='>' ||
         c=='"' || c=='|' || c=='?' || c=='*')return 0;
      p++;
    }
    n=(size_t)(p-begin);
    if(!n || n>240 ||
       (n==1 && begin[0]=='.') ||
       (n==2 && begin[0]=='.' && begin[1]=='.') ||
       begin[n-1]=='.' || begin[n-1]==' ' ||
       spider_loose_device(begin,n))return 0;
    if(*used+n+2>cap)return 0;
    if(*used && out[*used-1]!='\\' && out[*used-1]!='/')out[(*used)++]='\\';
    memcpy(out+*used,begin,n);
    *used+=n;
    out[*used]=0;
    if(*p){
      p++;
      if(*p=='\\'||*p=='/')return 0;
      if(!*p && !allow_trailing)return 0;
    }
  }
  return *used<cap;
}
static int spider_loose_path(char *out,size_t cap,
                              const char *exe_directory,
                              const char *pkr_directory,
                              const char *filename){
  size_t used,base;
  const char *dir=pkr_directory;
  if(!out || !cap || !exe_directory || !dir || !filename || !*filename)return 0;
  base=strlen(exe_directory);
  if(base<3 || base+6>=cap || base>240)return 0;
  memcpy(out,exe_directory,base+1);
  used=base;
  if(out[used-1]!='\\' && out[used-1]!='/')out[used++]='\\';
  memcpy(out+used,"data",4);used+=4;out[used]=0;
  if(spider_loose_fold(dir[0])=='d' &&
     spider_loose_fold(dir[1])=='a' &&
     spider_loose_fold(dir[2])=='t' &&
     spider_loose_fold(dir[3])=='a' &&
     (dir[4]==0 || dir[4]=='\\' || dir[4]=='/')){
    dir+=4;
    if(*dir)dir++;
  }
  if(!spider_loose_append(out,cap,&used,dir,1))return 0;
  if(!spider_loose_append(out,cap,&used,filename,0))return 0;
  return 1;
}
#endif
