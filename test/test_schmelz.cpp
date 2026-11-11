// SCHMELZ engine: pitch of every registration, octave and solo sound, envelope times, split, level, and stability of
// the whole chain at all sample rates and extreme settings.
// g++ -O2 -std=c++17 test/test_schmelz.cpp -o /tmp/test_schmelz && /tmp/test_schmelz
#include "../src/schmelz/SchmelzPresets.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
using namespace gschmelz;

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
	return (float) std::sqrt(re * re + im * im);
}

int main() {
	const float fs = 48000.f;
	// 1. strings: every registration in tune; 8VA an octave up; BOTH keeps the fundamental
	for (int r = 0; r < NUM_REGS; r++)
		for (int oct = 0; oct < NUM_OCT; oct++) {
			auto v = std::make_unique<StringsVoice>();
			v->setSampleRate(fs);
			StringsParams p;
			p.reg = r / (NUM_REGS - 1.f), p.oct = oct, p.cresc = 0.f;
			float f = 220.f;
			std::vector<float> x(24000);
			for (size_t i = 0; i < x.size(); i++)
				x[i] = v->process(p, f, true, i == 0, 1.f);
			float want = oct == OCT_8VA ? 2.f * f : f;
			float got = peakNear(x, 4000, 16000, fs, want);
			float cents = 1200.f * std::log2(got / want);
			CHECK(std::fabs(cents) < 3.f, "strings %s oct %d: %+.1f cents", REG_NAMES[r], oct, cents);
			if (oct == OCT_8VA && REGS[r].sub < 0.05f) {  // no fundamental left without the sub
				float e1 = energyAt(x, 4000, 16000, fs, f), e2 = energyAt(x, 4000, 16000, fs, 2.f * f);
				CHECK(e1 < 0.1f * e2, "strings %s 8VA: fundamental %.3f vs octave %.3f", REG_NAMES[r], e1, e2);
			}
		}
	// 2. solo: every sound in tune
	for (int k = 0; k < NUM_SOUNDS; k++) {
		auto v = std::make_unique<SoloVoice>();
		v->setSampleRate(fs);
		SoloParams p;
		p.sound = k / (NUM_SOUNDS - 1.f), p.sus = 1.f;
		float f = 196.f;
		std::vector<float> x(24000);
		for (size_t i = 0; i < x.size(); i++)
			x[i] = v->process(p, f, true, i == 0, 1.f);
		float got = peakNear(x, 2000, 16000, fs, f);
		float cents = 1200.f * std::log2(got / f);
		CHECK(std::fabs(cents) < 3.f, "solo %s: %+.1f cents", SOUND_NAMES[k], cents);
	}
	// 3. envelopes: crescendo reaches 90 % at its time; release falls 60 dB in its time
	{
		auto v = std::make_unique<StringsVoice>();
		v->setSampleRate(fs);
		StringsParams p;
		p.cresc = 0.6f, p.rel = 0.5f;
		float T = crescSeconds(p.cresc);
		int n90 = -1;
		for (int i = 0; i < (int) (fs * T * 3); i++) {
			v->process(p, 220.f, true, i == 0, 1.f);
			if (n90 < 0 && v->env >= 0.9f)
				n90 = i;
		}
		CHECK(n90 > 0 && std::fabs(n90 / fs / T - 1.f) < 0.15f, "crescendo: 90 %% after %.2f s, wanted %.2f s", n90 / fs, T);
		float R = releaseSeconds(p.rel), e0 = v->env;
		for (int i = 0; i < (int) (fs * R); i++)
			v->process(p, 220.f, false, false, 1.f);
		CHECK(v->env / e0 < 0.0015f && v->env / e0 > 0.0005f, "release: %.5f after %.2f s", v->env / e0, R);
	}
	// 4. split
	CHECK(soloTakes(SPLIT_LAYER, 1.f) && stringsTakes(SPLIT_LAYER, 1.f), "layer");
	CHECK(soloTakes(SPLIT_LOW, -0.5f) && !stringsTakes(SPLIT_LOW, -0.5f) && stringsTakes(SPLIT_LOW, 0.2f), "split low");
	CHECK(soloTakes(SPLIT_HIGH, 0.5f) && !soloTakes(SPLIT_HIGH, -0.1f) && stringsTakes(SPLIT_HIGH, -0.1f), "split high");

	// 5. the whole chain: a four-note chord on both sections is loud but not squashed, and nothing explodes
	for (float sr : {44100.f, 96000.f, 192000.f})
		for (int ens = 0; ens < NUM_ENS; ens++)
			for (int extreme = 0; extreme < 2; extreme++) {
				std::unique_ptr<StringsVoice[]> sv(new StringsVoice[16]);
				std::unique_ptr<SoloVoice[]> so(new SoloVoice[16]);
				auto bus = std::make_unique<Bus>();
				bus->setSampleRate(sr);
				for (int i = 0; i < 16; i++)
					sv[i].setSampleRate(sr), so[i].setSampleRate(sr), sv[i].seed(0x1234u * (i + 1)), so[i].seed(0x777u * (i + 3));
				BusParams bp;
				bp.ens = ens;
				StringsParams sp;
				SoloParams op;
				if (extreme) {
					bp.ensDepth = bp.anim = bp.animRate = bp.phaser = bp.phaserRate = bp.verb = bp.size = 1.f;
					bp.trem = bp.tremRate = bp.vol = 1.f;
					sp.tone = 1.f, op.sus = 1.f;
				}
				float chord[4] = {-0.75f, -0.4167f, -0.1667f, 0.f};
				int voices = extreme ? 16 : 4;
				float peak = 0.f, sum2 = 0.f;
				bool bad = false;
				int n = (int) (sr * 1.5f);
				for (int i = 0; i < n; i++) {
					float an = bus->animate(bp);
					float s = 0.f, o = 0.f;
					for (int c = 0; c < voices; c++) {
						StringsParams p = sp;
						p.reg = clampf((extreme ? c / 15.f : 2.f / 7.f) + an, 0.f, 1.f);
						p.oct = extreme ? c % 3 : OCT_BASE;
						op.sound = extreme ? (c % 5) / 4.f : 0.25f;
						float f = C4 * std::pow(2.f, chord[c % 4] + (extreme ? (c / 4) - 1 : 0));
						s += sv[c].process(p, f, true, i == 0, 1.f);
						o += so[c].process(op, f, true, i == 0, 1.f);
					}
					float L, R, so_, st;
					bus->process(bp, s, o, L, R, st, so_);
					bad |= !std::isfinite(L) || !std::isfinite(R) || !std::isfinite(st) || !std::isfinite(so_);
					peak = std::max(peak, std::max(std::fabs(L), std::fabs(R)));
					if (i > n / 2)
						sum2 += L * L;
				}
				float rms = std::sqrt(sum2 / (n - n / 2 - 1));
				CHECK(!bad && peak <= 10.001f, "chain sr %.0f ens %d extreme %d: peak %.2f", sr, ens, extreme, peak);
				if (!extreme)
					CHECK(rms > 0.8f && peak < 9.f, "level sr %.0f ens %d: rms %.2f V, peak %.2f V", sr, ens, rms, peak);
			}

	// 6. every preset, played as GENRA's pad track does: six four-note chords one after another, each starting where
	//    the one before is let go (its release rings into the next), then a long tail; mapped as Schmelz::process
	//    does. Measured before the safety limiter (outGain scaled down, then back up): the typical level while it plays (median of 50 ms windows),
	//    and how high the loudest moments go. Since 2.34 a preset must stay clear of the limiter (knee 6 V): its
	//    peaks (all but the loudest 0.01 % of samples) at most 6 V, so nothing is squashed or distorted, also not
	//    while chords overlap. Level about 1 V rms, even between presets, silent at the end.
	{
		const float sr = 48000.f;
		const float KNEE = 6.f;
		float lvl[NUM_PRESETS];
		for (int i = 0; i < NUM_PRESETS; i++) {
			const Preset& pr = PRESETS[i];
			StringsParams sp;
			SoloParams op;
			BusParams bp;
			pr.apply(sp, op, bp);
			const float scale = 0.05f; // keep the limiter out of the measurement
			bp.outGain = pr.gain * scale;
			const int NV = 8;
			std::unique_ptr<StringsVoice[]> sv(new StringsVoice[NV]);
			std::unique_ptr<SoloVoice[]> so(new SoloVoice[NV]);
			auto bus = std::make_unique<Bus>();
			bus->setSampleRate(sr);
			for (int c = 0; c < NV; c++)
				sv[c].setSampleRate(sr), so[c].setSampleRate(sr), sv[c].seed(0x1234u * (c + 1)), so[c].seed(0x777u * (c + 3));
			static const float CH[6][4] = {{-0.75f, -0.4167f, -0.1667f, 0.f},  {-0.5833f, -0.25f, 0.f, 0.25f},
			                               {-0.8333f, -0.5f, -0.25f, 0.0833f}, {-0.6667f, -0.3333f, -0.0833f, 0.1667f},
			                               {-0.9167f, -0.5833f, -0.3333f, 0.f}, {-0.75f, -0.4167f, -0.0833f, 0.25f}};
			const int len = (int) (1.5f * sr), played = 6 * len, n = played + (int) (10.f * sr), w = (int) (0.05f * sr);
			bool fin = true;
			float tail = 0.f, acc = 0.f;
			int cnt = 0;
			std::vector<float> mags, wins;
			mags.reserve(2 * played + 2 * sr);
			for (int k = 0; k < n; k++) {
				float an = bus->animate(bp);
				int cur = k / len; // the chord playing (6+ = none)
				float s = 0.f, o = 0.f;
				for (int c = 0; c < NV; c++) {
					int set = c / 4, me = cur < 6 && cur % 2 == set ? cur : (cur >= 1 && cur - 1 < 6 && (cur - 1) % 2 == set ? cur - 1 : -1);
					int chord = me >= 0 ? me : (set == 0 ? 4 : 5);
					bool g = me >= 0 && me == cur;
					bool trig = g && k == cur * len;
					StringsParams p = sp;
					p.reg = clampf(pr.reg + an, 0.f, 1.f);
					float pc = CH[chord][c % 4];
					float f = C4 * std::pow(2.f, pc);
					s += sv[c].process(p, f, g, trig, 0.8f);
					if (stringsTakes(pr.split, pc) || soloTakes(pr.split, pc))
						o += so[c].process(op, f, g || (pr.hold && k < played), trig, 0.8f);
				}
				float L, R, so_, st;
				bus->process(bp, s, o, L, R, st, so_);
				L /= scale, R /= scale;
				fin &= std::isfinite(L) && std::isfinite(R);
				if (k < played + (int) sr)
					mags.push_back(std::fabs(L)), mags.push_back(std::fabs(R));
				acc += 0.5f * (L * L + R * R);
				if (++cnt == w) {
					float r = std::sqrt(acc / w);
					if (k < played && k >= len / 2)
						wins.push_back(r);
					if (k >= n - 6 * w)
						tail = std::max(tail, r);
					acc = 0.f, cnt = 0;
				}
			}
			std::nth_element(wins.begin(), wins.begin() + wins.size() / 2, wins.end());
			float lv = wins[wins.size() / 2]; // the typical level while the chords play
			size_t q = mags.size() - mags.size() / 10000; // all but the loudest 0.01 %
			std::nth_element(mags.begin(), mags.begin() + q, mags.end());
			float peak = mags[q];
			float top = *std::max_element(mags.begin() + q, mags.end());
			lvl[i] = lv;
			printf("  %02d %-18s rms %.2f V  peak %.2f V  top %.2f V  tail %.4f V\n", i + 1, pr.name, lv, peak, top, tail);
			CHECK(fin, "preset %s: not finite", pr.name);
			CHECK(lv > 0.6f, "preset %s: too quiet (%.2f V rms)", pr.name, lv);
			CHECK(lv < 1.15f, "preset %s: too loud (%.2f V rms)", pr.name, lv);
			CHECK(peak <= KNEE * 1.01f, "preset %s: peaks %.2f V reach the limiter (knee %.0f V)", pr.name, peak, KNEE);
			CHECK(tail < 0.01f * lv + 1e-4f, "preset %s: still sounding after the release (%.4f V)", pr.name, tail);
		}
		float lo = 1e9f, hi = 0.f;
		for (float v : lvl)
			lo = std::min(lo, v), hi = std::max(hi, v);
		CHECK(hi / lo < 1.8f, "preset levels too far apart (%.2f .. %.2f V rms)", lo, hi);
		for (int i = 0; i < NUM_PRESETS; i++)
			for (int j = 0; j < i; j++)
				CHECK(std::strcmp(PRESETS[i].name, PRESETS[j].name) != 0, "preset name twice: %s", PRESETS[i].name);
	}
	// 7. the registrations run low to high: BASS first, the violin above the cello
	CHECK(std::strcmp(REG_NAMES[0], "BASS") == 0 && std::strcmp(REG_NAMES[3], "VIOLIN") == 0, "registration order");
	CHECK(REGS[0].lp < REGS[1].lp && REGS[1].lp < REGS[2].lp && REGS[2].lp < REGS[3].lp, "strings not low to high");

	printf(fails ? "%d FAILED\n" : "all SCHMELZ tests passed\n", fails);
	return fails != 0;
}
