// SPDX-License-Identifier: GPL-3.0-only
// GRUND – the bass voice for GENRA's bass track: ten bass models (upright, arco, finger, pick, slap, fretless,
// synth, reese, FM, 808), an amp with tone filter, drive and compressor, ADSR, and an FX bus (octaver, EQ, chorus,
// width, reverb) whose details GRUND FX shows. Plain C++ without Rack, so test/test_grund.cpp can run it.
// The voice runs at 2× the sample rate (strings, oscillators, filter), the bus at the sample rate.
#pragma once
#include "../mono/MonoDsp.hpp"
#include "../schmelz/SchmelzDsp.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ggrund {

using gmono::clampf;
using gmono::sat;
static const float PI = 3.14159265358979f;
static const float C4 = 261.6256f;

enum Model { UPRIGHT, ARCO, FINGER, PICK, SLAP, FRETLESS, SYNTH, REESE, FM, B808, NUM_MODELS };
static const char* const MODEL_NAMES[NUM_MODELS] = {"UPRIGHT",  "ARCO",  "FINGER", "PICK", "SLAP",
                                                    "FRETLESS", "SYNTH", "REESE",  "FM",   "808"};
static const char* const MODEL_INFO[NUM_MODELS] = {
    "Plucked double bass", "Bowed double bass", "Electric, fingers", "Electric, plectrum", "Electric, slapped",
    "Electric, no frets",  "Analog, saw / square", "Detuned saws",   "2-op DX bass",        "Sine, pitch drop"};
// what TIMBRE and BITE do, per model (tooltips, manual)
static const char* const MODEL_HELP[NUM_MODELS] = {
    "TIMBRE: where the string is plucked (neck to bridge). BITE: finger attack",
    "TIMBRE: bow position (soft to bright). BITE: bow pressure and scratch",
    "TIMBRE: pickup (neck to bridge). BITE: finger attack",
    "TIMBRE: pick position. BITE: pick hardness and click",
    "TIMBRE: brightness of the strings. BITE: thumb thump (ACC: pop)",
    "TIMBRE: mwah (the singing filter). BITE: finger attack",
    "TIMBRE: saw to square. BITE: attack click",
    "TIMBRE: detune of the two saws. BITE: attack click",
    "TIMBRE: modulator ratio (0.5 to 3). BITE: FM index on the attack",
    "TIMBRE: distortion. BITE: pitch drop on the attack"};

enum Amp { DI, FLIPTOP, STACK, FUZZAMP, NUM_AMPS };
static const char* const AMP_NAMES[NUM_AMPS] = {"DI", "FLIP-TOP", "STACK", "FUZZ"};
static const char* const AMP_INFO[NUM_AMPS] = {"Direct, clean", "Tube amp, 1x15 cab", "Rock stack, 8x10 cab",
                                               "Fuzz into a 2x10"};
enum DriveType { D_TUBE, D_FET, D_FUZZ, D_CRUSH, NUM_DRIVES };
static const char* const DRIVE_NAMES[NUM_DRIVES] = {"TUBE", "FET", "FUZZ", "CRUSH"};
static const char* const DRIVE_INFO[NUM_DRIVES] = {"Warm, even", "Tight, bright", "Broken up", "Bits and rate"};
enum VoiceMode { MODE_MONO, MODE_LEGATO, NUM_MODES };

inline float wrap(float p) { return p - std::floor(p); }
inline float sin2pi(float p) {
	float x = p - std::floor(p + 0.5f);
	if (x > 0.25f)
		x = 0.5f - x;
	else if (x < -0.25f)
		x = -0.5f - x;
	float y = 2.f * PI * x, y2 = y * y;
	return y * (1.f - y2 / 6.f * (1.f - y2 / 20.f * (1.f - y2 / 42.f)));
}
inline float glideSeconds(float k) { return 0.5f * k * k; }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }

// ---------------------------------------------------------------- the settings behind the FX (GRUND FX shows them)
struct FxSettings {
	bool compOn = true;
	float thresh = 0.5f, ratio = 0.4f, cAtk = 0.3f, cRel = 0.4f;
	bool driveOn = false;
	int dType = D_TUBE;
	float dTone = 0.5f, dMix = 0.5f;
	bool eqOn = true;
	float hpf = 0.15f, low = 0.5f, mid = 0.5f, high = 0.5f;  // 0.5 = flat (±12 dB)
	bool choOn = true;
	float choRate = 0.3f, choDepth = 0.5f;
	bool verbOn = true;
	float vSize = 0.35f, vDamp = 0.5f, vPre = 0.2f;
	bool choHi = true, verbHi = true;  // only the highs into chorus / reverb (the bass stays mono and firm)
	static const int N = 23;
	void toArray(float* a) const {
		const float v[N] = {(float) compOn, thresh, ratio, cAtk, cRel, (float) driveOn, (float) dType, dTone, dMix,
		                    (float) eqOn, hpf, low, mid, high, (float) choOn, choRate, choDepth, (float) verbOn,
		                    vSize, vDamp, vPre, (float) choHi, (float) verbHi};
		std::copy(v, v + N, a);
	}
	void fromArray(const float* a, int n) {
		FxSettings d;
		float v[N];
		d.toArray(v);
		for (int i = 0; i < n && i < (int) N; i++)
			v[i] = a[i];
		compOn = v[0] > 0.5f, thresh = v[1], ratio = v[2], cAtk = v[3], cRel = v[4];
		driveOn = v[5] > 0.5f, dType = std::max(0, std::min(NUM_DRIVES - 1, (int) std::round(v[6]))), dTone = v[7], dMix = v[8];
		eqOn = v[9] > 0.5f, hpf = v[10], low = v[11], mid = v[12], high = v[13];
		choOn = v[14] > 0.5f, choRate = v[15], choDepth = v[16];
		verbOn = v[17] > 0.5f, vSize = v[18], vDamp = v[19], vPre = v[20];
		choHi = v[21] > 0.5f, verbHi = v[22] > 0.5f;
	}
};

