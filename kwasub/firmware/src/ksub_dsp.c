/* SPDX-License-Identifier: GPL-3.0-only */
/* K-WASUB: the sound. Integers only, no 64-bit division (the chip has no FPU).
 *
 *   core 1   the voices: two oscillators (8 engines each), sub, noise, the filter, three envelopes, the
 *            two LFOs and the matrix, at a control rate of KS_CB samples      -> vbus (mono)
 *   core 0   the audio interrupt: drive, crush, chorus, delay, reverb, glue, the limiter -> L R
 *
 * With the second core running (KS_DUAL, fm1_cpu1_start) core 1 renders the next block while core 0 puts the
 * effects on this one (one block of latency, 5.8 ms). Without it (the emulator, or until the start of the
 * second core is checked on the FM-1) core 0 does both, one after the other: the same code, the same sound. */
#define KS_CB 16u                               /* samples per control block */
#define NVOICE 4u
#define CMB_N 1024u                             /* comb filter line per voice */

#if KS_DUAL
#define KS_SYNC() fm1_csync()                   /* (what one core wrote, the other sees: fm1_cpu1.h) */
#else
#define KS_SYNC() RING_PUBLISH()
#endif
#define BLOCK_TICKS (HALF_FRAMES * (FM1_TICKS_PER_US * 1000000u / FS))   /* 5.8 ms in 24 MHz ticks */
#define BLOCK_TICKS100 (BLOCK_TICKS / 100u)
static volatile uint8_t ks_shed;                /* the load governor's step (0: the whole sound) */
static volatile uint32_t ks_late;               /* blocks cut short (never past the block: the watchdog) */

/* ---- what the main loop sends to the voices (one writer, one reader) */
enum { EV_ON, EV_OFF, EV_PANIC, EV_SYNC, EV_SEQ, EV_DRUM };   /* EV_SEQ: a 1 play (b 1: from the bar), 0 stop; EV_DRUM: track a, velocity b */
static volatile uint32_t evq[64], ev_w, ev_r;
static void ev_push(uint32_t t, uint32_t a, uint32_t b)
{
    if (ev_w - ev_r >= 64u) return;
    evq[ev_w % 64u] = t | a << 8 | b << 16;
    KS_SYNC();
    ev_w++;
}

/* ---- shared with the main loop */
static int8_t patch[NP_MAX];                    /* the sound (the main loop writes, the voices read) */
typedef struct { uint8_t note[16], fl[16], len, rate, swing, pad; } seqpat_t;   /* 36 B */
enum { SF_ON = 1, SF_ACC = 2, SF_SLIDE = 4 };
#define SQ_NPAT 16u
#define PF_MAGIC 0x5Bu
#define MC_MAGIC 0x4Du
static struct {                                 /* stored (the store's second sector): what plays, whatever the patch */
    uint8_t magic, sum, cur, ar_on, ar_mode, ar_rate, ar_oct, ar_gate;
    seqpat_t pat[SQ_NPAT];                      /* (sum: up to here) */
    uint8_t mc_magic, mc_dst[16];               /* MACROS: four targets each (a destination, an amount) */
    int8_t mc_amt[16];
    uint8_t cc_magic, cc_num[16], cc_id[16];    /* MIDI LEARN: a CC (0xFF: none) and what it moves */
    uint8_t eq_magic, eq_t[4], eq_f[4], eq_q[4];   /* the EQ: each band's type, frequency, width */
    int8_t eq_g[4];                             /* ... and gain (dB) */
    uint8_t mx_magic, mx_pre[4], ab_pre, ab_parts, ab_time;   /* MORPH: the corners' sounds, A/B's B, its parts and time */
} perf;
/* SEQ BASS past the 16 steps perf holds (a pattern plays up to 64), and each step's chance and condition: stored on
 * the store's third sector */
#define SQ_MAX 64u
#define SX_MAGIC 0x53u
enum { SC_ALWAYS, SC_1_2, SC_2_2, SC_1_4, SC_2_4, SC_3_4, SC_4_4, SC_START, SC_NSTART, SC_BEFORE, SC_AFTER, NSC };
static struct {
    uint8_t magic, sum, pad[2];
    struct { uint8_t note[SQ_MAX - 16u], fl[SQ_MAX - 16u], cond[SQ_MAX], prob[SQ_MAX]; } p[SQ_NPAT];   /* prob: 0..100 % */
} sqx;
/* a step's note length: cond's high nibble (the condition is the low one); 0 is half a step, as before 0.9.53 */
#define SQ_HOLD 0xFFFFFFFFu
static const uint32_t SQ_GL[16] = {0x800000u, 0x200000u, 0x400000u, 0xC00000u, 1u << 24, 2u << 24, 3u << 24, 4u << 24,
                                   6u << 24, 8u << 24, 12u << 24, 16u << 24, 24u << 24, 32u << 24, 64u << 24, SQ_HOLD};
/* DRUMS (the SEQ page's fourth view): 8 tracks of an 808 or a 909, synthesised here; patterns of up to 64 steps, a hit,
 * an accent, a chance and a condition on every step. Stored on the store's fourth and fifth sectors. */
#define NDK 8u
#define DK_NPAT 8u
#define DK_MAX 64u
#define DK_MAGIC 0x44u
enum { DK_KICK, DK_SNARE, DK_CLAP, DK_CH, DK_OH, DK_TOM, DK_PERC, DK_CYM };
typedef struct { uint8_t len, swing, pad[2], on[DK_MAX], acc[DK_MAX], pc[NDK][DK_MAX]; } dkpat_t;   /* pc: chance (high nibble) | condition */
static struct {
    uint8_t magic, sum, cur, kit, vol, mode, mute, pad;   /* kit 0 808, 1 909; mode & 3: 0 bass + drums, 1 drums only, 2 bass only; mode & 4: SEQ BASS OFF */
    int8_t prm[NDK][4];                         /* each track's LEVEL 0..127, TUNE -64..63, DECAY 0..127, TONE 0..127 */
    dkpat_t pat[DK_NPAT];
} dk;
#define SQ_BASS_ON (!(dk.mode & 4u) && (dk.mode & 3u) != 1u)   /* mode bit 2: the SEQ BASS sequence OFF (bass free to play live while the drums run) */
static const uint8_t DK_PROB[16] = {100, 95, 90, 85, 80, 75, 70, 65, 60, 50, 40, 33, 25, 20, 10, 5};
/* PARAMETER LOCKS (0.9.57): a step's own value for a setting, on the drums (per track) and SEQ BASS; one list of 512 for
 * all their patterns, on the store's sixth sector. pat: SEQ BASS's (PL_BASS | 0..15) or a drum pattern (0..7); tp: the
 * track << 4 | what is locked */
#define PL_MAX 512u
#define PL_MAGIC 0x50u
#define PL_BASS 0x80u
enum { PK_LVL, PK_TUNE, PK_DEC, PK_TONE, PK_RAT, PK_ECHO, NPKD };   /* DRUMS: LEVEL TUNE DECAY TONE, RATCHET (1..4 hits), ECHO (DUB TECHNO's send) */
enum { PB_CUT, PB_RES, PB_ENV, PB_DRV, PB_WOB, PB_SHAPE, PB_SMP, NPKB };   /* SEQ BASS: CUTOFF RESO FILT ENV DRIVE, WOB RATE, A SHAPE, SAMPLE START */
static const uint8_t PB_ID[NPKB] = {F_CUT, F_RES, F_ENV, F_DRV, L1_DIV, A_P1, SM_START};
typedef struct { uint8_t pat, step, tp; int8_t v; } plk_t;
static struct { uint8_t magic, sum; uint16_t n; plk_t l[PL_MAX]; } pl;
static uint8_t dk_lkm[NDK], dk_rn[NDK];         /* the step playing: each track's locks (bits), its ratchet hits to come */
static int8_t dk_lkv[NDK][NPKD];
static uint32_t dk_rc[NDK], dk_riv[NDK];        /* ... the samples to the next ratchet hit, between two */
static volatile uint8_t sq_lkm;                 /* SEQ BASS: the note playing's locks (bits) and values */
static int8_t sq_lkv[NPKB];
#define LKB(b, x) (sq_lkm >> (b) & 1u ? (int32_t)sq_lkv[b] : (int32_t)(x))
static uint8_t *sq_np(uint32_t k, uint32_t i) { k %= SQ_NPAT; i %= SQ_MAX; return i < 16u ? &perf.pat[k].note[i] : &sqx.p[k].note[i - 16u]; }
static uint8_t *sq_fp(uint32_t k, uint32_t i) { k %= SQ_NPAT; i %= SQ_MAX; return i < 16u ? &perf.pat[k].fl[i] : &sqx.p[k].fl[i - 16u]; }
/* the EQ (held FX), on every sound: 4 biquads a side, Q24 (b0 b1 b2 a1 a2); the main loop works them out (eq_n),
 * the audio takes them at the next block (eq_new) */
enum { EQ_OFF, EQ_LOCUT, EQ_LOSH, EQ_BELL, EQ_NOTCH, EQ_HISH, EQ_HICUT, NEQT };
static int32_t eq_c[4][5], eq_n[4][5], eq_st[2][4][5];   /* (state: x1 x2 y1 y2, and what >> 24 dropped: fed back) */
static volatile uint8_t eq_new, eq_nmask;
static uint8_t eq_mask;
static volatile uint8_t mc_val[4];              /* the four macros now, 0..127 (K1..K4 in the MACROS window; not stored) */
/* FX LIVE (not stored, all off at the start): DJ filter (- low pass, + high pass) and its resonance, PUMP (the
 * sidechain dip on the tempo) and its rate, STUTTER (the last slice played again and again), TAPE (slower, to a stop) */
static volatile int8_t lfx_flt;
static volatile uint8_t lfx_fres = 40, lfx_pump, lfx_prate, lfx_stut, lfx_tape, lfx_frz;   /* lfx_frz: FREEZE (1.0.48) */
static volatile uint32_t ks_bpm10 = 1400;       /* tempo x 10 */
static volatile int32_t ks_wheel, ks_press, ks_bend;   /* Q15 */
static volatile uint32_t ks_beats;              /* Q24 beats, kept modulo 48 beats (every division fits) */
static volatile uint8_t ks_sounding;
static int16_t scope[512];                      /* the output, one sample in 4 (the PLAY page draws it) */
static volatile uint32_t scope_w;
static volatile uint32_t lfo_ph_show[3];
enum { L3_SHP, L3_SYNC, L3_RATE, L3_DIV, L3_DEP, L3_DST, NL3 };
static volatile int8_t l3cfg[NL3];              /* LFO 3 (1.0.17): kept in the settings, whatever the patch (the main loop copies them in) */        /* where the LFOs are (the LFO page's dot) */
static volatile uint8_t env_st_show[3];         /* the newest voice's envelopes: stage, level Q15 (the ENV dots) */
static volatile int32_t env_lv_show[3], cut_show; /* ... and its cutoff after the modulation (Q8 units) */
static volatile uint32_t sm_show;               /* the newest voice's place in its sample (Q16; the SAMPLE view) */
static volatile int32_t step_val;               /* the STEP effect: its level now (Q15), its step */
static volatile uint8_t step_pos;
static volatile uint8_t ks_straight;            /* LFO GRID: round divisions only */
static volatile int32_t gmod[ND];               /* the global destinations' modulation (Q8 units), once a block */
static int8_t gval[ND - D_N0];                  /* the parameters of the destinations from D_AEA, modulated (gv_update, once a block) */
#define GP(d) gval[(d) - D_N0]
static void gv_update(void)
{
    uint32_t k;
    for (k = 0; k < ND - D_N0; k++) {
        uint32_t id = GD_ID[k];
        int32_t g = gmod[D_N0 + k];               /* (the whole depth: the whole range) */
        gval[k] = (int8_t)(g ? clampi(patch[id] + (int32_t)(((int64_t)g * (PD[id].hi - PD[id].lo)) >> 15), PD[id].lo, PD[id].hi) : patch[id]);
    }
}
static volatile int32_t meter_pk, meter_gr;     /* the output's peak and the glue's gain (Q15), for MASTER */
#define BEATS_WRAP (48u << 24)
/* RESAMPLE: the output, 11025 Hz mu-law, into RAM; OSC C plays it as its 16th sample (lost at power off) */
#define RS_RATE 11025u
#define RS_MAX (4u * RS_RATE)
static uint8_t rs_buf[RS_MAX] POOL;
static ks_smp_t rs_desc = {"RESAMPLE", 0u, 0u, 0u, 5523423u, 0};   /* (len 0: nothing yet; k: 11025 Hz, played as it was at C3) */
#define SM_RS 16                                /* SAMPLE's value for the RAM one (RESAMPLE); the samples added in 0.9.55 come after it */
static const ks_smp_t *sm_desc(int32_t sel) { return sel == SM_RS ? &rs_desc : &KS_SMP[(uint32_t)(sel < SM_RS ? sel - 1 : sel - 2) % KS_NSMP]; }
static volatile uint32_t rs_want, rs_pos;       /* recording: how many samples, how many so far (0: not recording) */
static int32_t rs_acc;
static uint32_t rs_n;
static uint8_t rs_enc(int32_t x)                /* mu-law: the nearest of KS_MULAW (it rises with the index) */
{
    uint32_t lo = 0, hi = 255;
    while (lo < hi) { uint32_t mid = (lo + hi) >> 1; if (KS_MULAW[mid] < x) lo = mid + 1u; else hi = mid; }
    if (lo && x - KS_MULAW[lo - 1u] < KS_MULAW[lo] - x) lo--;
    return (uint8_t)lo;
}
static volatile int32_t ott_show[3] = {4096, 4096, 4096};   /* MULTIBAND: each band's gain now (Q12), for its view */   /* SYSTEM: AUTO LEVEL (the normaliser), DEEP BASS (0 off, 1 deep, 2 max) */

/* ---- helpers */
static inline int32_t sin32(uint32_t ph)
{
    uint32_t i = ph >> 22;
    int32_t a = KS_SIN[i], f = (int32_t)((ph >> 6) & 0xFFFFu);
    return a + (((KS_SIN[i + 1u] - a) * f) >> 16);
}
static inline int32_t mul15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }
static inline int32_t sat15(int32_t x)          /* soft clip: x - 4/27 x^3, flat beyond +-1.5 */
{
    int32_t x2;
    if (x > 49152) return 32767;
    if (x < -49152) return -32767;
    x2 = (x * (x >> 1)) >> 14;
    return x - mul15(mul15(x, x2), 4855);
}
static inline int32_t clip(int32_t x, int32_t m) { return x > m ? m : x < -m ? -m : x; }
static uint32_t ks_rng = 0x9E3779B9u;
static inline uint32_t rnd(void)
{
    ks_rng ^= ks_rng << 13;
    ks_rng ^= ks_rng >> 17;
    ks_rng ^= ks_rng << 5;
    return ks_rng;
}
static uint32_t pitch_inc(int32_t q8)           /* note in 1/256 semitone -> phase step */
{
    int32_t q6, d;
    uint32_t o;
    if (q8 < 0) q8 = 0;
    if (q8 > 131 * 256) q8 = 131 * 256;
    q6 = q8 >> 2;
    if (q6 >= 120 * 64) return KS_PITCH[q6 - 120 * 64];
    d = 120 * 64 - q6;
    o = ((uint32_t)d + 767u) / 768u;
    return KS_PITCH[q6 + (int32_t)o * 768 - 120 * 64] >> o;
}
static inline uint32_t blep_rcp(uint32_t inc) { return (1u << 30) / ((inc >> 12) | 1u); }
static inline int32_t blep(uint32_t t, uint32_t inc, uint32_t rcp)   /* the PolyBLEP of a step of -2 at t = 0 */
{
    int32_t x;
    if (t < inc) {
        x = 32768 - (int32_t)(((t >> 12) * rcp) >> 15);
        return -((x * x) >> 15);
    }
    if (-t < inc) {
        x = 32768 - (int32_t)((((0u - t) >> 12) * rcp) >> 15);
        return (x * x) >> 15;
    }
    return 0;
}
static inline int32_t saw_bl(uint32_t ph, uint32_t inc, uint32_t rcp) { return (int32_t)(ph >> 16) - 32768 - blep(ph, inc, rcp); }
static inline int32_t sqr_bl(uint32_t ph, uint32_t pw, uint32_t inc, uint32_t rcp)   /* -1 then +1 from pw */
{
    return (ph < pw ? -32767 : 32767) - blep(ph, inc, rcp) + blep(ph - pw, inc, rcp);
}
static inline int32_t tri32(uint32_t ph) { return (int32_t)((ph ^ (uint32_t)((int32_t)ph >> 31)) >> 15) - 32768; }
static inline int32_t lfold(int32_t x)          /* a triangle fold of any x (Q15) back into +-1 */
{
    uint32_t f = (uint32_t)((x + 32768) * 2) & 0xFFFFu;
    int32_t d = (int32_t)f - 32768;
    return 32767 - 2 * (d < 0 ? -d : d);
}
static inline int32_t lvl15(int32_t v) { return v <= 0 ? 0 : v * v * 2; }   /* 0..127 -> Q15, squared */

/* ================================================================ the oscillators */
typedef struct {
    uint32_t ph[7], ph2, f[3];
    int32_t fb1, fb2;
} osc_t;
typedef struct {
    uint8_t eng, sync;                          /* sync: restart with the master (X MODE SYNC: B with A) */
    int32_t p1, p2, p3, p4;                     /* 0..32767 after the modulation (p4: FOLD, or CS's RING) */
    uint32_t inc, mph, minc, mrq;               /* the master's phase and step; its step / ours, Q8 */
} oscp_t;
/* hard sync, in the loops that read ph: when the master turns, ph starts again where the master is in its turn */
#define SYNC_STEP() do { if (sy) { uint32_t m0 = mp; mp += q->minc; if (mp < m0) ph = (mp >> 8) * q->mrq - inc; } } while (0)

static const uint16_t VOX_F[5][3] = {{800, 1150, 2900}, {400, 1600, 2700}, {270, 1850, 2600}, {450, 800, 2830}, {325, 700, 2530}};
static const uint16_t VOX_A[5][3] = {{32767, 16384, 8192}, {32767, 13107, 6554}, {32767, 9830, 6554}, {32767, 14746, 3277}, {32767, 8192, 2621}};

