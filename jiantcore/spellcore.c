/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT VOICE for VCV Rack: the SPELL core, VOICE's fifth language. SPELLBOX's speech chip and text to speech
 * (instruments/spellbox, GPL-3.0-only, by pjac1810: github.com/pjac1810/spellbox) as one small library, used the way
 * SPELLBOX's own SING, LETTER SYNTH and SPEAK use it (src/main.c sing_key, tts.c tts_say), without its games. */
#include "../instruments/spellbox/speech.c"
#include "../instruments/spellbox/tts.c"
#include "spellcore.h"

#if defined(_WIN32)
#define JSP_EXPORT __declspec(dllexport)
#else
#define JSP_EXPORT __attribute__((visibility("default")))
#endif

static uint32_t jsp_sample_rate(void) { return SAMPLE_RATE; }

static void jsp_init(uint32_t seed)
{
    fm1_rng = seed ? seed : 0x2545F491u;
    sp_init();
    sp_stop();
    sp_sing = 0, sp_hold = 0, sp_mark = 0;
}

static void jsp_set(uint32_t j, int32_t v)
{
    switch (j) {
    case JSP_PITCH: SP.pitch = (int16_t)v; break;
    case JSP_SPEED: SP.speed = (int16_t)v; break;
    case JSP_CLOCK: SP.clock = (int16_t)v; break;
    case JSP_BEND: SP.bend = (int16_t)v; break;
    case JSP_STUTTER: SP.stutter = (int16_t)v; break;
    case JSP_MONO: SP.mono = (int16_t)v; break;
    case JSP_CRUSH: SP.crush = (int16_t)v; break;
    case JSP_TONE: SP.tone = (int16_t)v; break;
    case JSP_ARTIC: SP.artic = (int16_t)v; break;
    default: break;
    }
}

static uint32_t jsp_count(void) { return SP_PHONEME; }
static uint32_t jsp_letter_base(void) { return SP_LETTER; }
static uint32_t jsp_word_base(void) { return SP_WORD; }
static const char *jsp_text(uint32_t id) { return id < SP_COUNT ? SP_TEXT[id] : ""; }

static void jsp_say(uint32_t id, uint32_t note, uint32_t hold)
{
    if (id >= SP_PHONEME)
        return;
    sp_stop();
    sp_sing = (uint8_t)(note > 127u ? 127u : note);
    sp_hold = (uint8_t)(hold ? 1u : 0u);
    sp_queue(SPQ_SAY((uint16_t)id));
}
static void jsp_sing_note(uint32_t note) { sp_sing = (uint8_t)(note > 127u ? 127u : note); }
static void jsp_release(void) { sp_hold = 0; }

/* tts.c tts_say with a mark before each word (word i: i + 1) */
static void jsp_speak(const char *text)
{
    char up[128];
    uint8_t ph[120], wd[120];
    int n = 0, np = 0, w = 0;
    for (; text[n] && n < 127; n++) {
        char c = text[n];
        up[n] = c >= 'a' && c <= 'z' ? (char)(c - 32) : c;
    }
    up[n] = 0;
    sp_stop();
    sp_sing = 0, sp_hold = 0, sp_mark = 0;
    int i = 0;
    while (i < n && np < 96) {
        while (i < n && !(up[i] >= 'A' && up[i] <= 'Z')) i++;
        int a = i;
        while (i < n && ((up[i] >= 'A' && up[i] <= 'Z') || up[i] == '\'')) i++;
        if (i > a) {
            char word[40];
            int m = 0;
            for (int j = a; j < i && m < 39; j++)
                if (up[j] != '\'') word[m++] = up[j];
            int k = tts_word(word, m, ph + np, 96 - np);
            for (int j = 0; j < k; j++) wd[np + j] = (uint8_t)w;
            np += k > 0 ? k : 0;
            w++;
        }
    }
    int stressed = 0, last = -1;
    for (int j = 0; j < np; j++)
        if (ph[j] & PH_STRESS) stressed++, last = j;
    /* the pitch: 85 Hz down to 71 Hz over the phrase; stressed vowels a step up, the last one falling */
    int s = 0, first = 1, cur = -1;
    for (int j = 0; j < np; j++) {
        uint8_t p = ph[j] & 0x3F;
        if (wd[j] != cur) {
            cur = wd[j];
            sp_queue(SPQ_MARK((uint16_t)(cur + 1)));
            first = 1;
        }
        if (is_vowel_ph(p)) {
            int hz = 85 - 14 * j / (np ? np : 1);
            if (ph[j] & PH_STRESS) hz = j == last && stressed > 1 ? hz * 9 / 10 : hz * 115 / 100 - s * 3, s++;
            sp_queue(SPQ_PITCH((uint16_t)(8000 * 16 / hz)));
        }
        uint16_t it = (uint16_t)(SP_PHONEME + p);
        sp_queue(first ? SPQ_SAY(it) : SPQ_SAYC(it));
        first = 0;
    }
    sp_queue(SPQ_PITCH(0));
}

static void jsp_stop(void) { sp_stop(), sp_hold = 0; }
static uint32_t jsp_busy(void) { return (uint32_t)sp_busy(); }
static uint32_t jsp_mark(void) { return sp_mark; }
static uint32_t jsp_progress(void) { return sp_progress(); }
static void jsp_frame(int32_t *k10, int32_t *energy)
{
    const int32_t *k = sp_frame(energy);
    for (int i = 0; i < 10; i++) k10[i] = k[i];
}

static void jsp_render(uint32_t n, float *out)
{
    int32_t buf[2 * 64];
    while (n) {
        uint32_t m = n > 64u ? 64u : n;
        for (uint32_t i = 0; i < 2u * m; i++) buf[i] = 0;
        sp_render(buf, m);
        for (uint32_t i = 0; i < m; i++) *out++ = (float)buf[2u * i] / 32768.0f;
        n -= m;
    }
}

static const jsp_api JSP = {
    JSP_API_VERSION, jsp_sample_rate, jsp_init, jsp_set, jsp_count, jsp_letter_base, jsp_word_base, jsp_text,
    jsp_say, jsp_sing_note, jsp_release, jsp_speak, jsp_stop, jsp_busy, jsp_mark, jsp_progress, jsp_frame, jsp_render,
};

JSP_EXPORT const jsp_api *jsp_get(void) { return &JSP; }
