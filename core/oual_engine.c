/* oual_engine.c: see oual_engine.h. Integer only. GPL-3.0-only. */
#include <string.h>
#include "oual_engine.h"
#include "oual_codec3.h"

#define OUAL_MINLOOP 8            /* shortest loop window, source samples */
static inline OUAL_CODE uint32_t oual_xs(uint32_t *s){ uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; return *s=x; }
static OUAL_CODE uint32_t oual_isqrt32(uint32_t n){ uint32_t r=0,b=1u<<30; while(b>n)b>>=2; while(b){ if(n>=r+b){ n-=r+b; r=(r>>1)+b; } else r>>=1; b>>=2; } return r; }

/* sine of a 32-bit phase (one cycle = 2^32), table with linear interpolation */
static inline OUAL_CODE int32_t oual_sin32(uint32_t ph){ uint32_t i=ph>>22; int32_t a=oual_sin_tab[i], b=oual_sin_tab[i+1]; return a+(((b-a)*(int32_t)((ph>>6)&0xFFFF))>>16); }  /* |b-a| <= 201, so this stays in 32 bits */

/* ---- decoded-block cache (the data stays packed in flash; blocks are decoded on demand) ---- */
static inline OUAL_CODE const int16_t *oual_blk(oual_engine *e, oual_voice *v, int32_t b){
  for(int i=0;i<OUAL_CACHE_SLOTS;i++) if(v->slot[i].block==b){ v->slot[i].age=++v->tick; return v->slot[i].buf; }
  int old=0; for(int i=1;i<OUAL_CACHE_SLOTS;i++) if(v->slot[i].age<v->slot[old].age) old=i;
  oc3_decode_super(v->snd.data,(uint32_t)b,v->slot[old].buf); v->slot[old].block=b; v->slot[old].age=++v->tick; e->stat_decoded+=OC3_SUPER;
  return v->slot[old].buf; }
static inline OUAL_CODE int32_t oual_smp(oual_engine *e, oual_voice *v, int32_t i){
  if(i<0) i=0; if(i>=(int32_t)v->snd.frames) i=(int32_t)v->snd.frames-1;
  int32_t b=i/OC3_SUPER; return oual_blk(e,v,b)[i-b*OC3_SUPER]; }

/* ---- control-rate derivations ---- */
static OUAL_CODE void oual_derive(oual_engine *e, oual_voice *v){
  int c=v->transpose; int oct=0; while(c<0){ c+=1200; oct--; } while(c>=1200){ c-=1200; oct++; }
  uint64_t r=oual_pow2_cents[c];                           /* Q16 */
  uint64_t base=((uint64_t)v->snd.rate<<16)/ (e->out_rate?e->out_rate:44100);   /* source/output rate, Q16 */
  uint64_t p=(r*base)>>16; if(oct>0) p<<=oct; else if(oct<0) p>>=-oct;
  v->pitch_q16=(uint32_t)p; v->step=(uint64_t)v->pitch_q16<<16;
  v->dgain_q8=256+(uint32_t)(v->drive*2560/1000);
  { uint32_t k=1000-(uint32_t)v->srr; v->srr_q16=(uint32_t)(((uint64_t)k*k*65536u)/1000000u); }
  { int bits=(1000-v->crush)*16/1000+1; v->crush_shift = bits>=16?0:16-bits; }
  v->gate_t=v->on?(1<<23):0; v->am_t=(int32_t)(((int64_t)v->am<<23)/1000);
  { uint32_t k=1000-(uint32_t)v->level_y; v->level_t=(int32_t)(((uint64_t)k*k<<23)/1000000u); }
  { uint32_t px=(uint32_t)v->pan_x*32768u/1000u; v->panr_t=(int32_t)(oual_isqrt32(px<<15)<<8); v->panl_t=(int32_t)(oual_isqrt32((32768u-px)<<15)<<8); }
  v->am_dph=(uint32_t)(((uint64_t)12000<<32)/(e->out_rate?e->out_rate:44100)); v->dirty=0; }

/* ---- loop window: the patch's "grnish" subpatch. On every wrap the window is re-drawn around the base window:
 *   start' = start + L*rp*(r1-0.5);  end' = start' + L + L*rp*(r2-0.5);  both clipped to the sound. ---- */
