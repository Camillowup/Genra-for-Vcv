// SPDX-License-Identifier: GPL-3.0-only
// GRUND – the factory presets, by style. Plain C++ without Rack: test/test_grund.cpp plays every one.
// Knobs 0..1 (times: 1 ms · 10000^x; TONE: 20 Hz · 1000^x); the FX settings are GRUND FX's knobs.
#pragma once
#include "GrundDsp.hpp"

namespace ggrund {

/** The knobs in GRUND's ParamId order. */
static const int NUM_PRESET_PARAMS = 25;

/** GRUND FX: comp (on, thresh, ratio, attack, release), drive (on, type, tone, mix), EQ (hpf, low, mid, high),
 *  chorus (on, rate, depth), reverb (size, damp, pre-delay). EQ, chorus and reverb on (GRUND's knobs say how much),
 *  HI ONLY on. */
inline FxSettings fx(bool cOn, float th, float ra, float ca, float cr, bool dOn, int dt, float dto, float dmx, float hpf,
                     float lo, float mi, float hi, bool chOn, float rate, float depth, float vs, float vd, float vp) {
	FxSettings f;
	f.compOn = cOn, f.thresh = th, f.ratio = ra, f.cAtk = ca, f.cRel = cr;
	f.driveOn = dOn, f.dType = dt, f.dTone = dto, f.dMix = dmx;
	f.eqOn = true, f.hpf = hpf, f.low = lo, f.mid = mi, f.high = hi;
	(void) chOn;  // chorus section on: the CHORUS knob on GRUND sets how much (0 = none), in every preset
	f.choOn = true, f.choRate = rate, f.choDepth = depth;
	f.verbOn = true, f.vSize = vs, f.vDamp = vd, f.vPre = vp;
	f.choHi = f.verbHi = true;
	return f;
}

struct Preset {
	const char* style;
	const char* name;
	int model;
	float timbre, bite, body, sub, mute, glide, accent;
	int mode, amp;
	float tone, res, fenv, fdec, drive, comp, key;
	float atk, dec, sus, rel;
	float chorus, octave, width, verb;
	FxSettings fx;
	float gain;  // the output level that makes this preset loud and even with the others

