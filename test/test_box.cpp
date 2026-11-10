// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX voices (src/box/BoxVoices.hpp): every preset of GENRA MONO, FUNKE and SCHMELZ with the macros at 0, 0.5 and 1
// plays a finite, audible phrase that stays below 12 V, and the macros change the sound.
// g++ -std=gnu++17 -O2 test/test_box.cpp -o tb && ./tb
#include "../src/box/BoxVoices.hpp"
#include <cstdio>
#include <vector>

using namespace boxv;

static int fails = 0;
static void check(bool ok, const char* what, const char* name, float a, float b, float v) {
	if (!ok) {
		fails++;
		std::printf("FAIL %s  %s  macros %.3f/%.3f  (%g)\n", what, name, a, b, v);
	}
}

struct Res {
	bool fin = true;
	float peak = 0.f, rms = 0.f, tail = 0.f;
};

template <class F>
static Res run(float sr, float secs, float on, F fn) {
	Res r;
	int n = (int) (secs * sr), g = (int) (on * sr);
	double acc = 0.0, tacc = 0.0;
	int tn = 0;
	for (int i = 0; i < n; i++) {
		float L = 0.f, R = 0.f;
		fn(i, i < g, L, R);
		r.fin &= std::isfinite(L) && std::isfinite(R);
		r.peak = std::max(r.peak, std::max(std::fabs(L), std::fabs(R)));
		acc += 0.5 * (L * L + R * R);
		if (i >= n - (int) (0.1f * sr))
			tacc += 0.5 * (L * L + R * R), tn++;
	}
	r.rms = (float) std::sqrt(acc / n);
	r.tail = (float) std::sqrt(tacc / std::max(tn, 1));
	return r;
}

int main() {
	const float sr = 48000.f;
	const float M[3] = {0.f, 0.5f, 1.f};
	int played = 0;
	// GENRA MONO: a bass line
	for (int pr = 0; pr < Bass::count(); pr++) {
		float rmsAt[3];
		for (int k = 0; k < 3; k++) {
			Bass v;
			v.setSampleRate(sr);
			v.set(pr, M[k], M[k]);
			Res r = run(sr, 4.f, 1.6f, [&](int i, bool on, float& L, float& R) {
				MonoIn in;
				int st = (int) (i / (0.2f * sr));
				in.voct = -2.f + (st % 4 == 2 ? 7.f / 12.f : 0.f);
				in.gate = on && (i % (int) (0.2f * sr)) < (int) (0.14f * sr);
				in.vel = 0.8f;
				v.process(in, L, R);
			});
			check(r.fin, "MONO finite", Bass::name(pr), M[k], M[k], 0);
			check(r.peak < 12.f, "MONO peak", Bass::name(pr), M[k], M[k], r.peak);
			rmsAt[k] = r.rms;
			played++;
		}
		check(rmsAt[1] > 0.05f, "MONO audible", Bass::name(pr), 0.5f, 0.5f, rmsAt[1]);
		check(rmsAt[0] > 0.25f * rmsAt[1] && rmsAt[2] > 0.25f * rmsAt[1], "MONO macros keep it audible", Bass::name(pr), rmsAt[1], rmsAt[2], rmsAt[0]);
	}
	// FUNKE: a melody
	for (int pr = 0; pr < Melody::count(); pr++) {
		float rmsAt[3];
		for (int k = 0; k < 3; k++) {
			Melody v;
			v.setSampleRate(sr);
			v.set(pr, M[k], M[k]);
			Res r = run(sr, 6.f, 2.f, [&](int i, bool on, float& L, float& R) {
				MonoIn in;
				int st = (int) (i / (0.25f * sr));
				static const float N[4] = {0.f, 3.f / 12.f, 7.f / 12.f, 10.f / 12.f};
				in.voct = N[st % 4];
				in.gate = on && (i % (int) (0.25f * sr)) < (int) (0.18f * sr);
				v.process(in, 124.f, L, R);
			});
			check(r.fin, "FUNKE finite", Melody::name(pr), M[k], M[k], 0);
			check(r.peak < 12.f, "FUNKE peak", Melody::name(pr), M[k], M[k], r.peak);
			rmsAt[k] = r.rms;
			played++;
		}
		check(rmsAt[1] > 0.02f, "FUNKE audible", Melody::name(pr), 0.5f, 0.5f, rmsAt[1]);
		check(rmsAt[0] > 0.25f * rmsAt[1] && rmsAt[2] > 0.25f * rmsAt[1], "FUNKE macros keep it audible", Melody::name(pr), rmsAt[1], rmsAt[2], rmsAt[0]);
	}
	// SCHMELZ: a 4-note chord
	for (int pr = 0; pr < Pads::count(); pr++)
		for (int k = 0; k < 3; k++) {
			Pads v;
			v.setSampleRate(sr);
			v.set(pr, M[k], M[k]);
			const float ch[4] = {-0.75f, -0.4167f, -0.1667f, 0.f};
			Res r = run(sr, 8.f, 2.5f, [&](int i, bool on, float& L, float& R) {
				bool g[4] = {on, on, on, on};
				v.process(ch, g, 0.8f, L, R);
			});
			check(r.fin, "SCHMELZ finite", Pads::name(pr), M[k], M[k], 0);
			check(r.peak < 12.f, "SCHMELZ peak", Pads::name(pr), M[k], M[k], r.peak);
			check(r.rms > 0.05f, "SCHMELZ audible", Pads::name(pr), M[k], M[k], r.rms);
			played++;
		}
	// "same genre" matching (src/GenreMatch.hpp)
	genrematch::setGenreNames({"House", "Deep House", "Witch House", "UK Garage", "Drum & Bass", "Lo-Fi", "2-Step", "Ambient"});
	check(styleScore("HOUSE · GARAGE", "UK Garage") > 0, "style match", "UK Garage", 0, 0, 0);
	check(styleScore("AMBIENT", "Drum & Bass") == 0, "style mismatch", "Drum & Bass", 0, 0, 0);
	check(styleScore("WITCH HOUSE", "House") == 0, "another genre's name", "Witch House", 0, 0, 0);
	check(styleScore("LO-FI", "Lo-Fi") == 2, "whole name", "Lo-Fi", 0, 0, 0);
	check(styleScore("2-STEP · GARAGE", "2-Step") == 2, "whole name", "2-Step", 0, 0, 0);
	std::printf("%d phrases, %d failures\n", played, fails);
	return fails ? 1 : 0;
}
