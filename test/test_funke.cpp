// FUNKE engine: pitch of every model, level match between the models, glide on SLIDE, the voice modes, the echo
// time from the clock, and stability of every model × filter at all sample rates and extreme settings.
// g++ -O2 -std=c++17 test/test_funke.cpp -o /tmp/test_funke && /tmp/test_funke [wav-dir]
#include "../src/funke/FunkePresets.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace gfunke;

static int fails = 0;
#define CHECK(c, ...)                    \
	do {                                 \
		if (!(c)) {                      \
			fails++;                     \
			printf("FAIL " __VA_ARGS__); \
			printf("\n");                \
		}                                \
	} while (0)

// the strongest partial within ±1 semitone of `expect` (Hann window, fine scan)
static float peakNear(const std::vector<float>& x, size_t from, size_t n, float fs, float expect) {
	float best = expect, bm = -1.f;
	for (float c = -100.f; c <= 100.f; c += 0.5f) {
		float f = expect * std::pow(2.f, c / 1200.f);
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
static float energyAt(const std::vector<float>& x, size_t from, size_t n, float fs, float f) {
	double re = 0, im = 0, w = 2.0 * M_PI * f / fs;
	for (size_t i = 0; i < n; i++) {
		double h = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
		re += h * x[from + i] * std::cos(w * i);
		im += h * x[from + i] * std::sin(w * i);
	}
	return (float) std::sqrt(re * re + im * im) / n;
}
static float rms(const std::vector<float>& x, size_t a, size_t b) {
	double s = 0;
	for (size_t i = a; i < b; i++)
		s += x[i] * x[i];
	return (float) std::sqrt(s / std::max<size_t>(1, b - a));
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

/** Plays one note (gate for `on` s, then `len` s in total) into a fresh engine. Dry: no FX. */
static void note(Params p, float fs, float voct, float on, float len, std::vector<float>& L, std::vector<float>& R,
                 bool dry = true) {
	auto e = std::make_unique<Engine>();
	e->setSampleRate(fs);
	if (dry)
		p.chorus = p.echo = p.verb = p.lofi = 0.f, p.width = 0.f, p.vdepth = 0.f;
	size_t n = (size_t) (len * fs);
	L.assign(n, 0.f);
	R.assign(n, 0.f);
	Channel c;
	c.voct = voct;
	for (size_t i = 0; i < n; i++) {
		c.gate = i < (size_t) (on * fs);
		float env;
		e->process(p, &c, 1, false, 0.25f, L[i], R[i], env);
	}
}

int main(int argc, char** argv) {
	const char* dir = argc > 1 ? argv[1] : nullptr;
	const float fs = 48000.f;
	Params base;
	base.cutoff = 1.f, base.key = 0.f, base.fenv = 0.f, base.res = 0.f;

	// 1. every model in tune at three pitches, and their levels close to each other
	float lv[NUM_MODELS];
	for (int m = 0; m < NUM_MODELS; m++) {
		for (float voct : {-1.f, 0.f, 1.25f}) {
			Params p = base;
			p.model = m;
			if (m == SAWLEAD)
				p.timbre = 0.f;  // the detuned saws beat: measure with the least detune
			std::vector<float> L, R;
			note(p, fs, voct, 0.6f, 0.7f, L, R);
			float f = C4 * std::pow(2.f, voct);
			float got = peakNear(L, 2400, 9600, fs, f);
			float cents = 1200.f * std::log2(got / f);
			CHECK(std::fabs(cents) < 6.f, "model %s at %.0f Hz: %.1f cents off", MODEL_NAMES[m], f, cents);
			// the fundamental must be there (not only overtones)
			CHECK(energyAt(L, 2400, 9600, fs, f) > 0.05f, "model %s at %.0f Hz: weak fundamental %.3f",
			      MODEL_NAMES[m], f, energyAt(L, 2400, 9600, fs, f));
			if (voct == 0.f)
				lv[m] = rms(L, 480, 9600);
		}
	}
	float lo = 1e9f, hi = 0.f;
	for (int m = 0; m < NUM_MODELS; m++) {
		printf("  %-9s level %.2f V rms\n", MODEL_NAMES[m], lv[m]);
		lo = std::min(lo, lv[m]);
		hi = std::max(hi, lv[m]);
	}
	CHECK(hi / lo < 2.2f, "model levels too far apart (%.2f .. %.2f V rms)", lo, hi);
	CHECK(hi < 3.5f && lo > 0.6f, "model levels out of range (%.2f .. %.2f V rms)", lo, hi);

	// 2. TIMBRE and BITE change the sound of every model (the spectrum moves)
	for (int m = 0; m < NUM_MODELS; m++)
		for (int which = 0; which < 2; which++) {
			float a[2];
			for (int k = 0; k < 2; k++) {
				Params p = base;
				p.model = m;
				(which ? p.bite : p.timbre) = (float) k;
				std::vector<float> L, R;
				note(p, fs, 0.f, 0.3f, 0.3f, L, R);
				// brightness: energy of the 3rd..8th harmonic against the fundamental, early in the note
				float e1 = energyAt(L, 0, 4800, fs, C4), eh = 0.f;
				for (int h = 2; h <= 8; h++)
					eh += energyAt(L, 0, 4800, fs, C4 * h) + energyAt(L, 0, 4800, fs, C4 * h * 0.9f);
				a[k] = eh / (e1 + 1e-6f) + rms(L, 0, 4800) * 0.01f;
			}
			CHECK(std::fabs(a[1] - a[0]) > 0.02f * std::max(a[0], a[1]) + 1e-4f, "%s: %s does nothing (%.3f, %.3f)",
			      MODEL_NAMES[m], which ? "BITE" : "TIMBRE", a[0], a[1]);
		}

	// 3. glide: with SLIDE high the pitch moves over time, without it jumps (MONO, SLIDE patched)
	{
		auto e = std::make_unique<Engine>();
		e->setSampleRate(fs);
		Params p = base;
		p.mode = MODE_MONO, p.model = SAWLEAD, p.glide = 0.6f;
		Channel c;
		c.gate = true;
		float l, r, env;
		for (int i = 0; i < 4800; i++)
			e->process(p, &c, 1, true, 0.25f, l, r, env);
		c.voct = 1.f;
		c.slide = true;
		for (int i = 0; i < 480; i++)
			e->process(p, &c, 1, true, 0.25f, l, r, env);
		c.slide = false;  // GENRA's SLIDE is only a pulse: the glide must go on
		for (int i = 0; i < 2400; i++)
			e->process(p, &c, 1, true, 0.25f, l, r, env);
		float mid = e->v[0].pitch;
		CHECK(mid > 0.2f && mid < 0.98f, "glide: pitch %.3f after 60 ms (want on the way)", mid);
		for (int i = 0; i < 96000; i++)
			e->process(p, &c, 1, true, 0.25f, l, r, env);
		CHECK(std::fabs(e->v[0].pitch - 1.f) < 0.001f, "glide does not arrive: %.4f", e->v[0].pitch);
		// no SLIDE: a jump
		c.voct = 0.f;
		e->process(p, &c, 1, true, 0.25f, l, r, env);
		e->process(p, &c, 1, true, 0.25f, l, r, env);
		CHECK(std::fabs(e->v[0].pitch) < 0.001f, "without SLIDE the pitch should jump, at %.3f", e->v[0].pitch);
	}

	// 4. POLY: a mono cable with overlapping release tails uses several voices; MONO uses one
	for (int mode : {MODE_POLY, MODE_MONO}) {
		auto e = std::make_unique<Engine>();
		e->setSampleRate(fs);
		Params p = base;
		p.mode = mode, p.model = RHODES, p.rel = 0.8f;
		Channel c;
		float l, r, env;
		for (int k = 0; k < 4; k++) {
			c.voct = k / 12.f;
			c.gate = true;
			for (int i = 0; i < 2400; i++)
				e->process(p, &c, 1, false, 0.25f, l, r, env);
			c.gate = false;
			for (int i = 0; i < 240; i++)
				e->process(p, &c, 1, false, 0.25f, l, r, env);
		}
		int on = 0;
		for (int i = 0; i < MAXV; i++)
			on += e->v[i].active;
		if (mode == MODE_POLY)
			CHECK(on >= 3, "POLY: only %d voices sounding", on);
		else
			CHECK(on <= 1, "MONO: %d voices sounding", on);
	}

	// 5. LEGATO: a second held note (poly cable) does not restart the envelope, MONO does
	for (int mode : {MODE_LEGATO, MODE_MONO}) {
		auto e = std::make_unique<Engine>();
		e->setSampleRate(fs);
		Params p = base;
		p.mode = mode, p.model = SAWLEAD, p.atk = 0.75f;  // a slow attack (~1 s)
		Channel c[2];
		c[0].gate = true;
		float l, r, env;
		for (int i = 0; i < 24000; i++)
			e->process(p, c, 2, false, 0.25f, l, r, env);
		float before = e->v[0].aenv.v;
		c[1].gate = true;
		c[1].voct = 0.5f;
		for (int i = 0; i < 4; i++)
			e->process(p, c, 2, false, 0.25f, l, r, env);
		bool restarted = e->v[0].aenv.stage == 1 && e->v[0].age < 0.01f;
		if (mode == MODE_LEGATO)
			CHECK(!restarted && e->v[0].aenv.v >= before * 0.99f, "LEGATO restarted the note");
		else
			CHECK(restarted, "MONO did not restart the note");
		CHECK(std::fabs(e->v[0].target - 0.5f) < 1e-4f, "mode %d: the newest note does not sound", mode);
		// let go of the new note: back to the first, without a new attack
		c[1].gate = false;
		for (int i = 0; i < 4; i++)
			e->process(p, c, 2, false, 0.25f, l, r, env);
		CHECK(std::fabs(e->v[0].target) < 1e-4f && e->v[0].gate, "mode %d: no return to the held note", mode);
	}

	// 6. the echo repeats after the clocked time (1/8D at 120 BPM = 375 ms)
	{
		auto e = std::make_unique<Engine>();
		e->setSampleRate(fs);
		Params p = base;
		p.model = MARIMBA, p.echo = 1.f, p.fdbk = 0.f, p.chorus = p.verb = p.lofi = p.width = 0.f;
		Channel c;
		std::vector<float> L(48000);
		float r, env;
		for (int i = 0; i < 48000; i++) {
			c.gate = i < 480;
			e->process(p, &c, 1, false, 0.375f, L[i], r, env);
		}
		float a = rms(L, 0, 4800), b = rms(L, 18000, 22800), quiet = rms(L, 9600, 14400);
		CHECK(b > 0.15f * a && b > 3.f * quiet, "no echo at 375 ms (%.3f, before %.3f, note %.3f)", b, quiet, a);
	}

	// 7. stability: every model × filter at all sample rates, high resonance, drive, all FX up, fast notes
	for (float sr : {44100.f, 48000.f, 96000.f, 192000.f})
		for (int m = 0; m < NUM_MODELS; m++)
			for (int ft = 0; ft < gmono::NUM_FILTERS; ft++) {
				auto e = std::make_unique<Engine>();
				e->setSampleRate(sr);
				Params p;
				p.model = m, p.ftype = ft, p.res = 1.f, p.drive = 1.f, p.fenv = 1.f, p.cutoff = 0.9f, p.key = 1.f;
				p.timbre = p.bite = p.sub = p.width = 1.f;
				p.chorus = p.echo = p.lofi = p.verb = 1.f, p.fdbk = 1.f, p.vdepth = 1.f, p.vdelay = 0.f;
				p.accent = 1.f;
				Channel c[3];
				float peak = 0.f;
				bool bad = false;
				size_t n = (size_t) (0.5f * sr);
				for (size_t i = 0; i < n; i++) {
					for (int k = 0; k < 3; k++) {
						c[k].gate = ((i / (size_t) (0.05f * sr)) + k) % 3 != 0;
						c[k].voct = -3.f + 7.f * ((i / 997 + k * 5) % 13) / 12.f;
						c[k].acc = k == 1;
						c[k].vel = 1.f;
					}
					float l, r, env;
					e->process(p, c, 3, false, 0.1f, l, r, env);
					if (!std::isfinite(l) || !std::isfinite(r))
						bad = true;
					peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
				}
				CHECK(!bad && peak <= 8.01f, "unstable: %s × %s at %.0f Hz (peak %.2f)", MODEL_NAMES[m],
				      gmono::FILTER_NAMES[ft], sr, peak);
			}

	// demo WAVs: a phrase per model with the default settings
	if (dir) {
		const float mel[] = {0, 3, 7, 10, 12, 10, 7, 5, 3, 7, 0, -2};
		for (int m = 0; m < NUM_MODELS; m++) {
			auto e = std::make_unique<Engine>();
			e->setSampleRate(fs);
			Params p;
			p.model = m;
			Channel c;
			std::vector<float> L, R;
			for (int k = 0; k < 16; k++)
				for (int i = 0; i < 9000; i++) {
					c.voct = (k < 12 ? mel[k] : 0.f) / 12.f;
					c.gate = k < 12 && i < (k % 4 == 3 ? 8000 : 5000);
					c.acc = k % 4 == 0;
					c.slide = k == 5 && i < 2000;
					float l, r, env;
					e->process(p, &c, 1, true, 0.28125f, l, r, env);
					L.push_back(l), R.push_back(r);
				}
			char fn[256];
			snprintf(fn, sizeof fn, "%s/funke_%d_%s.wav", dir, m, MODEL_NAMES[m]);
			wav(fn, L, R, (int) fs);
		}
	}

	// every preset: a chord (Poly) or a note (Mono, Legato) for 1.5 s, then 7 s of tail. Finite, audible, a sane
	// peak, close in loudness to the others, and silent at the end (echo and reverb have died away).
	{
		float lvl[NUM_PRESETS];
		for (int i = 0; i < NUM_PRESETS; i++) {
			const Preset& pr = PRESETS[i];
			Params p = pr.params();
			auto e = std::make_unique<Engine>();
			e->setSampleRate(fs);
			int n = pr.mode == MODE_POLY ? 3 : 1;
			Channel c[3];
			const float chord[3] = {0.f, 4.f / 12.f, 7.f / 12.f};
			size_t on = (size_t) (1.5f * fs), len = (size_t) (8.5f * fs);
			std::vector<float> L(len), R(len);
			bool fin = true;
			float peak = 0.f;
			for (size_t k = 0; k < len; k++) {
				for (int v = 0; v < n; v++) {
					c[v].voct = chord[v];
					c[v].gate = k < on;
					c[v].vel = 0.8f;
				}
				float env;
				e->process(p, c, n, false, 0.375f, L[k], R[k], env);
				fin &= std::isfinite(L[k]) && std::isfinite(R[k]);
				peak = std::max(peak, std::max(std::fabs(L[k]), std::fabs(R[k])));
			}
			// loudness: the loudest 50 ms while the keys are held (plucks and pads alike)
			float lv = 0.f;
			for (size_t a = 0, w = (size_t) (0.05f * fs); a + w <= on; a += w / 2)
				lv = std::max(lv, 0.5f * (rms(L, a, a + w) + rms(R, a, a + w)));
			float tail = 0.5f * (rms(L, len - (size_t) (0.3f * fs), len) + rms(R, len - (size_t) (0.3f * fs), len));
			lvl[i] = lv;
			printf("  %02d %-18s rms %.2f V  peak %.2f V  tail %.4f V\n", i + 1, pr.name, lv, peak, tail);
			CHECK(fin, "preset %s: not finite", pr.name);
			CHECK(lv > 2.1f, "preset %s: too quiet (%.2f V rms)", pr.name, lv);  // loud and even: about 2.4 V
			CHECK(peak <= 8.01f, "preset %s: peak %.2f V (above the limiter's ceiling)", pr.name, peak);
			CHECK(tail < 0.01f * lv + 1e-4f, "preset %s: still sounding after the release (%.4f V)", pr.name, tail);
			if (dir) {
				char fn[256];
				snprintf(fn, sizeof fn, "%s/funke_preset_%02d.wav", dir, i + 1);
				wav(fn, L, R, (int) fs);
			}
		}
		float lo = 1e9f, hi = 0.f;
		for (float v : lvl)
			lo = std::min(lo, v), hi = std::max(hi, v);
		CHECK(hi / lo < 1.2f, "preset levels too far apart (%.2f .. %.2f V rms)", lo, hi);
		for (int i = 0; i < NUM_PRESETS; i++)
			for (int j = 0; j < i; j++)
				CHECK(std::strcmp(PRESETS[i].name, PRESETS[j].name) != 0, "preset name twice: %s", PRESETS[i].name);
	}

	printf(fails ? "%d FAILED\n" : "all FUNKE tests passed\n", fails);
	return fails ? 1 : 0;
}
