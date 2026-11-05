// SPDX-License-Identifier: MIT AND GPL-3.0-only
//
// MONK's voice: MonkSynth's DSP (dsp/voice.c, synth.c, delay.c), Copyright (c) 2026 Jonathan Taylor, MIT License:
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
//   documentation files (the "Software"), to deal in the Software without restriction, including without limitation
//   the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
//   permit persons to whom the Software is furnished to do so, subject to the following conditions: The above
//   copyright notice and this permission notice shall be included in all copies or substantial portions of the
//   Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
//
// as Charles Vestal ported it to the M-VAVE FM-1 for MONK-1 (firmware/src/dsp/monk.c, GPL-3.0-only; github.com/
// charlesvestal/fm1-monk), here as a class for VCV Rack. What changed from MONK-1's monk.c, and why:
//   - one object per module instance (the firmware keeps everything in statics), no command queue: Rack calls it from
//     the engine thread only.
//   - any sample rate: the grain, its window, the decays, the envelope, the glides, the vibrato and the echo are sized
//     from `fs` (MONK-1: 44.1 kHz fixed). The echo runs at the full rate in float (MONK-1: half rate in int16, for
//     memory), with the same tap times and the same +-32 ceiling.
//   - FEEDBACK is a parameter (MONK-1 and upstream: 0.5).
//   - notes from a gate and V/OCT instead of a key stack: gateOn() is a key pressed (attack, from where the envelope
//     is), pitch() moves the held note (it glides, as moving between held keys does, without a new attack), gateOff()
//     lets go. Pitch is in MIDI note units and may be fractional.
//   - the knobs' slew and the velocity's glide are per block as in MONK-1 (256 samples at 44.1 kHz), scaled to the
//     block and rate used here.
// The sound otherwise follows MONK-1 (and so upstream) step for step: the FOF grain of three damped formant sines and
// breath, the five vowels on Catmull-Rom splines, the 32-steps-a-semitone period, the vibrato with its jitter, the two
// glides, the ADSR, the unison detune and spread, the stereo echo's taps, pressure into the vowel or the pitch.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace monkdsp {

// the parameters, normalized 0..1 as upstream stores them (iOS Params.swift order), and the new FEEDBACK
enum {
	GLIDE, VOWEL, DELAY, HEAD, VIBRATO, VIBRATE, BREATH, ATTACK, DECAY, SUSTAIN, RELEASE, VOICES, DETUNE, DELAYRATE,
	LEVEL, SPREAD, TUNE, FEEDBACK,
	NPARAM
};
// where pressure goes
enum { AT_VOWEL, AT_PITCH, AT_BOTH, AT_BOTH_INV, NAT };

static const int NUM_FORMANTS = 3, SPLINE_SIZE = 1280, SPLINE_SEG = 320, SINE_SIZE = 1024, MAX_UNISON = 10;
static const float PITCH_TABLE_BASE = 8.175799f, ENV_ANTICLICK = 0.003f, PI = 3.14159265358979f;
static const int RAMP_TICKS = 10;
static const float FORMANT_FREQS[NUM_FORMANTS][5] = {
    {280.0f, 450.0f, 800.0f, 350.0f, 270.0f},
    {600.0f, 800.0f, 1150.0f, 2000.0f, 2140.0f},
    {2240.0f, 2830.0f, 2900.0f, 2800.0f, 2950.0f},
};
static const float FORMANT_BW[NUM_FORMANTS] = {32.5f, 47.5f, 62.5f};

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float clamp01(float v) { return clampf(v, 0.0f, 1.0f); }

/** The tables a voice reads (one set per Monk, sized for its rate). */
struct Tables {
	float fs = 44100.f;
	int grainLen = 882;
	float sine[SINE_SIZE];
	float formant[NUM_FORMANTS][SPLINE_SIZE];
	std::vector<float> env[NUM_FORMANTS];   // the formant's decay times the window
	std::vector<float> asp;                 // the aspiration times the window
	float p2[384];                          // 2^(k / 384)

