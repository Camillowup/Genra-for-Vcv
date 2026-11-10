// SPDX-License-Identifier: GPL-3.0-only
// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX – the three tonal voices (GENRA MONO, FUNKE, SCHMELZ) as the box plays them: an engine of each instrument,
// a factory preset and two macros that move a few of the preset's values. Plain C++ without Rack: test/test_box.cpp.
// (2.31: GENRA MONO plays the bass instead of GRUND.)
// (DRUM-X lives in a core library and needs Rack's resampler: src/box/GenraBox.cpp.)
#pragma once
#include "../mono/MonoPresets.hpp"
#include "../GenreMatch.hpp"
#include "../funke/FunkePresets.hpp"
#include "../schmelz/SchmelzPresets.hpp"
#include <cmath>
#include <memory>
#include <string>

namespace boxv {

inline float c01(float x) { return x < 0.f ? 0.f : (x > 1.f ? 1.f : x); }
/** A macro at 0.5 leaves the preset as it is; 0 / 1 move the value by -range/2 / +range/2. */
inline float mac(float base, float m, float range) { return c01(base + (m - 0.5f) * range); }
/** The same, with a smaller way down (a filter closed too far would silence the preset). */
inline float mac2(float base, float m, float down, float up) {
	return c01(base + (m < 0.5f ? (m - 0.5f) * 2.f * down : (m - 0.5f) * 2.f * up));
}

/** Does preset style `style` fit the genre? (the match of src/GenreMatch.hpp: whole names first, then words) */
inline int styleScore(const char* style, const std::string& genreName) { return genrematch::score(style, genreName); }

/** One monophonic note channel as GENRA's outputs give it. */
struct MonoIn {
	float voct = 0.f;
	bool gate = false;
	float vel = 0.8f;
	bool acc = false;
	bool slide = false;
};

// ---------------------------------------------------------------- GENRA MONO: the bass track
struct Bass {
	std::unique_ptr<gmono::Voice> v{new gmono::Voice};
	gmono::Params p;
	int preset = -1;
	float ma = -1.f, mb = -1.f;
	static int count() { return gmono::NUM_PRESETS; }
	static const char* name(int i) { return gmono::PRESETS[i].name; }
	static const char* style(int i) { return gmono::PRESETS[i].style; }
	void setSampleRate(float sr) { v->setSampleRate(sr); }
	/** preset and the macros CUTOFF, RESO (control rate) */
	void set(int pr, float a, float b) {
		if (pr == preset && a == ma && b == mb)
			return;
		preset = pr, ma = a, mb = b;
		p = gmono::PRESETS[pr].params();
		p.cutoff = mac2(p.cutoff, a, std::min(0.15f, 0.35f * p.cutoff), 0.40f); // a dark sub closes less
		p.res = mac2(p.res, b, 0.30f, 0.45f);
		p.width = 0.f; // the box's bass stays mono
	}
	void process(const MonoIn& in, float& L, float& R) {
		gmono::Inputs gi;
		gi.voct = in.voct, gi.gate = in.gate, gi.vel = in.vel, gi.acc = in.acc;
		gi.slide = in.slide, gi.slideConnected = true;
		v->process(p, gi, L, R);
	}
};

// ---------------------------------------------------------------- FUNKE: the melody track
struct Melody {
	std::unique_ptr<gfunke::Engine> e{new gfunke::Engine};
	gfunke::Params p;
	gfunke::Channel ch;
	int preset = -1;
	float ma = -1.f, mb = -1.f;
	static int count() { return gfunke::NUM_PRESETS; }
	static const char* name(int i) { return gfunke::PRESETS[i].name; }
	static const char* style(int i) { return gfunke::PRESETS[i].style; }
	void setSampleRate(float sr) { e->setSampleRate(sr); }
	/** preset and the macros BRIGHT, SPACE */
	void set(int pr, float a, float b) {
		if (pr == preset && a == ma && b == mb)
			return;
		preset = pr, ma = a, mb = b;
		p = gfunke::PRESETS[pr].params();
		p.cutoff = mac2(p.cutoff, a, 0.08f, 0.35f);
		p.timbre = mac2(p.timbre, a, 0.1f, 0.2f);
		p.verb = mac2(p.verb, b, 0.25f, 0.45f);
		p.echo = mac2(p.echo, b, 0.2f, 0.3f);
	}
	void process(const MonoIn& in, float bpm, float& L, float& R) {
		ch.voct = in.voct, ch.gate = in.gate, ch.vel = in.vel, ch.acc = in.acc, ch.slide = in.slide;
		float env;
		float echoS = 60.f / (bpm > 20.f ? bpm : 120.f) * gfunke::DIV_BEATS[p.div < 0 ? 0 : (p.div >= gfunke::NUM_DIVS ? gfunke::NUM_DIVS - 1 : p.div)];
		e->process(p, &ch, 1, true, echoS, L, R, env);
	}
};

// ---------------------------------------------------------------- SCHMELZ: the pads track (4 voices)
struct Pads {
	static const int N = 4;
	std::unique_ptr<gschmelz::StringsVoice[]> sv{new gschmelz::StringsVoice[N]};
	std::unique_ptr<gschmelz::SoloVoice[]> so{new gschmelz::SoloVoice[N]};
	std::unique_ptr<gschmelz::Bus> bus{new gschmelz::Bus};
	gschmelz::StringsParams sp;
	gschmelz::SoloParams op;
	gschmelz::BusParams bp;
	int preset = -1, split = 0;
	bool hold = false;
	float reg = 0.f, ma = -1.f, mb = -1.f;
	bool gate[N] = {}, strSide[N] = {}, soloSide[N] = {}, held[N] = {};
	float lastV[N] = {1e9f, 1e9f, 1e9f, 1e9f}, hz[N] = {};

