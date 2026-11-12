// SPDX-License-Identifier: GPL-3.0-only
// GENRA FX: the send effects for MIX (REVERB on send A, DELAY on send B). Written for GENRA, no third-party code.
//
// Both effects are wet only (a send/return), stereo, float, for any sample rate 44.1-192 kHz. All memory is taken in
// setSampleRate(); process() never allocates. Parameters are set at control rate with set(); what would click when it
// jumps (line lengths, delay time, gains) is slewed per sample inside process().
//
// REVERB: an 8-line feedback delay network. The lines are mixed by a normalised Hadamard matrix (orthogonal, so the
//   loop keeps its energy and the decay is set by the per-line gains alone, from the RT60 formula), each line has a
//   damping lowpass, the lines' read positions drift slowly (against metallic ringing), and the input runs through
//   four allpass diffusers per channel. Types HALL, ROOM, PLATE, SPACE differ in line lengths, diffusion, damping
//   and drift. SHIMMER replaces part of the loop's common component by a copy one octave up (a two-tap granular
//   shifter), FREEZE holds the loop at gain 1 with the input closed. A soft limiter inside the loop keeps any
//   setting from running away.
// DELAY: two lines with a Hermite read. The time is a division of the tempo (1/32 to 1/1, straight, dotted or
//   triplet) or free in ms; it glides to a new value (a slewed read, like turning a tape machine's speed). The loop
//   has a tilt filter (TONE), a DC blocker and a soft saturator, so FEEDBACK up to 110 % self-oscillates without
//   running away. TAPE adds wow, flutter, more saturation and a darker loop; DUCK lowers the echoes while the input
//   plays; PING-PONG feeds the left line from the input and crosses the feedback between the sides.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace gfx {

static constexpr float PI = 3.14159265358979f;

inline float flushDn(float x) { return std::fabs(x) < 1e-20f ? 0.f : x; }
inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
/** tanh-like soft clip, |x| < 3 smooth, beyond held at 1 */
inline float softSat(float x) {
	x = clampf(x, -3.f, 3.f);
	return x * (27.f + x * x) / (27.f + 9.f * x * x);
}
/** the outputs' ceiling: transparent at normal levels, never above 12 V */
inline float outLimit(float x) { return 12.f * softSat(x * (1.f / 12.f)); }
/** a one-pole coefficient for a time constant in seconds */
inline float slewCoef(float seconds, float sr) { return 1.f - std::exp(-1.f / std::max(1e-6f, seconds * sr)); }

/** A power-of-two ring buffer with fractional reads (delay in samples, >= 1 for the interpolated reads). */
struct Ring {
	std::vector<float> buf;
	int mask = 0, w = 0;
	void init(int minLen) {
		int n = 1;
		while (n < minLen)
			n <<= 1;
		buf.assign((size_t) n, 0.f);
		mask = n - 1;
		w = 0;
	}
	void clear() { std::fill(buf.begin(), buf.end(), 0.f), w = 0; }
	void write(float x) {
		buf[(size_t) w] = x;
		w = (w + 1) & mask;
	}
	float tap(int d) const { return buf[(size_t) ((w - d) & mask)]; }
	float readLinear(float d) const {
		int i = (int) d;
		float f = d - (float) i;
		float a = tap(i), b = tap(i + 1);
		return a + (b - a) * f;
	}
	float readHermite(float d) const {
		int i = (int) d;
		float f = d - (float) i;
		float xm1 = tap(i - 1), x0 = tap(i), x1 = tap(i + 1), x2 = tap(i + 2);
		float c1 = 0.5f * (x1 - xm1);
		float c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
		float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * f + c2) * f + c1) * f + x0;
	}
};

/** A Schroeder allpass (diffuser): y = -g x + d, d gets x + g y. */
struct Allpass {
	Ring r;
	int len = 1;
	float g = 0.6f;
	float process(float x) {
		float d = r.tap(len);
		float v = x + g * d;
		r.write(v);
		return d - g * v;
	}
};

