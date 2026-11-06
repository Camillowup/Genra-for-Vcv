// MIX engine: unity path, level curve, EQ shelves, balance, mute/solo, send/return, ducking, width, limiter.
// g++ -O2 -std=c++17 test/test_mix.cpp -o /tmp/test_mix && /tmp/test_mix
#include "../src/mix/MixDsp.hpp"
#include <cstdio>
#include <memory>
#include <vector>
using namespace gmix;

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

struct Run {
	ChannelParams p[NCH];
	MasterParams m;
	std::unique_ptr<Mixer> mx{new Mixer};
	Run() { mx->setSampleRate(FS); }
	/** Sine (amplitude a, frequency f) into the channels in `mask`; returns the RMS of L and R over the last half. */
	void sine(int mask, float f, float a, float& rl, float& rr, float duckEvery = 0.f, std::vector<float>* outL = nullptr,
	          float* sendRms = nullptr, float retA = 0.f) {
		int n = (int) FS;
		double sl = 0, sr = 0, ss = 0;
		for (int i = 0; i < n; i++) {
			float x = a * std::sin(2.f * PI * f * i / FS);
			float in[NCH][2] = {};
			for (int c = 0; c < NCH; c++)
				if (mask & (1 << c))
					in[c][0] = in[c][1] = x;
			float duckIn = duckEvery > 0.f && std::fmod(i / FS, duckEvery) < 0.005f ? 10.f : 0.f;
			float L, R, SL, SR;
			mx->process(p, m, in, duckIn, retA * x, retA * x, L, R, SL, SR);
			if (outL)
				outL->push_back(L);
			if (i >= n / 2)
				sl += L * L, sr += R * R, ss += SL * SL;
		}
		rl = (float) std::sqrt(sl / (n - n / 2));
		rr = (float) std::sqrt(sr / (n - n / 2));
		if (sendRms)
			*sendRms = (float) std::sqrt(ss / (n - n / 2));
	}
};
static float db(float x, float ref) { return 20.f * std::log10(x / ref); }

