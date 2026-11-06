// ABLAUF song parts: kept tracks stay exact, each part does what it promises (sparse intro, rolling build with a gap,
// break without kick, thinning outro, fuller drop), over every genre and loop length.
// g++ -std=c++17 -O2 -Isrc test/test_ablauf.cpp src/Arranger.cpp src/Engine.cpp src/Genres.cpp src/Edit.cpp src/Store.cpp src/Midi.cpp -o /tmp/ta && /tmp/ta
#include "../src/Arranger.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace genra;

static int fails = 0;
#define CHECK(c, ...)                    \
	do {                                 \
		if (!(c)) {                      \
			if (fails < 40) {            \
				printf("FAIL " __VA_ARGS__); \
				printf("\n");            \
			}                            \
			fails++;                     \
		}                                \
	} while (0)

static Params baseP(int g, int bars) {
	Params P;
	P.genre = g;
	P.key = 9;
	P.scale = prof(g).scale;
	P.lengthBars = bars;
	return P;
}

static void makeSource(Slot& s, int g, int lenIdx, Rng& rng) {
	initPattern(s.pat);
	Params P = baseP(g, LENGTH_BARS[lenIdx]);
	bool lk[NUM_TRACKS] = {false, false, false, false};
	generate(s.pat, g, rng, lk, false, P);
	layoutTimeline(s.pat, LENGTH_BARS[lenIdx] * 16);
	s.lengthIdx = (int8_t)lenIdx;
	s.used = true;
	s.mods = ArrMods();
	s.mods.dens[2] = 0.07f; // the source's own offsets must survive on kept tracks
}

// what plays over one pass (fill off)
struct Count {
	int drum[NUM_DRUMS] = {};
	int drumFrom[NUM_DRUMS] = {}; // from step `from` on
	int bass = 0, bassFrom = 0, mel = 0, melFrom = 0;
	int snareSegs[4] = {}; // snare hits per quarter of the loop
	float vel = 0.f;
	int velN = 0;
};
static Count count(const Slot& s, const Params& P0, int from) {
	Count c;
	Params P = withMods(P0, s.mods);
	int L = LENGTH_BARS[s.lengthIdx] * 16;
	for (int st = 0; st < L; st++) {
		Hit h[NUM_DRUMS];
		drumStep(s.pat, P, st, L, false, h);
		for (int v = 0; v < NUM_DRUMS; v++)
			if (h[v].on) {
				c.drum[v]++;
				if (st >= from)
					c.drumFrom[v]++;
				if (v == SNARE)
					c.snareSegs[st * 4 / L]++;
				if (!std::isfinite(h[v].vel))
					c.velN = -1000;
			}
		NoteEv b = bassAt(s.pat, P, st, L), m = melAt(s.pat, P, st, L);
		c.bass += b.on;
		c.mel += m.on;
		if (st >= from)
			c.bassFrom += b.on, c.melFrom += m.on;
	}
	return c;
}

static bool sameTrack(const Pattern& a, const Pattern& b, int t) {
	switch (t) {
		case T_DRUMS: return !std::memcmp(a.drums, b.drums, sizeof(a.drums)) && a.fixed[t] == b.fixed[t];
		case T_BASS: return !std::memcmp(a.bass, b.bass, sizeof(a.bass)) && a.fixed[t] == b.fixed[t];
		case T_MELODY: return !std::memcmp(a.mel, b.mel, sizeof(a.mel)) && a.fixed[t] == b.fixed[t];
		default: return !std::memcmp(a.pads, b.pads, sizeof(a.pads)) && a.fixed[t] == b.fixed[t];
	}
}

