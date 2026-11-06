// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – the factory presets, by style (keep the list in step with PRESETS in tools/mono_panel.py).
// Plain C++ without Rack: the values are the knob positions, in the order of GenraMono's ParamId, so
// test/test_mono.cpp can play every preset through the engine.
#pragma once
#include "MonoDsp.hpp"

namespace gmono {

static const int NUM_PRESET_PARAMS = 25;

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

	/** The values in ParamId order. */
	void values(float* v) const {
		const float a[NUM_PRESET_PARAMS] = {wave1, wave2, tune1, tune2, level1, level2, glide,
		                                    ftype, cutoff, res, egAmt, drive, keytrk,
		                                    fA, fD, fS, fR, aA, aD, aS, aR,
		                                    rate, lfoTri, lfoPitch, lfoFilter};
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
		return p;
	}
};

// tune: +12 st = 1, −12 st = −1, +7 st = 0.764, a few cents ≈ 0.05–0.15
static const Preset PRESETS[] = {
    //  style, name,                 wave1 wave2 tune1 tune2  lvl1 lvl2 glide  filter    cut  res   eg   drive ktrk   fA   fD   fS   fR   aA   aD   aS   aR   rate tri  >pit >flt
    {"SUB", "DEEP SUB",              0.00f, 0.00f, 0.f, -1.00f, .85f, .45f, .00f, LADDER24, .30f, .00f, .10f, .10f, .50f, .00f, .45f, .00f, .40f, .15f, .50f, 1.0f, .45f, .45f, 1, .00f, .00f},
    {"SUB", "DUB SUB",               0.20f, 0.00f, 0.f, -1.00f, .70f, .70f, .10f, LADDER24, .28f, .25f, .25f, .30f, .30f, .00f, .55f, .20f, .45f, .20f, .60f, 1.0f, .55f, .45f, 1, .00f, .00f},
    {"SUB", "808 GLIDE",             0.00f, 0.33f, 0.f,  0.00f, .90f, .15f, .45f, LADDER24, .25f, .10f, .30f, .45f, .50f, .00f, .50f, .00f, .50f, .00f, .78f, .00f, .70f, .45f, 1, .00f, .00f},
    {"SUB", "ROUND SUB",             0.10f, 0.10f, 0.f,  0.06f, .80f, .60f, .00f, LADDER12, .35f, .10f, .15f, .20f, .40f, .00f, .50f, .10f, .40f, .10f, .50f, .90f, .40f, .45f, 1, .00f, .00f},

    {"CLASSIC", "TAURUS",            0.33f, 0.33f, 0.f,  0.08f, .85f, .85f, .10f, LADDER24, .38f, .35f, .45f, .35f, .30f, .00f, .55f, .15f, .45f, .00f, .50f, 1.0f, .40f, .45f, 1, .00f, .00f},
    {"CLASSIC", "FAT MOOG",          0.33f, 0.50f, 0.f, -1.00f, .85f, .75f, .00f, LADDER24, .42f, .30f, .50f, .50f, .30f, .00f, .50f, .25f, .45f, .00f, .50f, 1.0f, .35f, .45f, 1, .00f, .00f},
    {"CLASSIC", "FUNK PLUCK",        0.67f, 0.33f, 0.f,  0.07f, .80f, .70f, .00f, LADDER24, .30f, .50f, .70f, .25f, .50f, .00f, .42f, .00f, .35f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f},
    {"CLASSIC", "SYNTHWAVE",         0.33f, 0.33f, 0.f,  0.12f, .80f, .80f, .00f, OTA,      .40f, .20f, .40f, .15f, .40f, .00f, .48f, .20f, .40f, .00f, .45f, .85f, .35f, .35f, 1, .00f, .05f},

    {"GARAGE · 2-STEP", "ORGAN BASS",    0.67f, 0.67f, 0.f,  1.00f, .80f, .45f, .00f, SVF_LP,   .50f, .15f, .30f, .20f, .50f, .00f, .40f, .10f, .35f, .00f, .45f, .75f, .30f, .45f, 1, .00f, .00f},
    {"GARAGE · 2-STEP", "2-STEP BOUNCE", 0.33f, 0.00f, 0.f, -1.00f, .80f, .60f, .20f, LADDER24, .33f, .40f, .60f, .30f, .40f, .00f, .45f, .05f, .40f, .00f, .50f, .70f, .35f, .45f, 1, .00f, .00f},
    {"GARAGE · 2-STEP", "SPEED REESE",   0.33f, 0.33f, 0.f,  0.18f, .80f, .80f, .00f, LADDER12, .45f, .30f, .35f, .45f, .30f, .20f, .55f, .40f, .45f, .00f, .50f, 1.0f, .35f, .30f, 1, .00f, .10f},

    {"DUB · DUBSTEP · GRIME", "REESE",        0.33f, 0.33f, 0.f,  0.14f, .80f, .80f, .00f, LADDER24, .40f, .20f, .15f, .55f, .30f, .30f, .60f, .60f, .50f, .10f, .50f, 1.0f, .45f, .20f, 1, .00f, .15f},
    {"DUB · DUBSTEP · GRIME", "WOBBLE",       0.45f, 0.33f, 0.f, -1.00f, .80f, .70f, .00f, LADDER24, .25f, .45f, .00f, .50f, .30f, .00f, .50f, .00f, .40f, .05f, .50f, 1.0f, .35f, .52f, 1, .00f, .70f},
    {"DUB · DUBSTEP · GRIME", "GRIME SQUARE", 0.67f, 0.67f, 0.f, -1.00f, .85f, .60f, .15f, MS20,     .45f, .30f, .35f, .50f, .30f, .00f, .48f, .20f, .40f, .00f, .50f, .80f, .30f, .45f, 1, .00f, .00f},

    {"ACID", "ACID 303",             0.33f, 0.00f, 0.f,  0.00f, .95f, .00f, .25f, DIODE,    .32f, .78f, .60f, .45f, .30f, .00f, .48f, .00f, .40f, .00f, .50f, .80f, .25f, .45f, 1, .00f, .00f},
    {"ACID", "SQUELCH",              0.67f, 0.00f, 0.f,  0.00f, .85f, .00f, .30f, DIODE,    .25f, .88f, .75f, .55f, .30f, .00f, .42f, .00f, .35f, .00f, .50f, .80f, .25f, .45f, 1, .00f, .00f},
    {"ACID", "RUBBER",               0.33f, 0.67f, 0.f, -1.00f, .80f, .50f, .25f, MS20,     .30f, .65f, .55f, .30f, .30f, .00f, .50f, .00f, .40f, .00f, .50f, .85f, .30f, .45f, 1, .00f, .00f},

    {"HOUSE · TECHNO · ELECTRO", "DEEP HOUSE",   0.50f, 0.00f, 0.f, -1.00f, .75f, .60f, .00f, LADDER24, .30f, .25f, .45f, .20f, .40f, .00f, .50f, .10f, .40f, .00f, .52f, .70f, .35f, .45f, 1, .00f, .00f},
    {"HOUSE · TECHNO · ELECTRO", "MINIMAL BLIP", 0.67f, 0.00f, 0.f,  1.00f, .80f, .30f, .00f, LADDER12, .35f, .40f, .60f, .20f, .50f, .00f, .33f, .00f, .30f, .00f, .40f, .00f, .30f, .45f, 1, .00f, .00f},
    {"HOUSE · TECHNO · ELECTRO", "ELECTRO ZAP",  0.85f, 0.33f, 0.f,  0.08f, .80f, .60f, .00f, OTA,      .30f, .50f, .80f, .35f, .30f, .00f, .36f, .00f, .30f, .00f, .50f, .60f, .30f, .45f, 1, .00f, .00f},

    {"HIP-HOP · LO-FI", "BOOM BAP",      0.15f, 0.00f, 0.f,  0.00f, .85f, .40f, .10f, LADDER24, .26f, .15f, .30f, .45f, .40f, .00f, .45f, .00f, .40f, .00f, .62f, .30f, .45f, .45f, 1, .00f, .00f},
    {"HIP-HOP · LO-FI", "LO-FI ROUND",   0.00f, 0.33f, 0.f,  0.10f, .80f, .35f, .00f, OTA,      .30f, .10f, .15f, .30f, .40f, .20f, .55f, .20f, .50f, .15f, .55f, .85f, .50f, .40f, 1, .06f, .00f},
    {"HIP-HOP · LO-FI", "TRIP-HOP",      0.33f, 0.67f, 0.f, -1.00f, .75f, .65f, .20f, LADDER24, .27f, .35f, .30f, .55f, .30f, .25f, .60f, .20f, .55f, .20f, .60f, .90f, .55f, .30f, 1, .00f, .12f},
};
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

}  // namespace gmono
