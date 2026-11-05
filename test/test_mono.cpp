// GENRA MONO engine: pitch, stability of every filter at full resonance and drive, self-oscillation, envelopes.
// g++ -O2 -std=c++17 test/test_mono.cpp -o /tmp/test_mono && /tmp/test_mono
#include "../src/mono/MonoDsp.hpp"
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
	printf("mono fails=%d\n", fails);
	return fails;
}
