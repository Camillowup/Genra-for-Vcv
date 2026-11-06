/* SPDX-License-Identifier: GPL-3.0-only */
/* K-WASUB: what the screen shows. Four themes share the same visuals and the same items:
 *   CLASSIC  the first design: banner, title, a visual, rows or boxes
 *   STUDIO   no banner: a status bar and the eight pages as tabs, the items as a grid of cells (the knob's colour,
 *            what modulates it), a wide visual
 *   PURE     after Pure SUB: a lime tab, a big visual, four rings for the four knobs, the row being edited below
 *   RACK     the pages on a rail at the left, big type, tall rows
 * Every visual is drawn with integers only (no FPU), from the patch and what the voices say (ksub_dsp.c). */
enum { TH_GLOW, TH_STUDIO, TH_PURE, TH_RACK, TH_TECH, TH_RIBBON, TH_RIBBONC, TH_CLASSIC, NTHEMES };   /* (CLASSIC: GLOW flat, 1.0.19) */
enum { VZ_NONE, VZ_SCOPE, VZ_OSCA, VZ_OSCB, VZ_SUB, VZ_MIX, VZ_ENV0, VZ_ENV1, VZ_ENV2, VZ_FILT, VZ_LFO1, VZ_LFO2, VZ_STEP,
       VZ_SEQ, VZ_SMP, VZ_DRIVE, VZ_CRUSH, VZ_CHORUS, VZ_DELAY, VZ_REVERB, VZ_MASTER, VZ_COMP, VZ_PHASER, VZ_OTT, VZ_CPU, VZ_DRUM };
static const uint8_t VZ_OF[] = {VZ_SCOPE, VZ_SCOPE, VZ_OSCA, VZ_OSCB, VZ_SUB, VZ_MIX, VZ_SMP, VZ_ENV0, VZ_ENV1, VZ_ENV2, VZ_FILT, VZ_FILT,
                                VZ_SEQ, VZ_DRUM, VZ_LFO1, VZ_LFO2, VZ_STEP, VZ_NONE, VZ_NONE, VZ_DRIVE, VZ_REVERB, VZ_DELAY, VZ_OTT, VZ_COMP, VZ_NONE,
                                VZ_CHORUS, VZ_PHASER, VZ_CRUSH, VZ_MASTER, VZ_NONE, VZ_NONE, VZ_NONE, VZ_NONE, VZ_CPU, VZ_NONE};
typedef char vz_fits[sizeof VZ_OF == sizeof VIEWS / sizeof VIEWS[0] ? 1 : -1];
static const char *const PAGE_SHORT[NPAGE] = {"PLAY", "BASS", "ENV", "FILT", "LFO", "FX", "PRE", "SYS"};
/* LANGUAGE (SYSTEM): only the view titles (v->cat, top of every screen) are translated; parameter names and values
 * stay in English, as on most hardware synths, and to keep every screen's layout untouched */
static const char *const L_CAT[27][3] = {
    {"CURRENT PATCH", "PATCH ACTUEL", "PARCHE ACTUAL"},
    {"LIVE MIX", "MIX EN DIRECT", "MEZCLA EN VIVO"},
    {"OSCILLATOR A", "OSCILLATEUR A", "OSCILADOR A"},
    {"OSCILLATOR B", "OSCILLATEUR B", "OSCILADOR B"},
    {"SUB + NOISE", "SUB + BRUIT", "SUB + RUIDO"},
    {"COMBINE A + B", "COMBINER A + B", "COMBINAR A + B"},
    {"OSCILLATOR C \xB7 SAMPLE", "OSCILLATEUR C \xB7 SAMPLE", "OSCILADOR C \xB7 SAMPLE"},
    {"AMP ENVELOPE", "ENVELOPPE AMPLI", "ENVOLVENTE AMP"},
    {"FILTER ENVELOPE", "ENVELOPPE FILTRE", "ENVOLVENTE FILTRO"},
    {"MOD ENVELOPE", "ENVELOPPE MOD", "ENVOLVENTE MOD"},
    {"FILTER / COLOUR", "FILTRE / COULEUR", "FILTRO / COLOR"},
    {"FILTER MOD", "MOD DU FILTRE", "MOD DEL FILTRO"},
    {"SEQ BASS", "SEQ BASS", "SEQ BASS"},
    {"LFO 1 \xB7 WOBBLE", "LFO 1 \xB7 WOBBLE", "LFO 1 \xB7 WOBBLE"},
    {"LFO 2 \xB7 MOTION", "LFO 2 \xB7 MOUVEMENT", "LFO 2 \xB7 MOVIMIENTO"},
    {"STEP FX", "STEP FX", "STEP FX"},
    {"MOD MATRIX", "MATRICE DE MOD", "MATRIZ DE MOD"},
    {"LFO 3 \xB7 GLOBAL", "LFO 3 \xB7 GLOBAL", "LFO 3 \xB7 GLOBAL"},
    {"FX RACK", "RACK FX", "RACK FX"},
    {"EQ", "EQ", "EQ"},
    {"PRESET BANK A", "BANQUE DE PRESETS A", "BANCO DE PRESETS A"},
    {"PRESET BANK B", "BANQUE DE PRESETS B", "BANCO DE PRESETS B"},
    {"SAVE PATCH", "ENREGISTRER LE PATCH", "GUARDAR EL PARCHE"},
    {"ARPEGGIATOR", "ARPEGIATEUR", "ARPEGIADOR"},
    {"GLOBAL SETUP", "REGLAGES GENERAUX", "AJUSTES GENERALES"},
    {"MIDI", "MIDI", "MIDI"},
    {"DRUMS", "BATTERIE", "BATERIA"},
};
static const char *cat_tr(const char *cat)      /* v->cat in the user's language (SYSTEM > LANGUAGE); English if not found */
{
    uint32_t i;
    if (!set.lang) return cat;
    for (i = 0; i < 27u; i++) if (str_eq(cat, L_CAT[i][0])) return L_CAT[i][set.lang];
    return cat;
}
static const uint16_t KNOB_RGB[4] = {RGB(255, 92, 92), RGB(255, 204, 64), RGB(92, 226, 116), RGB(84, 164, 255)};
static int16_t live[224];

/* ---------------------------------------------------------------- small pieces */
static void draw_poly(int32_t x, int32_t y, int32_t w, int32_t h, const int16_t *pt, uint32_t n, uint16_t c)   /* Q15 points */
{
    uint32_t i;
    int32_t cy = y + h / 2, px = x, py = cy - (pt[0] * (h / 2 - 1)) / 32768;
    if (!g_vis(y - 1, h + 2)) return;
    for (i = 1; i < n; i++) {
        int32_t xx = x + (int32_t)(i * (uint32_t)(w - 1) / (n - 1u)), yy = cy - (pt[i] * (h / 2 - 1)) / 32768;
        uint16_t cc = c;
        if (g_wgrad && c == K_WAVE) {           /* (SYNTHWAVE RAD: yellow, then orange, then light red, left to right) */
            uint32_t t = i * 512u / n;
            cc = t < 256u ? g_mix(RGB(255, 230, 40), RGB(255, 140, 20), t) : g_mix(RGB(255, 140, 20), RGB(255, 96, 96), t - 256u);
        }
        g_line(px, py, xx, yy, cc);
        g_line(px, py + 1, xx, yy + 1, g_mix(K_BG, cc, 120));
        px = xx;
        py = yy;
    }
}
static void g_col(int32_t x, int32_t ytop, int32_t ybot, uint16_t fill, uint16_t edge)   /* a filled column with a lit top */
{
    if (ytop < ybot) g_rect(x, ytop, 1, ybot - ytop, fill);
    g_px(x, ytop, edge);
    g_px(x, ytop + 1, edge);
}
static void g_ring(int32_t cx, int32_t cy, int32_t r, uint32_t frac, uint16_t c)   /* 270 degrees, frac 0..256 */
{
    uint32_t i, n = 90;
    if (!g_vis(cy - r - 1, 2 * r + 3)) return;
    for (i = 0; i <= n; i++) {
        uint32_t a = 0x60000000u + i * (0xC0000000u / n), k;
        uint16_t col = i * 256u <= frac * n ? c : K_TRACK;
        for (k = 0; k < 3u; k++) {
            int32_t rr = r - (int32_t)k;
            g_px(cx + ((sin32(a + 0x40000000u) * rr) >> 15), cy + ((sin32(a) * rr) >> 15), col);
        }
    }
    {
        uint32_t a = 0x60000000u + (uint32_t)(((uint64_t)0xC0000000u * frac) >> 8);
        g_line(cx, cy, cx + ((sin32(a + 0x40000000u) * (r - 6)) >> 15), cy + ((sin32(a) * (r - 6)) >> 15), K_WHITE);
    }
}
static int32_t dest_of(uint32_t id)             /* the destination that moves this parameter (-1: none) */
{
    switch (id) {
    case F_CUT: return D_CUT; case F_RES: return D_RES; case F_DRV: return D_DRV;
    case A_LVL: return D_ALVL; case A_P1: return D_AP1; case A_P2: return D_AP2; case A_P3: return D_AP3;
    case I_ABMIX: return D_ABMIX;
    case A_OCT: case A_SEMI: case A_FINE: return D_APIT;
    case B_LVL: return D_BLVL; case B_P1: return D_BP1; case B_P2: return D_BP2; case B_P3: return D_BP3;
    case B_OCT: case B_SEMI: case B_FINE: return D_BPIT;
    case X_AMT: return D_XMOD; case S_LVL: return D_SUB; case N_LVL: return D_NOISE; case N_COL: return D_NCOL;
    case L1_DEP: return D_WOB;
    case F_ENV: return D_FENV; case L2_DEP: return D_L2DEP; case L1_RATE: case L1_DIV: return D_L1RATE;
    case L2_RATE: case L2_DIV: return D_L2RATE; case ST_GATE: return D_GATE;
    case DR_AMT: return D_DRAMT; case DR_MIX: return D_DRMIX; case CR_BITS: return D_CRBITS; case CR_RATE: return D_CRRATE;
    case CR_MIX: return D_CRMIX; case CH_RATE: return D_CHRATE; case CH_DEP: return D_CHDEP; case CH_MIX: return D_CHMIX;
    case DL_FB: return D_DLFB; case DL_TONE: return D_DLTONE; case DL_MIX: return D_DLMIX; case RV_SIZE: return D_RVSIZE;
    case RV_DAMP: return D_RVDAMP; case RV_MIX: return D_RVMIX; case MS_GLUE: return D_GLUE; case MS_SUB: return D_SUBB;
    case MS_VOL: return D_VOL; case I_L1RATE: return D_L1RATE; case I_L2RATE: return D_L2RATE;
    case CP_AMT: return D_CPAMT; case PH_RATE: return D_PHRATE; case PH_DEP: return D_PHDEP; case PH_MIX: return D_PHMIX;
    case G_EQ_F: return (int32_t)(D_EQ1F + (eq_cur & 3u)); case G_EQ_G: return (int32_t)(D_EQ1G + (eq_cur & 3u));   /* (the band being set) */
    default: {                                  /* the others (1.0.16): one destination each */
        uint32_t k;
        for (k = 0; k < ND - D_N0; k++) if (GD_ID[k] == id) return (int32_t)(D_N0 + k);
        return -1;
    }
    }
}
static const char *mod_tag(uint32_t id)         /* what modulates it, in two or three letters */
{
    int32_t d = dest_of(id), k;
    if (d < 0) return 0;
    if (patch[L1_DEP] && patch[L1_DST] == d) return "L1";
    if (patch[L2_DEP] && patch[L2_DST] == d) return "L2";
    if (set.l3[L3_DEP] && set.l3[L3_DST] == d) return "L3";
    if (patch[ST_ON] && patch[ST_DEP] && patch[ST_DST] == d) return "STP";
    if ((patch[ME_AMT] && patch[ME_DST] == d) || (d == D_CUT && patch[F_ENV])) return "ENV";
    for (k = 0; k < 4; k++) if (patch[M1_SRC + 3 * k] && patch[M1_AMT + 3 * k] && patch[M1_DST + 3 * k] == d) return "MX";
    return 0;
}
static int32_t knob_of(const view_t *v, uint32_t id)
{
    int32_t k;
    for (k = 0; k < 4; k++) if (vknob(v, (uint32_t)k) == id) return k;
    return -1;
}

/* ---------------------------------------------------------------- the visuals */
static void vis_env(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k)
{
    static const uint8_t PA[3][4] = {{AE_A, AE_D, AE_S, AE_R}, {FE_A, FE_D, FE_S, FE_R}, {ME_A, ME_D, 0, ME_D}};
    int32_t a = patch[PA[k][0]], d = patch[PA[k][1]], s = k == 2u ? 0 : patch[PA[k][2]] * 258, r = patch[PA[k][3]];
    int32_t wa = 2 + a, wd = 2 + d, ws = k == 2u ? 0 : 40, wr = k == 2u ? 0 : 2 + r, tot = wa + wd + ws + wr, W = w - 10;
    int32_t x0 = x + 5, xa, xd, xs, xr, y0 = y + h - 4, H = h - 9, px, st = env_st_show[k], lv = env_lv_show[k];
    uint16_t fill = g_mix(K_PANEL, K_WAVE, 70);
    xa = x0 + wa * W / tot; xd = xa + wd * W / tot; xs = xd + ws * W / tot; xr = xs + wr * W / tot;
    if (!g_vis(y, h)) return;
    for (px = x0; px <= xr; px++) {
        int32_t l, t;
        if (px <= xa) l = xa > x0 ? 32767 * (px - x0) / (xa - x0) : 32767;
        else if (px <= xd) { t = 32767 - 32767 * (px - xa) / (xd - xa + 1); l = s + mul15(32767 - s, mul15(t, t)); }
        else if (px <= xs) l = s;
        else { t = 32767 - 32767 * (px - xs) / (xr - xs + 1); l = mul15(s, mul15(t, t)); }
        g_col(px, y0 - l * H / 32767, y0, fill, K_WAVE);
    }
    g_rect(x0, y0, W, 1, K_LINE);
    if (st) {                                   /* where the newest note is on it */
        int32_t dx;
        if (st == 1) dx = x0 + (xa - x0) * lv / 32767;
        else if (st == 2) dx = xa + (xd - xa) * (32767 - lv) / (32767 - s + 1);
        else if (st == 3) dx = xd + (xs - xd) / 2;
        else dx = xs + (xr - xs) * (s ? clampi(32767 - lv * 32767 / (s + 1), 0, 32767) : 32767 - lv) / 32767;
        dx = clampi(dx, x0, xr);
        g_rect(dx - 3, y0 - lv * H / 32767 - 3, 7, 7, K_LIME);
        g_rect(dx - 1, y0 - lv * H / 32767 - 1, 3, 3, K_INK);
    }
}
static void vis_filter(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t ic = patch[F_CUT] * 2, res = patch[F_RES], ft = patch[F_TYPE], px, W = w - 10, top = y + 3, H = h - 7;
    int32_t peak = res * 15 * 256 / 127, fc = KS_CUT_HZ[ic];
    uint16_t fill = g_mix(K_PANEL, K_WAVE, 70);
    if (!g_vis(y, h)) return;
    for (px = 0; px < W; px++) {
        int32_t i = px * 255 / (W - 1), o = (i - ic) * 256 / 26, ao = o < 0 ? -o : o, db = 0, b;   /* o: octaves Q8, db Q8 */
        b = ao < 154 ? peak - peak * ao / 154 : 0;   /* the resonance, over 0.6 octave */
        switch (ft) {
        case FT_LP24: db = o > 0 ? -24 * o : 0; db += b; break;
        case FT_LP12: db = o > 0 ? -12 * o : 0; db += b; break;
        case FT_HP: db = o < 0 ? 24 * o : 0; db += b; break;
        case FT_BP: db = -12 * ao + b; break;
        case FT_NOTCH: db = ao < 77 ? -36 * 256 + 36 * 256 * ao / 77 : 0; break;
        case FT_VOWEL: {                        /* the three formants of the vowel the cutoff points at */
            uint32_t vw = (uint32_t)q8to15(patch[F_CUT] * 256) * 4u, vi = vw >> 15, k;
            int32_t hz = KS_CUT_HZ[i];
            if (vi > 3u) vi = 3u;
            db = -18 * 256;
            for (k = 0; k < 3u; k++) {
                int32_t f = VOX_F[vi][k], d = hz > f ? hz - f : f - hz, bw = f / 4 + 60;
                int32_t pk = -18 * 256 + (18 * 256 + peak) * (bw - d) / bw - (int32_t)k * 6 * 256;
                if (d < bw && pk > db) db = pk;
            }
            break;
        }
        default: {                              /* COMB: peaks every cutoff Hz */
            db = (sin32((uint32_t)((uint32_t)KS_CUT_HZ[i] * (0xFFFFFFFFu / (uint32_t)(fc ? fc : 1))) + 0x40000000u) * (6 + res / 8)) >> 7;
            break;
        }
        }
        db = clampi(db, -36 * 256, 18 * 256);
        g_col(x + 5 + px, top + (18 * 256 - db) * H / (54 * 256), y + h - 4, fill, K_WAVE);
    }
    {
        int32_t xc = x + 5 + ic * (W - 1) / 255, xl = x + 5 + (cut_show >> 7) * (W - 1) / 255;
        g_rect(xc, top, 1, H, K_LINE);
        if (ks_sounding) { g_rect(xl - 3, top + 2, 7, 7, K_LIME); g_rect(xl - 1, top + 4, 3, 3, K_INK); }
    }
}
static void vis_lfo(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k)
{
    int16_t pt[112];
    uint32_t i, ph = lfo_ph_show[k];
    lfo_t l;
    if (!g_vis(y, h)) return;
    memset(&l, 0, sizeof l);
    l.sh = 20000; l.sh0 = -12000;
    for (i = 0; i < 112u; i++) pt[i] = (int16_t)lfo_shape(&l, i * (0xFFFFFFFFu / 56u), (uint32_t)patch[L1_SHP + k * LFO_2]);
    g_rect(x + 4, y + h / 2, w - 8, 1, K_LINE);
    draw_poly(x + 4, y + 2, w - 8, h - 4, pt, 112, K_WAVE);
    {
        int32_t dx = x + 4 + (int32_t)((ph >> 24) * (uint32_t)(w - 9) / 510u), dy = y + h / 2 - (lfo[k].out * (h / 2 - 3)) / 32768;
        g_rect(dx - 3, dy - 3, 7, 7, K_LIME);
    }
}
static void vis_step(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t i, bw = (w - 8) / 16, x0 = x + 4 + (w - 8 - bw * 16) / 2, H = h - 8;
    if (!g_vis(y, h)) return;
    for (i = 0; i < 16; i++) {
        int32_t lv;
        char c = STEP_PAT[patch[ST_PAT] & 15][i];
        lv = patch[ST_PAT] == 15 ? (int32_t)(((uint32_t)i * 2654435761u) >> 28) : c <= '9' ? c - '0' : c - 'A' + 10;
        g_rect(x0 + i * bw + 1, y + 4, bw - 2, H, K_TRACK);
        g_rect(x0 + i * bw + 1, y + 4 + H - lv * H / 15, bw - 2, lv * H / 15,
               !patch[ST_ON] ? K_LINE : i == step_pos ? K_LIME : (i & 3) ? K_WAVE : g_mix(K_WAVE, K_WHITE, 90));
    }
}
static void vis_seq(int32_t x, int32_t y, int32_t w, int32_t h)   /* SEQ BASS: 16 steps as a little piano roll (the cursor's page of 16) */
{
    const seqpat_t *q = sq_pat();
    uint32_t k = perf.cur % SQ_NPAT;
    int32_t i, len = q->len >= 1u && q->len <= SQ_MAX ? q->len : 16, bw = (w - 8) / 16, x0 = x + 4 + (w - 8 - bw * 16) / 2, H = h - 11, top = y + 4;
    int32_t pg = sq_cur_step / 16, np = (len + 15) / 16;
    uint64_t lkd = 0;                           /* (the steps with a PARAMETER LOCK) */
    if (!g_vis(y, h)) return;
    uint64_t aud = 0;                           /* (... with AUTOMATION) */
    { uint32_t j, n = pl.n <= PL_MAX ? pl.n : 0u; for (j = 0; j < n; j++) if (pl.l[j].step < 64u) { if (pl.l[j].pat == (PL_BASS | k)) lkd |= 1ull << pl.l[j].step; else if (pl.l[j].pat == (PL_AUTO | k)) aud |= 1ull << pl.l[j].step; } }
    for (i = 0; i < 16; i++) {
        int32_t s = pg * 16 + i, bx = x0 + i * bw, f = *sq_fp(k, (uint32_t)s), nh;
        int on = sq_play && s == sq_show;
        g_rect(bx + 1, top, bw - 2, H, s >= len ? K_BG : on ? g_mix(K_TRACK, K_LIME, 40) : K_TRACK);
        if (s < len && (f & SF_ON)) {
            uint16_t c = on ? K_LIME : (f & SF_ACC) ? K_ORANGE : K_WAVE;
            nh = 4 + *sq_np(k, (uint32_t)s) * (H - 4) / 48;
            g_rect(bx + 1, top + H - nh, bw - 2, 3, c);
            g_rect(bx + 2, top + H - nh + 3, bw - 4, nh - 3, g_mix(K_TRACK, c, 45));
            if (f & SF_SLIDE) g_rect(bx + bw - 2, top + H - nh + 1, 3, 1, c);   /* (into the next) */
            if ((sqx.p[k].cond[s] & 15u) || sqx.p[k].prob[s] < 100u) g_rect(bx + bw / 2 - 1, top + H - nh - 4, 3, 2, K_WHITE);   /* (a chance or a condition: a dot above) */
            if (lkd >> s & 1u) { g_rect(bx + 1, top, bw - 2, 2, K_CYAN); g_rect(bx + 1, top + 2, 2, 3, K_CYAN); }   /* (locked: a corner at the top) */
            {                                   /* (a note longer than its step: a tail over the next ones, to the page's end) */
                uint32_t gi = sqx.p[k].cond[s] >> 4, tail = gi == 15u ? (uint32_t)(16 - i) * (uint32_t)bw : (uint32_t)(((uint64_t)SQ_GL[gi] * (uint32_t)bw) >> 24);
                int32_t tw = (int32_t)tail - bw, room = (16 - i - 1) * bw;
                if (tw > 0) g_rect(bx + bw - 1, top + H - nh + 1, tw < room ? tw : room, 1, c);
            }
        }
        g_rect(bx + 1, top + H + 2, bw - 2, 3, s == sq_cur_step ? (sq_rec ? K_ORANGE : K_WHITE) : (i & 3) ? K_BG : K_LINE);
        if (aud >> s & 1u) g_rect(bx + 2, top + H - 2, bw - 4, 2, K_MOD);   /* (automated: a line at the foot) */
    }
    if (np > 1)                                 /* the pages, top right: this one white, the one playing lime */
        for (i = 0; i < np; i++)
            g_rect(x + w - 6 - (np - i) * 9, top + 1, 7, 3, i == pg ? K_WHITE : sq_play && sq_show != 0xFF && sq_show / 16 == i ? K_LIME : K_LINE);
}

/* DRUMS: the 8 tracks x the 16 steps of the cursor's page, each instrument its colour; the track chosen lit, the step
 * playing a column, the cursor underlined, the muted tracks grey, a dot where a chance or a condition is set */