	void build(float sr) {
		fs = sr;
		grainLen = (int) (sr * 0.02f);
		for (int i = 0; i < SINE_SIZE; i++)
			sine[i] = std::sin(2.f * PI * (float) i / SINE_SIZE);
		for (int f = 0; f < NUM_FORMANTS; f++) {   // Catmull-Rom across the five vowels
			const float* pts = FORMANT_FREQS[f];
			float padded[7] = {pts[0], pts[0], pts[1], pts[2], pts[3], pts[4], pts[4]};
			for (int seg = 0; seg < 4; seg++) {
				float p0 = padded[seg], p1 = padded[seg + 1], p2_ = padded[seg + 2], p3 = padded[seg + 3];
				for (int i = 0; i < SPLINE_SEG; i++) {
					float t = (float) i / SPLINE_SEG, t2 = t * t, t3 = t2 * t;
					formant[f][seg * SPLINE_SEG + i] =
					    0.5f * ((2.0f * p1) + (-p0 + p2_) * t + (2.0f * p0 - 5.0f * p1 + 4.0f * p2_ - p3) * t2 +
					            (-p0 + 3.0f * p1 - 3.0f * p2_ + p3) * t3);
				}
			}
		}
		// the grain window: a short cosine attack, flat, a cosine release that stops short of zero
		float bw = 50.0f * PI, q1 = sr * 0.000202f, q2 = sr * 0.000263f;
		uint32_t attack_len = (uint32_t) (sr * 0.0018f), release_start = (uint32_t) (sr * 0.013f);
		uint32_t release_len = (uint32_t) (sr * 0.007f), decay_len = (uint32_t) grainLen * 4u;
		for (int f = 0; f < NUM_FORMANTS; f++)
			env[f].assign(grainLen, 0.f);
		asp.assign(grainLen, 0.f);
		for (uint32_t n = 0; n < (uint32_t) grainLen; n++) {
			float w = 1.0f, d1 = 0.0f, d2 = 0.0f;
			if (n < attack_len)
				w = 0.5f * (1.0f - std::cos(PI * (float) n / (float) attack_len));
			if (n >= release_start)
				w = 0.5f * (1.0f - std::cos(PI * (float) (release_len + n) / (float) release_len));
			for (int f = 0; f < NUM_FORMANTS; f++) {
				uint32_t di = (uint32_t) (FORMANT_BW[f] * (float) n / 50.0f);
				env[f][n] = (di < decay_len ? std::exp(-bw * (float) di / sr) : 0.0f) * w;
			}
			float s1 = std::sin(2.f * PI * (float) n / q1);   // the breath: two inharmonic decaying sines
			uint32_t i1 = (uint32_t) ((float) n * 3.0f);
			if (i1 < decay_len)
				d1 = std::exp(-bw * (float) i1 / sr);
			float s2 = std::sin(2.f * PI * (float) n / q2);
			uint32_t i2 = (uint32_t) ((float) n * 3.5f);
			if (i2 < decay_len)
				d2 = std::exp(-bw * (float) i2 / sr);
			asp[n] = (s1 * d1 + s2 * d2) * w;
		}
		for (int i = 0; i < 384; i++)
			p2[i] = std::exp2((float) i / 384.0f);
	}

	float lookupFormant(int f, float vowel) const {
		float pos = clamp01(vowel) * (SPLINE_SIZE - 1);
		int idx = (int) pos;
		if (idx > SPLINE_SIZE - 2)
			idx = SPLINE_SIZE - 2;
		float frac = pos - (float) idx;
		return formant[f][idx] * (1.0f - frac) + formant[f][idx + 1] * frac;
	}
};

