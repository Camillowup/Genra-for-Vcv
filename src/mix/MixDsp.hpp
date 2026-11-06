// SPDX-License-Identifier: GPL-3.0-only
// MIX – the engine of the 4-track stereo mixer: per channel low/high shelf EQ, balance, level, mute/solo (smoothed),
// a post-fader send and ducking; on the master a send/return loop, stereo width (mid/side), drive and a soft limiter
// above 8 V. Peak meters for the panel. Plain C++ without Rack, so test/test_mix.cpp can run it.
#pragma once
#include <algorithm>
#include <cmath>

namespace gmix {

static const float PI = 3.14159265358979f;
static const int NCH = 4;

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }

/** Level knobs: 0 → silent, 0.75 → unity, 1 → +5 dB (a squared curve, as on most mixers). */
inline float levelGain(float k) {
	float x = clampf(k, 0.f, 1.f) / 0.75f;
	return x * x;
}
inline float levelDb(float k) { return 20.f * std::log10(std::max(levelGain(k), 1e-6f)); }
/** EQ knobs: −1 .. 1 → ±12 dB. */
inline float eqDb(float k) { return 12.f * clampf(k, -1.f, 1.f); }
/** Duck recovery (the whole way back): 20 ms .. 1 s. */
inline float duckSeconds(float k) { return 0.02f * std::pow(50.f, clampf(k, 0.f, 1.f)); }

/** Soft limiter: linear up to 8 V, then bends towards ±10 V. */
inline float limit(float x) {
	float a = std::fabs(x);
	if (a <= 8.f)
		return x;
	float y = 8.f + 2.f * std::tanh((a - 8.f) / 2.f);
	return x < 0.f ? -y : y;
}

