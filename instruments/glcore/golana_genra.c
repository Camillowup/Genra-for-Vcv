/* SPDX-License-Identifier: GPL-3.0-only
 * GOLANA for VCV Rack: GENRA's own presets, by style (as FUNKE's styles), written as golana_factory.c writes Golana's
 * factory sounds: the values that differ from INIT (golana_prog.c gl_defaults). Loud and punchy; the module evens
 * their levels (src/golana/GolanaLevels.hpp, from test/glcore_test.c).
 * Units (golana_factory.c): time 40 ~ 20 ms, 60 ~ 80 ms, 80 ~ 0.3 s, 90 ~ 0.7 s, 100 ~ 1.4 s, 110 ~ 3 s;
 * LFO FREQ 20 ~ 0.15 Hz, 40 ~ 0.4 Hz, 60 ~ 1.2 Hz, 85 ~ 4.4 Hz; DTIME 0 1/4, 1 1/8, 2 1/16, 4 1/8T, 10 1/8D, 12 1/4D;
 * P_ARATE 0 1/4, 1 1/8, 2 1/16, 3 1/32; LFO SYNC 5 1/4, 6 1/8, 7 1/8T, 8 1/16. */
#define S(i, v) {(uint8_t)(i), (int16_t)(v)}
#define PSRC(k) (GL_P1SRC + 3 * (k))
#define PDST(k) (GL_P1DST + 3 * (k))
#define PINT(k) (GL_P1INT + 3 * (k))
#define F_MS20 4

typedef struct { const char *style; gl_prog_t p; } gl_genra_t;
#define G(st, n, a) {st, {n, a, (uint8_t)NELEM(a)}}

/* ------------------------------------------------------------------ SOUL · JAZZ · LO-FI --- */
static const gl_set_t G_POLYKEYS[] = {          /* a soft pulse and a triangle, a short filter blip: an electric piano */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 12), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 12), S(GL_MIX2, 55),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 62), S(GL_RESO, 10), S(GL_FEGINT, 30), S(GL_KTRK, 40),
    S(GL_FA, 0), S(GL_FD, 82), S(GL_FS, 20), S(GL_FR, 75),
    S(P_ATK, 0), S(P_DEC, 98), S(P_SUS, 55), S(P_REL, 78), S(P_LEVEL, 118),
    S(GL_MFX, 1), S(P_IA, 30), S(P_IB, 55), S(P_IMIX, 80), S(GL_RTYPE, 0), S(GL_RSIZE, 70), S(P_REV, 22),
};
static const gl_set_t G_LOFICHORDS[] = {        /* a worn pair of saws, far down the filter, drifting like old tape */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 12), S(GL_MIX2, 60), S(GL_DRIFT, 95),
    S(GL_SUBTYPE, U_SIN1), S(GL_SUB, 50), S(GL_NOISE, 10),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 52), S(GL_RESO, 12), S(GL_FEGINT, 18), S(GL_FD, 92), S(GL_FS, 40),
    S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 30), S(GL_L2KEY, 0), S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 3),
    S(P_ATK, 6), S(P_DEC, 100), S(P_SUS, 80), S(P_REL, 88), S(P_LEVEL, 120),
    S(GL_MFX, 1), S(P_IA, 18), S(P_IB, 60), S(P_IMIX, 70), S(GL_RTYPE, 0), S(GL_RSIZE, 80), S(P_REV, 30),
};
static const gl_set_t G_SOFTSQUARE[] = {        /* a round square lead, a late gentle vibrato, a little glide */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 0), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, -12),
    S(GL_MIX2, 40), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 66), S(GL_RESO, 18), S(GL_FEGINT, 14), S(GL_FD, 85),
    S(GL_FS, 70), S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 86), S(GL_L2KEY, 2),
    S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 12), S(P_SUS, 120), S(P_REL, 66), S(P_GLIDE, 30), S(P_LEVEL, 118),
    S(GL_DTYPE, 0), S(GL_DTIME, 10), S(GL_DFDBK, 35), S(P_DLY, 28), S(GL_RTYPE, 0), S(P_REV, 25),
};
static const gl_set_t G_DUSTYBELLS[] = {        /* sines rung against each other, dull and close, a short echo */
    S(GL_O1WAVE, W_SINE), S(GL_O1C1, 24), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 24), S(GL_O2TUNE, 8), S(GL_MIX2, 10),
    S(GL_DRIFT, 40), S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 78), S(GL_RESO, 0),
    S(GL_FA, 0), S(GL_FD, 90), S(GL_FS, 0), S(GL_FR, 90),
    S(PSRC(1), Q_FEG), S(PDST(1), D_CTRL1), S(PINT(1), 35),
    S(P_ATK, 0), S(P_DEC, 100), S(P_SUS, 0), S(P_REL, 95), S(P_LEVEL, 124),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 40), S(P_DLY, 32), S(GL_RTYPE, 0), S(GL_RSIZE, 85), S(P_REV, 30),
};

