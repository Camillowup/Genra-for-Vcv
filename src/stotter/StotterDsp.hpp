// SPDX-License-Identifier: GPL-3.0-only
// STOTTER – the performance FX engine: a DJ-isolator EQ, a ring buffer that always records the last bars, twelve
// pad effects (stutter, halftime, reverse, tape stop, scratch, gate, trance gate, pump, echo out, freeze, bitcrush,
// rearrange) played as one-bar patterns in the manner of Flex Beat, a DJ filter, drive and a dry/wet mix.
// Plain C++ without Rack, so test/test_stotter.cpp can run it.
//
// Signal: IN (or the captured loop) → GAIN → [EQ, if PRE] → ring buffer → pad effect → [EQ, if not PRE] → filter
// → drive → MIX with the dry input.
// Time: an internal clock (120 BPM or TAP) or the CLOCK input (16tel/8tel/Viertel/24 PPQN, as GENRA). Positions are
// in beats; a bar has 4 beats.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace gstot {

static const float PI = 3.14159265358979f;
static const int NPADS = 16, NBANKS = 4;
static const float MAX_SECONDS = 10.f;  // ring buffer: two bars at 48 BPM

inline float clampf(float x, float a, float b) { return std::min(std::max(x, a), b); }
inline float dbToGain(float db) { return std::pow(10.f, db / 20.f); }
/** Leaves ±7 V alone, bends anything louder softly towards ±10 V (resonance, stacked effects). */
inline float limit(float x) {
	float a = std::fabs(x);
	if (a <= 7.f) return x;
	return std::copysign(7.f + 3.f * std::tanh((a - 7.f) / 3.f), x);
}
/** Soft saturation, tanh-like (Padé), exact ±1 at |x| = 3. */
inline float sat(float x) {
	x = clampf(x, -3.f, 3.f);
	return x * (27.f + x * x) / (27.f + 9.f * x * x);
}

// ---------------------------------------------------------------- the effects and their three parameters
enum Fx { STUTTER, HALFTIME, REVERSE, TAPESTOP, SCRATCH, GATE, TRANCE, PUMP, ECHO, FREEZE, CRUSH, REARRANGE, NUM_FX };
static const char* const FX_NAMES[NUM_FX] = {"STUTTER", "HALFTIME", "REVERSE", "TAPE STOP", "SCRATCH", "GATE",
                                             "TRANCE",  "PUMP",     "ECHO OUT", "FREEZE",   "BITCRUSH", "REARRANGE"};
/** The row (and so the colour) an effect belongs to: 0 stutter, 1 time, 2 gate, 3 fx. */
static const int FX_ROW[NUM_FX] = {0, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3};
static const char* const ROW_NAMES[4] = {"STUTTER", "TIME", "GATE", "FX"};
static const char* const PARAM_NAMES[NUM_FX][3] = {
    {"LENGTH", "DECAY", "PITCH"},  {"SPEED", "SMOOTH", "MIX"},  {"LENGTH", "SMOOTH", "MIX"},
    {"TIME", "CURVE", "RESTART"},  {"RATE", "DEPTH", "MOVE"},   {"RATE", "DEPTH", "EDGE"},
    {"PATTERN", "RATE", "DEPTH"},  {"RATE", "DEPTH", "RELEASE"}, {"TIME", "FEEDBACK", "HALL"},
    {"SIZE", "SPREAD", "MIX"},     {"BITS", "RATE", "MIX"},     {"GRID", "RANDOM", "REPEAT"}};

/** Note values (in beats), longest first. */
static const int NDIV = 10;
static const float DIVS[NDIV] = {2.f, 1.f, 2.f / 3.f, 0.5f, 1.f / 3.f, 0.25f, 1.f / 6.f, 0.125f, 1.f / 12.f, 0.0625f};
static const char* const DIV_NAMES[NDIV] = {"1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T", "1/64"};
inline int pick(float v, int n) { return std::min(n - 1, std::max(0, (int) std::lround(clampf(v, 0.f, 1.f) * (n - 1)))); }
inline float valueOf(int i, int n) { return n > 1 ? (float) i / (n - 1) : 0.f; }

static const float REV_BEATS[4] = {4.f, 2.f, 1.f, 0.5f};
static const char* const REV_NAMES[4] = {"1 BAR", "1/2", "1/4", "1/8"};
static const float SCR_BEATS[3] = {1.f, 0.5f, 0.25f};
static const char* const SCR_NAMES[3] = {"1/4", "1/8", "1/16"};
static const char* const SCR_MOVES[3] = {"BABY", "FORWARD", "CHIRP"};
static const float PUMP_BEATS[3] = {2.f, 1.f, 0.5f};
static const char* const PUMP_NAMES[3] = {"1/2", "1/4", "1/8"};
static const float ECHO_BEATS[5] = {0.5f, 0.75f, 1.f, 1.5f, 2.f};
static const char* const ECHO_NAMES[5] = {"1/8", "3/16", "1/4", "3/8", "1/2"};
static const float REARR_BEATS[2] = {0.5f, 0.25f};
static const char* const REARR_NAMES[2] = {"1/8", "1/16"};
static const int NTRANCE = 8;
static const uint16_t TRANCE_PAT[NTRANCE] = {0xAAAA, 0xDDDD, 0xB6DB, 0xEEEE, 0xA5A5, 0xCCCC, 0xF5AF, 0x9249};

struct PadCfg {
	int fx = STUTTER;
	float p[3] = {0.5f, 0.f, 0.5f};
};

