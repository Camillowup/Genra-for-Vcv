// SPDX-License-Identifier: GPL-3.0-only
// SCHLAG – the sample drum machine for GENRA's drum track (after the Polyend Play): the parts without Rack.
// A WAV reader, the knob functions (names, ranges, how they read), the voice that plays a sample (start / end with
// reverse, tune, attack / decay, DJ filter, drive, bits, pan) and the repeat (ratchet) types.
// Tested in test/test_schlag.cpp. The module: Schlag.cpp.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace schlag {

static const int NT = 8;      // tracks, in GENRA's order
static const int MAXS = 128;  // steps per track (GENRA's longest loop: 8 bars of 16ths)
static const int NROW = 4;    // assignable knobs per track
static const char* const TRACK_NAMES[NT] = {"KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"};

// ---------------------------------------------------------------- a sample in memory
struct Sample {
	std::string name;           // file name (with extension)
	std::vector<float> data;    // interleaved, 1 or 2 channels, ±1
	int channels = 1;
	int frames = 0;
	float rate = 44100.f;
	float peaks[64] = {};       // for the display: the loudest value in each 64th
	void makePeaks() {
		for (int i = 0; i < 64; i++) {
			int a = (int) ((int64_t) frames * i / 64), b = (int) ((int64_t) frames * (i + 1) / 64);
			float p = 0.f;
			for (int f = a; f < b; f++)
				for (int c = 0; c < channels; c++)
					p = std::max(p, std::fabs(data[(size_t) f * channels + c]));
			peaks[i] = std::min(1.f, p);
		}
	}
};

/** Reads a WAV file from memory (PCM 8/16/24/32 bit, float 32/64, mono or more channels: the first two are kept).
 * maxSeconds: longer files are cut. Returns nullptr (and err) when the data cannot be read. */
inline std::shared_ptr<Sample> readWavData(const std::vector<uint8_t>& buf, std::string& err, float maxSeconds = 30.f) {
	if (buf.size() < 44) {
		err = "not a WAV file";
		return nullptr;
	}
	auto u16 = [&](size_t o) { return (uint32_t) buf[o] | ((uint32_t) buf[o + 1] << 8); };
	auto u32 = [&](size_t o) { return u16(o) | (u16(o + 2) << 16); };
	if (std::memcmp(buf.data(), "RIFF", 4) != 0 || std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
		err = "not a WAV file";
		return nullptr;
	}
	int fmt = 0, ch = 0, bits = 0;
	uint32_t rate = 0;
	size_t dataOff = 0, dataLen = 0;
	size_t p = 12;
	while (p + 8 <= buf.size()) {
		uint32_t sz = u32(p + 4);
		size_t body = p + 8;
		if (!std::memcmp(buf.data() + p, "fmt ", 4) && body + 16 <= buf.size()) {
			fmt = (int) u16(body);
			ch = (int) u16(body + 2);
			rate = u32(body + 4);
			bits = (int) u16(body + 14);
			if (fmt == 0xFFFE && body + 26 <= buf.size())
				fmt = (int) u16(body + 24); // WAVE_FORMAT_EXTENSIBLE: the sub format's first two bytes
		}
		else if (!std::memcmp(buf.data() + p, "data", 4)) {
			dataOff = body;
			dataLen = std::min<size_t>(sz, buf.size() - body);
			break;
		}
		p = body + sz + (sz & 1u);
	}
	if (!dataOff || ch < 1 || rate < 1000 || rate > 768000 || !(fmt == 1 || fmt == 3)) {
		err = "unsupported WAV format";
		return nullptr;
	}
	int bps = bits / 8;
	if (!(fmt == 1 ? (bps >= 1 && bps <= 4) : (bps == 4 || bps == 8))) {
		err = "unsupported WAV format";
		return nullptr;
	}
	size_t frameBytes = (size_t) bps * ch;
	int frames = (int) std::min<size_t>(dataLen / frameBytes, (size_t) (maxSeconds * rate));
	if (frames < 1) {
		err = "empty";
		return nullptr;
	}
	auto s = std::make_shared<Sample>();
	s->channels = ch >= 2 ? 2 : 1;
	s->frames = frames;
	s->rate = (float) rate;
	s->data.resize((size_t) frames * s->channels);
	const uint8_t* d = buf.data() + dataOff;
	for (int i = 0; i < frames; i++) {
		for (int c = 0; c < s->channels; c++) {
			const uint8_t* q = d + (size_t) i * frameBytes + (size_t) c * bps;
			float v = 0.f;
			if (fmt == 1) {
				switch (bps) {
					case 1: v = ((int) q[0] - 128) / 128.f; break;
					case 2: v = (int16_t) (q[0] | (q[1] << 8)) / 32768.f; break;
					case 3: {
						int32_t x = (int32_t) ((uint32_t) q[0] << 8 | (uint32_t) q[1] << 16 | (uint32_t) q[2] << 24) >> 8;
						v = x / 8388608.f;
						break;
					}
					default: {
						int32_t x = (int32_t) ((uint32_t) q[0] | (uint32_t) q[1] << 8 | (uint32_t) q[2] << 16 | (uint32_t) q[3] << 24);
						v = (float) (x / 2147483648.0);
						break;
					}
				}
			}
			else if (bps == 4) {
				float x;
				std::memcpy(&x, q, 4);
				v = x;
			}
			else {
				double x;
				std::memcpy(&x, q, 8);
				v = (float) x;
			}
			if (!std::isfinite(v))
				v = 0.f;
			s->data[(size_t) i * s->channels + c] = std::max(-1.f, std::min(1.f, v));
		}
	}
	s->makePeaks();
	return s;
}

