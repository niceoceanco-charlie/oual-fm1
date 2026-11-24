/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 * oual mode screens: included by lofi_ui.c. A version 2 pack turns the instrument into ten looping voices (oual_engine.h).
 * Keys 1..10 switch the voices; SELECT picks the voice the pages edit; K1..K4 are the four tiles of each page.
 *   HOME  STATE, TRANSPOSE, RAND POS, LEVEL      FX   CRUSH, SRR, DRIVE, AM      EDIT  START, END, PAN, ENV RATE
 *   GLO   MASTER                                 SEL  STATE                       SAVE  as before (saves the ten voices' settings)
 * ENV, LFO, ARP and SEQ belong to the sampler and show a note. Not built yet: the envelope shape editor, presets. */
/* The audio interrupt's load, from the stats the firmware already keeps: the longest time one 128-frame block took, against the time a block lasts. */
struct fm1_audio_stats;
extern uint32_t fm1_audio_block_ticks(void);
extern void fm1_audio_stats_reset_peak(void);
static unsigned cpu_pct, cpu_over; static uint32_t cpu_t;
static void cpu_poll(uint32_t now)
{
    if (now - cpu_t < 500U) return; cpu_t = now;
    struct fm1_audio_stats st; fm1_audio_stats_read(&st); uint32_t bt = fm1_audio_block_ticks();
    unsigned p = bt ? (unsigned)((uint64_t)st.peak_ticks * 100 / bt) : 0; if (p != cpu_pct || st.overruns != cpu_over) { cpu_pct = p; cpu_over = st.overruns; dirty |= 1; }
}
static void o_pct(tile_t *t, const char *name, int v1000)
{
    char b[8]; unsigned n = (unsigned)fmt_int(b, v1000 / 10); b[n++] = '%'; b[n] = 0; tile(t, name, b, v1000 / 10);
}
static unsigned o_voice(void) { return az < 0 || az >= OUAL_VOICES ? 0u : (unsigned)az; }
static int oual_tiles(unsigned pg, tile_t t[4])
{
    unsigned v = o_voice(), n; char b[16];
    for (unsigned i = 0; i < 4; ++i) tile(&t[i], "", "", -1);
    switch (pg) {
    case PG_HOME: {
        int tr = le_oual_get(v, OUAL_P_TRANSPOSE), a = tr < 0 ? -tr : tr; n = 0; b[n++] = tr < 0 ? '-' : '+'; n += (unsigned)fmt_int(b + n, a / 100); n = put(b, n, " ST");
        tile(&t[0], "STATE", le_oual_get(v, OUAL_P_ON) ? "ON" : "OFF", -1); tile(&t[1], "TRANSP", b, (tr + 1200) / 24);
        o_pct(&t[2], "RAND POS", le_oual_get(v, OUAL_P_RAND_POS)); o_pct(&t[3], "LEVEL", 1000 - le_oual_get(v, OUAL_P_LEVEL_Y)); return 1; }
    case PG_FX: o_pct(&t[0], "CRUSH", le_oual_get(v, OUAL_P_CRUSH)); o_pct(&t[1], "SRR", le_oual_get(v, OUAL_P_SRR)); o_pct(&t[2], "DRIVE", le_oual_get(v, OUAL_P_DRIVE)); o_pct(&t[3], "AM", le_oual_get(v, OUAL_P_AM)); return 1;
    case PG_EDIT: {
        int e = le_oual_get(v, OUAL_P_ENV_RATE), a = e < 0 ? -e : e; n = 0; b[n++] = e < 0 ? '-' : '+'; n += (unsigned)fmt_int(b + n, a / 1024); b[n++] = '.'; { unsigned f = (unsigned)(a % 1024) * 100 / 1024; b[n++] = (char)('0' + f / 10); b[n++] = (char)('0' + f % 10); } b[n] = 0;
        o_pct(&t[0], "START", le_oual_get(v, OUAL_P_START) * 1000 / 65536); o_pct(&t[1], "END", le_oual_get(v, OUAL_P_END) * 1000 / 65536); o_pct(&t[2], "PAN", le_oual_get(v, OUAL_P_PAN_X)); tile(&t[3], "ENV RATE", b, (e + 1024) * 100 / 2048); return 1; }
    case PG_GLO: tile_int(&t[0], "MASTER", (int)le_get_level(), 0, 127); o_pct(&t[1], "REVERB", le_oual_get_reverb()); tile_int(&t[2], "CPU PEAK", (int)cpu_pct, 0, 100); tile_int(&t[3], "OVERRUNS", (int)cpu_over, 0, 100); return 1;
    case PG_SEL: tile(&t[0], "STATE", le_oual_get(v, OUAL_P_ON) ? "ON" : "OFF", -1); return 1;
    case PG_SAVE: return 0;
    default: return 1;
    }
}
static int oual_adjust(unsigned pg, unsigned k, int d)
{
    unsigned v = o_voice(); int sgn = d > 0 ? 1 : -1;
#define OSET(p, f) le_oual_set(v, p, (f))
#define OGET(p) le_oual_get(v, p)
    switch (pg) {
    case PG_HOME: if (k == 0) OSET(OUAL_P_ON, sgn > 0); else if (k == 1) OSET(OUAL_P_TRANSPOSE, OGET(OUAL_P_TRANSPOSE) + d * 100); else if (k == 2) OSET(OUAL_P_RAND_POS, OGET(OUAL_P_RAND_POS) + d * 10); else OSET(OUAL_P_LEVEL_Y, OGET(OUAL_P_LEVEL_Y) - d * 10); break;
    case PG_FX: { static const uint8_t pm[4] = {OUAL_P_CRUSH, OUAL_P_SRR, OUAL_P_DRIVE, OUAL_P_AM}; OSET(pm[k], OGET(pm[k]) + d * 10); break; }
    case PG_EDIT: if (k == 0) OSET(OUAL_P_START, OGET(OUAL_P_START) + d * 328); else if (k == 1) OSET(OUAL_P_END, OGET(OUAL_P_END) + d * 328); else if (k == 2) OSET(OUAL_P_PAN_X, OGET(OUAL_P_PAN_X) + d * 10); else OSET(OUAL_P_ENV_RATE, OGET(OUAL_P_ENV_RATE) + d * 16); break;
    case PG_GLO: if (k == 0) le_set_level((unsigned)bump((int)le_get_level(), d, 0, 127)); else if (k == 1) le_oual_set_reverb(le_oual_get_reverb() + d * 10); else { fm1_audio_stats_reset_peak(); cpu_t = 0; } break;      /* K3 / K4: start the CPU peak over */
    case PG_SEL: if (k == 0) OSET(OUAL_P_ON, sgn > 0); break;
    case PG_SAVE: return 0;
    default: break;
    }
#undef OSET
#undef OGET
    return 1;
}
static void oual_vis_note(void)      /* pages that belong to the sampler */
{
    draw_text(8, 60, 1, "NOT USED IN OUAL MODE", COL_DIM, COL_BG); draw_text(8, 76, 1, "HOME FX EDIT GLO SEL SAVE", COL_FG, COL_BG); draw_text(8, 92, 1, "KEYS 1-10 SWITCH THE VOICES", COL_DIM, COL_BG);
}