static uint32_t isqrt32(uint32_t v) { uint32_t r = 0, b = 1u << 15; for (; b; b >>= 1) if ((r | b) * (r | b) <= v) r |= b; return r; }
static void dk_orb(int32_t cx, int32_t cy, int32_t r2, uint16_t col, uint32_t a)   /* a sphere: lit from the top left, a shadowed side, no gleam */
{
    int32_t dx, dy, e = (r2 + 3) / 2;
    uint16_t dark = g_mix(0x0000u, col, 60), lite = g_mix(col, 0xFFFFu, 28);
    if (!g_vis(cy - e, 2 * e + 1)) return;
    for (dy = -e; dy <= e; dy++)
        for (dx = -e; dx <= e; dx++) {
            int32_t d2 = 4 * (dx * dx + dy * dy), in = (r2 - 1) * (r2 - 1), out = (r2 + 1) * (r2 + 1), nx, ny, nz, dot;
            uint32_t al, z2, n2;
            uint16_t c;
            if (d2 >= out) continue;
            al = d2 <= in ? a : a * (uint32_t)(out - d2) / (uint32_t)(out - in);
            nx = dx * 512 / r2; ny = dy * 512 / r2;   /* (the surface's direction, 256 = 1) */
            n2 = (uint32_t)(nx * nx + ny * ny);
            z2 = n2 >= 65536u ? 0u : 65536u - n2;
            nz = (int32_t)isqrt32(z2);
            dot = (nx * -90 + ny * -128 + nz * 202) >> 8; if (dot < 0) dot = 0; if (dot > 256) dot = 256;
            c = dot < 170 ? g_mix(dark, col, (uint32_t)dot * 256u / 170u) : g_mix(col, lite, (uint32_t)(dot - 170) * 256u / 86u);
            g_pxa(cx + dx, cy + dy, c, al);
        }
}
static inline int hold_dot_cursor(int sel, int32_t s) { return sel && s == (int32_t)dk_cur; }
static void dk_dot(int32_t cx, int32_t cy, int32_t r2, uint16_t c, uint32_t a)   /* a soft round dot, radius r2 / 2 pixels (half pixels: 3 = 1.5) */
{
    int32_t dx, dy, e = (r2 + 3) / 2;
    for (dy = -e; dy <= e; dy++)
        for (dx = -e; dx <= e; dx++) {
            int32_t d2 = 4 * (dx * dx + dy * dy), in = (r2 - 1) * (r2 - 1), out = (r2 + 1) * (r2 + 1);
            if (d2 <= in) g_pxa(cx + dx, cy + dy, c, a);
            else if (d2 < out) g_pxa(cx + dx, cy + dy, c, a * (uint32_t)(out - d2) / (uint32_t)(out - in));
        }
}
static void vis_drum(int32_t x, int32_t y, int32_t w, int32_t h)
{
    const dkpat_t *q = dk_pp();
    int32_t len = (int32_t)dk_len(), pg = dk_cur / 16, np = (len + 15) / 16, bw = (w - 30) / 16, x0 = x + 26, rh = (h - 6) / (int32_t)NDK, t, i;
    uint8_t lkd[DK_MAX];                        /* (each step's tracks with a PARAMETER LOCK) */
    if (!g_vis(y, h)) return;
    memset(lkd, 0, sizeof lkd);
    { uint32_t j, n = pl.n <= PL_MAX ? pl.n : 0u; for (j = 0; j < n; j++) if (pl.l[j].pat == dk.cur % DK_NPAT && pl.l[j].step < DK_MAX) lkd[pl.l[j].step] |= (uint8_t)(1u << (pl.l[j].tp >> 4 & 7u)); }
    if (rh < 3) rh = 3;
    {                                           /* the step playing (a soft column) and the step held */
        int32_t ps = sq_play && dk_show != 0xFF && (int32_t)dk_show / 16 == pg ? (int32_t)dk_show % 16 : -1;
        int32_t hs = dk_hold && (int32_t)(dk_hold - 1u) / 16 == pg ? (int32_t)(dk_hold - 1u) % 16 : -1;
        if (ps >= 0) g_rect(x0 + ps * bw, y + 1, bw, h - 2, g_mix(K_BG, K_WHITE, 22));
        if (hs >= 0) g_rect(x0 + hs * bw, y + 1, bw, h - 2, g_mix(K_BG, K_WHITE, 50));
    }
    for (t = 0; t < (int32_t)NDK; t++) {
        int32_t ry = y + 3 + t * rh, cy = ry + rh / 2, mute = dk.mute >> t & 1u, sel = t == (int32_t)dk_trk;
        int32_t R = (bw < rh ? bw : rh) - 1;     /* (the dot's diameter, in pixels) */
        uint16_t col = mute ? g_mix(K_BG, K_GREY, 150) : K_LIME;   /* (every track the theme's accent) */
        if (!g_vis(ry, rh)) continue;
        if (sel) { g_rect(x + 2, ry, w - 4, rh, g_mix(K_BG, col, 26)); g_rect(x + 2, ry + 1, 2, rh - 2, col); }   /* (the track chosen: a tint, a bar at the left) */
        dk_dot(x + 13, cy, (sel ? R + 1 : R - 2) + 3, 0x0000u, 256);   /* (the track's mark: a bigger sphere when chosen, a thick black outline) */
        dk_orb(x + 13, cy, sel ? R + 1 : R - 2, col, 256);
        for (i = 0; i < 16; i++) {
            int32_t s = pg * 16 + i, cx = x0 + i * bw + bw / 2;
            uint32_t on = s < len && (q->on[s] >> t & 1u), acc = q->acc[s] >> t & 1u, now = sq_play && s == (int32_t)dk_show;
            if (s >= len) continue;
            if (!on) {                          /* (an empty step: a small dim dot, a bigger one on the beat) */
                dk_dot(cx, cy, (i & 3) ? 3 : 4, K_BG, 200);            /* (a socket: dark, a faint rim of the colour) */
                dk_dot(cx, cy, (i & 3) ? 2 : 3, now ? K_WHITE : col, now ? 150 : (i & 3) ? 70 : 120);
                if (hold_dot_cursor(sel, s)) dk_dot(cx, cy, 3, K_WHITE, 200);
                continue;
            }
            dk_dot(cx, cy, (acc ? R : R - 2) + 3, 0x0000u, 256);   /* (a thick black outline, then the sphere; an accent bigger and lighter) */
            dk_orb(cx, cy, acc ? R : R - 2, now ? K_WHITE : acc ? g_mix(col, 0xFFFFu, 60) : col, 256);
            if (q->pc[t][s] & 0xFFu) dk_dot(cx, cy, R / 2 > 2 ? R - 4 : 1, K_BG, 200);   /* (a chance or a condition: a hollow) */
            if (lkd[s] >> t & 1u) g_rect(cx + bw / 2 - 2, ry + 1, 2, 2, K_WHITE);   /* (locked: a white spark at the corner) */
            if (hold_dot_cursor(sel, s)) g_rect(cx - 2, ry + rh - 1, 5, 1, 0xFFFFu);
        }
    }
    for (i = 0; np > 1 && i < np; i++)          /* the pages, bottom right */
        g_rect(x + w - 6 - (np - i) * 9, y + h - 3, 7, 2, i == pg ? K_WHITE : sq_play && dk_show != 0xFF && dk_show / 16 == i ? K_LIME : K_LINE);
}

/* ---- the oscillators, each engine its own picture */
static void vis_wt(const int8_t *o, int32_t x, int32_t y, int32_t w, int32_t h)   /* WAVE: the 8 frames in depth, the one read lit */
{
    uint32_t tb = ((uint32_t)o[3] * 258u) >> 12, pos = (uint32_t)o[2] * 7u * 256u / 127u, f, i;
    int32_t fw = w * 62 / 100, fh = h * 52 / 100, dx = (w - 8 - fw) / 7, dy = (h - 8 - fh) / 7;
    char t[24], *p;
    if (tb > 7u) tb = 7u;
    for (f = 8; f-- > 0;) {                     /* back to front */
        const int16_t *wv = KS_WT + ((tb * 8u + f) << 8);
        int32_t ox = x + 4 + (int32_t)f * dx, oy = y + 4 + (int32_t)(7u - f) * dy, px = 0, py = 0, near = (int32_t)(pos >> 8) == (int32_t)f || ((pos + 128u) >> 8) == f;
        uint16_t c = near ? g_mix(K_LINE, K_WAVE, 140) : K_LINE;
        for (i = 0; i <= 32u; i++) {
            int32_t xx = ox + (int32_t)i * fw / 32, yy = oy + fh / 2 - wv[(i * 8u) & 255u] * (fh / 2 - 1) / 32768;
            if (i) g_line(px, py, xx, yy, c);
            px = xx; py = yy;
        }
    }
    {                                           /* the frame read now: between two, where it is */
        uint32_t f0 = pos >> 8, fk = pos & 255u, f1 = f0 < 7u ? f0 + 1u : 7u;
        const int16_t *a = KS_WT + ((tb * 8u + f0) << 8), *b = KS_WT + ((tb * 8u + f1) << 8);
        int32_t ox = x + 4 + (int32_t)(pos * (uint32_t)dx >> 8), oy = y + 4 + (int32_t)((7u * 256u - pos) * (uint32_t)dy >> 8), px = 0, py = 0;
        for (i = 0; i <= 48u; i++) {
            uint32_t ix = (i * 256u / 48u) & 255u;
            int32_t v = a[ix] + (((b[ix] - a[ix]) * (int32_t)fk) >> 8);
            int32_t xx = ox + (int32_t)i * fw / 48, yy = oy + fh / 2 - v * (fh / 2 - 1) / 32768;
            if (i) { g_line(px, py, xx, yy, K_WAVE); g_line(px, py + 1, xx, yy + 1, g_mix(K_BG, K_WAVE, 120)); }
            px = xx; py = yy;
        }
    }
    p = str_cat(t, L_WT[tb]);
    p = str_cat(p, " \xB7 ");
    p = fmt_u(p, (uint32_t)o[2] * 100u / 127u, 0);
    str_cat(p, "%");
    if (h >= 34) g_text_r(T_XS, x + w - 4, y + h - 11, t, K_GREY);
}
static void vis_fm4(const int8_t *o, uint32_t al, int32_t x, int32_t y, int32_t w, int32_t h)   /* FM, 4 operators: the ALGO drawn,
                                                 * the heard ones on the right, each modulator one column further left */
{
    uint32_t a = FM_ALG[al], to[5], dep[5], j, c, nc[4] = {0, 0, 0, 0}, row[5], rk = (uint32_t)(o[3] * 258) >> 11 & 15u;
    int32_t bx[5], byy[5], bw, bh, top = y + 12;
    char t[20];
    to[1] = 0; to[2] = a >> 6 & 1u; to[4] = a & 7u;   /* (bits: 1, 2, 3 = the operators it modulates) */
    to[3] = (a >> 3 & 1u) | (a >> 4 & 1u) << 1;
    for (j = 1; j <= 4u; j++) {                  /* the depth: 0 heard, else one more than the deepest it modulates */
        uint32_t d = 0, m = to[j], b;
        for (b = 0; b < 3u; b++) if (m >> b & 1u) { uint32_t e = dep[b + 1] + 1u; if (e > d) d = e; }
        dep[j] = d; row[j] = nc[d]++;
    }
    for (c = 0; c < 4u && nc[c]; c++) {}
    if (c == 1u) { c = 4; for (j = 1; j <= 4u; j++) { dep[j] = j - 1u; row[j] = 0; nc[j - 1u] = 1; } }   /* (all heard: side by side) */
    bw = (w - 8) / (int32_t)c - 10; if (bw > 44) bw = 44;
    for (j = 1; j <= 4u; j++) {
        int32_t rh = (y + h - 2 - top) / (int32_t)nc[dep[j]];
        bh = rh - 3 > 16 ? 16 : rh - 3;
        bx[j] = x + w - 6 - bw - (int32_t)dep[j] * (bw + 10);
        byy[j] = top + (int32_t)row[j] * rh + (rh - bh) / 2;
    }
    for (j = 2; j <= 4u; j++) {                  /* the modulations: right to the operator modulated */
        uint32_t b;
        for (b = 0; b < 3u; b++) if (to[j] >> b & 1u) g_line(bx[j] + bw, byy[j] + bh / 2, bx[b + 1], byy[b + 1] + bh / 2, g_mix(K_LINE, K_CYAN, 120 + o[2]));
    }
    for (j = 1; j <= 4u; j++) {
        uint16_t col = to[j] ? K_CYAN : K_WAVE;
        char *p = t;
        *p++ = (char)('0' + j); *p++ = ' '; *p++ = 'x'; *p = 0;
        if (j == 1u) str_cat(p, "1");
        else if (j == 2u) str_cat(p, L_RATIO[rk]);
        else { uint32_t r = FM_R34[rk][j - 3u]; p = fmt_u(p, r >> 8, 0); if (r & 255u) { *p++ = '.'; fmt_u(p, (r & 255u) * 10u >> 8, 0); } }
        g_box(bx[j], byy[j], bw, bh, K_PANEL, col);
        g_text_c(T_XS, bx[j] + bw / 2, byy[j] + bh / 2 - 4, t, col);
        if (!to[j]) g_rect(bx[j] + bw, byy[j] + bh / 2, x + w - 2 - bx[j] - bw, 1, K_WAVE);   /* heard: out */
    }
    if (o[4]) { uint16_t cf = g_mix(K_LINE, K_MOD, 100 + o[4]); g_rect(bx[4] + 2, byy[4] - 2, bw - 4, 1, cf); g_rect(bx[4] + 2, byy[4] - 2, 1, 2, cf); g_rect(bx[4] + bw - 3, byy[4] - 2, 1, 2, cf); }
    str_cat(str_cat(t, "ALGO "), L_ALG[al]);
    g_text(T_XS, x + 4, y + 3, t, K_GREY);
}
static void vis_fm(const int8_t *o, uint32_t k, int32_t x, int32_t y, int32_t w, int32_t h)   /* FM: the two operators */
{
    int32_t bw = w * 30 / 100, bh = h > 46 ? 22 : h - 16, by = y + (h - bh) / 2 + (h > 46 ? 4 : 2), mx = x + 8, cx = x + w - bw - 26;
    int32_t th = 1 + o[2] / 32, fbk = o[4], i;
    char t[16];
    uint32_t al = (uint32_t)patch[k ? B_P4 : A_P4] >> 4 & 7u;
    if (al) { vis_fm4(o, al, x, y, w, h); return; }
    if (bh < 12) bh = 12;
    g_box(mx, by, bw, bh, K_PANEL, K_CYAN);     /* the modulator, its ratio */
    str_cat(str_cat(t, "MOD x"), L_RATIO[(o[3] * 258) >> 11 & 15]);
    g_text_c(T_XS, mx + bw / 2, by + bh / 2 - 4, t, K_CYAN);
    g_box(cx, by, bw, bh, K_PANEL, K_WAVE);     /* the carrier */
    g_text_c(T_XS, cx + bw / 2, by + bh / 2 - 4, "CARRIER", K_WAVE);
    for (i = 0; i < th; i++) g_rect(mx + bw, by + bh / 2 - th / 2 + i, cx - mx - bw - 4, 1, g_mix(K_LINE, K_CYAN, 120 + o[2]));   /* the index */
    g_rect(cx - 6, by + bh / 2 - 3, 2, 7, K_CYAN); g_rect(cx - 4, by + bh / 2 - 2, 2, 5, K_CYAN); g_rect(cx - 2, by + bh / 2 - 1, 2, 3, K_CYAN);
    str_cat(fmt_u(str_cat(t, "INDEX "), (uint32_t)o[2] * 100u / 127u, 0), "%");
    if (h > 46) g_text_c(T_XS, (mx + bw + cx) / 2, by + bh / 2 + 5, t, K_GREY);
    if (fbk && by - 6 > y) {                    /* the feedback: out of the modulator, back into it */
        uint16_t c = g_mix(K_LINE, K_MOD, 100 + fbk);
        g_rect(mx + bw / 2, by - 6, bw / 2 + 6, 1, c); g_rect(mx + bw + 6, by - 6, 1, bh / 2 + 6, c);
        g_rect(mx + bw / 2, by - 6, 1, 6, c);
        if (h > 46) g_text(T_XS, mx, y + 3, "FEEDBACK", K_MOD);
    }
    g_rect(cx + bw, by + bh / 2, x + w - 4 - cx - bw, 1, K_WAVE);   /* out */
    if (patch[X_MODE] == 2 && !k) g_text_r(T_XS, x + w - 4, y + 3, "+ B X-MOD", K_ORANGE);
}
static void vis_growl(const int8_t *o, uint32_t al, int32_t x, int32_t y, int32_t w, int32_t h)   /* GROWL: the three formants of the vowel now */
{
    const growl_t *gt = &GR_TYPE[al];
    int32_t pos = o[3] * 258 * 4, vi = pos >> 15, fr = pos & 32767, wd = 3 + ((127 - o[4]) >> 4), j, i, px = 0, py = 0;
    int32_t xc[3], W = w - 8, H = h - 16, by = y + h - 4;
    char t[20];
    if (vi > 3) { vi = 3; fr = 32767; }
    for (j = 0; j < 3; j++) {                   /* (where each sits: 100 Hz .. 4 kHz, on a square-root scale) */
        int32_t hz = GR_VOW[vi][j] + (((GR_VOW[vi + 1][j] - GR_VOW[vi][j]) * fr) >> 15), r = 0;
        hz = (hz * gt->shift) >> 7;
        while ((r + 1) * (r + 1) <= hz) r++;
        xc[j] = x + 4 + (r - 10) * W / 54;
    }
    for (i = 0; i <= W; i += 2) {
        int32_t xx = x + 4 + i, v = 0;
        for (j = 0; j < 3; j++) { int32_t d = (xx - xc[j]) * 16 / wd; v += (j ? (j == 1 ? 700 : 420) : 1000) * 256 / (256 + d * d); }
        v = by - v * H / 1100;
        if (v < y + 12) v = y + 12;
        if (i) g_line(px, py, xx, v, K_WAVE);
        g_rect(xx, v + 1, 2, by - v, g_mix(K_BG, K_WAVE, 50));
        px = xx; py = v;
    }
    for (j = 0; j < 3; j++) g_rect(xc[j], y + 12, 1, 3, K_CYAN);
    { char *p = t; *p++ = "UOAEI"[vi + (fr > 16384)]; *p = 0; g_text_r(T_M, x + w - 6, y + 3, t, K_WAVE); }
    str_cat(str_cat(t, "TYPE "), L_GRT[al]);
    g_text(T_XS, x + 4, y + 3, t, K_GREY);
}
static void vis_add(const int8_t *o, uint32_t reg, int32_t x, int32_t y, int32_t w, int32_t h)   /* ADDITIVE: the 16 partials as drawbars */
{
    int32_t a[25], k, bw = (w - 10) / 16, H = h - 16, mx = 1;
    char t[20];
    add_amps(o[2] * 258, o[3] * 258, o[4] * 258, reg, (int32_t *)a);
    for (k = 1; k <= 16; k++) if (a[k] > mx) mx = a[k];
    for (k = 1; k <= 16; k++) {
        int32_t bh = a[k] * H / mx, bx = x + 5 + (k - 1) * bw;
        uint16_t c = k == 1 ? K_ORANGE : k & 1 ? K_CYAN : K_WAVE;
        g_rect(bx, y + h - 4 - H, bw - 1, H, g_mix(K_BG, K_LINE, 60));
        if (bh) g_rect(bx, y + h - 4 - bh, bw - 1, bh, c);
    }
    str_cat(str_cat(t, "REGISTER "), L_ADR[reg]);
    g_text(T_XS, x + 4, y + 3, t, K_GREY);
}
static void vis_kick(const int8_t *o, int32_t x, int32_t y, int32_t w, int32_t h)   /* 808: the pitch falling from PUNCH to the note over DROP */
{
    char t[24], *p;
    int32_t i, px = 0, py = 0, tau = 5 + (int32_t)(((int64_t)(o[3] * 258) * (o[3] * 258) * 295) >> 30), top = y + 4 + (h - 8) * (127 - o[2]) / 127;
    uint32_t e = 1u << 15;                     /* (the drop, Q15, over 400 ms across the width) */
    if (!g_vis(y, h)) return;
    for (i = 0; i <= w - 8; i++) {
        int32_t xx = x + 4 + i, yy = y + h - 5 - (int32_t)(((int64_t)(y + h - 5 - top) * e) >> 15);
        if (i) g_line(px, py, xx, yy, K_WAVE);
        px = xx; py = yy;
        e -= (uint32_t)(((uint64_t)e * (uint32_t)(400 * 32768 / (w - 8))) / (uint32_t)(tau * 32768 + 1)) ;
    }
    if (h < 40) return;
    p = str_cat(t, "+"); p = fmt_u(p, (uint32_t)o[2] * 48u / 127u, 0); p = str_cat(p, " ST \xB7 "); p = fmt_u(p, (uint32_t)tau, 0); str_cat(p, " ms");
    g_text(T_XS, x + 4, y + 3, t, K_GREY);
    if (o[4]) { str_cat(fmt_u(str_cat(t, "DRIVE "), (uint32_t)o[4] * 100u / 127u, 0), "%"); g_text_r(T_XS, x + w - 4, y + 3, t, K_LIME); }
}
static void vis_cs(const int8_t *o, const int16_t *wv, int32_t x, int32_t y, int32_t w, int32_t h)   /* CS: the two channels' wave, their gap, the motion */
{
    char t[24], *p;
    uint32_t c = (uint32_t)(((uint64_t)((uint32_t)o[3] * 258u) * ((uint32_t)o[3] * 258u) * 350u) >> 30);
    draw_poly(x + 3, y + 2, w - 6, h - 4, wv, 112, K_WAVE);
    if (h < 40) return;
    p = str_cat(t, "I + II  ");
    p = fmt_u(p, c / 10u, 0); *p++ = '.'; p = fmt_u(p, c % 10u, 0); str_cat(p, " CT");
    g_text(T_XS, x + 4, y + 3, t, K_GREY);
    str_cat(fmt_u(str_cat(t, "MOTION "), (uint32_t)o[4] * 100u / 127u, 0), "%");
    g_text_r(T_XS, x + w - 4, y + 3, t, o[4] > 64 ? K_LIME : K_GREY);
}
static void vis_analog(const int8_t *o, const int16_t *wv, int32_t x, int32_t y, int32_t w, int32_t h)   /* ANALOG: the wave, its place on the scale */
{
    static const char *const W[4] = {"SIN", "TRI", "SAW", "SQR"};
    int32_t sh = h >= 44 ? 12 : 0, i, mk = x + 8 + o[2] * (w - 16) / 127;
    draw_poly(x + 3, y + 2, w - 6, h - 4 - sh, wv, 112, K_WAVE);
    if (!sh) return;
    g_rect(x + 8, y + h - 9, w - 16, 1, K_LINE);
    for (i = 0; i < 4; i++) {
        int32_t tx = x + 8 + i * (w - 16) / 3;
        g_rect(tx, y + h - 11, 1, 5, K_LINE);
        g_text_c(T_XS, tx + (i == 0 ? 8 : i == 3 ? -8 : 0), y + h - 20, W[i], K_GREY);
    }
    g_rect(mk - 1, y + h - 13, 3, 9, K_LIME);
}
static void vis_smp(int32_t x, int32_t y, int32_t w, int32_t h)   /* OSC C: the sample's shape, where the newest note is in it */
{
    const ks_smp_t *sm;
    int32_t W = w - 8, cy = y + h / 2, H = h / 2 - 3, i;
    uint32_t at, lp;
    if (!patch[SM_SEL]) { g_text_c(T_XS, x + w / 2, cy - 4, "OFF \xB7 TURN SAMPLE", K_GREY); return; }
    if (patch[SM_SEL] == SM_RS && !rs_desc.len) { g_text_c(T_XS, x + w / 2, cy - 4, rs_want ? "RECORDING THE OUTPUT..." : "EMPTY \xB7 RESAMPLE: TURN +", rs_want ? K_LIME : K_GREY); return; }
    sm = sm_desc(patch[SM_SEL]);
    lp = patch[SM_MODE] == 1 ? (sm->loop < sm->len ? sm->loop : 0u) : patch[SM_MODE] == 2 ? sm->len : sm->loop;
    {
        uint32_t s0 = (uint32_t)patch[SM_START] * sm->len >> 7, e0 = sm->len - ((uint32_t)patch[SM_END] * sm->len >> 7);
        for (i = 0; i < W; i++) {               /* its peaks, column by column (the looped part brighter; START / END: what is cut, dim) */
            uint32_t a = (uint32_t)i * sm->len / (uint32_t)W, b = (uint32_t)(i + 1) * sm->len / (uint32_t)W, j;
            int32_t pk = 0;
            uint16_t c = lp < sm->len && a >= lp ? K_WAVE : g_mix(K_LINE, K_WAVE, 170);
            for (j = a; j < b; j += 4u) { int32_t v = KS_MULAW[sm == &rs_desc ? rs_buf[j] : KS_SMP_DATA[sm->off + j]]; if (v < 0) v = -v; if (v > pk) pk = v; }
            if (a < s0 || a >= e0) c = g_mix(K_PANEL, K_LINE, 120);
            g_rect(x + 4 + i, cy - pk * H / 32768, 1, 1 + 2 * (pk * H / 32768), c);
        }
        if (s0) g_rect(x + 4 + (int32_t)(s0 * (uint32_t)W / sm->len), y + 2, 1, h - 4, K_ORANGE);   /* (the cuts) */
        if (e0 < sm->len) g_rect(x + 4 + (int32_t)(e0 * (uint32_t)W / sm->len) - 1, y + 2, 1, h - 4, K_ORANGE);
    }
    if (ks_sounding) {                          /* (the newest note) */
        at = (sm_show >> 16) % sm->len;
        g_rect(x + 4 + (int32_t)(at * (uint32_t)W / sm->len), y + 2, 2, h - 4, K_LIME);
    }
}
static void vis_osc(uint32_t k, int32_t x, int32_t y, int32_t w, int32_t h)
{
    const int8_t *o = patch + k * OSC_B;
    switch ((uint32_t)o[0] % NENG) {
    case E_WAVE: vis_wt(o, x, y, w, h); break;
    case E_FM: vis_fm(o, k, x, y, w, h); break;
    case E_CS: vis_cs(o, k ? prev_b : prev_a, x, y, w, h); break;
    case E_KICK: vis_kick(o, x, y, w, h); break;
    case E_GROWL: vis_growl(o, (uint32_t)patch[k ? B_P4 : A_P4] >> 4 & 7u, x, y, w, h); break;
    case E_ADD: vis_add(o, (uint32_t)patch[k ? B_P4 : A_P4] >> 4 & 7u, x, y, w, h); break;
    case E_ANALOG: vis_analog(o, k ? prev_b : prev_a, x, y, w, h); break;
    default: draw_poly(x + 3, y + 2, w - 6, h - 4, k ? prev_b : prev_a, 112, K_WAVE); break;
    }
}
static void vis_fx(uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t px, W = w - 10, x0 = x + 5, cy = y + h / 2, H2 = h / 2 - 3;
    uint16_t fill = g_mix(K_PANEL, K_WAVE, 70);
    if (!g_vis(y, h)) return;
    switch (kind) {
    case VZ_DRIVE: {                            /* the transfer curve: in -> out */
        int32_t g = 256 + patch[DR_AMT] * patch[DR_AMT] * 3 / 4;
        g_line(x0, cy + H2, x0 + W, cy - H2, K_LINE);
        for (px = 0; px < W; px++) {
            int32_t in = (px * 65534 / (W - 1)) - 32767, o = (int32_t)(((int64_t)in * g) >> 8);
            switch (patch[DR_TYPE]) {
            case 0: o = sat15(o); break;
            case 1: o = sat15(o + (int32_t)(((int64_t)o * o) >> 17)); break;
            case 2: o = clip(o, 26000) * 5 / 4; break;
            default: o = lfold(o); break;
            }
            g_px(x0 + px, cy - o * H2 / 32768, K_WAVE);
            g_px(x0 + px, cy - o * H2 / 32768 + 1, K_WAVE);
        }
        break;
    }
    case VZ_CRUSH: {                            /* one cycle, held and stepped */
        int32_t bits = 16 - ((patch[CR_BITS] * 14) >> 7), hold = 1 + ((patch[CR_RATE] * 31) >> 7), lv = 1 << (bits > 5 ? 5 : bits - 1);
        for (px = 0; px < W; px++) {
            int32_t q = px - px % (hold > 1 ? hold : 1), s = sin32((uint32_t)q * (0xFFFFFFFFu / (uint32_t)W));
            s = (s * lv / 32768) * 32767 / lv;
            g_col(x0 + px, cy - s * H2 / 32768, cy, fill, K_WAVE);
        }
        break;
    }
    case VZ_CHORUS: {                           /* left and right, sliding against each other */
        uint32_t t = fm1_ms * (uint32_t)(1 + patch[CH_RATE]) * 40000u;
        int32_t dep = patch[CH_DEP] * 40;
        for (px = 0; px < W; px++) {
            uint32_t ph = (uint32_t)px * (0xFFFFFFFFu / (uint32_t)W) * 2u;
            g_px(x0 + px, cy - sin32(ph + (uint32_t)((sin32(t) * dep) >> 4)) * H2 / 32768, K_WAVE);
            g_px(x0 + px, cy - sin32(ph - (uint32_t)((sin32(t) * dep) >> 4)) * H2 / 32768, K_CYAN);
        }
        break;
    }
    case VZ_DELAY: {                            /* the repeats: spacing by the time, height by the feedback; each type its look */
        int32_t sp = 6 + (patch[DL_TIME] - 3) * 3, a = 32767, k = 0, ty = patch[DL_TYPE] & 3;
        for (px = x0; px < x0 + W && a > 600; px += sp, k++) {
            int32_t hh = a * (h - 8) / 32767, jx = ty == 1 ? (int32_t)((((uint32_t)k * 2654435761u) >> 30) & 3u) - 1 : 0;   /* (TAPE: a wobble) */
            uint16_t c = k == 0 ? K_WHITE : ty == 2 ? K_WAVE : ty == 3 ? g_mix(K_WAVE, K_BG, (uint32_t)(k * 22 > 160 ? 160 : k * 22)) : (k & 1) ? K_WAVE : K_CYAN;
            g_rect(px + jx, y + h - 4 - hh, ty == 1 ? 5 : 4, hh, c);
            if (ty == 1 && k) g_rect(px + jx - 1, y + h - 4 - hh, 1, hh, g_mix(K_BG, c, 90));
            a = k == 0 ? a * 3 / 4 : mul15(a, patch[DL_FB] * (ty == 3 ? 270 : 250));
            if (ty == 2 && k) a = a * 31 / 32 + 0;
        }
        g_text_r(T_XS, x + w - 4, y + 3, L_DLT[ty], K_GREY);
        break;
    }
    case VZ_REVERB: {                           /* the tail: ROOM early spikes and short, HALL long, PLATE dense, SPRING wobbling */
        static const int16_t DK[4] = {120, 0, -20, 40};
        int32_t ty = patch[RV_TYPE] & 3, a = 32767, dec = clampi(32767 - (127 - patch[RV_SIZE]) * 6 - 40 - DK[ty] * 2, 28000, 32760);
        for (px = 0; px < W; px++) {
            uint32_t hsh = (uint32_t)px * 2654435761u;
            int32_t r = (int32_t)((hsh >> 24) & 255u) * a >> 8;
            if (ty == 2) r = (r + a) >> 1;      /* (dense) */
            if (ty == 3) r = mul15(r, 16384 + (sin32((uint32_t)px * 0x0A000000u) >> 1));
            if (ty == 0 && px < W / 6 && (hsh >> 29) == 0u) r = a;   /* (early reflections) */
            g_col(x0 + px, y + h - 4 - r * (h - 8) / 32767, y + h - 4, fill, ty == 2 ? K_WHITE : K_WAVE);
            a = mul15(a, dec);
        }
        g_text_r(T_XS, x + w - 4, y + 3, L_RVT[ty], K_GREY);
        break;
    }
    case VZ_COMP: {                             /* the curve in -> out (the knee, the ratio), the gain taken now, the PUMP */
        int32_t amt = patch[CP_AMT], thr = 32767 - amt * 200, r = 2 + amt / 16, gw = h - 8, gx = x0, gy = y + 4;
        int32_t gr = 32767 - clampi(meter_cp, 0, 32767);
        g_rect(gx, gy + gw, gw, 1, K_LINE); g_rect(gx, gy, 1, gw, K_LINE);
        g_line(gx, gy + gw, gx + gw, gy, K_LINE);
        for (px = 0; px < gw; px++) {
            int32_t in = px * 32767 / (gw - 1), o2 = in > thr ? thr + (in - thr) / r : in;
            o2 = (int32_t)(((int64_t)o2 * (32768 + amt * 150)) >> 15);
            if (o2 > 32767) o2 = 32767;
            g_px(gx + px, gy + gw - o2 * gw / 32768, K_WAVE);
            g_px(gx + px, gy + gw - o2 * gw / 32768 + 1, g_mix(K_BG, K_WAVE, 120));
        }
        g_text(T_XS, gx + gw + 8, y + 4, "GAIN TAKEN", K_GREY);
        g_bar(gx + gw + 8, y + 15, W - gw - 10, 5, (uint32_t)(gr >> 6) > 256u ? 256u : (uint32_t)(gr >> 6), K_ORANGE);
        if (patch[CP_PUMP] && h > 32) {         /* one turn of the PUMP */
            int32_t pw = W - gw - 10, px0 = gx + gw + 8, ppy = 0;
            for (px = 0; px < pw; px++) {
                int32_t g = pump_gain((uint32_t)(px * (0x1000000 / pw)) << ((patch[CP_PRATE] & 3) == 3 ? 2 : (patch[CP_PRATE] & 3) == 2 ? 1 : 0), 0u, patch[CP_PUMP]);
                int32_t yy = y + h - 4 - g * (h - 30) / 32768;
                if (px) g_line(px0 + px - 1, ppy, px0 + px, yy, K_CYAN);
                ppy = yy;
            }
            g_text(T_XS, px0, y + 24, "PUMP", K_CYAN);
        }
        break;
    }
    case VZ_PHASER: {                           /* the notches, where the LFO has them now */
        int32_t lfo = (tri32(fx.phph) + 32768) >> 1, n1 = 10 + mul15(lfo, patch[PH_DEP] * 160) * (W - 40) / 32768, py0 = 0;
        for (px = 0; px < W; px++) {
            int32_t d1 = px - n1, d2 = px - n1 * 2 - 12, g, yy;
            d1 = d1 < 0 ? -d1 : d1; d2 = d2 < 0 ? -d2 : d2;
            g = 32767 - (d1 < 16 ? (16 - d1) * 1800 : 0) - (d2 < 22 ? (22 - d2) * 1300 : 0);
            g = mul15(clampi(g, 2000, 32767), 16384 + patch[PH_MIX] * 129) + (32767 - (16384 + patch[PH_MIX] * 129));
            yy = y + h - 4 - clampi(g, 0, 32767) * (h - 8) / 32768;
            if (px) g_line(x0 + px - 1, py0, x0 + px, yy, K_WAVE);
            py0 = yy;
        }
        break;
    }
    case VZ_OTT: {                              /* MULTIBAND: each band's gain now, up (lime) or down (orange) from x1 */
        static const char *const BN[3] = {"LOW", "MID", "HIGH"};
        int32_t k, bw = (W - 16) / 3;
        for (k = 0; k < 3; k++) {
            int32_t bx = x0 + 4 + k * (bw + 4), g = patch[OT_AMT] ? ott_show[k] : 4096, mid = cy, hh = (g - 4096) * H2 / 8192;
            g_rect(bx, mid, bw, 1, K_LINE);
            if (hh > 0) g_rect(bx + 2, mid - hh, bw - 4, hh, K_WAVE); else if (hh < 0) g_rect(bx + 2, mid, bw - 4, -hh, K_ORANGE);
            g_text_c(T_XS, bx + bw / 2, y + h - 12, BN[k], K_GREY);
        }
        break;
    }
    case VZ_MASTER: {                           /* the output's peak and what the glue takes */
        int32_t pk = clampi(meter_pk, 0, 32767), gr = 32767 - clampi(meter_gr, 0, 32767), bw = W - 30;
        g_text(T_XS, x0, y + 6, "OUT", K_GREY);
        g_bar(x0 + 28, y + 7, bw, 6, (uint32_t)(pk >> 7), pk > 30000 ? K_ORANGE : K_WAVE);
        g_text(T_XS, x0, y + h - 14, "GLUE", K_GREY);
        g_bar(x0 + 28, y + h - 13, bw, 6, (uint32_t)(gr >> 6), K_CYAN);
        break;
    }
    default:                                     /* CPU: the block's share */
        g_text(T_XS, x0, y + h / 2 - 7, "C0", K_GREY);
        g_bar(x0 + 20, y + h / 2 - 6, W - 20, 6, ks_load[0] * 256u / 100u, ks_load[0] > 70u ? K_ORANGE : K_WAVE);
        break;
    }
}
static void draw_visual(uint32_t kind, int32_t x, int32_t y, int32_t w, int32_t h, int box)
{
    if (!kind || !g_vis(y, h)) return;
    if (box) g_box(x, y, w, h, K_PANEL, K_LINE);
    switch (kind) {
    case VZ_SCOPE:
        if (ks_sounding) {                      /* the last ~20 ms of sound, from a rising zero crossing */
            uint32_t sw = scope_w - 512u, k, at = 0;
            for (k = 1; k < 288u; k++) if (scope[(sw + k - 1u) & 511u] < 0 && scope[(sw + k) & 511u] >= 0) { at = k; break; }
            for (k = 0; k < 224u; k++) live[k] = scope[(sw + at + k) & 511u];
            draw_poly(x + 3, y + 2, w - 6, h - 4, live, 224, K_WAVE);
        } else
            draw_poly(x + 3, y + 2, w - 6, h - 4, prev_wave, 224, K_WAVE);
        break;
    case VZ_OSCA: vis_osc(0, x, y, w, h); break;
    case VZ_SMP: vis_smp(x, y, w, h); break;
    case VZ_OSCB: vis_osc(1, x, y, w, h); break;
    case VZ_SUB: draw_poly(x + 3, y + 2, w - 6, h - 4, prev_s, 112, K_WAVE); break;
    case VZ_MIX: draw_poly(x + 3, y + 2, w - 6, h - 4, prev_wave, 224, K_WAVE); break;
    case VZ_ENV0: case VZ_ENV1: case VZ_ENV2: vis_env(x, y, w, h, kind - VZ_ENV0); break;
    case VZ_FILT: vis_filter(x, y, w, h); break;
    case VZ_LFO1: case VZ_LFO2: vis_lfo(x, y, w, h, kind - VZ_LFO1); break;
    case VZ_STEP: vis_step(x, y, w, h); break;
    case VZ_SEQ: vis_seq(x, y, w, h); break;
    case VZ_DRUM: vis_drum(x, y, w, h); break;
    default: vis_fx(kind, x, y, w, h); break;
    }
}

