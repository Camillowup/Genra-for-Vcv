/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT ANIMAL for VCV Rack: the C interface of the ANIMAL core (core/animalcore.c).
 * The JIANT firmware's ANIMAL engine on one part (its voices, envelope, filter and effects as the FM-1 renders them),
 * and its creature (ui_engvis.c vx_animal) drawn on a canvas. No device UI, no sequencer. Each module instance loads
 * a private copy of the library (the firmware's state is static); it exports one symbol, jan_get. */
#ifndef ANIMALCORE_H
#define ANIMALCORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define JAN_API_VERSION 2u
enum { JAN_VIS_W = 232, JAN_VIS_H = 150 };

/* the part's parameters the module uses, by its own index (pid() gives the firmware's id) */
enum {
    JAN_MODE, JAN_E1, JAN_E2, JAN_E3, JAN_E4, JAN_E5, JAN_E6, JAN_E7,   /* the engine's eight (E0 = MODE) */
    JAN_ATK, JAN_DEC, JAN_SUS, JAN_REL,
    JAN_DIST, JAN_CHOR, JAN_DLY, JAN_REV,
    JAN_LEVEL, JAN_VOICE,
    JAN_NP
};

typedef struct jan_api {
    uint32_t api;                                    /* JAN_API_VERSION */
    uint32_t (*sample_rate)(void);                   /* the engine's rate (44100) */
    void (*init)(uint32_t seed);
    void (*render)(uint32_t n, float *l, float *r);  /* n frames, -1..1 */
    void (*note_on)(uint32_t note, uint32_t vel);    /* MIDI note 0..127, velocity 1..127 */
    void (*note_off)(uint32_t note);
    void (*all_off)(void);
    /* parameter j (JAN_*): set, get, its label, range and value as the FM-1's screen writes them (the engine's eight
     * as the current MODE names them) */
    void (*set)(uint32_t j, int32_t value);
    int32_t (*get)(uint32_t j);
    const char *(*label)(uint32_t j);
    int32_t (*min)(uint32_t j);
    int32_t (*max)(uint32_t j);
    void (*format)(uint32_t j, int32_t v, char *out, uint32_t n);
    uint32_t (*npresets)(void);
    const char *(*preset_name)(uint32_t i);
    void (*load_preset)(uint32_t i);                 /* the factory sound: E1..E8, envelope, FX sends */
    /* the creature: w x h (<= JAN_VIS_W x JAN_VIS_H) RGB565, byte-swapped (the FM-1 panel's order) */
    const uint16_t *(*vis)(uint32_t w, uint32_t h);
    uint32_t (*active_voices)(void);
} jan_api;

typedef const jan_api *(*jan_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
