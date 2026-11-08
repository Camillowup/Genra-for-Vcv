// SPDX-License-Identifier: GPL-3.0-only
// FUNKE – the factory presets, by style. Plain C++ without Rack: test/test_funke.cpp plays every one.
// Knobs 0..1 (times: 1 ms · 10000^x; CUTOFF: 20 Hz · 1000^x; FENV -1..1). Every knob and switch is set.
#pragma once
#include "FunkeDsp.hpp"

namespace gfunke {

/** The knobs in FUNKE's ParamId order. */
static const int NUM_PRESET_PARAMS = 29;

struct Preset {
	const char* style;
	const char* name;
	int model;
	float timbre, bite, width, sub;
	int mode;
	float glide, accent;
	int ftype;
	float cutoff, res, fenv, key, fatk, fdec, drive;
	float atk, dec, sus, rel;
	float vdepth, vrate, vdelay;
	int div;
	float chorus, echo, fdbk, lofi, verb;
	float gain = 1.f;  // the output level that makes this preset loud and even with the others

	void values(float* v) const {
		const float a[NUM_PRESET_PARAMS] = {(float) model, timbre, bite,  width, sub,   (float) mode, glide, accent,
		                                    (float) ftype, cutoff, res,   fenv,  key,   fatk,  fdec,  drive,
		                                    atk,           dec,    sus,   rel,   vdepth, vrate, vdelay, (float) div,
		                                    chorus,        echo,   fdbk,  lofi,  verb};
		for (int i = 0; i < NUM_PRESET_PARAMS; i++)
			v[i] = a[i];
	}
	Params params() const {
		Params p;
		p.model = model, p.timbre = timbre, p.bite = bite, p.width = width, p.sub = sub, p.mode = mode;
		p.glide = glide, p.accent = accent, p.ftype = ftype, p.cutoff = cutoff, p.res = res, p.fenv = fenv;
		p.key = key, p.fatk = fatk, p.fdec = fdec, p.drive = drive;
		p.atk = atk, p.dec = dec, p.sus = sus, p.rel = rel;
		p.vdepth = vdepth, p.vrate = vrate, p.vdelay = vdelay;
		p.div = div, p.chorus = chorus, p.echo = echo, p.fdbk = fdbk, p.lofi = lofi, p.verb = verb;
		p.outGain = gain;
		return p;
	}
};

// clang-format off
//  style, name,              model,  timbre bite width sub  mode,      glide acc  filter,   cut  res  fenv key  fatk fdec drive  atk  dec  sus  rel   vdep vrate vdel  echo,   chor echo fdbk lofi verb
static const Preset PRESETS[] = {
	{"SOUL · JAZZ · LO-FI", "CLASSIC RHODES", RHODES, .45f, .45f, .50f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER12, .85f, .05f, .10f, .50f, .00f, .45f, .05f, .00f, .70f, .55f, .50f, .00f, .50f, .40f, DIV_8D, .30f, .00f, .30f, .00f, .20f, /*gain*/ 1.138f},
	{"SOUL · JAZZ · LO-FI", "NEO SOUL KEYS",  RHODES, .60f, .35f, .60f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER12, .75f, .10f, .15f, .50f, .00f, .50f, .10f, .00f, .70f, .60f, .55f, .00f, .50f, .40f, DIV_8D, .55f, .10f, .30f, .00f, .25f, /*gain*/ 1.052f},
	{"SOUL · JAZZ · LO-FI", "LO-FI KEYS",     RHODES, .30f, .25f, .40f, .00f, MODE_POLY, .30f, .40f, gmono::LADDER24, .60f, .10f, .10f, .40f, .00f, .50f, .15f, .02f, .70f, .50f, .50f, .05f, .30f, .20f, DIV_8,  .35f, .10f, .30f, .55f, .20f, /*gain*/ 1.277f},
	{"SOUL · JAZZ · LO-FI", "JAZZ ORGAN",     ORGAN,  .70f, .60f, .40f, .00f, MODE_POLY, .30f, .40f, gmono::LADDER12, .90f, .00f, .00f, .50f, .00f, .40f, .20f, .00f, .50f, 1.0f, .25f, .25f, .55f, .30f, DIV_8,  .45f, .00f, .30f, .00f, .15f, /*gain*/ 0.944f},
	{"SOUL · JAZZ · LO-FI", "VIBES", MARIMBA, .45f, .30f, .50f, .00f, MODE_POLY, .30f, .50f, gmono::SVF_LP, .90f, .05f, .10f, .50f, .00f, .45f, .00f, .00f, .75f, .00f, .65f, .15f, .40f, .00f, DIV_8D, .20f, .10f, .30f, .00f, .30f, /*gain*/ 0.818f},

	{"HOUSE · GARAGE", "M1 ORGAN",            ORGAN,  .55f, .75f, .40f, .20f, MODE_POLY, .30f, .50f, gmono::LADDER12, .80f, .10f, .20f, .50f, .00f, .35f, .10f, .00f, .55f, .70f, .30f, .00f, .50f, .40f, DIV_8D, .20f, .15f, .30f, .00f, .20f, /*gain*/ 1.143f},
	{"HOUSE · GARAGE", "GARAGE BELL",         BELL,   .35f, .55f, .50f, .00f, MODE_POLY, .30f, .50f, gmono::SVF_LP,   .85f, .05f, .15f, .50f, .00f, .40f, .00f, .00f, .60f, .10f, .50f, .00f, .50f, .40f, DIV_8D, .25f, .20f, .35f, .00f, .25f, /*gain*/ 1.440f},
	{"HOUSE · GARAGE", "DEEP CHORD STAB", SAWLEAD, .40f, .30f, .60f, .00f, MODE_POLY, .30f, .60f, gmono::LADDER24, .62f, .20f, .55f, .40f, .00f, .38f, .30f, .00f, .55f, .00f, .40f, .00f, .50f, .40f, DIV_8D, .40f, .30f, .45f, .00f, .30f, /*gain*/ 1.941f},
	{"HOUSE · GARAGE", "FILTER HOUSE STAB", SAWLEAD, .30f, .50f, .50f, .15f, MODE_POLY, .30f, .60f, gmono::MS20, .68f, .40f, .50f, .40f, .00f, .30f, .35f, .00f, .40f, .00f, .30f, .00f, .50f, .40f, DIV_8, .20f, .15f, .30f, .00f, .15f, /*gain*/ 2.188f},

	{"DUB · REGGAE", "DUB SKANK", ORGAN, .40f, .65f, .30f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER12, .80f, .15f, .30f, .50f, .00f, .30f, .30f, .00f, .35f, .30f, .25f, .00f, .50f, .40f, DIV_8D, .10f, .45f, .60f, .15f, .35f, /*gain*/ 1.542f},
	{"DUB · REGGAE", "DUB MELODICA",          FLUTE,  .65f, .45f, .30f, .00f, MODE_MONO, .30f, .50f, gmono::LADDER12, .70f, .10f, .10f, .50f, .20f, .50f, .15f, .02f, .50f, .85f, .35f, .20f, .55f, .50f, DIV_8D, .10f, .40f, .55f, .10f, .30f, /*gain*/ 0.832f},

	{"TECHNO · ELECTRO", "ACID LEAD", SQUARE, .30f, .50f, .10f, .00f, MODE_LEGATO, .25f, .80f, gmono::DIODE, .40f, .75f, .60f, .50f, .00f, .30f, .70f, .00f, .40f, .60f, .20f, .00f, .50f, .40f, DIV_8D, .00f, .25f, .40f, .00f, .10f, /*gain*/ 1.995f},
	{"TECHNO · ELECTRO", "HYPNOTIC BLIP", MARIMBA, .60f, .60f, .40f, .40f, MODE_MONO, .30f, .50f, gmono::SVF_LP, .70f, .45f, .35f, .50f, .00f, .25f, .60f, .00f, .30f, .20f, .25f, .00f, .50f, .40f, DIV_8D, .10f, .40f, .55f, .00f, .20f, /*gain*/ 1.697f},
	{"TECHNO · ELECTRO", "DETROIT STRINGS", SAWLEAD, .60f, .20f, .80f, .00f, MODE_POLY, .30f, .40f, gmono::LADDER24, .55f, .10f, .15f, .50f, .50f, .70f, .00f, .62f, .70f, .75f, .68f, .10f, .45f, .50f, DIV_4, .60f, .15f, .40f, .00f, .40f, /*gain*/ 0.828f},
	{"TECHNO · ELECTRO", "DUB TECHNO STAB", SAWLEAD, .30f, .20f, .70f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER24, .60f, .30f, .45f, .40f, .00f, .45f, .45f, .00f, .50f, .10f, .45f, .00f, .50f, .40f, DIV_8D, .55f, .50f, .70f, .00f, .35f, /*gain*/ 2.001f},
	{"TECHNO · ELECTRO", "ELECTRO ZAP", SQUARE, .50f, .60f, .20f, .60f, MODE_MONO, .30f, .60f, gmono::OTA, .72f, .50f, .70f, .50f, .00f, .15f, .60f, .00f, .40f, .50f, .20f, .00f, .50f, .40f, DIV_16, .00f, .20f, .30f, .00f, .10f, /*gain*/ 2.223f},

	{"GRIME · DUBSTEP · DRILL", "GRIME PULSE", SQUARE, .20f, .30f, .20f, .60f, MODE_MONO, .20f, .50f, gmono::LADDER24, .65f, .10f, .20f, .40f, .00f, .30f, .45f, .00f, .35f, .40f, .20f, .00f, .50f, .40f, DIV_8, .00f, .10f, .30f, .00f, .10f, /*gain*/ 2.040f},
	{"GRIME · DUBSTEP · DRILL", "DRILL BELL", BELL,   .55f, .50f, .50f, .00f, MODE_POLY, .30f, .50f, gmono::SVF_LP,   .80f, .05f, .10f, .50f, .00f, .40f, .00f, .00f, .65f, .05f, .55f, .10f, .55f, .20f, DIV_8D, .20f, .25f, .40f, .10f, .30f, /*gain*/ 1.510f},
	{"GRIME · DUBSTEP · DRILL", "DUBSTEP PLUCK", PLUCK, .50f, .60f, .50f, .25f, MODE_POLY, .30f, .60f, gmono::LADDER24, .75f, .30f, .45f, .50f, .00f, .30f, .45f, .00f, .45f, .00f, .35f, .00f, .50f, .40f, DIV_8D, .20f, .30f, .45f, .00f, .25f, /*gain*/ 1.805f},

	{"POP · SYNTHWAVE · TRANCE", "SUPERSAW", SAWLEAD, .80f, .30f, .90f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER24, .75f, .10f, .15f, .50f, .00f, .50f, .05f, .02f, .60f, .60f, .60f, .00f, .50f, .40f, DIV_8D, .30f, .25f, .45f, .00f, .40f, /*gain*/ 0.808f},
	{"POP · SYNTHWAVE · TRANCE", "80S POLY",  SAWLEAD,.40f, .40f, .70f, .00f, MODE_POLY, .30f, .50f, gmono::LADDER24, .60f, .20f, .35f, .50f, .00f, .45f, .05f, .01f, .60f, .60f, .50f, .05f, .50f, .50f, DIV_8D, .65f, .15f, .35f, .00f, .30f, /*gain*/ 1.012f},
	{"POP · SYNTHWAVE · TRANCE", "KALIMBA",   PLUCK,  .35f, .55f, .40f, .00f, MODE_POLY, .30f, .50f, gmono::SVF_LP,   .80f, .05f, .10f, .50f, .00f, .40f, .00f, .00f, .60f, .00f, .50f, .00f, .50f, .40f, DIV_8D, .10f, .20f, .35f, .00f, .30f, /*gain*/ 1.225f},
	{"POP · SYNTHWAVE · TRANCE", "FLUTE SOLO",FLUTE,  .45f, .50f, .30f, .00f, MODE_LEGATO,.35f,.50f, gmono::LADDER12, .80f, .05f, .05f, .50f, .10f, .50f, .00f, .05f, .50f, .90f, .40f, .30f, .50f, .55f, DIV_8D, .10f, .15f, .35f, .00f, .35f, /*gain*/ 0.779f},

	{"AMBIENT", "AMBIENT BELLS",              BELL,   .70f, .30f, .80f, .00f, MODE_POLY, .30f, .40f, gmono::SVF_LP,   .75f, .05f, .05f, .50f, .00f, .50f, .00f, .01f, .80f, .20f, .75f, .05f, .30f, .50f, DIV_4,  .40f, .35f, .55f, .00f, .60f, /*gain*/ 0.767f},
	{"AMBIENT", "SPACE RHODES", RHODES, .40f, .30f, .80f, .00f, MODE_POLY, .30f, .40f, gmono::LADDER12, .70f, .05f, .10f, .50f, .00f, .50f, .05f, .00f, .75f, .50f, .70f, .00f, .50f, .40f, DIV_4, .50f, .35f, .55f, .10f, .45f, /*gain*/ 0.715f},
	{"AMBIENT", "AIR PAD", FLUTE, .20f, .20f, .50f, .00f, MODE_POLY, .30f, .30f, gmono::LADDER12, .40f, .05f, .00f, .20f, .00f, .50f, .00f, .70f, .70f, .50f, .75f, .10f, .35f, .60f, DIV_4, .30f, .30f, .50f, .00f, .45f, /*gain*/ 0.791f},
};
// clang-format on
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

}  // namespace gfunke
