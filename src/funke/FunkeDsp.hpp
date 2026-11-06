// SPDX-License-Identifier: GPL-3.0-only
// FUNKE – the melody voice for GENRA's melody track: eight sound models (Rhodes, organ, bell, marimba, pluck, square,
// saw lead, flute), the eight filters of GENRA MONO, amp and filter envelopes, delayed vibrato, glide on GENRA's
// SLIDE, accent on ACC, and an FX bus (lo-fi, chorus, tempo-synced echo, reverb). Plain C++ without Rack, so
// test/test_funke.cpp can run it. The voices run at 2× the sample rate, like GENRA MONO.
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

namespace gfunke {

using gmono::clampf;
using gmono::sat;
static const float PI = 3.14159265358979f;
static const float C4 = 261.6256f;

enum Model { RHODES, ORGAN, BELL, MARIMBA, PLUCK, SQUARE, SAWLEAD, FLUTE, NUM_MODELS };
static const char* const MODEL_NAMES[NUM_MODELS] = {"RHODES", "ORGAN", "BELL", "MARIMBA",
                                                    "PLUCK",  "SQUARE", "SAW LEAD", "FLUTE"};
static const char* const MODEL_INFO[NUM_MODELS] = {"FM tine",    "Drawbars, M1", "FM bell",   "Mallet",
                                                   "Plucked string", "Pulse, grime", "Detuned saws", "Sine, breath"};
// what TIMBRE and BITE do, per model (tooltips, manual)
static const char* const MODEL_HELP[NUM_MODELS] = {
    "TIMBRE: tine brightness and bark. BITE: hammer attack",
    "TIMBRE: drawbars, mellow to full. BITE: percussion and key click",
    "TIMBRE: partial ratio (glass to gong). BITE: strike hardness",
    "TIMBRE: mallet hardness (overtones). BITE: mallet click",
    "TIMBRE: string brightness and sustain. BITE: pick brightness",
    "TIMBRE: pulse width (50 % to narrow). BITE: attack pitch blip",
    "TIMBRE: detune of the three saws (0 = in unison). BITE: octave and drive",
    "TIMBRE: sine to breathy flute. BITE: chiff on the attack"};

enum VoiceMode { MODE_MONO, MODE_LEGATO, MODE_POLY, NUM_MODES };
enum EchoDiv { DIV_16, DIV_8, DIV_8D, DIV_4, NUM_DIVS };
static const char* const DIV_NAMES[NUM_DIVS] = {"1/16", "1/8", "1/8D", "1/4"};
static const float DIV_BEATS[NUM_DIVS] = {0.25f, 0.5f, 0.75f, 1.f};
static const int MAXV = 8;
static const int MAXCH = 16;

/** sin(2π p) for any p, polynomial (error < 2e-4) – the organ and the FM models call it a lot. */
inline float sin2pi(float p) {
	float x = p - std::floor(p + 0.5f);  // -0.5 .. 0.5
	if (x > 0.25f)
		x = 0.5f - x;
	else if (x < -0.25f)
		x = -0.5f - x;
	float y = 2.f * PI * x, y2 = y * y;
	return y * (1.f - y2 / 6.f * (1.f - y2 / 20.f * (1.f - y2 / 42.f)));
}

inline float wrap(float p) { return p - std::floor(p); }
inline float decayCoef(float tau, float fs) { return std::exp(-1.f / (std::max(tau, 1e-4f) * fs)); }

/** Glide knob 0..1 → time constant (s). */
inline float glideSeconds(float k) { return 0.6f * k * k; }
/** Vibrato rate knob → Hz, delay knob → s. */
inline float vibRate(float k) { return 1.5f * std::pow(6.f, clampf(k, 0.f, 1.f)); }  // 1.5 .. 9 Hz
inline float vibDelay(float k) { return 2.f * clampf(k, 0.f, 1.f) * clampf(k, 0.f, 1.f); }  // 0 .. 2 s

// ---------------------------------------------------------------- parameters
struct Params {
	int model = RHODES;
	float timbre = 0.5f, bite = 0.4f, width = 0.4f, sub = 0.f;
	int mode = MODE_POLY;
	float glide = 0.3f, accent = 0.5f;
	int ftype = gmono::LADDER24;
	float cutoff = 0.7f, res = 0.15f, fenv = 0.3f /* -1..1 */, key = 0.5f, fatk = 0.f, fdec = 0.45f, drive = 0.f;
	float atk = 0.f, dec = 0.5f, sus = 1.f, rel = 0.55f;
	float vdepth = 0.f, vrate = 0.5f, vdelay = 0.4f;
	int div = DIV_8D;
	float chorus = 0.2f, echo = 0.2f, fdbk = 0.35f, lofi = 0.f, verb = 0.2f;
	// CV (already scaled): cutoff octaves, resonance (0..1 units), vibrato depth (0..1 units)
	float cutCv = 0.f, resCv = 0.f, vibCv = 0.f;
	// filled in by the engine once per sample (shared by the voices): the envelope times in seconds
	float tAtk = 0.001f, tDec = 0.1f, tRel = 0.1f, tFatk = 0.001f, tFdec = 0.1f;
};

/** One input channel (one GENRA melody line, or one channel of a polyphonic cable). */
struct Channel {
	float voct = 0.f;
	bool gate = false;
	float vel = 0.8f;  // 0..1
	bool acc = false;
	bool slide = false;
};

// ---------------------------------------------------------------- the voice
struct Voice {
	static const int OS = 2;
	static const int KS = 8192;
	float fs = 48000.f, fsOs = 96000.f;

