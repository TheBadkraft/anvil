#define _POSIX_C_SOURCE 200809L
#include "anvil_flat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
int main(void){
 const char*L[]={"0.000000000000000123456789012345","0.000000000000000123456789012345e33","123e-5","123e-5","-123e-5","0.0","-0.0","-0e5","0e22","0e-22","-0.000e-400","0e400","0.00000000000000000000000000000000000000000000000000000000000000000000000000001","00000000000000000000000000000000000000000000000000000000000001.5e0","1.5E+0","999999999999999e22","999999999999999e-22","999999999999999e23","1e-22","1e-23","9999999999999999.0","123456789012345.0e-22","0.1234567890123456","1.00000000000000000000000000000000000000000000000000000000000000000000001","0.5e0000000000000000000000000000000000000000000000000000000000000000000000022","1e+0000022","5e-324","2.4703282292062327e-324","2.4703282292062328e-324","1.7976931348623157e308","1.7976931348623158e308","-1e999","123456789012345e-1","0.000001e22","100000000000000.0"};
 int bad=0;
 for(unsigned i=0;i<sizeof L/sizeof*L;i++){ char s[512]; int n=snprintf(s,sizeof s,"#!aml\n\nx := %s;\n",L[i]); anvil_document d=anvil_load_buffer(s,n); if(!d||anvil_has_errors(d)){printf("REJECTED %s\n",L[i]); if(d)anvil_dispose(d); continue;}
  anvil_numeric_t r=anvil_value_get_numeric(anvil_statement_get_value(anvil_document_find_statement(d,"x"))); double w=strtod(L[i],0);
  int ok = isinf(w)? (r.overflowed) : (!r.overflowed && !memcmp(&w,&r.as_double,8));
  printf("%s %s %.17g\n",ok?"ok  ":"BAD ",L[i],r.as_double); if(!ok)bad++; anvil_dispose(d);}
 printf("bad=%d\n",bad); return bad;}