static inline int64_t cs_pwc(int64_t w) { return w < 0x0CCCCCCCll ? 0x0CCCCCCCll : w > 0xF3333333ll ? 0xF3333333ll : w; }   /* (CS: a pulse width kept in 5 .. 95 %) */
/* render n samples of one oscillator; pm (or 0): phase modulation per sample */
static uint32_t add_amps(int32_t p1, int32_t p2, int32_t p3, uint32_t reg, int32_t *a)   /* ADDITIVE: the 16 partials' levels (a[1..16], Q15,
                                                 * their sum at most full level); returns the highest one sounding */
{
    int32_t t = 14000 + ((p1 * 18767) >> 15), w = 32767, sum = 0, ev = p3 < 16384 ? 32767 - p3 * 2 : 32767, od = p3 < 16384 ? 32767 : 32767 - (p3 - 16384) * 2;
    uint32_t k, j, nh = 1u + (((uint32_t)p2 * 7u) >> 15), top = 0;
    for (k = 1; k <= 16u; k++) {
        int32_t r = 0;
        if (!reg) r = k & 1u ? 0 : 32767 / (int32_t)(k >> 1);   /* SAW: every harmonic of the note at 1/h */
        else for (j = 0; j < 9u; j++) if (ADD_BAR[j] == k) r = (ADD_REG[reg][j] - '0') * 4095;
        if (k > 2u * nh) r = 0;                 /* HARMONICS: how many of the note's */
        if (!(k & 1u) && k >= 4u) r = (int32_t)(((int64_t)r * ((k >> 1) & 1u ? od : ev)) >> 15);   /* ODD/EVEN */
        if (k > 2u) { w = (w * t) >> 15; r = (r * w) >> 15; }   /* TILT: each partial up a little darker */
        a[k] = r; sum += r;
        if (r) top = k;
    }
    if (sum > 32767) for (k = 1; k <= 16u; k++) a[k] = a[k] * 32767 / sum;
    return top;
}
static void osc_core(osc_t *o, const oscp_t *q, int32_t *out, const int32_t *pm, uint32_t n)
{
    uint32_t i, inc = q->inc, rcp = blep_rcp(inc), ph = o->ph[0], mp = q->mph, sy = q->sync;
    switch (q->eng) {
    case E_ANALOG: {                            /* sine > triangle > saw > square, a width, a drive */
        int32_t w = q->p1 * 3, seg = w >> 15, k = w & 32767, g = 32768 + q->p3 * 7;
        uint32_t pw = (uint32_t)(16384 + ((q->p2 * 14745) >> 15)) << 17;
        if (seg > 2) { seg = 2; k = 32767; }
        for (i = 0; i < n; i++) {
            int32_t a, b, x;
            uint32_t p;
            SYNC_STEP();
            ph += inc;
            p = ph + (pm ? (uint32_t)pm[i] : 0u);
            switch (seg) {
            case 0: a = sin32(p - 0x40000000u); b = tri32(p); break;
            case 1: a = tri32(p); b = saw_bl(p, inc, rcp); break;
            default: a = saw_bl(p, inc, rcp); b = sqr_bl(p, pw, inc, rcp); break;
            }
            x = a + (((b - a) * k) >> 15);
            out[i] = q->p3 ? sat15(mul15(x, g) >> 0) : x;
        }
        break;
    }
    case E_CS: {                                /* CS: a CS-80 voice's two channels (after twochannels/CS-1): each a sawtooth and a pulse
                                                 * (WAVE: saw > both > pulse, the pulse narrowing at the top), its own PWM triangle
                                                 * (0.47 / 0.71 Hz), channel II detuned (DETUNE, quadratic: up to 35 cents apart);
                                                 * MOTION: PWM depth, then the voice cards' drift (each channel wanders on its own,
                                                 * up to 8 cents) and a breath of noise */
        int32_t wv = q->p1, sw = wv < 16384 ? 32767 : 32767 - ((wv - 16384) << 1), uw = wv < 16384 ? wv << 1 : 32767;
        int32_t det = (int32_t)(((int64_t)q->p2 * q->p2) >> 15), dq = (det * 662) >> 15;   /* (Q16: +-17.5 cents each) */
        int32_t mo = q->p3, pd = mo >= 16384 ? 32767 : mo << 1, dr = mo > 16384 ? (mo - 16384) << 1 : 0, nz = dr >> 4;
        int32_t norm = (int32_t)((32767u << 15) / (uint32_t)(sw + uw)), d1, d2, l1, l2, dc1, dc2;
        int64_t pwb = 0x80000000ll - (int64_t)(((uint64_t)(wv > 16384 ? wv - 16384 : 0) * 0x59999999u) >> 14);   /* 50 % > 15 % */
        uint32_t inc1, inc2, rc1, rc2, pw1, pw2, ph1 = o->ph[1], nr = o->f[0] ? o->f[0] : 0x1234567u;
        int two = ks_shed < 3u;                  /* (the CPU short: channel I alone) */
        if (dr) {                                /* the drift: a slow random walk each */
            o->fb1 += (((int32_t)(rnd() >> 17) - 16384) >> 4) - (o->fb1 >> 7);
            o->fb2 += (((int32_t)(rnd() >> 17) - 16384) >> 4) - (o->fb2 >> 7);
            o->fb1 = clip(o->fb1, 32767); o->fb2 = clip(o->fb2, 32767);
        }
        d1 = -dq + (int32_t)(((int64_t)mul15(o->fb1, dr) * 302) >> 15);   /* (302: 8 cents, Q16) */
        d2 = dq + (int32_t)(((int64_t)mul15(o->fb2, dr) * 302) >> 15);
        inc1 = inc + (uint32_t)(int32_t)(((int64_t)inc * d1) >> 16);
        inc2 = inc + (uint32_t)(int32_t)(((int64_t)inc * d2) >> 16);
        rc1 = blep_rcp(inc1); rc2 = blep_rcp(inc2);
        inc = inc1;                              /* (SYNC_STEP: channel I follows the master) */
        o->ph[2] += 45773u * n; o->ph[3] += 69147u * n;   /* (the two PWM triangles, once a block) */
        l1 = tri32(o->ph[2]); l2 = tri32(o->ph[3] + 0x80000000u);
        pw1 = (uint32_t)cs_pwc(pwb + (((int64_t)mul15(l1, pd) * 0x26666666ll) >> 15));   /* (+-15 %, kept in 5 .. 95 %) */
        pw2 = (uint32_t)cs_pwc(pwb + (((int64_t)mul15(l2, pd) * 0x26666666ll) >> 15));
        dc1 = 32767 - (int32_t)(pw1 >> 16); dc2 = 32767 - (int32_t)(pw2 >> 16);
        for (i = 0; i < n; i++) {
            int32_t a, b, x;
            uint32_t p, p2;
            SYNC_STEP();
            ph += inc1;
            p = ph + (pm ? (uint32_t)pm[i] : 0u);
            a = mul15(saw_bl(p, inc1, rc1), sw) + mul15(sqr_bl(p, pw1, inc1, rc1) - dc1, uw);   /* (dc: a narrow pulse's offset taken out) */
            if (two) {
                ph1 += inc2;
                p2 = ph1 + (pm ? (uint32_t)pm[i] : 0u);
                b = mul15(saw_bl(p2, inc2, rc2), sw) + mul15(sqr_bl(p2, pw2, inc2, rc2) - dc2, uw);
                x = (a + b) >> 1;
                if (q->p4) x += mul15(mul15(a, b) - x, q->p4);   /* RING: channel I times channel II, the bell in a CS */
            } else x = a;
            x = mul15(x, norm);
            if (nz) { nr ^= nr << 13; nr ^= nr >> 17; nr ^= nr << 5; x += mul15((int32_t)(nr >> 17) - 16384, nz); }
            out[i] = clip(x, 32767);
        }
        o->ph[0] = ph; o->ph[1] = ph1; o->f[0] = nr;
        return;
    }
    case E_WAVE: {                              /* 8 frames, read with a warp of the phase */
        uint32_t pos = (uint32_t)q->p1 * 7u, fr = pos >> 15, fk = pos & 32767u, mip = inc > 34000000u;
        const int16_t *t0, *t1;
        uint32_t K = 0, r1 = 0, r2 = 0;
        if (fr > 6u) { fr = 6u; fk = 32767u; }
        t0 = KS_WT + (((mip * 8u + ((uint32_t)q->p2 >> 12)) * 8u + fr) << 8);
        t1 = t0 + 256;
        if (q->p3) {
            uint32_t k16 = 32768u - (((uint32_t)q->p3 * 29491u) >> 15);   /* the knee: 0.5 .. 0.05 */
            K = k16 << 16;
            r1 = (32768u << 16) / k16;
            r2 = (32768u << 16) / (65536u - k16);
        }
        for (i = 0; i < n; i++) {
            uint32_t p, ix, f;
            int32_t a, b;
            SYNC_STEP();
            ph += inc;
            p = ph + (pm ? (uint32_t)pm[i] : 0u);
            if (K) p = p < K ? (uint32_t)(((uint64_t)p * r1) >> 16) : 0x80000000u + (uint32_t)(((uint64_t)(p - K) * r2) >> 16);
            ix = p >> 24;
            f = (p >> 9) & 32767u;
            a = t0[ix] + (((t0[(ix + 1u) & 255u] - t0[ix]) * (int32_t)f) >> 15);
            b = t1[ix] + (((t1[(ix + 1u) & 255u] - t1[ix]) * (int32_t)f) >> 15);
            out[i] = a + (((b - a) * (int32_t)fk) >> 15);
        }
        break;
    }
    case E_FM: {                                /* 4 operators: ALGO (P4) sets who modulates whom; INDEX, the ratios (RATIO's set), FEEDBACK on the top one */
        int32_t ik = (int32_t)((((q->p1 * q->p1) >> 15) * 6366) >> 10), fk = (q->p3 * 978) >> 10;
        uint32_t al = (uint32_t)q->p4 >> 12, rk = (uint32_t)q->p2 >> 11, pm2 = o->ph2;
        uint32_t im = (uint32_t)(((uint64_t)inc * RATIO_Q8[rk]) >> 8);
        if (!al) {                              /* ALGO 1, 2>1: the classic pair, the feedback on the modulator */
            for (i = 0; i < n; i++) {
                int32_t m;
                pm2 += im;
                m = sin32(pm2 + (uint32_t)((((o->fb1 + o->fb2) >> 1) * fk)));
                o->fb2 = o->fb1;
                o->fb1 = m;
                SYNC_STEP();
                ph += inc;
                out[i] = sin32(ph + (pm ? (uint32_t)pm[i] : 0u) + (uint32_t)((int64_t)m * ik));
            }
        } else {
            uint32_t a = FM_ALG[al], p3 = o->ph[1], p4 = o->ph[2];
            uint32_t i3 = (uint32_t)(((uint64_t)inc * FM_R34[rk][0]) >> 8), i4 = (uint32_t)(((uint64_t)inc * FM_R34[rk][1]) >> 8);
            int32_t a43 = a >> 2 & 1, a42 = a >> 1 & 1, a41 = a & 1, a32 = a >> 4 & 1, a31 = a >> 3 & 1, a21 = a >> 6 & 1;
            int32_t c2 = !(a >> 6 & 1), c3 = !(a >> 3 & 3), c4 = !(a & 7), lv = a ? 32767 : q->p1;   /* (1+2+3+4: INDEX sets 2, 3, 4's level, drawbars) */
            int32_t g = a ? 32767 / (1 + c2 + c3 + c4) : (int32_t)(1073676289u / (uint32_t)(32767 + 3 * lv));
            for (i = 0; i < n; i++) {
                int32_t s4, s3, s2, s1;
                p4 += i4; p3 += i3; pm2 += im;
                s4 = sin32(p4 + (uint32_t)((((o->fb1 + o->fb2) >> 1) * fk)));
                o->fb2 = o->fb1;
                o->fb1 = s4;
                s3 = sin32(p3 + (uint32_t)((int64_t)(s4 * a43) * ik));
                s2 = sin32(pm2 + (uint32_t)((int64_t)(s4 * a42 + s3 * a32) * ik));
                SYNC_STEP();
                ph += inc;
                s1 = sin32(ph + (pm ? (uint32_t)pm[i] : 0u) + (uint32_t)((int64_t)(s4 * a41 + s3 * a31 + s2 * a21) * ik));
                out[i] = ((s1 + (int32_t)(((int64_t)(s2 * c2 + s3 * c3 + s4 * c4) * lv) >> 15)) * g) >> 15;
            }
            o->ph[1] = p3; o->ph[2] = p4;
        }
        o->ph2 = pm2;
        break;
    }
    case E_GROWL: {                             /* an FM pair (TYPE) driven by BITE into three formants that glide U O A E I (VOWEL) */
        const growl_t *gt = &GR_TYPE[((uint32_t)q->p4 >> 12) & 7u];
        int32_t ik = (int32_t)((((q->p1 * q->p1) >> 15) * 6366) >> 10), fk = gt->fb * 122, fc[3], j;
        int32_t pos = q->p2 * 4, vi = pos >> 15, fr = pos & 32767, qd = 11500 - ((q->p3 * 9500) >> 15);
        int32_t dg = 32768 + ((gt->drive * (8192 + (q->p3 >> 1))) >> 5), sl = gt->sub * 128, mk = 32768 + ((q->p3 * 3) >> 1);
        int32_t *lp = (int32_t *)&o->ph[1], *bp = (int32_t *)&o->ph[5], b3 = (int32_t)o->f[0];
        uint32_t im = (uint32_t)(((uint64_t)inc * gt->ratio) >> 8), pm2 = o->ph2, sp = o->ph[4];
        if (vi > 3) { vi = 3; fr = 32767; }
        int32_t l0 = clampi(lp[0], -40000, 40000), l1 = clampi(lp[1], -40000, 40000), l2 = clampi(lp[2], -40000, 40000);   /* (the states, 1/32 scale:
                                                 * 32-bit products, no clamp per sample; whatever another engine left in the slots, bounded) */
        int32_t b0 = clampi(bp[0], -40000, 40000), b1 = clampi(bp[1], -40000, 40000), b2 = clampi(b3, -40000, 40000);
        for (j = 0; j < 3; j++) {               /* the formants now: the vowel's, shifted by TYPE; 2 sin(pi f / fs), Q15 */
            int32_t hz = GR_VOW[vi][j] + (((GR_VOW[vi + 1][j] - GR_VOW[vi][j]) * fr) >> 15);
            hz = (hz * gt->shift) >> 7;
            if (hz > 7000) hz = 7000;
            fc[j] = sin32((uint32_t)hz * 48695u) * 2;
        }
        for (i = 0; i < n; i++) {
            int32_t m, c, x, y, hp;
            pm2 += im; sp += inc >> 1;
            m = sin32(pm2 + (uint32_t)((((o->fb1 + o->fb2) >> 1) * fk)));
            o->fb2 = o->fb1;
            o->fb1 = m;
            SYNC_STEP();
            ph += inc;
            c = sin32(ph + (pm ? (uint32_t)pm[i] : 0u) + (uint32_t)((int64_t)m * ik));
            x = sat15(mul15(c, dg)) >> 5;
            l0 += (fc[0] * b0) >> 15; hp = x - l0 - ((qd * b0) >> 15); b0 += (fc[0] * hp) >> 15;   /* (Chamberlin state variable, the band pass) */
            l1 += (fc[1] * b1) >> 15; hp = x - l1 - ((qd * b1) >> 15); b1 += (fc[1] * hp) >> 15;
            l2 += (fc[2] * b2) >> 15; hp = x - l2 - ((qd * b2) >> 15); b2 += (fc[2] * hp) >> 15;
            y = (((b0 * qd) >> 15) + ((((b1 * qd) >> 15) * 22000) >> 15) + ((((b2 * qd) >> 15) * 13000) >> 15)) * 32;
            y = mul15(y, mk) + (c >> 2) + (c >> 3) + ((sin32(sp) * sl) >> 15);   /* (BITE's narrower formants made up; the note itself under it, and TYPE's sub) */
            out[i] = sat15(y);
        }
        lp[0] = clampi(l0, -40000, 40000); lp[1] = clampi(l1, -40000, 40000); lp[2] = clampi(l2, -40000, 40000);
        bp[0] = clampi(b0, -40000, 40000); bp[1] = clampi(b1, -40000, 40000); b3 = clampi(b2, -40000, 40000);
        o->ph2 = pm2; o->ph[4] = sp; o->f[0] = (uint32_t)b3;
        break;
    }
    case E_ADD: {                               /* 16 partials on half the note (the 16' sub first), by recursion: sin(k+1) = 2 cos sin(k) - sin(k-1) */
        int32_t a[25];
        uint32_t k, hb = inc >> 1, top = add_amps(q->p1, q->p2, q->p3, ((uint32_t)q->p4 >> 12) & 7u, a), pb = o->ph2;
        uint32_t lim = 0x6C000000u / (hb + 1u);  /* (none past 0.42 of the sample rate) */
        if (top > lim) top = lim;
        for (i = 0; i < n; i++) {
            int32_t s0 = 0, s1, c, acc, sn;
            pb += hb;
            ph += inc;
            s1 = sin32(pb + (pm ? (uint32_t)pm[i] >> 1 : 0u));
            c = sin32(pb + (pm ? (uint32_t)pm[i] >> 1 : 0u) + 0x40000000u);
            acc = a[1] * s1;
            for (k = 2; k <= top; k++) { sn = ((c * s1) >> 14) - s0; s0 = s1; s1 = sn; acc += a[k] * s1; }
            out[i] = acc >> 15;
        }
        o->ph2 = pb;
        break;
    }
    case E_PHASE: {                             /* Casio CZ: a cosine read through a bent phase, or a windowed resonance */
        uint32_t ty = ((uint32_t)q->p2 * 6u) >> 15, k16, r1, r2, K, R;
        int32_t body = q->p3 >> 1;
        k16 = ty == 2u ? 65536u - (((uint32_t)q->p1 * 63000u) >> 15) : 32768u - (((uint32_t)q->p1 * 31500u) >> 15);
        K = ty == 2u && k16 >= 65536u ? 0xFFFFFFFFu : k16 << 16;
        r1 = ty == 2u ? 0xFFFFFFFFu / k16 : (32768u << 16) / k16;
        r2 = (32768u << 16) / (65536u - (k16 >= 65536u ? 65535u : k16));
        R = 256u + (((uint32_t)q->p1 * 3840u) >> 15);   /* the resonance ratio, Q8: 1 .. 16 */
        for (i = 0; i < n; i++) {
            uint32_t p, t2;
            int32_t x, w;
            ph += inc;
            p = ph + (pm ? (uint32_t)pm[i] : 0u);
            switch (ty) {
            case 0:
                t2 = p < K ? (uint32_t)(((uint64_t)p * r1) >> 16) : 0x80000000u + (uint32_t)(((uint64_t)(p - K) * r2) >> 16);
                x = -sin32(t2 + 0x40000000u);
                break;
            case 1: {
                uint32_t u = p & 0x7FFFFFFFu;
                t2 = u < K ? (uint32_t)(((uint64_t)u * r1) >> 16) : 0x80000000u;
                if (t2 > 0x80000000u) t2 = 0x80000000u;
                x = -sin32((p & 0x80000000u) + t2 + 0x40000000u);
                break;
            }
            case 2:
                t2 = p < K ? (uint32_t)(((uint64_t)p * r1) >> 16) : 0u;
                x = -sin32(t2 + 0x40000000u);
                break;
            default:
                w = ty == 3u ? 32767 - (int32_t)(p >> 17)
                  : ty == 4u ? (p < 0x80000000u ? (int32_t)(p >> 16) : (int32_t)((~p) >> 16)) : (p < 0x80000000u ? 32767 : (int32_t)((~p) >> 16));
                x = -mul15(sin32((uint32_t)(((uint64_t)p * R) >> 8) + 0x40000000u), w);
                break;
            }
            out[i] = x + mul15(sin32(p) - x, body);
        }
        break;
    }
    case E_SYNC: {                              /* a slave at 1..8 x reset by the note's phase */
        uint32_t R = 4096u + (((uint32_t)q->p1 * 7u) >> 3), is = (uint32_t)(((uint64_t)inc * R) >> 12), rs = blep_rcp(is), ps = o->ph2;
        int32_t k = q->p2, soft = q->p3;
        for (i = 0; i < n; i++) {
            uint32_t old = ph;
            int32_t a, b, x;
            ph += inc + (pm ? (uint32_t)pm[i] : 0u);
            ps += is;
            if (ph < old) ps = (uint32_t)(((uint64_t)ph * R) >> 12);
            a = saw_bl(ps, is, rs);
            b = ps < 0x80000000u ? -32767 : 32767;
            x = a + mul15(b - a, k);
            out[i] = x - mul15(x, mul15((int32_t)(ph >> 17), soft));
        }
        o->ph2 = ps;
        break;
    }
    case E_KICK: {                              /* 808: a sine that falls from PUNCH (up to 4 octaves above) back to the note over DROP
                                                 * (5 .. 300 ms), DRIVE (a soft clip, 1 to 8 times), CLICK (a 3 ms bite at the start);
                                                 * f[1] the drop (Q24), f[2] the click (Q15): set at every attack (note_on) */
        int32_t semi = q->p1 * 48, tau = 5 + (int32_t)(((int64_t)q->p2 * q->p2 * 295) >> 30);   /* (semi: Q15 x 48 = semitones Q15) */
        uint32_t r = (1u << 20) / ((uint32_t)tau * 44u + 1u), e0 = o->f[1], e1, i0, i1, a12, b12;
        int32_t g = 32768 + q->p3 * 7, cl = q->p4, ce = (int32_t)o->f[2], di;
        e1 = e0 - (uint32_t)(((uint64_t)e0 * r * n) >> 20);   /* (the drop at the block's end: one step, the block is short) */
        if (e1 > e0) e1 = 0;
        a12 = pitch_inc(48 * 256 + (int32_t)(((int64_t)(e0 >> 9) * semi) >> 22));   /* (Q8 semitones: up to 48 x 256 at the top of the drop) */
        b12 = pitch_inc(48 * 256);
        i0 = (uint32_t)(((uint64_t)inc * (a12 / ((b12 >> 12) | 1u))) >> 12);
        a12 = pitch_inc(48 * 256 + (int32_t)(((int64_t)(e1 >> 9) * semi) >> 22));
        i1 = (uint32_t)(((uint64_t)inc * (a12 / ((b12 >> 12) | 1u))) >> 12);
        di = ((int32_t)(i1 >> 4) - (int32_t)(i0 >> 4)) / (int32_t)n;   /* (from one to the other across the block) */
        for (i = 0; i < n; i++) {
            int32_t x;
            uint32_t p;
            ph += i0 + (uint32_t)(di * (int32_t)i) * 16u;
            p = ph + (pm ? (uint32_t)pm[i] : 0u);
            x = sin32(p);
            if (cl && ce > 0) { x += mul15(mul15(sin32(p * 7u + 0x40000000u), ce), cl); ce -= ce >> 6; if (ce < 64) ce = 0; }   /* (the click: 7 times up, gone in ~3 ms) */
            out[i] = q->p3 ? sat15(mul15(x, g) >> 0) : x;
        }
        o->f[1] = e1; o->f[2] = (uint32_t)ce;
        break;
    }
    default:                                    /* (1.0.26: FOLD and VOCAL gone: patch_sanitize moves the sounds; silence here) */
        for (i = 0; i < n; i++) out[i] = 0;
        break;
    }
    o->ph[0] = ph;
}
static void osc_render(osc_t *o, const oscp_t *q, int32_t *out, const int32_t *pm, uint32_t n)
{
    uint32_t i;
    osc_core(o, q, out, pm, n);
    if (q->p4 && q->eng != E_CS && q->eng != E_KICK && q->eng != E_FM && q->eng != E_GROWL && q->eng != E_ADD) {              /* FOLD: the engine's wave driven into a triangle folder, 1 to 6 times (west coast, after any engine) */
        int32_t g = 32768 + q->p4 * 5;
        for (i = 0; i < n; i++) {               /* (the wave as it is inside +-1; past it, reflected back, as often as it takes) */
            int32_t y = (int32_t)(((int64_t)out[i] * g) >> 15);
            while (y > 32767 || y < -32767) y = y > 32767 ? 65534 - y : -65534 - y;
            out[i] = y;
        }
    }
}