/** The factory set of a bank: the panel's four rows. */
inline PadCfg defaultPad(int pad) {
	PadCfg c;
	static const int FXS[NPADS] = {STUTTER, STUTTER, STUTTER, STUTTER, HALFTIME, REVERSE, TAPESTOP, SCRATCH,
	                               GATE,    GATE,    TRANCE,  PUMP,    ECHO,     FREEZE,  CRUSH,    REARRANGE};
	c.fx = FXS[pad % NPADS];
	switch (pad % NPADS) {
		case 0: c.p[0] = valueOf(1, NDIV); c.p[1] = 0.f; c.p[2] = 0.5f; break;         // 1/4
		case 1: c.p[0] = valueOf(3, NDIV); c.p[1] = 0.f; c.p[2] = 0.5f; break;         // 1/8
		case 2: c.p[0] = valueOf(5, NDIV); c.p[1] = 0.2f; c.p[2] = 0.5f; break;        // 1/16
		case 3: c.p[0] = valueOf(7, NDIV); c.p[1] = 0.3f; c.p[2] = 0.75f; break;       // 1/32, rising
		case 4: c.p[0] = 0.f; c.p[1] = 0.3f; c.p[2] = 1.f; break;                      // half speed
		case 5: c.p[0] = valueOf(2, 4); c.p[1] = 0.3f; c.p[2] = 1.f; break;            // 1/4
		case 6: c.p[0] = 0.5f; c.p[1] = 0.4f; c.p[2] = 0.f; break;
		case 7: c.p[0] = valueOf(1, 3); c.p[1] = 0.4f; c.p[2] = 0.f; break;
		case 8: c.p[0] = valueOf(3, NDIV); c.p[1] = 1.f; c.p[2] = 0.2f; break;         // 1/8
		case 9: c.p[0] = valueOf(5, NDIV); c.p[1] = 1.f; c.p[2] = 0.2f; break;         // 1/16
		case 10: c.p[0] = 0.f; c.p[1] = 0.f; c.p[2] = 1.f; break;
		case 11: c.p[0] = valueOf(1, 3); c.p[1] = 0.8f; c.p[2] = 0.5f; break;
		case 12: c.p[0] = valueOf(1, 5); c.p[1] = 0.6f; c.p[2] = 0.3f; break;
		case 13: c.p[0] = 0.4f; c.p[1] = 0.3f; c.p[2] = 1.f; break;
		case 14: c.p[0] = 0.55f; c.p[1] = 0.5f; c.p[2] = 1.f; break;
		default: c.p[0] = 1.f; c.p[1] = 0.5f; c.p[2] = 0.2f; break;
	}
	return c;
}

// the parameter mappings, shared by the engine and the display
inline float stutterPitch(float v) { return (v - 0.5f) * 2.f; }                    // semitones per repeat, ±1
inline float smoothMs(float v) { return 1.f + v * 29.f; }                          // 1 … 30 ms
inline float tapeBeats(float v) { return 0.25f * std::pow(16.f, v); }              // 1/16 bar … 1 bar … 4 bars
inline float tapeCurve(float v) { return 0.3f * std::pow(10.f, v); }               // 0.3 … 3
inline float scratchMs(float v) { return 10.f + v * 190.f; }                       // 10 … 200 ms
inline float edgeMs(float v) { return 0.5f + v * v * 60.f; }                       // 0.5 … 60 ms
inline float pumpRelease(float v) { return 0.08f + v * 0.6f; }                      // share of the period
inline float echoFeedback(float v) { return v * 0.92f; }
inline float freezeMs(float v) { return 20.f * std::pow(25.f, v); }                // 20 … 500 ms
inline float crushBits(float v) { return 16.f - v * 15.f; }                         // 16 … 1
inline float crushRate(float v) { return 48000.f * std::pow(1.f / 96.f, v); }      // 48 kHz … 500 Hz

/** The value of parameter k of an effect as text, for the display and the tooltips. */
inline std::string paramText(int fx, int k, float v) {
	char b[32];
	auto pct = [&](float x) { snprintf(b, sizeof b, "%d %%", (int) std::lround(x * 100.f)); return std::string(b); };
	switch (fx) {
		case STUTTER:
			if (k == 0) return DIV_NAMES[pick(v, NDIV)];
			if (k == 1) return pct(v);
			snprintf(b, sizeof b, "%+.1f ST", stutterPitch(v));
			return b;
		case HALFTIME:
			if (k == 0) return v < 0.5f ? "1/2" : "1/4";
			if (k == 1) { snprintf(b, sizeof b, "%.0f MS", smoothMs(v)); return b; }
			return pct(v);
		case REVERSE:
			if (k == 0) return REV_NAMES[pick(v, 4)];
			if (k == 1) { snprintf(b, sizeof b, "%.0f MS", smoothMs(v)); return b; }
			return pct(v);
		case TAPESTOP:
			if (k == 0) { snprintf(b, sizeof b, "%.2f BEAT", tapeBeats(v)); return b; }
			if (k == 1) { snprintf(b, sizeof b, "%.1f", tapeCurve(v)); return b; }
			return v < 0.5f ? "OFF" : "ON";
		case SCRATCH:
			if (k == 0) return SCR_NAMES[pick(v, 3)];
			if (k == 1) { snprintf(b, sizeof b, "%.0f MS", scratchMs(v)); return b; }
			return SCR_MOVES[pick(v, 3)];
		case GATE:
			if (k == 0) return DIV_NAMES[pick(v, NDIV)];
			if (k == 1) return pct(v);
			snprintf(b, sizeof b, "%.1f MS", edgeMs(v));
			return b;
		case TRANCE:
			if (k == 0) { snprintf(b, sizeof b, "%d", pick(v, NTRANCE) + 1); return b; }
			if (k == 1) return v < 0.5f ? "1/16" : "1/32";
			return pct(v);
		case PUMP:
			if (k == 0) return PUMP_NAMES[pick(v, 3)];
			if (k == 1) return pct(v);
			return pct(pumpRelease(v));
		case ECHO:
			if (k == 0) return ECHO_NAMES[pick(v, 5)];
			if (k == 1) return pct(echoFeedback(v));
			return pct(v);
		case FREEZE:
			if (k == 0) { snprintf(b, sizeof b, "%.0f MS", freezeMs(v)); return b; }
			return pct(v);
		case CRUSH:
			if (k == 0) { snprintf(b, sizeof b, "%.0f BIT", std::round(crushBits(v))); return b; }
			if (k == 1) {
				float r = crushRate(v);
				if (r >= 1000.f) snprintf(b, sizeof b, "%.1f KHZ", r / 1000.f);
				else snprintf(b, sizeof b, "%.0f HZ", r);
				return b;
			}
			return pct(v);
		default:
			if (k == 0) return REARR_NAMES[pick(v, 2)];
			return pct(v);
	}
}