enum { ENV_IDLE, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

struct Voice {
	const Tables* T = nullptr;
	bool active = false;
	float current_pitch = 57.f, target_pitch = 57.f;   // note units
	float pitch_bend_offset = 0.f;
	float glide_param = 0.f, min_glide = 0.f;
	float pitch_step = 0.f;
	int pitch_ramp_ticks = 0;
	float vowel_step = 0.f;
	int vowel_ramp_ticks = 0;
	int ramp_counter = 0, ramp_period = 441;
	float vibrato_phase = 0.f, vibrato_depth = 0.f, vibrato_rate = 0.5f, rate_scale = 1.f;
	float random_jitter = 4.f;
	uint32_t jitter_counter = 0, jitter_period = 4586;
	uint32_t rng_state = 12345u;
	uint32_t write_pos = 0, read_pos = 0, offset = 0, olapMask = 0;
	float current_vowel = 0.5f, target_vowel = 0.5f, current_voice = 0.5f, aspiration_amp = 0.5f;
	bool grain_dirty = true;
	float g_vowel = -1.f, g_voice = -1.f, g_asp = -1.f;   // what the grain in grain[] was made of
	int last_step = 0x7FFFFFFF;
	uint32_t last_period = 1;
	int env_stage = ENV_IDLE;
	float env_level = 0.f, env_attack = 0.f, env_decay = 0.f, env_sustain = 1.f, env_release = 0.f;
	std::vector<float> olap, grain;

	void init(const Tables* t, int i, uint32_t block) {
		T = t;
		uint32_t need = (uint32_t) t->grainLen + block + 2u, n = 1;
		while (n < need)
			n <<= 1;
		olap.assign(n, 0.f);
		olapMask = n - 1u;
		grain.assign(t->grainLen, 0.f);
		active = false;
		current_pitch = target_pitch = 57.0f;   // 220 Hz
		pitch_bend_offset = 0.0f;
		glide_param = min_glide = 0.0f;
		pitch_ramp_ticks = vowel_ramp_ticks = 0;
		random_jitter = 4.0f;
		jitter_counter = 0;
		jitter_period = (uint32_t) (4586.f * t->fs / 44100.f);
		rng_state = 12345u + (uint32_t) i * 7919u;   // staggered: unison voices are separate singers
		vibrato_phase = (float) (SINE_SIZE * i) / MAX_UNISON;
		vibrato_depth = 0.0f;
		vibrato_rate = 0.5f;
		rate_scale = 1.0f;
		ramp_period = (int) (t->fs * 0.01f);
		ramp_counter = ramp_period;
		current_vowel = target_vowel = 0.5f;
		current_voice = 0.5f;
		aspiration_amp = 0.5f;
		grain_dirty = true;
		g_vowel = -1.0f;
		last_step = 0x7FFFFFFF;
		write_pos = read_pos = offset = 0;
		env_stage = ENV_IDLE;
		env_level = 0.0f;
		env_attack = env_decay = env_release = 0.0f;
		env_sustain = 1.0f;
	}

	float nextRand() {
		rng_state = rng_state * 1103515245u + 12345u;
		return (float) ((rng_state >> 16) & 0x7FFF) / 32768.0f;
	}

	float envTick() {
		float rate, fs = T->fs;
		switch (env_stage) {
			case ENV_ATTACK:
				rate = 1.0f / (std::max(env_attack, ENV_ANTICLICK) * fs);
				env_level += rate;
				if (env_level >= 1.0f) {
					env_level = 1.0f;
					env_stage = env_decay > 0.0f ? ENV_DECAY : ENV_SUSTAIN;
				}
				break;
			case ENV_DECAY:
				rate = env_decay > 0.0f ? 1.0f / (env_decay * fs) : 1.0f;
				env_level -= rate * (1.0f - env_sustain);
				if (env_level <= env_sustain) {
					env_level = env_sustain;
					env_stage = ENV_SUSTAIN;
				}
				break;
			case ENV_RELEASE:
				rate = 1.0f / (std::max(env_release, ENV_ANTICLICK) * fs);
				env_level -= rate;
				if (env_level <= 0.0f) {
					env_level = 0.0f;
					env_stage = ENV_IDLE;
					active = false;
				}
				break;
			default:
				break;
		}
		return env_level;
	}