/* ------------------------------------------------------------------------ HOUSE · GARAGE --- */
static const gl_set_t G_HOUSEORGAN[] = {        /* the M1-style organ: square and octave, a quick pluck of the filter */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 0), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 12), S(GL_MIX2, 100),
    S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 45),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 76), S(GL_RESO, 22), S(GL_FEGINT, 28), S(GL_KTRK, 40),
    S(GL_FA, 0), S(GL_FD, 70), S(GL_FS, 30), S(GL_FR, 55),
    S(P_ATK, 0), S(P_DEC, 80), S(P_SUS, 60), S(P_REL, 52), S(P_LEVEL, 120),
    S(GL_MFX, 1), S(P_IA, 60), S(P_IB, 45), S(P_IMIX, 70), S(GL_RTYPE, 0), S(GL_RSIZE, 70), S(P_REV, 22),
};
static const gl_set_t G_DEEPCHORD[] = {         /* a soft saw chord muffled by the filter, a dotted echo and a room */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 8), S(GL_MIX2, 90), S(GL_DRIFT, 20),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 50), S(GL_RESO, 30), S(GL_FEGINT, 26), S(GL_KTRK, 30),
    S(GL_FA, 0), S(GL_FD, 78), S(GL_FS, 15), S(GL_FR, 70),
    S(P_ATK, 0), S(P_DEC, 88), S(P_SUS, 30), S(P_REL, 72), S(P_LEVEL, 122),
    S(GL_MFX, 3), S(P_IA, 25), S(P_IB, 60), S(P_IMIX, 70),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 50), S(P_DLY, 38), S(GL_RTYPE, 2), S(GL_RSIZE, 95), S(P_REV, 35),
};
static const gl_set_t G_GARAGESTAB[] = {        /* a short, biting saw stab through the MS20 filter */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_MIX), S(GL_O2SEMI, 12), S(GL_MIX2, 70),
    S(GL_FTYPE, F_MS20), S(GL_CUTOFF, 58), S(GL_RESO, 60), S(GL_FEGINT, 36), S(GL_KTRK, 30),
    S(GL_FA, 0), S(GL_FD, 66), S(GL_FS, 0), S(GL_FR, 50),
    S(P_ATK, 0), S(P_DEC, 74), S(P_SUS, 0), S(P_REL, 50), S(P_LEVEL, 120), S(GL_DIST, 1),
    S(GL_DTYPE, 1), S(GL_DTIME, 2), S(GL_DFDBK, 30), S(P_DLY, 25), S(GL_RTYPE, 0), S(P_REV, 18),
};
static const gl_set_t G_DISCOBRASS[] = {        /* brassy saws swelling open, a fast chorus: a disco section */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 12), S(GL_MIX2, 110),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 52), S(GL_RESO, 15), S(GL_FEGINT, 42), S(GL_KTRK, 40),
    S(GL_FA, 58), S(GL_FD, 80), S(GL_FS, 60), S(GL_FR, 62),
    S(P_ATK, 30), S(P_DEC, 85), S(P_SUS, 112), S(P_REL, 58), S(P_LEVEL, 118),
    S(GL_MFX, 1), S(P_IA, 55), S(P_IB, 50), S(P_IMIX, 80), S(GL_RTYPE, 0), S(P_REV, 25),
};

