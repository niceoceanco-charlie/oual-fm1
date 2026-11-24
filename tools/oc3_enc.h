/* oc3_enc.h: host-only encoder for oual_codec3.h. GPL-3.0-only. The search (all 16 scales per group, analysis by synthesis,
 * a penalty that keeps the LMS weights bounded) follows the prototype in ferluht/fm1-lofi-sampler tools/lofi_codec.c (GPL-3.0). */
#ifndef OC3_ENC_H
#define OC3_ENC_H
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../core/oual_codec3.h"
static uint64_t oc3_pen(const int32_t s[8]){ int64_t a=0; for(int i=4;i<8;i++) a+=(int64_t)s[i]*s[i]; int64_t v=(a>>18)-0x8ff; return v>0?(uint64_t)(v*v):0; }
/* in: n samples. out: oc3_blocks(n)*OC3_SUPER_BYTES bytes, zeroed by the caller. */
static void oc3_encode(const int16_t *in0, uint32_t n, uint8_t *out){
  uint32_t nb=oc3_blocks(n); int16_t *in=calloc((size_t)nb*OC3_SUPER,2); memcpy(in,in0,(size_t)n*2);
  int32_t st[8]={0,0,0,0,0,0,-8192,16384}; int prev=0;
  for(uint32_t b=0;b<nb;b++){
    uint8_t*p=out+(size_t)b*OC3_SUPER_BYTES;
    for(int i=0;i<8;i++){ p[2*i]=(uint8_t)(st[i]&255); p[2*i+1]=(uint8_t)((st[i]>>8)&255); }
    uint8_t*chunk=p+16;
    for(int ch=0;ch<2;ch++){
      uint8_t*codes=chunk,*sc=chunk+OC3_CODEBYTES; uint32_t bit=0;
      for(int g=0;g<16;g++){
        uint64_t best=UINT64_MAX; int32_t bs[8]; uint8_t bc[OC3_GROUP]; int bsf=prev;
        for(int sr=0;sr<16;sr++){
          int sf=(sr+prev)&15; int32_t t[8]; memcpy(t,st,sizeof t); uint64_t rank=0; uint8_t cc[OC3_GROUP]; int i;
          const int16_t*dq=oual_dqt3+sf*8;
          for(i=0;i<OC3_GROUP;i++){
            long pos=(long)b*OC3_SUPER+ch*OC3_CHUNK+g*OC3_GROUP+i; int32_t pcm=in[pos], pr=oc3_pred(t);
            int bq=0; int32_t be=INT32_MAX;
            for(int q=0;q<8;q++){ int32_t e=abs(pcm-oc3_sat(pr+dq[q])); if(e<be){be=e;bq=q;} }
            int32_t res=dq[bq], rec=oc3_sat(pr+res); int64_t er=pcm-rec; rank+=(uint64_t)(er*er)+oc3_pen(t);
            if(rank>best) break; cc[i]=(uint8_t)bq; oc3_upd(t,rec,res); }
          if(i==OC3_GROUP && rank<best){ best=rank; memcpy(bs,t,sizeof t); memcpy(bc,cc,sizeof cc); bsf=sf; } }
        prev=bsf; memcpy(st,bs,sizeof st);
        for(int i=0;i<OC3_GROUP;i++){ for(int k=2;k>=0;k--){ if((bc[i]>>k)&1) codes[bit>>3]|=(uint8_t)(0x80>>(bit&7)); bit++; } }
        if(g&1) sc[g>>1]|=(uint8_t)bsf; else sc[g>>1]|=(uint8_t)(bsf<<4);
      }
      chunk+=OC3_CHUNKBYTES; } }
  free(in); }
#endif
