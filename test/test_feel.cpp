// 2.30: genre habits (chord moves, accents, song parts), melody phrases, VARY, the new genres and scales.
// g++ -std=c++17 -O2 -Isrc test/test_feel.cpp src/Arranger.cpp src/Engine.cpp src/Genres.cpp src/Edit.cpp src/Store.cpp src/Midi.cpp -o /tmp/tf && /tmp/tf
#include "../src/Arranger.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
using namespace genra;

static int fails = 0;
#define CHECK(c, ...)                        \
	do {                                     \
		if (!(c)) {                          \
			if (fails < 40) {                \
				printf("FAIL " __VA_ARGS__); \
				printf("\n");                \
			}                                \
			fails++;                         \
		}                                    \
	} while (0)

static int genreByName(const char* n) {
	for (int g = 0; g < NUM_GENRES; g++)
		if (!std::strcmp(prof(profileOf(g, 0)).name, n))
			return g;
	return -1;
}
static Params baseP(int p, int bars) {
	Params P;
	P.genre = p;
	P.key = 9;
	P.scale = prof(p).scale;
	P.lengthBars = bars;
	return P;
}
static int degreeOfRoot(const int* sc, int root) {
	for (int d = 0; d < 7; d++)
		if (sc[d] == root)
			return d;
	return -1;
}