/* ================================================================ envelopes, LFOs */
typedef struct {
    uint8_t st;                                 /* 0 off, 1 attack, 2 decay, 3 sustain, 4 release */
    int32_t lv;                                 /* Q30 */
} env_t;
static void env_gate(env_t *e, int on) { e->st = on ? 1u : (e->st ? 4u : 0u); }
#define ANTICLICK ((1u << 30) / 6u)                /* the amp envelope's shortest attack: 6 control blocks of 16 samples, ~2.2 ms (the filter's: half) */
static int32_t env_run(env_t *e, uint32_t a, uint32_t d, int32_t s, uint32_t r, uint32_t slow)   /* s: Q30; returns Q15; slow: the shortest attack step (0: none) */
{
    switch (e->st) {
    case 1: {
        uint32_t st = KS_ENV_ATK[a];
        e->lv += (int32_t)(st < slow ? slow : st);
        if (e->lv >= (1 << 30)) { e->lv = 1 << 30; e->st = 2; }
        break;
    }
    case 2:
        e->lv = s + (int32_t)(((int64_t)(e->lv - s) * KS_ENV_DEC[d]) >> 24);
        if (e->lv - s < (1 << 16) && e->lv - s > -(1 << 16)) { e->lv = s; e->st = 3; }
        break;
    case 3:
        e->lv = s;
        break;
    case 4:
        e->lv = (int32_t)(((int64_t)e->lv * KS_ENV_DEC[r]) >> 24);
        if (e->lv < (1 << 18)) { e->lv = 0; e->st = 0; }
        break;
    default:
        e->lv = 0;
    }
    return e->lv >> 15;
}

typedef struct {
    uint32_t ph, b0, last;
    int32_t out, sh, sh0;
} lfo_t;
static lfo_t lfo[3];
static uint32_t lfo_key[3];                     /* a note asked the LFOs to start again */
static int32_t lfo_shape(lfo_t *l, uint32_t ph, uint32_t shape)
{
    switch (shape) {
    case 0: return sin32(ph);
    case 1: return tri32(ph + 0x40000000u);
    case 2: return 32767 - (int32_t)(ph >> 16);
    case 3: return (int32_t)(ph >> 16) - 32768;
    case 4: return ph < 0x80000000u ? 32767 : -32767;
    case 5: return l->sh;
    case 6: {                                   /* WUB: open at the start of the cycle, closing with a curve */
        int32_t t = 32767 - (int32_t)(ph >> 17);
        return (mul15(t, t) << 1) - 32767;
    }
    case 7: return (int32_t)(ph >> 30) * 21845 - 32767;
    default: return l->sh0 + mul15(l->sh - l->sh0, (int32_t)(ph >> 17));
    }
}
static void lfo_run(uint32_t k, uint32_t beats)
{
    lfo_t *l = &lfo[k];
    static int8_t l3p[L1_SMO + 1];               /* (LFO 3: its settings, from the song, where LFO 1's are) */
    const int8_t *p = k < 2u ? patch + k * LFO_2 : l3p;
    if (k >= 2u) {
        l3p[L1_SHP] = l3cfg[L3_SHP]; l3p[L1_SYNC] = l3cfg[L3_SYNC]; l3p[L1_RATE] = l3cfg[L3_RATE]; l3p[L1_DIV] = l3cfg[L3_DIV];
        l3p[L1_DEP] = l3cfg[L3_DEP]; l3p[L1_DST] = l3cfg[L3_DST]; l3p[L1_TRIG] = l3p[L1_PH] = l3p[L1_SMO] = 0;
    }
    uint32_t ph, off = (uint32_t)(uint8_t)p[L1_PH] << 25;
    int32_t raw, smo = p[L1_SMO];
    if (lfo_key[k]) {
        lfo_key[k] = 0;
        if (p[L1_TRIG]) { l->b0 = beats; l->ph = 0; }
    }
    if (p[L1_SYNC]) {
        uint32_t b = beats - (p[L1_TRIG] ? l->b0 : 0u);
        if (b >= BEATS_WRAP) b += BEATS_WRAP;   /* (wrapped below the start) */
        int32_t dv = clampi((k ? p[L1_DIV] : LKB(PB_WOB, p[L1_DIV])) + (k < 2u ? gmod[D_L1RATE + k] >> 8 : 0) * 15 / 127, 0, 15);
        if (ks_straight) dv = DIV_NEAR[dv];
        ph = (uint32_t)((uint64_t)(b >> 8) * DIV_CPB[dv]);
        l->ph = ph;
    } else
        ph = l->ph += KS_LFO_INC[clampi(p[L1_RATE] + (k < 2u ? gmod[D_L1RATE + k] >> 8 : 0), 0, 127)];
    ph += off;
    if (ph < l->last && l->last - ph > 0x40000000u) {   /* a new cycle: a new random value */
        l->sh0 = l->sh;
        l->sh = ((int32_t)(rnd() >> 17) - 16384) * 2;
    }
    l->last = ph;
    lfo_ph_show[k] = ph;
    raw = lfo_shape(l, ph, (uint32_t)p[L1_SHP]);
    l->out = smo ? l->out + mul15(raw - l->out, 32767 - ((smo * 255) >> 0)) : raw;
}

/* ================================================================ the voices */
typedef struct {
    uint8_t on, note, vel, gate, slide;         /* slide: the SEQ BASS glides into this note */
    uint32_t sm_pos, sm_g[2], sm_age[2];        /* OSC C: where in its sample (Q16); SPEED: its two grains, their age */
    int8_t sm_d, sm_gd[2];                      /* ... which way it reads (1, -1; 0: not started, 2: over), each grain's way (0: silent) */
    int32_t pitch, target;                      /* Q8 semitones (glide) */
    osc_t oa, ob;
    uint32_t phs;
    env_t ea, ef, em;
    int32_t y1, y2, y3, y4, g, amp;             /* the ladder, its g (Q16), the amp (Q15), both smoothed */
    int32_t b1, l1, b2, l2, b3, l3;             /* the vowel filter */
    int16_t cmb[CMB_N];
    uint32_t cw;
    int32_t drift_a, drift_b, rand, ncol;
    uint32_t age;
} voice_t;
static voice_t vo[NVOICE];
static uint8_t held[16], nheld;                 /* the keys held, the last on top (mono) */
static uint32_t vage;
#define SM_GRAIN 1024u                          /* SPEED: a grain, in samples played (two, half a grain apart) */
static inline int32_t sm_read(const uint8_t *d, uint32_t pos, uint32_t e)   /* OSC C: the sample at pos (Q16), between two */
{
    uint32_t ix = pos >> 16;
    int32_t a = KS_MULAW[d[ix]], b = ix + 1u < e ? KS_MULAW[d[ix + 1u]] : a;
    return a + mul15(b - a, (int32_t)((pos >> 1) & 32767u));
}
static int sm_adv(uint32_t *g, int32_t dir, uint32_t inc, uint32_t S, uint32_t E, uint32_t L, int loop)   /* a read position on, either way, inside [S, E) (Q16): 0 when over */
{
    if (dir > 0) {
        *g += inc;
        if (*g >= E) { if (!loop) return 0; *g = L + (*g - E) % (E - L); }
    } else if (*g < S + inc) {
        if (!loop) return 0;
        *g = E - 1u - (S + inc - *g) % (E - S);
    } else *g -= inc;
    return 1;
}
static uint8_t ks_slide;                        /* (SEQ BASS) the next note_on slides: no new attack, a glide */
static int32_t glide_from = -1;                 /* the last note started (Q8): where the next one glides from */

static void voice_start(voice_t *v, uint32_t note, uint32_t vel, int legato)
{
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->target = (int32_t)note << 8;
    v->slide = ks_slide && v->on && v->gate;
    if (!v->on && patch[V_GLIDE] && glide_from >= 0 && patch[V_MODE] != VM_LEGATO)
        v->pitch = glide_from;              /* (a voice that was silent glides from the last note played) */
    else if (!v->on || (!patch[V_GLIDE] && !v->slide) || (patch[V_MODE] == VM_LEGATO && !legato && !v->slide))
        v->pitch = v->target;               /* (MONO and POLY glide always, LEGATO only between held keys) */
    glide_from = v->target;
    v->age = ++vage;
    if (legato && (patch[V_MODE] == VM_LEGATO || v->slide) && v->on && v->gate) return;   /* legato: no new attack */
    v->oa.f[1] = v->ob.f[1] = 1u << 24;          /* (808: the drop and the click start again) */
    v->oa.f[2] = v->ob.f[2] = 32767u;
    if (patch[A_ENG] == E_KICK) v->oa.ph[0] = 0;   /* (and its sine from 0: the same punch every time) */
    if (patch[B_ENG] == E_KICK) v->ob.ph[0] = 0;
    if (patch[X_RESET] && (!v->on || !v->ea.lv)) {
        uint32_t k;
        for (k = 0; k < 7u; k++) v->oa.ph[k] = v->ob.ph[k] = k * 0x24924924u;
        v->oa.ph[0] = v->ob.ph[0] = 0;
        v->oa.ph2 = v->ob.ph2 = 0;
        v->phs = 0;
    } else if (patch[S_LOCK] && !patch[A_SEMI] && (!v->on || !v->ea.lv)) {   /* (SUB LOCK, A not reset: the sub takes A's phase, down its octaves; only for a voice that was silent: a note over a note jumped the sub's phase, a click; 1.0.41) */
        int32_t sh = 2 - patch[S_OCT] + patch[A_OCT];
        if (sh >= 0 && sh < 8) v->phs = v->oa.ph[0] >> sh;
    }
    if (!v->on) { v->y1 = v->y2 = v->y3 = v->y4 = 0; v->b1 = v->l1 = v->b2 = v->l2 = v->b3 = v->l3 = 0; }
    v->sm_pos = 0;
    v->sm_g[0] = v->sm_g[1] = 0;
    v->sm_age[0] = 0;
    v->sm_age[1] = SM_GRAIN / 2u;
    v->sm_d = 0;
    v->on = 1;
    v->gate = 1;
    v->rand = ((int32_t)(rnd() >> 17) - 16384) * 2;
    env_gate(&v->ea, 1);
    env_gate(&v->ef, 1);
    env_gate(&v->em, 1);
    lfo_key[0] = lfo_key[1] = 1;
}
static void voice_release(voice_t *v)
{
    v->gate = 0;
    env_gate(&v->ea, 0);
    env_gate(&v->ef, 0);
    env_gate(&v->em, 0);
}

static void note_on(uint32_t n, uint32_t vel)
{
    uint32_t i, best = 0;
    if (patch[V_MODE] != VM_POLY) {
        int legato = nheld > 0;
        for (i = 0; i < nheld && held[i] != n; i++) {}
        if (i < nheld) { for (; i + 1u < nheld; i++) held[i] = held[i + 1u]; nheld--; }
        if (nheld < 16u) held[nheld++] = (uint8_t)n;
        for (i = 1; i < NVOICE; i++) if (vo[i].on) voice_release(&vo[i]);
        voice_start(&vo[0], n, vel, legato && vo[0].gate);
        return;
    }
    for (i = 0; i < NVOICE; i++) if (vo[i].on && vo[i].note == n) { best = i; goto go; }
    for (i = 0; i < NVOICE; i++) if (!vo[i].on) { best = i; goto go; }
    for (i = 1; i < NVOICE; i++) if (vo[i].age < vo[best].age) best = i;   /* the oldest gives way */
go:
    voice_start(&vo[best], n, vel, 0);
}
static void note_off(uint32_t n)
{
    uint32_t i;
    if (patch[V_MODE] != VM_POLY) {
        for (i = 0; i < nheld && held[i] != n; i++) {}
        if (i >= nheld) return;
        for (; i + 1u < nheld; i++) held[i] = held[i + 1u];
        nheld--;
        if (vo[0].note != n) return;
        if (nheld) {                            /* back to the key still held, gliding (legato) */
            vo[0].target = (int32_t)held[nheld - 1u] << 8;
            vo[0].note = held[nheld - 1u];
            if (!patch[V_GLIDE]) vo[0].pitch = vo[0].target;
        } else
            voice_release(&vo[0]);
        return;
    }
    for (i = 0; i < NVOICE; i++)
        if (vo[i].on && vo[i].gate && vo[i].note == n) voice_release(&vo[i]);
}

/* the modulation of one voice for one block: param units x 256 (or Q8 semitones for the pitch) */
static void route(int32_t *m, const int32_t *src);
static void voice_mods(voice_t *v, int32_t *m, int32_t fenv, int32_t menv)
{
    int32_t src[NSRC];
    src[SR_OFF] = 0;
    src[SR_LFO1] = lfo[0].out;
    src[SR_LFO2] = lfo[1].out;
    src[SR_FENV] = fenv;
    src[SR_MENV] = menv;
    src[SR_VEL] = v->vel * 258;
    src[SR_WHEEL] = ks_wheel;
    src[SR_PRESS] = ks_press;
    src[SR_NOTE] = clip(((int32_t)v->note - 60) * 512, 32767);
    src[SR_RAND] = v->rand;
    src[SR_STEP] = step_val;
    src[SR_LFO3] = lfo[2].out;
    route(m, src);
    m[D_CUT] += ((LKB(PB_ENV, patch[F_ENV]) + (m[D_FENV] >> 8)) * fenv) >> 6;
    m[D_CUT] += (int32_t)((((int64_t)(v->pitch - 60 * 256) * 276) >> 8) * GP(D_FKEY) >> 7);
    m[D_CUT] += (GP(D_FVEL) * (v->vel - 64)) >> 1;
}
/* the matrix, the two LFOs, the STEP effect and the mod envelope into m (param units x 256, Q8 semitones) */
static void route(int32_t *m, const int32_t *src)
{
    int32_t wob;
    uint32_t k;
    for (k = 0; k < ND; k++) m[k] = 0;
    for (k = 0; k < 4u; k++) {                  /* the matrix, then the LFOs (their depth can be a destination) */
        const int8_t *s = patch + M1_SRC + 3u * k;
        if (s[0] && s[1]) {
            int32_t a = s[2];
            if (IS_PITCH(s[1])) m[s[1]] += (int32_t)(((int64_t)(a < 0 ? -a * a : a * a) * 3 * src[s[0]]) >> 15);
            else m[s[1]] += (a * src[s[0]]) >> 6;
        }
    }
    wob = m[D_WOB] >> 8;
    for (k = 0; k < 2u; k++) {
        const int8_t *p = patch + k * LFO_2;
        int32_t d = clampi(p[L1_DEP] + (k == 0u ? wob : m[D_L2DEP] >> 8), -64, 64), dst = p[L1_DST];   /* (LFO 1 can move LFO 2) */
        if (!dst || !d) continue;
        if (IS_PITCH(dst)) m[dst] += (int32_t)(((int64_t)(d < 0 ? -d * d : d * d) * 3 * lfo[k].out) >> 15);
        else m[dst] += (d * lfo[k].out) >> 6;
    }
    if (l3cfg[L3_DEP] && l3cfg[L3_DST] > 0 && l3cfg[L3_DST] < ND) {   /* LFO 3 */
        int32_t d = l3cfg[L3_DEP], dst = l3cfg[L3_DST];
        if (IS_PITCH(dst)) m[dst] += (int32_t)(((int64_t)(d < 0 ? -d * d : d * d) * 3 * lfo[2].out) >> 15);
        else m[dst] += (d * lfo[2].out) >> 6;
    }
    if (patch[ST_ON] && patch[ST_DST] && GP(D_STDEP)) {   /* the STEP effect's own destination */
        int32_t a = GP(D_STDEP);
        if (IS_PITCH(patch[ST_DST])) m[patch[ST_DST]] += (int32_t)(((int64_t)(a < 0 ? -a * a : a * a) * 3 * step_val) >> 15);
        else m[patch[ST_DST]] += (a * step_val) >> 6;
    }
    if (patch[ME_DST] && GP(D_MEAMT)) {
        int32_t a = GP(D_MEAMT);
        if (IS_PITCH(patch[ME_DST])) m[patch[ME_DST]] += (int32_t)(((int64_t)(a < 0 ? -a * a : a * a) * 3 * src[SR_MENV]) >> 15);
        else m[patch[ME_DST]] += (a * src[SR_MENV]) >> 6;
    }
    if (perf.mc_magic == MC_MAGIC)              /* the MACROS: each knob onto its four targets */
        for (k = 0; k < 16u; k++) {
            int32_t mv = mc_val[k >> 2], a = perf.mc_amt[k], d = perf.mc_dst[k];
            if (!mv || !a || !d || d >= ND) continue;
            if (IS_PITCH(d)) m[d] += (int32_t)(((int64_t)(a < 0 ? -a * a : a * a) * 3 * (mv * 258)) >> 15);
            else m[d] += (a * mv * 258) >> 6;
        }
}
/* the global destinations (the effects, the rates, the gate): once a block, the newest note's envelopes */
static void fx_mods(void)
{
    int32_t src[NSRC], m[ND];
    uint32_t k;
    voice_t *v = 0;
    for (k = 0; k < NVOICE; k++) if (vo[k].on && vo[k].age == vage) v = &vo[k];
    src[SR_OFF] = 0;
    src[SR_LFO1] = lfo[0].out;
    src[SR_LFO2] = lfo[1].out;
    src[SR_FENV] = v ? v->ef.lv >> 15 : 0;
    src[SR_MENV] = v ? v->em.lv >> 15 : 0;
    src[SR_VEL] = v ? v->vel * 258 : 0;
    src[SR_WHEEL] = ks_wheel;
    src[SR_PRESS] = ks_press;
    src[SR_NOTE] = v ? clip(((int32_t)v->note - 60) * 512, 32767) : 0;
    src[SR_RAND] = v ? v->rand : 0;
    src[SR_STEP] = step_val;
    src[SR_LFO3] = lfo[2].out;
    route(m, src);
    for (k = D_G0; k < ND; k++) gmod[k] = m[k];
}
static int32_t fpv(uint32_t id, uint32_t d)     /* a parameter of the effects, modulated */
{
    return clampi(patch[id] + (gmod[d] >> 8), PD[id].lo, PD[id].hi);
}
static inline int32_t pv(int32_t base, int32_t mod) { return clampi(base * 256 + mod, 0, 127 * 256); }   /* -> Q8 units */
static int32_t cut_s = -1;                      /* CUTOFF as turned (and its lock), Q8, glided over ~15 ms: no steps heard (1.0.35) */
static void cut_glide(void)                     /* (once per control block, before the voices) */
{
    int32_t t = LKB(PB_CUT, patch[F_CUT]) * 256, d, st;
    if (cut_s < 0) { cut_s = t; return; }
    d = t - cut_s;
    st = (d * 3) >> 7;
    if (!st && d) st = d > 0 ? 1 : -1;
    cut_s += st;
}
static inline int32_t q8to15(int32_t q8) { return q8 + (q8 >> 7); }

