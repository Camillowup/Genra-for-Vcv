// STOTTER engine: transparency, kills, stutter timing, quantised launch, all effects stable, clock, capture loop.
// g++ -O2 -std=c++17 test/test_stotter.cpp -o /tmp/test_stotter && /tmp/test_stotter
#include "../src/stotter/StotterDsp.hpp"
#include <cstdio>
#include <vector>
using namespace gstot;

static int fails = 0;
#define CHECK(c, ...)                    \
	do {                                 \
		if (!(c)) {                      \
			fails++;                     \
			printf("FAIL " __VA_ARGS__); \
			printf("\n");                \
		}                                \
	} while (0)

static const float FS = 48000.f;

static float rms(const std::vector<float>& x, size_t a, size_t b) {
	double s = 0;
	for (size_t i = a; i < b; i++) s += x[i] * x[i];
	return (float) std::sqrt(s / std::max<size_t>(1, b - a));
}
static float db(float x) { return 20.f * std::log10(std::max(x, 1e-9f)); }

/** Runs n samples of a sine (f Hz, 5 V) or noise (f = 0); returns the left output. */
static std::vector<float> run(Engine& e, int n, float f, std::vector<float>* in = nullptr) {
	std::vector<float> out(n);
	static uint32_t r = 1;
	for (int i = 0; i < n; i++) {
		float x;
		if (f > 0.f) x = 5.f * std::sin(2.f * PI * f * (float) (e.W + 1) / FS);
		else {
			r = r * 1664525u + 1013904223u;
			x = ((r >> 8) * (1.f / 16777216.f) * 2.f - 1.f) * 5.f;
		}
		if (in) in->push_back(x);
		float l, rr;
		e.process(x, x, l, rr);
		out[i] = l;
	}
	return out;
}

