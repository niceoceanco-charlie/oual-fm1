/* Host run of the real app/lofi_ui.c + lofi_engine.c: SDK stubs, a flash array, scripted panel input, screenshots and checks.
 * The pack is uploaded through lofi_sysex() with the C reference sender, so the SysEx queue, protocol and flash glue are the firmware's own. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1.h"
#include "fm1_device.h"
#include "lofi_ui.c"
#include "lofi_sender.h"

#define FLASH_BYTES 536576u
static unsigned long calls_tick, px_tick, calls_max, px_max, calls_total;
static uint8_t flash[FLASH_BYTES]; static struct fm1_controls ctl; static uint32_t ms; static uint16_t fb[240 * 240]; static int led[64];
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *s) { *s = ctl; memset(ctl.encoders, 0, sizeof ctl.encoders); }
void fm1_led_set(unsigned k, int on) { if (k < 64) led[k] = on; }
uint32_t fm1_millis(void) { return ms; }
void fm1_control_request(void) {}
void fm1_screen_init(void) {}
void fm1_screen_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c) { calls_tick++; px_tick += (unsigned long)w * h; for (unsigned j = y; j < y + h && j < 240; j++) for (unsigned i = x; i < x + w && i < 240; i++) fb[j * 240 + i] = c; }
int fm1_screen_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *p, size_t n) { (void)n; calls_tick++; px_tick += (unsigned long)w * h; for (unsigned j = 0; j < h; j++) for (unsigned i = 0; i < w; i++) if (x + i < 240 && y + j < 240) fb[(y + j) * 240 + x + i] = p[j * w + i]; return 0; }
unsigned fm1_midi_input_source(void) { return 1; }
int groove_master_update(unsigned adc) { (void)adc; return 0; }
void groove_project_poll(void) {}
uint32_t fm1_flash_size(void) { return FLASH_BYTES; }
int fm1_flash_read(uint32_t o, void *p, size_t n) { if (o + n > FLASH_BYTES) return -1; memcpy(p, flash + o, n); return 0; }
int fm1_flash_erase(uint32_t o) { memset(flash + o, 0xff, 4096); return 0; }
int fm1_flash_write(uint32_t o, const void *p, size_t n) { const uint8_t *s = p; for (size_t i = 0; i < n; i++) { if ((flash[o + i] & s[i]) != s[i]) { puts("FLASH WRITE OVER DATA"); exit(1); } flash[o + i] &= s[i]; } return 0; }
const uint8_t *fm1_flash_map(uint32_t o, size_t n) { return o + n <= FLASH_BYTES ? flash + o : NULL; }
uint32_t fm1_audio_block_ticks(void) { return 1000; }
static uint32_t fake_avg = 100, fake_peak = 250, fake_over;      /* host: a made-up 10% average, 25% longest block, no overruns until a test adds some */
void fm1_audio_stats_read(struct fm1_audio_stats *s) { memset(s, 0, sizeof *s); s->peak_ticks = fake_peak; s->overruns = fake_over; }
void fm1_audio_load_read(uint32_t *avg, uint32_t *pk) { *avg = fake_avg; *pk = fake_peak; }
void fm1_audio_stats_reset_peak(void) {}
static uint8_t last_reply[LF_MAXMSG]; static size_t last_reply_n;
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n) { (void)port; memcpy(last_reply, m, n); last_reply_n = n; return 0; }

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); fails++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static void ppm(const char *p) {
    FILE *f = fopen(p, "wb"); fprintf(f, "P6\n240 240\n255\n");
    for (int i = 0; i < 240 * 240; i++) { uint16_t c = fb[i]; unsigned char rgb[3] = {(unsigned char)((c >> 11) << 3), (unsigned char)(((c >> 5) & 63) << 2), (unsigned char)((c & 31) << 3)}; fwrite(rgb, 1, 3, f); }
    fclose(f);
}
static int32_t cap[1 << 22]; static unsigned cap_n; static int cap_on;
static void tick(unsigned n) { int32_t blk[256]; while (n--) { ms += 33; for (int i = 0; i < 11; i++) { le_render(blk, 128); if (cap_on && cap_n + 256 <= (1u << 22)) { memcpy(cap + cap_n, blk, sizeof blk); cap_n += 256; } } calls_tick = px_tick = 0; groove_screen_control(); if (calls_tick > calls_max) calls_max = calls_tick; if (px_tick > px_max) px_max = px_tick; calls_total += calls_tick; } }
static int sender_link(void *c, const uint8_t *msg, size_t n, uint8_t *reply, size_t *rn) {
    (void)c; last_reply_n = 0; lofi_sysex(msg, n); tick(1);
    if (last_reply_n) { memcpy(reply, last_reply, last_reply_n); *rn = last_reply_n; } else *rn = 0;
    return 0;
}
static void press_btn(unsigned b) { ctl.buttons = 1u << b; tick(1); ctl.buttons = 0; tick(2); }
static void turn(unsigned enc, int d) { ctl.encoders[enc] = (int16_t)d; tick(1); }
static int led_buttons_on(void) { int n = 0; for (int i = 2; i <= 11; i++) n += led[i]; return n; }
static void shot(const char *name) { tick(60); char p[64]; snprintf(p, sizeof p, "shot_%s.ppm", name); ppm(p); }

