// SPDX-License-Identifier: GPL-3.0-only
// SCHMELZ – the sound engine of a string machine after the 70s/80s classics:
//   STRINGS: per voice saw, pulse, sub and octave-up oscillators (PolyBLEP), shaped by a registration that morphs
//            through eight timbres (violin … synth: mix, lowpass, highpass, vowel formant, detune, brass sweep),
//            OCTAVE base / both / 8va, crescendo (attack) and release; then the ensemble (three modulated delay taps
//            at 120°, as in the string ensembles), chorus, phaser and ANIMATE (an LFO moving the registration).
//   SOLO:    per voice FM sine, saw, pulse and sub through a resonant lowpass with its own envelope, morphing through
//            BASS, E-PIANO, CLAVI, SYNTH and BELL; ADSR, hold, tremolo, velocity sensitivity.
//   Shared reverb (Schroeder/Freeverb style) on both.
// Plain C++ without Rack, so the tests in test/test_schmelz.cpp can run it.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace gschmelz {

static const float PI = 3.14159265358979f;
static const float C4 = 261.6256f;  // 0 V on V/OCT

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float lerpLog(float a, float b, float t) { return a * std::pow(b / a, t); }
/** Soft saturation, tanh-like (Padé), exact ±1 at |x| = 3. */
inline float sat(float x) {
	x = clampf(x, -3.f, 3.f);
	return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

// ---------------------------------------------------------------- knob scalings (shared with the tooltips)
inline float crescSeconds(float k) { return 0.003f * std::pow(2000.f, clampf(k, 0.f, 1.f)); }    // 3 ms .. 6 s
inline float releaseSeconds(float k) { return 0.01f * std::pow(800.f, clampf(k, 0.f, 1.f)); }    // 10 ms .. 8 s
inline float attackSeconds(float k) { return 0.001f * std::pow(2000.f, clampf(k, 0.f, 1.f)); }   // 1 ms .. 2 s
inline float decaySeconds(float k) { return 0.02f * std::pow(300.f, clampf(k, 0.f, 1.f)); }      // 20 ms .. 6 s
inline float animRate(float k) { return 0.05f * std::pow(80.f, clampf(k, 0.f, 1.f)); }           // 0.05 .. 4 Hz
inline float phaserRate(float k) { return 0.05f * std::pow(100.f, clampf(k, 0.f, 1.f)); }        // 0.05 .. 5 Hz
inline float tremRate(float k) { return 1.f * std::pow(12.f, clampf(k, 0.f, 1.f)); }             // 1 .. 12 Hz

// ---------------------------------------------------------------- registrations and solo sounds
enum { NUM_REGS = 8, NUM_SOUNDS = 5 };
static const char* const REG_NAMES[NUM_REGS] = {"VIOLIN", "VIOLA", "CELLO", "BASS", "BRASS", "ORGAN", "CHOIR", "SYNTH"};
static const char* const REG_SHORT[NUM_REGS] = {"VLN", "VLA", "CEL", "BAS", "BRS", "ORG", "CHO", "SYN"};
static const char* const SOUND_NAMES[NUM_SOUNDS] = {"BASS", "E-PIANO", "CLAVI", "SYNTH", "BELL"};
enum Oct { OCT_BASE, OCT_BOTH, OCT_8VA, NUM_OCT };
enum Ens { ENS_OFF, ENS_ENS, ENS_BOTH, ENS_CHORUS, NUM_ENS };
static const char* const ENS_NAMES[NUM_ENS] = {"OFF", "ENSEMBLE", "ENSEMBLE + CHORUS", "CHORUS"};
enum Split { SPLIT_LAYER, SPLIT_LOW, SPLIT_HIGH, NUM_SPLIT };

struct Reg {
	float saw, pulse, pw, sub, up;  // oscillator levels, pulse width
	float lp, hp;                   // filter corners (Hz)
	float formant;                  // vowel formant share
	float detune;                   // spread between voices (cents)
	float sweep;                    // brass: the lowpass opens by this many octaves at the start of a note
	float level;                    // loudness match
};
static const Reg REGS[NUM_REGS] = {
    // saw  pulse  pw    sub   up    lp     hp    form  det   sweep level
    {1.0f, 0.0f, 0.5f, 0.0f, 0.15f, 6000, 300, 0.15f, 4.f, 0.f, 1.10f},   // VIOLIN
    {1.0f, 0.1f, 0.5f, 0.1f, 0.05f, 4200, 180, 0.20f, 4.f, 0.f, 1.00f},   // VIOLA
    {1.0f, 0.2f, 0.5f, 0.35f, 0.0f, 2800, 90, 0.15f, 3.f, 0.f, 0.95f},    // CELLO
    {0.8f, 0.2f, 0.5f, 0.8f, 0.0f, 1600, 40, 0.05f, 2.f, 0.f, 0.95f},     // BASS
    {1.0f, 0.3f, 0.4f, 0.2f, 0.1f, 1300, 80, 0.0f, 6.f, 2.2f, 1.00f},     // BRASS
    {0.0f, 1.0f, 0.5f, 0.6f, 0.6f, 5000, 60, 0.0f, 0.f, 0.f, 0.60f},      // ORGAN
    {0.7f, 0.3f, 0.5f, 0.1f, 0.0f, 3500, 150, 1.0f, 7.f, 0.f, 1.25f},     // CHOIR
    {0.8f, 0.6f, 0.3f, 0.3f, 0.2f, 8000, 60, 0.0f, 12.f, 0.f, 0.80f},     // SYNTH
};
/** The registration at k (0..1): neighbours blended. */
inline Reg regAt(float k) {
	float x = clampf(k, 0.f, 1.f) * (NUM_REGS - 1);
	int i = std::min((int) x, NUM_REGS - 2);
	float t = x - i;
	const Reg &a = REGS[i], &b = REGS[i + 1];
	Reg r;
	r.saw = lerp(a.saw, b.saw, t), r.pulse = lerp(a.pulse, b.pulse, t), r.pw = lerp(a.pw, b.pw, t);
	r.sub = lerp(a.sub, b.sub, t), r.up = lerp(a.up, b.up, t);
	r.lp = lerpLog(a.lp, b.lp, t), r.hp = lerpLog(a.hp, b.hp, t);
	r.formant = lerp(a.formant, b.formant, t), r.detune = lerp(a.detune, b.detune, t);
	r.sweep = lerp(a.sweep, b.sweep, t), r.level = lerp(a.level, b.level, t);
	return r;
}

struct Snd {
	float fm, ratio, index, idxDecay;  // FM sine: level, modulator ratio, index, index decay (s)
	float saw, pulse, pw, sub;
	float lp, lpEnv, lpDecay, res;     // lowpass (Hz), its envelope (octaves) and decay (s), resonance
	float level;
};
static const Snd SOUNDS[NUM_SOUNDS] = {
    // fm   ratio index idxDec saw   pulse  pw    sub   lp     lpEnv lpDec res   level
    {0.2f, 1.0f, 0.5f, 0.2f, 0.8f, 0.3f, 0.5f, 0.6f, 350, 2.6f, 0.25f, 0.35f, 1.0f},   // BASS
    {1.0f, 1.0f, 2.4f, 0.9f, 0.0f, 0.0f, 0.5f, 0.0f, 7000, 0.0f, 1.0f, 0.0f, 1.1f},    // E-PIANO
    {0.0f, 1.0f, 0.0f, 0.2f, 0.1f, 1.0f, 0.14f, 0.0f, 2600, 1.6f, 0.18f, 0.45f, 1.0f}, // CLAVI
    {0.0f, 1.0f, 0.0f, 0.2f, 1.0f, 0.4f, 0.5f, 0.2f, 1800, 1.4f, 0.6f, 0.3f, 0.8f},    // SYNTH
    {1.0f, 3.5f, 3.0f, 1.6f, 0.0f, 0.0f, 0.5f, 0.0f, 9000, 0.0f, 1.0f, 0.0f, 0.9f},    // BELL
};
inline Snd soundAt(float k) {
	float x = clampf(k, 0.f, 1.f) * (NUM_SOUNDS - 1);
	int i = std::min((int) x, NUM_SOUNDS - 2);
	float t = x - i;
	const Snd &a = SOUNDS[i], &b = SOUNDS[i + 1];
	Snd s;
	s.fm = lerp(a.fm, b.fm, t), s.ratio = lerpLog(a.ratio, b.ratio, t), s.index = lerp(a.index, b.index, t);
	s.idxDecay = lerpLog(a.idxDecay, b.idxDecay, t);
	s.saw = lerp(a.saw, b.saw, t), s.pulse = lerp(a.pulse, b.pulse, t), s.pw = lerp(a.pw, b.pw, t);
	s.sub = lerp(a.sub, b.sub, t);
	s.lp = lerpLog(a.lp, b.lp, t), s.lpEnv = lerp(a.lpEnv, b.lpEnv, t), s.lpDecay = lerpLog(a.lpDecay, b.lpDecay, t);
	s.res = lerp(a.res, b.res, t), s.level = lerp(a.level, b.level, t);
	return s;
}

// ---------------------------------------------------------------- building blocks
inline float polyblep(float t, float dt) {
	if (t < dt) {
		t /= dt;
		return t + t - t * t - 1.f;
	}
	if (t > 1.f - dt) {
		t = (t - 1.f) / dt;
		return t * t + t + t + 1.f;
	}
	return 0.f;
}
struct Phase {
	float p = 0.f;
	float tick(float dt) {
		p += dt;
		if (p >= 1.f)
			p -= std::floor(p);
		return p;
	}
};
inline float sawAt(float t, float dt) { return 2.f * t - 1.f - polyblep(t, dt); }
inline float pulseAt(float t, float dt, float pw) {
	float y = t < pw ? 1.f : -1.f;
	y += polyblep(t, dt);
	float t2 = t - pw;
	if (t2 < 0.f)
		t2 += 1.f;
	y -= polyblep(t2, dt);
	return y - (2.f * pw - 1.f);  // no DC
}

/** Topology-preserving state-variable filter (Simper). */
struct Svf {
	float ic1 = 0.f, ic2 = 0.f;
	float g = 0.1f, k = 1.4f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
	void set(float f, float q, float fs) {
		g = std::tan(PI * clampf(f, 10.f, fs * 0.45f) / fs);
		k = 1.f / std::max(q, 0.3f);
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}
	void process(float v0, float& lp, float& bp) {
		float v3 = v0 - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2, bp = v1;
	}
	float lowpass(float x) {
		float lp, bp;
		process(x, lp, bp);
		return lp;
	}
};

struct Rng {
	uint32_t s = 0x9e3779b9u;
	float uni() {  // −1 .. 1
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return (float) (int32_t) s * (1.f / 2147483648.f);
	}
};

// ---------------------------------------------------------------- strings voice
struct StringsParams {
	float reg = 0.15f;  // 0..1 (after ANIMATE)
	int oct = OCT_BASE;
	float cresc = 0.3f, rel = 0.45f, tone = 0.5f;
};

struct StringsVoice {
	Phase ph, phSub, phUp, ph2;  // ph2: the upper octave of BOTH
	Svf lpf, f1, f2;
	float hpz = 0.f;
	float env = 0.f, sweep = 0.f, vel = 1.f;
	bool gate = false;
	float detune = 0.f;  // −1 .. 1, per voice
	float fs = 48000.f;
	int ctl = 0;
	float hpA = 0.f;
	Reg r;

	void setSampleRate(float sr) {
		fs = sr;
		ctl = 0;
	}
	/** Free-running oscillators: every voice starts at its own phases. */
	void seed(uint32_t s) {
		Rng g;
		g.s = s ? s : 1u;
		ph.p = 0.5f + 0.5f * g.uni(), phSub.p = 0.5f + 0.5f * g.uni();
		phUp.p = 0.5f + 0.5f * g.uni(), ph2.p = 0.5f + 0.5f * g.uni();
		detune = g.uni();
	}
	/** One sample. f: note frequency; trig: the gate's rising edge. */
	float process(const StringsParams& p, float f, bool gateNow, bool trig, float velocity) {
		gate = gateNow;
		if (trig) {
			vel = velocity;
			sweep = 1.f;
		}
		float t = gate ? crescSeconds(p.cresc) : releaseSeconds(p.rel);
		if (gate)
			env += (1.f - env) * (1.f - std::exp(-2.2f / (t * fs)));  // ~90 % after the crescendo time
		else
			env *= std::exp(-6.9f / (t * fs));
		sweep *= std::exp(-1.f / (0.25f * fs));
		if (env < 1e-5f && !gate)
			return 0.f;

		if (--ctl <= 0) {
			ctl = 16;
			r = regAt(p.reg);
			float toneSc = std::pow(2.f, (clampf(p.tone, 0.f, 1.f) - 0.5f) * 3.f);
			float cut = r.lp * toneSc * std::pow(2.f, r.sweep * sweep);
			lpf.set(cut, 0.75f, fs);
			f1.set(730.f, 5.f, fs);
			f2.set(1150.f, 6.f, fs);
			hpA = std::exp(-2.f * PI * r.hp / fs);
		}
		float fr = f * std::pow(2.f, detune * r.detune / 1200.f);
		float base = p.oct == OCT_8VA ? 2.f * fr : fr;
		float dt = base / fs;
		float t0 = ph.tick(dt);
		float x = r.saw * sawAt(t0, dt) + r.pulse * pulseAt(t0, dt, r.pw);
		float dts = dt * 0.5f, ts = phSub.tick(dts);
		x += r.sub * pulseAt(ts, dts, 0.5f) * 0.8f;
		float dtu = dt * 2.f, tu = phUp.tick(dtu);
		x += r.up * sawAt(tu, dtu);
		if (p.oct == OCT_BOTH) {
			float dt2 = dt * 2.0f, t2 = ph2.tick(dt2);
			x += 0.65f * (r.saw * sawAt(t2, dt2) + r.pulse * pulseAt(t2, dt2, r.pw));
		}
		float y = lpf.lowpass(x);
		if (r.formant > 0.01f) {
			float lp, b1, b2;
			f1.process(y, lp, b1);
			f2.process(y, lp, b2);
			y = lerp(y, (b1 + 0.7f * b2) * 2.2f, r.formant);
		}
		// one-pole highpass
		hpz = hpA * hpz + (1.f - hpA) * y;
		y -= hpz;
		return y * env * (0.55f + 0.45f * vel) * r.level * 0.35f;
	}
};

// ---------------------------------------------------------------- solo voice
struct SoloParams {
	float sound = 0.25f;
	float atk = 0.f, dec = 0.5f, sus = 0.6f;
	float velSens = 0.5f;
};

struct SoloVoice {
	Phase ph, phM, phSub;
	Svf lpf;
	float env = 0.f, fenv = 0.f, ienv = 0.f, vel = 1.f;
	int stage = 0;  // 0 idle, 1 attack, 2 decay/sustain, 3 release
	bool gate = false;
	float fs = 48000.f;
	int ctl = 0;
	Snd s;

	void setSampleRate(float sr) {
		fs = sr;
		ctl = 0;
	}
	void seed(uint32_t sd) {
		Rng g;
		g.s = sd ? sd : 1u;
		ph.p = 0.5f + 0.5f * g.uni(), phM.p = 0.5f + 0.5f * g.uni(), phSub.p = 0.5f + 0.5f * g.uni();
	}
	float process(const SoloParams& p, float f, bool gateNow, bool trig, float velocity) {
		if (trig) {
			vel = velocity;
			stage = 1;
			fenv = ienv = 1.f;
		}
		if (gate && !gateNow)
			stage = env > 0.f ? 3 : 0;
		gate = gateNow;
		// ADSR (RC curves); release time = DECAY
		float dec = decaySeconds(p.dec);
		if (stage == 1) {
			env += (1.25f - env) * (1.f - std::exp(-1.5f / (attackSeconds(p.atk) * fs)));
			if (env >= 1.f)
				env = 1.f, stage = 2;
		} else if (stage == 2) {
			env += (clampf(p.sus, 0.f, 1.f) - env) * (1.f - std::exp(-4.6f / (dec * fs)));
		} else if (stage == 3) {
			env *= std::exp(-6.9f / (dec * fs));
			if (env < 1e-5f)
				env = 0.f, stage = 0;
		}
		if (stage == 0)
			return 0.f;
		if (--ctl <= 0) {
			ctl = 16;
			s = soundAt(p.sound);
		}
		fenv *= std::exp(-1.f / (s.lpDecay * fs));
		ienv *= std::exp(-1.f / (s.idxDecay * fs));
		if ((ctl & 3) == 0)
			lpf.set(s.lp * std::pow(2.f, s.lpEnv * fenv) * (0.7f + 0.6f * vel), 0.7f + s.res * 4.f, fs);

		float dt = f / fs;
		float t = ph.tick(dt);
		float x = 0.f;
		if (s.fm > 0.01f) {
			float m = std::sin(2.f * PI * phM.tick(dt * s.ratio));
			float idx = s.index * (0.25f + 0.75f * ienv) * (0.6f + 0.4f * vel);
			x += s.fm * std::sin(2.f * PI * t + idx * m);
		}
		x += s.saw * sawAt(t, dt) + s.pulse * pulseAt(t, dt, s.pw);
		if (s.sub > 0.01f) {
			float dts = dt * 0.5f;
			x += s.sub * pulseAt(phSub.tick(dts), dts, 0.5f) * 0.8f;
		}
		float y = lpf.lowpass(x);
		float amp = 1.f - clampf(p.velSens, 0.f, 1.f) * (1.f - vel);
		return y * env * amp * s.level * 0.4f;
	}
};

// ---------------------------------------------------------------- effects
/** A delay line with linear interpolation, sized for 192 kHz. */
struct Delay {
	static const int N = 16384;
	float buf[N] = {};
	int w = 0;
	void push(float x) {
		buf[w] = x;
		w = (w + 1) & (N - 1);
	}
	float read(float samples) const {
		samples = clampf(samples, 1.f, N - 2.f);
		float rp = (float) w - samples;
		while (rp < 0.f)
			rp += N;
		int i = (int) rp;
		float f = rp - i;
		return buf[i & (N - 1)] * (1.f - f) + buf[(i + 1) & (N - 1)] * f;
	}
};

/** The string ensemble: three taps of one delay, each modulated by a slow and a fast LFO, 120° apart. */
struct Ensemble {
	Delay d;
	float ps = 0.f, pf = 0.f, fs = 48000.f;
	void process(float x, float depth, float& l, float& r) {
		d.push(x);
		ps += 0.63f / fs;
		pf += 5.7f / fs;
		ps -= std::floor(ps);
		pf -= std::floor(pf);
		float tap[3];
		for (int i = 0; i < 3; i++) {
			float o = i / 3.f;
			float ms = 0.0016f * std::sin(2.f * PI * (ps + o)) + 0.0004f * std::sin(2.f * PI * (pf + o));
			tap[i] = d.read((0.0065f + ms * depth) * fs);
		}
		l = (tap[0] + 0.6f * tap[1]) * 0.7f + 0.2f * x;
		r = (tap[2] + 0.6f * tap[1]) * 0.7f + 0.2f * x;
	}
};

/** A slow, wide chorus for one channel. */
struct Chorus {
	Delay d;
	float p = 0.f, fs = 48000.f, offset = 0.f;
	float process(float x, float depth) {
		d.push(x);
		p += 0.27f / fs;
		p -= std::floor(p);
		float m = 0.003f * depth * std::sin(2.f * PI * (p + offset));
		return 0.55f * x + 0.6f * d.read((0.012f + m) * fs);
	}
};

/** Six first-order allpasses swept by an LFO, with feedback. */
struct Phaser {
	float z[6] = {}, fb = 0.f, p = 0.f, fs = 48000.f, offset = 0.f;
	float process(float x, float amount, float rate) {
		if (amount <= 0.001f)
			return x;
		p += rate / fs;
		p -= std::floor(p);
		float lfo = 0.5f + 0.5f * std::sin(2.f * PI * (p + offset));
		float fc = 200.f * std::pow(18.f, lfo);
		float t = std::tan(PI * std::min(fc, fs * 0.45f) / fs);
		float a = (t - 1.f) / (t + 1.f);
		float y = x + fb * 0.55f;
		for (int i = 0; i < 6; i++) {
			float o = a * y + z[i];
			z[i] = y - a * o;
			y = o;
		}
		fb = y;
		return x * (1.f - 0.5f * amount) + y * 0.5f * amount;
	}
};

/** Schroeder/Freeverb-style reverb: 4 damped combs + 2 allpasses per channel. */
struct Reverb {
	struct Line {
		float* b = nullptr;
		int n = 1, i = 0;
		float z = 0.f;
	};
	static const int MAXL = 8192;
	float mem[12][MAXL] = {};
	Line comb[2][4], ap[2][2];
	float fs = 48000.f;
	void setSampleRate(float sr) {
		fs = sr;
		static const float cl[4] = {0.0297f, 0.0371f, 0.0411f, 0.0437f};
		static const float al[2] = {0.0050f, 0.0017f};
		int m = 0;
		for (int c = 0; c < 2; c++) {
			float spread = c ? 1.031f : 1.f;
			for (int i = 0; i < 4; i++) {
				comb[c][i].b = mem[m++];
				comb[c][i].n = std::min(MAXL - 1, (int) (cl[i] * spread * sr));
				comb[c][i].i = 0;
			}
			for (int i = 0; i < 2; i++) {
				ap[c][i].b = mem[m++];
				ap[c][i].n = std::min(MAXL - 1, (int) (al[i] * spread * sr));
				ap[c][i].i = 0;
			}
		}
		for (int i = 0; i < 12; i++)
			std::memset(mem[i], 0, sizeof(mem[i]));
	}
	Reverb() { setSampleRate(48000.f); }
	void process(float inL, float inR, float size, float& l, float& r) {
		float fb = 0.70f + 0.27f * clampf(size, 0.f, 1.f);
		float damp = 0.35f;
		float in[2] = {inL, inR}, o[2];
		for (int c = 0; c < 2; c++) {
			float x = in[c] * 0.25f, s = 0.f;
			for (int i = 0; i < 4; i++) {
				Line& L = comb[c][i];
				float y = L.b[L.i];
				L.z = y * (1.f - damp) + L.z * damp;
				L.b[L.i] = x + L.z * fb;
				L.i = (L.i + 1) % L.n;
				s += y;
			}
			for (int i = 0; i < 2; i++) {
				Line& A = ap[c][i];
				float y = A.b[A.i];
				A.b[A.i] = s + y * 0.5f;
				A.i = (A.i + 1) % A.n;
				s = y - s * 0.5f;
			}
			o[c] = s;
		}
		l = o[0], r = o[1];
	}
};

/** The nearest registration's name (for the display). */
inline const char* regName(float k) { return REG_NAMES[(int) std::lround(clampf(k, 0.f, 1.f) * (NUM_REGS - 1))]; }

/** SPLIT when the solo plays from the strings inputs: who takes a note (split point C4 = 0 V). */
inline bool soloTakes(int split, float voct) { return split == SPLIT_LAYER || (split == SPLIT_LOW ? voct < 0.f : voct >= 0.f); }
inline bool stringsTakes(int split, float voct) { return split == SPLIT_LAYER || !soloTakes(split, voct); }

// ---------------------------------------------------------------- the bus: everything after the voices
struct BusParams {
	int ens = ENS_BOTH;
	float ensDepth = 0.7f;
	float anim = 0.f, animRate = 0.3f;
	float phaser = 0.f, phaserRate = 0.3f;
	float verb = 0.25f, size = 0.6f;
	float trem = 0.f, tremRate = 0.4f;
	float bal = 0.5f, vol = 0.7f;
};

struct Bus {
	Ensemble ens;
	Chorus chL, chR;
	Phaser phL, phR;
	Reverb verb;
	float fs = 48000.f;
	float animPh = 0.f, tremPh = 0.f;
	float dcx[2] = {}, dcy[2] = {}, dcR = 0.9987f;

	void setSampleRate(float sr) {
		fs = sr;
		ens.fs = chL.fs = chR.fs = phL.fs = phR.fs = sr;
		chR.offset = 0.5f;
		phR.offset = 0.25f;
		verb.setSampleRate(sr);
		dcR = 1.f - 2.f * PI * 8.f / sr;
	}
	/** ANIMATE: the offset (registration units) to add for this sample. */
	float animate(const BusParams& p) {
		animPh += animRate(p.animRate) / fs;
		animPh -= std::floor(animPh);
		return clampf(p.anim, 0.f, 1.f) * 0.45f * std::sin(2.f * PI * animPh);
	}
	/** str, solo: the summed voices. Returns L/R (about ±5 V at full use) and the dry STR and SOLO outputs. */
	void process(const BusParams& p, float str, float solo, float& L, float& R, float& strOut, float& soloOut) {
		float sl = str, sr = str;
		float d = clampf(p.ensDepth, 0.f, 1.f);
		switch (p.ens) {
			case ENS_ENS: ens.process(str, d, sl, sr); break;
			case ENS_BOTH: {
				float el, er;
				ens.process(str, d, el, er);
				sl = chL.process(el, d) * 0.8f;
				sr = chR.process(er, d) * 0.8f;
			} break;
			case ENS_CHORUS:
				sl = chL.process(str, d);
				sr = chR.process(str, d);
				break;
			default: ens.process(str, d, L, R); break;  // keep the delay filled
		}
		strOut = 0.5f * (sl + sr);
		float pr = phaserRate(p.phaserRate);
		sl = phL.process(sl, p.phaser, pr);
		sr = phR.process(sr, p.phaser, pr);

		tremPh += tremRate(p.tremRate) / fs;
		tremPh -= std::floor(tremPh);
		float tw = std::sin(2.f * PI * tremPh), t = clampf(p.trem, 0.f, 1.f);
		soloOut = solo;
		float ol = solo * (1.f - t * (0.5f + 0.5f * tw)), orr = solo * (1.f - t * (0.5f - 0.5f * tw));

		float b = clampf(p.bal, 0.f, 1.f);
		float gs = std::min(1.f, 2.f * (1.f - b)), go = std::min(1.f, 2.f * b);
		float l = sl * gs + ol * go, r = sr * gs + orr * go;
		float wl, wr;
		verb.process(l, r, p.size, wl, wr);
		float v = clampf(p.verb, 0.f, 1.f);
		l = l * (1.f - 0.4f * v) + wl * v * 1.4f;
		r = r * (1.f - 0.4f * v) + wr * v * 1.4f;
		float in[2] = {l, r};
		for (int c = 0; c < 2; c++) {
			float y = in[c] - dcx[c] + dcR * dcy[c];
			dcx[c] = in[c], dcy[c] = y;
			in[c] = y;
		}
		float g = 6.f * clampf(p.vol, 0.f, 1.f);
		L = 10.f * sat(in[0] * g / 10.f);
		R = 10.f * sat(in[1] * g / 10.f);
		strOut *= 5.f;
		soloOut *= 5.f;
	}
};

}  // namespace gschmelz
