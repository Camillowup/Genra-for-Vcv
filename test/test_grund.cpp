// GRUND engine: every model and every preset plays a finite, audible note at the right pitch, stays in a sane
// level, is silent after the release; stability at all sample rates and extreme knobs; the octaver and the sidechain.
// g++ -O2 -std=c++17 test/test_grund.cpp -o /tmp/test_grund && /tmp/test_grund [wav-dir]
#include <algorithm>
#include "../src/grund/GrundPresets.hpp"
#include <cstdio>
#include <memory>
#include <vector>
using namespace ggrund;

static int fails = 0;
#define CHECK(c, ...)                    \
	do {                                 \
		if (!(c)) {                      \
			fails++;                     \
			printf("FAIL " __VA_ARGS__); \
			printf("\n");                \
		}                                \
	} while (0)

static float rms(const std::vector<float>& x, size_t a, size_t b) {
	double s = 0;
	b = std::min(b, x.size());
	for (size_t i = a; i < b; i++)
		s += x[i] * x[i];
	return (float) std::sqrt(s / std::max<size_t>(1, b - a));
}
static float peakAbs(const std::vector<float>& x) {
	float m = 0.f;
	for (float v : x)
		m = std::max(m, std::fabs(v));
	return m;
}
static bool finite(const std::vector<float>& x) {
	for (float v : x)
		if (!std::isfinite(v))
			return false;
	return true;
}
/** The strongest frequency between lo and hi (Hann window, 0.5 % steps). */
static float strongest(const std::vector<float>& x, size_t from, size_t n, float fs, float lo, float hi) {
	float best = lo, bm = -1.f;
	for (float f = lo; f <= hi; f *= 1.005f) {
		double re = 0, im = 0, w = 2.0 * M_PI * f / fs;
		for (size_t i = 0; i < n; i++) {
			double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
			re += h * x[from + i] * std::cos(w * i);
			im += h * x[from + i] * std::sin(w * i);
		}
		float m = (float) (re * re + im * im);
		if (m > bm)
			bm = m, best = f;
	}
	return best;
}

static void wav(const char* fn, const std::vector<float>& l, const std::vector<float>& r, int fs) {
	FILE* f = fopen(fn, "wb");
	if (!f)
		return;
	int n = (int) l.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f);
	u32(36 + bytes);
	fwrite("WAVEfmt ", 1, 8, f);
	u32(16), u16(1), u16(2), u32(fs), u32(fs * 4), u16(4), u16(16);
	fwrite("data", 1, 4, f);
	u32(bytes);
	for (int i = 0; i < n; i++) {
		int16_t a = (int16_t) clampf(l[i] / 10.f * 32767.f, -32767.f, 32767.f);
		int16_t b = (int16_t) clampf(r[i] / 10.f * 32767.f, -32767.f, 32767.f);
		fwrite(&a, 2, 1, f);
		fwrite(&b, 2, 1, f);
	}
	fclose(f);
}

/** One note (gate `on` s, `len` s in total). */
static void note(const Params& p, float fs, float voct, float on, float len, std::vector<float>& L,
                 std::vector<float>& R, bool acc = false) {
	std::unique_ptr<Engine> e(new Engine);
	e->setSampleRate(fs);
	int n = (int) (len * fs), g = (int) (on * fs);
	L.assign(n, 0.f), R.assign(n, 0.f);
	Input in;
	in.voct = voct;
	in.vel = 0.8f;
	in.acc = acc;
	for (int i = 0; i < n; i++) {
		in.gate = i < g;
		float env;
		e->process(p, in, L[i], R[i], env);
	}
}

