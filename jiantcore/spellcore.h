/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT VOICE for VCV Rack: the C interface of the SPELL core (jiantcore/spellcore.c), VOICE's fifth language.
 * SPELLBOX's LPC speech chip (instruments/spellbox: 10th-order lattice, 8 kHz, an 8-bit DAC and a little speaker),
 * its words, letters and phonemes, and its text to speech (a 12 000-word dictionary, endings, letter-to-sound rules).
 * One voice. Each module instance loads a private copy of the library (the chip's state is static); it exports one
 * symbol, jsp_get. */
#ifndef SPELLCORE_H
#define SPELLCORE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define JSP_API_VERSION 1u

/* the chip's knobs (sb.h sp_param_t) */
enum {
    JSP_PITCH,    /* semitones -12..12 */
    JSP_SPEED,    /* 0..127, 64 normal */
    JSP_CLOCK,    /* 0..127, 64 = 8 kHz (4 .. 14 kHz) */
    JSP_BEND,     /* 0..127 corrupted coefficients */
    JSP_STUTTER,  /* 0..127 frames played again */
    JSP_MONO,     /* 0..127 the intonation flattened */
    JSP_CRUSH,    /* 0..5 bits off the DAC */
    JSP_TONE,     /* 0..127 the speaker */
    JSP_ARTIC,    /* 0..127 the unvoiced consonants' level */
    JSP_NP
};

typedef struct jsp_api {
    uint32_t api;                                  /* JSP_API_VERSION */
    uint32_t (*sample_rate)(void);                 /* 44100 */
    void (*init)(uint32_t seed);
    void (*set)(uint32_t j, int32_t v);
    /* the utterances: 0..25 the letters, then numbers, phrases, the 100 words (word_base), the phonemes */
    uint32_t (*count)(void);                       /* the utterances before the phonemes (what SING can sing) */
    uint32_t (*letter_base)(void);
    uint32_t (*word_base)(void);
    const char *(*text)(uint32_t id);
    /* say an utterance: note > 0 sings its voiced frames at that MIDI note, hold keeps its loudest vowel until release */
    void (*say)(uint32_t id, uint32_t note, uint32_t hold);
    void (*sing_note)(uint32_t note);              /* the note it sings now (legato) */
    void (*release)(void);                         /* hold off: it says the rest of the word */
    /* speak words (capitals, spaces): the phonemes with an intonation; marks: word i reached = i + 1 (mark) */
    void (*speak)(const char *text);
    void (*stop)(void);
    uint32_t (*busy)(void);
    uint32_t (*mark)(void);
    uint32_t (*progress)(void);                    /* 0..256 of the utterance under way */
    void (*frame)(int32_t *k10, int32_t *energy);  /* the lattice now: ten coefficients (x 16384), the energy */
    void (*render)(uint32_t n, float *out);        /* n frames, mono, -1..1, at sample_rate() */
} jsp_api;

typedef const jsp_api *(*jsp_get_fn)(void);

#ifdef __cplusplus
}
#endif
#endif