struct OnePoleLP {
	float s = 0.f, a = 0.f; // y = (1-a) x + a y
	void setHz(float hz, float sr) { a = std::exp(-2.f * PI * clampf(hz, 10.f, 0.49f * sr) / sr); }
	float process(float x) {
		s = flushDn((1.f - a) * x + a * s);
		return s;
	}
};
struct OnePoleHP {
	OnePoleLP lp;
	void setHz(float hz, float sr) { lp.setHz(hz, sr); }
	float process(float x) { return x - lp.process(x); }
};

/** In-place fast Walsh-Hadamard transform of 8 values, normalised (orthogonal). */
inline void hadamard8(float* v) {
	for (int h = 1; h < 8; h <<= 1)
		for (int i = 0; i < 8; i += h << 1)
			for (int j = i; j < i + h; j++) {
				float a = v[j], b = v[j + h];
				v[j] = a + b;
				v[j + h] = a - b;
			}
	const float k = 0.35355339f; // 1/sqrt(8)
	for (int i = 0; i < 8; i++)
		v[i] *= k;
}

/** Pitch up by an octave: two read heads sweeping through a window, crossfaded with sin^2 / cos^2 (sum 1). */
struct OctaveUp {
	Ring r;
	float W = 2880.f, phase = 0.f;
	OnePoleLP lp;
	void init(float sr) {
		W = 0.06f * sr;
		r.init((int) (W * 2.f) + 8);
		phase = 0.f;
		lp.setHz(7000.f, sr);
	}
	void clear() { r.clear(), phase = 0.f, lp.s = 0.f; }
	/** 2.37, while SHIMMER is off: only the window's phase moves on (as it did when zeros were fed), nothing is read
	 * or written. silence() empties the buffer when SHIMMER goes off. */
	void idle() {
		phase += 1.f / W;
		if (phase >= 1.f)
			phase -= 1.f;
	}
	void silence() { std::fill(r.buf.begin(), r.buf.end(), 0.f), lp.s = 0.f; }
	float process(float x) {
		r.write(x);
		phase += 1.f / W; // ratio 2: the delay shrinks by one sample per sample
		if (phase >= 1.f)
			phase -= 1.f;
		float p2 = phase + 0.5f;
		if (p2 >= 1.f)
			p2 -= 1.f;
		float d1 = 1.f + W * (1.f - phase), d2 = 1.f + W * (1.f - p2);
		float s1 = std::sin(PI * phase), s2 = std::sin(PI * p2);
		return lp.process(r.readLinear(d1) * s1 * s1 + r.readLinear(d2) * s2 * s2);
	}
};

// ===================================================================================================== REVERB
enum ReverbType { HALL, ROOM, PLATE, SPACE, NUM_REVERB_TYPES };
static const char* const REVERB_TYPE_NAMES[NUM_REVERB_TYPES] = {"HALL", "ROOM", "PLATE", "SPACE"};

struct ReverbParams {
	int type = HALL;
	float size = 0.5f;       // 0..1: the line lengths x0.5..x1.5
	float decay = 2.5f;      // seconds (RT60), 0.1..30
	float predelayMs = 10.f; // 0..250
	float damp = 0.4f;       // 0..1: bright .. dark
	float width = 1.f;       // 0 mono .. 1 full .. 1.5 wider
	float shimmer = 0.f;     // 0..1
	float level = 1.f;       // output gain
	bool freeze = false;
};