/** A short name of a pad for its face: the effect and its main setting. */
inline std::string padLabel(const PadCfg& c) {
	switch (c.fx) {
		case STUTTER: case GATE: return DIV_NAMES[pick(c.p[0], NDIV)];
		case HALFTIME: return c.p[0] < 0.5f ? "HALF" : "QUARTER";
		case REVERSE: return "REVERSE";
		case TAPESTOP: return "TAPE";
		case SCRATCH: return "SCRATCH";
		case TRANCE: return "TRANCE";
		case PUMP: return "PUMP";
		case ECHO: return "ECHO";
		case FREEZE: return "FREEZE";
		case CRUSH: return "CRUSH";
		default: return "REARR";
	}
}

inline uint32_t hash32(uint32_t x) {
	x ^= x >> 16; x *= 0x7feb352d; x ^= x >> 15; x *= 0x846ca68b; x ^= x >> 16;
	return x;
}

/** The pattern of an effect over one bar for the display: x 0…1 (the bar), result 0…1. Read positions for the
 * buffer effects, the gain for the gates. */
inline float displayCurve(const PadCfg& c, float x) {
	float b = 4.f * x;  // beats
	auto frac = [](float v) { return v - std::floor(v); };
	const float* p = c.p;
	switch (c.fx) {
		case STUTTER: return frac(b / DIVS[pick(p[0], NDIV)]);
		case HALFTIME: return x * (p[0] < 0.5f ? 1.f : 0.5f);
		case REVERSE: return 1.f - frac(b / REV_BEATS[pick(p[0], 4)]);
		case TAPESTOP: {
			float q = b / tapeBeats(p[0]);
			return q < 1.f ? std::pow(1.f - q, tapeCurve(p[1])) : 0.f;
		}
		case SCRATCH: {
			float ph = frac(b / SCR_BEATS[pick(p[0], 3)]);
			return 0.5f - 0.5f * std::cos(2.f * PI * ph);
		}
		case GATE: {
			float ph = frac(b / DIVS[pick(p[0], NDIV)]);
			return ph < 0.5f ? 1.f : 1.f - p[1];
		}
		case TRANCE: {
			int st = (int) (b / (p[1] < 0.5f ? 0.25f : 0.125f)) & 15;
			return (TRANCE_PAT[pick(p[0], NTRANCE)] >> (15 - st)) & 1 ? 1.f : 1.f - p[2];
		}
		case PUMP: {
			float ph = frac(b / PUMP_BEATS[pick(p[0], 3)]);
			return 1.f - p[1] * std::exp(-ph / (pumpRelease(p[2]) * 0.4f));
		}
		case ECHO: {
			float d = ECHO_BEATS[pick(p[0], 5)];
			float k = std::floor(b / d);
			return frac(b / d) < 0.1f ? std::pow(std::max(echoFeedback(p[1]), 0.05f), k) : 0.f;
		}
		case FREEZE: return 0.5f - 0.5f * std::cos(2.f * PI * frac(8.f * x));
		case CRUSH: {
			float lv = std::min(std::pow(2.f, std::round(crushBits(p[0])) - 1.f), 6.f);
			return 0.5f + 0.5f * std::round(std::sin(2.f * PI * x * 2.f) * lv) / lv;
		}
		default: {
			float g = REARR_BEATS[pick(p[0], 2)];
			int n = (int) std::lround(4.f / g), k = (int) (b / g);
			int s = (hash32(k * 7919u + 13u) % 100) < (uint32_t) (p[1] * 100.f) ? (int) (hash32(k + 77u) % n) : k;
			return (s + frac(b / g)) / n;
		}
	}
}

// ---------------------------------------------------------------- building blocks
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	float z1 = 0, z2 = 0;
	float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
	void set(int type, float f, float q, float fs) {  // 0 LP, 1 HP, 2 AP (RBJ)
		float w = 2.f * PI * clampf(f, 5.f, fs * 0.45f) / fs;
		float cw = std::cos(w), al = std::sin(w) / (2.f * q);
		float a0 = 1.f + al;
		if (type == 0) { b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = b0; }
		else if (type == 1) { b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = b0; }
		else { b0 = 1 - al; b1 = -2 * cw; b2 = 1 + al; }
		b0 /= a0; b1 /= a0; b2 /= a0;
		a1 = -2 * cw / a0;
		a2 = (1 - al) / a0;
	}
};

/** DJ isolator: three bands split by Linkwitz-Riley crossovers (24 dB/oct), the sum flat. */
struct Isolator {
	Biquad lo[2], rest[2], mid[2], hi[2], ap;  // pairs of Butterworth sections; ap: the low band's phase at f2
	float f1 = -1, f2 = -1;
	void set(float a, float b, float fs) {
		if (std::fabs(a - f1) < 0.5f && std::fabs(b - f2) < 0.5f) return;
		f1 = a; f2 = b;
		const float Q = 0.70710678f;
		for (int i = 0; i < 2; i++) {
			// keep the filter state, only the coefficients change
			Biquad* fs_[4] = {&lo[i], &rest[i], &mid[i], &hi[i]};
			int ty[4] = {0, 1, 0, 1};
			float fr[4] = {a, a, b, b};
			for (int k = 0; k < 4; k++) {
				float z1 = fs_[k]->z1, z2 = fs_[k]->z2;
				fs_[k]->set(ty[k], fr[k], Q, fs);
				fs_[k]->z1 = z1; fs_[k]->z2 = z2;
			}
		}
		float z1 = ap.z1, z2 = ap.z2;
		ap.set(2, b, Q, fs);
		ap.z1 = z1; ap.z2 = z2;
	}
	float process(float x, float gl, float gm, float gh) {
		float l = ap.process(lo[1].process(lo[0].process(x)));
		float r = rest[1].process(rest[0].process(x));
		float m = mid[1].process(mid[0].process(r));
		float h = hi[1].process(hi[0].process(r));
		return l * gl + m * gm + h * gh;
	}
};