/* ---------------------------------------------------------------- the titles */
static const char *view_title(uint32_t vi, char *t, char *sub)
{
    const view_t *v = &VIEWS[vi];
    char *p;
    *sub = 0;
    switch (v->lay) {
    case LY_PLAY:
        p = str_cat(sub, L_STYLE[clampi(patch[P_STYLE], 0, NSTYLE - 1)]);
        p = str_cat(p, " \xB7 ");
        p = eng_str(p, patch);
        p = str_cat(p, set.bank ? " \xB7 B" : " \xB7 A");
        fmt_u(p, (uint32_t)set.idx + 1u, 2);
        return pname;
    case LY_PRESET:
        p = str_cat(sub, vi == PAGE_V0[PG_PRE] ? "FACTORY \xB7 " : "USER \xB7 ");
        p = fmt_u(p, vi == PAGE_V0[PG_PRE] ? NFACTORY : NUSER, 0);
        p = str_cat(p, " \xB7 TAG ");
        str_cat(p, L_TAG[pre_tag]);
        return "SELECT PATCH";
    case LY_SAVE: str_cat(sub, "PRESS SAVE AGAIN TO WRITE"); return pname;
    case LY_SYS:
        if (vi != PAGE_V0[PG_SYS]) { str_cat(sub, "LEARN \xB7 CHANNEL \xB7 CLOCK \xB7 START / STOP \xB7 PROGRAM"); return "MIDI"; }
        str_cat(sub, "DNB / DUB / DUBSTEP / TECHNO EDITION"); return "DEVICE SETUP";
    case LY_LFO: {
        uint32_t k = vi - PAGE_V0[PG_LFO];
        const int8_t *q = patch + k * LFO_2;
        p = str_cat(t, k ? "LFO 2 \xB7 " : "LFO 1 \xB7 ");   /* (1.0.38: the number big, WOBBLE / MOTION in the small header) */
        item_text(k ? I_L2RATE : I_L1RATE, p);
        p = str_cat(sub, L_SHAPE[q[L1_SHP]]);
        p = str_cat(p, " \xB7 ");
        p = str_cat(p, dst_name((uint32_t)q[L1_DST]));
        *p++ = ' ';
        item_text(L1_DEP + k * LFO_2, p);
        return t;
    }
    case LY_STEP:
        p = str_cat(t, "STEP ");
        str_cat(p, L_SPAT[patch[ST_PAT] & 15]);
        p = str_cat(sub, L_SRATE[clampi(patch[ST_RATE], 0, 5)]);
        p = str_cat(p, " \xB7 GATE ");
        item_text(ST_GATE, p); p += str_len(p);
        p = str_cat(p, " \xB7 ");
        str_cat(p, patch[ST_ON] ? dst_name((uint32_t)patch[ST_DST]) : "OFF");
        return t;
    default: break;
    }
    if (vi == V_SEQ) {                          /* the pattern; playing or not, REC, its rate */
        p = str_cat(sub, sq_play ? "PLAYING" : "PLAY = START");
        if (sq_rec) p = str_cat(p, " \xB7 REC");
        p = str_cat(p, " \xB7 ");
        p = str_cat(p, L_SRATE[sq_pat()->rate % 6u]);
        str_cat(p, sq_play ? (sq_rec ? " \xB7 KEYS RECORD" : " \xB7 KEYS TRANSPOSE") : sq_rec ? " \xB7 KEYS WRITE" : " \xB7 REC = WRITE");
        item_text(G_SQ_PAT, t);
        return t;
    }
    if (vi == V_DRUM) {                         /* the pattern, the kit, the instrument, the page; what the keys do */
        p = str_cat(t, "DRUMS P");
        p = fmt_u(p, dk.cur + 1u, 0);
        if (dk_psel) { p = str_cat(p, " "); str_cat(p, DKP[(dk_psel - 1u) / 3u % NDKP].name); }
        p = str_cat(str_cat(sub, dk_kname(dk.kit)), " \xB7 ");
        p = str_cat(p, dk_tname(dk_trk));
        if (dk_len() > 16u) { p = str_cat(p, " \xB7 PAGE "); p = fmt_u(p, dk_cur / 16u + 1u, 0); p = str_cat(p, "/"); p = fmt_u(p, (dk_len() + 15u) / 16u, 0); }
        if (sq_rec) str_cat(p, sq_play ? " \xB7 REC LIVE" : " \xB7 REC");
        else str_cat(p, sq_play ? " \xB7 PLAYING" : " \xB7 BLACK: INSTR \xB7 WHITE: STEPS");
        return t;
    }
    if (vi == V_ARP) {
        p = str_cat(sub, L_SRATE[perf.ar_rate % 6u]);
        p = str_cat(p, " \xB7 ");
        p = fmt_u(p, perf.ar_oct + 1u, 0);
        str_cat(p, perf.ar_oct ? " OCTAVES \xB7 PLAY HELD = HOLD" : " OCTAVE \xB7 PLAY HELD = HOLD");
        p = str_cat(t, "ARP ");
        str_cat(p, perf.ar_on ? L_ARMODE[perf.ar_mode % 5u] : "OFF");
        return t;
    }
    if (page == PG_BASS && vi <= 3u && vi >= 2u) { uint32_t e = (uint32_t)patch[vi == 2u ? A_ENG : B_ENG] % NENG; str_cat(sub, ENG_SUB[e]); return ENG_TITLE[e]; }
    if (page == PG_BASS && vi == V_SMP) {       /* OSC C: the sample, how it plays */
        if (!patch[SM_SEL]) { str_cat(sub, "18 SOUNDS \xB7 9 TEXTURES \xB7 RESAMPLE"); return "NO SAMPLE"; }
        if (patch[SM_SEL] == SM_RS && !rs_desc.len) { str_cat(sub, "THE OUTPUT, 2 .. 4 s, INTO OSC C"); return "RESAMPLE"; }
        {
            const ks_smp_t *sm = sm_desc(patch[SM_SEL]);
            int keys = patch[SM_KEY] == 1 || (patch[SM_KEY] == 0 && sm->keys);
            int loops = patch[SM_MODE] == 1 || (patch[SM_MODE] == 0 && sm->loop < sm->len);
            p = str_cat(sub, sm->keys ? "SOUND" : "TEXTURE");
            p = str_cat(p, loops ? " \xB7 LOOP" : " \xB7 ONCE");
            if (patch[SM_DIR]) p = str_cat(p, patch[SM_DIR] == 1 ? " \xB7 REVERSE" : " \xB7 PING-PONG");
            if (patch[SM_SPEED]) p = str_cat(p, patch[SM_SPEED] > 0 ? " \xB7 SLOW" : " \xB7 FAST");
            str_cat(p, keys ? " \xB7 FROM THE KEYS" : " \xB7 FIXED PITCH");
            return sm->name;
        }
    }
    if (page == PG_BASS && vi == 4u) { str_cat(sub, patch[S_LOCK] ? "LOW END \xB7 LOCKED ON OSC A" : "LOW END CHARACTER"); return "SUB / NOISE"; }
    if (page == PG_BASS) { str_cat(sub, "TWO ENGINES, ONE BASS"); return patch[X_MODE] == 0 ? "A + B" : patch[X_MODE] == 1 ? "A x B  RING" : "B > A  X-MOD"; }
    if (page == PG_ENV) {
        if (vi == PAGE_V0[PG_ENV] + 2u) str_cat(sub, dst_name((uint32_t)patch[ME_DST]));
        return vi == PAGE_V0[PG_ENV] ? "ADSR CONTROL" : vi == PAGE_V0[PG_ENV] + 1u ? "FILTER ADSR" : "PITCH / MOD";
    }
    if (page == PG_FILT) { str_cat(sub, FT_SUB[patch[F_TYPE]]); return FT_TITLE[patch[F_TYPE]]; }
    if (page == PG_LFO && vi == PAGE_V0[PG_LFO] + 4u) {   /* LFO 3: its shape, where it goes, how much */
        char *q = str_cat(sub, L_SHAPE[clampi(set.l3[L3_SHP], 0, 8)]);
        q = str_cat(q, " \xB7 "); q = str_cat(q, dst_name((uint32_t)set.l3[L3_DST])); *q++ = ' ';
        lk_text(L1_DEP, set.l3[L3_DEP], q);
        return "LFO 3";
    }
    if (page == PG_LFO) { str_cat(sub, "4 SLOTS \xB7 SOURCE > DEST"); return "ROUTING"; }
    {
        uint32_t f = vi - PAGE_V0[PG_FX];
        p = sub;
        if (f >= NFXV) f = 0;
        if (f == 0u) p = str_cat(p, L_DRT[patch[DR_TYPE] & 3]);
        if (f == 3u) p = str_cat(p, "OTT \xB7 3 BANDS");
        if (f == 4u && patch[CP_PUMP]) { p = str_cat(p, "PUMP "); p = str_cat(p, L_PRATE[patch[CP_PRATE] & 3]); }
        if (f == 2u) { p = str_cat(p, L_DLT[patch[DL_TYPE] & 3]); p = str_cat(p, " "); p = str_cat(p, L_DIV[patch[DL_TIME] & 15]); }
        if (f == 1u) p = str_cat(p, L_RVT[patch[RV_TYPE] & 3]);
        if (f == 9u) { p = str_cat(p, "MONO BASS "); p = str_cat(p, L_MONO[(uint32_t)patch[MB_MONO] % 7u]); }
        if (p != sub) p = str_cat(p, " \xB7 ");
        str_cat(p, FX_ON[f] ? (patch[FX_ON[f]] ? "ON" : "OFF") : f == 3u ? (patch[OT_AMT] ? "ON" : "OFF") : "ALWAYS ON");
        return FX_TITLE[f];
    }
}