/** The same from a file (plain fopen: the module reads its files with Rack's system::readFile, which knows UTF-8
 * paths on Windows). */
inline std::shared_ptr<Sample> readWav(const std::string& path, std::string& err, float maxSeconds = 30.f) {
	FILE* f = std::fopen(path.c_str(), "rb");
	if (!f) {
		err = "cannot open";
		return nullptr;
	}
	std::vector<uint8_t> buf;
	std::fseek(f, 0, SEEK_END);
	long len = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (len > 0 && len < 512L * 1024 * 1024) {
		buf.resize((size_t) len);
		if (std::fread(buf.data(), 1, buf.size(), f) != buf.size())
			buf.clear();
	}
	std::fclose(f);
	return readWavData(buf, err, maxSeconds);
}

// ---------------------------------------------------------------- the knob functions (the dropdowns)
// The order is stored in patches: only append.
enum Fn {
	F_LEVEL, F_PAN, F_TUNE, F_MICRO, F_START, F_END, F_ATTACK, F_DECAY,       // SOUND
	F_SENDA, F_SENDB,                                                         // SPACE
	F_CUTOFF, F_RES, F_DRIVE, F_BITS,                                         // TONE
	F_REPTYPE, F_REPGRID, F_CHANCE, F_ACTION,                                 // PLAY
	NUM_FN
};
enum Group { G_SOUND, G_SPACE, G_TONE, G_PLAY, NUM_GROUPS };
static const char* const GROUP_NAMES[NUM_GROUPS] = {"SOUND", "SPACE", "TONE", "PLAY"};

struct FnInfo {
	const char* name;   // in the field (short)
	const char* hint;   // in the dropdown
	int group;
	float def;          // default knob value (0..1)
	int steps;          // > 0: a list (the knob picks one of `steps`)
};

static const int NUM_REPEAT_TYPES = 18;
static const char* const REPEAT_TYPES[NUM_REPEAT_TYPES] = {
	"OFF", "STRAIGHT", "FADE", "RAISE", "PONG", "ARP DOWN", "ARP UP", "TANK", "ALIEN",
	"ECHO", "YOUR CRUSH", "GLITTER", "EFFECTRON", "TO AND FROM", "ROUND TRIP", "OCTAVES", "GAME OVER", "DOWN AND UP"};
enum RepType { R_OFF, R_STRAIGHT, R_FADE, R_RAISE, R_PONG, R_ARPDOWN, R_ARPUP, R_TANK, R_ALIEN, R_ECHO, R_CRUSH,
               R_GLITTER, R_EFFECTRON, R_TOFRO, R_ROUNDTRIP, R_OCTAVES, R_GAMEOVER, R_DOWNUP };

struct RepGrid {
	int hits, steps;
};
static const int NUM_REPEAT_GRIDS = 19;
static const RepGrid REPEAT_GRIDS[NUM_REPEAT_GRIDS] = {
	{2, 1}, {3, 1}, {4, 1}, {6, 1}, {8, 1}, {2, 2}, {3, 2}, {4, 2}, {6, 2}, {8, 2},
	{3, 4}, {4, 4}, {6, 4}, {8, 4}, {16, 4}, {4, 8}, {8, 8}, {12, 8}, {16, 8}};
