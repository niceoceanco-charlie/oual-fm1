/* enc3: host encoder for the 3-bit codec. Output .o3 = 12-byte header ("OC3A", frames, rate) + superblocks.  usage: enc3 in.raw16 out.o3 [decoded.raw16]   (in = mono s16le)
 * Prints frames, bytes, SNR of the decoded result against the input. GPL-3.0-only. */
#include <stdio.h>
#include <math.h>
#include "oc3_enc.h"
int main(int c,char**v){
  if(c<3){ fprintf(stderr,"usage: enc3 in.raw16 out.o3 [decoded.raw16]\n"); return 1; }
  FILE*f=fopen(v[1],"rb"); if(!f){perror(v[1]);return 1;} fseek(f,0,SEEK_END); long n=ftell(f)/2; fseek(f,0,SEEK_SET);
  int16_t*in=calloc(n+1,2); if(fread(in,2,n,f)!=(size_t)n) return 1; fclose(f);
  uint32_t nb=oc3_blocks(n); uint8_t*out=calloc(nb,OC3_SUPER_BYTES); oc3_encode(in,n,out);
  FILE*o=fopen(v[2],"wb"); { uint32_t hdr[3]={0x4133434fu,(uint32_t)n,22050u}; fwrite(hdr,4,3,o); }  /* "OC3A", frames, source rate */
  fwrite(out,1,(size_t)nb*OC3_SUPER_BYTES,o); fclose(o);
  double se=0,sn=0; int16_t blk[OC3_SUPER]; FILE*d=c>3?fopen(v[3],"wb"):NULL;
  for(uint32_t b=0;b<nb;b++){ oc3_decode_super(out,b,blk); for(int i=0;i<OC3_SUPER;i++){ long pos=(long)b*OC3_SUPER+i; if(pos>=n)break; double e=in[pos]-blk[i]; se+=e*e; sn+=(double)in[pos]*in[pos]; if(d)fwrite(&blk[i],2,1,d);} }
  if(d)fclose(d);
  printf("frames=%ld bytes=%u (%.2f KB/s at 22050) SNR=%.1f dB\n",n,nb*OC3_SUPER_BYTES,nb*OC3_SUPER_BYTES/(n/22050.0)/1000,10*log10(sn/(se+1e-9)));
  return 0; }
