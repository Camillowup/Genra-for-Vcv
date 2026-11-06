/* SPDX-License-Identifier: GPL-3.0-only */
/* K-WASUB: the eight pages, the panel, the presets, MIDI, app_main.
 *
 *   HOME 01 PLAY     EDIT 02 BASS     ENV 03 ENVELOPE  SEQ 04 FILTER + SEQ BASS
 *   LFO  05 LFO      FX   06 FX       ARP 07 PRESETS + ARP  GLO 08 SYSTEM
 *   the page's button again, or SEL: its next view (OSC A, OSC B, SUB, COMBINE...)
 *   SELECT: the row   ALGORITHM: the row's value   K1..K4: what the footer says   PRESETS: the next patch
 *   PLAY: SEQ BASS play / stop   REC: TAP tempo (SEQ view: step record)   OCT- / OCT+: octave (both: all off)
 *   SAVE: save; held: OCT- UNDO, OCT+ REDO (PRESETS: FAVORITE)
 * Held buttons (a loading bar says what comes): a page button 1.2 s: RANDOM on that page; with K1..K4: that page's
 * knobs from anywhere (SHIFT); LFO or ENV held + a knob or ALGO: assigns LFO 1 / the mod envelope to it (QUICK ASSIGN);
 * SEL held: hear the saved sound (COMPARE); SAVE held on PRESETS: FAVORITE (elsewhere UNDO); PLAY held: HOLD.
 * Page buttons, SEL and PLAY act on release. */
enum { PG_PLAY, PG_BASS, PG_ENV, PG_FILT, PG_LFO, PG_FX, PG_PRE, PG_SYS, NPAGE };
static const uint8_t PAGE_BTN[NPAGE] = {BTN_HOME, BTN_EDIT, BTN_ENV, BTN_SEQ, BTN_LFO, BTN_FX, BTN_ARP, BTN_GLO};
enum { LY_PLAY, LY_LIST, LY_BOX, LY_STEP, LY_LFO, LY_PRESET, LY_SAVE, LY_SYS, LY_EQ };
enum { I_L1RATE = 140, I_L2RATE, I_ABMIX, G_MIDICH, G_BPM, G_CLOCK, G_OCT, G_CORES, G_CPU, G_FW, G_THEME, G_PAL, G_FONT, G_GRID, G_BROWSE, G_ACCEL, G_C3COL, G_UI, G_ASK, G_ZOOM, G_MLEARN, G_MMAP, G_MSTART, G_MPROG, G_MCLK,
       G_SQ_STEP, G_SQ_NOTE, G_SQ_TYPE, G_SQ_PAT, G_SQ_LEN, G_SQ_RATE, G_SQ_SWING,   /* SEQ BASS */
       G_AR_ON, G_AR_MODE, G_AR_RATE, G_AR_OCT, G_AR_GATE,                          /* ARP */
       G_LV_FLT, G_LV_PUMP, G_LV_STUT, G_LV_TAPE, G_LV_PRATE, G_LV_FRES,             /* SEL: FX LIVE */
       G_MC1, G_MC2, G_MC3, G_MC4, G_MC_DST, G_MC_AMT,                               /* SEL: MACROS */
       G_EQ_F, G_EQ_G, G_EQ_T, G_EQ_Q,                                               /* FX held: the EQ (the band eq_cur) */
       G_RST, G_CON, G_NORM, G_DEEP,                                                 /* SYSTEM: why the last stop; CONTRAST; AUTO LEVEL; DEEP BASS */
       G_RS_GO, G_RS_LEN,                                                            /* OSC C: RESAMPLE (record), its length */
       G_BRI, G_QLFO, G_ENG8, G_ASAVE, G_LANG,                                        /* SYSTEM: BRIGHTNESS; which LFO LFO held assigns; ENGINES (4 or 8); AUTOSAVE; LANGUAGE */
       G_SQ_PROB, G_SQ_COND,                                                         /* SEQ BASS: the step's chance, its condition (last: MIDI LEARN keeps the ids) */
       G_SQ_GATE, G_SQ_PRESET,                                                       /* SEQ BASS: the step's note length; a pattern from the library */
       G_DK_LVL, G_DK_TUNE, G_DK_DEC, G_DK_TONE, G_DK_TRK, G_DK_STEP, G_DK_HIT, G_DK_PROB, G_DK_COND,   /* DRUMS: the track's sound; the step */
       G_DK_PAT, G_DK_PRE, G_DK_LEN, G_DK_KIT, G_DK_VOL, G_DK_MODE, G_DK_SWING,       /* DRUMS: the pattern, the kit */
       G_SYNC,                                                                       /* SYNC OUT (POCKET OPERATOR, VOLCA) */
       G_SQ_LKP, G_SQ_LKV,                                                           /* SEQ BASS: the step's PARAMETER LOCK (what, its value) */
       G_SQ_ON,                                                                      /* SEQ BASS: the sequence ON / OFF (OFF: the drums play, the bass is free to play live) */
       G_NEWSONG,                                                                    /* SYSTEM: NEW SONG (the sound, every pattern, the locks and the automation back to empty) */
       G_L3_RATE, G_L3_DEP, G_L3_SHP, G_L3_DST, G_L3_SYNC,                           /* LFO 3 (in the settings: whatever the patch) */
       G_DK_KSAT, G_DK_SNAP, G_DK_ECHO,                                              /* DRUMS: KICK DRIVE, SNAPPY, DUB ECHO (1.0.32, in dk.pad) */
       G_MIDIMON, G_MIDIRT,                                                          /* MIDI: the last message that came in, and the last system one (1.0.46) */
       I_NONE = 255 };

typedef struct {
    uint8_t lay;
    const char *cat;
    uint8_t n, it[25];                          /* the rows, in the order shown: K1..K4 are the first four */
    const char *tag;
} view_t;
#define OSCV(L, X) {LY_LIST, "OSCILLATOR " L, 11, {X##_ENG, X##_P1, X##_P2, X##_P3, X##_P4, X##_LVL, X##_OCT, X##_SEMI, X##_FINE, V_MODE, V_GLIDE}, "EDIT"}   /* (the voice mode and the glide at the bottom: one voice plays both oscillators) */
#define FXV(...) {LY_LIST, "FX RACK", 4, {__VA_ARGS__}, "FX"}
static const view_t VIEWS[] = {
    {LY_PLAY, "CURRENT PATCH", 4, {F_CUT, I_L1RATE, L1_DEP, F_RES}, "HOME"},
    {LY_PLAY, "LIVE MIX", 4, {S_LVL, DR_AMT, DL_MIX, MS_VOL}, "HOME"},
    OSCV("A", A), OSCV("B", B),
    {LY_LIST, "SUB + NOISE", 7, {S_LVL, S_OCT, S_WAVE, N_LVL, S_LOCK, N_COL, S_ROUTE}, "EDIT"},
    {LY_LIST, "COMBINE A + B", 13, {I_ABMIX, X_MODE, X_AMT, B_ENG, A_ENG, A_LVL, B_LVL, A_OCT, B_OCT, B_SEMI, B_FINE, X_DRIFT, X_RESET}, "EDIT"},   /* (HOME held opens it: the two oscillators together) */
    {LY_LIST, "OSCILLATOR C \xB7 SAMPLE", 11, {SM_SEL, SM_LVL, SM_START, SM_END, G_RS_GO, G_RS_LEN, SM_SPEED, SM_DIR, SM_MODE, SM_TUNE, SM_KEY}, "EDIT"},
    {LY_BOX, "AMP ENVELOPE", 4, {AE_A, AE_D, AE_S, AE_R}, "ENV"},
    {LY_BOX, "FILTER ENVELOPE", 4, {FE_A, FE_D, FE_S, FE_R}, "ENV"},
    {LY_BOX, "MOD ENVELOPE", 4, {ME_A, ME_D, ME_AMT, ME_DST}, "ENV"},
    {LY_BOX, "FILTER / COLOUR", 4, {F_CUT, F_RES, F_DRV, F_TYPE}, "FX"},
    {LY_LIST, "FILTER MOD", 6, {F_ENV, F_KEY, F_VEL, F_TYPE, F_CUT, F_RES}, "FX"},
    {LY_LIST, "SEQ BASS", 14, {G_SQ_ON, G_SQ_STEP, G_SQ_NOTE, G_SQ_GATE, G_SQ_TYPE, G_SQ_PROB, G_SQ_COND, G_SQ_LKP, G_SQ_LKV, G_SQ_PAT, G_SQ_PRESET, G_SQ_LEN, G_SQ_RATE, G_SQ_SWING}, "SEQ"},
    {LY_LIST, "DRUMS", 19, {G_DK_LVL, G_DK_TUNE, G_DK_DEC, G_DK_TONE, G_DK_TRK, G_DK_STEP, G_DK_HIT, G_DK_PROB, G_DK_COND, G_DK_PAT, G_DK_PRE,
                            G_DK_LEN, G_DK_KIT, G_DK_VOL, G_DK_KSAT, G_DK_SNAP, G_DK_ECHO, G_DK_MODE, G_DK_SWING}, "SEQ"},   /* (black keys: the track, white: its steps, OCT: the page) */
    {LY_LFO, "LFO 1 \xB7 WOBBLE", 9, {I_L1RATE, L1_DEP, L1_SHP, L1_DST, G_GRID, L1_SYNC, L1_TRIG, L1_PH, L1_SMO}, "LFO"},
    {LY_LFO, "LFO 2 \xB7 MOTION", 9, {I_L2RATE, L2_DEP, L2_SHP, L2_DST, G_GRID, L2_SYNC, L2_TRIG, L2_PH, L2_SMO}, "LFO"},
    {LY_STEP, "STEP FX", 7, {ST_PAT, ST_RATE, ST_GATE, ST_DEP, ST_ON, ST_DST, ST_SMO}, "LFO"},
    {LY_LIST, "MOD MATRIX", 12, {M1_AMT, M2_AMT, M3_AMT, M4_AMT, M1_SRC, M1_DST, M2_SRC, M2_DST, M3_SRC, M3_DST, M4_SRC, M4_DST}, "LFO"},
    {LY_LIST, "LFO 3 \xB7 GLOBAL", 5, {G_L3_RATE, G_L3_DEP, G_L3_SHP, G_L3_DST, G_L3_SYNC}, "LFO"},   /* (1.0.17: the third LFO, whatever the patch) */
    FXV(DR_ON, DR_AMT, DR_MIX, DR_TYPE),       /* (SATURATION, REVERB, DELAY first, each with its ON first (1.0.37); MASTER last) */
    {LY_LIST, "FX RACK", 5, {RV_ON, RV_SIZE, RV_MIX, RV_TYPE, RV_DAMP}, "FX"},
    {LY_LIST, "FX RACK", 6, {DL_ON, DL_TIME, DL_FB, DL_MIX, DL_TYPE, DL_TONE}, "FX"},
    {LY_LIST, "FX RACK", 3, {OT_AMT, OT_LOW, OT_HIGH}, "FX"},
    {LY_LIST, "FX RACK", 6, {CP_ON, CP_AMT, CP_PUMP, CP_ATK, CP_REL, CP_PRATE}, "FX"},
    {LY_EQ, "EQ", 0, {0}, "FX"},                /* (the EQ: opens as the window it is, win 3) */
    FXV(CH_ON, CH_RATE, CH_DEP, CH_MIX),
    {LY_LIST, "FX RACK", 5, {PH_ON, PH_RATE, PH_DEP, PH_FB, PH_MIX}, "FX"},
    FXV(CR_ON, CR_BITS, CR_RATE, CR_MIX),
    {LY_LIST, "FX RACK", 4, {MS_VOL, MS_GLUE, MS_SUB, MB_MONO}, "FX"},
    {LY_PRESET, "PRESET BANK A", 0, {0}, "PRESET"},
    {LY_PRESET, "PRESET BANK B", 0, {0}, "PRESET"},
    {LY_SAVE, "SAVE PATCH", 0, {0}, "PRESET"},
    {LY_LIST, "ARPEGGIATOR", 5, {G_AR_ON, G_AR_MODE, G_AR_RATE, G_AR_OCT, G_AR_GATE}, "ARP"},
    {LY_SYS, "GLOBAL SETUP", 24, {G_BPM, V_MODE, G_UI, G_THEME, G_PAL, V_GLIDE, G_QLFO, G_BRI, G_ASAVE, G_LANG, G_DEEP, G_NORM, G_ACCEL, G_GRID, V_BEND, V_VEL,
                                  G_FONT, G_ZOOM, G_C3COL, G_BROWSE, G_ASK, G_NEWSONG, G_CPU, G_RST}, "SYSTEM"},
    {LY_SYS, "MIDI", 10, {G_MLEARN, G_MMAP, G_MIDICH, G_CLOCK, G_MSTART, G_MPROG, G_MCLK, G_SYNC, G_MIDIMON, G_MIDIRT}, "SYSTEM"},
};
/* what K1..K4 turn: the first four rows (the preset lists and SAVE have their own) */
static uint32_t vknob(const view_t *v, uint32_t i) { return v->lay == LY_PRESET || v->lay == LY_SAVE || i >= v->n ? I_NONE : v->it[i]; }
#define NFXV 10u                                /* the FX page's views, in order: their ON (0: none), their title */
static const uint8_t FX_ON[NFXV] = {DR_ON, RV_ON, DL_ON, 0, CP_ON, 0, CH_ON, PH_ON, CR_ON, 0};
static const uint8_t PAGE_V0[NPAGE + 1] = {0, 2, 7, 10, 14, 19, 29, 33, 35};   /* each page's first view */
#define V_EQ 24u
#define ARP_VIEW 3u                             /* the ARP button's page: PRESET BANK A, B, SAVE, then ARPEGGIATOR (its view 3); the button opens it first */
#define V_SMP 6u
#define V_SEQ 12u                               /* SEQ BASS (the SEQ button's third view), DRUMS (its fourth), ARP (the ARP button's last) */
#define V_DRUM 13u
#define V_ARP 32u
static const char *const FX_TITLE[NFXV] = {"SATURATION", "REVERB", "DELAY", "MULTIBAND", "COMPRESSOR", "EQ", "CHORUS", "PHASER", "BITCRUSH", "MASTER"};
static const char *const FT_TITLE[NFT] = {"DARK EDGE", "SOFT EDGE", "BAND RAZOR", "THIN AIR", "HOLLOW", "TALK BOX", "METAL COMB"};
static const char *const FT_SUB[NFT] = {"LOW PASS \xB7 24 dB", "LOW PASS \xB7 12 dB", "BAND PASS \xB7 12 dB", "HIGH PASS \xB7 24 dB",
                                        "NOTCH \xB7 12 dB", "FORMANT \xB7 A E I O U", "COMB \xB7 TUNED BY CUTOFF"};

/* ---- state */
#define NUSER 32u
typedef struct {
    uint8_t magic, sum;
    char name[14];
    int8_t p[NP_STORE];
} upatch_t;                                     /* 128 B: 32 of them fill one 4 KiB sector of the store */
#define UP_MAGIC 0xB5u
static upatch_t ustore[NUSER];
#define UX_MAGIC 0x5Au                          /* (0x59: 20 a slot, before OTT / MONO BASS / SUB LOCK; 0x58: 16, before PLAY and SPEED) */
#define UX_AT 0x400u                            /* (in the 2nd sector, after SEQ BASS / ARP / MACROS) */
static struct {                                 /* the user slots' settings past NP_STORE (compressor, phaser, types) */
    uint8_t magic, sum;
    int8_t p[NUSER][NP_MAX - NP_STORE];
} uext;
typedef char ids_fit[G_MIDIRT < I_NONE ? 1 : -1];
typedef char np_fits[NP <= I_L1RATE && NP <= NP_MAX ? 1 : -1];   /* (the patch's ids under the others') */
#define CC_MAGIC 0x44u                          /* (0x43: before 0.9.55, the ids past the patch 4 lower) */
#define WK_AT 0xC00u                            /* (the 2nd sector, after the slots' rest: the sound being made, AUTOSAVE) */
static struct {
    uint8_t magic, sum, bank, idx;
    char name[14];
    int8_t p[NP_MAX];
} work;
#define WK_MAGIC 0x57u
typedef char work_fits[WK_AT >= UX_AT + sizeof uext && WK_AT + sizeof work <= 4096u ? 1 : -1];
static uint8_t wk_sum(void) { const uint8_t *b = (const uint8_t *)&work + 2; uint32_t i, x = 0x5Au; for (i = 2; i < sizeof work; i++) x = (x * 7u + *b++) & 0xFFu; return (uint8_t)x; }
typedef char perf_fits_before_uext[sizeof perf <= UX_AT ? 1 : -1];   /* (the 2nd sector: SEQ/ARP/MACROS, then the slots' rest) */
typedef char uext_fits[UX_AT + sizeof uext <= 4096u ? 1 : -1];
static uint8_t ux_sum(void) { const uint8_t *b = (const uint8_t *)uext.p; uint32_t i, x = 0x77; for (i = 0; i < sizeof uext.p; i++) x = (x * 7u + b[i]) & 0xFFu; return (uint8_t)x; }
static void ux_migrate(void)                    /* a store saved with 16 or 20 a slot: spread out, the new ones at their default */
{
    static uint8_t old[NUSER * 20u];
    uint32_t i, x = 0x77, k, m = uext.magic == 0x58u ? 16u : uext.magic == 0x59u ? 20u : 0u, keep;
    if (!m) return;
    memcpy(old, uext.p, NUSER * m);
    for (i = 0; i < NUSER * m; i++) x = (x * 7u + old[i]) & 0xFFu;
    if (x != uext.sum) { uext.magic = 0; return; }
    keep = m == 16u ? 16u : SM_SPEED + 1u - NP_STORE;   /* (what that layout really held) */
    for (k = 0; k < NUSER; k++)
        for (i = 0; i < NP_MAX - NP_STORE; i++)
            uext.p[k][i] = i < keep ? (int8_t)old[k * m + i] : NP_STORE + i < NP ? PD[NP_STORE + i].def : 0;
    uext.magic = UX_MAGIC;
    uext.sum = ux_sum();
}
static struct {
    uint8_t ver, midich, clock, oct;            /* midich 0: omni; clock 0 internal, 1 MIDI */
    uint16_t bpm10;
    uint8_t bank, idx, cores;                   /* cores 1: both (the voices on core 1) */
    uint8_t theme, pal, font;                   /* CLASSIC STUDIO PURE RACK; its palettes; the font */
    uint8_t grid;                               /* LFO GRID: 0 ALL, 1 STRAIGHT (round divisions only) */
    uint8_t browse, fav[12];                    /* PRESETS: 0 all, 1 favorites only; the favorites (A then B) */
    uint8_t accel;                              /* the knobs' acceleration: 0 OFF, 1 ON, 2 FAST (appended: older saves load) */
    uint8_t c3col;                              /* SHIFT's three columns: 0 the theme's colour, 1 green / yellow / orange */
    uint8_t ui, ask;                            /* UI MODE: 0 CLASSIC, 1 SHIFT; ask which at each start */
    uint8_t zoom;                               /* LOUPE: the rows' and values' type as big as they fit */
    uint8_t mstart, mprog, mclk;                /* MIDI: start/stop the SEQ, program change loads, the clock out */
    uint8_t contrast;                           /* CONTRAST: 0 NORMAL, 1 HIGH (black and white), 2 MAX (+ the panel's darker gamma) */
    uint8_t norm, deep, askv;                   /* AUTO LEVEL on, DEEP BASS (0 off, 1 deep, 2 max); askv: the start question was switched off once (0.10) */
    uint8_t bri;                                /* BRIGHTNESS: 0 ECO, 1 NORMAL, 2 BRIGHT, 3 MAX */
    uint8_t qlfo;                               /* QUICK ASSIGN (LFO held + a knob): 0 LFO 1, 1 LFO 2 */
    uint8_t eng8;                               /* ENGINES: 0 ESSENTIAL (the first four: ANALOG CS WAVE FM), 1 ALL (eight) */
    uint8_t asave;                              /* AUTOSAVE: the sound being made kept every minute (back at the next start) */
    uint8_t pquant;                             /* FX PADS: the keys wait for the next 0 OFF, 1 BAR, 1/2, 1/4, 1/8, 1/16, 1/24, 1/32, 1/64 */
    uint8_t lang;                               /* LANGUAGE: 0 ENGLISH, 1 FRANCAIS, 2 ESPANOL (the view titles; the rest stays in English) */
    uint8_t sync;                               /* SYNC OUT: 1 a pulse every eighth on the left (POCKET OPERATOR SY, VOLCA), the sound in mono on the right */
    int8_t l3[NL3];                             /* LFO 3: shape, sync, rate, division, depth, destination (whatever the patch: 1.0.17) */
    uint8_t fav2[4];                            /* the favourites 96 .. 127 (1.0.32: 96 factory presets, 32 user) */
    uint8_t fav3[4];                            /* the favourites 128 .. 159 (1.0.50: 97 factory presets) */
} set = {6, 0, 0, 0xFF, 1400, 0, 0, 1, 0, 0, 0, 0, 0, {0}, 1, 0, 0, 1, 0, 1, 1, 0, 1, 1, 1, 0, 3, 1, 0, 1, 0, 0, 0, {0, 1, 64, 9, 0, D_CUT}, {0}};
static void l3_push(void) { uint32_t i; for (i = 0; i < NL3; i++) l3cfg[i] = set.l3[i]; }   /* (the settings to the audio) */
static char pname[14];
static uint8_t new_arm;                         /* SAVE, K4 turned once: turn it again to start a new preset */
static uint32_t new_ms;
static uint8_t page, vcur[NPAGE], cursor[sizeof VIEWS / sizeof VIEWS[0]], pre_cur[2], save_slot, save_pos, hold;
static uint8_t key_note[27], latched[128], phys[128];
static uint32_t prev_keys, toast_ms, set_ms, last_turn_ms, turn_start_ms, tap_ms[4];
static uint8_t set_dirty, prev_dirty = 1;
static char toast_txt[40];
static int16_t prev_wave[224], prev_a[112], prev_b[112], prev_s[112];   /* the pictures of the sound */
static uint8_t led_want[41];
#define OCT ((int32_t)(int8_t)set.oct)

static void toast(const char *s)
{
    uint32_t i;
    for (i = 0; i < 39u && s[i]; i++) toast_txt[i] = s[i];
    toast_txt[i] = 0;
    toast_ms = fm1_ms | 1u;
}
static const view_t *view(void) { return &VIEWS[PAGE_V0[page] + vcur[page]]; }
static uint32_t vidx(void) { return PAGE_V0[page] + vcur[page]; }

#if KS_DUAL
#define DUAL_MAGIC 0x4B533143u                  /* "KS1C": a start on two cores not yet confirmed (survives a reset) */
static uint32_t dual_mark[2] __attribute__((section(".noinit")));
static uint32_t dual_ms;
#endif

/* ---- the user store (one sector) */
static uint8_t sqx_dirty;
typedef char sqx_fits[sizeof sqx <= 4096u ? 1 : -1];
static uint8_t sqx_sum(void) { const uint8_t *b = (const uint8_t *)sqx.p; uint32_t i, x = 0x29; for (i = 0; i < sizeof sqx.p; i++) x = (x * 5u + b[i]) & 0xFFu; return (uint8_t)x; }
static void sqx_mark(void) { sqx.sum = sqx_sum(); sqx_dirty = 1; }   /* (the sum kept current: a backup taken any time loads back) */
static void sqx_defaults(void)                  /* steps 17..64 rests, every step 100 % and ALWAYS */
{
    memset(&sqx, 0, sizeof sqx);
    { uint32_t k; for (k = 0; k < SQ_NPAT; k++) memset(sqx.p[k].prob, 100, SQ_MAX); }
    sqx.magic = SX_MAGIC;
    sqx_mark();
}
/* DRUMS: the 4th and 5th sectors (dk is 5 KiB) */
static uint8_t dk_dirty;
typedef char dk_fits[sizeof dk <= 8192u ? 1 : -1];
static uint8_t dk_sum(void) { const uint8_t *b = (const uint8_t *)&dk; uint32_t i, x = 0x3D; for (i = 2; i < sizeof dk; i++) x = (x * 5u + b[i]) & 0xFFu; return (uint8_t)x; }
static void dk_mark(void) { dk.sum = dk_sum(); dk_dirty = 1; }
static void dk_defaults(void)                   /* the patterns empty (PLAY plays SEQ BASS as before), the sounds in the middle */
{
    static const int8_t P0[NDK][4] = {{105, 0, 64, 40}, {95, 0, 56, 70}, {90, 0, 60, 50}, {80, 0, 60, 64}, {78, 0, 60, 64},
                                      {95, 0, 64, 64}, {85, 0, 64, 64}, {70, 0, 64, 64}};
    uint32_t k;
    memset(&dk, 0, sizeof dk);
    dk.magic = DK_MAGIC; dk.vol = 100;
    memcpy(dk.prm, P0, sizeof dk.prm);
    for (k = 0; k < DK_NPAT; k++) dk.pat[k].len = 32;
    dk_mark();
}
/* PARAMETER LOCKS: the 6th sector (pl is 2 KiB) */
static uint8_t pl_dirty;
typedef char pl_fits[sizeof pl <= 4096u ? 1 : -1];
static uint8_t pl_sum(void) { const uint8_t *b = (const uint8_t *)&pl; uint32_t i, x = 0x3E; for (i = 2; i < sizeof pl; i++) x = (x * 5u + b[i]) & 0xFFu; return (uint8_t)x; }
static void pl_mark(void) { pl.sum = pl_sum(); pl_dirty = 1; }
static void pl_defaults(void) { memset(&pl, 0, sizeof pl); pl.magic = PL_MAGIC; pl_mark(); }
#if defined(KAWA_WASM)
static uint8_t w_store[24576];
static uint32_t w_store_gen;
static void store_load(void) { memcpy(ustore, w_store, sizeof ustore); memcpy(&perf, w_store + 4096, sizeof perf); memcpy(&uext, w_store + 4096 + UX_AT, sizeof uext); memcpy(&work, w_store + 4096 + WK_AT, sizeof work); memcpy(&sqx, w_store + 8192, sizeof sqx); memcpy(&dk, w_store + 12288, sizeof dk); memcpy(&pl, w_store + 20480, sizeof pl); }
static int store_save(void) { memcpy(w_store, ustore, sizeof ustore); w_store_gen++; return 0; }
static int perf_save(void) { memcpy(w_store + 4096, &perf, sizeof perf); memcpy(w_store + 4096 + UX_AT, &uext, sizeof uext); memcpy(w_store + 4096 + WK_AT, &work, sizeof work); if (sqx_dirty) { memcpy(w_store + 8192, &sqx, sizeof sqx); sqx_dirty = 0; } if (dk_dirty) { memcpy(w_store + 12288, &dk, sizeof dk); dk_dirty = 0; } if (pl_dirty) { memcpy(w_store + 20480, &pl, sizeof pl); pl_dirty = 0; } w_store_gen++; return 0; }
static int settings_write(void) { return settings_save(&set, sizeof set); }
#else
static void store_load(void)
{
    if (!flash_ok || st_read(STORE_LO, ustore, sizeof ustore)) memset(ustore, 0xFF, sizeof ustore);
    if (!flash_ok || st_read(STORE_LO + 0x1000u, &perf, sizeof perf)) memset(&perf, 0xFF, sizeof perf);
    if (!flash_ok || st_read(STORE_LO + 0x1000u + UX_AT, &uext, sizeof uext)) memset(&uext, 0xFF, sizeof uext);
    if (!flash_ok || st_read(STORE_LO + 0x1000u + WK_AT, &work, sizeof work)) memset(&work, 0xFF, sizeof work);
    if (!flash_ok || st_read(STORE_LO + 0x2000u, &sqx, sizeof sqx)) memset(&sqx, 0xFF, sizeof sqx);
    if (!flash_ok || st_read(STORE_LO + 0x3000u, &dk, sizeof dk)) memset(&dk, 0xFF, sizeof dk);
    if (!flash_ok || st_read(STORE_LO + 0x5000u, &pl, sizeof pl)) memset(&pl, 0xFF, sizeof pl);
}
static int perf_save(void)                      /* the store's second sector: SEQ BASS, ARP, MACROS, the slots' rest */
{
    uint32_t took;
    int rc;
    if (!flash_ok) return -1;
    c1_pause();
    rc = fl_erase4k(STORE_LO + 0x1000u, &took);
    if (!rc) rc = fl_write(STORE_LO + 0x1000u, (const uint8_t *)&perf, sizeof perf);
    if (!rc) rc = fl_write(STORE_LO + 0x1000u + UX_AT, (const uint8_t *)&uext, sizeof uext);
    if (!rc && work.magic == WK_MAGIC) rc = fl_write(STORE_LO + 0x1000u + WK_AT, (const uint8_t *)&work, sizeof work);
    if (!rc && sqx_dirty) {                     /* (the third sector: only when SEQ BASS's long part changed) */
        rc = fl_erase4k(STORE_LO + 0x2000u, &took);
        if (!rc) rc = fl_write(STORE_LO + 0x2000u, (const uint8_t *)&sqx, sizeof sqx);
        if (!rc) sqx_dirty = 0;
    }
    if (!rc && dk_dirty) {                      /* (DRUMS: the 4th and 5th sectors, when they changed) */
        rc = fl_erase4k(STORE_LO + 0x3000u, &took);
        if (!rc) rc = fl_erase4k(STORE_LO + 0x4000u, &took);
        if (!rc) rc = fl_write(STORE_LO + 0x3000u, (const uint8_t *)&dk, sizeof dk);
        if (!rc) dk_dirty = 0;
    }
    if (!rc && pl_dirty) {                      /* (PARAMETER LOCKS: the 6th sector) */
        rc = fl_erase4k(STORE_LO + 0x5000u, &took);
        if (!rc) rc = fl_write(STORE_LO + 0x5000u, (const uint8_t *)&pl, sizeof pl);
        if (!rc) pl_dirty = 0;
    }
    c1_resume();
    return rc;
}
static int store_save(void)                     /* (core 1 waits in RAM while the flash is busy) */
{
    uint32_t took;
    int rc;
    if (!flash_ok) return -1;
    c1_pause();
    rc = fl_erase4k(STORE_LO, &took);
    if (!rc) rc = fl_write(STORE_LO, (const uint8_t *)ustore, sizeof ustore);
    c1_resume();
    return rc;
}
static int settings_write(void)
{
    int rc;
    c1_pause();
    rc = settings_save(&set, sizeof set);
    c1_resume();
    return rc;
}
#endif
/* ---- SEQ BASS, ARP: kept apart from the patches (a line goes on when the sound changes) */
static uint8_t sq_cur_step, perf_dirty;
static uint32_t perf_ms;
static const char *const L_SQTYPE[5] = {"REST", "NOTE", "ACCENT", "SLIDE", "ACC+SLIDE"};
static const uint8_t SQ_FL[5] = {0, SF_ON, SF_ON | SF_ACC, SF_ON | SF_SLIDE, SF_ON | SF_ACC | SF_SLIDE};
static const char *const L_ARMODE[5] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER"};
static const char *const NOTE_N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
/* the first lines, one token a step: a note and its octave above C2 ("D#1"), ^ accent, ~ slide, - a rest */
static const char *const SQ_DEF[8][2] = {
    {"DNB ROLL", "C0^ - - C0 - - C0 - - - C0^ - D#0 - C0 -"},
    {"DUB STEPPA", "C0^ - C0 - G0 - F0 - C0^ - C0 - D#0 - F0~ G0"},
    {"TECHNO OFF", "- - C0 - - - C0^ - - - C0 - - - C0 C1"},
    {"HALFTIME", "C0^~ C0 - - - - - - F0^ - - - D#0~ C0 - -"},
    {"ACID", "C0 C1^ - C0~ D#0 C0 G0^ - C0 A#0~ C1 - C0^ F0 - D#0"},
    {"2-STEP", "C0^ - - C0 - - G0 - - C0 - - A#0~ G0 - -"},
    {"OCTAVES", "C0 C1 C0 C1 C0 C1 C0 C1 D#0 D#1 D#0 D#1 F0 F1 G0 G1"},
    {"ROOT", "C0 - - - C0 - - - C0 - - - C0 - - -"}};
