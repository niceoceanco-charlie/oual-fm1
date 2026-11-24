/* Compares the integer engine with a floating-point model of the same voice chain on real material.
 * The model is the straightforward maths of the signal flow (it shares the interpretation in docs/PORT_NOTES.md), so this
 * measures the cost of using integers, not parity with Max. Usage: test_ref file1.o3 [file2.o3 ...]  (run from tools output) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/oual_engine.h"
#include "../core/oual_codec3.h"
#define N 44100
static double herm(double ym,double y0,double y1,double y2,double t){ double c1=.5*(y1-ym),c2=ym-2.5*y0+2*y1-.5*y2,c3=.5*(y2-ym)+1.5*(y0-y1); return ((c3*t+c2)*t+c1)*t+y0; }
static double knee(double a,double s,double w){ if(a<=s) return a; if(a>=s+w) return s+w/2; return a-(a-s)*(a-s)/(2*w); }
int main(int c,char**v){
  if(c<2){ fprintf(stderr,"usage: test_ref a.o3 [b.o3...]\n"); return 1; }
  double worst=1e9; srand(5);
  for(int fi=1;fi<c;fi++){
    FILE*f=fopen(v[fi],"rb"); fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); uint8_t*b=malloc(sz); if(fread(b,1,sz,f)!=(size_t)sz) return 1; fclose(f);
    uint32_t h[3]; memcpy(h,b,12); uint32_t frames=h[1]; uint8_t*data=b+12; uint32_t nb=oc3_blocks(frames);
    int16_t *pcm=malloc(nb*OC3_SUPER*2); for(uint32_t k=0;k<nb;k++) oc3_decode_super(data,k,pcm+k*OC3_SUPER);
    for(int trial=0;trial<6;trial++){
      int tr=(rand()%2400)-1200, crush=trial&1?(rand()%500):0, srr=trial&2?(rand()%700):0, drive=rand()%1000, am=trial==3?500:0, st=rand()%20000, en=st+8000+rand()%30000;
      if(trial>=4){ srr=0;crush=0;drive=0;am=0; if(trial==4) tr=0; } if(en>65536) en=65536; int pan=rand()%1000, ly=rand()%800;
      oual_engine e; oual_init(&e,44100,1); oual_sound o={data,frames,22050}; oual_set_sound(&e,0,&o); oual_set(&e,0,OUAL_P_ON,1);
      oual_set(&e,0,OUAL_P_TRANSPOSE,tr); oual_set(&e,0,OUAL_P_CRUSH,crush); oual_set(&e,0,OUAL_P_SRR,srr); oual_set(&e,0,OUAL_P_DRIVE,drive);
      oual_set(&e,0,OUAL_P_AM,am); oual_set(&e,0,OUAL_P_START,st); oual_set(&e,0,OUAL_P_END,en); oual_set(&e,0,OUAL_P_PAN_X,pan); oual_set(&e,0,OUAL_P_LEVEL_Y,ly);
      int16_t *out=malloc(N*4); oual_render(&e,out,N);
      /* float model */
      double ratio=(double)e.v[0].step/4294967296.0 /* the engine's own quantised step: removes drift that is not an audible difference */, ls=floor((double)st*frames/65536), le=floor((double)en*frames/65536), len=le-ls, pos=ls, held=0, acc=0, amph=0;
      double dg=1+10*drive/1000.0; int bits=(1000-crush)*16/1000+1; double lvl=pow((1000-ly)/1000.0,2); double px=pan/1000.0, gl=sqrt(1-px), gr=sqrt(px);
      double se=0,sn=0,mxe=0; for(int i=0;i<N;i++){
        long ix=(long)floor(pos); double t=pos-ix; double y[4]; for(int k=0;k<4;k++){ long q=ix-1+k; if(q<0)q=0; if(q>=(long)frames)q=frames-1; y[k]=pcm[q]/32768.0; }
        double s=herm(y[0],y[1],y[2],y[3],t); double ph=(pos-ls)/len; double w=sin(M_PI*ph)*10; if(w>1)w=1; s*=w;
        if(dg!=1){ double a=fabs(s*dg); a=knee(a,0.5,1.0); s=s<0?-a:a; }
        if(e.v[0].srr_q16<65536){ acc+=e.v[0].srr_q16/65536.0; if(acc>=1){ acc-=1; held=s; } s=held; }
        if(bits<16){ double q=pow(2,bits-1); s=floor(s*q+0.5)/q; if(s>1)s=1; }
        double ac=am/1000.0; s*=(1-ac)+ac*0.5*(1+cos(2*M_PI*amph)); amph+=e.v[0].am_dph/4294967296.0; amph-=floor(amph);
        s*=lvl; double m=0.8; double l=s*gl*m, r=s*gr*m; double aL=fabs(l); aL=aL>0.7?knee(aL-0.7,0,0.6)+0.7:aL; if(l<0)aL=-aL; (void)r;
        l=aL; if(i>N/2){ double d=out[2*i]/32768.0-l; se+=d*d; sn+=l*l; if(fabs(d)>mxe) mxe=fabs(d); }
        pos+=ratio; if(pos>=le) pos=ls+(pos-le);
      }
      double snr=10*log10((sn+1e-12)/(se+1e-12)); if(sn<1e-6) snr=99;
      printf("%-26s tr %5d crush %3d srr %3d drive %3d am %3d: %.1f dB, worst sample off by %.1f LSB\n",strrchr(v[fi],'/')?strrchr(v[fi],'/')+1:v[fi],tr,crush,srr,drive,am,snr,mxe*32768);
      { double eff=(mxe*32768<=8)?99:snr; if(eff<worst) worst=eff; } free(out); }
  }
  printf("worst %.1f dB (a case counts as 99 if its worst sample is within 8 LSB, i.e. only 16-bit rounding)\n",worst); return worst>30?0:1; }