/* -------------------------------------------------------------------------- DUB · REGGAE --- */
static const gl_set_t G_DUBSIREN[] = {          /* the sound system siren: a square, its pitch swept by LFO 2 */
    S(P_VOICE, V_MONO), S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 10), S(GL_MIX1, 127),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 92), S(GL_RESO, 25),
    S(GL_L2WAVE, L_TRI), S(GL_L2FREQ, 70), S(GL_L2KEY, 2), S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 40),
    S(GL_L1WAVE, L_SAW), S(GL_L1FREQ, 25), S(GL_L1KEY, 2), S(PSRC(3), Q_LFO1), S(PDST(3), D_LFO2F), S(PINT(3), -30),
    S(P_ATK, 4), S(P_SUS, 127), S(P_REL, 70), S(P_LEVEL, 110),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 85), S(P_DLY, 70), S(GL_RTYPE, 1), S(GL_RSIZE, 90), S(P_REV, 40),
};
static const gl_set_t G_DUBCHORD[] = {          /* the off-beat skank: a short chord into a long, dark echo */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 30), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 12), S(GL_MIX2, 60),
    S(GL_FTYPE, F_BPF), S(GL_CUTOFF, 70), S(GL_RESO, 30), S(GL_FEGINT, 20),
    S(GL_FA, 0), S(GL_FD, 62), S(GL_FS, 0), S(GL_FR, 50),
    S(P_ATK, 0), S(P_DEC, 70), S(P_SUS, 0), S(P_REL, 48), S(P_LEVEL, 124),
    S(GL_DTYPE, 1), S(GL_DTIME, 12), S(GL_DFDBK, 92), S(P_DLY, 75), S(GL_RTYPE, 1), S(GL_RSIZE, 95), S(P_REV, 45),
};
static const gl_set_t G_MELODICA[] = {          /* a reedy pulse with a breath of noise, played one note at a time */
    S(P_VOICE, V_MONO), S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 45), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 12),
    S(GL_O2TUNE, 6), S(GL_MIX2, 35), S(GL_NOISE, 22),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 74), S(GL_RESO, 15), S(GL_FEGINT, 10), S(GL_FA, 50), S(GL_FS, 100),
    S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 84), S(GL_L2KEY, 2), S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 40), S(P_SUS, 118), S(P_REL, 55), S(P_LEVEL, 114),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 70), S(P_DLY, 50), S(GL_RTYPE, 1), S(P_REV, 35),
};

/* ---------------------------------------------------------------------- TECHNO · ELECTRO --- */
static const gl_set_t G_ACIDLINE[] = {          /* Golana's ACID LINE, harder: the resonance up, DIST on, a short echo */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_MIX1, 127),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 32), S(GL_RESO, 112), S(GL_FEGINT, 50), S(GL_KTRK, 30),
    S(GL_FA, 0), S(GL_FD, 66), S(GL_FS, 0), S(GL_FR, 55), S(GL_EGRESET, 1), S(GL_DIST, 1),
    S(P_ATK, 0), S(P_DEC, 90), S(P_SUS, 127), S(P_REL, 40), S(P_GLIDE, 25), S(P_LEVEL, 108),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 30),
};
static const gl_set_t G_HYPNOARP[] = {          /* a 16th arp of plucks, LFO 1 (on the bar) sweeping the cutoff */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 7), S(GL_MIX2, 50),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 48), S(GL_RESO, 70), S(GL_FEGINT, 30), S(GL_FD, 64), S(GL_FS, 0),
    S(GL_L1WAVE, L_TRI), S(GL_L1SYNC, 2), S(GL_L1KEY, 0), S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 28),
    S(P_ATK, 0), S(P_DEC, 72), S(P_SUS, 0), S(P_REL, 50), S(P_LEVEL, 120),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 6), S(P_AOCT, 2), S(P_ARATE, 2), S(P_AGATE, 60),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 50), S(P_DLY, 40), S(GL_RTYPE, 0), S(P_REV, 20),
};
static const gl_set_t G_DUBTECHNO[] = {         /* the Basic Channel chord: dark, short, drowned in echoes */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 7), S(GL_O2TUNE, 6), S(GL_MIX2, 80),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 44), S(GL_RESO, 40), S(GL_FEGINT, 24),
    S(GL_FA, 0), S(GL_FD, 72), S(GL_FS, 10), S(GL_FR, 66),
    S(GL_L1WAVE, L_SIN), S(GL_L1FREQ, 22), S(GL_L1KEY, 0), S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 10),
    S(P_ATK, 0), S(P_DEC, 82), S(P_SUS, 15), S(P_REL, 70), S(P_LEVEL, 124),
    S(GL_MFX, 3), S(P_IA, 20), S(P_IB, 70), S(P_IMIX, 60),
    S(GL_DTYPE, 2), S(GL_DTIME, 10), S(GL_DFDBK, 88), S(P_DLY, 72), S(GL_RTYPE, 2), S(GL_RSIZE, 112), S(P_REV, 50),
};
static const gl_set_t G_ELECTROSYNC[] = {       /* OSC 2 synced and swept by the filter EG: the electro scream */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_MIX1, 40), S(GL_O2WAVE, X_SAW), S(GL_O2MOD, 2), S(GL_O2SEMI, 7),
    S(GL_MIX2, 127), S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 88), S(GL_RESO, 25),
    S(GL_FA, 0), S(GL_FD, 80), S(GL_FS, 20), S(GL_FR, 60),
    S(PSRC(1), Q_FEG), S(PDST(1), D_OSC2), S(PINT(1), 48),
    S(P_ATK, 0), S(P_SUS, 120), S(P_REL, 50), S(P_GLIDE, 15), S(P_LEVEL, 112), S(GL_DIST, 1),
    S(GL_DTYPE, 1), S(GL_DTIME, 2), S(GL_DFDBK, 40), S(P_DLY, 30),
};
static const gl_set_t G_MINIMALBLIP[] = {       /* a tiny sine blip, bounced left and right */
    S(GL_O1WAVE, W_SINE), S(GL_O1C1, 30), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 12), S(GL_MIX2, 0),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 100),
    S(GL_FA, 0), S(GL_FD, 55), S(GL_FS, 0), S(PSRC(1), Q_FEG), S(PDST(1), D_CTRL1), S(PINT(1), 40),
    S(P_ATK, 0), S(P_DEC, 62), S(P_SUS, 0), S(P_REL, 52), S(P_LEVEL, 127),
    S(GL_DTYPE, 2), S(GL_DTIME, 4), S(GL_DFDBK, 60), S(P_DLY, 55), S(GL_RTYPE, 0), S(P_REV, 15),
};

