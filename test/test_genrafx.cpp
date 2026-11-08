// GENRA FX DSP: the reverb and the delay stay finite and bounded at every extreme, decay about as long as set, the
// delay hits its division, silence stays silence. Also prints the CPU cost. Build: g++ -std=gnu++17 -O2
#include "../src/genrafx/GenraFxDsp.hpp"
#include <chrono>
#include <cstdio>
#include <random>

static int fails = 0;
#define CHECK(c, ...) \
	do { \
		if (!(c)) { \
			printf("FAIL: " __VA_ARGS__); \
			printf("\n"); \
			fails++; \
		} \
	} while (0)

static float noise(std::mt19937& g) { return std::uniform_real_distribution<float>(-1.f, 1.f)(g); }

/** RT60 from the energy decay of a short burst: the slope between -10 and -40 dB, extrapolated to 60 dB. */
static float measureRt60(gfx::Reverb& rv, float sr, float maxS) {
	std::mt19937 g(1);
	int frame = (int) (0.01f * sr);
	std::vector<float> e;
	float acc = 0.f;
	int n = (int) (maxS * sr);
	for (int i = 0; i < n; i++) {
		float x = i < (int) (0.01f * sr) ? 5.f * noise(g) : 0.f, l, r;
		rv.process(x, x, l, r);
		acc += l * l + r * r;
		if ((i + 1) % frame == 0) {
			e.push_back(10.f * std::log10(acc / frame + 1e-30f));
			acc = 0.f;
		}
	}
	int pk = (int) (std::max_element(e.begin(), e.end()) - e.begin());
	float top = e[(size_t) pk];
	int t10 = -1, t40 = -1;
	for (size_t i = (size_t) pk; i < e.size(); i++) {
		if (t10 < 0 && e[i] < top - 10.f)
			t10 = (int) i;
		if (t40 < 0 && e[i] < top - 40.f) {
			t40 = (int) i;
			break;
		}
	}
	if (t10 < 0 || t40 < 0)
		return -1.f;
	return 60.f * (float) (t40 - t10) * 0.01f / 30.f;
}