static const int DEFAULT_GRID = 2; // 4 hits in 1 step

static const int NUM_CHANCES = 18;
static const char* const CHANCES[NUM_CHANCES] = {
	"ALWAYS", "90 %", "80 %", "70 %", "60 %", "50 %", "40 %", "30 %", "20 %", "10 %",
	"PLAY 1 / SKIP 1", "PLAY 2 / SKIP 2", "PLAY 3 / SKIP 1", "PLAY 1 / SKIP 3",
	"SKIP 1 / PLAY 1", "SKIP 2 / PLAY 2", "SKIP 3 / PLAY 1", "SKIP 1 / PLAY 3"};
/** Whether a hit passes the chance: r a random number in [0, 1), loop the loop count (play / skip). */
inline bool chancePasses(int c, float r, int64_t loop) {
	if (c <= 0)
		return true;
	if (c <= 9)
		return r < (10 - c) / 10.f;
	static const int N[8] = {1, 2, 3, 1, 1, 2, 3, 1}, M[8] = {1, 2, 1, 3, 1, 2, 1, 3};
	int k = std::min(c - 10, 7);
	bool skipFirst = k >= 4;
	int cyc = N[k] + M[k];
	int pos = (int) (((loop % cyc) + cyc) % cyc);
	return skipFirst ? pos >= N[k] : pos < N[k];
}

static const int NUM_ACTIONS = 10;
static const char* const ACTIONS[NUM_ACTIONS] = {"PLAY STEP", "RND SAMPLE", "RND NOTE", "OCTAVE UP", "RND MICRO",
                                                 "RND CUTOFF", "RND REPEAT", "RND START", "RND END", "HUMANIZE"};
enum Action { A_PLAY, A_SAMPLE, A_NOTE, A_OCTAVE, A_MICRO, A_CUTOFF, A_REPEAT, A_START, A_END, A_HUMANIZE };

static const FnInfo FN[NUM_FN] = {
	{"LEVEL", "volume", G_SOUND, 48.f / 54.f, 0},
	{"PAN", "L · C · R", G_SOUND, 0.5f, 0},
	{"TUNE", "semitones", G_SOUND, 0.5f, 49},
	{"MICROTUNE", "±100 cents", G_SOUND, 0.5f, 0},
	{"START", "sample start", G_SOUND, 0.f, 0},
	{"END", "end < start = reverse", G_SOUND, 1.f, 0},
	{"ATTACK", "fade-in", G_SOUND, 0.f, 0},
	{"DECAY", "fade-out", G_SOUND, 1.f, 0},
	{"SEND A", "to SEND A", G_SPACE, 0.f, 0},
	{"SEND B", "to SEND B", G_SPACE, 0.f, 0},
	{"CUTOFF", "LP · off · HP", G_TONE, 0.5f, 0},
	{"RESONANCE", "filter peak", G_TONE, 0.2f, 0},
	{"DRIVE", "overdrive", G_TONE, 0.f, 0},
	{"BITS", "bit depth", G_TONE, 0.f, 0},
	{"REPEAT TYPE", "18 types", G_PLAY, 0.f, NUM_REPEAT_TYPES},
	{"REPEAT GRID", "hits | steps", G_PLAY, DEFAULT_GRID / (float) (NUM_REPEAT_GRIDS - 1), NUM_REPEAT_GRIDS},
	{"CHANCE", "always · % · play/skip", G_PLAY, 0.f, NUM_CHANCES},
	{"ACTION", "random X · humanize", G_PLAY, 0.f, NUM_ACTIONS},
};
// the knobs' functions on a new module (per row)
static const int DEFAULT_FN[NROW] = {F_LEVEL, F_TUNE, F_DECAY, F_SENDA};

/** A list function's entry for a knob value. */
inline int stepOf(int fn, float v) {
	int n = FN[fn].steps;
	if (n <= 1)
		return 0;
	return std::max(0, std::min(n - 1, (int) std::lround(v * (n - 1))));
}
inline float valueOfStep(int fn, int i) {
	int n = FN[fn].steps;
	return n > 1 ? std::max(0, std::min(n - 1, i)) / (float) (n - 1) : 0.f;
}