/* --------------------------------------------------------------- GRIME · DUBSTEP · DRILL --- */
static const gl_set_t G_GRIMESQUARE[] = {       /* the hollow square of eskibeat, cold and dry */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 0), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, -12), S(GL_MIX2, 70),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 78), S(GL_RESO, 15), S(GL_FEGINT, 12), S(GL_FD, 70), S(GL_FS, 80),
    S(P_ATK, 0), S(P_DEC, 80), S(P_SUS, 100), S(P_REL, 40), S(P_LEVEL, 116),
    S(GL_RTYPE, 0), S(GL_RSIZE, 60), S(P_REV, 12),
};
static const gl_set_t G_REESE[] = {             /* Golana's REESE: two saws beating over a sine an octave down */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 18), S(GL_MIX2, 120),
    S(GL_SUBTYPE, U_SIN1), S(GL_SUB, 105), S(GL_DRIFT, 30),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 54), S(GL_RESO, 25), S(GL_FEGINT, 12), S(GL_FD, 90), S(GL_FS, 70),
    S(P_ATK, 4), S(P_SUS, 124), S(P_REL, 55), S(P_GLIDE, 20), S(P_LEVEL, 108),
    S(GL_MFX, 4), S(P_IA, 20), S(P_IB, 70), S(P_IMIX, 70),
};
static const gl_set_t G_DRILLGLIDE[] = {        /* a dark sine lead sliding from note to note */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SINE), S(GL_O1C1, 8), S(GL_SUBTYPE, U_SIN1), S(GL_SUB, 50),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 90),
    S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 84), S(GL_L2KEY, 2), S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 6), S(P_SUS, 124), S(P_REL, 70), S(P_GLIDE, 62), S(P_LEVEL, 124),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 35), S(GL_RTYPE, 2), S(GL_RSIZE, 100), S(P_REV, 35),
};
static const gl_set_t G_WOBBLELEAD[] = {        /* LFO 1 on the tempo opens and shuts the filter: patch RETRIG for the drop */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, -12), S(GL_O2TUNE, 10),
    S(GL_MIX2, 110), S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 70), S(GL_FOLD, 20),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 30), S(GL_RESO, 55),
    S(GL_L1WAVE, L_SIN), S(GL_L1SYNC, 7), S(GL_L1KEY, 2), S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 50),
    S(P_ATK, 0), S(P_SUS, 127), S(P_REL, 40), S(P_GLIDE, 10), S(P_LEVEL, 106), S(GL_DIST, 1),
};

