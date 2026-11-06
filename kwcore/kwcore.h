/* SPDX-License-Identifier: GPL-3.0-only
 * K-WASUB for VCV Rack: the C interface of the K-WASUB core (kwcore.c).
 * The K-WASUB firmware (Omnipotard, GPL-3.0) as it runs in its browser emulator: the voices, filter, envelopes, LFOs
 * and the effects rack at 44.1 kHz. Each module instance loads a private copy of the library (the firmware's state
 * is static); it exports one symbol, kw_get. */
#ifndef KWCORE_H
#define KWCORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define KW_API_VERSION 1u
enum { KW_FRAMES = 256 };                           /* frames per render call */
enum { KW_EQ_TYPE, KW_EQ_FREQ, KW_EQ_GAIN, KW_EQ_Q }; /* eq_get / eq_set */

typedef struct kw_api {
    uint32_t api;                                     /* KW_API_VERSION */
    uint32_t (*sample_rate)(void);                    /* 44100 */
    void (*init)(void);
    /* KW_FRAMES frames, L R L R, -1..1; also one pass of the firmware's main loop (MIDI in, the EQ, the LFO's view) */
    const float *(*render)(void);
    void (*midi)(uint32_t st, uint32_t d1, uint32_t d2);
    /* the patch: the firmware's parameter ids (ksub_data.c), int8 values in [lo, hi] */
    uint32_t (*np)(void);
    int32_t (*get)(uint32_t id);
    void (*set)(uint32_t id, int32_t v);
    int32_t (*lo)(uint32_t id);
    int32_t (*hi)(uint32_t id);
    const char *(*name)(uint32_t id);                 /* the long name ("A LEVEL") */
    const char *(*sname)(uint32_t id);                /* the short one; an oscillator's P1..P4: what the engine calls it */
    void (*text)(uint32_t id, char *out, uint32_t n); /* the value as the FM-1 writes it */
    /* the 97 factory sounds */
    uint32_t (*npresets)(void);
    const char *(*preset_name)(uint32_t i);
    void (*load_preset)(uint32_t i);
    /* the EQ (4 bands): type (EQ_OFF .. EQ_HICUT), frequency (0..47), gain (-12..12 dB), width (0..7) */
    int32_t (*eq_get)(uint32_t band, uint32_t what);
    void (*eq_set)(uint32_t band, uint32_t what, int32_t v);
    /* the tempo (BPM x 10, 30..300 BPM) and the bar's start (the LFOs and the PUMP lock to it) */
    void (*set_bpm10)(uint32_t bpm10);
    uint32_t (*bpm10)(void);
    void (*sync)(void);
    /* what plays: the newest voice's amp envelope (0..1), LFO 1 (-1..1), voices sounding */
    float (*env)(void);
    float (*lfo)(void);
    uint32_t (*voices)(void);
    /* the LFO 1 cycle's position (0..1), for the display */
    float (*lfo_phase)(void);
} kw_api;

typedef const kw_api *(*kw_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
