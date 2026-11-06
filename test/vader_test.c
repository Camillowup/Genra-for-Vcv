/* The VADER engine as UMLAUF uses it (vader/, without Rack): every corner engine, every mode, every shape, every
 * preset plays, stays finite and in range; the orbiter moves; the corner bus adds up; two instances render at once on
 * two threads (the patched scratch buffers).
 *
 *   make RACK_DIR=... build/vader/libvader.a
 *   gcc -std=gnu11 -O2 -Ivader/src -Ivader/third_party/mpc-vst-plugins/wrapper test/vader_test.c build/vader/libvader.a \
 *       -lstdc++ -lm -lpthread -o vt && ./vt vader/shapes */
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "engine.h"
#include "overlay.h"

void vader_render_f(void *inst, float *l, float *r, int frames);
void *vader_view_bus(void *inst);
void vader_set_corner_bus(void *inst, float (*bus)[128]);
int vader_voices_on(void *inst);
int vader_list_count(void *inst, int wav);

static const mpc_engine_t *E;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static void setf(void *s, const char *k, float v) {
    char b[32];
    snprintf(b, sizeof b, "%.6f", v);
    E->set_param(s, k, b);
}
static void note(void *s, int n, int on) {
    uint8_t m[3] = {(uint8_t)(on ? 0x90 : 0x80), (uint8_t)n, (uint8_t)(on ? 100 : 0)};
    E->midi(s, m, 3);
}
/* frames at 44.1 kHz; returns the peak, sets *bad if anything was not finite */
static float run(void *s, int frames, int *bad, double *rms) {
    float l[128], r[128], pk = 0;
    double acc = 0;
    long n = 0;
    for (int done = 0; done < frames; done += 128) {
        vader_render_f(s, l, r, 128);
        for (int i = 0; i < 128; i++) {
            if (!isfinite(l[i]) || !isfinite(r[i])) *bad = 1;
            float a = fabsf(l[i]) > fabsf(r[i]) ? fabsf(l[i]) : fabsf(r[i]);
            if (a > pk) pk = a;
            acc += (double)l[i] * l[i] + (double)r[i] * r[i];
            n += 2;
        }
    }
    if (rms) *rms = sqrt(acc / (n ? n : 1));
    return pk;
}

static const char *ENG[15] = {"SYNTH", "ANALOG", "TRIO", "WAVE", "CHIP", "MACRO", "WHEEL", "VOICE", "NOISE", "TONE",
                              "METAL", "BAZZ", "DRONE", "TEXTURE", "FX"};