// ---------------------------------------------------------------- parameters
struct Params {
	int model = FINGER;
	float timbre = 0.5f, bite = 0.4f, body = 0.5f, sub = 0.f, mute = 0.2f;
	int mode = MODE_MONO;
	float glide = 0.2f, accent = 0.5f;
	int amp = DI;
	float tone = 0.6f, res = 0.1f, fenv = 0.3f, fdec = 0.4f, drive = 0.1f, comp = 0.3f, key = 0.4f;
	float atk = 0.f, dec = 0.5f, sus = 0.8f, rel = 0.3f;
	float chorus = 0.f, octave = 0.f, width = 0.2f, verb = 0.1f;
	float cutCv = 0.f;  // octaves
	FxSettings fx;
};

struct Input {
	float voct = -2.f;
	bool gate = false;
	float vel = 0.8f;
	bool acc = false;
	bool slide = false;
	bool slideConnected = false;
	float sc = 0.f;            // sidechain (GRUND FX), volts
	bool scConnected = false;
};

// ---------------------------------------------------------------- building blocks
struct Biq {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	void set(float B0, float B1, float B2, float A0, float A1, float A2) {
		b0 = B0 / A0, b1 = B1 / A0, b2 = B2 / A0, a1 = A1 / A0, a2 = A2 / A0;
	}
	void lowpass(float f, float fs, float q) {
		float w = 2.f * PI * f / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		set((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void highpass(float f, float fs, float q) {
		float w = 2.f * PI * f / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		set((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
	}
	void bandpass(float f, float fs, float q) {  // 0 dB peak
		float w = 2.f * PI * f / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		set(al, 0, -al, 1 + al, -2 * c, 1 - al);
	}
	void peak(float f, float fs, float q, float db) {
		float A = std::pow(10.f, db / 40.f), w = 2.f * PI * f / fs, c = std::cos(w), al = std::sin(w) / (2.f * q);
		set(1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
	}
	void lowshelf(float f, float fs, float db) {
		float A = std::pow(10.f, db / 40.f), w = 2.f * PI * f / fs, c = std::cos(w), s = std::sin(w);
		float al = s / 2.f * std::sqrt(2.f), sa = 2.f * std::sqrt(A) * al;
		set(A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
		    (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
	}
	void highshelf(float f, float fs, float db) {
		float A = std::pow(10.f, db / 40.f), w = 2.f * PI * f / fs, c = std::cos(w), s = std::sin(w);
		float al = s / 2.f * std::sqrt(2.f), sa = 2.f * std::sqrt(A) * al;
		set(A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
		    (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
	}
	float process(float x) {
		float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1, x1 = x, y2 = y1, y1 = y;
		return y;
	}
	void reset() { x1 = x2 = y1 = y2 = 0.f; }
};

struct Ring {
	std::vector<float> b;
	int w = 0, n = 4;
	void resize(int len) {
		n = std::max(len, 4);
		b.assign(n, 0.f);
		w = 0;
	}
	void push(float x) {
		b[w] = x;
		if (++w >= n)
			w = 0;
	}
	float read(float d) const {
		d = clampf(d, 1.f, n - 2.f);
		float rp = w - d;
		if (rp < 0.f)
			rp += n;
		int i = (int) rp;
		if (i >= n) // (w - d a hair below 0 rounds to n after += n: index n is past the end)
			i -= n, rp -= n;
		float f = rp - i;
		int j = i + 1 >= n ? 0 : i + 1;
		return b[i] + (b[j] - b[i]) * f;
	}
};

/** One-pole lowpass. */
struct OnePole {
	float z = 0.f, a = 1.f;
	void set(float f, float fs) { a = 1.f - std::exp(-2.f * PI * f / fs); }
	float process(float x) { return z += (x - z) * a; }
};

/** PolyBLEP saw, phase 0..1, dt = f / fs. */
inline float blep(float t, float dt) {
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

// ---------------------------------------------------------------- the voice (mono)
struct Voice {
	static const int OS = 2;
	float fs = 48000.f, fsOs = 96000.f;

	int model = FINGER;
	float pitch = -2.f, target = -2.f;
	bool gate = false, active = false, started = false;
	float vel = 0.8f, acc = 0.f, age = 0.f;

	// string (Karplus-Strong with a fractional delay and a loop filter)
	std::vector<float> ks;
	int ksN = 4, ksW = 0;
	float ksLp = 0.f, ksAp = 0.f, ksApX = 0.f;
	float bowPh = 0.f;
	// oscillators
	float ph[6] = {};
	float drop = 0.f;  // 808 pitch drop (octaves), decays
	float dropK = 0.f;
	float fmIdx = 0.f, fmK = 0.f;
	float click = 0.f, clickK = 0.f, pop = 0.f, popK = 0.f, thump = 0.f, thumpK = 0.f;
	float muteEnv = 1.f;
	float muteTauC = -1.f, muteDtC = -1.f, muteMul = 1.f;
	float biteEnv = 0.f, biteK = 0.f, biteAmt = 0.f;  // BITE: the tone opens on the attack, then closes (all models)
	uint32_t rng = 0x9e3779b9u;

	gmono::Filter filter;
	gmono::Biquad d1, d2;
	Biq body1, body2, mwah;
	gmono::Env aenv, fenv;
	float lastEnv = 0.f;
	// declick: a sounding voice struck again restarts its string / phases; the step is faded out (~3 ms)
	bool restrike = false;
	float prev = 0.f, jump = 0.f, declickK = 0.99f;
	// cached
	float mk[12], mv[12];
	float lastBody = -1.f, lastFs = -1.f;

	Voice() {
		for (float& k : mk)
			k = -9.f;
		setSampleRate(48000.f);
	}
	void setSampleRate(float sr) {
		fs = sr;
		fsOs = sr * OS;
		declickK = std::exp(-1.f / (0.003f * sr));
		float f = std::min(0.45f * sr, 20000.f);
		d1.lowpass(f, fsOs, 0.5412f);
		d2.lowpass(f, fsOs, 1.3066f);
		ksN = (int) (fsOs / 14.f) + 8;  // down to ~14 Hz (B-1 and a slide below)
		ks.assign(ksN, 0.f);
		ksW = 0;
		lastBody = -1.f;
		for (float& k : mk)
			k = -9.f;
	}
	float noise() {
		rng ^= rng << 13;
		rng ^= rng >> 17;
		rng ^= rng << 5;
		return (int32_t) rng * 4.6566e-10f;
	}
	float memo(int i, float k, float (*f)(float)) {
		if (k != mk[i])
			mk[i] = k, mv[i] = f(k);
		return mv[i];
	}
	static bool isString(int m) { return m <= FRETLESS; }

	/** A new note (not legato): excite the model. */
	void strike(const Params& p, float v, bool a) {
		if (active)
			restrike = true;
		model = p.model;
		vel = v;
		acc = a ? 1.f : 0.f;
		age = 0.f;
		active = true;
		muteEnv = 1.f;
		float bite = clampf(p.bite + 0.3f * acc * p.accent, 0.f, 1.f);
		float f = C4 * std::pow(2.f, pitch);
		// clicks and thumps (at fsOs)
		click = (model == PICK ? 0.9f : model == SYNTH || model == REESE ? 0.35f : 0.25f) * bite;
		clickK = std::exp(-1.f / (0.0015f * fsOs));
		thump = model == SLAP ? 1.2f * (0.4f + 0.6f * bite) : 0.f;
		thumpK = std::exp(-1.f / (0.012f * fsOs));
		pop = (model == SLAP && a) ? 1.4f : 0.f;  // the popped string on an accent
		popK = std::exp(-1.f / (0.02f * fsOs));
		biteEnv = 1.f;
		biteAmt = bite;
		biteK = std::exp(-1.f / ((0.015f + 0.06f * (1.f - bite)) * fsOs));
		drop = model == B808 ? 1.6f * bite : 0.f;
		dropK = std::exp(-1.f / (0.045f * fsOs));
		fmIdx = model == FM ? 1.f + 5.f * bite : 0.f;
		fmK = std::exp(-1.f / (0.12f * fsOs));
		if (isString(model)) {
			// fill the string: a noise burst, shaped by the bite (bright or soft) and the pluck position
			int n = std::min(ksN - 4, std::max(8, (int) (fsOs / std::max(f, 15.f))));
			float lp = 0.f, a1 = 0.08f + 0.85f * bite * bite;
			if (model == PICK)
				a1 = std::min(1.f, a1 + 0.3f);
			if (model == SLAP)
				a1 = std::min(1.f, a1 + 0.2f);
			for (int i = 0; i < ksN; i++)
				ks[i] = 0.f;
			float posf = 0.08f + 0.35f * (1.f - clampf(p.timbre, 0.f, 1.f));  // pluck near the bridge = brighter
			if (model == ARCO)
				a1 *= 0.4f;
			std::vector<float> tmp(n);
			for (int i = 0; i < n; i++) {
				lp += (noise() - lp) * a1;
				tmp[i] = lp;
			}
			// the comb of the pluck position, and a smooth window so it does not tick
			int pd = std::max(1, (int) (posf * n));
			float peak = 1e-6f;
			for (int i = 0; i < n; i++) {
				float x = tmp[i] - (i >= pd ? tmp[i - pd] : 0.f);
				float w = std::sin(PI * (i + 0.5f) / n);
				ks[i] = x * w;
				peak = std::max(peak, std::fabs(ks[i]));
			}
			for (int i = 0; i < n; i++)
				ks[i] *= (model == ARCO ? 0.3f : 1.f) / peak;
			ksW = n;
			ksLp = ksAp = ksApX = 0.f;
		}
		if (model == SYNTH || model == REESE || model == FM || model == B808) {
			for (float& x : ph)
				x = 0.f;
			ph[1] = 0.37f;  // REESE: the second saw a little off
		}
	}

	float stringOut(const Params& p, float f, float muteK) {
		// the loop: delay of one period (fractional, allpass), a damping low-pass and the loop gain
		float period = fsOs / std::max(f, 15.f);
		float d = clampf(period - 0.5f, 2.f, (float) ksN - 3.f);
		int di = (int) d;
		float fr = d - di;
		float c = (1.f - fr) / (1.f + fr);  // allpass for the fraction
		int r = ksW - di;
		if (r < 0)
			r += ksN;
		float x = ks[r];
		float y = c * x + ksApX - c * ksAp;
		ksApX = x, ksAp = y;
		float bright = model == UPRIGHT ? 0.35f : model == ARCO ? 0.45f : model == FRETLESS ? 0.5f
		             : model == PICK ? 0.8f : model == SLAP ? 0.85f : 0.6f;
		bright *= 0.6f + 0.6f * clampf(p.timbre, 0.f, 1.f);
		float a = clampf(0.05f + 0.9f * bright * (1.f - 0.85f * p.mute), 0.02f, 0.98f);
		ksLp += (y - ksLp) * a;
		float sustain = model == UPRIGHT ? 0.9965f : model == FRETLESS ? 0.9993f : model == ARCO ? 0.9995f : 0.9988f;
		sustain = 1.f - (1.f - sustain) * muteK;
		float g = ksLp * sustain;
		if (model == ARCO && gate) {
			// the bow: a sawtooth-like stick-slip at the pitch plus scratch, fed while the gate is up
			bowPh = wrap(bowPh + f / fsOs);
			float press = 0.25f + 0.5f * clampf(p.bite, 0.f, 1.f);
			float bow = (bowPh * 2.f - 1.f) * 0.012f * press + noise() * 0.004f * press;
			float att = std::min(1.f, age / 0.12f);
			g += bow * att;
			g = g / (1.f + 0.3f * std::fabs(g));
		}
		ks[ksW] = g;
		if (++ksW >= ksN)
			ksW = 0;
		return y;
	}

	/** One sample. p: params (tAtk… in cached form via memo), returns the voice's mono output (±1-ish). */
	float process(const Params& p, const Input& in, float glideK) {
		float dt = 1.f / fs;
		// pitch: glide on GENRA's SLIDE (or always, with SLIDE unplugged and GLIDE up); FRETLESS slides a little anyway
		float gk = glideK;
		if (model == FRETLESS && glideK >= 1.f)
			gk = 1.f - std::exp(-dt / 0.03f);
		pitch += (target - pitch) * gk;
		age += dt;
		float tA = memo(0, p.atk, gmono::knobTime), tD = memo(1, p.dec, gmono::knobTime), tR = memo(2, p.rel, gmono::knobTime);
		float tFd = memo(3, p.fdec, gmono::knobTime);
		if (model == ARCO)
			tA = std::max(tA, 0.06f);  // a bow never starts in an instant
		float ae = aenv.process(gate, tA, tD, p.sus, tR, dt);
		float fe = fenv.process(gate, 0.001f, tFd, 0.f, tFd, dt);
		lastEnv = ae;
		// MUTE: the palm on the strings – everything dies away faster
		float mu = clampf(p.mute, 0.f, 1.f);
		float muteK = 1.f + 14.f * mu * mu;
		float muteTau = 2.5f / (1.f + 40.f * mu * mu);  // 2.5 s .. 60 ms: from ringing to a dead, palm-muted thud
		if (muteTau != muteTauC || dt != muteDtC)
			muteTauC = muteTau, muteDtC = dt, muteMul = std::exp(-dt / muteTau);
		muteEnv *= muteMul;
		float tim = clampf(p.timbre, 0.f, 1.f);
		float f = C4 * std::pow(2.f, pitch + drop);
		float y = 0.f;
		// body resonances (wood, cabinet) – set when BODY or the model changes
		if (p.body != lastBody || fs != lastFs || model != (int) mk[11]) {
			lastBody = p.body, lastFs = fs, mk[11] = (float) model;
			bool up = model == UPRIGHT || model == ARCO;
			body1.bandpass(up ? 95.f : 120.f, fsOs, up ? 2.2f : 1.4f);
			body2.bandpass(up ? 230.f : 650.f, fsOs, up ? 3.f : 1.2f);
		}
		float mw = 0.f;
		if (model == FRETLESS) {
			// the "mwah": a resonant band sweeping down after each attack
			float fm = 300.f + 2200.f * tim * (0.25f + 0.75f * fe);
			mwah.bandpass(std::min(fm, 0.4f * fsOs), fsOs, 2.5f);
		}
		for (int i = 0; i < OS; i++) {
			float x = 0.f;
			if (isString(model)) {
				x = stringOut(p, f, muteK);
				if (model == FINGER || model == PICK || model == SLAP || model == FRETLESS) {
					// magnetic pickup: neck (round) to bridge (nasal)
					float pu = 0.15f + 0.5f * tim;
					x = x * (1.f - pu) + (x - body2.y1 * 0.4f) * pu;
				}
				if (model == FRETLESS)
					x += 0.5f * mwah.process(x);
			}
			else if (model == SYNTH) {
				float dtp = f / fsOs;
				ph[0] = wrap(ph[0] + dtp);
				float saw = 2.f * ph[0] - 1.f - blep(ph[0], dtp);
				float pw = tim < 0.5f ? 0.5f : 0.5f - 0.4f * (tim - 0.5f) * 2.f;  // square, then narrower pulses
				float p2 = wrap(ph[0] + pw);
				float sq = saw - (2.f * p2 - 1.f - blep(p2, dtp));
				float mixSq = std::min(1.f, tim * 2.f);
				x = 0.6f * ((1.f - mixSq) * saw + mixSq * sq * 0.8f);
			}
			else if (model == REESE) {
				float det = std::pow(2.f, (0.03f + 0.9f * tim * tim) / 12.f);  // from a slow beat to a wide, nasty reese
				float d0 = f / fsOs, d2 = f * det / fsOs;
				ph[0] = wrap(ph[0] + d0);
				ph[1] = wrap(ph[1] + d2);
				float s0 = 2.f * ph[0] - 1.f - blep(ph[0], d0), s1 = 2.f * ph[1] - 1.f - blep(ph[1], d2);
				x = 0.38f * (s0 + s1);
			}
			else if (model == FM) {
				float ratio = 0.5f + 2.5f * tim * tim;
				ratio = std::round(ratio * 2.f) / 2.f;  // musical ratios: 0.5, 1, 1.5 …
				ph[2] = wrap(ph[2] + f * ratio / fsOs);
				ph[0] = wrap(ph[0] + f / fsOs);
				float m = sin2pi(ph[2]) * (fmIdx * 0.16f + 0.25f);
				x = 0.7f * sin2pi(ph[0] + m);
				fmIdx = 1.f + (fmIdx - 1.f) * fmK;
			}
			else {  // 808
				ph[0] = wrap(ph[0] + f / fsOs);
				float s = sin2pi(ph[0]);
				float drv = 1.f + 14.f * tim * tim;  // clean sine to a square-ish, distorted 808
				x = 0.8f * sat(s * drv) / std::min(drv, 1.6f);
				drop *= dropK;
			}
			// clicks, thumps, pops
			if (click > 1e-4f) {
				x += click * noise() * 0.5f;
				click *= clickK;
			}
			if (thump > 1e-4f) {
				x += thump * sin2pi(age * 70.f) * 0.5f;
				thump *= thumpK;
			}
			if (pop > 1e-4f) {
				x += pop * (sin2pi(ph[5] = wrap(ph[5] + f * 4.f / fsOs)) * 0.35f + noise() * 0.1f);
				pop *= popK;
			}
			// SUB: a sine an octave down
			if (p.sub > 0.001f) {
				ph[4] = wrap(ph[4] + 0.5f * f / fsOs);
				x += 0.6f * p.sub * sin2pi(ph[4]);
			}
			// BODY
			float bd = clampf(p.body, 0.f, 1.f);
			x += bd * (3.2f * body1.process(x) + 1.4f * body2.process(x));
			// TONE: the amp's low-pass, with its envelope, key tracking and ACC
			float oct = std::log2(20.f) + clampf(p.tone, 0.f, 1.f) * std::log2(1000.f)
			            + (p.fenv + 0.5f * acc * p.accent) * 6.f * fe * (0.6f + 0.4f * vel) + p.key * (pitch + 2.f) + p.cutCv
			            + (tim - 0.5f) * 2.4f                 // TIMBRE: darker .. brighter, on every model
			            + biteAmt * 4.5f * biteEnv;           // BITE: the attack opens the tone
			biteEnv *= biteK;
			float fc = std::pow(2.f, clampf(oct, 3.f, 15.f));
			float g = gmono::Filter::coef(fc, fsOs);
			x = filter.processG(x, g, p.res, gmono::LADDER12);
			y = d2.process(d1.process(x));
		}
		// the models at about the same loudness (the strings are peaky: their transient is louder than their body)
		static const float LEVEL[NUM_MODELS] = {2.3f, 2.8f, 1.9f, 2.3f, 2.1f, 1.8f, 0.55f, 0.75f, 0.45f, 0.62f};
		float gain = LEVEL[model] * ae * muteEnv * (0.35f + 0.65f * vel) * (1.f + 0.6f * acc * p.accent);
		y *= gain;
		if (!std::isfinite(y)) {
			y = 0.f;
			filter.reset();
			for (float& k : ks)
				k = 0.f;
		}
		if (restrike) {
			restrike = false;
			jump = prev - y;
		}
		y += jump;
		jump *= declickK;
		if (std::fabs(jump) < 1e-7f)
			jump = 0.f;
		prev = y;
		if (!gate && aenv.stage == 0)
			active = false;
		return y;
	}
};

// ---------------------------------------------------------------- the bus: octaver, amp (drive, cab), drive, comp, EQ, chorus, width, reverb
struct Bus {
	float fs = 48000.f;
	// octaver: flip-flop on the zero crossings of the low-passed signal
	OnePole octIn, octOut1, octOut2;
	float octSign = 1.f, octLast = 0.f;
	// amp / cab
	Biq cabLp, cabBump, cabScoop, ampHp;
	int ampNow = -1;
	// drive section
	OnePole dTonePole;
	float crushPh = 0.f, crushHold = 0.f;
	// comp
	float cEnv = 0.f, cGain = 1.f;
	float gr = 0.f;  // gain reduction 0..1 (for the meter)
	// EQ
	Biq hp1, hp2, lowSh, midPk, highSh;
	float eqK[4] = {-1, -1, -1, -1};
	// chorus / width / reverb
	Ring chL, chR, pre;
	float chPh = 0.f;
	Biq choSplit, verbSplit, dampL, dampR;
	float dampNow = -1.f;
	gschmelz::Reverb verb;
	int verbTail = 0;
	float dcx[2] = {}, dcy[2] = {};
	int quiet = 0;

	void setSampleRate(float sr) {
		fs = sr;
		octIn.set(260.f, sr);
		octOut1.set(220.f, sr);
		octOut2.set(220.f, sr);
		chL.resize((int) (0.03f * sr));
		chR.resize((int) (0.03f * sr));
		pre.resize((int) (0.12f * sr));
		choSplit.highpass(250.f, sr, 0.707f);
		verbSplit.highpass(220.f, sr, 0.707f);
		verb.setSampleRate(sr);
		ampNow = -1;
		for (float& k : eqK)
			k = -1.f;
		dampNow = -1.f;
	}
	Bus() { setSampleRate(48000.f); }

	void setAmp(int a) {
		ampNow = a;
		switch (a) {
			default:
			case DI:
				cabLp.lowpass(16000.f, fs, 0.6f), cabBump.peak(100.f, fs, 0.8f, 0.f), cabScoop.peak(800.f, fs, 0.7f, 0.f);
				break;
			case FLIPTOP:
				cabLp.lowpass(4500.f, fs, 0.75f), cabBump.peak(110.f, fs, 0.9f, 3.f), cabScoop.peak(900.f, fs, 0.8f, 1.5f);
				break;
			case STACK:
				cabLp.lowpass(3600.f, fs, 0.9f), cabBump.peak(130.f, fs, 1.f, 4.f), cabScoop.peak(600.f, fs, 0.9f, -4.f);
				break;
			case FUZZAMP:
				cabLp.lowpass(5000.f, fs, 0.8f), cabBump.peak(90.f, fs, 0.9f, 2.f), cabScoop.peak(1200.f, fs, 0.8f, 3.f);
				break;
		}
		ampHp.highpass(28.f, fs, 0.7f);
	}

	static float shape(float x, int type, float drv) {
		switch (type) {
			case D_FET: return std::tanh(x * drv) / std::tanh(std::max(1.f, drv * 0.5f));
			case D_FUZZ: {
				float y = x * drv * 3.f;
				return clampf(y, -0.8f, 0.8f) * 1.1f;
			}
			default: {  // tube: asymmetric, soft
				float y = x * drv + 0.15f * x * x * drv;
				return sat(y) / std::max(1.f, 0.6f * drv);
			}
		}
	}

	/** in: the voice. octave: OCTAVE macro. Returns stereo. */
	void process(const Params& p, const Input& in, float x, float& L, float& R) {
		const FxSettings& fx = p.fx;
		bool inQuiet = std::fabs(x) < 1e-7f && !(in.scConnected && std::fabs(in.sc) > 0.05f);
		if (inQuiet && quiet > (int) (4.f * fs)) {
			L = R = 0.f;
			gr = 0.f;
			return;
		}
		// OCTAVE: a sub-octave from a flip-flop on the zero crossings (an octave pedal), smoothed
		if (p.octave > 0.001f) {
			float lp = octIn.process(x);
			if ((lp > 0.f) != (octLast > 0.f) && lp > 0.f)
				octSign = -octSign;
			octLast = lp;
			float env = std::fabs(lp);
			float o = octOut2.process(octOut1.process(octSign * std::min(env * 3.f, 1.f) * 0.6f));
			x += p.octave * 1.6f * o;
		}
		// AMP: drive into the cabinet
		if (p.amp != ampNow)
			setAmp(p.amp);
		float drv = 1.f + 9.f * p.drive * p.drive;
		float pin = ampHp.process(x);
		float y;
		switch (p.amp) {
			case DI: y = sat(pin * (1.f + 2.f * p.drive)) / (1.f + 0.4f * p.drive); break;
			case FUZZAMP: y = shape(pin, D_FUZZ, 1.5f + drv) * 0.7f; break;
			case STACK: y = shape(pin, D_TUBE, 1.5f * drv) * 0.9f; break;
			default: y = shape(pin, D_TUBE, drv) * 0.95f; break;
		}
		y = cabScoop.process(cabBump.process(cabLp.process(y)));
		// DRIVE (GRUND FX): a parallel drive with its own tone
		if (fx.driveOn && fx.dMix > 0.001f) {
			float d;
			if (fx.dType == D_CRUSH) {
				float rate = 1.f / (1.f + 12.f * fx.dTone * fx.dTone);
				crushPh += rate;
				if (crushPh >= 1.f) {
					crushPh -= 1.f;
					float q = std::pow(2.f, 12.f - 9.f * fx.dTone);
					crushHold = std::round(y * q) / q;
				}
				d = crushHold;
			}
			else {
				d = shape(y, fx.dType, 2.f + 10.f * fx.dMix);
				dTonePole.set(800.f + 9000.f * fx.dTone, fs);
				d = dTonePole.process(d);
			}
			y = y * (1.f - 0.6f * fx.dMix) + d * fx.dMix * 0.8f;
		}
		// COMP: the macro sets how hard; GRUND FX the threshold, ratio and times. SC: a key from outside ducks it
		float c = clampf(p.comp, 0.f, 1.f);
		if (fx.compOn && (c > 0.001f || in.scConnected)) {
			float det = in.scConnected ? std::fabs(in.sc) * 0.2f : std::fabs(y);
			float atk = 0.0005f + 0.03f * fx.cAtk * fx.cAtk, rel = 0.03f + 0.6f * fx.cRel * fx.cRel;
			float k = det > cEnv ? 1.f - std::exp(-1.f / (atk * fs)) : 1.f - std::exp(-1.f / (rel * fs));
			cEnv += (det - cEnv) * k;
			float thrDb = -4.f - 30.f * (in.scConnected ? std::max(c, 0.5f) : c) * (0.4f + 1.2f * fx.thresh);
			float ratio = 1.5f + 18.5f * fx.ratio * fx.ratio;
			float lev = 20.f * std::log10(std::max(cEnv, 1e-6f));
			float over = lev - thrDb;
			float redDb = over > 0.f ? over * (1.f - 1.f / ratio) : 0.f;
			float target = dbToGain(-redDb);
			cGain = target;
			gr = clampf(redDb / 24.f, 0.f, 1.f);
			float makeup = in.scConnected ? 1.f : dbToGain(std::min(12.f, 0.4f * (-thrDb - 4.f) * (1.f - 1.f / ratio)));
			y *= cGain * makeup;
		}
		else
			gr = 0.f, cEnv = 0.f;
		// EQ
		if (fx.eqOn) {
			if (fx.hpf != eqK[0]) {
				eqK[0] = fx.hpf;
				float f = 20.f * std::pow(10.f, clampf(fx.hpf, 0.f, 1.f));  // 20 .. 200 Hz
				hp1.highpass(f, fs, 0.54f);
				hp2.highpass(f, fs, 1.31f);
			}
			if (fx.low != eqK[1])
				eqK[1] = fx.low, lowSh.lowshelf(110.f, fs, (fx.low - 0.5f) * 24.f);
			if (fx.mid != eqK[2])
				eqK[2] = fx.mid, midPk.peak(700.f, fs, 0.8f, (fx.mid - 0.5f) * 24.f);
			if (fx.high != eqK[3])
				eqK[3] = fx.high, highSh.highshelf(3200.f, fs, (fx.high - 0.5f) * 24.f);
			y = highSh.process(midPk.process(lowSh.process(hp2.process(hp1.process(y)))));
		}
		// CHORUS (+ WIDTH): only the highs by default, the bass stays mono
		float l = y, r = y;
		float hi = choSplit.process(y);
		float ch = fx.choOn ? clampf(p.chorus, 0.f, 1.f) : 0.f;
		float w = clampf(p.width, 0.f, 1.f);
		float src = fx.choHi ? hi : y;
		chL.push(src);
		chR.push(src);
		if (ch > 0.001f || w > 0.001f) {
			chPh = wrap(chPh + (0.15f + 2.5f * fx.choRate * fx.choRate) / fs);
			float dep = (0.0008f + 0.004f * fx.choDepth) * fs;
			float wl = chL.read(0.009f * fs + dep * sin2pi(chPh)), wr = chR.read(0.011f * fs + dep * sin2pi(chPh + 0.5f));
			l += 0.8f * ch * (wl - 0.5f * src) + 0.35f * w * (wl - wr);
			r += 0.8f * ch * (wr - 0.5f * src) - 0.35f * w * (wl - wr);
		}
		// REVERB (pre-delay, size, damping); only the highs by default
		float v = fx.verbOn ? clampf(p.verb, 0.f, 1.f) : 0.f;
		if (v > 0.001f)
			verbTail = (int) (8.f * fs);  // after VERB goes to 0 the tail still rings out
		else if (verbTail > 0)
			verbTail--;
		if (verbTail > 0) {
			float vin = fx.verbHi ? verbSplit.process(0.5f * (l + r)) : 0.5f * (l + r);
			pre.push(vin);
			float pd = std::max(1.f, (0.001f + 0.1f * fx.vPre) * fs);
			float pv = pre.read(pd);
			if (fx.vDamp != dampNow) {
				dampNow = fx.vDamp;
				float f = 12000.f * std::pow(0.1f, clampf(fx.vDamp, 0.f, 1.f));  // 12 k .. 1.2 k
				dampL.lowpass(f, fs, 0.7f);
				dampR.lowpass(f, fs, 0.7f);
			}
			float vl, vr;
			verb.process(pv * v, pv * v, clampf(fx.vSize, 0.f, 1.f), vl, vr);
			l += 1.2f * dampL.process(vl);
			r += 1.2f * dampR.process(vr);
		}
		float o[2] = {l, r};
		for (int i = 0; i < 2; i++) {
			float z = o[i] - dcx[i] + 0.9995f * dcy[i];
			dcx[i] = o[i], dcy[i] = z;
			o[i] = z;
		}
		L = o[0], R = o[1];
		quiet = (inQuiet && std::fabs(L) + std::fabs(R) < 1e-6f) ? quiet + 1 : 0;
	}
};

// ---------------------------------------------------------------- the engine: notes, glide, voice, bus
struct Engine {
	float fs = 48000.f;
	std::unique_ptr<Voice> voice{new Voice};
	std::unique_ptr<Bus> bus{new Bus};
	bool lastGate = false;
	float glideT = -1.f, glideFs = -1.f, glideK = 1.f;
	// for the display
	float lastNote = -2.f;
	bool showGlide = false, showAcc = false;

	Engine() { setSampleRate(48000.f); }
	void setSampleRate(float sr) {
		fs = sr;
		voice->setSampleRate(sr);
		bus->setSampleRate(sr);
	}
	void allOff() {
		voice->gate = false;
		voice->active = false;
		lastGate = false;
	}

	/** One sample: L/R ±5 V, env 0..1. */
	void process(const Params& p, const Input& in, float& L, float& R, float& env) {
		Voice& v = *voice;
		bool rise = in.gate && !lastGate;
		bool legatoSlide = in.slideConnected ? in.slide : false;
		// glide: on SLIDE (when patched), else always with GLIDE up
		float gs = glideSeconds(p.glide);
		bool glide = in.slideConnected ? legatoSlide : gs > 1e-3f;
		if (in.slideConnected && legatoSlide)
			gs = std::max(gs, 0.04f);
		if (gs != glideT || fs != glideFs)
			glideT = gs, glideFs = fs, glideK = gs > 1e-4f ? 1.f - std::exp(-1.f / (gs * fs)) : 1.f;
		v.target = in.voct;
		if (!v.started) {
			v.pitch = in.voct;
			v.started = true;
		}
		if (rise) {
			// LEGATO: a new note while one sounds (GENRA's slides) glides without a new attack
			bool tie = (p.mode == MODE_LEGATO || legatoSlide) && v.active && v.aenv.stage != 0 && v.aenv.stage != 3;
			if (!glide)
				v.pitch = in.voct;
			if (!tie)
				v.strike(p, in.vel, in.acc);
			showAcc = in.acc;
			showGlide = glide;
			lastNote = in.voct;
		}
		else if (in.gate && std::fabs(in.voct - v.target) > 1e-4f)
			lastNote = in.voct;
		v.gate = in.gate;
		lastGate = in.gate;
		float y = v.active || v.gate ? v.process(p, in, glide ? glideK : 1.f) : 0.f;
		env = v.lastEnv;
		float l, r;
		bus->process(p, in, y * 1.2f, l, r);
		// a soft limit at ±10 V (transparent below about ±3 V)
		L = 10.f * sat(0.5f * l) / 1.f;
		R = 10.f * sat(0.5f * r) / 1.f;
		if (!std::isfinite(L) || !std::isfinite(R))
			L = R = 0.f;
	}
};

inline void noteName(float voct, char* out) {
	static const char* const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	int n = (int) std::round(voct * 12.f) + 48;
	n = std::max(0, std::min(127, n));
	std::snprintf(out, 8, "%s%d", N[n % 12], n / 12);
}

}  // namespace ggrund
