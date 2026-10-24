#include <stdio.h>
#include "../ini_recovery.h"
int main(int argc,char **argv) {
  SpiderIniRecovered *r;
  unsigned i;
  (void)SpiderIni_Validate;
  if(argc!=2)return 2;
  r=malloc(sizeof(*r)); if(!r)return 3;
  if(!SpiderIni_Recover(argv[1],r)){free(r);return 4;}
  printf("RECOVERED:%u\n",r->count);
  for(i=0;i<r->count;i++)printf("[%s] %s=%s\n",r->entries[i].section,r->entries[i].key,r->entries[i].value);
  free(r);return 0;
}