int main() {
	initGenres();

	// ---- the new genres and scales
	{
		const char* NEW[5] = {"Witch House", "Darkwave", "Dungeon Synth", "Industrial", "Horror"};
		for (int k = 0; k < 5; k++) {
			int g = genreByName(NEW[k]);
			CHECK(g == 59 + k, "genre %s at %d (want %d)", NEW[k], g, 59 + k);
			CHECK(g >= 0 && numVariants(g) >= 2, "%s: variants", NEW[k]);
		}
		CHECK(NUM_SCALES == 12 && !std::strcmp(SCALES[9].name, "Locrian") && SCALES[10].iv[2] == 4 && SCALES[11].iv[6] == 11, "new scales");
		for (int p = 0; p < numProfiles(); p++) {
			const Genre& G = prof(p);
			CHECK(G.scale >= 0 && G.scale < NUM_SCALES, "%s scale", G.name);
			CHECK(G.cadTo >= -1 && G.cadTo <= 6, "%s cadTo", G.name);
			for (int k = 0; k < G.nHabit; k++)
				CHECK(G.habit[k].from >= 0 && G.habit[k].from < 7 && G.habit[k].to >= 0 && G.habit[k].to < 7 && G.habit[k].w > 0.f,
				      "%s habit %d", G.name, k);
			CHECK(G.padRise > 0.f && G.padRise <= 0.5f && (G.buildBars == 0 || G.buildBars == 4 || G.buildBars == 8 || G.buildBars == 16),
			      "%s part settings", G.name);
		}
		CHECK(songFormForGenre("Darkwave") == SF_TRANCE && songFormForGenre("Horror") == SF_DUB, "new genres' song forms");
	}

	// ---- chord habits (FREE progressions)
	{
		int synth = profileOf(genreByName("Synthwave"), 0);
		int trance = profileOf(genreByName("Trance"), 0);
		int house = profileOf(genreByName("House"), 0);
		int vMajor = 0, vAll = 0, aba = 0, progs = 0, cad = 0, cadN = 0;
		for (int seed = 0; seed < 400; seed++) {
			for (int which = 0; which < 2; which++) {
				int p = which ? trance : synth;
				const Genre& G = prof(p);
				Params P = baseP(p, 4);
				P.chordMode = CM_FREE;
				Pattern pat;
				initPattern(pat);
				Rng rng(1000 + seed * 7 + which);
				generateChords(pat, p, rng, P);
				const int* sc = SCALES[G.scale].iv;
				int d[MAX_PROG];
				for (int i = 0; i < pat.nProg; i++)
					d[i] = degreeOfRoot(sc, pat.prog[i].root);
				for (int i = 0; i < pat.nProg; i++)
					if (which == 0 && d[i] == 4) {
						vAll++;
						Chord c = resolveSpec(pat.prog[i], P, 0.f);
						vMajor += c.iv[1] == 4;
					}
				if (which == 1) {
					progs++;
					for (int i = 2; i < pat.nProg; i++)
						aba += d[i] == d[i - 2] && d[i - 1] != d[i];
				}
				if (pat.nProg >= 3 && d[0] == 0) {
					cadN++;
					cad += d[pat.nProg - 1] == G.cadTo;
				}
			}
		}
		CHECK(vAll > 0 && vMajor == vAll, "synthwave: V in minor is major (%d of %d)", vMajor, vAll);
		CHECK(aba == 0, "trance: A-B-A in %d of %d progressions", aba, progs);
		CHECK(cadN > 0 && cad * 2 > cadN, "the last chord leads home from the genre's degree (%d of %d)", cad, cadN);
		// a genre in a major key keeps the generic rules
		const Genre& B = prof(profileOf(genreByName("Bossa Nova"), 0));
		CHECK(B.nHabit == 0 && !B.raiseV, "bossa: no habits");
		(void)house;
	}

	// ---- accents
	{
		auto avgVel = [&](const char* gname, int voice, bool off8, int& n) {
			int p = profileOf(genreByName(gname), 0);
			Params P = baseP(p, 4);
			P.density[T_DRUMS] = 0.8f;
			float sum = 0.f;
			n = 0;
			for (int seed = 0; seed < 60; seed++) {
				Pattern pat;
				initPattern(pat);
				Rng rng(31 + seed);
				bool lk[NUM_TRACKS] = {};
				generate(pat, p, rng, lk, false, P);
				for (int st = 0; st < 64; st++) {
					if ((st % 4 == 2) != off8 || st % 2)
						continue;
					Hit h = drumHit(pat, P, voice, st, 64, false);
					if (h.on) {
						sum += h.vel;
						n++;
					}
				}
			}
			return n ? sum / n : 0.f;
		};
		int n1, n2;
		float hOff = avgVel("House", OHAT, true, n1);
		avgVel("Techno", KICK, false, n2);
		CHECK(n1 > 0 && hOff > 0.85f, "house: off-beat open hats loud (%.2f, %d)", hOff, n1);
		CHECK(n2 > 0, "techno kicks");
		// laid-back snare (hip-hop), ghost hats (drum & bass), flams (afrobeat)
		int late = 0, ghost = 0, flam = 0;
		for (int seed = 0; seed < 40; seed++) {
			for (const char* gname : {"Hip-Hop", "Drum & Bass", "Afrobeat"}) {
				int p = profileOf(genreByName(gname), 0);
				Params P = baseP(p, 4);
				P.ghost = 0.6f;
				P.density[T_DRUMS] = 0.7f;
				Pattern pat;
				initPattern(pat);
				Rng rng(500 + seed);
				bool lk[NUM_TRACKS] = {};
				generate(pat, p, rng, lk, false, P);
				for (int st = 0; st < 64; st++) {
					Hit sn = drumHit(pat, P, SNARE, st, 64, false), ch = drumHit(pat, P, CHAT, st, 64, false);
					Hit pc = drumHit(pat, P, PERC, st, 64, false);
					if (!std::strcmp(gname, "Hip-Hop") && sn.on && sn.delay >= 0.006f)
						late++;
					if (!std::strcmp(gname, "Drum & Bass") && ch.on && ch.vel < 0.4f && st % 2)
						ghost++;
					if (!std::strcmp(gname, "Afrobeat") && pc.on && pc.ratchet == 2)
						flam++;
				}
			}
		}
		CHECK(late > 0, "hip-hop: laid-back snare");
		CHECK(ghost > 0, "drum & bass: ghost hats");
		CHECK(flam > 0, "afrobeat: flams on the perc");
	}

	// ---- song parts by genre
	{
		Rng rng(99);
		std::unique_ptr<Slot> src(new Slot), out(new Slot);
		for (const char* gname : {"Dubstep", "Techno", "Drum & Bass"}) {
			int p = profileOf(genreByName(gname), 0);
			const Genre& G = prof(p);
			initPattern(src->pat);
			Params P = baseP(p, 4);
			P.density[T_DRUMS] = 0.7f;
			bool lk[NUM_TRACKS] = {};
			generate(src->pat, p, rng, lk, false, P);
			layoutTimeline(src->pat, 64);
			src->lengthIdx = 2;
			src->used = true;
			src->mods = ArrMods();
			makeSection(*src, *out, SEC_BUILD, 0xF, P, rng);
			int kicks = 0;
			for (int st = 48; st < 64; st++) {
				Hit h[NUM_DRUMS];
				drumStep(out->pat, withMods(P, out->mods), st, 64, false, h);
				PartPos pp = partPos(out->pat, st, 64);
				shapeDrums(pp, h);
				kicks += h[KICK].on;
			}
			if (G.buildKick)
				CHECK(kicks > 0, "%s: kick in the build's last bar", gname);
			else
				CHECK(kicks == 0, "%s: the gap before the drop (%d kicks)", gname, kicks);
			// a long break: what the genre keeps through its first half
			makeSection(*src, *out, SEC_BREAK, 0xF, P, rng);
			int bk = 0, bh = 0;
			for (int st = 0; st < 64; st++) {
				Hit h[NUM_DRUMS];
				drumStep(out->pat, withMods(P, out->mods), st, 64, false, h);
				PartPos pp = partPos(out->pat, st * 2, 128 * 2); // 32 bars, the first half
				shapeDrums(pp, h);
				bk += h[KICK].on;
				bh += h[CHAT].on || h[OHAT].on;
			}
			CHECK(((G.breakKeep & 1) != 0) == (bk > 0), "%s: break kick %d (keep %d)", gname, bk, G.breakKeep);
			if (G.breakKeep & 2)
				CHECK(bh > 0, "%s: break keeps its hats", gname);
			CHECK(out->mods.dens[T_PADS] == G.padRise, "%s: pads' rise", gname);
		}
	}

	// ---- melody phrases: the period closes on the root, the answer stays open, the sequence is the motif moved
	{
		int closed = 0, closedN = 0, seqOk = 0, seqN = 0;
		for (int p = 0; p < numProfiles(); p++) {
			const Genre& G = prof(p);
			if (G.melStyle != MS_LEAD && G.melStyle != MS_SPARSE)
				continue;
			Params P = baseP(p, 8);
			Pattern pat;
			initPattern(pat);
			Rng rng(4242 + p);
			bool lk[NUM_TRACKS] = {};
			generate(pat, p, rng, lk, false, P);
			for (int bar : {3, 7}) {
				int land = -1;
				for (int s = 8; s < 16; s++)
					if (pat.mel[bar * 16 + s].r == 0)
						land = s;
				if (land < 0)
					continue; // the genre's rhythm has no late note: nothing to land on
				closedN++;
				int a = pat.mel[bar * 16 + land].a1;
				closed += a == 0 || a == 7;
				for (int s = land + 1; s < 16; s++)
					CHECK(pat.mel[bar * 16 + s].r == 255, "%s: a rest after the close", G.name);
			}
			for (int s = 0; s < 16; s++) { // bar 3 is bar 1 a third higher (where it is not reflected at the top)
				int a = pat.mel[s].a1, b = pat.mel[32 + s].a1;
				if (a + 2 <= 8) {
					seqN++;
					seqOk += b == a + 2 || b == a + 3 || b == a + 1; // (+-1: the pentatonic nudge)
				}
				CHECK(pat.mel[s].r == pat.mel[32 + s].r, "%s: the sequence keeps the motif's rhythm", G.name);
			}
			// the walk is still there when asked for
			P.melPhrase = false;
			Pattern w;
			initPattern(w);
			Rng r2(4242 + p);
			generate(w, p, r2, lk, false, P);
		}
		CHECK(closedN > 0 && closed == closedN, "phrases close on the root (%d of %d)", closed, closedN);
		CHECK(seqN > 0 && seqOk * 10 >= seqN * 9, "the sequence follows the motif (%d of %d)", seqOk, seqN);
	}

	// ---- VARY: a part that comes again is its last variant, a little changed; voices it leaves out stay out
	{
		Rng rng(7);
		int p = profileOf(genreByName("Techno"), 0);
		Params P = baseP(p, 4);
		std::unique_ptr<Slot> src(new Slot), first(new Slot), again(new Slot), fresh(new Slot);
		initPattern(src->pat);
		bool lk[NUM_TRACKS] = {};
		generate(src->pat, p, rng, lk, false, P);
		layoutTimeline(src->pat, 64);
		src->lengthIdx = 2;
		src->used = true;
		makeSection(*src, *first, SEC_BUILD, 0xF, P, rng);
		makeSection(*src, *again, SEC_BUILD, 0xF, P, rng);
		varySection(*first, *again, 0xF, P, rng);
		makeSection(*src, *fresh, SEC_BUILD, 0xF, P, rng);
		auto diff = [](const Slot& a, const Slot& b) {
			int d = 0;
			for (int v = 0; v < NUM_DRUMS; v++)
				for (int s = 0; s < 64; s++)
					d += (a.pat.drums[v][s].fv > 0) != (b.pat.drums[v][s].fv > 0);
			for (int s = 0; s < 64; s++)
				d += a.pat.bass[s].force != b.pat.bass[s].force || a.pat.bass[s].absNote != b.pat.bass[s].absNote;
			return d;
		};
		int dv = diff(*first, *again), df = diff(*first, *fresh);
		CHECK(dv > 0 && dv < df, "vary: close to the first (%d) and not a new roll (%d)", dv, df);
		bool snare = false;
		for (int s = 0; s < MAX_STEPS; s++)
			snare |= again->pat.drums[SNARE][s].fv > 0;
		CHECK(!snare, "vary: the build's frozen drums have no snare (the roll comes from the row)");
		CHECK(again->tag == SEC_BUILD && again->pat.partTag == SEC_BUILD + 1, "vary: still the part");
	}

	printf("feel fails=%d\n", fails);
	return fails ? 1 : 0;
}