struct Reverb {
	struct TypeDef {
		float ms[8];      // line lengths at size 0.5 (x1.0)
		float diffMs[4];  // input diffusers
		float diffG;      // diffusion
		float driftMs;    // read drift depth
		float dampBias;   // added to DAMP (plate brighter, space darker)
		float outGain;    // the wet level: continuous material comes back about 3 dB under its input
		float rtCorr;     // the damping shortens the broadband decay: the loop is set this much longer, so RT60 ~ DECAY
	};
	static const TypeDef& def(int t) {
		static const TypeDef D[NUM_REVERB_TYPES] = {
		    {{43.7f, 53.9f, 61.3f, 71.9f, 83.1f, 97.3f, 107.9f, 121.7f}, {4.77f, 3.59f, 12.73f, 9.31f}, 0.70f, 0.55f, 0.f, 1.8f, 1.12f},
		    {{13.1f, 16.7f, 19.3f, 23.9f, 27.1f, 31.7f, 35.3f, 41.9f}, {2.91f, 2.13f, 6.37f, 4.79f}, 0.62f, 0.22f, 0.05f, 1.6f, 1.11f},
		    {{17.3f, 23.1f, 29.9f, 34.7f, 41.3f, 47.9f, 53.9f, 61.1f}, {3.71f, 2.67f, 9.13f, 6.97f}, 0.76f, 0.32f, -0.2f, 1.25f, 1.18f},
		    {{89.3f, 107.1f, 127.9f, 149.3f, 173.1f, 199.7f, 229.3f, 263.9f}, {6.11f, 4.43f, 17.9f, 13.3f}, 0.72f, 1.2f, 0.12f, 2.3f, 1.16f}};
		return D[std::min(std::max(t, 0), NUM_REVERB_TYPES - 1)];
	}

	float sr = 48000.f;
	Ring line[8], pre[2];
	Allpass diff[2][4];
	OnePoleLP damp[8];
	OnePoleHP dc[8];
	OctaveUp shift;
	float len[8] = {}, lenT[8] = {};     // current / target lengths in samples
	float gain[8] = {}, gainT[8] = {};   // per-line loop gain
	float lfo[8] = {}, lfoInc[8] = {}, drift = 0.f;
	float driftCur = 0.f, driftT = 0.f;  // the read drift (off while frozen)
	float preT = 0.f, preCur = 0.f;
	float inGain = 1.f, inGainT = 1.f;   // closes with FREEZE
	float outLvl = 1.f, outLvlT = 1.f, widthCur = 1.f, widthT = 1.f, shim = 0.f, shimT = 0.f;
	float kSlow = 0.f, kFast = 0.f;
	ReverbParams P;
	// 2.37
	bool setValid = false;   // set() was run for P at this sample rate (it only computes again when a value changed)
	bool shimOn = false;     // the octave shifter runs (SHIMMER above 0); off: its buffer is empty
	int quiet = 0, quietMax = 1 << 30; // samples in a row with nothing in and nothing but zeros through the lines
	bool asleep = false;     // for tests and displays: the last sample was not computed

