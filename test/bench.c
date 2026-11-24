/* Host timing of the engine: NOT a device measurement (different CPU, no flash wait states). Gives ops-shaped numbers to compare
 * against the FM-1 once a measurement build exists. Usage: bench file.o3 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../core/oual_engine.h"
int main(int c,char**v){
  if(c<2) return 1; FILE*f=fopen(v[1],"rb"); fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); uint8_t*b=malloc(sz); if(fread(b,1,sz,f)!=(size_t)sz) return 1; fclose(f);
  uint32_t h[3]; memcpy(h,b,12); oual_sound o={b+12,h[1],h[2]}; int16_t *out=malloc(44100*4);
  for(int rv=0;rv<2;rv++) for(int nv=1;nv<=10;nv+=(nv==1?4:5)){
    static oual_engine e; oual_init(&e,44100,1); oual_set_reverb(&e,rv?1000:0);
    for(int i=0;i<nv;i++){ oual_set_sound(&e,i,&o); oual_set(&e,i,OUAL_P_ON,1); oual_set(&e,i,OUAL_P_TRANSPOSE,(i%5)*200-300); oual_set(&e,i,OUAL_P_START,i*4000); oual_set(&e,i,OUAL_P_END,i*4000+9000);
      oual_set(&e,i,OUAL_P_DRIVE,300); oual_set(&e,i,OUAL_P_CRUSH,200); oual_set(&e,i,OUAL_P_SRR,200); oual_set(&e,i,OUAL_P_AM,300); oual_set(&e,i,OUAL_P_RAND_POS,300); }
    oual_render(&e,out,4410); e.stat_decoded=0; clock_t t0=clock(); for(int s=0;s<10;s++) oual_render(&e,out,44100); double dt=(double)(clock()-t0)/CLOCKS_PER_SEC;
    printf("reverb %s, %2d voices, everything on: %.1f ms of host CPU per second of audio (%.2f%% of one host core); decoded %.0f samples/s; %.0f ns per voice-sample\n",rv?"on ":"off",nv,dt*100,dt*10,e.stat_decoded/10.0,dt*1e9/(10.0*44100*nv)); }
  return 0; }