static OUAL_CODE void oual_new_window(oual_voice *v){
  int32_t dur=(int32_t)v->snd.frames; int64_t bs=((int64_t)v->start_q16*dur)>>16, be=((int64_t)v->end_q16*dur)>>16;
  if(be<bs){ int64_t t=bs; bs=be; be=t; }
  int64_t L=be-bs, sc=(L*v->rand_pos)/1000, s=bs, e;
  if(sc){ s=bs+((sc*((int64_t)(oual_xs(&v->rng)&0xFFFF)-32768))>>16); }
  if(s<0)s=0; if(s>dur)s=dur;
  e=s+L; if(sc) e+=((sc*((int64_t)(oual_xs(&v->rng)&0xFFFF)-32768))>>16);
  if(e<0)e=0; if(e>dur)e=dur;
  if(e-s<OUAL_MINLOOP){ e=s+OUAL_MINLOOP; if(e>dur){ e=dur; s=dur-OUAL_MINLOOP; if(s<0)s=0; } }
  v->ls=(int32_t)s; v->le=(int32_t)e;
  { uint64_t len=(uint64_t)(v->le-v->ls); v->dph=(uint32_t)((v->step)/len); }   /* loop phase per output sample */
  if(v->env_rate){ int64_t k=(1024*1024)/v->env_rate; if(k>64*1024)k=64*1024; if(k<-64*1024)k=-64*1024;
    v->env_dph=(uint32_t)(int32_t)(((int64_t)v->dph*k)>>10); } else v->env_dph=0; }

void oual_init(oual_engine *e, uint32_t out_rate, uint32_t seed){
  memset(e,0,sizeof *e); e->out_rate=out_rate; e->master_q15=26214; /* 0.8 */
  { uint32_t r=out_rate?out_rate:44100; e->rvb_n0=(uint16_t)(r*65/1000); e->rvb_n1=(uint16_t)(r*67/1000); e->rvb_n2=(uint16_t)(r*85/1000);
    if(e->rvb_n0>OUAL_RVB_N0)e->rvb_n0=OUAL_RVB_N0; if(e->rvb_n1>OUAL_RVB_N1)e->rvb_n1=OUAL_RVB_N1; if(e->rvb_n2>OUAL_RVB_N2)e->rvb_n2=OUAL_RVB_N2; }
  for(int i=0;i<OUAL_VOICES;i++){ oual_voice *v=&e->v[i]; v->rng=seed*2654435761u+i*40503u+1; if(!v->rng)v->rng=1;
    v->end_q16=65536; v->pan_x=500; v->level_y=500; v->env_rate=1024; for(int k=0;k<OUAL_ENV_POINTS;k++) v->env[k]=32767;
    for(int s=0;s<OUAL_CACHE_SLOTS;s++) v->slot[s].block=-1; v->dirty=1; } }

void oual_set_sound(oual_engine *e, int i, const oual_sound *s){
  oual_voice *v=&e->v[i]; v->snd=*s; for(int k=0;k<OUAL_CACHE_SLOTS;k++){ v->slot[k].block=-1; v->slot[k].age=0; }
  v->started=0; v->dirty=1; }
void oual_set_envelope(oual_engine *e, int i, const int16_t env[OUAL_ENV_POINTS]){ memcpy(e->v[i].env,env,sizeof e->v[i].env); }
OUAL_CODE void oual_set(oual_engine *e, int i, enum oual_param p, int val){
  oual_voice *v=&e->v[i];
  switch(p){
    case OUAL_P_ON: v->on=val?1:0; break;       case OUAL_P_TRANSPOSE: v->transpose=val; break;
    case OUAL_P_RAND_POS: v->rand_pos=val; break; case OUAL_P_CRUSH: v->crush=val; break;
    case OUAL_P_SRR: v->srr=val; break;          case OUAL_P_DRIVE: v->drive=val; break;
    case OUAL_P_AM: v->am=val; break;            case OUAL_P_START: v->start_q16=(uint32_t)val; break;
    case OUAL_P_END: v->end_q16=(uint32_t)val; break; case OUAL_P_ENV_RATE: v->env_rate=val; break;
    case OUAL_P_PAN_X: v->pan_x=val; break;      case OUAL_P_LEVEL_Y: v->level_y=val; break; case OUAL_P_COUNT: return; }
  v->dirty=1; if(p==OUAL_P_START||p==OUAL_P_END||p==OUAL_P_TRANSPOSE||p==OUAL_P_ENV_RATE) v->started=v->started?2:0; }