/* -------------------------------------------------------------- POP · SYNTHWAVE · TRANCE --- */
static const gl_set_t G_SUPERSAW[] = {          /* Golana's SUPERSAW: every voice on the note, spread wide */
    S(P_VOICE, V_UNISON), S(P_DETUNE, 88), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 11), S(GL_MIX2, 120),
    S(GL_SUBTYPE, U_SAW1), S(GL_SUB, 35), S(GL_DRIFT, 20),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 108), S(GL_RESO, 10), S(GL_FEGINT, 8), S(GL_FD, 90), S(GL_FS, 100),
    S(P_ATK, 4), S(P_SUS, 124), S(P_REL, 72), S(P_GLIDE, 18), S(P_LEVEL, 96), S(P_SPRD, 127),
    S(GL_MFX, 3), S(P_IA, 28), S(P_IB, 80), S(P_IMIX, 100),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 40), S(GL_RTYPE, 2), S(GL_RSIZE, 110), S(P_REV, 45),
};
static const gl_set_t G_80SBRASS[] = {          /* Golana's 80S BRASS, a touch brighter */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 9), S(GL_MIX2, 110),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 54), S(GL_RESO, 12), S(GL_FEGINT, 42), S(GL_KTRK, 40),
    S(GL_FA, 60), S(GL_FD, 85), S(GL_FS, 70), S(GL_FR, 70),
    S(P_ATK, 45), S(P_DEC, 90), S(P_SUS, 112), S(P_REL, 65), S(P_LEVEL, 112),
    S(GL_MFX, 3), S(P_IA, 30), S(P_IB, 50), S(P_IMIX, 70), S(GL_RTYPE, 2), S(P_REV, 28),
};
static const gl_set_t G_SYNTHWAVE[] = {         /* a PWM pulse on a sub square, the Juno chorus, a big hall */
    S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 35), S(GL_O1C2, 40), S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 45), S(GL_L1KEY, 0),
    S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 7), S(GL_MIX2, 70), S(GL_SUBTYPE, U_SQR1), S(GL_SUB, 60), S(GL_DRIFT, 20),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 70), S(GL_RESO, 15), S(GL_FEGINT, 24), S(GL_KTRK, 30),
    S(GL_FA, 6), S(GL_FD, 88), S(GL_FS, 50), S(GL_FR, 80),
    S(P_ATK, 8), S(P_DEC, 90), S(P_SUS, 112), S(P_REL, 80), S(P_LEVEL, 112),
    S(GL_MFX, 1), S(P_IA, 35), S(P_IB, 85), S(P_IMIX, 120),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 30), S(GL_RTYPE, 2), S(GL_RSIZE, 108), S(P_REV, 40),
};
static const gl_set_t G_GLIDELEAD[] = {         /* Golana's GLIDE LEAD: PWM and a fifth above, portamento */
    S(P_VOICE, V_LEGATO), S(GL_O1WAVE, W_PULSE), S(GL_O1C1, 35), S(GL_O1C2, 40),
    S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 7), S(GL_O2TUNE, 6), S(GL_MIX2, 70),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 84), S(GL_RESO, 30), S(GL_FEGINT, 18), S(GL_FD, 80), S(GL_FS, 60),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 45), S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 86), S(GL_L2KEY, 2),
    S(PSRC(0), Q_LFO2), S(PDST(0), D_PITCH), S(PINT(0), 2),
    S(P_ATK, 10), S(P_SUS, 120), S(P_REL, 55), S(P_GLIDE, 55), S(P_LEVEL, 108),
    S(GL_DTYPE, 1), S(GL_DTIME, 1), S(GL_DFDBK, 55), S(P_DLY, 45), S(GL_RTYPE, 2), S(P_REV, 25),
};
static const gl_set_t G_TRANCEPLUCK[] = {       /* a bright pluck, the arp up and down over two octaves, echoes */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, 12), S(GL_O2TUNE, 8), S(GL_MIX2, 80),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 56), S(GL_RESO, 35), S(GL_FEGINT, 40), S(GL_FD, 66), S(GL_FS, 0),
    S(P_ATK, 0), S(P_DEC, 74), S(P_SUS, 0), S(P_REL, 60), S(P_LEVEL, 122),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 2), S(P_AOCT, 2), S(P_ARATE, 2), S(P_AGATE, 70),
    S(GL_MFX, 3), S(P_IA, 25), S(P_IB, 60), S(P_IMIX, 60),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 55), S(P_DLY, 48), S(GL_RTYPE, 2), S(GL_RSIZE, 105), S(P_REV, 40),
};
static const gl_set_t G_UNISONLEAD[] = {        /* every voice on one note, saw and square, wide: a lead for the drop */
    S(P_VOICE, V_UNISON), S(P_DETUNE, 60), S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SQR), S(GL_O2SEMI, 12),
    S(GL_MIX2, 70), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 80), S(GL_RESO, 25), S(GL_FEGINT, 20), S(GL_FD, 82),
    S(GL_FS, 70), S(P_ATK, 2), S(P_SUS, 122), S(P_REL, 60), S(P_GLIDE, 22), S(P_LEVEL, 104), S(P_SPRD, 110),
    S(GL_DTYPE, 1), S(GL_DTIME, 10), S(GL_DFDBK, 45), S(P_DLY, 35), S(GL_RTYPE, 2), S(P_REV, 30),
};