/** A bass line (GENRA-like 16ths at 120 BPM with slides and accents). */
static void line(const Params& p, float fs, std::vector<float>& L, std::vector<float>& R) {
	std::unique_ptr<Engine> e(new Engine);
	e->setSampleRate(fs);
	const float notes[16] = {-2, -2, -1.75f, -2, -1.5f, -1.5f, -2, -1.583f, -2, -2, -1, -2, -1.75f, -1.75f, -2, -1.583f};
	int step = (int) (0.125f * fs), n = step * 32;
	L.assign(n, 0.f), R.assign(n, 0.f);
	Input in;
	in.slideConnected = true;
	for (int i = 0; i < n; i++) {
		int s = (i / step) % 16, k = i % step;
		in.voct = notes[s];
		in.gate = k < step * 0.6f || s == 4 || s == 12;  // slides: tied into the next step
		in.slide = s == 4 || s == 12;
		in.acc = s % 4 == 0;
		in.vel = in.acc ? 1.f : 0.7f;
		float env;
		e->process(p, in, L[i], R[i], env);
	}
}


// 2.34: the loudest moments, measured before the safety limiter: all but the loudest 0.01 % of samples
static float peakQ(std::vector<float> a, const std::vector<float>* b = nullptr) {
	if (b)
		a.insert(a.end(), b->begin(), b->end());
	for (auto& x : a)
		x = std::fabs(x);
	if (a.empty())
		return 0.f;
	size_t q = a.size() - a.size() / 10000;
	if (q >= a.size())
		q = a.size() - 1;
	std::nth_element(a.begin(), a.begin() + q, a.end());
	return a[q];
}
static const float RAW = 0.05f; // presets measured with the output level scaled down: the limiter stays out