	void values(float* v) const {
		const float a[NUM_PRESET_PARAMS] = {(float) model, timbre, bite, body, sub, mute, glide, accent, (float) mode,
		                                    (float) amp, tone, res, fenv, fdec, drive, comp, key, atk, dec, sus, rel,
		                                    chorus, octave, width, verb};
		for (int i = 0; i < NUM_PRESET_PARAMS; i++)
			v[i] = a[i];
	}
	Params params() const {
		Params p;
		p.model = model, p.timbre = timbre, p.bite = bite, p.body = body, p.sub = sub, p.mute = mute;
		p.glide = glide, p.accent = accent, p.mode = mode, p.amp = amp;
		p.tone = tone, p.res = res, p.fenv = fenv, p.fdec = fdec, p.drive = drive, p.comp = comp, p.key = key;
		p.atk = atk, p.dec = dec, p.sus = sus, p.rel = rel;
		p.chorus = chorus, p.octave = octave, p.width = width, p.verb = verb;
		p.fx = fx;
		p.outGain = gain;
		return p;
	}
};

// clang-format off
//  style, name,                     model,    timbre bite  body  sub   mute  glide acc   mode         amp      tone  res   fenv  fdec  drive comp  key   atk   dec   sus   rel   chor  oct   width verb  fx
static const Preset PRESETS[] = {
	{"FUNK · SOUL · DISCO", "MOTOWN P-BASS", FINGER, 0.25f, 0.35f, 0.6f, 0.0f, 0.55f, 0.15f, 0.5f, MODE_MONO, FLIPTOP, 0.42f, 0.10f, 0.15f, 0.35f, 0.15f, 0.40f, 0.30f, 0.00f, 0.55f, 0.75f, 0.30f, 0.0f, 0.0f, 0.10f, 0.05f,
	 fx(true, 0.5f, 0.4f, 0.3f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.52f, 0.40f, false, 0.3f, 0.5f, 0.25f, 0.6f, 0.1f), /*gain*/ 1.248f},
	{"FUNK · SOUL · DISCO", "FUNK SLAP", SLAP, 0.65f, 0.75f, 0.4f, 0.0f, 0.25f, 0.10f, 0.8f, MODE_MONO, DI, 0.72f, 0.12f, 0.35f, 0.30f, 0.10f, 0.55f, 0.35f, 0.00f, 0.55f, 0.70f, 0.30f, 0.0f, 0.0f, 0.25f, 0.06f,
	 fx(true, 0.6f, 0.5f, 0.15f, 0.35f, false, D_FET, 0.6f, 0.2f, 0.18f, 0.55f, 0.40f, 0.62f, false, 0.3f, 0.5f, 0.25f, 0.5f, 0.1f), /*gain*/ 1.895f},
	{"FUNK · SOUL · DISCO", "DISCO PICK", PICK, 0.60f, 0.65f, 0.4f, 0.0f, 0.30f, 0.10f, 0.6f, MODE_MONO, DI, 0.66f, 0.12f, 0.30f, 0.30f, 0.15f, 0.50f, 0.35f, 0.00f, 0.50f, 0.70f, 0.28f, 0.0f, 0.35f, 0.20f, 0.06f,
	 fx(true, 0.5f, 0.5f, 0.2f, 0.35f, false, D_TUBE, 0.5f, 0.3f, 0.18f, 0.55f, 0.50f, 0.58f, false, 0.3f, 0.5f, 0.25f, 0.5f, 0.1f), /*gain*/ 1.420f},
	{"FUNK · SOUL · DISCO", "FRETLESS SING", FRETLESS, 0.55f, 0.35f, 0.5f, 0.0f, 0.10f, 0.35f, 0.4f, MODE_LEGATO, DI, 0.62f, 0.15f, 0.25f, 0.45f, 0.10f, 0.40f, 0.35f, 0.02f, 0.60f, 0.85f, 0.45f, 0.55f, 0.0f, 0.45f, 0.15f,
	 fx(true, 0.45f, 0.35f, 0.3f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.55f, 0.55f, true, 0.25f, 0.6f, 0.45f, 0.5f, 0.2f), /*gain*/ 1.309f},

	{"JAZZ · LO-FI · TRIP-HOP", "JAZZ UPRIGHT", UPRIGHT, 0.45f, 0.40f, 0.75f, 0.0f, 0.30f, 0.10f, 0.5f, MODE_MONO, FLIPTOP, 0.48f, 0.08f, 0.15f, 0.35f, 0.10f, 0.30f, 0.30f, 0.00f, 0.55f, 0.60f, 0.35f, 0.0f, 0.0f, 0.15f, 0.12f,
	 fx(true, 0.45f, 0.35f, 0.3f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.12f, 0.52f, 0.48f, 0.45f, false, 0.3f, 0.5f, 0.35f, 0.55f, 0.15f), /*gain*/ 1.136f},
	{"JAZZ · LO-FI · TRIP-HOP", "WALKING BASS", UPRIGHT, 0.60f, 0.60f, 0.65f, 0.0f, 0.20f, 0.05f, 0.5f, MODE_MONO, DI, 0.58f, 0.08f, 0.20f, 0.30f, 0.05f, 0.30f, 0.35f, 0.00f, 0.50f, 0.55f, 0.30f, 0.0f, 0.0f, 0.15f, 0.10f,
	 fx(true, 0.45f, 0.35f, 0.25f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.12f, 0.50f, 0.52f, 0.55f, false, 0.3f, 0.5f, 0.3f, 0.5f, 0.12f), /*gain*/ 1.010f},
	{"JAZZ · LO-FI · TRIP-HOP", "LO-FI FINGER", FINGER, 0.15f, 0.20f, 0.6f, 0.15f, 0.45f, 0.15f, 0.3f, MODE_MONO, FLIPTOP, 0.32f, 0.10f, 0.10f, 0.40f, 0.20f, 0.45f, 0.25f, 0.01f, 0.55f, 0.70f, 0.35f, 0.15f, 0.0f, 0.15f, 0.10f,
	 fx(true, 0.5f, 0.4f, 0.35f, 0.5f, true, D_CRUSH, 0.25f, 0.25f, 0.15f, 0.55f, 0.45f, 0.35f, true, 0.2f, 0.4f, 0.35f, 0.7f, 0.15f), /*gain*/ 1.432f},
	{"JAZZ · LO-FI · TRIP-HOP", "ARCO DARK", ARCO, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.25f, 0.3f, MODE_LEGATO, DI, 0.45f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.30f, 0.70f, 0.85f, 0.55f, 0.0f, 0.0f, 0.25f, 0.30f,
	 fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 1.440f},

	{"DUB · REGGAE", "DUB ROOTS", FINGER, 0.10f, 0.20f, 0.70f, 0.20f, 0.60f, 0.10f, 0.3f, MODE_MONO, FLIPTOP, 0.28f, 0.05f, 0.05f, 0.40f, 0.10f, 0.40f, 0.20f, 0.01f, 0.60f, 0.80f, 0.35f, 0.0f, 0.0f, 0.05f, 0.05f,
	 fx(true, 0.45f, 0.4f, 0.35f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.08f, 0.65f, 0.42f, 0.30f, false, 0.3f, 0.5f, 0.3f, 0.7f, 0.1f), /*gain*/ 1.367f},
	{"DUB · REGGAE", "STEPPERS SUB", SYNTH, 0.00f, 0.05f, 0.30f, 0.60f, 0.00f, 0.10f, 0.3f, MODE_MONO, DI, 0.22f, 0.00f, 0.00f, 0.40f, 0.05f, 0.30f, 0.10f, 0.05f, 0.60f, 0.90f, 0.35f, 0.0f, 0.0f, 0.00f, 0.00f,
	 fx(true, 0.4f, 0.35f, 0.3f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.886f},

	{"HOUSE · GARAGE", "DEEP HOUSE", SYNTH, 0.20f, 0.30f, 0.40f, 0.20f, 0.30f, 0.05f, 0.5f, MODE_MONO, DI, 0.38f, 0.25f, 0.35f, 0.30f, 0.10f, 0.40f, 0.35f, 0.00f, 0.45f, 0.40f, 0.25f, 0.0f, 0.0f, 0.15f, 0.08f,
	 fx(true, 0.5f, 0.4f, 0.25f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.48f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.55f, 0.12f), /*gain*/ 1.832f},
	{"HOUSE · GARAGE", "ORGAN BASS", FM, 0.20f, 0.45f, 0.30f, 0.15f, 0.25f, 0.05f, 0.5f, MODE_MONO, DI, 0.60f, 0.10f, 0.20f, 0.30f, 0.10f, 0.40f, 0.35f, 0.00f, 0.45f, 0.65f, 0.25f, 0.0f, 0.0f, 0.15f, 0.08f,
	 fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.55f, 0.50f, false, 0.3f, 0.5f, 0.3f, 0.5f, 0.1f), /*gain*/ 1.705f},
	{"HOUSE · GARAGE", "2-STEP SUB", B808, 0.15f, 0.25f, 0.30f, 0.0f, 0.35f, 0.10f, 0.4f, MODE_MONO, DI, 0.40f, 0.00f, 0.10f, 0.25f, 0.10f, 0.40f, 0.15f, 0.00f, 0.45f, 0.30f, 0.25f, 0.0f, 0.0f, 0.00f, 0.00f,
	 fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.58f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.869f},

	{"TECHNO", "TECHNO ROLL", SYNTH, 0.10f, 0.40f, 0.30f, 0.10f, 0.45f, 0.00f, 0.5f, MODE_MONO, STACK, 0.42f, 0.30f, 0.45f, 0.25f, 0.35f, 0.55f, 0.35f, 0.00f, 0.38f, 0.20f, 0.22f, 0.0f, 0.0f, 0.10f, 0.05f,
	 fx(true, 0.55f, 0.5f, 0.15f, 0.3f, false, D_FET, 0.5f, 0.3f, 0.18f, 0.55f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.477f},
	{"TECHNO", "RUMBLE", B808, 0.55f, 0.20f, 0.40f, 0.0f, 0.10f, 0.00f, 0.3f, MODE_MONO, FUZZAMP, 0.25f, 0.05f, 0.05f, 0.40f, 0.45f, 0.45f, 0.10f, 0.00f, 0.55f, 0.50f, 0.40f, 0.0f, 0.0f, 0.10f, 0.65f,
	 fx(true, 0.5f, 0.5f, 0.2f, 0.4f, true, D_TUBE, 0.3f, 0.4f, 0.15f, 0.60f, 0.40f, 0.30f, false, 0.3f, 0.5f, 0.75f, 0.85f, 0.05f), /*gain*/ 0.899f},
	{"TECHNO", "HARD RUMBLE", B808, 0.85f, 0.35f, 0.30f, 0.0f, 0.30f, 0.00f, 0.4f, MODE_MONO, FUZZAMP, 0.30f, 0.10f, 0.10f, 0.30f, 0.75f, 0.60f, 0.10f, 0.00f, 0.40f, 0.30f, 0.25f, 0.0f, 0.0f, 0.15f, 0.50f,
	 fx(true, 0.6f, 0.6f, 0.1f, 0.3f, true, D_FUZZ, 0.35f, 0.5f, 0.18f, 0.58f, 0.45f, 0.35f, false, 0.3f, 0.5f, 0.6f, 0.85f, 0.03f), /*gain*/ 1.531f},
	{"TECHNO", "HYPNOTIC PLUCK", SYNTH, 0.35f, 0.45f, 0.20f, 0.0f, 0.55f, 0.00f, 0.4f, MODE_MONO, DI, 0.35f, 0.15f, 0.55f, 0.18f, 0.10f, 0.40f, 0.40f, 0.00f, 0.30f, 0.00f, 0.15f, 0.0f, 0.0f, 0.20f, 0.10f,
	 fx(true, 0.5f, 0.4f, 0.15f, 0.3f, false, D_TUBE, 0.5f, 0.3f, 0.20f, 0.50f, 0.50f, 0.50f, false, 0.3f, 0.5f, 0.3f, 0.5f, 0.1f), /*gain*/ 2.244f},
	{"TECHNO", "OFFBEAT SUB", SYNTH, 0.00f, 0.10f, 0.30f, 0.50f, 0.40f, 0.00f, 0.3f, MODE_MONO, DI, 0.25f, 0.00f, 0.10f, 0.25f, 0.05f, 0.40f, 0.10f, 0.00f, 0.40f, 0.30f, 0.20f, 0.0f, 0.0f, 0.00f, 0.00f,
	 fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 3.238f},
	{"TECHNO", "DUB TECHNO", SYNTH, 0.30f, 0.20f, 0.35f, 0.20f, 0.30f, 0.05f, 0.3f, MODE_MONO, FLIPTOP, 0.30f, 0.20f, 0.20f, 0.40f, 0.10f, 0.40f, 0.25f, 0.01f, 0.50f, 0.50f, 0.35f, 0.25f, 0.0f, 0.30f, 0.30f,
	 fx(true, 0.45f, 0.4f, 0.3f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.45f, 0.35f, true, 0.15f, 0.6f, 0.65f, 0.7f, 0.25f), /*gain*/ 1.501f},
	{"TECHNO", "INDUSTRIAL", FM, 0.70f, 0.80f, 0.20f, 0.10f, 0.40f, 0.00f, 0.6f, MODE_MONO, FUZZAMP, 0.55f, 0.20f, 0.30f, 0.25f, 0.70f, 0.60f, 0.35f, 0.00f, 0.35f, 0.30f, 0.20f, 0.0f, 0.0f, 0.20f, 0.15f,
	 fx(true, 0.6f, 0.6f, 0.1f, 0.3f, true, D_FUZZ, 0.6f, 0.45f, 0.20f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.4f, 0.6f, 0.05f), /*gain*/ 1.758f},

	{"DRUM'N'BASS · JUNGLE", "REESE", REESE, 0.45f, 0.20f, 0.30f, 0.25f, 0.00f, 0.15f, 0.3f, MODE_LEGATO, DI, 0.45f, 0.20f, 0.10f, 0.40f, 0.20f, 0.40f, 0.30f, 0.01f, 0.60f, 0.90f, 0.30f, 0.20f, 0.0f, 0.55f, 0.10f,
	 fx(true, 0.5f, 0.4f, 0.25f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.50f, 0.45f, true, 0.12f, 0.5f, 0.35f, 0.6f, 0.1f), /*gain*/ 1.327f},
	{"DRUM'N'BASS · JUNGLE", "NEURO REESE", REESE, 0.70f, 0.40f, 0.20f, 0.20f, 0.00f, 0.10f, 0.5f, MODE_LEGATO, FUZZAMP, 0.50f, 0.45f, 0.30f, 0.35f, 0.55f, 0.55f, 0.30f, 0.00f, 0.55f, 0.90f, 0.25f, 0.15f, 0.0f, 0.50f, 0.08f,
	 fx(true, 0.55f, 0.55f, 0.15f, 0.35f, true, D_FUZZ, 0.6f, 0.45f, 0.20f, 0.55f, 0.55f, 0.55f, true, 0.2f, 0.5f, 0.35f, 0.6f, 0.05f), /*gain*/ 1.373f},
	{"DRUM'N'BASS · JUNGLE", "JUNGLE SUB", B808, 0.10f, 0.15f, 0.30f, 0.0f, 0.00f, 0.15f, 0.3f, MODE_MONO, DI, 0.35f, 0.00f, 0.05f, 0.40f, 0.10f, 0.35f, 0.10f, 0.00f, 0.70f, 0.80f, 0.45f, 0.0f, 0.0f, 0.00f, 0.00f,
	 fx(true, 0.45f, 0.35f, 0.25f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.023f},

	{"HIP-HOP · TRAP · DRILL", "808 BOOM", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, DI, 0.45f, 0.00f, 0.05f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.85f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f,
	 fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.065f},
	{"HIP-HOP · TRAP · DRILL", "DRILL SLIDE", B808, 0.60f, 0.20f, 0.20f, 0.0f, 0.00f, 0.45f, 0.4f, MODE_LEGATO, FUZZAMP, 0.45f, 0.05f, 0.05f, 0.40f, 0.40f, 0.45f, 0.10f, 0.00f, 0.85f, 0.00f, 0.50f, 0.0f, 0.0f, 0.05f, 0.00f,
	 fx(true, 0.5f, 0.45f, 0.2f, 0.4f, true, D_TUBE, 0.45f, 0.4f, 0.12f, 0.58f, 0.50f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 0.876f},
	{"HIP-HOP · TRAP · DRILL", "BOOM BAP", UPRIGHT, 0.35f, 0.35f, 0.65f, 0.10f, 0.45f, 0.05f, 0.4f, MODE_MONO, FLIPTOP, 0.36f, 0.10f, 0.10f, 0.35f, 0.15f, 0.45f, 0.25f, 0.00f, 0.50f, 0.50f, 0.30f, 0.0f, 0.0f, 0.10f, 0.08f,
	 fx(true, 0.55f, 0.45f, 0.25f, 0.45f, true, D_CRUSH, 0.3f, 0.2f, 0.12f, 0.58f, 0.45f, 0.35f, false, 0.3f, 0.5f, 0.35f, 0.65f, 0.1f), /*gain*/ 1.662f},

	{"DUBSTEP · GRIME", "GRIME SQUARE", SYNTH, 1.00f, 0.30f, 0.20f, 0.20f, 0.45f, 0.05f, 0.4f, MODE_MONO, DI, 0.45f, 0.10f, 0.20f, 0.25f, 0.15f, 0.45f, 0.30f, 0.00f, 0.35f, 0.40f, 0.20f, 0.0f, 0.0f, 0.10f, 0.05f,
	 fx(true, 0.5f, 0.45f, 0.2f, 0.35f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.317f},
	{"DUBSTEP · GRIME", "GROWL", FM, 0.55f, 0.70f, 0.20f, 0.30f, 0.10f, 0.10f, 0.5f, MODE_LEGATO, FUZZAMP, 0.50f, 0.40f, 0.40f, 0.40f, 0.55f, 0.55f, 0.30f, 0.00f, 0.55f, 0.85f, 0.25f, 0.10f, 0.0f, 0.40f, 0.08f,
	 fx(true, 0.55f, 0.55f, 0.15f, 0.35f, true, D_FUZZ, 0.55f, 0.5f, 0.18f, 0.55f, 0.60f, 0.55f, true, 0.4f, 0.5f, 0.35f, 0.6f, 0.05f), /*gain*/ 1.275f},

	{"ROCK · POP", "ROCK PICK", PICK, 0.55f, 0.70f, 0.40f, 0.0f, 0.25f, 0.05f, 0.5f, MODE_MONO, STACK, 0.55f, 0.15f, 0.25f, 0.35f, 0.40f, 0.50f, 0.35f, 0.00f, 0.55f, 0.75f, 0.30f, 0.0f, 0.0f, 0.15f, 0.08f,
	 fx(true, 0.55f, 0.5f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.18f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.35f, 0.5f, 0.1f), /*gain*/ 2.238f},
	{"ROCK · POP", "80S DX BASS", FM, 0.30f, 0.65f, 0.30f, 0.10f, 0.20f, 0.05f, 0.6f, MODE_MONO, DI, 0.65f, 0.10f, 0.25f, 0.25f, 0.10f, 0.45f, 0.40f, 0.00f, 0.45f, 0.55f, 0.25f, 0.30f, 0.0f, 0.35f, 0.15f,
	 fx(true, 0.5f, 0.45f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.50f, 0.60f, true, 0.35f, 0.5f, 0.4f, 0.5f, 0.15f), /*gain*/ 1.812f},
	// ---- GENRE PRESETS (2.31, tools/genre_presets.py): one per genre that had none; do not edit by hand
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "2-STEP SKIP", B808, 0.15f, 0.25f, 0.30f, 0.0f, 0.35f, 0.15f, 0.55f, MODE_MONO, DI, 0.45f, 0.00f, 0.10f, 0.25f, 0.10f, 0.40f, 0.15f, 0.00f, 0.40f, 0.30f, 0.25f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.58f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.818f},
	{"MINIMAL · ACID", "MINIMAL CLICK", SYNTH, 0.35f, 0.45f, 0.20f, 0.0f, 0.55f, 0.00f, 0.4f, MODE_MONO, DI, 0.32f, 0.15f, 0.45f, 0.18f, 0.10f, 0.40f, 0.40f, 0.00f, 0.32f, 0.00f, 0.15f, 0.0f, 0.0f, 0.20f, 0.12f, fx(true, 0.5f, 0.4f, 0.15f, 0.3f, false, D_TUBE, 0.5f, 0.3f, 0.20f, 0.50f, 0.50f, 0.50f, false, 0.3f, 0.5f, 0.3f, 0.5f, 0.1f), /*gain*/ 2.167f},
	{"MINIMAL · ACID", "ACID RUBBER", SYNTH, 0.10f, 0.40f, 0.30f, 0.10f, 0.45f, 0.20f, 0.80f, MODE_LEGATO, STACK, 0.42f, 0.65f, 0.70f, 0.20f, 0.35f, 0.55f, 0.35f, 0.00f, 0.38f, 0.20f, 0.22f, 0.0f, 0.0f, 0.10f, 0.05f, fx(true, 0.55f, 0.5f, 0.15f, 0.3f, false, D_FET, 0.5f, 0.3f, 0.18f, 0.55f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.476f},
	{"SYNTHWAVE · DARKWAVE · ELECTRO", "ELECTRO FUNK", SYNTH, 0.70f, 0.30f, 0.20f, 0.20f, 0.45f, 0.05f, 0.4f, MODE_MONO, DI, 0.55f, 0.10f, 0.50f, 0.25f, 0.15f, 0.45f, 0.30f, 0.00f, 0.30f, 0.20f, 0.20f, 0.0f, 0.0f, 0.10f, 0.05f, fx(true, 0.5f, 0.45f, 0.2f, 0.35f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.478f},
	{"TRANCE · PSYTRANCE", "TRANCE ROLLER", SYNTH, 0.00f, 0.10f, 0.30f, 0.50f, 0.40f, 0.00f, 0.3f, MODE_MONO, DI, 0.32f, 0.00f, 0.30f, 0.25f, 0.15f, 0.40f, 0.10f, 0.00f, 0.25f, 0.30f, 0.20f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.111f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKBEAT PICK", PICK, 0.55f, 0.70f, 0.40f, 0.0f, 0.25f, 0.05f, 0.5f, MODE_MONO, DI, 0.50f, 0.15f, 0.25f, 0.35f, 0.25f, 0.50f, 0.35f, 0.00f, 0.55f, 0.75f, 0.30f, 0.0f, 0.0f, 0.15f, 0.08f, fx(true, 0.55f, 0.5f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.18f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.35f, 0.5f, 0.1f), /*gain*/ 1.575f},
	{"DOWNTEMPO", "DOWNTEMPO ROUND", FINGER, 0.15f, 0.20f, 0.6f, 0.15f, 0.45f, 0.15f, 0.3f, MODE_MONO, FLIPTOP, 0.30f, 0.10f, 0.10f, 0.40f, 0.20f, 0.45f, 0.25f, 0.01f, 0.55f, 0.70f, 0.40f, 0.20f, 0.0f, 0.15f, 0.15f, fx(true, 0.5f, 0.4f, 0.35f, 0.5f, true, D_CRUSH, 0.25f, 0.25f, 0.15f, 0.55f, 0.45f, 0.35f, true, 0.2f, 0.4f, 0.35f, 0.7f, 0.15f), /*gain*/ 1.449f},
	{"SYNTHWAVE · DARKWAVE · ELECTRO", "SYNTHWAVE BASS", SYNTH, 0.50f, 0.65f, 0.30f, 0.10f, 0.20f, 0.05f, 0.6f, MODE_MONO, DI, 0.50f, 0.10f, 0.35f, 0.25f, 0.10f, 0.45f, 0.40f, 0.00f, 0.45f, 0.55f, 0.25f, 0.30f, 0.30f, 0.35f, 0.15f, fx(true, 0.5f, 0.45f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.50f, 0.60f, true, 0.35f, 0.5f, 0.4f, 0.5f, 0.15f), /*gain*/ 1.867f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "AMBIENT SWELL", ARCO, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.25f, 0.3f, MODE_LEGATO, DI, 0.35f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.45f, 0.70f, 0.85f, 0.70f, 0.0f, 0.0f, 0.25f, 0.45f, fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 1.280f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEAT FINGER", FINGER, 0.25f, 0.50f, 0.6f, 0.0f, 0.40f, 0.15f, 0.5f, MODE_MONO, FLIPTOP, 0.50f, 0.10f, 0.15f, 0.35f, 0.15f, 0.40f, 0.30f, 0.00f, 0.50f, 0.75f, 0.30f, 0.0f, 0.0f, 0.10f, 0.05f, fx(true, 0.5f, 0.4f, 0.3f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.52f, 0.40f, false, 0.3f, 0.5f, 0.25f, 0.6f, 0.1f), /*gain*/ 1.270f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEATS SUB", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.15f, 0.4f, MODE_MONO, DI, 0.40f, 0.00f, 0.05f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.55f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.632f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "LOG DRUM", B808, 0.55f, 0.50f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, DI, 0.55f, 0.00f, 0.30f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.35f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.576f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "GQOM BOOM", B808, 0.55f, 0.20f, 0.40f, 0.0f, 0.10f, 0.00f, 0.3f, MODE_MONO, FUZZAMP, 0.30f, 0.05f, 0.05f, 0.40f, 0.50f, 0.45f, 0.10f, 0.00f, 0.50f, 0.50f, 0.40f, 0.0f, 0.0f, 0.10f, 0.65f, fx(true, 0.5f, 0.5f, 0.2f, 0.4f, true, D_TUBE, 0.3f, 0.4f, 0.15f, 0.60f, 0.40f, 0.30f, false, 0.3f, 0.5f, 0.75f, 0.85f, 0.05f), /*gain*/ 0.899f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "DEMBOW 808", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, DI, 0.45f, 0.00f, 0.05f, 0.40f, 0.25f, 0.40f, 0.10f, 0.00f, 0.45f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.682f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "DANCEHALL SUB", SYNTH, 0.00f, 0.05f, 0.30f, 0.60f, 0.00f, 0.10f, 0.3f, MODE_MONO, DI, 0.30f, 0.00f, 0.00f, 0.40f, 0.05f, 0.30f, 0.10f, 0.05f, 0.45f, 0.50f, 0.35f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.4f, 0.35f, 0.3f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.935f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "MOOMBAH GROWL", FM, 0.55f, 0.70f, 0.20f, 0.30f, 0.10f, 0.10f, 0.5f, MODE_LEGATO, FUZZAMP, 0.45f, 0.40f, 0.30f, 0.40f, 0.55f, 0.55f, 0.30f, 0.00f, 0.55f, 0.85f, 0.25f, 0.10f, 0.0f, 0.40f, 0.08f, fx(true, 0.55f, 0.55f, 0.15f, 0.35f, true, D_FUZZ, 0.55f, 0.5f, 0.18f, 0.55f, 0.60f, 0.55f, true, 0.4f, 0.5f, 0.35f, 0.6f, 0.05f), /*gain*/ 1.291f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BAILE 808", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, FUZZAMP, 0.50f, 0.00f, 0.05f, 0.40f, 0.45f, 0.40f, 0.10f, 0.00f, 0.85f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 0.713f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "CUMBIA PICK", PICK, 0.25f, 0.35f, 0.6f, 0.0f, 0.55f, 0.15f, 0.5f, MODE_MONO, FLIPTOP, 0.50f, 0.10f, 0.15f, 0.35f, 0.15f, 0.40f, 0.30f, 0.00f, 0.40f, 0.75f, 0.30f, 0.0f, 0.0f, 0.10f, 0.05f, fx(true, 0.5f, 0.4f, 0.3f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.52f, 0.40f, false, 0.3f, 0.5f, 0.25f, 0.6f, 0.1f), /*gain*/ 1.225f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BOSSA UPRIGHT", UPRIGHT, 0.45f, 0.40f, 0.75f, 0.0f, 0.30f, 0.10f, 0.5f, MODE_MONO, FLIPTOP, 0.45f, 0.08f, 0.15f, 0.35f, 0.10f, 0.30f, 0.30f, 0.00f, 0.65f, 0.60f, 0.35f, 0.0f, 0.0f, 0.15f, 0.12f, fx(true, 0.45f, 0.35f, 0.3f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.12f, 0.52f, 0.48f, 0.45f, false, 0.3f, 0.5f, 0.35f, 0.55f, 0.15f), /*gain*/ 1.134f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "UK FUNKY BOUNCE", SYNTH, 0.20f, 0.30f, 0.40f, 0.20f, 0.30f, 0.05f, 0.5f, MODE_MONO, DI, 0.38f, 0.25f, 0.45f, 0.30f, 0.10f, 0.40f, 0.35f, 0.00f, 0.35f, 0.40f, 0.25f, 0.0f, 0.0f, 0.15f, 0.08f, fx(true, 0.5f, 0.4f, 0.25f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.55f, 0.48f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.55f, 0.12f), /*gain*/ 1.760f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "NICHE WARP", FM, 0.20f, 0.45f, 0.30f, 0.15f, 0.25f, 0.10f, 0.5f, MODE_MONO, DI, 0.55f, 0.25f, 0.45f, 0.30f, 0.10f, 0.40f, 0.35f, 0.00f, 0.45f, 0.65f, 0.25f, 0.0f, 0.0f, 0.15f, 0.08f, fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.55f, 0.50f, false, 0.3f, 0.5f, 0.3f, 0.5f, 0.1f), /*gain*/ 1.684f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "FOOTWORK 808", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, DI, 0.50f, 0.00f, 0.05f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.35f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.863f},
	{"JERSEY CLUB · BALTIMORE CLUB", "JERSEY BOUNCE", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.20f, 0.4f, MODE_MONO, DI, 0.50f, 0.00f, 0.05f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.30f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.787f},
	{"JERSEY CLUB · BALTIMORE CLUB", "BALTIMORE SUB", B808, 0.15f, 0.25f, 0.30f, 0.0f, 0.35f, 0.10f, 0.4f, MODE_MONO, DI, 0.45f, 0.00f, 0.10f, 0.25f, 0.10f, 0.40f, 0.15f, 0.00f, 0.35f, 0.30f, 0.25f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.58f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.928f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKCORE TEAR", REESE, 0.70f, 0.40f, 0.20f, 0.20f, 0.00f, 0.10f, 0.5f, MODE_LEGATO, FUZZAMP, 0.55f, 0.45f, 0.30f, 0.35f, 0.70f, 0.55f, 0.30f, 0.00f, 0.55f, 0.90f, 0.25f, 0.15f, 0.0f, 0.50f, 0.08f, fx(true, 0.55f, 0.55f, 0.15f, 0.35f, true, D_FUZZ, 0.6f, 0.45f, 0.20f, 0.55f, 0.55f, 0.55f, true, 0.2f, 0.5f, 0.35f, 0.6f, 0.05f), /*gain*/ 1.348f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BIG BEAT FUZZ", PICK, 0.55f, 0.70f, 0.40f, 0.0f, 0.25f, 0.05f, 0.5f, MODE_MONO, FUZZAMP, 0.55f, 0.15f, 0.25f, 0.35f, 0.55f, 0.50f, 0.35f, 0.00f, 0.55f, 0.75f, 0.30f, 0.0f, 0.0f, 0.15f, 0.08f, fx(true, 0.55f, 0.5f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.18f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.35f, 0.5f, 0.1f), /*gain*/ 1.431f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKS ROLLER", SYNTH, 0.10f, 0.40f, 0.30f, 0.10f, 0.45f, 0.00f, 0.5f, MODE_MONO, STACK, 0.42f, 0.45f, 0.55f, 0.25f, 0.35f, 0.55f, 0.35f, 0.00f, 0.38f, 0.20f, 0.22f, 0.0f, 0.0f, 0.10f, 0.05f, fx(true, 0.55f, 0.5f, 0.15f, 0.3f, false, D_FET, 0.5f, 0.3f, 0.18f, 0.55f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 2.449f},
	{"PHONK", "PHONK 808", B808, 0.60f, 0.20f, 0.20f, 0.0f, 0.00f, 0.45f, 0.4f, MODE_LEGATO, FUZZAMP, 0.50f, 0.05f, 0.05f, 0.40f, 0.60f, 0.45f, 0.10f, 0.00f, 0.85f, 0.00f, 0.50f, 0.0f, 0.0f, 0.05f, 0.00f, fx(true, 0.5f, 0.45f, 0.2f, 0.4f, true, D_TUBE, 0.45f, 0.4f, 0.12f, 0.58f, 0.50f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 0.860f},
	{"HARDCORE · EBM · INDUSTRIAL", "HARDCORE KICKBASS", B808, 0.85f, 0.35f, 0.30f, 0.0f, 0.30f, 0.00f, 0.4f, MODE_MONO, FUZZAMP, 0.40f, 0.10f, 0.10f, 0.30f, 0.75f, 0.60f, 0.10f, 0.00f, 0.25f, 0.30f, 0.25f, 0.0f, 0.0f, 0.15f, 0.50f, fx(true, 0.6f, 0.6f, 0.1f, 0.3f, true, D_FUZZ, 0.35f, 0.5f, 0.18f, 0.58f, 0.45f, 0.35f, false, 0.3f, 0.5f, 0.6f, 0.85f, 0.03f), /*gain*/ 1.551f},
	{"TRANCE · PSYTRANCE", "PSY ROLL", SYNTH, 0.00f, 0.10f, 0.30f, 0.50f, 0.40f, 0.00f, 0.3f, MODE_MONO, DI, 0.35f, 0.20f, 0.45f, 0.25f, 0.05f, 0.40f, 0.10f, 0.00f, 0.18f, 0.30f, 0.20f, 0.0f, 0.0f, 0.00f, 0.00f, fx(true, 0.5f, 0.4f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.60f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.651f},
	{"HARDCORE · EBM · INDUSTRIAL", "EBM SEQUENCE", FM, 0.70f, 0.80f, 0.20f, 0.10f, 0.40f, 0.00f, 0.6f, MODE_MONO, FUZZAMP, 0.45f, 0.20f, 0.30f, 0.25f, 0.45f, 0.60f, 0.35f, 0.00f, 0.25f, 0.30f, 0.20f, 0.0f, 0.0f, 0.20f, 0.15f, fx(true, 0.6f, 0.6f, 0.1f, 0.3f, true, D_FUZZ, 0.6f, 0.45f, 0.20f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.4f, 0.6f, 0.05f), /*gain*/ 1.803f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "DRONE BOW", ARCO, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.25f, 0.3f, MODE_LEGATO, DI, 0.30f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.50f, 0.70f, 0.90f, 0.70f, 0.0f, 0.0f, 0.25f, 0.30f, fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 1.355f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "DARK SUB DRONE", SYNTH, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.25f, 0.3f, MODE_LEGATO, DI, 0.22f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.40f, 0.70f, 0.85f, 0.70f, 0.0f, 0.0f, 0.25f, 0.50f, fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 0.879f},
	{"AMBIENT · DRONE · DARK AMBIENT · BERLIN SCHOOL", "BERLIN SEQ", SYNTH, 0.30f, 0.65f, 0.30f, 0.10f, 0.20f, 0.05f, 0.6f, MODE_MONO, DI, 0.45f, 0.30f, 0.50f, 0.25f, 0.10f, 0.45f, 0.40f, 0.00f, 0.30f, 0.55f, 0.25f, 0.30f, 0.0f, 0.35f, 0.15f, fx(true, 0.5f, 0.45f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.15f, 0.52f, 0.50f, 0.60f, true, 0.35f, 0.5f, 0.4f, 0.5f, 0.15f), /*gain*/ 2.000f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "WITCH 808 DRAG", B808, 0.25f, 0.30f, 0.30f, 0.0f, 0.00f, 0.10f, 0.4f, MODE_MONO, DI, 0.30f, 0.00f, 0.05f, 0.40f, 0.20f, 0.40f, 0.10f, 0.00f, 0.90f, 0.00f, 0.55f, 0.0f, 0.0f, 0.00f, 0.35f, fx(true, 0.5f, 0.4f, 0.2f, 0.45f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.60f, 0.45f, 0.45f, false, 0.3f, 0.5f, 0.3f, 0.6f, 0.1f), /*gain*/ 1.129f},
	{"SYNTHWAVE · DARKWAVE · ELECTRO", "DARKWAVE PICK", PICK, 0.55f, 0.70f, 0.40f, 0.0f, 0.25f, 0.05f, 0.5f, MODE_MONO, STACK, 0.50f, 0.15f, 0.25f, 0.35f, 0.40f, 0.50f, 0.35f, 0.00f, 0.55f, 0.75f, 0.30f, 0.40f, 0.0f, 0.30f, 0.08f, fx(true, 0.55f, 0.5f, 0.2f, 0.4f, false, D_TUBE, 0.5f, 0.3f, 0.18f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.35f, 0.5f, 0.1f), /*gain*/ 2.295f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "DUNGEON CELLO", ARCO, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.25f, 0.3f, MODE_LEGATO, DI, 0.40f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.30f, 0.70f, 0.85f, 0.55f, 0.0f, 0.0f, 0.25f, 0.40f, fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 1.440f},
	{"HARDCORE · EBM · INDUSTRIAL", "INDUSTRIAL GRIND", FM, 0.70f, 0.80f, 0.20f, 0.10f, 0.40f, 0.00f, 0.6f, MODE_MONO, FUZZAMP, 0.50f, 0.20f, 0.30f, 0.25f, 0.80f, 0.60f, 0.35f, 0.00f, 0.35f, 0.30f, 0.20f, 0.0f, 0.0f, 0.20f, 0.15f, fx(true, 0.6f, 0.6f, 0.1f, 0.3f, true, D_FUZZ, 0.6f, 0.45f, 0.20f, 0.55f, 0.55f, 0.55f, false, 0.3f, 0.5f, 0.4f, 0.6f, 0.05f), /*gain*/ 1.737f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "HORROR BOW", ARCO, 0.35f, 0.40f, 0.70f, 0.0f, 0.0f, 0.40f, 0.3f, MODE_LEGATO, DI, 0.25f, 0.10f, 0.05f, 0.50f, 0.05f, 0.25f, 0.30f, 0.30f, 0.70f, 0.85f, 0.55f, 0.0f, 0.0f, 0.25f, 0.45f, fx(true, 0.4f, 0.3f, 0.4f, 0.5f, false, D_TUBE, 0.5f, 0.3f, 0.10f, 0.55f, 0.45f, 0.40f, false, 0.3f, 0.5f, 0.6f, 0.65f, 0.3f), /*gain*/ 1.732f},
	// ---- END GENRE PRESETS
};
// clang-format on
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

}  // namespace ggrund