static inline OUAL_CODE int32_t oual_hermite(int32_t ym, int32_t y0, int32_t y1, int32_t y2, int32_t t /*Q12*/){
  int32_t c1=(y1-ym)>>1, c2=ym-((5*y0)>>1)+2*y1-(y2>>1), c3=((y2-ym)>>1)+((3*(y0-y1))>>1);
  return y0+((((((c3*t)>>12)+c2)*t>>12)+c1)*t>>12); }

static OUAL_CODE void oual_render_voice(oual_engine *e, oual_voice *v, int32_t *mix, int frames){
  if(!v->snd.data || !v->snd.frames) return;
  if(v->dirty) oual_derive(e,v);
  if(!v->on && v->gate<(1<<8)) { v->gate=0; return; }       /* off and faded out: costs nothing */
  if(v->started!=1){ int fresh=!v->started; oual_new_window(v); if(fresh){ v->pos=(uint64_t)v->ls<<32; v->ph=0; v->env_ph=0; v->srr_acc=0; v->held=0; }
    else { if((int32_t)(v->pos>>32)<v->ls||(int32_t)(v->pos>>32)>=v->le) { v->pos=(uint64_t)v->ls<<32; v->ph=0; } }
    v->started=1; }
  for(int n=0;n<frames;n++){
    int32_t idx=(int32_t)(v->pos>>32); int32_t t=(int32_t)((v->pos>>20)&0xFFF);
    int32_t s;
    { int32_t i0=idx-1, b0=i0/OC3_SUPER, r0=i0-b0*OC3_SUPER;
      if(i0>=0 && r0<=OC3_SUPER-4 && idx+2<(int32_t)v->snd.frames){ const int16_t *q=oual_blk(e,v,b0)+r0; s=oual_hermite(q[0],q[1],q[2],q[3],t); }   /* fast path: all four taps in one block */
      else s=oual_hermite(oual_smp(e,v,idx-1),oual_smp(e,v,idx),oual_smp(e,v,idx+1),oual_smp(e,v,idx+2),t); }
    /* edge window: clip(sin(pi*phase), 0.1)*10 */
    { int32_t sw=oual_sin32(v->ph>>1)*10; if(sw>32767)sw=32767; s=(s*sw)>>15; }
    /* Drive: linear to 0.5, quadratic knee to 1.0 at 1.5 (an interpretation of overdrive~), bypassed at drive 0 */
    if(v->dgain_q8!=256){ int32_t x=(s*(int32_t)v->dgain_q8)>>8, a=x<0?-x:x;
      if(a>49152) a=32767; else if(a>16384){ int32_t d=a-16384; a=a-((d*d)>>16); }
      s=x<0?-a:a; }
    /* SRR: sample and hold at ratio srr_q16 */
    if(v->srr_q16<65536){ v->srr_acc+=v->srr_q16; if(v->srr_acc>=65536){ v->srr_acc-=65536; v->held=s; } s=v->held; }
    /* Crush: round to `bits` bits */
    if(v->crush_shift){ int sh=v->crush_shift; s=((s+(1<<(sh-1)))>>sh)*(1<<sh); if(s>32767)s=32767; }
    /* AM: crossfade between 1 and a 12 kHz raised cosine */
    v->am_c+=(v->am_t-v->am_c)>>8;
    { uint32_t cph=v->am_ph; v->am_ph+=v->am_dph;
      int32_t cs=oual_sin32(cph+0x40000000u), g=(32768+cs)>>1, a=v->am_c>>8;
      s=(s*((32768-a)+((a*g)>>15)))>>15; }
    /* gate, level, envelope, pan */
    v->gate+=(v->gate_t-v->gate)>>8; v->level+=(v->level_t-v->level)>>8;
    { uint32_t ep=v->env_ph>>26; int32_t ev=v->env[ep]+((((int32_t)v->env[(ep+1)&(OUAL_ENV_POINTS-1)]-v->env[ep])*(int32_t)((v->env_ph>>10)&0xFFFF))>>16);
      int32_t g=((v->gate>>8)*(v->level>>8))>>15; g=(g*ev)>>15; s=(s*g)>>15; }
    v->panl+=(v->panl_t-v->panl)>>8; v->panr+=(v->panr_t-v->panr)>>8;
    mix[2*n]+=(s*(v->panl>>8))>>15; mix[2*n+1]+=(s*(v->panr>>8))>>15;
    /* advance */
    v->pos+=v->step; v->ph+=v->dph; v->env_ph+=v->env_dph;
    if((int32_t)(v->pos>>32)>=v->le){
      uint64_t over=v->pos-((uint64_t)v->le<<32); e->stat_wraps++;
      oual_new_window(v); v->pos=((uint64_t)v->ls<<32)+over; if((int32_t)(v->pos>>32)>=v->le) v->pos=(uint64_t)v->ls<<32;
      v->ph=(uint32_t)(((over>>16)/(uint64_t)(v->le-v->ls))<<16); v->env_ph=0; } } }