	void setSampleRate(float rate) {
		sr = rate;
		setValid = false;
		quiet = 0;
		shimOn = false;
		for (int i = 0; i < 8; i++)
			line[i].init((int) (0.47f * sr) + 64); // SPACE x1.5 plus drift
		for (int c = 0; c < 2; c++) {
			pre[c].init((int) (0.26f * sr) + 8);
			for (int k = 0; k < 4; k++)
				diff[c][k].r.init((int) (0.03f * sr) + 8);
		}
		shift.init(sr);
		// everything has been silent for sure once zeros went through the pre-delay, the diffusers, the longest line
		// and the shifter one after the other
		quietMax = (int) (line[0].buf.size() + pre[0].buf.size() + 4 * diff[0][0].r.buf.size() + shift.r.buf.size()) + 64;
		for (int i = 0; i < 8; i++) {
			dc[i].setHz(8.f, sr);
			lfoInc[i] = 2.f * PI * (0.07f + 0.061f * (float) i) / sr;
			lfo[i] = 0.7f * (float) i;
		}
		kSlow = slewCoef(0.12f, sr);
		kFast = slewCoef(0.02f, sr);
		set(P);
		for (int i = 0; i < 8; i++)
			len[i] = lenT[i], gain[i] = gainT[i];
		driftCur = driftT, preCur = preT, inGain = inGainT, outLvl = outLvlT, widthCur = widthT, shim = shimT;
	}
	void reset() {
		for (int i = 0; i < 8; i++) {
			line[i].clear();
			damp[i].s = 0.f;
			dc[i].lp.s = 0.f;
		}
		for (int c = 0; c < 2; c++) {
			pre[c].clear();
			for (int k = 0; k < 4; k++)
				diff[c][k].r.clear();
		}
		shift.clear();
		quiet = 0;
		shimOn = false;
	}
	static bool same(const ReverbParams& a, const ReverbParams& b) {
		return a.type == b.type && a.size == b.size && a.decay == b.decay && a.predelayMs == b.predelayMs && a.damp == b.damp
		       && a.width == b.width && a.shimmer == b.shimmer && a.level == b.level && a.freeze == b.freeze;
	}
	/** Control rate: computes the targets the audio rate slews to. 2.37: only when a value changed (it is called every
	 * 32 samples and ran 17 pow / exp each time); the result is the same, set() depends on p and the sample rate alone. */
	void set(const ReverbParams& p) {
		if (setValid && same(p, P))
			return;
		setValid = true;
		P = p;
		const TypeDef& D = def(p.type);
		float scale = 0.5f + clampf(p.size, 0.f, 1.f);
		float rt = clampf(p.decay, 0.1f, 30.f) * D.rtCorr;
		float dmp = clampf(p.damp + D.dampBias, 0.f, 1.f);
		float hz = 16000.f * std::pow(0.07f, dmp); // 16 kHz .. ~1.1 kHz
		for (int i = 0; i < 8; i++) {
			lenT[i] = D.ms[i] * 0.001f * sr * scale;
			gainT[i] = p.freeze ? 1.f : std::pow(10.f, -3.f * lenT[i] / (rt * sr));
			damp[i].setHz(p.freeze ? 0.49f * sr : hz, sr);
		}
		for (int c = 0; c < 2; c++)
			for (int k = 0; k < 4; k++) {
				diff[c][k].len = std::max(1, (int) (D.diffMs[k] * 0.001f * sr * (c ? 1.07f : 1.f) * (0.7f + 0.3f * scale)));
				diff[c][k].g = D.diffG;
			}
		drift = D.driftMs * 0.001f * sr;
		driftT = p.freeze ? 0.f : 1.f;
		for (int i = 0; i < 8; i++)
			dc[i].setHz(p.freeze ? 0.5f : 8.f, sr);
		preT = std::max(1.f, clampf(p.predelayMs, 0.f, 250.f) * 0.001f * sr);
		inGainT = p.freeze ? 0.f : 1.f;
		outLvlT = clampf(p.level, 0.f, 2.f) * D.outGain;
		widthT = clampf(p.width, 0.f, 1.5f);
		shimT = p.freeze ? 0.f : clampf(p.shimmer, 0.f, 1.f) * 0.5f;
	}
	/** The reverb rests (2.37): nothing comes in and only exact zeros have gone through every delay for longer than
	 * they all hold together, and no filter holds anything. Then the output is zero whatever is computed, so only the
	 * things that move on their own are kept moving (the slews towards the knobs and the drift's phases): the first
	 * sample that comes in finds the reverb exactly as if it had been computed all along. */
	bool resting(float inL, float inR) {
		if (inL != 0.f || inR != 0.f) {
			quiet = 0;
			return false;
		}
		if (quiet <= quietMax)
			return false;
		for (int i = 0; i < 8; i++)
			if (damp[i].s != 0.f || dc[i].lp.s != 0.f)
				return false;
		return shift.lp.s == 0.f;
	}
	void process(float inL, float inR, float& outL, float& outR) {
		inGain += (inGainT - inGain) * kFast;
		preCur += (preT - preCur) * kSlow;
		outLvl += (outLvlT - outLvl) * kFast;
		widthCur += (widthT - widthCur) * kFast;
		shim += (shimT - shim) * kSlow;
		driftCur += (driftT - driftCur) * kSlow;
		asleep = resting(inL, inR);
		if (asleep) {
			for (int i = 0; i < 8; i++) {
				len[i] += (lenT[i] - len[i]) * kSlow;
				gain[i] += (gainT[i] - gain[i]) * kSlow;
				lfo[i] += lfoInc[i];
				if (lfo[i] > 2.f * PI)
					lfo[i] -= 2.f * PI;
			}
			shift.idle();
			outL = outR = 0.f;
			return;
		}
		// pre-delay, then the diffusers
		pre[0].write(inL * inGain);
		pre[1].write(inR * inGain);
		float a = pre[0].readLinear(preCur), b = pre[1].readLinear(preCur);
		for (int k = 0; k < 4; k++) {
			a = diff[0][k].process(a);
			b = diff[1][k].process(b);
		}
		// read the lines (drifting), damp, mix
		float v[8];
		for (int i = 0; i < 8; i++) {
			len[i] += (lenT[i] - len[i]) * kSlow;
			gain[i] += (gainT[i] - gain[i]) * kSlow;
			lfo[i] += lfoInc[i];
			if (lfo[i] > 2.f * PI)
				lfo[i] -= 2.f * PI;
			float d = len[i] + 2.f + drift * driftCur * (1.f + std::sin(lfo[i]));
			v[i] = damp[i].process(line[i].readHermite(d));
		}
		// the output taps before the mix: two orthogonal sign patterns
		float oL = v[0] - v[1] + v[2] + v[3] - v[4] + v[5] - v[6] + v[7];
		float oR = v[0] + v[1] - v[2] + v[3] + v[4] - v[5] - v[6] + v[7];
		hadamard8(v);
		bool zeros = inL == 0.f && inR == 0.f && a == 0.f && b == 0.f; // (for resting)
		if (shim > 1e-4f) { // replace part of the common component by a copy an octave up
			shimOn = true;
			float m = 0.f;
			for (int i = 0; i < 8; i++)
				m += v[i];
			m *= 0.125f;
			float up = shift.process(m);
			float dlt = shim * (up - m);
			for (int i = 0; i < 8; i++)
				v[i] += dlt;
			zeros = zeros && m == 0.f && up == 0.f;
		}
		else {
			// 2.37: with SHIMMER off the shifter rests (it was fed zeros: four samples read and two sin for nothing).
			// Its buffer is emptied once, so it starts from silence when SHIMMER comes on again.
			if (shimOn) {
				shimOn = false;
				shift.silence();
			}
			shift.idle();
		}
		for (int i = 0; i < 8; i++) {
			float x = dc[i].process(v[i] * gain[i]);
			// the inputs: left into the even lines, right into the odd ones, signs alternating
			x += ((i & 1) ? b : a) * ((i & 2) ? -0.5f : 0.5f);
			// safety: soft above ~8 V, transparent below
			if (std::fabs(x) > 8.f)
				x = 8.f * softSat(x * 0.125f) * 1.2f;
			x = flushDn(x);
			zeros = zeros && x == 0.f && v[i] == 0.f;
			line[i].write(x);
		}
		quiet = zeros ? std::min(quiet + 1, 1 << 30) : 0;
		oL *= 0.35f, oR *= 0.35f;
		float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * widthCur;
		outL = outLimit((mid + side) * outLvl);
		outR = outLimit((mid - side) * outLvl);
	}
};

