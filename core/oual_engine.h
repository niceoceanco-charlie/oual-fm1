/* oual_engine.h: a 10-voice looping sampler with Crush, SRR, Drive, AM, random loop positions and a per-voice envelope.
 * GPL-3.0-only. Integer only (no float, no libm), so it can move onto the FM-1 unchanged.
 *
 * Re-implemented from what Philip Meyer's oual_qroccss Max patch does (MIT, (c) 2025 Philip Meyer; "Original: anonymous,
 * 2007; Update 1: Philip Meyer, 2024"). No Max code is copied. See docs/PORT_NOTES.md for what was read from the patch and
 * what is an interpretation.
 */
#ifndef OUAL_ENGINE_H
#define OUAL_ENGINE_H
#include <stdint.h>
#include <stdint.h>
#ifndef OC3_SUPER
#define OC3_SUPER 640   /* the codec block, see oual_codec3.h */
#endif
#define OUAL_VOICES 10
#define OUAL_ENV_POINTS 64
#define OUAL_CACHE_SLOTS 3

typedef struct { const uint8_t *data; uint32_t frames; uint32_t rate; } oual_sound; /* 3-bit packed, see oual_codec3.h */

enum oual_param {
  OUAL_P_ON,            /* 0 or 1 */
  OUAL_P_TRANSPOSE,     /* cents, -1200..1200 (the patch's Transpose knob is +-12 semitones) */
  OUAL_P_RAND_POS,      /* 0..1000 permille (knob 0..100 percent) */
  OUAL_P_CRUSH,         /* 0..1000: bits = (1000-v)*16/1000 + 1 */
  OUAL_P_SRR,           /* 0..1000: hold ratio = ((1000-v)/1000)^2 */
  OUAL_P_DRIVE,         /* 0..1000: drive factor = 1 + 10*v/1000 */
  OUAL_P_AM,            /* 0..1000: mix of the 12 kHz amplitude modulation */
  OUAL_P_START,         /* loop start, 0..65536 of the sound's length */
  OUAL_P_END,           /* loop end, 0..65536 */
  OUAL_P_ENV_RATE,      /* -1024..1024 (= -1.0..1.0), envelope cycles per loop = 1/|v| */
  OUAL_P_PAN_X,         /* 0..1000 (0 left .. 1 right, equal power) */
  OUAL_P_LEVEL_Y,       /* 0..1000, level = (1 - y)^2 */
  OUAL_P_COUNT
};

typedef struct {
  int16_t buf[OC3_SUPER]; int32_t block; uint32_t age;
} oual_slot;

typedef struct {
  /* control values */
  oual_sound snd;
  int on, transpose, rand_pos, crush, srr, drive, am, env_rate, pan_x, level_y;
  uint32_t start_q16, end_q16;
  int16_t env[OUAL_ENV_POINTS];
  /* derived at control rate */
  uint32_t pitch_q16;      /* playback ratio incl. source/output rate, Q16 */
  uint32_t dgain_q8;       /* drive factor, Q8 */
  uint32_t srr_q16;        /* hold ratio, Q16 (65536 = off) */
  int crush_shift;         /* 16 - bits, 0 = off */
  int32_t gate_t, am_t, level_t, panl_t, panr_t;    /* smoothing targets, Q23 */
  /* running */
  int32_t gate, am_c, level, panl, panr;            /* smoothed, Q23 */
  uint64_t pos;            /* source position, Q32 */
  uint64_t step;           /* Q32 per output sample */
  uint32_t ph, dph;        /* loop phase, Q32 of the loop */
  uint32_t env_ph, env_dph;
  int32_t ls, le;          /* current loop window, in source samples */
  uint32_t srr_acc; int32_t held; uint32_t am_ph, am_dph;
  uint32_t rng; int started, dirty;
  oual_slot slot[OUAL_CACHE_SLOTS]; uint32_t tick;
} oual_voice;

typedef struct {
  uint32_t out_rate;
  int master_q15;
  oual_voice v[OUAL_VOICES];
  uint32_t stat_decoded;   /* samples decoded since init (cost accounting) */
  uint32_t stat_wraps;
} oual_engine;

void oual_init(oual_engine *e, uint32_t out_rate, uint32_t seed);
void oual_set_sound(oual_engine *e, int v, const oual_sound *s);
void oual_set(oual_engine *e, int v, enum oual_param p, int value);   /* safe to call from the audio side */
void oual_set_envelope(oual_engine *e, int v, const int16_t env[OUAL_ENV_POINTS]);
/* Interleaved stereo int16, `frames` frames. */
void oual_render(oual_engine *e, int16_t *out, int frames);
#endif