/* ---------------------------------------------------------------- rows, cells, boxes */
static void draw_row(int32_t y, const char *lab, const char *val, int sel, uint16_t valc);
static void rb_frame(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t edge, uint16_t c0, uint16_t c1);
static void row_g(int32_t x, int32_t y, int32_t w, int32_t h, const char *lab, const char *val, int sel, uint16_t valc, const ks_font_t *f)
{
    int32_t ty, room;
    const ks_font_t *fl = f, *fv;
    if (!g_vis(y, h)) return;
    if (theme == TH_GLOW || theme == TH_CLASSIC || (g_rad && !g_rflat)) {   /* GLOW, CLASSIC (flat) (and the RAD palettes): the row chosen lit from the left, the others only a line */
        if (sel) { g_box(x, y, w, h, K_PANEL, K_LIME); g_hgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_PANEL, K_LIME, 120), K_PANEL); }
        else g_hgrad(x + 4, y + h - 1, w - 8, 1, g_mix(K_BG, K_LIME, 90), K_BG);
    } else if (theme == TH_RACK) rb_frame(x, y, w, h, 6, sel ? K_LIME : K_LINE, sel ? K_LIMED : K_PANEL, sel ? K_LIMED : K_PANEL);   /* (RACK: rounder) */
    else g_box(x, y, w, h, sel ? K_LIMED : K_PANEL, sel ? K_LIME : K_LINE);
    room = w - 16 - 8 - g_text_w(fl, lab);      /* (the label, a gap, the value: smaller if they would touch) */
    fv = g_fit(f, val, room);
    if (g_text_w(fv, val) > room) { fl = fv = g_fit(f, lab, w / 2); fv = g_fit(fv, val, w - 24 - g_text_w(fl, lab)); }
    ty = y + (h - fl->h) / 2 + 1;
    g_text(fl, x + 8, ty, lab, K_WHITE);
    g_text_r(fv, x + w - 8, y + (h - fv->h) / 2 + 1, val, sel ? K_LIME : valc);
}
static void draw_row(int32_t y, const char *lab, const char *val, int sel, uint16_t valc) { row_g(6, y, 228, 24, lab, val, sel, valc, T_S); }
static void rows_g(const view_t *v, int32_t x, int32_t y0, int32_t w, int32_t h, int32_t step, uint32_t vis, const ks_font_t *f)
{
    uint32_t c = cursor[vidx()], top = 0, i;
    if (v->n > vis) {
        uint32_t at = vis >= 3u ? 2u : vis - 1u;   /* (the row chosen: third from the top, or the last one shown in a short list) */
        top = c >= at ? c - at : 0u;
        if (top + vis > v->n) top = v->n - vis;
        if (g_vis(y0, (int32_t)vis * step)) {   /* where we are in the list */
            int32_t hh = (int32_t)vis * step - (step - h), bh = hh * (int32_t)vis / v->n;
            g_rect(x + w + 2, y0, 2, hh, K_TRACK);
            g_rect(x + w + 2, y0 + (hh - bh) * (int32_t)top / (int32_t)(v->n - vis), 2, bh, K_GREY);
        }
    }
    for (i = 0; i < vis && top + i < v->n; i++) {
        uint32_t id = v->it[top + i];
        char b[24];
        item_text(id, b);
        row_g(x, y0 + (int32_t)i * step, w, h, item_name(id), b, top + i == c, id == G_CPU ? K_GREY : K_CYAN, f);
    }
}
static void draw_vbox(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t id, int sel, const char *lab)
{
    char b[24];
    if (!g_vis(y, h)) return;
    item_text(id, b);
    g_box(x, y, w, h, sel ? K_LIMED : K_PANEL, sel ? K_LIME : K_LINE);
    if (theme == TH_GLOW || theme == TH_CLASSIC || (g_rad && !g_rflat)) g_vgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_PANEL, K_LIME, sel ? 100 : 36), g_rad ? g_rcell : K_BG);   /* (GLOW, RAD: lit from the top) */
    g_text(T_XS, x + 7, y + 6, lab ? lab : item_name(id), sel ? K_LIME : K_GREY);
    if (h >= 50) g_text(g_fit(T_B, b, w - 14), x + 7, y + 18, b, sel ? K_LIME : K_WHITE);
    else g_text(g_fit(T_M, b, w - 14), x + 7, y + 17, b, sel ? K_LIME : K_WHITE);
    g_vbar(x + 7, y + h - 10, w - 14, 5, item_frac(id), sel ? K_LIME : K_CYAN);
}
static void draw_cell(const view_t *v, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t id, int sel)   /* STUDIO */
{
    char b[24];
    const char *tag = mod_tag(id);
    int32_t k = knob_of(v, id);
    if (!g_vis(y, h)) return;
    item_text(id, b);
    g_box(x, y, w, h, sel ? K_LIMED : K_PANEL, sel ? K_LIME : K_LINE);
    if (g_rad && !g_rflat) g_vgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_PANEL, K_LIME, sel ? 100 : 40), g_rcell);
    if (k >= 0) {
        char kn[3] = {'K', (char)('1' + k), 0};
        g_rect(x + 5, y + 5, 5, 5, PAL_BW ? K_WHITE : KNOB_RGB[k]);
        g_text(T_XS, x + 13, y + 4, kn, K_GREY);
    }
    if (tag) {
        int32_t tw = g_text_w(T_XS, tag) + 6;
        g_box(x + w - tw - 3, y + 3, tw, 10, K_MOD, K_MOD);
        g_text(T_XS, x + w - tw, y + 4, tag, K_INK);
    }
    if (h < 50) g_text(g_fit(T_XS, item_sname(id), w - (k >= 0 ? 30 : 8)), x + (k >= 0 ? 26 : 5), y + 4, item_sname(id), K_GREY);   /* (the short cell: name, value, bar in 44 px) */
    else g_text(T_XS, x + 5, y + 16, item_sname(id), K_GREY);
    {                                           /* (too long even small: cut, with a dot) */
        const ks_font_t *f = g_fit(T_M, b, w - 9);
        uint32_t n = str_len(b);
        while (n > 2u && g_text_w(f, b) > w - 9) { b[--n] = 0; b[n - 1u] = '.'; }
        g_text(f, x + 5, y + (h < 50 ? 16 : 27), b, sel ? K_LIME : K_WHITE);
    }
    g_vbar(x + 5, y + h - (h < 50 ? 8 : 9), w - 10, 4, item_frac(id), sel ? K_LIME : K_CYAN);
}

/* the foot: what the knobs do, as many as fit */
static void foot_text(const view_t *v, char *b, int32_t maxw)   /* all four if they fit, else shorter (K1 -> 1, names cut), else fewer */
{
    char *p;
    uint32_t k, n = 4, tight = 0;
    for (;;) {
        p = b;
        *p = 0;
        for (k = 0; k < n; k++) {
            const char *nm;
            uint32_t j;
            if (vknob(v, k) == I_NONE) continue;
            if (p != b) p = str_cat(p, tight ? " " : " \xB7 ");
            if (!tight) *p++ = 'K';
            *p++ = (char)('1' + k); *p++ = tight ? ':' : ' ';
            nm = item_sname(vknob(v, k));
            for (j = 0; nm[j] && (tight < 2u || j < 5u); j++) *p++ = nm[j];
            *p = 0;
        }
        if (g_text_w(T_XS, b) < maxw || n <= 1u) break;
        if (tight < 2u) tight++; else n--;
    }
}
static const char *foot_fixed(uint32_t vi)
{
    switch (VIEWS[vi].lay) {
    case LY_PRESET: return "K1 SCROLL \xB7 K2 FAVS \xB7 K3 LOAD \xB7 K4 TAG";
    case LY_SAVE: return "KEYS TYPE \xB7 K1 SLOT \xB7 K2 PLACE";
    default: return 0;
    }
}
static void view_dots(int32_t xr, int32_t y)    /* the page's views: one dot each, the one shown lit */
{
    uint32_t nv = (uint32_t)(PAGE_V0[page + 1u] - PAGE_V0[page]), i;
    int32_t x = xr - (int32_t)nv * 5;
    for (i = 0; nv > 1u && i < nv; i++) g_rect(x + (int32_t)i * 5, y + 1, 3, 3, i == vcur[page] ? K_LIME : K_LINE);
}
/* the battery, top right: five segments (the percentage only in SYSTEM); returns its left edge */
static int32_t batt_draw(int32_t xr, int32_t y, int pct)
{
    int32_t x = xr - 18, lit = (batt_pct + 19) / 20, i, l = x;
    uint16_t col = lit <= 1 ? K_ORANGE : K_LIME;
    g_box(x, y + 1, 17, 7, K_BG, K_GREY);
    g_rect(x + 17, y + 3, 1, 3, K_GREY);
    for (i = 0; i < 5; i++) g_rect(x + 2 + i * 3, y + 3, 2, 3, i < lit ? col : K_LINE);
    if (pct) {
        char b[8];
        str_cat(fmt_u(b, (uint32_t)batt_pct, 0), "%");
        g_text_r(T_XS, x - 3, y, b, K_WHITE);
        l = x - 3 - g_text_w(T_XS, b);
    }
    return l;
}

/* ---------------------------------------------------------------- the lists that every theme shares */
static void presets_g(uint32_t bank, int32_t x, int32_t y0, int32_t w, int32_t h, int32_t step, uint32_t vis, const ks_font_t *f)
{
    uint32_t c = pre_cur[bank], n = bank ? NUSER : NFACTORY, top, i, m = 0, ci = 0, k;
    uint8_t lst[NFACTORY > NUSER ? NFACTORY : NUSER];
    for (k = 0; k < n; k++) if (pre_match(bank, k)) { if (k == c) ci = m; lst[m++] = (uint8_t)k; }   /* (a TAG: only those) */
    if (!m) { if (g_vis(y0, 30)) { char t[32]; str_cat(str_cat(t, "NO SOUND TAGGED "), L_TAG[pre_tag]); g_text_c(T_S, x + w / 2, y0 + 8, t, K_GREY); } return; }
    top = ci >= 2u ? ci - 2u : 0u;
    if (vis > m) vis = m;
    if (top + vis > m) top = m - vis;
    for (i = 0; i < vis; i++) {
        k = lst[top + i];
        int32_t y = y0 + (int32_t)i * step;
        char b[24], *p;
        const char *nm = bank ? (up_valid(k) ? ustore[k].name : "- EMPTY -") : FACTORY[k].name;
        if (!g_vis(y, h)) continue;
        p = fmt_u(b, k + 1u, 2);
        *p++ = fav_is(bank, k) ? '*' : ' '; *p++ = ' ';
        str_cat(p, nm);
        g_box(x, y, w, h, k == c ? K_LIMED : K_PANEL, k == c ? K_LIME : K_LINE);
        g_text(f, x + 8, y + (h >= 30 ? 4 : (h - f->h) / 2 + 1), b, k == c ? K_LIME : K_WHITE);
        if (!bank || up_valid(k)) {             /* its music and its engines, small */
            int8_t pp[NP_MAX];
            char nb[14], e[32];
            if (bank) user_get(k, pp); else preset_factory(k, pp, nb);
            eng_str(e, pp);
            if (h >= 30) {
                g_text(T_XS, x + 8, y + h - 11, e, K_CYAN);
                g_text_r(T_XS, x + w - 16, y + h - 11, L_STYLE[clampi(pp[P_STYLE], 0, NSTYLE - 1)], K_MOD);
            } else {                            /* (only what fits beside the name: wide fonts) */
                const char *st = L_STYLE[clampi(pp[P_STYLE], 0, NSTYLE - 1)];
                int32_t room = w - 30 - g_text_w(f, b);
                if (g_text_w(T_XS, st) < room) g_text_r(T_XS, x + w - 14, y + h / 2 - 8, st, k == c ? K_LIME : K_GREY);
                if (g_text_w(T_XS, e) < room) g_text_r(T_XS, x + w - 14, y + h / 2 + 1, e, K_CYAN);
            }
        }
        if (set.bank == bank && set.idx == k) { /* the one playing */
            int32_t j;
            for (j = 0; j < 4; j++) g_rect(x + w - 10 + j, y + h / 2 - 4 + j, 1, 8 - 2 * j, k == c ? K_LIME : K_CYAN);
        }
    }
}
static void save_g(int32_t x, int32_t y0, int32_t w, int32_t ty)   /* the name being written, its slot */
{
    int32_t cx = x, i;
    for (i = 0; i < (int32_t)save_pos; i++) { char ch[2] = {pname[i], 0}; cx += g_text_w(T_L, ch); }
    if (g_vis(ty + T_L->h, 2)) { char ch[2] = {pname[save_pos] ? pname[save_pos] : ' ', 0}; g_rect(cx, ty + T_L->h, g_text_w(T_L, ch), 2, K_LIME); }
    {                                           /* one row per knob: its name in the middle column, the value on the right */
        char v1[24], v2[8], v3[8], *q;
        int32_t kx = x + w * 45 / 100, r;
        static const char *const KN[5] = {"K1", "K2", "K3", "ALGO", "K4"};
        const char *lab[5] = {"SLOT", "PLACE", "LETTER", "STYLE", "NEW"}, *val[5];
        q = str_cat(v1, "B");
        q = fmt_u(q, save_slot + 1u, 2);
        q = str_cat(q, " \xB7 ");
        str_cat(q, up_valid(save_slot) ? ustore[save_slot].name : "EMPTY");
        q = fmt_u(v2, save_pos + 1u, 0);
        str_cat(q, "/13");
        v3[0] = pname[save_pos] && pname[save_pos] != ' ' ? pname[save_pos] : '_'; v3[1] = 0;
        val[0] = v1; val[1] = v2; val[2] = v3; val[3] = L_STYLE[clampi(patch[P_STYLE], 0, NSTYLE - 1)];
        val[4] = new_arm == 1u ? "< PRESET: AGAIN" : new_arm == 2u ? "SONG: AGAIN >" : "<PRESET SONG>";
        for (i = 0; i < 5; i++) {
            r = y0 + i * 20;
            row_g(x - 2, r, w, 18, lab[i], val[i], i == 0, i == 4 && new_arm ? K_LIME : K_CYAN, T_S);
            if (g_vis(r, 18)) g_text(T_XS, kx, r + 5, KN[i], K_LIME);
        }
    }
}

/* ---------------------------------------------------------------- GLOW: simple, gradients, one bright accent */
static void glow_vis(uint32_t vz, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!vz || !g_vis(y, h)) return;
    g_box(x, y, w, h, K_PANEL, g_mix(K_LINE, K_LIME, 90));
    g_vgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_PANEL, K_LIME, 30), K_BG);
    draw_visual(vz, x, y, w, h, 0);
}
static int big_vis(uint32_t vi) { return vi == V_DRUM || vi == V_SEQ || vi == V_SMP; }   /* (the views whose picture is what you play: a pattern, a sample) */
static void th_glow(const view_t *v, uint32_t vi)
{
    char t[32], sub[64], n[8], fb[64], *p;
    const char *ti = view_title(vi, t, sub), *ff = foot_fixed(vi);
    uint32_t vz = VZ_OF[vi], c = cursor[vi], i;
    if (g_vis(0, 36)) {
        int32_t bx = batt_draw(234, 5, v->lay == LY_SYS);
        g_text(T_S, 8, 5, cat_tr(v->cat), K_LIME);
        p = fmt_u(n, (uint32_t)page + 1u, 2);
        g_text_r(T_XS, bx - 6, 7, n, K_GREY);
        view_dots(bx - 6 - g_text_w(T_XS, n) - 6, 8);
        g_hgrad(6, 20, 228, 2, K_LIME, K_BG);
    }
    g_text(T_L, 8, 25, ti, K_WHITE);
    g_text(T_XS, 8, 49, sub, v->lay == LY_LIST && page == PG_FX ? K_ORANGE : K_GREY);
    if (v->lay == LY_PLAY) {
        p = t;
        if (hold) p = str_cat(p, "HOLD \xB7 ");
        p = fmt_u(p, ks_bpm10 / 10u, 0);
        str_cat(p, " BPM");
        g_text_r(T_XS, 232, 49, t, hold ? K_LIME : K_CYAN);
        glow_vis(VZ_SCOPE, 6, 60, 228, 52);
        for (i = 0; i < 4u; i++)
            draw_vbox(i & 1u ? 123 : 6, 116 + (int32_t)(i >> 1) * 52, 111, 47, v->it[i], i == c,
                      v->it[i] == I_L1RATE ? "WOB RATE" : v->it[i] == L1_DEP ? "WOB DEPTH" : 0);
    } else if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], 6, 62, 228, 26, 31, 5, T_S);
    else if (v->lay == LY_SAVE) save_g(8, 64, 228, 37);
    else if (v->lay == LY_BOX) {
        glow_vis(vz, 6, 60, 228, 58);
        for (i = 0; i < 4u; i++) draw_vbox(i & 1u ? 123 : 6, 122 + (int32_t)(i >> 1) * 50, 111, 46, v->it[i], i == c, 0);
    } else if (vz && vi == V_SMP) {             /* (the sample: a lower picture, three rows under it, the one chosen lit) */
        glow_vis(vz, 6, 60, 228, 74);
        rows_g(v, 6, 138, 228, 23, 26, 3, T_S);
    } else if (vz && big_vis(vi)) {
        glow_vis(vz, 6, 60, 228, 100);
        rows_g(v, 6, 164, 228, 23, 26, 2, T_S);
    } else if (vz) {
        glow_vis(vz, 6, 60, 228, 50);
        rows_g(v, 6, 114, 228, 23, 26, 4, T_S);
    } else
        rows_g(v, 6, 62, 228, 26, 30, 5, T_S);
    if (g_vis(218, 20)) {
        g_hgrad(6, 220, 228, 1, K_BG, g_mix(K_BG, K_LIME, 160));
        if (ff) str_cat(fb, ff); else foot_text(v, fb, 180);
        g_text(T_XS, 8, 226, fb, K_GREY);
        g_text_r(T_XS, 232, 226, v->tag, K_LIME);
    }
}

/* ---------------------------------------------------------------- TECH: the picture on most of the page, four tiles, one line
 * the background glows top to bottom; the picture's window left to right, the tiles bottom to top, the line right to left */
static void tech_win(int32_t x, int32_t y, int32_t w, int32_t h, int dir, uint16_t tint, int sel)
{
    if (!g_vis(y, h)) return;
    g_box(x, y, w, h, K_BG, sel ? K_LIME : g_mix(K_LINE, tint, 100));
    if (dir == 0) g_hgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_BG, tint, 70), g_mix(K_BG, K_CYAN, 34));
    else if (dir == 1) g_hgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_BG, K_CYAN, 50), g_mix(K_BG, tint, 12));
    else if (dir == 2) g_vgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_BG, tint, 6), g_mix(K_BG, tint, sel ? 120 : 64));
    else g_vgrad(x + 1, y + 1, w - 2, h - 2, g_mix(K_BG, tint, sel ? 110 : 56), K_BG);
}
static void th_tech(const view_t *v, uint32_t vi)
{
    char t[32], sub[64], b[24], nm[24], fb[64], *p;
    const char *ti = view_title(vi, t, sub), *ff = foot_fixed(vi);
    int32_t ph;
    uint32_t vz = VZ_OF[vi], c = cursor[vi], i;
    if (g_vis(0, 18)) {                         /* the bar: the name, the page, the views */
        int32_t bx = batt_draw(234, 4, v->lay == LY_SYS);
        g_text(T_XS, 6, 4, cat_tr(v->cat), K_LIME);
        p = fmt_u(b, (uint32_t)page + 1u, 2);
        g_text_r(T_XS, bx - 6, 4, b, K_GREY);
        view_dots(bx - 6 - g_text_w(T_XS, b) - 6, 5);
    }
    if (v->lay == LY_PRESET || v->lay == LY_SAVE || v->lay == LY_SYS) {   /* the lists take the whole page */
        g_text(T_S, 6, 17, ti, K_WHITE);
        if (g_text_w(T_S, ti) + g_text_w(T_XS, sub) < 214) g_text_r(T_XS, 234, 20, sub, K_GREY);
        if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], 4, 34, 232, 26, 30, 6, T_S);
        else if (v->lay == LY_SAVE) save_g(8, 38, 228, 37);
        else rows_g(v, 4, 36, 232, 24, 28, 6, T_S);
        goto foot;
    }
    int grid = vz && v->n <= 4u && !big_vis(vi);   /* (four settings and a picture: a smaller picture, the four as a 2 x 2 of big tiles) */
    ph = vz && v->n > 4u && !big_vis(vi) ? 62 : grid ? 78 : 126;   /* the picture: small when the page has rows to show under the tiles */
    tech_win(4, 18, 232, ph, 0, K_LIME, 0);
    if (vz && big_vis(vi)) {                    /* (a pattern, a sample: the title above it, not on it) */
        g_text(T_S, 10, 22, ti, K_WHITE);
        if (g_text_w(T_S, ti) + g_text_w(T_XS, sub) < 214) g_text(T_XS, 18 + g_text_w(T_S, ti), 25, sub, K_GREY);
        draw_visual(vz, 5, 38, 230, ph - 21, 0);
    } else if (vz) {
        draw_visual(vz, 5, 19, 230, ph - 2, 0);
        g_text(T_S, 10, 22, ti, K_WHITE);
        if (g_text_w(T_S, ti) + g_text_w(T_XS, sub) < 214) g_text(T_XS, 18 + g_text_w(T_S, ti), 25, sub, K_GREY);
    } else {                                    /* no picture: the title, and the row chosen, big */
        uint32_t id = v->n ? v->it[c] : I_NONE;
        g_text(T_L, 10, 24, ti, K_WHITE);
        g_text(T_XS, 10, 50, sub, K_GREY);
        if (id != I_NONE) {
            item_text(id, b);
            g_text(T_XS, 10, 76, item_name(id), K_LIME);
            g_text(g_fit(T_L, b, 214), 10, 90, b, K_WHITE);
            g_vbar(10, 124, 216, 6, item_frac(id), K_LIME);
        }
    }
    for (i = 0; grid && i < 4u; i++) {          /* the four knobs, 2 x 2: K1 K2 / K3 K4 */
        int32_t x = 4 + (int32_t)(i & 1u) * 118, y = ph + 22 + (int32_t)(i >> 1) * 54;
        uint32_t id = vknob(v, i);
        int on = id != I_NONE && v->n && v->it[c] == id;
        if (id == I_NONE || !g_vis(y, 50)) continue;
        tech_win(x, y, 114, 50, 2, K_LIME, on);
        p = str_cat(nm, "K"); *p++ = (char)('1' + i); *p = 0;
        g_text(T_XS, x + 6, y + 5, nm, KNOB_RGB[i]);
        g_text(g_fit(T_XS, item_name(id), 86), x + 22, y + 5, item_name(id), K_GREY);
        item_text(id, b);
        g_text(g_fit(T_L, b, 102), x + 6, y + 17, b, on ? K_LIME : K_WHITE);
        g_vbar(x + 6, y + 42, 102, 4, item_frac(id), on ? K_LIME : K_CYAN);
    }
    for (i = 0; !grid && i < 4u; i++) {         /* the four knobs: K1..K4 */
        int32_t x = 4 + (int32_t)i * 59, y = ph + 22;
        uint32_t id = vknob(v, i);
        int on = id != I_NONE && v->n && v->it[c] == id;
        if (id == I_NONE || !g_vis(y, 54)) continue;
        tech_win(x, y, 56, 54, 2, K_LIME, on);
        p = str_cat(nm, "K"); *p++ = (char)('1' + i); *p = 0;
        g_text(T_XS, x + 5, y + 4, nm, KNOB_RGB[i]);
        g_text(g_fit(T_XS, item_sname(id), 34), x + 20, y + 4, item_sname(id), K_GREY);
        item_text(id, b);
        g_text(g_fit(T_M, b, 48), x + 5, y + 19, b, on ? K_LIME : K_WHITE);
        g_vbar(x + 5, y + 44, 46, 4, item_frac(id), on ? K_LIME : K_CYAN);
    }
    if (vz && v->n > 4u && !big_vis(vi)) rows_g(v, 4, ph + 80, 232, 24, 26, 3, T_S);   /* (the small picture leaves room for the rows) */
    else if (v->n > 4u && g_vis(206, 17)) {     /* the row SELECT is on, and its value (ALGO) */
        uint32_t id = v->it[c];
        tech_win(4, 206, 232, 17, 3, K_CYAN, 0);
        item_text(id, b);
        g_text(T_XS, 10, 211, item_name(id), K_WHITE);
        g_text_r(T_XS, 230, 211, b, K_LIME);
    }
foot:
    if (g_vis(224, 14)) {
        if (ff) str_cat(fb, ff); else str_cat(fb, v->n > 4u ? "SELECT: ROW \xB7 ALGO: VALUE" : "K1-K4: THE FOUR");
        g_text(T_XS, 6, 227, fb, K_GREY);
        g_text_r(T_XS, 234, 227, v->tag, K_LIME);
    }
}

/* ---------------------------------------------------------------- STUDIO */
static void th_studio(const view_t *v, uint32_t vi)
{
    char t[32], sub[64], s[40], fb[64], *p;
    const char *ti = view_title(vi, t, sub), *ff = foot_fixed(vi);
    uint32_t vz = VZ_OF[vi], c = cursor[vi], i;
    if (g_vis(0, 28)) {                         /* status: the patch, its slot, the tempo, the load; the eight pages */
        g_rect(0, 0, 240, 15, K_PANEL);
        g_text(T_S, 5, 3, pname, pdirty ? K_ORANGE : K_WHITE);
        p = str_cat(s, set.bank ? "B" : "A");
        p = fmt_u(p, (uint32_t)set.idx + 1u, 2);
        p = str_cat(p, "  ");
        p = fmt_u(p, ks_bpm10 / 10u, 0);
        if (hold) p = str_cat(p, "  HOLD");
        p = str_cat(p, "  C0 ");
        p = fmt_u(p, ks_load[0], 0);
        str_cat(p, "%");
        g_text_r(T_XS, batt_draw(235, 3, v->lay == LY_SYS) - 6, 4, s, K_GREY);
        for (i = 0; i < NPAGE; i++) {
            int32_t x = 1 + (int32_t)i * 30;
            if (i == page) g_box(x, 17, 28, 11, K_LIME, K_LIME);
            else g_box(x, 17, 28, 11, K_BG, K_LINE);
            g_text_c(T_XS, x + 14, 19, PAGE_SHORT[i], i == page ? K_INK : K_GREY);
        }
    }
    if (g_vis(30, 32)) {
        g_text(T_XS, 5, 31, cat_tr(v->cat), K_LIME);
        view_dots(235, 32);
        g_text(T_B, 5, 41, ti, K_WHITE);
        g_text(T_XS, 5, 59, sub, K_MOD);
    }
    if (v->lay == LY_PLAY) {                    /* scope, then the four macros as long bars in the knobs' colours */
        draw_visual(VZ_SCOPE, 3, 70, 234, 44, 1);
        for (i = 0; i < 4u; i++) {
            uint32_t id = v->it[i];
            int32_t y = 120 + (int32_t)i * 25;
            char b[24];
            if (!g_vis(y, 24)) continue;
            item_text(id, b);
            g_rect(5, y + 5, 6, 6, PAL_BW ? K_WHITE : KNOB_RGB[i]);
            g_text(T_S, 16, y + 3, id == I_L1RATE ? "WOB RATE" : id == L1_DEP ? "WOB DEPTH" : item_name(id), i == c ? K_LIME : K_GREY);
            g_text_r(T_M, 235, y + 2, b, K_WHITE);
            g_vbar(16, y + 17, 219, 4, item_frac(id), PAL_BW ? K_WAVE : KNOB_RGB[i]);
        }
    } else if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], 3, 70, 234, 28, 31, 5, T_S);
    else if (v->lay == LY_SAVE) save_g(6, 98, 228, 70);
    else if (v->lay == LY_SYS || !v->n || vi == PAGE_V0[PG_LFO] + 3u) rows_g(v, 3, 70, 230, 22, 26, 5, T_S);
    else {                                      /* the items in cells of four, a page of eight round the row */
        uint32_t per = v->n <= 4u || vi == V_DRUM || vi == V_SEQ || vi == V_SMP ? 4u : v->n <= 8u ? 8u : v->n <= 12u ? 12u : 8u, first = (c / per) * per;   /* (SEQ BASS, DRUMS, OSC C: four, the pattern or the sample big) */
        int32_t ch = per == 12u ? 44 : 56, cs = per == 12u ? 47 : 60;   /* (up to twelve: three short rows, nothing else on the page) */
        for (i = 0; i < per && first + i < v->n; i++)
            draw_cell(v, 3 + (int32_t)(i & 3u) * 59, 70 + (int32_t)(i >> 2) * cs, 56, ch, v->it[first + i], first + i == c);
        if (v->n > per && per != 12u && g_vis(per == 4u ? 128 : 186, 4)) {   /* which eight (which four) */
            uint32_t pg, np = (v->n + per - 1u) / per;
            for (pg = 0; pg < np; pg++) g_rect(110 + (int32_t)pg * 8, per == 4u ? 128 : 186, 5, 3, pg == c / per ? K_LIME : K_LINE);
        }
        if (per == 4u) draw_visual(vz, 3, 133, 234, 86, 1);
        else if (per == 8u) draw_visual(vz, 3, 192, 234, 28, 1);
    }
    if (g_vis(222, 12)) {
        if (ff) str_cat(fb, ff); else foot_text(v, fb, 200);
        g_text(T_XS, 5, 226, fb, K_GREY);
        g_text_r(T_XS, 235, 226, v->tag, K_CYAN);
    }
}

