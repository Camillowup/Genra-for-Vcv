// Tests for SCHLAG's parts without Rack (src/schlag/SchlagDsp.hpp): g++ -std=gnu++17 -O2 test/test_schlag.cpp
#include "../src/schlag/SchlagDsp.hpp"
#include <cassert>
#include <cstdio>

using namespace schlag;

static int fails = 0;
#define CHECK(c)                                                    \
	do {                                                            \
		if (!(c)) {                                                 \
			std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
			fails++;                                                \
		}                                                           \
	} while (0)

static void put16(FILE* f, uint16_t v) { std::fputc(v & 255, f), std::fputc(v >> 8, f); }
static void put32(FILE* f, uint32_t v) { put16(f, v & 0xffff), put16(f, v >> 16); }

/** A WAV file: n frames of a decaying sine, `ch` channels, 16 or 24 bit PCM or 32 bit float. */
static void writeWav(const char* path, int n, int ch, int bits, bool flt, int rate = 44100) {
	FILE* f = std::fopen(path, "wb");
	int bps = bits / 8;
	uint32_t data = (uint32_t) (n * ch * bps);
	std::fwrite("RIFF", 1, 4, f);
	put32(f, 36 + 8 + data);
	std::fwrite("WAVEfmt ", 1, 8, f);
	put32(f, 16);
	put16(f, flt ? 3 : 1);
	put16(f, (uint16_t) ch);
	put32(f, (uint32_t) rate);
	put32(f, (uint32_t) (rate * ch * bps));
	put16(f, (uint16_t) (ch * bps));
	put16(f, (uint16_t) bits);
	std::fwrite("LIST", 1, 4, f); // a chunk to skip
	put32(f, 0);
	std::fwrite("data", 1, 4, f);
	put32(f, data);
	for (int i = 0; i < n; i++)
		for (int c = 0; c < ch; c++) {
			float v = 0.8f * std::sin(i * 0.05f) * std::exp(-i / (float) n * 3.f) * (c ? -1.f : 1.f);
			if (flt)
				std::fwrite(&v, 4, 1, f);
			else if (bits == 16)
				put16(f, (uint16_t) (int16_t) std::lround(v * 32767.f));
			else {
				int32_t x = (int32_t) std::lround(v * 8388607.f);
				std::fputc(x & 255, f), std::fputc((x >> 8) & 255, f), std::fputc((x >> 16) & 255, f);
			}
		}
	std::fclose(f);
}

