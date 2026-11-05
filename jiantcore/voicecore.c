/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT VOICE for VCV Rack: the VOICE core. The firmware's sources as its browser build compiles them
 * (web/emu/felucca_web.c), used the way its host renders use them (tests/hostsim.c: one part, mix_block), without the
 * device's main loop: part 1 plays the VOICE engine (eng_formant.c, its words sing.c), notes go to it directly
 * (voice.c trk_note_on / trk_note_off), the lyric is the part's (sing.c lyr_set), and the vocal tract is drawn by the
 * firmware's own code (ui_engvis.c vx_draw) on its canvas (gfx.c). Nothing of the firmware changes for this file. */
#include "../jiant/web/emu/felucca_web.c"
#include "voicecore.h"

#if defined(_WIN32)
#define JVO_EXPORT __declspec(dllexport)
#else
#define JVO_EXPORT __attribute__((visibility("default")))
#endif

#define JVO_T (&trk[0])

static uint32_t jvo_ms_due;
static float jvo_l[CTL], jvo_r[CTL];
static uint32_t jvo_have, jvo_pos;

static const uint8_t JVO_ID[JVO_NP] = {P_E0, P_E0 + 1, P_E0 + 2, P_E0 + 3, P_E0 + 4, P_E0 + 5, P_E0 + 6, P_E0 + 7,
                                       P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV,
                                       P_LEVEL, P_VOICE, P_VLANG, P_VLYR};

static void jvo_init(uint32_t seed)
{
    memset(nor, 0xFF, sizeof nor);               /* a fresh device's flash: the settings' defaults */
    persist_boot();
    settings_init();
    prefs_update();
    host_tracks_init();                          /* (sing_init: the words) */
    trk_live = 1;
    session_dice(seed ? seed : 1u);
    host_preset(JVO_T, ENGI_VOICE, 0);
    jvo_have = jvo_pos = 0;
}

static void jvo_block(void)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    for (i = 0; i < CTL; i++) {
        int32_t l = o[2u * i], r = o[2u * i + 1u];
        l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
        r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
        jvo_l[i] = (float)l / 32768.0f;
        jvo_r[i] = (float)r / 32768.0f;
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)l;
    }
    jvo_ms_due += CTL * 1000u;
    while (jvo_ms_due >= FS) {
        jvo_ms_due -= FS;
        fm1_ms++;
        web_ticks += 1000u * FM1_TICKS_PER_US;
    }
    jvo_have = CTL;
    jvo_pos = 0;
}

static void jvo_render(uint32_t n, float *l, float *r)
{
    while (n) {
        uint32_t m;
        if (jvo_pos >= jvo_have)
            jvo_block();
        m = jvo_have - jvo_pos < n ? jvo_have - jvo_pos : n;
        memcpy(l, jvo_l + jvo_pos, m * sizeof *l);
        memcpy(r, jvo_r + jvo_pos, m * sizeof *r);
        jvo_pos += m;
        l += m;
        r += m;
        n -= m;
    }
}

static void jvo_note_on(uint32_t note, uint32_t vel)
{
    trk_note_on(JVO_T, note & 127u, vel < 1u ? 1u : vel > 127u ? 127u : vel);
}
static void jvo_note_off(uint32_t note) { trk_note_off(JVO_T, note & 127u); }
static void jvo_all_off(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (JVO_T->v[i].gate)
            trk_note_off(JVO_T, JVO_T->v[i].note);
}

static const param_desc_t *jvo_desc(uint32_t j) { return track_desc(JVO_T, JVO_ID[j < JVO_NP ? j : 0]); }
static void jvo_set(uint32_t j, int32_t v)
{
    if (j < JVO_NP)
        JVO_T->p[JVO_ID[j]] = (int16_t)v;
}
static int32_t jvo_getp(uint32_t j) { return j < JVO_NP ? JVO_T->p[JVO_ID[j]] : 0; }
static const char *jvo_label(uint32_t j) { return jvo_desc(j)->label; }
static int32_t jvo_min(uint32_t j) { return jvo_desc(j)->min; }
static int32_t jvo_max(uint32_t j) { return jvo_desc(j)->max; }
static void jvo_format(uint32_t j, int32_t v, char *out, uint32_t n)
{
    char b[24];
    const char *u = "";
    uint32_t i, k;
    if (!n)
        return;
    param_format(jvo_desc(j), v, b, &u);
    for (i = 0; b[i] && i + 1u < n; i++)
        out[i] = b[i];
    for (k = 0; u && u[k] && i + 1u < n; k++)
        out[i++] = u[k];
    out[i] = 0;
}

static uint32_t jvo_npresets(void) { return ENG_FORMANT.npresets; }
static const char *jvo_preset_name(uint32_t i) { return ENG_FORMANT.presets[i % ENG_FORMANT.npresets].name; }
static void jvo_load_preset(uint32_t i)
{
    int16_t level = JVO_T->p[P_LEVEL];
    host_preset(JVO_T, ENGI_VOICE, i % ENG_FORMANT.npresets);
    /* (LANG and LYRIC: the preset's words, hostsim host_preset_values: LANG OFF for a vowel sound) */
    JVO_T->p[P_LEVEL] = level;                   /* the module's LEVEL stays */
}

static void jvo_set_text(const char *s) { lyr_set(0, s); }
static void jvo_restart(void) { sing[0].rst = 1; }
static void jvo_word(char *out, uint32_t n, uint32_t *syl, uint32_t *nsyl)
{
    const sing_t *s = sing_view(JVO_T);
    uint32_t i;
    if (!n)
        return;
    for (i = 0; JVO_T->p[P_VLANG] && s->w[i] && i < sizeof s->w && i + 1u < n; i++)
        out[i] = s->w[i];
    out[i] = 0;
    if (syl)
        *syl = s->wsy;
    if (nsyl)
        *nsyl = s->wns;
}

static const uint16_t *jvo_vis(uint32_t w, uint32_t h)
{
    if (w > JVO_VIS_W)
        w = JVO_VIS_W;
    if (h > JVO_VIS_H)
        h = JVO_VIS_H;
    ui.frame++;
    cv_begin(w, h, T_BG);
    vx_draw(JVO_T, 0, 0, (int32_t)w, (int32_t)h, -1);
    return cv_px;
}
static uint32_t jvo_active(void)
{
    uint32_t i, a = 0;
    for (i = 0; i < NVOICE; i++)
        a += JVO_T->v[i].active ? 1u : 0u;
    return a;
}
static uint32_t jvo_sample_rate(void) { return FS; }

static const jvo_api jvo_table = {
    JVO_API_VERSION,
    jvo_sample_rate,
    jvo_init,
    jvo_render,
    jvo_note_on, jvo_note_off, jvo_all_off,
    jvo_set, jvo_getp, jvo_label, jvo_min, jvo_max, jvo_format,
    jvo_npresets, jvo_preset_name, jvo_load_preset,
    jvo_set_text, jvo_restart, jvo_word,
    jvo_vis,
    jvo_active,
};

JVO_EXPORT const jvo_api *jvo_get(void) { return &jvo_table; }
