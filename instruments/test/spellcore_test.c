// SPDX-License-Identifier: GPL-3.0-only
// The SPELL core without Rack (VOICE's fifth language): SING a word held at a note, LETTER, SPEAK a phrase with its
// word marks, all knobs at their ends (bent, crushed, fast, slow). Fails on silence, NaN or a level out of range.
//   gcc -std=gnu11 -O2 -I. test/spellcore_test.c -lm -o st && ./st [out.raw]
#include "../../jiantcore/spellcore.c"
#include <stdio.h>
#include <string.h>
#include <math.h>

static FILE* raw;
static int fails;

static double run(const jsp_api* a, int frames, float* peak) {
    float b[441];
    double s = 0;
    *peak = 0;
    for (int f = 0; f < frames; f += 441) {
        a->render(441, b);
        for (int i = 0; i < 441; i++) {
            if (!isfinite(b[i])) { printf("NaN\n"); fails++; return 0; }
            s += b[i] * b[i];
            if (fabsf(b[i]) > *peak) *peak = fabsf(b[i]);
        }
        if (raw) fwrite(b, sizeof b, 1, raw);
    }
    return sqrt(s / frames);
}

static void check(const char* what, double rms, float peak) {
    int ok = rms > 0.003 && peak <= 1.0f;
    printf("%-28s rms %.4f peak %.3f %s\n", what, rms, peak, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

int main(int argc, char** argv) {
    raw = argc > 1 ? fopen(argv[1], "wb") : NULL;
    const jsp_api* a = jsp_get();
    if (a->api != JSP_API_VERSION) return 1;
    a->init(7);
    float pk;
    uint32_t apple = 0;
    for (uint32_t i = 0; i < a->count(); i++)
        if (!strcmp(a->text(i), "APPLE")) apple = i;
    printf("utterances %u, APPLE %u, words from %u\n", a->count(), apple, a->word_base());
    a->say(apple, 60, 1);
    double r = run(a, 44100, &pk);
    a->release();
    r += run(a, 22050, &pk);
    check("SING APPLE at C4, held", r, pk);
    a->say(a->letter_base() + 1, 67, 1);
    r = run(a, 22050, &pk);
    a->release();
    check("LETTER B at G4", r, pk);
    a->speak("hello, I am a robot");
    uint32_t marks = 0;
    r = 0;
    int it = 0;
    for (; it < 400 && a->busy(); it++) {
        float p2;
        r += run(a, 441, &p2);
        if (p2 > pk) pk = p2;
        if (a->mark() > marks) marks = a->mark();
    }
    check("SPEAK hello I am a robot", r / (it ? it : 1), pk);
    printf("  spoken in %.2f s\n", it / 100.0);
    printf("  marks reached: %u (5 words)\n", marks);
    if (marks != 5) fails++;
    const int ends[][2] = {{JSP_CLOCK, 0}, {JSP_CLOCK, 127}, {JSP_BEND, 127}, {JSP_STUTTER, 127}, {JSP_CRUSH, 5},
                           {JSP_SPEED, 0}, {JSP_SPEED, 127}, {JSP_PITCH, -12}, {JSP_PITCH, 12}, {JSP_ARTIC, 0}};
    for (unsigned e = 0; e < sizeof ends / sizeof ends[0]; e++) {
        a->set(ends[e][0], ends[e][1]);
        a->speak("the quick brown fox");
        r = run(a, 44100, &pk);
        char n[40];
        snprintf(n, sizeof n, "knob %d at %d", ends[e][0], ends[e][1]);
        check(n, r, pk);
        a->set(JSP_CLOCK, 64), a->set(JSP_BEND, 0), a->set(JSP_STUTTER, 0), a->set(JSP_CRUSH, 0), a->set(JSP_SPEED, 64);
        a->set(JSP_PITCH, 0), a->set(JSP_ARTIC, 127);
    }
    if (raw) fclose(raw);
    printf(fails ? "FAILED\n" : "all ok\n");
    return fails ? 1 : 0;
}