/* ---------------------------------------------------------------- PURE */
static void th_pure(const view_t *v, uint32_t vi)
{
    char t[32], sub[64], s[40], *p;
    const char *ti = view_title(vi, t, sub);
    uint32_t vz = VZ_OF[vi], c = cursor[vi], i, nv = (uint32_t)(PAGE_V0[page + 1u] - PAGE_V0[page]);
    if (g_vis(0, 18)) {                         /* the section on a lime tab with a cut corner */
        const char *cc = cat_tr(v->cat); int32_t w = g_text_w(T_S, cc) + 18, y;
        for (y = 0; y < 17; y++) if (g_vis(y, 1)) g_rect(0, y, w + (16 - y) / 2 + 4, 1, K_LIME);
        g_text(T_S, 7, 4, cc, K_INK);
        p = fmt_u(s, (uint32_t)page + 1u, 2);
        if (nv > 1u) { p = str_cat(p, " \xB7 "); p = fmt_u(p, vcur[page] + 1u, 0); p = str_cat(p, "/"); fmt_u(p, nv, 0); }
        g_text_r(T_XS, batt_draw(234, 4, v->lay == LY_SYS) - 6, 5, s, K_GREY);
    }
    if (v->lay == LY_PRESET || v->lay == LY_SAVE || v->lay == LY_SYS || !vz) {   /* lists: rows under the title */
        g_text(T_B, 8, 22, ti, K_WHITE);
        g_text(T_XS, 8, 40, sub, K_GREY);
        if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], 6, 52, 228, 28, 32, 5, T_S);
        else if (v->lay == LY_SAVE) save_g(8, 70, 228, 22);
        else rows_g(v, 6, 52, 226, 24, 29, 6, T_S);
        if (g_vis(222, 12)) { char fb[64]; const char *ff = foot_fixed(vi); if (ff) str_cat(fb, ff); else foot_text(v, fb, 220); g_text(T_XS, 8, 228, fb, K_GREY); }
        return;
    }
    if (g_vis(20, 28)) {                        /* the title between chevrons */
        g_text(T_M, 6, 23, "<", K_GREY);
        g_text_r(T_M, 234, 23, ">", K_GREY);
        g_text_c(T_B, 120, 21, ti, K_WHITE);
        g_text_c(T_XS, 120, 39, sub, K_GREY);
    }
    draw_visual(vz, 6, 49, 228, 76, 1);
    for (i = 0; i < 4u; i++) {                  /* the four knobs as rings */
        uint32_t id = vknob(v, i);
        int32_t cx = 34 + (int32_t)i * 57;
        char b[24];
        if (id == I_NONE || !g_vis(128, 76)) continue;
        item_text(id, b);
        g_ring(cx, 148, 17, item_frac(id), (v->lay == LY_PLAY ? i == c : v->it[c] == id) ? K_LIME : K_WAVE);
        g_text_c(T_XS, cx, 170, id == I_L1RATE || id == I_L2RATE ? "RATE" : item_sname(id), K_GREY);
        g_text_c(g_text_w(T_S, b) > 54 ? T_XS : T_S, cx, 180, b, K_WHITE);
    }
    if (g_vis(196, 44)) {                       /* the row being edited (SELECT, ALGO), then what moves the page */
        uint32_t id = v->it[c];
        char b[24];
        g_box(4, 196, 232, 40, K_PANEL, K_LINE);
        item_text(id, b);
        {
            const char *lb = v->lay == LY_PLAY ? "MACRO" : "EDIT";
            g_text(T_XS, 10, 201, lb, K_LIME);
            g_text(T_S, 16 + g_text_w(T_XS, lb), 200, id == I_L1RATE ? "WOB RATE" : item_name(id), K_WHITE);
        }
        g_text_r(T_S, 230, 200, b, K_WAVE);
        p = s;
        if (v->lay == LY_PLAY) { p = str_cat(p, hold ? "HOLD \xB7 " : ""); p = fmt_u(p, ks_bpm10 / 10u, 0); str_cat(p, " BPM \xB7 PRESETS: NEXT"); }
        else {
            const char *tg = mod_tag(id);
            p = str_cat(p, "MOD ");
            str_cat(p, tg ? tg : "\xB7 NONE ON THIS ROW");
        }
        g_text(T_XS, 10, 220, s, K_GREY);
    }
}

/* ---------------------------------------------------------------- RIBBON: the views side by side (left / right), each a tall window
 * that scrolls down with SELECT, full of pictures; RIBBON CLASSIC: the same look, the classic rows inside */
static const uint8_t RB_IN[8] = {5, 3, 2, 1, 1, 0, 0, 0};   /* a corner of radius 8: the inset of each row */
static void rb_rrect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c0, uint16_t c1)   /* rounded, top c0 to bottom c1 */
{
    int32_t j;
    if (!g_vis(y, h)) return;
    for (j = 0; j < h; j++) {
        int32_t in = 0, yy = y + j;
        if (!g_vis(yy, 1)) continue;
        if (j < r) in = RB_IN[j * 8 / r] * r / 8;
        else if (j >= h - r) in = RB_IN[(h - 1 - j) * 8 / r] * r / 8;
        g_rect(x + in, yy, w - 2 * in, 1, c0 == c1 ? c0 : g_mix(c0, c1, (uint32_t)(j * 256 / h)));
    }
}
static void rb_frame(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t edge, uint16_t c0, uint16_t c1)
{
    rb_rrect(x, y, w, h, r, edge, edge);
    rb_rrect(x + 1, y + 1, w - 2, h - 2, r - 1, c0, c1);
}
static void rb_arc(int32_t cx, int32_t cy, int32_t r, uint32_t frac, uint16_t c)   /* 270 degrees, 4 px, no needle */
{
    uint32_t i, n = 72, k;
    if (!g_vis(cy - r - 1, 2 * r + 3)) return;
    for (i = 0; i <= n; i++) {
        uint32_t a = 0x60000000u + i * (0xC0000000u / n);
        uint16_t col = i * 256u <= frac * n && frac ? c : K_TRACK;
        for (k = 0; k < 4u; k++) {
            int32_t rr = r - (int32_t)k;
            g_px(cx + ((sin32(a + 0x40000000u) * rr) >> 15), cy + ((sin32(a) * rr) >> 15), col);
        }
    }
}
static void rb_tri(int32_t x, int32_t y, int dir, uint16_t c)   /* a small arrow: dir -1 left, 1 right, 2 down */
{
    int32_t i;
    for (i = 0; i < 4; i++) {
        if (dir == 2) g_rect(x - 3 + i, y + i, 7 - 2 * i, 1, c);
        else g_rect(dir < 0 ? x + i : x + 3 - i, y - i, 1, 2 * i + 1, c);
    }
}
static void rb_name(uint32_t vi, char *b)       /* a view's short name, for the ribbon */
{
    static const char *const FR[] = {"CURRENT PATCH", "LIVE MIX", "OSCILLATOR A", "OSCILLATOR B", "OSCILLATOR C \xB7 SAMPLE", "SUB + NOISE",
                                     "COMBINE A + B", "AMP ENVELOPE", "FILTER ENVELOPE", "MOD ENVELOPE", "FILTER / COLOUR", "FILTER MOD",
                                     "SEQ BASS", "LFO 1 \xB7 WOBBLE", "LFO 2 \xB7 MOTION", "STEP FX", "MOD MATRIX", "PRESET BANK A",
                                     "PRESET BANK B", "SAVE PATCH", "ARPEGGIATOR", "GLOBAL SETUP"};
    static const char *const TO[] = {"PLAY", "MIX", "OSC A", "OSC B", "SAMPLE", "SUB", "A + B", "AMP", "F.ENV", "M.ENV", "FILTER", "F.MOD",
                                     "SEQ", "LFO 1", "LFO 2", "STEP", "MATRIX", "BANK A", "BANK B", "SAVE", "ARP", "SYSTEM"};
    const char *c = VIEWS[vi].cat;
    uint32_t i;
    char t[32], sub[64];
    if (str_eq(c, "FX RACK")) {                /* (the effects by their short names) */
        static const char *const FXL[] = {"SATURATION", "MULTIBAND", "COMPRESSOR", "BITCRUSH", "REVERB"}, *const FXS[] = {"SAT", "OTT", "COMP", "CRUSH", "VERB"};
        const char *ti = view_title(vi, t, sub);
        for (i = 0; i < 5u; i++) if (str_eq(ti, FXL[i])) { str_cat(b, FXS[i]); return; }
        str_cat(b, ti);
        return;
    }
    for (i = 0; i < sizeof FR / sizeof FR[0]; i++) if (str_eq(c, FR[i])) { str_cat(b, TO[i]); return; }
    str_cat(b, c);
}
static uint32_t rb_vi, rb_ms;
static int8_t rb_dir;
static int32_t rb_content(const view_t *v, uint32_t vi, int32_t x, int32_t y0, int32_t w, int draw, int32_t *sel_y)   /* RIBBON: the tall page; its height */
{
    static const uint16_t *const RC[4] = {&kc[KC_LIME], &kc[KC_CYAN], &kc[KC_ORANGE], &kc[KC_MOD]};
    uint32_t vz = VZ_OF[vi], c = cursor[vi], i, n = v->n;
    int32_t y = y0;
    char b[24], lab[48];
    *sel_y = 0;
    if (vz) {                                   /* the picture (a pattern, a sample: taller) */
        int32_t ph = big_vis(vi) ? 96 : 58;
        if (draw && g_vis(y + 4, ph)) {
            int32_t k;
            rb_rrect(x + 4, y + 4, w - 8, ph, 6, g_mix(K_BG, K_PANEL, 90), g_mix(K_BG, K_PANEL, 90));
            if (!big_vis(vi)) for (k = 1; k < 4; k++) g_rect(x + 4 + (w - 8) * k / 4, y + 6, 1, ph - 4, g_mix(K_BG, K_LINE, 110));
            draw_visual(vz, x + 6, y + 6, w - 12, ph - 4, 0);
        }
        y += ph + 8;
    }
    if (n) {                                    /* the first four: dials, each in its colour */
        uint32_t m = n < 4u ? n : 4u;
        for (i = 0; i < m; i++) {
            int32_t cx = x + (w * (2 * (int32_t)i + 1)) / 8, cy = y + 18, sel = c == i;
            uint32_t id = v->it[i];
            if (sel) *sel_y = y - y0;
            if (!draw || !g_vis(cy - 16, 44)) continue;
            rb_arc(cx, cy, 14, item_frac(id), *RC[i]);
            item_text(id, b);
            if (g_text_w(T_XS, b) <= 24) g_text_c(T_XS, cx, cy - 4, b, K_WHITE);   /* (the value in the dial, or under it when longer) */
            else g_text_c(g_fit(T_XS, b, w / 4 - 2), cx, cy + 26, b, K_WHITE);
            g_text_c(g_fit(T_XS, item_sname(id), w / 4 - 2), cx, cy + 16, item_sname(id), sel ? *RC[i] : K_GREY);
            if (sel) g_rect(cx - 8, cy + 36, 17, 2, *RC[i]);
        }
        y += 60;
    }
    if (vz == VZ_OSCA || vz == VZ_OSCB) {       /* the harmonics of the wave shown */
        if (draw && g_vis(y, 56)) {
            const int16_t *wv = vz == VZ_OSCA ? prev_a : prev_b;
            int32_t mag[16], mx = 1, k, j;
            for (k = 0; k < 16; k++) {
                int32_t re = 0, im = 0;
                for (j = 0; j < 112; j++) {
                    uint32_t ph = (uint32_t)((uint64_t)(k + 1) * (uint32_t)j * 0x100000000ull / 112u);
                    re += (wv[j] * sin32(ph + 0x40000000u)) >> 15;
                    im += (wv[j] * sin32(ph)) >> 15;
                }
                mag[k] = (re < 0 ? -re : re) + (im < 0 ? -im : im);
                if (mag[k] > mx) mx = mag[k];
            }
            g_text(T_XS, x + 8, y, "HARMONICS", K_GREY);
            for (k = 0; k < 16; k++) {
                int32_t hh = 2 + mag[k] * 36 / mx;
                g_rect(x + 8 + k * ((w - 16) / 16), y + 48 - hh, (w - 16) / 16 - 3, hh, g_mix(K_LIME, K_CYAN, (uint32_t)(k * 17)));
            }
        }
        y += 58;
    }
    for (i = 4; i < n;) {                       /* the rest: choices as pills, the glide as its curve, values in boxes of three */
        uint32_t id = v->it[i];
        int32_t lo, hi;
        item_range(id, &lo, &hi);
        if (item_patch(id) && PD[id].fmt == FM_LIST && hi - lo <= 2) {
            int32_t m = hi - lo + 1, pw = (w - 16 - (m - 1) * 6) / m, k, cur = item_get(id);
            if (c == i) *sel_y = y - y0;
            if (draw && g_vis(y, 32)) {
                { char *q = str_cat(str_cat(lab, item_name(id)), " \xB7 "); for (k = 0; k < m; k++) q = str_cat(k ? str_cat(q, " / ") : q, PD[id].list[lo + k]); }
                g_text(g_fit(T_XS, lab, w - 16), x + 8, y, lab, c == i ? K_LIME : K_GREY);
                for (k = 0; k < m; k++) {
                    int on = cur == lo + k, px = x + 8 + k * (pw + 6);
                    rb_frame(px, y + 11, pw, 17, 8, on ? K_LIME : c == i ? K_GREY : K_LINE, on ? K_LIME : g_mix(K_BG, K_PANEL, 90), on ? K_LIME : g_mix(K_BG, K_PANEL, 90));
                    g_text_c(g_fit(T_XS, PD[id].list[lo + k], pw - 4), px + pw / 2, y + 15, PD[id].list[lo + k], on ? K_INK : K_GREY);
                }
            }
            y += 34; i++;
        } else if (id == V_GLIDE) {
            if (c == i) *sel_y = y - y0;
            if (draw && g_vis(y, 42)) {
                int32_t k, lv = 0, tau = 2 + (int32_t)item_frac(id) / 5, py = y + 38;
                item_text(id, b);
                str_cat(str_cat(lab, "GLIDE  "), b);
                g_text(T_XS, x + 8, y, lab, c == i ? K_LIME : K_GREY);
                for (k = 0; k < w - 20; k++) {
                    int32_t yy;
                    lv += (256 - lv) / tau + 1;
                    if (lv > 256) lv = 256;
                    yy = y + 38 - 24 * lv / 256;
                    if (k) g_line(x + 8 + k - 1, py, x + 8 + k, yy, K_CYAN);
                    py = yy;
                }
            }
            y += 44; i++;
        } else {                                /* up to three values in boxes (a long value: a whole row) */
            uint32_t g = 0, k;
            int32_t bw = (w - 16 - 12) / 3;
            for (g = 0; g < 3u && i + g < n; g++) {
                uint32_t id2 = v->it[i + g];
                int32_t l2, h2;
                item_range(id2, &l2, &h2);
                item_text(id2, b);
                if (id2 == V_GLIDE || (item_patch(id2) && PD[id2].fmt == FM_LIST && h2 - l2 <= 2) || g_text_w(T_S, b) > bw - 4) break;
            }
            if (!g) {                           /* a long one: a row */
                if (c == i) *sel_y = y - y0;
                if (draw && g_vis(y, 22)) { item_text(id, b); row_g(x + 8, y, w - 16, 20, item_name(id), b, c == i, K_CYAN, T_XS); }
                y += 24; i++;
                continue;
            }
            { char *q = lab; for (k = 0; k < g; k++) { if (k) q = str_cat(q, " / "); q = str_cat(q, item_sname(v->it[i + k])); } }
            for (k = 0; k < g; k++) if (c == i + k) *sel_y = y - y0;
            if (draw && g_vis(y, 44)) {
                g_text(g_fit(T_XS, lab, w - 16), x + 8, y, lab, K_GREY);
                for (k = 0; k < g; k++) {
                    int32_t bx = x + 8 + (int32_t)k * (bw + 6), sel = c == i + k;
                    item_text(v->it[i + k], b);
                    rb_frame(bx, y + 11, bw, 30, 5, sel ? K_LIME : K_LINE, g_mix(K_BG, K_PANEL, 110), g_mix(K_BG, K_PANEL, 70));
                    g_text_c(g_fit(T_S, b, bw - 4), bx + bw / 2, y + 15, b, sel ? K_LIME : K_WHITE);
                    g_text_c(g_fit(T_XS, item_sname(v->it[i + k]), bw - 4), bx + bw / 2, y + 29, item_sname(v->it[i + k]), K_GREY);
                }
            }
            y += 46; i += g;
        }
    }
    return y - y0 + 6;
}
static uint32_t hint_vi = 0xFFFFu, hint_ms;
static void hint_overlay(const view_t *v, uint32_t vi)   /* (the themes with no footer) what the knobs do, then the buttons: see-through, at the bottom */
{
    const char *ff = foot_fixed(vi), *l2 = v->lay == LY_PRESET ? "SAVE: KEEP THIS SOUND \xB7 SAVE HELD: FAVORITE" :
                     v->lay == LY_SAVE ? "SAVE AGAIN: WRITE \xB7 ANOTHER PAGE: CANCEL" : "SELECT: ROW \xB7 ALGO: VALUE \xB7 SAVE: SAVE";
    char fb[64];
    int32_t y, x;
    if (vi != hint_vi) { hint_vi = vi; hint_ms = fm1_ms; }
    if (v->lay != LY_PRESET && v->lay != LY_SAVE && fm1_ms - hint_ms > 4000u) return;   /* (4 s on a new view; always on PRESETS and SAVE) */
    if (!g_vis(204, 34)) return;
    for (y = 204; y < 238; y++) if (g_vis(y, 1)) for (x = 4; x < 236; x++) g_pxa(x, y, K_BG, y == 204 || y == 237 ? 140u : 200u);
    g_rect(4, 204, 232, 1, g_mix(K_BG, K_LIME, 160));
    if (ff) str_cat(fb, ff); else foot_text(v, fb, 224);
    g_text_c(g_fit(T_XS, fb, 226), 120, 209, fb, K_WHITE);
    g_text_c(g_fit(T_XS, l2, 226), 120, 223, l2, K_LIME);
}
static void th_ribbon(const view_t *v, uint32_t vi, int classic)
{
    const int32_t wy = 26, wh = 206;
    int32_t ww = 190, wx = 25, k, off = 0, tall, sel_y, scroll = 0, cy0 = wy + 28, cy1 = wy + wh - 2;
    char t[32], sub[64], b[24];
    const char *ti = view_title(vi, t, sub);
    uint32_t nv = sizeof VIEWS / sizeof VIEWS[0], stack = !classic && (v->lay == LY_PLAY || v->lay == LY_LIST || v->lay == LY_BOX || v->lay == LY_LFO || v->lay == LY_STEP);
    if (classic) { wx = 2; ww = 236; }         /* RIBBON CLASSIC: the whole page, no neighbours, no sliding */
    if (vi != rb_vi) { rb_dir = vi > rb_vi ? 1 : -1; rb_ms = fm1_ms; rb_vi = vi; }
    if (!classic && fm1_ms - rb_ms < 180u) off = rb_dir * (int32_t)(180u - (fm1_ms - rb_ms)) * 200 / 180;
    if (g_vis(0, 19)) {                         /* the ribbon: the views around, the one shown lit */
        for (k = -3; k <= 3; k++) {
            int32_t j = (int32_t)vi + k, cx = 120 + k * 50 + off / 4;
            if (j < 0 || j >= (int32_t)nv || VIEWS[j].lay == LY_EQ) continue;
            b[0] = 0; rb_name((uint32_t)j, b);
            if (!k) rb_rrect(cx - 24, 3, 48, 13, 6, K_LIME, K_LIME);
            g_text_c(g_fit(T_XS, b, 46), cx, 5, b, k ? K_GREY : K_INK);
        }
        g_rect(0, 18, 240, 1, K_LINE);
    }
    for (k = -1; k <= 1 && !classic; k += 2) {   /* the neighbours peek in at the sides */
        int32_t nx = k < 0 ? -176 : 226, nj = (int32_t)vi + k;
        uint16_t acc = k < 0 ? K_ORANGE : K_CYAN;
        if (nj < 0 || nj >= (int32_t)nv) continue;
        rb_frame(nx + off, wy, ww, wh, 8, g_mix(K_BG, acc, 70), g_mix(K_BG, K_PANEL, 110), g_mix(K_BG, K_PANEL, 70));
        rb_rrect(nx + off + 1, wy + 1, ww - 2, 24, 7, g_mix(K_BG, acc, 60), g_mix(K_BG, K_PANEL, 110));
        rb_tri(k < 0 ? 4 : 232, 129, k, g_mix(K_BG, K_LIME, 200));
    }
    wx += off;
    rb_rrect(wx + 4, wy + 6, ww, wh, 8, g_mix(K_BG, 0x0000u, 140), g_mix(K_BG, 0x0000u, 200));   /* the shadow */
    rb_frame(wx, wy, ww, wh, 8, g_mix(K_LINE, K_LIME, 120), g_mix(K_PANEL, K_LIME, 26), K_PANEL);
    rb_rrect(wx + 1, wy + 1, ww - 2, 26, 7, g_mix(K_PANEL, K_LIME, 90), K_PANEL);   /* its head: the title, what it is */
    g_clip(wx + 2, wy + 1, wx + ww - 2, wy + 27);
    g_text(g_fit(T_M, ti, ww - 16), wx + 8, wy + 4, ti, K_WHITE);
    g_text(g_fit(T_XS, sub, ww - 16), wx + 8, wy + 17, sub, K_LIME);
    g_clip(wx + 2 > 0 ? wx + 2 : 0, cy0, wx + ww - 2 < 240 ? wx + ww - 2 : 240, cy1);
    if (stack) {                                /* RIBBON: the tall page, scrolled to the row chosen */
        tall = rb_content(v, vi, wx, cy0, ww - 4, 0, &sel_y);
        scroll = clampi(sel_y + 64 - (cy1 - cy0), 0, tall > cy1 - cy0 ? tall - (cy1 - cy0) : 0);   /* (the row chosen kept in sight, with what follows it) */
        rb_content(v, vi, wx, cy0 - scroll, ww - 4, 1, &sel_y);
        if (tall > cy1 - cy0) {                 /* its bar, a fade at the bottom and an arrow: there is more */
            int32_t bh = (cy1 - cy0) * (cy1 - cy0) / tall, by = cy0 + scroll * (cy1 - cy0 - bh) / (tall - (cy1 - cy0));
            g_clip(0, 0, 240, 240);
            rb_rrect(wx + ww - 5, by + 2, 3, bh - 4, 1, K_LIME, K_LIME);
            if (scroll < tall - (cy1 - cy0) - 2) {
                int32_t j, xx;
                for (j = 0; j < 16; j++) if (g_vis(cy1 - 16 + j, 1)) for (xx = wx + 2; xx < wx + ww - 6; xx++) g_pxa(xx, cy1 - 16 + j, K_PANEL, (uint32_t)(j * 16));
                rb_tri(wx + ww / 2, cy1 - 7, 2, K_LIME);
            }
        }
    } else if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], wx + 6, cy0 + 2, ww - 16, 26, 29, 6, T_S);
    else if (v->lay == LY_SAVE) save_g(wx + 8, cy0 + 4, ww - 16, 34);
    else {                                      /* RIBBON CLASSIC (and the lists): the picture, then the rows */
        int32_t ry = cy0 + 2;
        uint32_t vz = VZ_OF[vi];
        if (vz) {
            int32_t ph = big_vis(vi) ? 100 : 58;
            rb_rrect(wx + 6, ry, ww - 12, ph, 6, g_mix(K_BG, K_PANEL, 90), g_mix(K_BG, K_PANEL, 90));
            draw_visual(vz, wx + 8, ry + 2, ww - 16, ph - 4, 0);
            ry += ph + 6;
        }
        rows_g(v, wx + 6, ry, ww - 18, 22, 25, (uint32_t)((cy1 - ry) / 25), T_S);
    }
    g_clip(0, 0, 240, 240);
    hint_overlay(v, vi);
}

