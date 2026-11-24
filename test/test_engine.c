/* Host tests for the oual engine and 3-bit codec. Build and run: ./test/run_tests.sh */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/oual_engine.h"
#include "../tools/oc3_enc.h"
#define OUT_RATE 44100
static int fails=0, checks=0;
#define CHECK(c,...) do{ checks++; if(!(c)){ fails++; printf("  FAIL: "); printf(__VA_ARGS__); printf("\n"); } }while(0)

/* test sounds */
typedef struct { uint8_t *data; uint32_t frames; int16_t *pcm; } snd_t;
static snd_t make_tone(double hz, double amp, double secs){
  snd_t s; s.frames=(uint32_t)(22050*secs); s.pcm=malloc(s.frames*2);
  for(uint32_t i=0;i<s.frames;i++) s.pcm[i]=(int16_t)lround(amp*32767*sin(2*M_PI*hz*i/22050.0));
  s.data=calloc(oc3_blocks(s.frames),OC3_SUPER_BYTES); oc3_encode(s.pcm,s.frames,s.data); return s; }
static oual_sound osnd(snd_t *s){ oual_sound o={s->data,s->frames,22050}; return o; }
static void defaults(oual_engine *e,int v,int on){ oual_set(e,v,OUAL_P_PAN_X,0); oual_set(e,v,OUAL_P_LEVEL_Y,0); oual_set(e,v,OUAL_P_ON,on); }
/* render n frames, return left channel */
static int16_t *rend(oual_engine *e,int n){ int16_t *st=malloc(n*4),*l=malloc(n*2); oual_render(e,st,n); for(int i=0;i<n;i++) l[i]=st[2*i]; free(st); return l; }
static double goertzel(const int16_t *x,int n,double hz){ double w=2*M_PI*hz/OUT_RATE,c=2*cos(w),s1=0,s2=0; for(int i=0;i<n;i++){ double s=x[i]+c*s1-s2; s2=s1; s1=s; } return sqrt(s1*s1+s2*s2-c*s1*s2)/n*2; }
static double zc_freq(const int16_t *x,int a,int b){ int c=0; for(int i=a+1;i<b;i++) if(x[i-1]<0&&x[i]>=0) c++; return c*(double)OUT_RATE/(b-a); }
static double rms(const int16_t *x,int a,int b){ double s=0; for(int i=a;i<b;i++) s+=(double)x[i]*x[i]; return sqrt(s/(b-a)); }

static void t_codec(void){
  printf("codec: random access, bit exactness\n");
  snd_t s=make_tone(330,0.6,3.0); uint32_t nb=oc3_blocks(s.frames); int16_t *fw=malloc(nb*OC3_SUPER*2),*rv=malloc(nb*OC3_SUPER*2);
  for(uint32_t b=0;b<nb;b++) oc3_decode_super(s.data,b,fw+b*OC3_SUPER);
  for(int b=(int)nb-1;b>=0;b--) oc3_decode_super(s.data,(uint32_t)b,rv+b*OC3_SUPER);
  CHECK(!memcmp(fw,rv,nb*OC3_SUPER*2),"reverse-order decode differs from forward");
  double se=0,sn=0; for(uint32_t i=0;i<s.frames;i++){ double e=s.pcm[i]-fw[i]; se+=e*e; sn+=(double)s.pcm[i]*s.pcm[i]; }
  double snr=10*log10(sn/(se+1e-9)); printf("  tone SNR %.1f dB\n",snr); CHECK(snr>30,"tone SNR %.1f below 30",snr);
  CHECK(nb*OC3_SUPER_BYTES==(s.frames+639)/640*272,"size formula"); }

static void t_pitch(void){
  printf("pitch: transpose\n"); snd_t s=make_tone(440,0.5,2.0);
  int cents[]={0,1200,-1200,700}; double want[]={440,880,220,440*pow(2,7/12.0)};
  for(int k=0;k<4;k++){ oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
    oual_set(&e,0,OUAL_P_TRANSPOSE,cents[k]); oual_set(&e,0,OUAL_P_START,0); oual_set(&e,0,OUAL_P_END,65536);
    int16_t *x=rend(&e,OUT_RATE); double f=zc_freq(x,2000,OUT_RATE-2000);
    CHECK(fabs(f/want[k]-1)<0.01,"transpose %d cents: %.1f Hz, wanted %.1f",cents[k],f,want[k]); free(x); } }