// ranges (keep fnText in step)
inline float levelGain(float v) { return v <= 0.001f ? 0.f : std::pow(10.f, (-48.f + 54.f * v) / 20.f); }
inline float tuneSemis(float v) { return (float) stepOf(F_TUNE, v) - 24.f; }
inline float microCents(float v) { return (v - 0.5f) * 200.f; }
inline float attackMs(float v) { return 200.f * v * v; }
/** Decay: the time to silence in ms; < 0 = none (the whole sample). */
inline float decayMs(float v) { return v >= 0.995f ? -1.f : 10.f * std::pow(200.f, v); }
inline int bitsOf(float v) { return v < 0.01f ? 16 : std::max(2, 16 - (int) std::lround(v * 14.f)); }

inline std::string fnText(int fn, float v) {
	char b[48];
	switch (fn) {
		case F_LEVEL: {
			if (v <= 0.001f)
				return "-INF dB";
			std::snprintf(b, sizeof b, "%+.1f dB", -48.f + 54.f * v);
			return b;
		}
		case F_PAN: {
			int p = (int) std::lround((v - 0.5f) * 200.f);
			if (p == 0)
				return "C";
			std::snprintf(b, sizeof b, "%d %s", std::abs(p), p < 0 ? "L" : "R");
			return b;
		}
		case F_TUNE: std::snprintf(b, sizeof b, "%+d ST", (int) tuneSemis(v)); return b;
		case F_MICRO: std::snprintf(b, sizeof b, "%+d CT", (int) std::lround(microCents(v))); return b;
		case F_ATTACK: std::snprintf(b, sizeof b, "%.0f MS", attackMs(v)); return b;
		case F_DECAY: {
			float ms = decayMs(v);
			if (ms < 0.f)
				return "FULL";
			if (ms >= 1000.f)
				std::snprintf(b, sizeof b, "%.2f S", ms / 1000.f);
			else
				std::snprintf(b, sizeof b, "%.0f MS", ms);
			return b;
		}
		case F_CUTOFF: {
			if (v > 0.48f && v < 0.52f)
				return "OFF";
			std::snprintf(b, sizeof b, "%s %d %%", v < 0.5f ? "LP" : "HP", (int) std::lround(std::fabs(v - 0.5f) * 200.f));
			return b;
		}
		case F_BITS: {
			int bits = bitsOf(v);
			if (bits >= 16)
				return "OFF";
			std::snprintf(b, sizeof b, "%d BIT", bits);
			return b;
		}
		case F_REPTYPE: return REPEAT_TYPES[stepOf(fn, v)];
		case F_REPGRID: {
			const RepGrid& g = REPEAT_GRIDS[stepOf(fn, v)];
			std::snprintf(b, sizeof b, "%d HITS | %d STEP%s", g.hits, g.steps, g.steps > 1 ? "S" : "");
			return b;
		}
		case F_CHANCE: return CHANCES[stepOf(fn, v)];
		case F_ACTION: return ACTIONS[stepOf(fn, v)];
		default: std::snprintf(b, sizeof b, "%d %%", (int) std::lround(v * 100.f)); return b;
	}
}

// ---------------------------------------------------------------- the DJ filter (TPT state variable, stereo)
struct Svf {
	float ic1[2] = {}, ic2[2] = {};
	float g = 0.f, k = 1.4f, a1 = 0.f, a2 = 0.f, a3 = 0.f;
	int mode = 0; // 0 off, 1 low-pass, 2 high-pass
	float lastV = -1.f, lastRes = -1.f, lastSr = 0.f;
	/** v: the CUTOFF value (0..0.48 LP, 0.52..1 HP), res 0..1. Recomputes only on change. */
	void set(float v, float res, float sr) {
		if (v == lastV && res == lastRes && sr == lastSr)
			return;
		lastV = v, lastRes = res, lastSr = sr;
		float fc;
		if (v < 0.48f) {
			mode = 1;
			fc = 20000.f * std::pow(2.f, -(0.48f - v) / 0.48f * 9.8f);
		}
		else if (v > 0.52f) {
			mode = 2;
			fc = 20.f * std::pow(2.f, (v - 0.52f) / 0.48f * 9.8f);
		}
		else {
			mode = 0;
			return;
		}
		fc = std::min(fc, 0.45f * sr);
		g = std::tan(3.14159265f * fc / sr);
		k = 2.f - 1.9f * std::max(0.f, std::min(1.f, res));
		a1 = 1.f / (1.f + g * (g + k));
		a2 = g * a1;
		a3 = g * a2;
	}
	void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0.f; }
	float process(float x, int c) {
		if (!mode)
			return x;
		float v3 = x - ic2[c];
		float v1 = a1 * ic1[c] + a2 * v3;
		float v2 = ic2[c] + a2 * ic1[c] + a3 * v3;
		ic1[c] = 2.f * v1 - ic1[c];
		ic2[c] = 2.f * v2 - ic2[c];
		if (mode == 1)
			return v2;
		return x - k * v1 - v2;
	}
};