/* ---------------------------------------------------------------- RACK */
static void th_rack(const view_t *v, uint32_t vi)
{
    char t[32], sub[64], fb[64];
    const char *ti = view_title(vi, t, sub), *ff = foot_fixed(vi);
    uint32_t vz = VZ_OF[vi], i;
    if (g_vis(0, 240)) {                        /* the rail: the eight pages */
        rb_rrect(1, 1, 30, 238, 8, K_PANEL, K_PANEL);   /* (the rail: rounded, a notch between two pages) */
        for (i = 1; i < NPAGE; i++) {
            int32_t ny = 3 + (int32_t)i * 29 - 2;
            g_rect(1, ny - 1, 3, 3, K_BG); g_rect(28, ny - 1, 3, 3, K_BG);
            g_rect(7, ny, 18, 1, g_mix(K_PANEL, K_BG, 140));
        }
        for (i = 0; i < NPAGE; i++) {
            int32_t y = 3 + (int32_t)i * 29;
            char nb[3];
            if (!g_vis(y, 27)) continue;
            if (i == page) rb_frame(2, y, 27, 26, 7, K_LIME, K_LIME, K_LIME);   /* (the page: a rounded block) */
            fmt_u(nb, i + 1u, 0);
            g_text_c(T_XS, 15, y + 3, nb, i == page ? K_INK : K_GREY);
            g_text_c(T_XS, 15, y + 14, PAGE_SHORT[i], i == page ? K_INK : K_WHITE);
        }
    }
    if (g_vis(0, 50)) {
        g_text(T_S, 37, 4, cat_tr(v->cat), K_LIME);
        view_dots(batt_draw(235, 5, v->lay == LY_SYS) - 6, 6);
        g_text(T_L, 37, 17, ti, K_WHITE);
        g_text(T_XS, 37, 40, sub, K_GREY);
    }
    if (v->lay == LY_PLAY) {
        uint32_t c = cursor[vi];
        draw_visual(VZ_SCOPE, 35, 51, 201, 46, 1);
        for (i = 0; i < 4u; i++) {
            uint32_t id = v->it[i];
            char b[24];
            item_text(id, b);
            row_g(35, 101 + (int32_t)i * 29, 199, 26, id == I_L1RATE ? "WOB RATE" : id == L1_DEP ? "WOB DEPTH" : item_name(id), b, i == c, K_CYAN, T_M);
        }
    } else if (v->lay == LY_PRESET) presets_g(vi - PAGE_V0[PG_PRE], 35, 51, 199, 31, 34, 5, T_M);
    else if (v->lay == LY_SAVE) save_g(37, 70, 199, 17);
    else if (vz && big_vis(vi)) {
        draw_visual(vz, 35, 51, 201, 100, 1);
        rows_g(v, 35, 155, 197, 24, 26, 2, T_M);
    } else if (vz) {
        draw_visual(vz, 35, 51, 201, 36, 1);
        rows_g(v, 35, 91, 197, 24, 26, 5, T_M);
    } else
        rows_g(v, 35, 51, 197, 24, 26, 6, T_M);
    if (g_vis(222, 12)) {
        if (ff) str_cat(fb, ff); else foot_text(v, fb, 190);
        g_text(T_XS, 37, 228, fb, K_GREY);
    }
}

/* ---------------------------------------------------------------- SHIFT mode: three columns
 * the base (knobs alone) at the left, OCT- held in the middle, OCT+ held at the right; the column not played is dimmed.
 * SYSTEM > SHIFT COLOURS: THEME (the theme's colour, a little lighter in the middle, a little towards its second colour
 * at the right) or fluo green / yellow / orange. The name of a setting shows in full only while it turns (at the top,
 * with what it was), the cells keep their initials */
static const uint16_t C3_RGB[3] = {RGB(96, 255, 64), RGB(255, 224, 48), RGB(255, 144, 48)};
static uint16_t c3c(uint32_t k)
{
    if (set.c3col && !PAL_BW) return C3_RGB[k];
    return k == 0u ? K_LIME : k == 1u ? g_mix(K_LIME, K_WHITE, 80) : g_mix(K_LIME, K_CYAN, 80);
}
static void draw_c3(void)
{
    const col3_t *c = &COL3[page];
    uint32_t act = c3_col(), k;
    int32_t pk = -1;
    char t[48], sub[64], b[24], *p;
    if (vpop_ms) for (k = 0; k < 12u; k++) if (c->id[k >> 2][k & 3u] == vpop_id) pk = (int32_t)k;
    if (g_vis(0, 46)) {
        if (pk >= 0) {                          /* the setting turning: its column, its knob, its name, its value */
            uint16_t cc = c3c((uint32_t)pk >> 2);
            g_box(4, 2, 232, 42, K_PANEL, cc);
            p = str_cat(t, c->head[pk >> 2]);
            p = str_cat(p, " \xB7 K");
            *p++ = (char)('1' + (pk & 3)); *p = 0;
            g_text(T_XS, 10, 6, t, cc);
            p = t;                              /* (the effects' settings with their effect's name) */
            if (vpop_id > DR_ON && vpop_id < MS_GLUE)
                p = str_cat(p, vpop_id < CR_ON ? "SAT " : vpop_id < CH_ON ? "CRUSH " : vpop_id < DL_ON ? "CHORUS " : vpop_id < RV_ON ? "DELAY " : "VERB ");
            str_cat(p, item_name(vpop_id));
            g_text(T_S, 10, 22, t, K_WHITE);
            item_text(vpop_id, b);
            g_text_r(g_text_w(T_B, b) > 120 ? T_S : T_B, 230, 18, b, cc);
            if (!str_eq(b, vpop_before)) { str_cat(str_cat(t, "WAS "), vpop_before); g_text_r(T_XS, 230, 6, t, K_MOD); }
        } else {
            const char *ti;
            *sub = 0;
            if (page == PG_BASS) {
                p = str_cat(t, L_ENG[(uint32_t)patch[A_ENG] % NENG]); p = str_cat(p, " + "); str_cat(p, L_ENG[(uint32_t)patch[B_ENG] % NENG]);
                p = str_cat(sub, "MODE "); str_cat(p, L_XMODE[patch[X_MODE] & 3]);
                ti = t;
            } else if (page == PG_FX) {
                p = str_cat(t, "SAT "); p = str_cat(p, patch[DR_ON] ? L_DRT[patch[DR_TYPE] & 3] : "OFF");
                p = str_cat(p, " \xB7 DELAY "); str_cat(p, patch[DL_ON] ? L_DIV[patch[DL_TIME] & 15] : "OFF");
                str_cat(sub, patch[RV_ON] ? "REVERB ON" : "REVERB OFF");
                ti = t;
            } else if (page == PG_ENV) {
                str_cat(t, "3 ENVELOPES");
                str_cat(str_cat(sub, "MOD ENV > "), dst_name((uint32_t)patch[ME_DST]));
                ti = t;
            } else ti = view_title(PAGE_V0[page], t, sub);
            g_text(T_XS, 6, 3, c->title, c3c(0));
            g_text(T_B, 6, 13, ti, K_WHITE);
            g_text(T_XS, 6, 33, sub, K_GREY);
            p = str_cat(b, PAGE_SHORT[page]);
            str_cat(p, " \xB7 SEL: MORE");
            g_text_r(T_XS, 234, 3, b, K_GREY);
        }
    }
    if (g_vis(46, 13))                          /* the three heads: the one played filled */
        for (k = 0; k < 3u; k++) {
            int32_t x = 12 + (int32_t)k * 76;
            uint16_t cc = c3c(k);
            p = t;
            if (k) p = str_cat(p, k == 1u ? "(-) " : "(+) ");   /* (the sign first, both on the left) */
            p = str_cat(p, c->head[k]);
            g_box(x, 46, 74, 12, k == act ? cc : K_BG, cc);
            g_text_c(T_XS, x + 37, 48, t, k == act ? K_INK : cc);
        }
    for (k = 0; k < 12u; k++) {                 /* the cells: initials, value, bar, what modulates it */
        uint32_t col = k >> 2, r = k & 3u, id = c->id[col][r];
        int32_t x = 12 + (int32_t)col * 76, y = 61 + (int32_t)r * 39;
        int on = col == act;
        uint16_t cc = c3c(col), dim = g_mix(K_PANEL, cc, 120);
        const ks_font_t *f;
        const char *tg;
        if (!g_vis(y, 37)) continue;
        if (col == 0u) { char kn[3] = {'K', (char)('1' + r), 0}; g_text_c(T_XS, 6, y + 14, kn, K_GREY); }
        g_box(x, y, 74, 37, K_PANEL, k == c3_cur ? (on ? K_WHITE : K_GREY) : on ? g_mix(K_LINE, cc, 70) : K_LINE);
        if (c->ini[col][r]) str_cat(t, c->ini[col][r]);
        else { const char *sn = item_sname(id); t[0] = sn[0]; t[1] = sn[0] ? sn[1] : 0; t[2] = t[1] ? sn[2] : 0; t[3] = 0; }
        g_text(T_XS, x + 4, y + 3, t, on ? cc : dim);
        if ((tg = mod_tag(id)) != 0) g_text_r(T_XS, x + 70, y + 3, tg, K_MOD);
        item_text(id, b);
        f = g_text_w(T_M, b) <= 66 ? T_M : g_text_w(T_S, b) <= 66 ? T_S : T_XS;
        g_text(f, x + 4, y + 14, b, on ? K_WHITE : K_GREY);
        g_vbar(x + 4, y + 31, 66, 3, item_frac(id), on ? cc : dim);
    }
    if (g_vis(220, 20)) {                       /* which button gives which column */
        g_rect(0, 219, 240, 1, K_LINE);
        g_rect(5, 226, 7, 7, c3c(1));
        g_text(T_XS, 15, 225, "OCT- HOLD", K_GREY);
        g_text_c(T_XS, 120, 225, "TAP: OCTAVE", K_GREY);
        g_text_r(T_XS, 223, 225, "OCT+ HOLD", K_GREY);
        g_rect(228, 226, 7, 7, c3c(2));
    }
}
/* ---------------------------------------------------------------- the SEL windows: FX LIVE, MACROS
 * (in the theme's colours, the same in every theme: four big rows for K1..K4, what SELECT and ALGO reach below) */
static void win_head(const char *ti, const char *sub, const char *k)
{
    if (!g_vis(0, 40)) return;
    g_text(T_XS, 6, 3, k, K_LIME);
    g_text_r(T_XS, 234, 3, win == 7u ? "SEL: MORPH X/Y" : win == 4u ? "SEL: FX LIVE" : win == 1u ? "SEL: MACROS" : win == 2u ? "SEL: MORPH A/B" : "SEL: BACK", K_GREY);
    g_text(T_B, 6, 13, ti, K_WHITE);
    g_text(T_XS, 6, 31, sub, K_GREY);
}
static void win_row(int32_t y, const char *kn, uint32_t id, int sel, int on, int bip)
{
    char b[24];
    if (!g_vis(y, 36)) return;
    g_box(4, y, 232, 36, on ? K_LIMED : K_PANEL, sel ? K_WHITE : on ? K_LIME : K_LINE);
    g_text(T_XS, 10, y + 4, kn, on ? K_LIME : K_GREY);
    g_text(T_S, 10 + g_text_w(T_XS, kn) + 6, y + 3, item_name(id), K_WHITE);
    item_text(id, b);
    g_text_r(g_fit(T_M, b, 220 - 22 - g_text_w(T_XS, kn) - g_text_w(T_S, item_name(id))), 230, y + 3, b, on ? K_LIME : K_WHITE);
    g_vbar(10, y + 26, 220, 5, item_frac(id), on ? K_LIME : K_CYAN);
    if (bip) g_rect(119, y + 24, 2, 9, K_GREY);  /* (the middle: off) */
}
/* MORPH X/Y: the pad (A B on top, C D below), the dot (where the LFO took it; the knobs' place in grey), each sound's share */
static void mx_dot(int32_t x, int32_t y, int32_t r, uint16_t c) { g_rect(x - r + 1, y - r, 2 * r - 1, 2 * r + 1, c); g_rect(x - r, y - r + 1, 2 * r + 1, 2 * r - 1, c); }
static void draw_mxy(void)
{
    static const char *const CN[4] = {"A", "B", "C", "D"};
    const int32_t px = 4, py = 42, ps = 152;
    uint16_t cc[4];
    int32_t k, ex = mx_mode == MX_XY ? mx_ex : mx_x, ey = mx_mode == MX_XY ? mx_ey : mx_y, w[4];
    char t[40], *p;
    cc[0] = K_CYAN; cc[1] = K_MOD; cc[2] = K_LIME; cc[3] = K_ORANGE;
    w[0] = (127 - ex) * ey; w[1] = ex * ey; w[2] = (127 - ex) * (127 - ey); w[3] = ex * (127 - ey);
    p = str_cat(t, "X "); p = fmt_u(p, (uint32_t)ex * 100u / 127u, 0); p = str_cat(p, " \xB7 Y "); p = fmt_u(p, (uint32_t)ey * 100u / 127u, 0);
    if (ex != mx_x || ey != mx_y) str_cat(p, " \xB7 MODULATED");
    win_head("MORPH X/Y", t, "SEL 2/5");
    if (g_vis(py, ps)) {
        int32_t dx = px + 6 + ex * (ps - 12) / 127, dy = py + 6 + (127 - ey) * (ps - 12) / 127;
        int32_t gy0 = g_y0 > py + 1 ? g_y0 : py + 1, gy1 = g_y0 + (int32_t)BAND_H < py + ps - 1 ? g_y0 + (int32_t)BAND_H : py + ps - 1, cx, cy;
        g_box(px, py, ps, ps, K_PANEL, K_LINE);
        for (cy = gy0 & ~1; cy < gy1; cy += 2)  /* each corner's colour glowing from its corner (2 x 2 cells, this band only) */
            for (cx = px + 1; cx < px + ps - 1; cx += 2) {
                int32_t u = (cx - px) * 256 / ps, v = (cy - py) * 256 / ps, q[4], r = 0, g = 0, b = 0, t = 0;
                q[0] = (256 - u) * (256 - v) >> 8; q[1] = u * (256 - v) >> 8; q[2] = (256 - u) * v >> 8; q[3] = u * v >> 8;
                for (k = 0; k < 4; k++) {
                    int32_t w2 = q[k] * q[k] >> 8;
                    r += w2 * (cc[k] >> 11); g += w2 * ((cc[k] >> 5) & 63); b += w2 * (cc[k] & 31); t += w2;
                }
                if (t > 0) g_rect(cx, cy, 2, 2, g_mix(K_PANEL, (uint16_t)((r / t) << 11 | (g / t) << 5 | (b / t)), (uint32_t)(t * 2 < 256 ? t * 2 : 256) * 100u / 256u));
            }
        g_rect(px + ps / 2, py + 2, 1, ps - 4, K_LINE);
        g_rect(px + 2, py + ps / 2, ps - 4, 1, K_LINE);
        g_rect(dx, py + 1, 1, ps - 2, g_mix(K_PANEL, K_WHITE, 80));
        g_rect(px + 1, dy, ps - 2, 1, g_mix(K_PANEL, K_WHITE, 80));
        g_text(T_S, px + 5, py + 3, CN[0], cc[0]);
        g_text_r(T_S, px + ps - 5, py + 3, CN[1], cc[1]);
        g_text(T_S, px + 5, py + ps - 16, CN[2], cc[2]);
        g_text_r(T_S, px + ps - 5, py + ps - 16, CN[3], cc[3]);
        if (ex != mx_x || ey != mx_y) mx_dot(px + 6 + mx_x * (ps - 12) / 127, py + 6 + (127 - mx_y) * (ps - 12) / 127, 2, K_GREY);
        mx_dot(dx, dy, 4, K_WHITE);
    }
    for (k = 0; k < 4; k++) {                   /* the four sounds and their share */
        int32_t x = 160, y = 42 + k * 38;
        const char *nm = mx_name(perf.mx_pre[k]);
        if (!g_vis(y, 36)) continue;
        g_box(x, y, 76, 36, K_PANEL, mx_corner == k ? K_WHITE : K_LINE);
        p = str_cat(t, CN[k]); p = str_cat(p, " \xB7 "); p = fmt_u(p, (uint32_t)w[k] * 100u / 16129u, 0); str_cat(p, " %");
        g_text(T_XS, x + 5, y + 3, t, cc[k]);
        g_text(g_fit(T_XS, nm, 68), x + 5, y + 14, nm, K_WHITE);
        g_bar(x + 5, y + 27, 66, 4, (uint32_t)w[k] * 256u / 16129u, cc[k]);
    }
    if (g_vis(198, 42)) {
        const char *nm = mx_name(perf.mx_pre[mx_corner & 3u]);
        g_box(4, 198, 232, 40, K_PANEL, cc[mx_corner & 3u]);
        p = str_cat(t, "CORNER "); str_cat(p, CN[mx_corner & 3u]);
        g_text(T_XS, 10, 202, t, cc[mx_corner & 3u]);
        g_text_r(T_XS, 230, 202, "K3 CORNER \xB7 K4 ITS SOUND", K_GREY);
        g_text(g_fit(T_S, nm, 150), 10, 216, perf.mx_pre[mx_corner & 3u] ? nm : "NOW (OPENED WITH)", K_WHITE);
        if (!perf.mx_pre[mx_corner & 3u]) g_text_r(T_XS, 230, 220, mx_now_nm, K_GREY);
    }
}
static const char *mx_label(uint32_t id, char *t)   /* a setting with its section: DELAY TIME, AMP ATTACK */
{
    const char *g = id >= DR_ON && id <= DR_MIX ? "SAT" : id >= CR_ON && id <= CR_MIX ? "CRUSH" : id >= CH_ON && id <= CH_MIX ? "CHORUS" :
                    (id >= DL_ON && id <= DL_MIX) || id == DL_TYPE ? "DELAY" : (id >= RV_ON && id <= RV_MIX) || id == RV_TYPE ? "VERB" :
                    id >= CP_ON && id <= CP_PRATE ? "COMP" : id >= PH_ON && id <= PH_MIX ? "PHASER" : id >= ST_ON && id <= ST_SMO ? "STEP" :
                    id >= L1_SHP && id < L2_SHP ? "LFO1" : id >= L2_SHP && id < M1_SRC ? "LFO2" : id >= F_TYPE && id <= F_VEL ? "FILT" :
                    id >= AE_A && id <= AE_R ? "AMP" : id >= FE_A && id <= FE_R ? "F.ENV" : id >= ME_A && id <= ME_DST ? "M.ENV" :
                    (id >= SM_SEL && id <= SM_SPEED) || (id >= SM_START && id <= SM_DIR) ? "SMP" : 0;
    const char *n = item_name(id);
    char *p = t;
    if (n[0] >= '0' && n[0] <= '9' && n[2] == ' ') n += 3;   /* (01 ATTACK) */
    if (g) { p = str_cat(p, g); *p++ = ' '; }
    str_cat(p, n[0] == 'E' && n[1] == 'F' ? "ON" : n);
    return t;
}
/* MORPH A/B: the two sounds and where between them, what moves the most (A cyan, B violet, now white) */
static void draw_mab(void)
{
    int32_t k, a = mx_mode == MX_AB ? mx_ea : ab_amt, n = 0, top[5], sc[5];
    uint32_t i, parts = perf.ab_parts % 6u;
    char t[48], *p;
    p = fmt_u(t, (uint32_t)a * 100u / 127u, 0); p = str_cat(p, " % \xB7 "); p = str_cat(p, L_PARTS[parts]);
    p = str_cat(p, " \xB7 "); str_cat(p, L_ABT[perf.ab_time % 7u]);
    win_head("MORPH A/B", t, "SEL 5/5");
    if (g_vis(42, 46)) {
        const char *nb = mx_name(perf.ab_pre);
        g_box(4, 42, 232, 44, K_PANEL, K_LINE);
        g_text(T_XS, 10, 46, "A \xB7 NOW", K_CYAN);
        g_text_r(T_XS, 230, 46, "B \xB7 K2", K_MOD);
        g_text(g_fit(T_S, mx_now_nm, 104), 10, 56, mx_now_nm, K_WHITE);
        g_text_r(g_fit(T_S, nb, 104), 230, 56, nb, K_WHITE);
        for (k = 0; k < 214; k++) g_rect(12 + k, 76, 1, 4, g_mix(K_CYAN, K_MOD, (uint32_t)k * 256u / 214u));
        g_rect(11 + a * 214 / 127 - 1, 72, 3, 12, K_WHITE);
    }
    for (i = 0; i < NP; i++) {                  /* the five that move the most */
        int32_t d;
        if (!mx_moves(i, ab_snap, 2, parts)) continue;
        d = ab_snap[1][i] - ab_snap[0][i];
        d = (d < 0 ? -d : d) * 256 / (PD[i].hi - PD[i].lo ? PD[i].hi - PD[i].lo : 1);
        if (PD[i].fmt == FM_LIST || PD[i].fmt == FM_DEST || PD[i].fmt == FM_DIV) d = 300;   /* (a switch: first) */
        {
            int32_t m = n < 5 ? n : 5, j;
            for (k = 0; k < m && d <= sc[k]; k++) {}
            if (k < 5) { for (j = (m < 5 ? m : 4); j > k; j--) { top[j] = top[j - 1]; sc[j] = sc[j - 1]; } top[k] = (int32_t)i; sc[k] = d; }
        }
        n++;
    }
    if (g_vis(90, 12)) {
        p = str_cat(t, "WHAT MOVES ("); p = fmt_u(p, (uint32_t)n, 0); str_cat(p, ")");
        g_text(T_XS, 8, 91, n ? t : "NOTHING MOVES: B IS THE SAME HERE", K_GREY);
    }
    for (k = 0; k < 5 && k < n; k++) {
        int32_t y = 104 + k * 18, id = top[k], lo = PD[id].lo, w = PD[id].hi - lo ? PD[id].hi - lo : 1;
        int32_t fa = (ab_snap[0][id] - lo) * 140 / w, fb = (ab_snap[1][id] - lo) * 140 / w, fc = (patch[id] - lo) * 140 / w;
        if (!g_vis(y, 12)) continue;
        mx_label((uint32_t)id, t);
        g_text(g_fit(T_XS, t, 80), 8, y, t, K_WHITE);
        g_rect(90, y + 3, 140, 4, K_TRACK);
        g_rect(90 + (fa < fb ? fa : fb), y + 3, (fa < fb ? fb - fa : fa - fb) + 1, 4, g_mix(K_CYAN, K_MOD, 128));
        g_rect(90 + fa, y + 1, 1, 8, K_CYAN);
        g_rect(90 + fb, y + 1, 1, 8, K_MOD);
        g_rect(90 + fc - 1, y, 3, 10, K_WHITE);
    }
    if (g_vis(198, 42)) {
        g_box(4, 198, 232, 40, K_PANEL, K_LIME);
        g_text(T_XS, 10, 202, "K1 MORPH \xB7 K2 B \xB7 K3 PARTS \xB7 K4 TIME", K_GREY);
        if (ab_run) str_cat(t, "GOING...");
        else if (!ABT_BEATS[perf.ab_time % 7u]) str_cat(t, "K4: A TIME, THEN PLAY");
        else { p = str_cat(t, "PLAY: TO "); p = str_cat(p, a < 64 ? "B" : "A"); p = str_cat(p, " IN "); str_cat(p, L_ABT[perf.ab_time % 7u]); }
        g_text(T_S, 10, 216, t, ab_run ? K_LIME : K_WHITE);
    }
}
/* FX PADS: the white keys (the wobble: rate, depth, shape) above, the black ones (effects held) below */
static void draw_pads(void)
{
    uint32_t k, j, cur_rate = 99, cur_dep = 99, cur_shp = 99;
    int32_t bd = 99, i;
    if (g_vis(0, 40)) {
        g_text(T_XS, 6, 3, "FX PADS", K_LIME);
        g_text_r(T_XS, 234, 3, "FX OR A PAGE BUTTON: BACK", K_GREY);
        char t[64], b[16], *p;
        g_text(T_B, 6, 13, "KEYS = EFFECTS", K_WHITE);
        p = str_cat(t, "Q ");                   /* the quantize (ALGO): the keys wait for it */
        str_cat(p, L_PQ[set.pquant % 9u]);
        g_text_r(T_M, 234, 15, t, set.pquant ? K_LIME : K_GREY);
        p = str_cat(t, "K1 "); item_text(F_CUT, b); p = str_cat(p, b);   /* the four knobs and their values */
        p = str_cat(p, " \xB7 K2 "); item_text(F_RES, b); p = str_cat(p, b);
        p = str_cat(p, " \xB7 K3 "); item_text(F_TYPE, b); p = str_cat(p, b);
        p = str_cat(p, " \xB7 K4 A/B "); p = fmt_u(p, (uint32_t)ab_amt * 100u / 127u, 0); str_cat(p, "%");
        g_text(g_fit(T_XS, t, 228), 6, 31, t, K_CYAN);
    }
    for (k = 0; k < 8u; k++) if (patch[L1_SYNC] && patch[L1_DIV] == (int8_t)PW_DIV[k]) cur_rate = k;
    for (k = 0; k < 4u; k++) {
        int32_t d = patch[L1_DEP] - PW_DEP[k]; d = d < 0 ? -d : d;
        if (d < bd) { bd = d; cur_dep = 8u + k; }
        if (patch[L1_SHP] == (int8_t)PW_SHP[k]) cur_shp = 12u + k;
    }
    if (g_vis(42, 12)) g_text(T_XS, 6, 43, "WHITE KEYS \xB7 WOBBLE: BARS / BEATS", K_CYAN);
    for (k = 0; k < 16u; k++) {                 /* two rows of eight: the rate, then the depth and the shape */
        int32_t x = 4 + (int32_t)(k & 7u) * 29, y = k < 8u ? 54 : 94;
        int on = k == cur_rate || k == cur_dep || k == cur_shp, hot = (pad_hot >> PK_WHITE[k]) & 1u;
        if (k == 8u && g_vis(84, 10)) g_text(T_XS, 6, 85, "DEPTH \xB7 SHAPE", K_CYAN);
        if (!g_vis(y, 28)) continue;
        g_box(x, y, 28, 28, hot ? K_LIME : on ? K_LIMED : K_PANEL, hot ? K_WHITE : on ? K_LIME : K_LINE);
        g_text_c(g_text_w(T_S, PW_NAME[k]) > 24 ? T_XS : T_S, x + 14, y + 8, PW_NAME[k], hot ? K_INK : on ? K_LIME : K_WHITE);
    }
    if (g_vis(130, 12)) g_text(T_XS, 6, 131, "BLACK KEYS \xB7 EFFECTS WHILE HELD", K_ORANGE);
    for (j = 0; j < (uint32_t)NPB; j++) {
        int32_t x = 4 + (int32_t)(j % 4u) * 58, y = 142 + (int32_t)(j / 4u) * 30;
        int hot = (pad_mask >> j) & 1u;
        i = (int32_t)j;
        if (!g_vis(y, 27)) continue;
        g_box(x, y, 56, 27, hot ? K_ORANGE : K_PANEL, hot ? K_WHITE : K_LINE);
        g_text_c(T_XS, x + 28, y + 3, PB_SHORT[j], hot ? K_INK : K_WHITE);
        g_text_c(T_XS, x + 28, y + 15, i < 4 ? "STUTTER" : i < 8 ? "LIVE" : "BURST", hot ? K_INK : K_GREY);
    }
}
/* PERFORM (SEL): the four macros, the morph, the wobble, the pads; nothing to scroll */
static void draw_perf(void)
{
    static const char *const PBS[NPB] = {"S4", "S8", "S16", "S32", "TAPE", "LP", "HP", "PUMP", "SAT", "PHS", "CRSH"};
    uint32_t k, padl = (fm1_in.buttons >> BTN_OCT_UP) & 1u, cur = 99;
    char t[40], b[24], *p;
    int32_t ex = mx_mode == MX_XY ? mx_ex : mx_x, ey = mx_mode == MX_XY ? mx_ey : mx_y;
    if (g_vis(0, 40)) {
        g_text(T_XS, 6, 3, "PERFORM", K_LIME);
        g_text_r(T_XS, 234, 3, "SEL: MORPH X/Y", K_GREY);
        g_text(T_B, 6, 13, pname, K_WHITE);
        p = str_cat(t, "WOBBLE ");
        item_text(I_L1RATE, p);
        p += str_len(p);
        p = str_cat(p, " \xB7 DEPTH ");
        item_text(L1_DEP, b);
        str_cat(p, b);
        g_text(T_XS, 6, 31, t, K_CYAN);
    }
    for (k = 0; k < 4u; k++) {                  /* the macros: K1..K4, named after what they move */
        int32_t x = 4 + (int32_t)(k & 1u) * 78, y = 42 + (int32_t)(k >> 1) * 40;
        int on = mc_val[k] != 0;
        if (!g_vis(y, 38)) continue;
        g_box(x, y, 76, 38, on ? K_LIMED : K_PANEL, on ? K_LIME : K_LINE);
        p = str_cat(t, "K"); *p++ = (char)('1' + k); *p = 0;
        g_text(T_XS, x + 5, y + 3, t, KNOB_RGB[k]);
        g_text(g_fit(T_XS, mc_name(k), 52), x + 20, y + 3, mc_name(k), K_WHITE);
        item_text(G_MC1 + k, b);
        g_text_r(T_S, x + 71, y + 14, b, on ? K_LIME : K_WHITE);
        g_bar(x + 5, y + 30, 66, 4, item_frac(G_MC1 + k), on ? K_LIME : K_CYAN);
    }
    if (g_vis(42, 78)) {                        /* the morph: the point among the four corners */
        int32_t px = 162, py = 42, ps = 74, dx = px + 4 + ex * (ps - 8) / 127, dy = py + 4 + (127 - ey) * (ps - 8) / 127;
        g_box(px, py, ps, ps + 4, K_PANEL, K_LINE);
        g_rect(dx, py + 2, 1, ps, g_mix(K_PANEL, K_WHITE, 70));
        g_rect(px + 2, dy, ps - 4, 1, g_mix(K_PANEL, K_WHITE, 70));
        g_text(T_XS, px + 3, py + 2, "A", K_CYAN); g_text_r(T_XS, px + ps - 3, py + 2, "B", K_MOD);
        g_text(T_XS, px + 3, py + ps - 8, "C", K_LIME); g_text_r(T_XS, px + ps - 3, py + ps - 8, "D", K_ORANGE);
        g_rect(dx - 2, dy - 2, 5, 5, K_WHITE);
    }
    for (k = 0; k < 8u; k++) if (patch[L1_SYNC] && patch[L1_DIV] == (int8_t)PW_DIV[k]) cur = k;
    if (g_vis(124, 12)) g_text(T_XS, 6, 125, padl ? "PADS ON THE KEYS (OCT+ HELD)" : "OCT+ HELD: THE KEYS ARE PADS", padl ? K_LIME : K_GREY);
    for (k = 0; k < 8u; k++) {                  /* the white keys' rates */
        int32_t x = 4 + (int32_t)k * 29, y = 137;
        int on = k == cur, hot = padl && ((pad_hot >> PK_WHITE[k]) & 1u);
        if (!g_vis(y, 22)) continue;
        g_box(x, y, 28, 22, hot ? K_LIME : on ? K_LIMED : K_PANEL, on || hot ? K_LIME : K_LINE);
        g_text_c(T_XS, x + 14, y + 7, PW_NAME[k], hot ? K_INK : on ? K_LIME : K_WHITE);
    }
    for (k = 0; k < (uint32_t)NPB; k++) {       /* the black keys' effects */
        int32_t x = 4 + (int32_t)k * 21, y = 163;
        int hot = (pad_mask >> k) & 1u;
        if (!g_vis(y, 22)) continue;
        g_box(x, y, 20, 22, hot ? K_ORANGE : K_PANEL, hot ? K_WHITE : K_LINE);
        g_text_c(T_XS, x + 10, y + 7, PBS[k], hot ? K_INK : K_GREY);
    }
    if (g_vis(190, 50)) {
        g_text(T_XS, 6, 192, "MORPH", K_GREY);
        p = str_cat(t, "X "); p = fmt_u(p, (uint32_t)ex * 100u / 127u, 0); p = str_cat(p, " \xB7 Y "); fmt_u(p, (uint32_t)ey * 100u / 127u, 0);
        g_text(T_S, 46, 190, t, K_WHITE);
        g_rect(0, 212, 240, 1, K_LINE);
        g_text(T_XS, 6, 218, "K1-K4 MACROS \xB7 SELECT X \xB7 ALGO Y", K_GREY);
        g_text(T_XS, 6, 229, "OCT+ HELD + KEYS: PADS \xB7 A PAGE BUTTON: BACK", K_GREY);
    }
}
static void draw_win(void)
{
    uint32_t k;
    char t[48], b[24], *p;
    if (win == 1u) {
        win_head("FX LIVE", "PLAYED LIVE \xB7 NOT SAVED \xB7 OFF AT START", "SEL 3/5");
        for (k = 0; k < 4u; k++) {
            static const char *const KN[4] = {"K1", "K2", "K3", "K4"};
            uint32_t id = LV_IT[k];
            win_row(42 + (int32_t)k * 39, KN[k], id, win_cur == k, item_get(id) != 0, id == G_LV_FLT);
        }
        for (k = 0; k < 2u; k++) {              /* the two settings behind: SELECT, ALGO */
            int32_t x = 4 + (int32_t)k * 117, y = 200;
            uint32_t id = LV_IT[4u + k];
            if (!g_vis(y, 22)) continue;
            g_box(x, y, 115, 22, K_PANEL, win_cur == 4u + k ? K_WHITE : K_LINE);
            g_text(T_XS, x + 6, y + 7, item_name(id), K_GREY);
            item_text(id, b);
            g_text_r(T_S, x + 109, y + 5, b, K_WHITE);
        }
        if (g_vis(226, 12)) g_text(T_XS, 6, 228, "SELECT + ALGO: ALL SIX \xB7 A PAGE BUTTON: BACK", K_GREY);
        return;
    }
    if (win == 6u) { draw_pads(); return; }
    if (win == 7u) { draw_perf(); return; }
    if (win == 4u) { draw_mxy(); return; }
    if (win == 5u) { draw_mab(); return; }
    win_head("MACROS", "4 KNOBS \xB7 4 TARGETS EACH \xB7 EVERY SOUND", "SEL 4/5");
    for (k = 0; k < 4u; k++) {                  /* each macro: its value, its targets */
        int32_t y = 42 + (int32_t)k * 39, x, s;
        int on = mc_val[k] != 0;
        if (!g_vis(y, 36)) continue;
        g_box(4, y, 232, 36, on ? K_LIMED : K_PANEL, (mc_pos >> 3) == k ? K_WHITE : on ? K_LIME : K_LINE);
        p = str_cat(t, "M");
        *p++ = (char)('1' + k); *p = 0;
        g_text(T_M, 10, y + 3, t, on ? K_LIME : K_WHITE);
        item_text(G_MC1 + k, b);
        g_text_r(T_M, 230, y + 3, b, on ? K_LIME : K_WHITE);
        g_vbar(10, y + 26, 220, 5, item_frac(G_MC1 + k), on ? K_LIME : K_CYAN);
        for (s = 0, x = 40; s < 4; s++) {       /* the targets, short: CUTOFF +34 */
            uint32_t q = k * 4u + (uint32_t)s, d = perf.mc_dst[q];
            int32_t a = perf.mc_amt[q], w;
            const char *nm;
            if (!d || !a) continue;
            nm = dst_name(d);
            p = t;
            for (w = 0; w < 7 && nm[w]; w++) *p++ = nm[w];
            *p++ = ' ';
            p = a ? fmt_s(p, a * 100 / 64) : fmt_u(p, 0, 0);
            *p = 0;
            w = g_text_w(T_XS, t);
            if (x + w > 190) break;
            g_text(T_XS, x, y + 7, t, (mc_pos >> 1) == q ? K_WHITE : K_MOD);
            x += w + 8;
        }
    }
    if (g_vis(198, 42)) {                       /* the target being edited */
        uint32_t q = (mc_pos >> 1) & 15u;
        g_box(4, 198, 232, 40, K_PANEL, K_LIME);
        p = str_cat(t, "M");
        *p++ = (char)('1' + (q >> 2));
        p = str_cat(p, " \xB7 TARGET ");
        *p++ = (char)('1' + (q & 3u)); *p = 0;
        g_text(T_XS, 10, 202, t, K_LIME);
        g_text_r(T_XS, 230, 202, "SELECT: NEXT \xB7 ALGO: CHANGE", K_GREY);
        g_text(T_S, 10, 217, perf.mc_dst[q] ? dst_name(perf.mc_dst[q]) : "- NONE -", (mc_pos & 1u) ? K_GREY : K_WHITE);
        item_text(G_MC_AMT, b);
        g_text_r(T_S, 230, 217, b, (mc_pos & 1u) ? K_WHITE : K_GREY);
        if (mc_pos & 1u) g_rect(230 - g_text_w(T_S, b), 233, g_text_w(T_S, b), 2, K_LIME);
        else g_rect(10, 233, g_text_w(T_S, perf.mc_dst[q] ? dst_name(perf.mc_dst[q]) : "- NONE -"), 2, K_LIME);
    }
}

