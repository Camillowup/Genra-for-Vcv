/* SPDX-License-Identifier: GPL-3.0-only
 * GOLANA for VCV Rack: the C interface of the Golana core (glcore.c).
 * The Golana firmware (Gadlele, based on Felucca, GPL-3.0) as its host renders run it (tests/golana_sound.c): part 1
 * plays the GOLANA engine with its voices, arp and effects at 44.1 kHz. Each module instance loads a private copy
 * of the library (the firmware's state is static); it exports one symbol, gl_get. */
#ifndef GLCORE_H
#define GLCORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define GL_API_VERSION 1u
enum { GL_FRAMES = 128 };                      /* frames per render call (4 of the firmware's control blocks) */

/* the values a knob may move that are no part parameter (gl_api.vget / vset) */
enum { GLV_ARPRES, GLV_DCOLOR, GLV_DMIX, GLV_BPM, GLV_N };

/* what the module's knobs move: gl_api.id gives the firmware's parameter id (core.h GL_* / P_*) */
enum {
    GLK_O1WAVE, GLK_O1C1, GLK_O1C2, GLK_DRIFT, GLK_O2WAVE, GLK_O2MOD, GLK_O2SEMI, GLK_O2TUNE,
    GLK_MIX1, GLK_MIX2, GLK_NOISE, GLK_SUBTYPE, GLK_SUB, GLK_FOLD,
    GLK_FTYPE, GLK_CUTOFF, GLK_RESO, GLK_EGINT, GLK_KTRK, GLK_DIST, GLK_EGRESET,
    GLK_FA, GLK_FD, GLK_FS, GLK_FR, GLK_ATK, GLK_DEC, GLK_SUS, GLK_REL,
    GLK_L1WAVE, GLK_L1KEY, GLK_L1SYNC, GLK_L1FREQ, GLK_L2WAVE, GLK_L2KEY, GLK_L2SYNC, GLK_L2FREQ,
    GLK_VOICE, GLK_DETUNE, GLK_GLIDE,
    GLK_P1SRC, GLK_P1DST, GLK_P1INT, GLK_P2SRC, GLK_P2DST, GLK_P2INT,
    GLK_P3SRC, GLK_P3DST, GLK_P3INT, GLK_P4SRC, GLK_P4DST, GLK_P4INT,
    GLK_ARPON, GLK_ARPTYPE, GLK_AOCT, GLK_ASWING, GLK_AGATE, GLK_AHOLD, GLK_ARPKSYNC,
    GLK_IMIX, GLK_DLY, GLK_REV, GLK_SPRD, GLK_LEVEL,
    GLK_MFX, GLK_IA, GLK_IB, GLK_DTYPE, GLK_DTIME, GLK_DFDBK, GLK_RTYPE, GLK_RSIZE, GLK_RDAMP, GLK_ROUTE,
    GLK_N
};

typedef struct gl_api {
    uint32_t api;                              /* GL_API_VERSION */
    uint32_t (*sample_rate)(void);             /* 44100 */
    void (*init)(void);
    /* GL_FRAMES frames, L R L R, -1..1 at the firmware's full level */
    const float *(*render)(void);
    void (*midi)(uint32_t st, uint32_t d1, uint32_t d2);  /* note on / off, CC 1, pressure, bend (channel 1) */
    void (*all_off)(void);
    /* part 1's parameters (core.h P_* / GL_*), as the firmware stores them */
    uint32_t (*np)(void);
    uint32_t (*id)(uint32_t glk);                      /* GLK_* -> the parameter id */
    int32_t (*get)(uint32_t id);
    void (*set)(uint32_t id, int32_t v);
    int32_t (*lo)(uint32_t id);
    int32_t (*hi)(uint32_t id);
    void (*text)(uint32_t id, char *out, uint32_t n);    /* the value as the firmware writes it */
    /* GLV_* */
    int32_t (*vget)(uint32_t k);
    void (*vset)(uint32_t k, int32_t v);
    /* the presets: GENRA's own (by style) and Golana's 25 factory sounds */
    uint32_t (*npresets)(void);
    const char *(*preset_name)(uint32_t i);
    const char *(*preset_style)(uint32_t i);
    void (*load_preset)(uint32_t i);
    /* LFO n (0, 1) from the start of its cycle, every voice's; sync: the arp and the LFOs to the bar's start */
    void (*retrig)(uint32_t n);
    void (*sync)(void);
    /* what plays: voices sounding, a voice's note (-1: none) */
    uint32_t (*voices)(void);
    int32_t (*voice_note)(uint32_t v);
} gl_api;

typedef const gl_api *(*gl_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