int main() {
	initGenres();
	Rng rng(777);
	std::unique_ptr<Slot> src(new Slot), out(new Slot);
	int n = 0;
	for (int g = 0; g < numProfiles(); g++) {
		for (int li = 0; li < 4; li++) {
			int L = LENGTH_BARS[li] * 16;
			makeSource(*src, g, li, rng);
			Params P = baseP(g, LENGTH_BARS[li]);
			Count c0 = count(*src, P, L - 16 > 0 ? L - 16 : L / 2);
			const char* gn = prof(g).name;
			for (int sec = 0; sec < NUM_SECTIONS; sec++) {
				// 1. only the melody re-rolled: drums, bass, pads byte for byte, their offsets too
				makeSection(*src, *out, sec, 1u << T_MELODY, P, rng);
				for (int t : {T_DRUMS, T_BASS, T_PADS})
					CHECK(sameTrack(src->pat, out->pat, t), "%s L%d %s: kept track %d changed", gn, L, SECTION_NAMES[sec], t);
				CHECK(out->tag == sec && out->used, "tag");
				CHECK(out->mods.dens[T_DRUMS] == src->mods.dens[T_DRUMS] && out->mods.fill == src->mods.fill, "kept offsets");
				CHECK(!std::memcmp(out->pat.prog, src->pat.prog, sizeof(src->pat.prog)), "chords must stay");
				// 2. everything re-rolled
				makeSection(*src, *out, sec, 0xF, P, rng);
				Count c = count(*out, P, L >= 32 ? L - 16 : L / 2);
				CHECK(c.velN >= 0, "nan velocity");
				int lb = L >= 32 ? L - 16 : L / 2;
				Count cl = count(*out, P, lb);
				switch (sec) {
					case SEC_INTRO:
						CHECK(c.drum[KICK] > 0, "%s L%d intro: no kick at all", gn, L);
						CHECK(c.drum[TOM] == 0 && c.drum[PERC] == 0, "%s intro: toms/perc", gn);
						if (L >= 32) {
							Count h = count(*out, P, L / 2);
							CHECK(h.bass == h.bassFrom, "%s L%d intro: bass before the middle", gn, L);
							CHECK(h.drum[SNARE] == h.drumFrom[SNARE], "%s intro: snare in the first half", gn);
							CHECK(h.mel == h.melFrom, "%s intro: melody in the first half", gn);
						}
						CHECK(c.drum[CHAT] <= L / 2, "%s intro: hats too dense (%d)", gn, c.drum[CHAT]);
						break;
					case SEC_BUILD: {
						CHECK(c.drum[SNARE] >= L / 4, "%s L%d build: roll too thin (%d)", gn, L, c.drum[SNARE]);
						for (int q = 1; q < 4; q++)
							CHECK(c.snareSegs[q] >= c.snareSegs[q - 1], "%s L%d build: roll must get denser", gn, L);
						int lbBass = L >= 32 ? L - 16 : L - 4;
						Count cb = count(*out, P, lbBass);
						CHECK(cb.bassFrom == 0, "%s L%d build: bass in the gap", gn, L);
						// the last beat: no kick
						Hit h[NUM_DRUMS];
						for (int st = L - 4; st < L; st++) {
							drumStep(out->pat, withMods(P, out->mods), st, L, false, h);
							CHECK(!h[KICK].on, "%s L%d build: kick on the last beat", gn, L);
						}
						drumStep(out->pat, withMods(P, out->mods), L - 1, L, false, h);
						CHECK(h[SNARE].on && h[SNARE].ratchet >= 2, "%s build: no 32nds at the end", gn);
						CHECK(out->mods.fill < 0.f, "build: random fills must be off");
					} break;
					case SEC_BREAK:
						CHECK(c.drum[KICK] == 0, "%s L%d break: kick", gn, L);
						CHECK(c.bass > 0 && c.bass <= 8 * LENGTH_BARS[li], "%s L%d break: bass roots %d", gn, L, c.bass);
						CHECK(out->mods.dens[T_PADS] > 0.2f, "break: pads in front");
						break;
					case SEC_OUTRO:
						if (L >= 32) {
							CHECK(cl.drumFrom[SNARE] == 0 && cl.drumFrom[CHAT] == 0, "%s L%d outro: last bar not thin", gn, L);
							CHECK(cl.drumFrom[KICK] > 0, "%s L%d outro: no kick in the last bar", gn, L);
							CHECK(cl.bassFrom == 0, "%s outro: bass in the last bar", gn);
						}
						break;
					case SEC_DROP:
						CHECK(out->mods.dens[T_DRUMS] > 0.f && out->mods.dens[T_BASS] > 0.f, "drop offsets");
						CHECK(bassAt(out->pat, withMods(P, out->mods), 0, L).on, "%s drop: no bass on the one", gn);
						CHECK(!out->pat.fixed[T_DRUMS], "drop stays generated");
						break;
					case SEC_VERSE:
						CHECK(out->mods.dens[T_MELODY] < 0.f, "verse: melody leaves room");
						CHECK(!out->pat.fixed[T_MELODY], "verse stays generated");
						break;
				}
				// 3. it survives storage
				std::vector<uint8_t> b = savePattern(out->pat);
				Pattern back;
				CHECK(loadPattern(b, back) && !std::memcmp(&back.drums, &out->pat.drums, sizeof(back.drums)), "storage round trip");
				n++;
			}
			(void)c0;
		}
	}
	// the same source and seed state give a different result on the second press (re-roll)
	{
		Rng r(5);
		makeSource(*src, 3, 2, r);
		Params P = baseP(3, 4);
		std::unique_ptr<Slot> a(new Slot), b(new Slot);
		makeSection(*src, *a, SEC_DROP, 0xF, P, r);
		makeSection(*src, *b, SEC_DROP, 0xF, P, r);
		CHECK(std::memcmp(a->pat.drums, b->pat.drums, sizeof(a->pat.drums)) != 0, "second press must differ");
	}
	printf("%d parts made, %s\n", n, fails ? "FAILED" : "all ABLAUF tests passed");
	return fails != 0;
}
