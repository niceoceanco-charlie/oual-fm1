/* oual_codec3.h: 3-bit LMS-ADPCM, decoder (device and host). GPL-3.0-only.
 *
 * Format and idea follow ferluht/fm1-lofi-sampler docs/CODEC.md (GPL-3.0, itself derived from the MIT-licensed SEA codec by
 * Dani Biro): 4-tap sign-sign LMS predictor, a 4-bit scale per group of 20 samples, a code per sample. This file is a
 * separate implementation widened to 3 bits per code (8 levels).
 *
 * Layout: a sound is N superblocks of 640 samples = 272 bytes each:
 *   16 bytes seed (8 x int16 LE: 4 history samples, 4 weights) | chunk 0 | chunk 1
 *   chunk (320 samples) = 120 bytes of codes (16 groups x 20 codes x 3 bits, MSB first) | 8 bytes of scales
 *   (16 nibbles; even group = high nibble).
 * Every superblock decodes on its own from its seed, so loops and jumps need no search. 3.4 bytes per 8 samples.
 */
#ifndef OUAL_CODEC3_H
#define OUAL_CODEC3_H
#include <stdint.h>
#include "oual_tables.h"
#define OC3_GROUP 20
#define OC3_CHUNK 320
#define OC3_SUPER 640
#define OC3_CODEBYTES 120
#define OC3_CHUNKBYTES 128
#define OC3_SUPER_BYTES 272
static inline int32_t oc3_sat(int32_t x){ return x>32767?32767:(x<-32768?-32768:x); }
/* Predictor step shared by the decoder and the host encoder. */
static inline int32_t oc3_pred(const int32_t s[8]){ int64_t a=0; for(int i=0;i<4;i++) a+=(int64_t)s[i]*s[4+i]; return (int32_t)(a>>13); }
static inline void oc3_upd(int32_t s[8], int32_t pcm, int32_t res){
  int32_t d=res>>4;
  for(int i=0;i<4;i++) s[4+i]=oc3_sat(s[4+i]+(s[i]<0?-d:d));
  s[0]=s[1]; s[1]=s[2]; s[2]=s[3]; s[3]=pcm; }
/* Decode superblock `idx` of the sound at `data` into out[640]. */
static inline void oc3_decode_super(const uint8_t *data, uint32_t idx, int16_t *out){
  const uint8_t *p=data+(uint32_t)idx*OC3_SUPER_BYTES; int32_t s[8];
  for(int i=0;i<8;i++) s[i]=(int16_t)(p[2*i]|(p[2*i+1]<<8));
  p+=16;
  for(int c=0;c<2;c++){
    const uint8_t *codes=p, *sc=p+OC3_CODEBYTES; uint32_t bit=0;
    for(int g=0;g<16;g++){
      uint32_t scale=(g&1)?(sc[g>>1]&15):(sc[g>>1]>>4); const int16_t *dq=oual_dqt3+scale*8;
      for(int i=0;i<OC3_GROUP;i++){
        uint32_t by=bit>>3, sh=bit&7; uint32_t w=((uint32_t)codes[by]<<8)|(by+1<OC3_CODEBYTES?codes[by+1]:0);
        uint32_t code=(w>>(13-sh))&7; bit+=3;
        int32_t res=dq[code], pcm=oc3_sat(oc3_pred(s)+res);
        oc3_upd(s,pcm,res); *out++=(int16_t)pcm; } }
    p+=OC3_CHUNKBYTES; } }
static inline uint32_t oc3_blocks(uint32_t frames){ return (frames+OC3_SUPER-1)/OC3_SUPER; }
#endif