// ===================================================================================================== DELAY
enum DivMode { STRAIGHT, DOTTED, TRIPLET };
static const int NUM_DIVS = 6;
static const char* const DIV_NAMES[NUM_DIVS] = {"1/32", "1/16", "1/8", "1/4", "1/2", "1/1"};
static const float DIV_BEATS[NUM_DIVS] = {0.125f, 0.25f, 0.5f, 1.f, 2.f, 4.f}; // in quarter notes

struct DelayParams {
	bool sync = true;
	int div = 2;          // index into DIV_NAMES
	int mode = STRAIGHT;
	float bpm = 120.f;
	float ms = 375.f;     // free time when !sync
	float feedback = 0.4f; // 0..1.1
	float tone = 0.f;     // -1 dark .. 0 flat .. +1 thin
	float tape = 0.f;     // 0..1
	float duck = 0.f;     // 0..1
	float width = 1.f;    // 0..1.5
	bool pingpong = false;
	float level = 1.f;
};

inline float delaySeconds(const DelayParams& p) {
	if (!p.sync)
		return clampf(p.ms, 1.f, 5000.f) * 0.001f;
	float beats = DIV_BEATS[std::min(std::max(p.div, 0), NUM_DIVS - 1)];
	if (p.mode == DOTTED)
		beats *= 1.5f;
	else if (p.mode == TRIPLET)
		beats *= 2.f / 3.f;
	return beats * 60.f / clampf(p.bpm, 20.f, 400.f);
}