/* a line of 16 tokens -> notes, flags, note lengths; ":" then a length: h 1/2 (the default), s 1/8, q 1/4, t 3/4,
 * 1 2 3 4 6 8 steps, c 12, g 16, x 24, w 32, z 64, H held until the next note */
static void sq_parse(const char *t, uint8_t *note, uint8_t *fl, uint8_t *gl)
{
    static const char GL_CH[17] = "hsqt123468cgxwzH";
    uint32_t i;
    for (i = 0; i < 16u; i++) { note[i] = 12; fl[i] = 0; gl[i] = 0; }
    for (i = 0; i < 16u && *t; i++) {
        while (*t == ' ') t++;
        if (*t == '-') { t++; continue; }
        if (*t >= 'A' && *t <= 'G') {
            static const uint8_t L2N[7] = {9, 11, 0, 2, 4, 5, 7};
            int32_t n = L2N[*t++ - 'A'];
            if (*t == '#') { n++; t++; }
            if (*t >= '0' && *t <= '3') n += (*t++ - '0') * 12;
            note[i] = (uint8_t)clampi(n + 12, 0, 48);
            fl[i] = SF_ON;
            while (*t == '^' || *t == '~') fl[i] |= *t++ == '^' ? SF_ACC : SF_SLIDE;
            if (*t == ':' && t[1]) { uint32_t g; for (g = 0; g < 16u && GL_CH[g] != t[1]; g++) {} gl[i] = (uint8_t)(g & 15u); t += 2; }
        }
        while (*t && *t != ' ') t++;
    }
}
/* the SEQ BASS library: each pattern in 16, 32 and 64 steps. A riff: played again, the third quarter of the 64 a fourth up,
 * the last bar ending on a fill. Long notes (stretch): the same notes spread over the length. */
static const struct { const char *name, *t; uint8_t stretch; } SQP[] = {
    {"DNB ROLL", "C0^ - - C0 - - C0 - - - C0^ - D#0 - C0 -", 0},
    {"DUB STEPPA", "C0^ - C0 - G0 - F0 - C0^ - C0 - D#0 - F0~ G0", 0},
    {"TECHNO OFF", "- - C0 - - - C0^ - - - C0 - - - C0 C1", 0},
    {"TECHNO ROLL", "- C0 C0^ C0 - C0 C0^ C0 - C0 C0^ C0 - C0 C1^ C0", 0},   /* (the kick's step left free, three 16ths after it, the off-beat pushed) */
    {"DNB ROLLER", "C0^ - - C0 - C0 - - C0^ - - C0 - C0 G0 -", 0},
    {"BOUNCE", "C0^ - C1 - - C0 - C1^ - C0 - C1 - - G0~ C1", 0},
    {"OFF BOUNCE", "- C0^:s - C0:s - C1^:s - C0:s - C0^:s - C1:s - C0^:s G0 C0", 0},
    {"SUB PULSE", "C0^:4 - - - C0:2 - C0:2 - C0^:4 - - - G0:2 - F0:2 -", 0},
    {"HALFTIME", "C0^~ C0 - - - - - - F0^ - - - D#0~ C0 - -", 0},
    {"ACID", "C0 C1^ - C0~ D#0 C0 G0^ - C0 A#0~ C1 - C0^ F0 - D#0", 0},
    {"2-STEP", "C0^ - - C0 - - G0 - - C0 - - A#0~ G0 - -", 0},
    {"OCTAVES", "C0 C1 C0 C1 C0 C1 C0 C1 D#0 D#1 D#0 D#1 F0 F1 G0 G1", 0},
    {"ROOT", "C0 - - - C0 - - - C0 - - - C0 - - -", 0},
    {"CONTINUOUS", "C0^:1 C0:1 C0:1 C0:1 D#0:1 D#0:1 C0:1 C0:1 G0^:1 G0:1 F0:1 F0:1 D#0:1 D#0:1 C0:1 C0:1", 0},
    {"LEGATO LINE", "C0^~ D#0~ F0~ G0~ A#0~ G0~ F0~ D#0~ C0^~ D#0~ F0~ G0~ D#0~ F0~ D#0~ C0", 0},
    {"ROLLING 16", "C0^ C0 C0 C0 C0^ C0 C0 C0 C0^ C0 C0 C0 C0^ C0 C0 C0", 0},
    {"DRONE", "C0^:H - - - - - - - - - - - - - - -", 1},
    {"DRONE FIFTH", "C0^:8 - - - - - - - G0:8 - - - - - - -", 1},
    {"SUSTAIN", "C0^:4 - - - D#0:4 - - - F0:4 - - - D#0:4 - - -", 1},
    {"LONG REESE", "C0^:6 - - - - - C0:2 - D#0^:6 - - - - - F0~:2 -", 1},
    {"WOBBLE HALF", "C0^:8 - - - - - - - C0:6 - - - - - G0~ C1", 1},
    {"PULSE", "C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q C0:q", 0},
    {"STACCATO 8", "C0^:s - C0:s - C0:s - C0:s - D#0^:s - D#0:s - F0:s - G0:s -", 0},
    {"GALLOP", "C0^ - C0 C0 C0 - C0 C0 C0^ - C0 C0 C0 - C0 C0", 0},
    {"HOUSE OFF", "- - C0^ - - - C0^ - - - C0^ - - - C0^ -", 0},
    {"UK GARAGE", "C0^ - - C0 - - - - - C0 - G0 - - C0~ D#0", 0},
    {"JUNGLE", "C0^ - - - - - - C0 - C0 - - G0~ C0 - -", 0},
    {"DUBSTEP", "C0^:3 - - - - - - - - - C0:2 - G0^:3 - - -", 0},
    {"NEURO STAB", "C0^:s - C0:s - - C1^:s - - C0:s - D#0^:s - - - G0:s -", 0},
    {"TRANCE ROLL", "- C0 C0 C0 - C0 C0 C0 - C0 C0 C0 - C0 C0 C0", 0},
    {"ARP MINOR", "C0 D#0 G0 C1 G0 D#0 C0 D#0 G0 C1 D#1 C1 G0 D#0 C0 G0", 0},
    {"SYNCOPE", "C0^ - - C0 - - C0^ - - C0 - - C0^ - C0 -", 0},
    {"MINIMAL", "C0:q - - - - - - - - - C0:q - - - - -", 0},
    {"WALKING", "C0 - D#0 - F0 - F#0 - G0 - A#0 - G0 - F0 -", 0},
    {"STABS", "C0^:2 - - - - - - - D#0^:2 - - - - - - -", 0}};
#define NSQP (sizeof SQP / sizeof SQP[0])
/* the DRUMS library: 8 tracks (KICK SNARE CLAP CL-HAT OP-HAT TOM PERC CYMBAL), "|" between them; x a hit, X an accent,
 * ? a hit half the time. 32 and 64: played again, a snare fill at the end (64: a cymbal on the first step) */
static const struct { const char *name, *t; } DKP[] = {
    {"TECHNO", "X...X...X...X...||....x.......x...|..x...x...x...x.|||.......x......x.|"},
    {"HOUSE", "X...X...X...X...||....x.......x...|x...x...x...x...|..x...x...x...x.|||"},
    {"DUB", "X.........x.....|........X.......||x.x.x.x.x.x.x.x.||..............x.|......x.......x.|"},
    {"DNB", "X.........X.....|....X.......X...||x.x.x.x.x.x.x.x.|.............x..|||"},
    {"JUNGLE", "X.........x.....|....X..?.?..X..?||x.xxx.xxx.xxx.xx|||.......x........|"},
    {"DUBSTEP", "X.........x.....|........X.......||x.x.x.x.x.x.x.x.||..............x.|......x......?..|"},
    {"BREAKBEAT", "X.....x...x.....|....X.......X..?||x.x.x.x.x.x.x.x.|..............x.|||"},
    {"UK GARAGE", "X......x..x.....|....X.......X...||..x...x...x...xx|.............x..|||"},
    {"ELECTRO", "X.....x.....x...|....X.......X...|....x.......x...|x.x.x.x.x.x.x.x.|||x.....x.....x...|"},
    {"MINIMAL", "X...X...X...X...||............x...|..x...x...x...x.|||...x.....x...x..|"},
    {"HALFTIME", "X.......x.x.....|........X.......||x.x.x.x.x.x.x.x.||..............x.||"},
    {"TRAP", "X......x..x.....|........X.......||xxxxxxxxx?x?xxxx||||"},
    {"ROLLING HATS", "X...X...X...X...||....x.......x...|xXxxxXxxxXxxxXxx||||"},
    {"FOUR FLOOR", "X...X...X...X...|||||||"},
    {"DUB TECHNO", "X...X...X...X...|||x.?.x.?.x.?.x.?.|..x...x...x...x.||...X......x...x.|"},   /* (made for the DUB TECHNO kit: PERC its chord) */
    {"ROCK", "X.......X.X.....|....X.......X...||x.x.x.x.x.x.x.x.||||"},                    /* (and these for ACOUSTIC) */
    {"FUNK", "X..x..x...X..x..|....X..?.?..X..?||xxxxxxxxxxxxxx.x|..............x.|||"},
    {"JAZZ RIDE", "x...x...x...x...|......?.......?.||....x.......x...||||x...x..xx...x..x"}};
#define NDKP (sizeof DKP / sizeof DKP[0])
static uint8_t perf_sum(void)                   /* (up to the macros: they check themselves, mc_magic) */
{
    const uint8_t *b = (const uint8_t *)&perf;
    uint32_t i, x = 0x3C, n = (uint32_t)((const uint8_t *)&perf.mc_magic - b);
    for (i = 2; i < n; i++) x = (x * 5u + b[i]) & 0xFFu;
    return (uint8_t)x;
}
/* the first MACROS: FILTER (cutoff, resonance), WOBBLE (depth, rate), GRIT (filter drive, noise, saturation), TONE
 * (both oscillators' first control, X-MOD) */
static void mc_defaults(void)
{
    static const uint8_t D[16] = {D_CUT, D_RES, 0, 0, D_WOB, D_L1RATE, 0, 0, D_DRV, D_NOISE, D_DRAMT, 0, D_AP1, D_BP1, D_XMOD, 0};
    static const int8_t A[16] = {44, 18, 0, 0, 48, 16, 0, 0, 60, 20, 40, 0, 40, 40, 30, 0};
    memcpy(perf.mc_dst, D, 16);
    memcpy(perf.mc_amt, A, 16);
    perf.mc_magic = MC_MAGIC;
}
/* ---- the SEL windows: 1 FX LIVE, 2 MACROS (SEL again: the next, then back to the page) */

/* ---- the EQ (the FX page's EQ view): four bands, one set at a time: SELECT the band, K1 FREQ, K2 GAIN, K3 Q, K4 SHAPE,
 * ALGO the frequency too; on every sound, kept with SEQ BASS */
static const char *const L_EQT[NEQT] = {"OFF", "LOW CUT", "LOW SHELF", "BELL", "NOTCH", "HIGH SHELF", "HIGH CUT"};
static const char *const L_EQS[NEQT] = {"OFF", "LCUT", "LSHF", "BELL", "NTCH", "HSHF", "HCUT"};
static uint8_t eq_cur, eq_dirty = 1;
static int16_t eq_curve[112];                   /* the response, dB x 16, 20 Hz .. 18 kHz (the EQ window) */
static void eq_defaults(void)
{
    static const uint8_t T[4] = {EQ_LOSH, EQ_BELL, EQ_BELL, EQ_HISH}, F[4] = {10, 19, 32, 41};
    uint32_t k;
    for (k = 0; k < 4u; k++) { perf.eq_t[k] = T[k]; perf.eq_f[k] = F[k]; perf.eq_q[k] = 3; perf.eq_g[k] = 0; }
    perf.eq_magic = 0x45u;
}
static int eq_live(uint32_t k)                  /* a band that does something */
{
    uint32_t t = perf.eq_t[k];
    return t == EQ_LOCUT || t == EQ_HICUT || t == EQ_NOTCH || ((t == EQ_LOSH || t == EQ_BELL || t == EQ_HISH) && perf.eq_g[k]);
}
static int32_t q24_div(int64_t n, int64_t d)    /* n * 2^24 / d, rounded, by hand (the FM-1 has no 64-bit division) */
{
    uint64_t un = (uint64_t)(n < 0 ? -n : n) << 24, ud = (uint64_t)(d < 0 ? -d : d), q = 0, r = 0;
    int i;
    if (!ud) return 0;
    for (i = 63; i >= 0; i--) {
        r = r << 1 | (un >> i & 1u);
        if (r >= ud) { r -= ud; q |= 1ull << i; }
    }
    if (2u * r >= ud) q++;
    return (int32_t)((n < 0) != (d < 0) ? -(int64_t)q : (int64_t)q);
}
static void eq_coef(uint32_t k, int32_t *o, int32_t fq8, int32_t gdb)   /* RBJ's biquads, Q24 (a0 divided out exactly: q24_div)        */
{                                                                        /* fq8: the frequency step, Q8 (between two steps: in between) */
    const int64_t ONE = 1 << 24;
    uint32_t fi, fr, ph, gi = (uint32_t)clampi(gdb + 12, 0, 24);
    fq8 = clampi(fq8, 0, 47 * 256);
    fi = (uint32_t)fq8 >> 8; fr = (uint32_t)fq8 & 255u;
    ph = fi < 47u ? KS_EQ_INC[fi] + (uint32_t)(((uint64_t)(KS_EQ_INC[fi + 1u] - KS_EQ_INC[fi]) * fr) >> 8) : KS_EQ_INC[47];
    int64_t s = (int64_t)sin32(ph) << 9, hs = (int64_t)sin32(ph >> 1) << 9, c = ONE - ((2 * hs * hs) >> 24);   /* (cos as 1 - 2 sin^2(w/2): exact near 0, where a narrow notch needs it) */
    int64_t al = (s * KS_EQ_IQ[perf.eq_q[k] & 7u]) >> 14, A = (int64_t)KS_EQ_A[gi] << 10, IA = (int64_t)KS_EQ_IA[gi] << 10;
    int64_t Ap = A + ONE, Am = A - ONE, sa = ((int64_t)KS_EQ_SQ[gi] << 10) * al >> 23, b0, b1, b2, a0, a1, a2;
    switch (perf.eq_t[k]) {
    case EQ_LOCUT: b0 = (ONE + c) / 2; b1 = -(ONE + c); b2 = b0; a0 = ONE + al; a1 = -2 * c; a2 = ONE - al; break;
    case EQ_HICUT: b0 = (ONE - c) / 2; b1 = ONE - c; b2 = b0; a0 = ONE + al; a1 = -2 * c; a2 = ONE - al; break;
    case EQ_NOTCH: b0 = ONE; b1 = -2 * c; b2 = ONE; a0 = ONE + al; a1 = -2 * c; a2 = ONE - al; break;
    case EQ_LOSH:
        b0 = (A * (Ap - (Am * c >> 24) + sa)) >> 24; b1 = (2 * A * (Am - (Ap * c >> 24))) >> 24; b2 = (A * (Ap - (Am * c >> 24) - sa)) >> 24;
        a0 = Ap + (Am * c >> 24) + sa; a1 = -2 * (Am + (Ap * c >> 24)); a2 = Ap + (Am * c >> 24) - sa; break;
    case EQ_HISH:
        b0 = (A * (Ap + (Am * c >> 24) + sa)) >> 24; b1 = (-2 * A * (Am + (Ap * c >> 24))) >> 24; b2 = (A * (Ap + (Am * c >> 24) - sa)) >> 24;
        a0 = Ap - (Am * c >> 24) + sa; a1 = 2 * (Am - (Ap * c >> 24)); a2 = Ap - (Am * c >> 24) - sa; break;
    default:                                    /* BELL */
        b0 = ONE + (al * A >> 24); b1 = -2 * c; b2 = ONE - (al * A >> 24); a0 = ONE + (al * IA >> 24); a1 = -2 * c; a2 = ONE - (al * IA >> 24); break;
    }
    /* (each one divided by a0 in full: the old 1/a0 in Q16 left the low narrow notches' */
    o[0] = q24_div(b0, a0); o[1] = q24_div(b1, a0); o[2] = q24_div(b2, a0);   /* sums off, a rumble and a hiss) */
    o[3] = q24_div(a1, a0); o[4] = q24_div(a2, a0);
}
static void p_eqhead(char *t)                  /* the band being set, its type and width (the EQ's top line) */
{
    char *p = str_cat(t, "BAND ");
    *p++ = (char)('1' + (eq_cur & 3u)); *p = 0;
    p = str_cat(p, " \xB7 ");
    p = str_cat(p, L_EQT[perf.eq_t[eq_cur & 3u] % NEQT]);
    p = str_cat(p, " \xB7 Q ");
    p = fmt_u(p, KS_EQ_Q10[perf.eq_q[eq_cur & 3u] & 7u] / 10u, 0);
    *p++ = '.'; fmt_u(p, KS_EQ_Q10[perf.eq_q[eq_cur & 3u] & 7u] % 10u, 0);
}
static int eq_coefs(void)                       /* the coefficients to the audio (once it took the last), an LFO's move included */
{
    int32_t q[4][5];
    uint32_t k, mask = 0;
    if (eq_new) return 0;                       /* (the audio has not taken the last ones yet: next time) */
    for (k = 0; k < 4u; k++) {
        int32_t mf = gmod[D_EQ1F + k], mg = gmod[D_EQ1G + k];   /* (the whole depth: +-48 steps, +-12 dB) */
        int32_t gdb = perf.eq_g[k] + ((mg * 3) >> 13);
        uint32_t t = perf.eq_t[k];
        if (!eq_live(k) && !(mg && (t == EQ_LOSH || t == EQ_BELL || t == EQ_HISH))) continue;
        eq_coef(k, q[k], (int32_t)(perf.eq_f[k] % 48u) * 256 + ((mf * 3) >> 3), gdb);
        mask |= 1u << k;
    }
    memcpy(eq_n, q, sizeof q);
    eq_nmask = (uint8_t)mask;
    eq_new = 1;
    return 1;
}
static void eq_update(void)                     /* the coefficients, the curve */
{
    uint32_t k, x, mask = 0;
    if (!eq_coefs()) return;
    eq_dirty = 0;
    for (k = 0; k < 4u; k++) if (eq_live(k)) mask |= 1u << k;
    for (x = 0; x < 112u; x++) {                /* the curve: each band's shape in octaves from its frequency (the sound */
        int32_t fq8 = (int32_t)(x * 47u * 256u / 111u), db = 0;   /* itself is the biquads: this is their picture) */
        for (k = 0; k < 4u; k++) {
            int32_t d = (fq8 - (int32_t)(perf.eq_f[k] % 48u) * 256) * 53 / 256, q2 = (int32_t)KS_EQ_Q10[perf.eq_q[k] & 7u] * 2 / 10 + 1;
            int32_t g = perf.eq_g[k] * 16, u, sm, ad = d < 0 ? -d : d;   /* (d: octaves Q8, 0.209 octave a step) */
            if (!(mask >> k & 1u)) continue;
            u = d * q2;
            sm = clampi(128 - d * 64 / 256, 0, 256);                   /* (a shelf's slope: 0 to 1 over 4 octaves) */
            switch (perf.eq_t[k]) {
            case EQ_BELL: db += g * 256 / (256 + (int32_t)(((int64_t)u * u) >> 8)); break;
            case EQ_NOTCH: db += -24 * 16 * 256 / (256 + (int32_t)(((int64_t)u * u * 16) >> 8)); break;
            case EQ_LOSH: db += g * sm / 256; break;
            case EQ_HISH: db += g * (256 - sm) / 256; break;
            case EQ_LOCUT: db += d < 0 ? -48 - 12 * 16 * ad / 256 : -48 * 256 / (256 + ((int32_t)(((int64_t)d * d) >> 8)) * 8); break;
            default: db += d > 0 ? -48 - 12 * 16 * ad / 256 : -48 * 256 / (256 + ((int32_t)(((int64_t)d * d) >> 8)) * 8); break;   /* HIGH CUT */
            }
        }
        eq_curve[x] = (int16_t)clampi(db, -24 * 16, 24 * 16);
    }
}
/* BRIGHTNESS: the backlight is on, full (PA2 is a plain switch: no steps there); the lower steps dim the colours (theme_set), MAX leaves them as
 * drawn. Nothing is sent to the panel: WRCTRLD / WRDISBV (0x53, 0x51) broke the FM-1's screen (letters eaten, grey cards gone, until a restart) */
/* CONTRAST on the panel: MAX picks the ST7789's darker gamma curve (GAMSET 0x26: 1 = 2.2, the default; 4 = 2.5) */
static void lcd_contrast(uint32_t c)
{
#if !defined(KAWA_WASM) && !defined(DINGHY_HOST)
    static uint8_t gm;
    lcd_sync();
    gm = c >= 2u ? 0x04u : 0x01u;
    lcd_cmd(0x26);
    lcd_data(&gm, 1);
#else
    (void)c;
#endif
}
/* ---- why it stopped last time (the reset's source, a crash's mark): on the start screens and in SYSTEM */
static char rst_why[28];
static uint8_t rst_bad;                         /* a stop that was not asked for */
static void rst_read(void)
{
#if defined(KAWA_WASM)
    str_cat(rst_why, "-");
#else
    uint8_t r = fm1_boot.p3_rst;
    const char *t = r & 4u ? "WATCHDOG (TOO BUSY)" : r & 2u ? "LOW VOLTAGE (POWER)" : r & 0x20u ? "1.2 V LOW (POWER)" : r & 0x10u ? "LONG PRESS" : 0;
    if (fm1_crash_mark[0] == 0xC4A54ED0u) { t = "CRASH"; fm1_crash_mark[0] = 0; }
    rst_bad = t && !(r & 0x10u);
    str_cat(rst_why, t ? t : r & 1u ? "POWER OFF" : "RESTART");
    if (t && t[0] == 'C') {                     /* CRASH: the address, to look up in the map */
        char *p = rst_why + 5; uint32_t pc = fm1_crash_mark[1]; int i;
        *p++ = ' ';
        for (i = 28; i >= 0; i -= 4) *p++ = "0123456789ABCDEF"[(pc >> i) & 15u];
        *p = 0;
    }
#endif
}
static uint8_t mm_st, mm_d1, mm_d2, mm_n, mm_rt, mm_rn;   /* the last channel message in, and the last system one but the clock (the MIDI page shows them) */
static uint8_t ml_state, ml_id;                 /* MIDI LEARN: 1 waits for a setting, 2 for a CC; the setting */
static uint32_t cc_count(void) { uint32_t k, n = 0; if (perf.cc_magic == CC_MAGIC) for (k = 0; k < 16u; k++) n += perf.cc_num[k] != 0xFFu; return n; }
static uint8_t first_run;                       /* no settings yet: the very first start (it asks CLASSIC or SHIFT, once) */
static uint8_t rs_secs = 1, rs_rec_on;          /* RESAMPLE: its length - 2 (s); a recording was started (its end selects it) */
static uint8_t win, win_cur, mc_pos;            /* the window, its row (FX LIVE), the target being edited (MACROS: 0..31) */
static const uint8_t LV_IT[6] = {G_LV_FLT, G_LV_PUMP, G_LV_STUT, G_LV_TAPE, G_LV_PRATE, G_LV_FRES};
static void perf_defaults(void)
{
    uint32_t k, i;
    memset(&perf, 0, sizeof perf);
    perf.magic = PF_MAGIC; perf.ar_rate = 3; perf.ar_oct = 0; perf.ar_gate = 5;
    for (k = 0; k < SQ_NPAT; k++) {
        seqpat_t *q = &perf.pat[k];
        uint8_t gl[16];
        q->len = 16; q->rate = 3;
        if (k < 8u) sq_parse(SQ_DEF[k][1], q->note, q->fl, gl);
    }
    (void)i;
    perf.sum = perf_sum();
    mc_defaults();
}
static void perf_touch(void) { perf.sum = perf_sum(); perf_dirty = 1; perf_ms = fm1_ms; }
static void sqx_touch(void) { sqx_mark(); perf_touch(); }
static seqpat_t *sq_pat(void) { return &perf.pat[perf.cur % SQ_NPAT]; }
static void dk_touch(void) { dk_mark(); perf_touch(); }
static dkpat_t *dk_pp(void) { return &dk.pat[dk.cur % DK_NPAT]; }
static uint32_t dk_len(void) { uint32_t l = dk_pp()->len; return l >= 1u && l <= DK_MAX ? l : 16u; }
static uint32_t sq_len(void) { uint32_t l = sq_pat()->len; return l >= 1u && l <= SQ_MAX ? l : 16u; }
static const char *dk_tname(uint32_t t)
{
    static const char *const N[NDK] = {"KICK", "SNARE", "CLAP", "CLOSED HAT", "OPEN HAT", "TOM", "PERC", "CYMBAL"};
    static const char *const PERC[DK_NKIT] = {"COWBELL", "RIMSHOT", "CHORD", "CONGA"};
    return t % NDK == DK_PERC ? PERC[dk.kit % DK_NKIT] : N[t % NDK];
}
static const char *dk_kname(uint32_t k)
{
    static const char *const K[DK_NKIT] = {"808", "909", "DUB TECHNO", "ACOUSTIC"};
    return K[k % DK_NKIT];
}
static const char *const SC_NAME[NSC] = {"ALWAYS", "1/2", "2/2", "1/4", "2/4", "3/4", "4/4", "START", "NOT START", "IF BEFORE", "IF AFTER"};
/* the libraries: PRESET 0 is the pattern as it was before turning (MY PATTERN), kept until something is changed by hand */
static uint8_t sq_psel, dk_psel, dk_trk, dk_cur;
static struct { uint8_t note[SQ_MAX], fl[SQ_MAX], cond[SQ_MAX], prob[SQ_MAX], len, rate, swing, k, ok; } sq_bk;
static struct { dkpat_t p; uint8_t k, ok; } dk_bk;
static void sq_edited(void) { sq_psel = 0; sq_bk.ok = 0; }
static void dk_edited(void) { dk_psel = 0; dk_bk.ok = 0; }
static void sq_backup(int restore)
{
    uint32_t k = perf.cur % SQ_NPAT, i;
    seqpat_t *q = &perf.pat[k];
    if (restore && (!sq_bk.ok || sq_bk.k != k)) return;
    for (i = 0; i < SQ_MAX; i++) {
        if (restore) { *sq_np(k, i) = sq_bk.note[i]; *sq_fp(k, i) = sq_bk.fl[i]; sqx.p[k].cond[i] = sq_bk.cond[i]; sqx.p[k].prob[i] = sq_bk.prob[i]; }
        else { sq_bk.note[i] = *sq_np(k, i); sq_bk.fl[i] = *sq_fp(k, i); sq_bk.cond[i] = sqx.p[k].cond[i]; sq_bk.prob[i] = sqx.p[k].prob[i]; }
    }
    if (restore) { q->len = sq_bk.len; q->rate = sq_bk.rate; q->swing = sq_bk.swing; }
    else { sq_bk.len = q->len; sq_bk.rate = q->rate; sq_bk.swing = q->swing; sq_bk.k = (uint8_t)k; sq_bk.ok = 1; }
}
static uint32_t sq_gl_near(uint32_t g, uint32_t m)   /* a note length index, m times as long */
{
    uint32_t i, best = 0;
    uint64_t want = (uint64_t)SQ_GL[g] * m, bd = ~0ull;
    if (g == 15u) return 15u;
    for (i = 0; i < 15u; i++) { uint64_t d = SQ_GL[i] > want ? SQ_GL[i] - want : want - SQ_GL[i]; if (d < bd) { bd = d; best = i; } }
    return best;
}
static uint32_t pre_size(uint32_t l) { return l <= 16u ? 16u : l <= 32u ? 32u : 64u; }   /* (a library pattern: as long as the pattern it goes in) */
static void sq_load_preset(uint32_t sel)        /* 1.. : SQP[sel - 1], at the pattern's length: 32 the riff and its fill, 64 twice that, 16 the riff alone */
{
    uint32_t pi = (sel - 1u) % NSQP, size = pre_size(sq_len()), m = size / 16u, k = perf.cur % SQ_NPAT, i, seg, root = 12;
    uint8_t n[16], f[16], g[16];
    sq_parse(SQP[pi].t, n, f, g);
    for (i = 0; i < 16u; i++) if (f[i] & SF_ON) { root = n[i]; break; }
    for (i = 0; i < SQ_MAX; i++) { *sq_np(k, i) = (uint8_t)root; *sq_fp(k, i) = 0; sqx.p[k].cond[i] = 0; sqx.p[k].prob[i] = 100; }
    if (SQP[pi].stretch) {
        for (i = 0; i < 16u; i++)
            if (f[i] & SF_ON) { *sq_np(k, i * m) = n[i]; *sq_fp(k, i * m) = f[i]; sqx.p[k].cond[i * m] = (uint8_t)(sq_gl_near(g[i], m) << 4); }
    } else {
        for (seg = 0; seg < m; seg++)
            for (i = 0; i < 16u; i++) {
                uint32_t at = seg * 16u + i;
                *sq_np(k, at) = (uint8_t)clampi(n[i] + (m == 4u && seg == 2u ? 5 : 0), 0, 48);
                *sq_fp(k, at) = f[i];
                sqx.p[k].cond[at] = (uint8_t)(g[i] << 4);
            }
        if (m > 1u) {                           /* the fill: the root, its fifth, the octave, the fifth */
            static const uint8_t UP[4] = {0, 7, 12, 7}, FL[4] = {SF_ON | SF_ACC, SF_ON, SF_ON | SF_SLIDE, SF_ON};
            for (i = 0; i < 4u; i++) { uint32_t at = size - 4u + i; *sq_np(k, at) = (uint8_t)clampi((int32_t)(root + UP[i]), 0, 48); *sq_fp(k, at) = FL[i]; sqx.p[k].cond[at] = 0; }
        }
    }
    perf.pat[k].len = (uint8_t)size;
    perf.pat[k].rate = 3;
    if (sq_cur_step >= size) sq_cur_step = 0;
    sqx_touch();
}
static void dk_load_preset(uint32_t sel)        /* 1.. : DKP[sel - 1], at the pattern's length (as SEQ BASS) */
{
    uint32_t pi = (sel - 1u) % NDKP, size = pre_size(dk_len()), m = size / 16u, t, i, seg;
    dkpat_t *q = &dk.pat[dk.cur % DK_NPAT];
    const char *c = DKP[pi].t;
    memset(q->on, 0, sizeof q->on); memset(q->acc, 0, sizeof q->acc); memset(q->pc, 0, sizeof q->pc);
    for (t = 0; t < NDK; t++) {
        for (i = 0; i < 16u && *c && *c != '|'; i++, c++)
            if (*c != '.')
                for (seg = 0; seg < m; seg++) {
                    uint32_t at = seg * 16u + i;
                    q->on[at] |= (uint8_t)(1u << t);
                    if (*c == 'X') q->acc[at] |= (uint8_t)(1u << t);
                    if (*c == '?') q->pc[t][at] = 9u << 4;   /* (DK_PROB[9]: 50 %) */
                }
        while (*c && *c != '|') c++;
        if (*c == '|') c++;
    }
    if (m > 1u) {                               /* the fill: four snares (ACOUSTIC: three toms), the last one accented; 64: a cymbal to start */
        for (i = size - 4u; i < size; i++) {
            t = dk.kit % DK_NKIT == DK_ACOU && i < size - 1u ? DK_TOM : DK_SNARE;
            q->on[i] |= (uint8_t)(1u << t); q->pc[t][i] = 0;
        }
        q->acc[size - 1u] |= 1u << DK_SNARE;
        if (m == 4u) { q->on[0] |= 1u << DK_CYM; q->acc[0] |= 1u << DK_CYM; }
    }
    q->len = (uint8_t)size;
    if (dk_cur >= size) dk_cur = 0;
    dk_touch();
}
static void seq_play(int on)
{
    ev_push(EV_SEQ, on ? 1u : 0u, set.clock ? 0u : 1u);
    if (!on) sq_rec = 0;
}