/** Topology-preserving state-variable filter (Simper), LP and HP outputs. */
struct Svf {
	float ic1 = 0, ic2 = 0;
	void process(float x, float g, float k, float& lp, float& hp) {
		float a1 = 1.f / (1.f + g * (g + k));
		float a2 = g * a1, a3 = g * a2;
		float v3 = x - ic2;
		float v1 = a1 * ic1 + a2 * v3;
		float v2 = ic2 + a2 * ic1 + a3 * v3;
		ic1 = 2.f * v1 - ic1;
		ic2 = 2.f * v2 - ic2;
		lp = v2;
		hp = x - k * v1 - v2;
	}
};

struct Allpass {
	std::vector<float> d;
	int i = 0;
	float g = 0.6f;
	void init(int n) { d.assign(std::max(n, 1), 0.f); i = 0; }
	float process(float x) {
		float v = d[i];
		float y = -g * x + v;
		float s = x + g * y;
		d[i] = std::fabs(s) < 1e-12f ? 0.f : s;
		if (++i >= (int) d.size()) i = 0;
		return y;
	}
};

// ---------------------------------------------------------------- the engine
struct Controls {
	float gain = 0.f;                      // −1…1: ±12 dB
	float high = 0.f, mid = 0.f, low = 0.f; // −1…1: −∞ … 0 dB … +6 dB
	float freq = 0.5f;                     // mid band centre 200 Hz … 5 kHz
	bool killLo = false, killMid = false, killHi = false;
	bool pre = true;                       // EQ before the effects
	float cut = 0.f;                       // −1 LP … 0 off … +1 HP
	float reso = 0.2f, drive = 0.f;
	float intensity = 0.5f;                // 0.5: the pad as set; below: less wet, above: harder
	int quant = 3;                         // OFF, 1/16, 1/8, 1/4, 1/2, 1 BAR
	float mix = 1.f;
	int mode = 0;                          // 0 LOOP, 1 ONE SHOT, 2 HOLD
	bool latch = false;
	int clockMode = 0;                     // as GENRA: 16tel, 8tel, Viertel, 24 PPQN
};
static const int NQUANT = 6;
static const float QUANT_BEATS[NQUANT] = {0.f, 0.25f, 0.5f, 1.f, 2.f, 4.f};
static const char* const QUANT_NAMES[NQUANT] = {"OFF", "1/16", "1/8", "1/4", "1/2", "1 BAR"};
static const char* const MODE_NAMES[3] = {"LOOP", "ONE SHOT", "HOLD"};
static const int PPB[4] = {4, 2, 1, 24};

/** EQ knob (−1…1) to gain: −1 is a kill, 0 unity, +1 +6 dB. */
inline float eqGain(float v) {
	if (v >= 0.f) return dbToGain(6.f * v);
	if (v <= -0.97f) return 0.f;
	return dbToGain(40.f * v);
}
inline float midFreq(float v) { return 200.f * std::pow(25.f, clampf(v, 0.f, 1.f)); }

struct Engine {
	float fs = 48000.f;
	Controls c;
	PadCfg pads[NBANKS * NPADS];
	int bank = 0;

	// time
	double beat = 0.0;        // beats since reset
	double spb = 24000.0;     // samples per beat
	float bpm = 120.f;
	bool ext = false;         // following the CLOCK input
	long clockCount = -1;
	int64_t lastPulse = -1, sinceClock = 1 << 30;
	int64_t taps[4] = {0, 0, 0, 0};
	int ntaps = 0;
	int64_t lastTap = -1;

	// the ring buffer (absolute sample indices; the latest written is W)
	std::vector<float> bl, br;
	int64_t W = -1;
	int64_t bufN = 1;
	int64_t Wk = 0;           // W % bufN, kept up to date (no 64-bit % per sample)

	// the pad state
	int act = -1;             // slot (bank * 16 + pad) playing, −1: none
	int actPad = -1;          // its pad 0…15 (for the lights)
	bool actOne = false;      // a one-shot
	int want = -2;            // slot to switch to (−1: stop, −2: nothing to do)
	int wantPad = -1;
	bool wantOne = false;
	int64_t wantW = 0;        // the grid point of the launch
	int pending = -1, pendingPad = -1;
	bool pendingOne = false;
	int retSlot = -1, retPad = -1;  // after a one-shot: back to this loop
	int held = -1;            // pad held in HOLD mode
	int heldSlot = -1;        // its slot when pressed (the bank may change while it is held)
	float env = 0.f;          // effect fade (3 ms)
	int64_t pStart = 0;       // start of the current pattern bar
	int patterns = 0;

	// effect state
	double rpos = 0;          // tape stop
	int64_t grainStart[2][2]; // freeze: [grain][channel]
	double grainPh[2];
	int64_t freezeEnd = 0;
	int slice = 0, lastStep = -1;  // rearrange
	uint32_t rng = 12345u;
	float hold[2] = {0, 0}, crushPh = 0;
	float gsm = 1.f;          // smoothed gain of the gates and scratch
	std::vector<float> dl[2]; // echo
	int64_t dW = 0;
	int64_t dWk = 0;          // dW % the echo line's length
	float dlp[2] = {0, 0};
	Allpass hall[2][4];
	float echoIn = 1.f;

	// capture loop (REC 1 BAR)
	enum { CAP_OFF, CAP_ARMED, CAP_REC, CAP_PLAY };
	int cap = CAP_OFF;
	std::vector<float> loop[2];
	int64_t capLen = 0, capPos = 0;
	static const int CAP_X = 256;

	// the rest of the chain
	Isolator iso[2];
	Svf svf[2];
	float gCut = 0.f;         // smoothed filter control
	float gCutC = 1e30f, djG = 0.f;  // the DJ filter's g, worked out again only when gCut moves
	// the one-pole coefficients, constant for a sample rate
	float kCut = 0.f, kEcho = 0.f, kScratch = 0.f, gateMs = -1.f, kGate = 0.f;
	int ctl = 0;
	float gl = 1, gm = 1, gh = 1, ingain = 1;