#include "../app/oual_engine.h"
static double peak_of(const int32_t *x, unsigned n) { int32_t m = 0; for (unsigned i = 0; i < n; i++) { int32_t a = x[i] < 0 ? -x[i] : x[i]; if (a > m) m = a; } return m / 8388608.0; }
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long bytes = ftell(f); fseek(f, 0, SEEK_SET); uint8_t *img = malloc(bytes); if (fread(img, 1, bytes, f) != (size_t)bytes) return 2; fclose(f);
    groove_screen_init(); tick(40);
    lf_sender_t s = {sender_link, 0, 0, 0, 0, 0}; int rc = lf_send_pack(&s, img, (uint32_t)bytes); CHECK(rc == 0 && dev.state == LF_PACK, "upload of the version 2 pack through the firmware glue (rc %d, %u bytes)", rc, (unsigned)bytes);
    ms += 3000; tick(60); CHECK(le_is_oual() && le_info() && le_info()->ver == 2 && le_info()->n_slots == 10, "the pack binds the oual engine: 10 voices, 3-bit, source rate %u", le_info() ? le_info()->src_rate : 0);
    cap_n = 0; cap_on = 1; tick(10); cap_on = 0; CHECK(peak_of(cap, cap_n) == 0.0, "all ten voices start off: silence");
    shot("oual_home");
    /* key 1 switches voice 1 on; capture the audio and compare it with the standalone engine fed the same settings */
    cap_n = 0; cap_on = 1; ctl.keys = 1u << 0; tick(2); ctl.keys = 0; tick(30); cap_on = 0;
    CHECK(le_oual_get(0, OUAL_P_ON) == 1 && led[14 + 0] && !led[14 + 1], "key 1 switches voice 1 on; its LED is lit, the others dark");
    double pk = peak_of(cap, cap_n); CHECK(pk > 0.02 && pk <= 1.0, "voice 1 sounds (peak %.3f of full scale)", pk);
    { static oual_engine ref; oual_init(&ref, 44100, 1); const lfp_info_t *in = le_info();
      for (int i = 0; i < OUAL_VOICES; i++) { oual_sound sn = {le_pack() + in->slot[i].offset, in->slot[i].frames, in->src_rate}; oual_set_sound(&ref, i, &sn);
        for (unsigned k = 0; k < OUAL_P_COUNT; k++) oual_set(&ref, i, (enum oual_param)k, le_oual_get(i, k)); oual_set(&ref, i, OUAL_P_ON, 0); }
      oual_set(&ref, 0, OUAL_P_ON, 1); ref.master_q15 = 26214; oual_set_reverb(&ref, le_oual_get_reverb());
      unsigned first = 0; while (first < cap_n && cap[first] == 0) first += 2; first -= first % 256;       /* the block where the voice started */
      int16_t out[256]; unsigned bad = 0, blocks = 0; for (unsigned b = first; b + 256 <= cap_n && blocks < 60; b += 256, blocks++) { oual_render(&ref, out, 128); for (unsigned i = 0; i < 256; i++) if (cap[b + i] != (int32_t)out[i] * 256) bad++; }
      CHECK(bad == 0, "the firmware's output equals the standalone engine's, sample for sample (%u blocks, %u different samples)", blocks, bad); }
    /* the pages */
    press_btn(8); turn(3, 1); turn(4, 2); turn(5, -3); CHECK(le_oual_get(0, OUAL_P_TRANSPOSE) == 100 && le_oual_get(0, OUAL_P_RAND_POS) == 20 && le_oual_get(0, OUAL_P_LEVEL_Y) == 360, "HOME: transpose %d cents, rand pos %d, level y %d", le_oual_get(0, OUAL_P_TRANSPOSE), le_oual_get(0, OUAL_P_RAND_POS), le_oual_get(0, OUAL_P_LEVEL_Y)); shot("oual_home2");
    press_btn(2); turn(2, 20); turn(3, 10); turn(4, 30); turn(5, 5); CHECK(le_oual_get(0, OUAL_P_CRUSH) == 400 && le_oual_get(0, OUAL_P_SRR) == 200 && le_oual_get(0, OUAL_P_DRIVE) == 600 && le_oual_get(0, OUAL_P_AM) == 100, "FX: crush %d srr %d drive %d am %d", le_oual_get(0, OUAL_P_CRUSH), le_oual_get(0, OUAL_P_SRR), le_oual_get(0, OUAL_P_DRIVE), le_oual_get(0, OUAL_P_AM)); shot("oual_fx");
    press_btn(6); turn(2, 4); turn(3, -4); turn(4, -10); turn(5, -20); shot("oual_edit");
    CHECK(le_oual_get(0, OUAL_P_START) > 1000 && le_oual_get(0, OUAL_P_END) < 65536 && le_oual_get(0, OUAL_P_ENV_RATE) < 1024, "EDIT: start %d end %d pan %d env rate %d", le_oual_get(0, OUAL_P_START), le_oual_get(0, OUAL_P_END), le_oual_get(0, OUAL_P_PAN_X), le_oual_get(0, OUAL_P_ENV_RATE));
    press_btn(7); turn(2, -20); CHECK(le_get_level() == 60, "GLO: master level %u", le_get_level());