static uint8_t up_sum(const upatch_t *u)
{
    uint32_t i, s = 0x5A;
    for (i = 0; i < sizeof u->name; i++) s += (uint8_t)u->name[i];
    for (i = 0; i < NP_STORE; i++) s = (s * 3u + (uint8_t)u->p[i]) & 0xFFu;
    return (uint8_t)s;
}
static int up_valid(uint32_t k) { return k < NUSER && ustore[k].magic == UP_MAGIC && ustore[k].sum == up_sum(&ustore[k]); }
static void user_get(uint32_t k, int8_t *p)     /* a user slot, whole: sector 1, then its rest (or the defaults) */
{
    patch_defaults(p);
    memcpy(p, ustore[k].p, NP_STORE);
    if (uext.magic == UX_MAGIC && uext.sum == ux_sum()) memcpy(p + NP_STORE, uext.p[k], NP_MAX - NP_STORE);
    patch_sanitize(p);
}

/* the engines a patch plays: "SUPER + ANALOG", "FM x FOLD" (ring), "VOCAL < FM" (B cross-modulates A) */
static char *eng_str(char *p, const int8_t *pp)
{
    int a = pp[A_LVL] || pp[X_MODE] == 1, b = pp[B_LVL] || pp[X_MODE];
    if (a) p = str_cat(p, L_ENG[(uint32_t)pp[A_ENG] % NENG]);
    if (a && b) p = str_cat(p, pp[X_MODE] == 1 ? " x " : pp[X_MODE] == 2 ? " < " : " + ");
    if (b) p = str_cat(p, L_ENG[(uint32_t)pp[B_ENG] % NENG]);
    if (!a && !b) p = str_cat(p, "SUB");
    return p;
}
static uint8_t dpop_id;                         /* the destination list on screen (for this parameter) */
static uint32_t dpop_ms;
static uint8_t pdirty;                          /* the sound changed since it was loaded or saved */
static uint8_t pend_on, pend_bank, pend_k;      /* a load waiting for SEL (the sound changed) */
static uint32_t pend_ms;

/* ---- undo: the patch before each change (turns of the same knob within 1.5 s are one change), 16 deep */
#define NUNDO 16u
static int8_t undo_p[NUNDO][NP_MAX];
static char undo_nm[NUNDO][14];
static uint32_t undo_w, undo_n, undo_ms, undo_id = 0xFFFFu;
static uint8_t booted;
static int8_t redo_p[NUNDO][NP_MAX];             /* what UNDO took back, for REDO (a new change clears it) */
static char redo_nm[NUNDO][14];
static uint32_t redo_n;
static uint8_t ur_on;                           /* SAVE held: OCT- UNDO, OCT+ REDO */
static void undo_keep(void)                     /* the sound now onto the undo ring */
{
    memcpy(undo_p[undo_w], patch, NP_MAX);
    memcpy(undo_nm[undo_w], pname, 14);
    undo_w = (undo_w + 1u) % NUNDO;
    if (undo_n < NUNDO) undo_n++;
}
static void undo_push(void)
{
    if (!booted) return;
    undo_keep();
    redo_n = 0;
}
static void undo_mark(uint32_t id)              /* a parameter is about to change */
{
    pdirty = 1;
    if (id != undo_id || fm1_ms - undo_ms > 1500u) undo_push();
    undo_id = id;
    undo_ms = fm1_ms;
}
static void undo(void)
{
    char t[16], *p;
    if (!undo_n) { toast("NOTHING TO UNDO"); return; }
    if (redo_n == NUNDO) {                      /* (full: the oldest goes) */
        uint32_t k;
        for (k = 1; k < NUNDO; k++) { memcpy(redo_p[k - 1u], redo_p[k], NP_MAX); memcpy(redo_nm[k - 1u], redo_nm[k], 14); }
        redo_n--;
    }
    memcpy(redo_p[redo_n], patch, NP_MAX);
    memcpy(redo_nm[redo_n], pname, 14);
    redo_n++;
    undo_w = (undo_w + NUNDO - 1u) % NUNDO;
    undo_n--;
    memcpy(patch, undo_p[undo_w], NP_MAX);
    memcpy(pname, undo_nm[undo_w], 14);
    undo_id = 0xFFFFu;
    prev_dirty = 1;
    p = str_cat(t, "UNDO \xB7 ");
    p = fmt_u(p, undo_n, 0);
    str_cat(p, " LEFT");
    toast(t);
}
static void redo(void)
{
    char t[16], *p;
    if (!redo_n) { toast("NOTHING TO REDO"); return; }
    undo_keep();
    redo_n--;
    memcpy(patch, redo_p[redo_n], NP_MAX);
    memcpy(pname, redo_nm[redo_n], 14);
    undo_id = 0xFFFFu;
    prev_dirty = 1;
    pdirty = 1;
    p = str_cat(t, "REDO \xB7 ");
    p = fmt_u(p, redo_n, 0);
    str_cat(p, " LEFT");
    toast(t);
}

/* ---- favorites: a bit for each preset (A, then the user's B) */
static uint32_t fav_i(uint32_t bank, uint32_t k) { return bank ? NFACTORY + k : k; }
static uint8_t *fav_b(uint32_t i) { return i < 96u ? &set.fav[i >> 3] : i < 128u ? &set.fav2[(i - 96u) >> 3] : &set.fav3[(i - 128u) >> 3 & 3u]; }   /* (the byte holding favourite i, 0 .. 159) */
static int fav_is(uint32_t bank, uint32_t k) { uint32_t i = fav_i(bank, k); return i < 160u && (*fav_b(i) >> (i & 7u) & 1u); }
static int fav_any(void) { uint32_t i; for (i = 0; i < 160u; i += 8u) if (*fav_b(i)) return 1; return 0; }

/* ---- compare: SEL held plays the saved sound, released the edited one comes back */
static int8_t cmp_buf[NP_MAX];
static uint8_t cmp_on;

/* ---- the value being turned, big, for a second (before -> after) */
static uint8_t vpop_id;
static uint32_t vpop_ms;
static char vpop_before[24], vpop_pre[12];
static uint32_t splash_until;
static struct { uint8_t b, used, fired; uint32_t at, next; } hb = {0xFF, 0, 0, 0, 0};   /* the button held */
/* SHIFT mode (SYSTEM > UI MODE, asked at each start): a page's first screen is 12 settings in three columns of four,
 * one row a knob: the knobs alone move the left column (the base), OCT- held the middle one, OCT+ held the right one
 * (OCT tapped: the octave). The page's button (or SEL) then goes through its other views, and back. CLASSIC: the views
 * only, OCT is the octave. ini: the cell's initials (0: the engine's) */
typedef struct { const char *title, *head[3]; uint8_t id[3][4]; const char *ini[3][4]; } col3_t;
static const col3_t COL3[NPAGE] = {
    [PG_PLAY] = {"PLAY \xB7 LIVE", {"LIVE", "MIX", "TONE"},
                 {{F_CUT, I_L1RATE, L1_DEP, F_RES}, {S_LVL, DR_AMT, DL_MIX, MS_VOL}, {A_P1, B_P1, X_AMT, F_ENV}},
                 {{"CUT", "RT", "DP", "RES"}, {"SUB", "SAT", "DLY", "VOL"}, {"A1", "B1", "XM", "ENV"}}},
    [PG_BASS] = {"EDIT \xB7 OSCILLATORS", {"MIX", "OSC A", "OSC B"},
                 {{A_LVL, B_LVL, I_ABMIX, X_AMT}, {A_ENG, A_P1, A_P2, A_P3}, {B_ENG, B_P1, B_P2, B_P3}},
                 {{"A", "B", "A/B", "XM"}, {"ENG", 0, 0, 0}, {"ENG", 0, 0, 0}}},   /* (the engine on top, K1) */
    [PG_ENV] = {"ENVELOPES", {"AMP", "FILTER", "MOD"},
                {{AE_A, AE_D, AE_S, AE_R}, {FE_A, FE_D, FE_S, FE_R}, {ME_A, ME_D, ME_AMT, ME_DST}},
                {{"A", "D", "S", "R"}, {"A", "D", "S", "R"}, {"A", "D", "AMT", "DST"}}},
    [PG_FILT] = {"FILTER", {"FILTER", "KEYS", "SUB"},
                 {{F_CUT, F_RES, F_DRV, F_ENV}, {F_TYPE, F_KEY, F_VEL, S_ROUTE}, {S_WAVE, S_OCT, S_LOCK, N_LVL}},
                 {{"CUT", "RES", "DRV", "ENV"}, {"TYP", "KEY", "VEL", "RTE"}, {"WAV", "OCT", "LCK", "NOI"}}},
    [PG_LFO] = {"LFO \xB7 LIVE", {"WOBBLE", "LFO 1", "LFO 2"},
                {{I_L1RATE, L1_DEP, L1_DST, G_GRID}, {L1_SHP, L1_PH, L1_SMO, L1_TRIG}, {L2_SHP, I_L2RATE, L2_DEP, L2_DST}},
                {{"RT", "DP", "DST", "GRD"}, {"SHP", "PH", "SMO", "TRG"}, {"SHP", "RT", "DP", "DST"}}},
    [PG_FX] = {"FX RACK", {"DRIVE", "DYNAMICS", "SPACE"},   /* (what changes a bass most on the knobs alone) */
               {{DR_AMT, OT_AMT, DR_MIX, DR_TYPE}, {CP_AMT, CP_PUMP, MB_MONO, MS_VOL}, {DL_MIX, DL_TIME, DL_FB, RV_MIX}},
               {{"SAT", "OTT", "MIX", "TYP"}, {"CMP", "PMP", "MNO", "VOL"}, {"DLY", "DT", "FB", "REV"}}},
};
static uint8_t c3_show[NPAGE] = {1, 1, 1, 1, 1, 1, 0, 0}, c3_cur;
static uint8_t ask_on, ask_sel;                 /* the start's question: CLASSIC or SHIFT (ask_on 2: then the look, the very first start only) */
static uint8_t theme_ask;                       /* the very first start (no settings yet): the look is asked too */
static const uint8_t ASK_TH[4] = {7, 1, 2, 6};  /* CLASSIC, STUDIO, PURE, RIBBON CLASSIC */
static uint32_t ask_ms;
#define C3_ON (set.ui && COL3[page].title && c3_show[page])
static uint32_t c3_col(void) { return hb.b == BTN_OCT_DOWN ? 1u : hb.b == BTN_OCT_UP ? 2u : 0u; }
static uint32_t hold_time(uint32_t b);
#define PRE_BROWSING (page == PG_PRE && vcur[PG_PRE] < 2u)   /* (SAVE held there: a favorite) */
static const char *hold_label(uint32_t b);
static uint32_t ui_rng = 0x2545F491u;
static uint32_t ui_rnd(void) { ui_rng ^= ui_rng << 13; ui_rng ^= ui_rng >> 17; ui_rng ^= ui_rng << 5; return ui_rng; }

/* a macro's name: what its first target moves (MACROS sets them) */
static const char *dst_name(uint32_t d);
static const char *mc_name(uint32_t k)
{
    static char t[4][14];
    uint32_t j;
    k &= 3u;
    for (j = 0; j < 4u; j++) {
        uint32_t q = k * 4u + j;
        if (perf.mc_magic == MC_MAGIC && perf.mc_dst[q] && perf.mc_amt[q] && perf.mc_dst[q] < ND) {
            const char *n = dst_name(perf.mc_dst[q]);
            uint32_t i;
            for (i = 0; i < 13u && n[i]; i++) t[k][i] = n[i];
            t[k][i] = 0;
            return t[k];
        }
    }
    t[k][0] = 'M'; t[k][1] = (char)('1' + k); t[k][2] = 0;
    return t[k];
}

/* TAGS (the PRESETS views, K4): what a sound is, read from its settings: SUB REESE GROWL WOBBLE PLUCK 808 DRONE */
enum { TG_SUB, TG_REESE, TG_GROWL, TG_WOBBLE, TG_PLUCK, TG_808, TG_DRONE, NTAG };
static const char *const L_TAG[NTAG + 1] = {"ALL", "SUB", "REESE", "GROWL", "WOBBLE", "PLUCK", "808", "DRONE"};
static uint8_t pre_tag, tag_c[NFACTORY + NUSER], tag_ok;
static uint32_t tag_of(const int8_t *p)
{
    uint32_t t = 0, k;
    int ga = p[A_LVL] > 0, gb = p[B_LVL] > 0;
    for (k = 0; k < 2u; k++) {
        int32_t e = p[k ? B_ENG : A_ENG];
        if (!(k ? gb : ga)) continue;
        if (e == E_CS && p[k ? B_P2 : A_P2] >= 40) t |= 1u << TG_REESE;   /* (CS, two channels well apart: the reese's beating) */
        if (e == E_FM || e == E_GROWL || e == E_PHASE || p[k ? B_P4 : A_P4] >= 40) t |= 1u << TG_GROWL;   /* (FM, PHASE, or folded hard) */
    }
    if (p[F_TYPE] == FT_VOWEL) t |= 1u << TG_GROWL;
    if (p[S_LVL] >= 70 || (p[F_CUT] <= 45 && !ga && !gb)) t |= 1u << TG_SUB;
    if ((p[L1_DEP] >= 16 || p[L1_DEP] <= -16) && p[L1_DST]) t |= 1u << TG_WOBBLE;
    if (p[AE_S] < 40 && p[AE_D] < 75) t |= 1u << TG_PLUCK;
    if ((p[SM_SEL] == 1 && p[SM_LVL]) || (p[S_LVL] >= 60 && p[AE_S] < 40 && p[AE_D] >= 70)) t |= 1u << TG_808;
    if ((p[AE_S] >= 100 && p[AE_R] >= 70) || p[AE_A] >= 40) t |= 1u << TG_DRONE;
    return t;
}
static void tags_make(void)
{
    int8_t pp[NP_MAX];
    char nb[14];
    uint32_t k;
    for (k = 0; k < NFACTORY; k++) { preset_factory(k, pp, nb); tag_c[k] = (uint8_t)tag_of(pp); }
    for (k = 0; k < NUSER; k++) { if (up_valid(k)) { user_get(k, pp); tag_c[NFACTORY + k] = (uint8_t)tag_of(pp); } else tag_c[NFACTORY + k] = 0; }
    tag_ok = 1;
}
static int pre_match(uint32_t bank, uint32_t k)
{
    if (bank && !up_valid(k)) return !pre_tag;
    if (!pre_tag) return 1;
    if (!tag_ok) tags_make();
    return (tag_c[bank ? NFACTORY + k : k] >> (pre_tag - 1u)) & 1u;
}
static uint32_t pre_count(uint32_t bank) { uint32_t k, n = 0, m = bank ? NUSER : NFACTORY; for (k = 0; k < m; k++) n += (uint32_t)pre_match(bank, k); return n; }

/* MORPH's state (the module is after save_user): a load lets go of the morph */
enum { MX_OFF, MX_XY, MX_AB };
static uint8_t mx_ready, mx_mode, ab_run;
static void load_preset(uint32_t bank, uint32_t k)
{
    int8_t p[NP_MAX];
    uint32_t i;
    if (bank && !up_valid(k)) { toast("EMPTY SLOT"); return; }
    undo_push();                                /* (a load can be undone too) */
    undo_id = 0xFFFFu;
    if (bank) {
        user_get(k, p);
        for (i = 0; i < 13u; i++) pname[i] = ustore[k].name[i];
        pname[13] = 0;
    } else
        preset_factory(k, p, pname);
    memcpy(patch, p, NP_MAX);
    if (!fx_flush) fx_flush = 1;                /* (the last sound's echoes and reverb do not play on under this one) */
    set.bank = (uint8_t)bank;
    set.idx = (uint8_t)k;
    set_dirty = 1;
    set_ms = fm1_ms;
    prev_dirty = 1;
    pdirty = 0;
    pend_on = 0;
    mx_ready = 0; mx_mode = MX_OFF; ab_run = 0;
}
static void save_user(uint32_t k)
{
    upatch_t *u = &ustore[k];
    uint32_t i;
    u->magic = UP_MAGIC;
    for (i = 0; i < 14u; i++) u->name[i] = i < 13u ? pname[i] : 0;
    memcpy(u->p, patch, NP_STORE);
    u->sum = up_sum(u);
    if (uext.magic != UX_MAGIC || uext.sum != ux_sum()) {   /* (none yet: every slot's rest at its default) */
        uint32_t jj, ii;
        for (jj = 0; jj < NUSER; jj++) for (ii = 0; ii < NP_MAX - NP_STORE; ii++) uext.p[jj][ii] = NP_STORE + ii < NP ? PD[NP_STORE + ii].def : 0;
        uext.magic = UX_MAGIC;
    }
    memcpy(uext.p[k], patch + NP_STORE, NP_MAX - NP_STORE);
    uext.sum = ux_sum();
    tag_ok = 0;
    if (store_save() || perf_save()) { toast("SAVE FAILED"); return; }
    pdirty = 0;
    set.bank = 1;
    set.idx = (uint8_t)k;
    set_dirty = 1;
    toast("SAVED");
}

/* ---- MORPH (SEL windows 3 and 4): X/Y between four sounds at the corners, A/B between two. The main loop writes
 * the blend into the patch (not the audio): a setting the same in every sound is left alone (an edit stays), a list
 * (an engine, a type, a destination) takes the nearest sound's. A sound: 0 the one playing when MORPH opened, then
 * the factory presets, then the user slots. MORPH X, Y, A/B are destinations too (an LFO walks the sound). */
#define MX_MAGIC 0x5Au                          /* (0x59 also marks the item ids renumbered in 0.10: older CC maps to globals go) */
#define MX_VAR 4u                               /* 0 .. 3: the sound playing, as it is / cutoff 1/4 res 3/4 / wobble full 1/8 / full 1/16 */
#define MX_NCODE (MX_VAR + NFACTORY + NUSER)
typedef char mx_codes_fit[MX_NCODE <= 255u ? 1 : -1];
static int8_t mx_snap[4][NP_MAX], ab_snap[2][NP_MAX], mx_now[NP_MAX];
static char mx_now_nm[14];
static uint8_t mx_x, mx_y = 127, ab_amt, mx_corner, mx_skip[(NP + 7u) / 8u], mx_last[3], mx_ex, mx_ey = 127, mx_ea;
static uint8_t ab_from, ab_to;
static uint32_t ab_t0, ab_dur;
static const char *const L_PARTS[6] = {"ALL", "OSC", "FILTER", "ENV", "MOD", "FX"};
static const char *const L_ABT[7] = {"OFF", "1 BEAT", "1 BAR", "2 BARS", "4 BARS", "8 BARS", "16 BARS"};
static const uint8_t ABT_BEATS[7] = {0, 1, 4, 8, 16, 32, 64};
static void mx_defaults(void)
{
    perf.mx_magic = MX_MAGIC;
    perf.mx_pre[0] = 0;                         /* the same sound at the four corners: as it is, darker and sharper, wobbling 1/8, 1/16 */
    perf.mx_pre[1] = 1;
    perf.mx_pre[2] = 2;
    perf.mx_pre[3] = 3;
    perf.ab_pre = 2;
    perf.ab_parts = 0;
    perf.ab_time = 3;
}
static int mx_code_ok(uint32_t c) { return c < MX_NCODE && (c < MX_VAR + NFACTORY || up_valid(c - MX_VAR - NFACTORY)); }
static uint32_t mx_code_step(uint32_t c, int32_t d)
{
    int32_t s = d > 0 ? 1 : -1, n = d > 0 ? d : -d;
    while (n--) {
        int32_t k = (int32_t)c;
        do k += s; while (k >= 0 && k < (int32_t)MX_NCODE && !mx_code_ok((uint32_t)k));
        if (k < 0 || k >= (int32_t)MX_NCODE) break;
        c = (uint32_t)k;
    }
    return c;
}
static const char *mx_name(uint32_t c)
{
    static const char *const VN[MX_VAR] = {"", "NOW \xB7 CUT 1/4", "NOW \xB7 WOB 1/8", "NOW \xB7 WOB 1/16"};
    if (!c || !mx_code_ok(c)) return mx_now_nm;
    if (c < MX_VAR) return VN[c];
    if (c < MX_VAR + NFACTORY) return FACTORY[c - MX_VAR].name;
    return ustore[c - MX_VAR - NFACTORY].name;
}
static int mx_isdst(int32_t d);
static void mx_sound(uint32_t c, int8_t *p)
{
    char nm[14];
    if (c < MX_VAR || !mx_code_ok(c)) {         /* the sound playing, and its three variations */
        memcpy(p, mx_now, NP_MAX);
        if (c == 1u) { p[F_CUT] = 32; p[F_RES] = 95; }
        else if (c == 2u || c == 3u) {
            p[L1_SYNC] = 1; p[L1_DIV] = c == 2u ? 9 : 11; p[L1_DEP] = 63;
            if (!p[L1_DST] || IS_PITCH(p[L1_DST]) || mx_isdst(p[L1_DST])) p[L1_DST] = D_CUT;
        }
    } else if (c < MX_VAR + NFACTORY) preset_factory(c - MX_VAR, p, nm);
    else user_get(c - MX_VAR - NFACTORY, p);
}
static int mx_isdst(int32_t d) { return d == D_MORPHX || d == D_MORPHY || d == D_MORPH; }
static void mx_skipset(uint32_t a, uint32_t n) { for (; n; n--, a++) mx_skip[a >> 3] |= (uint8_t)(1u << (a & 7u)); }
static void mx_skipfrom(const int8_t *p)        /* what drives the morph is not morphed (its LFO, matrix slot...) */
{
    uint32_t k;
    for (k = 0; k < 2u; k++) if (mx_isdst(p[L1_DST + k * LFO_2])) mx_skipset(L1_SHP + k * LFO_2, LFO_2);
    for (k = 0; k < 4u; k++) if (mx_isdst(p[M1_DST + k * 3u])) mx_skipset(M1_SRC + k * 3u, 3);
    if (mx_isdst(p[ST_DST])) mx_skipset(ST_ON, ST_SMO - ST_ON + 1u);
    if (mx_isdst(p[ME_DST])) mx_skipset(ME_A, 4);
}
static void mx_reskip(void)
{
    uint32_t k;
    memset(mx_skip, 0, sizeof mx_skip);
    mx_skipset(P_STYLE, 1);
    mx_skipfrom(patch);
    for (k = 0; k < 4u; k++) mx_skipfrom(mx_snap[k]);
    mx_skipfrom(ab_snap[1]);
    mx_last[2] = MX_OFF;                        /* (worked out again) */
}
static void mx_load(void)                       /* the sound now becomes "NOW", the others are read */
{
    uint32_t k;
    if (perf.mx_magic != MX_MAGIC) mx_defaults();
    memcpy(mx_now, patch, NP_MAX);
    memcpy(mx_now_nm, pname, 14);
    for (k = 0; k < 4u; k++) mx_sound(perf.mx_pre[k], mx_snap[k]);
    memcpy(ab_snap[0], mx_now, NP_MAX);
    mx_sound(perf.ab_pre, ab_snap[1]);
    mx_ready = 1;
    mx_reskip();
}
static void mx_open(void)                       /* a MORPH window opens: a fresh "NOW" unless a morph is going on */
{
    if (mx_mode == MX_OFF) mx_ready = 0;
    if (!mx_ready) mx_load();
}
static uint32_t mx_part(uint32_t i)             /* A/B's PARTS: 1 OSC, 2 FILTER, 3 ENV, 4 MOD, 5 FX (6: ALL only) */
{
    if (i <= X_RESET || (i >= SM_SEL && i <= SM_SPEED) || (i >= SM_START && i <= SM_DIR)) return 1;
    if (i >= F_TYPE && i <= F_VEL) return 2;
    if (i >= AE_A && i <= ME_DST) return 3;
    if ((i >= L1_SHP && i <= M4_AMT) || (i >= ST_ON && i <= ST_SMO)) return 4;
    if (i >= V_MODE && i <= P_STYLE) return 6;
    return 5;
}
static int mx_moves(uint32_t i, int8_t (*s)[NP_MAX], uint32_t n, uint32_t parts)
{
    uint32_t k;
    if (mx_skip[i >> 3] & (1u << (i & 7u))) return 0;
    if (parts && mx_part(i) != parts) return 0;
    for (k = 1; k < n; k++) if (s[k][i] != s[0][i]) return 1;
    return 0;
}
static void mx_blend(int8_t (*s)[NP_MAX], const int32_t *w, uint32_t n, uint32_t parts)
{
    uint32_t i, k, big = 0, osc = 0;
    int32_t tot = 0;
    for (k = 0; k < n; k++) { tot += w[k]; if (w[k] > w[big]) big = k; }
    if (tot <= 0) return;
    for (i = 0; i < NP; i++) {
        int32_t v, a = 0;
        uint8_t f = PD[i].fmt;
        if (!mx_moves(i, s, n, parts)) continue;
        if (f == FM_LIST || f == FM_DEST || f == FM_DIV) v = s[big][i];
        else {
            for (k = 0; k < n; k++) a += w[k] * s[k][i];
            v = (a + (a < 0 ? -tot / 2 : tot / 2)) / tot;
        }
        if (patch[i] != v) { patch[i] = (int8_t)v; if (i <= N_LVL || i == S_WAVE || i == S_OCT) osc = 1; }
    }
    if (osc) prev_dirty = 1;
}
static void ab_go(void)                         /* PLAY in A/B: across, on the tempo */
{
    uint32_t b = ABT_BEATS[perf.ab_time % 7u];
    if (!b) { toast("K4: SET A TIME"); return; }
    mx_open();
    undo_mark(0xFFF1u);
    ab_from = ab_amt;
    ab_to = ab_amt < 64u ? 127u : 0u;
    ab_dur = b * 600000u / (ks_bpm10 ? ks_bpm10 : 1400u);
    ab_t0 = fm1_ms;
    ab_run = 1;
    mx_mode = MX_AB;
}
static void mx_tick(void)
{
    int32_t gx = gmod[D_MORPHX] >> 8, gy = gmod[D_MORPHY] >> 8, ga = gmod[D_MORPH] >> 8, w[4], x, y, a;
    if (ab_run) {
        uint32_t t = fm1_ms - ab_t0;
        if (t >= ab_dur) { t = ab_dur; ab_run = 0; }
        ab_amt = (uint8_t)((int32_t)ab_from + ((int32_t)ab_to - (int32_t)ab_from) * (int32_t)t / (int32_t)(ab_dur ? ab_dur : 1u));
    }
    if (ga && mx_mode != MX_AB) mx_mode = MX_AB;
    else if ((gx || gy) && mx_mode != MX_XY && !ab_run) mx_mode = MX_XY;
    if (mx_mode == MX_OFF) return;
    if (!mx_ready) mx_load();
    if (mx_mode == MX_XY) {
        x = clampi(mx_x + gx, 0, 127);
        y = clampi(mx_y + gy, 0, 127);
        if (mx_last[2] == MX_XY && x == mx_last[0] && y == mx_last[1]) return;
        w[0] = (127 - x) * y; w[1] = x * y; w[2] = (127 - x) * (127 - y); w[3] = x * (127 - y);
        mx_blend(mx_snap, w, 4, 0);
        mx_ex = (uint8_t)x; mx_ey = (uint8_t)y;
        mx_last[0] = (uint8_t)x; mx_last[1] = (uint8_t)y; mx_last[2] = MX_XY;
    } else {
        a = clampi(ab_amt + ga, 0, 127);
        if (mx_last[2] == MX_AB && a == mx_last[0]) return;
        w[0] = 127 - a; w[1] = a;
        mx_blend(ab_snap, w, 2, perf.ab_parts % 6u);
        mx_ea = (uint8_t)a;
        mx_last[0] = (uint8_t)a; mx_last[2] = MX_AB;
    }
}
static void mx_knob(uint32_t k, int32_t d)      /* K1..K4 (SELECT and ALGO: K3 and K4) in a MORPH window */
{
    if (win == 4u || win == 7u) {
        if (k == 0u || k == 1u) {
            mx_open();
            undo_mark(0xFFF0u);
            if (k == 0u) mx_x = (uint8_t)clampi(mx_x + d, 0, 127);
            else mx_y = (uint8_t)clampi(mx_y + d, 0, 127);
            mx_mode = MX_XY;
        } else if (k == 2u) mx_corner = (uint8_t)clampi(mx_corner + d, 0, 3);
        else {
            uint32_t c = mx_corner & 3u;
            perf.mx_pre[c] = (uint8_t)mx_code_step(perf.mx_pre[c], d);
            perf_touch();
            mx_sound(perf.mx_pre[c], mx_snap[c]);
            mx_reskip();
        }
        return;
    }
    if (k == 0u) {
        mx_open();
        undo_mark(0xFFF1u);
        ab_run = 0;
        ab_amt = (uint8_t)clampi(ab_amt + d, 0, 127);
        mx_mode = MX_AB;
    } else if (k == 1u) {
        perf.ab_pre = (uint8_t)mx_code_step(perf.ab_pre, d);
        perf_touch();
        mx_sound(perf.ab_pre, ab_snap[1]);
        mx_reskip();
    } else if (k == 2u) { perf.ab_parts = (uint8_t)clampi(perf.ab_parts + d, 0, 5); perf_touch(); mx_last[2] = MX_OFF; }
    else { perf.ab_time = (uint8_t)clampi(perf.ab_time + d, 0, 6); perf_touch(); }
}

/* ---- FX PADS (FX held, window 6): the 27 keys play effects, not notes. WHITE keys: the wobble (LFO 1) at once: its rate
 * 1 bar, 1/2, then 4 8 16 24 32 48 a bar; its depth OFF LOW MID FULL; its shape SINE TRI SQR S&H. BLACK keys: effects
 * for as long as the key is down: four stutters, tape stop, DJ filter low / high, pump, saturation, phaser, crush. */
enum { PB_ST1, PB_ST2, PB_ST3, PB_ST4, PB_TAPE, PB_DJLP, PB_DJHP, PB_PUMP, PB_SAT, PB_FRZ, PB_CRUSH, NPB };   /* (1.0.48: FREEZE where PHASER was) */
static const char *const PB_NAME[NPB] = {"STUTTER 1/4", "STUTTER 1/8", "STUTTER 1/16", "STUTTER 1/32", "TAPE STOP", "DJ FILTER LOW",
                                         "DJ FILTER HIGH", "PUMP", "SATURATE", "FREEZE", "BIT CRUSH"};