static void *thread_play(void *arg) {
    void *s = arg;
    int bad = 0;
    note(s, 48, 1);
    note(s, 55, 1);
    float pk = run(s, 44100 * 2, &bad, NULL);
    CHECK(!bad && pk > 0.01f, "two threads: peak %.3f bad %d", pk, bad);
    return NULL;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "vader/shapes";
    E = mpc_engine();
    void *s = E->create(dir);
    CHECK(s != NULL, "create");
    if (!s) return 1;
    CHECK(vader_list_count(s, 0) >= 7, "the example shapes: %d found in %s", vader_list_count(s, 0), dir);
    int bad = 0;
    double rms;

    /* the default sound */
    note(s, 60, 1);
    float pk = run(s, 44100, &bad, &rms);
    CHECK(!bad && pk > 0.02f && pk < 1.5f, "default: peak %.3f", pk);
    printf("default sound: peak %.3f, rms %.3f, voices %d\n", pk, rms, vader_voices_on(s));
    note(s, 60, 0);
    run(s, 44100 * 3, &bad, NULL);
    CHECK(vader_voices_on(s) == 0, "release: %d voices still on", vader_voices_on(s));

    /* the orbiter moves, the corner weights add up to 1 */
    {
        ovl_view_t v0, v1;
        note(s, 60, 1);
        run(s, 4410, &bad, NULL);
        overlay_read_view(vader_view_bus(s), &v0);
        run(s, 44100 / 2, &bad, NULL);
        overlay_read_view(vader_view_bus(s), &v1);
        float d = fabsf(v1.ox - v0.ox) + fabsf(v1.oy - v0.oy);
        CHECK(d > 0.01f, "the orbiter stands still (%.4f)", d);
        float w = v1.w[0] + v1.w[1] + v1.w[2] + v1.w[3];
        CHECK(fabsf(w - 1) < 0.01f, "weights add to %.3f", w);
        CHECK(v1.env > 0.1f, "env %.3f", v1.env);
        note(s, 60, 0);
        run(s, 44100 * 3, &bad, NULL);
    }

    /* the corner bus: dry, the four together close to the voices' sum (no effects: compare against the mix with FX off) */
    {
        static float bus[4][128];
        setf(s, "mix_rev", 0); setf(s, "mix_ch", 0); setf(s, "mix_dly", 0); setf(s, "drive", 0);
        vader_set_corner_bus(s, bus);
        note(s, 57, 1);
        float l[128], r[128];
        double sumc = 0, sumo = 0;
        for (int k = 0; k < 200; k++) {
            memset(bus, 0, sizeof bus);
            vader_render_f(s, l, r, 128);
            for (int i = 0; i < 128; i++) {
                float c = bus[0][i] + bus[1][i] + bus[2][i] + bus[3][i];
                sumc += fabs(c);
                sumo += fabs(l[i] + r[i]);
            }
        }
        CHECK(sumc > 1.0, "corner bus empty (%.3f)", sumc);
        printf("corner bus: %.1f, mix: %.1f\n", sumc, sumo);
        vader_set_corner_bus(s, NULL);
        note(s, 57, 0);
        run(s, 44100 * 3, &bad, NULL);
    }

    /* every engine in every corner */
    for (int e = 0; e < 15; e++) {
        const char *keys[4] = {"a_eng", "b_eng", "c_eng", "d_eng"};
        for (int c = 0; c < 4; c++) setf(s, keys[c], (float)e);
        bad = 0;
        note(s, 52, 1);
        pk = run(s, 44100, &bad, &rms);
        note(s, 52, 0);
        run(s, 44100 * 3, &bad, NULL);
        CHECK(!bad && pk > 0.005f && pk < 2.0f, "engine %s: peak %.3f bad %d", ENG[e], pk, bad);
        printf("engine %-8s peak %.3f rms %.3f\n", ENG[e], pk, rms);
    }
    for (int c = 0; c < 4; c++) { const char *k[4] = {"a_eng", "b_eng", "c_eng", "d_eng"}; setf(s, k[c], 0); }

    /* every mode */
    for (int m = 0; m < 27; m++) {
        setf(s, "sim_mode", (float)m);
        bad = 0;
        ovl_view_t v0, v1;
        note(s, 60, 1);
        run(s, 4410, &bad, NULL);
        overlay_read_view(vader_view_bus(s), &v0);
        pk = run(s, 44100, &bad, NULL);
        overlay_read_view(vader_view_bus(s), &v1);
        note(s, 60, 0);
        run(s, 44100 * 2, &bad, NULL);
        CHECK(!bad && pk > 0.005f && pk < 2.0f, "mode %d: peak %.3f bad %d", m, pk, bad);
        CHECK(isfinite(v1.ox) && fabsf(v1.ox) <= 1.001f && fabsf(v1.oy) <= 1.001f, "mode %d: orbiter at %.3f %.3f", m, v1.ox, v1.oy);
    }
    setf(s, "sim_mode", 0);

    /* every shape (not USER) */
    for (int sh = 0; sh < 51; sh++) {
        if (sh == 15) continue;
        setf(s, "shape", (float)sh);
        bad = 0;
        note(s, 60, 1);
        pk = run(s, 22050, &bad, NULL);
        note(s, 60, 0);
        run(s, 44100 * 2, &bad, NULL);
        CHECK(!bad && pk > 0.005f && pk < 2.0f, "shape %d: peak %.3f bad %d", sh, pk, bad);
    }

    /* every preset (the six sample presets have no samples here: only finite) */
    for (int p = 0; p < 22; p++) {
        setf(s, "preset", (float)p);
        char name[64] = "";
        char key[16];
        snprintf(key, sizeof key, "preset:%d", p);
        E->get_param(s, key, name, sizeof name);
        bad = 0;
        note(s, 48, 1); note(s, 55, 1); note(s, 64, 1);
        pk = run(s, 44100, &bad, NULL);
        note(s, 48, 0); note(s, 55, 0); note(s, 64, 0);
        run(s, 44100 * 4, &bad, NULL);
        CHECK(!bad && pk < 3.0f, "preset %d %s: peak %.3f bad %d", p, name, pk, bad);
        printf("preset %2d %-16s peak %.3f\n", p, name, pk);
    }

    /* state round trip */
    {
        setf(s, "preset", 3);
        setf(s, "a_cutoff", 0.123f);
        int need = E->get_param(s, "state", NULL, 0);
        char *st = malloc((size_t)need);
        int n = E->get_param(s, "state", st, need);
        CHECK(n > 100, "state: %d bytes", n);
        void *s2 = E->create(dir);
        E->set_param(s2, "state", st);
        char b[32] = "";
        E->get_param(s2, "a_cutoff", b, sizeof b);
        CHECK(fabsf(strtof(b, NULL) - 0.123f) < 0.002f, "state: a_cutoff %s", b);
        E->destroy(s2);
        free(st);
    }

    /* names the module shows */
    {
        char b[64] = "";
        setf(s, "a_eng", 7);   /* VOICE */
        E->get_param(s, "a_v1_name", b, sizeof b);
        CHECK(!strcmp(b, "VOWEL"), "a_v1_name: '%s'", b);
        setf(s, "sim_mode", 4);   /* KEPLER */
        E->get_param(s, "mk2_name", b, sizeof b);
        CHECK(!strcmp(b, "ECCENTRIC"), "mk2_name: '%s'", b);
        setf(s, "a_eng", 0);
        setf(s, "sim_mode", 0);
    }

    /* CPU: 8 notes, 2 s */
    {
        setf(s, "preset", 0);
        for (int k = 0; k < 8; k++) note(s, 48 + k * 3, 1);
        clock_t t0 = clock();
        run(s, 44100 * 2, &bad, NULL);
        double sec = (double)(clock() - t0) / CLOCKS_PER_SEC;
        printf("8 voices: %.1f %% of a core\n", 100.0 * sec / 2.0);
        for (int k = 0; k < 8; k++) note(s, 48 + k * 3, 0);
    }

    /* two instances, two threads */
    {
        void *a = E->create(dir), *b = E->create(dir);
        pthread_t ta, tb;
        pthread_create(&ta, NULL, thread_play, a);
        pthread_create(&tb, NULL, thread_play, b);
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);
        E->destroy(a);
        E->destroy(b);
    }

    E->destroy(s);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