int main() {
	const float RATES[4] = {44100.f, 48000.f, 96000.f, 192000.f};

	// ---- silence in, silence out
	for (float sr : RATES) {
		gfx::Reverb rv;
		rv.setSampleRate(sr);
		gfx::Delay dl;
		dl.setSampleRate(sr);
		float mx = 0.f;
		for (int i = 0; i < (int) sr; i++) {
			float a, b, c, d;
			rv.process(0.f, 0.f, a, b);
			dl.process(0.f, 0.f, c, d);
			mx = std::max({mx, std::fabs(a), std::fabs(b), std::fabs(c), std::fabs(d)});
		}
		CHECK(mx == 0.f, "silence at %.0f Hz gives %g", sr, mx);
	}

	// ---- reverb decay follows DECAY
	struct Case { int type; float decay; } cases[] = {{gfx::HALL, 2.f}, {gfx::ROOM, 0.8f}, {gfx::PLATE, 3.f}, {gfx::SPACE, 8.f}};
	for (auto& c : cases) {
		gfx::Reverb rv;
		gfx::ReverbParams p;
		p.type = c.type;
		p.decay = c.decay;
		p.damp = 0.f;
		p.predelayMs = 0.f;
		rv.P = p;
		rv.setSampleRate(48000.f);
		float rt = measureRt60(rv, 48000.f, c.decay * 1.6f + 1.f);
		printf("reverb %-5s decay %.1f s: measured RT60 %.2f s\n", gfx::REVERB_TYPE_NAMES[c.type], c.decay, rt);
		CHECK(rt > c.decay * 0.6f && rt < c.decay * 1.4f, "%s: RT60 %.2f s for %.1f s", gfx::REVERB_TYPE_NAMES[c.type], rt, c.decay);
	}

	// ---- delay hits its division (120 bpm)
	struct DCase { int div, mode; float sec; } dcases[] = {
	    {2, gfx::STRAIGHT, 0.25f}, {2, gfx::DOTTED, 0.375f}, {3, gfx::TRIPLET, 1.f / 3.f}, {0, gfx::STRAIGHT, 0.0625f}, {5, gfx::STRAIGHT, 2.f}};
	for (auto& c : dcases) {
		gfx::Delay dl;
		gfx::DelayParams p;
		p.div = c.div, p.mode = c.mode, p.bpm = 120.f, p.feedback = 0.f;
		dl.P = p;
		dl.setSampleRate(48000.f);
		int n = (int) (c.sec * 48000.f) + 2000, at = -1;
		float best = 0.f;
		for (int i = 0; i < n; i++) {
			float l, r;
			dl.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, l, r);
			if (std::fabs(l) > best)
				best = std::fabs(l), at = i;
		}
		int want = (int) std::lround(c.sec * 48000.f);
		CHECK(std::abs(at - want) <= 1, "delay %s mode %d: echo at %d, want %d", gfx::DIV_NAMES[c.div], c.mode, at, want);
	}

	// ---- extremes for 60 s at every rate: freeze + shimmer, feedback 110 % with tape, ping-pong
	for (float sr : RATES) {
		std::mt19937 g(7);
		gfx::Reverb rv;
		gfx::ReverbParams rp;
		rp.type = gfx::SPACE, rp.size = 1.f, rp.decay = 30.f, rp.damp = 0.f, rp.shimmer = 1.f, rp.width = 1.5f, rp.level = 2.f;
		rv.P = rp;
		rv.setSampleRate(sr);
		gfx::Delay dl;
		gfx::DelayParams dp;
		dp.feedback = 1.1f, dp.tape = 1.f, dp.pingpong = true, dp.tone = 1.f, dp.width = 1.5f, dp.level = 2.f, dp.div = 0;
		dl.P = dp;
		dl.setSampleRate(sr);
		gfx::Delay dl2;
		gfx::DelayParams dp2 = dp;
		dp2.pingpong = false, dp2.tone = -1.f, dp2.div = 5, dp2.mode = gfx::DOTTED;
		dl2.P = dp2;
		dl2.setSampleRate(sr);
		float pkR = 0.f, pkD = 0.f, tailR = 0.f;
		bool ok = true;
		long n = (long) (60.f * sr);
		for (long i = 0; i < n; i++) {
			if (i == (long) (20.f * sr)) { // freeze after 20 s of continuous noise
				rp.freeze = true;
				rv.set(rp);
			}
			if (i == (long) (40.f * sr)) { // and a sudden size/type change while frozen
				rp.type = gfx::ROOM, rp.size = 0.f;
				rv.set(rp);
			}
			float x = i < (long) (20.f * sr) ? 3.f * noise(g) : 0.f;
			float b = i < (long) (0.5f * sr) ? 5.f * noise(g) : 0.f;
			float a1, a2, b1, b2, c1, c2;
			rv.process(x, -x, a1, a2);
			dl.process(b, b, b1, b2);
			dl2.process(b, 0.f, c1, c2);
			if (!std::isfinite(a1 + a2 + b1 + b2 + c1 + c2)) {
				ok = false;
				break;
			}
			pkR = std::max({pkR, std::fabs(a1), std::fabs(a2)});
			pkD = std::max({pkD, std::fabs(b1), std::fabs(b2), std::fabs(c1), std::fabs(c2)});
			if (i > (long) (39.f * sr) && i < (long) (40.f * sr))
				tailR = std::max(tailR, std::fabs(a1));
		}
		printf("extremes %.0f Hz: reverb peak %.2f V (frozen 19 s later %.2f V), delay peak %.2f V\n", sr, pkR, tailR, pkD);
		CHECK(ok, "not finite at %.0f Hz", sr);
		CHECK(pkR <= 12.f && pkD <= 12.f, "runaway at %.0f Hz: reverb %.1f delay %.1f", sr, pkR, pkD);
		CHECK(tailR > 0.01f, "freeze lost its tail at %.0f Hz", sr);
	}

	// ---- CPU at 48 kHz
	{
		std::mt19937 g(3);
		gfx::Reverb rv;
		gfx::ReverbParams rp;
		rp.shimmer = 0.5f;
		rv.P = rp;
		rv.setSampleRate(48000.f);
		gfx::Delay dl;
		gfx::DelayParams dp;
		dp.tape = 0.5f, dp.duck = 0.5f;
		dl.P = dp;
		dl.setSampleRate(48000.f);
		const int N = 48000 * 10;
		std::vector<float> in(N);
		for (auto& v : in)
			v = noise(g);
		float sink = 0.f;
		auto t0 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) {
			float l, r;
			rv.process(in[(size_t) i], in[(size_t) i], l, r);
			sink += l;
		}
		auto t1 = std::chrono::steady_clock::now();
		for (int i = 0; i < N; i++) {
			float l, r;
			dl.process(in[(size_t) i], in[(size_t) i], l, r);
			sink += l;
		}
		auto t2 = std::chrono::steady_clock::now();
		double rus = std::chrono::duration<double, std::micro>(t1 - t0).count() / N;
		double dus = std::chrono::duration<double, std::micro>(t2 - t1).count() / N;
		printf("CPU 48 kHz: reverb %.3f us/sample (%.1f %% of a sample), delay %.3f us/sample (%.1f %%) [%g]\n", rus,
		       rus / (1e6 / 48000.0) * 100.0, dus, dus / (1e6 / 48000.0) * 100.0, (double) (sink * 0.f));
	}

	printf("genrafx fails=%d\n", fails);
	return fails ? 1 : 0;
}