static const char *const PB_SHORT[NPB] = {"ST 1/4", "ST 1/8", "ST 1/16", "ST 1/32", "TAPE", "DJ LOW", "DJ HIGH", "PUMP", "SAT", "FREEZE", "CRUSH"};
static const char *const PW_NAME[16] = {"1 BAR", "1/2", "4", "8", "16", "24", "32", "48", "OFF", "LOW", "MID", "FULL", "SINE", "TRI", "SQR", "S&H"};
static const uint8_t PW_DIV[8] = {2, 3, 6, 9, 11, 12, 13, 14};      /* DIV: 1 bar, 1/2, 1/4, 1/8, 1/16, 1/16T, 1/32, 1/32T */
static const uint8_t PW_DEP[4] = {0, 16, 40, 63}, PW_SHP[4] = {0, 1, 4, 5};
static const uint8_t PK_WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
static const uint8_t PK_BLACK[NPB] = {1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25};
static uint16_t pad_mask, pad_ov;               /* the black pads down; those that changed the patch (to put back) */
static int8_t pad_sv[2][3];
static uint32_t pad_hot;                        /* the keys down now (the screen) */
static void pad_apply(void)
{
    static const uint8_t F[2][3] = {{DR_ON, DR_AMT, DR_MIX}, {CR_ON, CR_BITS, CR_MIX}}, EP[2] = {PB_SAT, PB_CRUSH};
    static const int8_t V[2][3] = {{1, 110, 127}, {1, 24, 127}};
    uint32_t m = pad_mask, k, j;
    int32_t st = 0;
    for (k = 0; k < 4u; k++) if (m >> (PB_ST1 + k) & 1u) st = (int32_t)k + 1;
    lfx_stut = (uint8_t)st;
    lfx_tape = m >> PB_TAPE & 1u ? 100u : 0u;
    lfx_flt = m >> PB_DJLP & 1u ? -55 : m >> PB_DJHP & 1u ? 55 : 0;
    lfx_pump = m >> PB_PUMP & 1u ? 100u : 0u;
    lfx_frz = (uint8_t)(m >> PB_FRZ & 1u);      /* FREEZE: the delay's last cycle held (it needs the delay on) */
    for (k = 0; k < 2u; k++) {                  /* the effects with a patch of their own: set while down, put back after */
        uint32_t on = m >> EP[k] & 1u, was = pad_ov >> k & 1u;
        if (on && !was) { for (j = 0; j < 3u; j++) { pad_sv[k][j] = patch[F[k][j]]; patch[F[k][j]] = V[k][j]; } pad_ov |= (uint16_t)(1u << k); }
        else if (!on && was) { for (j = 0; j < 3u; j++) patch[F[k][j]] = pad_sv[k][j]; pad_ov &= (uint16_t)~(1u << k); }
    }
}
static void pad_key(uint32_t k, int down)
{
    uint32_t i;
    if (down) pad_hot |= 1u << k; else pad_hot &= ~(1u << k);
    for (i = 0; i < NPB; i++)
        if (PK_BLACK[i] == k) {
            if (down && i == PB_FRZ && !patch[DL_ON]) toast("FREEZE: TURN THE DELAY ON");
            if (down) pad_mask |= (uint16_t)(1u << i); else pad_mask &= (uint16_t)~(1u << i);
            pad_apply();
            return;
        }
    if (!down) return;
    for (i = 0; i < 16u; i++)
        if (PK_WHITE[i] == k) {
            undo_mark(0xFFF2u);
            if (i < 8u) { patch[L1_SYNC] = 1; patch[L1_DIV] = (int8_t)PW_DIV[i]; }
            else if (i < 12u) patch[L1_DEP] = (int8_t)PW_DEP[i - 8u];
            else patch[L1_SHP] = (int8_t)PW_SHP[i - 12u];
            return;
        }
}
/* FX PADS QUANTIZE (ALGO in the pads): a key waits for the next division of the beat (the LFOs' clock, ks_beats) */
static const char *const L_PQ[9] = {"OFF", "1 BAR", "1/2", "1/4", "1/8", "1/16", "1/24", "1/32", "1/64"};
static const uint32_t PQ_DIV[9] = {0, 4u << 24, 2u << 24, 1u << 24, 1u << 23, 1u << 22, (1u << 24) / 6u, 1u << 21, 1u << 20};
#define NPQ 8u
static struct { uint8_t k, down; uint32_t at, wait; } pq[NPQ];   /* the keys waiting: their beat when pressed, how long to wait */
static uint32_t pq_n;
static uint32_t pq_since(uint32_t at) { return (ks_beats + BEATS_WRAP - at) % BEATS_WRAP; }
static void pad_quant(uint32_t k, int down)
{
    uint32_t div = PQ_DIV[set.pquant % 9u], b = ks_beats, wait, i;
    if (!div || pq_n >= NPQ) { pad_key(k, down); return; }
    wait = div - b % div;
    for (i = 0; i < pq_n; i++)                  /* (a key let go before its press played: it lasts one division at least) */
        if (pq[i].k == k && pq[i].down) { uint32_t left = pq[i].wait - pq_since(pq[i].at); if (wait < left + div) wait = left + div; }
    pq[pq_n].k = (uint8_t)k; pq[pq_n].down = (uint8_t)down; pq[pq_n].at = b; pq[pq_n].wait = wait;
    pq_n++;
    if (down) pad_hot |= 1u << k;               /* (lit at once: it is coming) */
}
/* WOBBLE TAP (1.0.48): LFO held, a white key: LFO 1 on that division, at the next division of the FX PADS' QUANTIZE (1 BAR,
 * 1/4: the next bar, the next beat; OFF: at once) */
static const uint8_t WT_DIV[8] = {3, 6, 9, 11, 13, 8, 10, 12};   /* 1/2 1/4 1/8 1/16 1/32 1/4T 1/8T 1/16T (L_DIV) */
static struct { uint8_t on, div; uint32_t at, wait; } wtap;
static void wob_tap(uint32_t i)
{
    uint32_t q = PQ_DIV[set.pquant % 9u];
    char t[32], *p;
    wtap.on = 1; wtap.div = WT_DIV[i & 7u]; wtap.at = ks_beats; wtap.wait = q ? q - ks_beats % q : 0u;
    p = str_cat(str_cat(t, "WOB "), L_DIV[wtap.div]);
    if (q) str_cat(str_cat(p, " \xB7 NEXT "), L_PQ[set.pquant % 9u]);
    toast(t);
}
static void pad_quant_tick(void)
{
    uint32_t i = 0, j;
    if (wtap.on && pq_since(wtap.at) >= wtap.wait) { wtap.on = 0; undo_mark(0xFFF2u); patch[L1_SYNC] = 1; patch[L1_DIV] = (int8_t)wtap.div; }
    while (i < pq_n) {
        if (pq_since(pq[i].at) >= pq[i].wait) {
            pad_key(pq[i].k, pq[i].down);
            for (j = i + 1u; j < pq_n; j++) pq[j - 1u] = pq[j];
            pq_n--;
        } else i++;
    }
}
static void pad_release_all(void) { pq_n = 0; pad_mask = 0; pad_hot = 0; pad_apply(); }
/* REC LIVE (PLAY on): the nearest step to now (the one playing, or the next past its half); the time held sets the length */
static struct { uint8_t on, n, step; uint32_t at; } rec_hold;
static uint32_t sq_now_step(void)
{
    uint32_t len = sq_len(), s = sq_show == 0xFF ? 0u : sq_show, fr = (uint32_t)(((uint64_t)ks_beats * STEP_PB[sq_pat()->rate % 6u]) & 0xFFFFFFu);
    return (fr >= 0x800000u ? s + 1u : s) % len;
}
static uint32_t dk_now_step(void)
{
    uint32_t len = dk_len(), s = dk_show == 0xFF ? 0u : dk_show, fr = (uint32_t)(((uint64_t)ks_beats * 4u) & 0xFFFFFFu);
    return (fr >= 0x800000u ? s + 1u : s) % len;
}
/* ---- PARAMETER LOCKS: the list (pl); a step held on DRUMS, the LOCK rows of SEQ BASS, or REC while it plays */
static const char *const PLD_NAME[NPKD] = {"LEVEL", "TUNE", "DECAY", "TONE", "RATCHET", "ECHO"};
static const char *const PLB_NAME[NPKB] = {"CUTOFF", "RESO", "FILT ENV", "DRIVE", "WOB RATE", "A SHAPE", "SMP START"};
static uint8_t sq_lkp;                          /* SEQ BASS: the LOCK row's setting */
static uint8_t dk_hold, dk_hold_used, dk_hold_was;   /* DRUMS: the step held (+1; 0 none), a lock set while held, it was on */
static int32_t pl_find(uint32_t pat, uint32_t step, uint32_t tp)
{
    uint32_t i, n = pl.n <= PL_MAX ? pl.n : 0u;
    for (i = 0; i < n; i++) if (pl.l[i].pat == pat && pl.l[i].step == step && pl.l[i].tp == tp) return (int32_t)i;
    return -1;
}
static int pl_set(uint32_t pat, uint32_t step, uint32_t tp, int32_t v)
{
    int32_t i = pl_find(pat, step, tp);
    if (i < 0) {
        if (pl.n >= PL_MAX) { toast("LOCKS FULL (512)"); return 0; }
        i = pl.n;
        pl.l[i].pat = (uint8_t)pat; pl.l[i].step = (uint8_t)step; pl.l[i].tp = (uint8_t)tp; pl.l[i].v = (int8_t)v;
        pl.n++;                                 /* (after the entry: the sound reads up to n) */
    } else pl.l[i].v = (int8_t)v;
    pl_mark(); perf_touch();
    return 1;
}
static void pl_drop(uint32_t pat, int32_t step, int32_t trk, int32_t what)   /* (-1: any) */
{
    uint32_t i = 0;
    while (i < pl.n && pl.n <= PL_MAX) {
        const plk_t *e = &pl.l[i];
        if (e->pat == pat && (step < 0 || e->step == (uint32_t)step) && (trk < 0 || (e->tp >> 4) == (uint32_t)trk) && (what < 0 || (e->tp & 15u) == (uint32_t)what)) {
            pl.l[i] = pl.l[pl.n - 1u]; pl.n--;
        } else i++;
    }
    pl_mark(); perf_touch();
}
/* AUTOMATION (0.9.59): REC held while it plays, a knob turned records its setting on SEQ BASS's step now; played back by
 * the main loop at each step (the value held to the next automated step), the sound's own value back at STOP. REC held
 * without a knob: the automation of the steps it passes is erased. In the same list: pat PL_AUTO | the SEQ BASS pattern,
 * tp the setting (a patch parameter) */
#define PL_AUTO 0x40u
static uint8_t aut_saved[(NP + 7u) / 8u], aut_last = 0xFF, aut_erasing, aut_any;
static int8_t aut_base[NP];
static uint32_t aut_erased;
static void aut_hold_base(uint32_t id) { if (!(aut_saved[id >> 3] >> (id & 7u) & 1u)) { aut_saved[id >> 3] |= (uint8_t)(1u << (id & 7u)); aut_base[id] = patch[id]; aut_any = 1; } }
static void aut_restore(void)                   /* STOP: every automated setting back to the sound's own value */
{
    uint32_t id;
    for (id = 0; id < NP; id++) if (aut_saved[id >> 3] >> (id & 7u) & 1u) patch[id] = aut_base[id];
    memset(aut_saved, 0, sizeof aut_saved);
    aut_last = 0xFF; aut_any = 0;
    prev_dirty = 1;
}
static void lk_toast(uint32_t step, const char *what, const char *val)
{
    char t[56], *p = str_cat(t, "STEP ");
    p = fmt_u(p, step + 1u, 0); p = str_cat(p, " \xB7 "); p = str_cat(p, what); p = str_cat(p, " "); str_cat(p, val);
    toast(t);
}
static void dk_lock_at(uint32_t s, uint32_t w, int32_t d)   /* DRUMS: lock w of the track chosen on step s, d further */
{
    static const int8_t LO[NPKD] = {0, -64, 0, 0, 1, 0}, HI[NPKD] = {127, 63, 127, 127, 4, 127};
    uint32_t t = dk_trk % NDK, k = dk.cur % DK_NPAT;
    int32_t i = pl_find(k, s, t << 4 | w), v;
    char b[12];
    v = i >= 0 ? pl.l[i].v : w < 4u ? dk.prm[t][w] : w == PK_RAT ? 1 : DK_SEND[t] / 258;
    v = clampi(v + d, LO[w], HI[w]);
    if (!(dk_pp()->on[s] >> t & 1u)) { dk_pp()->on[s] |= (uint8_t)(1u << t); dk_edited(); dk_touch(); }   /* (a lock makes a hit) */
    if (!pl_set(k, s, t << 4 | w, v)) return;
    dk_hold_used = 1;
    if (w == PK_RAT) { char *p = fmt_u(b, (uint32_t)v, 0); str_cat(p, v > 1 ? " HITS" : " HIT"); }
    else if (w == PK_TUNE) fmt_s(b, v);
    else fmt_u(b, (uint32_t)v, 0);
    lk_toast(s, PLD_NAME[w], b);
}
static void lk_text(uint32_t id, int32_t v, char *b);
static void sq_lock_at(uint32_t s, uint32_t w, int32_t d)   /* SEQ BASS: lock w on step s, d further (REC while it plays) */
{
    uint32_t k = perf.cur % SQ_NPAT, id = PB_ID[w];
    int32_t i = pl_find(PL_BASS | k, s, w), v = i >= 0 ? pl.l[i].v : patch[id];
    char b[24];
    v = clampi(v + d * (PD[id].hi - PD[id].lo > 64 ? 2 : 1), PD[id].lo, PD[id].hi);
    if (!pl_set(PL_BASS | k, s, w, v)) return;
    sq_lkv[w] = (int8_t)v; sq_lkm |= (uint8_t)(1u << w);   /* (heard at once) */
    lk_text(id, v, b);
    lk_toast(s, PLB_NAME[w], b);
}
/* ---- DRUMS (its view, no window open): BLACK keys 1-8 choose the instrument (and play it; REC LIVE: record it now),
 * 9 mutes it, 10 clears its track (OCT- held + a black key 1-8: that one muted / back); WHITE keys 1-16: its steps on
 * the page shown (OCT- / OCT+: the page) */
static int dk_view(void) { return page == PG_FILT && vidx() == V_DRUM && !win && !ask_on && !C3_ON; }   /* (SHIFT: not while its 3 columns show) */
static void dk_key(uint32_t k)
{
    dkpat_t *q = dk_pp();
    uint32_t i, s, len = dk_len(), t = dk_trk % NDK;
    for (i = 0; i < 11u && PK_BLACK[i] != k; i++) {}
    if (i < 11u) {
        if (i < NDK && (fm1_in.buttons >> BTN_OCT_DOWN & 1u)) {   /* OCT- (SHIFT) held: that instrument muted / back */
            char tt[32];
            dk.mute ^= (uint8_t)(1u << i);
            if (hb.b == BTN_OCT_DOWN) hb.used = 1;   /* (no page, no UNDO when OCT- is let go) */
            dk_touch();
            str_cat(str_cat(tt, dk_tname(i)), dk.mute >> i & 1u ? " MUTED" : " ON");
            toast(tt);
            return;
        }
        if (i < NDK) {
            dk_trk = (uint8_t)i;
            ev_push(EV_DRUM, i, 110);
            if (sq_rec && sq_play) { s = dk_now_step(); q->on[s] |= (uint8_t)(1u << i); dk_edited(); dk_touch(); }
            else toast(dk_tname(i));
        } else if (i == 8u) { dk.mute ^= (uint8_t)(1u << t); dk_touch(); toast(dk.mute >> t & 1u ? "TRACK MUTED" : "TRACK ON"); }
        else if (i == 9u && dk_hold) { pl_drop(dk.cur % DK_NPAT, dk_hold - 1, (int32_t)t, -1); dk_hold_used = 1; toast("STEP LOCKS CLEARED"); }
        else if (i == 9u) {
            for (s = 0; s < DK_MAX; s++) { q->on[s] &= (uint8_t)~(1u << t); q->acc[s] &= (uint8_t)~(1u << t); q->pc[t][s] = 0; }
            pl_drop(dk.cur % DK_NPAT, -1, (int32_t)t, -1);
            dk_edited(); dk_touch(); toast("TRACK CLEARED");
        }
        return;
    }
    for (i = 0; i < 16u && PK_WHITE[i] != k; i++) {}
    if (i >= 16u) return;
    s = dk_cur / 16u * 16u + i;
    if (s >= len) { toast("PAST THE PATTERN'S END"); return; }
    dk_cur = (uint8_t)s;                        /* (held: K1-K4, ALGO, SELECT lock it; let go untouched: an on step goes off) */
    dk_hold = (uint8_t)(s + 1u); dk_hold_used = 0; dk_hold_was = (uint8_t)(q->on[s] >> t & 1u);
    if (!dk_hold_was) {
        q->on[s] |= (uint8_t)(1u << t);
        if (!sq_play) ev_push(EV_DRUM, t, 96);
        dk_edited(); dk_touch();
    }
}
static void dk_key_up(uint32_t k)
{
    uint32_t i, s, t = dk_trk % NDK;
    dkpat_t *q = dk_pp();
    for (i = 0; i < 16u && PK_WHITE[i] != k; i++) {}
    if (i >= 16u || !dk_hold) return;
    s = dk_cur / 16u * 16u + i;
    if (s + 1u != dk_hold) return;
    if (dk_hold_was && !dk_hold_used) {
        q->on[s] &= (uint8_t)~(1u << t); q->acc[s] &= (uint8_t)~(1u << t);
        pl_drop(dk.cur % DK_NPAT, (int32_t)s, (int32_t)t, -1);
        dk_edited(); dk_touch();
    }
    dk_hold = 0;
}

/* ---- SAVE (the patch's name): the keys are a letter keyboard, as drawn: black A B C | D E | F G H | I J | K, white L M N O P Q R S
 * T U V W X Y Z and a space; a key writes its letter where the cursor is and moves on (K2 the place, K3 digits and signs) */
static uint32_t kb_down;                        /* the keys down (the SAVE screen lights them) */
static int save_view(void) { return page == PG_PRE && VIEWS[vidx()].lay == LY_SAVE && !win && !ask_on && !C3_ON; }
static void save_key(uint32_t k)
{
    static const char BL[11] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K'}, WH[16] = {'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', ' '};
    uint32_t i;
    char ch = 0;
    for (i = 0; i < 11u; i++) if (PK_BLACK[i] == k) ch = BL[i];
    for (i = 0; i < 16u; i++) if (PK_WHITE[i] == k) ch = WH[i];
    if (!ch) return;
    pname[save_pos % 13u] = ch;
    save_pos = (uint8_t)(save_pos < 12u ? save_pos + 1u : 12u);
    g_force = 1;
}

