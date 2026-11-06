/* SPDX-License-Identifier: GPL-3.0-only */
/* K-WASUB: the patch (its parameters, their names, ranges and formats), the lists, the factory presets. */

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* ---- the parameters of a patch (int8 each; the order is the stored order: append only) */
enum {
    A_ENG, A_LVL, A_P1, A_P2, A_P3, A_OCT, A_SEMI, A_FINE,          /* oscillator A */
    B_ENG, B_LVL, B_P1, B_P2, B_P3, B_OCT, B_SEMI, B_FINE,          /* oscillator B (same layout: +8) */
    S_WAVE, S_OCT, S_LVL, S_ROUTE, N_LVL, N_COL,                    /* sub, noise */
    X_MODE, X_AMT, X_DRIFT, X_RESET,                                /* how A and B combine */
    F_TYPE, F_CUT, F_RES, F_DRV, F_ENV, F_KEY, F_VEL,               /* filter */
    AE_A, AE_D, AE_S, AE_R, FE_A, FE_D, FE_S, FE_R,                 /* amp and filter envelopes */
    ME_A, ME_D, ME_AMT, ME_DST,                                     /* mod envelope (AD) */
    L1_SHP, L1_SYNC, L1_RATE, L1_DIV, L1_DEP, L1_DST, L1_TRIG, L1_PH, L1_SMO,   /* LFO 1 (the wobble) */
    L2_SHP, L2_SYNC, L2_RATE, L2_DIV, L2_DEP, L2_DST, L2_TRIG, L2_PH, L2_SMO,   /* LFO 2 (same layout: +9) */
    M1_SRC, M1_DST, M1_AMT, M2_SRC, M2_DST, M2_AMT, M3_SRC, M3_DST, M3_AMT, M4_SRC, M4_DST, M4_AMT,
    DR_ON, DR_TYPE, DR_AMT, DR_MIX, CR_ON, CR_BITS, CR_RATE, CR_MIX, CH_ON, CH_RATE, CH_DEP, CH_MIX,
    DL_ON, DL_TIME, DL_FB, DL_TONE, DL_MIX, RV_ON, RV_SIZE, RV_DAMP, RV_MIX, MS_GLUE, MS_SUB, MS_VOL,
    V_MODE, V_GLIDE, V_BEND, V_VEL, P_STYLE,
    ST_ON, ST_PAT, ST_RATE, ST_GATE, ST_DST, ST_DEP, ST_SMO,     /* the STEP effect: 16 steps on the tempo */
    CP_ON, CP_AMT, CP_ATK, CP_REL, CP_PUMP, CP_PRATE,            /* the compressor and its PUMP (from 111: the store's 2nd sector) */
    RV_TYPE, DL_TYPE,                                            /* ROOM HALL PLATE SPRING; PING-PONG TAPE DIGITAL DUB */
    PH_ON, PH_RATE, PH_DEP, PH_FB, PH_MIX,                       /* the phaser */
    SM_SEL, SM_LVL, SM_TUNE, SM_KEY,                             /* OSC C, the SAMPLE layer: which, level, tune, keys */
    SM_MODE, SM_SPEED,                                           /* ... looped or once, slower (the same pitch) */
    OT_AMT, OT_LOW, OT_HIGH,                                     /* MULTIBAND (OTT): 3 bands squeezed up and down; the low and high bands' level */
    MB_MONO, S_LOCK,                                             /* MONO BASS under a frequency; the sub locked on OSC A (its pitch, its phase) */
    SM_START, SM_END, SM_DIR,                                    /* OSC C: its start and end cut, FORWARD / REVERSE / PING-PONG (0.9.55) */
    A_P4, B_P4,                                                  /* each oscillator's 4th: FOLD after the engine (CS: RING, channel I x II) (1.0.26) */
    NP
};
#define NP_MAX 140u                             /* a patch in memory */
#define NP_STORE 112u                           /* ... in a user slot (sector 1); the rest beside SEQ BASS (sector 2) */
#define OSC_B (B_ENG - A_ENG)
#define LFO_2 (L2_SHP - L1_SHP)
#define NENG 11
enum { E_ANALOG, E_CS, E_WAVE, E_FM, E_PHASE, E_GONE5, E_SYNC, E_GONE7, E_KICK, E_GROWL, E_ADD };   /* (1.0.26: FOLD and VOCAL gone; the numbers kept, saved sounds move: patch_sanitize) */
#define NENGO 9u
static const uint8_t ENG_ORDER[NENGO] = {E_ANALOG, E_KICK, E_CS, E_WAVE, E_FM, E_GROWL, E_ADD, E_PHASE, E_SYNC};   /* the engines the knob walks */
enum { FT_LP24, FT_LP12, FT_BP, FT_HP, FT_NOTCH, FT_VOWEL, FT_COMB, NFT };
enum { D_OFF, D_CUT, D_RES, D_PITCH, D_AP1, D_AP2, D_BP1, D_BP2, D_ALVL, D_BLVL, D_XMOD, D_DRV, D_AMP, D_SUB, D_NOISE,
       D_WOB, D_AP3, D_BP3, D_APIT, D_BPIT, D_NCOL, D_FENV, D_L2DEP,
       D_L1RATE, D_L2RATE, D_GATE, D_DRAMT, D_DRMIX, D_CRBITS, D_CRRATE, D_CRMIX, D_CHRATE, D_CHDEP, D_CHMIX,   /* global: */
       D_DLFB, D_DLTONE, D_DLMIX, D_RVSIZE, D_RVDAMP, D_RVMIX, D_GLUE, D_SUBB, D_VOL,   /* once a block, not a voice */
       D_CPAMT, D_PHRATE, D_PHDEP, D_PHMIX,
       D_MORPHX, D_MORPHY, D_MORPH,               /* MORPH X/Y and A/B (the main loop reads them: ksub_ui.c) */
       D_ABMIX,                                   /* the A/B MIX of the oscillators (per voice: A one way, B the other) */
       D_EQ1F, D_EQ2F, D_EQ3F, D_EQ4F, D_EQ1G, D_EQ2G, D_EQ3G, D_EQ4G,   /* the EQ's bands (the main loop works the coefficients out again) */
       D_AEA, D_AED, D_AES, D_AER, D_FEA, D_FED, D_FES, D_FER, D_MEA, D_MED, D_MEAMT,   /* (1.0.16: from D_AEA, one parameter each: GD_ID) */
       D_GLIDE, D_DRIFT, D_FKEY, D_FVEL, D_STDEP, D_SMLVL, D_SMTUNE, D_SMSPEED, D_SMSTART, D_SMEND,
       D_CPATK, D_CPREL, D_CPPUMP, D_PHFB, D_OTAMT, D_OTLOW, D_OTHIGH, D_DLTIME,
       D_AP4, D_BP4,                              /* (1.0.26: each oscillator's FOLD / RING) */
       ND };   /* (stored in patches: append only) */
#define IS_PITCH(d) ((d) == D_PITCH || (d) == D_APIT || (d) == D_BPIT)
/* the order DEST steps through: the oscillators' EDIT rows together (ksub_ui.c skips an oscillator that is off) */
static const uint8_t DST_ORDER[ND] = {D_OFF, D_CUT, D_RES, D_MORPHX, D_MORPHY, D_MORPH, D_DRV, D_DRAMT, D_FENV, D_PITCH, D_ALVL, D_ABMIX, D_AP1, D_AP2, D_AP3, D_AP4, D_APIT, D_BLVL,
                                      D_BP1, D_BP2, D_BP3, D_BP4, D_BPIT, D_XMOD, D_SUB, D_NOISE, D_NCOL, D_AMP, D_WOB, D_L1RATE,
                                      D_L2RATE, D_L2DEP, D_GATE, D_DRMIX, D_CRBITS, D_CRRATE, D_CRMIX, D_CHRATE,
                                      D_CHDEP, D_CHMIX, D_PHRATE, D_PHDEP, D_PHMIX, D_DLFB, D_DLTONE, D_DLMIX, D_RVSIZE, D_RVDAMP,
                                      D_RVMIX, D_CPAMT, D_GLUE, D_SUBB, D_VOL,
                                      D_EQ1F, D_EQ1G, D_EQ2F, D_EQ2G, D_EQ3F, D_EQ3G, D_EQ4F, D_EQ4G,
                                      D_AEA, D_AED, D_AES, D_AER, D_FEA, D_FED, D_FES, D_FER, D_MEA, D_MED, D_MEAMT, D_GLIDE, D_DRIFT, D_FKEY, D_FVEL,
                                      D_STDEP, D_SMLVL, D_SMTUNE, D_SMSPEED, D_SMSTART, D_SMEND, D_CPATK, D_CPREL, D_CPPUMP, D_PHFB, D_OTAMT, D_OTLOW,
                                      D_OTHIGH, D_DLTIME};
/* the parameter each of the destinations from D_AEA moves (the DSP reads them modulated: GP) */
#define D_N0 D_AEA
static const uint8_t GD_ID[ND - D_N0] = {AE_A, AE_D, AE_S, AE_R, FE_A, FE_D, FE_S, FE_R, ME_A, ME_D, ME_AMT, V_GLIDE, X_DRIFT, F_KEY, F_VEL,
                                         ST_DEP, SM_LVL, SM_TUNE, SM_SPEED, SM_START, SM_END, CP_ATK, CP_REL, CP_PUMP, PH_FB, OT_AMT, OT_LOW,
                                         OT_HIGH, DL_TIME, A_P4, B_P4};
/* the parameter each global destination moves */
#define D_G0 D_L1RATE

enum { SR_OFF, SR_LFO1, SR_LFO2, SR_FENV, SR_MENV, SR_VEL, SR_WHEEL, SR_PRESS, SR_NOTE, SR_RAND, SR_STEP, SR_LFO3, NSRC };
enum { VM_MONO, VM_LEGATO, VM_POLY };

/* formats of the values on the screen */
enum { FM_PCT, FM_BIP, FM_LIST, FM_INT, FM_SEMI, FM_CENT, FM_TIME, FM_CUT, FM_LFOHZ, FM_DIV, FM_VOL, FM_OSCP, FM_DEST };

