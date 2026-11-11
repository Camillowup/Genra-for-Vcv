/* SPDX-License-Identifier: GPL-3.0-only */
/* GENRA: what SPELLBOX's speech.c and tts.c take from the FM-1 platform (foma's fm1.h), for the SPELL core
 * (jiantcore/spellcore.c): the sample rate, a note's phase step, a random number. */
#ifndef FM1_H
#define FM1_H
#include <stdint.h>
#include <math.h>

#define SAMPLE_RATE 44100u

static uint32_t fm1_rng = 0x2545F491u;
static inline uint32_t random32(void) { /* xorshift */
  uint32_t x = fm1_rng;
  x ^= x << 13, x ^= x >> 17, x ^= x << 5;
  return fm1_rng = x;
}
/* the phase step per frame of a MIDI note at SAMPLE_RATE (2^32 = one cycle) */
static inline uint32_t note_inc(int note) {
  double f = 440.0 * pow(2.0, (note - 69) / 12.0);
  return (uint32_t)(f / SAMPLE_RATE * 4294967296.0);
}
#endif