int main() {
	const float ref = 5.f / std::sqrt(2.f);  // RMS of a 5 V sine
	// 1. unity: default knobs pass a 5 V sine unchanged on both sides; mono input in L only is normalled by the module
	{
		Run r;
		float l, rr;
		r.sine(1, 440.f, 5.f, l, rr);
		CHECK(std::fabs(db(l, ref)) < 0.05f && std::fabs(db(rr, ref)) < 0.05f, "unity: %.2f / %.2f dB", db(l, ref), db(rr, ref));
	}
	// 2. level curve
	CHECK(std::fabs(levelDb(0.75f)) < 1e-4f && std::fabs(levelDb(1.f) - 4.99f) < 0.02f && levelGain(0.f) == 0.f, "level curve");
	// 3. EQ: shelves give ±12 dB well inside their band, and leave the middle alone
	for (int hi = 0; hi < 2; hi++)
		for (float k : {-1.f, 1.f}) {
			Run r;
			(hi ? r.p[0].high : r.p[0].low) = k;
			float l, rr, lm, rm;
			r.sine(1, hi ? 16000.f : 30.f, 1.f, l, rr);
			Run r2;
			(hi ? r2.p[0].high : r2.p[0].low) = k;
			r2.sine(1, 1000.f, 1.f, lm, rm);
			float want = 12.f * k, got = db(l, 1.f / std::sqrt(2.f)), mid = db(lm, 1.f / std::sqrt(2.f));
			CHECK(std::fabs(got - want) < 1.f && std::fabs(mid) < 1.5f, "eq %s %+.0f: %.2f dB, 1 kHz %.2f dB",
			      hi ? "high" : "low", k, got, mid);
		}
	// 4. balance: hard left silences the right
	{
		Run r;
		r.p[0].pan = -1.f;
		float l, rr;
		r.sine(1, 440.f, 5.f, l, rr);
		CHECK(std::fabs(db(l, ref)) < 0.05f && rr < 1e-3f, "pan left: L %.2f dB, R %.5f", db(l, ref), rr);
	}
	// 5. mute and solo
	{
		Run r;
		r.p[0].mute = true;
		float l, rr;
		r.sine(1, 440.f, 5.f, l, rr);
		CHECK(l < 1e-3f, "mute: %.5f", l);
		Run s;
		s.p[1].solo = true;  // channel 1 soloed: channel 0 (playing) is silent
		s.sine(1, 440.f, 5.f, l, rr);
		CHECK(l < 1e-3f, "solo other: %.5f", l);
		Run t;
		t.p[0].solo = true;
		t.sine(1, 440.f, 5.f, l, rr);
		CHECK(std::fabs(db(l, ref)) < 0.05f, "solo self: %.2f dB", db(l, ref));
	}
	// 6. send (post-fader) and return
	{
		Run r;
		r.p[0].send = 1.f;
		float l, rr, s;
		r.sine(1, 440.f, 5.f, l, rr, 0.f, nullptr, &s);
		CHECK(std::fabs(db(s, ref)) < 0.05f, "send: %.2f dB", db(s, ref));
		Run q;
		q.sine(0, 440.f, 5.f, l, rr, 0.f, nullptr, nullptr, 1.f);
		CHECK(std::fabs(db(l, ref)) < 0.05f, "return: %.2f dB", db(l, ref));
	}
	// 7. ducking: a channel with DUCK 1 drops by the depth right after the trigger and recovers within TIME
	{
		Run r;
		r.p[0].duck = 1.f;
		r.m.duck = 1.f, r.m.time = 0.4f;
		std::vector<float> out;
		float l, rr;
		r.sine(1, 1000.f, 5.f, l, rr, 0.5f, &out);
		auto peakAt = [&](float t0, float t1) {
			float m = 0.f;
			for (int i = (int) (t0 * FS); i < (int) (t1 * FS); i++)
				m = std::max(m, std::fabs(out[i]));
			return m;
		};
		float T = duckSeconds(0.4f);
		float dip = peakAt(0.506f, 0.512f), back = peakAt(0.5f + T, 0.5f + T + 0.01f), before = peakAt(0.45f, 0.49f);
		CHECK(dip < 0.12f * before && back > 0.9f * before, "duck: before %.2f, dip %.2f, after TIME %.2f", before, dip, back);
		Run u;  // DUCK 0 on the channel: untouched
		u.m.duck = 1.f;
		u.sine(1, 1000.f, 5.f, l, rr, 0.5f);
		CHECK(std::fabs(db(l, ref)) < 0.05f, "duck off: %.2f dB", db(l, ref));
	}
	// 8. width 0: mono (L == R) even with a hard-panned source
	{
		Run r;
		r.p[0].pan = -1.f;
		r.m.width = 0.f;
		float l, rr;
		r.sine(1, 440.f, 5.f, l, rr);
		CHECK(std::fabs(l - rr) < 1e-3f, "width 0: L %.3f R %.3f", l, rr);
	}
	// 9. everything at once, full drive, all channels hot: bounded and finite
	{
		Run r;
		for (auto& p : r.p)
			p.level = 1.f, p.high = p.low = 1.f, p.send = 1.f;
		r.m.drive = 1.f, r.m.master = 1.f, r.m.width = 1.f, r.m.ret = 1.f;
		std::vector<float> out;
		float l, rr;
		r.sine(15, 80.f, 10.f, l, rr, 0.25f, &out, nullptr, 1.f);
		float mx = 0.f;
		bool ok = true;
		for (float x : out)
			ok &= std::isfinite(x), mx = std::max(mx, std::fabs(x));
		CHECK(ok && mx <= 10.f, "hot: max %.3f", mx);
	}
	// 10. meter: a 5 V sine lights the 0 dB segment (11 of 12)
	{
		Meter m;
		m.setSampleRate(FS);
		for (int i = 0; i < 4800; i++)
			m.feed(5.f * std::sin(2.f * PI * 440.f * i / FS));
		CHECK(m.segments() == 11, "meter: %d segments", m.segments());
	}
	printf(fails ? "%d FAILED\n" : "all MIX tests passed\n", fails);
	return fails != 0;
}