	// note
	int model = RHODES;
	float pitch = 0.f, target = 0.f;  // V (C4 = 0)
	bool gate = false, active = false;
	float vel = 0.8f, acc = 0.f, age = 0.f, pan = 0.f;
	unsigned long order = 0;  // note-on count, for stealing

	// models
	float ph[10] = {};
	float ex[4] = {}, exk[4] = {};  // decaying excitations and their per-sample factors (at fsOs)
	float tau0 = 1.f;
	std::unique_ptr<float[]> ks{new float[KS]()};
	int ksw = 0;
	float ksz = 0.f, ksF = -1.f, ksT = -1.f, ksA = 0.5f, ksD = 100.f, ksG = 0.99f;
	float br1 = 0.f, br2 = 0.f;  // breath resonator
	float click = 0.f;
	uint32_t rng = 0x2545f491u;

	gmono::Filter filter;
	gmono::Biquad d1, d2;
	gmono::Env aenv, fenv;
	float dcx = 0.f, dcy = 0.f;
	float lastEnv = 0.f;

	void setSampleRate(float sr) {
		fs = sr;
		fsOs = sr * OS;
		float f = std::min(0.45f * sr, 20000.f);
		d1.lowpass(f, fsOs, 0.5412f);
		d2.lowpass(f, fsOs, 1.3066f);
	}
	Voice() { setSampleRate(48000.f); }

	float noise() {
		rng ^= rng << 13;
		rng ^= rng >> 17;
		rng ^= rng << 5;
		return (int32_t) rng * 4.6566e-10f;
	}

	/** A new note. strike: start the model's excitation and the envelopes (false: legato, only the pitch moves). */
	void noteOn(const Params& p, float voct, float v, bool accent, bool strike, bool snap) {
		target = voct;
		if (snap || !active) {
			pitch = voct;
			gliding = false;
		}
		gate = true;
		if (!strike)
			return;
		bool wasActive = active;
		active = true;
		model = p.model;
		vel = v;
		acc = accent ? 1.f : 0.f;
		age = 0.f;
		aenv.gate = false;  // retrigger
		fenv.gate = false;
		aenv.stage = 0;
		fenv.stage = 0;
		if (aenv.v < 1e-4f)
			filter.reset();
		float f = C4 * std::pow(2.f, voct);
		float bite = clampf(p.bite * (0.55f + 0.45f * v) + acc * 0.35f * p.accent, 0.f, 1.f);
		auto set = [&](int i, float level, float tau) {
			ex[i] = level;
			exk[i] = decayCoef(tau, fsOs);
		};
		float low = std::pow(220.f / std::max(f, 20.f), 0.5f);  // low notes ring longer
		switch (model) {
			case RHODES:
				for (int i = 0; i < 4; i++)
					ph[i] = 0.f;
				set(0, 1.f, 3.2f * low);    // amplitude
				set(1, 1.f, 0.9f * low);    // tine brightness
				set(2, bite, 0.018f);       // hammer
				set(3, 0.f, 1.f);
				break;
			case ORGAN:
				set(0, 1.f, 1e4f);
				set(1, bite, 0.22f);        // percussion
				set(2, bite, 0.004f);       // key click
				set(3, 0.f, 1.f);
				break;
			case BELL:
				for (int i = 0; i < 4; i++)
					ph[i] = 0.f;
				set(0, 1.f, 3.5f * std::pow(440.f / std::max(f, 20.f), 0.3f));
				set(1, 0.4f + 0.6f * bite, 1.1f);
				set(2, bite, 0.03f);
				set(3, 0.f, 1.f);
				break;
			case MARIMBA:
				for (int i = 0; i < 4; i++)
					ph[i] = 0.f;
				tau0 = 0.55f * low;
				set(0, 1.f, tau0);
				set(1, 1.f, tau0 * 0.22f);
				set(2, 1.f, tau0 * 0.08f);
				set(3, bite, 0.0035f);      // mallet click
				break;
			case PLUCK: {
				// fill the string with noise, low-passed by BITE (the pick)
				float a = 0.08f + 0.9f * bite * bite, z = 0.f;
				int n = std::min(KS - 2, (int) (fsOs / std::max(f, 20.f)) + 2);
				for (int i = 0; i < KS; i++)
					ks[i] = 0.f;
				float mean = 0.f;
				for (int i = 0; i < n; i++) {
					z += (noise() - z) * a;
					ks[i] = z;
					mean += z;
				}
				mean /= n;
				float peak = 1e-6f;
				for (int i = 0; i < n; i++) {
					ks[i] -= mean;
					peak = std::max(peak, std::fabs(ks[i]));
				}
				for (int i = 0; i < n; i++)
					ks[i] /= peak;
				ksw = n;
				ksz = 0.f;
				set(0, 1.f, 1e4f);
				set(1, 0.f, 1.f);
				set(2, 0.f, 1.f);
				set(3, 0.f, 1.f);
			} break;
			case SQUARE:
				set(0, 1.f, 1e4f);
				set(1, bite, 0.03f);        // pitch blip
				set(2, 0.f, 1.f);
				set(3, 0.f, 1.f);
				break;
			case SAWLEAD:
				if (!wasActive)
					ph[0] = 0.f, ph[1] = 0.37f, ph[2] = 0.71f, ph[3] = 0.f;
				set(0, 1.f, 1e4f);
				set(1, 0.f, 1.f);
				set(2, 0.f, 1.f);
				set(3, 0.f, 1.f);
				break;
			case FLUTE:
				set(0, 1.f, 1e4f);
				set(1, bite, 0.045f);       // chiff
				set(2, 0.f, 1.f);
				set(3, 0.f, 1.f);
				break;
			default: break;
		}
	}
	void noteOff() { gate = false; }