	// the grain period: the pitch quantized to 32 steps a semitone (audible, and intended)
	uint32_t grainPeriod(float note) {
		int step = (int) std::floor((note - 12.0f) * 32.0f);
		if (step == last_step)
			return last_period;
		last_step = step;
		int oct = step >= 0 ? step / 384 : -((-step + 383) / 384);
		int k = step - oct * 384;
		float freq = T->p2[k] * PITCH_TABLE_BASE;
		freq = oct >= 0 ? freq * (float) (1u << (oct > 20 ? 20 : oct)) : freq / (float) (1u << (-oct > 20 ? 20 : -oct));
		last_period = std::max(1u, (uint32_t) (T->fs / freq));
		return last_period;
	}

	void applyPortamento() {
		if (min_glide > 0.001f) {
			if (--ramp_counter > 0)
				return;
			ramp_counter = ramp_period;
			if (pitch_ramp_ticks > 0) {
				pitch_ramp_ticks--;
				current_pitch += pitch_step;
				if (pitch_ramp_ticks == 0)
					current_pitch = target_pitch;
			}
			if (vowel_ramp_ticks > 0) {
				vowel_ramp_ticks--;
				current_vowel += vowel_step;
				if (vowel_ramp_ticks == 0)
					current_vowel = target_vowel;
			}
		} else if (glide_param > 0.001f) {
			float diff = target_pitch - current_pitch;
			float rate = 12.0f / ((glide_param + 0.01f) * T->fs);
			if (diff > 0.2f)
				current_pitch += rate;
			else if (diff < -0.2f)
				current_pitch -= rate;
			else
				current_pitch = target_pitch;
			current_vowel = target_vowel;
		} else {
			current_pitch = target_pitch;
			current_vowel = target_vowel;
		}
	}

	float computeVibrato() {
		float rate = (vibrato_depth * 0.2f + 1.0f) * random_jitter * rate_scale;
		vibrato_phase += rate * ((float) SINE_SIZE / T->fs);
		while (vibrato_phase >= (float) SINE_SIZE)
			vibrato_phase -= (float) SINE_SIZE;
		if (++jitter_counter >= jitter_period) {
			jitter_counter = 0;
			random_jitter = nextRand() * 2.0f + 5.0f;
		}
		return (vibrato_depth + 0.2f) * T->sine[(int) vibrato_phase % SINE_SIZE];
	}

	void computeGrain() {
		float fsc = current_voice * 0.5f + 0.75f, k[NUM_FORMANTS], a = aspiration_amp;
		const float *e0 = T->env[0].data(), *e1 = T->env[1].data(), *e2 = T->env[2].data(), *as = T->asp.data();
		for (int f = 0; f < NUM_FORMANTS; f++)   // sine-table steps per sample
			k[f] = T->lookupFormant(f, current_vowel) * fsc * ((float) SINE_SIZE / T->fs);
		for (int i = 0; i < T->grainLen; i++) {
			float fi = (float) i;
			grain[i] = T->sine[(uint32_t) (fi * k[0]) & (SINE_SIZE - 1)] * e0[i] +
			           T->sine[(uint32_t) (fi * k[1]) & (SINE_SIZE - 1)] * e1[i] +
			           T->sine[(uint32_t) (fi * k[2]) & (SINE_SIZE - 1)] * e2[i] + as[i] * a;
		}
		g_vowel = current_vowel;
		g_voice = current_voice;
		g_asp = a;
		grain_dirty = false;
	}

	void overlapAdd() {
		write_pos = (write_pos + offset) & olapMask;
		uint32_t s = write_pos, n = (uint32_t) T->grainLen;
		for (uint32_t i = 0; i < n; i++)
			olap[(s + i) & olapMask] += grain[i];
	}

