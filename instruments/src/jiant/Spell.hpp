// SPDX-License-Identifier: GPL-3.0-only
// VOICE's SPELL (its fifth language): SPELLBOX's LPC talking-toy voice (SpellCore), one voice. What the ENGINE knobs
// mean there, the envelope and the effects its sound goes through (the JIANT engine's own are in its firmware: SPELL
// gets small ones of its own, set by the same ENVELOPE and FX knobs), and SPELL's factory sounds.
#pragma once
#include <rack.hpp>
#include <cmath>
#include <string>
#include <vector>
#include "SpellCore.hpp"

namespace jiant_voice {

enum { SPM_SING, SPM_LETTER, SPM_SPEAK, SPM_N };
static const char* SPM_NAMES[SPM_N] = {"SING", "LETTER", "SPEAK"};
static const int SPELL_LOOPS[5] = {0, 1, 2, 4, 8};  // LOOP: the lyric from its start every n notes (0: FREE)
static const int SPELL_KLETTER = 26, SPELL_KWORDS = 8;  // LETTER: A-Z or one letter; SPEAK: 1..8 words a note, ALL

/** The eight ENGINE knobs (0..1, CV added) as SPELL reads them. */
struct SpellKnobs {
	int pos = 0;     // 0 AUTO (each note the next word) .. 127 that word
	int sel = 0;     // SING: the utterance; LETTER: 0 every key its letter, 1..26 one letter; SPEAK: 0..7 = 1..8 words, 8 ALL
	int artic = 127; // the consonants (0..127)
	int shift = 0;   // semitones -12..12
	int speed = 64, tone = 64, mono = 0;
	int loop = 0;    // index into SPELL_LOOPS

	static int iv(float x, int lo, int hi) { return lo + (int) std::round(rack::math::clamp(x, 0.f, 1.f) * (hi - lo)); }
	void read(const float* x, int mode, int count) {
		pos = iv(x[0], 0, 127);
		sel = mode == SPM_SING ? iv(x[1], 0, count - 1) : mode == SPM_LETTER ? iv(x[1], 0, SPELL_KLETTER) : iv(x[1], 0, SPELL_KWORDS);
		artic = iv(x[2], 0, 127);
		shift = iv(x[3], -12, 12);
		speed = iv(x[4], 0, 127);
		tone = iv(x[5], 0, 127);
		mono = iv(x[6], 0, 127);
		loop = iv(x[7], 0, 4);
	}
};

inline const char* spellLabel(int k, int mode) {
	static const char* L[8] = {"POS", "WORD", "ARTIC", "SHIFT", "SPEED", "TONE", "MONO", "LOOP"};
	if (k == 1)
		return mode == SPM_LETTER ? "LETTER" : mode == SPM_SPEAK ? "WORDS" : "WORD";
	return L[k & 7];
}

inline std::string spellValue(int k, const SpellKnobs& s, int mode, const jsp_api* a) {
	switch (k) {
	case 0: return s.pos ? rack::string::f("%d", s.pos) : "AUTO";
	case 1:
		if (mode == SPM_SING)
			return a ? a->text((uint32_t) s.sel) : rack::string::f("%d", s.sel);
		if (mode == SPM_LETTER)
			return s.sel ? std::string(1, (char) ('A' + s.sel - 1)) : "A-Z";
		return s.sel >= SPELL_KWORDS ? "ALL" : rack::string::f("%d", s.sel + 1);
	case 2: return rack::string::f("%d%%", (int) std::round(s.artic * 100.f / 127.f));
	case 3: return rack::string::f("%+dst", s.shift);
	case 4: return rack::string::f("%d%%", s.speed <= 64 ? 50 + s.speed * 50 / 64 : 100 + (s.speed - 64) * 100 / 63);
	case 5: return rack::string::f("%d%%", (int) std::round(s.tone * 100.f / 127.f));
	case 6: return rack::string::f("%d%%", (int) std::round(s.mono * 100.f / 127.f));
	default: return s.loop ? rack::string::f("%d", SPELL_LOOPS[s.loop]) : "FREE";
	}
}

/** SPELL's factory sounds, after the JIANT engine's (the knobs as 0..1; ENVELOPE, FX as the FM-1's 0..127). */
struct SpellPreset {
	const char* name;
	int mode, voice;
	float e[8];
	int env[4], fx[4];
	const char* word;  // SING: the word to sing
};
static const SpellPreset SPELL_PRESETS[] = {
	{"TALKING TOY", SPM_SING, 1, {0.f, 0.f, 1.f, 0.5f, 0.504f, 0.6f, 0.f, 0.f}, {0, 0, 127, 12}, {0, 18, 14, 24}, "APPLE"},
	{"LETTER KEYS", SPM_LETTER, 1, {0.f, 0.f, 1.f, 0.5f, 0.504f, 0.55f, 0.f, 0.f}, {0, 0, 127, 16}, {0, 0, 10, 20}, nullptr},
	{"ROBOT READER", SPM_SPEAK, 1, {0.f, 0.f, 1.f, 0.5f, 0.504f, 0.45f, 0.3f, 0.f}, {0, 0, 127, 20}, {12, 0, 30, 18}, nullptr},
};
static const int N_SPELL_PRESETS = sizeof(SPELL_PRESETS) / sizeof(SPELL_PRESETS[0]);

/** ATK DEC SUS REL (the FM-1's 0..127) on SING and LETTER: the gate shapes the held word. */
struct SpellEnv {
	int stage = 0;  // 0 idle, 1 attack, 2 decay/sustain, 3 release
	float env = 0.f;
	static float secs(int v) { float x = v / 127.f; return 0.002f + x * x * 4.f; }
	void gate(bool on) { stage = on ? 1 : (stage ? 3 : 0); }
	float process(const int* adsr, float sr) {
		switch (stage) {
		case 1:
			env += 1.f / (secs(adsr[0]) * sr);
			if (env >= 1.f) {
				env = 1.f;
				stage = 2;
			}
			break;
		case 2: {
			float sus = adsr[2] / 127.f;
			env = sus + (env - sus) * std::exp(-4.f / (secs(adsr[1]) * sr));
			break;
		}
		case 3:
			env *= std::exp(-4.f / (secs(adsr[3]) * sr));
			if (env < 1e-4f) {
				env = 0.f;
				stage = 0;
			}
			break;
		default: env = 0.f;
		}
		return env;
	}
};

/** DIST CHORUS DELAY REVERB (the FM-1's 0..127) for SPELL: a soft clip, a two-tap chorus, a stereo echo, a small
 * Freeverb-style room. Mono in, stereo out, at 44.1 kHz. */
struct SpellFx {
	static const int CB = 2048, DB = 16384;
	float cbuf[CB] = {}, dl[DB] = {}, dr[DB] = {};
	int cw = 0, dw = 0;
	float lfo = 0.f;
	struct Comb {
		std::vector<float> b;
		int i = 0;
		float st = 0.f;
		explicit Comb(int n) : b(n, 0.f) {}
		float run(float x) {
			float y = b[i];
			st = y * 0.75f + st * 0.25f;
			b[i] = x + st * 0.8f;
			if (++i >= (int) b.size())
				i = 0;
			return y;
		}
	};
	struct Allpass {
		std::vector<float> b;
		int i = 0;
		explicit Allpass(int n) : b(n, 0.f) {}
		float run(float x) {
			float y = b[i];
			b[i] = x + y * 0.5f;
			if (++i >= (int) b.size())
				i = 0;
			return y - x;
		}
	};
	std::vector<Comb> cl{Comb(1116), Comb(1188), Comb(1277), Comb(1356)}, cr{Comb(1139), Comb(1211), Comb(1300), Comb(1379)};
	std::vector<Allpass> al{Allpass(556), Allpass(441)}, ar{Allpass(579), Allpass(464)};

