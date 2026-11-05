/* SPDX-License-Identifier: GPL-3.0-only */
/* MONK-1's twelve singers, drawn by the firmware's own code (monkface.c). */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { uint8_t shuffle, tick, pos; } monkface_idle_t;
int monkface_count(void);
const char *monkface_name(int ch);
const float *monkface_voice(int ch);   /* 17 values, MK_* order, 0..1 */
uint16_t monkface_accent(int ch);      /* RGB565 */
void monkface_scene(int ch, uint16_t *px240);
/* vowel 0..1, loud 0..1; scene from monkface_scene (may be px itself if it is redrawn each time) */
void monkface_frame(int ch, float vowel, float loud, int blink, const uint16_t *scene, uint16_t *px240);
void monkface_idle_reset(monkface_idle_t *s);
void monkface_idle_tick(monkface_idle_t *s);
float monkface_idle_vowel(const monkface_idle_t *s);
int monkface_idle_blink(const monkface_idle_t *s);
#ifdef __cplusplus
}
#endif