/* ---- items: a parameter of the patch, or one of the globals */
static int item_patch(uint32_t id) { return id < NP; }
static uint32_t rate_id(uint32_t id) { uint32_t o = id == I_L2RATE ? LFO_2 : 0u; return patch[L1_SYNC + o] ? L1_DIV + o : L1_RATE + o; }
static void item_range(uint32_t id, int32_t *lo, int32_t *hi)
{
    if (id == I_L1RATE || id == I_L2RATE) id = rate_id(id);
    if (item_patch(id)) {
        *lo = PD[id].lo; *hi = PD[id].hi;
        if (id == A_ENG || id == B_ENG) *hi = NENG - 1;   /* (the ones there are: ENG_ORDER, edit_item) */
        return;
    }
    *lo = 0;
    *hi = id == G_MIDICH ? 16 : id == G_BPM ? 2400 : id == G_CLOCK || id == G_QLFO || id == G_ENG8 || id == G_ASAVE || id == G_LANG || id == G_NORM || id == G_RS_GO || id == G_GRID || id == G_BROWSE || id == G_C3COL || id == G_UI || id == G_ASK || id == G_ZOOM || id == G_MLEARN || id == G_MSTART || id == G_MPROG || id == G_MCLK || id == G_SYNC || id == G_NEWSONG || id == G_L3_SYNC ? 1 : id == G_MMAP ? 16 : id == I_ABMIX ? 127 : id == G_BRI ? 3 : id == G_DEEP ? 4 : id == G_SQ_ON ? 2 : id == G_ACCEL || id == G_CON || id == G_RS_LEN ? 2 : id == G_THEME ? 7 : id == G_FONT ? (int32_t)NFONT - 1 : id == G_PAL ? (int32_t)(2u * NPAL_BASE - 1u) : id == G_OCT ? 6 : 0;
    if (id == G_ACCEL) *hi = 3;                 /* (OFF, ON, FAST, ULTRA) */
    if (id == G_LANG) *hi = 2;                  /* (ENGLISH, FRANCAIS, ESPANOL) */
    if (id == G_BPM) *lo = 600;
    if (id == G_L3_RATE) *hi = set.l3[L3_SYNC] ? 15 : 127;
    if (id == G_L3_DEP) { *lo = -64; *hi = 63; }
    if (id == G_L3_SHP) { *lo = PD[L1_SHP].lo; *hi = PD[L1_SHP].hi; }
    if (id == G_L3_DST) *hi = ND - 1;
    if (id >= G_SQ_STEP && id <= G_AR_GATE) {
        static const int8_t LO[12] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1}, HI[12] = {15, 48, 4, 15, 16, 5, 50, 1, 4, 5, 3, 10};
        *lo = LO[id - G_SQ_STEP]; *hi = HI[id - G_SQ_STEP];
        if (id == G_SQ_STEP) *hi = (int32_t)sq_len() - 1;
        if (id == G_SQ_LEN) *hi = SQ_MAX;
    }
    if (id == G_SQ_PROB) *hi = 100;
    if (id == G_SQ_COND) *hi = NSC - 1;
    if (id == G_SQ_LKP) *hi = NPKB - 1;
    if (id == G_SQ_LKV) { *lo = PD[PB_ID[sq_lkp % NPKB]].lo - 1; *hi = PD[PB_ID[sq_lkp % NPKB]].hi; }   /* (under the setting's range: OFF) */
    if (id == G_SQ_GATE) *hi = 15;
    if (id == G_SQ_PRESET) *hi = (int32_t)NSQP;
    if (id == G_DK_KSAT || id == G_DK_SNAP) { *lo = 0; *hi = 7; }
    if (id == G_DK_ECHO) { *lo = 0; *hi = 1; }
    if (id >= G_DK_LVL && id <= G_DK_SWING) {
        static const int16_t LO[16] = {0, -64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0};
        static const int16_t HI[16] = {127, 63, 127, 127, NDK - 1, DK_MAX - 1, 2, 15, NSC - 1, DK_NPAT - 1, NDKP, DK_MAX, DK_NKIT - 1, 127, 2, 50};
        *lo = LO[id - G_DK_LVL]; *hi = HI[id - G_DK_LVL];
        if (id == G_DK_STEP) *hi = (int32_t)dk_len() - 1;
    }
    if (id >= G_EQ_F && id <= G_EQ_Q) {
        static const int8_t LO[4] = {0, -12, 0, 0}, HI[4] = {47, 12, NEQT - 1, 7};
        *lo = LO[id - G_EQ_F]; *hi = HI[id - G_EQ_F];
    }
    if (id >= G_LV_FLT && id <= G_MC_AMT) {
        static const int8_t LO[12] = {-64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -64}, HI[12] = {63, 127, 5, 127, 3, 127, 127, 127, 127, 127, ND - 1, 63};
        *lo = LO[id - G_LV_FLT]; *hi = HI[id - G_LV_FLT];
    }
}
static uint32_t sq_type(uint32_t f) { return !(f & SF_ON) ? 0u : (f & SF_ACC) && (f & SF_SLIDE) ? 4u : (f & SF_SLIDE) ? 3u : (f & SF_ACC) ? 2u : 1u; }
static uint8_t lk_fmt_on, lk_fmt_id;            /* lk_text: item_text of a lock's value, not the patch's */
static int8_t lk_fmt_v;
static int32_t item_get(uint32_t id)
{
    if (id == I_L1RATE || id == I_L2RATE) id = rate_id(id);
    if (item_patch(id)) return lk_fmt_on && id == lk_fmt_id ? lk_fmt_v : patch[id];
    switch (id) {
    case G_MIDICH: return set.midich;
    case G_BPM: return set.bpm10;
    case G_CLOCK: return set.clock;
    case G_SYNC: return set.sync;
    case G_L3_RATE: return set.l3[set.l3[L3_SYNC] ? L3_DIV : L3_RATE];
    case G_L3_DEP: return set.l3[L3_DEP];
    case G_L3_SHP: return set.l3[L3_SHP];
    case G_L3_DST: return set.l3[L3_DST];
    case G_L3_SYNC: return set.l3[L3_SYNC];
    case G_OCT: return OCT + 3;
    case G_THEME: return set.theme;
    case G_PAL: return set.pal;
    case G_FONT: return set.font;
    case G_GRID: return set.grid;
    case G_BROWSE: return set.browse;
    case G_ACCEL: return set.accel;
    case G_C3COL: return set.c3col;
    case G_UI: return set.ui;
    case G_ASK: return set.ask;
    case G_ZOOM: return set.zoom;
    case G_CON: return set.contrast;
    case G_NORM: return set.norm;
    case G_BRI: return set.bri;
    case G_QLFO: return set.qlfo;
    case G_ENG8: return set.eng8;
    case G_ASAVE: return set.asave;
    case G_LANG: return set.lang;
    case G_RS_GO: return rs_want ? 1 : 0;
    case G_RS_LEN: return rs_secs;
    case G_DEEP: return set.deep;
    case G_MLEARN: return ml_state != 0;
    case G_MMAP: return (int32_t)cc_count();
    case G_MSTART: return set.mstart;
    case G_MPROG: return set.mprog;
    case G_MCLK: return set.mclk;
    case I_ABMIX: {                             /* A/B MIX: from the two levels (the louder one is the scale) */
        int32_t a = patch[A_LVL], b = patch[B_LVL];
        if (!a && !b) return 64;
        return a >= b ? b * 64 / a : 127 - a * 63 / b;
    }
    case G_SQ_STEP: return sq_cur_step;
    case G_SQ_NOTE: return *sq_np(perf.cur, sq_cur_step);
    case G_SQ_TYPE: return (int32_t)sq_type(*sq_fp(perf.cur, sq_cur_step));
    case G_SQ_PROB: return sqx.p[perf.cur % SQ_NPAT].prob[sq_cur_step % SQ_MAX];
    case G_SQ_COND: return sqx.p[perf.cur % SQ_NPAT].cond[sq_cur_step % SQ_MAX] & 15u;
    case G_SQ_LKP: return sq_lkp % NPKB;
    case G_SQ_LKV: { int32_t i = pl_find(PL_BASS | perf.cur % SQ_NPAT, sq_cur_step % SQ_MAX, sq_lkp % NPKB); return i >= 0 ? pl.l[i].v : PD[PB_ID[sq_lkp % NPKB]].lo - 1; }
    case G_SQ_GATE: return sqx.p[perf.cur % SQ_NPAT].cond[sq_cur_step % SQ_MAX] >> 4;
    case G_SQ_PRESET: return sq_psel;
    case G_DK_KSAT: return dk.pad & 7u;
    case G_DK_SNAP: return dk.pad >> 3 & 7u;
    case G_DK_ECHO: return !(dk.pad & 0x40u);
    case G_DK_LVL: case G_DK_TUNE: case G_DK_DEC: case G_DK_TONE: return dk.prm[dk_trk % NDK][id - G_DK_LVL];
    case G_DK_TRK: return dk_trk;
    case G_DK_STEP: return dk_cur;
    case G_DK_HIT: { const dkpat_t *q = dk_pp(); uint32_t i = dk_cur % DK_MAX, t = dk_trk % NDK; return !(q->on[i] >> t & 1u) ? 0 : q->acc[i] >> t & 1u ? 2 : 1; }
    case G_DK_PROB: return 15 - (dk_pp()->pc[dk_trk % NDK][dk_cur % DK_MAX] >> 4);
    case G_DK_COND: return dk_pp()->pc[dk_trk % NDK][dk_cur % DK_MAX] & 15u;
    case G_DK_PAT: return dk.cur;
    case G_DK_PRE: return dk_psel;
    case G_DK_LEN: return (int32_t)dk_len();
    case G_DK_KIT: return dk.kit;
    case G_DK_VOL: return dk.vol;
    case G_DK_MODE: return dk.mode & 3u;
    case G_SQ_ON: return dk.mode & 4u ? 0 : dk.mode & 8u ? 2 : 1;   /* (1.0.51: 1 ON, the keys play over it; 2 ON, the keys transpose it) */
    case G_NEWSONG: return 0;
    case G_DK_SWING: return dk_pp()->swing;
    case G_SQ_PAT: return perf.cur;
    case G_SQ_LEN: return sq_pat()->len;
    case G_SQ_RATE: return sq_pat()->rate;
    case G_SQ_SWING: return sq_pat()->swing;
    case G_AR_ON: return perf.ar_on;
    case G_AR_MODE: return perf.ar_mode;
    case G_AR_RATE: return perf.ar_rate;
    case G_AR_OCT: return perf.ar_oct;
    case G_AR_GATE: return perf.ar_gate;
    case G_LV_FLT: return lfx_flt;
    case G_LV_PUMP: return lfx_pump;
    case G_LV_STUT: return lfx_stut;
    case G_LV_TAPE: return lfx_tape;
    case G_LV_PRATE: return lfx_prate;
    case G_LV_FRES: return lfx_fres;
    case G_MC1: case G_MC2: case G_MC3: case G_MC4: return mc_val[id - G_MC1];
    case G_MC_DST: return perf.mc_dst[(mc_pos >> 1) & 15u];
    case G_MC_AMT: return perf.mc_amt[(mc_pos >> 1) & 15u];
    case G_EQ_F: return perf.eq_f[eq_cur & 3u];
    case G_EQ_G: return perf.eq_g[eq_cur & 3u];
    case G_EQ_T: return perf.eq_t[eq_cur & 3u];
    case G_EQ_Q: return perf.eq_q[eq_cur & 3u];
    default: return 0;
    }
}
static void new_step(uint32_t which);
static void item_set(uint32_t id, int32_t v)
{
    int32_t lo, hi;
    if (id == G_NEWSONG) { new_step(2u); return; }   /* (a button: turn it twice) */
    if (id == I_L1RATE || id == I_L2RATE) id = rate_id(id);
    item_range(id, &lo, &hi);
    v = clampi(v, lo, hi);
    if (item_patch(id)) {
        if (patch[id] != v) undo_mark(id);
        patch[id] = (int8_t)v;
        if (aut_saved[id >> 3] >> (id & 7u) & 1u) aut_base[id] = (int8_t)v;   /* (an automated setting turned by hand: its own value now) */
        if (PD[id].fmt == FM_DEST && (v == D_DRAMT || v == D_DRMIX) && !patch[DR_ON]) { patch[DR_ON] = 1; toast("SATURATION ON"); }
        if (id < N_LVL || id == S_LVL || id == S_WAVE || id == S_OCT) prev_dirty = 1;
        return;
    }
    if (id >= G_EQ_F && id <= G_EQ_Q) {
        uint32_t k = eq_cur & 3u;
        if (id == G_EQ_F) perf.eq_f[k] = (uint8_t)v;
        else if (id == G_EQ_G) { perf.eq_g[k] = (int8_t)v; if (!perf.eq_t[k]) perf.eq_t[k] = EQ_BELL; }   /* (a gain on an OFF band: a bell) */
        else if (id == G_EQ_T) perf.eq_t[k] = (uint8_t)v;
        else perf.eq_q[k] = (uint8_t)v;
        eq_dirty = 1;
        perf_touch();
        return;
    }
    if (id >= G_LV_FLT && id <= G_MC_AMT) {
        switch (id) {
        case G_LV_FLT: lfx_flt = (int8_t)v; return;
        case G_LV_PUMP: lfx_pump = (uint8_t)v; return;
        case G_LV_STUT: lfx_stut = (uint8_t)v; return;
        case G_LV_TAPE: lfx_tape = (uint8_t)v; return;
        case G_LV_PRATE: lfx_prate = (uint8_t)v; return;
        case G_LV_FRES: lfx_fres = (uint8_t)v; return;
        case G_MC_DST: perf.mc_dst[(mc_pos >> 1) & 15u] = (uint8_t)v; perf_touch(); return;
        case G_MC_AMT: perf.mc_amt[(mc_pos >> 1) & 15u] = (int8_t)v; perf_touch(); return;
        default: mc_val[id - G_MC1] = (uint8_t)v; return;
        }
    }
    if (id == G_SQ_LKP) { sq_lkp = (uint8_t)v; return; }
    if (id == G_SQ_LKV) {                       /* the cursor's step: its lock on, changed, or OFF */
        uint32_t k = PL_BASS | perf.cur % SQ_NPAT, s = sq_cur_step % SQ_MAX, w = sq_lkp % NPKB;
        if (v < PD[PB_ID[w]].lo) { if (pl_find(k, s, w) >= 0) pl_drop(k, (int32_t)s, 0, (int32_t)w); }
        else pl_set(k, s, w, v);
        return;
    }
    if (id == G_SQ_PROB || id == G_SQ_COND || id == G_SQ_GATE) {
        uint32_t k = perf.cur % SQ_NPAT, i = sq_cur_step % SQ_MAX;
        uint8_t *c = &sqx.p[k].cond[i];
        if (id == G_SQ_PROB) sqx.p[k].prob[i] = (uint8_t)v;
        else if (id == G_SQ_COND) *c = (uint8_t)((*c & 0xF0u) | (uint32_t)v);
        else *c = (uint8_t)((*c & 15u) | (uint32_t)v << 4);
        sq_edited();
        sqx_touch();
        return;
    }
    if (id == G_SQ_ON) {                        /* the sequence on / off (the drums go on) */
        dk.mode = (uint8_t)(!v ? dk.mode | 4u : v == 1 ? dk.mode & 3u : (dk.mode & 3u) | 8u);
        dk_touch();
        return;
    }
    if (id == G_DK_KSAT || id == G_DK_SNAP || id == G_DK_ECHO) {   /* (three settings in dk.pad: bits 0-2, 3-5, 6 the echo OFF) */
        uint32_t p = dk.pad;
        p = id == G_DK_KSAT ? (p & ~7u) | (uint32_t)v : id == G_DK_SNAP ? (p & ~0x38u) | (uint32_t)v << 3 : v ? p & ~0x40u : p | 0x40u;
        dk.pad = (uint8_t)p;
        dk_touch();
        return;
    }
    if (id == G_SQ_PRESET) {                    /* the library: at once; back to 0 puts the pattern back as it was */
        if (!sq_psel && v) sq_backup(0);
        if (v) sq_load_preset((uint32_t)v);
        else { sq_backup(1); sqx_touch(); }
        sq_psel = (uint8_t)v;
        return;
    }
    if (id >= G_DK_LVL && id <= G_DK_SWING) {
        dkpat_t *q = dk_pp();
        uint32_t t = dk_trk % NDK, i = dk_cur % DK_MAX;
        switch (id) {
        case G_DK_TRK: dk_trk = (uint8_t)v; return;
        case G_DK_STEP: dk_cur = (uint8_t)v; return;
        case G_DK_HIT:
            q->on[i] = (uint8_t)((q->on[i] & ~(1u << t)) | (v ? 1u << t : 0u));
            q->acc[i] = (uint8_t)((q->acc[i] & ~(1u << t)) | (v == 2 ? 1u << t : 0u));
            dk_edited(); break;
        case G_DK_PROB: q->pc[t][i] = (uint8_t)((q->pc[t][i] & 15u) | (uint32_t)(15 - v) << 4); dk_edited(); break;
        case G_DK_COND: q->pc[t][i] = (uint8_t)((q->pc[t][i] & 0xF0u) | (uint32_t)v); dk_edited(); break;
        case G_DK_PAT: dk.cur = (uint8_t)v; dk_edited(); if (dk_cur >= dk_len()) dk_cur = 0; break;
        case G_DK_PRE:
            if (!dk_psel && v) { dk_bk.p = *q; dk_bk.k = dk.cur; dk_bk.ok = 1; }
            if (v) dk_load_preset((uint32_t)v);
            else if (dk_bk.ok && dk_bk.k == dk.cur) *q = dk_bk.p;
            dk_psel = (uint8_t)v;
            break;
        case G_DK_LEN: q->len = (uint8_t)v; if (dk_cur >= v) dk_cur = (uint8_t)(v - 1); if (dk_psel) dk_load_preset(dk_psel); else dk_edited(); break;   /* (a preset: played again at the new length) */
        case G_DK_KIT: dk.kit = (uint8_t)v; break;
        case G_DK_VOL: dk.vol = (uint8_t)v; break;
        case G_DK_MODE: dk.mode = (uint8_t)((dk.mode & 12u) | (uint32_t)v); break;
        case G_DK_SWING: q->swing = (uint8_t)v; break;
        default: dk.prm[t][id - G_DK_LVL] = (int8_t)v; break;
        }
        dk_touch();
        return;
    }
    if (id >= G_SQ_STEP && id <= G_AR_GATE) {
        seqpat_t *q = sq_pat();
        uint8_t *nf = sq_fp(perf.cur, sq_cur_step);
        switch (id) {
        case G_SQ_STEP: sq_cur_step = (uint8_t)v; return;
        case G_SQ_NOTE: *sq_np(perf.cur, sq_cur_step) = (uint8_t)v; if (!(*nf & SF_ON)) *nf = SF_ON; if (sq_cur_step >= 16u) sqx_mark(); sq_edited(); break;
        case G_SQ_TYPE: *nf = SQ_FL[v]; if (sq_cur_step >= 16u) sqx_mark(); sq_edited(); break;
        case G_SQ_PAT: perf.cur = (uint8_t)v; sq_edited(); if (sq_cur_step >= sq_len()) sq_cur_step = 0; break;
        case G_SQ_LEN: q->len = (uint8_t)v; if (sq_cur_step >= v) sq_cur_step = (uint8_t)(v - 1); if (sq_psel) sq_load_preset(sq_psel); else sq_edited(); break;
        case G_SQ_RATE: q->rate = (uint8_t)v; sq_edited(); break;
        case G_SQ_SWING: q->swing = (uint8_t)v; sq_edited(); break;
        case G_AR_ON: perf.ar_on = (uint8_t)v; break;
        case G_AR_MODE: perf.ar_mode = (uint8_t)v; break;
        case G_AR_RATE: perf.ar_rate = (uint8_t)v; break;
        case G_AR_OCT: perf.ar_oct = (uint8_t)v; break;
        case G_AR_GATE: perf.ar_gate = (uint8_t)v; break;
        }
        perf_touch();
        return;
    }
    switch (id) {
    case G_MIDICH: set.midich = (uint8_t)v; break;
    case G_BPM: set.bpm10 = (uint16_t)v; ks_bpm10 = (uint32_t)v; break;
    case G_CLOCK: set.clock = (uint8_t)v; break;
    case G_SYNC: set.sync = (uint8_t)v; ks_sync = (uint8_t)v; break;
    case G_L3_RATE: set.l3[set.l3[L3_SYNC] ? L3_DIV : L3_RATE] = (int8_t)v; l3_push(); break;
    case G_L3_DEP: set.l3[L3_DEP] = (int8_t)v; l3_push(); break;
    case G_L3_SHP: set.l3[L3_SHP] = (int8_t)v; l3_push(); break;
    case G_L3_DST: set.l3[L3_DST] = (int8_t)v; l3_push(); break;
    case G_L3_SYNC: set.l3[L3_SYNC] = (int8_t)v; l3_push(); break;
    case G_OCT: set.oct = (uint8_t)(int8_t)(v - 3); break;
    case G_THEME: set.theme = (uint8_t)v; theme_set(set.theme, set.pal); g_force = 1; break;
    case G_PAL:
        set.pal = (uint8_t)v;
        theme_set(set.theme, set.pal); font_set(set.font, set.zoom); g_force = 1; break;
    case G_FONT: set.font = (uint8_t)v; font_set(set.font, set.zoom); g_force = 1; break;
    case G_GRID: set.grid = (uint8_t)v; ks_straight = set.grid; break;
    case G_BROWSE: set.browse = (uint8_t)v; break;
    case G_ACCEL: set.accel = (uint8_t)v; break;
    case G_C3COL: set.c3col = (uint8_t)v; break;
    case G_UI: set.ui = (uint8_t)v; c3_show[page] = (uint8_t)v; vcur[page] = 0; break;
    case G_ASK: set.ask = (uint8_t)v; break;
    case G_ZOOM: set.zoom = (uint8_t)v; font_set(set.font, set.zoom); g_force = 1; break;
    case G_NORM: set.norm = (uint8_t)v; ks_norm = (uint8_t)v; break;
    case G_QLFO: set.qlfo = (uint8_t)v; break;
    case G_ENG8: set.eng8 = (uint8_t)v; toast(v ? "ENGINES: ALL 8" : "ENGINES: ESSENTIAL 4"); break;
    case G_ASAVE: set.asave = (uint8_t)v; break;
    case G_LANG: set.lang = (uint8_t)v; g_force = 1; break;
    case G_BRI: set.bri = (uint8_t)v; g_bri = (uint8_t)v; theme_set(set.theme, set.pal); g_force = 1; break;
    case G_RS_LEN: rs_secs = (uint8_t)v; return;
    case G_RS_GO:                               /* RESAMPLE: the output for 2 .. 4 s into OSC C */
        if (v && !rs_want) { rs_desc.len = 0; rs_desc.name = "RESAMPLE"; rs_pos = 0; rs_n = 0; rs_acc = 0; rs_rec_on = 1; rs_want = (2u + rs_secs) * RS_RATE; toast("RECORDING THE OUTPUT"); }
        else if (!v && rs_want) { rs_desc.len = rs_pos; rs_want = 0; }
        return;
    case G_DEEP: set.deep = (uint8_t)v; ks_deep = (uint8_t)v; break;
    case G_CON: set.contrast = (uint8_t)v; g_con = set.contrast; lcd_contrast(v); theme_set(set.theme, set.pal); g_force = 1; break;
    case G_MLEARN: ml_state = (uint8_t)v; toast(v ? "LEARN: TURN A KNOB" : "LEARN OFF"); return;
    case G_MMAP: if (!v) { memset(perf.cc_num, 0xFF, sizeof perf.cc_num); perf_touch(); toast("CC MAP CLEARED"); } return;
    case G_MSTART: set.mstart = (uint8_t)v; break;
    case G_MPROG: set.mprog = (uint8_t)v; break;
    case G_MCLK: set.mclk = (uint8_t)v; break;
    case I_ABMIX: {                             /* the louder of A and B stays, the other follows the balance */
        int32_t a = patch[A_LVL], b = patch[B_LVL], l = a > b ? a : b;
        if (!l) l = 100;
        item_set(A_LVL, v <= 64 ? l : l * (127 - v) / 63);
        item_set(B_LVL, v >= 64 ? l : l * v / 64);
        return;
    }
    default: return;
    }
    set_dirty = 1;
    set_ms = fm1_ms;
}
static int osc_pk(uint32_t id, uint32_t *eng)   /* 1..4: the oscillator's P1..P3, and P4 (FOLD / RING) */
{
    uint32_t b = id >= B_ENG && id <= B_FINE ? OSC_B : 0u, k = id - b;
    if (id == A_P4 || id == B_P4) { *eng = (uint32_t)patch[id == A_P4 ? A_ENG : B_ENG]; return 4; }
    if (id > B_FINE || k < A_P1 || k > A_P3) return 0;
    *eng = (uint32_t)patch[A_ENG + b];
    return (int)(k - A_P1 + 1u);
}
static const char *item_name(uint32_t id)
{
    static char b[20];
    uint32_t e;
    int k = osc_pk(id, &e);
    if (k) { char *p = str_cat(b, id >= B_ENG && id != A_P4 ? "B " : "A "); str_cat(p, k == 4 ? ENG_P4[e % NENG] : ENG_P[e][k - 1]); return b; }
    if (id == I_L1RATE || id == I_L2RATE) return PD[rate_id(id)].name;
    if (item_patch(id)) return PD[id].name;
    switch (id) {
    case G_MIDICH: return "MIDI CHANNEL";
    case G_BPM: return "TEMPO";
    case G_CLOCK: return "CLOCK";
    case G_SYNC: return "SYNC OUT";
    case G_L3_RATE: return set.l3[L3_SYNC] ? "DIVISION" : "RATE";   /* (the view says LFO 3) */
    case G_L3_DEP: return "DEPTH";
    case G_L3_SHP: return "SHAPE";
    case G_L3_DST: return "DEST";
    case G_L3_SYNC: return "SYNC";
    case G_OCT: return "KEYS OCTAVE";
    case G_CPU: return "CPU LOAD";
    case G_THEME: return "THEME";
    case G_PAL: return "PALETTE";
    case G_FONT: return "FONT";
    case G_GRID: return "LFO GRID";
    case G_BROWSE: return "PRESETS";
    case G_ACCEL: return "KNOB ACCEL";
    case G_C3COL: return "SHIFT COLOURS";
    case G_UI: return "UI MODE";
    case G_ASK: return "ASK AT START";
    case G_ZOOM: return "LOUPE";
    case G_CON: return "CONTRAST";
    case G_NORM: return "AUTO LEVEL";
    case G_BRI: return "BRIGHTNESS";
    case G_QLFO: return "LFO HELD = LFO";
    case G_ENG8: return "ENGINES";
    case G_ASAVE: return "AUTOSAVE";
    case G_LANG: return "LANGUAGE";
    case G_RS_GO: return "RESAMPLE";
    case G_RS_LEN: return "REC LENGTH";
    case G_DEEP: return "DEEP BASS";
    case G_RST: return "LAST STOP";
    case G_MLEARN: return "MIDI LEARN";
    case G_MMAP: return "CC MAP";
    case G_MSTART: return "START / STOP";
    case G_MPROG: return "PROGRAM CHANGE";
    case G_MCLK: return "CLOCK OUT";
    case I_ABMIX: return "A / B MIX";
    case G_FW: return "FIRMWARE";
    case G_MIDIMON: return "LAST NOTE / CC";
    case G_MIDIRT: return "LAST START / STOP";
    case G_SQ_STEP: return "STEP";
    case G_SQ_NOTE: return "NOTE";
    case G_SQ_TYPE: return "STEP TYPE";
    case G_SQ_PROB: return "PROBABILITY";
    case G_SQ_COND: return "TRIG CONDITION";
    case G_SQ_LKP: return "P-LOCK";
    case G_SQ_LKV: return "LOCK VALUE";
    case G_SQ_PAT: return "PATTERN";
    case G_SQ_LEN: return "PATTERN LENGTH";
    case G_SQ_GATE: return "NOTE LENGTH";
    case G_SQ_PRESET: return "PRESET";
    case G_DK_LVL: return "LEVEL";
    case G_DK_TUNE: return "TUNE";
    case G_DK_DEC: return "DECAY";
    case G_DK_TONE: return "TONE";
    case G_DK_TRK: return "INSTRUMENT";
    case G_DK_STEP: return "STEP";
    case G_DK_HIT: return "HIT";
    case G_DK_PROB: return "PROBABILITY";
    case G_DK_COND: return "TRIG CONDITION";
    case G_DK_PAT: return "PATTERN";
    case G_DK_PRE: return "PRESET";
    case G_DK_LEN: return "PATTERN LENGTH";
    case G_DK_KIT: return "KIT";
    case G_DK_VOL: return "DRUM VOLUME";
    case G_DK_MODE: return "PLAYS";
    case G_SQ_ON: return "SEQUENCE";
    case G_NEWSONG: return "NEW SONG";
    case G_DK_SWING: return "SWING";
    case G_DK_KSAT: return "KICK DRIVE";
    case G_DK_SNAP: return "SNARE SNAPPY";
    case G_DK_ECHO: return "DUB ECHO";
    case G_SQ_RATE: return "RATE";
    case G_SQ_SWING: return "SWING";
    case G_AR_ON: return "ARP";
    case G_AR_MODE: return "MODE";
    case G_AR_RATE: return "RATE";
    case G_AR_OCT: return "OCTAVES";
    case G_AR_GATE: return "GATE";
    case G_LV_FLT: return "DJ FILTER";
    case G_LV_PUMP: return "PUMP";
    case G_LV_STUT: return "STUTTER";
    case G_LV_TAPE: return "TAPE STOP";
    case G_LV_PRATE: return "PUMP RATE";
    case G_LV_FRES: return "FILTER RESO";
    case G_MC1: return "MACRO 1";
    case G_MC2: return "MACRO 2";
    case G_MC3: return "MACRO 3";
    case G_MC4: return "MACRO 4";
    case G_MC_DST: return "TARGET";
    case G_MC_AMT: return "AMOUNT";
    case G_EQ_F: return "FREQUENCY";
    case G_EQ_G: return "GAIN";
    case G_EQ_T: return "SHAPE";
    case G_EQ_Q: return "WIDTH (Q)";
    default: return "";
    }
}
static const char *item_sname(uint32_t id)
{
    uint32_t e;
    int k = osc_pk(id, &e);
    if (k) return k == 4 ? ENG_P4[e % NENG] : ENG_P[e % NENG][k - 1];   /* (P4: its own table; 1.0.34, ENG_P[e][3] ran past the last engine) */
    if (id == I_L1RATE || id == I_L2RATE) return "RATE";
    if (item_patch(id)) return PD[id].sname;
    return id == G_MIDICH ? "MIDI" : id == G_BPM ? "BPM" : id == G_CLOCK ? "CLOCK" : id == G_OCT ? "OCT" : id == G_THEME ? "THEME" : id == G_PAL ? "PALETTE" : id == G_FONT ? "FONT" : id == G_GRID ? "GRID" : id == G_BROWSE ? "BROWSE" : id == G_ACCEL ? "ACCEL" : id == G_C3COL ? "COLOURS" : id == G_UI ? "UI" : id == G_ASK ? "ASK" : id == G_ZOOM ? "LOUPE" : id == G_CON ? "CONTRAST" : id == G_NORM ? "LEVEL" : id == G_BRI ? "BRIGHT" : id == G_QLFO ? "QLFO" : id == G_ENG8 ? "ENGINES" : id == G_ASAVE ? "AUTOSAVE" : id == G_LANG ? "LANGUAGE" : id == G_RS_GO ? "RESAMP" : id == G_RS_LEN ? "LENGTH" : id == G_DEEP ? "DEEP" : id == G_MLEARN ? "LEARN" : id == G_MMAP ? "CC MAP" : id == I_ABMIX ? "A/B" : id == G_SQ_TYPE ? "TYPE" : id == G_SQ_PROB ? "PROB" : id == G_SQ_COND ? "COND" : id == G_SQ_PAT ? "PAT" : id == G_SQ_LEN || id == G_DK_LEN ? "STEPS" : id == G_SQ_GATE ? "NOTE LEN" : id == G_DK_LVL ? "LVL" : id == G_DK_DEC ? "DEC" : id == G_DK_TRK ? "INSTR" : id == G_DK_PROB ? "PROB" : id == G_DK_COND ? "COND" : id == G_DK_PAT ? "PAT" : id == G_DK_VOL ? "VOL" : id == G_AR_OCT ? "OCT" : item_name(id);
}
static uint32_t ilog2q8(uint32_t x)             /* log2(x) in Q8, x >= 1 */
{
    uint32_t m = 31;
    while (!(x >> m)) m--;
    return (m << 8) | (m >= 8u ? (x >> (m - 8u)) & 255u : (x << (8u - m)) & 255u);
}
/* a destination's name: the oscillators' with their engine's words (A WAVE, B DETUNE...) */
static const char *dst_name(uint32_t d)
{
    static char nb[2][20];
    static uint32_t t;
    uint32_t o = d == D_BP1 || d == D_BP2 || d == D_BP3 ? OSC_B : 0u;
    int32_t k = d == D_AP1 || d == D_BP1 ? 0 : d == D_AP2 || d == D_BP2 ? 1 : d == D_AP3 || d == D_BP3 ? 2 : -1;
    if (k >= 0) {
        char *p = nb[t ^= 1u];
        str_cat(str_cat(p, o ? "B " : "A "), ENG_P[(uint32_t)patch[A_ENG + o] % NENG][k]);
        return p;
    }
    if (d == D_AP4 || d == D_BP4) {             /* (the 4th: FOLD, or RING on CS) */
        char *p = nb[t ^= 1u];
        str_cat(str_cat(p, d == D_BP4 ? "B " : "A "), ENG_P4[(uint32_t)patch[d == D_BP4 ? B_ENG : A_ENG] % NENG]);
        return p;
    }
    return d < ND ? L_DST[d] : "";
}
static int dst_on(uint32_t d)                   /* an oscillator that is off is not offered */
{
    if (d == D_ALVL || d == D_AP1 || d == D_AP2 || d == D_AP3 || d == D_AP4 || d == D_APIT) return patch[A_LVL] || patch[X_MODE] == 1;
    if (d == D_BLVL || d == D_BP1 || d == D_BP2 || d == D_BP3 || d == D_BP4 || d == D_BPIT) return patch[B_LVL] || patch[X_MODE];
    if (d == D_GATE) return patch[ST_ON];       /* an effect that is off is not offered either */
    if (d >= D_CRBITS && d <= D_CRMIX) return patch[CR_ON];
    if (d >= D_CHRATE && d <= D_CHMIX) return patch[CH_ON];
    if (d >= D_DLFB && d <= D_DLMIX) return patch[DL_ON];
    if (d >= D_RVSIZE && d <= D_RVMIX) return patch[RV_ON];
    if (d == D_CPAMT) return patch[CP_ON];
    if (d >= D_PHRATE && d <= D_PHMIX) return patch[PH_ON];
    return 1;
}
static int32_t dst_step(int32_t v, int32_t d)
{
    int32_t i, at = 0;
    for (i = 0; i < ND; i++) if (DST_ORDER[i] == v) at = i;
    while (d) {
        int32_t s = d > 0 ? 1 : -1;
        for (i = at + s; i >= 0 && i < ND && !dst_on(DST_ORDER[i]); i += s) {}
        if (i < 0 || i >= ND) break;
        at = i;
        d -= s;
    }
    return DST_ORDER[at];
}

static void item_text(uint32_t id, char *b)
{
    int32_t v = item_get(id);
    uint32_t e, rid = id == I_L1RATE || id == I_L2RATE ? rate_id(id) : id;
    int k = osc_pk(id, &e);
    *b = 0;
    if (k) {
        int32_t q = v * 258;
        if (e == E_ANALOG && k == 1) {
            static const char *const W[4] = {"SINE", "TRI", "SAW", "SQUARE"};
            if ((v * 3) % 127 != 0) *b++ = '~';
            str_cat(b, W[(v * 3 + 63) / 127]);
        } else if (e == E_KICK && k == 1) { b = str_cat(b, "+"); b = fmt_u(b, (uint32_t)v * 48u / 127u, 0); str_cat(b, " ST"); }   /* 808: PUNCH, DROP */
        else if (e == E_KICK && k == 2) { b = fmt_u(b, 5u + (uint32_t)(((uint64_t)q * q * 295u) >> 30), 0); str_cat(b, " ms"); }
        else if (e == E_CS && k == 1) {          /* CS WAVE: SAW, BOTH, PULSE (~ in between) */
            static const char *const W[3] = {"SAW", "BOTH", "PULSE"};
            str_cat(b, W[(v * 2 + 63) / 127]);
        } else if (e == E_CS && k == 2) {          /* CS DETUNE: the two channels apart, cents */
            uint32_t c = (uint32_t)(((uint64_t)q * q * 350u) >> 30);   /* (tenths: 0 .. 350) */
            b = fmt_u(b, c / 10u, 0); *b++ = '.'; b = fmt_u(b, c % 10u, 0); str_cat(b, " CT");
        }
        else if (e == E_WAVE && k == 2) str_cat(b, L_WT[q >> 12]);
        else if (e == E_FM && k == 2) {          /* FM RATIO: operator 2's, then 3's and 4's when the ALGO uses them */
            b = str_cat(b, L_RATIO[q >> 11]);
            if (patch[id >= B_ENG ? B_P4 : A_P4] >= 16) {
                const uint16_t *r = FM_R34[q >> 11];
                uint32_t j;
                for (j = 0; j < 2u; j++) { *b++ = ' '; b = fmt_u(b, r[j] >> 8, 0); if (r[j] & 255u) { *b++ = '.'; b = fmt_u(b, (r[j] & 255u) * 10u >> 8, 0); } }
            }
        } else if (e == E_FM && k == 4) str_cat(b, L_ALG[v >> 4]);
        else if (e == E_GROWL && k == 4) str_cat(b, L_GRT[v >> 4]);
        else if (e == E_ADD && k == 4) str_cat(b, L_ADR[v >> 4]);
        else if (e == E_GROWL && k == 2) {      /* VOWEL: U O A E I, and the glide between two */
            int32_t pos = q * 4, vi = pos >> 15, fr = pos & 32767;
            if (vi > 3) { vi = 3; fr = 32767; }
            *b++ = "UOAEI"[vi];
            if (fr > 28672) b[-1] = "UOAEI"[vi + 1];
            else if (fr > 4096) { *b++ = '>'; *b++ = "UOAEI"[vi + 1]; }
            *b = 0;
        } else if (e == E_ADD && k == 2) { b = fmt_u(b, 1u + (((uint32_t)q * 7u) >> 15), 0); str_cat(b, " HARM"); }
        else if (e == E_ADD && k == 3) {
            if (!v) str_cat(b, "ALL");
            else { b = str_cat(b, v < 64 ? "ODD " : "EVEN "); b = fmt_u(b, (uint32_t)(v < 64 ? v : v - 63) * 100u / 64u, 0); str_cat(b, "%"); }
        }
        else if (e == E_PHASE && k == 2) str_cat(b, L_PDT[(q * 6) >> 15]);
        else if (e == E_SYNC && k == 1) { uint32_t r = 10u + (uint32_t)v * 70u / 127u; b = str_cat(b, "x"); b = fmt_u(b, r / 10u, 0); *b++ = '.'; fmt_u(b, r % 10u, 0); }
        else { b = fmt_u(b, (uint32_t)v * 100u / 127u, 0); str_cat(b, "%"); }
        return;
    }
    if (!item_patch(rid)) {
        switch (id) {
        case G_MIDICH: if (v) fmt_u(b, (uint32_t)v, 2); else str_cat(b, "OMNI"); break;
        case G_BPM: b = fmt_u(b, (uint32_t)v / 10u, 0); *b++ = '.'; b = fmt_u(b, (uint32_t)v % 10u, 0); str_cat(b, " BPM"); break;
        case G_CLOCK: str_cat(b, v ? "MIDI" : "INTERNAL"); break;
        case G_SYNC: str_cat(b, v ? "PO / VOLCA (LEFT)" : "OFF"); break;
        case G_L3_RATE: lk_text(set.l3[L3_SYNC] ? L1_DIV : L1_RATE, v, b); break;   /* (shown as LFO 1's are) */
        case G_L3_DEP: lk_text(L1_DEP, v, b); break;
        case G_L3_SHP: lk_text(L1_SHP, v, b); break;
        case G_L3_DST: str_cat(b, dst_name((uint32_t)v)); break;
        case G_L3_SYNC: lk_text(L1_SYNC, v, b); break;
        case G_OCT: fmt_s(b, v - 3); break;
        case G_THEME: { static const char *const TN[8] = {"CLASSIC GLOW", "STUDIO", "PURE", "RACK", "TECH", "RIBBON", "RIBBON CLASSIC", "CLASSIC"}; str_cat(b, TN[(uint32_t)v % 8u]); break; }
        case G_PAL: { static const char *const PN[11] = {"SYNTHWAVE", "MINT", "BLACK / WHITE", "INVERTED", "RED", "ICE", "FLUO", "PINK", "GOLD", "SKY", "BLAZE"}; int32_t q = v >= (int32_t)NPAL_BASE ? v - (int32_t)NPAL_BASE : v; str_cat(b, q < 2 ? PAL_NAME[set.theme % 7u][q] : PN[(q - 2) % 11]); if (v >= (int32_t)NPAL_BASE) str_cat(b, " RAD"); break; }
        case G_GRID: str_cat(b, v ? "STRAIGHT" : "ALL (T, D)"); break;
        case G_BROWSE: str_cat(b, v ? "FAVORITES" : "ALL"); break;
        case G_ACCEL: str_cat(b, v == 3 ? "ULTRA" : v == 2 ? "FAST" : v ? "ON" : "OFF"); break;
        case G_C3COL: str_cat(b, v ? "GREEN YEL ORANGE" : "THEME"); break;
        case G_UI: str_cat(b, v ? "SHIFT (3 COLUMNS)" : "CLASSIC"); break;
        case G_ASK: str_cat(b, v ? "YES" : "NO"); break;
        case G_ZOOM: str_cat(b, v ? "ON (BIG TYPE)" : "OFF"); break;
        case G_RS_GO: {
            char *q;
            if (rs_want) { q = str_cat(b, "REC "); q = fmt_u(q, rs_pos * 10u / RS_RATE / 10u, 0); *q++ = '.'; q = fmt_u(q, rs_pos * 10u / RS_RATE % 10u, 0); str_cat(q, " s"); }
            else if (rs_desc.len) { q = str_cat(b, "DONE "); q = fmt_u(q, rs_desc.len / RS_RATE, 0); str_cat(q, " s \xB7 AGAIN: +"); }
            else str_cat(b, "TURN +: RECORD");
            break;
        }
        case G_RS_LEN: { char *q = fmt_u(b, 2u + (uint32_t)v, 0); str_cat(q, " s"); break; }
        case G_QLFO: str_cat(b, v ? "LFO 2" : "LFO 1"); break;
        case G_ENG8: str_cat(b, v ? "ALL (8 ENGINES)" : "ESSENTIAL (4)"); break;
        case G_ASAVE: str_cat(b, v ? "EVERY MINUTE" : "OFF"); break;
        case G_LANG: { static const char *const LN[3] = {"ENGLISH", "FRANCAIS", "ESPANOL"}; str_cat(b, LN[v % 3]); break; }
        case G_BRI: str_cat(b, v == 3 ? "MAX" : v == 2 ? "BRIGHT" : v == 1 ? "NORMAL" : "ECO"); break;
        case G_NORM: str_cat(b, v ? "ON (EVEN LEVEL)" : "OFF"); break;
        case G_DEEP: str_cat(b, v == 4 ? "DEEP + HARMONICS" : v == 3 ? "SUB HARMONICS" : v == 2 ? "MAX (+7 dB UNDER 90 Hz)" : v ? "DEEP (+4 dB)" : "OFF"); break;
        case G_CON: str_cat(b, v == 2 ? "MAX (DEEP BLACK)" : v ? "HIGH" : "NORMAL"); break;
        case G_RST: str_cat(b, rst_why); break;
        case G_MLEARN: str_cat(b, ml_state == 1u ? "TURN A KNOB" : ml_state == 2u ? "SEND A CC" : "OFF"); break;
        case G_MMAP: if (v) { b = fmt_u(b, (uint32_t)v, 0); str_cat(b, v > 1 ? " CCS \xB7 LEFT: CLEAR" : " CC \xB7 LEFT: CLEAR"); } else str_cat(b, "EMPTY"); break;
        case G_MSTART: str_cat(b, v ? "PLAYS THE SEQ" : "IGNORED"); break;
        case G_MPROG: str_cat(b, v ? "LOADS A PRESET" : "IGNORED"); break;
        case G_MCLK: str_cat(b, v ? "SENT (INTERNAL)" : "OFF"); break;
        case I_ABMIX:
            if (v == 64) { str_cat(b, "A = B"); break; }
            b = str_cat(b, "A "); b = fmt_u(b, (uint32_t)(v <= 64 ? 100 : (127 - v) * 100 / 63), 0);
            b = str_cat(b, "% B "); b = fmt_u(b, (uint32_t)(v >= 64 ? 100 : v * 100 / 64), 0); str_cat(b, "%"); break;
        case G_FONT: { static const char *const FN[NFONT] = {"SANS", "MONO", "HYPER", "PIXEL", "OUTFIT", "INSTRUMENT", "TEKTUR", "JETBRAINS", "BIG SHOULDERS"}; str_cat(b, FN[(uint32_t)v % NFONT]); break; }
        case G_CPU:                             /* each core's share of a block, and the governor's step */
            b = str_cat(b, "C0 "); b = fmt_u(b, ks_load[0], 0); b = str_cat(b, "%");
            if (dual_on) { b = str_cat(b, " C1 "); b = fmt_u(b, ks_load[1], 0); b = str_cat(b, "%"); }
            if (ks_shed) { b = str_cat(b, " \xB7 -"); fmt_u(b, ks_shed, 0); }
            break;
        case G_FW: str_cat(b, "K-WASUB " APP_VER " BETA"); break;
        case G_MIDIMON: case G_MIDIRT: {        /* the last channel message in (status, data 1, data 2, in hex), and the last system one (FA start, FB continue, FC stop...): what a sequencer sends when it stops */
            static const char HX[] = "0123456789ABCDEF";
            uint8_t s0 = id == G_MIDIMON ? mm_st : mm_rt;
            if (!(id == G_MIDIMON ? mm_n : mm_rn)) { str_cat(b, "NONE YET"); break; }
            *b++ = HX[s0 >> 4]; *b++ = HX[s0 & 15u];
            if (id == G_MIDIMON) { *b++ = ' '; *b++ = HX[mm_d1 >> 4]; *b++ = HX[mm_d1 & 15u]; *b++ = ' '; *b++ = HX[mm_d2 >> 4]; *b++ = HX[mm_d2 & 15u]; }
            *b = 0;
            break;
        }
        case G_SQ_STEP: b = fmt_u(b, (uint32_t)v + 1u, 2); b = str_cat(b, "/"); fmt_u(b, sq_pat()->len, 2); break;
        case G_SQ_NOTE:
            if (!(*sq_fp(perf.cur, sq_cur_step) & SF_ON)) { str_cat(b, "REST"); break; }
            b = str_cat(b, NOTE_N[v % 12]); fmt_u(b, 1u + (uint32_t)v / 12u, 0); break;
        case G_SQ_TYPE: str_cat(b, L_SQTYPE[v % 5]); break;
        case G_SQ_PROB: b = fmt_u(b, (uint32_t)v, 0); str_cat(b, "%"); break;
        case G_SQ_COND: case G_DK_COND: str_cat(b, SC_NAME[(uint32_t)v % NSC]); break;
        case G_SQ_LKP: str_cat(b, PLB_NAME[(uint32_t)v % NPKB]); break;
        case G_SQ_LKV: if (v < PD[PB_ID[sq_lkp % NPKB]].lo) str_cat(b, "OFF"); else lk_text(PB_ID[sq_lkp % NPKB], v, b); break;
        case G_SQ_GATE: {
            static const char *const GN[16] = {"1/2 STEP", "1/8 STEP", "1/4 STEP", "3/4 STEP", "1 STEP", "2 STEPS", "3 STEPS", "4 STEPS",
                                               "6 STEPS", "8 STEPS", "12 STEPS", "16 STEPS", "24 STEPS", "32 STEPS", "64 STEPS", "HOLD (TIE)"};
            str_cat(b, GN[(uint32_t)v & 15u]); break;
        }
        case G_SQ_PRESET: case G_DK_PRE:
            if (!v) { str_cat(b, "MY PATTERN"); break; }
            b = str_cat(b, id == G_SQ_PRESET ? SQP[(uint32_t)(v - 1) % NSQP].name : DKP[(uint32_t)(v - 1) % NDKP].name);
            b = str_cat(b, " \xB7 ");
            fmt_u(b, pre_size(id == G_SQ_PRESET ? sq_len() : dk_len()), 0);
            break;
        case G_DK_LVL: case G_DK_DEC: case G_DK_TONE: case G_DK_VOL: fmt_u(b, (uint32_t)v, 0); break;
        case G_DK_TUNE: if (v) fmt_s(b, v); else str_cat(b, "0"); break;
        case G_DK_TRK: str_cat(b, dk_tname((uint32_t)v)); break;
        case G_DK_STEP: b = fmt_u(b, (uint32_t)v + 1u, 2); b = str_cat(b, "/"); fmt_u(b, dk_len(), 2); break;
        case G_DK_HIT: str_cat(b, v == 2 ? "ACCENT" : v ? "ON" : "OFF"); break;
        case G_DK_PROB: b = fmt_u(b, DK_PROB[15 - (v & 15)], 0); str_cat(b, "%"); break;
        case G_DK_PAT: b = str_cat(b, "P"); fmt_u(b, (uint32_t)v + 1u, 0); break;
        case G_DK_LEN: b = fmt_u(b, (uint32_t)v, 0); str_cat(b, " STEPS"); break;
        case G_DK_KIT: str_cat(b, dk_kname((uint32_t)v)); break;
        case G_DK_KSAT: case G_DK_SNAP: if (v) fmt_u(b, (uint32_t)v, 0); else str_cat(b, "OFF"); break;
        case G_DK_ECHO: str_cat(b, v ? "ON" : "OFF"); break;
        case G_SQ_ON: str_cat(b, v == 2 ? "ON \xB7 KEYS TRANSPOSE" : v ? "ON \xB7 KEYS PLAY" : "OFF (BASS LIVE)"); break;
        case G_NEWSONG: str_cat(b, new_arm == 2u ? "TURN AGAIN: ERASES ALL" : "TURN ALGO"); break;
        case G_DK_MODE: str_cat(b, v == 2 ? "BASS ONLY" : v ? "DRUMS ONLY" : "BASS + DRUMS"); break;
        case G_DK_SWING: if (v) { b = fmt_u(b, 50u + (uint32_t)v / 2u, 0); str_cat(b, "%"); } else str_cat(b, "OFF"); break;
        case G_SQ_PAT: b = str_cat(b, "P"); b = fmt_u(b, (uint32_t)v + 1u, 2); if (v < 8) { b = str_cat(b, " "); str_cat(b, SQ_DEF[v][0]); } break;
        case G_SQ_LEN: b = fmt_u(b, (uint32_t)v, 0); str_cat(b, " STEPS"); break;
        case G_SQ_RATE: case G_AR_RATE: str_cat(b, L_SRATE[v % 6]); break;
        case G_SQ_SWING: if (v) { b = fmt_u(b, 50u + (uint32_t)v / 2u, 0); str_cat(b, "%"); } else str_cat(b, "OFF"); break;
        case G_AR_ON: str_cat(b, v ? "ON" : "OFF"); break;
        case G_AR_MODE: str_cat(b, L_ARMODE[v % 5]); break;
        case G_AR_OCT: fmt_u(b, (uint32_t)v + 1u, 0); break;
        case G_AR_GATE: b = fmt_u(b, (uint32_t)v * 10u, 0); str_cat(b, "%"); break;
        case G_LV_FLT:
            if (!v) { str_cat(b, "OFF"); break; }
            b = str_cat(b, v < 0 ? "LOW PASS " : "HIGH PASS ");
            b = fmt_u(b, (uint32_t)(v < 0 ? -v : v) * 100u / 64u, 0); str_cat(b, "%"); break;
        case G_LV_STUT: { static const char *const ST[6] = {"OFF", "1/4", "1/8", "1/16", "1/32", "1/64"}; str_cat(b, ST[v % 6]); break; }
        case G_LV_TAPE: if (!v) str_cat(b, "OFF"); else if (v >= 127) str_cat(b, "STOP"); else { b = str_cat(b, "SLOW "); b = fmt_u(b, (uint32_t)v * 100u / 127u, 0); str_cat(b, "%"); } break;
        case G_LV_PRATE: { static const char *const PR[4] = {"1/4", "1/8", "1/2", "1 BAR"}; str_cat(b, PR[v & 3]); break; }
        case G_LV_PUMP: case G_LV_FRES: case G_MC1: case G_MC2: case G_MC3: case G_MC4:
            if (id == G_LV_PUMP && !v) { str_cat(b, "OFF"); break; }
            b = fmt_u(b, (uint32_t)v * 100u / 127u, 0); str_cat(b, "%"); break;
        case G_MC_DST: str_cat(b, v ? dst_name((uint32_t)v) : "-"); break;
        case G_MC_AMT: b = v ? fmt_s(b, v * 100 / 64) : fmt_u(b, 0, 0); str_cat(b, "%"); break;
        case G_EQ_F: { uint32_t h = KS_EQ_HZ[v % 48]; if (h < 1000u) { b = fmt_u(b, h, 0); str_cat(b, " Hz"); } else { b = fmt_u(b, h / 1000u, 0); *b++ = '.'; b = fmt_u(b, h / 100u % 10u, 0); str_cat(b, " kHz"); } break; }
        case G_EQ_G: b = v ? fmt_s(b, v) : fmt_u(b, 0, 0); str_cat(b, " dB"); break;
        case G_EQ_T: str_cat(b, L_EQT[v % NEQT]); break;
        case G_EQ_Q: { uint32_t q = KS_EQ_Q10[v & 7]; b = str_cat(b, "Q "); b = fmt_u(b, q / 10u, 0); *b++ = '.'; fmt_u(b, q % 10u, 0); break; }
        }
        return;
    }
    if (rid == SM_SPEED) {                      /* (turned left past 100 %: faster, up to 400 %) */
        b = fmt_u(b, (v >= 0 ? 256u - (uint32_t)v * 230u / 127u : 256u + (uint32_t)(-v) * 12u) * 100u / 256u, 0);
        str_cat(b, v > 0 ? "% (SLOWER)" : v < 0 ? "% (FASTER)" : "%");
        return;
    }
    if (rid == SM_START || rid == SM_END) {     /* (where it starts, where it ends, in % of the sample) */
        b = fmt_u(b, rid == SM_START ? (uint32_t)v * 100u / 128u : 100u - (uint32_t)v * 100u / 128u, 0);
        str_cat(b, "%");
        return;
    }
    switch (PD[rid].fmt) {
    case FM_PCT: b = fmt_u(b, (uint32_t)v * 100u / 127u, 0); str_cat(b, "%"); break;
    case FM_BIP: b = v ? fmt_s(b, v * 100 / 64) : fmt_u(b, 0, 0); str_cat(b, "%"); break;
    case FM_LIST: case FM_DIV: str_cat(b, PD[rid].list[(rid == L1_DIV || rid == L2_DIV) && set.grid ? DIV_NEAR[v & 15] : v]); break;
    case FM_DEST: str_cat(b, dst_name((uint32_t)v)); break;
    case FM_INT: if (v) fmt_s(b, v); else fmt_u(b, 0, 0); break;
    case FM_SEMI: b = v && PD[rid].lo < 0 ? fmt_s(b, v) : fmt_u(b, (uint32_t)(v < 0 ? -v : v), 0); str_cat(b, " st"); break;
    case FM_CENT: b = v ? fmt_s(b, v) : fmt_u(b, 0, 0); str_cat(b, " ct"); break;
    case FM_TIME: {
        uint32_t t = rid == V_GLIDE ? KS_GLIDE_MS10[v] : KS_ENV_MS10[v];
        if (rid == V_GLIDE && !v) { str_cat(b, "OFF"); break; }
        if (t < 100u) { b = fmt_u(b, t / 10u, 0); *b++ = '.'; b = fmt_u(b, t % 10u, 0); str_cat(b, " ms"); }
        else if (t < 10000u) { b = fmt_u(b, t / 10u, 0); str_cat(b, " ms"); }
        else { b = fmt_u(b, t / 10000u, 0); *b++ = '.'; b = fmt_u(b, t / 1000u % 10u, 0); str_cat(b, " s"); }
        break;
    }
    case FM_CUT: {
        uint32_t hz = KS_CUT_HZ[v * 2];
        if (hz < 1000u) { b = fmt_u(b, hz, 0); str_cat(b, " Hz"); }
        else { b = fmt_u(b, hz / 1000u, 0); *b++ = '.'; b = fmt_u(b, hz / 100u % 10u, 0); str_cat(b, " kHz"); }
        break;
    }
    case FM_LFOHZ: {
        uint32_t h = KS_LFO_HZ100[v];
        b = fmt_u(b, h / 100u, 0); *b++ = '.'; b = fmt_u(b, h % 100u, 2); str_cat(b, " Hz");
        break;
    }
    case FM_VOL: {
        int32_t db;
        if (!v) { str_cat(b, "-INF"); break; }
        db = (int32_t)(ilog2q8((uint32_t)(v * v)) - ilog2q8(10000u)) * 771 / 65536;   /* 10 log10(v^2/100^2) */
        b = db ? fmt_s(b, db) : fmt_u(b, 0, 0);
        str_cat(b, " dB");
        break;
    }
    }
}
static uint32_t item_frac(uint32_t id)          /* 0..256 for the bars */
{
    int32_t lo, hi, v = item_get(id);
    item_range(id, &lo, &hi);
    return hi > lo ? (uint32_t)((v - lo) * 256 / (hi - lo)) : 0u;
}