	Engine() {
		for (int i = 0; i < NBANKS * NPADS; i++)
			pads[i] = defaultPad(i);
		setSampleRate(48000.f);
	}

	void setSampleRate(float sr) {
		fs = sr;
		bufN = (int64_t) (sr * MAX_SECONDS) + 1024;
		bl.assign(bufN, 0.f);
		br.assign(bufN, 0.f);
		for (int k = 0; k < 2; k++) {
			dl[k].assign((size_t) (sr * 4.f) + 16, 0.f);
			loop[k].assign((size_t) (sr * 5.f) + CAP_X + 16, 0.f);
			static const float MS[4] = {5.1f, 7.3f, 11.7f, 17.3f};
			for (int i = 0; i < 4; i++)
				hall[k][i].init((int) (MS[i] * (k ? 1.07f : 1.f) * sr / 1000.f));
			iso[k].f1 = -1;
		}
		W = -1;
		Wk = bufN - 1;
		dW = 0;
		dWk = 0;
		gCutC = 1e30f;
		gateMs = -1.f;
		kCut = 1.f - std::exp(-1.f / (0.01f * fs));
		kEcho = 1.f - std::exp(-1.f / (0.004f * fs));
		kScratch = 1.f - std::exp(-1.f / (0.001f * fs));
		spb = fs * 60.0 / bpm;
		cap = CAP_OFF;
		// every sample position above counted on the old clock: start the pads and the tempo measurement fresh
		act = actPad = -1;
		actOne = false;
		want = -2;
		wantPad = -1;
		wantW = 0;
		pending = pendingPad = -1;
		retSlot = retPad = -1;
		held = heldSlot = -1;
		env = 0.f;
		pStart = 0;
		freezeEnd = 0;
		lastPulse = lastTap = -1;
		ntaps = 0;
	}

	// ---------------------------------------------------------------- time
	double barLen() const { return 4.0 * spb; }
	void setBpm(float b) {
		bpm = clampf(b, 48.f, 300.f);
		spb = fs * 60.0 / bpm;
	}
	/** A rising edge at the CLOCK input. */
	void clockPulse() {
		int ppb = PPB[std::min(std::max(c.clockMode, 0), 3)];
		if (lastPulse >= 0 && ext) {
			double iv = (double) (W - lastPulse);
			if (iv > 16 && iv < fs * 2.0) {
				double s = iv * ppb;
				spb = clockCount <= 1 ? s : spb + (s - spb) * 0.5;
				spb = std::min(std::max(spb, fs * 60.0 / 300.0), fs * 60.0 / 48.0); // the same range as TAP
				bpm = (float) (fs * 60.0 / spb);
			}
		}
		lastPulse = W;
		sinceClock = 0;
		ext = true;
		clockCount++;
		beat = (double) clockCount / ppb;
	}
	void resetPulse() {
		clockCount = -1;
		beat = 0.0;
	}
	void tap() {
		if (lastTap >= 0) {
			double iv = (double) (W - lastTap);
			if (iv > fs * 0.2 && iv < fs * 2.0) {
				taps[ntaps % 4] = (int64_t) iv;
				ntaps++;
				int n = std::min(ntaps, 4);
				double s = 0;
				for (int i = 0; i < n; i++) s += taps[i];
				if (!ext) setBpm((float) (fs * 60.0 / (s / n)));
			}
			else
				ntaps = 0;
		}
		lastTap = W;
		if (!ext) beat = std::round(beat);
	}

	// ---------------------------------------------------------------- pads
	int slot(int pad) const { return bank * NPADS + pad; }
	void request(int pad, bool one) {
		pending = slot(pad);
		pendingPad = pad;
		pendingOne = one;
		if (QUANT_BEATS[c.quant] <= 0.f) fire();
	}
	void fire() {
		if (pending < 0) return;
		if (pendingOne) {
			if (act >= 0 && !actOne) { retSlot = act; retPad = actPad; }
		}
		else
			retSlot = retPad = -1;
		want = pending;
		wantPad = pendingPad;
		wantOne = pendingOne;
		wantW = W;
		pending = pendingPad = -1;
	}
	void stop() {
		pending = pendingPad = -1;
		retSlot = retPad = -1;
		want = -1;
		held = heldSlot = -1;
	}
	void press(int pad) {
		int s = slot(pad);
		bool playing = (act == s && want == -2) || want == s;
		switch (c.mode) {
			case 2:  // HOLD
				if (c.latch && (playing || pending == s)) { stop(); return; }
				held = pad;
				heldSlot = s;
				request(pad, false);
				return;
			case 1: request(pad, true); return;  // ONE SHOT
			default:                              // LOOP: the same pad again stops
				if (playing || pending == s) { stop(); return; }
				request(pad, false);
		}
	}
	void release(int pad) {
		// (no mode check: switched from HOLD to LOOP while held, the release still ends it)
		if (held < 0 || c.latch || held != pad) return;
		held = -1;
		int s = heldSlot;
		heldSlot = -1;
		if (pending == s) pending = pendingPad = -1;
		if (act == s || want == s) want = -1;
	}
	void kill() {
		stop();
		env = std::min(env, 0.5f);
	}
	/** 0 off, 1 playing, 2 waiting for the grid. */
	int padState(int pad) const {
		int s = slot(pad);
		if (pending == s) return 2;
		if (want >= 0 && want == s) return 1;
		if (act == s && want != -1) return 1;
		return 0;
	}
	void recToggle() {
		if (cap == CAP_OFF) cap = CAP_ARMED;
		else cap = CAP_OFF;
	}