static const int8_t FT_MIX[5][5] = {{0, 0, 0, 0, 1}, {0, 0, 1, 0, 0}, {0, 2, -2, 0, 0}, {1, -4, 6, -4, 1}, {1, -2, 2, 0, 0}};

/* one voice, one control block, added into acc */
static void voice_block(voice_t *v, int32_t *acc)
{
    int32_t m[ND], fenv, menv, amp, p, bend, bufa[KS_CB], bufb[KS_CB], pmb[KS_CB], s[KS_CB], smb[KS_CB], cut, res, drv, la, lb, lsub, ln, i;
    oscp_t qa, qb;
    const int8_t *P = patch;
    uint32_t inc_sub, rsub, ft = (uint32_t)P[F_TYPE];
    int32_t amp_t, g_t, dg, da, gain;

    fenv = env_run(&v->ef, (uint32_t)GP(D_FEA), (uint32_t)GP(D_FED), GP(D_FES) << 23, (uint32_t)GP(D_FER), ANTICLICK / 2u);   /* (GP: the LFOs can move them) */
    menv = env_run(&v->em, (uint32_t)GP(D_MEA), (uint32_t)GP(D_MED), 0, (uint32_t)GP(D_MED), 0u);
    amp = env_run(&v->ea, (uint32_t)GP(D_AEA), (uint32_t)GP(D_AED), GP(D_AES) << 23, (uint32_t)GP(D_AER), ANTICLICK);   /* (never faster than ~2.2 ms: a hard attack clicked; 1.0.42) */
    if (!v->ea.st && !v->ea.lv) { v->on = 0; v->amp = 0; return; }
    if (GP(D_GLIDE) || v->slide)                /* (a SEQ slide without GLIDE: ~60 ms) */
    {                                           /* (the time shown; the gap rounded toward the note: a glide up stopped ~0.2 semitone flat, 1.0.52) */
        int64_t d = (int64_t)(v->pitch - v->target), g = KS_GLIDE_DEC[GP(D_GLIDE) ? GP(D_GLIDE) : 50];
        d = d < 0 ? -((-d * g) >> 24) : (d * g) >> 24;
        v->pitch = v->target + (int32_t)d;
    }
    else v->pitch = v->target;
    voice_mods(v, m, fenv, menv);
    if (GP(D_DRIFT)) {                          /* slow wandering of each oscillator */
        v->drift_a = clip(v->drift_a + ((int32_t)(rnd() >> 24) - 128) - (v->drift_a >> 6), 4000);
        v->drift_b = clip(v->drift_b + ((int32_t)(rnd() >> 24) - 128) - (v->drift_b >> 6), 4000);
    }
    bend = (int32_t)(((int64_t)ks_bend * P[V_BEND]) >> 7);
    p = v->pitch + m[D_PITCH] + bend;

    qa.eng = (uint8_t)P[A_ENG];
    qa.p1 = q8to15(pv(LKB(PB_SHAPE, P[A_P1]), m[D_AP1]));
    qa.p2 = q8to15(pv(P[A_P2], m[D_AP2]));
    qa.p3 = q8to15(pv(P[A_P3], m[D_AP3]));
    qa.p4 = GP(D_AP4) * 258;
    qa.inc = pitch_inc(p + m[D_APIT] + ((P[A_OCT] * 12 + P[A_SEMI]) << 8) + P[A_FINE] * 256 / 100 + ((v->drift_a * GP(D_DRIFT)) >> 9));
    qb.eng = (uint8_t)P[B_ENG];
    qb.p1 = q8to15(pv(P[B_P1], m[D_BP1]));
    qb.p2 = q8to15(pv(P[B_P2], m[D_BP2]));
    qb.p3 = q8to15(pv(P[B_P3], m[D_BP3]));
    qb.p4 = GP(D_BP4) * 258;
    qb.inc = pitch_inc(p + m[D_BPIT] + ((P[B_OCT] * 12 + P[B_SEMI]) << 8) + P[B_FINE] * 256 / 100 + ((v->drift_b * GP(D_DRIFT)) >> 9)
                       + (P[X_MODE] == 3 ? (pv(P[X_AMT], m[D_XMOD]) >> 8) * 6144 / 127 : 0));   /* (SYNC: X-MOD = +0..24 semitones) */
    qa.sync = 0;
    qb.sync = P[X_MODE] == 3;
    if (qb.sync) { qb.mph = v->oa.ph[0]; qb.minc = qa.inc; qb.mrq = qb.inc / ((qa.inc >> 8) + 1u); }
    inc_sub = pitch_inc(p - (2 - P[S_OCT]) * 12 * 256);
    if (P[S_LOCK] && !P[A_SEMI]) {              /* SUB LOCK: the sub is OSC A an octave or two down, its drift and fine included */
        int32_t sh = 2 - P[S_OCT] + P[A_OCT];
        if (sh >= 0 && sh < 8) inc_sub = qa.inc >> sh;
    }
    la = lvl15(pv(P[A_LVL], m[D_ALVL]) >> 8);
    lb = lvl15(pv(P[B_LVL], m[D_BLVL]) >> 8);
    if (m[D_ABMIX]) {                           /* A/B MIX moved: up, toward B (A down); down, toward A (B down) */
        int32_t bal = clampi(m[D_ABMIX] >> 8, -127, 127);
        if (bal > 0) la = la * (127 - bal) / 127; else lb = lb * (127 + bal) / 127;
    }
    lsub = lvl15(pv(P[S_LVL], m[D_SUB]) >> 8);
    ln = lvl15(pv(P[N_LVL], m[D_NOISE]) >> 8);

    /* the oscillators: B first (it can modulate A) */
    if (lb || P[X_MODE]) osc_render(&v->ob, &qb, bufb, 0, KS_CB);
    else for (i = 0; i < (int32_t)KS_CB; i++) bufb[i] = 0;
    if (P[X_MODE] == 2) {
        int32_t xk = (pv(P[X_AMT], m[D_XMOD]) >> 8) * 650;
        for (i = 0; i < (int32_t)KS_CB; i++) pmb[i] = (int32_t)(uint32_t)((int64_t)bufb[i] * xk);
    }
    if (la || P[X_MODE] == 1) osc_render(&v->oa, &qa, bufa, P[X_MODE] == 2 ? pmb : 0, KS_CB);
    else {
        for (i = 0; i < (int32_t)KS_CB; i++) bufa[i] = 0;
        if (qb.sync) v->oa.ph[0] += qa.inc * KS_CB;   /* (A silent still leads B) */
    }
    {
        int32_t ncoef = 32767 - (pv(P[N_COL], m[D_NCOL]) >> 8) * 250, rm = P[X_MODE] == 1;
        for (i = 0; i < (int32_t)KS_CB; i++) {
            int32_t x = mul15(bufa[i], la), b = rm ? mul15(bufa[i], bufb[i]) : bufb[i];
            x += mul15(b, lb);
            if (ln) {
                v->ncol += mul15((int32_t)(rnd() >> 17) - 16384 - v->ncol, ncoef);
                x += mul15(v->ncol << 1, ln);
            }
            s[i] = x >> 1;
        }
    }
    for (i = 0; i < (int32_t)KS_CB; i++) smb[i] = 0;
    if (P[SM_SEL] && GP(D_SMLVL) && (P[SM_SEL] != SM_RS || rs_desc.len)) {   /* OSC C: the sample (after the filter, as the sub), from the keys (C3 as made) or fixed */
        const ks_smp_t *sm = sm_desc(P[SM_SEL]);
        const uint8_t *d = P[SM_SEL] == SM_RS ? rs_buf : KS_SMP_DATA + sm->off;
        int keys = P[SM_KEY] == 1 || (P[SM_KEY] == 0 && sm->keys);
        uint32_t st = (uint32_t)(((uint64_t)pitch_inc((keys ? p : 48 * 256) + GP(D_SMTUNE) * 256) * sm->k) >> 32);
        uint32_t len = sm->len, s0, e0, l0, S, E, L, hinc, sp, dir = (uint32_t)P[SM_DIR] % 3u;
        int loop = P[SM_MODE] == 1 || (P[SM_MODE] == 0 && sm->loop < len);
        int32_t lv = lvl15(pv(GP(D_SMLVL), 0) >> 8);
        s0 = (uint32_t)LKB(PB_SMP, GP(D_SMSTART)) * len >> 7;  /* START / END: the part played */
        e0 = len - ((uint32_t)GP(D_SMEND) * len >> 7);
        if (e0 < s0 + 32u) e0 = s0 + 32u < len ? s0 + 32u : len;
        if (s0 + 32u > e0) s0 = e0 > 32u ? e0 - 32u : 0u;
        l0 = sm->loop < len ? sm->loop : 0u;    /* (the loop: from the sample's loop point, inside the part) */
        if (l0 < s0 || l0 >= e0) l0 = s0;
        S = s0 << 16; E = e0 << 16; L = l0 << 16;
        { int32_t spd = GP(D_SMSPEED); sp = spd >= 0 ? 256u - (uint32_t)spd * 230u / 127u : 256u + (uint32_t)(-spd) * 12u; }   /* SPEED, Q8: 0.1 .. 1 .. 4 (the same pitch) */
        if (st > (8u << 16)) st = 8u << 16;
        hinc = sp == 256u ? st : (uint32_t)(((uint64_t)st * sp) >> 8);
        if (!v->sm_d) { v->sm_d = dir == 1u ? -1 : 1; v->sm_pos = v->sm_g[0] = v->sm_g[1] = dir == 1u ? E - 1u : S; v->sm_gd[0] = v->sm_gd[1] = v->sm_d; }   /* (the grain half way in: the attack at once) */
        for (i = 0; i < (int32_t)KS_CB && v->sm_d != 2; i++) {
            uint32_t h;
            int32_t acc = 0;
            if (v->sm_pos < S || v->sm_pos >= E) v->sm_pos = v->sm_d < 0 ? E - 1u : S;   /* (START / END turned while it plays) */
            if (sp == 256u) smb[i] = mul15(sm_read(d, v->sm_pos, e0), lv);   /* (full speed: read straight) */
            else {
                for (h = 0; h < 2u; h++) {      /* slower or faster: two grains read at the pitch, each restarting where the play is */
                    uint32_t age = v->sm_age[h];
                    if (age >= SM_GRAIN) { v->sm_age[h] = age = 0; v->sm_g[h] = v->sm_pos; v->sm_gd[h] = v->sm_d; }
                    v->sm_age[h]++;
                    if (!v->sm_gd[h]) continue;
                    acc += mul15(sm_read(d, v->sm_g[h], e0), (int32_t)((age < SM_GRAIN / 2u ? age : SM_GRAIN - age) << 6));   /* (a triangle) */
                    if (!sm_adv(&v->sm_g[h], v->sm_gd[h], st, S, E, v->sm_gd[h] > 0 ? L : S, loop && dir != 2u)) v->sm_gd[h] = 0;
                }
                smb[i] = mul15(acc, lv);
            }
            if (dir == 2u) {                    /* PING-PONG: there and back (ONCE: one round) */
                if (v->sm_d > 0) {
                    v->sm_pos += hinc;
                    if (v->sm_pos >= E) { v->sm_pos = E - 1u - (v->sm_pos - E) % (E - S); v->sm_d = -1; }
                } else if (v->sm_pos < S + hinc) {
                    if (!loop) v->sm_d = 2;
                    else { v->sm_pos = S + (S + hinc - v->sm_pos) % (E - S); v->sm_d = 1; }
                } else v->sm_pos -= hinc;
            } else if (!sm_adv(&v->sm_pos, v->sm_d, hinc, S, E, dir == 1u ? S : L, loop)) v->sm_d = 2;
        }
        if (v->age == vage) sm_show = v->sm_pos;
    }

    /* the filter */
    cut = clampi((cut_s < 0 ? LKB(PB_CUT, P[F_CUT]) * 256 : cut_s) + m[D_CUT], 0, 127 * 256);
    if (v->age == vage) {                      /* (the newest voice: what the screen shows) */
        env_st_show[0] = v->ea.st; env_lv_show[0] = v->ea.lv >> 15;
        env_st_show[1] = v->ef.st; env_lv_show[1] = v->ef.lv >> 15;
        env_st_show[2] = v->em.st; env_lv_show[2] = v->em.lv >> 15;
        cut_show = cut;
    }
    res = pv(LKB(PB_RES, P[F_RES]), m[D_RES]) >> 8;
    drv = pv(LKB(PB_DRV, P[F_DRV]), m[D_DRV]) >> 8;
    gain = 4096 + drv * 160;
    if (ft <= FT_NOTCH) {
        const int8_t *c = FT_MIX[ft];
        int32_t k = res * 125, comp = ft <= FT_LP12 || ft == FT_NOTCH ? 4096 + (k >> 1) : 4096,   /* (NOTCH too: it lost ~7.5 dB at RES 100; 1.0.47) */ x0 = cut >> 7;
        g_t = KS_CUT_G[x0] + (((KS_CUT_G[x0 + 1] - KS_CUT_G[x0]) * (cut & 127)) >> 7);
        if (res > 56) { int32_t cap = 60493 - (res - 56) * 120; if (g_t > cap) g_t = cap; }   /* (the top of the cutoff with a high RESONANCE: the ladder rang at fs/2, a 22 kHz crackle at a note's filter-envelope peak; 1.0.41) */
        if (!v->g) v->g = g_t;
        dg = (g_t - v->g) / (int32_t)KS_CB;
        for (i = 0; i < (int32_t)KS_CB; i++) {
            int32_t x = (int32_t)(((int64_t)s[i] * gain) >> 12), u;   /* (64-bit: the macros can push the mix past 2^17) */
            if (drv) x = sat15(x);
            v->g += dg;
            u = sat15(((x * comp) >> 12) - (int32_t)(((int64_t)v->y4 * k) >> 12));
            v->y1 += (int32_t)(((int64_t)(u - v->y1) * v->g) >> 16);
            v->y2 += (int32_t)(((int64_t)(v->y1 - v->y2) * v->g) >> 16);
            v->y3 += (int32_t)(((int64_t)(v->y2 - v->y3) * v->g) >> 16);
            v->y4 += (int32_t)(((int64_t)(v->y3 - v->y4) * v->g) >> 16);
            s[i] = (c[0] * u + c[1] * v->y1 + c[2] * v->y2 + c[3] * v->y3 + c[4] * v->y4) << 1;
        }
        v->g = g_t;
    } else if (ft == FT_VOWEL) {                /* two formants and a third, the cutoff walks A E I O U */
        uint32_t w = (uint32_t)q8to15(cut) * 4u, vi = w >> 15, vk = w & 32767u, k;
        int32_t f[3], a[3], q = 32768 - res * 230;
        if (vi > 3u) { vi = 3u; vk = 32767u; }
        for (k = 0; k < 3u; k++) {
            int32_t hz = VOX_F[vi][k] + ((((int32_t)VOX_F[vi + 1u][k] - VOX_F[vi][k]) * (int32_t)vk) >> 15);
            f[k] = sin32((uint32_t)hz * 48696u) << 1;
            a[k] = VOX_A[vi][k] + ((((int32_t)VOX_A[vi + 1u][k] - VOX_A[vi][k]) * (int32_t)vk) >> 15);
        }
        for (i = 0; i < (int32_t)KS_CB; i++) {
            int32_t x = (int32_t)(((int64_t)s[i] * gain) >> 12), h;
            if (drv) x = sat15(x);
            h = x - v->l1 - mul15(v->b1, q); v->b1 = clip(v->b1 + mul15(f[0], h), 200000); v->l1 += mul15(f[0], v->b1);
            h = x - v->l2 - mul15(v->b2, q); v->b2 = clip(v->b2 + mul15(f[1], h), 200000); v->l2 += mul15(f[1], v->b2);
            h = x - v->l3 - mul15(v->b3, q); v->b3 = clip(v->b3 + mul15(f[2], h), 200000); v->l3 += mul15(f[2], v->b3);
            v->l1 = clip(v->l1, 200000);
            s[i] = (mul15(v->b1, a[0]) + mul15(v->b2, a[1]) + mul15(v->b3, a[2])) * 3 / 2 + (v->l1 >> 2);
        }
    } else {                                    /* COMB: a line tuned by the cutoff (with KEY TRACK: the note) */
        int32_t hz = KS_CUT_HZ[cut >> 7], L, fb = res * 254;
        if (hz < 44) hz = 44;
        L = (int32_t)((44100u << 8) / (uint32_t)hz);
        if (L > (int32_t)(CMB_N - 2u) << 8) L = (int32_t)(CMB_N - 2u) << 8;
        if (L < 2 << 8) L = 2 << 8;
        for (i = 0; i < (int32_t)KS_CB; i++) {
            int32_t x = (int32_t)(((int64_t)s[i] * gain) >> 12), d0, d1, y;
            uint32_t r = (v->cw - (uint32_t)(L >> 8)) & (CMB_N - 1u);
            if (drv) x = sat15(x);
            d0 = v->cmb[r];
            d1 = v->cmb[(r - 1u) & (CMB_N - 1u)];
            y = x + mul15(d0 + (((d1 - d0) * (L & 255)) >> 8), fb);
            y = sat15(y);
            v->cmb[v->cw & (CMB_N - 1u)] = (int16_t)y;
            v->cw++;
            s[i] = (x + y);
        }
    }

    /* the sub (direct or into the filter: here after it, so it stays clean), the amp */
    rsub = blep_rcp(inc_sub);
    amp_t = mul15(amp, 32767 - mul15(P[V_VEL] * 258, 32767 - v->vel * 258));
    amp_t = clip(amp_t + (m[D_AMP] >> 1), 32767);
    if (amp_t < 0) amp_t = 0;
    da = (amp_t - v->amp) / (int32_t)KS_CB;
    for (i = 0; i < (int32_t)KS_CB; i++) {
        int32_t x = s[i] + smb[i], sb;
        v->phs += inc_sub;
        if (lsub) {
            switch (P[S_WAVE]) {
            case 0: sb = sin32(v->phs); break;
            case 1: sb = tri32(v->phs + 0x40000000u); break;
            case 2: sb = sqr_bl(v->phs, 0x80000000u, inc_sub, rsub) * 3 / 4; break;
            default: sb = sqr_bl(v->phs, 0x40000000u, inc_sub, rsub) * 3 / 4; break;
            }
            x += mul15(sb, lsub);
        }
        v->amp += da;
        acc[i] += mul15(x, v->amp);
    }
    v->amp = amp_t;
}