	Pads() {
		for (int c = 0; c < N; c++)
			sv[c].seed(0x1234u * (c + 1)), so[c].seed(0x777u * (c + 3));
	}
	static int count() { return gschmelz::NUM_PRESETS; }
	static const char* name(int i) { return gschmelz::PRESETS[i].name; }
	static const char* style(int i) { return gschmelz::PRESETS[i].style; }
	void setSampleRate(float sr) {
		bus->setSampleRate(sr);
		for (int c = 0; c < N; c++)
			sv[c].setSampleRate(sr), so[c].setSampleRate(sr);
	}
	/** preset and the macros MOTION, SPACE */
	void set(int pr, float a, float b) {
		if (pr == preset && a == ma && b == mb)
			return;
		preset = pr, ma = a, mb = b;
		const gschmelz::Preset& P = gschmelz::PRESETS[pr];
		P.apply(sp, op, bp);
		split = P.split;
		hold = P.hold != 0;
		reg = P.reg;
		bp.anim = mac(bp.anim, a, 0.9f);
		bp.phaser = mac(bp.phaser, a, 0.5f);
		bp.ensDepth = mac(bp.ensDepth, a, 0.4f);
		bp.verb = mac(bp.verb, b, 0.9f);
		bp.size = mac(bp.size, b, 0.5f);
	}
	void process(const float* voct, const bool* gates, float vel, float& L, float& R) {
		float anim = bus->animate(bp);
		float str = 0.f, solo = 0.f;
		for (int c = 0; c < N; c++) {
			bool was = gate[c];
			gate[c] = gates[c];
			bool rise = gate[c] && !was;
			if (rise) {
				strSide[c] = gschmelz::stringsTakes(split, voct[c]);
				soloSide[c] = gschmelz::soloTakes(split, voct[c]);
			}
			if (voct[c] != lastV[c]) {
				lastV[c] = voct[c];
				hz[c] = gschmelz::C4 * std::pow(2.f, voct[c]);
			}
			gschmelz::StringsParams q = sp;
			q.reg = c01(reg + anim);
			if (sv[c].envStep(q, gate[c] && strSide[c], rise && strSide[c], vel))
				str += sv[c].render(q, hz[c]);
			bool trig = rise && soloSide[c];
			if (trig)
				held[c] = true;
			if (!hold)
				held[c] = false;
			bool g = (gate[c] && soloSide[c]) || (hold && held[c]);
			if (so[c].envStep(op, g, trig, vel))
				solo += so[c].render(op, hz[c]);
		}
		float st, s2;
		bus->process(bp, str, solo, L, R, st, s2);
	}
};

} // namespace boxv
