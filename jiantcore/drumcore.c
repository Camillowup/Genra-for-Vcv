/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT DRUM-X for VCV Rack: the DRUM-X core. The firmware's sources as its browser build compiles them
 * (web/emu/felucca_web.c), used the way its host renders use them (tests/hostsim.c: one part, mix_block), without the
 * device's main loop: part 1 plays the DRUM engine, hits go to it as notes on its GM map (voice.c trk_note_on), the
 * kit's sounds are the firmware's dx_kit, and the kit's creatures are drawn by its own code (ui_engvis.c vx_kit).
 * Nothing of the firmware changes for this file. */
#include "../jiant/web/emu/felucca_web.c"
#include "drumcore.h"

#if defined(_WIN32)
#define JDX_EXPORT __declspec(dllexport)
#else
#define JDX_EXPORT __attribute__((visibility("default")))
#endif

#define JDX_T (&trk[0])

static uint32_t jdx_ms_due;
static float jdx_l[CTL], jdx_r[CTL];
static uint32_t jdx_have, jdx_pos;
static int32_t jdx_solo = -1;                    /* isolate(): the lane alone, or -1 */
static int16_t jdx_want[JDX_NP];                 /* what the module set (the lane levels and sends as asked) */

/* a lane at its own pitch on the GM map (eng_drum.c DRUM_GM, offset 0) */
static const uint8_t JDX_NOTE[JDX_NLANE] = {36, 38, 39, 42, 46, 45, 37, 49};
static const uint8_t JDX_ID[JDX_NP] = {P_E0, P_E0 + 1, P_E0 + 2, P_E0 + 3, P_E0 + 4, P_E0 + 5, P_E0 + 6, P_E0 + 7,
                                       P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7,
                                       P_DIST, P_DLY, P_REV, P_LEVEL};

/* a parameter into the part: an isolated core keeps every other lane at 0 and its effects off */
static void jdx_apply(uint32_t j)
{
    int32_t v = jdx_want[j];
    if (jdx_solo >= 0) {
        if (j >= JDX_LN0 && j <= JDX_LN7 && (int32_t)(j - JDX_LN0) != jdx_solo)
            v = 0;
        if (j == JDX_DIST || j == JDX_DLY || j == JDX_REV)
            v = 0;
    }
    JDX_T->p[JDX_ID[j]] = (int16_t)v;
}

static void jdx_init(uint32_t seed)
{
    uint32_t j;
    memset(nor, 0xFF, sizeof nor);               /* a fresh device's flash: the settings' defaults */
    persist_boot();
    settings_init();
    prefs_update();
    host_tracks_init();
    trk_live = 1;
    session_dice(seed ? seed : 1u);
    dx_kit_load(0);
    host_preset(JDX_T, ENGI_DRUM, 0);
    for (j = 0; j < JDX_NP; j++)
        jdx_want[j] = JDX_T->p[JDX_ID[j]];
    jdx_have = jdx_pos = 0;
}

static void jdx_block(void)
{
    int32_t o[2 * CTL];
    uint32_t i;
    mix_block(o, CTL);
    for (i = 0; i < CTL; i++) {
        int32_t l = o[2u * i], r = o[2u * i + 1u];
        l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
        r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
        jdx_l[i] = (float)l / 32768.0f;
        jdx_r[i] = (float)r / 32768.0f;
        scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)l;
    }
    jdx_ms_due += CTL * 1000u;
    while (jdx_ms_due >= FS) {
        jdx_ms_due -= FS;
        fm1_ms++;
        web_ticks += 1000u * FM1_TICKS_PER_US;
    }
    jdx_have = CTL;
    jdx_pos = 0;
}

static void jdx_render(uint32_t n, float *l, float *r)
{
    while (n) {
        uint32_t m;
        if (jdx_pos >= jdx_have)
            jdx_block();
        m = jdx_have - jdx_pos < n ? jdx_have - jdx_pos : n;
        memcpy(l, jdx_l + jdx_pos, m * sizeof *l);
        memcpy(r, jdx_r + jdx_pos, m * sizeof *r);
        jdx_pos += m;
        l += m;
        r += m;
        n -= m;
    }
}

static uint32_t jdx_vel(uint32_t v) { return v < 1u ? 1u : v > 127u ? 127u : v; }
static void jdx_note_on(uint32_t note, uint32_t vel)
{
    trk_note_on(JDX_T, note & 127u, jdx_vel(vel));
    trk_note_off(JDX_T, note & 127u);            /* (a drum: the hit is the envelope, a release ends nothing) */
}
static void jdx_hit(uint32_t lane, uint32_t vel) { jdx_note_on(JDX_NOTE[lane & 7u], vel); }