int main() {
	// ---- 1. idle: the EQ (allpass sum) and the filter at its centre leave the level alone
	for (float f : {60.f, 300.f, 1000.f, 3000.f, 9000.f}) {
		Engine e;
		auto y = run(e, 24000, f);
		float d = db(rms(y, 12000, 24000) / (5.f / std::sqrt(2.f)));
		CHECK(std::fabs(d) < 0.3f, "idle level at %.0f Hz: %+.2f dB", f, d);
	}
	// ---- 2. kills
	{
		Engine e;
		e.c.killLo = true;
		auto y = run(e, 24000, 50.f);
		CHECK(db(rms(y, 12000, 24000) / 3.5355f) < -30.f, "low kill at 50 Hz: %.1f dB", db(rms(y, 12000, 24000) / 3.5355f));
		Engine h;
		h.c.killHi = true;
		y = run(h, 24000, 12000.f);
		CHECK(db(rms(y, 12000, 24000) / 3.5355f) < -30.f, "high kill at 12 kHz: %.1f dB", db(rms(y, 12000, 24000) / 3.5355f));
		Engine m;
		m.c.killMid = true;
		y = run(m, 24000, 950.f);
		CHECK(db(rms(y, 12000, 24000) / 3.5355f) < -25.f, "mid kill at 950 Hz: %.1f dB", db(rms(y, 12000, 24000) / 3.5355f));
	}
	// ---- 3. stutter 1/16 at 120 BPM repeats every 6000 samples
	{
		Engine e;
		e.c.quant = 0;
		e.pads[2].p[1] = 0.f;  // no decay
		run(e, 5000, 0.f);
		e.press(2);
		auto y = run(e, 48000, 0.f);
		double err = 0, en = 0;
		for (int i = 13000; i < 40000; i++) {
			if ((i % 6000) < 400 || (i % 6000) > 5600) continue;  // the fades at the slice edges
			err += std::fabs(y[i] - y[i + 6000]);
			en += std::fabs(y[i]);
		}
		CHECK(en > 0 && err / en < 0.02, "stutter not periodic: %.3f", en > 0 ? err / en : -1.0);
		CHECK(e.padState(2) == 1, "stutter pad not lit");
		e.press(2);  // LOOP: the same pad stops it
		run(e, 2000, 0.f);
		CHECK(e.act < 0 && e.padState(2) == 0, "stutter did not stop");
	}
	// ---- 4. launch on the 1/4 grid
	{
		Engine e;
		e.c.quant = 3;
		e.pads[8].p[1] = 1.f;  // gate 1/8, full depth
		run(e, 7000, 440.f);   // beat 0.29
		e.press(8);
		CHECK(e.padState(8) == 2, "pad not pending");
		run(e, 24000 - 7000 - 1, 440.f);
		CHECK(e.act < 0, "launched before the grid");
		run(e, 2, 440.f);
		CHECK(e.act == 8, "not launched on the beat (act %d)", e.act);
	}
	// ---- 5. every effect, many settings, intensity and filter extremes: finite and bounded
	{
		uint32_t r = 7;
		for (int fx = 0; fx < NUM_FX; fx++)
			for (int trial = 0; trial < 6; trial++) {
				Engine e;
				e.c.quant = trial % NQUANT;
				e.c.intensity = (trial % 3) * 0.5f;
				e.c.cut = trial < 3 ? -0.9f + 0.6f * trial : 0.95f;
				e.c.reso = 1.f;
				e.c.drive = (trial % 2) ? 1.f : 0.f;
				e.c.pre = trial % 2;
				for (int k = 0; k < 3; k++) {
					r = r * 1664525u + 1013904223u;
					e.pads[0].p[k] = (r >> 8) * (1.f / 16777216.f);
				}
				e.pads[0].fx = fx;
				run(e, 30000, 0.f);
				e.press(0);
				auto y = run(e, 120000, trial % 2 ? 0.f : 220.f);
				float mx = 0;
				bool fin = true;
				for (float v : y) {
					if (!std::isfinite(v)) fin = false;
					mx = std::max(mx, std::fabs(v));
				}
				CHECK(fin && mx < 10.5f, "%s trial %d: finite %d, peak %.2f V", FX_NAMES[fx], trial, fin, mx);
				for (int k = 0; k < 3; k++) (void) paramText(fx, k, e.pads[0].p[k]);
				for (int i = 0; i <= 20; i++) {
					float c = displayCurve(e.pads[0], i / 20.f);
					CHECK(std::isfinite(c) && c >= -0.01f && c <= 1.01f, "%s display %.2f", FX_NAMES[fx], c);
				}
			}
	}
	// ---- 6. tape stop without restart ends in silence; one shot returns to the loop
	{
		Engine e;
		e.c.quant = 0;
		e.pads[6].p[0] = 0.25f;  // short
		e.pads[6].p[2] = 0.f;
		e.press(6);
		auto y = run(e, 60000, 220.f);
		CHECK(rms(y, 50000, 60000) < 0.01f, "tape stop not silent: %.3f", rms(y, 50000, 60000));
		Engine o;
		o.c.quant = 0;
		o.press(8);             // gate loops
		run(o, 1000, 220.f);
		o.c.mode = 1;
		o.press(0);             // one shot: one bar of stutter
		run(o, 2000, 220.f);
		CHECK(o.act == 0 && o.actOne, "one shot not running");
		run(o, 96000, 220.f);
		CHECK(o.act == 8 && !o.actOne, "one shot did not return to the loop (act %d)", o.act);
	}
	// ---- 7. HOLD: plays while held; latch toggles
	{
		Engine e;
		e.c.quant = 0;
		e.c.mode = 2;
		e.press(5);
		run(e, 4000, 220.f);
		CHECK(e.act == 5, "hold: not playing");
		e.release(5);
		run(e, 2000, 220.f);
		CHECK(e.act < 0, "hold: still playing after release");
		e.c.latch = true;
		e.press(5);
		e.release(5);
		run(e, 4000, 220.f);
		CHECK(e.act == 5, "latch: stopped on release");
		e.press(5);
		run(e, 2000, 220.f);
		CHECK(e.act < 0, "latch: second press did not stop");
	}
	// ---- 8. external clock: 16ths 125 ms apart → 120 BPM; quiet → back to the internal clock
	{
		Engine e;
		e.setBpm(90.f);
		for (int p = 0; p < 16; p++) {
			e.clockPulse();
			run(e, 6000, 0.f);
		}
		CHECK(std::fabs(e.bpm - 120.f) < 0.5f, "clock: %.2f BPM", e.bpm);
		CHECK(std::fabs(e.beat - 15.f / 4.f) < 0.26, "clock: beat %.3f", e.beat);
		run(e, 100000, 0.f);
		CHECK(!e.ext, "clock timeout");
	}
	// ---- 9. capture loop: plays the recorded bar again
	{
		Engine e;
		run(e, 1000, 0.f);
		e.recToggle();
		run(e, 95000 + 96000 + 1000, 0.f);  // arm, wait for the bar, record it
		CHECK(e.cap == Engine::CAP_PLAY, "capture not playing (%d)", e.cap);
		auto y = run(e, 200000, 0.f);  // noise in: only the loop repeats bar after bar
		double err = 0, en = 0;
		for (int i = 1000; i < 100000; i++) {
			err += std::fabs(y[i] - y[i + 96000]);
			en += std::fabs(y[i]);
		}
		CHECK(en > 0 && err / en < 0.01, "capture loop does not repeat: %.4f", en > 0 ? err / en : -1.0);
		e.recToggle();
		CHECK(e.cap == Engine::CAP_OFF, "capture not off");
	}
	// ---- 10. tap tempo
	{
		Engine e;
		for (int t = 0; t < 5; t++) {
			e.tap();
			run(e, 24000, 0.f);  // 0.5 s → 120 BPM
		}
		e.setBpm(e.bpm);
		CHECK(std::fabs(e.bpm - 120.f) < 0.5f, "tap: %.2f BPM", e.bpm);
	}
	printf(fails ? "%d FAILED\n" : "all STOTTER tests passed\n", fails);
	return fails ? 1 : 0;
}