/* the STEP effect: the pattern's level at this step of the beat grid, smoothed (once a control block) */
static void step_run(uint32_t beats)
{
    static int32_t sv;
    uint32_t spb = STEP_PB[clampi(patch[ST_RATE], 0, 5)], n = (uint32_t)(((uint64_t)(beats >> 8) * spb) >> 16), pos = n & 15u;
    int32_t raw, smo = patch[ST_SMO];
    char c;
    if (!patch[ST_ON]) { step_val = 32767; sv = 32767; return; }
    if (patch[ST_PAT] == 15) {                  /* RANDOM: a hash of the step and the bar */
        uint32_t h = (n >> 4) * 2654435761u ^ pos * 40503u;
        h ^= h >> 13;
        raw = (int32_t)(((h * 0x9E3779B1u) >> 28) & 15u) * 2184;
    } else {
        c = STEP_PAT[patch[ST_PAT] & 15][pos];
        raw = (c <= '9' ? c - '0' : c - 'A' + 10) * 2184;
    }
    sv = smo ? sv + mul15(raw - sv, 32767 - smo * 255) : raw;
    step_val = sv;
    step_pos = (uint8_t)pos;
}


/* ================================================================ SEQ BASS and ARP (the voices' core, at each KS_CB)
 * SEQ BASS: 16 patterns of up to 64 steps, each a note (C1 + 0..48), on or a rest, an accent, a slide into the next
 * (no new attack, a glide), a chance (%) and a condition: 1/2..4/4 (that time round in 2 or 4), START (the first time
 * round only), NOT START, IF BEFORE (the step before played), IF AFTER (the step after plays: drawn a step ahead).
 * While it plays the keys transpose it (C4 = as written) and do not sound.
 * ARP: the keys held (or HOLD) played one at a time on the tempo: UP, DOWN, UP-DN, RANDOM, ORDER, over 1..4
 * octaves. Both run on ks_beats: on the MIDI clock with the LFOs. */
static volatile uint8_t sq_play, sq_rec;        /* playing (the voices say), step record (the main loop says) */
static volatile uint8_t sq_show = 0xFF;         /* the step playing (the screen) */
static int32_t sq_tr;                           /* the keys' transposition */
static uint8_t sq_note = 0xFF, sq_slide, ar_held[16], ar_n, ar_was;
static uint32_t sq_tick = 0xFFFFFFFFu, sq_step, sq_glen, ar_i;   /* sq_glen: what is left of the note, from this step's start (Q24 steps) */
static uint32_t sq_at;                          /* (the control block's place in the audio block: a drum hit lands there) */
static uint32_t sq_pass, sq_nx_i = 0xFFFFFFFFu;  /* the times round since PLAY; the next step drawn (pattern << 8 | step) */
static uint8_t sq_last, sq_nx_ok;               /* the last step played; the next one's draw */
static uint32_t sq_draw(uint32_t k, uint32_t i, uint32_t pass)   /* on, its chance, its condition (IF BEFORE / AFTER: later) */
{
    uint32_t c = (sqx.p[k].cond[i] & 15u) % NSC, pr = sqx.p[k].prob[i];
    if (!(*sq_fp(k, i) & SF_ON)) return 0;
    if (pr < 100u && (rnd() >> 8) % 100u >= pr) return 0;
    if (c >= SC_1_2 && c <= SC_2_2) return pass % 2u == c - SC_1_2;
    if (c >= SC_1_4 && c <= SC_4_4) return pass % 4u == c - SC_1_4;
    if (c == SC_START) return pass == 0;
    if (c == SC_NSTART) return pass != 0;
    return 1;
}
static void sq_off(void) { if (sq_note != 0xFF) note_off(sq_note); sq_note = 0xFF; sq_slide = 0; }
static void sq_sound(uint32_t n, uint32_t vel, int slide_in, uint32_t gate)   /* (a slide: the new one, then the old one off) */
{
    uint8_t old = sq_note;
    ks_slide = (uint8_t)slide_in;
    note_on(n, vel);
    ks_slide = 0;
    if (old != 0xFF && old != n) note_off(old);
    sq_note = (uint8_t)n;
    sq_glen = gate;
}
static void ar_add(uint32_t n) { uint32_t i; for (i = 0; i < ar_n; i++) if (ar_held[i] == n) return; if (ar_n < 16u) ar_held[ar_n++] = (uint8_t)n; }
static void ar_del(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < ar_n && ar_held[i] != n; i++) {}
    if (i >= ar_n) return;
    for (; i + 1u < ar_n; i++) ar_held[i] = ar_held[i + 1u];
    ar_n--;
}
static void seq_step_on(void)
{
    if (sq_play) {
        uint32_t k = perf.cur % SQ_NPAT, l = perf.pat[k].len, len = l >= 1u && l <= SQ_MAX ? l : 16u, i, j, c, ok, f;
        if (sq_step == 0xFFFFFFFFu) { sq_step = 0; sq_pass = 0; }
        else if (++sq_step >= len) { sq_step = 0; sq_pass++; }
        i = sq_step;
        if (sq_note != 0xFF && sq_glen != SQ_HOLD) sq_glen = sq_glen > (1u << 24) ? sq_glen - (1u << 24) : 0u;   /* (a long note goes on) */
        ok = sq_nx_i == (k << 8 | i) ? sq_nx_ok : sq_draw(k, i, sq_pass);
        j = i + 1u >= len ? 0u : i + 1u;
        sq_nx_ok = (uint8_t)sq_draw(k, j, j ? sq_pass : sq_pass + 1u);
        sq_nx_i = k << 8 | j;
        c = (sqx.p[k].cond[i] & 15u) % NSC;
        if (c == SC_BEFORE) ok = ok && sq_last;
        if (c == SC_AFTER) ok = ok && sq_nx_ok;   /* (and a next IF BEFORE: the two together) */
        sq_last = (uint8_t)ok;
        sq_show = (uint8_t)i;
        f = *sq_fp(k, i);
        if (ok && SQ_BASS_ON) {
            int slide = sq_slide;
            uint32_t q, n = pl.n <= PL_MAX ? pl.n : 0u, mk = 0;   /* (its locks, until the next note) */
            for (q = 0; q < n; q++) {
                const plk_t *e = &pl.l[q];
                if (e->pat == (PL_BASS | k) && e->step == i && (e->tp & 15u) < NPKB) { mk |= 1u << (e->tp & 15u); sq_lkv[e->tp & 15u] = e->v; }
            }
            sq_lkm = (uint8_t)mk;
            sq_sound((uint32_t)clampi(24 + *sq_np(k, i) + sq_tr, 0, 127), f & SF_ACC ? 127u : 90u, slide, SQ_GL[sqx.p[k].cond[i] >> 4]);
            sq_slide = (uint8_t)(f & SF_SLIDE);
        } else if (sq_slide || !SQ_BASS_ON) sq_off();   /* (a rest: a slide stops; a long note goes on to its end) */
        return;
    }
    {                                           /* ARP */
        uint8_t so[16];
        uint32_t i, j, n = ar_n, oct = perf.ar_oct % 4u + 1u, tot = n * oct, k, nt;
        for (i = 0; i < n; i++) so[i] = ar_held[i];
        if (perf.ar_mode != 4u)                 /* (ORDER: as played; the others from the lowest) */
            for (i = 1; i < n; i++) for (j = i; j > 0 && so[j - 1u] > so[j]; j--) { uint8_t t = so[j]; so[j] = so[j - 1u]; so[j - 1u] = t; }
        switch (perf.ar_mode) {
        case 1: k = tot - 1u - ar_i % tot; break;
        case 2: { uint32_t per = tot > 1u ? 2u * tot - 2u : 1u, q = ar_i % per; k = q < tot ? q : per - q; break; }
        case 3: k = (rnd() >> 8) % tot; break;
        default: k = ar_i % tot; break;
        }
        ar_i++;
        nt = so[k % n] + 12u * (k / n);
        sq_show = (uint8_t)(k & 15u);
        sq_sound(nt > 127u ? 127u : nt, 100u, 0, (uint32_t)clampi(perf.ar_gate, 1, 10) * 0x199999u);
        sq_slide = perf.ar_gate >= 10u;
    }
}
/* ---- DRUMS: the sequencer (on SEQ BASS's clock, its own pattern, length and swing; 1/16 steps) */
static uint32_t dk_tick = 0xFFFFFFFFu, dk_step = 0xFFFFFFFFu, dk_pass, dk_nx_i = 0xFFFFFFFFu;
static uint8_t dk_last, dk_nx;
static volatile uint8_t dk_show = 0xFF, dk_hit;  /* the step playing; the tracks hit since the screen looked (it clears them) */
static volatile uint16_t dk_trig_at[NDK] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};   /* a hit's place in this block */
static volatile uint8_t dk_trig_vel[NDK];
static uint32_t dk_draw(uint32_t k, uint32_t t, uint32_t i, uint32_t pass)   /* (as sq_draw) */
{
    const dkpat_t *p = &dk.pat[k];
    uint32_t pc = p->pc[t][i], c = (pc & 15u) % NSC, pr = DK_PROB[pc >> 4];
    if (!(p->on[i] >> t & 1u)) return 0;
    if (pr < 100u && (rnd() >> 8) % 100u >= pr) return 0;
    if (c >= SC_1_2 && c <= SC_2_2) return pass % 2u == c - SC_1_2;
    if (c >= SC_1_4 && c <= SC_4_4) return pass % 4u == c - SC_1_4;
    if (c == SC_START) return pass == 0;
    if (c == SC_NSTART) return pass != 0;
    return 1;
}
static void dk_step_on(void)
{
    uint32_t k = dk.cur % DK_NPAT, l = dk.pat[k].len, len = l >= 1u && l <= DK_MAX ? l : 16u, i, j, t, ok = 0, nx = 0;
    if (dk_step == 0xFFFFFFFFu) { dk_step = 0; dk_pass = 0; }
    else if (++dk_step >= len) { dk_step = 0; dk_pass++; }
    i = dk_step;
    j = i + 1u >= len ? 0u : i + 1u;
    for (t = 0; t < NDK; t++) {
        ok |= (dk_nx_i == (k << 8 | i) ? (uint32_t)dk_nx >> t & 1u : dk_draw(k, t, i, dk_pass)) << t;
        nx |= dk_draw(k, t, j, j ? dk_pass : dk_pass + 1u) << t;
    }
    dk_nx = (uint8_t)nx;
    dk_nx_i = k << 8 | j;
    for (t = 0; t < NDK; t++) {
        uint32_t c = (dk.pat[k].pc[t][i] & 15u) % NSC;
        if ((c == SC_BEFORE && !(dk_last >> t & 1u)) || (c == SC_AFTER && !(nx >> t & 1u))) ok &= ~(1u << t);
    }
    dk_last = (uint8_t)ok;
    dk_show = (uint8_t)i;
    if ((dk.mode & 3u) == 2u) return;                  /* (BASS ONLY) */
    ok &= ~(uint32_t)dk.mute;
    {                                           /* this step's locks, track by track */
        uint32_t q, n = pl.n <= PL_MAX ? pl.n : 0u;
        for (t = 0; t < NDK; t++) if (ok >> t & 1u) dk_lkm[t] = 0;
        for (q = 0; q < n; q++) {
            const plk_t *e = &pl.l[q];
            uint32_t w = e->tp & 15u;
            t = e->tp >> 4 & 7u;
            if (e->pat == k && e->step == i && w < NPKD && (ok >> t & 1u)) { dk_lkm[t] |= (uint8_t)(1u << w); dk_lkv[t][w] = e->v; }
        }
    }
    for (t = 0; t < NDK; t++)
        if (ok >> t & 1u) {
            uint32_t r = dk_lkm[t] >> PK_RAT & 1u ? (uint32_t)clampi(dk_lkv[t][PK_RAT], 1, 4) : 1u;
            dk_trig_at[t] = (uint16_t)sq_at; dk_trig_vel[t] = dk.pat[k].acc[i] >> t & 1u ? 127u : 96u;
            dk_rn[t] = (uint8_t)(r - 1u);       /* RATCHET: r hits through the step */
            dk_riv[t] = 6615000u / (ks_bpm10 | 1u) / r;
            dk_rc[t] = 0;
        }
    dk_hit |= (uint8_t)ok;
}
static void dk_clock(uint32_t beats)
{
    uint64_t t = (uint64_t)beats * 4u;
    uint32_t tick = (uint32_t)(t >> 24), fr = (uint32_t)t & 0xFFFFFFu, sw = dk.pat[dk.cur % DK_NPAT].swing;
    if (sw && (tick & 1u)) {                    /* swing: the odd steps late by up to half a step */
        uint32_t s = (sw > 50u ? 50u : sw) * 167772u;
        if (fr < s) tick--;
    }
    if (tick != dk_tick) { dk_tick = tick; dk_step_on(); }
}

/* ---- DRUMS: the voices, synthesised (808: sines, squares, filtered noise; 909: brighter, more noise, a click) */
typedef struct {
    uint32_t ph, inc, ph2, inc2, sweep, m[6], minc[6];
    int32_t amp, pe, ne;                        /* envelopes, Q30 */
    uint32_t acf, pcf, ncf;                     /* their fall a sample (2^32 / time constant) */
    int32_t lo, bp, h1, h2, fa, fq, gain, tone;
    uint16_t cl;
    uint8_t clk, on;
} dkv_t;
static dkv_t dkv[NDK];
static int32_t dkb[HALF_FRAMES];
static const uint16_t DK_TR[32] = {256, 280, 306, 335, 366, 400, 438, 479, 524, 573, 626, 685, 749, 819, 895, 979, 1071, 1171, 1281, 1400,
                                   1531, 1675, 1831, 2003, 2190, 2395, 2619, 2864, 3132, 3425, 3746, 4096};   /* x1 .. x16, Q8 */
/* the kits: 808, 909, DUB TECHNO (a deep soft kick, noisy hats, a minor chord stab on PERC, a dark dub echo on the
 * snare, clap, toms, chord and open hats), ACOUSTIC (a beater kick, snare wires, toms with an overtone, a conga, brighter
 * hats and a ride) */
#define DK_NKIT 4u
enum { DK_808, DK_909, DK_DUB, DK_ACOU };
static const uint16_t DK_TMIN[DK_NKIT][NDK] = {{80, 30, 25, 4, 60, 50, 25, 150}, {50, 25, 22, 4, 50, 40, 6, 120},
                                              {150, 22, 40, 5, 80, 70, 60, 200}, {60, 30, 25, 4, 40, 80, 40, 140}};   /* (ACOUSTIC's snare, hats, cymbal drier: 1.0.32) */   /* the shortest decay (ms) */
static const uint16_t DK_TRIM[DK_NKIT][NDK] = {{470, 315, 675, 1855, 1915, 305, 320, 2095}, {480, 335, 990, 2615, 2615, 285, 360, 2590},
                                              {480, 360, 1030, 1740, 1855, 305, 485, 1835}, {495, 250, 640, 2210, 1795, 300, 340, 1190}};   /* (levels evened out at the default sounds, Q8; the hats were far too quiet before 0.9.54) */