	bool hasEnv() const { return env_attack > 0.0f || env_decay > 0.0f || env_sustain < 1.0f || env_release > 0.0f; }

	void noteOn(float note) {
		bool was = active;
		active = true;
		target_pitch = note;
		if (!was) {
			current_pitch = note;
			current_vowel = target_vowel;   // start on the vowel, not where the last note left it
			vowel_ramp_ticks = 0;
			grain_dirty = true;
			write_pos = read_pos;           // the buffer drains by itself: no pop
			offset = 0;
		}
		if (hasEnv()) {
			env_stage = ENV_ATTACK;         // from where it is: no click on a retrigger
		} else {
			env_level = 1.0f;
			env_stage = ENV_SUSTAIN;
		}
	}

	void noteOff() {
		if (hasEnv()) {
			env_stage = ENV_RELEASE;
		} else {
			active = false;
			env_stage = ENV_IDLE;
			env_level = 0.0f;
		}
	}

	bool isActive() const { return active || env_stage == ENV_RELEASE; }

	void setVowel(float vowel) {
		float t = clamp01(vowel), eff = glide_param > min_glide ? glide_param : min_glide;
		if (eff < 0.001f) {
			current_vowel = t;
			vowel_ramp_ticks = 0;
		} else {
			int n = RAMP_TICKS;
			if (eff > 0.5f)
				n = RAMP_TICKS + (int) ((eff - 0.5f) * 180.0f);
			vowel_step = (t - current_vowel) / (float) n;
			vowel_ramp_ticks = n;
		}
		target_vowel = t;
	}

	float amplitude() const {   // upstream's pitch compensation
		return clampf((current_pitch - 12.0f) * (-1.0f / 72.0f) + 2.0f, 0.1f, 3.0f);
	}

	void process(float* out, uint32_t n) {
		bool env = hasEnv() || env_stage == ENV_RELEASE;
		for (uint32_t i = 0; i < n; i++) {
			if (active || env_stage == ENV_RELEASE) {
				applyPortamento();
				float pitch = current_pitch + pitch_bend_offset + computeVibrato();
				uint32_t period = grainPeriod(pitch);
				if (offset >= period || grain_dirty) {
					if (grain_dirty || current_vowel != g_vowel || current_voice != g_voice || aspiration_amp != g_asp)
						computeGrain();
					overlapAdd();
					offset = 0;
				}
			}
			offset++;
		}
		for (uint32_t i = 0; i < n; i++) {
			uint32_t r = read_pos & olapMask;
			float s = olap[r];
			olap[r] = 0.0f;
			read_pos = r + 1u;
			if (env)
				s *= envTick();
			out[i] += s;
		}
	}
};

/** The stereo echo: dry + two taps, each channel feeding back into itself. */
struct Delay {
	std::vector<float> l, r;
	uint32_t n = 1, w = 0;
	float fs = 44100.f, target_l = 0, target_r = 0, cur_l = 0, cur_r = 0, coeff = 0, rate = 0.5f, feedback = 0.5f,
	      mix = 0.5f;

