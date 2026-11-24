/* mkpack3: build a version 2 (oual) pack image from .o3 files.  usage: mkpack3 out.lfp NAME a.o3[:start:end] b.o3 ...   (up to 10 sounds)
 * start/end = the voice's loop window as fractions of the sound (default 0 and 1). Layout: fw/firmware/app/lofi_pack.h. GPL-3.0-only. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
static void p32(uint8_t *p, uint32_t v){ p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24); }
int main(int c,char**v){
  if(c<4||c>13){ fprintf(stderr,"usage: mkpack3 out.lfp NAME a.o3[:start:end] ... (1..10 sounds)\n"); return 1; }
  int n=c-3; uint8_t *img=calloc(1152+10*400000,1); uint32_t off=1152, total=0;
  p32(img,0x3150464cu); img[4]=2; img[6]=(uint8_t)n; strncpy((char*)img+8,v[2],15); p32(img+32,22050);
  for(int i=0;i<n;i++){
    char path[512]; double st=0,en=1; strncpy(path,v[3+i],511); path[511]=0; char*col=strchr(path,':'); if(col){ *col=0; sscanf(col+1,"%lf:%lf",&st,&en); }
    FILE*f=fopen(path,"rb"); if(!f){ perror(path); return 1; } fseek(f,0,SEEK_END); long sz=ftell(f)-12; fseek(f,0,SEEK_SET);
    uint32_t h[3]; if(fread(h,4,3,f)!=3||h[0]!=0x4133434fu){ fprintf(stderr,"%s: not .o3\n",path); return 1; }
    if(fread(img+off,1,(size_t)sz,f)!=(size_t)sz) return 1; fclose(f);
    uint8_t*s=img+64+i*32; p32(s,off); p32(s+4,h[1]); p32(s+8,(uint32_t)lround(st*h[1])); p32(s+12,(uint32_t)lround(en*h[1])); s[16]=s[17]=(uint8_t)i; s[19]=1;
    const char*base=strrchr(path,'/'); base=base?base+1:path; char nm[12]={0}; strncpy(nm,base,10); for(char*q=nm;*q;q++){ if(*q=='.')*q=0; } strncpy((char*)s+20,nm,11);
    off+=(uint32_t)sz; total+=h[1]; }
  p32(img+24,total); FILE*o=fopen(v[1],"wb"); fwrite(img,1,off,o); fclose(o); printf("%s: %d sounds, %u bytes\n",v[1],n,off); return 0; }