// ---------------------------------------------------------------- what a hit plays with
struct HitParams {
	float gain = 1.f;        // velocity × level × accent (the level is also applied live)
	float semis = 0.f;       // tune + micro + action / repeat
	float start = 0.f, end = 1.f;
	float attackMs = 0.f, decayMs = -1.f;
	float panOffset = 0.f;   // added to PAN (repeats)
	float cutShift = 0.f;    // subtracted from CUTOFF (darker repeats), or
	float cutForce = -1.f;   // ≥ 0: CUTOFF for this hit (RND CUTOFF)
	int bitsForce = 16;      // < 16: crush this hit at least this hard
};

/** One playing sample. */
struct Voice {
	std::shared_ptr<Sample> smp;
	bool active = false;
	double pos = 0.0, inc = 1.0;
	int dir = 1;
	double endPos = 0.0;
	float t = 0.f;              // samples since the start
	float atk = 0.f, dec = -1.f; // in samples
	float gain = 1.f;
	float fade = 1.f, fadeStep = 0.f; // a fade-out to silence (retrigger, choke, mute)
	HitParams hp;
	Svf flt;

	void start(const std::shared_ptr<Sample>& s, const HitParams& p, float sr) {
		smp = s;
		hp = p;
		active = s && s->frames > 1;
		if (!active)
			return;
		double n = (double) s->frames - 1;
		double a = std::max(0.f, std::min(1.f, p.start)) * n, b = std::max(0.f, std::min(1.f, p.end)) * n;
		if (std::fabs(b - a) < 8.0) {
			active = false;
			return;
		}
		dir = b >= a ? 1 : -1;
		pos = a;
		endPos = b;
		inc = (double) s->rate / sr * std::pow(2.0, p.semis / 12.0);
		t = 0.f;
		atk = p.attackMs * 0.001f * sr;
		dec = p.decayMs < 0.f ? -1.f : std::max(1.f, p.decayMs * 0.001f * sr);
		gain = p.gain;
		fade = 1.f;
		fadeStep = 0.f;
		flt.reset();
		flt.lastV = -1.f;
	}
	void fadeOut(float sr, float ms) {
		if (active)
			fadeStep = std::max(fadeStep, 1.f / std::max(1.f, ms * 0.001f * sr));
	}
	/** 4-point Hermite at pos for channel c. */
	float read(int c) const {
		const Sample& s = *smp;
		int i = (int) pos;
		float fr = (float) (pos - i);
		auto at = [&](int k) {
			k = std::max(0, std::min(s.frames - 1, k));
			return s.data[(size_t) k * s.channels + (s.channels > 1 ? c : 0)];
		};
		float xm1 = at(i - 1), x0 = at(i), x1 = at(i + 1), x2 = at(i + 2);
		float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
		return ((c3 * fr + c2) * fr + c1) * fr + x0;
	}
	/** The next frame (stereo, before pan and level). drive 0..1, bits 2..16. */
	void next(float& l, float& r, float drive, int bits) {
		l = r = 0.f;
		if (!active)
			return;
		if ((dir > 0 && pos >= endPos) || (dir < 0 && pos <= endPos)) {
			active = false;
			return;
		}
		float env = 1.f;
		if (atk > 0.f && t < atk)
			env = t / atk;
		if (dec > 0.f) {
			if (t >= dec) {
				active = false;
				return;
			}
			float d = 1.f - t / dec;
			env *= d * d;
		}
		// a short fade at the region's end against clicks (2 ms at 48 kHz)
		double left = (endPos - pos) * dir / inc;
		if (left < 96.0)
			env *= (float) (left / 96.0);
		if (fadeStep > 0.f) {
			fade -= fadeStep;
			if (fade <= 0.f) {
				active = false;
				return;
			}
			env *= fade;
		}
		float a = read(0), b = smp->channels > 1 ? read(1) : a;
		a = flt.process(a, 0);
		b = smp->channels > 1 ? flt.process(b, 1) : a;
		if (drive > 0.001f) {
			float g = 1.f + drive * 12.f, n = 1.f / std::tanh(g);
			a = std::tanh(a * g) * n;
			b = std::tanh(b * g) * n;
		}
		if (bits < 16) {
			float q = (float) (1 << (bits - 1));
			a = std::round(a * q) / q;
			b = std::round(b * q) / q;
		}
		float e = env * gain;
		l = a * e;
		r = b * e;
		pos += inc * dir;
		t += 1.f;
	}
};