	/** One sample of the model at the oversampled rate: f = frequency (Hz). */
	float core(const Params& p, float f) {
		float dt = f / fsOs;
		float T = clampf(p.timbre, 0.f, 1.f);
		float B = clampf(p.bite * (0.55f + 0.45f * vel) + acc * 0.35f * p.accent, 0.f, 1.f);
		float y = 0.f;
		switch (model) {
			default:
			case RHODES: {
				// carrier and tine modulator at 1:1, the hammer adds a fast high partial
				ph[0] = wrap(ph[0] + dt);
				ph[1] = wrap(ph[1] + dt);
				ph[2] = wrap(ph[2] + dt * 7.03f);
				float idx = (0.15f + 1.6f * T) * ex[1] + 2.2f * ex[2];
				float c = sin2pi(ph[0] + idx * 0.16f * sin2pi(ph[1]));
				if (7.03f * f < 0.4f * fsOs)
					c += 0.22f * ex[2] * sin2pi(ph[2]);
				c += 0.35f * T * c * c;  // the bark: even harmonics, stronger with TIMBRE
				y = 0.9f * c * ex[0];
			} break;
			case ORGAN: {
				// drawbars 16' 8' 5⅓' 4' 2⅔' 2' 1⅗' 1⅓' 1', four registrations morphed by TIMBRE
				static const float R[9] = {0.5f, 1.f, 1.5f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f};
				static const float REG[4][9] = {{0.35f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f},
				                                {0.6f, 1.f, 0.55f, 0.45f, 0.f, 0.f, 0.f, 0.f, 0.f},
				                                {0.8f, 1.f, 0.8f, 0.8f, 0.35f, 0.45f, 0.f, 0.f, 0.2f},
				                                {1.f, 1.f, 1.f, 1.f, 0.6f, 0.8f, 0.45f, 0.5f, 0.7f}};
				float x = T * 3.f;
				int r = std::min((int) x, 2);
				float t = x - r, sum = 0.f, norm = 0.f;
				for (int i = 0; i < 9; i++) {
					float lv = REG[r][i] + (REG[r + 1][i] - REG[r][i]) * t;
					if (i == 3)
						lv += 0.6f * ex[1];  // percussion on 4' and 2⅔'
					if (i == 4)
						lv += 0.45f * ex[1];
					ph[i] = wrap(ph[i] + dt * R[i]);
					if (lv > 0.f && R[i] * f < 0.42f * fsOs)
						sum += lv * sin2pi(ph[i]);
					norm += lv;
				}
				y = sum / std::max(1.f, 0.55f * norm);
				click += (noise() - click) * 0.5f;
				y += 0.5f * ex[2] * click;
				y *= 0.8f;
			} break;
			case BELL: {
				// FM with an inharmonic ratio (TIMBRE), a second chime partial
				float ratio = 1.41f + T * (5.19f - 1.41f);
				ph[0] = wrap(ph[0] + dt);
				ph[1] = wrap(ph[1] + dt * ratio);
				ph[2] = wrap(ph[2] + dt * 2.76f);
				float idx = (0.6f + 2.4f * ex[1]) * (0.5f + 0.5f * B) + 2.f * ex[2];
				float c = sin2pi(ph[0] + idx * 0.16f * sin2pi(ph[1]));
				if (2.76f * f < 0.4f * fsOs)
					c += 0.3f * sin2pi(ph[2]) * ex[1];
				y = 0.75f * c * ex[0];
			} break;
			case MARIMBA: {
				static const float R[3] = {1.f, 3.99f, 9.2f};
				float lv[3] = {1.f, (0.12f + 0.55f * T) * (1.f + 1.2f * B), 0.45f * T * T * (1.f + 2.f * B) + 0.08f * B};
				for (int i = 0; i < 3; i++) {
					ph[i] = wrap(ph[i] + dt * R[i]);
					if (R[i] * f < 0.42f * fsOs)
						y += lv[i] * ex[i] * sin2pi(ph[i]);
				}
				click += (noise() - click) * 0.35f;
				y += 0.9f * ex[3] * click;
				y *= 0.95f;
			} break;
			case PLUCK: {
				// Karplus-Strong: the loop filter (1-a) + a z⁻¹ darkens with less TIMBRE, the loop gain sets the ring
				// the loop: a one-pole low-pass (darker with less TIMBRE); its delay at f is taken off the line
				if (f != ksF || T != ksT) {
					ksF = f, ksT = T;
					float P = fsOs / std::max(f, 20.f);
					ksA = 0.9f - 0.75f * T;
					float w = 2.f * PI * f / fsOs;
					float lpDelay = std::atan2(ksA * std::sin(w), 1.f - ksA * std::cos(w)) / w;
					ksD = clampf(P - lpDelay, 2.f, (float) (KS - 2));
					// one pass per period: the ring time (−60 dB) grows with TIMBRE, shorter for high notes
					float t60 = (0.5f + 4.f * T) * std::pow(220.f / std::max(f, 20.f), 0.5f);
					ksG = std::pow(10.f, -3.f * (P / fsOs) / t60);
				}
				float a = ksA, d = ksD;
				float rp = ksw - d;
				while (rp < 0.f)
					rp += KS;
				int i0 = (int) rp;
				float fr = rp - i0;
				float s0 = ks[i0 % KS], s1 = ks[(i0 + 1) % KS];
				float s = s0 + (s1 - s0) * fr;
				float lp = (1.f - a) * s + a * ksz;
				ksz = lp;
				ks[ksw % KS] = lp * ksG;
				ksw = (ksw + 1) % KS;
				y = 3.2f * lp;
			} break;
			case SQUARE: {
				// pulse; BITE drops in from an octave above
				float pw = 0.5f - 0.42f * T;
				float fb = f * std::pow(2.f, ex[1]);
				float d = fb / fsOs;
				ph[0] = wrap(ph[0] + d);
				float t = ph[0];
				float v = t < pw ? 1.f : -1.f;
				v += gmono::polyblep(t, d);
				float t2 = t - pw;
				if (t2 < 0.f)
					t2 += 1.f;
				v -= gmono::polyblep(t2, d);
				y = 0.55f * v;
			} break;
			case SAWLEAD: {
				// three saws, ± detune by TIMBRE; BITE adds a saw an octave up and drives the mix
				float cents = 32.f * T * T;
				float m[4] = {1.f, std::pow(2.f, cents / 1200.f), std::pow(2.f, -cents / 1200.f), 2.f};
				float lv[4] = {1.f, 0.75f, 0.75f, 0.7f * B};
				float s = 0.f;
				for (int i = 0; i < 4; i++) {
					float d = dt * m[i];
					ph[i] = wrap(ph[i] + d);
					s += lv[i] * (2.f * ph[i] - 1.f - gmono::polyblep(ph[i], d));
				}
				s *= 0.42f;
				y = sat(s * (1.f + 2.5f * B)) / (1.f + 0.6f * B) * 1.3f;
			} break;
			case FLUTE: {
				ph[0] = wrap(ph[0] + dt);
				float s = sin2pi(ph[0]);
				s += 0.28f * T * sin2pi(2.f * ph[0]) + 0.12f * T * sin2pi(3.f * ph[0]);
				// breath: noise through a resonator at the note, louder with TIMBRE; the chiff on the attack
				float g = std::tan(PI * std::min(f, 0.4f * fsOs) / fsOs), k = 0.25f;
				float n = noise();
				float h = (n - (k + g) * br1 - br2) / (1.f + g * (k + g));
				float bp = g * h + br1;
				br1 = g * h + bp;
				float lp = g * bp + br2;
				br2 = g * bp + lp;
				s += (0.05f + 0.3f * T) * bp * 0.5f;
				s += ex[1] * (0.6f * n + 0.6f * sin2pi(2.f * ph[0]));
				y = 0.8f * s;
			} break;
		}
		for (int i = 0; i < 4; i++) {
			ex[i] *= exk[i];
			if (ex[i] < 1e-7f)
				ex[i] = 0.f;  // no denormals
		}
		return y;
	}