static const char *const L_ENG[NENG] = {"ANALOG", "CS", "WAVE", "FM", "PHASE", "-", "SYNC", "-", "808", "GROWL", "ADDITIVE"};
static const char *const L_SUBW[] = {"SINE", "TRI", "SQUARE", "PULSE"};
static const char *const L_SUBO[] = {"-2 OCT", "-1 OCT", "0 OCT"};
static const char *const L_ROUTE[] = {"FILTER", "DIRECT"};
static const char *const L_XMODE[] = {"ADD", "RING", "X-MOD", "SYNC"};   /* SYNC: B restarts with A */
static const char *const L_FT[NFT] = {"LP 24", "LP 12", "BAND", "HIGH", "NOTCH", "VOWEL", "COMB"};
static const char *const L_SHAPE[] = {"SINE", "TRI", "SAW DN", "SAW UP", "SQUARE", "S&H", "WUB", "STEPS", "DRIFT"};
static const char *const L_DIV[16] = {"4 BAR", "2 BAR", "1 BAR", "1/2", "1/4D", "1/2T", "1/4", "1/8D", "1/4T", "1/8", "1/8T",
                                      "1/16", "1/16T", "1/32", "1/32T", "1/64"};
static const uint32_t DIV_CPB[16] = {4096, 8192, 16384, 32768, 43691, 49152, 65536, 87381, 98304, 131072, 196608,
                                     262144, 393216, 524288, 786432, 1048576};   /* cycles per beat, Q16 */
/* LFO GRID STRAIGHT (for playing live): only the round divisions, a T or D one plays as the nearest round one */
static const uint8_t DIV_STRAIGHT[9] = {0, 1, 2, 3, 6, 9, 11, 13, 15};
static const uint8_t DIV_NEAR[16] = {0, 1, 2, 3, 3, 6, 6, 6, 9, 9, 11, 11, 13, 13, 15, 15};
static const char *const L_DST[ND] = {"OFF", "CUTOFF", "RESO", "PITCH", "A SHAPE", "A COLOR", "B SHAPE", "B COLOR",
                                      "A LEVEL", "B LEVEL", "X-MOD", "FILT DRIVE", "AMP", "SUB", "NOISE", "WOB DEPTH",
                                      "A EDGE", "B EDGE", "A PITCH", "B PITCH", "NOISE COLOR", "FILT ENV", "LFO2 DEPTH",
                                      "WOB RATE", "LFO2 RATE", "STEP GATE", "SAT AMOUNT", "SAT MIX", "CRUSH BITS",
                                      "CRUSH RATE", "CRUSH MIX", "CHORUS RATE", "CHORUS DEPTH", "CHORUS MIX", "DELAY FDBK",
                                      "DELAY TONE", "DELAY MIX", "VERB SIZE", "VERB DAMP", "VERB MIX", "GLUE", "SUB BOOST",
                                      "VOLUME", "COMP AMOUNT", "PHASER RATE", "PHASER DEPTH", "PHASER MIX",
                                      "MORPH X", "MORPH Y", "MORPH A/B", "A/B MIX",
                                      "EQ1 FREQ", "EQ2 FREQ", "EQ3 FREQ", "EQ4 FREQ", "EQ1 GAIN", "EQ2 GAIN", "EQ3 GAIN", "EQ4 GAIN",
                                      "AMP ATTACK", "AMP DECAY", "AMP SUSTAIN", "AMP RELEASE", "FENV ATTACK", "FENV DECAY", "FENV SUSTAIN",
                                      "FENV RELEASE", "MENV ATTACK", "MENV DECAY", "MENV AMOUNT", "GLIDE", "DRIFT", "KEY TRACK", "FILT VELO",
                                      "STEP DEPTH", "C LEVEL", "C TUNE", "C SPEED", "C START", "C END", "COMP ATTACK", "COMP RELEASE",
                                      "COMP PUMP", "PHASER FDBK", "OTT AMOUNT", "OTT LOW", "OTT HIGH", "DELAY TIME", "A FOLD", "B FOLD"};
static const char *const L_SRC[NSRC] = {"OFF", "LFO 1", "LFO 2", "FILT ENV", "MOD ENV", "VELOCITY", "MOD WHEEL",
                                        "PRESSURE", "NOTE", "RANDOM", "STEP", "LFO 3"};
/* the STEP patterns: 16 steps, a level 0..F each ("RANDOM": a new one every bar) */
static const char *const L_SPAT[16] = {"OFFBEAT", "PUMP", "DUB CHOP", "TRANCE", "TRIPLET", "BROKEN", "SKANK", "GALLOP",
                                      "STAIRS UP", "STAIRS DN", "DNB ROLL", "HALFTIME", "STUTTER", "ACID", "SYNCOPE", "RANDOM"};
static const char *const STEP_PAT[16] = {"0F0F0F0F0F0F0F0F", "37BF37BF37BF37BF", "F0F00F00F0F00F00", "F0FF0FF0F0FF0FF0",
                                        "F00F00F00F00F00F", "F00F00F0F00F0F00", "00F000F000F000F0", "F0FFF0FFF0FFF0FF",
                                        "0123456789ABCDEF", "FEDCBA9876543210", "F0F0FFF0F0F0FF0F", "FFFF0000FFFF0000",
                                        "FFF0F0F0FFFF0000", "F8A3F5C1E07B92D4", "F00F0F00F00F0F00", "0000000000000000"};
static const char *const L_SRATE[6] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const uint8_t STEP_PB[6] = {1, 2, 3, 4, 6, 8};   /* steps a beat */
static const char *const L_TRIG[] = {"FREE", "NOTE"};
static const char *const L_ONOFF[] = {"OFF", "ON"};
static const char *const L_DRT[] = {"SOFT", "TUBE", "HARD", "FOLD"};
static const char *const L_PRATE[] = {"1/4", "1/8", "1/2", "1 BAR"};
static const char *const L_RVT[] = {"ROOM", "HALL", "PLATE", "SPRING"};
static const char *const L_DLT[] = {"PING-PONG", "TAPE", "DIGITAL", "DUB"};
static const char *const L_SMP[29] = {"OFF", "808 BOOM", "KICK", "SNARE", "OI VOX", "REESE LOOP", "GROWL LOOP", "PLUCK", "FM BELL",
                                      "METAL HIT", "BRASS STAB", "VINYL", "TAPE HISS", "RAIN", "WIND", "DATA", "RESAMPLE",
                                      "PICK", "MALLET", "CHIFF", "HAMMER", "ZAP", "SLAP", "CLICK", "SCRATCH",
                                      "SHIMMER", "DARK PAD", "BUBBLES", "MACHINE"};   /* (KS_SMP's; 16: the RAM one, the ones after it from 0.9.55) */
static const char *const L_SMD[3] = {"FORWARD", "REVERSE", "PING-PONG"};
static const char *const L_SMK[3] = {"AUTO", "KEYS", "FIXED"};
static const char *const L_MONO[7] = {"OFF", "60 Hz", "80 Hz", "100 Hz", "120 Hz", "150 Hz", "200 Hz"};
static const char *const L_SMM[3] = {"AUTO", "LOOP", "ONCE"};
static const char *const L_VM[] = {"MONO", "LEGATO", "POLY"};
static const char *const L_STYLE[] = {"DNB", "DUB", "DUBSTEP", "TECHNO", "JUNGLE", "UK GARAGE", "TRAP", "HOUSE", "NEUROFUNK",
                                      "DUB TECHNO", "BREAKS", "MINIMAL"};   /* the music a preset is for */
#define NSTYLE 12

/* the engines' three timbre controls: names (long, short) and how they read */
static const char *const ENG_P[NENG][3] = {
    {"WAVE", "PWIDTH", "DRIVE"}, {"WAVE", "DETUNE", "MOTION"}, {"POSITION", "TABLE", "WARP"},
    {"INDEX", "RATIO", "FEEDBACK"}, {"AMOUNT", "TYPE", "BODY"}, {"-", "-", "-"},
    {"SYNC", "WAVE", "SOFT"}, {"-", "-", "-"}, {"PUNCH", "DROP", "DRIVE"},
    {"GROWL", "VOWEL", "BITE"}, {"TILT", "HARMONICS", "ODD/EVEN"}};
static const char *const ENG_P4[NENG] = {"FOLD", "RING", "FOLD", "ALGO", "FOLD", "-", "FOLD", "-", "CLICK", "TYPE", "REGISTER"};   /* each engine's 4th setting */
static const char *const ENG_TITLE[NENG] = {"ANALOG / VA", "CS / DUAL", "WAVETABLE", "FM / 4-OP",
                                            "PHASE DIST", "-", "HARD SYNC", "-", "808 / KICK", "GROWL", "ADD DRAWBAR"};
static const char *const ENG_SUB[NENG] = {"SUBTRACTIVE CORE", "2 CHANNELS: SAW + PULSE, PWM, DRIFT", "8 TABLES x 8 FRAMES",
                                          "4 OPERATORS, 8 ALGORITHMS", "CZ-STYLE PHASE DISTORTION", "-",
                                          "MASTER / SLAVE SYNC", "-", "SINE, PITCH DROP, DRIVE, CLICK",
                                          "FM INTO 3 VOWEL FORMANTS", "16 PARTIALS, 8 REGISTERS"};
static const char *const L_WT[8] = {"BASIC", "HARMONIC", "GROWL", "VOCAL", "FM", "PWM", "DIGITAL", "SYNC"};
static const char *const L_PDT[6] = {"SAW", "SQUARE", "PULSE", "RESO SAW", "RESO TRI", "RESO TRAP"};
static const char *const L_RATIO[16] = {"0.50", "1.00", "1.50", "2.00", "2.50", "3.00", "3.50", "4.00", "5.00", "6.00",
                                        "7.00", "8.00", "9.00", "11.0", "1.41", "3.14"};
