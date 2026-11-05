/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT DRUM-X for VCV Rack: the C interface of the DRUM-X core (core/drumcore.c).
 * The JIANT firmware's DRUM engine (DRUM-X: 8 lanes, each a synthesized sound with patches A and B and a MORPH) on
 * one part, with its effects, and its kit of creatures (ui_engvis.c vx_kit) drawn on a canvas. Each module instance
 * loads private copies of the library (the firmware's state is static): one plays the mix, one per patched lane
 * output plays that lane alone. It exports one symbol, jdx_get. */
#ifndef DRUMCORE_H
#define DRUMCORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define JDX_API_VERSION 1u
enum { JDX_VIS_W = 232, JDX_VIS_H = 150, JDX_NLANE = 8, JDX_NSND = 5 };

/* the part's parameters the module uses (pid: the firmware's id) */
enum {
    JDX_MORPH, JDX_TUNE, JDX_TONE, JDX_DECAY, JDX_NOISE, JDX_FMENV, JDX_FOLD, JDX_DRIVE,   /* the kit's (E1..E8) */
    JDX_LN0, JDX_LN7 = JDX_LN0 + 7,                                                       /* the lanes' levels */
    JDX_DIST, JDX_DLY, JDX_REV, JDX_LEVEL,
    JDX_NP
};
/* a lane's sound, patch A or B: PITCH PMOD DECAY NOISE COLOR, 0..127 */
enum { JDX_PITCH, JDX_PMOD, JDX_SDECAY, JDX_SNOISE, JDX_COLOR };

typedef struct jdx_api {
    uint32_t api;                                    /* JDX_API_VERSION */
    uint32_t (*sample_rate)(void);                   /* 44100 */
    void (*init)(uint32_t seed);
    void (*render)(uint32_t n, float *l, float *r);  /* n frames, -1..1 */
    void (*hit)(uint32_t lane, uint32_t vel);        /* a lane struck at its own pitch, velocity 1..127 */
    void (*note_on)(uint32_t note, uint32_t vel);    /* a MIDI note on the GM drum map (36 the kick) */
    void (*set)(uint32_t j, int32_t v);
    int32_t (*get)(uint32_t j);
    void (*format)(uint32_t j, int32_t v, char *out, uint32_t n);
    int32_t (*snd_get)(uint32_t lane, uint32_t patch, uint32_t k);       /* patch 0 A, 1 B */
    void (*snd_set)(uint32_t lane, uint32_t patch, uint32_t k, int32_t v);
    void (*snd_format)(uint32_t k, int32_t v, char *out, uint32_t n);
    uint32_t (*nkits)(void);
    const char *(*kit_name)(uint32_t i);
    void (*load_kit)(uint32_t i);                    /* the factory kit: its sounds, the macros, the sends */
    /* the kit's sounds as bytes (to keep with the patch) */
    uint32_t (*kit_size)(void);
    void (*kit_export)(uint8_t *out);
    void (*kit_import)(const uint8_t *in);
    /* one lane alone (0..7), every other lane silent and no effects; -1: the whole kit */
    void (*isolate)(int32_t lane);
    float (*lane_env)(uint32_t lane);                /* a lane's hit now, 0..1 (its pad's light) */
    /* the kit: w x h RGB565, byte-swapped (the FM-1 panel's order); lane: the sound edited, lit (-1 none) */
    const uint16_t *(*vis)(uint32_t w, uint32_t h, int32_t lane);
} jdx_api;

typedef const jdx_api *(*jdx_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