	/** One output sample (stereo, about ±1 at full level). vib: the vibrato in semitones before the delay fade. */
	bool gliding = false;
	void process(const Params& p, float vibSemis, float glideCoef, bool glideOn, float& L, float& R) {
		if (!active) {
			L = R = 0.f;
			lastEnv = 0.f;
			return;
		}
		float dt = 1.f / fs;
		age += dt;
		// a glide, once begun, runs to the target (GENRA's SLIDE is only a short pulse)
		if (glideOn)
			gliding = true;
		if (gliding && glideCoef < 1.f) {
			pitch += (target - pitch) * glideCoef;
			if (std::fabs(target - pitch) < 0.0005f) {
				pitch = target;
				gliding = false;
			}
		} else {
			pitch = target;
			gliding = false;
		}
		// vibrato after its delay, faded in over half the delay time
		float vd = vibDelay(p.vdelay);
		float fade = vd <= 0.001f ? 1.f : clampf((age - vd * 0.5f) / (vd * 0.5f), 0.f, 1.f);
		float f = C4 * std::pow(2.f, pitch + vibSemis * fade / 12.f);
		f = clampf(f, 10.f, 0.4f * fsOs);

		float fe = fenv.process(gate, p.tFatk, p.tFdec, 0.f, p.tFdec, dt);
		float ae = aenv.process(gate, p.tAtk, p.tDec, p.sus, p.tRel, dt);
		lastEnv = ae;
		float envAmt = p.fenv * (1.f + 0.8f * acc * p.accent);
		float oct = std::log2(20.f) + clampf(p.cutoff, 0.f, 1.f) * std::log2(1000.f)
		            + envAmt * 7.f * fe * (0.6f + 0.4f * vel) + p.key * pitch + p.cutCv;
		float fc = std::pow(2.f, clampf(oct, 3.f, 15.f));
		float res = clampf(p.res + p.resCv, 0.f, 1.f);
		float dr = clampf(p.drive, 0.f, 1.f);
		float dg = 1.f + 7.f * dr * dr;
		float subL = clampf(p.sub, 0.f, 1.f);

		float y = 0.f;
		for (int i = 0; i < OS; i++) {
			float x = core(p, f);
			if (subL > 0.f) {
				// the sub follows the model's own decay
				ph[9] = wrap(ph[9] + 0.5f * f / fsOs);
				float se = (model == RHODES || model == BELL || model == MARIMBA) ? ex[0] : 1.f;
				x += 0.75f * subL * se * sin2pi(ph[9]);
			}
			float v = filter.process(x * 0.8f * dg, fc, res, fsOs, p.ftype);
			y = d2.process(d1.process(v));
		}
		y /= 0.65f + 0.35f * std::sqrt(dg);
		// DC from the bark and the drive
		float yy = y - dcx + 0.9995f * dcy;
		dcx = y, dcy = yy;
		y = yy;
		float gain = ae * (0.35f + 0.65f * vel) * (1.f + 0.7f * acc * p.accent);
		y *= gain;
		if (!std::isfinite(y)) {
			y = 0.f;
			filter.reset();
			for (int i = 0; i < KS; i++)
				ks[i] = 0.f;
		}
		float pn = clampf(pan, -1.f, 1.f);
		L = y * std::sqrt(0.5f * (1.f - pn)) * 1.4142f;
		R = y * std::sqrt(0.5f * (1.f + pn)) * 1.4142f;
		// the PLUCK string keeps ringing in its buffer; the voice ends with the envelope
		if (!gate && aenv.stage == 0)
			active = false;
		bool decayed = (model == RHODES || model == BELL || model == MARIMBA) && ex[0] < 1e-4f;
		if (decayed && age > 0.05f)
			active = false;
	}
};

// ---------------------------------------------------------------- the bus: lo-fi, width, chorus, echo, reverb
struct Ring {
	std::vector<float> b;
	int w = 0;
	void resize(int n) {
		b.assign(std::max(n, 4), 0.f);
		w = 0;
	}
	void push(float x) {
		b[w] = x;
		w = (w + 1) % (int) b.size();
	}
	/** d samples ago (fractional, ≥ 1). */
	float read(float d) const {
		int n = (int) b.size();
		d = clampf(d, 1.f, n - 2.f);
		float rp = w - d;
		while (rp < 0.f)
			rp += n;
		int i = (int) rp;
		float f = rp - i;
		float a = b[i % n], c = b[(i + 1) % n];
		return a + (c - a) * f;
	}
};

struct Bus {
	float fs = 48000.f;
	// lo-fi
	Ring wowL, wowR;
	float wowPh = 0.f, flPh = 0.f, holdPh = 0.f, holdL = 0.f, holdR = 0.f, tapeL = 0.f, tapeR = 0.f;
	// width, chorus
	Ring wid, chL, chR;
	float chPh = 0.f, widPh = 0.f, sideHp = 0.f;
	// echo
	Ring eL, eR;
	float eTime = -1.f, eLp[2] = {}, eHp[2] = {};
	gschmelz::Reverb verb;
	float dcx[2] = {}, dcy[2] = {};