static const uint16_t RATIO_Q8[16] = {128, 256, 384, 512, 640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2304, 2816, 361, 804};
static const char *const L_VOX = "AEIOU";
/* GROWL: an FM pair (TYPE: the modulator's ratio, its feedback, a drive, a sub) into three formant filters that glide
 * U > O > A > E > I (VOWEL); GROWL the FM index, BITE the formants' resonance and the drive into them */
typedef struct { uint16_t ratio; uint8_t fb, drive, sub, shift; } growl_t;   /* ratio Q8; fb, drive, sub 0..255; shift: the formants, Q7 */
static const growl_t GR_TYPE[8] = {{256, 40, 40, 0, 128}, {512, 30, 50, 0, 128}, {128, 50, 60, 140, 116}, {256, 160, 150, 0, 128},
                                   {361, 70, 80, 0, 140}, {256, 60, 70, 60, 166}, {768, 90, 110, 0, 190}, {256, 120, 255, 0, 128}};
static const char *const L_GRT[8] = {"CLASSIC", "HOLLOW", "SUB GROWL", "SNARL", "METAL", "TALK", "SCREECH", "RIP"};
static const uint16_t GR_VOW[5][3] = {{325, 700, 2530}, {450, 800, 2830}, {800, 1200, 2500}, {400, 2000, 2550}, {270, 2300, 3000}};   /* U O A E I, Hz */
/* ADDITIVE: 16 partials on half the note (1: the 16' sub, 2: the note, 3: 5 1/3', 4: 4' ...); REGISTER: SAW (every
 * harmonic at 1/h) or a drawbar setting (16' 8' 5 1/3' 4' 2 2/3' 2' 1 3/5' 1 1/3' 1', 0..8); TILT, HARMONICS, ODD/EVEN shape it */
static const uint8_t ADD_BAR[9] = {1, 2, 3, 4, 6, 8, 10, 12, 16};
static const char ADD_REG[8][10] = {"", "888000000", "880000000", "888888888", "888800000", "806000000", "008080800", "868446468"};
static const char *const L_ADR[8] = {"SAW", "JAZZ 888", "SUB 880", "FULL", "GOSPEL", "HOLLOW", "FLUTE", "CHURCH"};
/* FM's 4 operators: ALGO, who modulates whom (bits: 4>1 4>2 4>3, 3>1 3>2, -, 2>1; an operator that modulates nobody is heard);
 * RATIO's set: operator 2 at L_RATIO, 3 and 4 at these (Q8) */
static const uint8_t FM_ALG[8] = {0x40, 0x54, 0x52, 0x51, 0x4C, 0x44, 0x07, 0x00};
static const char *const L_ALG[8] = {"2>1", "4>3>2>1", "34>2>1", "3>2>1<4", "4>3>1<2", "2>1 4>3", "4>123", "1+2+3+4"};
static const uint16_t FM_R34[16][2] = {{256, 512}, {512, 768}, {256, 768}, {256, 1024}, {256, 1280}, {256, 512}, {256, 1792}, {512, 256},
                                       {256, 768}, {512, 256}, {256, 768}, {512, 256}, {256, 768}, {512, 256}, {722, 256}, {361, 512}};

typedef struct {
    const char *name, *sname;                   /* the row's name, the knob's (footer) */
    int8_t lo, hi, def;
    uint8_t fmt;
    const char *const *list;
} pdesc_t;

#define OSC_PD(L) \
    {L "ENGINE", "ENGINE", 0, NENG - 1, 0, FM_LIST, L_ENG}, {L "LEVEL", "LEVEL", 0, 127, 100, FM_PCT, 0}, \
    {L "P1", "P1", 0, 127, 85, FM_OSCP, 0}, {L "P2", "P2", 0, 127, 0, FM_OSCP, 0}, {L "P3", "P3", 0, 127, 0, FM_OSCP, 0}, \
    {L "OCTAVE", "OCT", -3, 2, 0, FM_INT, 0}, {L "SEMI", "SEMI", -12, 12, 0, FM_SEMI, 0}, \
    {L "FINE", "FINE", -50, 50, 0, FM_CENT, 0}
#define LFO_PD(L) \
    {"SHAPE", "SHAPE", 0, 8, 0, FM_LIST, L_SHAPE}, {"SYNC", "SYNC", 0, 1, 1, FM_LIST, L_ONOFF}, \
    {"RATE", "RATE", 0, 127, 70, FM_LFOHZ, 0}, {"RATE / SYNC", "RATE", 0, 15, 9, FM_DIV, L_DIV}, \
    {"DEPTH", "DEPTH", -64, 63, 0, FM_BIP, 0}, {"DEST", "DEST", 0, ND - 1, D_CUT, FM_DEST, L_DST}, \
    {"RETRIG", "TRIG", 0, 1, 0, FM_LIST, L_TRIG}, {"PHASE", "PHASE", 0, 127, 0, FM_PCT, 0}, \
    {"SMOOTH", "SMOOTH", 0, 127, 0, FM_PCT, 0}
#define MX_PD(N) \
    {N " SOURCE", "SRC" N, 0, NSRC - 1, 0, FM_LIST, L_SRC}, {N " DEST", "DST" N, 0, ND - 1, 0, FM_DEST, L_DST}, \
    {N " AMOUNT", "AMT" N, -64, 63, 0, FM_BIP, 0}

static const pdesc_t PD[NP] = {
    OSC_PD("A "), OSC_PD("B "),
    {"SUB WAVE", "WAVE", 0, 3, 0, FM_LIST, L_SUBW}, {"SUB OCTAVE", "OCT", 0, 2, 1, FM_LIST, L_SUBO},
    {"SUB LEVEL", "SUB", 0, 127, 0, FM_PCT, 0}, {"SUB ROUTE", "ROUTE", 0, 1, 1, FM_LIST, L_ROUTE},
    {"NOISE LEVEL", "NOISE", 0, 127, 0, FM_PCT, 0}, {"NOISE COLOR", "COLOR", 0, 127, 40, FM_PCT, 0},
    {"MODE", "MODE", 0, 3, 0, FM_LIST, L_XMODE}, {"X-MOD", "XMOD", 0, 127, 0, FM_PCT, 0},
    {"DRIFT", "DRIFT", 0, 127, 10, FM_PCT, 0}, {"KEY RESET", "RESET", 0, 1, 1, FM_LIST, L_ONOFF},
    {"TYPE", "TYPE", 0, NFT - 1, 0, FM_LIST, L_FT}, {"CUTOFF", "FREQ", 0, 127, 90, FM_CUT, 0},
    {"RESONANCE", "RES", 0, 127, 15, FM_PCT, 0}, {"DRIVE", "DRIVE", 0, 127, 0, FM_PCT, 0},
    {"ENV AMOUNT", "ENV", -64, 63, 0, FM_BIP, 0}, {"KEY TRACK", "KEY", 0, 127, 64, FM_PCT, 0},
    {"VELOCITY", "VEL", 0, 127, 0, FM_PCT, 0},
    {"01 ATTACK", "A", 0, 127, 0, FM_TIME, 0}, {"02 DECAY", "D", 0, 127, 70, FM_TIME, 0},
    {"03 SUSTAIN", "S", 0, 127, 127, FM_PCT, 0}, {"04 RELEASE", "R", 0, 127, 40, FM_TIME, 0},
    {"01 ATTACK", "A", 0, 127, 0, FM_TIME, 0}, {"02 DECAY", "D", 0, 127, 70, FM_TIME, 0},
    {"03 SUSTAIN", "S", 0, 127, 64, FM_PCT, 0}, {"04 RELEASE", "R", 0, 127, 40, FM_TIME, 0},
    {"01 ATTACK", "A", 0, 127, 0, FM_TIME, 0}, {"02 DECAY", "D", 0, 127, 50, FM_TIME, 0},
    {"03 AMOUNT", "AMT", -64, 63, 0, FM_BIP, 0}, {"04 DEST", "DEST", 0, ND - 1, D_PITCH, FM_DEST, L_DST},
    LFO_PD(1), LFO_PD(2), MX_PD("1"), MX_PD("2"), MX_PD("3"), MX_PD("4"),
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"TYPE", "TYPE", 0, 3, 0, FM_LIST, L_DRT},
    {"AMOUNT", "AMT", 0, 127, 40, FM_PCT, 0}, {"MIX", "MIX", 0, 127, 127, FM_PCT, 0},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"BITS", "BITS", 0, 127, 40, FM_PCT, 0},
    {"RATE", "RATE", 0, 127, 20, FM_PCT, 0}, {"MIX", "MIX", 0, 127, 64, FM_PCT, 0},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"RATE", "RATE", 0, 127, 30, FM_PCT, 0},
    {"DEPTH", "DEPTH", 0, 127, 60, FM_PCT, 0}, {"MIX", "MIX", 0, 127, 50, FM_PCT, 0},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"TIME", "TIME", 3, 13, 8, FM_DIV, L_DIV},
    {"FEEDBACK", "FDBK", 0, 127, 60, FM_PCT, 0}, {"TONE", "TONE", 0, 127, 50, FM_PCT, 0},
    {"MIX", "MIX", 0, 127, 30, FM_PCT, 0},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"SIZE", "SIZE", 0, 127, 70, FM_PCT, 0},
    {"DAMP", "DAMP", 0, 127, 60, FM_PCT, 0}, {"MIX", "MIX", 0, 127, 25, FM_PCT, 0},
    {"GLUE", "GLUE", 0, 127, 30, FM_PCT, 0}, {"SUB BOOST", "BOOST", 0, 127, 0, FM_PCT, 0},
    {"OUTPUT", "OUT", 0, 127, 90, FM_VOL, 0},
    {"VOICE MODE", "VOICE", 0, 2, VM_MONO, FM_LIST, L_VM}, {"GLIDE", "GLIDE", 0, 127, 0, FM_TIME, 0},
    {"BEND RANGE", "BEND", 0, 24, 2, FM_SEMI, 0}, {"VELOCITY", "VELO", 0, 127, 40, FM_PCT, 0},
    {"STYLE", "STYLE", 0, NSTYLE - 1, 0, FM_LIST, L_STYLE},
    {"STEP", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"PATTERN", "PATTERN", 0, 15, 0, FM_LIST, L_SPAT},
    {"RATE", "RATE", 0, 5, 3, FM_LIST, L_SRATE}, {"GATE", "GATE", 0, 127, 100, FM_PCT, 0},
    {"DEST", "DEST", 0, ND - 1, D_OFF, FM_DEST, L_DST}, {"DEPTH", "DEPTH", -64, 63, 0, FM_BIP, 0},
    {"SMOOTH", "SMOOTH", 0, 127, 10, FM_PCT, 0},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"AMOUNT", "AMT", 0, 127, 50, FM_PCT, 0},
    {"ATTACK", "ATK", 0, 127, 20, FM_PCT, 0}, {"RELEASE", "REL", 0, 127, 50, FM_PCT, 0},
    {"PUMP", "PUMP", 0, 127, 0, FM_PCT, 0}, {"PUMP RATE", "P.RATE", 0, 3, 0, FM_LIST, L_PRATE},
    {"TYPE", "TYPE", 0, 3, 1, FM_LIST, L_RVT}, {"TYPE", "TYPE", 0, 3, 0, FM_LIST, L_DLT},
    {"EFFECT", "ON", 0, 1, 0, FM_LIST, L_ONOFF}, {"RATE", "RATE", 0, 127, 40, FM_LFOHZ, 0},
    {"DEPTH", "DEPTH", 0, 127, 80, FM_PCT, 0}, {"FEEDBACK", "FDBK", 0, 127, 50, FM_PCT, 0},
    {"MIX", "MIX", 0, 127, 64, FM_PCT, 0},
    {"SAMPLE", "SAMPLE", 0, 28, 0, FM_LIST, L_SMP}, {"LEVEL", "LEVEL", 0, 127, 90, FM_PCT, 0},
    {"TUNE", "TUNE", -24, 24, 0, FM_SEMI, 0}, {"PITCH", "PITCH", 0, 2, 0, FM_LIST, L_SMK},
    {"PLAY", "PLAY", 0, 2, 0, FM_LIST, L_SMM}, {"SPEED", "SPEED", -64, 127, 0, FM_PCT, 0},
    {"AMOUNT", "OTT", 0, 127, 0, FM_PCT, 0}, {"LOW BAND", "LOW", -64, 63, 0, FM_BIP, 0}, {"HIGH BAND", "HIGH", -64, 63, 0, FM_BIP, 0},
    {"MONO BASS", "MONO", 0, 6, 4, FM_LIST, L_MONO}, {"SUB LOCK", "LOCK", 0, 1, 1, FM_LIST, L_ONOFF},
    {"START", "START", 0, 127, 0, FM_PCT, 0}, {"END", "END", 0, 127, 0, FM_PCT, 0}, {"DIRECTION", "DIR", 0, 2, 0, FM_LIST, L_SMD},
    {"A P4", "P4", 0, 127, 0, FM_OSCP, 0}, {"B P4", "P4", 0, 127, 0, FM_OSCP, 0}
};

