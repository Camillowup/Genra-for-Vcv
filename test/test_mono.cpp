// GENRA MONO engine: pitch, stability of every filter at full resonance and drive, self-oscillation, envelopes.
// g++ -O2 -std=c++17 test/test_mono.cpp -o /tmp/test_mono && /tmp/test_mono
#include "../src/mono/MonoPresets.hpp"
#include <cstdio>
#include <vector>
using namespace gmono;

static int fails = 0;
#define CHECK(c, ...)                \
	do {                             \
		if (!(c)) {                  \
			fails++;                 \
			printf("FAIL " __VA_ARGS__); \
			printf("\n");            \
		}                            \
	} while (0)

// frequency from rising zero crossings
static float freqOf(const std::vector<float>& x, float fs) {
	int first = -1, last = -1, n = 0;
	for (size_t i = 1; i < x.size(); i++)
		if (x[i - 1] <= 0.f && x[i] > 0.f) {
			if (first < 0)
				first = i;
			last = i;
			n++;
		}
	return n > 1 ? (n - 1) * fs / (last - first) : 0.f;
}

static float rms(const std::vector<float>& x, size_t from) {
	double s = 0;
	for (size_t i = from; i < x.size(); i++)
		s += x[i] * x[i];
	return (float) std::sqrt(s / (x.size() - from));
}