static const uint16_t DK_SEND[NDK] = {0, 16000, 18000, 3000, 9000, 14000, 22000, 7000};   /* DUB TECHNO: into its echo, Q15 */
static const uint32_t DK_METAL[6] = {19994485u, 29645987u, 35995916u, 50906562u, 52591436u, 77913239u};   /* 808: 205 304 370 523 540 800 Hz */
static uint32_t dk_cf(uint32_t ms10) { uint32_t tau = ms10 * 441u / 100u; return 0xFFFFFFFFu / (tau < 20u ? 20u : tau); }   /* (2^32/tau, 32-bit: no 64-bit divide on the chip) */   /* (tenths of ms) */
static inline int32_t dk_env(int32_t *e, uint32_t cf) { int32_t v = *e; *e = v - (int32_t)(((int64_t)v * cf) >> 32); return v >> 15; }
static inline int32_t dk_noise(void) { return (int32_t)rnd() >> 17; }
static inline int32_t dk_saw(uint32_t ph) { return (int32_t)(ph >> 18) - 8192; }   /* (+-8192) */
static void dk_start(uint32_t t, uint32_t vel)
{
    dkv_t *v = &dkv[t];
    int8_t pp[4];                               /* (its sound, or this step's locks) */
    const int8_t *p = pp;
    uint32_t kit = dk.kit % DK_NKIT, ms10, q;
    for (q = 0; q < 4u; q++) pp[q] = dk_lkm[t] >> q & 1u ? dk_lkv[t][q] : dk.prm[t][q];
    ms10 = DK_TMIN[kit][t] * 10u * DK_TR[((uint32_t)p[2] & 127u) >> 2] / 256u;
    int32_t tq8 = p[1] * 48, tone = p[3] & 127, g;
    uint32_t tf = pitch_inc(60 * 256 + tq8) / ((pitch_inc(60 * 256) >> 12) | 1u);   /* TUNE, Q12 (no 64-bit divide on the chip) */
    memset(v, 0, sizeof *v);
    g = mul15(mul15(lvl15(p[0] & 127), vel >= 120u ? 32767 : 23000), lvl15(dk.vol & 127));
    v->gain = (g * (int32_t)DK_TRIM[kit][t]) >> 8;
    v->tone = tone;
    v->amp = 1 << 30;
    v->acf = dk_cf(ms10);
    v->on = 1;
    switch (t) {
    case DK_KICK: {                             /* a sine falling in pitch, a click (TONE) */
        static const uint8_t NOTE[DK_NKIT] = {31, 33, 29, 33};
        v->inc = pitch_inc(NOTE[kit] * 256 + tq8);
        v->sweep = kit == DK_909 ? v->inc * 3u : kit == DK_DUB ? v->inc : kit == DK_ACOU ? v->inc * 4u : v->inc + (v->inc >> 1);
        v->pe = 1 << 30; v->pcf = dk_cf(kit == DK_909 ? 200u : kit == DK_DUB ? 90u : kit == DK_ACOU ? 45u : 120u);
        v->ne = kit == DK_DUB ? tone << 19 : kit == DK_ACOU ? (tone + 48) << 21 : tone << 22;
        v->ncf = dk_cf(kit == DK_ACOU ? 30u : 15u);
        break;
    }
    case DK_SNARE: {                            /* two sines and a high-passed noise (TONE: the snares) */
        static const uint8_t N1[DK_NKIT] = {54, 55, 57, 53}, N2[DK_NKIT] = {64, 66, 69, 60}, NL[DK_NKIT] = {12, 15, 9, 14};
        uint32_t snap = dk.pad >> 3 & 7u;      /* SNAPPY: the wires louder and longer */
        v->inc = pitch_inc(N1[kit] * 256 + tq8);
        v->inc2 = pitch_inc(N2[kit] * 256 + tq8);
        v->sweep = kit == DK_909 ? v->inc / 3u : kit == DK_ACOU ? v->inc / 2u : 0u;
        v->pe = 1 << 30; v->pcf = dk_cf(kit == DK_ACOU ? 60u : 150u);
        v->ne = 1 << 30; v->ncf = dk_cf(ms10 * NL[kit] * (10u + snap * 4u) / 100u);
        v->fa = kit == DK_909 ? 9000 : kit == DK_DUB ? 4500 : kit == DK_ACOU ? 7000 : 6000;
        v->fq = (kit == DK_ACOU ? 88 : kit == DK_DUB ? 56 : 64) * (8 + (int32_t)snap * 2) / 8;   /* (the noise's share, /32) */
        break;
    }
    case DK_CLAP:                               /* noise in a band, four quick bursts, then the tail */
        v->fa = (int32_t)((kit == DK_909 ? 1300 : kit == DK_DUB ? 750 : kit == DK_ACOU ? 1700 : 1000) + tone * 12) * 4781 / 1024;
        v->fq = kit == DK_DUB ? 8000 : 11000;
        v->ncf = dk_cf(25u);
        break;
    case DK_CH:
    case DK_OH:
    case DK_CYM: {                              /* six squares (909, DUB, ACOUSTIC: and noise), high-passed twice (TONE: brighter) */
        uint32_t mul = kit == DK_ACOU ? (t == DK_CYM ? 3u : 6u) : t == DK_CYM ? 2u : 4u;   /* (x 1/2) */
        for (q = 0; q < 6u; q++) v->minc[q] = (uint32_t)(((uint64_t)DK_METAL[q] * tf) >> 12) * mul / 2u;
        v->fa = (kit == DK_ACOU && t == DK_CYM ? 9000 : kit == DK_DUB ? 15000 : 13000) + tone * 110;
        if (t == DK_CH) dkv[DK_OH].acf = dk_cf(30u);   /* (the closed hat chokes the open one) */
        break;
    }
    case DK_TOM:                                /* a sine falling in pitch (TONE: how far) */
        v->inc = pitch_inc((kit == DK_909 ? 47 : kit == DK_DUB ? 43 : 45) * 256 + tq8);
        v->sweep = (uint32_t)(((uint64_t)v->inc * (uint32_t)(kit == DK_ACOU ? (tone >> 1) + 6 : kit == DK_DUB ? tone + 40 : tone + 16)) >> 7);
        v->pe = 1 << 30; v->pcf = dk_cf(kit == DK_DUB ? 500u : kit == DK_ACOU ? 150u : 300u);
        if (kit == DK_909) { v->ne = 1 << 29; v->ncf = dk_cf(40u); }
        if (kit == DK_ACOU) { v->ne = 1 << 28; v->ncf = dk_cf(25u); v->inc2 = v->inc + (v->inc >> 1) + (v->inc >> 4); }   /* (the skin's overtone) */
        break;
    default:                                    /* PERC: 808 cowbell, 909 rimshot, DUB a minor chord stab, ACOUSTIC a conga */
        if (kit == DK_808) {                    /* (two squares in a band) */
            v->minc[0] = (uint32_t)(((uint64_t)52591436u * tf) >> 12);
            v->minc[1] = (uint32_t)(((uint64_t)77913239u * tf) >> 12);
            v->fa = (int32_t)(800 + tone * 16) * 4781 / 1024; v->fq = 20000;
        } else if (kit == DK_909) {             /* (two sines, a click) */
            v->inc = (uint32_t)(((uint64_t)48695774u * tf) >> 12);
            v->inc2 = (uint32_t)(((uint64_t)165565633u * tf) >> 12);
            v->ne = tone << 23; v->ncf = dk_cf(10u);
        } else if (kit == DK_DUB) {             /* (four saws, root minor seventh, a closing low-pass: TONE how open) */
            static const uint8_t CH[4] = {0, 3, 7, 10};
            for (q = 0; q < 4u; q++) { v->minc[q] = pitch_inc((50 + CH[q]) * 256 + tq8 + (int32_t)q * 3); v->m[q] = rnd(); }
            v->fa = 2500 + tone * 90;
            v->fq = 21000;
            v->pe = 1 << 30; v->pcf = dk_cf(ms10 * 6u / 10u + 200u);
        } else {                                /* (a sine dropping a little, a slap) */
            v->inc = pitch_inc(62 * 256 + tq8);
            v->sweep = v->inc >> 2;
            v->pe = 1 << 30; v->pcf = dk_cf(40u);
            v->ne = tone << 22; v->ncf = dk_cf(8u);
        }
        break;
    }
}
static int32_t dk_voice(dkv_t *v, uint32_t t, uint32_t kit)   /* one sample */
{
    int32_t x, a, s;
    uint32_t q;
    switch (t) {
    case DK_KICK:
        a = dk_env(&v->amp, v->acf);
        v->ph += v->inc + (uint32_t)(((uint64_t)v->sweep * (uint32_t)dk_env(&v->pe, v->pcf)) >> 15);
        s = sin32(v->ph);
        if (v->ne) s += mul15(dk_noise(), dk_env(&v->ne, v->ncf));
        if (kit == DK_909) s = sat15(s + (s >> 1));
        else if (kit == DK_ACOU) { v->lo += (s - v->lo) >> 2; s = sat15(v->lo + (v->lo >> 2)); }   /* (the beater a little dulled) */
        if (dk.pad & 7u) s = sat15(mul15(s, 32768 + (int32_t)(dk.pad & 7u) * 14000));   /* KICK DRIVE: up to 4 times into a soft clip */
        return mul15(s, a);
    case DK_SNARE: {
        int32_t n = dk_noise(), e = dk_env(&v->ne, v->ncf), body;
        a = dk_env(&v->amp, v->acf);
        v->ph += v->inc + (v->sweep ? (uint32_t)(((uint64_t)v->sweep * (uint32_t)dk_env(&v->pe, v->pcf)) >> 15) : 0u);
        v->ph2 += v->inc2;
        body = (sin32(v->ph) * 3 + sin32(v->ph2) * 2) / 5;
        v->lo += mul15(n - v->lo, v->fa);
        return mul15(body, mul15(a, 32767 - v->tone * 120)) + ((mul15(n - v->lo, mul15(e, 6000 + v->tone * 200)) * v->fq) >> 5);
    }
    case DK_CLAP: {
        int32_t hp, env;
        if (v->clk < 4u) { if (!v->cl) { v->ne = 1 << 30; v->cl = 353; v->clk++; } v->cl--; }
        hp = dk_noise() - v->lo - mul15(v->fq, v->bp);
        v->bp += mul15(v->fa, hp);
        v->lo += mul15(v->fa, v->bp);
        env = dk_env(&v->ne, v->ncf);
        if (v->clk >= 4u) env += dk_env(&v->amp, v->acf) >> 1;
        return mul15(v->bp, env);
    }
    case DK_CH:
    case DK_OH:
    case DK_CYM:
        a = dk_env(&v->amp, v->acf);
        for (s = 0, q = 0; q < 6u; q++) { v->m[q] += v->minc[q]; s += v->m[q] >> 31 ? 5000 : -5000; }
        if (kit == DK_909) s = (s * 2 + dk_noise() * 3) / 5;
        else if (kit == DK_DUB) s = (s + dk_noise() * 5) / 4;
        else if (kit == DK_ACOU) s = (s * 2 + dk_noise() * 4) / 5;
        v->h1 += mul15(s - v->h1, v->fa); x = s - v->h1;
        v->h2 += mul15(x - v->h2, v->fa); x -= v->h2;
        return mul15(x, a);
    case DK_TOM:
        a = dk_env(&v->amp, v->acf);
        v->ph += v->inc + (uint32_t)(((uint64_t)v->sweep * (uint32_t)dk_env(&v->pe, v->pcf)) >> 15);
        s = sin32(v->ph);
        if (v->inc2) { v->ph2 += v->inc2; s = (s * 3 + sin32(v->ph2)) / 4; }
        if (v->ne) s += mul15(dk_noise(), dk_env(&v->ne, v->ncf)) >> 1;
        return mul15(s, a);
    default:
        a = dk_env(&v->amp, v->acf);
        if (kit == DK_808) {
            int32_t hp;
            v->m[0] += v->minc[0]; v->m[1] += v->minc[1];
            s = (v->m[0] >> 31 ? 12000 : -12000) + (v->m[1] >> 31 ? 12000 : -12000);
            hp = s - v->lo - mul15(v->fq, v->bp);
            v->bp += mul15(v->fa, hp);
            v->lo += mul15(v->fa, v->bp);
            s = v->bp + (s >> 2);
        } else if (kit == DK_909) {
            v->ph += v->inc; v->ph2 += v->inc2;
            s = (sin32(v->ph) + sin32(v->ph2)) >> 1;
            if (v->ne) s += mul15(dk_noise(), dk_env(&v->ne, v->ncf));
        } else if (kit == DK_DUB) {
            int32_t hp, f = 900 + mul15(v->fa, dk_env(&v->pe, v->pcf));
            for (s = 0, q = 0; q < 4u; q++) { v->m[q] += v->minc[q]; s += dk_saw(v->m[q]); }
            hp = s - v->lo - mul15(v->fq, v->bp);
            v->bp += mul15(f, hp);
            v->lo += mul15(f, v->bp);
            s = v->lo;
        } else {
            v->ph += v->inc + (uint32_t)(((uint64_t)v->sweep * (uint32_t)dk_env(&v->pe, v->pcf)) >> 15);
            s = sin32(v->ph);
            if (v->ne) s += mul15(dk_noise(), dk_env(&v->ne, v->ncf)) >> 1;
        }
        return mul15(s, a);
    }
}
/* DUB TECHNO's echo: a dotted eighth (an eighth when slow), dark and fading, at a quarter of the rate (11 kHz, its own
 * buffer: 0.56 s) */
#define DE_N 6144u
static int16_t de_buf[DE_N] POOL;
static struct { uint32_t w, k; int32_t acc, prev, cur, lp, hp; uint8_t on; } de;
static int32_t dke[HALF_FRAMES];
static void dk_echo(uint32_t n)
{
    uint32_t i, t = 4961250u / (ks_bpm10 | 1u);   /* (3/4 of a beat at 11025 Hz) */
    if (t > DE_N - 2u) t = t * 2u / 3u;
    if (t > DE_N - 2u) t = DE_N - 2u;
    if (t < 64u) t = 64u;
    if (!de.on) { memset(de_buf, 0, sizeof de_buf); memset(&de, 0, sizeof de); de.on = 1; }
    for (i = 0; i < n; i++) {
        de.acc += dke[i];
        if (++de.k == 4u) {                     /* (every fourth sample: in, out, feedback) */
            int32_t in = de.acc >> 2, y = de_buf[(de.w + DE_N - t) % DE_N];
            de.acc = 0; de.k = 0;
            de.lp += mul15(y - de.lp, 11000);
            de.hp += (de.lp - de.hp) >> 6;
            de_buf[de.w] = (int16_t)sat15(in + mul15(de.lp - de.hp, 19000));
            if (++de.w == DE_N) de.w = 0;
            de.prev = de.cur; de.cur = y;
        }
        dkb[i] += (de.prev + (((de.cur - de.prev) * (int32_t)de.k) >> 2)) >> 1;
    }
}
static void dk_render(uint32_t n)               /* the drums for this block -> dkb (mono) */
{
    uint32_t t, i, kit = dk.kit % DK_NKIT;
    for (i = 0; i < n; i++) dkb[i] = dke[i] = 0;
    for (t = 0; t < NDK; t++) {
        dkv_t *v = &dkv[t];
        uint32_t at = dk_trig_at[t];
        int32_t snd = kit != DK_DUB || (dk.pad & 0x40u) ? 0 : dk_lkm[t] >> PK_ECHO & 1u ? dk_lkv[t][PK_ECHO] * 258 : DK_SEND[t], o;
        if (at == 0xFFFFu && !v->on && !dk_rn[t]) continue;
        dk_trig_at[t] = 0xFFFFu;
        for (i = 0; i < n; i++) {
            if (i == at) { dk_start(t, dk_trig_vel[t]); dk_rc[t] = dk_riv[t]; }
            else if (dk_rn[t] && dk_rc[t] && !--dk_rc[t]) { dk_start(t, dk_trig_vel[t] - 16u); if (--dk_rn[t]) dk_rc[t] = dk_riv[t]; }   /* (RATCHET: the next hit) */
            if (!v->on) continue;
            o = mul15(dk_voice(v, t, kit), v->gain);
            dkb[i] += o;
            if (snd) dke[i] += mul15(o, snd);
            if (v->amp < (1 << 16) && (t != DK_CLAP || v->clk >= 4u)) v->on = 0;
        }
    }
    if (kit == DK_DUB && !(dk.pad & 0x40u)) dk_echo(n);   /* (DUB ECHO OFF: the kit dry) */
    else de.on = 0;
}

static void seq_clock(uint32_t beats)
{
    const seqpat_t *p = &perf.pat[perf.cur % SQ_NPAT];
    uint32_t rate = sq_play ? p->rate : perf.ar_rate, sw = sq_play ? p->swing : 0u, tick, fr;
    uint64_t t = (uint64_t)beats * STEP_PB[rate % 6u];
    if (ar_was && !perf.ar_on) ar_n = 0;        /* (the ARP turned off: what it held is let go) */
    ar_was = perf.ar_on;
    if (!sq_play && !(perf.ar_on && ar_n)) {
        if (sq_note != 0xFF) sq_off();
        sq_tick = 0xFFFFFFFFu;
        sq_show = 0xFF;
        ar_i = 0;
        dk_tick = 0xFFFFFFFFu; dk_step = 0xFFFFFFFFu; dk_show = 0xFF;
        return;
    }
    tick = (uint32_t)(t >> 24);
    fr = (uint32_t)t & 0xFFFFFFu;
    if (sw && (tick & 1u)) {                    /* swing: the odd steps late by up to half a step */
        uint32_t s = (sw > 50u ? 50u : sw) * 167772u;
        if (fr < s) { tick--; fr += 0x1000000u; } else fr -= s;
    }
    if (tick != sq_tick) { sq_tick = tick; seq_step_on(); }
    if (sq_note != 0xFF && !sq_slide && sq_glen < (1u << 24) && fr >= sq_glen) { note_off(sq_note); sq_note = 0xFF; }
    if (sq_play) dk_clock(beats);
}

/* all the voices for n samples (core 1's job) -> dst, mono */
static void voices_render(int32_t *dst, uint32_t n, uint32_t t0, uint32_t budget)
{
    uint32_t i, k, binc, any = 0, lim = ks_shed >= 3u ? 1u : ks_shed >= 2u ? 2u : NVOICE, on = 0;
    gv_update();
    while (ev_r != ev_w) {
        uint32_t e = evq[ev_r % 64u], t = e & 255u, a = (e >> 8) & 255u, b = e >> 16;
        ev_r++;
        uint32_t sqr = sq_play && SQ_BASS_ON, sqb = sqr && (dk.mode & 8u);   /* (the keys transpose the bass sequence only with SEQUENCE: ON · KEYS TRANSPOSE; else they play, over it too: 1.0.51) */
        if (t == EV_ON) {
            if (sqb && !sq_rec) sq_tr = (int32_t)a - 60;   /* (the SEQ plays: the keys transpose it) */
            else if (perf.ar_on && !sqr) ar_add(a);
            else note_on(a, b);
        } else if (t == EV_OFF) {
            if (sqb && !sq_rec) {}
            else if (perf.ar_on && !sqr) ar_del(a);
            else note_off(a);
        } else if (t == EV_SEQ) {
            sq_off();
            sq_play = (uint8_t)a;
            sq_step = 0xFFFFFFFFu;
            sq_tick = 0xFFFFFFFFu;
            sq_nx_i = 0xFFFFFFFFu;
            sq_last = 0;
            dk_tick = 0xFFFFFFFFu; dk_step = 0xFFFFFFFFu; dk_nx_i = 0xFFFFFFFFu; dk_last = 0; dk_show = 0xFF;
            sq_lkm = 0;                         /* (no lock past the play) */
            if (a && b) ks_beats = 0;           /* (the internal clock: from the bar, the LFOs too) */
        } else if (t == EV_PANIC) { for (k = 0; k < NVOICE; k++) { vo[k].on = 0; vo[k].ea.st = 0; vo[k].ea.lv = 0; } nheld = 0; ar_n = 0; sq_note = 0xFF; }
        else if (t == EV_SYNC) ks_beats = a ? b * 699051u : 0u;   /* (the MIDI clock: its tick, 1/24 beat) */
        else if (t == EV_DRUM) { dk_trig_at[a % NDK] = 0; dk_trig_vel[a % NDK] = (uint8_t)b; dk_lkm[a % NDK] = 0; dk_rn[a % NDK] = 0; }   /* (a key: the sound as set, no locks) */
    }
    for (k = 0; k < NVOICE; k++) on += vo[k].on;
    while (on > lim) {                          /* the governor took voices away: the oldest go */
        uint32_t old = NVOICE;
        for (k = 0; k < NVOICE; k++) if (vo[k].on && (old == NVOICE || vo[k].age < vo[old].age)) old = k;
        vo[old].on = 0;
        vo[old].ea.st = 0;
        vo[old].ea.lv = 0;
        on--;
    }
    { static uint32_t brem; uint32_t bq = ks_bpm10 * 664858u + brem; binc = bq >> 16; brem = bq & 0xFFFFu; }   /* (the rest kept: the clock no longer runs slow, up to 10 ms a minute; 1.0.47) */          /* beats per block, Q24: bpm10 x 16 x 2^24 / (600 x 44100) */
    for (i = 0; i < n; i += KS_CB) {
        int32_t *acc = dst + i;
        if (i && fm1_ticks() - t0 > budget) {   /* out of time: the rest of the block stays silent (the last part faded: no click) */
            for (k = 0; k < KS_CB; k++) dst[i - KS_CB + k] = dst[i - KS_CB + k] / (int32_t)KS_CB * (int32_t)(KS_CB - k);
            for (k = i; k < n; k++) dst[k] = 0;
            ks_late++;
            break;
        }
        ks_beats = (ks_beats + binc) % BEATS_WRAP;
        lfo_run(0, ks_beats);
        lfo_run(1, ks_beats);
        lfo_run(2, ks_beats);
        cut_glide();
        step_run(ks_beats);
        sq_at = i;
        seq_clock(ks_beats);
        for (k = 0; k < KS_CB; k++) acc[k] = 0;
        for (k = 0; k < NVOICE; k++)
            if (vo[k].on) { voice_block(&vo[k], acc); any = 1; }
        if (patch[ST_ON] && fpv(ST_GATE, D_GATE)) {   /* the gate: the steps cut the sound, ramped over the block */
            static int32_t g0 = 32767;
            int32_t g1 = 32767 - (((32767 - step_val) * fpv(ST_GATE, D_GATE)) >> 7), dg = (g1 - g0) / (int32_t)KS_CB;
            for (k = 0; k < KS_CB; k++) { g0 += dg; acc[k] = mul15(acc[k], g0); }
            g0 = g1;
        }
    }
    ks_sounding = (uint8_t)any;
    fx_mods();
}

/* ================================================================ the effects (core 0) */
#define DL_N 32768u
#define CH_N 2048u
static int16_t dl_buf[2][DL_N] POOL;
#define LV_N 32768u                             /* FX LIVE: the last 0.74 s of the voices (STUTTER, TAPE) */
static int16_t lv_buf[LV_N] POOL;
static struct {
    uint32_t w, st_start, st_len, st_pos, tp_lag, tp_fade;
    uint8_t st_on, st_div, tp_on;
    int32_t dj_l[2], dj_b[2];
} lv;
static int16_t ch_buf[CH_N];
static const uint16_t RV_LEN[8] = {1116, 1188, 1277, 1356, 1139, 1211, 1300, 1379}, AP_LEN[4] = {556, 441, 579, 464};
static int16_t rv_c[1116 + 1188 + 1277 + 1356 + 1139 + 1211 + 1300 + 1379] POOL;
static int16_t rv_a[556 + 441 + 579 + 464];
static struct {
    int32_t dco[2], dc_y, cr_hold, cr_n, hp_ch, dl_lp[2], dl_hp[2], dl_t, rv_lp[8], rv_hp, sub_lp, env, gain;
    uint32_t dl_w, ch_w, chph, rv_i[8], ap_i[4];
    int32_t cp_env, cp_g, ph_x[2][4], ph_y[2][4], ph_last[2], sp_hp, sp_lp;   /* compressor, phaser, the spring's band */
    uint32_t cp_n, phph, dl_wow;
    int32_t lo[2], hh, lg, ng, nenv;                /* the bass shelf's two lows, the limiter's gain, the normaliser's gain (Q12) and level */
    int32_t mb[2][2];                           /* MONO BASS: two lows a side */
    int32_t ot_l[2][2], ot_h[2], ot_g[3];       /* MULTIBAND: the low band's two poles, the high split's, each band's gain (Q12) */
} fx;
static volatile uint8_t ks_norm = 1, ks_deep = 1, ks_sync;
static uint32_t sy_e8, sy_n;                    /* SYNC OUT: the eighth now, what is left of its pulse */
#if defined(KAWA_WASM)
#define KS_SYNC_AMP clampi((int32_t)(30000 * 4096 / (w_master > 256 ? w_master : 256)), 0, 400000)
#elif defined(DINGHY_HOST)
#define KS_SYNC_AMP 30000
#else
#define KS_SYNC_AMP clampi((int32_t)(30000 * 4096 / (master_q12 > 256 ? master_q12 : 256)), 0, 400000)   /* (full scale whatever the MASTER knob: the platform scales after) */
#endif
static volatile int32_t meter_cp = 32767;       /* the compressor's gain (the COMPRESSOR view) */
/* PUMP: the dip on the tempo, deepest at the beat, a curve back up (rate 0 1/4, 1 1/8, 2 1/2, 3 a bar); Q15 gain */
static int32_t pump_gain(uint32_t pos, uint32_t rate, int32_t depth)
{
    static const uint8_t SH[4] = {0, 1, 1, 2};
    uint32_t fr = (rate == 1u ? pos << 1 : rate ? pos >> SH[rate & 3u] : pos) & 0xFFFFFFu;
    int32_t u = 32767 - (int32_t)(fr >> 9), g = 32767 - mul15(mul15(u, u), depth * 258);
    if ((fr >> 16) < 5u) g = 32767 - (32767 - g) * (int32_t)(fr >> 8) / 1280;   /* (a 2 % slope in, no click) */
    return g;
}
/* the reverb's types: its combs' length (Q8), feedback and damping added; SPRING: a band into short combs */
static const uint16_t RV_SC[4] = {150, 256, 205, 86};
static const int16_t RV_FBA[4] = {-1800, 0, 1500, 4200}, RV_DA[4] = {-2000, 0, -7000, 3000};

