/* SPDX-License-Identifier: GPL-3.0-only
 * GOLANA for VCV Rack: the core. The Golana firmware's sources (golana/, unchanged) compiled as its host renders build
 * them (tests/hostsim.c, tests/golana_sound.c): the engines, the voices, the sequencer's arp and MIDI, the effects,
 * without the device's screen, keys and main loop. Part 1 plays the GOLANA engine; notes come in as MIDI (seq.c: the
 * arp hears them), the module's knobs write part 1's parameters, and gl_sync derives what the firmware reads from
 * them, as Golana's own interface does after an edit. Nothing of the firmware changes for this file. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define __attribute__(x)
#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "felucca_tables.h"
#include "../golana/firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;
#include "../golana/firmware/src/core.h"
#include "../golana/firmware/src/engines.c"
#include "../golana/firmware/src/params.c"
#include "../golana/firmware/src/mod.c"
#include "../golana/firmware/src/voice.c"
#include "../golana/firmware/src/slicer.c"
#include "../golana/firmware/src/fx.c"
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#include "../golana/firmware/src/usb.c"
#include "../golana/firmware/src/midi_uart.c"
#include "../golana/firmware/src/song_chain.c"
#include "../golana/firmware/src/seq.c"
#undef __attribute__
#include "glcore.h"
#include "golana_genra.c"            /* GENRA's own presets, by style (in the format of golana_factory.c) */

#if defined(_WIN32)
#define GL_EXPORT __declspec(dllexport)
#else
#define GL_EXPORT __attribute__((visibility("default")))
#endif

#define GT (&trk[0])

static float gl_out[2 * GL_FRAMES];
static int gl_dirty;                         /* a parameter changed: gl_sync before the next block */
static int32_t gl_dcolor = 70, gl_dmix = 110;   /* the delay's colour and level (Golana: fixed by gl_sync) */
static uint32_t gl_ms_due;
/* N_GRES (golana_ui.c: the arp's RES) -> P_ARATE */
static const uint8_t GL_RES_DIV[6] = {3, 5, 2, 4, 1, 0};

static void gl_after_sync(void)
{
    song.g[G_DCOLOR] = (int16_t)gl_dcolor;
    song.g[G_DMIX] = (int16_t)gl_dmix;
}

/* hostsim.c's host_tracks_init (as felucca_init: defaults, empty patterns) */
static void host_tracks_init(void)
{
    uint32_t i, k;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0; i++)
            trk[k].p[i] = TP[i].def;
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].time = ST_REST;
    }
}

static void gl_reset_part(void)
{
    int16_t v[P_COUNT];
    uint32_t k;
    host_tracks_init();
    for (k = 0; k < NTRK; k++) {             /* every part GOLANA at INIT (only part 1 plays) */
        gl_factory(0xFF, v, 0);
        gl_apply(&trk[k], v);
        trk[k].engine = trk[k].eng_req;
        eng_mem_claim(&trk[k]);
    }
    song.sel = 0;
    song.g[G_ROUTE] = 1;                     /* SEL: every MIDI channel plays part 1 */
    song.g[G_BPM] = 120;
    song.g[G_CLOCK] = 0;                     /* the internal clock at the module's tempo */
    song.master_q12 = 4096;
}

static uint32_t c_rate(void) { return FS; }
static void c_init(void)
{
    gl_reset_part();
    gl_dcolor = 70;
    gl_dmix = 110;
    gl_after_sync();
    gl_dirty = 0;
}

static const float *c_render(void)
{
    int32_t o[2 * CTL];
    uint32_t b, i;
    for (b = 0; b < GL_FRAMES / CTL; b++) {
        if (gl_dirty) {
            gl_dirty = 0;
            gl_sync(GT);
            gl_after_sync();
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            /* (golana_sound.c: >> 1 is Q15 at the master's full level) */
            gl_out[2u * (b * CTL + i)] = (float)(o[2u * i] >> 1) * (1.0f / 32768.0f);
            gl_out[2u * (b * CTL + i) + 1u] = (float)(o[2u * i + 1u] >> 1) * (1.0f / 32768.0f);
        }
        gl_ms_due += CTL * 1000u;            /* the firmware's millisecond clock (MIDI timestamps) */
        while (gl_ms_due >= FS) {
            gl_ms_due -= FS;
            fm1_ms++;
        }
    }
    return gl_out;
}