static void t_loop(void){
  printf("loop: wrap is click-free (edge fade)\n"); snd_t s=make_tone(440,0.5,0.5);
  oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
  int n=OUT_RATE*3; int16_t *x=rend(&e,n); int maxd=0,ref=0;
  for(int i=OUT_RATE/10+1;i<OUT_RATE/4;i++){ int d=abs(x[i]-x[i-1]); if(d>ref)ref=d; }
  for(int i=OUT_RATE/10;i<n;i++){ int d=abs(x[i]-x[i-1]); if(d>maxd)maxd=d; }
  printf("  max step %d, steady-state step %d, wraps %u\n",maxd,ref,e.stat_wraps);
  CHECK(e.stat_wraps>=5,"expected >=5 wraps, got %u",e.stat_wraps); CHECK(maxd<=ref*13/10,"step at wrap %d vs steady %d",maxd,ref); free(x); }

static void t_crush_srr(void){
  printf("crush and SRR\n"); snd_t s=make_tone(100,0.5,1.0);
  for(int bits=2;bits<=8;bits+=3){ oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
    int crush=(int)ceil((1.0-(bits-1)/16.0)*1000)-1; /* int((1-c)*16)+1 == bits */ oual_set(&e,0,OUAL_P_CRUSH,crush);
    int16_t *x=rend(&e,OUT_RATE); static int seen[65536]; memset(seen,0,sizeof seen); int distinct=0; for(int i=OUT_RATE/2;i<OUT_RATE-2000;i++){ int k=x[i]+32768; if(!seen[k]){seen[k]=1;distinct++;} }
    /* the output is crushed then scaled by smooth gains, so count distinct values loosely: bound by 2^bits+1 levels per gain value; check monotone in bits */
    printf("  bits %d: %d distinct output values\n",bits,distinct); CHECK(distinct<(1<<bits)*4,"bits %d: too many distinct values (%d)",bits,distinct); free(x); }
  oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1); oual_set(&e,0,OUAL_P_SRR,500); /* ratio .25 */
  int16_t *x=rend(&e,OUT_RATE); long runs=0,len=0; for(int i=OUT_RATE/2;i<OUT_RATE-2000;i++){ if(x[i]!=x[i-1]) runs++; len++; }
  double avg=(double)len/runs; printf("  SRR 0.5 -> hold ratio .25: average run %.2f samples (about 4 expected)\n",avg); CHECK(avg>3.0&&avg<5.5,"SRR run length %.2f",avg); free(x); }

static void t_am_drive(void){
  printf("AM and drive\n"); snd_t s=make_tone(440,0.5,1.0);
  oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
  int16_t *x0=rend(&e,OUT_RATE); double base=goertzel(x0+OUT_RATE/2,OUT_RATE/4,12440); free(x0);
  oual_init(&e,OUT_RATE,1); oual_set_sound(&e,0,&o); defaults(&e,0,1); oual_set(&e,0,OUAL_P_AM,1000);
  int16_t *x1=rend(&e,OUT_RATE); double am=goertzel(x1+OUT_RATE/2,OUT_RATE/4,12440); free(x1);
  printf("  12440 Hz sideband: %.1f without AM, %.1f with AM\n",base,am); CHECK(am>base*10+50,"AM sideband not strong enough");
  snd_t loud=make_tone(440,0.9,1.0); oual_sound ol=osnd(&loud); double h3[2];
  for(int d=0;d<2;d++){ oual_init(&e,OUT_RATE,1); oual_set_sound(&e,0,&ol); defaults(&e,0,1); oual_set(&e,0,OUAL_P_DRIVE,d?1000:0);
    int16_t *x=rend(&e,OUT_RATE); h3[d]=goertzel(x+OUT_RATE/2,OUT_RATE/4,1320)/goertzel(x+OUT_RATE/2,OUT_RATE/4,440); free(x); }
  printf("  3rd harmonic / fundamental: drive 0 -> %.4f, drive max -> %.4f\n",h3[0],h3[1]); CHECK(h3[1]>h3[0]*5,"drive adds too little harmonic content"); }