static uint8_t fz_on;                           /* FREEZE: on (its loop set), and the loop's length */
static uint32_t fz_T;
static volatile uint8_t fx_flush;               /* 1: the effects' memories emptied (a sound loaded), 2: and the drums' echo too (PANIC); 1.0.36 */
static void fx_clear(uint32_t what)
{
    static uint8_t dl_was;
    if (what) {
        memset(dl_buf, 0, sizeof dl_buf); memset(rv_c, 0, sizeof rv_c); memset(rv_a, 0, sizeof rv_a); memset(ch_buf, 0, sizeof ch_buf);
        memset(fx.dl_lp, 0, sizeof fx.dl_lp); memset(fx.dl_hp, 0, sizeof fx.dl_hp); memset(fx.rv_lp, 0, sizeof fx.rv_lp);
        memset(fx.ph_x, 0, sizeof fx.ph_x); memset(fx.ph_y, 0, sizeof fx.ph_y); memset(fx.ph_last, 0, sizeof fx.ph_last);
        lv.st_on = 0; lv.tp_on = 0; lv.tp_fade = 0;
        if (what >= 2u) de.on = 0;              /* (dk_render empties it when it starts again) */
        dl_was = patch[DL_ON] ? 1u : 0u;
        return;
    }
    if (patch[DL_ON] && !dl_was) { memset(dl_buf, 0, sizeof dl_buf); memset(fx.dl_lp, 0, sizeof fx.dl_lp); memset(fx.dl_hp, 0, sizeof fx.dl_hp); }   /* (the delay switched on: not what it held when it went off) */
    dl_was = patch[DL_ON] ? 1u : 0u;
}
static void fx_render(const int32_t *in, int32_t *out, uint32_t n)
{
    const int8_t *P = patch;
    uint32_t i, k;
    { uint32_t f = fx_flush; fx_flush = 0; fx_clear(f); }
    int32_t da = fpv(DR_AMT, D_DRAMT), cr = fpv(CR_RATE, D_CRRATE), chr = fpv(CH_RATE, D_CHRATE), mv = fpv(MS_VOL, D_VOL);
    int32_t dr_g = 256 + da * da * 3 / 4, dr_mix = fpv(DR_MIX, D_DRMIX) * 258, cr_mix = fpv(CR_MIX, D_CRMIX) * 258;
    int32_t bits = 16 - ((fpv(CR_BITS, D_CRBITS) * 14) >> 7), hold = 1 + ((cr * 31) >> 7);
    int32_t ch_rate = 4870 + chr * chr * 30, ch_dep = fpv(CH_DEP, D_CHDEP) * 2, ch_mix = fpv(CH_MIX, D_CHMIX) * 258;
    int32_t dl_fb = fpv(DL_FB, D_DLFB) * 250, dl_fbd, dl_lpk = 3000 + fpv(DL_TONE, D_DLTONE) * 233, dl_mix = fpv(DL_MIX, D_DLMIX) * 258, dl_t;
    int32_t rv_fb = 22000 + fpv(RV_SIZE, D_RVSIZE) * 80, rv_d = fpv(RV_DAMP, D_RVDAMP) * 200, rv_mix = fpv(RV_MIX, D_RVMIX) * 258;
    int32_t vol = (mv * mv * 419) >> 10, boost = fpv(MS_SUB, D_SUBB) * 200, glue = fpv(MS_GLUE, D_GLUE);
    int32_t thr = 32767 - glue * 180, makeup = 4096 + glue * 24;
    int32_t dj_f = 0, dj_q = 0, dj_mix = 0, dj_df = 0, dj_dm = 0;
    static const int32_t MB_A[7] = {0, 267, 356, 445, 534, 667, 889};   /* one pole at 60 .. 200 Hz, Q15 */
    int32_t mba = MB_A[(uint32_t)P[MB_MONO] % 7u], ot_amt = GP(D_OTAMT) * 258, ot_sum[3] = {0, 0, 0}, ot_gl, ot_gm, ot_gh;
    int32_t shelf = ks_deep == 2u ? 300 : ks_deep == 1u || ks_deep == 4u ? 150 : 0, harm = ks_deep >= 3u, ng = fx.ng ? fx.ng : 4096, lg = fx.lg ? fx.lg : 32768, nsum = 0;
    if (!fx.ot_g[0]) fx.ot_g[0] = fx.ot_g[1] = fx.ot_g[2] = 4096;
    ot_gl = (int32_t)(((int64_t)fx.ot_g[0] * (4096 + GP(D_OTLOW) * 32)) >> 12);   /* (LOW / HIGH BAND: +-0.5) */
    ot_gm = fx.ot_g[1];
    ot_gh = (int32_t)(((int64_t)fx.ot_g[2] * (4096 + GP(D_OTHIGH) * 32)) >> 12);
    dk_render(n > HALF_FRAMES ? HALF_FRAMES : n);
    if (eq_new) { memcpy(eq_c, eq_n, sizeof eq_c); if (eq_nmask & ~eq_mask) memset(eq_st, 0, sizeof eq_st); eq_mask = eq_nmask; eq_new = 0; }
    uint32_t pb = ks_beats, pinc = (ks_bpm10 * 41546u) >> 16;   /* (the beat, Q24, and its step a sample: PUMP) */
    int32_t cp_amt = fpv(CP_AMT, D_CPAMT), cp_thr = 32767 - cp_amt * 200, cp_r = 2 + cp_amt / 16, cp_mk = 32768 + cp_amt * 150;
    int32_t cp_atk = 32768 / (1 + GP(D_CPATK) * 11), cp_rel = 1048576 / (441 + GP(D_CPREL) * 270), cp_pump = P[CP_ON] ? GP(D_CPPUMP) : 0;
    int32_t ph_dep = fpv(PH_DEP, D_PHDEP) * 160, ph_fb = GP(D_PHFB) * 200, ph_mix = fpv(PH_MIX, D_PHMIX) * 258;
    uint32_t ph_inc = KS_LFO_INC[fpv(PH_RATE, D_PHRATE)] / KS_CB, rvt = (uint32_t)P[RV_TYPE] & 3u, dlt = (uint32_t)P[DL_TYPE] & 3u;
    int32_t rv_fb2 = clampi(rv_fb + RV_FBA[rvt], 0, 31800), rv_d2 = clampi(rv_d + RV_DA[rvt], 0, 32000);
    {                                           /* the delay time from the tempo */
        uint32_t cpb = DIV_CPB[GP(D_DLTIME) & 15], spb = 26460000u / (ks_bpm10 | 1u), t = (spb << 8) / (cpb >> 8);
        dl_t = (int32_t)(t > DL_N - 4u ? DL_N - 4u : t) << 8;
        if (!fx.dl_t) fx.dl_t = dl_t;
    }
    {                                           /* the glue: a level each block, a gain to follow it */
        int32_t pk = 0, tg = 32767;
        for (i = 0; i < n; i++) { int32_t a = in[i] < 0 ? -in[i] : in[i]; if (a > pk) pk = a; }
        fx.env = pk > fx.env ? fx.env + ((pk - fx.env) >> 1) : fx.env - (fx.env >> 5);
        if (glue && fx.env > thr) tg = (((thr + ((fx.env - thr) >> 2)) >> 2) << 15) / (fx.env >> 2);
        if (!fx.gain) fx.gain = 32767;
        fx.gain += (tg - fx.gain) >> 2;
        meter_gr = fx.gain;
        meter_pk -= meter_pk >> 3;              /* (the meter falls back) */
    }
    {                                           /* FX LIVE: the STUTTER slice from the tempo */
        uint32_t spb = 26460000u / (ks_bpm10 | 1u), len = lfx_stut ? spb >> (lfx_stut - 1u) : 0u;
        if (len > LV_N - 256u) len = LV_N - 256u;
        if (lv.st_on && (!lfx_stut || lfx_stut != lv.st_div)) lv.st_on = 0;   /* (another length: from the same end) */
        if (lfx_stut && !lv.st_on) { lv.st_on = 1; lv.st_div = lfx_stut; lv.st_len = len < 64u ? 64u : len; lv.st_start = lv.w - lv.st_len; lv.st_pos = 0; }
    }
    if (lfx_flt || lfx_pump) {                  /* the DJ filter's coefficients and the PUMP's place on the beat */
        int32_t v = lfx_flt < 0 ? -lfx_flt : lfx_flt;
        dj_f = lfx_flt < 0 ? (64 - v) * (64 - v) * 6 + 360 : v * v * 6 + 120;
        if (dj_f > 24000) dj_f = 24000;
        dj_q = 46000 - lfx_fres * 280;
        dj_mix = v * 4096 > 32767 ? 32767 : v * 4096;
    }
    {                                           /* (the DJ filter glided: a third of the way a block, ramped over it; 1.0.35) */
        static int32_t f0 = -1, m0;
        int32_t f1, m1;
        if (f0 < 0 || !lfx_flt) { f0 = dj_f; m0 = lfx_flt ? dj_mix : 0; }
        f1 = f0 + (dj_f - f0) / 3; m1 = m0 + (dj_mix - m0) / 3;
        dj_df = (f1 - f0) / (int32_t)n; dj_dm = (m1 - m0) / (int32_t)n;
        dj_f = f0; dj_mix = m0;
        f0 = f1; m0 = m1;
    }
    for (i = 0; i < n; i++) {
        int32_t x = in[i] * 7 / 8, L, R, w;
        if (lv.st_on) {                         /* STUTTER: the slice again, a short fade at its ends; nothing written */
            uint32_t q = lv.st_pos, e = lv.st_len - q, g = q < e ? q : e;
            int32_t y = lv_buf[(lv.st_start + q) & (LV_N - 1u)];
            x = g < 64u ? (y * (int32_t)g) >> 6 : y;
            if (++lv.st_pos >= lv.st_len) lv.st_pos = 0;
        } else {
            lv_buf[lv.w & (LV_N - 1u)] = (int16_t)sat15(x);
            lv.w++;
            if (lfx_tape || lv.tp_fade) {       /* TAPE: read slower and slower; let go, back to now through a fade */
                uint32_t sp = lfx_tape ? 65536u - (uint32_t)lfx_tape * 516u : 65536u, r, f, pos;
                int32_t a, b, y;
                if (!lv.tp_on && lfx_tape) { lv.tp_on = 1; lv.tp_lag = 0; }
                lv.tp_lag += 65536u - sp;       /* (how far behind now, Q16: it grows while slower) */
                if (lv.tp_lag > (LV_N - 512u) << 16) { lv.tp_lag = (LV_N - 512u) << 16; sp = 0; }   /* (as far as it keeps: stopped) */
                pos = (lv.w << 16) - lv.tp_lag - (2u << 16);
                r = pos >> 16; f = (pos >> 1) & 32767u;
                a = lv_buf[r & (LV_N - 1u)]; b = lv_buf[(r + 1u) & (LV_N - 1u)];
                y = a + (((b - a) * (int32_t)f) >> 15);
                if (sp < 16384u) y = (int32_t)(((int64_t)y * sp) >> 14);   /* (the stop fades out) */
                if (lfx_tape) x = y;
                else { x = y + (((x - y) * (int32_t)(256u - lv.tp_fade)) >> 8); lv.tp_fade--; }
            }
            if (!lfx_tape && lv.tp_on) { lv.tp_on = 0; lv.tp_fade = 256u; }
        }
        if (P[DR_ON]) {                         /* drive */
            int32_t y = (int32_t)(((int64_t)x * dr_g) >> 8);
            switch (P[DR_TYPE]) {
            case 0: y = sat15(y); break;
            case 1: y = sat15(y + (int32_t)(((int64_t)y * y) >> 17)); break;
            case 2: y = clip(y, 26000) * 5 / 4; break;
            default: y = lfold(y); break;
            }
            x += mul15(y - x, dr_mix);
            fx.dc_y += (x * 256 - fx.dc_y) >> 11;  /* (the tube's DC away) */
            x -= fx.dc_y >> 8;
        }
        if (P[CR_ON]) {                         /* crush: bits and rate */
            if (++fx.cr_n >= hold) {
                int32_t sh = 16 - bits;
                fx.cr_n = 0;
                fx.cr_hold = sh > 0 ? (clip(x, 32767) >> sh) * (1 << sh) : x;
            }
            x += mul15(fx.cr_hold - x, cr_mix);
        }
        if (P[CP_ON]) {                         /* the compressor: a peak follower, the gain every 16 samples, slid */
            int32_t a = x < 0 ? -x : x;
            if (a > 131071) a = 131071;
            if (a > fx.cp_env) fx.cp_env += (int32_t)(((int64_t)(a - fx.cp_env) * cp_atk) >> 15);
            else fx.cp_env -= (int32_t)(((int64_t)(fx.cp_env - a) * cp_rel) >> 20) + 1;
            if (fx.cp_env < 0) fx.cp_env = 0;
            if (!fx.cp_g) fx.cp_g = 32767;
            if (!(fx.cp_n++ & 15u)) {
                int32_t e = fx.cp_env > 1 ? fx.cp_env : 1, gt = 32767;
                if (e > cp_thr && e >= 4) gt = (((cp_thr + (e - cp_thr) / cp_r) >> 2) << 15) / (e >> 2);   /* (32-bit: e < 2^17) */
                fx.cp_g += (gt - fx.cp_g) >> 1;
                meter_cp = fx.cp_g;
            }
            x = (int32_t)(((int64_t)mul15(x, fx.cp_g) * cp_mk) >> 15);
        } else meter_cp = 32767;
        x = clip(x, 120000);
        L = R = x;
        if (P[CH_ON] && !ks_shed) {             /* chorus on what is above ~150 Hz (the sub stays mono) */
            int32_t hp, t0, t1;
            uint32_t r;
            fx.hp_ch += (x - fx.hp_ch) >> 6;
            hp = x - fx.hp_ch;
            ch_buf[fx.ch_w & (CH_N - 1u)] = (int16_t)clip(hp, 32767);
            fx.chph += (uint32_t)ch_rate;
            t0 = (441 << 8) + tri32(fx.chph) * ch_dep / 128;
            t1 = (441 << 8) + tri32(fx.chph + 0x80000000u) * ch_dep / 128;
            r = (fx.ch_w - (uint32_t)(t0 >> 8)) & (CH_N - 1u);
            w = ch_buf[r] + (((ch_buf[(r - 1u) & (CH_N - 1u)] - ch_buf[r]) * (t0 & 255)) >> 8);
            L += mul15(w, ch_mix);
            r = (fx.ch_w - (uint32_t)(t1 >> 8)) & (CH_N - 1u);
            w = ch_buf[r] + (((ch_buf[(r - 1u) & (CH_N - 1u)] - ch_buf[r]) * (t1 & 255)) >> 8);
            R += mul15(w, ch_mix);
            fx.ch_w++;
        }
        if (P[PH_ON] && !ks_shed) {             /* the phaser: four all-passes a side, swept by its LFO, a feedback */
            uint32_t c;
            fx.phph += ph_inc;
            for (c = 0; c < 2u; c++) {
                int32_t lfo = (tri32(fx.phph + (c ? 0x40000000u : 0u)) + 32768) >> 1;   /* (0..32767) */
                int32_t a = 4900 + mul15(lfo, ph_dep), y = clip(x + mul15(fx.ph_last[c], ph_fb), 120000);
                if (a > 31000) a = 31000;
                for (k = 0; k < 4u; k++) {      /* y = a (x - y1) + x1 */
                    int32_t o = fx.ph_x[c][k] + mul15(a, y - fx.ph_y[c][k]);
                    fx.ph_x[c][k] = y;
                    fx.ph_y[c][k] = o = clip(o, 150000);
                    y = o;
                }
                fx.ph_last[c] = y;
                if (c) R += mul15((y - x) >> 1, ph_mix); else L += mul15((y - x) >> 1, ph_mix);
            }
        }
        if (!lfx_frz) fz_on = 0;
        if (P[DL_ON] && lfx_frz) {              /* FREEZE: the delay's last cycle replayed as it is, read and written back unchanged (no input, no
                                                 * filter, no gain: nothing can build up); the seam crossfaded once when it starts (1.0.48) */
            int32_t o_fz[2];
            uint32_t r;
            if (!fz_on) {
                uint32_t j, N = 256u;
                fz_on = 1;
                fz_T = (uint32_t)(fx.dl_t >> 8);
                if (fz_T < 2u * N) fz_T = 2u * N;
                if (fz_T > DL_N - 2u) fz_T = DL_N - 2u;
                for (k = 0; k < 2u; k++)        /* (the loop's end eased into what came before its start: no click at the join) */
                    for (j = 0; j < N; j++) {
                        uint32_t e = (fx.dl_w - N + j) & (DL_N - 1u), sidx = (fx.dl_w - fz_T - N + j) & (DL_N - 1u);
                        int32_t a = (int32_t)(j * 128u / N);
                        dl_buf[k][e] = (int16_t)((dl_buf[k][e] * (128 - a) + dl_buf[k][sidx] * a) >> 7);
                    }
            }
            r = (fx.dl_w - fz_T) & (DL_N - 1u);
            for (k = 0; k < 2u; k++) {
                int32_t v = dl_buf[k][r];
                dl_buf[k][fx.dl_w & (DL_N - 1u)] = (int16_t)v;
                o_fz[k] = v;
            }
            fx.dl_w++;
            {
                int32_t mx = dl_mix > 22000 ? dl_mix : 22000;   /* (heard even with a low MIX) */
                L += mul15(o_fz[0], mx);
                R += mul15(o_fz[1], mx);
            }
        } else if (P[DL_ON]) {                  /* the delay: PING-PONG, TAPE (wow, saturated, darker), DIGITAL, DUB */
            int32_t o[2], t = fx.dl_t, lpk = dlt == 2u ? 32767 : dlt == 3u ? dl_lpk * 2 / 3 : dlt == 1u ? dl_lpk * 3 / 4 : dl_lpk;
            int ping = dlt == 0u || dlt == 3u;
            dl_fbd = dlt == 1u ? dl_fb * 4 / 5 : dlt == 2u ? (dl_fb > 30000 ? 30000 : dl_fb) : dlt == 3u ? (dl_fb + (dl_fb >> 3) > 31500 ? 31500 : dl_fb + (dl_fb >> 3)) : dl_fb;
            /* (the loop's gain kept under 1 on every type: TAPE's saturation adds a quarter, DUB an eighth; 1.0.36, they rang on for ever) */
            fx.dl_t += (dl_t - fx.dl_t) >> 11;
            if (dlt == 1u) {                    /* TAPE: wow, about 0.4 Hz, a little flutter on top */
                fx.dl_wow += 39000u;
                t += (int32_t)(((int64_t)sin32(fx.dl_wow) * (fx.dl_t >> 9)) >> 15) + (sin32(fx.dl_wow * 13u) >> 9);
                if (t < 512) t = 512;
            }
            for (k = 0; k < 2u; k++) {
                uint32_t r = (fx.dl_w - (uint32_t)(t >> 8)) & (DL_N - 1u);
                int32_t a = dl_buf[k][r], b = dl_buf[k][(r - 1u) & (DL_N - 1u)];
                o[k] = a + (((b - a) * (t & 255)) >> 8);
            }
            for (k = 0; k < 2u; k++) {
                int32_t fbk = mul15(o[ping ? k ^ 1u : k], dl_fbd), ink = ping ? (k == 0u ? (x >> 1) : 0) : (x >> 1), w2;
                fx.dl_lp[k] += mul15(fbk - fx.dl_lp[k], lpk);
                fx.dl_hp[k] += (fx.dl_lp[k] - fx.dl_hp[k]) >> (dlt == 2u ? 12 : dlt == 3u ? 6 : 7);
                w2 = ink + fx.dl_lp[k] - fx.dl_hp[k];
                dl_buf[k][fx.dl_w & (DL_N - 1u)] = (int16_t)(dlt == 2u ? clip(w2, 32767) : sat15(dlt == 1u ? w2 + (w2 >> 2) : w2));
            }
            fx.dl_w++;
            L += mul15(o[0], dl_mix);
            R += mul15(o[1], dl_mix);
        }
        if (P[RV_ON] && !ks_shed) {             /* a small Freeverb: 4 combs and 2 all-passes a side */
            int32_t rin, acc[2] = {0, 0};
            uint32_t base = 0;
            fx.rv_hp += (x - fx.rv_hp) >> 5;
            rin = (x - fx.rv_hp) >> 3;
            if (rvt == 3u) {                    /* SPRING: a band (about 300 Hz .. 3 kHz) into the short combs */
                fx.sp_hp += (rin - fx.sp_hp) >> 4;
                fx.sp_lp += ((rin - fx.sp_hp) - fx.sp_lp) >> 1;
                rin = fx.sp_lp << 1;
            }
            for (k = 0; k < 8u; k++) {
                int16_t *c = rv_c + base;
                uint32_t len = (RV_LEN[k] * RV_SC[rvt]) >> 8;
                int32_t y;
                if (fx.rv_i[k] >= len) fx.rv_i[k] = 0;
                y = c[fx.rv_i[k]];
                fx.rv_lp[k] = y + mul15(fx.rv_lp[k] - y, rv_d2);
                c[fx.rv_i[k]] = (int16_t)clip(rin + mul15(fx.rv_lp[k], rv_fb2), 32767);
                if (++fx.rv_i[k] >= len) fx.rv_i[k] = 0;
                acc[k >> 2] += y;
                base += RV_LEN[k];
            }
            base = 0;
            for (k = 0; k < 4u; k++) {
                int16_t *a = rv_a + base;
                uint32_t len = (AP_LEN[k] * (rvt == 3u ? 128u : RV_SC[rvt])) >> 8;
                int32_t b, in2 = acc[k >> 1];
                if (fx.ap_i[k] >= len) fx.ap_i[k] = 0;
                b = a[fx.ap_i[k]];
                a[fx.ap_i[k]] = (int16_t)clip(in2 + (b >> 1), 32767);
                acc[k >> 1] = b - in2;
                if (++fx.ap_i[k] >= len) fx.ap_i[k] = 0;
                base += AP_LEN[k];
            }
            L += mul15(acc[0], rv_mix);
            R += mul15(acc[1], rv_mix);
        }
        if (boost) {                            /* sub boost (mono, both sides) */
            fx.sub_lp += (x * 256 - fx.sub_lp) >> 8;
            L += mul15(fx.sub_lp >> 8, boost);
            R += mul15(fx.sub_lp >> 8, boost);
        }
        if (lfx_flt) {                          /* the DJ filter: a state-variable pair, - low pass, + high pass */
            int32_t c[2] = {L, R};
            for (k = 0; k < 2u; k++) {
                int32_t lo, hi, o;
                if (!k) { dj_f += dj_df; dj_mix += dj_dm; }
                lv.dj_l[k] += mul15(dj_f, lv.dj_b[k]);
                hi = clip(c[k], 120000) - lv.dj_l[k] - mul15(dj_q, lv.dj_b[k]);
                lv.dj_b[k] = clip(lv.dj_b[k] + mul15(dj_f, hi), 200000);
                lo = lv.dj_l[k] = clip(lv.dj_l[k], 200000);
                o = lfx_flt < 0 ? lo : hi;
                c[k] += mul15(o - c[k], dj_mix);
            }
            L = c[0]; R = c[1];
        }
        if (lfx_pump || cp_pump) {              /* PUMP (FX LIVE, the compressor's): the dip on the tempo */
            uint32_t pos = pb + pinc * i;
            int32_t g = 32767;
            if (lfx_pump) g = pump_gain(pos, lfx_prate, lfx_pump);
            if (cp_pump) g = mul15(g, pump_gain(pos, (uint32_t)P[CP_PRATE], cp_pump));
            L = mul15(L, g); R = mul15(R, g);
        }
        if (eq_mask) {                          /* the EQ: the bands in use, in turn */
            int32_t c2[2] = {L, R};
            uint32_t ch, b;
            for (ch = 0; ch < 2u; ch++)
                for (b = 0; b < 4u; b++) {
                    const int32_t *q = eq_c[b];
                    int32_t *z = eq_st[ch][b], y;
                    if (!(eq_mask >> b & 1u)) continue;
                    int64_t acc = (int64_t)q[0] * c2[ch] + (int64_t)q[1] * z[0] + (int64_t)q[2] * z[1] - (int64_t)q[3] * z[2] - (int64_t)q[4] * z[3] + z[4];
                    y = (int32_t)(acc >> 24);
                    z[4] = (int32_t)(acc - ((int64_t)y << 24));   /* (fraction saving: a narrow notch's poles, near the circle, made a whistle of the rounding) */
                    y = clip(y, 400000);
                    z[1] = z[0]; z[0] = c2[ch]; z[3] = z[2]; z[2] = y;
                    c2[ch] = y;
                }
            L = c2[0]; R = c2[1];
        }
        if (ot_amt) {                           /* MULTIBAND (OTT): low < 120 Hz < mid < 2.5 kHz < high, each brought toward its level */
            int32_t c2[2], ch;
            c2[0] = L; c2[1] = R;
            for (ch = 0; ch < 2; ch++) {
                int32_t x = c2[ch], lw, hg, md, y;
                fx.ot_l[ch][0] += (int32_t)(((int64_t)((x << 8) - fx.ot_l[ch][0]) * 534) >> 15);
                fx.ot_l[ch][1] += (int32_t)(((int64_t)(fx.ot_l[ch][0] - fx.ot_l[ch][1]) * 534) >> 15);
                fx.ot_h[ch] += (int32_t)(((int64_t)((x << 8) - fx.ot_h[ch]) * 11670) >> 15);
                lw = fx.ot_l[ch][1] >> 8;
                hg = x - (fx.ot_h[ch] >> 8);
                md = x - lw - hg;
                ot_sum[0] += lw < 0 ? -lw : lw; ot_sum[1] += md < 0 ? -md : md; ot_sum[2] += hg < 0 ? -hg : hg;
                y = (int32_t)(((int64_t)lw * ot_gl + (int64_t)md * ot_gm + (int64_t)hg * ot_gh) >> 12);
                c2[ch] = x + mul15(y - x, ot_amt);
            }
            L = c2[0]; R = c2[1];
        }
        if (mba) {                              /* MONO BASS: under the frequency, both sides get the same */
            int32_t ml;
            fx.mb[0][0] += (int32_t)(((int64_t)((L << 8) - fx.mb[0][0]) * mba) >> 15);
            fx.mb[0][1] += (int32_t)(((int64_t)(fx.mb[0][0] - fx.mb[0][1]) * mba) >> 15);
            fx.mb[1][0] += (int32_t)(((int64_t)((R << 8) - fx.mb[1][0]) * mba) >> 15);
            fx.mb[1][1] += (int32_t)(((int64_t)(fx.mb[1][0] - fx.mb[1][1]) * mba) >> 15);
            ml = (fx.mb[0][1] + fx.mb[1][1]) >> 9;
            L += ml - (fx.mb[0][1] >> 8);
            R += ml - (fx.mb[1][1] >> 8);
        }
        if (shelf || harm) {                    /* DEEP BASS: a low shelf, ~+4 dB (MAX +7) under 90 Hz, so the lows weigh more */
            fx.lo[0] += ((((L * 256) - fx.lo[0]) >> 6) * 105) >> 7;
            fx.lo[1] += ((((R * 256) - fx.lo[1]) >> 6) * 105) >> 7;
            L += ((fx.lo[0] >> 8) * shelf) >> 8;
            R += ((fx.lo[1] >> 8) * shelf) >> 8;
            if (harm) {                         /* SUB HARMONICS (1.0.48): the lows (under ~90 Hz) driven into a soft clip, and only what it adds
                                                 * above ~120 Hz mixed in: the sub heard on small speakers, the lows themselves not louder */
                int32_t s = ((fx.lo[0] + fx.lo[1]) >> 9), h = sat15(s * 4), b;
                fx.hh += (h - fx.hh) >> 6;     /* (a low pass at ~110 Hz: what is left over it is the harmonics) */
                b = h - fx.hh;
                L += mul15(b, 14000);
                R += mul15(b, 14000);
            }
        }
        L = mul15(L, fx.gain);
        R = mul15(R, fx.gain);
        { int32_t a = L < 0 ? -L : L; if (a > meter_pk) meter_pk = a; }
        fx.dco[0] += (L * 256 - fx.dco[0]) >> 12;   /* (no DC out; Q8, so it settles at 0) */
        fx.dco[1] += (R * 256 - fx.dco[1]) >> 12;
        L -= fx.dco[0] >> 8;
        R -= fx.dco[1] >> 8;
        if (ks_norm) {                          /* AUTO LEVEL: the gain that follows the level (below, a block at a time) */
            L = (int32_t)(((int64_t)L * ng) >> 12);
            R = (int32_t)(((int64_t)R * ng) >> 12);
            nsum += (L < 0 ? -L : L) + (R < 0 ? -R : R);
        }
        L += dkb[i] * 3 / 2;                       /* the drums: after AUTO LEVEL (it does not pump them), before MASTER and the limiter */
        R += dkb[i] * 3 / 2;
        L = (int32_t)(((int64_t)L * vol * makeup) >> 24);
        R = (int32_t)(((int64_t)R * vol * makeup) >> 24);
        {                                       /* the limiter (always on): the gain falls at once under the ceiling, comes back in ~50 ms */
            int32_t a = L < 0 ? -L : L, b = R < 0 ? -R : R, pk = a > b ? a : b;
            if (pk > 29000) { int32_t t = (int32_t)((29000u << 15) / (uint32_t)(pk < 4000000 ? pk : 4000000)); if (t < lg) lg = t; else lg += (32768 - lg) >> 11; }
            else if (lg < 32768) lg += (32768 - lg) >> 11;
            L = sat15(mul15(L, lg));
            R = sat15(mul15(R, lg));
        }
        if (ks_sync) {                          /* SYNC OUT: the left a pulse every eighth while it plays (POCKET OPERATOR, VOLCA: 2 a beat), the right the sound in mono */
            uint32_t e8 = ((pb + BEATS_WRAP - pinc * (n - (uint32_t)i)) >> 23) % 96u;   /* (pb: the clock after this block's voices) */
            if (e8 != sy_e8) { sy_e8 = e8; if (sq_play) sy_n = 110u; }   /* (2.5 ms) */
            out[2u * i] += sy_n ? (sy_n--, KS_SYNC_AMP) : 0;
            out[2u * i + 1u] += (L + R) >> 1;
        } else {
            out[2u * i] += L;
            out[2u * i + 1u] += R;
        }
        if (rs_want) {                          /* RESAMPLE: 4 samples averaged, one kept */
            rs_acc += (L + R) >> 1;
            if (++rs_n == 4u) {
                uint32_t at = rs_pos;
                rs_buf[at] = rs_enc(rs_acc >> 2);
                rs_acc = 0; rs_n = 0;
                if (++at >= rs_want) { rs_desc.len = at; rs_want = 0; }
                rs_pos = at;
            }
        }
        if (!(i & 3u)) scope[scope_w++ & 511u] = (int16_t)((L + R) >> 1);
    }
    fx.lg = lg;
    if (ot_amt) {                               /* MULTIBAND: each band's gain toward the square root of target / level (x0.25 .. x3) */
        static const int32_t T[3] = {6000, 4000, 2000};
        for (k = 0; k < 3u; k++) {
            int32_t lv = ot_sum[k] / (int32_t)(2u * n), tg = 4096, g = fx.ot_g[k];
            if (lv > 150) {
                uint32_t r = (uint32_t)((T[k] << 12) / lv), q = 0, b = 1u << 30, x = r << 12 > (1u << 30) ? (1u << 30) : r << 12;
                while (b > x) b >>= 2;
                while (b) { if (x >= q + b) { x -= q + b; q = (q >> 1) + b; } else q >>= 1; b >>= 2; }
                tg = clampi((int32_t)q, 1024, 12288);
            }
            g += (tg - g) >> (tg < g ? 1 : 4);
            fx.ot_g[k] = g;
            ott_show[k] = g;
        }
    }
    if (!ks_norm) fx.ng = 4096;
    else {                                      /* the level (a mean of |L| + |R|), and the gain toward 8000: up in ~6 s, down in ~0.7 s, still in silence */
        int32_t a = nsum / (int32_t)(2u * n), e = fx.nenv + ((a - fx.nenv) >> 3);
        fx.nenv = e;
        if (e > 600) {
            int32_t err = 8000 - e;
            ng += (int32_t)(((int64_t)err * ng) >> (err > 0 ? 22 : 20));
            fx.ng = clampi(ng, 2048, 10240);    /* (x0.5 .. x2.5) */
        } else fx.ng = ng;
    }
}