/* ---- the reverb: in -> *0.14 -> reson~ -> three allpass filters (y = c*w + w[n-D], w = x - c*w[n-D], c = 0.7) -> *0.5, added to the dry signal ---- */
OUAL_CODE void oual_set_reverb(oual_engine *e, int permille){ e->rvb_permille = permille < 0 ? 0 : permille > 1000 ? 1000 : permille; }
static OUAL_CODE int32_t oual_ap(int16_t *d, uint16_t *ix, unsigned n, int32_t x){
  unsigned i=*ix; int32_t wd=d[i]; int32_t w=x-((22938*wd)>>15); if(w>32767)w=32767; if(w<-32768)w=-32768;
  int32_t y=((22938*w)>>15)+wd; d[i]=(int16_t)w; if(++i>=n) i=0; *ix=(uint16_t)i; return y>32767?32767:y<-32768?-32768:y; }
static OUAL_CODE __attribute__((noinline)) void oual_reson_block(oual_rvb_ch *c, const int32_t *in, int32_t *out, int n, int stride){   /* 64-bit products kept out of line */
  int32_t y1=c->y1, y2=c->y2;
  for(int k=0;k<n;k++){
    int32_t x=in[k*stride]>>4;                        /* the mix is up to 19 bits; the wet path works at 16 */
    int32_t y=(int32_t)(((int64_t)OUAL_RVB_A0*x)>>16)+(int32_t)(((int64_t)OUAL_RVB_B1*y1)>>24)+(int32_t)(((int64_t)OUAL_RVB_B2*y2)>>24);
    y2=y1; y1=y; int32_t o=y>>8; out[k]=o>32767?32767:o<-32768?-32768:o; }
  c->y1=y1; c->y2=y2; }
OUAL_CODE void oual_rvb_run(oual_engine *e, int32_t *mix, int frames){
  int32_t tmp[512];
  for(int ch=0;ch<2;ch++){
    oual_rvb_ch *c=&e->rvb[ch]; oual_reson_block(c,mix+ch,tmp,frames,2);
    for(int k=0;k<frames;k++){
      int32_t w=oual_ap(c->d0,&c->i0,e->rvb_n0,tmp[k]); w=oual_ap(c->d1,&c->i1,e->rvb_n1,w); w=oual_ap(c->d2,&c->i2,e->rvb_n2,w);
      mix[2*k+ch]+=(w*e->rvb_permille/1000)*8; } } }      /* *0.5 and back to the mix scale: (w * 16) / 2 */
OUAL_CODE void oual_render(oual_engine *e, int16_t *out, int frames){
  static int32_t mix[2*512]; 
  while(frames>0){ int n=frames>512?512:frames; for(int i=0;i<2*n;i++) mix[i]=0;   /* no libc in the audio interrupt */
    for(int i=0;i<OUAL_VOICES;i++) oual_render_voice(e,&e->v[i],mix,n);
    if(e->rvb_permille) oual_rvb_run(e,mix,n);
    for(int i=0;i<2*n;i++){ int32_t x=((mix[i]*(e->master_q15>>4))>>11), a=x<0?-x:x;   /* 32-bit only: 10 voices (19 bits) x 11 bits */  /* limiter: linear to 0.7, knee to 1.0 at 1.3 */
      if(a>42598) a=32767; else if(a>22938){ int32_t d=a-22938; a=a-((((d*d)>>15)*27307)>>15); if(a>32767)a=32767; }
      out[i]=(int16_t)(x<0?-a:a); }
    out+=2*n; frames-=n; } }
