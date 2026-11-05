/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT ANIMAL for VCV Rack: the ANIMAL core. The firmware's sources as its browser build compiles them
 * (web/emu/felucca_web.c), used the way its host renders use them (tests/hostsim.c: one part, mix_block), without the
 * device's main loop: part 1 plays the ANIMAL engine, notes go to it directly (voice.c trk_note_on / trk_note_off), and
 * the creature is drawn by the firmware's own code (ui_engvis.c vx_draw) on its canvas (gfx.c).
 * Nothing of the firmware changes for this file. */
#include "../jiant/web/emu/felucca_web.c"
#include "animalcore.h"

#if defined(_WIN32)
#define JAN_EXPORT __declspec(dllexport)
#else
#define JAN_EXPORT __attribute__((visibility("default")))
#endif

#define JAN_T (&trk[0])

static uint32_t jan_ms_due;
static float jan_l[CTL], jan_r[CTL];
static uint32_t jan_have, jan_pos;           /* frames of the last block not handed out yet */

static const uint8_t JAN_ID[JAN_NP] = {P_E0, P_E0 + 1, P_E0 + 2, P_E0 + 3, P_E0 + 4, P_E0 + 5, P_E0 + 6, P_E0 + 7,
                                       P_ATK, P_DEC, P_SUS, P_REL, P_DIST, P_CHOR, P_DLY, P_REV, P_LEVEL, P_VOICE};

static void jan_init(uint32_t seed)
{
    memset(nor, 0xFF, sizeof nor);               /* a fresh device's flash: the settings' defaults */
    persist_boot();
    settings_init();
    prefs_update();
    host_tracks_init();
    trk_live = 1;
    session_dice(seed ? seed : 1u);
    host_preset(JAN_T, ENGI_ANIMAL, 0);
    JAN_T->p[P_VOICE] = V_POLY;
    jan_have = jan_pos = 0;
}

/* one control block (CTL frames) of the part through the effects and the bus, as hostsim renders; the clock moves
 * with it (the creature's and the engine's time), the scope gets the left channel (audio.c, the creature's level) */
static void jan_block(void)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    for (i = 0; i < CTL; i++) {
        int32_t l = o[2u * i], r = o[2u * i + 1u];
        l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
        r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
        jan_l[i] = (float)l / 32768.0f;
        jan_r[i] = (float)r / 32768.0f;
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)l;
    }
    jan_ms_due += CTL * 1000u;
    while (jan_ms_due >= FS) {
        jan_ms_due -= FS;
        fm1_ms++;
        web_ticks += 1000u * FM1_TICKS_PER_US;
    }
    jan_have = CTL;
    jan_pos = 0;
}

static void jan_render(uint32_t n, float *l, float *r)
{
    while (n) {
        uint32_t m;
        if (jan_pos >= jan_have)
            jan_block();
        m = jan_have - jan_pos < n ? jan_have - jan_pos : n;
        memcpy(l, jan_l + jan_pos, m * sizeof *l);
        memcpy(r, jan_r + jan_pos, m * sizeof *r);
        jan_pos += m;
        l += m;
        r += m;
        n -= m;
    }
}

static void jan_note_on(uint32_t note, uint32_t vel)
{
    trk_note_on(JAN_T, note & 127u, vel < 1u ? 1u : vel > 127u ? 127u : vel);
}
static void jan_note_off(uint32_t note) { trk_note_off(JAN_T, note & 127u); }
static void jan_all_off(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (JAN_T->v[i].gate)
            trk_note_off(JAN_T, JAN_T->v[i].note);
}

static const param_desc_t *jan_desc(uint32_t j) { return track_desc(JAN_T, JAN_ID[j < JAN_NP ? j : 0]); }
static void jan_set(uint32_t j, int32_t v)
{
    if (j < JAN_NP)
        JAN_T->p[JAN_ID[j]] = (int16_t)v;
}
static int32_t jan_getp(uint32_t j) { return j < JAN_NP ? JAN_T->p[JAN_ID[j]] : 0; }
static const char *jan_label(uint32_t j) { return jan_desc(j)->label; }
static int32_t jan_min(uint32_t j) { return jan_desc(j)->min; }
static int32_t jan_max(uint32_t j) { return jan_desc(j)->max; }
static void jan_format(uint32_t j, int32_t v, char *out, uint32_t n)
{
    char b[24];
    const char *u = "";
    uint32_t i, k;
    if (!n)
        return;
    param_format(jan_desc(j), v, b, &u);
    for (i = 0; b[i] && i + 1u < n; i++)
        out[i] = b[i];
    for (k = 0; u && u[k] && i + 1u < n; k++)
        out[i++] = u[k];
    out[i] = 0;
}

static uint32_t jan_npresets(void) { return ENG_ANIMAL.npresets; }
static const char *jan_preset_name(uint32_t i) { return ENG_ANIMAL.presets[i % ENG_ANIMAL.npresets].name; }
static void jan_load_preset(uint32_t i)
{
    int16_t voice = JAN_T->p[P_VOICE], level = JAN_T->p[P_LEVEL];
    host_preset(JAN_T, ENGI_ANIMAL, i % ENG_ANIMAL.npresets);
    JAN_T->p[P_VOICE] = voice;                       /* the module's VOICE and LEVEL stay */
    JAN_T->p[P_LEVEL] = level;
}

static const uint16_t *jan_vis(uint32_t w, uint32_t h)
{
    if (w > JAN_VIS_W)
        w = JAN_VIS_W;
    if (h > JAN_VIS_H)
        h = JAN_VIS_H;
    ui.frame++;
    cv_begin(w, h, T_BG);
    vx_draw(JAN_T, 0, 0, (int32_t)w, (int32_t)h, -1);
    return cv_px;
}
static uint32_t jan_active(void)
{
    uint32_t i, a = 0;
    for (i = 0; i < NVOICE; i++)
        a += JAN_T->v[i].active ? 1u : 0u;
    return a;
}
static uint32_t jan_sample_rate(void) { return FS; }

static const jan_api jan_table = {
    JAN_API_VERSION,
    jan_sample_rate,
    jan_init,
    jan_render,
    jan_note_on, jan_note_off, jan_all_off,
    jan_set, jan_getp, jan_label, jan_min, jan_max, jan_format,
    jan_npresets, jan_preset_name, jan_load_preset,
    jan_vis,
    jan_active,
};

JAN_EXPORT const jan_api *jan_get(void) { return &jan_table; }