/* ------------------------------------------------------------------------------ AMBIENT --- */
static const gl_set_t G_SOFTPAD[] = {           /* Golana's SOFT PAD: triangles, a breathing filter, a long hall */
    S(GL_O1WAVE, W_TRI), S(GL_O1C1, 12), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 10), S(GL_MIX2, 60),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 60), S(GL_RESO, 15), S(GL_FEGINT, 14), S(GL_KTRK, 25),
    S(GL_FA, 95), S(GL_FD, 105), S(GL_FS, 90), S(GL_FR, 100),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 30), S(GL_L1KEY, 0), S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 8),
    S(P_ATK, 92), S(P_DEC, 100), S(P_SUS, 120), S(P_REL, 105), S(P_LEVEL, 116),
    S(GL_MFX, 3), S(P_IA, 25), S(P_IB, 85), S(P_IMIX, 100),
    S(GL_RTYPE, 2), S(GL_RSIZE, 110), S(GL_RDAMP, 70), S(P_REV, 60),
};
static const gl_set_t G_PHASEPAD[] = {          /* Golana's PHASE PAD: two saws through a slow phaser */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2TUNE, 7), S(GL_MIX2, 100),
    S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 68), S(GL_RESO, 18), S(GL_FEGINT, 8), S(GL_FA, 90), S(GL_FD, 100),
    S(GL_FS, 90), S(GL_FR, 100),
    S(P_ATK, 88), S(P_DEC, 100), S(P_SUS, 118), S(P_REL, 102), S(P_LEVEL, 108),
    S(GL_MFX, 4), S(P_IA, 18), S(P_IB, 110), S(P_IMIX, 127), S(GL_RTYPE, 2), S(GL_RSIZE, 105), S(P_REV, 50),
};
static const gl_set_t G_SLOWARP[] = {           /* a half-note arp of glassy sines into a huge hall */
    S(GL_O1WAVE, W_SINE), S(GL_O1C1, 20), S(GL_O2WAVE, X_TRI), S(GL_O2SEMI, 19), S(GL_MIX2, 40),
    S(GL_FTYPE, F_LPF12), S(GL_CUTOFF, 84), S(GL_RESO, 8),
    S(P_ATK, 30), S(P_DEC, 100), S(P_SUS, 60), S(P_REL, 100), S(P_LEVEL, 120),
    S(GL_ARPON, 1), S(GL_ARPTYPE, 7), S(P_AOCT, 3), S(P_ARATE, 0), S(P_AGATE, 100),
    S(GL_DTYPE, 1), S(GL_DTIME, 12), S(GL_DFDBK, 70), S(P_DLY, 55),
    S(GL_RTYPE, 2), S(GL_RSIZE, 124), S(GL_RDAMP, 60), S(P_REV, 80),
};
static const gl_set_t G_DRONE[] = {             /* folded saws and noise, two slow LFOs: it never ends */
    S(GL_O1WAVE, W_SAW), S(GL_O2WAVE, X_SAW), S(GL_O2SEMI, -12), S(GL_O2TUNE, 5), S(GL_MIX2, 110), S(GL_FOLD, 30),
    S(GL_NOISE, 35), S(GL_DRIFT, 80), S(GL_FTYPE, F_LPF24), S(GL_CUTOFF, 42), S(GL_RESO, 65),
    S(GL_L1WAVE, L_TRI), S(GL_L1FREQ, 14), S(GL_L1KEY, 0), S(GL_L2WAVE, L_SIN), S(GL_L2FREQ, 20),
    S(PSRC(2), Q_LFO1), S(PDST(2), D_CUT), S(PINT(2), 22), S(PSRC(3), Q_LFO2), S(PDST(3), D_NOISE), S(PINT(3), 20),
    S(P_ATK, 100), S(P_DEC, 100), S(P_SUS, 127), S(P_REL, 110), S(P_LEVEL, 112),
    S(GL_RTYPE, 2), S(GL_RSIZE, 124), S(GL_RDAMP, 80), S(P_REV, 70),
};