	void setSampleRate(float sr) {
		fs = sr;
		wowL.resize((int) (0.02f * sr));
		wowR.resize((int) (0.02f * sr));
		wid.resize((int) (0.01f * sr));
		chL.resize((int) (0.04f * sr));
		chR.resize((int) (0.04f * sr));
		eL.resize((int) (3.2f * sr));
		eR.resize((int) (3.2f * sr));
		eTime = -1.f;
		verb.setSampleRate(sr);
	}
	Bus() { setSampleRate(48000.f); }

	/** echoSeconds: the echo time (from the clock and the division). */
	void process(const Params& p, float inL, float inR, float echoSeconds, float& L, float& R) {
		float l = inL, r = inR;
		// LO-FI: tape wow and flutter, then fewer samples and bits, then a tape low-pass
		float lf = clampf(p.lofi, 0.f, 1.f);
		wowL.push(l);
		wowR.push(r);
		if (lf > 0.001f) {
			wowPh = wrap(wowPh + 0.55f / fs);
			flPh = wrap(flPh + 6.3f / fs);
			float m = (0.0035f * lf * (0.5f + 0.5f * sin2pi(wowPh)) + 0.00025f * lf * sin2pi(flPh) + 0.0005f) * fs;
			l = wowL.read(m);
			r = wowR.read(m + 0.00007f * fs);
			float rate = 1.f / (1.f + 6.f * lf * lf);  // held samples
			holdPh += rate;
			if (holdPh >= 1.f) {
				holdPh -= 1.f;
				float bits = 15.f - 10.f * lf, q = std::pow(2.f, bits);
				holdL = std::round(l * q) / q;
				holdR = std::round(r * q) / q;
			}
			float mix = clampf(lf * 3.f, 0.f, 1.f);
			l += (holdL - l) * mix;
			r += (holdR - r) * mix;
			float a = 1.f - std::exp(-2.f * PI * (14000.f - 10000.f * lf) / fs);
			tapeL += (l - tapeL) * a;
			tapeR += (r - tapeR) * a;
			l = tapeL + (l - tapeL) * (1.f - mix);
			r = tapeR + (r - tapeR) * (1.f - mix);
			l = sat(l * (1.f + 0.6f * lf)) / (1.f + 0.3f * lf);
			r = sat(r * (1.f + 0.6f * lf)) / (1.f + 0.3f * lf);
		} else {
			tapeL = l, tapeR = r;
		}
		// WIDTH: a slowly moving short delay as side signal (the voices' pan spread is the other half)
		float w = clampf(p.width, 0.f, 1.f);
		float mid = 0.5f * (l + r);
		wid.push(mid);
		widPh = wrap(widPh + 0.13f / fs);
		float side = wid.read((0.0045f + 0.0015f * sin2pi(widPh)) * fs) - mid;
		sideHp += (side - sideHp) * (2.f * PI * 250.f / fs);
		side -= sideHp;
		l += 0.35f * w * side;
		r -= 0.35f * w * side;
		// CHORUS: two modulated delays, opposite phases
		float c = clampf(p.chorus, 0.f, 1.f);
		chL.push(l);
		chR.push(r);
		chPh = wrap(chPh + 0.45f / fs);
		if (c > 0.001f) {
			float dl = (0.0075f + 0.0028f * sin2pi(chPh)) * fs, drr = (0.0075f + 0.0028f * sin2pi(chPh + 0.5f)) * fs;
			float wl = chL.read(dl), wr = chR.read(drr);
			l = l * (1.f - 0.3f * c) + wl * 0.75f * c;
			r = r * (1.f - 0.3f * c) + wr * 0.75f * c;
		}
		// ECHO: ping-pong, darker each repeat; the time glides when the tempo changes
		float target = clampf(echoSeconds, 0.01f, 3.f) * fs;
		if (eTime < 0.f)
			eTime = target;
		eTime += (target - eTime) * (1.f - std::exp(-1.f / (0.08f * fs)));
		float yl = eL.read(eTime), yr = eR.read(eTime);
		float fb = clampf(p.fdbk, 0.f, 1.f) * 0.92f;
		float a = 1.f - std::exp(-2.f * PI * 4200.f / fs), h = 2.f * PI * 120.f / fs;
		float in[2] = {yr * fb + 0.5f * (l + r) * 0.7f, yl * fb};  // into left: the right repeat + the input
		for (int i = 0; i < 2; i++) {
			eLp[i] += (in[i] - eLp[i]) * a;
			eHp[i] += (eLp[i] - eHp[i]) * h;
			in[i] = sat(eLp[i] - eHp[i]);
		}
		eL.push(in[0]);
		eR.push(in[1]);
		float e = clampf(p.echo, 0.f, 1.f);
		l += yl * e * 0.9f;
		r += yr * e * 0.9f;
		// REVERB
		float v = clampf(p.verb, 0.f, 1.f), vl, vr;
		verb.process(l, r, 0.72f, vl, vr);
		l = l * (1.f - 0.3f * v) + vl * v * 1.3f;
		r = r * (1.f - 0.3f * v) + vr * v * 1.3f;
		float o[2] = {l, r};
		for (int i = 0; i < 2; i++) {
			float y = o[i] - dcx[i] + 0.9995f * dcy[i];
			dcx[i] = o[i], dcy[i] = y;
			o[i] = y;
		}
		L = o[0], R = o[1];
	}
};

// ---------------------------------------------------------------- the engine: voice allocation, glide, vibrato, bus
struct Engine {
	float fs = 48000.f;
	std::unique_ptr<Voice[]> v{new Voice[MAXV]};
	std::unique_ptr<Bus> bus{new Bus};
	Channel last[MAXCH];
	int owner[MAXCH];       // voice per channel (poly), -1 none
	int stack[MAXCH] = {};  // mono: held channels in the order they came
	int held = 0;
	int monoCh = -1;
	unsigned long count = 0;
	float vibPh = 0.f;
	int lastVoice = 0;
	int lastMode = -1;
	// for the display
	float lastNote = 0.f;
	bool showGlide = false, showAcc = false;