/** Soft saturation, tanh-like (Padé), exact ±1 at |x| = 3. */
inline float sat(float x) {
	x = clampf(x, -3.f, 3.f);
	return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

/** RBJ shelving biquad (transposed direct form II). */
struct Shelf {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
	float lastDb = 0.f;
	bool flat = true;
	void set(bool high, float f, float db, float fs) {
		lastDb = db;
		flat = std::fabs(db) < 0.01f;
		float A = std::pow(10.f, db / 40.f);
		float w = 2.f * PI * f / fs, cw = std::cos(w), sw = std::sin(w);
		float al = sw / 2.f * std::sqrt(2.f);  // shelf slope S = 1
		float sa = 2.f * std::sqrt(A) * al;
		float n0, n1, n2, d0, d1, d2;
		if (!high) {
			n0 = A * ((A + 1) - (A - 1) * cw + sa);
			n1 = 2 * A * ((A - 1) - (A + 1) * cw);
			n2 = A * ((A + 1) - (A - 1) * cw - sa);
			d0 = (A + 1) + (A - 1) * cw + sa;
			d1 = -2 * ((A - 1) + (A + 1) * cw);
			d2 = (A + 1) + (A - 1) * cw - sa;
		} else {
			n0 = A * ((A + 1) + (A - 1) * cw + sa);
			n1 = -2 * A * ((A - 1) + (A + 1) * cw);
			n2 = A * ((A + 1) + (A - 1) * cw - sa);
			d0 = (A + 1) - (A - 1) * cw + sa;
			d1 = 2 * ((A - 1) - (A + 1) * cw);
			d2 = (A + 1) - (A - 1) * cw - sa;
		}
		b0 = n0 / d0, b1 = n1 / d0, b2 = n2 / d0, a1 = d1 / d0, a2 = d2 / d0;
	}
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

/** Peak meter: instant attack, ~300 ms fall; read as segments of 12 (−36 … +3 dB). */
struct Meter {
	float v = 0.f, fall = 0.9998f;
	void setSampleRate(float fs) { fall = std::exp(-1.f / (0.3f * fs / 6.9f)); }
	void feed(float x) {
		float a = std::fabs(x);
		v = a > v ? a : v * fall;
	}
	/** Lit segments for a level where 5 V = 0 dB. */
	int segments() const {
		float db = 20.f * std::log10(std::max(v, 1e-6f) / 5.f);
		return clampf(std::floor((db + 36.f) / 39.f * 12.f + 0.5f), 0.f, 12.f);
	}
};

struct ChannelParams {
	float high = 0.f, low = 0.f;  // −1 .. 1
	float send = 0.f, duck = 0.f;
	float pan = 0.f;  // −1 .. 1 (knob + CV)
	float level = 0.75f;
	float vca = 1.f;  // LVL CV, 0..1 (1 when unplugged)
	bool mute = false, solo = false;
};

struct MasterParams {
	float duck = 0.6f, time = 0.4f, width = 0.5f, ret = 0.75f, drive = 0.f, send = 0.75f, master = 0.75f;
};

struct Channel {
	Shelf lo[2], hi[2];
	float g = 0.f;  // smoothed gain (level × mute/solo × vca)
	float pl = 1.f, pr = 1.f;
	Meter meter;
	float fs = 48000.f;
	int ctl = 0;
	void setSampleRate(float sr) {
		fs = sr;
		meter.setSampleRate(sr);
		ctl = 0;
		for (int c = 0; c < 2; c++) {
			lo[c].set(false, 120.f, lo[c].lastDb, sr);
			hi[c].set(true, 6000.f, hi[c].lastDb, sr);
		}
	}
	/** One sample: in L/R (R normalled to L by the caller), out L/R and the send (post-fader). */
	void process(const ChannelParams& p, bool audible, float duckGain, float inL, float inR, float& l, float& r,
	             float& sl, float& sr) {
		if (--ctl <= 0) {
			ctl = 32;
			float dl = eqDb(p.low), dh = eqDb(p.high);
			for (int c = 0; c < 2; c++) {
				if (std::fabs(dl - lo[c].lastDb) > 0.01f)
					lo[c].set(false, 120.f, dl, fs);
				if (std::fabs(dh - hi[c].lastDb) > 0.01f)
					hi[c].set(true, 6000.f, dh, fs);
			}
			// balance: the opposite side falls with a quarter sine, the near side stays at unity
			float pan = clampf(p.pan, -1.f, 1.f);
			pl = pan > 0.f ? std::cos(pan * PI * 0.5f) : 1.f;
			pr = pan < 0.f ? std::cos(-pan * PI * 0.5f) : 1.f;
		}
		float target = audible ? levelGain(p.level) * clampf(p.vca, 0.f, 1.f) : 0.f;
		g += (target - g) * 0.002f * (48000.f / fs);  // ~10 ms, no zipper noise or clicks on mute
		float x[2] = {inL, inR};
		for (int c = 0; c < 2; c++) {
			if (!lo[c].flat)
				x[c] = lo[c].process(x[c]);
			if (!hi[c].flat)
				x[c] = hi[c].process(x[c]);
		}
		float d = 1.f - clampf(p.duck, 0.f, 1.f) * (1.f - duckGain);
		l = x[0] * g * pl * d;
		r = x[1] * g * pr * d;
		float s = clampf(p.send, 0.f, 1.f);
		sl = l * s, sr = r * s;
		meter.feed(std::max(std::fabs(l), std::fabs(r)));
	}
};

struct Mixer {
	Channel ch[NCH];
	Meter mL, mR;
	float fs = 48000.f;
	float duckPh = 1.f, duckSm = 0.f;  // duckPh: 0 at the trigger … 1 recovered
	bool duckGate = false;

	void setSampleRate(float sr) {
		fs = sr;
		for (auto& c : ch)
			c.setSampleRate(sr);
		mL.setSampleRate(sr);
		mR.setSampleRate(sr);
	}

	/** The ducking gain (1 = no ducking) for this sample, from the DUCK input (a trigger or gate, ≥ 1 V). */
	float duck(const MasterParams& m, float duckIn) {
		bool g = duckIn >= 1.f ? true : (duckIn <= 0.1f ? false : duckGate);
		if (g && !duckGate)
			duckPh = 0.f;
		duckGate = g;
		duckPh = std::min(1.f, duckPh + 1.f / (duckSeconds(m.time) * fs));
		float env = 0.5f + 0.5f * std::cos(PI * duckPh);  // stays down, then comes back smoothly within TIME
		duckSm += (env - duckSm) * std::min(1.f, 1.f / (0.0005f * fs));  // 0.5 ms: no click on the kick
		return 1.f - clampf(m.duck, 0.f, 1.f) * 0.95f * duckSm;
	}

	/** in: 4 × L/R; ret: return L/R. Outputs L/R (limited) and send L/R. */
	void process(const ChannelParams* p, const MasterParams& m, const float (*in)[2], float duckIn, float retL,
	             float retR, float& outL, float& outR, float& sendL, float& sendR) {
		bool anySolo = false;
		for (int i = 0; i < NCH; i++)
			anySolo |= p[i].solo;
		float dg = duck(m, duckIn);
		float L = 0.f, R = 0.f, SL = 0.f, SR = 0.f;
		for (int i = 0; i < NCH; i++) {
			bool audible = !p[i].mute && (!anySolo || p[i].solo);
			float l, r, sl, sr;
			ch[i].process(p[i], audible, dg, in[i][0], in[i][1], l, r, sl, sr);
			L += l, R += r, SL += sl, SR += sr;
		}
		float sg = levelGain(m.send);
		sendL = SL * sg, sendR = SR * sg;
		float rg = levelGain(m.ret);
		L += retL * rg, R += retR * rg;
		// width: mid/side, 0.5 = as is, 0 = mono, 1 = double side
		float mid = 0.5f * (L + R), side = 0.5f * (L - R) * 2.f * clampf(m.width, 0.f, 1.f);
		L = mid + side, R = mid - side;
		float d = clampf(m.drive, 0.f, 1.f);
		if (d > 0.001f) {
			float gd = 1.f + 3.f * d;
			float comp = 1.f / std::sqrt(gd);
			L = 5.f * sat(L * gd / 5.f) * comp * (1.f + d);
			R = 5.f * sat(R * gd / 5.f) * comp * (1.f + d);
		}
		float mg = levelGain(m.master);
		outL = limit(L * mg);
		outR = limit(R * mg);
		mL.feed(outL);
		mR.feed(outR);
	}
};

}  // namespace gmix