/* ---- the factory presets: the defaults, then (parameter, value) pairs, -1 ends */
typedef struct {
    const char *name;
    const int8_t *ov;
} preset_t;
#define PR(nm, ...) {nm, (const int8_t[]){__VA_ARGS__, -1}}
static const preset_t FACTORY[] = {
    PR("DEEP PRESSURE", MS_VOL, 76, P_STYLE, 0, A_ENG, E_ANALOG, A_P1, 0, A_LVL, 70, S_LVL, 105, S_WAVE, 0, F_CUT, 70, F_RES, 20,
       F_DRV, 30, AE_R, 30, ME_AMT, 20, ME_D, 35, DR_ON, 1, DR_TYPE, 1, DR_AMT, 30, DR_MIX, 60, MS_SUB, 30, X_DRIFT, 6),
    PR("SUB TIDE", P_STYLE, 0, A_ENG, E_ANALOG, A_P1, 0, A_LVL, 110, F_CUT, 127, F_RES, 0, AE_A, 10, AE_R, 55,
       L1_DEP, 3, L1_DST, D_PITCH, L1_SYNC, 0, L1_RATE, 60, V_GLIDE, 45),
    PR("IRON DUB", MS_VOL, 116, P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 110, A_P2, 30, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 60, B_FINE, 8,
       F_CUT, 55, F_RES, 45, F_ENV, 30, FE_D, 60, FE_S, 20, AE_D, 75, AE_S, 90, S_LVL, 80, DL_ON, 1, DL_TIME, 7,
       DL_FB, 85, DL_TONE, 35, DL_MIX, 45, RV_ON, 1, RV_MIX, 25, V_MODE, VM_LEGATO, V_GLIDE, 30),
    PR("NIGHT BUS", MS_VOL, 127, P_STYLE, 3, A_LVL, 122, A_ENG, E_WAVE, A_P1, 30, A_P2, 16, B_ENG, E_ANALOG, B_P1, 42, B_LVL, 50, B_OCT, -1,
       F_CUT, 60, F_RES, 50, F_ENV, 40, FE_D, 55, FE_S, 0, AE_D, 60, AE_S, 70, L2_DEP, 20, L2_DST, D_AP1,
       L2_SYNC, 1, L2_DIV, 2, CH_ON, 1, DL_ON, 1, DL_TIME, 8, DL_MIX, 25),
    PR("CONCRETE", MS_VOL, 70, P_STYLE, 3, A_ENG, E_FM, A_P1, 55, A_P2, 24, A_P3, 45, S_LVL, 60, F_CUT, 80, F_RES, 30,
       AE_D, 45, AE_S, 40, ME_AMT, 30, ME_DST, D_AP1, ME_D, 45, DR_ON, 1, DR_TYPE, 2, DR_AMT, 50, MS_GLUE, 60),
    PR("REESE CLASSIC", MS_VOL, 113, P_STYLE, 0, A_ENG, E_CS, A_P1, 12, A_P2, 82, A_P3, 30, A_LVL, 100, S_LVL, 70, F_CUT, 75,
       F_RES, 25, L2_DEP, 18, L2_DST, D_AP1, L2_SYNC, 0, L2_RATE, 30, L2_SHP, 1, CH_ON, 1, CH_MIX, 40, X_DRIFT, 30),
    PR("WIDE REESE", P_STYLE, 8, A_ENG, E_CS, A_P1, 10, A_P2, 100, A_P3, 70, B_ENG, E_CS, B_P1, 5, B_P2, 70, B_P3, 20,
       B_LVL, 70, B_OCT, -1, S_LVL, 85, F_CUT, 68, F_RES, 35, F_DRV, 40, CH_ON, 1, CH_DEP, 80, CH_MIX, 60,
       DR_ON, 1, DR_AMT, 25, DR_MIX, 50),
    PR("WOBBLE 01", P_STYLE, 2, A_ENG, E_ANALOG, A_P1, 85, B_ENG, E_ANALOG, B_P1, 127, B_LVL, 70, B_FINE, -10,
       S_LVL, 90, F_CUT, 40, F_RES, 70, F_DRV, 50, L1_SHP, 0, L1_DIV, 9, L1_DEP, 45, L1_DST, D_CUT, L1_TRIG, 1,
       DR_ON, 1, DR_AMT, 50, DR_MIX, 80, M1_SRC, SR_WHEEL, M1_DST, D_WOB, M1_AMT, 40),
    PR("YOI TALKER", MS_VOL, 74, P_STYLE, 2, A_ENG, E_WAVE, A_P1, 10, A_P2, 48, A_P3, 40, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 60,
       S_LVL, 80, F_TYPE, FT_VOWEL, F_CUT, 20, F_RES, 80, L1_SHP, 1, L1_DIV, 9, L1_DEP, 50, L1_DST, D_CUT,
       L1_TRIG, 1, L2_SHP, 1, L2_DIV, 9, L2_DEP, 40, L2_DST, D_AP1, L2_TRIG, 1, DR_ON, 1, DR_AMT, 45),
    PR("NEURO GROWL", MS_VOL, 102, P_STYLE, 8, A_ENG, E_GROWL, A_P1, 70, A_P2, 50, A_P3, 80, B_ENG, E_WAVE, B_P1, 50, B_P2, 32, B_LVL, 60,
       X_MODE, 2, X_AMT, 30, S_LVL, 80, F_TYPE, FT_COMB, F_CUT, 70, F_RES, 75, F_KEY, 127, L1_SHP, 6, L1_DIV, 11,
       L1_DEP, 45, L1_DST, D_AP2, L1_TRIG, 1, M1_SRC, SR_LFO1, M1_DST, D_CUT, M1_AMT, 25, DR_ON, 1, DR_TYPE, 3,
       DR_AMT, 40, MS_GLUE, 70),
    PR("METAL FM", MS_VOL, 76, P_STYLE, 2, A_ENG, E_FM, A_P1, 80, A_P2, 120, A_P3, 30, S_LVL, 90, F_CUT, 85, F_RES, 40,
       L1_SHP, 4, L1_DIV, 10, L1_DEP, 35, L1_DST, D_AP1, L1_TRIG, 1, L1_SMO, 40, DR_ON, 1, DR_TYPE, 2, DR_AMT, 35),
    PR("808 DROP", P_STYLE, 6, A_ENG, E_KICK, A_P1, 75, A_P2, 55, A_P3, 30, A_P4, 20, A_LVL, 120, F_CUT, 127, F_RES, 0, AE_D, 85,
       AE_S, 0, AE_R, 60, V_GLIDE, 35, DR_ON, 1, DR_TYPE, 1, DR_AMT, 35),
    PR("DUB STAB", MS_VOL, 127, P_STYLE, 9, A_ENG, E_ANALOG, A_P1, 85, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 90, B_SEMI, 7,
       F_TYPE, FT_LP12, F_CUT, 64, F_RES, 40, F_ENV, 45, FE_D, 50, FE_S, 0, AE_D, 55, AE_S, 0, AE_R, 55,
       V_MODE, VM_POLY, DL_ON, 1, DL_TIME, 7, DL_FB, 90, DL_TONE, 30, DL_MIX, 55, RV_ON, 1, RV_SIZE, 100, RV_MIX, 40),
    PR("TECHNO RUMBLE", P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 50, S_LVL, 100, N_LVL, 25, N_COL, 90, F_CUT, 45,
       F_RES, 55, F_ENV, 30, FE_D, 45, FE_S, 10, AE_D, 60, AE_S, 50, RV_ON, 1, RV_SIZE, 120, RV_DAMP, 100,
       RV_MIX, 50, DR_ON, 1, DR_AMT, 60, DR_MIX, 70, MS_GLUE, 50),
    PR("ACID LINE", MS_VOL, 77, P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 85, S_LVL, 40, F_TYPE, FT_LP24, F_CUT, 64, F_RES, 100,
       F_ENV, 50, FE_D, 45, FE_S, 0, AE_D, 50, AE_S, 80, V_MODE, VM_LEGATO, V_GLIDE, 25, DR_ON, 1, DR_TYPE, 1,
       DR_AMT, 50, DL_ON, 1, DL_TIME, 10, DL_MIX, 20),
    PR("PHASE HORN", MS_VOL, 77, P_STYLE, 2, A_ENG, E_FM, A_P1, 70, A_P2, 8, A_P3, 60, S_LVL, 80, F_CUT, 95, F_RES, 20,
       L1_DIV, 10, L1_DEP, 40, L1_DST, D_AP1, L1_TRIG, 1, DR_ON, 1, DR_AMT, 40),
    PR("SYNC RIPPER", MS_VOL, 73, P_STYLE, 8, A_ENG, E_WAVE, A_P1, 40, A_P2, 112, A_P3, 30, S_LVL, 80, F_CUT, 100, F_RES, 25,
       ME_AMT, 40, ME_DST, D_AP1, ME_D, 60, L1_DIV, 11, L1_DEP, 30, L1_DST, D_AP1, DR_ON, 1, DR_TYPE, 2, DR_AMT, 45),
    PR("HOLLOW TUBE", MS_VOL, 119, P_STYLE, 2, A_ENG, E_WAVE, A_P1, 70, A_P2, 32, A_P3, 30, S_LVL, 90, F_TYPE, FT_NOTCH, F_CUT, 50,
       F_RES, 60, L1_SHP, 0, L1_DIV, 10, L1_DEP, 40, L1_DST, D_CUT, L1_TRIG, 1),
    PR("WUB MACHINE", MS_VOL, 74, P_STYLE, 2, A_ENG, E_CS, A_P1, 16, A_P2, 45, A_P3, 40, B_ENG, E_FM, B_P1, 50, B_P2, 16, B_LVL, 60,
       S_LVL, 90, F_CUT, 30, F_RES, 60, F_DRV, 70, L1_SHP, 6, L1_DIV, 10, L1_DEP, 55, L1_DST, D_CUT, L1_TRIG, 1,
       M1_SRC, SR_LFO1, M1_DST, D_BP1, M1_AMT, 30, DR_ON, 1, DR_AMT, 60, MS_GLUE, 70),
    PR("DIGI BITE", MS_VOL, 108, P_STYLE, 10, A_ENG, E_WAVE, A_P1, 60, A_P2, 96, A_P3, 20, S_LVL, 85, F_CUT, 90, F_RES, 30,
       CR_ON, 1, CR_BITS, 70, CR_RATE, 40, CR_MIX, 70, L1_DIV, 11, L1_DEP, 45, L1_DST, D_AP1, L1_TRIG, 1),
    PR("ROLLER SUB", MS_VOL, 112, P_STYLE, 0, A_ENG, E_ANALOG, A_P1, 42, A_LVL, 60, S_LVL, 110, S_WAVE, 1, F_CUT, 60, F_RES, 10,
       AE_D, 50, AE_S, 100, AE_R, 25, V_MODE, VM_LEGATO, V_GLIDE, 20, MS_SUB, 50),
    PR("HOOVER DNB", MS_VOL, 113, P_STYLE, 4, A_ENG, E_CS, A_P1, 70, A_P2, 110, A_P3, 95, B_ENG, E_ANALOG, B_P1, 127, B_P2, 80,
       B_LVL, 60, B_OCT, -1, S_LVL, 60, F_CUT, 95, ME_AMT, -20, ME_DST, D_PITCH, ME_A, 40, ME_D, 70,
       V_GLIDE, 40, CH_ON, 1, CH_MIX, 60, L2_DEP, 25, L2_DST, D_BP2, L2_SYNC, 0, L2_RATE, 50),
    PR("STEPPA DUB", MS_VOL, 109, P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 10, A_P3, 30, S_LVL, 110, F_CUT, 45, F_RES, 15, AE_D, 70,
       AE_S, 60, DL_ON, 1, DL_TIME, 6, DL_FB, 70, DL_TONE, 25, DL_MIX, 35, MS_SUB, 60, MS_GLUE, 40),
    PR("LIQUID SUB", P_STYLE, 0, A_ENG, E_ANALOG, A_P1, 42, A_LVL, 60, S_LVL, 110, F_CUT, 75, AE_A, 8, AE_R, 50,
       V_MODE, VM_LEGATO, V_GLIDE, 30, CH_ON, 1, CH_MIX, 30),
    PR("ROLLING REESE", P_STYLE, 0, A_ENG, E_CS, A_P1, 14, A_P2, 62, A_P3, 22, B_ENG, E_ANALOG, B_P1, 85, B_OCT, -1, B_LVL, 50,
       S_LVL, 90, F_CUT, 70, F_RES, 30, L2_DEP, 25, L2_DST, D_AP1, L2_SYNC, 1, L2_DIV, 6, DR_ON, 1, DR_TYPE, 1, DR_AMT, 30),
    PR("AMEN SUB", MS_VOL, 124, P_STYLE, 4, A_ENG, E_ANALOG, A_P1, 0, A_P3, 50, S_LVL, 100, ME_AMT, 20, ME_DST, D_PITCH, ME_D, 30,
       AE_D, 70, AE_S, 60, V_GLIDE, 20),
    PR("JUNGLE BOOM", P_STYLE, 4, A_ENG, E_ANALOG, A_P1, 0, A_LVL, 115, AE_D, 95, AE_S, 0, AE_R, 70, ME_AMT, 24,
       ME_DST, D_PITCH, ME_D, 45, DR_ON, 1, DR_TYPE, 1, DR_AMT, 45),
    PR("RIDDIM SQUARE", MS_VOL, 74, P_STYLE, 2, A_ENG, E_ANALOG, A_P1, 127, A_P2, 40, B_ENG, E_ANALOG, B_P1, 127, B_OCT, -1, B_LVL, 60,
       S_LVL, 80, F_CUT, 35, F_RES, 50, L1_SHP, 4, L1_DIV, 10, L1_DEP, 50, L1_DST, D_CUT, L1_TRIG, 1, DR_ON, 1,
       DR_TYPE, 2, DR_AMT, 55),
    PR("TEAROUT", MS_VOL, 127, P_STYLE, 2, A_ENG, E_GROWL, A_P1, 90, A_P2, 30, A_P3, 70, A_P4, 112, B_ENG, E_WAVE, B_P1, 60, B_P2, 32, B_LVL, 50, X_MODE, 2,
       X_AMT, 50, S_LVL, 80, F_CUT, 105, F_RES, 35, L1_SHP, 6, L1_DIV, 11, L1_DEP, 40,
       L1_DST, D_AP1, L1_TRIG, 1, DR_ON, 1, DR_TYPE, 3, DR_AMT, 50, MS_GLUE, 90),
    PR("BROSTEP SAW", MS_VOL, 74, P_STYLE, 2, A_ENG, E_CS, A_P1, 30, A_P2, 80, A_P3, 60, S_LVL, 85, F_CUT, 30, F_RES, 70, F_DRV, 60,
       L1_DIV, 9, L1_DEP, 55, L1_DST, D_CUT, L1_TRIG, 1, DR_ON, 1, DR_AMT, 60),
    PR("NEURO STAB", P_STYLE, 8, A_ENG, E_WAVE, A_P1, 70, A_P2, 112, B_ENG, E_FM, B_P1, 50, B_LVL, 60, X_MODE, 1,
       S_LVL, 70, F_CUT, 85, F_RES, 45, ME_AMT, 50, ME_DST, D_AP1, ME_D, 40, AE_D, 50, AE_S, 30, DR_ON, 1, DR_TYPE, 2,
       DR_AMT, 50),
    PR("SPEED BASS", MS_VOL, 127, P_STYLE, 5, A_ENG, E_ANALOG, A_P1, 100, S_LVL, 60, F_TYPE, FT_LP12, F_CUT, 50, F_RES, 50, F_ENV, 45,
       FE_D, 40, FE_S, 10, AE_D, 55, AE_S, 70, V_MODE, VM_LEGATO, V_GLIDE, 22),
    PR("UKG ORGAN", P_STYLE, 5, MS_VOL, 127, A_LVL, 120, A_ENG, E_WAVE, A_P1, 40, A_P2, 16, B_ENG, E_ANALOG, B_P1, 0, B_OCT, 1, B_LVL, 50,
       AE_D, 60, AE_S, 50, F_CUT, 90, CH_ON, 1),
    PR("TRAP 808", MS_VOL, 79, P_STYLE, 6, A_ENG, E_KICK, A_P1, 60, A_P2, 45, A_P3, 45, A_P4, 10, A_LVL, 120, F_CUT, 127, F_RES, 0, AE_D, 100, AE_S, 40,
       AE_R, 75, V_GLIDE, 50, DR_ON, 1, DR_TYPE, 1, DR_AMT, 45),
    PR("DEEP HOUSE", P_STYLE, 7, MS_VOL, 127, A_ENG, E_ANALOG, A_P1, 42, S_LVL, 70, F_TYPE, FT_LP12, F_CUT, 60, F_ENV, 25,
       FE_D, 45, AE_D, 60, AE_S, 60, AE_R, 30),
    PR("HOUSE ORGAN", P_STYLE, 7, MS_VOL, 127, A_LVL, 120, A_ENG, E_ADD, A_P1, 75, A_P2, 127, A_P4, 16, AE_D, 45, AE_S, 65, V_MODE, VM_POLY, F_CUT, 95,
       S_LVL, 40),
    PR("BERLIN RUMBLE", P_STYLE, 3, MS_VOL, 127, A_LVL, 120, A_ENG, E_ANALOG, A_P1, 85, N_LVL, 30, N_COL, 100, S_LVL, 90, F_CUT, 40, F_RES, 40,
       AE_D, 72, AE_S, 0, RV_ON, 1, RV_SIZE, 120, RV_DAMP, 110, RV_MIX, 60, DL_ON, 1, DL_TIME, 9, DL_FB, 70,
       DL_TONE, 25, DL_MIX, 30, MS_GLUE, 60),
    PR("DUBTECH PULSE", P_STYLE, 9, MS_VOL, 127, A_LVL, 120, A_ENG, E_ANALOG, A_P1, 85, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 80, B_SEMI, 7,
       F_TYPE, FT_LP12, F_CUT, 60, F_RES, 45, F_ENV, 40, FE_D, 55, FE_S, 0, AE_D, 60, AE_S, 0, V_MODE, VM_POLY,
       DL_ON, 1, DL_TIME, 8, DL_FB, 95, DL_TONE, 20, DL_MIX, 60, RV_ON, 1, RV_SIZE, 120, RV_MIX, 50, CH_ON, 1),
    PR("SKANK UNIT", P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 42, S_LVL, 110, F_CUT, 50, AE_A, 10, AE_D, 75, AE_S, 70,
       DL_ON, 1, DL_TIME, 6, DL_FB, 60, DL_MIX, 25, MS_SUB, 70),
    PR("BREAKS HOOVER", P_STYLE, 10, A_ENG, E_CS, A_P1, 64, A_P2, 120, A_P3, 100, ME_AMT, -25, ME_DST, D_PITCH,
       ME_A, 30, ME_D, 60, F_CUT, 90, S_LVL, 60, V_GLIDE, 35, CH_ON, 1, CH_MIX, 70),
    PR("MINIMAL CLICK", P_STYLE, 11, MS_VOL, 127, A_LVL, 120, A_ENG, E_FM, A_P1, 50, A_P2, 40, A_P3, 0, S_LVL, 70, AE_D, 35, AE_S, 0, AE_R, 20, F_CUT, 70,
       F_ENV, 30, FE_D, 25, FE_S, 0, DL_ON, 1, DL_TIME, 10, DL_MIX, 20),
    PR("YOI RIDDIM", MS_VOL, 71, P_STYLE, 2, A_ENG, E_WAVE, A_P1, 64, A_P2, 48, A_P3, 30, S_LVL, 80, F_TYPE, FT_VOWEL, F_CUT, 40, F_RES, 90,
       L1_SHP, 7, L1_DIV, 10, L1_DEP, 60, L1_DST, D_CUT, L1_TRIG, 1, DR_ON, 1, DR_AMT, 45),
    PR("INIT", P_STYLE, 0),
    /* (after INIT: the slots before keep their numbers) */
    PR("SYNC SWEEP", P_STYLE, 2, A_ENG, E_ANALOG, A_P1, 85, A_LVL, 0, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 110, X_MODE, 3,
       X_AMT, 30, S_LVL, 90, F_CUT, 95, F_RES, 20, L1_SHP, 0, L1_DIV, 9, L1_DEP, 45, L1_DST, D_XMOD, L1_TRIG, 1,
       DR_ON, 1, DR_TYPE, 1, DR_AMT, 40, CP_ON, 1, CP_AMT, 50),
    PR("PUMP HOUSE", MS_VOL, 127, P_STYLE, 7, A_LVL, 122, A_ENG, E_ANALOG, A_P1, 100, A_P2, 60, S_LVL, 80, F_CUT, 60, F_RES, 35, F_ENV, 35, FE_D, 40,
       FE_S, 10, AE_D, 60, AE_S, 60, AE_R, 25, CP_ON, 1, CP_AMT, 45, CP_PUMP, 90, RV_ON, 1, RV_TYPE, 0, RV_MIX, 15),
    PR("PHASE DUB", SM_SEL, 11, SM_LVL, 45, P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 42, S_LVL, 110, F_CUT, 55, PH_ON, 1, PH_RATE, 30, PH_DEP, 90,
       PH_FB, 70, PH_MIX, 80, DL_ON, 1, DL_TYPE, 1, DL_TIME, 7, DL_FB, 80, DL_MIX, 35, RV_ON, 1, RV_TYPE, 3, RV_MIX, 30,
       V_MODE, VM_LEGATO, V_GLIDE, 30),
    PR("TAPE TECHNO", SM_SEL, 12, SM_LVL, 35, MS_VOL, 127, P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 85, N_LVL, 20, S_LVL, 90, F_CUT, 45, F_RES, 45, F_ENV, 30, FE_D, 45,
       FE_S, 0, AE_D, 55, AE_S, 50, DL_ON, 1, DL_TYPE, 1, DL_TIME, 8, DL_FB, 75, DL_MIX, 30, RV_ON, 1, RV_SIZE, 100,
       RV_MIX, 25, CP_ON, 1, CP_AMT, 60, CP_PUMP, 50),
    PR("REESE PHASER", P_STYLE, 0, A_ENG, E_CS, A_P1, 20, A_P2, 90, A_P3, 50, S_LVL, 80, F_CUT, 70, F_RES, 25, PH_ON, 1,
       PH_RATE, 20, PH_DEP, 100, PH_FB, 60, PH_MIX, 70, X_DRIFT, 25, CP_ON, 1, CP_AMT, 40),
    PR("TECHNO ROLLER", MS_VOL, 112, P_STYLE, 3, A_ENG, E_CS, A_P1, 0, A_P2, 25, A_P3, 20, A_LVL, 115, S_LVL, 85, F_TYPE, FT_LP24,
       F_CUT, 55, F_RES, 35, F_ENV, 55, FE_D, 35, FE_S, 0, AE_D, 55, AE_S, 20, AE_R, 15, DR_ON, 1, DR_AMT, 45, RV_ON, 1, RV_SIZE, 60,
       RV_MIX, 18, MS_GLUE, 60),
    PR("WAVE MORPH", MS_VOL, 125, P_STYLE, 9, A_ENG, E_WAVE, A_P1, 0, A_P2, 32, S_LVL, 85, F_CUT, 65, F_RES, 30, L2_SHP, 1, L2_SYNC, 1,
       L2_DIV, 1, L2_DEP, 60, L2_DST, D_AP1, DL_ON, 1, DL_TYPE, 3, DL_TIME, 7, DL_FB, 85, DL_MIX, 35, RV_ON, 1, RV_TYPE, 2,
       RV_MIX, 25),
    PR("PLATE STAB", MS_VOL, 127, P_STYLE, 8, A_ENG, E_FM, A_P1, 90, A_P2, 24, A_P3, 60, S_LVL, 70, F_CUT, 90, F_RES, 40, AE_D, 45, AE_S, 20,
       AE_R, 30, ME_AMT, -45, ME_DST, D_AP1, ME_D, 35, RV_ON, 1, RV_TYPE, 2, RV_MIX, 30, CP_ON, 1, CP_AMT, 55),
    /* (1.0.1: 808s, acids and subs, on the four ESSENTIAL engines) */
    PR("808 BOOMER", MS_VOL, 92, P_STYLE, 6, A_ENG, E_KICK, A_P1, 90, A_P2, 70, A_P3, 55, A_P4, 30, A_LVL, 120, S_LVL, 0, F_CUT, 127, F_RES, 0, AE_A, 0,
       AE_D, 110, AE_S, 0, AE_R, 70, V_MODE, VM_LEGATO, V_GLIDE, 60,
       DR_ON, 1, DR_TYPE, 1, DR_AMT, 55, DR_MIX, 70, SM_SEL, 23, SM_LVL, 45),
    PR("808 DIST", MS_VOL, 100, P_STYLE, 6, A_ENG, E_KICK, A_P1, 70, A_P2, 40, A_P3, 110, A_P4, 25, A_LVL, 115, S_LVL, 0, F_CUT, 90, F_RES, 20, AE_D, 95,
       AE_S, 30, AE_R, 60, V_MODE, VM_LEGATO, V_GLIDE, 45, DR_ON, 1, DR_TYPE, 2,
       DR_AMT, 80, DR_MIX, 85, OT_AMT, 50),
    PR("808 KNOCK", MS_VOL, 127, P_STYLE, 4, A_ENG, E_ANALOG, A_P1, 42, A_LVL, 120, S_LVL, 50, S_WAVE, 0, F_CUT, 110, AE_D, 70, AE_S, 0,
       AE_R, 45, ME_AMT, 40, ME_DST, D_PITCH, ME_D, 20, SM_SEL, 2, SM_LVL, 40, DR_ON, 1, DR_TYPE, 1, DR_AMT, 40),
    PR("ACID SQUELCH", MS_VOL, 124, P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 85, S_LVL, 30, F_TYPE, FT_LP24, F_CUT, 68, F_RES, 115, F_ENV, 75,
       FE_A, 4, AE_A, 3, FE_D, 35, FE_S, 0, AE_D, 60, AE_S, 90, V_MODE, VM_LEGATO, V_GLIDE, 18, DR_ON, 1, DR_TYPE, 2, DR_AMT, 45,
       DR_MIX, 60, DL_ON, 1, DL_TIME, 7, DL_MIX, 18),
    PR("ACID SQUARE", MS_VOL, 78, P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 127, S_LVL, 30, F_TYPE, FT_LP24, F_CUT, 58, F_RES, 105, F_ENV, 60,
       FE_D, 30, FE_S, 0, AE_D, 55, AE_S, 85, L1_SYNC, 1, L1_DIV, 1, L1_DEP, 30, L1_DST, D_CUT, V_MODE, VM_LEGATO,
       V_GLIDE, 20, DR_ON, 1, DR_TYPE, 1, DR_AMT, 40),
    PR("ACID FM ZAP", MS_VOL, 127, P_STYLE, 11, A_ENG, E_FM, A_P1, 50, A_P2, 8, A_P3, 50, S_LVL, 40, F_TYPE, FT_LP12, F_CUT, 48, F_RES, 100,
       F_ENV, 70, FE_D, 30, FE_S, 0, ME_AMT, 40, ME_DST, D_AP1, ME_D, 30, AE_D, 50, AE_S, 70, V_MODE, VM_LEGATO, V_GLIDE, 15,
       DL_ON, 1, DL_TIME, 9, DL_MIX, 15),
    PR("PURE SUB", P_STYLE, 0, A_ENG, E_ANALOG, A_P1, 0, A_LVL, 110, S_LVL, 90, S_WAVE, 0, F_CUT, 127, F_RES, 0, AE_A, 4,
       AE_R, 40, V_MODE, VM_LEGATO, V_GLIDE, 15, MS_SUB, 40, MB_MONO, 4),
    PR("WARM TRI SUB", P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 42, A_P3, 50, A_LVL, 105, S_LVL, 100, S_WAVE, 1, F_CUT, 80, AE_A, 6,
       AE_R, 35, DR_ON, 1, DR_TYPE, 1, DR_AMT, 25, DR_MIX, 50, V_MODE, VM_LEGATO, V_GLIDE, 20),
    PR("FM SUB GROWL", P_STYLE, 1, A_ENG, E_FM, A_P1, 25, A_P2, 8, A_P3, 20, A_LVL, 100, S_LVL, 100, F_CUT, 70, F_RES, 15,
       L1_SYNC, 1, L1_DIV, 9, L1_DEP, 20, L1_DST, D_AP1, L1_TRIG, 1, V_MODE, VM_LEGATO, V_GLIDE, 25),
    /* the CS engine (1.0.24, after twochannels/CS-1): its two channels, the PWM, the drift (the 96 favourite bits: full) */
    PR("CS BRASS BASS", MS_VOL, 111, P_STYLE, 3, A_ENG, E_CS, A_P1, 64, A_P2, 50, A_P3, 40, A_LVL, 110, S_LVL, 80, F_TYPE, FT_LP24,
       F_CUT, 45, F_RES, 30, F_ENV, 50, FE_A, 20, FE_D, 60, FE_S, 30, AE_A, 8, AE_S, 90, AE_R, 30),
    PR("CS SQUELCH", MS_VOL, 127, P_STYLE, 11, A_ENG, E_CS, A_P1, 100, A_P2, 20, A_P3, 30, A_LVL, 110, S_LVL, 60, F_TYPE, FT_LP12,
       F_CUT, 50, F_RES, 95, F_ENV, 70, FE_D, 40, FE_S, 0, AE_D, 60, AE_S, 50, V_MODE, VM_LEGATO, V_GLIDE, 30),
    PR("CS XMAS BASS", MS_VOL, 100, P_STYLE, 1, A_ENG, E_CS, A_P1, 0, A_P2, 30, A_P3, 20, A_LVL, 105, S_LVL, 90, F_CUT, 55, F_RES, 40,
       F_ENV, 40, FE_D, 55, FE_S, 20, AE_A, 4, AE_S, 100, AE_R, 25),
    PR("CS DRIFT PAD", MS_VOL, 100, P_STYLE, 9, A_ENG, E_CS, A_P1, 40, A_P2, 100, A_P3, 110, A_LVL, 100, S_LVL, 40, F_CUT, 60, F_RES, 20,
       V_MODE, VM_POLY, AE_A, 60, AE_S, 100, AE_R, 70, RV_ON, 1, RV_SIZE, 110, RV_MIX, 50, DL_ON, 1, DL_MIX, 30, CH_ON, 1),
    /* 1.0.31: sixteen more, one per kind of modern bass: rolling, clean, bouncy, deep sub, and the newer engines */
    PR("ROLL GROWL", P_STYLE, 0, A_ENG, E_GROWL, A_P1, 55, A_P2, 70, A_P3, 60, A_LVL, 110, S_LVL, 90, F_CUT, 90, F_RES, 20,
       AE_D, 50, AE_S, 25, AE_R, 15, L1_SHP, 0, L1_SYNC, 1, L1_DIV, 9, L1_DEP, 30, L1_DST, D_AP2, L1_TRIG, 1, DR_ON, 1, DR_AMT, 35),
    PR("TECH ROLL FM", P_STYLE, 3, A_ENG, E_FM, A_P1, 35, A_P2, 16, A_P3, 20, A_P4, 16, A_LVL, 115, S_LVL, 80, F_TYPE, FT_LP24,
       F_CUT, 50, F_RES, 30, F_ENV, 50, FE_D, 30, FE_S, 0, AE_D, 45, AE_S, 10, AE_R, 12, DR_ON, 1, DR_AMT, 40, MS_GLUE, 60),
    PR("CLEAN PICK", MS_VOL, 100, P_STYLE, 7, A_ENG, E_ANALOG, A_P1, 75, A_LVL, 120, S_LVL, 70, F_CUT, 75, F_RES, 10, F_ENV, 25, FE_D, 40, FE_S, 30,
       AE_A, 0, AE_D, 70, AE_S, 60, AE_R, 20),
    PR("GLASS BASS", P_STYLE, 5, A_ENG, E_ADD, A_P1, 90, A_P2, 127, A_P4, 32, A_LVL, 115, S_LVL, 50, F_CUT, 110, F_RES, 5,
       AE_D, 65, AE_S, 40, AE_R, 25, CH_ON, 1, CH_DEP, 30, CH_MIX, 30),
    PR("BOUNCE SQUARE", MS_VOL, 127, P_STYLE, 5, A_ENG, E_ANALOG, A_P1, 127, A_P2, 30, A_LVL, 110, S_LVL, 70, F_TYPE, FT_LP24, F_CUT, 45, F_RES, 50,
       F_ENV, 70, FE_D, 25, FE_S, 0, AE_D, 60, AE_S, 25, AE_R, 15, V_MODE, VM_LEGATO, V_GLIDE, 15),
    PR("BOUNCY FM", MS_VOL, 109, P_STYLE, 10, A_ENG, E_FM, A_P1, 50, A_P2, 24, A_P3, 15, A_P4, 80, A_LVL, 115, S_LVL, 70, F_CUT, 85, F_RES, 20,
       ME_AMT, 20, ME_DST, D_PITCH, ME_D, 15, AE_D, 50, AE_S, 15, AE_R, 15),
    PR("DEEP SUB 30", P_STYLE, 1, A_ENG, E_ANALOG, A_P1, 0, A_LVL, 90, S_LVL, 127, F_CUT, 50, F_RES, 0, AE_S, 127, AE_R, 30,
       V_MODE, VM_LEGATO, V_GLIDE, 25, MS_SUB, 80),
    PR("SUB DRAWBAR", P_STYLE, 9, A_ENG, E_ADD, A_P1, 30, A_P2, 20, A_P4, 32, A_LVL, 120, S_LVL, 90, F_CUT, 70, F_RES, 0,
       AE_S, 110, AE_R, 30, MS_SUB, 60),
    PR("SNARL GROWL", MS_VOL, 77, P_STYLE, 2, A_ENG, E_GROWL, A_P1, 80, A_P2, 40, A_P3, 100, A_P4, 48, A_LVL, 110, S_LVL, 80, F_CUT, 100, F_RES, 20,
       L1_SHP, 0, L1_SYNC, 1, L1_DIV, 9, L1_DEP, 50, L1_DST, D_AP2, L1_TRIG, 1, DR_ON, 1, DR_TYPE, 2, DR_AMT, 45),
    PR("TALK GROWL", MS_VOL, 107, P_STYLE, 2, A_ENG, E_GROWL, A_P1, 65, A_P2, 30, A_P3, 80, A_P4, 80, A_LVL, 110, S_LVL, 85, F_CUT, 110, F_RES, 10,
       L1_SHP, 1, L1_SYNC, 1, L1_DIV, 6, L1_DEP, 60, L1_DST, D_AP2, L1_TRIG, 1, V_MODE, VM_LEGATO, V_GLIDE, 25),
    PR("SCREECH RIP", P_STYLE, 8, A_ENG, E_GROWL, A_P1, 90, A_P2, 60, A_P3, 110, A_P4, 96, A_LVL, 105, S_LVL, 70, F_CUT, 110, F_RES, 20,
       L1_SHP, 4, L1_SYNC, 1, L1_DIV, 11, L1_DEP, 35, L1_DST, D_AP1, L1_TRIG, 1, DR_ON, 1, DR_TYPE, 3, DR_AMT, 50, MS_GLUE, 80),
    PR("GOSPEL ORGAN", MS_VOL, 113, P_STYLE, 7, A_ENG, E_ADD, A_P1, 100, A_P2, 127, A_P4, 64, A_LVL, 110, S_LVL, 40, F_CUT, 120, F_RES, 0,
       AE_S, 100, AE_R, 30, V_MODE, VM_POLY, CH_ON, 1, CH_DEP, 50, CH_MIX, 40),
    PR("FM STACK", MS_VOL, 77, P_STYLE, 8, A_ENG, E_FM, A_P1, 70, A_P2, 40, A_P3, 50, A_P4, 16, A_LVL, 110, S_LVL, 80, F_CUT, 95, F_RES, 30,
       L1_SHP, 0, L1_SYNC, 1, L1_DIV, 9, L1_DEP, 40, L1_DST, D_AP1, L1_TRIG, 1, DR_ON, 1, DR_AMT, 40),
    PR("DX BASS", MS_VOL, 127, P_STYLE, 7, A_ENG, E_FM, A_P1, 55, A_P2, 16, A_P3, 30, A_P4, 64, A_LVL, 127, S_LVL, 60, F_CUT, 100, F_RES, 0,
       AE_D, 60, AE_S, 65, AE_R, 20),
    PR("808 SLIDE", P_STYLE, 6, A_ENG, E_KICK, A_P1, 50, A_P2, 80, A_P3, 60, A_P4, 10, A_LVL, 120, S_LVL, 0, F_CUT, 127, F_RES, 0,
       AE_D, 100, AE_S, 60, AE_R, 50, V_MODE, VM_LEGATO, V_GLIDE, 40),
    PR("CS ROLLER", P_STYLE, 0, A_ENG, E_CS, A_P1, 0, A_P2, 60, A_P3, 40, A_LVL, 110, S_LVL, 85, F_TYPE, FT_LP24, F_CUT, 45, F_RES, 30,
       F_ENV, 50, FE_D, 35, FE_S, 0, AE_D, 50, AE_S, 30, AE_R, 15, DR_ON, 1, DR_AMT, 35),
    /* 1.0.32: sixteen more on the newer engines, each its own: vowels, drawbars, 4-op FM, 808 layers */
    PR("VOWEL WOBBLE", MS_VOL, 92, P_STYLE, 2, A_ENG, E_GROWL, A_P1, 60, A_P2, 20, A_P3, 70, B_ENG, E_ANALOG, B_P1, 85, B_LVL, 35,
       S_LVL, 85, F_CUT, 110, L1_SHP, 0, L1_SYNC, 1, L1_DIV, 9, L1_DEP, 70, L1_DST, D_AP2, L1_TRIG, 1, DR_ON, 1, DR_AMT, 30),
    PR("HOLLOW GROWL", MS_VOL, 82, P_STYLE, 8, A_ENG, E_GROWL, A_P1, 60, A_P2, 90, A_P3, 50, A_P4, 16, S_LVL, 85, F_CUT, 110,
       L2_SHP, 1, L2_SYNC, 1, L2_DIV, 3, L2_DEP, 50, L2_DST, D_AP1, DR_ON, 1, DR_TYPE, 1, DR_AMT, 40),
    PR("SUB GROWL DNB", MS_VOL, 127, P_STYLE, 0, A_ENG, E_GROWL, A_P1, 45, A_P2, 20, A_P3, 40, A_P4, 32, S_LVL, 110, F_CUT, 95,
       AE_D, 60, AE_S, 70, AE_R, 15, V_MODE, VM_LEGATO, V_GLIDE, 25),
    PR("METAL YOY", MS_VOL, 80, P_STYLE, 8, A_ENG, E_GROWL, A_P1, 75, A_P2, 60, A_P3, 90, A_P4, 64, S_LVL, 75, F_CUT, 115,
       L1_SHP, 1, L1_SYNC, 1, L1_DIV, 11, L1_DEP, 55, L1_DST, D_AP2, L1_TRIG, 1, DR_ON, 1, DR_TYPE, 2, DR_AMT, 45, MS_GLUE, 70),
    PR("TALKBOX BASS", MS_VOL, 127, P_STYLE, 5, A_ENG, E_GROWL, A_P1, 50, A_P2, 10, A_P3, 75, A_P4, 80, S_LVL, 70, F_CUT, 115,
       ME_AMT, 60, ME_DST, D_AP2, ME_A, 10, ME_D, 55, AE_D, 60, AE_S, 80, AE_R, 15),
    PR("REESE DRAWBAR", MS_VOL, 62, P_STYLE, 0, A_ENG, E_ADD, A_P1, 110, A_P2, 127, B_ENG, E_ADD, B_P1, 110, B_P2, 127, B_LVL, 100, B_FINE, 14,
       S_LVL, 80, F_TYPE, FT_LP24, F_CUT, 55, F_RES, 30, DR_ON, 1, DR_AMT, 35, PH_ON, 1, PH_RATE, 20, PH_DEP, 60),
    PR("HOLLOW ORGAN", MS_VOL, 127, P_STYLE, 9, A_ENG, E_ADD, A_P1, 90, A_P2, 127, A_P4, 80, S_LVL, 60, F_CUT, 100, F_RES, 20, AE_D, 70, AE_S, 80,
       V_MODE, VM_POLY, DL_ON, 1, DL_TYPE, 3, DL_TIME, 7, DL_FB, 70, DL_MIX, 35, RV_ON, 1, RV_SIZE, 110, RV_MIX, 35),
    PR("FLUTE SUB", P_STYLE, 1, A_ENG, E_ADD, A_P1, 40, A_P2, 30, A_P4, 96, S_LVL, 100, F_CUT, 70, AE_A, 10, AE_S, 120, AE_R, 35,
       V_MODE, VM_LEGATO, V_GLIDE, 30),
    PR("CHURCH BASS", P_STYLE, 3, A_ENG, E_ADD, A_P1, 80, A_P2, 127, A_P4, 112, S_LVL, 70, F_CUT, 85, F_RES, 25, F_ENV, 30, FE_D, 50,
       AE_D, 70, AE_S, 60, DR_ON, 1, DR_AMT, 25),
    PR("PERC ORGAN", MS_VOL, 127, P_STYLE, 7, A_ENG, E_ADD, A_P1, 100, A_P2, 127, A_P3, 30, A_P4, 16, S_LVL, 60, F_CUT, 120,
       AE_D, 35, AE_S, 70, AE_R, 15, CH_ON, 1, CH_DEP, 40, CH_MIX, 35),
    PR("FM KNOCK", MS_VOL, 127, P_STYLE, 3, A_ENG, E_FM, A_P1, 60, A_P2, 48, A_P3, 10, A_P4, 96, S_LVL, 85, F_CUT, 100,
       ME_AMT, 30, ME_DST, D_PITCH, ME_D, 12, AE_D, 35, AE_S, 0, AE_R, 12, DR_ON, 1, DR_AMT, 40),
    PR("FM ORGAN BASS", MS_VOL, 110, P_STYLE, 7, A_ENG, E_FM, A_P1, 90, A_P2, 32, A_P3, 20, A_P4, 112, S_LVL, 70, F_CUT, 110,
       AE_D, 60, AE_S, 70, AE_R, 20),
    PR("NEURO STACK", MS_VOL, 76, P_STYLE, 8, A_ENG, E_FM, A_P1, 85, A_P2, 40, A_P3, 70, A_P4, 32, S_LVL, 80, F_CUT, 100, F_RES, 30,
       L1_SHP, 0, L1_SYNC, 1, L1_DIV, 9, L1_DEP, 45, L1_DST, D_AP1, L1_TRIG, 1, DR_ON, 1, DR_TYPE, 2, DR_AMT, 45),
    PR("808 GROWL", MS_VOL, 110, P_STYLE, 6, A_ENG, E_KICK, A_P1, 55, A_P2, 60, A_P3, 50, A_P4, 15, B_ENG, E_GROWL, B_P1, 60, B_P2, 40,
       B_P3, 70, B_LVL, 60, S_LVL, 0, F_CUT, 120, AE_D, 100, AE_S, 60, AE_R, 40, V_MODE, VM_LEGATO, V_GLIDE, 30),
    PR("808 FOLDED", P_STYLE, 6, A_ENG, E_KICK, A_P1, 65, A_P2, 70, A_P3, 80, A_P4, 25, S_LVL, 0, F_CUT, 110, F_RES, 20,
       AE_D, 95, AE_S, 50, AE_R, 40, DR_ON, 1, DR_TYPE, 3, DR_AMT, 55),
    PR("CS VOX", MS_VOL, 115, P_STYLE, 2, A_ENG, E_CS, A_P1, 0, A_P2, 40, A_P3, 40, B_ENG, E_GROWL, B_P1, 55, B_P2, 50, B_P3, 70,
       B_P4, 80, B_LVL, 80, S_LVL, 75, F_CUT, 90, F_RES, 25, L1_SHP, 0, L1_SYNC, 1, L1_DIV, 6, L1_DEP, 50, L1_DST, D_BP2, L1_TRIG, 1),
    /* (1.0.50: ACID LINE with the mod envelope on the cutoff, a slow swell: 806 ms up, 26 ms down, +53 %; the cutoff at 1.1 kHz) */
    PR("aCiD StUff", MS_VOL, 77, P_STYLE, 3, A_ENG, E_ANALOG, A_P1, 85, S_LVL, 40, F_TYPE, FT_LP24, F_CUT, 75, F_RES, 100,
       F_ENV, 50, FE_D, 45, FE_S, 0, AE_D, 50, AE_S, 80, V_MODE, VM_LEGATO, V_GLIDE, 25, DR_ON, 1, DR_TYPE, 1,
       DR_AMT, 50, DL_ON, 1, DL_TIME, 10, DL_MIX, 20, ME_A, 93, ME_D, 50, ME_AMT, 34, ME_DST, D_CUT),
};
#define NFACTORY (sizeof FACTORY / sizeof FACTORY[0])

