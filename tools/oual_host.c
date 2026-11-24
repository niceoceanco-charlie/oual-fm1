/* oual_host: render the 10-voice engine offline to a WAV.   usage: oual_host patch.txt out.wav seconds
 * patch.txt: "seed N", "master 0.8", and per voice:
 *   voice 3 sound=file.o3 on=1 transpose=-3 start=0.2 end=0.25 rand=0.3 crush=0.1 srr=0.2 drive=0.3 am=0.2 env=0.5 pan=0.3 level_y=0.4
 * values are the Max patch's own units: transpose in semitones, the rest 0..1 (knob percent / 100), env -1..1.
 * Control changes over time: "at 2.5 voice 3 crush=0.8" applies the setting 2.5 s into the render. GPL-3.0-only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../core/oual_engine.h"
static void apply(oual_engine *e,int v,const char *kv){
  char k[32]; double x; if(sscanf(kv,"%31[^=]=%lf",k,&x)!=2) return;
  if(!strcmp(k,"on")) oual_set(e,v,OUAL_P_ON,(int)x); else if(!strcmp(k,"transpose")) oual_set(e,v,OUAL_P_TRANSPOSE,(int)lround(x*100));
  else if(!strcmp(k,"rand")) oual_set(e,v,OUAL_P_RAND_POS,(int)lround(x*1000)); else if(!strcmp(k,"crush")) oual_set(e,v,OUAL_P_CRUSH,(int)lround(x*1000));
  else if(!strcmp(k,"srr")) oual_set(e,v,OUAL_P_SRR,(int)lround(x*1000)); else if(!strcmp(k,"drive")) oual_set(e,v,OUAL_P_DRIVE,(int)lround(x*1000));
  else if(!strcmp(k,"am")) oual_set(e,v,OUAL_P_AM,(int)lround(x*1000)); else if(!strcmp(k,"start")) oual_set(e,v,OUAL_P_START,(int)lround(x*65536));
  else if(!strcmp(k,"end")) oual_set(e,v,OUAL_P_END,(int)lround(x*65536)); else if(!strcmp(k,"env")) oual_set(e,v,OUAL_P_ENV_RATE,(int)lround(x*1024));
  else if(!strcmp(k,"pan")) oual_set(e,v,OUAL_P_PAN_X,(int)lround(x*1000)); else if(!strcmp(k,"level_y")) oual_set(e,v,OUAL_P_LEVEL_Y,(int)lround(x*1000)); }
static void load(oual_engine *e,int v,const char *path){
  FILE*f=fopen(path,"rb"); if(!f){ perror(path); exit(1);} fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET);
  uint8_t*b=malloc(sz); if(fread(b,1,sz,f)!=(size_t)sz) exit(1); fclose(f); uint32_t h[3]; memcpy(h,b,12);
  if(h[0]!=0x4133434fu){ fprintf(stderr,"%s: not an .o3 file\n",path); exit(1);} oual_sound s={b+12,h[1],h[2]}; oual_set_sound(e,v,&s); }
typedef struct { double t; int v; char kv[64]; } ev_t;
int main(int c,char**argv){
  if(c<4){ fprintf(stderr,"usage: oual_host patch.txt out.wav seconds\n"); return 1; }
  FILE*f=fopen(argv[1],"r"); if(!f){ perror(argv[1]); return 1; } char line[512]; oual_engine *e=calloc(1,sizeof *e); uint32_t seed=1; oual_init(e,44100,seed);
  ev_t *evs=malloc(sizeof(ev_t)*4096); int ne=0; double master=0.8;
  while(fgets(line,sizeof line,f)){ char *tok=strtok(line," \t\r\n"); if(!tok||tok[0]=='#') continue;
    if(!strcmp(tok,"seed")){ seed=(uint32_t)atoi(strtok(NULL," \t\r\n")); oual_init(e,44100,seed); }
    else if(!strcmp(tok,"master")){ master=atof(strtok(NULL," \t\r\n")); e->master_q15=(int)lround(master*32768); }
    else if(!strcmp(tok,"voice")){ int v=atoi(strtok(NULL," \t\r\n"))-1; char*kv; while((kv=strtok(NULL," \t\r\n"))){ if(!strncmp(kv,"sound=",6)) load(e,v,kv+6); else apply(e,v,kv); } }
    else if(!strcmp(tok,"at")){ double t=atof(strtok(NULL," \t\r\n")); strtok(NULL," \t\r\n"); int v=atoi(strtok(NULL," \t\r\n"))-1; char*kv; while((kv=strtok(NULL," \t\r\n"))){ ev_t *x=&evs[ne++]; x->t=t; x->v=v; strncpy(x->kv,kv,63); x->kv[63]=0; } } }
  fclose(f); int total=(int)lround(atof(argv[3])*44100); int16_t *out=malloc((size_t)total*4); int done=0;
  while(done<total){ int n=128; if(n>total-done)n=total-done; for(int i=0;i<ne;i++) if(evs[i].t*44100>=done&&evs[i].t*44100<done+n) apply(e,evs[i].v,evs[i].kv);
    oual_render(e,out+2*done,n); done+=n; }
  FILE*o=fopen(argv[2],"wb"); uint32_t dl=(uint32_t)total*4,hd[11]={0x46464952u,36+dl,0x45564157u,0x20746d66u,16,0x00020001u,44100,44100*4,0x00100004u,0x61746164u,dl};
  fwrite(hd,4,11,o); fwrite(out,1,dl,o); fclose(o);
  printf("rendered %.1f s, decoded %u samples, %u loop wraps\n",total/44100.0,e->stat_decoded,e->stat_wraps); return 0; }