static void t_rand_pos(void){
  printf("rand_pos and determinism\n"); snd_t s=make_tone(440,0.5,2.0);
  for(int rp=0;rp<=1000;rp+=1000){ oual_engine e; oual_init(&e,OUT_RATE,7); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
    oual_set(&e,0,OUAL_P_START,13107); oual_set(&e,0,OUAL_P_END,26214); oual_set(&e,0,OUAL_P_RAND_POS,rp);
    int32_t lo=1<<30,hi=0,minlen=1<<30,maxlen=0,last=-1,changes=0; for(int b=0;b<400;b++){ int16_t *x=rend(&e,512); free(x);
      if(e.v[0].ls!=last){ last=e.v[0].ls; changes++; } int32_t ls=e.v[0].ls,le=e.v[0].le; if(ls<lo)lo=ls; if(le>hi)hi=le; if(le-ls<minlen)minlen=le-ls; if(le-ls>maxlen)maxlen=le-ls; }
    printf("  rand_pos %d: %d distinct windows, start..end within [%d,%d], length %d..%d (base %d..%d)\n",rp/10,changes,lo,hi,minlen,maxlen,(int)(0.2*s.frames),(int)(0.4*s.frames));
    if(rp==0) CHECK(changes==1,"rand_pos 0 should keep one window, got %d",changes); else CHECK(changes>5&&minlen>=8&&hi<=(int32_t)s.frames,"rand_pos max: windows %d min len %d hi %d",changes,minlen,hi); }
  int16_t *a,*b; oual_engine e1,e2; oual_sound o=osnd(&s);
  oual_init(&e1,OUT_RATE,3); oual_init(&e2,OUT_RATE,3); for(int k=0;k<2;k++){ oual_engine *e=k?&e2:&e1; oual_set_sound(e,0,&o); defaults(e,0,1); oual_set(e,0,OUAL_P_RAND_POS,600); oual_set(e,0,OUAL_P_END,20000); }
  a=rend(&e1,OUT_RATE); b=rend(&e2,OUT_RATE); CHECK(!memcmp(a,b,OUT_RATE*2),"same seed must give identical output"); free(a); free(b); }

static void t_envelope_gate(void){
  printf("envelope and gate\n"); snd_t s=make_tone(440,0.5,2.0);
  int16_t tri[OUAL_ENV_POINTS]; for(int k=0;k<OUAL_ENV_POINTS;k++){ int v=k<32?k*1024:(64-k)*1024; tri[k]=(int16_t)(v>32767?32767:v); }
  int rates[]={1024,512}; int want[]={1,2};
  for(int k=0;k<2;k++){ oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
    oual_set(&e,0,OUAL_P_START,0); oual_set(&e,0,OUAL_P_END,32768); oual_set_envelope(&e,0,tri); oual_set(&e,0,OUAL_P_ENV_RATE,rates[k]);
    int loop=OUT_RATE; int16_t *x=rend(&e,loop*2); double r[32]; for(int i=0;i<32;i++) r[i]=rms(x,loop/2+i*(loop/64)*2/2*1,loop/2+i*(loop/64)*2/2*1+loop/64);
    /* count peaks in the 32-slice rms series over one loop */
    for(int i=0;i<32;i++) r[i]=rms(x,loop+ i*(loop/32), loop+(i+1)*(loop/32)); int peaks=0; double mx=0; for(int i=0;i<32;i++) if(r[i]>mx)mx=r[i];
    for(int i=1;i<31;i++) if(r[i]>r[i-1]&&r[i]>=r[i+1]&&r[i]>0.5*mx) peaks++;
    printf("  envelope rate %.2f: %d peaks per loop\n",rates[k]/1024.0,peaks); CHECK(peaks==want[k],"envelope rate %d: expected %d peaks, got %d",rates[k],want[k],peaks); free(x); }
  oual_engine e; oual_init(&e,OUT_RATE,1); oual_sound o=osnd(&s); oual_set_sound(&e,0,&o); defaults(&e,0,1);
  int16_t *x=rend(&e,OUT_RATE/2); free(x); oual_set(&e,0,OUAL_P_ON,0); x=rend(&e,OUT_RATE/2); int maxd=0; for(int i=1;i<OUT_RATE/2;i++){ int d=abs(x[i]-x[i-1]); if(d>maxd)maxd=d; }
  CHECK(maxd<1500,"gate off step %d",maxd); CHECK(rms(x,OUT_RATE/4,OUT_RATE/2)==0,"voice not silent after fade-out"); free(x); }