	static float tap(const float* b, int n, int w, float d) {
		float p = w - d;
		while (p < 0)
			p += n;
		int i0 = (int) p;
		float f = p - i0;
		return b[i0 % n] * (1.f - f) + b[(i0 + 1) % n] * f;
	}

	void process(float x, const int* fx, float& l, float& r) {
		if (fx[0] > 0) {
			float g = 1.f + fx[0] / 127.f * 14.f;
			x = std::tanh(g * x) / std::tanh(g);
		}
		l = r = x;
		// chorus
		cbuf[cw] = x;
		lfo += 0.7f / 44100.f;
		if (lfo >= 1.f)
			lfo -= 1.f;
		if (fx[1] > 0) {
			float c = fx[1] / 127.f * 0.7f, s = std::sin(2.f * M_PI * lfo), co = std::cos(2.f * M_PI * lfo);
			l += c * tap(cbuf, CB, cw, 530.f + 176.f * s);
			r += c * tap(cbuf, CB, cw, 530.f + 176.f * co);
		}
		cw = (cw + 1) % CB;
		// echo
		float el = dl[(dw + DB - 14112) % DB], er = dr[(dw + DB - 10584) % DB];
		float ds = fx[2] / 127.f;
		dl[dw] = x * ds + er * 0.45f;
		dr[dw] = x * ds + el * 0.45f;
		dw = (dw + 1) % DB;
		l += el, r += er;
		// room
		if (fx[3] > 0) {
			float in = x * fx[3] / 127.f * 0.12f, wl = 0.f, wr = 0.f;
			for (auto& c : cl)
				wl += c.run(in);
			for (auto& c : cr)
				wr += c.run(in);
			for (auto& a : al)
				wl = a.run(wl);
			for (auto& a : ar)
				wr = a.run(wr);
			l += wl, r += wr;
		}
	}
};

}  // namespace jiant_voice
