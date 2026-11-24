/* gen_testsounds: writes 10 synthetic 2 s mono 22.05 kHz raw files (tones, noise bursts, a click train) for CI, so the repo needs none of Charlie's own sounds.
 * usage: gen_testsounds outdir      -> outdir/t01.raw .. t10.raw. GPL-3.0-only. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
int main(int c,char**v){
  if(c<2) return 1; uint32_t rng=12345;
  for(int k=0;k<10;k++){
    char p[512]; snprintf(p,sizeof p,"%s/t%02d.raw",v[1],k+1); FILE*f=fopen(p,"wb"); if(!f){ perror(p); return 1; }
    int n=44100; double hz=110*pow(2,k/3.0);
    for(int i=0;i<n;i++){ double t=i/22050.0, x=0;
      switch(k%5){ case 0: x=sin(2*M_PI*hz*t)*0.5; break;
        case 1: x=(sin(2*M_PI*hz*t)+0.5*sin(2*M_PI*hz*2.01*t))*0.35*(0.6+0.4*sin(2*M_PI*3*t)); break;
        case 2: rng=rng*1664525u+1013904223u; x=(((rng>>16)&0x7fff)/16384.0-1)*0.3*exp(-fmod(t,0.25)*12); break;
        case 3: x=((fmod(t*hz,1.0))*2-1)*0.3; break;
        default: x=(fmod(t,0.125)<0.004?1:0)*0.6*sin(2*M_PI*880*t); break; }
      int16_t s=(int16_t)lrint(x*32767); fwrite(&s,2,1,f); }
    fclose(f); }
  return 0; }