/* ---- the picture of the sound (PLAY page, when nothing plays): two cycles of A + B + sub */
static void preview_make(void)
{
    osc_t oa, ob;
    oscp_t qa = {0}, qb = {0};
    int32_t a[16], bb[16], pk = 1, i, j, la = lvl15(patch[A_LVL]), lb = lvl15(patch[B_LVL]), ls = lvl15(patch[S_LVL]);
    int32_t w[224];
    uint32_t ps = 0;
    memset(&oa, 0, sizeof oa);
    memset(&ob, 0, sizeof ob);
    qa.eng = (uint8_t)patch[A_ENG]; qa.p1 = patch[A_P1] * 258; qa.p2 = patch[A_P2] * 258; qa.p3 = patch[A_P3] * 258;
    qb.eng = (uint8_t)patch[B_ENG]; qb.p1 = patch[B_P1] * 258; qb.p2 = patch[B_P2] * 258; qb.p3 = patch[B_P3] * 258;
    qa.inc = 0xFFFFFFFFu / 112u;
    qb.inc = qa.inc;
    for (i = 0; i < 224; i += 16) {
        osc_render(&oa, &qa, a, 0, 16);
        osc_render(&ob, &qb, bb, 0, 16);
        for (j = 0; j < 16; j++) {
            int32_t x = mul15(a[j], la) + mul15(patch[X_MODE] == 1 ? mul15(a[j], bb[j]) : bb[j], lb);
            ps += qa.inc >> (2 - patch[S_OCT]);
            x += mul15(sin32(ps), ls);
            w[i + j] = x;
            if ((x < 0 ? -x : x) > pk) pk = x < 0 ? -x : x;
            if (i + j < 112) { prev_a[i + j] = (int16_t)clip(a[j], 32767); prev_b[i + j] = (int16_t)clip(bb[j], 32767); }
        }
    }
    for (i = 0; i < 112; i++) {                 /* the sub, one cycle */
        uint32_t ph = (uint32_t)i * (0xFFFFFFFFu / 112u);
        prev_s[i] = (int16_t)(patch[S_WAVE] == 0 ? sin32(ph) : patch[S_WAVE] == 1 ? tri32(ph + 0x40000000u)
                              : (ph < (patch[S_WAVE] == 2 ? 0x80000000u : 0x40000000u) ? -30000 : 30000));
    }
    pk = (32000 << 12) / pk;                     /* (a gain, Q12: no 64-bit division) */
    for (i = 0; i < 224; i++) prev_wave[i] = (int16_t)((w[i] * pk) >> 12);
}

#include "ksub_draw.c"

/* ================================================================ the panel */
static void led(uint32_t id, uint32_t lvl) { if (id < 41u) led_want[id] = (uint8_t)lvl; }
#if defined(DINGHY_HOST) || defined(KAWA_WASM)
static void leds_flush(void) { host_leds(led_want); }
#else
static void leds_flush(void)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++) {
        uint8_t on = 0, dim = 0;
        for (r = 1; r < 5u; r++) {
            int8_t id = FM1_KEYMAP[r][p];
            if (id < 0 || id > 40) continue;
            if (led_want[id] == 2u) on |= (uint8_t)(1u << r);
            else if (led_want[id] == 1u) dim |= (uint8_t)(1u << r);
        }
        fm1_led[p] = on;
        fm1_led_dim[p] = dim;
    }
}
#endif
static void ui_leds(void)
{
    uint32_t i, beat = ((ks_beats >> 24) & 1u) == 0u && ((ks_beats >> 21) & 7u) < 2u;
    for (i = 0; i < 41u; i++) led_want[i] = 0;
    if (new_arm && fm1_ms - new_ms > 3000u) { new_arm = 0; g_force = 1; }
    for (i = 0; i < NPAGE; i++) led(PAGE_BTN[i], i == page ? 2u : 1u);
    led(BTN_PLAY, sq_play ? (beat ? 2u : 1u) : hold ? 1u : 0u);
    led(BTN_REC, sq_rec ? 2u : beat ? 1u : 0u);
    led(BTN_SAVE, VIEWS[vidx()].lay == LY_SAVE ? 2u : 0u);
    if (dk_view()) {                            /* DRUMS: the white keys light where the chosen instrument hits on the page shown (a landmark, as the dots on the screen) */
        const dkpat_t *q = dk_pp();
        uint32_t len = dk_len(), t = dk_trk % NDK, base = dk_cur / 16u * 16u;
        for (i = 0; i < 16u; i++) if (base + i < len && (q->on[base + i] >> t & 1u)) led(KEY_LED(PK_WHITE[i]), 2u);
    }
    for (i = 0; i < 27u; i++) if (fm1_in.notes & (1u << i)) led(KEY_LED(i), 2u);
    leds_flush();
}

static void notes_all_off(void)
{
    uint32_t n;
    for (n = 0; n < 128u; n++) if (latched[n] || phys[n]) { ev_push(EV_OFF, n, 0); latched[n] = phys[n] = 0; }
}
static void panic_all(void)                     /* PANIC (REC held, OCT- + OCT+, MIDI CC 123): every note, the FX PADS and FX LIVE, and the
                                                 * delay, reverb, chorus and dub echo emptied (1.0.36: an old echo kept playing behind) */
{
    notes_all_off();
    ev_push(EV_PANIC, 0, 0);
    pad_release_all();
    lfx_flt = 0; lfx_pump = 0; lfx_stut = 0; lfx_tape = 0;
    fx_flush = 2;
    toast("ALL NOTES OFF");
}
static void key_down(uint32_t n, uint32_t vel)
{
    uint32_t i, any = 0;
    if (sq_rec) {                               /* REC: the key goes to the step (stopped: the cursor's, then the next; playing: the one now) */
        uint32_t k = perf.cur % SQ_NPAT, c = sq_play ? sq_now_step() : sq_cur_step % SQ_MAX;
        uint8_t *f = sq_fp(k, c);
        *sq_np(k, c) = (uint8_t)clampi((int32_t)n - 24, 0, 48);   /* (the note as heard: the step plays 24 + this; 1.0.44: it was n - 48 - 12 OCT, an octave under what was played at OCT -1) */
        if (sq_play) {
            *f = SF_ON;
            sqx.p[k].cond[c] &= 15u;
            sqx.p[k].prob[c] = 100;
            rec_hold.on = 1; rec_hold.n = (uint8_t)n; rec_hold.step = (uint8_t)c; rec_hold.at = ks_beats;
        } else {
            if (!(*f & SF_ON)) *f = SF_ON;
            sq_cur_step = (uint8_t)((c + 1u) % sq_len());
        }
        sq_edited();
        sqx_touch();
    }
    if (hold) {                                 /* a new phrase on a latched one: the old notes go */
        for (i = 0; i < 128u; i++) any |= phys[i];
        if (!any) for (i = 0; i < 128u; i++) if (latched[i]) { latched[i] = 0; ev_push(EV_OFF, i, 0); }
    }
    phys[n] = 1;
    ev_push(EV_ON, n, vel);
}
static void key_up(uint32_t n)
{
    if (rec_hold.on && rec_hold.n == n) {       /* REC LIVE: the note as long as the key was held (the nearest length) */
        uint32_t k = perf.cur % SQ_NPAT, held = (ks_beats + BEATS_WRAP - rec_hold.at) % BEATS_WRAP, i, best = 0;
        uint64_t q24 = (uint64_t)held * STEP_PB[sq_pat()->rate % 6u], bd = ~0ull;
        for (i = 0; i < 15u; i++) { uint64_t d = SQ_GL[i] > q24 ? SQ_GL[i] - q24 : q24 - SQ_GL[i]; if (d < bd) { bd = d; best = i; } }
        sqx.p[k].cond[rec_hold.step % SQ_MAX] = (uint8_t)((sqx.p[k].cond[rec_hold.step % SQ_MAX] & 15u) | best << 4);
        rec_hold.on = 0;
        sqx_touch();
    }
    if (!phys[n]) return;
    phys[n] = 0;
    if (hold) latched[n] = 1;
    else ev_push(EV_OFF, n, 0);
}

static uint32_t rate_like(uint32_t id) { return id == I_L1RATE || id == I_L2RATE ? rate_id(id) : id; }
static uint32_t turn_id = I_NONE;
static uint32_t fx_on_of(uint32_t id)           /* the ON of the effect this setting belongs to (0: none) */
{
    if (id > DR_ON && id <= DR_MIX) return DR_ON;
    if (id > CR_ON && id <= CR_MIX) return CR_ON;
    if (id > CH_ON && id <= CH_MIX) return CH_ON;
    if ((id > DL_ON && id <= DL_MIX) || id == DL_TYPE) return DL_ON;
    if ((id > RV_ON && id <= RV_MIX) || id == RV_TYPE) return RV_ON;
    if (id > ST_ON && id <= ST_SMO) return ST_ON;
    if (id > CP_ON && id <= CP_PRATE) return CP_ON;
    if (id > PH_ON && id <= PH_MIX) return PH_ON;
    return 0;
}
static void lk_text(uint32_t id, int32_t v, char *b) { lk_fmt_id = (uint8_t)id; lk_fmt_v = (int8_t)v; lk_fmt_on = 1; item_text(id, b); lk_fmt_on = 0; }
static void edit_item(uint32_t id, int32_t d)
{
    int32_t lo, hi, v = item_get(id), step = 1;
    uint32_t k;
    if (id == I_NONE || id == G_CPU || id == G_FW || id == G_MIDIMON || id == G_MIDIRT || id == G_RST) return;
    if (ml_state == 1u && id != G_MLEARN && id != G_MMAP) { ml_id = (uint8_t)id; ml_state = 2; toast("LEARN: SEND A CC"); }
    if (sq_rec && sq_play && !dk_view()) {      /* REC while it plays: a SEQ BASS setting turned is locked on the step now (the sound keeps its own) */
        uint32_t w, rid = rate_like(id);
        for (w = 0; w < NPKB && PB_ID[w] != rid; w++) {}
        if (w < NPKB) { sq_lock_at(sq_now_step(), w, d); return; }
    }
    k = fx_on_of(id);                           /* an effect (or STEP) that is off comes on when its setting goes up */
    if (k && !patch[k] && d > 0) {
        char t[24];
        uint32_t f;
        patch[k] = 1;
        for (f = 0; f < NFXV && FX_ON[f] != k; f++) {}
        str_cat(str_cat(t, f < NFXV ? FX_TITLE[f] : "STEP FX"), " ON");
        toast(t);
    }
    if ((item_patch(id) && PD[id].fmt == FM_DEST) || id == G_L3_DST) { item_set(id, dst_step(v, d)); dpop_id = (uint8_t)id; dpop_ms = fm1_ms | 1u; return; }
    if (item_patch(id) && PD[id].list == L_SRC) { item_set(id, v + d); dpop_id = (uint8_t)id; dpop_ms = fm1_ms | 1u; return; }   /* (a source: the list too) */
    if (set.grid && (rate_like(id) == L1_DIV || rate_like(id) == L2_DIV)) {   /* STRAIGHT: from round to round */
        int32_t i, at = 0;
        if (vpop_id != (uint8_t)id || !vpop_ms) { item_text(id, vpop_before); vpop_id = (uint8_t)id; }
        vpop_ms = fm1_ms | 1u;
        for (i = 0; i < 9; i++) if (DIV_STRAIGHT[i] == DIV_NEAR[v & 15]) at = i;
        item_set(id, DIV_STRAIGHT[clampi(at + d, 0, 8)]);
        return;
    }
    if (id != G_CPU && (vpop_id != (uint8_t)id || !vpop_ms)) { item_text(id, vpop_before); vpop_id = (uint8_t)id; }
    vpop_ms = fm1_ms | 1u;
    if (id == A_ENG || id == B_ENG) {           /* the engines, in their order (FOLD and VOCAL gone) */
        int32_t i, at = 0;
        for (i = 0; i < (int32_t)NENGO; i++) if (ENG_ORDER[i] == v) at = i;
        item_set(id, ENG_ORDER[clampi(at + d, 0, (int32_t)NENGO - 1)]);
        return;
    }
    if (id == SM_SEL) {                         /* SAMPLE: the samples in order, RESAMPLE (its value kept: 16) last */
        int32_t i, at = 0;
        static const int8_t ORD[29] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 16};
        for (i = 0; i < 29; i++) if (ORD[i] == v) at = i;
        item_set(id, ORD[clampi(at + d, 0, 28)]);
        return;
    }
    {                                           /* WAVETABLE: the table (WAVE's 2nd setting) one per click: the 8 tables sit 16 values apart */
        uint32_t e2;
        if (osc_pk(id, &e2) == 4 && (e2 == E_FM || e2 == E_GROWL || e2 == E_ADD)) {   /* (and FM's ALGO, GROWL's TYPE, ADDITIVE's REGISTER: one per click) */
            item_set(id, clampi((v >> 4) + (d > 0 ? 1 : d < 0 ? -1 : 0), 0, 7) * 16);
            return;
        }
        if (osc_pk(id, &e2) == 2 && e2 == E_WAVE) {
            int32_t t = clampi(((v * 258) >> 12) + (d > 0 ? 1 : d < 0 ? -1 : 0), 0, 7);
            item_set(id, (t * 4096 + 257) / 258);    /* (the first value of table t) */
            return;
        }
    }
    item_range(id, &lo, &hi);
    if (id != turn_id || fm1_ms - last_turn_ms > 150u) turn_start_ms = fm1_ms;   /* (a turn that goes on: ULTRA speeds it up) */
    if (set.accel && hi - lo > 24 && id == turn_id) {   /* ACCEL: the faster the turn, the bigger the step */
        uint32_t dt = fm1_ms - last_turn_ms, held = fm1_ms - turn_start_ms;
        step = dt < 20u ? 6 : dt < 40u ? 4 : dt < 70u ? 2 : 1;
        if (step > 1) {                         /* (turned slowly: always one by one, whatever the mode) */
            if (set.accel >= 2u) step *= 2;
            if (set.accel == 3u && held > 1000u) step *= 1 + (int32_t)((held - 1000u) / 400u > 3u ? 3u : (held - 1000u) / 400u);   /* (ULTRA: after a second, up to 4 times more) */
            if (hi - lo <= 64 && step > (set.accel == 3u ? 6 : 3)) step = set.accel == 3u ? 6 : 3;   /* (short ranges: never past a few) */
        }
        if (hi - lo > 300) step = step > 1 ? step * (hi - lo) / 128 : 10;   /* (the tempo, in tenths: slowly, one BPM a click) */
    }
    turn_id = id;
    last_turn_ms = fm1_ms;
    item_set(id, v + d * step);
}
static void ask_load(uint32_t bank, uint32_t k)   /* load at once (SAVE held: UNDO brings the sound back) */
{
    if (bank && !up_valid(k)) { toast("EMPTY SLOT"); return; }
    pre_cur[bank] = (uint8_t)k;
    load_preset(bank, k);
    toast(pname);
}
static void preset_step(int32_t d)              /* bank A then the user's saved patches of bank B, round */
{
    uint32_t n = NFACTORY + NUSER, at = pend_on ? (pend_bank ? NFACTORY + pend_k : pend_k) : (set.bank ? NFACTORY + set.idx : set.idx);
    uint32_t st = d < 0 ? n - 1u : 1u, cnt = (uint32_t)(d < 0 ? -d : d);
    int fo = set.browse && fav_any();          /* (FAVORITES: only those; a TAG: only those) */
    if (pre_tag && !pre_count(0) && !pre_count(1)) { toast("NO SOUND WITH THAT TAG"); return; }
    while (cnt--) {
        uint32_t tries = n, bk, k;              /* (one round at most: FAVORITES and a TAG may leave none; 1.0.35, it looped for ever) */
        for (;;) {
            at = (at + st) % n; bk = at >= NFACTORY; k = bk ? at - NFACTORY : at;
            if ((!bk || up_valid(k)) && (!fo || fav_is(bk, k)) && pre_match(bk, k)) break;
            if (!--tries) { toast("NO SOUND: FAVORITES + TAG"); return; }
        }
    }
    ask_load(at >= NFACTORY, at >= NFACTORY ? at - NFACTORY : at);
}
static void save_open(void)
{
    uint32_t i;
    if (set.bank) save_slot = set.idx;
    else for (save_slot = 0; save_slot < NUSER - 1u && up_valid(save_slot); save_slot++) {}
    for (i = 0; i < 13u && pname[i]; i++) {}
    for (; i < 13u; i++) pname[i] = ' ';        /* the name as 13 letters to edit */
    pname[13] = 0;
    save_pos = 0;
    page = PG_PRE;
    vcur[PG_PRE] = 2;
}
static void init_sound(void)                    /* the INIT factory sound, an empty name, the first empty slot (undo: SAVE held + OCT-) */
{
    uint32_t k, i;
    char nm[14];
    for (k = 0; k < NFACTORY; k++) { int8_t pp[NP_MAX]; preset_factory(k, pp, nm); if (nm[0] == 'I' && nm[1] == 'N' && nm[2] == 'I' && nm[3] == 'T' && !nm[4]) break; }
    if (k >= NFACTORY) { toast("NO INIT SOUND"); return; }
    load_preset(0, k);
    for (i = 0; i < 13u; i++) pname[i] = ' ';
    pname[13] = 0;
    save_pos = 0;
    for (save_slot = 0; save_slot < NUSER - 1u && up_valid(save_slot); save_slot++) {}
}
static void song_new(void)                      /* NEW SONG: the INIT sound, the drums, SEQ BASS, the locks and the automation empty; the EQ back flat; the saved patches, the macros and the settings stay */
{
    uint32_t k;
    ev_push(EV_SEQ, 0, 0);
    sq_rec = 0; rec_hold.on = 0; dk_hold = 0; aut_erasing = 0; aut_any = 0; aut_last = 0xFF;
    memset(aut_saved, 0, sizeof aut_saved);
    notes_all_off();
    init_sound();
    dk_defaults(); pl_defaults(); sqx_defaults(); eq_defaults(); eq_dirty = 1;
    for (k = 0; k < SQ_NPAT; k++) { memset(&perf.pat[k], 0, sizeof perf.pat[k]); perf.pat[k].len = 32; perf.pat[k].rate = 3; }
    perf.cur = 0; sq_cur_step = 0; sq_psel = dk_psel = 0; sq_bk.ok = dk_bk.ok = 0;
    dk_cur = dk_trk = 0;
    sqx_touch(); dk_touch(); pl_mark(); perf_touch();
    toast("NEW SONG");
    g_force = 1;
}
static void new_step(uint32_t which)            /* 1 NEW PRESET, 2 NEW SONG: the first turn asks, the second (within 3 s) does it */
{
    if (new_arm != which || fm1_ms - new_ms > 3000u) {
        new_arm = (uint8_t)which; new_ms = fm1_ms; g_force = 1;
        if (which == 2u) toast("NEW SONG? TURN AGAIN TO ERASE ALL");
        return;
    }
    new_arm = 0;
    if (which == 2u) song_new();
    else { init_sound(); toast("NEW PRESET: TYPE A NAME"); }
    g_force = 1;
}
static void name_trim(void) { int32_t i; for (i = 12; i >= 0 && pname[i] == ' '; i--) pname[i] = 0; }