	Engine() {
		for (int i = 0; i < MAXCH; i++)
			owner[i] = -1;
		setSampleRate(48000.f);
	}
	void setSampleRate(float sr) {
		fs = sr;
		for (int i = 0; i < MAXV; i++)
			v[i].setSampleRate(sr);
		bus->setSampleRate(sr);
	}
	void allOff() {
		for (int i = 0; i < MAXV; i++) {
			v[i].noteOff();
			v[i].active = false;
			v[i].aenv.v = 0.f;
			v[i].aenv.stage = 0;
		}
		for (int i = 0; i < MAXCH; i++) {
			owner[i] = -1;
			last[i].gate = false;
		}
		held = 0;
		monoCh = -1;
	}

	int freeVoice() {
		int best = 0;
		float score = 1e9f;
		for (int i = 0; i < MAXV; i++) {
			const Voice& x = v[i];
			// idle first, then released (quietest), then the oldest playing
			float s = !x.active ? -1e6f + i : (!x.gate ? x.lastEnv : 1e3f + (float) (x.order % 1000000));
			if (s < score)
				score = s, best = i;
		}
		return best;
	}

	/** ch: the channels (n of them); slideConn: SLIDE is patched; echoSeconds from the clock. */
	void process(const Params& p, const Channel* ch, int n, bool slideConn, float echoSeconds, float& L, float& R,
	             float& env) {
		n = std::min(n, MAXCH);
		Params q = p;
		q.tAtk = gmono::knobTime(p.atk), q.tDec = gmono::knobTime(p.dec), q.tRel = gmono::knobTime(p.rel);
		q.tFatk = gmono::knobTime(p.fatk), q.tFdec = gmono::knobTime(p.fdec);
		if (p.mode != lastMode) {
			if (lastMode >= 0)
				allOff();
			lastMode = p.mode;
		}
		float dt = 1.f / fs;
		float gs = glideSeconds(p.glide);
		float gc = gs > 1e-4f ? 1.f - std::exp(-dt / gs) : 1.f;
		vibPh = wrap(vibPh + vibRate(p.vrate) * dt);
		float depth = clampf(p.vdepth + p.vibCv, 0.f, 1.f);
		float vib = depth * depth * 0.9f * sin2pi(vibPh);  // up to ±0.9 semitone

		if (p.mode == MODE_POLY) {
			for (int c = 0; c < n; c++) {
				const Channel& in = ch[c];
				bool rise = in.gate && !last[c].gate, fall = !in.gate && last[c].gate;
				if (rise) {
					int k = freeVoice();
					for (int o = 0; o < MAXCH; o++)
						if (owner[o] == k)
							owner[o] = -1;
					owner[c] = k;
					v[k].order = ++count;
					v[k].pan = ((count % 2) ? 1.f : -1.f) * 0.55f * clampf(p.width, 0.f, 1.f) * (0.6f + 0.4f * ((count / 2) % 2));
					v[k].noteOn(p, in.voct, in.vel, in.acc, true, true);
					lastVoice = k;
					lastNote = in.voct;
					showAcc = in.acc;
				} else if (fall && owner[c] >= 0) {
					v[owner[c]].noteOff();
					owner[c] = -1;
				}
				if (in.gate && owner[c] >= 0) {
					Voice& x = v[owner[c]];
					if (in.voct != x.target) {
						x.target = in.voct;  // a slide or a bend while held
						lastNote = in.voct;
					}
				}
				last[c] = in;
			}
			for (int c = n; c < MAXCH; c++)
				if (last[c].gate) {
					if (owner[c] >= 0)
						v[owner[c]].noteOff();
					owner[c] = -1;
					last[c].gate = false;
				}
		} else {
			// MONO / LEGATO: one voice, the newest held note sounds
			Voice& x = v[0];
			lastVoice = 0;
			bool legato = p.mode == MODE_LEGATO;
			for (int c = 0; c < MAXCH; c++) {
				Channel in = c < n ? ch[c] : Channel();
				if (c >= n)
					in.gate = false;
				bool rise = in.gate && !last[c].gate, fall = !in.gate && last[c].gate;
				if (fall) {
					int j = 0;
					for (int i = 0; i < held; i++)
						if (stack[i] != c)
							stack[j++] = stack[i];
					held = j;
				}
				if (rise && held < MAXCH)
					stack[held++] = c;
				if (rise) {
					bool wasHeld = held > 1 || (monoCh >= 0 && monoCh != c && last[monoCh].gate);
					bool strike = !(legato && wasHeld);
					bool glide = legato ? (wasHeld || !slideConn) : (!slideConn);
					x.order = ++count;
					x.pan = 0.f;
					x.noteOn(p, in.voct, in.vel, in.acc, strike, !glide);
					monoCh = c;
					lastNote = in.voct;
					showAcc = in.acc;
				}
				if (c < n)
					last[c] = in;
				else
					last[c].gate = false;
			}
			if (held > 0) {
				int top = stack[held - 1];
				if (top != monoCh) {
					// the newest note was let go: back to the one held before, without a new attack
					monoCh = top;
					x.target = ch[top].voct;
				} else if (ch[top].voct != x.target) {
					x.target = ch[top].voct;
				}
				lastNote = x.target;
			} else if (x.gate) {
				x.noteOff();
			}
		}

		float l = 0.f, r = 0.f;
		bool anySlide = false;
		for (int i = 0; i < MAXV; i++) {
			Voice& x = v[i];
			if (!x.active)
				continue;
			// glide: on GENRA's SLIDE; with SLIDE unplugged in MONO/LEGATO always; LEGATO also between held notes
			bool slide = false;
			for (int c = 0; c < n; c++)
				if (ch[c].slide && (p.mode != MODE_POLY || owner[c] == i))
					slide = true;
			bool g = slide || (p.mode != MODE_POLY && !slideConn) || (p.mode == MODE_LEGATO && held > 1);
			if ((g || x.gliding) && std::fabs(x.target - x.pitch) > 0.002f)
				anySlide = true;
			float a, b;
			x.process(q, vib, gc, g, a, b);
			l += a, r += b;
		}
		showGlide = anySlide;
		env = v[lastVoice < MAXV ? lastVoice : 0].lastEnv;
		float sc = 1.1f;
		bus->process(p, l * sc, r * sc, echoSeconds, L, R);
		L = 5.f * sat(L * 0.8f);
		R = 5.f * sat(R * 0.8f);
	}
};

/** The note name of a V/OCT voltage (C4 = 0 V), e.g. "E4". */
inline void noteName(float voct, char* out) {
	static const char* const N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
	int semi = (int) std::lround(voct * 12.f) + 48;
	int o = (int) std::floor(semi / 12.f);
	int s = semi - o * 12;
	snprintf(out, 8, "%s%d", N[s], o);
}

}  // namespace gfunke