/* ---------------------------------------------------------------- the EQ (FX held): the curve on top, the 4 bands under it */
/* the EQ: one band at a time, each knob one job (K1 FREQ, K2 GAIN, K3 Q, K4 SHAPE), SELECT the band, ALGO its frequency;
 * on the picture the band chosen shows where it acts: a line at its frequency, its width (Q) as a bracket, its gain */
static void eq_shape(uint32_t t, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)   /* the band's shape, drawn */
{
    int32_t m = y + h / 2, a = x + w / 3, b = x + 2 * w / 3, top = y + 2, bot = y + h - 2;
    switch (t) {
    case EQ_LOCUT: g_line(x, bot, a, m, c); g_line(a, m, x + w, m, c); break;
    case EQ_LOSH: g_line(x, top, a, top, c); g_line(a, top, b, m, c); g_line(b, m, x + w, m, c); break;
    case EQ_BELL: g_line(x, m, a, m, c); g_line(a, m, x + w / 2, top, c); g_line(x + w / 2, top, b, m, c); g_line(b, m, x + w, m, c); break;
    case EQ_NOTCH: g_line(x, m, a + 4, m, c); g_line(a + 4, m, x + w / 2, bot, c); g_line(x + w / 2, bot, b - 4, m, c); g_line(b - 4, m, x + w, m, c); break;
    case EQ_HISH: g_line(x, m, a, m, c); g_line(a, m, b, top, c); g_line(b, top, x + w, top, c); break;
    case EQ_HICUT: g_line(x, m, b, m, c); g_line(b, m, x + w, bot, c); break;
    default: { int32_t k; for (k = 0; k < w; k += 6) g_rect(x + k, m, 3, 1, c); }
    }
}
static void draw_eq(void)
{
    static const uint16_t BWO[8] = {320, 230, 174, 139, 101, 72, 48, 29};   /* a bell's width in octaves x 100, by Q */
    const int32_t gx = 6, gy = 32, gw = 228, gh = 92, mid = gy + gh / 2;
    const uint32_t c = eq_cur & 3u, f = perf.eq_f[c] % 48u, t = perf.eq_t[c] % NEQT;
    const uint16_t kc4[4] = {PAL_BW ? K_WHITE : KNOB_RGB[0], PAL_BW ? K_WHITE : KNOB_RGB[1], PAL_BW ? K_WHITE : KNOB_RGB[2], PAL_BW ? K_WHITE : KNOB_RGB[3]};
    uint32_t k;
    int32_t x, py = 0, fx = gx + (int32_t)(f * (uint32_t)(gw - 1) / 47u);
    char b[16];
    if (g_vis(0, 30)) {                         /* the four bands as tabs: the one chosen lit (SELECT) */
        g_text(T_S, 6, 2, "EQ", K_LIME);
        g_text_r(T_XS, 234, 4, "SELECT: BAND", K_GREY);
        for (k = 0; k < 4u; k++) {
            int32_t x0 = 4 + (int32_t)k * 58, on = k == c;
            char n[3] = {(char)('1' + k), 0, 0};
            g_box(x0, 15, 56, 14, on ? K_LIME : K_PANEL, on ? K_LIME : K_LINE);
            g_text(T_XS, x0 + 4, 17, n, on ? K_INK : K_GREY);
            g_text(T_XS, x0 + 14, 17, L_EQS[perf.eq_t[k] % NEQT], on ? K_INK : perf.eq_t[k] ? K_WHITE : K_GREY);
        }
    }
    if (g_vis(gy, gh)) {
        static const uint16_t GRID[3] = {100, 1000, 10000};
        static const char *const GL[3] = {"100", "1k", "10k"};
        g_box(gx - 2, gy, gw + 4, gh, K_PANEL, K_LINE);
        for (k = 0; k < 3u; k++) {              /* 100 Hz, 1 kHz, 10 kHz */
            int32_t lx = gx + (int32_t)((uint32_t)(ilog2q8(GRID[k]) - ilog2q8(20)) * (uint32_t)(gw - 1) / (uint32_t)(ilog2q8(18000) - ilog2q8(20)));
            g_rect(lx, gy + 2, 1, gh - 4, K_LINE);
            g_text(T_XS, lx + 2, gy + gh - 10, GL[k], K_GREY);
        }
        g_rect(gx, mid, gw, 1, K_LINE);         /* 0 dB, +12, -12 */
        g_text(T_XS, gx + 2, gy + 3, "+12", K_GREY);
        g_text(T_XS, gx + 2, gy + gh - 20, "-12", K_GREY);
        if (t) {                                /* the band chosen: its width (Q) shaded, its frequency a line */
            int32_t half = (int32_t)BWO[perf.eq_q[c] & 7u] * (gw - 1) / 981 / 2, l = clampi(fx - half, gx, gx + gw - 1), r = clampi(fx + half, gx, gx + gw - 1);
            g_rect(l, gy + 1, r - l + 1, gh - 2, g_mix(K_PANEL, kc4[2], 34));
            g_rect(l, gy + 13, 1, 6, kc4[2]); g_rect(r, gy + 13, 1, 6, kc4[2]); g_rect(l, gy + 16, r - l + 1, 1, kc4[2]);   /* (the width: a bracket under the frequency) */
            for (x = gy + 2; x < gy + gh - 2; x += 4) g_rect(fx, x, 1, 2, kc4[0]);
        }
        for (x = 0; x < gw; x++) {              /* the curve, filled to 0 dB */
            uint32_t fi = (uint32_t)x * 111u * 256u / (uint32_t)(gw - 1), i0 = fi >> 8, i1 = i0 < 111u ? i0 + 1u : 111u;
            int32_t db = eq_curve[i0] + (((eq_curve[i1] - eq_curve[i0]) * (int32_t)(fi & 255u)) >> 8);
            int32_t y = clampi(mid - db * (gh / 2 - 6) / (16 * 14), gy + 2, gy + gh - 3);
            if (y != mid) g_rect(gx + x, y < mid ? y : mid, 1, y < mid ? mid - y : y - mid, g_mix(K_PANEL, K_WAVE, 60));
            if (x) g_line(gx + x - 1, py, gx + x, y, K_WAVE);
            py = y;
        }
        for (k = 0; k < 4u; k++) {              /* the bands: a dot each; the one chosen big, with its numbers */
            uint32_t fk = perf.eq_f[k] % 48u, ci = fk * 111u / 47u;
            int32_t bx = gx + (int32_t)(fk * (uint32_t)(gw - 1) / 47u), by = clampi(mid - eq_curve[ci] * (gh / 2 - 6) / (16 * 14), gy + 6, gy + gh - 7);
            char n[2] = {(char)('1' + k), 0};
            if (k == c) continue;
            g_rect(bx - 2, by - 2, 5, 5, perf.eq_t[k] ? K_GREY : K_LINE);
            g_text_c(T_XS, bx, by - 13 < gy + 2 ? by + 5 : by - 13, n, K_GREY);
        }
        {
            uint32_t ci = f * 111u / 47u;
            int32_t by = clampi(mid - eq_curve[ci] * (gh / 2 - 6) / (16 * 14), gy + 7, gy + gh - 8), tx;
            uint32_t sv = eq_cur;
            g_box(fx - 5, by - 5, 11, 11, K_LIME, K_WHITE);
            eq_cur = (uint8_t)c;
            item_text(G_EQ_F, b);
            tx = clampi(fx - g_text_w(T_XS, b) / 2, gx + 24, gx + gw - 2 - g_text_w(T_XS, b));
            g_text(T_XS, tx, gy + 3, b, kc4[0]);
            if (t == EQ_LOSH || t == EQ_BELL || t == EQ_HISH) {
                item_text(G_EQ_G, b);
                g_text(T_XS, fx + 9 + g_text_w(T_XS, b) > gx + gw ? fx - 9 - g_text_w(T_XS, b) : fx + 9, by - 4, b, kc4[1]);
            }
            eq_cur = (uint8_t)sv;
        }
    }
    {                                           /* the four knobs of the band chosen, each with its job written on it */
        static const char *const KN[4] = {"FREQ", "GAIN", "Q", "SHAPE"};
        static const uint8_t ID[4] = {G_EQ_F, G_EQ_G, G_EQ_Q, G_EQ_T};
        for (k = 0; k < 4u; k++) {
            int32_t x0 = 4 + (int32_t)k * 58, y0 = 128, v;
            char kn[3] = {'K', (char)('1' + k), 0};
            uint32_t sv = eq_cur;
            if (!g_vis(y0, 88)) continue;
            g_box(x0, y0, 56, 88, K_PANEL, kc4[k]);
            g_rect(x0 + 4, y0 + 5, 5, 5, kc4[k]);
            g_text(T_XS, x0 + 12, y0 + 4, kn, kc4[k]);
            g_text(g_fit(T_XS, KN[k], 30), x0 + 25, y0 + 4, KN[k], K_WHITE);
            eq_cur = (uint8_t)c;
            v = item_get(ID[k]);
            if (k == 3u) {
                eq_shape(t, x0 + 6, y0 + 22, 44, 30, K_LIME);
                g_text_c(g_fit(T_XS, L_EQT[t], 50), x0 + 28, y0 + 60, L_EQT[t], K_WHITE);
            } else {
                item_text(ID[k], b);
                if (k == 2u) {                  /* (Q: the width drawn, narrow to wide) */
                    int32_t hw = 4 + (int32_t)BWO[v & 7] * 18 / 320, cx = x0 + 28;
                    g_line(x0 + 6, y0 + 50, cx - hw, y0 + 50, K_LIME); g_line(cx - hw, y0 + 50, cx, y0 + 24, K_LIME);
                    g_line(cx, y0 + 24, cx + hw, y0 + 50, K_LIME); g_line(cx + hw, y0 + 50, x0 + 50, y0 + 50, K_LIME);
                    g_text_c(T_S, cx, y0 + 56, b + 2, K_WHITE);
                } else if (k == 1u && !(t == EQ_LOSH || t == EQ_BELL || t == EQ_HISH)) {
                    g_text(g_fit(T_M, b, 48), x0 + 4, y0 + 28, b, K_GREY);
                    g_text(T_XS, x0 + 4, y0 + 54, t ? "NO GAIN" : "TURN: A", K_GREY);
                    g_text(T_XS, x0 + 4, y0 + 64, t ? "ON A CUT" : "BELL", K_GREY);
                } else g_text(g_fit(T_M, b, 48), x0 + 4, y0 + 28, b, K_WHITE);
            }
            eq_cur = (uint8_t)sv;
            if (k == 1u) {                      /* (the gain: from the middle) */
                int32_t w = (int32_t)perf.eq_g[c] * 23 / 12;
                g_rect(x0 + 4, y0 + 78, 48, 4, K_TRACK);
                g_rect(w >= 0 ? x0 + 28 : x0 + 28 + w, y0 + 78, w >= 0 ? w + 1 : -w, 4, kc4[1]);
                g_rect(x0 + 28, y0 + 76, 1, 8, K_WHITE);
            } else if (k == 0u) g_vbar(x0 + 4, y0 + 78, 48, 4, f * 256u / 47u, kc4[0]);
            else if (k == 2u) g_vbar(x0 + 4, y0 + 78, 48, 4, (uint32_t)(perf.eq_q[c] & 7u) * 256u / 7u, kc4[2]);
            else g_vbar(x0 + 4, y0 + 78, 48, 4, t * 256u / (NEQT - 1u), kc4[3]);
        }
    }
    if (g_vis(220, 20)) g_text_c(T_XS, 120, 226, "SELECT: BAND  \xB7  ALGO: FREQ  \xB7  FX: NEXT", K_GREY);
}