static void first_view(uint32_t pg) { vcur[pg] = 0; c3_show[pg] = COL3[pg].title ? 1u : 0u; }
static void view_next(void)                     /* the page's next view (SHIFT: the 3 columns first) */
{
    uint32_t nv = (uint32_t)(PAGE_V0[page + 1u] - PAGE_V0[page]);
    if (set.ui && COL3[page].title) {
        if (c3_show[page]) { c3_show[page] = 0; vcur[page] = 0; }
        else if (vcur[page] + 1u >= nv) { c3_show[page] = 1; vcur[page] = 0; }
        else vcur[page]++;
        return;
    }
    vcur[page] = (uint8_t)((vcur[page] + 1u) % nv);
}
static void view_step(int32_t d)               /* the PRESET knob off HOME and PRESETS: the page's views, either way, round (SHIFT: its 3 columns first) */
{
    uint32_t nv = (uint32_t)(PAGE_V0[page + 1u] - PAGE_V0[page]), c3 = set.ui && COL3[page].title, n = nv + c3, at;
    at = c3 ? (c3_show[page] ? 0u : vcur[page] + 1u) : vcur[page];
    at = (uint32_t)(((int32_t)at + d % (int32_t)n + (int32_t)n) % (int32_t)n);
    if (c3) { c3_show[page] = (uint8_t)(at == 0u); vcur[page] = (uint8_t)(at ? at - 1u : 0u); }
    else vcur[page] = (uint8_t)at;
    vpop_ms = 0; g_force = 1;
}
static void win_step(int32_t d)                 /* ... in a SEL window: the windows, round (PERFORM, MORPH, FX LIVE, MACROS, MORPH A/B) */
{
    static const uint8_t W[5] = {7, 4, 1, 2, 5};
    int32_t i, at = 0;
    for (i = 0; i < 5; i++) if (W[i] == win) at = i;
    win = W[((at + d % 5) % 5 + 5) % 5];
    vpop_ms = 0; g_force = 1;
}
static void on_button(uint32_t b)
{
    uint32_t i;
    dpop_ms = 0;                                /* (a button closes the destination list) */
    if (pend_on) {                              /* the question: SEL loads, any other button keeps the sound */
        pend_on = 0;
        if (b == BTN_SEL) { load_preset(pend_bank, pend_k); toast(pname); }
        else toast("KEPT");
        return;
    }
    for (i = 0; i < NPAGE; i++)
        if (PAGE_BTN[i] == b) {
            if (win && win != 3u) { win = 0; vpop_ms = 0; page = (uint8_t)i; first_view(i); if (i == PG_PRE) vcur[i] = ARP_VIEW; name_trim(); return; }   /* (a window open: back to that page) */
            if (page == i) view_next();
            else { page = (uint8_t)i; first_view(i); if (i == PG_PRE) vcur[i] = ARP_VIEW; }   /* (another page: always its first view; the ARP button's: the ARPEGGIATOR, then PRESETS A, B; 1.0.45) */
            if (VIEWS[vidx()].lay == LY_SAVE) vcur[i]++;   /* (SAVE opens from SAVE only: on to the ARP) */
            name_trim();
            return;
        }
    switch (b) {
    case BTN_SEL:                               /* the windows: FX LIVE, MACROS, back (held: COMPARE) */
        { static const uint8_t NEXTW[8] = {7, 2, 5, 7, 1, 0, 0, 4}; win = NEXTW[win % 8u]; }   /* PERFORM, MORPH X/Y, FX LIVE, MACROS, MORPH A/B (from the EQ view: PERFORM) */
        if (win == 4u || win == 5u || win == 7u) mx_open();
        win_cur = 0;
        vpop_ms = 0;
        g_force = 1;
        break;
    case BTN_SAVE:
        if (VIEWS[vidx()].lay == LY_SAVE) { name_trim(); save_user(save_slot); vcur[PG_PRE] = 1; pre_cur[1] = save_slot; }
        else save_open();
        break;
    case BTN_PLAY:                              /* SEQ BASS: play / stop (held: HOLD, ui_input); in MORPH A/B: across */
        if (win == 5u) { ab_go(); break; }
        seq_play(!sq_play);
        toast(sq_play ? "SEQ STOP" : "SEQ PLAY");
        break;
    case BTN_REC: {                             /* SEQ view: step record; elsewhere TAP: the tempo from the last taps */
        if (vidx() == V_SEQ || vidx() == V_DRUM) {   /* REC: stopped, the keys write steps; playing, they record live */
            sq_rec ^= 1u;
            rec_hold.on = 0;
            toast(!sq_rec ? "REC OFF" : vidx() == V_DRUM ? (sq_play ? "REC LIVE: BLACK KEYS" : "REC: PLAY, THEN BLACK KEYS")
                                                         : (sq_play ? "REC LIVE: PLAY THE KEYS" : "REC: KEYS WRITE STEPS"));
            break;
        }
        uint32_t now = fm1_ms, d = now - tap_ms[0];
        if (d > 250u && d < 2000u) {
            uint32_t avg = tap_ms[1] && tap_ms[0] - tap_ms[1] < 2000u ? (d + (tap_ms[0] - tap_ms[1])) / 2u : d;
            item_set(G_BPM, (int32_t)(600000u / avg));
        }
        tap_ms[1] = tap_ms[0];
        tap_ms[0] = now;
        ev_push(EV_SYNC, 0, 0);
        toast("TAP");
        break;
    }
    case BTN_OCT_DOWN:
    case BTN_OCT_UP:
        if ((fm1_in.buttons & 3u) == 3u) { panic_all(); break; }
        if (dk_view()) {                        /* DRUMS: OCT- / OCT+ the page of 16 steps */
            uint32_t len = dk_len(), np = (len + 15u) / 16u, pg = dk_cur / 16u;
            char t[16], *p;
            pg = b == BTN_OCT_UP ? (pg + 1u) % np : (pg + np - 1u) % np;
            dk_cur = (uint8_t)(pg * 16u + dk_cur % 16u);
            if (dk_cur >= len) dk_cur = (uint8_t)(len - 1u);
            p = fmt_u(str_cat(t, "PAGE "), pg + 1u, 0); p = str_cat(p, "/"); fmt_u(p, np, 0);
            toast(t);
            break;
        }
        item_set(G_OCT, OCT + 3 + (b == BTN_OCT_UP ? 1 : -1));
        {
            char t[12], *p = str_cat(t, "OCTAVE ");
            if (OCT) fmt_s(p, OCT); else str_cat(p, "0");
            toast(t);
        }
        break;
    }
}

