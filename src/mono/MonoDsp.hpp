// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – the sound engine: two oscillators (triangle → saw → square → narrow pulse), a mixer that drives the
// filter, eight filter models, two analog-style ADSR envelopes, an LFO and a VCA. Plain C++ without Rack, so the tests
// in test/test_mono.cpp can run it. The oscillators and the filter run at 2× the sample rate.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace gmono {

static const float PI = 3.14159265358979f;
static const float C4 = 261.6256f;  // 0 V on V/OCT

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

/** Soft saturation, tanh-like (Padé), exact ±1 at |x| = 3. */
inline float sat(float x) {
	x = clampf(x, -3.f, 3.f);
	return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

/** The filter types, in the order of the FTYPE parameter (keep in step with tools/mono_panel.py). */
enum FilterType { LADDER24, LADDER12, DIODE, MS20, SVF_LP, SVF_BP, SVF_HP, OTA, NUM_FILTERS };
static const char* const FILTER_NAMES[NUM_FILTERS] = {"LADDER 24", "LADDER 12", "DIODE", "MS-20",
                                                      "SVF LP",    "SVF BP",    "SVF HP", "OTA"};
static const char* const FILTER_INFO[NUM_FILTERS] = {"Moog-style LP",  "Moog-style LP, 2-pole", "303-style LP",
                                                     "Sallen-Key LP",  "Oberheim SEM",          "Oberheim SEM",
                                                     "Oberheim SEM",   "Juno-style LP"};

// ---------------------------------------------------------------- oscillator
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

struct Osc {
	float phase = 0.f;
	/** One sample. dt = frequency / sample rate; shape 0..1: triangle → saw → square → narrow pulse. */
	float process(float dt, float shape) {
		dt = clampf(dt, 0.f, 0.45f);
		phase += dt;
		if (phase >= 1.f)
			phase -= 1.f;
		float t = phase;
		float s = clampf(shape, 0.f, 1.f) * 3.f;
		int seg = std::min((int) s, 2);
		float f = s - seg;
		auto saw = [&]() { return 2.f * t - 1.f - polyblep(t, dt); };
		auto pulse = [&](float pw) {
			float y = t < pw ? 1.f : -1.f;
			y += polyblep(t, dt);
			float t2 = t - pw;
			if (t2 < 0.f)
				t2 += 1.f;
			return y - polyblep(t2, dt);
		};
		if (seg == 0)
			return (1.f - f) * (1.f - 4.f * std::fabs(t - 0.5f)) + f * saw();
		if (seg == 1)
			return (1.f - f) * saw() + f * pulse(0.5f);
		return pulse(0.5f - 0.4f * f);
	}
};

// ---------------------------------------------------------------- filters
struct Filter {
	float s[4] = {};
	float z1 = 0.f, z2 = 0.f;  // SVF states
	float hp = 0.f;            // DIODE: input high-pass
	void reset() { *this = Filter(); }

	/** One sample at rate fs. fc in Hz, res 0..1. */
	float process(float x, float fc, float res, float fs, int type) {
		fc = clampf(fc, 8.f, 0.42f * fs);
		float g = std::tan(PI * fc / fs);
		res = clampf(res, 0.f, 1.f);
		switch (type) {
			default:
			case LADDER24:
			case LADDER12:
			case OTA: {
				// 4-pole ladder (zero-delay feedback). LADDER: the loop input saturates and the passband is
				// compensated, so the bass stays when the resonance goes up (the Minitaur's trait). OTA: every stage
				// saturates and there is no compensation.
				float G = g / (1.f + g), b = 1.f / (1.f + g);
				float k = res * 4.5f;
				float comp = type == OTA ? 0.f : 0.8f;
				float S = (G * G * G * s[0] + G * G * s[1] + G * s[2] + s[3]) * b;
				float u = (x * (1.f + k * comp) - k * S) / (1.f + k * G * G * G * G);
				u = sat(u);
				float y[4], in = u;
				for (int i = 0; i < 4; i++) {
					float v = ((type == OTA ? sat(in) : in) - s[i]) * G;
					y[i] = v + s[i];
					s[i] = y[i] + v;
					in = y[i];
				}
				return type == LADDER12 ? y[1] : (type == OTA ? 1.6f * y[3] : y[3]);
			}
			case DIODE: {
				// 303-style: three poles, a little bass taken out before, more bite in the loop, no compensation
				hp += (x - hp) * 0.006f;
				x -= hp;
				float G = g / (1.f + g), b = 1.f / (1.f + g);
				float k = res * 8.6f;
				float S = (G * G * s[0] + G * s[1] + s[2]) * b;
				float u = sat(1.2f * (x - k * S) / (1.f + k * G * G * G));
				float in = u, y = 0.f;
				for (int i = 0; i < 3; i++) {
					float v = (in - s[i]) * G;
					y = v + s[i];
					s[i] = y + v;
					in = y;
				}
				return 2.2f * y;
			}
			case MS20:
			case SVF_LP:
			case SVF_BP:
			case SVF_HP: {
				// state-variable filter (zero-delay feedback). MS-20: driven input and a damping that turns negative
				// at full resonance, held in check by the saturating band-pass state – it screams. SEM: gentle.
				bool ms = type == MS20;
				float R = ms ? 1.f - 1.1f * res : 1.f - 0.97f * res;
				if (ms)
					x = sat(1.6f * x);
				float h = (x - (2.f * R + g) * z1 - z2) / (1.f + 2.f * R * g + g * g);
				float bp = g * h + z1;
				z1 = g * h + bp;
				if (ms)
					z1 = 2.f * sat(0.5f * z1);
				float lp = g * bp + z2;
				z2 = g * bp + lp;
				if (type == SVF_BP)
					return bp * (2.f * R + 0.1f);
				float tame = ms ? 1.f : 0.35f + 0.65f * R;  // the SEM's resonance peak would be far louder
				return (type == SVF_HP ? h : lp) * tame;
			}
		}
	}
};

// ---------------------------------------------------------------- envelope, LFO
/** ADSR with analog RC curves: the attack aims past 1 (fast, punchy), decay and release fall exponentially. A new gate
 * starts the attack from the current level. */
struct Env {
	float v = 0.f;
	int stage = 0;  // 0 idle, 1 attack, 2 decay/sustain, 3 release
	bool gate = false;
	static float coef(float time, float dt) { return 1.f - std::exp(-dt / std::max(time, 1e-5f)); }
	float process(bool g, float a, float d, float s, float r, float dt) {
		if (g && !gate)
			stage = 1;
		if (!g && gate)
			stage = 3;
		gate = g;
		switch (stage) {
			case 1:
				v += (1.3f - v) * coef(a / 1.466f, dt);
				if (v >= 1.f) {
					v = 1.f;
					stage = 2;
				}
				break;
			case 2: v += (s - v) * coef(d / 4.f, dt); break;
			case 3:
				v -= v * coef(r / 4.f, dt);
				if (v < 1e-5f) {
					v = 0.f;
					stage = 0;
				}
				break;
			default: break;
		}
		return v;
	}
};

struct Lfo {
	float phase = 0.f;
	float process(float hz, bool square, float dt) {
		phase += hz * dt;
		phase -= std::floor(phase);
		return square ? (phase < 0.5f ? 1.f : -1.f) : 1.f - 4.f * std::fabs(phase - 0.5f);
	}
};

/** Biquad low-pass (RBJ), for the decimator. */
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
	void lowpass(float fc, float fs, float q) {
		float w = 2.f * PI * fc / fs, cs = std::cos(w), al = std::sin(w) / (2.f * q), a0 = 1.f + al;
		b0 = (1.f - cs) / 2.f / a0;
		b1 = (1.f - cs) / a0;
		b2 = b0;
		a1 = -2.f * cs / a0;
		a2 = (1.f - al) / a0;
	}
	float process(float x) {
		float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
		x2 = x1;
		x1 = x;
		y2 = y1;
		y1 = y;
		return y;
	}
};

