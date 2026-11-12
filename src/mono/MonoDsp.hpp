// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – the sound engine: two oscillators (triangle → saw → square → narrow pulse), a mixer that drives the
// filter, eight filter models, two analog-style ADSR envelopes, an LFO and a VCA. Plain C++ without Rack, so the tests
// in test/test_mono.cpp can run it. The oscillators and the filter run at 2× the sample rate.
#pragma once
#include <algorithm>
#include <cmath>
#include "../OutLimit.hpp"
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

/** The filter types, in the order of the FTYPE parameter (keep in step with tools/panels/mono_panel.py). */
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
	/** The coefficient for fc (the tan, once per sample when the filter runs oversampled). */
	static float coef(float fc, float fs) { return std::tan(PI * clampf(fc, 8.f, 0.42f * fs) / fs); }
	float process(float x, float fc, float res, float fs, int type) { return processG(x, coef(fc, fs), res, type); }
	float processG(float x, float g, float res, int type) {
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
	// 2.37: the three coefficients are kept until their time or the sample time changes (the exp was run every sample)
	float ck[3] = {-1.f, -1.f, -1.f}, cdt[3] = {-1.f, -1.f, -1.f}, cv[3] = {};
	float kept(int i, float time, float div, float dt) {
		if (time != ck[i] || dt != cdt[i])
			ck[i] = time, cdt[i] = dt, cv[i] = coef(time / div, dt);
		return cv[i];
	}
	float process(bool g, float a, float d, float s, float r, float dt) {
		if (g && !gate)
			stage = 1;
		if (!g && gate)
			stage = 3;
		gate = g;
		switch (stage) {
			case 1:
				v += (1.3f - v) * kept(0, a, 1.466f, dt);
				if (v >= 1.f) {
					v = 1.f;
					stage = 2;
				}
				break;
			case 2: v += (s - v) * kept(1, d, 4.f, dt); break;
			case 3:
				v -= v * kept(2, r, 4.f, dt);
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
	void restart() { phase = 0.25f; }  // triangle starts at its middle, rising
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
	void highpass(float fc, float fs, float q) {
		float w = 2.f * PI * fc / fs, cs = std::cos(w), al = std::sin(w) / (2.f * q), a0 = 1.f + al;
		b0 = (1.f + cs) / 2.f / a0;
		b1 = -(1.f + cs) / a0;
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

/** TUNE knobs -1..1 → ±12 semitones, fine around the middle. */
inline float tuneSemis(float t) { return 12.f * t * std::fabs(t); }

/** Time knobs 0..1 → 1 ms .. 10 s. */
inline float knobTime(float k) { return 0.001f * std::pow(10000.f, clampf(k, 0.f, 1.f)); }

// ---------------------------------------------------------------- the voice
struct Params {
	float outGain = 1.f;  // the preset's output level (not a knob: set when a preset loads)
	float wave1 = 0.33f, wave2 = 0.33f, tune1 = 0.f, tune2 = 0.f /* -1..1: ±12 semitones, fine at the middle */;
	float level1 = 0.8f, level2 = 0.8f, glide = 0.f;
	int ftype = LADDER24;
	float cutoff = 0.5f, res = 0.f, egAmt = 0.3f /* -1..1 */, drive = 0.f, keytrk = 0.f;
	float fA = 0.f, fD = 0.4f, fS = 0.f, fR = 0.3f;
	float aA = 0.f, aD = 0.4f, aS = 1.f, aR = 0.3f;
	float rate = 0.4f, lfoPitch = 0.f, lfoFilter = 0.f;
	bool lfoSquare = false;
	float accent = 0.5f;  // how strongly ACC acts, 0..1
	float width = 0.f;    // stereo width, 0 = mono
};

/** LFO divisions with SYNC, slow → fast, in beats (quarter notes). */
static const int NUM_DIVS = 17;
static const float DIV_BEATS[NUM_DIVS] = {16.f, 8.f, 4.f, 3.f, 2.f, 1.5f, 4.f / 3.f, 1.f, 0.75f, 2.f / 3.f,
                                          0.5f, 0.375f, 1.f / 3.f, 0.25f, 0.1875f, 1.f / 6.f, 0.125f};
static const char* const DIV_NAMES[NUM_DIVS] = {"4 bars", "2 bars", "1 bar", "1/2 dot", "1/2", "1/4 dot", "1/2 trip",
                                                "1/4", "1/8 dot", "1/4 trip", "1/8", "1/16 dot", "1/8 trip",
                                                "1/16", "1/32 dot", "1/16 trip", "1/32"};
/** RATE knob (+ RATE CV, 1 V = 2 steps) → division index. */
inline int divIndex(float rate, float rateCv) {
	int i = (int) std::round(clampf(rate, 0.f, 1.f) * (NUM_DIVS - 1) + rateCv * 2.f);
	return std::min(std::max(i, 0), NUM_DIVS - 1);
}

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
	bool acc = false;      // ACC high (taken at the start of a note)
	bool slideConnected = false, slide = false;  // SLIDE: glide only while high
	bool retrig = false;   // a trigger at RETRIG this sample: LFO starts over
	float beatSec = 0.f;   // > 0: SYNC has a clock, one beat lasts this long (RATE chooses the division)
};


/** Stereo for a bass: mid/side. The side is high-passed at 150 Hz (24 dB/oct), so the lows stay mono; WIDTH adds a
 *  slow chorus of the mid's highs to the side (opposite on both sides, the mono sum stays clean). */
struct Stereo {
	float fs = 48000.f;
	Biquad hp1, hp2, ch1, ch2;
	float lfo = 0.f;
	static const int N = 4096; // 11 ms of delay at 192 kHz fits
	float buf[N] = {};
	int w = 0;
	void setSampleRate(float sr) {
		fs = sr;
		hp1.highpass(150.f, sr, 0.5412f);
		hp2.highpass(150.f, sr, 1.3066f);
		ch1.highpass(250.f, sr, 0.5412f);
		ch2.highpass(250.f, sr, 1.3066f);
	}
	float read(float d) {
		float p = w - d;
		while (p < 0.f)
			p += N;
		int i = (int) p;
		float f = p - i;
		return buf[i % N] + (buf[(i + 1) % N] - buf[i % N]) * f;
	}
	void process(float& l, float& r, float width) {
		float m = 0.5f * (l + r);
		buf[w] = ch2.process(ch1.process(m));
		w = (w + 1) % N;
		lfo += 0.31f / fs;
		lfo -= std::floor(lfo);
		float chorus = read(fs * (0.008f + 0.003f * std::sin(2.f * PI * lfo)));
		float sd = hp2.process(hp1.process(0.5f * (l - r) + chorus * width * 0.45f));
		l = m + sd;
		r = m - sd;
	}
};

struct Voice {
	static const int OS = 2;
	float fs = 48000.f;
	Osc osc1, osc2;
	Filter filter, filterR;
	Stereo stereo;
	bool lastGate = false, accNote = false;
	Env fenv, aenv;
	Lfo lfo;
	Biquad dec1, dec2;
	float pitch = 0.f;  // V after glide
	bool started = false;
	float dc = 0.f, dcR = 0.f;
	uint32_t noise = 0x12345678u;

	void setSampleRate(float sr) {
		fs = sr;
		float f = std::min(0.45f * sr, 20000.f);
		dec1.lowpass(f, sr * OS, 0.5412f);
		dec2.lowpass(f, sr * OS, 1.3066f);
		decR1.lowpass(f, sr * OS, 0.5412f);
		decR2.lowpass(f, sr * OS, 1.3066f);
		stereo.setSampleRate(sr);
	}
	Biquad decR1, decR2;
	float mk[10] = {-9, -9, -9, -9, -9, -9, -9, -9, -9, -9}, mv[10] = {}; // knob / CV -> rate, time, ratio (cached)
	float glideT = -1.f, glideDt = -1.f, glideK = 1.f;
	int quiet = 0;
	Voice() { setSampleRate(48000.f); }

	/** One output sample, stereo (±5 V each). */
	void process(const Params& p, const Inputs& in, float& outL, float& outR) {
		float dt = 1.f / fs;
		// control rate = sample rate
		if (in.retrig)
			lfo.restart();
		// (pow / exp only when a knob or CV moved, not every sample)
		auto memo = [this](int i, float k, float (*f)(float)) {
			if (k != mk[i])
				mk[i] = k, mv[i] = f(k);
			return mv[i];
		};
		float hz = in.beatSec > 0.f ? 1.f / (DIV_BEATS[divIndex(p.rate, in.rateCv)] * in.beatSec)
		                            : memo(0, p.rate, [](float k) { return 0.05f * std::pow(1000.f, clampf(k, 0.f, 1.f)); })
		                                  * memo(1, in.rateCv, [](float k) { return std::pow(2.f, clampf(k, -5.f, 5.f)); });
		float l = lfo.process(hz, p.lfoSquare, dt);
		if (!started) {
			pitch = in.voct;
			started = true;
		}
		// accent is taken when a note starts
		if (in.gate && !lastGate)
			accNote = in.acc;
		lastGate = in.gate;
		float acc = accNote ? clampf(p.accent, 0.f, 1.f) : 0.f;
		// glide: with SLIDE patched only while SLIDE is high (GLIDE at 0 → a short 303-like slide), else always GLIDE
		float g = p.glide;
		if (in.slideConnected)
			g = in.slide ? std::max(p.glide, 0.25f) : 0.f;
		float gt = g * g * 1.5f;
		if (gt != glideT || dt != glideDt)
			glideT = gt, glideDt = dt, glideK = gt > 1e-4f ? Env::coef(gt, dt) : 1.f;
		pitch += (in.voct - pitch) * glideK;
		float fe = fenv.process(in.gate, memo(2, p.fA, knobTime), memo(3, p.fD, knobTime), p.fS, memo(4, p.fR, knobTime), dt);
		float ae = aenv.process(in.gate, memo(5, p.aA, knobTime), memo(6, p.aD, knobTime), p.aS, memo(7, p.aR, knobTime), dt);
		// silent for a while (envelope closed, no gate, the filter rang out): the oscillators and filters rest
		quiet = (ae <= 0.f && !in.gate) ? quiet + 1 : 0;
		if (quiet > (int) (0.25f * fs) && clampf(p.res + in.resCv / 10.f, 0.f, 1.f) < 0.9f) {
			dc += (0.f - dc) * (2.f * PI * 8.f * dt);
			dcR += (0.f - dcR) * (2.f * PI * 8.f * dt);
			outL = -5.f * dc;
			outR = -5.f * dcR;
			return;
		}

		float vib = p.lfoPitch * p.lfoPitch * l;  // up to ±1 octave
		float base = C4 * std::pow(2.f, pitch + vib);
		float f1 = base * memo(8, p.tune1, [](float k) { return std::pow(2.f, tuneSemis(k) / 12.f); });
		float f2 = base * memo(9, p.tune2, [](float k) { return std::pow(2.f, tuneSemis(k) / 12.f); });
		float fsOs = fs * OS;

		float oct = std::log2(20.f) + clampf(p.cutoff, 0.f, 1.f) * std::log2(1000.f)
		            + (p.egAmt + acc * 0.6f) * 7.f * fe * (0.6f + 0.4f * in.vel) + p.keytrk * pitch + in.cutoffCv
		            + p.lfoFilter * 3.f * l;
		float fc = std::pow(2.f, clampf(oct, 3.f, 15.f));
		float res = clampf(p.res + in.resCv / 10.f, 0.f, 1.f);
		float dr = clampf(p.drive + in.driveCv / 10.f, 0.f, 1.f);
		float driveGain = 1.f + 9.f * dr * dr;

		// WIDTH: VCO1 a little to the left, VCO2 to the right; the right side runs its own filter only when needed
		float wd = clampf(p.width, 0.f, 1.f);
		bool st = wd > 0.001f;
		float y = 0.f, yR = 0.f;
		float fg = Filter::coef(fc, fsOs); // (once per sample: fc does not change between the oversampled steps)
		for (int i = 0; i < OS; i++) {
			float o1 = p.level1 * osc1.process(f1 / fsOs, p.wave1), o2 = p.level2 * osc2.process(f2 / fsOs, p.wave2);
			noise = noise * 1664525u + 1013904223u;
			float n = ((int32_t) noise) * 4.6e-10f * 1e-3f;  // a breath of noise: lets the filter start to self-oscillate
			float pan = 0.35f * wd;
			float m = (1.f + pan) * o1 + (1.f - pan) * o2 + in.ext / 5.f + n;
			y = dec2.process(dec1.process(filter.processG(m * 0.7f * driveGain, fg, res, p.ftype)));
			if (st) {
				float mR = (1.f - pan) * o1 + (1.f + pan) * o2 + in.ext / 5.f - n;
				yR = decR2.process(decR1.process(filterR.processG(mR * 0.7f * driveGain, fg, res, p.ftype)));
			}
		}
		if (!st) {
			filterR = filter;  // stays in step, so turning WIDTH up does not click
			yR = y;
		}
		float gain = ae * in.vel * clampf(in.vca, 0.f, 1.f) * (1.f + 0.6f * acc) / (0.6f + 0.4f * std::sqrt(driveGain));
		y = sat(y * gain * 0.9f);
		yR = sat(yR * gain * 0.9f);
		if (st)
			stereo.process(y, yR, wd);
		dc += (y - dc) * (2.f * PI * 8.f * dt);
		dcR += (yR - dcR) * (2.f * PI * 8.f * dt);
		// the preset's level, then the safety limiter (transparent below ±6 V, ceiling ±8 V)
		outL = genra_out::limit(5.f * (y - dc) * p.outGain);
		outR = genra_out::limit(5.f * (yR - dcR) * p.outGain);
		if (!std::isfinite(outL) || !std::isfinite(outR)) {  // never let a bad state stick
			outL = outR = 0.f;
			filter.reset();
			filterR.reset();
			dc = dcR = 0.f;
		}
	}
};

}  // namespace gmono
