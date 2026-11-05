/* SPDX-License-Identifier: GPL-3.0-only */
/* MONK-1's singers for VCV Rack: the firmware's character drawing (app/face.c, its raster and the packed art from
 * tools/gen_art.py) built as one unit, with one entry point: a whole 240 x 240 frame in RGB565 (byte-swapped, as the
 * FM-1's panel takes it). The firmware redraws only the face's region; here the scene is drawn once per singer and
 * the region on top, each frame. Not thread-safe (the raster's buffers are static): call it from the UI thread. */
#include <stdint.h>
#include <string.h>
#include "monkface.h"
#define OM_POOL
#include "fw/fastmath.h"
#include "fw/art.h"
#include "fw/inflate.c"
#include "fw/face.c"

int monkface_count(void) { return ART_N; }
const char *monkface_name(int ch) { return ART[(ch % ART_N + ART_N) % ART_N].name; }
const float *monkface_voice(int ch) { return ART[(ch % ART_N + ART_N) % ART_N].p; }
uint16_t monkface_accent(int ch) { return ART[(ch % ART_N + ART_N) % ART_N].accent; }

void monkface_scene(int ch, uint16_t *px)
{
    const art_char_t *a = &ART[(ch % ART_N + ART_N) % ART_N];
    face_scene(a, px, 0, 0, 240, 240);
}

void monkface_frame(int ch, float vowel, float loud, int blink, const uint16_t *scene, uint16_t *px)
{
    static uint16_t reg[256 * 256];
    const art_char_t *a = &ART[(ch % ART_N + ART_N) % ART_N];
    face_pose_t p;
    int32_t y;
    p.vowel = (uint8_t)(fm_clampf(vowel, 0.0f, 1.0f) * (float)(FACE_VOWEL_FRAMES - 1) + 0.5f);
    p.loud = (uint8_t)(fm_clampf(loud, 0.0f, 1.0f) * 4.0f + 0.5f);
    p.blink = (uint8_t)(blink != 0);
    if (scene != px)
        memcpy(px, scene, 240u * 240u * 2u);
    face_draw(a, p, reg);
    for (y = 0; y < a->rh && a->ry + y < 240; y++)
        memcpy(px + (a->ry + y) * 240 + a->rx, reg + y * a->rw, (size_t)a->rw * 2u);
}

/* the idle animation (the iOS port's): advance every 100 ms */
void monkface_idle_reset(monkface_idle_t *s) { face_idle_reset((face_idle_t *)s); }
void monkface_idle_tick(monkface_idle_t *s) { face_idle_advance((face_idle_t *)s); }
float monkface_idle_vowel(const monkface_idle_t *s) { return face_idle_vowel((const face_idle_t *)s); }
int monkface_idle_blink(const monkface_idle_t *s) { return face_idle_blink((const face_idle_t *)s); }