/** Time knobs 0..1 → 1 ms .. 10 s. */
inline float knobTime(float k) { return 0.001f * std::pow(10000.f, clampf(k, 0.f, 1.f)); }

// ---------------------------------------------------------------- the voice
struct Params {
	float wave1 = 0.33f, wave2 = 0.33f, freq2 = 0.f /* -1..1 */, level1 = 0.8f, level2 = 0.8f, glide = 0.f;
	int ftype = LADDER24;
	float cutoff = 0.5f, res = 0.f, egAmt = 0.3f /* -1..1 */, drive = 0.f, keytrk = 0.f;
	float fA = 0.f, fD = 0.4f, fS = 0.f, fR = 0.3f;
	float aA = 0.f, aD = 0.4f, aS = 1.f, aR = 0.3f;
	float rate = 0.4f, lfoPitch = 0.f, lfoFilter = 0.f;
	bool lfoSquare = false;
};

struct Inputs {
	float voct = 0.f;      // V
	bool gate = false;
	float vel = 1.f;       // 0..1
	float cutoffCv = 0.f;  // V, 1 V/oct
	float resCv = 0.f;     // V, ±10 V = full range
	float driveCv = 0.f;   // V
	float rateCv = 0.f;    // V, 1 V/oct
	float vca = 1.f;       // 0..1
	float ext = 0.f;       // V (audio)
};