	void init(float sr) {
		fs = sr;
		n = (uint32_t) (17570.f * 2.f * sr / 44100.f) + 8u;
		l.assign(n, 0.f);
		r.assign(n, 0.f);
		w = 0;
		coeff = 1.0f / (0.05f * sr);   // ~50 ms smoothing
		taps();
		cur_l = target_l;
		cur_r = target_r;
	}
	void taps() {
		float rs = 0.1f + rate * 1.9f;   // ~310 / ~398 ms at rate 0.5
		target_l = std::min(13653.0f * rs * fs / 44100.f, (float) (n - 2u));
		target_r = std::min(17570.0f * rs * fs / 44100.f, (float) (n - 2u));
	}
	float read(const std::vector<float>& b, float pos) const {
		float rd = (float) w - pos;
		if (rd < 0.0f)
			rd += (float) n;
		int i = (int) rd;
		float fr = rd - (float) i;
		return b[i % n] * (1.0f - fr) + b[(uint32_t) (i + 1) % n] * fr;
	}
	void process(const float* in, float* ol, float* orr, uint32_t len) {
		for (uint32_t i = 0; i < len; i++) {
			cur_l += coeff * (target_l - cur_l);
			cur_r += coeff * (target_r - cur_r);
			float tl = read(l, cur_l), tr = read(r, cur_r);
			l[w] = clampf((tl * feedback + in[i]) * mix, -32.f, 32.f);
			r[w] = clampf((tr * feedback + in[i]) * mix, -32.f, 32.f);
			w = (w + 1u) % n;
			ol[i] = in[i] + tl;
			orr[i] = in[i] + tr;
		}
	}
};

/** The whole voice: the unison, the echo, and MONK-1's wrapper (slewed knobs, pressure, velocity). */
struct Monk {
	static const uint32_t BLOCK = 64;
	Tables T;
	Voice v[MAX_UNISON];
	Delay D;
	int unison_count = 1;
	float unison_detune = 0.f, unison_voice_spread = 0.f, base_voice = 0.5f, last_base_note = 57.f;
	bool held = false;
	float cc_volume = 0.1f, level = 1.f, cur_gain = 1.f, target_gain = 1.f;

	// the wrapper
	float p[NPARAM], cur[NPARAM];
	float expr = 0.f, expr_cur = 0.f;
	int route = AT_VOWEL;
	float depth = 0.5f, vel_sens = 0.5f, vel_gain = 1.f, vel_target = 1.f, bend_cur = 1e9f;
	float slew = 0.15f, velSlew = 0.2f;

	// what the panel shows (written by render)
	float uiVowel = 0.5f, uiAmp = 0.f, uiEnv = 0.f, uiPitch = 57.f;

	float mono[BLOCK], wl[BLOCK], wr[BLOCK];

	static bool smooth(int id) {   // knobs slewed rather than stepped (the Schwung port's choice)
		return id == VOWEL || id == DELAY || id == HEAD || id == VIBRATO || id == VIBRATE || id == BREATH ||
		       id == DETUNE || id == LEVEL || id == SPREAD || id == TUNE || id == FEEDBACK;
	}

	void init(float sr) {
		static const float DEF[NPARAM] = {   // upstream's defaults (processor.h), FEEDBACK as upstream's fixed 0.5
		    0.5f, 0.5f, 0.8f, 0.5f, 0.0f, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.5f, 0.5f};
		T.build(sr);
		for (int i = 0; i < MAX_UNISON; i++)
			v[i].init(&T, i, BLOCK);
		unison_count = 1;
		unison_detune = 0.0f;
		cur_gain = target_gain = 1.0f;
		held = false;
		cc_volume = 0.1f;
		level = 1.0f;
		base_voice = 0.5f;
		D.init(sr);
		float ratio = (float) BLOCK * 44100.f / (256.f * sr);   // MONK-1 slews per 256 samples at 44.1 kHz
		slew = 1.f - std::pow(1.f - 0.15f, ratio);
		velSlew = 1.f - std::pow(1.f - 0.2f, ratio);
		route = AT_VOWEL;
		depth = 0.5f;
		vel_sens = 0.5f;
		vel_gain = vel_target = 1.0f;
		expr = expr_cur = 0.0f;
		bend_cur = 1e9f;
		for (int i = 0; i < NPARAM; i++) {
			p[i] = cur[i] = DEF[i];
			engineParam(i, DEF[i]);
		}
		for (auto& x : v)
			x.setVowel(0.5f);
		applyBend();
	}