static void patch_defaults(int8_t *p)
{
    uint32_t i;
    for (i = 0; i < NP_MAX; i++)
        p[i] = i < NP ? PD[i].def : 0;
    p[B_LVL] = 0;
    p[B_ENG] = E_CS;
    p[L2_SYNC] = 0;
    p[L2_SHP] = 1;
}


static void patch_sanitize(int8_t *p)
{
    uint32_t i;
    for (i = 0; i < NP; i++)
        p[i] = (int8_t)clampi(p[i], PD[i].lo, PD[i].hi);
    for (i = 0; i < 2u; i++) {                  /* (1.0.26: a sound saved with FOLD plays ANALOG with its FOLD up; VOCAL plays FM) */
        int8_t *e = p + A_ENG + i * OSC_B;
        if (*e == E_GONE5) { *e = E_ANALOG; if (!p[A_P4 + i]) p[A_P4 + i] = 64; }
        else if (*e == E_GONE7) *e = E_FM;
    }
}

static void preset_factory(uint32_t k, int8_t *p, char *name)
{
    const int8_t *o;
    uint32_t i;
    if (k >= NFACTORY) k = 0;
    patch_defaults(p);
    for (o = FACTORY[k].ov; (uint8_t)o[0] != 0xFFu; o += 2)    /* (the ids from 128 up, A_P4 and B_P4, are negative as int8_t: 1.0.49, the list read on past them) */
        if ((uint8_t)o[0] < NP_MAX) p[(uint8_t)o[0]] = o[1];
    patch_sanitize(p);
    for (i = 0; i < 13u && FACTORY[k].name[i]; i++)
        name[i] = FACTORY[k].name[i];
    name[i] = 0;
}
