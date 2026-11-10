// SPDX-License-Identifier: GPL-3.0-only
// The K-WASUB firmware's parameter ids, copied from kwasub/firmware/src/ksub_data.c (keep in step when the firmware
// is updated: the order is the firmware's stored order).
#pragma once

namespace kwid {
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
/* the engines, and the order the ENGINE field walks (ksub_data.c: ENG_ORDER) */
enum { E_ANALOG, E_CS, E_WAVE, E_FM, E_PHASE, E_GONE5, E_SYNC, E_GONE7, E_KICK, E_GROWL, E_ADD, NENG };
static const int ENG_ORDER[9] = {E_ANALOG, E_KICK, E_CS, E_WAVE, E_FM, E_GROWL, E_ADD, E_PHASE, E_SYNC};
static const char* const ENG_NAMES[NENG] = {"ANALOG", "CS", "WAVETABLE", "FM", "PHASE DIST", "-", "HARD SYNC", "-",
                                            "808", "GROWL", "ADDITIVE"};
static const char* const FILTER_NAMES[7] = {"LP 24", "LP 12", "BAND", "HIGH", "NOTCH", "VOWEL", "COMB"};
static const char* const FILTER_INFO[7] = {"LADDER", "LADDER", "BAND PASS", "HIGH PASS", "NOTCH", "FORMANT", "COMB"};
enum { EQ_OFF, EQ_LOCUT, EQ_LOSH, EQ_BELL, EQ_NOTCH, EQ_HISH, EQ_HICUT };
}  // namespace kwid