	// ---------------------------------------------------------------- the synth
	float detuned(float base, int i, int n) const {
		if (n <= 1)
			return base;
		return base + unison_detune * ((float) i / (float) (n - 1) * 2.0f - 1.0f) * 0.01f;   // cents
	}
	void applyVoiceSpread() {
		int n = unison_count;
		for (int i = 0; i < n; i++) {
			float off = n > 1 ? unison_voice_spread * ((float) i / (float) (n - 1) * 2.0f - 1.0f) : 0.0f;
			v[i].current_voice = clamp01(base_voice + off);
		}
	}
	void applyUnison(float base) {
		last_base_note = base;
		for (int i = 0; i < unison_count; i++)
			v[i].noteOn(detuned(base, i, unison_count));
	}
	void setUnison(int count) {
		int old = unison_count;
		count = count < 1 ? 1 : count > MAX_UNISON ? MAX_UNISON : count;
		unison_count = count;
		target_gain = 1.f / std::sqrt((float) count);
		if (old == count || !v[0].isActive())
			return;
		for (int i = count; i < old; i++)   // fade the ones let go (the 3 ms anti-click)
			v[i].env_stage = ENV_RELEASE;
		for (int i = 0; i < count && i < old; i++)
			v[i].target_pitch = v[i].current_pitch = detuned(last_base_note, i, count);
		for (int i = old; i < count; i++)
			v[i].noteOn(detuned(last_base_note, i, count));
		applyVoiceSpread();
	}
	void setDetune(float cents) {
		int n = unison_count;
		unison_detune = cents;
		if (v[0].isActive() && n > 1)
			for (int i = 0; i < n; i++)
				v[i].target_pitch = v[i].current_pitch = detuned(last_base_note, i, n);
	}

	// a parameter into the engine, with the iOS port's curves (RenderContext.swift)
	void engineParam(int id, float x) {
		switch (id) {
			case GLIDE: for (auto& a : v) a.glide_param = x; break;
			case DELAY: D.mix = clamp01(x); break;
			case HEAD: base_voice = x; applyVoiceSpread(); break;
			case VIBRATO: for (auto& a : v) a.vibrato_depth = clamp01(x); break;
			case VIBRATE:
				for (auto& a : v) {
					a.vibrato_rate = clamp01(x);
					a.rate_scale = std::exp2((a.vibrato_rate * 2.0f - 1.0f) * 2.0f);
				}
				break;
			case BREATH: for (auto& a : v) a.aspiration_amp = clamp01(x); break;
			case ATTACK: for (auto& a : v) a.env_attack = x * 5.0f; break;
			case DECAY: for (auto& a : v) a.env_decay = x * 5.0f; break;
			case SUSTAIN: for (auto& a : v) a.env_sustain = clamp01(x); break;
			case RELEASE: for (auto& a : v) a.env_release = x * 5.0f; break;
			case VOICES: setUnison((int) (x * 9.0f + 1.5f)); break;
			case DETUNE: setDetune(x * 50.0f); break;
			case DELAYRATE: D.rate = clamp01(x); D.taps(); break;
			case LEVEL: level = x; break;
			case SPREAD: unison_voice_spread = x * 0.5f; applyVoiceSpread(); break;
			case FEEDBACK: D.feedback = clamp01(x) * 0.98f; break;
			default: break;
		}
	}