struct Voice {
	static const int OS = 2;
	float fs = 48000.f;
	Osc osc1, osc2;
	Filter filter;
	Env fenv, aenv;
	Lfo lfo;
	Biquad dec1, dec2;
	float pitch = 0.f;  // V after glide
	bool started = false;
	float dc = 0.f;
	uint32_t noise = 0x12345678u;

	void setSampleRate(float sr) {
		fs = sr;
		float f = std::min(0.45f * sr, 20000.f);
		dec1.lowpass(f, sr * OS, 0.5412f);
		dec2.lowpass(f, sr * OS, 1.3066f);
	}
	Voice() { setSampleRate(48000.f); }

	/** One output sample: audio (±5 V) and the LFO (±5 V). */
	void process(const Params& p, const Inputs& in, float& audio, float& lfoOut) {
		float dt = 1.f / fs;
		// control rate = sample rate
		float l = lfo.process(0.05f * std::pow(1000.f, clampf(p.rate, 0.f, 1.f)) * std::pow(2.f, clampf(in.rateCv, -5.f, 5.f)),
		                      p.lfoSquare, dt);
		lfoOut = 5.f * l;
		if (!started) {
			pitch = in.voct;
			started = true;
		}
		float gt = p.glide * p.glide * 1.5f;
		pitch += (in.voct - pitch) * (gt > 1e-4f ? Env::coef(gt, dt) : 1.f);
		float fe = fenv.process(in.gate, knobTime(p.fA), knobTime(p.fD), p.fS, knobTime(p.fR), dt);
		float ae = aenv.process(in.gate, knobTime(p.aA), knobTime(p.aD), p.aS, knobTime(p.aR), dt);

		float vib = p.lfoPitch * p.lfoPitch * l;  // up to ±1 octave
		float semi2 = 12.f * p.freq2 * std::fabs(p.freq2);
		float f1 = C4 * std::pow(2.f, pitch + vib);
		float f2 = f1 * std::pow(2.f, semi2 / 12.f);
		float fsOs = fs * OS;

		float oct = std::log2(20.f) + clampf(p.cutoff, 0.f, 1.f) * std::log2(1000.f)
		            + p.egAmt * 7.f * fe * (0.6f + 0.4f * in.vel) + p.keytrk * pitch + in.cutoffCv
		            + p.lfoFilter * 3.f * l;
		float fc = std::pow(2.f, clampf(oct, 3.f, 15.f));
		float res = clampf(p.res + in.resCv / 10.f, 0.f, 1.f);
		float dr = clampf(p.drive + in.driveCv / 10.f, 0.f, 1.f);
		float driveGain = 1.f + 9.f * dr * dr;

		float y = 0.f;
		for (int i = 0; i < OS; i++) {
			float m = p.level1 * osc1.process(f1 / fsOs, p.wave1) + p.level2 * osc2.process(f2 / fsOs, p.wave2)
			          + in.ext / 5.f;
			noise = noise * 1664525u + 1013904223u;
			m += ((int32_t) noise) * 4.6e-10f * 1e-3f;  // a breath of noise: lets the filter start to self-oscillate
			float x = m * 0.7f * driveGain;
			float f = filter.process(x, fc, res, fsOs, p.ftype);
			y = dec2.process(dec1.process(f));
		}
		y /= 0.6f + 0.4f * std::sqrt(driveGain);
		y *= ae * in.vel * clampf(in.vca, 0.f, 1.f);
		y = sat(y * 0.9f);
		dc += (y - dc) * (2.f * PI * 8.f * dt);
		audio = 5.f * (y - dc);
		if (!std::isfinite(audio)) {  // never let a bad state stick
			audio = 0.f;
			filter.reset();
		}
	}
};

}  // namespace gmono