int main() {
	const float fs = 48000.f;
	// 1. pitch: A3 (−0.25 V) with the filter open, every wave shape
	for (float w : {0.f, 0.33f, 0.66f, 1.f}) {
		Voice v;
		v.setSampleRate(fs);
		Params p;
		p.wave1 = w;
		p.level2 = 0.f;
		p.cutoff = 1.f;
		p.egAmt = 0.f;
		Inputs in;
		in.gate = true;
		in.voct = -0.25f;
		std::vector<float> o(fs);
		float l;
		for (auto& s : o)
			v.process(p, in, s, l);
		float f = freqOf(o, fs);
		CHECK(std::fabs(f - 220.f) < 1.f, "pitch wave %.2f: %.2f Hz", w, f);
	}
	// 1b. TUNE: full right = one octave up, a little = a few cents
	{
		Voice v;
		v.setSampleRate(fs);
		Params p;
		p.tune1 = 1.f;
		p.level2 = 0.f;
		p.cutoff = 1.f;
		p.egAmt = 0.f;
		Inputs in;
		in.gate = true;
		in.voct = -0.25f;
		std::vector<float> o(fs);
		float l;
		for (auto& s : o)
			v.process(p, in, s, l);
		float f = freqOf(o, fs);
		CHECK(std::fabs(f - 440.f) < 2.f, "tune +12: %.2f Hz", f);
		CHECK(std::fabs(tuneSemis(0.1f) - 0.12f) < 1e-4f, "tune fine");
	}
	// 2. every filter: full resonance, full drive, cutoff sweeps, hard notes: finite and bounded
	for (int t = 0; t < NUM_FILTERS; t++) {
		Voice v;
		v.setSampleRate(fs);
		Params p;
		p.ftype = t;
		p.res = 1.f;
		p.drive = 1.f;
		p.level1 = p.level2 = 1.f;
		p.wave1 = 0.5f;
		p.wave2 = 0.9f;
		p.egAmt = 1.f;
		p.fD = 0.2f;
		Inputs in;
		in.ext = 5.f;
		float peak = 0.f, l, s;
		bool finite = true;
		for (int i = 0; i < 4 * (int) fs; i++) {
			p.cutoff = 0.5f + 0.5f * std::sin(i * 0.0003f);
			in.gate = (i / 6000) % 2 == 0;
			in.voct = ((i / 6000) % 5) - 2.f;
			v.process(p, in, s, l);
			finite &= std::isfinite(s);
			peak = std::max(peak, std::fabs(s));
		}
		CHECK(finite, "%s not finite", FILTER_NAMES[t]);
		CHECK(peak <= 10.f && peak > 0.5f, "%s peak %.2f", FILTER_NAMES[t], peak);
		printf("%-10s peak %.2f V\n", FILTER_NAMES[t], peak);
	}
	// 3. self-oscillation of the ladder, the diode and the MS-20 at full resonance with silent oscillators,
	//    the SEM must not ring by itself
	for (int t : {LADDER24, LADDER12, DIODE, MS20, SVF_LP}) {
		Voice v;
		v.setSampleRate(fs);
		Params p;
		p.ftype = t;
		p.res = 1.f;
		p.level1 = p.level2 = 0.f;
		p.cutoff = 0.5f;  // ~630 Hz
		p.egAmt = 0.f;
		Inputs in;
		in.gate = true;
		std::vector<float> o(2 * (int) fs);
		float l;
		for (auto& s : o)
			v.process(p, in, s, l);
		float r = rms(o, fs);
		if (t == SVF_LP)
			CHECK(r < 0.01f, "SEM rings by itself: %.3f", r);
		else
			CHECK(r > 0.3f, "%s does not self-oscillate: rms %.3f", FILTER_NAMES[t], r);
		printf("%-10s self-osc rms %.3f V, %.0f Hz\n", FILTER_NAMES[t], r, freqOf(o, fs));
	}
	// 4. bass compensation: LADDER 24 keeps its low end at high resonance, more than the OTA
	auto bass = [&](int t, float res) {
		Voice v;
		v.setSampleRate(fs);
		Params p;
		p.ftype = t;
		p.res = res;
		p.level2 = 0.f;
		p.wave1 = 0.f;  // triangle
		p.cutoff = 0.62f;
		p.egAmt = 0.f;
		Inputs in;
		in.gate = true;
		in.voct = -3.f;  // C1
		std::vector<float> o(fs);
		float l;
		for (auto& s : o)
			v.process(p, in, s, l);
		return rms(o, fs / 2);
	};
	float l0 = bass(LADDER24, 0.f), l1 = bass(LADDER24, 0.9f), o0 = bass(OTA, 0.f), o1 = bass(OTA, 0.9f);
	printf("bass at res 0.9: ladder %.0f %%, ota %.0f %%\n", 100 * l1 / l0, 100 * o1 / o0);
	CHECK(l1 / l0 > 0.7f, "ladder loses its bass");
	CHECK(l1 / l0 > o1 / o0, "ladder thinner than ota");
	// 5. envelope: attack time and release to silence
	{
		Env e;
		float dt = 1.f / fs;
		int n = 0;
		while (e.process(true, 0.1f, 1.f, 0.5f, 0.2f, dt) < 1.f && n < fs)
			n++;
		CHECK(std::fabs(n / fs - 0.1f) < 0.01f, "attack %.3f s", n / fs);
		for (int i = 0; i < fs; i++)
			e.process(false, 0.1f, 1.f, 0.5f, 0.2f, dt);
		CHECK(e.v == 0.f && e.stage == 0, "release does not end");
	}
	// 6. presets: a bass line (C1 … C3, short and long notes) through every preset: sounds, stays in range, ends
	{
		const float notes[] = {-3.f, -3.f, -2.f, -2.58f, -1.f, -3.f, -2.42f, -2.f};
		for (int k = 0; k < NUM_PRESETS; k++) {
			const Preset& pr = PRESETS[k];
			Params p = pr.params();
			Voice v;
			v.setSampleRate(fs);
			Inputs in;
			std::vector<float> o;
			float l, peak = 0.f;
			bool finite = true;
			for (int n = 0; n < 8; n++) {
				in.voct = notes[n];
				int len = (int) (fs * (n % 3 == 2 ? 0.6f : 0.18f));
				for (int i = 0; i < (int) (fs * 0.25f); i++) {
					in.gate = i < len;
					float s;
					v.process(p, in, s, l);
					finite &= std::isfinite(s);
					peak = std::max(peak, std::fabs(s));
					o.push_back(s);
				}
			}
			float r = rms(o, 0);
			in.gate = false;
			std::vector<float> tail(fs * 3);
			for (auto& s : tail)
				v.process(p, in, s, l);
			float end = rms(tail, fs * 2.5f);
			printf("preset %2d %-14s rms %.2f V  peak %.2f V\n", k + 1, pr.name, r, peak);
			CHECK(finite, "preset %s not finite", pr.name);
			CHECK(r > 0.2f && peak > 1.5f, "preset %s too quiet (rms %.2f V, peak %.2f V)", pr.name, r, peak);
			CHECK(peak < 6.f, "preset %s too loud (%.2f V)", pr.name, peak);
			CHECK(end < 0.01f, "preset %s does not end (%.3f V)", pr.name, end);
			CHECK(pr.ftype >= 0 && pr.ftype < NUM_FILTERS, "preset %s filter", pr.name);
		}
	}
	// 7. ACC, SLIDE, stereo, LFO sync and retrigger (2.10.0)
	{
		auto note = [&](Params p, bool acc) {
			Voice v;
			v.setSampleRate(fs);
			Inputs in;
			in.voct = -2.f;
			in.acc = acc;
			std::vector<float> o(fs / 4);
			float r;
			for (size_t i = 0; i < o.size(); i++) {
				in.gate = i < o.size() / 2;
				v.process(p, in, o[i], r);
			}
			return rms(o, 0);
		};
		Params p;
		p.cutoff = 0.3f;
		p.egAmt = 0.3f;
		float plain = note(p, false), acc = note(p, true);
		p.accent = 0.f;
		float off = note(p, true);
		printf("accent: %.2f V → %.2f V (ACCENT 0: %.2f V)\n", plain, acc, off);
		CHECK(acc > plain * 1.25f, "ACC does not lift the note");
		CHECK(std::fabs(off - plain) < 0.01f * plain, "ACC acts at ACCENT 0");

		// SLIDE patched: no glide on plain notes, a glide while SLIDE is high
		auto pitchAfter = [&](bool slide, float ms) {
			Voice v;
			v.setSampleRate(fs);
			Params q;
			Inputs in;
			in.gate = true;
			in.slideConnected = true;
			in.voct = -2.f;
			float l, r;
			for (int i = 0; i < 4800; i++)
				v.process(q, in, l, r);
			in.voct = -1.f;
			in.slide = slide;
			for (int i = 0; i < (int) (fs * ms / 1000.f); i++)
				v.process(q, in, l, r);
			return v.pitch;
		};
		CHECK(std::fabs(pitchAfter(false, 1.f) + 1.f) < 1e-3f, "plain note glides");
		float mid = pitchAfter(true, 20.f);
		CHECK(mid > -1.9f && mid < -1.1f, "slide does not glide (%.3f)", mid);
		CHECK(std::fabs(pitchAfter(true, 2000.f) + 1.f) < 0.01f, "slide does not arrive");

		// stereo: WIDTH 0 = identical sides, WIDTH up = different sides with mono lows
		auto side = [&](float width, float& lowSide) {
			Voice v;
			v.setSampleRate(fs);
			Params q;
			q.wave1 = q.wave2 = 0.33f;
			q.tune2 = 0.1f;
			q.cutoff = 0.7f;
			q.width = width;
			Inputs in;
			in.gate = true;
			in.voct = -2.f;
			double d = 0, m = 0, lowS = 0, lowM = 0;
			Biquad s1, s2, m1, m2;  // 4-pole low-pass at 90 Hz: the fundamental (65 Hz) and below
			s1.lowpass(90.f, fs, 0.5412f), s2.lowpass(90.f, fs, 1.3066f);
			m1.lowpass(90.f, fs, 0.5412f), m2.lowpass(90.f, fs, 1.3066f);
			float l, r;
			for (int i = 0; i < fs; i++) {
				v.process(q, in, l, r);
				float ls = s2.process(s1.process(l - r)), lm = m2.process(m1.process(l + r));
				if (i < fs / 2)
					continue;
				d += (l - r) * (l - r);
				m += (l + r) * (l + r);
				lowS += ls * ls;
				lowM += lm * lm;
			}
			lowSide = (float) std::sqrt(lowS / lowM);
			return (float) std::sqrt(d / m);
		};
		float low0, low1;
		float s0 = side(0.f, low0), s1 = side(1.f, low1);
		printf("stereo side/mid: width 0 %.3f, width 1 %.3f (below 90 Hz %.4f)\n", s0, s1, low1);
		CHECK(s0 < 1e-6f, "WIDTH 0 is not mono");
		CHECK(s1 > 0.05f, "WIDTH 1 is not wide");
		CHECK(low1 < 0.05f, "lows not mono (%.3f)", low1);

		// LFO: SYNC at 120 BPM, RATE on 1/4 → 2 Hz; RETRIG starts it over
		{
			Voice v;
			v.setSampleRate(fs);
			Params q;
			q.rate = 7.f / 16.f;  // DIV_NAMES[7] = 1/4
			Inputs in;
			in.beatSec = 0.5f;
			float l, r, last = 0.f;
			int wraps = 0;
			for (int i = 0; i < fs * 1.25f; i++) {
				v.process(q, in, l, r);
				if (v.lfo.phase < last)
					wraps++;
				last = v.lfo.phase;
			}
			CHECK(wraps == 2, "sync 1/4 at 120 BPM: %d cycles in 1.25 s", wraps);
			in.retrig = true;
			v.process(q, in, l, r);
			CHECK(std::fabs(v.lfo.phase - 0.25f - 2.f / fs) < 1e-4f, "retrigger");
		}
	}
	printf("mono fails=%d\n", fails);
	return fails;
}
