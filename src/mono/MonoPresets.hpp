// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – the factory presets, by style (keep the list in step with PRESETS in tools/mono_panel.py).
// Plain C++ without Rack: the values are the knob positions, in the order of GenraMono's ParamId, so
// test/test_mono.cpp can play every preset through the engine.
#pragma once
#include "MonoDsp.hpp"

namespace gmono {

static const int NUM_PRESET_PARAMS = 27;

struct Preset {
	const char* style;
	const char* name;
	// oscillators: wave 0 tri · .33 saw · .67 square · 1 narrow pulse; tune -1..1 (±12 st, fine in the middle)
	float wave1, wave2, tune1, tune2, level1, level2, glide;
	// filter: type, cutoff (20 Hz · 1000^x), resonance, EG amount (-1..1), drive, key tracking
	float ftype, cutoff, res, egAmt, drive, keytrk;
	// envelopes: times 1 ms · 10000^x, sustain 0..1
	float fA, fD, fS, fR, aA, aD, aS, aR;
	// LFO: rate (0.05 Hz · 1000^x), wave (0 square, 1 triangle), → pitch, → filter
	float rate, lfoTri, lfoPitch, lfoFilter;
	// ACCENT (how strongly ACC acts), stereo WIDTH
	float accent, width;
	float gain;  // the output level that makes this preset loud and even with the others