	// ---------------------------------------------------------------- buffer access
	float at(const std::vector<float>& b, int64_t i) const {
		if (i > W) i = W;
		if (i < W - bufN + 2) i = W - bufN + 2;
		int64_t k = Wk - (W - i);  // W - i is 0 … bufN − 2: the same slot as i % bufN
		if (k < 0) k += bufN;
		return b[k];
	}
	float read(const std::vector<float>& b, double pos) const {
		int64_t i = (int64_t) std::floor(pos);
		float f = (float) (pos - i);
		float y0 = at(b, i - 1), y1 = at(b, i), y2 = at(b, i + 1), y3 = at(b, i + 2);
		// 4-point Hermite
		float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
		return ((c3 * f + c2) * f + c1) * f + y1;
	}
	void readLR(double pos, float& l, float& r) const {
		l = read(bl, pos);
		r = read(br, pos);
	}
	/** A fade window at the edges of a segment of length len (samples), position u. */
	float edge(double u, double len, float ms) const {
		double f = std::min((double) (ms * fs / 1000.f), len * 0.25);
		if (f < 1) return 1.f;
		return (float) std::min(1.0, std::min(u / f, (len - u) / f));
	}
	uint32_t rnd() { return rng = rng * 1664525u + 1013904223u; }
	float rndf() { return (rnd() >> 8) * (1.f / 16777216.f); }

	void startEffect() {
		const PadCfg& p = pads[act];
		pStart = wantW;
		patterns = 0;
		rpos = (double) pStart;
		lastStep = -1;
		gsm = 1.f;
		if (p.fx == FREEZE) {
			freezeEnd = W;
			grainPh[0] = 0.0;
			grainPh[1] = 0.5;
			for (int g = 0; g < 2; g++)
				for (int k = 0; k < 2; k++) grainStart[g][k] = W;
		}
		if (p.fx == ECHO) echoIn = 1.f;
	}

	// ---------------------------------------------------------------- one effect sample
	void effect(const PadCfg& p, float sl, float sr, float& ol, float& orr) {
		double bar = barLen();
		double pt = (double) (W - pStart);
		const float* v = p.p;
		float amt = std::min(1.f, c.intensity * 2.f);
		float hard = std::max(0.f, c.intensity * 2.f - 1.f);
		ol = sl;
		orr = sr;
		auto mixw = [&](float wl, float wr, float m) {
			ol = sl + (wl - sl) * m;
			orr = sr + (wr - sr) * m;
		};
		switch (p.fx) {
			case STUTTER: {
				double L = std::max(64.0, DIVS[pick(v[0], NDIV)] * spb);
				int k = (int) (pt / L);
				double u = pt - k * L;
				float semis = clampf(stutterPitch(v[2]) * k, -24.f, 24.f);
				double r = k > 0 ? std::pow(2.0, semis / 12.0) : 1.0;
				double ru = k > 0 ? std::fmod(u * r, L) : u;
				float g = std::pow(clampf(1.f - (0.35f + hard * 0.3f) * (v[1] + hard * 0.5f), 0.f, 1.f), (float) k);
				float w = edge(ru, L, 2.f);
				if (k == 0) w = (float) std::min(1.0, (L - u) / std::min(L * 0.25, fs * 0.002));
				float a, b;
				readLR(pStart + ru, a, b);
				mixw(a * g * w, b * g * w, amt);
				break;
			}
			case HALFTIME: {
				double r = v[0] < 0.5f ? 0.5 : 0.25;
				float a, b;
				readLR(pStart + pt * r, a, b);
				float w = edge(pt, bar, smoothMs(v[1]));
				mixw(a * w, b * w, amt * v[2]);
				break;
			}
			case REVERSE: {
				double L = REV_BEATS[pick(v[0], 4)] * spb;
				int k = (int) (pt / L);
				double u = pt - k * L;
				float a, b;
				readLR(pStart + k * L - 1.0 - u, a, b);
				float w = edge(u, L, smoothMs(v[1]));
				mixw(a * w, b * w, amt * v[2]);
				break;
			}
			case TAPESTOP: {
				double D = tapeBeats(v[0]) * spb * (1.f - 0.6f * hard);
				double q = pt / D;
				double rate = q < 1.0 ? std::pow(1.0 - q, (double) tapeCurve(v[1])) : 0.0;
				rpos += rate;
				float g = (float) std::min(1.0, rate * 30.0);
				float a, b;
				readLR(rpos, a, b);
				mixw(a * g, b * g, amt);
				break;
			}
			case SCRATCH: {
				double P = SCR_BEATS[pick(v[0], 3)] * spb;
				int k = (int) (pt / P);
				double ph = (pt - k * P) / P;
				double depth = scratchMs(v[1]) * (1.f + hard) * fs / 1000.0;
				int mv = pick(v[2], 3);
				double s = 0.5 - 0.5 * std::cos(2.0 * PI * ph);
				float g = 1.f;
				if (mv == 1) g = ph < 0.5 ? 1.f : 0.f;                       // forward: the back stroke cut
				if (mv == 2) g = (ph > 0.15 && ph < 0.6) ? 1.f : 0.f;        // chirp
				gsm += (g - gsm) * kScratch;
				double anchor = pStart + k * P - depth;
				float a, b;
				readLR(anchor + s * depth, a, b);
				mixw(a * gsm, b * gsm, amt);
				break;
			}
			case GATE: case TRANCE: case PUMP: {
				float g = 1.f, ms = 1.f, depth = 1.f;
				if (p.fx == GATE) {
					double d = DIVS[pick(v[0], NDIV)];
					double ph = beat / d - std::floor(beat / d);
					g = ph < 0.5 ? 1.f : 0.f;
					ms = edgeMs(v[2]) * (1.f - 0.8f * hard);
					depth = v[1];
				}
				else if (p.fx == TRANCE) {
					int st = (int) std::floor(beat / (v[1] < 0.5f ? 0.25 : 0.125)) & 15;
					g = (TRANCE_PAT[pick(v[0], NTRANCE)] >> (15 - st)) & 1 ? 1.f : 0.f;
					ms = 1.5f;
					depth = v[2];
				}
				else {
					double d = PUMP_BEATS[pick(v[0], 3)];
					double ph = beat / d - std::floor(beat / d);
					g = 1.f - (float) std::exp(-ph / (pumpRelease(v[2]) * (1.f + hard) * 0.4));
					ms = 0.7f;
					depth = v[1];
				}
				if (ms != gateMs) {
					gateMs = ms;
					kGate = 1.f - std::exp(-1.f / (ms * 0.001f * fs));
				}
				gsm += (g - gsm) * kGate;
				float m = 1.f - clampf(depth * amt + hard * 0.3f, 0.f, 1.f) * (1.f - gsm);
				ol = sl * m;
				orr = sr * m;
				break;
			}
			case ECHO: {
				// the delay runs all the time (echoTick); here the dry is cut and the tail rings
				mixw(echoOut[0], echoOut[1], amt);
				break;
			}
			case FREEZE: {
				double S = freezeMs(v[0]) * fs / 1000.0;
				float spread = clampf(v[1] + hard * 0.5f, 0.f, 1.f);
				double inc = 1.0 / S;
				float wl = 0, wr = 0;
				for (int g = 0; g < 2; g++) {
					grainPh[g] += inc;
					if (grainPh[g] >= 1.0) {
						grainPh[g] -= 1.0;
						for (int k = 0; k < 2; k++) {
							double jit = (k == 0 || spread > 0.f) ? rndf() * spread * 0.25 * fs : 0.0;
							grainStart[g][k] = freezeEnd - (int64_t) S - (int64_t) jit;
						}
					}
					float h = 0.5f - 0.5f * std::cos(2.f * PI * (float) grainPh[g]);
					double off = grainPh[g] * S;
					wl += read(bl, grainStart[g][0] + off) * h;
					wr += read(br, (spread > 0.f ? grainStart[g][1] : grainStart[g][0]) + off) * h;
				}
				mixw(wl, wr, amt * v[2]);
				break;
			}
			case CRUSH: {
				float bits = std::max(1.f, std::round(crushBits(v[0]) - hard * 6.f));
				float rate = crushRate(v[1]) / (1.f + hard * 4.f);
				crushPh += rate / fs;
				if (crushPh >= 1.f) {
					crushPh -= std::floor(crushPh);
					hold[0] = sl;
					hold[1] = sr;
				}
				float q = std::pow(2.f, bits - 1.f);
				float a = std::round(clampf(hold[0] * 0.2f, -1.f, 1.f) * q) / q * 5.f;
				float b = std::round(clampf(hold[1] * 0.2f, -1.f, 1.f) * q) / q * 5.f;
				mixw(a, b, amt * v[2]);
				break;
			}
			default: {  // REARRANGE: slices of the bar before, picked anew on every step
				double G = REARR_BEATS[pick(v[0], 2)] * spb;
				int n = std::max(1, (int) std::lround(bar / G));
				int k = (int) (pt / G);
				double u = pt - k * G;
				if (k != lastStep) {
					lastStep = k;
					float rr = clampf(v[1] + hard * 0.5f, 0.f, 1.f);
					if (k > 0 && rndf() < v[2]) {}
					else if (rndf() < rr) slice = (int) (rnd() % (uint32_t) n);
					else slice = k % n;
				}
				float a, b;
				readLR(pStart - bar + slice * G + u, a, b);
				float w = edge(u, G, 2.f);
				mixw(a * w, b * w, amt);
				break;
			}
		}
	}