int main(int argc, char** argv) {
	const char* dir = argc > 1 ? argv[1] : nullptr;
	const float fs = 48000.f;
	std::vector<float> L, R;

	// every model: audible, finite, sane level, right pitch (E1 = -1.667 V, 41.2 Hz), silent after the release
	for (int m = 0; m < NUM_MODELS; m++) {
		Params p;
		p.model = m;
		p.verb = 0.f, p.chorus = 0.f, p.octave = 0.f, p.width = 0.f;
		p.fx.compOn = false;
		p.tone = 0.7f, p.fenv = 0.1f, p.sub = 0.f, p.mute = 0.f;
		p.sus = 0.9f, p.rel = 0.25f;
		float v = -1.f - 8.f / 12.f;  // E1
		note(p, fs, v, 0.6f, 2.5f, L, R);
		float lvl = rms(L, (size_t) (0.05f * fs), (size_t) (0.5f * fs));
		CHECK(finite(L) && finite(R), "%s: not finite", MODEL_NAMES[m]);
		CHECK(lvl > 0.15f, "%s: too quiet (%.3f V rms)", MODEL_NAMES[m], lvl);
		CHECK(peakAbs(L) < 12.f, "%s: too loud (%.2f V peak)", MODEL_NAMES[m], peakAbs(L));
		float tail = rms(L, (size_t) (2.2f * fs), (size_t) (2.5f * fs));
		CHECK(tail < 0.01f, "%s: not silent after the release (%.4f)", MODEL_NAMES[m], tail);
		// pitch: at A1 (55 Hz) the strongest partial near the fundamental or its octave
		note(p, fs, -2.f + 9.f / 12.f, 1.0f, 1.0f, L, R);
		float f0 = strongest(L, (size_t) (0.25f * fs), (size_t) (0.4f * fs), fs, 45.f, 125.f);
		bool ok = std::fabs(1200.f * std::log2(f0 / 55.f)) < 40.f || std::fabs(1200.f * std::log2(f0 / 110.f)) < 40.f;
		CHECK(ok, "%s: pitch %.1f Hz, expected 55 Hz (or 110)", MODEL_NAMES[m], f0);
		printf("%-9s rms %.2f V  peak %.2f V  f %.1f Hz\n", MODEL_NAMES[m], lvl, peakAbs(L), f0);
	}

	// every preset: a bass line, finite, audible, sane
	for (int i = 0; i < NUM_PRESETS; i++) {
		Params p = PRESETS[i].params();
		p.outGain *= RAW;
		line(p, fs, L, R);
		for (auto& x : L)
			x /= RAW;
		for (auto& x : R)
			x /= RAW;
		float lvl = rms(L, 0, L.size()), pk = peakQ(L, &R);
		CHECK(finite(L) && finite(R), "preset %s: not finite", PRESETS[i].name);
		CHECK(lvl > 0.5f, "preset %s: too quiet (%.3f)", PRESETS[i].name, lvl);
		// 2.34: clear of the limiter (knee 6 V), so nothing is squashed
		CHECK(pk <= 6.06f, "preset %s: peaks %.2f V reach the limiter", PRESETS[i].name, pk);
		printf("%02d %-16s rms %.2f V  peak %.2f V\n", i + 1, PRESETS[i].name, lvl, pk);
		if (dir) {
			char fn[256];
			std::snprintf(fn, sizeof(fn), "%s/grund_%02d.wav", dir, i + 1);
			wav(fn, L, R, (int) fs);
		}
	}

	// stability: every model × amp at the sample rates, extreme knobs
	const float rates[4] = {44100.f, 48000.f, 96000.f, 192000.f};
	for (float sr : rates)
		for (int m = 0; m < NUM_MODELS; m++)
			for (int a = 0; a < NUM_AMPS; a++) {
				Params p;
				p.model = m, p.amp = a;
				p.timbre = 1.f, p.bite = 1.f, p.body = 1.f, p.sub = 1.f, p.res = 1.f, p.drive = 1.f, p.comp = 1.f;
				p.tone = 1.f, p.fenv = 1.f, p.octave = 1.f, p.chorus = 1.f, p.width = 1.f, p.verb = 1.f;
				p.fx.driveOn = true, p.fx.dMix = 1.f, p.fx.dType = (m + a) % NUM_DRIVES;
				p.fx.low = p.fx.high = p.fx.mid = 1.f;
				note(p, sr, -3.f, 0.3f, 0.6f, L, R, true);
				CHECK(finite(L) && finite(R) && peakAbs(L) < 40.f, "stability %s / %s at %.0f Hz (peak %.1f)",
				      MODEL_NAMES[m], AMP_NAMES[a], sr, peakAbs(L));
			}

	// the octaver adds energy an octave below
	{
		Params p;
		p.model = SYNTH, p.timbre = 1.f, p.verb = 0.f, p.chorus = 0.f, p.width = 0.f, p.fx.compOn = false;
		note(p, fs, -1.f, 1.f, 1.f, L, R);
		float dry = rms(L, (size_t) (0.2f * fs), (size_t) (0.9f * fs));
		p.octave = 1.f;
		note(p, fs, -1.f, 1.f, 1.f, L, R);
		float wet = rms(L, (size_t) (0.2f * fs), (size_t) (0.9f * fs));
		CHECK(wet > dry * 1.05f, "octaver: no change (%.3f -> %.3f)", dry, wet);
	}
	// the sidechain ducks
	{
		Params p;
		p.model = SYNTH, p.verb = 0.f, p.chorus = 0.f, p.comp = 0.5f;
		std::unique_ptr<Engine> e(new Engine);
		e->setSampleRate(fs);
		Input in;
		in.voct = -2.f, in.gate = true, in.scConnected = true;
		float l, r, env, a = 0.f, b = 0.f;
		for (int i = 0; i < (int) fs; i++) {
			in.sc = i > fs / 2 ? 10.f : 0.f;
			e->process(p, in, l, r, env);
			if (i > fs * 0.3f && i < fs * 0.45f)
				a += l * l;
			if (i > fs * 0.8f && i < fs * 0.95f)
				b += l * l;
		}
		CHECK(b < a * 0.6f, "sidechain: no ducking (%.3f -> %.3f)", a, b);
	}

	printf(fails ? "\n%d FAILED\n" : "\nall GRUND tests passed\n", fails);
	return fails ? 1 : 0;
}
