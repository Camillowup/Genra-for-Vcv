// SPDX-License-Identifier: GPL-3.0-only
// SCHMELZ – the factory presets, by style. Plain C++ without Rack: test/test_schmelz.cpp plays every one.
// REGISTRATION: BASS 0 · CELLO .14 · VIOLA .29 · VIOLIN .43 · BRASS .57 · ORGAN .71 · CHOIR .86 · SYNTH 1 (between: a
// blend). SOUND: BASS 0 · E-PIANO .25 · CLAVI .5 · SYNTH .75 · BELL 1. BALANCE: 0 strings only, .5 both, 1 solo only.
// Every knob and switch is set.
#pragma once
#include "SchmelzDsp.hpp"

namespace gschmelz {

/** The knobs in SCHMELZ's ParamId order. */
static const int NUM_PRESET_PARAMS = 24;

struct Preset {
	const char* style;
	const char* name;
	// strings
	float reg, cresc, rel, tone, ensDepth;
	int oct, ens;
	// solo
	float sound, decay, sustain, trem;
	int split;
	float atk, tremRate, vel;
	int hold;
	// bus
	float bal, anim, animRate, phaser, phaserRate, verb, size, volume;
	float gain;  // the output level that makes this preset loud and even with the others

	void values(float* v) const {
		const float a[NUM_PRESET_PARAMS] = {reg,      cresc,      rel,      tone,   ensDepth, (float) oct, (float) ens,
		                                    sound,    decay,      sustain,  trem,   (float) split, atk,   tremRate,
		                                    vel,      (float) hold, bal,    anim,   animRate, phaser,      phaserRate,
		                                    verb,     size,       volume};
		for (int i = 0; i < NUM_PRESET_PARAMS; i++)
			v[i] = a[i];
	}
	/** As Schmelz::process maps the knobs (no CV). */
	void apply(StringsParams& sp, SoloParams& op, BusParams& bp) const {
		sp.reg = reg, sp.oct = oct, sp.cresc = cresc, sp.rel = rel, sp.tone = tone;
		op.sound = sound, op.atk = atk, op.dec = decay, op.sus = sustain, op.velSens = vel;
		bp.ens = ens, bp.ensDepth = ensDepth, bp.anim = anim, bp.animRate = animRate, bp.phaser = phaser;
		bp.phaserRate = phaserRate, bp.verb = verb, bp.size = size, bp.trem = trem, bp.tremRate = tremRate;
		bp.bal = bal, bp.vol = volume;
		bp.outGain = gain;
	}
};

// clang-format off
//  style, name,                     reg   cresc rel   tone  ensD  oct,      ens,        sound decay sust  trem  split,       atk   trate vel   hold bal   anim  arate phas  prate verb  size  vol
static const Preset PRESETS[] = {
	{"STRING MACHINE CLASSICS", "SOLINA DREAM", .40f, .50f, .62f, .55f, .80f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .30f, .65f, 0.48f, /*gain*/ 1.517f},
	{"STRING MACHINE CLASSICS", "CRUMAR STRINGS", .32f, .45f, .55f, .60f, .65f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .25f, .25f, .25f, .60f, 0.91f, /*gain*/ 1.517f},
	{"STRING MACHINE CLASSICS", "ANALOG CELLO", .14f, .55f, .55f, .45f, .50f, OCT_BASE, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.69f, /*gain*/ 1.508f},
	{"STRING MACHINE CLASSICS", "STRING + BRASS", .47f, .40f, .50f, .75f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.93f, /*gain*/ 1.525f},
	{"STRING MACHINE CLASSICS", "PHASED STRINGS", .43f, .55f, .65f, .55f, .80f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .65f, .20f, .35f, .70f, 0.58f, /*gain*/ 1.496f},

	{"HOUSE · GARAGE · DISCO", "DISCO STRINGS", .43f, .30f, .45f, .70f, .60f, OCT_8VA, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .30f, .60f, 0.77f, /*gain*/ 1.505f},
	{"HOUSE · GARAGE · DISCO", "DEEP HOUSE PAD", .78f, .60f, .65f, .40f, .70f, OCT_BASE, ENS_BOTH, .25f, .60f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .20f, .00f, .35f, .00f, .30f, .35f, .70f, 0.43f, /*gain*/ 1.505f},
	{"HOUSE · GARAGE · DISCO", "GARAGE CHORDS", .71f, .20f, .40f, .55f, .50f, OCT_BASE, ENS_CHORUS, .25f, .45f, .30f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, .25f, .55f, 0.48f, /*gain*/ 1.505f},
	{"HOUSE · GARAGE · DISCO", "PIANO HOUSE STAB", .71f, .10f, .30f, .60f, .40f, OCT_BASE, ENS_CHORUS, .25f, .50f, .20f, .00f, SPLIT_LAYER, .00f, .40f, .60f, 0, .75f, .00f, .35f, .00f, .30f, .20f, .55f, 0.57f, /*gain*/ 1.517f},

	{"AMBIENT · LO-FI · TRIP-HOP", "AMBIENT CLOUD", .86f, .85f, .82f, .40f, .90f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .45f, .15f, .00f, .30f, .70f, .90f, 0.63f, /*gain*/ 1.505f},
	{"AMBIENT · LO-FI · TRIP-HOP", "LO-FI TAPE PAD", .35f, .60f, .60f, .30f, .90f, OCT_BASE, ENS_ENS, .25f, .60f, .40f, .30f, SPLIT_LAYER, .00f, .25f, .30f, 0, .15f, .20f, .10f, .00f, .30f, .30f, .60f, 0.60f, /*gain*/ 1.505f},
	{"AMBIENT · LO-FI · TRIP-HOP", "TRIP-HOP NOIR", .20f, .65f, .60f, .35f, .60f, OCT_BASE, ENS_ENS, 1.0f, .65f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .30f, .00f, .35f, .20f, .20f, .45f, .75f, 0.52f, /*gain*/ 1.517f},
	{"AMBIENT · LO-FI · TRIP-HOP", "EP + STRINGS", .35f, .50f, .55f, .50f, .70f, OCT_BASE, ENS_ENS, .25f, .60f, .40f, .30f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, .30f, .60f, 0.53f, /*gain*/ 1.508f},
	{"AMBIENT · LO-FI · TRIP-HOP", "GLASS CHOIR", .86f, .50f, .70f, .60f, .70f, OCT_8VA, ENS_BOTH, 1.0f, .70f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .45f, .00f, .35f, .00f, .30f, .55f, .80f, 0.23f, /*gain*/ 1.537f},

	{"TECHNO · DUB TECHNO", "DUB TECHNO CHORD", 1.0f, .15f, .45f, .30f, .60f, OCT_BASE, ENS_CHORUS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .20f, .30f, .55f, .80f, 0.60f, /*gain*/ 1.508f},
	{"TECHNO · DUB TECHNO", "DETROIT PAD", .93f, .55f, .60f, .55f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .25f, .25f, .00f, .30f, .40f, .70f, 0.96f, /*gain*/ 1.517f},
	{"TECHNO · DUB TECHNO", "DARK DRONE", .07f, .90f, .85f, .25f, .80f, OCT_BASE, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .35f, .05f, .35f, .10f, .60f, .90f, 0.94f, /*gain*/ 1.508f},
	{"TECHNO · DUB TECHNO", "HYPNOTIC SWEEP", .90f, .40f, .50f, .50f, .60f, OCT_BASE, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .70f, .30f, .50f, .45f, .35f, .65f, 0.98f, /*gain*/ 1.517f},

	{"DUB · REGGAE", "DUB ORGAN PAD", .71f, .25f, .50f, .45f, .40f, OCT_BASE, ENS_CHORUS, .50f, .40f, .30f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .20f, .00f, .35f, .00f, .30f, .50f, .75f, 0.25f, /*gain*/ 1.505f},

	{"GRIME · DRILL · DUBSTEP", "GRIME STRINGS", .43f, .05f, .30f, .65f, .30f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .00f, .00f, .35f, .00f, .30f, .15f, .50f, 0.57f, /*gain*/ 1.514f},
	{"GRIME · DRILL · DUBSTEP", "DRILL CHOIR", .86f, .45f, .60f, .45f, .70f, OCT_BASE, ENS_BOTH, 1.0f, .60f, .00f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .35f, .00f, .35f, .00f, .30f, .45f, .80f, 0.34f, /*gain*/ 1.483f},

	{"POP · SYNTHWAVE · TRANCE", "SYNTHWAVE PAD", 1.0f, .50f, .65f, .60f, .80f, OCT_BOTH, ENS_BOTH, .75f, .60f, .50f, .00f, SPLIT_LAYER, .02f, .40f, .30f, 0, .20f, .00f, .35f, .00f, .30f, .45f, .75f, 0.85f, /*gain*/ 1.516f},
	{"POP · SYNTHWAVE · TRANCE", "TRANCE SWELL", .95f, .80f, .55f, .70f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .30f, .50f, .30f, .60f, .50f, .80f, 0.49f, /*gain*/ 1.505f},
	{"POP · SYNTHWAVE · TRANCE", "CINEMATIC", .20f, .78f, .80f, .45f, .70f, OCT_BOTH, ENS_ENS, 1.0f, .75f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .25f, .00f, .35f, .00f, .30f, .65f, .90f, 0.25f, /*gain*/ 1.528f},
	{"POP · SYNTHWAVE · TRANCE", "ANALOG BRASS", .57f, .20f, .40f, .85f, .6f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.81f, /*gain*/ 1.508f},
	// ---- GENRE PRESETS (2.31, tools/genre_presets.py): one per genre that had none; do not edit by hand
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "2-STEP ORGAN", .71f, 0.15f, .40f, .55f, .50f, OCT_BASE, ENS_CHORUS, .25f, .45f, .30f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, .25f, .55f, 0.48f, /*gain*/ 1.505f},
	{"MINIMAL · ACID", "MINIMAL CHORD", 1.0f, .15f, .45f, .30f, .60f, OCT_BASE, ENS_CHORUS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .20f, .30f, 0.40f, 0.60f, 0.60f, /*gain*/ 2.387f},
	{"MINIMAL · ACID", "ACID SWEEP PAD", .90f, .40f, .50f, .50f, .60f, OCT_BASE, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, 0.60f, .30f, 0.60f, .45f, .35f, .65f, 0.98f, /*gain*/ 1.540f},
	{"DARKWAVE · ELECTRO", "ELECTRO PAD", 1.0f, 0.30f, .65f, .60f, .80f, OCT_BOTH, ENS_BOTH, .75f, .60f, .50f, .00f, SPLIT_LAYER, .02f, .40f, .30f, 0, .20f, .00f, .35f, .00f, .30f, 0.30f, .75f, 0.85f, /*gain*/ 1.992f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKBEAT STRINGS", .47f, 0.20f, .50f, .75f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.93f, /*gain*/ 1.517f},
	{"DRUM & BASS · JUNGLE", "JUNGLE STRINGS", .43f, 0.45f, .45f, .70f, .60f, OCT_8VA, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, 0.50f, .60f, 0.77f, /*gain*/ 1.126f},
	{"DRUM & BASS · JUNGLE", "LIQUID PAD", .86f, 0.60f, .82f, .40f, .90f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .45f, .15f, .00f, .30f, 0.55f, .90f, 0.63f, /*gain*/ 1.090f},
	{"HIP-HOP · TRAP · PHONK", "BOOM BAP STRINGS", .35f, .50f, .55f, .50f, .70f, OCT_BASE, ENS_ENS, .25f, .60f, .40f, .30f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, 0.25f, .60f, 0.53f, /*gain*/ 1.595f},
	{"HIP-HOP · TRAP · PHONK", "TRAP CHOIR", .86f, .45f, .60f, .45f, .70f, OCT_BASE, ENS_BOTH, 1.0f, .60f, .00f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .35f, .00f, .35f, .00f, .30f, 0.50f, .80f, 0.34f, /*gain*/ 1.354f},
	{"DOWNTEMPO", "DOWNTEMPO PAD", .35f, .60f, .60f, .30f, .90f, OCT_BASE, ENS_ENS, .25f, .60f, .40f, .30f, SPLIT_LAYER, .00f, .25f, .30f, 0, .15f, .20f, .10f, .00f, .30f, 0.40f, .60f, 0.60f, /*gain*/ 1.344f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEAT HORNS", .57f, 0.10f, .40f, .85f, .6f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.81f, /*gain*/ 1.508f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AFROBEATS PAD", 1.0f, .50f, .65f, .60f, .80f, OCT_BOTH, ENS_BOTH, .75f, .60f, .50f, .00f, SPLIT_LAYER, .02f, .40f, .30f, 0, .20f, .00f, .35f, .00f, .30f, 0.35f, .75f, 0.85f, /*gain*/ 1.814f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "AMAPIANO PIANO", .71f, .10f, .30f, .60f, .40f, OCT_BASE, ENS_CHORUS, 0.25f, .50f, .20f, .00f, SPLIT_LAYER, .00f, .40f, .60f, 0, .75f, .00f, .35f, .00f, .30f, 0.35f, .55f, 0.57f, /*gain*/ 1.622f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BAILE BRASS STAB", .57f, 0.10f, 0.30f, .85f, .6f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.81f, /*gain*/ 1.508f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "REGGAETON PAD", 1.0f, 0.35f, .65f, .60f, .80f, OCT_BOTH, ENS_BOTH, .75f, .60f, .50f, .00f, SPLIT_LAYER, .02f, .40f, .30f, 0, .20f, .00f, .35f, .00f, .30f, .45f, .75f, 0.85f, /*gain*/ 1.508f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "FOOTWORK VOX PAD", .86f, .50f, .70f, .60f, .70f, OCT_8VA, ENS_BOTH, 1.0f, .70f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .45f, .00f, .35f, .00f, .30f, 0.40f, .80f, 0.23f, /*gain*/ 1.854f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKCORE DREAD", .07f, 0.50f, 0.60f, .25f, .80f, OCT_BASE, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .35f, .05f, .35f, .10f, .60f, .90f, 0.94f, /*gain*/ 0.805f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "UK FUNKY CHORDS", .71f, .20f, .40f, .55f, .50f, OCT_BASE, ENS_CHORUS, .25f, .45f, .30f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, 0.35f, .55f, 0.48f, /*gain*/ 1.344f},
	{"2-STEP · UK FUNKY · BASSLINE · FOOTWORK", "BASSLINE STAB", .71f, .10f, 0.25f, .60f, .40f, OCT_BASE, ENS_CHORUS, .25f, .50f, .20f, .00f, SPLIT_LAYER, .00f, .40f, .60f, 0, .75f, .00f, .35f, .00f, .30f, .20f, .55f, 0.57f, /*gain*/ 1.517f},
	{"AFROBEAT · AFROBEATS · AMAPIANO · GQOM", "GQOM DREAD", .07f, 0.50f, 0.60f, .25f, .80f, OCT_BASE, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .35f, .05f, 0.20f, .10f, .60f, .90f, 0.94f, /*gain*/ 0.772f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BIG BEAT BRASS", .47f, 0.15f, .50f, 0.85f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.93f, /*gain*/ 1.517f},
	{"BREAKBEAT · BREAKS · BIG BEAT · BREAKCORE", "BREAKS PAD", .95f, 0.50f, .55f, .70f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .30f, .50f, .30f, .60f, .50f, .80f, 0.49f, /*gain*/ 1.400f},
	{"JERSEY CLUB · BALTIMORE CLUB", "JERSEY VOX", .86f, 0.20f, 0.40f, .60f, .70f, OCT_8VA, ENS_BOTH, 1.0f, .70f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .45f, .00f, .35f, .00f, .30f, .55f, .80f, 0.23f, /*gain*/ 1.537f},
	{"JERSEY CLUB · BALTIMORE CLUB", "BALTIMORE STAB", .71f, .10f, 0.20f, .60f, .40f, OCT_BASE, ENS_CHORUS, .25f, .50f, .20f, .00f, SPLIT_LAYER, .00f, .40f, .60f, 0, .75f, .00f, .35f, .00f, .30f, .20f, .55f, 0.57f, /*gain*/ 1.517f},
	{"HIP-HOP · TRAP · PHONK", "PHONK CHOIR", .86f, .45f, .60f, 0.35f, .70f, OCT_BASE, ENS_BOTH, 1.0f, .60f, .00f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .35f, .00f, .35f, .00f, .30f, .45f, .80f, 0.34f, /*gain*/ 1.483f},
	{"HARDCORE · EBM · INDUSTRIAL", "HARDCORE SWELL", .95f, 0.40f, .55f, 0.80f, .70f, OCT_BOTH, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .30f, .50f, .30f, .60f, .50f, .80f, 0.49f, /*gain*/ 1.400f},
	{"PSYTRANCE", "PSY SWEEP", .90f, .40f, .50f, .50f, .60f, OCT_BASE, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .70f, 0.55f, .50f, .45f, .35f, .65f, 0.98f, /*gain*/ 0.865f},
	{"HARDCORE · EBM · INDUSTRIAL", "EBM BRASS", .57f, 0.05f, 0.25f, .85f, .6f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .50f, 0, .00f, .00f, .35f, .00f, .30f, .25f, .60f, 0.81f, /*gain*/ 1.508f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "DANCEHALL ORGAN", .71f, .25f, .50f, .45f, .40f, OCT_BASE, ENS_CHORUS, .50f, .40f, .30f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .20f, .00f, .35f, .00f, .30f, 0.40f, .75f, 0.25f, /*gain*/ 1.781f},
	{"REGGAETON · DANCEHALL · MOOMBAHTON", "MOOMBAH PAD", 1.0f, 0.30f, .65f, .60f, .80f, OCT_BOTH, ENS_BOTH, .75f, .60f, .50f, .00f, SPLIT_LAYER, .02f, .40f, .30f, 0, .20f, .00f, .35f, .00f, .30f, 0.35f, .75f, 0.85f, /*gain*/ 1.805f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "CUMBIA ORGAN", .71f, .25f, .50f, .45f, .40f, OCT_BASE, ENS_CHORUS, .50f, .40f, .30f, 0.30f, SPLIT_LAYER, .00f, .40f, .40f, 0, .20f, .00f, .35f, .00f, .30f, .50f, .75f, 0.25f, /*gain*/ 1.505f},
	{"BAILE FUNK · CUMBIA · BOSSA NOVA", "BOSSA STRINGS", .35f, .50f, .55f, .50f, .70f, OCT_BASE, ENS_ENS, .25f, .60f, .40f, .30f, SPLIT_LAYER, .00f, .40f, .50f, 0, .50f, .00f, .35f, .00f, .30f, 0.40f, .60f, 0.53f, /*gain*/ 1.346f},
	{"DRONE · BERLIN SCHOOL", "DRONE CHOIR", 0.86f, .90f, 0.70f, .25f, .80f, OCT_BASE, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .35f, .05f, .35f, .10f, .60f, .90f, 0.94f, /*gain*/ 1.478f},
	{"DRONE · BERLIN SCHOOL", "BERLIN STRINGS", .40f, .50f, .62f, .55f, .80f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, 0.45f, .30f, .30f, .65f, 0.48f, /*gain*/ 2.064f},
	{"DARKWAVE · ELECTRO", "DARKWAVE STRINGS", .43f, .55f, .65f, 0.45f, .80f, OCT_BOTH, ENS_ENS, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .00f, .35f, .65f, .20f, .35f, .70f, 0.58f, /*gain*/ 1.497f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "DUNGEON CHOIR", 0.86f, .78f, .80f, .45f, .70f, OCT_BOTH, ENS_ENS, 1.0f, .75f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .25f, .00f, .35f, .00f, .30f, .65f, .90f, 0.25f, /*gain*/ 1.056f},
	{"HARDCORE · EBM · INDUSTRIAL", "INDUSTRIAL HUM", .07f, .90f, 0.60f, 0.40f, .80f, OCT_BASE, ENS_BOTH, .25f, .55f, .40f, .00f, SPLIT_LAYER, .00f, .40f, .30f, 0, .00f, .35f, .05f, 0.50f, .10f, .60f, .90f, 0.94f, /*gain*/ 1.571f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "HORROR STRINGS", 0.43f, .78f, .80f, .45f, .70f, OCT_BOTH, ENS_ENS, 1.0f, .75f, .10f, 0.40f, SPLIT_LAYER, .00f, .40f, .40f, 0, .25f, .00f, .35f, .00f, .30f, .65f, .90f, 0.25f, /*gain*/ 1.498f},
	{"WITCH HOUSE · HORROR · DUNGEON SYNTH", "WITCH CHOIR", .86f, .50f, .70f, 0.40f, .70f, OCT_8VA, ENS_BOTH, 1.0f, .70f, .10f, .00f, SPLIT_LAYER, .00f, .40f, .40f, 0, .45f, .00f, .35f, .00f, .30f, 0.60f, .80f, 0.23f, /*gain*/ 1.423f},
	// ---- END GENRE PRESETS
};
// clang-format on
static const int NUM_PRESETS = sizeof(PRESETS) / sizeof(PRESETS[0]);

}  // namespace gschmelz
