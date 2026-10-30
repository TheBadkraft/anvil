#define _POSIX_C_SOURCE 200809L
#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
static uint64_t st;
static uint64_t rnd(void){ st^=st<<13; st^=st>>7; st^=st<<17; return st; }
static int R(int n){ return (int)(rnd()%(uint64_t)n); }
static void digs(char*o,int n,int lead_nonzero){ for(int i=0;i<n;i++){ int d=R(10); if(i==0&&lead_nonzero&&d==0)d=1+R(9); if(R(12)==0)d=0; *o++='0'+d;} *o=0; }
static void gen(char*b){
  int mode=R(10); char ip[64]="",fp[64]="",ex[16]=""; const char*sg=R(3)==0?"-":""; 
  int ilen,flen,e=0,hase=0;
  switch(mode){
  case 0: case 1: ilen=1+R(20); flen=R(2)?R(21):0; digs(ip,ilen,R(4)!=0); digs(fp,flen,0); if(R(2)){hase=1;e=R(801)-400;} break;
  case 2: { /* boundary: total sig digits 13..17, effective e10 in -26..26 */
     int n=13+R(5); char m[40]; digs(m,n,1); int e10=R(53)-26; int fl=R(n+1); /* digits after point */
     /* value = m * 10^(e10) ; write as int.frac e(e10+fl) */
     memcpy(ip,m,n-fl); ip[n-fl]=0; if(!*ip){strcpy(ip,"0");} memcpy(fp,m+(n-fl),fl); fp[fl]=0; hase=1; e=e10+fl; if(R(3)==0&&fl==0&&e10==0)hase=0; } break;
  case 3: /* subnormal / overflow */ ilen=1+R(17); flen=R(3)?R(10):0; digs(ip,ilen,1); digs(fp,flen,0); hase=1; e=R(2)? -(300+R(30)) : (295+R(30)); if(R(2)) e=-(318+R(10)); break;
  case 4: { /* leading zeros after point */ int z=R(30); strcpy(ip,"0"); int k=0; for(;k<z;k++)fp[k]='0'; int n=1+R(17); char m[40]; digs(m,n,1); memcpy(fp+k,m,n); fp[k+n]=0; if(R(3)==0){hase=1;e=R(81)-40;} } break;
  case 5: { /* zeros */ int z=R(6); strcpy(ip,"0"); for(int i=0;i<R(4);i++)strcat(ip,"0"); for(int i=0;i<z;i++)fp[i]='0'; fp[z]=0; if(R(2)){hase=1;e=R(81)-40;} } break;
  case 6: { /* halfway-ish: 2^53+1 area and 2^k +- ; ints with .0 */ uint64_t base=(1ULL<<(53+R(8))); uint64_t v=base+ (rnd()%9)-4; char t[32]; snprintf(t,sizeof t,"%llu",(unsigned long long)v); strcpy(ip,t); if(R(2)) {strcpy(fp,"0"); } if(R(2)){hase=1;e=R(21)-10;} } break;
  case 7: { /* exact halfway decimal between doubles: (2m+1)*2^-k-style via 15-17 digit with trailing 5 */ int n=1+R(16); digs(ip,n,1); strcpy(fp,"5"); hase=1; e=R(60)-30;} break;
  case 8: { /* 15-16 digit ints with decimal point placement */ int n=15+R(2); char m[40]; digs(m,n,1); int p=R(n+1); memcpy(ip,m,p); ip[p]=0; if(!*ip)strcpy(ip,"0"); memcpy(fp,m+p,n-p); fp[n-p]=0; if(R(2)){hase=1;e=R(51)-25;} } break;
  default: ilen=1+R(5); flen=R(5); digs(ip,ilen,1); digs(fp,flen,0); if(R(2)){hase=1;e=R(21)-10;} break;
  }
  if(!*ip) strcpy(ip,"0");
  char *p=b; p+=sprintf(p,"%s%s",sg,ip);
  if(*fp) p+=sprintf(p,".%s",fp);
  if(hase){ const char*es=R(2)?"e":"E"; if(e<0)p+=sprintf(p,"%s-%d",es,-e); else p+=sprintf(p,"%s%s%d",es,R(2)?"+":"",e); }
  /* ensure it has . or e (non-integral) mostly */
  else if(!*fp && R(4)) strcat(p,".0");
}
#define PER 400
static long total, mism, rej, ints, overfl, fastish;
static void check(const char*lit, anvil_numeric_t n){
  double want=strtod(lit,NULL); int isint=!strpbrk(lit,".eE");
  if(isint){ ints++; if(!n.is_integral){ mism++; printf("MISMATCH classify %s\n",lit);} return; }
  total++;
  if(isinf(want)){ overfl++; if(!n.overflowed||n.is_integral){mism++; printf("MISMATCH overflow %s\n",lit);} return; }
  if(n.overflowed||n.is_integral||memcmp(&n.as_double,&want,8)!=0){ mism++; if(mism<40) printf("MISMATCH %s got %.17g (%016llx) want %.17g (%016llx) ovf=%d int=%d\n",lit,n.as_double,*(unsigned long long*)&n.as_double,want,*(unsigned long long*)&want,n.overflowed,n.is_integral);}
}
static anvil_numeric_t one(const char*lit, int*ok){
  char src[512]; int len=snprintf(src,sizeof src,"#!aml\n\nx := %s;\n",lit);
  anvil_document d=anvil_load_buffer(src,len); anvil_numeric_t r={.overflowed=true}; *ok=0;
  if(d){ anvil_statement s=anvil_document_find_statement(d,"x"); if(s&&!anvil_has_errors(d)){ r=anvil_value_get_numeric(anvil_statement_get_value(s)); *ok=1;} anvil_dispose(d);} return r;
}
int main(int argc,char**argv){
  st=strtoull(argv[1],0,10)*0x9E3779B97F4A7C15ULL+88172645463325252ULL; long N=atol(argv[2]);
  static char lits[PER][96]; static char src[PER*110+64];
  for(long done=0;done<N;done+=PER){
    char*p=src+sprintf(src,"#!aml\n\n");
    for(int i=0;i<PER;i++){ gen(lits[i]); p+=sprintf(p,"x%d := %s;\n",i,lits[i]); }
    anvil_document d=anvil_load_buffer(src,(size_t)(p-src)); int good=d&&!anvil_has_errors(d);
    if(good){ anvil_statement_iterator it=anvil_document_get_statements(d); anvil_statement s; int i=0;
      while(anvil_statement_iterator_next(it,&s)&&i<PER){ check(lits[i],anvil_value_get_numeric(anvil_statement_get_value(s))); i++; }
      anvil_statement_iterator_dispose(it); if(i!=PER){printf("count %d\n",i);mism++;} }
    if(d)anvil_dispose(d);
    if(!good) for(int i=0;i<PER;i++){ int ok; anvil_numeric_t n=one(lits[i],&ok); if(!ok){ rej++; if(rej<=15)printf("REJECTED %s\n",lits[i]); } else check(lits[i],n); }
  }
  printf("seed %s: compared=%ld ints=%ld overflow=%ld rejected=%ld mismatches=%ld\n",argv[1],total,ints,overfl,rej,mism);
  return mism!=0;
}
