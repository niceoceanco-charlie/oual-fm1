/* oual_wasm: the oual C engine (core/oual_engine.c, the same code the FM-1 runs) and the 3-bit encoder as a WebAssembly module,
 * with a small C ABI. One engine instance lives in static memory. Built freestanding (no libc, no Emscripten): see tools/build_wasm.sh.
 * GPL-3.0-only. Units are the engine's own (see core/oual_engine.h): cents, permille, 0..65536 for the loop window.
 * Memory: JS gets buffers with oual_w_alloc (zeroed, from an arena above __heap_base). Growing the memory detaches old
 * ArrayBuffers, so JS re-creates its views after any call that allocates. oual_w_init releases the whole arena. */
#include <stdint.h>
#include <stddef.h>
#include "../core/oual_engine.h"
#include "../tools/oc3_enc.h"

#define EXPORT(n) __attribute__((export_name(n)))
#define OUAL_W_API 1
#define OUAL_W_MAX_FRAMES 4096
void *oual_arena_alloc(uint32_t n); uint32_t oual_arena_mark(void); void oual_arena_release(uint32_t mark);

static oual_engine g_eng;
static int16_t g_out[2 * OUAL_W_MAX_FRAMES];
static int16_t g_env[OUAL_ENV_POINTS];
static uint32_t g_enc_len;
static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

EXPORT("oual_w_api") int oual_w_api(void) { return OUAL_W_API; }
EXPORT("oual_w_voices") int oual_w_voices(void) { return OUAL_VOICES; }
EXPORT("oual_w_params") int oual_w_params(void) { return OUAL_P_COUNT; }
EXPORT("oual_w_super") int oual_w_super(void) { return OC3_SUPER; }
/* Start over: engine reset (all voices off, no sounds, master 0.8, reverb 0), arena released. out_rate 8000..192000; the reverb's delay lines are sized for 48 kHz and shorten above that. */
EXPORT("oual_w_init") int oual_w_init(uint32_t out_rate, uint32_t seed) {
  if (out_rate < 8000 || out_rate > 192000) return -1;
  oual_arena_release(0); oual_init(&g_eng, out_rate, seed); g_enc_len = 0; return 0; }
EXPORT("oual_w_alloc") uint8_t *oual_w_alloc(uint32_t n) { return (uint8_t *)oual_arena_alloc(n); }
/* A sound already in memory: `data` is the packed 3-bit blocks (no header). */
EXPORT("oual_w_set_sound") int oual_w_set_sound(int v, const uint8_t *data, uint32_t frames, uint32_t rate) {
  if (v < 0 || v >= OUAL_VOICES || !data || !frames || rate < 4000 || rate > 96000) return -1;
  oual_sound s = { data, frames, rate }; oual_set_sound(&g_eng, v, &s); return 0; }
/* A .o3 file in memory ("OC3A", frames, rate, blocks), as written by tools/enc3 and oual_w_encode. */
EXPORT("oual_w_load_o3") int oual_w_load_o3(int v, const uint8_t *file, uint32_t len) {
  if (len < 12 || rd32(file) != 0x4133434fu) return -2;
  uint32_t frames = rd32(file + 4), rate = rd32(file + 8), blocks = (frames + OC3_SUPER - 1) / OC3_SUPER;
  if ((uint64_t)blocks * OC3_SUPER_BYTES + 12 > len) return -3;
  return oual_w_set_sound(v, file + 12, frames, rate); }
