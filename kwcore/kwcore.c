/* SPDX-License-Identifier: GPL-3.0-only
 * K-WASUB for VCV Rack: the core. The firmware (kwasub/, unchanged) compiled as for its browser emulator
 * (KAWA_WASM: web/wasm_host.c stands in for the FM-1), with the few calls the module needs on top. */
#define KAWA_WASM 1
#ifndef APP_VER
#define APP_VER "1.0.52"
#endif
#include "../kwasub/firmware/src/ksub.c"
#include "kwcore.h"

#if defined(_WIN32)
#define KW_EXPORT __declspec(dllexport)
#else
#define KW_EXPORT __attribute__((visibility("default")))
#endif

static uint32_t k_rate(void) { return FS; }
static void k_init(void)
{
    ksub_init();
    ksub_master(4096);                          /* unity: the module has its own level */
}
static const float *k_render(void)
{
    const float *f = ksub_audio();
    ksub_step();
    return f;
}
static void k_midi(uint32_t st, uint32_t d1, uint32_t d2) { ksub_midi(st, d1, d2); }
static uint32_t k_np(void) { return NP; }
static int32_t k_get(uint32_t id) { return id < NP ? patch[id] : 0; }
static void k_set(uint32_t id, int32_t v)
{
    if (id >= NP) return;
    patch[id] = (int8_t)clampi(v, PD[id].lo, PD[id].hi);
}
static int32_t k_lo(uint32_t id) { return id < NP ? PD[id].lo : 0; }
static int32_t k_hi(uint32_t id) { return id < NP ? PD[id].hi : 0; }
static const char *k_name(uint32_t id) { return id < NP ? PD[id].name : ""; }
static const char *k_sname(uint32_t id) { return id < NP ? item_sname(id) : ""; }
static void k_text(uint32_t id, char *out, uint32_t n)
{
    char b[96];
    uint32_t i;
    if (!n) return;
    b[0] = 0;
    if (id < NP) item_text(id, b);
    for (i = 0; i + 1u < n && b[i]; i++) out[i] = b[i];
    out[i] = 0;
}
static uint32_t k_npresets(void) { return (uint32_t)NFACTORY; }
static const char *k_preset_name(uint32_t i) { return FACTORY[i % NFACTORY].name; }
static void k_load_preset(uint32_t i) { load_preset(0, i % NFACTORY); }
static const char *k_preset_style(uint32_t i)
{
    /* the preset's list is (parameter, value) pairs up to -1; no STYLE in it: the first style */
    const int8_t *p = FACTORY[i % NFACTORY].ov;
    int st = 0;
    for (; p[0] >= 0; p += 2)
        if (p[0] == P_STYLE) st = p[1];
    return L_STYLE[clampi(st, 0, NSTYLE - 1)];
}
static int32_t k_eq_get(uint32_t band, uint32_t what)
{
    band &= 3u;
    switch (what) {
    case KW_EQ_TYPE: return perf.eq_t[band];
    case KW_EQ_FREQ: return perf.eq_f[band];
    case KW_EQ_GAIN: return perf.eq_g[band];
    default: return perf.eq_q[band];
    }
}
static void k_eq_set(uint32_t band, uint32_t what, int32_t v)
{
    band &= 3u;
    switch (what) {
    case KW_EQ_TYPE: perf.eq_t[band] = (uint8_t)clampi(v, 0, NEQT - 1); break;
    case KW_EQ_FREQ: perf.eq_f[band] = (uint8_t)clampi(v, 0, 47); break;
    case KW_EQ_GAIN: perf.eq_g[band] = (int8_t)clampi(v, -12, 12); break;
    default: perf.eq_q[band] = (uint8_t)clampi(v, 0, 7); break;
    }
    eq_dirty = 1;
}
static void k_set_bpm10(uint32_t b) { ks_bpm10 = b < 300u ? 300u : b > 3000u ? 3000u : b; }
static uint32_t k_bpm10(void) { return ks_bpm10; }
static void k_sync(void) { ev_push(EV_SYNC, 0, 0); }
static float k_env(void) { return (float)env_lv_show[0] * (1.0f / 32768.0f); }
static float k_lfo(void) { return (float)lfo[0].out * (1.0f / 32768.0f); }
static uint32_t k_voices(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++) n += vo[k].on ? 1u : 0u;
    return n;
}
static float k_lfo_phase(void) { return (float)(lfo_ph_show[0] >> 8) * (1.0f / 16777216.0f); }

static const kw_api API = {
    KW_API_VERSION, k_rate, k_init, k_render, k_midi, k_np, k_get, k_set, k_lo, k_hi, k_name, k_sname, k_text,
    k_npresets, k_preset_name, k_load_preset, k_eq_get, k_eq_set, k_set_bpm10, k_bpm10, k_sync, k_env, k_lfo,
    k_voices, k_lfo_phase, k_preset_style,
};

KW_EXPORT const kw_api *kw_get(void) { return &API; }