static const gl_genra_t GL_GENRA[] = {
    G("SOUL · JAZZ · LO-FI", "WARM POLY KEYS", G_POLYKEYS),
    G("SOUL · JAZZ · LO-FI", "LO-FI CHORDS", G_LOFICHORDS),
    G("SOUL · JAZZ · LO-FI", "SOFT SQUARE LEAD", G_SOFTSQUARE),
    G("SOUL · JAZZ · LO-FI", "DUSTY BELLS", G_DUSTYBELLS),
    G("HOUSE · GARAGE", "HOUSE ORGAN", G_HOUSEORGAN),
    G("HOUSE · GARAGE", "DEEP CHORD", G_DEEPCHORD),
    G("HOUSE · GARAGE", "GARAGE STAB", G_GARAGESTAB),
    G("HOUSE · GARAGE", "DISCO BRASS", G_DISCOBRASS),
    G("DUB · REGGAE", "DUB SIREN", G_DUBSIREN),
    G("DUB · REGGAE", "DUB CHORD ECHO", G_DUBCHORD),
    G("DUB · REGGAE", "MELODICA", G_MELODICA),
    G("TECHNO · ELECTRO", "ACID LINE", G_ACIDLINE),
    G("TECHNO · ELECTRO", "HYPNOTIC ARP", G_HYPNOARP),
    G("TECHNO · ELECTRO", "DUB TECHNO CHORD", G_DUBTECHNO),
    G("TECHNO · ELECTRO", "ELECTRO SYNC LEAD", G_ELECTROSYNC),
    G("TECHNO · ELECTRO", "MINIMAL BLIP", G_MINIMALBLIP),
    G("GRIME · DUBSTEP · DRILL", "GRIME SQUARE", G_GRIMESQUARE),
    G("GRIME · DUBSTEP · DRILL", "REESE", G_REESE),
    G("GRIME · DUBSTEP · DRILL", "DRILL GLIDE", G_DRILLGLIDE),
    G("GRIME · DUBSTEP · DRILL", "WOBBLE LEAD", G_WOBBLELEAD),
    G("POP · SYNTHWAVE · TRANCE", "SUPERSAW", G_SUPERSAW),
    G("POP · SYNTHWAVE · TRANCE", "80S BRASS", G_80SBRASS),
    G("POP · SYNTHWAVE · TRANCE", "SYNTHWAVE POLY", G_SYNTHWAVE),
    G("POP · SYNTHWAVE · TRANCE", "GLIDE LEAD", G_GLIDELEAD),
    G("POP · SYNTHWAVE · TRANCE", "TRANCE PLUCK", G_TRANCEPLUCK),
    G("POP · SYNTHWAVE · TRANCE", "UNISON LEAD", G_UNISONLEAD),
    G("AMBIENT", "SOFT PAD", G_SOFTPAD),
    G("AMBIENT", "PHASE PAD", G_PHASEPAD),
    G("AMBIENT", "SLOW ARP", G_SLOWARP),
    G("AMBIENT", "DRONE", G_DRONE),
};
#undef G
#undef S
#undef PSRC
#undef PDST
#undef PINT