/* the start's question: CLASSIC or SHIFT, what each is */
static void draw_ask(void)
{
    static const char *const TX[2][4] = {
        {"CLASSIC", "PAGES OF LISTS, LIKE A SYNTH", "SELECT: ROW  ALGO: ITS VALUE", "K1-K4: THE FIRST 4 ROWS"},
        {"SHIFT", "12 SETTINGS A PAGE, 3 COLUMNS", "KNOBS: LEFT  HOLD OCT-: MIDDLE", "HOLD OCT+: RIGHT  TAP: OCTAVE"}};
    uint32_t k;
    if (ask_on == 2u) {                         /* the very first start: the look, four little pictures (each in its own colours) */
        static const char *const TT[4] = {"CLASSIC", "STUDIO", "PURE", "RIBBON CLASSIC"};
        g_text_c(T_XS, 120, 6, "CHOOSE A LOOK", K_GREY);
        for (k = 0; k < 4u; k++) {
            int32_t x = 4 + (int32_t)(k & 1u) * 118, y = 18 + (int32_t)(k >> 1) * 100, sx = x + 5, sy = y + 5, i;
            if (!g_vis(y, 98)) continue;
            theme_set(ASK_TH[k], set.pal);      /* (the tile in that look's colours) */
            g_box(x, y, 114, 96, K_BG, K_LINE);
            switch (k) {
            case 0:                             /* CLASSIC: a title, a picture, the rows (flat) */
                g_rect(sx, sy, 50, 3, K_LIME); g_rect(sx, sy + 6, 104, 1, K_LIME);
                g_rect(sx, sy + 10, 70, 7, K_WHITE);
                g_box(sx, sy + 21, 104, 22, K_PANEL, K_LINE);
                for (i = 0; i < 100; i += 4) g_line(sx + 2 + i, sy + 32 - (int32_t)(sin32((uint32_t)i << 25) >> 12), sx + 6 + i, sy + 32 - (int32_t)(sin32((uint32_t)(i + 4) << 25) >> 12), K_LIME);
                for (i = 0; i < 3; i++) g_box(sx, sy + 46 + i * 9, 104, 8, i ? K_PANEL : K_LIMED, i ? K_LINE : K_LIME);
                break;
            case 1:                             /* STUDIO: a status bar, the eight pages as tabs, rows with their colours */
                g_rect(sx, sy, 104, 4, K_PANEL); g_rect(sx + 2, sy + 1, 40, 2, K_WHITE);
                for (i = 0; i < 8; i++) g_rect(sx + i * 13, sy + 7, 11, 5, i ? K_PANEL : K_LIME);
                g_rect(sx, sy + 15, 64, 6, K_WHITE);
                g_box(sx, sy + 24, 104, 18, K_PANEL, K_LINE);
                for (i = 0; i < 4; i++) {
                    static const uint16_t KC[4] = {0xF20Au, 0xFE00u, 0x3F2Au, 0x3B7Fu};
                    g_rect(sx, sy + 45 + i * 7, 4, 4, KC[i]); g_rect(sx + 7, sy + 45 + i * 7, 40, 2, K_WHITE);
                    g_rect(sx + 7, sy + 48 + i * 7, 30 + i * 15, 1, KC[i]);
                }
                break;
            case 2:                             /* PURE: a tab, a big wave, four knob rings */
                g_rect(sx, sy, 30, 6, K_LIME);
                for (i = 0; i < 100; i += 4) g_line(sx + 2 + i, sy + 26 - (int32_t)(sin32((uint32_t)i << 24) >> 11), sx + 6 + i, sy + 26 - (int32_t)(sin32((uint32_t)(i + 4) << 24) >> 11), K_LIME);
                for (i = 0; i < 4; i++) { dk_dot(sx + 13 + i * 26, sy + 60, 20, K_LIME, 256); dk_dot(sx + 13 + i * 26, sy + 60, 14, K_BG, 256); }
                break;
            default:                            /* RIBBON CLASSIC: the views side by side, the one shown in the middle */
                g_box(sx - 2, sy + 8, 18, 60, K_PANEL, K_LINE); g_box(sx + 88, sy + 8, 18, 60, K_PANEL, K_LINE);
                g_box(sx + 20, sy, 64, 74, K_PANEL, K_LIME);
                g_rect(sx + 24, sy + 4, 30, 4, K_LIME);
                for (i = 0; i < 5; i++) g_box(sx + 24, sy + 12 + i * 11, 56, 9, i ? K_BG : K_LIMED, i ? K_LINE : K_LIME);
                break;
            }
            g_text_c(T_S, x + 57, y + 79, TT[k], K_WHITE);
        }
        theme_set(ASK_TH[ask_sel], set.pal);   /* (back to the one chosen: its frame) */
        {
            int32_t x = 4 + (int32_t)(ask_sel & 1u) * 118, y = 18 + (int32_t)(ask_sel >> 1) * 100;
            if (g_vis(y - 2, 100)) { g_rect(x - 2, y - 2, 118, 2, K_LIME); g_rect(x - 2, y + 96, 118, 2, K_LIME); g_rect(x - 2, y - 2, 2, 100, K_LIME); g_rect(x + 114, y - 2, 2, 100, K_LIME); }
        }
        g_text_c(T_XS, 120, 222, "OCT- / OCT+ OR A KNOB \xB7 SEL: OK", K_WHITE);
        return;
    }
    if (rst_bad) { char t[40]; str_cat(str_cat(t, "LAST STOP: "), rst_why); g_text_c(T_XS, 120, 22, t, K_ORANGE); }
    else g_text_c(T_XS, 120, 22, "HOW DO YOU WANT TO PLAY?", K_GREY);
    for (k = 0; k < 2u; k++) {
        int32_t y = 36 + (int32_t)k * 84;
        int on = k == ask_sel;
        if (!g_vis(y, 78)) continue;
        g_box(8, y, 224, 78, on ? K_LIMED : K_PANEL, on ? K_LIME : K_LINE);
        g_text(T_B, 16, y + 6, TX[k][0], on ? K_LIME : K_WHITE);
        g_text_r(T_XS, 224, y + 10, k ? "OCT+" : "OCT-", on ? K_LIME : K_GREY);
        g_text(T_XS, 16, y + 30, TX[k][1], K_WHITE);
        g_text(T_XS, 16, y + 44, TX[k][2], K_GREY);
        g_text(T_XS, 16, y + 58, TX[k][3], K_GREY);
    }
    g_text_c(T_XS, 120, 210, "OCT- / OCT+ CHOOSE  \xB7  SEL: OK", K_WHITE);
    g_text_c(T_XS, 120, 225, "LATER: SYSTEM > UI MODE", K_GREY);
}



static void draw_logo_pic(void)             /* the start-screen picture (216 x 216, 16 greys), the current band */
{
    uint16_t lut[16];
    int32_t y, x, y1 = g_y0 + (int32_t)BAND_H;
    for (x = 0; x < 16; x++) lut[x] = g_mix(0x0000u, 0xFFFFu, (uint32_t)x * 17u);
    g_rect(0, g_y0, 240, (int32_t)BAND_H, 0x0000u);                   /* (black, whatever the palette) */
    for (y = g_y0; y < y1 && y < (int32_t)KS_LOGO_N; y++) {
        const uint8_t *row = KS_LOGO + KS_LOGO_ROW[y];   /* (the black packed: 0x00 then how many black bytes) */
        uint16_t *d = &g_buf[(uint32_t)(y - g_y0) * 240u + 12u];
        for (x = 0; x < (int32_t)KS_LOGO_N / 2;) {
            uint32_t v = *row++, n = 1;
            if (!v) n = *row++;
            for (; n && x < (int32_t)KS_LOGO_N / 2; n--, x++) { d[2 * x] = lut[v >> 4]; d[2 * x + 1] = lut[v & 15u]; }
        }
    }
}
/* SAVE: the keyboard the keys are, as on the FM-1: the 16 white keys (L .. Z, a space) with their letters at the foot, the
 * 11 black ones (A .. K) with theirs on top; the keys down light up */
static void draw_save_kbd(void)
{
    static const char BL[11] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K'}, WH[16] = {'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '_'};
    const int32_t kx = 4, ky = 176, kw = 14, kh = 48;
    int32_t i;
    char t[2] = {0, 0};
    if (!g_vis(ky - 2, kh + 4)) return;
    g_rect(kx - 2, ky - 2, 16 * kw + 4, kh + 4, 0x0000u);
    for (i = 0; i < 16; i++) {                  /* the white keys */
        int on = kb_down >> PK_WHITE[i] & 1u;
        g_box(kx + i * kw, ky, kw - 1, kh, on ? K_LIME : K_PANEL, on ? K_WHITE : K_LINE);
        t[0] = WH[i];
        g_text_c(T_XS, kx + i * kw + kw / 2, ky + kh - 13, t, on ? K_INK : K_WHITE);
    }
    for (i = 0; i < 11; i++) {                  /* the black ones, between the white keys before and after them */
        int32_t b = PK_BLACK[i], w = 0, bx;
        int on = kb_down >> b & 1u;
        while (w < 16 && PK_WHITE[w] != b - 1) w++;
        bx = kx + (w + 1) * kw - 5;
        g_rect(bx, ky, 10, kh * 55 / 100, on ? K_LIME : 0x0000u);   /* (no outline: the letter reads better) */
        t[0] = BL[i];
        g_text_c(T_XS, bx + 5, ky + 6, t, on ? K_INK : K_LIME);
    }
}
static void draw_all(void)
{
    const view_t *v = view();
    uint32_t vi = vidx();
    g_rect(0, g_y0, 240, (int32_t)BAND_H, K_BG);
    if (g_rad) {                                /* RAD palettes: the accent poured in from the top into the second colour at the bottom, on every theme */
        g_vgrad(0, 0, 240, 240, g_rtop, g_rbot);
    } else if (theme_pal == 2u) {   /* SYNTHWAVE: a light pink rising from the bottom */
        g_vgrad(0, 150, 240, 90, K_BG, g_mix(K_BG, RGB(255, 170, 220), 80));
    } else if (theme_pal == 8u) {               /* FLUO: a yellow glow from the top */
        g_vgrad(0, 0, 240, 80, g_mix(K_BG, RGB(240, 255, 40), 60), K_BG);
    } else if (theme_pal == 9u) {               /* PINK: synthwave, blue above and a pink haze below */
        g_vgrad(0, 0, 240, 120, g_mix(K_BG, RGB(40, 60, 160), 90), K_BG);
        g_vgrad(0, 160, 240, 80, K_BG, g_mix(K_BG, RGB(255, 60, 170), 50));
    } else if (theme == TH_RIBBON || theme == TH_RIBBONC) {   /* RIBBON: a deep night, a little lighter at the top */
        g_vgrad(0, 0, 240, 240, g_mix(K_BG, K_LINE, 60), g_mix(K_BG, 0x0000u, 140));
    } else if (g_sky) {                         /* SKY: a pale blue sky, deeper toward the bottom */
        g_vgrad(0, 0, 240, 240, g_mix(K_BG, 0xFFFFu, 90), g_mix(K_BG, RGB(120, 160, 220), 80));
    } else if (theme == TH_GLOW) {              /* GLOW: the accent glowing in from the top, the second colour from the bottom */
        g_vgrad(0, 0, 240, 96, g_mix(K_BG, K_LIME, 46), K_BG);
        g_vgrad(0, 196, 240, 44, K_BG, g_mix(K_BG, K_CYAN, 30));
    }
    if (theme == TH_TECH && !g_rad) {           /* TECH: a glow from the top (the accent), another from the bottom (the second colour) */
        g_vgrad(0, 0, 240, 130, g_mix(K_BG, K_LIME, 40), K_BG);
        g_vgrad(0, 130, 240, 110, K_BG, g_mix(K_BG, K_CYAN, 44));
    }
    if (ask_on && !splash_until) { draw_ask(); return; }   /* (the picture first) */
    if (win == 3u) draw_eq();
    else if (win) draw_win();
    else if (C3_ON) draw_c3();
    else
    switch (theme) {
    case TH_STUDIO: th_studio(v, vi); break;
    case TH_PURE: th_pure(v, vi); break;
    case TH_RACK: th_rack(v, vi); break;
    case TH_TECH: th_tech(v, vi); break;
    case TH_RIBBON: th_ribbon(v, vi, 0); break;
    case TH_RIBBONC: th_ribbon(v, vi, 1); break;
    default: th_glow(v, vi); break;
    }
    if (VIEWS[vidx()].lay == LY_SAVE && !win && !ask_on && !C3_ON && !splash_until) draw_save_kbd();
    if (dpop_ms && g_vis(22, 200)) {            /* the destination list: the ones before and after, the group of each */
        static const char *const GR[] = {"", "FILTER", "FILTER", "PITCH", "OSC A", "OSC A", "OSC B", "OSC B", "OSC A", "OSC B",
                                         "MIX", "FILTER", "AMP", "MIX", "MIX", "MOD", "OSC A", "OSC B", "OSC A", "OSC B", "MIX",
                                         "FILTER", "MOD", "MOD", "MOD", "STEP", "SAT", "SAT", "CRUSH", "CRUSH", "CRUSH", "CHORUS",
                                         "CHORUS", "CHORUS", "DELAY", "DELAY", "DELAY", "REVERB", "REVERB", "REVERB", "MASTER",
                                         "MASTER", "MASTER", "COMP", "PHASER", "PHASER", "PHASER", "MORPH", "MORPH", "MORPH"};
        uint8_t ls[ND];
        int src = dpop_id < NP && PD[dpop_id].list == L_SRC, mc = dpop_id == G_MC_DST;
        int32_t n = 0, at = 0, i, cur = mc ? perf.mc_dst[(mc_pos >> 1) & 15u] : item_get(dpop_id), y;   /* (item_get: LFO 3's destination is not in the patch) */
        char hd[24], pos[12], *p;
        if (src) for (i = 0; i < NSRC; i++) { if (i == cur) at = n; ls[n++] = (uint8_t)i; }
        else
            for (i = 0; i < ND; i++)
                if (mc || dst_on(DST_ORDER[i]) || DST_ORDER[i] == cur) { if (DST_ORDER[i] == cur) at = n; ls[n++] = DST_ORDER[i]; }
        g_box(18, 22, 204, 198, K_PANEL, K_LIME);
        if (mc) { p = str_cat(hd, "MACRO "); *p++ = (char)('1' + ((mc_pos >> 3) & 3u)); *p = 0; p = str_cat(p, " \xB7 TARGET"); }
        else if (src) { p = str_cat(hd, "MATRIX "); *p++ = (char)('1' + (dpop_id - M1_SRC) / 3); *p = 0; p = str_cat(p, " \xB7 SOURCE"); }
        else {
            p = str_cat(hd, dpop_id == ME_DST ? "MOD ENV" : dpop_id == ST_DST ? "STEP" : dpop_id == L2_DST ? "LFO 2" :
                            dpop_id == L1_DST ? "LFO 1" : "MATRIX");
            str_cat(p, " \xB7 DEST");
        }
        g_text(T_XS, 26, 27, hd, K_LIME);
        p = fmt_u(pos, (uint32_t)at + 1u, 0);
        p = str_cat(p, "/");
        fmt_u(p, (uint32_t)n, 0);
        g_text_r(T_XS, 214, 27, pos, K_GREY);
        for (i = -5; i <= 5; i++) {
            int32_t k = at + i;
            y = 112 + i * 16;
            if (k < 0 || k >= n) continue;
            if (!i) g_box(22, y - 2, 196, 16, K_LIME, K_LIME);
            g_text(T_S, 30, y + 1, src ? L_SRC[ls[k]] : ls[k] ? dst_name(ls[k]) : "OFF", !i ? K_INK : (i == -1 || i == 1) ? K_WHITE : K_GREY);
            if (!src) g_text_r(T_XS, 212, y + 2, GR[ls[k] < sizeof GR / sizeof GR[0] ? ls[k] : 0], !i ? K_INK : K_MOD);
        }
        if (at > 5) g_text_c(T_XS, 120, 38, "\x5E", K_GREY);
        if (n - at > 6) g_text_c(T_XS, 120, 206, "v", K_GREY);
    }
    if (pend_on && g_vis(126, 90)) {            /* the question */
        char q[32], e[32], *p;
        int8_t pp[NP_MAX];
        char nb[14];
        g_box(14, 126, 212, 88, K_PANEL, K_ORANGE);
        g_text_c(T_XS, 120, 134, "SOUND CHANGED \xB7 NOT SAVED", K_ORANGE);
        p = str_cat(q, "LOAD ");
        if (pend_bank) { p = str_cat(p, "B"); p = fmt_u(p, pend_k + 1u, 2); user_get(pend_k, pp); }
        else { p = fmt_u(p, pend_k + 1u, 2); preset_factory(pend_k, pp, nb); }
        p = str_cat(p, " ");
        str_cat(p, pend_bank ? ustore[pend_k].name : FACTORY[pend_k].name);
        g_text_c(T_S, 120, 150, q, K_WHITE);
        p = str_cat(e, L_STYLE[clampi(pp[P_STYLE], 0, NSTYLE - 1)]);
        p = str_cat(p, " \xB7 ");
        eng_str(p, pp);
        g_text_c(T_XS, 120, 168, e, K_CYAN);
        g_text_c(T_XS, 120, 188, "SEL = LOAD \xB7 OTHER BUTTON = KEEP", K_LIME);
        g_text_c(T_XS, 120, 200, "PRESETS: ANOTHER ONE", K_GREY);
    }
    {                                           /* the value turned, big: before -> now (below the picture, when there is one) */
        int32_t vy = VZ_OF[vidx()] ? 146 : 72;   /* (below the picture, the scope included) */
        if (vpop_ms && vpop_id == G_FONT) {     /* FONT: the ones around, each in itself */
            if (!dpop_ms && !pend_on && !win && g_vis(60, 112)) {
                static const char *const FN[NFONT] = {"SANS  AA 123", "MONO  AA 123", "HYPER  AA 123", "PIXEL  AA 123", "OUTFIT  AA 123", "INSTRUMENT  AA 123", "TEKTUR  AA 123", "JETBRAINS  AA 123", "BIG SHOULDERS  123"};
                int32_t cur = (int32_t)(set.font % NFONT), i, y;
                uint8_t bold = g_bold;
                g_box(20, 60, 200, 112, K_PANEL, K_LIME);
                g_text_c(T_XS, 120, 64, "FONT", K_GREY);
                for (i = cur - 2, y = 78; i <= cur + 2; i++, y += 18) {
                    if (i < 0 || i >= (int32_t)NFONT) continue;
                    if (i == cur) g_box(26, y - 2, 188, 18, K_LIMED, K_LIME);
                    g_bold = i == 2;
                    g_text_c(FAMS[i][1], 120, y + 1, FN[i], i == cur ? K_WHITE : K_GREY);
                }
                g_bold = bold;
            }
        } else if (vpop_ms && !dpop_ms && !pend_on && !C3_ON && !win && g_vis(vy, 66)) {
            char now[24], t[40];
            str_cat(str_cat(t, vpop_pre), item_name(vpop_id));
            item_text(vpop_id, now);
            g_box(20, vy, 200, 66, K_PANEL, K_LIME);
            g_text_c(T_XS, 120, vy + 5, t, K_GREY);
            g_text_c(g_text_w(T_L, now) > 190 ? T_B : T_L, 120, vy + 20, now, K_WHITE);
            if (!str_eq(now, vpop_before)) { str_cat(str_cat(t, "WAS "), vpop_before); g_text_c(T_XS, 120, vy + 52, t, K_MOD); }
        }
    }
    if (cmp_on && g_vis(0, 18)) {               /* A/B: the saved sound is playing */
        g_rect(0, 0, 240, 18, K_ORANGE);
        g_text_c(T_XS, 120, 4, "A/B \xB7 SAVED SOUND \xB7 RELEASE SEL", K_INK);
    }
    if (hb.b != 0xFF && (hb.b > BTN_OCT_UP || hb.next) && !hb.used && !hb.fired && fm1_ms - hb.at > 180u && g_vis(178, 42)) {   /* a button held: what comes */
        uint32_t el = fm1_ms - hb.at, fr = hb.next ? (el >= hb.next ? 256u : el * 256u / hb.next) : 0u;
        g_box(14, 180, 212, 38, K_PANEL, hb.next ? K_LIME : K_MOD);
        g_text_c(T_XS, 120, 186, hold_label(hb.b), hb.next ? K_WHITE : K_MOD);
        if (hb.next) g_bar(24, 202, 192, 7, fr, K_LIME);
        else g_text_c(T_XS, 120, 202, "WAITING FOR A KNOB...", K_GREY);
    }
    if (ur_on && g_vis(136, 52)) {               /* SAVE held: undo or redo, how many of each */
        char u[24], r[24];
        str_cat(fmt_u(str_cat(u, "OCT-  UNDO "), undo_n, 0), "");
        str_cat(fmt_u(str_cat(r, "REDO "), redo_n, 0), "  OCT+");
        g_box(10, 136, 220, 50, K_PANEL, K_LIME);
        g_text_c(T_XS, 120, 141, "SAVE HELD \xB7 UNDO / REDO", K_LIME);
        g_text(T_S, 18, 156, u, undo_n ? K_WHITE : K_GREY);
        g_text_r(T_S, 222, 156, r, redo_n ? K_WHITE : K_GREY);
        g_text_c(T_XS, 120, 174, "RELEASE SAVE: DONE", K_GREY);
    }
    if (splash_until) {                         /* the start: the picture (216 x 216, 16 greys), the patch and the cores beneath */
        char b[40], *p;
        draw_logo_pic();
        p = str_cat(b, pname);
        p = str_cat(p, " \xB7 ");
        eng_str(p, patch);
        if (g_vis(218, 22)) {
            g_text_c(T_XS, 120, 220, b, RGB(255, 255, 255));
            if (rst_bad) { str_cat(str_cat(b, "LAST STOP: "), rst_why); g_text_c(T_XS, 120, 231, b, RGB(255, 150, 60)); }
            else g_text_c(T_XS, 120, 231, dual_on ? "TWO CORES \xB7 PRESS ANY BUTTON OR KEY" : dual_why == DW_SILENT ? "CORE 1 SILENT \xB7 ONE CORE" : dual_why == DW_SAFE ? "ONE CORE (SAFE) \xB7 PRESS A KEY" : "PRESS ANY BUTTON OR KEY", RGB(150, 150, 150));
            g_text(T_XS, 4, 232, "BETA", RGB(88, 93, 95));            /* (a beta: on the left, the version on the right, both a little darker) */
            g_text_r(T_XS, 238, 232, "V" APP_VER, RGB(88, 93, 95));
        }
    }
    if (toast_ms && g_vis(196, 22)) {
        int32_t tw = g_text_w(T_S, toast_txt) + 24;   /* (a long one: wider, then smaller type) */
        tw = tw < 180 ? 180 : tw > 228 ? 228 : tw;
        g_box(120 - tw / 2, 196, tw, 22, K_PANEL, K_LIME);
        g_text_c(g_fit(T_S, toast_txt, tw - 8), 120, 202, toast_txt, K_WHITE);
    }
}

/* ---------------------------------------------------------------- the update screen (ota.c's ota_show hook): the name,
 * the step and a rounded loading bar on black (the start picture until 1.0, which did not show), drawn straight to
 * the LCD (no g_frame_send: that polls USB and could re-enter the flash write this runs during). Real hardware only:
 * on KAWA_WASM / DINGHY_HOST, platform.c never includes ota.c or lcd.c (no OTA, no LCD SPI), so nothing here is built
 * (or called) there either. */
#if !defined(DINGHY_HOST)
static uint8_t ota_step_g;
static int32_t ota_code_g;
static void draw_ota_pic(void)                  /* (1.0.1: text and a bar only, no picture: the start picture did not show during installs) */
{
    g_rect(0, g_y0, 240, (int32_t)BAND_H, 0x0000u);
    if (g_vis(70, 40)) {
        g_text_c(T_L, 120, 72, "K-WASUB", RGB(255, 255, 255));
        g_text_c(T_XS, 120, 100, "UPDATE \xB7 DO NOT UNPLUG", RGB(150, 150, 150));
    }
    if (g_vis(120, 50)) {
        if (ota_step_g < 9u) {
            static const char *const OL[6] = {"", "READING THE PACKAGE", "CHECKING IT", "STAGING THE LOADER", "CONFIRMING", "RESTARTING"};
            int32_t w = 192 * (int32_t)ota_step_g / 5;
            g_text_c(T_XS, 120, 126, OL[ota_step_g < 6u ? ota_step_g : 0], RGB(200, 200, 200));
            rb_frame(20, 142, 200, 14, 7, RGB(90, 90, 90), RGB(0, 0, 0), RGB(0, 0, 0));
            if (w > 0) rb_rrect(24, 145, w, 8, 4, RGB(255, 255, 255), RGB(255, 255, 255));
        } else
            g_text_c(T_S, 120, 136, ota_code_g == 1 ? "DRY RUN OK" : "FAILED", RGB(255, 255, 255));
    }
}
static void ota_push(void (*draw)(void))    /* like g_frame_send, but no polling and no diff-skip: a few redraws only, glitch-free */
{
    uint32_t b, i;
    for (b = 0; b < NBAND; b++) {
        g_y0 = (int32_t)(b * BAND_H);
        draw();
        for (i = 0; i < 240u * BAND_H; i++) { uint16_t c = g_buf[i]; g_txb[i] = (uint16_t)(c >> 8 | c << 8); }
        lcd_blit(0, (uint32_t)g_y0, 240, BAND_H, g_txb);
        lcd_sync();                 /* g_txb is reused right away: wait for this band's transfer, unlike the ping-pong main loop */
    }
}
static void ota_show(uint32_t step, int32_t code)
{
#if !defined(KAWA_WASM)
    if (recovery_active) return;
#endif
    ota_step_g = (uint8_t)step; ota_code_g = code;
    ota_push(draw_ota_pic);
}
#endif