	// ---------------------------------------------------------------- the wrapper
	/** A knob (0..1). Slewed ones move over the next blocks. */
	void param(int id, float x) {
		if (id < 0 || id >= NPARAM)
			return;
		x = clamp01(x);
		if (p[id] == x)
			return;
		p[id] = x;
		if (!smooth(id)) {
			cur[id] = x;
			engineParam(id, x);
		}
	}
	void pressure(float x) { expr = clamp01(x); }
	void touch(int r, float d, float vs) {
		route = r < 0 || r >= NAT ? 0 : r;
		depth = clamp01(d);
		vel_sens = clamp01(vs);
	}
	float velGainOf(float vel01) const {
		float x = clamp01(vel01);
		return (1.0f - vel_sens) + vel_sens * x * x * 1.6f;   // 1.6: a medium press (~100) keeps the level
	}
	/** A key pressed: note in MIDI units (fractional is fine), velocity 0..1. */
	void gateOn(float note, float vel01) {
		vel_target = velGainOf(vel01);
		if (!held)
			vel_gain = vel_target;   // a new phrase starts at its own level
		held = true;
		for (auto& a : v)
			a.min_glide = 0.0f;
		applyUnison(note);
	}
	/** The held note moves (V/OCT while the gate is high): it glides there, no new attack. */
	void pitch(float note) {
		if (!held || note == last_base_note)
			return;
		last_base_note = note;
		for (int i = 0; i < unison_count; i++)
			v[i].target_pitch = detuned(note, i, unison_count);
	}
	void gateOff() {
		held = false;
		for (auto& a : v)
			a.noteOff();
	}
	void panic() {
		held = false;
		for (auto& a : v) {
			a.active = false;
			a.env_stage = ENV_IDLE;
			a.env_level = 0.f;
		}
		expr = expr_cur = 0.f;
	}

	float combinedVowel() const {
		float d = expr_cur * depth, x = cur[VOWEL];
		switch (route) {
			case AT_BOTH_INV: return clamp01(x - d);
			case AT_PITCH: return x;
			default: return clamp01(x + d);
		}
	}
	void applyBend() {
		float b = (route == AT_VOWEL ? 0.0f : expr_cur * 2.0f) + (cur[TUNE] - 0.5f) * 24.0f;
		if (b != bend_cur) {
			bend_cur = b;
			for (auto& a : v)
				a.pitch_bend_offset = clampf(b, -12.0f, 12.0f);
		}
	}
	void slewBlock() {
		const float SNAP = 0.002f;
		for (int i = 0; i < NPARAM; i++) {
			if (!smooth(i))
				continue;
			float d = p[i] - cur[i];
			if (d == 0.0f)
				continue;
			cur[i] = (d > -SNAP && d < SNAP) ? p[i] : cur[i] + d * slew;
			if (i != VOWEL && i != TUNE)
				engineParam(i, cur[i]);
		}
		float de = expr - expr_cur;
		expr_cur = (de > -SNAP && de < SNAP) ? expr : expr_cur + de * slew;
		float vw = combinedVowel();
		if (vw != v[0].target_vowel)
			for (auto& a : v)
				a.setVowel(vw);
		applyBend();
		vel_gain += (vel_target - vel_gain) * velSlew;
	}

	/** BLOCK frames: L, R (with the echo) and DRY (without), each about +-1. */
	void render(float* outL, float* outR, float* outDry) {
		const uint32_t n = BLOCK;
		slewBlock();
		std::memset(mono, 0, sizeof(mono));
		for (int k = 0; k < MAX_UNISON; k++) {
			if (k >= unison_count && !v[k].isActive())
				continue;
			v[k].process(mono, n);
		}
		float gstep = (target_gain - cur_gain) / (float) n;
		for (uint32_t i = 0; i < n; i++) {
			mono[i] *= cur_gain;
			cur_gain += gstep;
		}
		cur_gain = target_gain;
		D.process(mono, wl, wr, n);
		float gain = v[0].amplitude() * cc_volume * level * vel_gain;
		for (uint32_t i = 0; i < n; i++) {
			outL[i] = clampf(std::tanh(wl[i] * gain), -0.999f, 0.999f);
			outR[i] = clampf(std::tanh(wr[i] * gain), -0.999f, 0.999f);
			outDry[i] = clampf(std::tanh(mono[i] * gain), -0.999f, 0.999f);
		}
		uiVowel = v[0].current_vowel;
		uiAmp = clamp01(v[0].amplitude());
		uiEnv = v[0].isActive() ? (v[0].hasEnv() || v[0].env_stage == ENV_RELEASE ? v[0].env_level : 1.f) : 0.f;
		uiPitch = v[0].current_pitch;
	}
};

}  // namespace monkdsp
