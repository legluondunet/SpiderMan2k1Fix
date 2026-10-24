#include <stdio.h>
#include "../ini_validation.h"
static void issue(void *opaque,unsigned line,int error,const char *text) {
  (void)opaque;
  printf("%s:%u:%s\n",error?"ERROR":"WARNING",line,text);
}
int main(int argc,char **argv) {
  SpiderIniResult r;
  if(argc!=2)return 2;
  r=SpiderIni_Validate(argv[1],issue,NULL);
  printf("RESULT:%u:%u:%d\n",r.errors,r.warnings,r.safe_to_save);
  return 0;
}