struct Delay {
	static constexpr float MAX_S = 5.f;
	float sr = 48000.f;
	Ring line[2];
	OnePoleLP lp[2], tapeLp[2];
	OnePoleHP hp[2], dcb[2];
	float cur = 1.f, target = 1.f, kTime = 0.f, kFast = 0.f;
	float fb = 0.f, fbT = 0.f, lvl = 1.f, lvlT = 1.f, wid = 1.f, widT = 1.f;
	float env = 0.f, envA = 0.f, envR = 0.f, duckG = 1.f;
	float wow = 0.f, flut = 0.f, wowInc = 0.f, flutInc = 0.f;
	float drive = 1.f, driveT = 1.f;
	DelayParams P;
	// 2.37 (as the reverb's)
	bool setValid = false;
	int quiet = 0, quietMax = 1 << 30;
	bool asleep = false;

	void setSampleRate(float rate) {
		sr = rate;
		setValid = false;
		quiet = 0;
		for (int c = 0; c < 2; c++) {
			line[c].init((int) (MAX_S * sr) + 16);
			dcb[c].setHz(20.f, sr);
		}
		quietMax = (int) line[0].buf.size() + 64;
		kTime = slewCoef(0.09f, sr);
		kFast = slewCoef(0.02f, sr);
		envA = slewCoef(0.005f, sr);
		envR = slewCoef(0.25f, sr);
		wowInc = 2.f * PI * 0.55f / sr;
		flutInc = 2.f * PI * 6.3f / sr;
		set(P);
		cur = target, fb = fbT, lvl = lvlT, wid = widT, drive = driveT;
	}
	void reset() {
		for (int c = 0; c < 2; c++) {
			line[c].clear();
			lp[c].s = tapeLp[c].s = hp[c].lp.s = dcb[c].lp.s = 0.f;
		}
		env = 0.f, duckG = 1.f, cur = target;
		quiet = 0;
	}
	static bool same(const DelayParams& a, const DelayParams& b) {
		return a.sync == b.sync && a.div == b.div && a.mode == b.mode && a.bpm == b.bpm && a.ms == b.ms
		       && a.feedback == b.feedback && a.tone == b.tone && a.tape == b.tape && a.duck == b.duck && a.width == b.width
		       && a.pingpong == b.pingpong && a.level == b.level;
	}
	/** 2.37: only when a value changed (called every 32 samples: 3 pow, 6 exp each time; same result). */
	void set(const DelayParams& p) {
		if (setValid && same(p, P))
			return;
		setValid = true;
		P = p;
		target = clampf(delaySeconds(p) * sr, 4.f, MAX_S * sr - 8.f);
		fbT = clampf(p.feedback, 0.f, 1.1f);
		float t = clampf(p.tone, -1.f, 1.f);
		float tape = clampf(p.tape, 0.f, 1.f);
		// TONE: below 0 a lowpass closes from 18 kHz to 900 Hz, above 0 a highpass opens from 25 Hz to 1.8 kHz
		float lpHz = t < 0.f ? 18000.f * std::pow(0.05f, -t) : 18000.f;
		lpHz = std::min(lpHz, 18000.f * std::pow(0.25f, tape)); // tape darkens the loop
		float hpHz = t > 0.f ? 25.f * std::pow(72.f, t) : 25.f;
		for (int c = 0; c < 2; c++) {
			lp[c].setHz(lpHz, sr);
			hp[c].setHz(hpHz, sr);
			tapeLp[c].setHz(9000.f, sr);
		}
		driveT = 1.f + 1.5f * tape;
		lvlT = clampf(p.level, 0.f, 2.f);
		widT = clampf(p.width, 0.f, 1.5f);
	}
	/** the loop's saturator: transparent at normal levels, holds self-oscillation near 5 V */
	float sat(float x) const { return 5.f * softSat(x * drive * 0.2f) / drive; }