static void c_midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    uint32_t s = st & 0xF0u;
    midi_enqueue((s >> 4) | s << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}
static void c_all_off(void)
{
    mi_r = mi_w;                             /* (notes still queued: dropped) */
    GT->nheld = 0;                           /* the arp's held keys */
    arp_forget(GT);
    trk_all_off(GT);
}

static uint32_t c_np(void) { return P_COUNT; }
static const uint8_t GLK_ID[GLK_N] = {
    GL_O1WAVE, GL_O1C1, GL_O1C2, GL_DRIFT, GL_O2WAVE, GL_O2MOD, GL_O2SEMI, GL_O2TUNE,
    GL_MIX1, GL_MIX2, GL_NOISE, GL_SUBTYPE, GL_SUB, GL_FOLD,
    GL_FTYPE, GL_CUTOFF, GL_RESO, GL_FEGINT, GL_KTRK, GL_DIST, GL_EGRESET,
    GL_FA, GL_FD, GL_FS, GL_FR, P_ATK, P_DEC, P_SUS, P_REL,
    GL_L1WAVE, GL_L1KEY, GL_L1SYNC, GL_L1FREQ, GL_L2WAVE, GL_L2KEY, GL_L2SYNC, GL_L2FREQ,
    P_VOICE, P_DETUNE, P_GLIDE,
    GL_P1SRC, GL_P1DST, GL_P1INT, GL_P1SRC + 3, GL_P1DST + 3, GL_P1INT + 3,
    GL_P1SRC + 6, GL_P1DST + 6, GL_P1INT + 6, GL_P1SRC + 9, GL_P1DST + 9, GL_P1INT + 9,
    GL_ARPON, GL_ARPTYPE, P_AOCT, P_ASWING, P_AGATE, P_AHOLD, GL_ARPKSYNC,
    P_IMIX, P_DLY, P_REV, P_SPRD, P_LEVEL,
    GL_MFX, P_IA, P_IB, GL_DTYPE, GL_DTIME, GL_DFDBK, GL_RTYPE, GL_RSIZE, GL_RDAMP, GL_ROUTE,
};
static uint32_t c_id(uint32_t k) { return k < GLK_N ? GLK_ID[k] : 0u; }
static int32_t c_get(uint32_t id) { return id < P_COUNT ? GT->p[id] : 0; }
static void c_set(uint32_t id, int32_t v)
{
    if (id >= P_COUNT)
        return;
    v = param_fit(param_desc_of(ENGI_GOLANA, id), v);
    if (GT->p[id] != v) {
        GT->p[id] = (int16_t)v;
        gl_dirty = 1;
    }
}
static int32_t c_lo(uint32_t id) { return id < P_COUNT ? param_desc_of(ENGI_GOLANA, id)->min : 0; }
static int32_t c_hi(uint32_t id) { return id < P_COUNT ? param_desc_of(ENGI_GOLANA, id)->max : 0; }
static void c_text(uint32_t id, char *out, uint32_t n)
{
    char b[32];
    const char *u = "";
    uint32_t i, k;
    if (!n)
        return;
    b[0] = 0;
    if (id < P_COUNT)   /* Golana's own meaning of the slot; the MOD FX's values as its TYPE names them */
        param_format(id >= P_IA && id <= P_IC ? track_desc(GT, id) : param_desc_of(ENGI_GOLANA, id), GT->p[id], b, &u);
    for (i = 0; b[i] && i + 1u < n; i++)
        out[i] = b[i];
    for (k = 0; u && u[k] && i + 1u < n; k++)
        out[i++] = u[k];
    out[i] = 0;
}