// ---------------------------------------------------------------- repeat types: what the k-th of n hits changes
struct RepeatMod {
	float semis = 0.f, gain = 1.f, pan = 0.f, cutShift = 0.f, start = -1.f;
	int bits = 16;
};
/** rnd: a random number in [0, 1) for this hit (ALIEN, GLITTER, EFFECTRON). k = 0 is the hit itself. */
inline RepeatMod repeatMod(int type, int k, int n, float rnd) {
	RepeatMod m;
	if (n < 2 || type == R_OFF || type == R_STRAIGHT)
		return m;
	float f = k / (float) (n - 1); // 0 .. 1 over the hits
	static const float UP[8] = {0, 3, 7, 12, 15, 19, 24, 27}, DOWN[8] = {0, -5, -9, -12, -17, -21, -24, -29};
	switch (type) {
		case R_FADE: m.gain = 1.f - 0.85f * f; break;
		case R_RAISE: m.gain = 0.2f + 0.8f * f; break;
		case R_PONG: m.pan = (k % 2) ? 0.8f : -0.8f; break;
		case R_ARPDOWN: m.semis = DOWN[k % 8]; break;
		case R_ARPUP: m.semis = UP[k % 8]; break;
		case R_TANK: m.semis = -2.f * k, m.cutShift = 0.06f * k; break;
		case R_ALIEN: m.semis = k ? std::floor(rnd * 25.f) - 12.f : 0.f; break;
		case R_ECHO: m.gain = std::pow(0.62f, (float) k), m.cutShift = 0.05f * k; break;
		case R_CRUSH: m.bits = std::max(2, 12 - 2 * k); break;
		case R_GLITTER: {
			static const float G[4] = {0, 12, 19, 24};
			if (k) {
				m.semis = G[(int) (rnd * 4.f) % 4];
				m.gain = 0.6f;
			}
			break;
		}
		case R_EFFECTRON: if (k) m.start = rnd * 0.6f; break;
		case R_TOFRO: {
			float tri = f < 0.5f ? f * 2.f : (1.f - f) * 2.f;
			m.semis = std::round(tri * 12.f);
			break;
		}
		case R_ROUNDTRIP: m.pan = -std::cos(f * 6.2831853f); break;
		case R_OCTAVES: m.semis = (k % 2) ? 12.f : 0.f; break;
		case R_GAMEOVER: m.semis = -3.f * k, m.bits = k ? 8 : 16, m.gain = 1.f - 0.4f * f; break;
		case R_DOWNUP: {
			float v = f < 0.5f ? f * 2.f : (1.f - f) * 2.f;
			m.semis = -std::round(v * 12.f);
			break;
		}
		default: break;
	}
	return m;
}

/** A small random generator (no allocation, for the audio thread). */
struct Rng {
	uint32_t s = 0x9e3779b9u;
	uint32_t next() {
		s ^= s << 13;
		s ^= s >> 17;
		s ^= s << 5;
		return s;
	}
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	int range(int a, int b) { return a + (int) (uni() * (b - a + 1)) % (b - a + 1 > 0 ? b - a + 1 : 1); }
};

/** RANDOM: a new sample for every step of a track, at most `strength` of the folder away from the base sample.
 * map: MAXS entries, -1 = the base sample. n: files in the folder. */
inline void rollMap(int16_t* map, int base, int n, float strength, Rng& rng) {
	int R = (int) std::lround(std::max(0.f, std::min(1.f, strength)) * (n - 1));
	for (int s = 0; s < MAXS; s++) {
		if (R <= 0 || n < 2) {
			map[s] = -1;
			continue;
		}
		int idx = std::max(0, std::min(n - 1, base + rng.range(-R, R)));
		map[s] = idx == base ? -1 : (int16_t) idx;
	}
}

}  // namespace schlag