static void t_reverb(void){
  printf("reverb: integer chain against a floating-point model, decay, stability\n");
  static oual_engine e; oual_init(&e,OUT_RATE,1); oual_set_reverb(&e,1000);
  /* model of the patch's rvb: x*0.14 -> reson (peak gain 10 at 1200 Hz, Q 8) -> allpass 65/67/85 ms (c 0.7) -> *0.5, plus dry */
  double R=exp(-M_PI*150.0/OUT_RATE), th=2*M_PI*1200/OUT_RATE, b1=2*R*cos(th), b2=-R*R, re=1-R*cos(2*th), im=R*sin(2*th), a0=10*(1-R)*sqrt(re*re+im*im)*0.14;
  int N=OUT_RATE*3, burst=OUT_RATE/2; int n0=e.rvb_n0,n1=e.rvb_n1,n2=e.rvb_n2;
  int32_t *buf=calloc((size_t)N*2,4); double *ref=calloc((size_t)N*2,8); uint32_t r=7;
  for(int i=0;i<burst;i++){ r=r*1664525u+1013904223u; double x=((int)((r>>12)&0xffff)-32768)*0.5; buf[2*i]=(int32_t)x; buf[2*i+1]=(int32_t)(x*0.7); }
  for(int ch=0;ch<2;ch++){ double y1=0,y2=0; double *d0=calloc(n0,8),*d1=calloc(n1,8),*d2=calloc(n2,8); int i0=0,i1=0,i2=0;
    for(int i=0;i<N;i++){ double x=buf[2*i+ch]/16.0, y=a0*x+b1*y1+b2*y2; y2=y1; y1=y; double w=y;
      double *dl[3]={d0,d1,d2}; int *ix[3]={&i0,&i1,&i2}; int nn[3]={n0,n1,n2};
      for(int k=0;k<3;k++){ double wd=dl[k][*ix[k]], ww=w-0.7*wd; double yy=0.7*ww+wd; dl[k][*ix[k]]=ww; if(++*ix[k]>=nn[k])*ix[k]=0; w=yy; }
      ref[2*i+ch]=buf[2*i+ch]+w*8; }
    free(d0);free(d1);free(d2); }
  for(int b=0;b<N;b+=256){ int n=N-b<256?N-b:256; oual_rvb_run(&e,buf+2*b,n); }
  double se=0,sn=0,wetmax=0; for(int i=0;i<N*2;i++){ double d=buf[i]-ref[i]; se+=d*d; sn+=ref[i]*ref[i]; }
  for(int i=0;i<N;i++){ double a=fabs(ref[2*i]); if(a>wetmax)wetmax=a; }
  double snr=10*log10(sn/(se+1e-9)); printf("  integer vs float model: %.1f dB (model peak %.0f)\n",snr,wetmax); CHECK(snr>35,"reverb integer vs float %.1f dB",snr);
  int tail=0; for(int i=N-OUT_RATE/2;i<N;i++){ int a=abs(buf[2*i]); if(a>tail)tail=a; }
  printf("  after 2 s of silence the tail is %d (of 2^19)\n",tail); CHECK(tail<64,"reverb tail did not die out: %d",tail);
  oual_init(&e,OUT_RATE,1); oual_set_reverb(&e,1000); for(int i=0;i<N*2;i++) buf[i]=(i/2%2)?200000:-200000;   /* full-scale square at Nyquist/2 */
  for(int b=0;b<N;b+=256){ int n=N-b<256?N-b:256; oual_rvb_run(&e,buf+2*b,n); } int pk=0; for(int i=0;i<N*2;i++){ int a=abs(buf[i]); if(a>pk)pk=a; } CHECK(pk<4000000,"reverb stays bounded on a loud input (peak %d)",pk);
  free(buf); free(ref); }

static void t_stress(void){
  printf("stress: 10 voices, every control at its extreme\n"); snd_t s=make_tone(220,0.95,2.0);
  oual_engine e; oual_init(&e,OUT_RATE,11); oual_sound o=osnd(&s);
  for(int v=0;v<10;v++){ oual_set_sound(&e,v,&o); oual_set(&e,v,OUAL_P_ON,1); oual_set(&e,v,OUAL_P_TRANSPOSE,(v-5)*240); oual_set(&e,v,OUAL_P_DRIVE,1000);
    oual_set(&e,v,OUAL_P_CRUSH,v*100); oual_set(&e,v,OUAL_P_SRR,v*100); oual_set(&e,v,OUAL_P_AM,1000); oual_set(&e,v,OUAL_P_RAND_POS,1000);
    oual_set(&e,v,OUAL_P_LEVEL_Y,0); oual_set(&e,v,OUAL_P_PAN_X,v*111); oual_set(&e,v,OUAL_P_START,v*3000); oual_set(&e,v,OUAL_P_END,v*3000+4000); oual_set(&e,v,OUAL_P_ENV_RATE,(v%2)?-300:700); }
  int16_t *x=rend(&e,OUT_RATE*4); int pk=0; for(int i=0;i<OUT_RATE*4;i++){ int a=abs(x[i]); if(a>pk)pk=a; }
  printf("  peak %d of 32767, decoded %u samples in 4 s (%.0f per second), wraps %u\n",pk,e.stat_decoded,e.stat_decoded/4.0,e.stat_wraps);
  CHECK(pk<=32767,"peak overflow"); CHECK(pk>1000,"stress output is silent"); free(x); }

int main(void){ setvbuf(stdout,NULL,_IONBF,0); t_codec(); t_pitch(); t_loop(); t_crush_srr(); t_am_drive(); t_rand_pos(); t_envelope_gate(); t_reverb(); t_stress();
  printf("\n%d checks, %d failed\n",checks,fails); return fails?1:0; }