static int32_t c_vget(uint32_t k)
{
    uint32_t i;
    switch (k) {
    case GLV_ARPRES:
        for (i = 0; i < 6u; i++)
            if (GL_RES_DIV[i] == (uint32_t)GT->p[P_ARATE])
                return (int32_t)i;
        return 2;
    case GLV_DCOLOR: return gl_dcolor;
    case GLV_DMIX: return gl_dmix;
    case GLV_BPM: return song.g[G_BPM];
    default: return 0;
    }
}
static void c_vset(uint32_t k, int32_t v)
{
    switch (k) {
    case GLV_ARPRES:
        GT->p[P_ARATE] = (int16_t)GL_RES_DIV[clamp(v, 0, 5)];
        break;
    case GLV_DCOLOR:
        gl_dcolor = clamp(v, 0, 127);
        gl_after_sync();
        break;
    case GLV_DMIX:
        gl_dmix = clamp(v, 0, 127);
        gl_after_sync();
        break;
    case GLV_BPM:
        song.g[G_BPM] = (int16_t)clamp(v, 40, 240);
        break;
    default:
        break;
    }
}

/* the presets: GENRA's first (golana_genra.c), then Golana's own 25 */
#define GL_NGENRA ((uint32_t)NELEM(GL_GENRA))
#define GL_NALL (GL_NGENRA + (uint32_t)NELEM(GL_FACTORY))
static uint32_t c_npresets(void) { return GL_NALL; }
static const char *c_preset_name(uint32_t i)
{
    i %= GL_NALL;
    return i < GL_NGENRA ? GL_GENRA[i].p.name : GL_FACTORY[i - GL_NGENRA].name;
}
static const char *c_preset_style(uint32_t i)
{
    i %= GL_NALL;
    return i < GL_NGENRA ? GL_GENRA[i].style : "GOLANA ORIGINALS";
}
static void c_load_preset(uint32_t i)
{
    int16_t v[P_COUNT];
    const gl_prog_t *g;
    uint32_t k;
    i %= GL_NALL;
    g = i < GL_NGENRA ? &GL_GENRA[i].p : &GL_FACTORY[i - GL_NGENRA];
    gl_defaults(v);
    for (k = 0; k < g->n; k++)
        if (g->set[k].id < P_COUNT)
            v[g->set[k].id] = (int16_t)param_fit(param_desc_of(ENGI_GOLANA, g->set[k].id), g->set[k].v);
    c_all_off();
    gl_apply(GT, v);
    GT->engine = GT->eng_req;
    gl_dcolor = 70;
    gl_dmix = 110;
    gl_after_sync();
    gl_dirty = 0;
}

static void c_retrig(uint32_t n)
{
    glp_t *g = gl_part(GT);
    uint32_t k;
    if (n == 0u) {
        g->free1 = g->tim1 = 0;
        for (k = 0; k < NVOICE; k++)
            g->v[k].l1 = 0;
    } else {
        g->free2 = g->tim2 = 0;
        for (k = 0; k < NVOICE; k++)
            g->v[k].l2 = 0;
    }
}
static void c_sync(void)
{
    GT->arp_pos = 0xFFFFFFF;                 /* (seq.c arp_step: the next block fires the first step) */
    GT->arp_st = 0;
    GT->arp_idx = 0;
    c_retrig(0);
    c_retrig(1);
}
static uint32_t c_voices(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < NVOICE; k++)
        n += GT->v[k].active ? 1u : 0u;
    return n;
}
static int32_t c_voice_note(uint32_t k)
{
    if (k >= NVOICE || !GT->v[k].active || !GT->v[k].gate)
        return -1;
    return GT->v[k].note;
}

static const gl_api API = {
    GL_API_VERSION, c_rate, c_init, c_render, c_midi, c_all_off, c_np, c_id, c_get, c_set, c_lo, c_hi, c_text, c_vget,
    c_vset, c_npresets, c_preset_name, c_preset_style, c_load_preset, c_retrig, c_sync, c_voices, c_voice_note,
};

GL_EXPORT const gl_api *gl_get(void) { return &API; }