	void process(float inL, float inR, float& outL, float& outR) {
		cur += (target - cur) * kTime;
		fb += (fbT - fb) * kFast;
		lvl += (lvlT - lvl) * kFast;
		wid += (widT - wid) * kFast;
		drive += (driveT - drive) * kFast;
		// tape: wow and flutter move the read position
		float tape = clampf(P.tape, 0.f, 1.f);
		wow += wowInc;
		flut += flutInc;
		if (wow > 2.f * PI)
			wow -= 2.f * PI;
		if (flut > 2.f * PI)
			flut -= 2.f * PI;
		// 2.37: the delay rests while nothing comes in and the whole line and every filter hold exact zeros (see the
		// reverb): the output is zero whatever is computed; the slews above and the duck's gain move on as they would
		if (inL != 0.f || inR != 0.f)
			quiet = 0;
		asleep = quiet > quietMax && env == 0.f;
		for (int c = 0; c < 2 && asleep; c++)
			asleep = lp[c].s == 0.f && hp[c].lp.s == 0.f && dcb[c].lp.s == 0.f
			         && (tape > 0.f ? tapeLp[c].s == 0.f : true); // (at TAPE 0 its low-pass is not in the loop)
		if (asleep) {
			duckG += (1.f - duckG) * kFast;
			outL = outR = 0.f;
			return;
		}
		// (2.37: at TAPE 0 the two sin are not computed; mod is 0 either way)
		float mod = tape > 0.f ? tape * sr * (0.0021f * std::sin(wow) + 0.00022f * std::sin(flut)) : 0.f;
		float dL = clampf(cur + mod, 2.f, MAX_S * sr - 4.f);
		float dR = clampf(cur + mod * 0.93f, 2.f, MAX_S * sr - 4.f);
		float yL = line[0].readHermite(dL), yR = line[1].readHermite(dR);
		// the loop: tone, DC block, saturation
		auto loop = [&](int c, float y) {
			float z = hp[c].process(lp[c].process(y));
			if (tape > 0.f)
				z += (tapeLp[c].process(z) - z) * tape * 0.5f;
			return sat(dcb[c].process(z)) * fb;
		};
		float fL = loop(0, yL), fR = loop(1, yR);
		float wL, wR;
		if (P.pingpong) {
			wL = flushDn(0.5f * (inL + inR) + fR);
			wR = flushDn(fL);
		}
		else {
			wL = flushDn(inL + fL);
			wR = flushDn(inR + fR);
		}
		line[0].write(wL);
		line[1].write(wR);
		bool zeros = inL == 0.f && inR == 0.f && wL == 0.f && wR == 0.f && yL == 0.f && yR == 0.f;
		quiet = zeros ? std::min(quiet + 1, 1 << 30) : 0;
		// DUCK: the echoes step back while the input plays
		float e = 0.5f * (std::fabs(inL) + std::fabs(inR));
		env += (e - env) * (e > env ? envA : envR);
		env = flushDn(env);
		float dg = 1.f - clampf(P.duck, 0.f, 1.f) * clampf(env * 0.6f, 0.f, 0.9f);
		duckG += (dg - duckG) * kFast;
		float g = lvl * duckG;
		float mid = 0.5f * (yL + yR), side = 0.5f * (yL - yR) * wid;
		outL = outLimit((mid + side) * g);
		outR = outLimit((mid - side) * g);
	}
};

} // namespace gfx