/* ================================================================ the two cores */
#ifndef KS_DUAL
#define KS_DUAL 0
#endif
static int32_t vbus[2][HALF_FRAMES], vzero[HALF_FRAMES];
static volatile uint32_t c1_req, c1_done, c1_park_req, c1_parked;
static volatile uint8_t dual_on;                /* the second core runs (fm1_cpu1_start said yes) */
static volatile uint8_t c1_failed;              /* core 1 could not keep up: back on one core (this session) */
enum { DW_NONE, DW_SILENT, DW_SLOW, DW_FLASH, DW_SAFE };
static volatile uint8_t dual_why;               /* why two cores do not run now (SYSTEM, the start screen) */
static volatile uint32_t c1_cut;                /* blocks core 1 could not finish (too slow) */
static volatile uint32_t ks_load[2];            /* % of a block, per core (the SYSTEM page) */

/* the load governor (as KAWA's, after Felucca's shed_check): two blocks in a row over 70 % and the sound thins
 * out a step: 1 the reverb and the chorus, 2 two voices and 3 saws a SUPER, 3 one voice and 2 saws. Two seconds
 * under 45 %: a step back. The sound thins out instead of crackling, and the watchdog never sees a block that
 * does not end (voices_render stops at its budget) */
static void governor(uint32_t pct, uint32_t bad)   /* bad: a block late or cut short (1.0.33: a step at once, and 10 s before a step back) */
{
    static uint8_t hist;
    static uint32_t calm, hold;
    hist = (uint8_t)(hist << 1 | (pct > 70u));
    if ((bad || (hist & 3u) == 3u || pct > 92u) && ks_shed < 3u) { ks_shed++; hist = 0; calm = 0; }   /* (past 92 %: at once) */
    if (bad) { hold = 1720u; calm = 0; }        /* (1720 blocks: 10 s) */
    if (pct < 45u && !bad) { if (++calm > (hold ? hold : 344u) && ks_shed) { ks_shed--; calm = 0; hold = 0; } }   /* 344 blocks: 2 s */
    else calm = 0;
}

#if KS_DUAL
static void core1_main(void)                    /* core 1: the voices of the next block, again and again */
{
    for (;;) {
        uint32_t r, t0;
        if (c1_park_req) { FL_FAR(fm1_cpu1_park)(&c1_park_req, &c1_parked); continue; }   /* (the flash is busy: in RAM) */
        KS_SYNC();
        r = c1_req;
        if (r == c1_done) continue;
        t0 = fm1_ticks();
        voices_render(vbus[r & 1u], HALF_FRAMES, t0, BLOCK_TICKS * 9u / 10u);
        ks_load[1] = (fm1_ticks() - t0) / BLOCK_TICKS100;
        if (ks_load[1] >= 90u) c1_cut++;        /* (out of time: the block's end silent) */
        KS_SYNC();
        c1_done = r;
    }
}
/* the main loop, around a flash erase or write: core 1 waits in RAM (or, if it does not answer, stops) */
static void c1_pause(void)
{
    uint32_t t0 = fm1_ticks();
    if (!dual_on) return;
    c1_park_req = 1;
    KS_SYNC();
    while (!c1_parked && fm1_ticks() - t0 < 20000u * FM1_TICKS_PER_US) {}
    if (!c1_parked) { fm1_cpu1_stop(); dual_on = 0; c1_park_req = 0; dual_why = DW_FLASH; }
}
static void c1_resume(void) { c1_park_req = 0; KS_SYNC(); }
static void app_flash_busy(void)                /* an update, UBOOT, a crash: core 1 stops for good */
{
    if (!dual_on) return;
    c1_pause();
    fm1_cpu1_stop();
    dual_on = 0;
    c1_park_req = 0;
}
#else
static void c1_pause(void) {}
static void c1_resume(void) {}
static void app_flash_busy(void) {}
#if defined(DINGHY_HOST) || defined(KAWA_WASM)        /* real hardware: fm1_cpu1.h's own fm1_cpu1_stop is still there to reach core 1's reset bit */
static void fm1_cpu1_stop(void) {}
#endif
#endif

static void app_audio(int32_t *out, uint32_t n)
{
    static uint32_t hist, cut0, warm, fx_t;     /* fx_t: what the effects took last block (the voices get the rest) */
    uint32_t t0 = fm1_ticks(), l1 = 0, t1, room = BLOCK_TICKS * 8u / 10u > fx_t + BLOCK_TICKS / 4u ? BLOCK_TICKS * 8u / 10u - fx_t : BLOCK_TICKS / 4u, late0 = ks_late, bad1 = 0;
    if (dual_on && !c1_park_req) {              /* core 1 did this block's voices during the last one */
        uint32_t cur = c1_req, bad, k, nb = 0;
        while (c1_done != cur && fm1_ticks() - t0 < (room < BLOCK_TICKS / 2u ? room : BLOCK_TICKS / 2u)) { KS_SYNC(); }   /* (never past what the effects leave) */
        t0 = fm1_ticks();
        bad = c1_done != cur || c1_cut != cut0; /* (late, or a block it cut short) */
        cut0 = c1_cut;
        if (warm < 64u) { warm++; bad = c1_done != cur; }   /* (its first blocks: only lateness counts) */
        bad1 = warm >= 64u && bad;
        hist = hist << 1 | bad;
        for (k = hist; k; k &= k - 1u) nb++;
        if (c1_done == cur && nb < 6u) {        /* (6 bad blocks in the last 32: core 1 does not keep up) */
            c1_req = cur + 1u;                  /* core 1 starts the next block now */
            KS_SYNC();
            t1 = fm1_ticks();
            fx_render(vbus[cur & 1u], out, n);
            fx_t = fm1_ticks() - t1;
        } else if (nb < 6u) {
            ks_late++;                          /* (core 1 late: a silent block, it goes on) */
            fx_render(vzero, out, n);
        } else {                                /* it does not keep up (or is stuck): one core from here */
            hist = 0;
            fm1_cpu1_stop();
            dual_on = 0;
            c1_failed = 1;
            dual_why = DW_SLOW;
            voices_render(vbus[0], n, t0, room / 2u);
            fx_render(vbus[0], out, n);
        }
        l1 = ks_load[1];
    } else if (dual_on)
        fx_render(vzero, out, n);               /* core 1 waits for the flash: silence, as on core 0 */
    else {                                      /* one core: the voices up to what the effects leave of 80 % */
        voices_render(vbus[0], n, t0, room);
        t1 = fm1_ticks();
        fx_render(vbus[0], out, n);
        fx_t = fm1_ticks() - t1;
    }
    ks_load[0] = (fm1_ticks() - t0) / BLOCK_TICKS100;
    governor(ks_load[0] > l1 ? ks_load[0] : l1, bad1 || ks_late != late0);
}