static void c3_edit(uint32_t id, int32_t d)
{
    vpop_pre[0] = 0;
    edit_item(id, d);
}
static void on_enc(uint32_t e, int32_t d)
{
    const view_t *v = view();
    uint32_t vi = vidx();
    if (dk_view() && e != ENC_PRESETS) {        /* DRUMS: a step held, its locks (K1-K4 LEVEL TUNE DECAY TONE, ALGO RATCHET, SELECT ECHO); REC playing: K1-K4 locked on the step now */
        if (dk_hold) { dk_lock_at(dk_hold - 1u, e == ENC_ALGORITHM ? PK_RAT : e == ENC_SELECT ? PK_ECHO : e - ENC_KNOB1, d); return; }
        if (sq_rec && sq_play && e >= ENC_KNOB1 && e <= ENC_KNOB4) { dk_lock_at(dk_now_step(), e - ENC_KNOB1, d); return; }
    }
    if (win == 3u && e != ENC_PRESETS) {        /* the EQ: the band chosen (SELECT); K1 FREQ, K2 GAIN, K3 Q, K4 SHAPE; ALGO its frequency */
        static const uint8_t EK[4] = {G_EQ_F, G_EQ_G, G_EQ_Q, G_EQ_T};
        vpop_pre[0] = 0;
        if (e == ENC_SELECT) { eq_cur = (uint8_t)clampi(eq_cur + d, 0, 3); g_force = 1; }
        else if (e == ENC_ALGORITHM) edit_item(G_EQ_F, d);
        else if (e >= ENC_KNOB1 && e <= ENC_KNOB4) edit_item(EK[e - ENC_KNOB1], d);
        return;
    }
    if (win && e != ENC_PRESETS) {              /* the SEL windows: K1..K4 their four, SELECT a row, ALGO its value */
        vpop_pre[0] = 0;
        if (win == 6u) {                        /* FX PADS: K1 CUTOFF, K2 RESONANCE, K3 FILTER TYPE, K4 MORPH A/B; ALGO the QUANTIZE */
            static const uint8_t PK[3] = {F_CUT, F_RES, F_TYPE};
            if (e == ENC_ALGORITHM) { set.pquant = (uint8_t)clampi(set.pquant + d, 0, 8); set_dirty = 1; set_ms = fm1_ms; }
            else if (e >= ENC_KNOB1 && e <= ENC_KNOB3) edit_item(PK[e - ENC_KNOB1], d);
            else if (e == ENC_KNOB4) mx_knob(0, d);
            return;
        }
        if (win == 7u) {                        /* PERFORM: K1..K4 the macros, SELECT / ALGO the morph's X / Y */
            if (e >= ENC_KNOB1 && e <= ENC_KNOB4) edit_item(G_MC1 + (e - ENC_KNOB1), d);
            else mx_knob(e == ENC_SELECT ? 0u : 1u, d);
            return;
        }
        if (win >= 4u) {                        /* MORPH: SELECT and ALGO are K3 and K4 */
            if (e >= ENC_KNOB1 && e <= ENC_KNOB4) mx_knob(e - ENC_KNOB1, d);
            else mx_knob(e == ENC_SELECT ? 2u : 3u, d);
            return;
        }
        if (win == 1u) {
            if (e == ENC_SELECT) win_cur = (uint8_t)clampi(win_cur + d, 0, 5);
            else if (e == ENC_ALGORITHM) edit_item(LV_IT[win_cur], d);
            else if (e >= ENC_KNOB1 && e <= ENC_KNOB4) { win_cur = (uint8_t)(e - ENC_KNOB1); edit_item(LV_IT[win_cur], d); }
            return;
        }
        if (e == ENC_SELECT) mc_pos = (uint8_t)clampi(mc_pos + d, 0, 31);
        else if (e == ENC_ALGORITHM) {
            if (mc_pos & 1u) edit_item(G_MC_AMT, d);
            else {                              /* (a target: every destination, the oscillators' with their engine's words) */
                int32_t i, at = 0, cur = perf.mc_dst[(mc_pos >> 1) & 15u];
                for (i = 0; i < ND; i++) if (DST_ORDER[i] == cur) at = i;
                item_set(G_MC_DST, DST_ORDER[clampi(at + d, 0, ND - 1)]);
                dpop_id = G_MC_DST; dpop_ms = fm1_ms | 1u;
                if (!perf.mc_amt[(mc_pos >> 1) & 15u] && perf.mc_dst[(mc_pos >> 1) & 15u]) item_set(G_MC_AMT, 32);
            }
        } else if (e >= ENC_KNOB1 && e <= ENC_KNOB4) edit_item(G_MC1 + (e - ENC_KNOB1), d);
        return;
    }
    if (C3_ON && e != ENC_PRESETS) {
        const col3_t *c = &COL3[page];
        if (e == ENC_SELECT) { c3_cur = (uint8_t)clampi(c3_cur + d, 0, 11); return; }
        if (e == ENC_ALGORITHM) { c3_edit(c->id[c3_cur >> 2][c3_cur & 3u], d); return; }
        if (e >= ENC_KNOB1 && e <= ENC_KNOB4) {
            uint32_t col = c3_col(), r = e - ENC_KNOB1;
            c3_cur = (uint8_t)(col * 4u + r);
            c3_edit(c->id[col][r], d);
        }
        return;
    }
    if (e == ENC_PRESETS) {                     /* HOME and PRESETS: the sounds; a SEL window: the windows; the other pages: their views */
        if (win == 6u) return;                  /* (FX PADS: nothing) */
        if (save_view()) return;                /* (SAVE: the sound being saved stays: 1.0.52) */
        if (win && win != 3u) win_step(d);
        else if (page == PG_PLAY || page == PG_PRE) preset_step(d);
        else view_step(d);
        return;
    }
    if (v->lay == LY_PRESET) {
        uint32_t bank = vi - PAGE_V0[PG_PRE], n = bank ? NUSER : NFACTORY;
        if (e == ENC_SELECT || e == ENC_KNOB1) {   /* (with a TAG: from one of those to the next) */
            int32_t at = pre_cur[bank], st = d > 0 ? 1 : -1, c = d > 0 ? d : -d;
            while (c--) { int32_t q = at; do q += st; while (q >= 0 && q < (int32_t)n && !pre_match(bank, (uint32_t)q)); if (q < 0 || q >= (int32_t)n) break; at = q; }
            pre_cur[bank] = (uint8_t)at;
        }
        else if (e == ENC_KNOB2) { item_set(G_BROWSE, d > 0); toast(set.browse ? "BROWSE: FAVORITES" : "BROWSE: ALL"); }
        else if (e == ENC_KNOB4) {              /* TAG: ALL SUB REESE GROWL WOBBLE PLUCK 808 DRONE */
            char t[24], *q;
            uint32_t k;
            pre_tag = (uint8_t)clampi(pre_tag + d, 0, NTAG);
            if (!pre_match(bank, pre_cur[bank])) for (k = 0; k < n; k++) if (pre_match(bank, k)) { pre_cur[bank] = (uint8_t)k; break; }
            q = str_cat(str_cat(t, "TAG "), L_TAG[pre_tag]); q = str_cat(q, " \xB7 "); fmt_u(q, pre_count(bank), 0);
            toast(t);
        }
        else if (e == ENC_KNOB3 || e == ENC_ALGORITHM) ask_load(bank, pre_cur[bank]);
        return;
    }
    if (v->lay == LY_SAVE) {
        if (e == ENC_KNOB1 || e == ENC_SELECT) save_slot = (uint8_t)clampi(save_slot + d, 0, NUSER - 1);
        else if (e == ENC_KNOB2) save_pos = (uint8_t)clampi(save_pos + d, 0, 12);
        else if (e == ENC_KNOB3) {
            static const char CH[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.#";
            int32_t i, n = (int32_t)sizeof CH - 1;
            for (i = 0; i < n && CH[i] != pname[save_pos]; i++) {}
            pname[save_pos] = CH[((i + d) % n + n) % n];
        } else if (e == ENC_ALGORITHM) patch[P_STYLE] = (int8_t)clampi(patch[P_STYLE] + d, 0, NSTYLE - 1);
        else if (e == ENC_KNOB4) new_step(d > 0 ? 2u : 1u);   /* (left: NEW PRESET, right: NEW SONG) */
        return;
    }
    if (e == ENC_SELECT) {
        if (v->n) cursor[vi] = (uint8_t)clampi(cursor[vi] + d, 0, v->n - 1);
        return;
    }
    if (e == ENC_ALGORITHM) {
        if (v->n) edit_item(v->it[cursor[vi]], d);
        return;
    }
    if (e >= ENC_KNOB1 && e <= ENC_KNOB4) {
        uint32_t id = vknob(v, e - ENC_KNOB1), i;
        if (id == I_NONE) return;
        edit_item(id, d);
        for (i = 0; i < v->n; i++) if (v->it[i] == id) cursor[vi] = (uint8_t)i;
    }
}

static int page_of_btn(uint32_t b) { uint32_t i; for (i = 0; i < NPAGE; i++) if (PAGE_BTN[i] == b) return (int)i; return -1; }
static int holdable(uint32_t b) { return page_of_btn(b) >= 0 || b == BTN_SAVE || b == BTN_SEL || b == BTN_PLAY || b == BTN_REC || b <= BTN_OCT_UP; }
static void hold_toggle(void)
{
    uint32_t i;
    hold ^= 1u;
    if (!hold) for (i = 0; i < 128u; i++) if (latched[i]) { latched[i] = 0; ev_push(EV_OFF, i, 0); }
    toast(hold ? "HOLD ON" : "HOLD OFF");
}
static uint32_t hold_time(uint32_t b)           /* how long until the held action (0: none, only combos) */
{
    if (b == BTN_SAVE) return PRE_BROWSING ? 700u : 600u;
    if (b == BTN_SEL) return 400u;
    if (b == BTN_FX) return 600u;
    if (b == BTN_REC && sq_play) return 0u;    /* (playing: REC held records the knobs, or erases) */
    if (b == BTN_HOME || b == BTN_REC) return 800u;
    if (b == BTN_GLO) return 900u;
    if (b == BTN_OCT_UP && win == 7u) return 0u;   /* (PERFORM: OCT+ held is the pads) */
    if (b <= BTN_OCT_UP && dk_view()) return 0u;   /* (DRUMS: OCT- held + a black key mutes; no UNDO there, SAVE held + OCT- still) */
    if (b <= BTN_OCT_UP) return 1000u;      /* (no knob turned: UNDO / REDO; with one it is SHIFT) */
    if (b == BTN_PLAY) return 600u;
    if (b == BTN_ARP) return 0u;
    if (b == BTN_LFO || b == BTN_ENV) return 0u;   /* (they wait for a knob: QUICK ASSIGN) */
    return 1200u;
}
static const char *hold_label(uint32_t b)
{
    static char t[32];
    int pg = page_of_btn(b);
    if (b == BTN_SAVE && PRE_BROWSING) { uint32_t bk = vcur[PG_PRE] ? 1u : 0u;
        return fav_is(bk, pre_cur[bk]) ? "REMOVE FROM FAVORITES" : "ADD TO FAVORITES"; }
    if (b == BTN_SAVE) return "UNDO / REDO";
    if (b == BTN_SEL) return "COMPARE: THE SAVED SOUND";
    if (b == BTN_FX) return win == 6u ? "BACK" : "FX PADS: THE KEYS PLAY EFFECTS";
    if (b == BTN_HOME) return "A + B: THE TWO OSCILLATORS";
    if (b == BTN_REC && sq_play) return "AUTOMATION: TURN A KNOB \xB7 NONE: ERASE";
    if (b == BTN_REC) return "PANIC: ALL NOTES OFF";
    if (b == BTN_GLO) return set.ui ? "BACK TO CLASSIC MODE" : "SWITCH TO SHIFT MODE";
    if (b == BTN_OCT_DOWN) return "UNDO  (OR TURN A KNOB: SHIFT)";
    if (b == BTN_OCT_UP) return "REDO  (OR TURN A KNOB: SHIFT)";
    if (b == BTN_PLAY) return hold ? "HOLD OFF" : "HOLD: THE KEYS STAY ON";
    if (b == BTN_LFO) return set.qlfo ? "KNOB: LFO 2 \xB7 OCT-: 1 \xB7 OCT+: 3" : "KNOB: LFO 1 \xB7 OCT-: 2 \xB7 OCT+: 3";
    if (b == BTN_ENV) return "TURN A KNOB: MOD ENV ON IT";
    str_cat(str_cat(t, "RANDOM "), PAGE_SHORT[pg >= 0 ? pg : 0]);
    return t;
}

/* QUICK ASSIGN (as Pure SUB's drag and drop): an LFO (or the mod envelope) onto what this knob moves, the turn
 * sets how much. LFO held: the LFO of SYSTEM's LFO HELD (LFO 2 at first); with OCT- held too, the other one */
static uint32_t qa_lfo2(void) { return (fm1_in.buttons >> BTN_OCT_DOWN & 1u) ? !set.qlfo : set.qlfo; }
static int qa_lfo3(void) { return (fm1_in.buttons >> BTN_OCT_UP & 1u) != 0u; }   /* (LFO and OCT+ held: LFO 3) */
static void aut_turn(uint32_t e, int32_t d)     /* REC held, a knob: record what it sets on the step playing */
{
    const view_t *v = view();
    uint32_t id, k = PL_AUTO | perf.cur % SQ_NPAT, s = sq_now_step(), r;
    int32_t i, val, lo, hi;
    char b[24], nm[24];
    if (dk_view() && e >= ENC_KNOB1 && e <= ENC_KNOB4) { dk_lock_at(dk_now_step(), e - ENC_KNOB1, d); return; }   /* (DRUMS: the track's locks) */
    id = e == ENC_ALGORITHM || e == ENC_SELECT ? (v->n ? v->it[cursor[vidx()]] : I_NONE) : vknob(v, e - ENC_KNOB1);
    if (win) id = I_NONE;
    else if (C3_ON) id = e == ENC_ALGORITHM || e == ENC_SELECT ? COL3[page].id[c3_cur >> 2][c3_cur & 3u] : COL3[page].id[c3_col()][e - ENC_KNOB1];
    r = id == I_NONE ? I_NONE : rate_like(id);
    if (r == I_NONE || !item_patch(r)) { toast("AUTOMATION: A SOUND SETTING ONLY"); return; }
    item_range(r, &lo, &hi);
    i = pl_find(k, s, r);
    val = clampi((i >= 0 ? pl.l[i].v : patch[r]) + d * (hi - lo > 64 ? 2 : 1), lo, hi);
    if (!pl_set(k, s, r, val)) return;
    aut_hold_base(r);
    patch[r] = (int8_t)val;                     /* (heard at once) */
    prev_dirty = 1;
    lk_text(r, val, b);
    str_cat(str_cat(nm, "AUTO "), item_name(r));
    lk_toast(s, nm, b);
}
static void aut_play(void)                      /* the main loop: a new SEQ BASS step, its automation into the sound; REC held alone erases */
{
    uint32_t q, n, k = perf.cur % SQ_NPAT, st = sq_show;
    if (!sq_play) { if (aut_any) aut_restore(); aut_last = 0xFF; return; }
    if (st == 0xFF || st == aut_last) return;
    aut_last = (uint8_t)st;
    if (aut_erasing) {                          /* (REC held, nothing turned: this step's automation goes) */
        uint32_t before = pl.n;
        pl_drop(PL_AUTO | k, (int32_t)st, -1, -1);
        if (dk_view()) pl_drop(dk.cur % DK_NPAT, (int32_t)(dk_show == 0xFF ? 0u : dk_show), (int32_t)(dk_trk % NDK), -1);
        aut_erased += before - pl.n;
        toast("ERASING AUTOMATION...");
        return;
    }
    n = pl.n <= PL_MAX ? pl.n : 0u;
    for (q = 0; q < n; q++) {
        const plk_t *e = &pl.l[q];
        if (e->pat == (PL_AUTO | k) && e->step == st && e->tp < NP) { aut_hold_base(e->tp); patch[e->tp] = e->v; prev_dirty = 1; }
    }
}
static void qa_dst(uint32_t src, int32_t dst, int32_t d);
static void quick_assign(uint32_t src, uint32_t id, int32_t d) { qa_dst(src, dest_of(id), d); }
static int32_t win_dest(uint32_t e)             /* the windows: what this knob moves, as a destination (-1: none) */
{
    if (win == 3u) return e == ENC_KNOB1 || e == ENC_ALGORITHM ? (int32_t)(D_EQ1F + (eq_cur & 3u)) : e == ENC_KNOB2 ? (int32_t)(D_EQ1G + (eq_cur & 3u)) : -1;
    if (win == 6u) return e == ENC_KNOB1 ? D_CUT : e == ENC_KNOB2 ? D_RES : e == ENC_KNOB4 ? D_MORPH : -1;
    if (win == 4u || win == 5u) return e == ENC_KNOB1 ? D_MORPHX : e == ENC_KNOB2 ? D_MORPHY : -1;
    if (win == 7u) return e == ENC_SELECT ? D_MORPHX : e == ENC_ALGORITHM ? D_MORPHY : -1;
    return -1;
}
static void qa_dst(uint32_t src, int32_t dst, int32_t d)
{
    uint32_t l2 = qa_lfo2(), o = src == BTN_LFO && l2 ? (uint32_t)LFO_2 : 0u;
    uint32_t pd = src == BTN_LFO ? L1_DST + o : ME_DST, pa = src == BTN_LFO ? L1_DEP + o : ME_AMT;
    char t[32], *p;
    if (dst < 0) { toast("NOT A DESTINATION"); return; }
    if (src == BTN_LFO && qa_lfo3()) {         /* LFO 3: kept in the settings */
        if (set.l3[L3_DST] != dst) { set.l3[L3_DST] = (int8_t)dst; set.l3[L3_DEP] = 0; }
        set.l3[L3_DEP] = (int8_t)clampi(set.l3[L3_DEP] + d * 2, -64, 63);
        l3_push(); set_dirty = 1; set_ms = fm1_ms;
        p = str_cat(t, "LFO3 > "); p = str_cat(p, dst_name((uint32_t)dst)); *p++ = ' ';
        lk_text(L1_DEP, set.l3[L3_DEP], p);
        toast(t);
        return;
    }
    if (patch[pd] != dst) { undo_mark(pd); patch[pd] = (int8_t)dst; patch[pa] = 0; }
    undo_mark(pa);
    patch[pa] = (int8_t)clampi(patch[pa] + d * 2, -64, 63);
    if ((dst == D_DRAMT || dst == D_DRMIX) && !patch[DR_ON]) patch[DR_ON] = 1;
    p = str_cat(t, src == BTN_LFO ? (l2 ? "LFO2 > " : "LFO1 > ") : "ENV > ");
    p = str_cat(p, dst_name((uint32_t)dst));
    *p++ = ' ';
    item_text(pa, p);
    toast(t);
}
/* RANDOM: the page's view shaken by about a quarter (lists move one time in five; switches and destinations stay) */
static void randomize(uint32_t pg)
{
    static const uint8_t PLAYR[] = {A_P1, A_P2, A_P3, B_P1, B_P2, B_P3, F_CUT, F_RES, X_AMT, L1_DIV, L1_SHP, L1_DEP};
    const view_t *v = &VIEWS[PAGE_V0[pg] + vcur[pg]];
    const uint8_t *it = pg == PG_PLAY ? PLAYR : v->it;
    uint32_t n = pg == PG_PLAY ? sizeof PLAYR : v->n, i;
    undo_push();
    undo_id = 0xFFFFu;
    for (i = 0; i < n; i++) {
        uint32_t id = it[i];
        int32_t lo, hi, r;
        if (id >= NP || PD[id].fmt == FM_DEST || PD[id].list == L_ONOFF || id == V_MODE || id == P_STYLE) continue;
        lo = PD[id].lo; hi = PD[id].hi;
        if (PD[id].fmt == FM_LIST || PD[id].fmt == FM_DIV) {
            if (ui_rnd() % 5u == 0u) patch[id] = (int8_t)(lo + (int32_t)(ui_rnd() % (uint32_t)(hi - lo + 1)));
            continue;
        }
        r = (hi - lo) / 4 + 1;
        patch[id] = (int8_t)clampi(patch[id] + (int32_t)(ui_rnd() % (uint32_t)(2 * r + 1)) - r, lo, hi);
    }
    pdirty = 1;
    prev_dirty = 1;
    toast("RANDOM \xB7 SAVE HELD = UNDO");
}
static void compare(int on)
{
    if (on) {
        int8_t p[NP_MAX];
        char nm[14];
        if (!pdirty) { toast("UNCHANGED"); return; }
        memcpy(cmp_buf, patch, NP_MAX);
        if (set.bank) user_get(set.idx, p);
        else preset_factory(set.idx, p, nm);
        memcpy(patch, p, NP_MAX);
        cmp_on = 1;
    } else if (cmp_on) {
        memcpy(patch, cmp_buf, NP_MAX);
        cmp_on = 0;
    }
    prev_dirty = 1;
}
static void favorite(void)                       /* the preset under the PRESETS cursor */
{
    uint32_t bk = vcur[PG_PRE] ? 1u : 0u, k = pre_cur[bk], i = fav_i(bk, k);
    if (i >= 160u || (bk && !up_valid(k))) { toast("EMPTY SLOT"); return; }
    *fav_b(i) ^= (uint8_t)(1u << (i & 7u));
    set_dirty = 1;
    set_ms = fm1_ms;
    toast(fav_is(bk, k) ? "* FAVORITE" : "NOT A FAVORITE");
}

static void ask_theme_done(uint32_t sel)       /* the very first start: the look chosen */
{
    static const char *const N[4] = {"CLASSIC", "STUDIO", "PURE", "RIBBON CLASSIC"};
    ask_on = 0;
    set.theme = ASK_TH[sel & 3u];
    theme_set(set.theme, set.pal); font_set(set.font, set.zoom);
    set_dirty = 1; set_ms = fm1_ms;
    toast(N[sel & 3u]);
    g_force = 1;
}
static void ask_done(uint32_t sel)
{
    if (theme_ask) {                            /* (the very first start: on to the look) */
        theme_ask = 0;
        if (set.ui != sel || first_run || set.askv < 2u) { set.ui = (uint8_t)sel; if (set.askv < 10u) set.askv = 10; set_dirty = 1; set_ms = fm1_ms; first_run = 0; }
        ask_on = 2; ask_sel = 0; ask_ms = fm1_ms;
        theme_set(ASK_TH[0], set.pal);          /* (each one shown as it is chosen) */
        g_force = 1;
        return;
    }
    ask_on = 0;
    splash_until = 0;
    if (set.ui != sel || first_run || set.askv < 2u) { set.ui = (uint8_t)sel; if (set.askv < 10u) set.askv = 10; set_dirty = 1; set_ms = fm1_ms; first_run = 0; }
    toast(sel ? "SHIFT MODE" : "CLASSIC MODE");
    g_force = 1;
}
/* DIM: 2 minutes untouched, the screen goes to its lowest brightness; the next touch wakes it (a button or a knob only
 * wakes it, a key wakes it and plays) */
static uint32_t idle_ms;
static uint8_t dimmed;
static void dim_set(int on)
{
    if (dimmed == (uint8_t)on) return;
    dimmed = (uint8_t)on;
    g_bri = on ? 0u : set.bri;
    theme_set(set.theme, set.pal);
    g_force = 1;
}
static void ui_input(void)
{
    uint32_t rel, pr = fm1_input_edges(&rel), b, k, now = fm1_in.notes, ne = fm1_input_note_edges();
    int32_t d;
    if (splash_until) {                         /* the start screen stays until a button, a key or a knob is touched (that touch does nothing else) */
        int any = pr || ne;
        for (b = 0; b < 7u; b++) if (fm1_enc_take(b)) any = 1;
        if (any) { splash_until = 0; idle_ms = fm1_ms; dim_set(0); g_force = 1; }
        ask_ms = fm1_ms;
        prev_keys = now;                        /* (a key held now does not play when the screen goes) */
        return;
    } else if (ask_on == 2u) {                  /* the very first start: the look, OCT- / OCT+ or a knob go through four, any other button says yes */
        int32_t mv = 0;
        for (b = 0; b < 7u; b++) if ((d = fm1_enc_take(b)) != 0) mv += d > 0 ? 1 : -1;
        if (pr & (1u << BTN_OCT_DOWN)) mv--;
        if (pr & (1u << BTN_OCT_UP)) mv++;
        if (mv) { ask_sel = (uint8_t)((ask_sel + 4 + (mv > 0 ? 1 : 3)) % 4u); theme_set(ASK_TH[ask_sel], set.pal); ask_ms = fm1_ms; g_force = 1; }
        else if (pr & ~3u) ask_theme_done(ask_sel);
        else if (fm1_ms - ask_ms > 30000u) ask_theme_done(ask_sel);
        pr = 0;
    } else if (ask_on) {                        /* the start's question: OCT- CLASSIC, OCT+ SHIFT, a knob turns, SEL says yes */
        for (b = 0; b < 7u; b++) if ((d = fm1_enc_take(b)) != 0) ask_sel = (uint8_t)(d > 0);
        if (pr & (1u << BTN_OCT_DOWN)) ask_done(0);
        else if (pr & (1u << BTN_OCT_UP)) ask_done(1);
        else if (pr & ~3u) ask_done(ask_sel);
        else if (fm1_ms - ask_ms > 20000u) ask_done(ask_sel);   /* (no answer: the one shown) */
        pr = 0;
    }
    if (pr || ne) { idle_ms = fm1_ms; if (dimmed) { dim_set(0); pr = 0; } }   /* (the touch that wakes the screen does nothing else) */
    for (b = 0; b < 14u; b++) {
        if (!(pr & (1u << b))) continue;
        splash_until = 0;
        if (hb.b == BTN_SAVE && !PRE_BROWSING && b <= BTN_OCT_UP) {   /* SAVE held: OCT- UNDO, OCT+ REDO */
            ur_on = 1; hb.fired = 1;
            if (b == BTN_OCT_DOWN) undo(); else redo();
            continue;
        }
        if (hb.b == BTN_LFO && b == BTN_OCT_DOWN) {   /* LFO held, then OCT-: the knobs put the other LFO on (no octave) */
            toast(qa_lfo2() ? "LFO 2 ON THE KNOBS" : "LFO 1 ON THE KNOBS");
            continue;
        }
        if (b <= BTN_OCT_UP && (fm1_in.buttons & 3u) == 3u) {   /* OCT- + OCT+: PANIC, in CLASSIC too (1.0.36) */
            panic_all();
            if (hb.b <= BTN_OCT_UP) hb.used = 1;
            continue;
        }
        if (holdable(b) && hb.b == 0xFF && !pend_on) { hb.b = (uint8_t)b; hb.used = hb.fired = 0; hb.at = fm1_ms; hb.next = hold_time(b); }
        else if (!holdable(b) || pend_on) on_button(b);
    }
    if (hb.b != 0xFF) {                         /* a button held: its action when the bar is full */
        uint32_t held = (fm1_in.buttons >> hb.b) & 1u;
        if (held && !hb.used && hb.next && fm1_ms - hb.at >= hb.next) {
            if (hb.b == BTN_SAVE && PRE_BROWSING) { favorite(); hb.next = 0; }
            else if (hb.b == BTN_SAVE) { ur_on = 1; hb.next = 0; }   /* (then OCT- UNDO, OCT+ REDO) */
            else if (hb.b == BTN_SEL) { compare(1); hb.next = 0; }
            else if (hb.b == BTN_PLAY) { hold_toggle(); hb.next = 0; }
            else if (hb.b == BTN_FX) { win = win == 6u ? 0u : 6u; vpop_ms = 0; g_force = 1; hb.next = 0; }
            else if (hb.b == BTN_HOME) {        /* HOME held: COMBINE A + B, the two oscillators' main settings */
                uint32_t j;
                for (j = 0; j < sizeof VIEWS / sizeof VIEWS[0]; j++) if (str_eq(VIEWS[j].cat, "COMBINE A + B")) break;
                for (page = 0; page + 1u < NPAGE && PAGE_V0[page + 1u] <= j; page++) {}
                vcur[page] = (uint8_t)(j - PAGE_V0[page]); win = 0; vpop_ms = 0; g_force = 1; hb.next = 0;
            }
            else if (hb.b == BTN_REC) { panic_all(); hb.next = 0; }
            else if (hb.b == BTN_GLO) { item_set(G_UI, set.ui ? 0 : 1); set_dirty = 1; set_ms = fm1_ms; toast(set.ui ? "SHIFT MODE" : "CLASSIC MODE"); hb.next = 0; }
            else if (hb.b <= BTN_OCT_UP) { if (hb.b == BTN_OCT_DOWN) undo(); else redo(); hb.next = 0; }
            else { randomize((uint32_t)page_of_btn(hb.b)); hb.next = 0; }
            hb.fired = 1;
        }
        if (hb.b == BTN_REC && held && sq_play && !hb.used && !aut_erasing && fm1_ms - hb.at >= 400u) { aut_erasing = 1; aut_erased = 0; aut_last = 0xFF; toast("ERASING AUTOMATION..."); }   /* (REC held alone: erase as it passes) */
        if ((rel >> hb.b) & 1u || !held) {
            uint32_t bb = hb.b;
            hb.b = 0xFF;
            if (aut_erasing) {                  /* (let go: how much went) */
                char t[32], *p;
                aut_erasing = 0; hb.used = 1;
                if (aut_erased) { p = str_cat(t, "AUTOMATION ERASED \xB7 "); fmt_u(p, aut_erased, 0); toast(t); }
                else toast("NO AUTOMATION THERE");
            }
            if (bb == BTN_SAVE) ur_on = 0;
            if (cmp_on) compare(0);
            else if (!hb.used && !hb.fired && (bb > BTN_OCT_UP || fm1_ms - hb.at < 500u)) on_button(bb);   /* (OCT held long: no octave) */
        }
    }
    for (b = 0; b < 7u; b++) if ((d = fm1_enc_take(b)) != 0) {
        idle_ms = fm1_ms;
        if (dimmed) { dim_set(0); continue; }
        if (cmp_on) continue;                   /* (comparing: nothing changes) */
        if (hb.b <= BTN_OCT_UP && b >= ENC_KNOB1 && b <= ENC_KNOB4) hb.used = 1;   /* (SHIFT: no octave on release) */
        if (hb.b == BTN_REC && sq_play && b != ENC_PRESETS) { hb.used = 1; aut_erasing = 0; aut_turn(b, d); continue; }   /* AUTOMATION: recorded */
        if (hb.b != 0xFF && (hb.b == BTN_LFO || hb.b == BTN_ENV || ((hb.b == BTN_OCT_DOWN || hb.b == BTN_OCT_UP) && (fm1_in.buttons >> BTN_LFO & 1u))) && ((b >= ENC_KNOB1 && b <= ENC_KNOB4) || b == ENC_ALGORITHM)) {   /* (OCT- then LFO held: LFO 1 too) */
            const view_t *v = view();
            uint32_t id = b == ENC_ALGORITHM ? (v->n ? v->it[cursor[vidx()]] : I_NONE) : vknob(v, b - ENC_KNOB1);
            hb.used = 1;
            if (win) { qa_dst(hb.b == BTN_ENV ? BTN_ENV : BTN_LFO, win_dest(b), d); continue; }   /* (the EQ, FX PADS, MORPH: their knobs too) */
            if (C3_ON) id = b == ENC_ALGORITHM ? COL3[page].id[c3_cur >> 2][c3_cur & 3u] : COL3[page].id[0][b - ENC_KNOB1];
            if (id != I_NONE) quick_assign(hb.b == BTN_ENV ? BTN_ENV : BTN_LFO, rate_like(id), d);
            else toast("NOT A DESTINATION");
            continue;
        }
        if (hb.b != 0xFF && page_of_btn(hb.b) >= 0 && b >= ENC_KNOB1 && b <= ENC_KNOB4) {   /* SHIFT: that page's knobs */
            uint32_t pg = (uint32_t)page_of_btn(hb.b), id = vknob(&VIEWS[PAGE_V0[pg] + vcur[pg]], b - ENC_KNOB1);
            hb.used = 1;
            if (id == I_NONE || VIEWS[PAGE_V0[pg] + vcur[pg]].lay >= LY_PRESET) continue;
            str_cat(str_cat(vpop_pre, PAGE_SHORT[pg]), " \xB7 ");
            edit_item(id, d);
            continue;
        }
        vpop_pre[0] = 0;
        on_enc(b, d);
    }
    for (k = 0; k < 27u; k++) {
        uint32_t m = 1u << k;
        if (save_view()) {                      /* SAVE: the keys type the name */
            if ((ne & m) || ((now & m) && !(prev_keys & m))) save_key(k);
            else if (!(now & m) && (prev_keys & m) && phys[key_note[k]]) key_up(key_note[k]);
            continue;
        }
        if (dk_view()) {                        /* DRUMS: the keys program the drums */
            if ((ne & m) || ((now & m) && !(prev_keys & m))) dk_key(k);
            else if (!(now & m) && (prev_keys & m)) {
                dk_key_up(k);
                if (phys[key_note[k]]) key_up(key_note[k]);   /* (a note held from another view: let go) */
            }
            continue;
        }
        if (hb.b == BTN_LFO && (fm1_in.buttons >> BTN_LFO & 1u) && !save_view()) {   /* WOBBLE TAP: LFO held, the first 8 white keys */
            if ((ne & m) || ((now & m) && !(prev_keys & m))) {
                uint32_t w;
                for (w = 0; w < 8u && PK_WHITE[w] != k; w++) {}
                if (w < 8u) { wob_tap(w); hb.used = 1; }
            }
            continue;
        }
        if (win == 6u || (win == 7u && (fm1_in.buttons >> BTN_OCT_UP & 1u))) {   /* FX PADS (PERFORM: OCT+ held): a key is a pad */
            if (hb.b == BTN_OCT_UP) hb.used = 1;
            if ((ne & m) || ((now & m) && !(prev_keys & m))) { if (win == 6u) pad_quant(k, 1); else pad_key(k, 1); }
            else if (!(now & m) && (prev_keys & m)) { if (win == 6u) pad_quant(k, 0); else pad_key(k, 0); }
            continue;
        }
        if ((ne & m) || ((now & m) && !(prev_keys & m))) {
            int32_t n = clampi(53 + (int32_t)k + 12 * OCT, 0, 127);
            if (prev_keys & m) key_up(key_note[k]);
            key_note[k] = (uint8_t)n;
            key_down((uint32_t)n, 100);
        } else if (!(now & m) && (prev_keys & m))
            key_up(key_note[k]);
    }
    prev_keys = now;
    kb_down = now;
}

/* ---- MIDI in: notes, CC (learnt ones first), bend, pressure, the clock, start / stop, program change */
static uint32_t clk_last, clk_per, clk_n;
static int midi_learnt(uint32_t cc, uint32_t v)  /* 1: a CC learnt (it moves its setting) */
{
    uint32_t k;
    if (perf.cc_magic != CC_MAGIC) return 0;
    if (ml_state == 2u) {                       /* (learning: this CC goes to the setting turned) */
        char t[24], *p;
        for (k = 0; k < 16u && perf.cc_num[k] != cc && perf.cc_num[k] != 0xFFu; k++) {}
        if (k == 16u) k = 15u;
        perf.cc_num[k] = (uint8_t)cc; perf.cc_id[k] = ml_id;
        ml_state = 0;
        perf_touch();
        p = str_cat(t, "CC "); p = fmt_u(p, cc, 0); p = str_cat(p, " > "); str_cat(p, item_name(ml_id));
        toast(t);
        return 1;
    }
    for (k = 0; k < 16u; k++)
        if (perf.cc_num[k] == cc) {
            int32_t lo, hi;
            item_range(perf.cc_id[k], &lo, &hi);
            item_set(perf.cc_id[k], lo + (int32_t)v * (hi - lo) / 127);
            return 1;
        }
    return 0;
}
static void midi_in(void)
{
    uint8_t st, d1, d2;
    while (midi_get(&st, &d1, &d2)) {
        uint32_t ty = st & 0xF0u, ch = (st & 15u) + 1u;
        if (st >= 0xF0u) { if (st != 0xF8) { mm_rt = st; mm_rn = 1; } }
        else { mm_st = st; mm_d1 = d1; mm_d2 = d2; mm_n = 1; }
        if (st == 0xF8) {                       /* the clock: its tempo, and the beat it is on */
            uint32_t t = fm1_ticks();
            if (set.clock && clk_last) {
                uint32_t p = t - clk_last;
                clk_per = clk_per ? clk_per + ((int32_t)(p - clk_per) >> 3) : p;
                if (clk_per) { uint32_t b = 600000000u / clk_per; if (b >= 300u && b <= 3000u) ks_bpm10 = b; }
                if (++clk_n % 24u == 0u) ev_push(EV_SYNC, 1, clk_n % 1152u);
            }
            clk_last = t;
            continue;
        }
        if (st == 0xFA) { clk_n = 0; ev_push(EV_SYNC, 0, 0); if (set.mstart) seq_play(1); continue; }
        if (st == 0xFB) { if (set.mstart) ev_push(EV_SEQ, 1, 0); continue; }   /* (continue: from where it is) */
        if (st == 0xFC) { if (set.mstart) seq_play(0); continue; }
        if (st >= 0xF0) continue;
        if (set.midich && ch != set.midich) continue;
        if (ty == 0x90 && d2) ev_push(EV_ON, d1, d2);
        else if (ty == 0x80 || ty == 0x90) ev_push(EV_OFF, d1, 0);
        else if (ty == 0xE0) ks_bend = (((int32_t)d2 << 7 | d1) - 8192) * 4;
        else if (ty == 0xD0) ks_press = d1 * 258;
        else if (ty == 0xC0) {                  /* program change: bank A, then the user's B */
            if (!set.mprog) continue;
            if (d1 < NFACTORY) { load_preset(0, d1); toast(pname); }
            else if (up_valid(d1 - NFACTORY)) { load_preset(1, d1 - NFACTORY); toast(pname); }
        }
        else if (ty == 0xB0) {
            if (midi_learnt(d1, d2)) continue;
            switch (d1) {
            case 1: ks_wheel = d2 * 258; break;
            case 74: patch[F_CUT] = (int8_t)d2; pdirty = 1; break;
            case 71: patch[F_RES] = (int8_t)d2; pdirty = 1; break;
            case 76: patch[L1_DIV] = (int8_t)(set.grid ? DIV_NEAR[d2 >> 3] : d2 >> 3); break;
            case 77: patch[L1_DEP] = (int8_t)(d2 - 64); break;
            case 123: panic_all(); break;
            }
        }
    }
}

static void midi_clock_out(void)                /* CLOCK OUT (internal clock): 24 a beat, start and stop with the SEQ */
{
    static uint32_t last = 0xFFFFFFFFu;
    static uint8_t was;
    uint32_t now = (uint32_t)(((uint64_t)ks_beats * 24u) >> 24), n;
    if (!set.mclk || set.clock) { last = 0xFFFFFFFFu; return; }
    if (sq_play != was) { was = sq_play; midi_send(sq_play ? 0xFAu : 0xFCu, 0, 0); }
    if (last == 0xFFFFFFFFu) last = now;
    n = (now + 1152u - last) % 1152u;           /* (ks_beats: 48 beats round) */
    if (n > 8u) n = 1u;                         /* (the main loop was away: no burst) */
    while (n--) midi_send(0xF8u, 0, 0);
    last = now;
}
static void app_poll(void) { midi_in(); midi_clock_out(); }
static void link_service(void);                 /* (ksub_link.c: the backup with the web page) */

/* ================================================================ the main loop */
static uint32_t ui_frame_ms, prev_ms;
static uint32_t asave_ms;
static void autosave_tick(void)                 /* AUTOSAVE: once a minute, when the sound changed and nothing sounds (erasing stalls the sound) */
{
    if (!set.asave || ks_sounding || fm1_ms - asave_ms < 60000u) return;
    asave_ms = fm1_ms;
    if (work.magic == WK_MAGIC && work.bank == set.bank && work.idx == set.idx && !memcmp(work.p, patch, NP_MAX)) return;
    work.magic = WK_MAGIC; work.bank = set.bank; work.idx = set.idx;
    memcpy(work.name, pname, sizeof work.name);
    memcpy(work.p, patch, NP_MAX);
    work.sum = wk_sum();
    if (perf_save()) toast("AUTOSAVE FAILED");
    else perf_dirty = 0;
}
static void store_fixup(void)                   /* the store as read (or restored from a backup): the old layouts brought up, the broken parts reset */
{
    uint32_t i;
    ux_migrate();
    for (i = 0; i < NUSER; i++) if (!up_valid(i)) ustore[i].magic = 0;
    if (perf.magic != PF_MAGIC || perf.sum != perf_sum()) perf_defaults();
    if (sqx.magic != SX_MAGIC || sqx.sum != sqx_sum()) { sqx_defaults(); perf_touch(); }
    if (dk.magic != DK_MAGIC || dk.sum != dk_sum()) { dk_defaults(); perf_touch(); }
    if (pl.magic != PL_MAGIC || pl.sum != pl_sum() || pl.n > PL_MAX) { pl_defaults(); perf_touch(); }
    perf.ar_on = 0;                             /* (the ARP starts off: a key does what it says) */
    if (perf.mc_magic != MC_MAGIC) mc_defaults();
    if (perf.cc_magic == 0x43u) {               /* (learnt before 0.9.55: the ids past the patch moved up 4, for OSC C's START, END, DIRECTION) */
        for (i = 0; i < 16u; i++) if (perf.cc_id[i] >= 136u && perf.cc_id[i] < I_NONE - 4u) perf.cc_id[i] = (uint8_t)(perf.cc_id[i] + 4u);
        perf.cc_magic = CC_MAGIC;
        perf_touch();
    }
    if (perf.cc_magic != CC_MAGIC) { memset(perf.cc_num, 0xFF, sizeof perf.cc_num); perf.cc_magic = CC_MAGIC; }
    if (perf.eq_magic != 0x45u) eq_defaults();
    if (perf.mx_magic != MX_MAGIC) {
        if (perf.mx_magic != 0x59u) for (i = 0; i < 16u; i++) if (perf.cc_id[i] >= I_L1RATE) perf.cc_num[i] = 0xFF;
        mx_defaults();
        perf_touch();
    }
    eq_dirty = 1;
}
static void app_init(void)
{
    uint32_t i;
    int ok;
    {                                           /* (a save from before a field was added: the fields it has, the rest as is) */
        const uint32_t LEN[17] = {sizeof set, (uint32_t)(((uint8_t *)set.fav3 - (uint8_t *)&set + 1u) & ~1u), (uint32_t)((uint8_t *)set.fav2 - (uint8_t *)&set), (uint32_t)((uint8_t *)set.l3 - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.sync - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.lang - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.pquant - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.asave - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.eng8 - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.qlfo - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.bri - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.norm - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.contrast - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.mstart - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.zoom - (uint8_t *)&set),
                                 (uint32_t)((uint8_t *)&set.ui - (uint8_t *)&set), (uint32_t)((uint8_t *)&set.accel - (uint8_t *)&set)};
        for (ok = 0, i = 0; i < 17u && !ok; i++) ok = !settings_load(&set, LEN[i]) && set.ver == 6u;
        theme_ask = !ok;                        /* (nothing saved yet: the very first start) */
        l3_push();
    }
    first_run = !ok || set.askv < 2u;           /* (the very first start, or a save from before 0.9.17: the question once) */
    if (!ok) {
        set.grid = set.browse = 0; set.font = 6; set.theme = 0; set.pal = 0;   /* (CLASSIC GLOW, FLUO GREEN, TEKTUR) */
        memset(set.fav, 0, sizeof set.fav); memset(set.fav2, 0, sizeof set.fav2); memset(set.fav3, 0, sizeof set.fav3);
        set.ver = 6; set.midich = 0; set.clock = 0; set.oct = 0xFF; set.bpm10 = 1400; set.bank = 0; set.idx = 0; set.cores = 1; set.accel = 2; set.c3col = 0; set.ui = 0; set.ask = 0; set.zoom = 1; set.mstart = 1; set.mprog = 1; set.mclk = 0; set.contrast = 1; set.norm = 1; set.deep = 1; set.bri = 3; set.qlfo = 1; set.eng8 = 0; set.asave = 1; set.lang = 0; set.askv = 7;
    }
    set.eng8 = set.eng8 ? 1u : 0u;
    set.qlfo = set.qlfo ? 1u : 0u;
    set.bri = set.bri < 4u ? set.bri : 3u;
    g_bri = set.bri;
    if (!set.askv) { set.ask = 0; set.askv = 1; set_dirty = 1; set_ms = fm1_ms; }
    if (set.askv == 3u) { set.qlfo = 1; set.askv = 4; set_dirty = 1; set_ms = fm1_ms; }   /* (0.9.44: LFO held puts LFO 2 on, once again) */
    if (set.askv == 2u) {                       /* (a save from before 0.9.31: LOUPE on, once; the RAD palettes two further, after SKY and BLAZE) */
        set.zoom = 1; set.askv = 4; set.qlfo = 1; set_dirty = 1; set_ms = fm1_ms;
        if (set.pal >= 11u && set.pal < 22u) set.pal = (uint8_t)(set.pal + 2u);
    }   /* (0.10: no more question at each start; SYSTEM > ASK brings it back) */
    if (set.askv == 4u) { set.askv = 5; set_dirty = 1; set_ms = fm1_ms; }   /* (0.9.56's OUTFIT step: folded into the next) */
    if (set.askv == 5u) { set.theme = 0; set.pal = 0; set.font = 6; set.zoom = 1; set.accel = 2; set.askv = 7; set_dirty = 1; set_ms = fm1_ms; }   /* (1.0.3: CLASSIC GLOW, FLUO GREEN, TEKTUR, LOUPE, once) */
    if (set.askv == 6u) { set.accel = 2; set.askv = 7; set_dirty = 1; set_ms = fm1_ms; }   /* (1.0.23: KNOB ACCEL FAST, once; new installs too) */
    set.norm = set.norm ? 1u : 0u; set.deep = set.deep < 3u ? set.deep : 1u;
    ks_norm = set.norm; ks_deep = set.deep;
    set.sync = set.sync ? 1u : 0u; ks_sync = set.sync;
    set.contrast = 2;                           /* no CONTRAST setting any more: always MAX (DEEP BLACK), the one look that shows right on the FM-1 */
    g_con = 2; lcd_contrast(2);
    set.cores = set.cores ? 1u : 0u;
    theme_set(set.theme, set.pal);
    font_set(set.font, set.zoom);
    set.grid = set.grid ? 1u : 0u;
    ks_straight = set.grid;
    set.bpm10 = (uint16_t)clampi(set.bpm10, 600, 2400);
    set.oct = 0xFF;                             /* (every session starts at octave -1; OCT- / OCT+ move it as before) */
    ks_bpm10 = set.bpm10;
    store_load();
    store_fixup();
    if (set.askv == 7u) {                       /* (1.0.24: 4 factory presets more, the CS ones: the user slots' favourites and MORPH codes follow, once) */
        int32_t i;
        for (i = 95; i >= 64; i--) {
            int32_t f = i - 4, on = f >= 60 && (set.fav[f >> 3] >> (f & 7) & 1u);
            set.fav[i >> 3] = (uint8_t)((set.fav[i >> 3] & ~(1u << (i & 7))) | (on ? 1u << (i & 7) : 0u));
        }
        for (i = 60; i < 64; i++) set.fav[i >> 3] &= (uint8_t)~(1u << (i & 7));   /* (the new ones: not favourites) */
        for (i = 0; i < 4; i++) if (perf.mx_pre[i] >= MX_VAR + 60u && perf.mx_pre[i] + 4u < MX_VAR + 96u) perf.mx_pre[i] = (uint8_t)(perf.mx_pre[i] + 4u);
        if (perf.ab_pre >= MX_VAR + 60u && perf.ab_pre + 4u < MX_VAR + 96u) perf.ab_pre = (uint8_t)(perf.ab_pre + 4u);
        perf_touch();
        set.askv = 8; set_dirty = 1; set_ms = fm1_ms;
    }
    if (set.askv == 8u) {                       /* (1.0.31: 16 factory presets more, after the 64: the user slots' favourites and MORPH codes move 16 on, once) */
        int32_t i;
        for (i = 127; i >= 64; i--) {
            int32_t f = i - 16, on = f >= 64 && (*fav_b((uint32_t)f) >> (f & 7) & 1u);
            uint8_t *b = fav_b((uint32_t)i);
            *b = (uint8_t)((*b & ~(1u << (i & 7))) | (on ? 1u << (i & 7) : 0u));
        }
        for (i = 0; i < 4; i++) if (perf.mx_pre[i] >= MX_VAR + 64u && perf.mx_pre[i] + 16u < MX_NCODE) perf.mx_pre[i] = (uint8_t)(perf.mx_pre[i] + 16u);
        if (perf.ab_pre >= MX_VAR + 64u && perf.ab_pre + 16u < MX_NCODE) perf.ab_pre = (uint8_t)(perf.ab_pre + 16u);
        perf_touch();
        set.askv = 9; set_dirty = 1; set_ms = fm1_ms;
    }
    if (set.askv == 9u) {                       /* (1.0.32: 16 more again, 96 factory presets: the same move, from 80) */
        int32_t i;
        for (i = 127; i >= 80; i--) {
            int32_t f = i - 16, on = f >= 80 && (*fav_b((uint32_t)f) >> (f & 7) & 1u);
            uint8_t *b = fav_b((uint32_t)i);
            *b = (uint8_t)((*b & ~(1u << (i & 7))) | (on ? 1u << (i & 7) : 0u));
        }
        for (i = 0; i < 4; i++) if (perf.mx_pre[i] >= MX_VAR + 80u && perf.mx_pre[i] + 16u < MX_NCODE) perf.mx_pre[i] = (uint8_t)(perf.mx_pre[i] + 16u);
        if (perf.ab_pre >= MX_VAR + 80u && perf.ab_pre + 16u < MX_NCODE) perf.ab_pre = (uint8_t)(perf.ab_pre + 16u);
        perf_touch();
        set.askv = 10; set_dirty = 1; set_ms = fm1_ms;
    }
    if (set.askv == 10u) {                      /* (1.0.49: the sound at start is HOOVER DNB, once, where it was the first preset) */
        uint32_t k;
        if (!set.bank && !set.idx) for (k = 0; k < NFACTORY; k++) if (str_eq(FACTORY[k].name, "HOOVER DNB")) { set.idx = (uint8_t)k; break; }
        set.askv = 11; set_dirty = 1; set_ms = fm1_ms;
    }
    if (set.askv == 11u) {                      /* (1.0.50: a 97th factory preset: the user slots' favourites and MORPH codes move 1 on, once) */
        int32_t i;
        for (i = 128; i >= 96; i--) {
            int32_t f = i - 1, on = f >= 96 && (*fav_b((uint32_t)f) >> (f & 7) & 1u);
            uint8_t *b = fav_b((uint32_t)i);
            *b = (uint8_t)((*b & ~(1u << (i & 7))) | (on ? 1u << (i & 7) : 0u));
        }
        for (i = 0; i < 4; i++) if (perf.mx_pre[i] >= MX_VAR + 96u && perf.mx_pre[i] + 1u < MX_NCODE) perf.mx_pre[i] = (uint8_t)(perf.mx_pre[i] + 1u);
        if (perf.ab_pre >= MX_VAR + 96u && perf.ab_pre + 1u < MX_NCODE) perf.ab_pre = (uint8_t)(perf.ab_pre + 1u);
        perf_touch();
        set.askv = 12; set_dirty = 1; set_ms = fm1_ms;
    }
    load_preset(set.bank, set.idx);
    if (set.asave && work.magic == WK_MAGIC && work.sum == wk_sum() && work.bank == set.bank && work.idx == set.idx) {   /* AUTOSAVE: the sound as it was left */
        int8_t p[NP_MAX];
        memcpy(p, work.p, NP_MAX);
        patch_sanitize(p);
        if (memcmp(p, patch, NP_MAX)) { memcpy(patch, p, NP_MAX); pdirty = 1; prev_dirty = 1; }
    }
    asave_ms = fm1_ms;
    if (set.bank && !up_valid(set.idx)) load_preset(0, 0);
    pre_cur[set.bank] = set.idx;
    set_dirty = 0;
    dual_on = 0;
    rst_read();
#if KS_DUAL
    if (set.cores) {                            /* the voices on core 1: unless the last try crashed */
        if (dual_mark[0] == DUAL_MAGIC && dual_mark[1] == ~DUAL_MAGIC && rst_bad) {
            set.cores = 0;                      /* (a crash or the watchdog within 10 s: one core from now, SYSTEM to try again) */
            set_dirty = 1;
            set_ms = fm1_ms;
            dual_why = DW_SAFE;
            toast("ONE CORE (SAFE)");
        } else {
            dual_mark[0] = DUAL_MAGIC;
            dual_mark[1] = ~DUAL_MAGIC;
            dual_ms = fm1_ms;
            dual_on = fm1_cpu1_start(core1_main) == 0;
            if (!dual_on) { dual_mark[0] = dual_mark[1] = 0; dual_why = DW_SILENT; toast("CORE 1 SILENT: ONE CORE"); }
        }
    }
    if (!set.cores) dual_mark[0] = dual_mark[1] = 0;
#endif
    booted = 1;
    splash_until = 1;                           /* the start screen: the picture, the last patch, the cores; until a touch */
    if (set.ask || first_run) { ask_on = 1; ask_sel = set.ui; ask_ms = fm1_ms; }   /* ... and the question (the first start only, or ASK on): CLASSIC or SHIFT */
    g_force = 1;
}

static void app_step(void)
{
    ui_input();
    if (eq_dirty) eq_update();
    else {                                      /* an LFO on the EQ: the coefficients again (every 2 ms), and once more when it stops */
        static uint8_t eq_moved;
        static uint32_t eq_ms;
        uint32_t k, any = 0;
        for (k = 0; k < 8u; k++) any |= gmod[D_EQ1F + k] != 0;
        if ((any || eq_moved) && fm1_ms - eq_ms >= 2u && eq_coefs()) { eq_ms = fm1_ms; eq_moved = (uint8_t)any; }
    }
    mx_tick();
    if (rs_rec_on && !rs_want) {                /* RESAMPLE done: OSC C plays it */
        rs_rec_on = 0;
        undo_mark(0xFFF3u);
        patch[SM_SEL] = SM_RS;
        if (!patch[SM_LVL]) patch[SM_LVL] = 90;
        toast("RESAMPLED \xB7 OSC C PLAYS IT");
    }
    {                                           /* the EQ view of the FX page is the EQ window; FX PADS let go of its keys on leaving */
        uint32_t on = page == PG_FX && vidx() == V_EQ && (!win || win == 3u) && !ask_on;
        if (on && !win) { win = 3; g_force = 1; }
        else if (!on && win == 3u) { win = 0; g_force = 1; }
        if (win != 6u && !(win == 7u && (fm1_in.buttons >> BTN_OCT_UP & 1u)) && (pad_mask || pad_ov || pad_hot)) pad_release_all();
    }
    if (toast_ms && (int32_t)(fm1_ms - toast_ms) > 1400) toast_ms = 0;
    if (pend_on && fm1_ms - pend_ms > 6000u) { pend_on = 0; toast("KEPT"); }
    if (dpop_ms && (int32_t)(fm1_ms - dpop_ms) > 1500) dpop_ms = 0;
    if (vpop_ms && (int32_t)(fm1_ms - vpop_ms) > 1000) vpop_ms = 0;   /* (toast_ms can be 1 ms ahead: | 1) */
    if (prev_dirty && fm1_ms - prev_ms > 60u) { prev_dirty = 0; prev_ms = fm1_ms; preview_make(); }
    if (fm1_ms - ui_frame_ms >= 33u) {          /* ~30 frames a second, only what changed is sent */
        ui_frame_ms = fm1_ms;
        ui_leds();
        g_frame_send(draw_all);
    }
    if (sq_rec && !sq_play && vidx() != V_SEQ && vidx() != V_DRUM) { sq_rec = 0; rec_hold.on = 0; }
    aut_play();
    if (dk_hold && !dk_view()) dk_hold = 0;   /* (playing, REC goes on in every view: the knobs record locks) */
    if (perf_dirty && !sq_play && !ks_sounding && fm1_ms - perf_ms > 2000u) { perf_dirty = 0; if (perf_save()) toast("SEQ NOT SAVED"); }
    if (set_dirty && !ks_sounding && fm1_ms - set_ms > 2000u) {   /* (erasing stalls the sound: only when silent) */
        set_dirty = 0;
        settings_write();
    }
    link_service();
    autosave_tick();
    pad_quant_tick();
    if (!dimmed && fm1_ms - idle_ms > 120000u) dim_set(1);
#if KS_DUAL
    if (c1_failed) {                            /* core 1 stalled (no sound, frozen LFOs): one core, kept */
        c1_failed = 0;                          /* (this session only: the next start tries two cores again) */
        dual_mark[0] = dual_mark[1] = 0;
        toast("CORE 1 TOO SLOW: 1 CORE");
    }
    if (dual_on && dual_mark[0] && fm1_ms - dual_ms > 10000u) dual_mark[0] = dual_mark[1] = 0;   /* 10 s on two cores: it holds */
#endif
}

static void app_main(void)
{
    app_init();
    for (;;) {
        platform_poll();
        app_step();
    }
}

#if defined(KAWA_WASM)
/* the emulator: what the knobs do on this page (their labels), the page, the user store */
WASM_EXPORT("ksub_load") void ksub_load(uint32_t bank, uint32_t k) { load_preset(bank, k); }
WASM_EXPORT("ksub_nfactory") uint32_t ksub_nfactory(void) { return NFACTORY; }
WASM_EXPORT("ksub_page") uint32_t ksub_page(void) { return page; }
WASM_EXPORT("ksub_knob_label") const char *ksub_knob_label(uint32_t i)
{
    const view_t *v = view();
    if (i >= 4u) return "";
    if (v->lay == LY_PRESET) return i == 0u ? "SCROLL" : i == 2u ? "LOAD" : "";
    if (v->lay == LY_SAVE) return i == 0u ? "SLOT" : i == 1u ? "POSITION" : i == 2u ? "LETTER" : "INIT";
    if (win == 3u) { static const char *const KE[4] = {"FREQ", "GAIN", "Q", "SHAPE"}; return KE[i]; }
    if (win == 6u) { static const char *const KP[4] = {"CUTOFF", "RESO", "TYPE", "A/B"}; return KP[i]; }
    if (win == 7u) return mc_name(i);
    if (win == 4u) { static const char *const KX[4] = {"X", "Y", "CORNER", "SOUND"}; return KX[i]; }
    if (win == 5u) { static const char *const KA[4] = {"MORPH", "B", "PARTS", "TIME"}; return KA[i]; }
    if (win) return item_sname(win == 1u ? LV_IT[i] : G_MC1 + i);
    if (C3_ON) return item_sname(COL3[page].id[c3_col()][i]);
    return vknob(v, i) == I_NONE ? "" : item_sname(vknob(v, i));
}
WASM_EXPORT("ksub_store") uint8_t *ksub_store(void) { return w_store; }
WASM_EXPORT("ksub_store_size") uint32_t ksub_store_size(void) { return sizeof w_store; }
WASM_EXPORT("ksub_store_gen") uint32_t ksub_store_gen(void) { return w_store_gen; }
WASM_EXPORT("ksub_patch_name") const char *ksub_patch_name(void) { return pname; }
#endif