	float echoOut[2] = {0, 0};
	const PadCfg* echoPad = nullptr;
	/** The echo line runs all the time, so ECHO OUT has a tail to ring out. */
	void echoTick(float sl, float sr, bool echoing, const PadCfg* p) {
		const float* v = p ? p->p : nullptr;
		float hard = std::max(0.f, c.intensity * 2.f - 1.f);
		float fb = v ? clampf(echoFeedback(v[1]) + hard * 0.3f, 0.f, 0.95f) : 0.5f;
		double d = (v ? ECHO_BEATS[pick(v[0], 5)] : 0.75f) * spb;
		int64_t n = (int64_t) dl[0].size();
		d = std::min(d, (double) n - 4.0);
		float target = echoing ? 0.f : 1.f;
		echoIn += (target - echoIn) * kEcho;
		float in[2] = {sl, sr};
		float hallAmt = v ? v[2] : 0.f;
		for (int k = 0; k < 2; k++) {
			double pos = (double) dW - d;
			int64_t i = (int64_t) std::floor(pos);
			float f = (float) (pos - i);
			int64_t i0 = dWk - (dW - i);  // dW - i is 0 … n − 3: the same slot as i mod n
			if (i0 < 0) i0 += n;
			int64_t i1 = i0 + 1 >= n ? 0 : i0 + 1;
			float y = dl[k][(size_t) i0] * (1.f - f) + dl[k][(size_t) i1] * f;
			dlp[k] += (y - dlp[k]) * 0.35f;  // dark repeats
			if (std::fabs(dlp[k]) < 1e-12f) dlp[k] = 0.f;
			float h = dlp[k];
			for (int a = 0; a < 4; a++) h = hall[k][a].process(h);
			echoOut[k] = dlp[k] + (h - dlp[k]) * hallAmt;
			float fbk = (k == 0 ? dlp[1] : dlp[0]) * 0.25f + dlp[k] * 0.75f;  // a little ping-pong
			float wv = sat((in[k] * echoIn + fbk * fb) * 0.2f) * 5.f;
			dl[k][(size_t) dWk] = std::fabs(wv) < 1e-12f ? 0.f : wv;  // no denormals in the dying tail
		}
		dW++;
		if (++dWk >= n) dWk = 0;
	}