/* A version 2 pack (.lfp, see tools/mkpack3.c): sound n goes to voice n, with the pack's loop window. Returns the number of sounds. */
EXPORT("oual_w_load_pack") int oual_w_load_pack(const uint8_t *pk, uint32_t len) {
  if (len < 1152 || rd32(pk) != 0x3150464cu || pk[4] != 2) return -2;
  int n = pk[6]; if (n < 1 || n > OUAL_VOICES) return -3;
  uint32_t rate = rd32(pk + 32);
  for (int i = 0; i < n; i++) {
    const uint8_t *s = pk + 64 + i * 32; uint32_t off = rd32(s), frames = rd32(s + 4), st = rd32(s + 8), en = rd32(s + 12);
    uint32_t bytes = ((frames + OC3_SUPER - 1) / OC3_SUPER) * OC3_SUPER_BYTES;
    if (!frames || off < 1152 || (uint64_t)off + bytes > len || oual_w_set_sound(i, pk + off, frames, rate) < 0) return -4;
    if (en > frames) en = frames; if (st >= en) { st = 0; en = frames; }
    oual_set(&g_eng, i, OUAL_P_START, (int)((uint64_t)st * 65536u / frames)); oual_set(&g_eng, i, OUAL_P_END, (int)((uint64_t)en * 65536u / frames)); }
  return n; }
EXPORT("oual_w_set") void oual_w_set(int v, int p, int value) { if (v >= 0 && v < OUAL_VOICES && p >= 0 && p < OUAL_P_COUNT) oual_set(&g_eng, v, (enum oual_param)p, value); }
EXPORT("oual_w_set_reverb") void oual_w_set_reverb(int permille) { oual_set_reverb(&g_eng, permille); }
EXPORT("oual_w_set_master") void oual_w_set_master(int q15) { g_eng.master_q15 = q15 < 0 ? 0 : q15 > 32768 ? 32768 : q15; }
EXPORT("oual_w_env_buf") int16_t *oual_w_env_buf(void) { return g_env; }       /* OUAL_ENV_POINTS int16 values, 32767 = full */
EXPORT("oual_w_set_envelope") void oual_w_set_envelope(int v) { if (v >= 0 && v < OUAL_VOICES) oual_set_envelope(&g_eng, v, g_env); }
EXPORT("oual_w_out") int16_t *oual_w_out(void) { return g_out; }               /* interleaved stereo int16 */
EXPORT("oual_w_render") int oual_w_render(int frames) {
  if (frames < 1 || frames > OUAL_W_MAX_FRAMES) return -1;
  oual_render(&g_eng, g_out, frames); return frames; }
EXPORT("oual_w_stat_decoded") uint32_t oual_w_stat_decoded(void) { return g_eng.stat_decoded; }
EXPORT("oual_w_stat_wraps") uint32_t oual_w_stat_wraps(void) { return g_eng.stat_wraps; }
/* Encode mono s16 PCM at the source rate (22050 is what the device uses) into a .o3 file in the arena; returns its address, its size is oual_w_encoded_len(). */
EXPORT("oual_w_encode") uint8_t *oual_w_encode(const int16_t *pcm, uint32_t frames, uint32_t rate) {
  g_enc_len = 0; if (!pcm || !frames || frames > 64u * 1024u * 1024u) return 0;
  uint32_t nb = oc3_blocks(frames), bytes = 12 + nb * OC3_SUPER_BYTES, mark = oual_arena_mark();
  uint8_t *o = (uint8_t *)oual_arena_alloc(bytes); if (!o) return 0;
  uint32_t after = oual_arena_mark();
  o[0] = 'O'; o[1] = 'C'; o[2] = '3'; o[3] = 'A'; o[4] = (uint8_t)frames; o[5] = (uint8_t)(frames >> 8); o[6] = (uint8_t)(frames >> 16); o[7] = (uint8_t)(frames >> 24);
  o[8] = (uint8_t)rate; o[9] = (uint8_t)(rate >> 8); o[10] = (uint8_t)(rate >> 16); o[11] = (uint8_t)(rate >> 24);
  oc3_encode(pcm, frames, o + 12); oual_arena_release(after); (void)mark; g_enc_len = bytes; return o; }
EXPORT("oual_w_encoded_len") uint32_t oual_w_encoded_len(void) { return g_enc_len; }