static void jdx_set(uint32_t j, int32_t v)
{
    if (j >= JDX_NP)
        return;
    jdx_want[j] = (int16_t)v;
    jdx_apply(j);
}
static int32_t jdx_getp(uint32_t j) { return j < JDX_NP ? jdx_want[j] : 0; }
static void jdx_format(uint32_t j, int32_t v, char *out, uint32_t n)
{
    char b[24];
    const char *u = "";
    uint32_t i, k;
    if (!n)
        return;
    param_format(track_desc(JDX_T, JDX_ID[j < JDX_NP ? j : 0]), v, b, &u);
    for (i = 0; b[i] && i + 1u < n; i++)
        out[i] = b[i];
    for (k = 0; u && u[k] && i + 1u < n; k++)
        out[i++] = u[k];
    out[i] = 0;
}

static int32_t jdx_snd_get(uint32_t lane, uint32_t patch, uint32_t k)
{
    const dx_lane_t *L = &dx_kit[lane & 7u];
    k = k < DXP_N ? k : 0u;
    return patch ? L->b[k] : L->a[k];
}
static void jdx_snd_set(uint32_t lane, uint32_t patch, uint32_t k, int32_t v)
{
    dx_lane_t *L = &dx_kit[lane & 7u];
    uint8_t u = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    if (k >= DXP_N)
        return;
    if (patch)
        L->b[k] = u;
    else
        L->a[k] = u;
}
static void jdx_snd_format(uint32_t k, int32_t v, char *out, uint32_t n)
{
    (void)k;
    if (n)
        snprintf(out, n, "%d", (int)v);
}

static uint32_t jdx_nkits(void) { return ENG_DRUM.npresets; }
static const char *jdx_kit_name(uint32_t i) { return ENG_DRUM.presets[i % ENG_DRUM.npresets].name; }
static void jdx_load_kit(uint32_t i)
{
    uint32_t j;
    int16_t level = jdx_want[JDX_LEVEL];
    i %= ENG_DRUM.npresets;
    dx_kit_load(i);
    host_preset(JDX_T, ENGI_DRUM, i);
    for (j = 0; j < JDX_NP; j++)
        jdx_want[j] = JDX_T->p[JDX_ID[j]];
    jdx_want[JDX_LEVEL] = level;                 /* the module's LEVEL stays */
    for (j = 0; j < JDX_NP; j++)
        jdx_apply(j);
}

static uint32_t jdx_kit_size(void) { return (uint32_t)(sizeof dx_kit + sizeof dx_lay + sizeof dx_snd); }
static void jdx_kit_export(uint8_t *out)
{
    memcpy(out, dx_kit, sizeof dx_kit);
    memcpy(out + sizeof dx_kit, dx_lay, sizeof dx_lay);
    memcpy(out + sizeof dx_kit + sizeof dx_lay, dx_snd, sizeof dx_snd);
}
static void jdx_kit_import(const uint8_t *in)
{
    memcpy(dx_kit, in, sizeof dx_kit);
    memcpy(dx_lay, in + sizeof dx_kit, sizeof dx_lay);
    memcpy(dx_snd, in + sizeof dx_kit + sizeof dx_lay, sizeof dx_snd);
}

static void jdx_isolate(int32_t lane)
{
    uint32_t j;
    jdx_solo = lane >= 0 && lane < JDX_NLANE ? lane : -1;
    for (j = 0; j < JDX_NP; j++)
        jdx_apply(j);
}

static float jdx_lane_env(uint32_t lane)
{
    const drum_lane_t *L = &drum_kit[0][lane & 7u];
    if (!L->x.live || !L->owner)
        return 0.0f;
    return (float)L->x.ea / 1073741824.0f;
}

static const uint16_t *jdx_vis(uint32_t w, uint32_t h, int32_t lane)
{
    int32_t s, cx, cy;
    if (w > JDX_VIS_W)
        w = JDX_VIS_W;
    if (h > JDX_VIS_H)
        h = JDX_VIS_H;
    ui.frame++;
    cv_begin(w, h, T_BG);
    /* as vx_draw places the kit (~128 x 92 units in the rectangle), with the sound edited lit */
    s = vx_fit((int32_t)w, (int32_t)h, 128, 92);
    cx = (int32_t)w * 8;
    cy = (int32_t)h * 8;
    vx_kit(JDX_T, cx - (s >> 4), cy + (48 * s >> 8), s, -1, lane >= 0 && lane < JDX_NLANE ? lane : -1);
    return cv_px;
}
static uint32_t jdx_sample_rate(void) { return FS; }

static const jdx_api jdx_table = {
    JDX_API_VERSION,
    jdx_sample_rate,
    jdx_init,
    jdx_render,
    jdx_hit, jdx_note_on,
    jdx_set, jdx_getp, jdx_format,
    jdx_snd_get, jdx_snd_set, jdx_snd_format,
    jdx_nkits, jdx_kit_name, jdx_load_kit,
    jdx_kit_size, jdx_kit_export, jdx_kit_import,
    jdx_isolate,
    jdx_lane_env,
    jdx_vis,
};

JDX_EXPORT const jdx_api *jdx_get(void) { return &jdx_table; }