	// ---------------------------------------------------------------- one sample
	void process(float inL, float inR, float& outL, float& outR) {
		W++;
		if (++Wk >= bufN) Wk = 0;
		// time
		if (++sinceClock > (int64_t) (fs * 2.f)) ext = false;
		double prevBeat = beat;
		if (ext) {
			int ppb = PPB[std::min(std::max(c.clockMode, 0), 3)];
			if (clockCount >= 0) beat = std::min(beat + 1.0 / spb, (double) (clockCount + 1) / ppb - 1e-6);
		}
		else
			beat += 1.0 / spb;

		// control rate
		if (--ctl <= 0) {
			ctl = 32;
			float fc = midFreq(c.freq);
			for (int k = 0; k < 2; k++) iso[k].set(fc / 3.1623f, fc * 3.1623f, fs);
			gl = c.killLo ? 0.f : eqGain(c.low);
			gm = c.killMid ? 0.f : eqGain(c.mid);
			gh = c.killHi ? 0.f : eqGain(c.high);
			ingain = dbToGain(12.f * clampf(c.gain, -1.f, 1.f));
			// the echo line follows the bank's first ECHO pad, so its tail is in time when the pad starts
			echoPad = nullptr;
			for (int i = 0; i < NPADS && !echoPad; i++)
				if (pads[slot(i)].fx == ECHO) echoPad = &pads[slot(i)];
		}

		// launch on the grid
		float qb = QUANT_BEATS[std::min(std::max(c.quant, 0), NQUANT - 1)];
		if (pending >= 0 && qb > 0.f && std::floor(beat / qb) != std::floor(prevBeat / qb)) fire();

		// capture loop
		float sl = inL, sr = inR;
		if (cap == CAP_ARMED && std::floor(beat / 4.0) != std::floor(prevBeat / 4.0)) {
			cap = CAP_REC;
			capLen = std::min((int64_t) barLen(), (int64_t) loop[0].size() - CAP_X - 4);
			capPos = 0;
		}
		if (cap == CAP_REC) {
			loop[0][capPos] = inL;
			loop[1][capPos] = inR;
			if (++capPos >= capLen + CAP_X) { cap = CAP_PLAY; capPos = CAP_X; }
		}
		if (cap == CAP_PLAY) {
			int64_t i = capPos;
			if (i < CAP_X) {  // the bar's end runs into its start
				float a = (float) i / CAP_X;
				sl = loop[0][i] * a + loop[0][capLen + i] * (1.f - a);
				sr = loop[1][i] * a + loop[1][capLen + i] * (1.f - a);
			}
			else {
				sl = loop[0][i];
				sr = loop[1][i];
			}
			if (++capPos >= capLen) capPos = 0;
		}

		sl *= ingain;
		sr *= ingain;
		float dryL = sl, dryR = sr;
		if (c.pre) {
			sl = iso[0].process(sl, gl, gm, gh);
			sr = iso[1].process(sr, gl, gm, gh);
		}
		bl[Wk] = sl;
		br[Wk] = sr;

		// switching pads: fade the running effect out, then start the next one
		if (want != -2) {
			if (act < 0 || env <= 0.f) {
				act = want;
				actPad = wantPad;
				actOne = wantOne;
				want = -2;
				if (act >= 0) {
					startEffect();
					env = 0.f;
				}
			}
		}
		float step = 1.f / (0.003f * fs);
		if (act >= 0 && want == -2) env = std::min(1.f, env + step);
		else env = std::max(0.f, env - step);

		// one pattern = one bar; a one-shot ends after it
		const PadCfg* cur = act >= 0 ? &pads[act] : nullptr;
		if (cur) {
			double bar = barLen();
			bool rolls = cur->fx != FREEZE && cur->fx != ECHO && !(cur->fx == TAPESTOP && cur->p[2] < 0.5f);
			if ((double) (W - pStart) >= bar && (rolls || actOne)) {
				patterns++;
				pStart += (int64_t) bar;
				rpos = (double) pStart;
				lastStep = -1;
				if (actOne && want == -2) {
					if (retSlot >= 0) { want = retSlot; wantPad = retPad; wantOne = false; wantW = W; }
					else want = -1;
					retSlot = retPad = -1;
				}
			}
		}
		echoTick(sl, sr, cur && cur->fx == ECHO && env > 0.f, cur && cur->fx == ECHO ? cur : echoPad);
		float el = sl, er = sr;
		if (cur && env > 0.f) {
			float a, b;
			effect(*cur, sl, sr, a, b);
			el = sl + (a - sl) * env;
			er = sr + (b - sr) * env;
		}
		if (act >= 0 && env <= 0.f && want == -1) { act = actPad = -1; want = -2; }

		if (!c.pre) {
			el = iso[0].process(el, gl, gm, gh);
			er = iso[1].process(er, gl, gm, gh);
		}

		// DJ filter: a dead zone around the centre, LP to the left, HP to the right
		gCut += (c.cut - gCut) * kCut;
		float ac = std::fabs(gCut);
		float blend = clampf((ac - 0.02f) / 0.06f, 0.f, 1.f);
		if (blend > 0.f) {
			if (gCut != gCutC) {  // pow and tan only while the filter control moves
				gCutC = gCut;
				float a = clampf((ac - 0.02f) / 0.98f, 0.f, 1.f);
				float fc = gCut < 0.f ? 20000.f * std::pow(40.f / 20000.f, a) : 20.f * std::pow(10000.f / 20.f, a);
				fc = std::min(fc, fs * 0.45f);
				djG = std::tan(PI * fc / fs);
			}
			float g = djG;
			float kq = 1.f / (0.707f + c.reso * 3.3f);  // up to Q 4: +12 dB at the cutoff
			float x[2] = {el, er};
			for (int ch = 0; ch < 2; ch++) {
				float lp, hp;
				svf[ch].process(x[ch], g, kq, lp, hp);
				float y = gCut < 0.f ? lp : hp;
				x[ch] += (y - x[ch]) * blend;
			}
			el = x[0];
			er = x[1];
		}
		if (c.drive > 0.001f) {
			float g = 1.f + c.drive * 7.f, mk = 1.f / (1.f + c.drive * 1.2f);
			el = sat(el * g * 0.2f) * 5.f * mk;
			er = sat(er * g * 0.2f) * 5.f * mk;
		}
		el = limit(el);
		er = limit(er);
		outL = dryL + (el - dryL) * c.mix;
		outR = dryR + (er - dryR) * c.mix;
	}

	/** Where in the pattern the running pad is (0…1), −1 if none. */
	float progress() const {
		if (act < 0) return -1.f;
		return clampf((float) ((W - pStart) / barLen()), 0.f, 1.f);
	}
};

}  // namespace gstot
