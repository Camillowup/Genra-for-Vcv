/* SPDX-License-Identifier: GPL-3.0-only
 * The K-WASUB core (kwcore/kwcore.c with the firmware) through its interface: every factory preset plays a note and
 * lets it go (finite, within -1..1, sounding, then silent), parameters clamp, the EQ and the tempo take their values.
 * gcc -std=gnu11 -O2 -w -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns -Ikwasub/firmware/src -Ikwasub/firmware/gen -Ikwasub/firmware/hal test/kwcore_test.c -o kt -lm && ./kt */
#include "../kwcore/kwcore.c"
int printf(const char *, ...);

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL " __VA_ARGS__); printf("\n"); } } while (0)

static int same(const char *x, const char *y)
{
    while (*x && *x == *y) x++, y++;
    return *x == *y;
}

static float run(const kw_api *a, int blocks, int *bad)
{
    float pk = 0.f;
    int b, i;
    for (b = 0; b < blocks; b++) {
        const float *f = a->render();
        for (i = 0; i < 2 * KW_FRAMES; i++) {
            float x = f[i] < 0.f ? -f[i] : f[i];
            if (!(x <= 1.0f)) (*bad)++;
            if (x > pk) pk = x;
        }
    }
    return pk;
}

int main(void)
{
    const kw_api *a = kw_get();
    uint32_t k, n;
    int bad = 0;
    CHECK(a->api == KW_API_VERSION, "api version");
    a->init();
    n = a->npresets();
    CHECK(n >= 90u, "presets: %u", n);
    CHECK(a->np() == 140u, "parameters: %u", a->np());
    for (k = 0; k < n; k++) {
        float on, off;
        a->load_preset(k);
        {   /* the style the list sorts by is the one the preset sets */
            char st[32];
            a->text(P_STYLE, st, sizeof st);
            CHECK(same(st, a->preset_style(k)), "%s: style %s, list says %s", a->preset_name(k), st,
                  a->preset_style(k));
        }
        a->midi(0x90, 36, 110);
        on = run(a, 60, &bad);
        a->midi(0x80, 36, 0);
        run(a, 900, &bad);
        off = run(a, 20, &bad);
        CHECK(on > 0.01f, "%s: silent (%.4f)", a->preset_name(k), on);
        CHECK(off < 0.02f || a->get(DL_ON) || a->get(RV_ON), "%s: rings on (%.4f)", a->preset_name(k), off);
        CHECK(a->voices() == 0u, "%s: a voice still on", a->preset_name(k));
    }
    CHECK(bad == 0, "%d samples out of -1..1 or not finite", bad);
    a->set(F_CUT, 500);
    CHECK(a->get(F_CUT) == a->hi(F_CUT), "clamp hi");
    a->set(A_OCT, -100);
    CHECK(a->get(A_OCT) == a->lo(A_OCT), "clamp lo");
    a->eq_set(1, KW_EQ_GAIN, 30);
    CHECK(a->eq_get(1, KW_EQ_GAIN) == 12, "eq gain clamp: %d", a->eq_get(1, KW_EQ_GAIN));
    a->set_bpm10(1745);
    CHECK(a->bpm10() == 1745u, "bpm");
    {
        char b[48];
        a->load_preset(0);
        a->text(F_TYPE, b, sizeof b);
        CHECK(b[0] != 0, "text");
    }
    /* four voices at once, everything loud: still within range */
    a->load_preset(9);
    a->set(V_MODE, 2);
    a->set(DR_ON, 1); a->set(DR_AMT, 127); a->set(F_RES, 127); a->set(MS_VOL, 127);
    a->midi(0x90, 24, 127); a->midi(0x90, 31, 127); a->midi(0x90, 36, 127); a->midi(0x90, 43, 127);
    run(a, 200, &bad);
    CHECK(a->voices() == 4u, "poly: %u voices", a->voices());
    CHECK(bad == 0, "poly: %d bad samples", bad);
    printf(fails ? "%d FAILED\n" : "all K-WASUB core tests passed\n", fails);
    return fails != 0;
}