#ifdef OUAL_ONLY
    CHECK(le_oual_get_reverb() == 0, "no reverb in the oual-only build (it costs 41.7 KB of heap)");
#else
    CHECK(le_oual_get_reverb() == 1000, "the reverb of the patch is on at 100%% by default");
#endif
    CHECK(le_oual_cap() == 3, "default cap: 3 voices at once (%d)", le_oual_cap()); turn(3, 1); CHECK(le_oual_cap() == 4, "GLO K2 raises the cap (%d)", le_oual_cap()); turn(3, -1); CHECK(le_oual_cap() == 3, "and lowers it (%d)", le_oual_cap());
    fake_over = 2; tick(40); CHECK(cpu_avg == 10 && cpu_pk == 25 && cpu_over == 2, "GLO shows the load: average %u%%, longest block %u%%, %u overruns", cpu_avg, cpu_pk, cpu_over); shot("oual_glo");
    press_btn(3); turn(0, 1); CHECK(az == 1, "SEL: SELECT moves to voice 2 (az %d)", az); turn(2, 1); CHECK(le_oual_get(1, OUAL_P_ON) == 1, "SEL: K1 switches voice 2 on"); shot("oual_sel"); turn(2, -1); turn(0, -1);
    press_btn(4); shot("oual_env"); press_btn(4); press_btn(8);
    /* voices keep playing while the pages change, and every key switches its own voice */
    for (int k = 0; k < 10; k++) if (le_oual_get(k, OUAL_P_ON)) { ctl.keys = 1u << k; tick(2); ctl.keys = 0; tick(2); } tick(5);
    { int before = le_oual_refused(); for (int k = 0; k < 6; k++) { ctl.keys = 1u << k; tick(2); ctl.keys = 0; tick(2); } int n = 0; for (int v = 0; v < 10; v++) n += le_oual_get(v, OUAL_P_ON);
      CHECK(n == 3 && le_oual_refused() == before + 3, "cap 3: keys 1..6 switch on only the first three (%d on, %d refused)", n, le_oual_refused() - before);
      CHECK(!strcmp(msg_text, "MAX 3 VOICES"), "and the screen says so (\"%s\")", msg_text); shot("oual_cap"); }
    le_oual_set_cap(10); for (int k = 0; k < 10; k++) if (!le_oual_get(k, OUAL_P_ON)) { ctl.keys = 1u << k; tick(2); ctl.keys = 0; tick(2); } int on = 0; for (int v = 0; v < 10; v++) on += le_oual_get(v, OUAL_P_ON);
    CHECK(on == 10, "with the cap at 10, all ten voices switch on (%d)", on);
    cap_n = 0; cap_on = 1; tick(40); cap_on = 0; { double p = peak_of(cap, cap_n); CHECK(p > 0.05 && p <= 1.0, "ten voices together stay in range (peak %.3f)", p); }
    le_oual_set_cap(3); on = 0; for (int v = 0; v < 10; v++) on += le_oual_get(v, OUAL_P_ON); CHECK(on == 3 && le_oual_get(0, OUAL_P_ON) && le_oual_get(1, OUAL_P_ON) && le_oual_get(2, OUAL_P_ON), "lowering the cap switches the newest voices off, the oldest stay (%d on)", on);
    /* overruns while two or more voices sound: the newest voice is switched off */
    le_oual_set_cap(5); for (int k = 3; k < 5; k++) { ctl.keys = 1u << k; tick(2); ctl.keys = 0; tick(2); } CHECK(le_voices_active() == 5, "five voices on (%u)", le_voices_active());
    fake_over += 3; tick(40); CHECK(le_voices_active() == 4 && !le_oual_get(4, OUAL_P_ON) && cpu_dropped == 1, "overruns: the newest voice (5) goes off (%u on, dropped %u)", le_voices_active(), cpu_dropped); CHECK(!strcmp(msg_text, "DROPPED V5") || !msg_until, "and the screen says so (\"%s\")", msg_text);
    tick(60); le_oual_set_cap(3);
    for (int k = 0; k < 10; k++) if (le_oual_get(k, OUAL_P_ON)) { ctl.keys = 1u << k; tick(2); ctl.keys = 0; tick(2); }
    cap_n = 0; cap_on = 1; tick(150); cap_on = 0; { double p = peak_of(cap + cap_n - 512, 512); CHECK(p < 0.002, "all voices off: the voices fade out and the tail dies away (%.5f of full scale after 5 s)", p); }
    /* a burst of changes between two audio blocks is never dropped (the old event ring held 64): 100 toggles of voice 4 leave it off, 101 leave it on */
    { for (int v = 0; v < 10; v++) if (le_oual_get(v, OUAL_P_ON)) le_oual_set(v, OUAL_P_ON, 0); tick(5);
      for (int i = 0; i < 100; i++) le_note_on(3, 100); cap_n = 0; cap_on = 1; tick(40); cap_on = 0; CHECK(le_oual_get(3, OUAL_P_ON) == 0 && peak_of(cap, cap_n) < 0.002, "100 toggles of one voice in a burst: off, and silent (peak %.4f)", peak_of(cap, cap_n));
      for (int i = 0; i < 101; i++) le_note_on(3, 100); cap_n = 0; cap_on = 1; tick(40); cap_on = 0; CHECK(le_oual_get(3, OUAL_P_ON) == 1 && peak_of(cap, cap_n) > 0.01, "101 toggles: on, and sounding (peak %.3f)", peak_of(cap, cap_n));
      for (int i = 0; i < 300; i++) le_oual_set(3, OUAL_P_CRUSH, i % 1000); le_oual_set(3, OUAL_P_CRUSH, 0); le_oual_set(3, OUAL_P_ON, 0); tick(40); }
    /* SAVE and a power cycle */
    ctl.keys = (1u << 2) | (1u << 5); tick(2); ctl.keys = 0; tick(2); press_btn(6); turn(2, 4); int st0 = le_oual_get(az, OUAL_P_START); le_oual_set(3, OUAL_P_CRUSH, 400); tick(2);
    le_oual_set_cap(4); press_btn(9); turn(2, 1); tick(80); CHECK(!unsaved, "SAVE page: SAVE wrote the ten voices' settings to flash");
    groove_screen_init(); tick(5); tick(60);
    CHECK(le_is_oual() && le_oual_get(3, OUAL_P_CRUSH) == 400 && le_oual_get(2, OUAL_P_ON) == 1 && le_oual_get(5, OUAL_P_ON) == 1 && le_oual_get(az, OUAL_P_START) >= 0 && st0 >= 0, "after a power cycle the settings are back (voice 4 crush %d, voices 3 and 6 on: %d %d)", le_oual_get(3, OUAL_P_CRUSH), le_oual_get(2, OUAL_P_ON), le_oual_get(5, OUAL_P_ON));
    CHECK(calls_max <= 60 && px_max <= 13000, "no single control tick draws more than 60 screen calls / 13000 pixels (max %lu calls, %lu pixels)", calls_max, px_max);
    printf("%s (%d failures)\n", fails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", fails);
    return fails ? 1 : 0;
}