int main() {
	// ---- WAV
	std::string err;
	writeWav("schlag_s16.wav", 4410, 1, 16, false);
	writeWav("schlag_s24.wav", 2000, 2, 24, false, 48000);
	writeWav("schlag_sf.wav", 1000, 2, 32, true);
	auto a = readWav("schlag_s16.wav", err);
	CHECK(a && a->frames == 4410 && a->channels == 1 && a->rate == 44100.f);
	CHECK(a && std::fabs(a->data[20] - 0.8f * std::sin(1.f) * std::exp(-20 / 4410.f * 3.f)) < 1e-3f);
	auto b = readWav("schlag_s24.wav", err);
	CHECK(b && b->frames == 2000 && b->channels == 2 && b->rate == 48000.f);
	CHECK(b && std::fabs(b->data[2 * 30] + b->data[2 * 30 + 1]) < 1e-4f); // right = -left
	auto c = readWav("schlag_sf.wav", err);
	CHECK(c && c->frames == 1000 && c->channels == 2);
	CHECK(!readWav("schlag_does_not_exist.wav", err));
	auto cut = readWav("schlag_s16.wav", err, 0.05f);
	CHECK(cut && cut->frames == 2205);
	CHECK(a && a->peaks[0] > 0.3f && a->peaks[63] < a->peaks[0]);

	// ---- voice: plays the region, forwards and reversed, and ends
	Voice v;
	HitParams hp;
	v.start(a, hp, 44100.f);
	int n = 0;
	float l, r, peak = 0.f;
	while (v.active && n < 10000) {
		v.next(l, r, 0.f, 16);
		peak = std::max(peak, std::fabs(l));
		n++;
	}
	CHECK(n > 4300 && n < 4420);
	CHECK(peak > 0.5f && peak <= 1.f);
	hp.start = 0.5f, hp.end = 0.1f; // reverse
	v.start(a, hp, 44100.f);
	CHECK(v.dir == -1);
	n = 0;
	while (v.active && n < 10000) {
		v.next(l, r, 0.f, 16);
		n++;
	}
	CHECK(n > 1700 && n < 1780);
	hp = HitParams();
	hp.semis = 12.f; // an octave up: half the length
	v.start(a, hp, 44100.f);
	n = 0;
	while (v.active && n < 10000) {
		v.next(l, r, 0.f, 16);
		n++;
	}
	CHECK(n > 2150 && n < 2210);
	hp = HitParams();
	hp.decayMs = 10.f; // decay: silent after 10 ms
	v.start(a, hp, 44100.f);
	n = 0;
	while (v.active && n < 10000) {
		v.next(l, r, 0.f, 16);
		n++;
	}
	CHECK(n <= 444);
	// fade out (choke)
	v.start(a, HitParams(), 44100.f);
	v.fadeOut(44100.f, 5.f);
	n = 0;
	while (v.active && n < 10000) {
		v.next(l, r, 0.f, 16);
		n++;
	}
	CHECK(n <= 222);
	// drive and bits stay in range
	v.start(a, HitParams(), 44100.f);
	for (int i = 0; i < 1000; i++) {
		v.next(l, r, 1.f, 3);
		CHECK(std::fabs(l) <= 1.0001f);
	}

	// ---- filter: low-pass darkens, high-pass removes DC, off passes
	Svf f;
	f.set(0.1f, 0.2f, 44100.f);
	CHECK(f.mode == 1);
	float acc = 0.f;
	for (int i = 0; i < 2000; i++) {
		float x = (i & 1) ? 1.f : -1.f; // Nyquist
		float y = f.process(x, 0);
		if (i > 1000)
			acc += std::fabs(y);
	}
	CHECK(acc / 1000.f < 0.05f);
	f.set(0.9f, 0.2f, 44100.f);
	CHECK(f.mode == 2);
	f.reset();
	float y = 0.f;
	for (int i = 0; i < 20000; i++)
		y = f.process(1.f, 0);
	CHECK(std::fabs(y) < 0.01f);
	f.set(0.5f, 0.2f, 44100.f);
	CHECK(f.mode == 0 && f.process(0.3f, 0) == 0.3f);

	// ---- functions
	CHECK(fnText(F_LEVEL, FN[F_LEVEL].def) == "+0.0 dB");
	CHECK(std::fabs(levelGain(FN[F_LEVEL].def) - 1.f) < 1e-4f);
	CHECK(fnText(F_TUNE, 0.5f) == "+0 ST");
	CHECK(tuneSemis(1.f) == 24.f && tuneSemis(0.f) == -24.f);
	CHECK(fnText(F_CUTOFF, 0.5f) == "OFF");
	CHECK(fnText(F_DECAY, 1.f) == "FULL");
	CHECK(fnText(F_REPGRID, FN[F_REPGRID].def) == "4 HITS | 1 STEP");
	for (int fn = 0; fn < NUM_FN; fn++)
		if (FN[fn].steps > 1)
			for (int i = 0; i < FN[fn].steps; i++)
				CHECK(stepOf(fn, valueOfStep(fn, i)) == i);
	CHECK(FN[F_REPTYPE].steps == NUM_REPEAT_TYPES && FN[F_CHANCE].steps == NUM_CHANCES && FN[F_ACTION].steps == NUM_ACTIONS);

	// ---- chance
	CHECK(chancePasses(0, 0.99f, 0));
	CHECK(chancePasses(5, 0.49f, 0) && !chancePasses(5, 0.51f, 0)); // 50 %
	// PLAY 2 / SKIP 2 over loops 0..3: play, play, skip, skip
	CHECK(chancePasses(11, 0, 0) && chancePasses(11, 0, 1) && !chancePasses(11, 0, 2) && !chancePasses(11, 0, 3));
	// SKIP 1 / PLAY 3: skip, play, play, play
	CHECK(!chancePasses(17, 0, 0) && chancePasses(17, 0, 1) && chancePasses(17, 0, 3) && !chancePasses(17, 0, 4));

	// ---- RANDOM
	Rng rng;
	int16_t map[MAXS];
	rollMap(map, 5, 20, 0.f, rng);
	for (int s = 0; s < MAXS; s++)
		CHECK(map[s] == -1);
	rollMap(map, 5, 20, 1.f / 19.f, rng); // ±1 around the base
	int changed = 0;
	for (int s = 0; s < MAXS; s++) {
		CHECK(map[s] == -1 || map[s] == 4 || map[s] == 6);
		changed += map[s] != -1;
	}
	CHECK(changed > 50 && changed < 120); // about two thirds
	rollMap(map, 0, 20, 1.f, rng);
	int hi = 0;
	for (int s = 0; s < MAXS; s++) {
		CHECK(map[s] >= -1 && map[s] < 20 && map[s] != 0);
		hi = std::max(hi, (int) map[s]);
	}
	CHECK(hi > 12);
	{ // base at the folder's start, full RANDOM: evenly over the folder, not piled up on the base (2.28.1)
		int hits[8] = {}, N = 0;
		for (int k = 0; k < 400; k++) {
			rollMap(map, 0, 8, 1.f, rng);
			for (int s = 0; s < MAXS; s++, N++)
				hits[map[s] < 0 ? 0 : map[s]]++;
		}
		for (int i = 0; i < 8; i++)
			CHECK(std::fabs(hits[i] / (float) N - 0.125f) < 0.02f);
	}
	rollMap(map, 0, 1, 1.f, rng); // one file: nothing to roll
	for (int s = 0; s < MAXS; s++)
		CHECK(map[s] == -1);

	// ---- repeat types
	for (int t = 0; t < NUM_REPEAT_TYPES; t++)
		for (int k = 0; k < 8; k++) {
			RepeatMod m = repeatMod(t, k, 8, 0.7f);
			CHECK(m.gain >= 0.f && m.gain <= 1.f && m.bits >= 2 && m.bits <= 16 && std::fabs(m.semis) <= 36.f);
			if (k == 0 && t != R_RAISE && t != R_PONG && t != R_ROUNDTRIP)
				CHECK(m.semis == 0.f);
		}
	CHECK(repeatMod(R_ARPUP, 2, 4, 0.f).semis == 7.f);

	if (fails) {
		std::printf("%d failures\n", fails);
		return 1;
	}
	std::printf("SCHLAG: all tests passed\n");
	return 0;
}
