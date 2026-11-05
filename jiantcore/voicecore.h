/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT VOICE for VCV Rack: the C interface of the VOICE core (core/voicecore.c).
 * The JIANT firmware's VOICE engine (a Klatt cascade formant singer: vowels, and words in Spanish, English and Japanese)
 * on one part, with its effects, and its vocal tract (ui_engvis.c vx_voice) drawn on a canvas. Each module instance
 * loads a private copy of the library (the firmware's state is static); it exports one symbol, jvo_get. */
#ifndef VOICECORE_H
#define VOICECORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define JVO_API_VERSION 1u
enum { JVO_VIS_W = 232, JVO_VIS_H = 150, JVO_TEXT_MAX = 63 };

/* the part's parameters the module uses */
enum {
    JVO_E1, JVO_E2, JVO_E3, JVO_E4, JVO_E5, JVO_E6, JVO_E7, JVO_E8,   /* the engine's eight (named by LANG) */
    JVO_ATK, JVO_DEC, JVO_SUS, JVO_REL,
    JVO_DIST, JVO_CHOR, JVO_DLY, JVO_REV,
    JVO_LEVEL, JVO_VOICE, JVO_LANG, JVO_LYRIC,
    JVO_NP
};

typedef struct jvo_api {
    uint32_t api;                                    /* JVO_API_VERSION */
    uint32_t (*sample_rate)(void);                   /* 44100 */
    void (*init)(uint32_t seed);
    void (*render)(uint32_t n, float *l, float *r);  /* n frames, -1..1 */
    void (*note_on)(uint32_t note, uint32_t vel);
    void (*note_off)(uint32_t note);
    void (*all_off)(void);
    void (*set)(uint32_t j, int32_t v);
    int32_t (*get)(uint32_t j);
    const char *(*label)(uint32_t j);
    int32_t (*min)(uint32_t j);
    int32_t (*max)(uint32_t j);
    void (*format)(uint32_t j, int32_t v, char *out, uint32_t n);   /* as the FM-1 writes it (LANG, LYRIC: names) */
    uint32_t (*npresets)(void);
    const char *(*preset_name)(uint32_t i);
    void (*load_preset)(uint32_t i);                 /* the sound, its envelope, sends, voice mode, LANG and LYRIC */
    /* the lyric (LYRIC = TEXT): up to JVO_TEXT_MAX ASCII characters; restart: the lyric from its start */
    void (*set_text)(const char *s);
    void (*restart)(void);
    /* what it sings now (LANG on): the word, its syllables, the one sounding (1..); "" when none */
    void (*word)(char *out, uint32_t n, uint32_t *syl, uint32_t *nsyl);
    /* the vocal tract: w x h RGB565, byte-swapped (the FM-1 panel's order) */
    const uint16_t *(*vis)(uint32_t w, uint32_t h);
    uint32_t (*active_voices)(void);
} jvo_api;

typedef const jvo_api *(*jvo_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