	/** The values in ParamId order. */
	void values(float* v) const {
		const float a[NUM_PRESET_PARAMS] = {wave1, wave2, tune1, tune2, level1, level2, glide,
		                                    ftype, cutoff, res, egAmt, drive, keytrk,
		                                    fA, fD, fS, fR, aA, aD, aS, aR,
		                                    rate, lfoTri, lfoPitch, lfoFilter, accent, width};
		for (int i = 0; i < NUM_PRESET_PARAMS; i++)
			v[i] = a[i];
	}
	Params params() const {
		Params p;
		p.wave1 = wave1, p.wave2 = wave2, p.tune1 = tune1, p.tune2 = tune2;
		p.level1 = level1, p.level2 = level2, p.glide = glide;
		p.ftype = (int) ftype, p.cutoff = cutoff, p.res = res, p.egAmt = egAmt, p.drive = drive, p.keytrk = keytrk;
		p.fA = fA, p.fD = fD, p.fS = fS, p.fR = fR, p.aA = aA, p.aD = aD, p.aS = aS, p.aR = aR;
		p.rate = rate, p.lfoSquare = lfoTri < 0.5f, p.lfoPitch = lfoPitch, p.lfoFilter = lfoFilter;
		p.accent = accent, p.width = width;
		p.outGain = gain;
		return p;
	}
};

// tune: +12 st = 1, −12 st = −1, +7 st = 0.764, a few cents ≈ 0.05–0.15
static const Preset PRESETS[] = {
    //  style, name,                 wave1 wave2 tune1 tune2  lvl1 lvl2 glide  filter    cut  res   eg   drive ktrk   fA   fD   fS   fR   aA   aD   aS   aR   rate tri  >pit >flt  acc  width
    {"SUB", "DEEP SUB",              0.00f, 0.00f, 0.f, -1.00f, .85f, .45f, .00f, LADDER24, .30f, .00f, .10f, .10f, .50f, .00f, .45f, .00f, .40f, .15f, .50f, 1.0f, .45f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.016f},
    {"SUB", "DUB SUB",               0.20f, 0.00f, 0.f, -1.00f, .70f, .70f, .10f, LADDER24, .28f, .25f, .25f, .30f, .30f, .00f, .55f, .20f, .45f, .20f, .60f, 1.0f, .55f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.566f},
    {"SUB", "808 GLIDE",             0.00f, 0.33f, 0.f,  0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.102f},
    {"SUB", "ROUND SUB",             0.10f, 0.10f, 0.f,  0.06f, .80f, .60f, .00f, LADDER12, .35f, .10f, .15f, .20f, .40f, .00f, .50f, .10f, .40f, .10f, .50f, .90f, .40f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.577f},

    {"CLASSIC", "TAURUS",            0.33f, 0.33f, 0.f,  0.08f, .85f, .85f, .10f, LADDER24, .38f, .35f, .45f, .35f, .30f, .00f, .55f, .15f, .45f, .00f, .50f, 1.0f, .40f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.244f},
    {"CLASSIC", "FAT MOOG",          0.33f, 0.50f, 0.f, -1.00f, .85f, .75f, .00f, LADDER24, .42f, .30f, .50f, .50f, .30f, .00f, .50f, .25f, .45f, .00f, .50f, 1.0f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.157f},
    {"CLASSIC", "FUNK PLUCK",        0.67f, 0.33f, 0.f,  0.07f, .80f, .70f, .00f, LADDER24, .30f, .50f, .70f, .25f, .50f, .00f, .42f, .00f, .35f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.424f},
    {"CLASSIC", "SYNTHWAVE",         0.33f, 0.33f, 0.f,  0.12f, .80f, .80f, .00f, OTA,      .40f, .20f, .40f, .15f, .40f, .00f, .48f, .20f, .40f, .00f, .45f, .85f, .35f, .35f, 1, .00f, .05f, 0.50f, 0.50f, /*gain*/ 1.894f},

    {"GARAGE · 2-STEP", "ORGAN BASS",    0.67f, 0.67f, 0.f,  1.00f, .80f, .45f, .00f, SVF_LP,   .50f, .15f, .30f, .20f, .50f, .00f, .40f, .10f, .35f, .00f, .45f, .75f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.233f},
    {"GARAGE · 2-STEP", "2-STEP BOUNCE", 0.33f, 0.00f, 0.f, -1.00f, .80f, .60f, .20f, LADDER24, .33f, .40f, .60f, .30f, .40f, .00f, .45f, .05f, .40f, .00f, .50f, .70f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.163f},
    {"GARAGE · 2-STEP", "SPEED REESE",   0.33f, 0.33f, 0.f,  0.18f, .80f, .80f, .00f, LADDER12, .45f, .30f, .35f, .45f, .30f, .20f, .55f, .40f, .45f, .00f, .50f, 1.0f, .35f, .30f, 1, .00f, .10f, 0.50f, 0.40f, /*gain*/ 1.145f},

    {"DUB · DUBSTEP · GRIME", "REESE",        0.33f, 0.33f, 0.f,  0.14f, .80f, .80f, .00f, LADDER24, .40f, .20f, .15f, .55f, .30f, .30f, .60f, .60f, .50f, .10f, .50f, 1.0f, .45f, .20f, 1, .00f, .15f, 0.50f, 0.50f, /*gain*/ 1.280f},
    {"DUB · DUBSTEP · GRIME", "WOBBLE",       0.45f, 0.33f, 0.f, -1.00f, .80f, .70f, .00f, LADDER24, .25f, .45f, .00f, .50f, .30f, .00f, .50f, .00f, .40f, .05f, .50f, 1.0f, .35f, .52f, 1, .00f, .70f, 0.50f, 0.00f, /*gain*/ 1.878f},
    {"DUB · DUBSTEP · GRIME", "GRIME SQUARE", 0.67f, 0.67f, 0.f, -1.00f, .85f, .60f, .15f, MS20,     .45f, .30f, .35f, .50f, .30f, .00f, .48f, .20f, .40f, .00f, .50f, .80f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.213f},

    {"ACID", "ACID 303",             0.33f, 0.00f, 0.f,  0.00f, .95f, .00f, .25f, DIODE,    .32f, .78f, .60f, .45f, .30f, .00f, .48f, .00f, .40f, .00f, .50f, .80f, .25f, .45f, 1, .00f, .00f, 0.70f, 0.00f, /*gain*/ 3.831f},
    {"ACID", "SQUELCH",              0.67f, 0.00f, 0.f,  0.00f, .85f, .00f, .30f, DIODE,    .25f, .88f, .75f, .55f, .30f, .00f, .42f, .00f, .35f, .00f, .50f, .80f, .25f, .45f, 1, .00f, .00f, 0.80f, 0.00f, /*gain*/ 3.289f},
    {"ACID", "RUBBER",               0.33f, 0.67f, 0.f, -1.00f, .80f, .50f, .25f, MS20,     .30f, .65f, .55f, .30f, .30f, .00f, .50f, .00f, .40f, .00f, .50f, .85f, .30f, .45f, 1, .00f, .00f, 0.60f, 0.00f, /*gain*/ 1.338f},

    {"HOUSE · TECHNO · ELECTRO", "DEEP HOUSE",   0.50f, 0.00f, 0.f, -1.00f, .75f, .60f, .00f, LADDER24, .30f, .25f, .45f, .20f, .40f, .00f, .50f, .10f, .40f, .00f, .52f, .70f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.635f},
    {"HOUSE · TECHNO · ELECTRO", "MINIMAL BLIP", 0.67f, 0.00f, 0.f,  1.00f, .80f, .30f, .00f, LADDER12, .35f, .40f, .60f, .20f, .50f, .00f, .33f, .00f, .30f, .00f, .40f, .00f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 6.000f},
    {"HOUSE · TECHNO · ELECTRO", "ELECTRO ZAP",  0.85f, 0.33f, 0.f,  0.08f, .80f, .60f, .00f, OTA,      .30f, .50f, .80f, .35f, .30f, .00f, .36f, .00f, .30f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.158f},

    {"HIP-HOP · LO-FI", "BOOM BAP",      0.15f, 0.00f, 0.f,  0.00f, .85f, .40f, .10f, LADDER24, .26f, .15f, .30f, .45f, .40f, .00f, .45f, .00f, .40f, .00f, .62f, .30f, .45f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.467f},
    {"HIP-HOP · LO-FI", "LO-FI ROUND",   0.00f, 0.33f, 0.f,  0.10f, .80f, .35f, .00f, OTA,      .30f, .10f, .15f, .30f, .40f, .20f, .55f, .20f, .50f, .15f, .55f, .85f, .50f, .40f, 1, .06f, .00f, 0.50f, 0.00f, /*gain*/ 1.934f},
    {"HIP-HOP · LO-FI", "TRIP-HOP",      0.33f, 0.67f, 0.f, -1.00f, .75f, .65f, .20f, LADDER24, .27f, .35f, .30f, .55f, .30f, .25f, .60f, .20f, .55f, .20f, .60f, .90f, .55f, .30f, 1, .00f, .12f, 0.50f, 0.00f, /*gain*/ 1.272f},
	// ---- GENRE PRESETS (2.31, tools/genre_presets.py): one per genre that had none; do not edit by hand
	{"MINIMAL", "MINIMAL ROLL", 0.50f, 0.00f, 0.f, -1.00f, .75f, .60f, .00f, LADDER24, 0.30f, .25f, 0.50f, .20f, .40f, .00f, .50f, .10f, .40f, .00f, 0.42f, 0.30f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 5.661f},
	{"TRANCE · PSYTRANCE", "TRANCE ROLLER", 0.33f, 0.33f, 0.f, 0.08f, .85f, .85f, .10f, LADDER24, 0.35f, .35f, .45f, .35f, .30f, .00f, .55f, .15f, .45f, .00f, 0.42f, 0.25f, 0.20f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.900f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKBEAT MOOG", 0.33f, 0.50f, 0.f, -1.00f, .85f, .75f, .00f, LADDER24, .42f, .30f, .50f, .50f, .30f, .00f, .50f, .25f, .45f, .00f, 0.40f, 0.50f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.063f},
	{"DRUM & BASS · JUNGLE", "JUNGLE SUB", 0.00f, 0.00f, 0.f, -1.00f, .85f, .45f, 0.15f, LADDER24, .30f, .00f, .10f, .10f, .50f, .00f, .45f, .00f, .40f, .15f, .50f, 1.0f, .45f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.990f},
	{"DRUM & BASS · JUNGLE", "DNB REESE", 0.33f, 0.33f, 0.f, 0.14f, .80f, .80f, .00f, LADDER24, 0.45f, .20f, .15f, 0.40f, .30f, .30f, .60f, .60f, .50f, .10f, .50f, 1.0f, .45f, .20f, 1, .00f, .15f, 0.50f, 0.50f, /*gain*/ 1.247f},
	{"TRAP · DRILL · PHONK", "TRAP 808", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, 0.30f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.287f},
	{"TRAP · DRILL · PHONK", "DRILL GLIDE", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, 0.60f, LADDER24, .25f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.036f},
	{"DOWNTEMPO · TRIP-HOP", "TRIP-HOP MOOG", 0.33f, 0.67f, 0.f, -1.00f, .75f, .65f, .20f, LADDER24, 0.30f, .35f, .30f, .55f, .30f, .25f, .60f, .20f, .55f, .20f, .60f, .90f, .55f, .30f, 1, .00f, .12f, 0.50f, 0.00f, /*gain*/ 1.240f},
	{"DOWNTEMPO · TRIP-HOP", "DOWNTEMPO ROUND", 0.00f, 0.33f, 0.f, 0.10f, .80f, .35f, .00f, OTA, 0.32f, .10f, .15f, .30f, .40f, .20f, .55f, .20f, .50f, .15f, .55f, .85f, .50f, .40f, 1, .06f, .00f, 0.50f, 0.00f, /*gain*/ 1.823f},
	{"SYNTHWAVE · DARKWAVE", "SYNTHWAVE PULSE", 0.67f, 0.33f, 0.f, 0.12f, .80f, .80f, .00f, OTA, .40f, .20f, .40f, .15f, .40f, .00f, .48f, .20f, .40f, .00f, .45f, .85f, .35f, .35f, 1, .00f, .05f, 0.50f, 0.50f, /*gain*/ 1.722f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "AMBIENT SUB", 0.10f, 0.10f, 0.f, 0.06f, .80f, .60f, .00f, LADDER12, .35f, .10f, .15f, .20f, .40f, .00f, .50f, .10f, .40f, 0.40f, .50f, .90f, 0.60f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.543f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEAT PLUCK", 0.67f, 0.33f, 0.f, 0.07f, .80f, .70f, .00f, LADDER24, 0.34f, .50f, .70f, .25f, .50f, .00f, .42f, .00f, .35f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.096f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEATS SUB", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, 0.20f, LADDER24, .25f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.213f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "LOG DRUM", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, 0.35f, 0.30f, 0.50f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, 0.55f, 0.00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 4.637f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BAILE 808", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, 0.60f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.213f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "DEMBOW SUB", 0.00f, 0.00f, 0.f, -1.00f, .85f, .45f, .00f, LADDER24, .30f, .00f, .10f, .10f, .50f, .00f, .45f, .00f, .40f, .15f, 0.40f, 0.60f, .45f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 4.772f},
	{"UK FUNKY · BASSLINE · FOOTWORK", "FOOTWORK 808", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, 0.58f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 5.733f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKCORE ACID", 0.67f, 0.00f, 0.f, 0.00f, .85f, .00f, .30f, DIODE, .25f, .88f, .75f, 0.60f, .30f, .00f, .42f, .00f, .35f, .00f, .50f, .80f, .25f, .45f, 1, .00f, .00f, 0.80f, 0.00f, /*gain*/ 3.219f},
	{"UK FUNKY · BASSLINE · FOOTWORK", "UK FUNKY BASS", 0.50f, 0.00f, 0.f, -1.00f, .75f, .60f, .00f, LADDER24, .30f, .25f, 0.55f, .20f, .40f, .00f, .50f, .10f, .40f, .00f, 0.35f, .70f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.730f},
	{"UK FUNKY · BASSLINE · FOOTWORK", "NICHE WOBBLE", 0.33f, 0.33f, 0.f, 0.18f, .80f, .80f, .00f, LADDER12, .45f, .30f, .35f, .45f, .30f, .20f, .55f, .40f, .45f, .00f, .50f, 1.0f, .35f, 0.55f, 1, .00f, 0.40f, 0.50f, 0.40f, /*gain*/ 1.150f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "GQOM SUB", 0.20f, 0.00f, 0.f, -1.00f, .70f, .70f, .10f, LADDER24, .28f, .25f, .25f, 0.50f, .30f, .00f, .55f, .20f, .45f, .20f, .60f, 1.0f, .55f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.401f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BIG BEAT FUZZ", 0.33f, 0.50f, 0.f, -1.00f, .85f, .75f, .00f, LADDER24, .42f, .30f, .50f, 0.80f, .30f, .00f, .50f, .25f, .45f, .00f, .50f, 1.0f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.210f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKS RUBBER", 0.33f, 0.67f, 0.f, -1.00f, .80f, .50f, .25f, MS20, .30f, 0.50f, .55f, .30f, .30f, .00f, .50f, .00f, .40f, .00f, .50f, .85f, .30f, .45f, 1, .00f, .00f, 0.60f, 0.00f, /*gain*/ 1.362f},
	{"JERSEY CLUB · BALTIMORE CLUB", "JERSEY BOUNCE", 0.33f, 0.00f, 0.f, -1.00f, .80f, .60f, .20f, LADDER24, .33f, .40f, .60f, .30f, .40f, .00f, .45f, .05f, .40f, .00f, 0.35f, .70f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.231f},
	{"JERSEY CLUB · BALTIMORE CLUB", "BALTIMORE BOUNCE", 0.33f, 0.00f, 0.f, -1.00f, .80f, .60f, 0.05f, LADDER24, .33f, .40f, .60f, .30f, .40f, .00f, .45f, .05f, .40f, .00f, .50f, .70f, .35f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.192f},
	{"TRAP · DRILL · PHONK", "PHONK 808", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, 0.70f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.364f},
	{"HARDCORE · EBM · INDUSTRIAL", "HARDCORE ZAP", 0.85f, 0.33f, 0.f, 0.08f, .80f, .60f, .00f, OTA, .30f, .50f, .80f, 0.70f, .30f, .00f, .36f, .00f, .30f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.172f},
	{"TRANCE · PSYTRANCE", "PSY ROLLER", 0.33f, 0.33f, 0.f, 0.08f, .85f, .85f, .10f, LADDER24, 0.40f, .35f, 0.60f, .35f, .30f, .00f, .55f, .15f, .45f, .00f, 0.40f, 0.20f, .40f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 4.572f},
	{"HARDCORE · EBM · INDUSTRIAL", "EBM SEQUENCE", 0.67f, 0.67f, 0.f, -1.00f, .85f, .60f, .15f, MS20, .45f, .30f, .35f, .50f, .30f, .00f, .48f, .20f, .40f, .00f, 0.30f, 0.20f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 4.231f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "DANCEHALL SUB", 0.20f, 0.00f, 0.f, -1.00f, .70f, .70f, .10f, LADDER24, .28f, .25f, .25f, .30f, .30f, .00f, .55f, .20f, .45f, .20f, 0.45f, 1.0f, .55f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.566f},
	{"ROOTS REGGAE", "ROOTS SUB", 0.20f, 0.00f, 0.f, -1.00f, .70f, .70f, .10f, LADDER24, 0.24f, .25f, .25f, .30f, .30f, .00f, .55f, .20f, .45f, .20f, .60f, 1.0f, .55f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.630f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "MOOMBAH WOBBLE", 0.45f, 0.33f, 0.f, -1.00f, .80f, .70f, .00f, LADDER24, .25f, .45f, .00f, .50f, .30f, .00f, .50f, .00f, .40f, .05f, .50f, 1.0f, .35f, 0.48f, 1, .00f, .70f, 0.50f, 0.00f, /*gain*/ 1.894f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "CUMBIA PLUCK", 0.67f, 0.33f, 0.f, 0.07f, .80f, .70f, .00f, LADDER24, .30f, .50f, .70f, .25f, .50f, .00f, .42f, .00f, .35f, .00f, 0.40f, .60f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.546f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BOSSA ROUND", 0.10f, 0.10f, 0.f, 0.06f, .80f, .60f, .00f, LADDER12, .35f, .10f, .15f, .20f, .40f, .00f, .50f, .10f, .40f, .10f, 0.45f, 0.60f, .40f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.225f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "DRONE REESE", 0.33f, 0.33f, 0.f, 0.14f, .80f, .80f, .00f, LADDER24, .40f, .20f, .15f, .55f, .30f, .30f, .60f, .60f, .50f, 0.50f, .50f, 1.0f, 0.60f, .20f, 1, .00f, .15f, 0.50f, 0.50f, /*gain*/ 1.326f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "DARK SUB", 0.00f, 0.00f, 0.f, -1.00f, .85f, .45f, .00f, LADDER24, .30f, .00f, .10f, .10f, .50f, .00f, .45f, .00f, .40f, 0.40f, .50f, 1.0f, 0.60f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 2.990f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "BERLIN SEQUENCE", 0.33f, 0.33f, 0.f, 0.08f, .85f, .85f, .10f, LADDER24, .38f, 0.40f, 0.60f, .35f, .30f, .00f, .55f, .15f, .45f, .00f, 0.30f, 0.20f, .40f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 5.156f},
	{"SYNTHWAVE · DARKWAVE", "DARKWAVE PULSE", 0.33f, 0.33f, 0.f, 0.12f, .80f, .80f, .00f, OTA, 0.36f, .20f, .40f, .15f, .40f, .00f, .48f, .20f, .40f, .00f, .45f, .85f, .35f, .35f, 1, .00f, .05f, 0.50f, 0.60f, /*gain*/ 1.945f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "DUNGEON DRONE", 0.10f, 0.10f, 0.f, 0.06f, .80f, .60f, .00f, LADDER12, .35f, .10f, .15f, .20f, .40f, .00f, .50f, .10f, .40f, 0.30f, .50f, .90f, 0.55f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.550f},
	{"HARDCORE · EBM · INDUSTRIAL", "INDUSTRIAL SQUARE", 0.67f, 0.67f, 0.f, -1.00f, .85f, .60f, .15f, MS20, .45f, .30f, .35f, 0.80f, .30f, .00f, .48f, .20f, .40f, .00f, .50f, .80f, .30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 1.322f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "HORROR GROWL", 0.33f, 0.33f, 0.f, 0.14f, .80f, .80f, .00f, LADDER24, .40f, .20f, .15f, .55f, .30f, .30f, .60f, .60f, .50f, .10f, .50f, 1.0f, .45f, 0.10f, 1, 0.08f, .15f, 0.50f, 0.50f, /*gain*/ 1.308f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "WITCH 808", 0.00f, 0.33f, 0.f, 0.00f, .90f, .15f, .45f, LADDER24, 0.22f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, 0.30f, .45f, 1, .00f, .00f, 0.50f, 0.00f, /*gain*/ 3.951f},
	// ---- END GENRE PRESETS
};
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

}  // namespace gmono
