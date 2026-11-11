// ABLAUF song parts: kept tracks stay exact, each part does what it promises (sparse intro, rolling build with a gap,
// break without kick, thinning outro, fuller drop), over every genre and loop length.
// g++ -std=c++17 -O2 -Isrc test/test_ablauf.cpp src/genra/Arranger.cpp src/genra/Engine.cpp src/genra/Genres.cpp src/genra/Edit.cpp src/genra/Store.cpp src/genra/Midi.cpp -o /tmp/ta && /tmp/ta
#include "../src/genra/Arranger.hpp"
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

// as GENRA plays a step (outside a song: the part's course over the loop)
static void playDrums(const Pattern& p, const Params& P, int st, int L, Hit h[NUM_DRUMS], int span = 0, int pos = -1) {
	drumStep(p, P, st, L, false, h);
	PartPos pp = partPos(p, pos >= 0 ? pos : st, span > 0 ? span : L);
	if (pp.sec >= 0 && (p.partMask & (1u << T_DRUMS)))
		shapeDrums(pp, h);
}
static NoteEv playNote(const Pattern& p, const Params& P, int st, int L, int t, int span = 0, int pos = -1) {
	NoteEv n = t == T_BASS ? bassAt(p, P, st, L) : melAt(p, P, st, L);
	PartPos pp = partPos(p, pos >= 0 ? pos : st, span > 0 ? span : L);
	if (pp.sec >= 0 && (p.partMask & (1u << t)))
		shapeNote(pp, t, n);
	return n;
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
		playDrums(s.pat, P, st, L, h);
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
		NoteEv b = playNote(s.pat, P, st, L, T_BASS), m = playNote(s.pat, P, st, L, T_MELODY);
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
							playDrums(out->pat, withMods(P, out->mods), st, L, h);
							CHECK(!h[KICK].on, "%s L%d build: kick on the last beat", gn, L);
						}
						playDrums(out->pat, withMods(P, out->mods), L - 1, L, h);
						CHECK(h[SNARE].on && h[SNARE].ratchet >= 2, "%s build: no 32nds at the end", gn);
						CHECK(out->mods.fill < 0.f, "build: random fills must be off");
					} break;
					case SEC_BREAK:
						if (prof(out->pat.trackGenre[T_DRUMS]).breakKeep & 1) // 2.30: techno keeps its kick through a break
							CHECK(c.drum[KICK] > 0 || c0.drum[KICK] == 0, "%s L%d break: the genre's kick is gone", gn, L);
						else
							CHECK(c.drum[KICK] == 0, "%s L%d break: kick", gn, L);
						CHECK(c.bass > 0 && c.bass <= 8 * LENGTH_BARS[li], "%s L%d break: bass roots %d", gn, L, c.bass);
						CHECK(out->mods.dens[T_PADS] == prof(out->pat.trackGenre[T_PADS]).padRise && out->mods.dens[T_PADS] > 0.1f,
						      "break: pads in front (the genre's rise)");
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
	// bass styles of BREAK and INTRO: every bar (from the entry on) has bass, except "bass out" (only a pickup);
	// each note sits on the chord's root, fifth or octave unless it comes from the source's own line (thinned)
	int styled = 0;
	for (int g = 0; g < numProfiles(); g += 3) {
		for (int li = 0; li < 4; li++) {
			int L = LENGTH_BARS[li] * 16;
			makeSource(*src, g, li, rng);
			Params P = baseP(g, LENGTH_BARS[li]);
			for (int sec : {SEC_BREAK, SEC_INTRO}) {
				int ns = sec == SEC_BREAK ? (int)NUM_BREAK_BASS : (int)NUM_INTRO_BASS;
				for (int st = 0; st < ns; st++) {
					PartOpts o;
					if (sec == SEC_BREAK)
						o.breakBass = st;
					else
						o.introBass = st;
					makeSection(*src, *out, sec, 1u << T_BASS, P, rng, o);
					Params Q = withMods(P, out->mods);
					int from = sec == SEC_INTRO && L >= 32 ? L / 2 : 0;
					bool silentOk = sec == SEC_BREAK && st == BB_OUT;
					int total = 0, heldTo = -1; // a long root held over a bar line sounds in that bar too
					for (int b = 0; b < L; b += 16) {
						int notes = heldTo > b ? 1 : 0;
						for (int s = b; s < b + 16 && s < L; s++) {
							NoteEv nn = playNote(out->pat, Q, s, L, T_BASS);
							if (!nn.on)
								continue;
							notes++;
							heldTo = std::max(heldTo, s + nn.len);
							CHECK(s >= from, "%s L%d %s bass %d: note before the entry", prof(g).name, L, SECTION_NAMES[sec], st);
						}
						total += notes;
						if (b >= from && !silentOk)
							CHECK(notes > 0, "%s L%d %s bass style %d: empty bar %d", prof(g).name, L, SECTION_NAMES[sec], st, b / 16);
					}
					if (silentOk) {
						CHECK(total >= 1 && total <= 2, "%s L%d bass out: pickup of 1-2 notes (%d)", prof(g).name, L, total);
						for (int s = 0; s < L - 4; s++)
							CHECK(!playNote(out->pat, Q, s, L, T_BASS).on, "bass out: a note before the last beat");
					}
					CHECK(sameTrack(src->pat, out->pat, T_DRUMS), "bass style: drums kept");
					styled++;
				}
			}
		}
	}
	// random style: different presses give different break basses
	{
		Rng r(11);
		makeSource(*src, 2, 2, r);
		Params P = baseP(2, 4);
		std::unique_ptr<Slot> a(new Slot);
		int diff = 0;
		makeSection(*src, *out, SEC_BREAK, 1u << T_BASS, P, r);
		for (int k = 0; k < 8; k++) {
			makeSection(*src, *a, SEC_BREAK, 1u << T_BASS, P, r);
			diff += std::memcmp(a->pat.bass, out->pat.bass, sizeof(a->pat.bass)) != 0;
		}
		CHECK(diff >= 6, "random break bass should vary (%d of 8)", diff);
	}
	printf("%d bass styles checked\n", styled);
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
	// MAKE SONG: every genre has a typical form; a few by name; the forms fit the row lengths (1-32 bars)
	{
		CHECK(songFormForGenre("Deep House") == SF_HOUSE && songFormForGenre("Trance") == SF_TRANCE &&
		          songFormForGenre("2-Step") == SF_GARAGE && songFormForGenre("Dubstep") == SF_BASS &&
		          songFormForGenre("Drum & Bass") == SF_JUNGLE && songFormForGenre("Lo-Fi") == SF_HIPHOP &&
		          songFormForGenre("Amapiano") == SF_AFRO && songFormForGenre("Ambient") == SF_DUB &&
		          songFormForGenre("Psytrance") == SF_TRANCE && songFormForGenre("Bassline") == SF_GARAGE &&
		          songFormForGenre("Phonk") == SF_HIPHOP && songFormForGenre("Bossa Nova") == SF_AFRO &&
		          songFormForGenre("Berlin School") == SF_DUB && songFormForGenre("Jersey Club") == SF_BASS,
		      "song form by genre");
		CHECK(songFormForGenre("nonsense") == SF_HOUSE && songFormForGenre(nullptr) == SF_HOUSE, "unknown genre form");
		int mapped = 0;
		for (int g = 0; g < numProfiles(); g++) {
			const char* gname = prof(g).name;
			bool house = std::strstr(gname, "House") || std::strstr(gname, "Techno") || !std::strcmp(gname, "Minimal") ||
			             !std::strcmp(gname, "Acid") || !std::strcmp(gname, "Hardcore") || !std::strcmp(gname, "EBM") ||
			             !std::strcmp(gname, "Industrial");
			int f = songFormForGenre(gname);
			if (f != SF_HOUSE || house)
				mapped++;
			else
				CHECK(false, "genre %s has no song form of its own\n", gname);
		}
		CHECK(songFormBars(SF_HOUSE) == 144, "house form length %d", songFormBars(SF_HOUSE));
		for (int f = 0; f < NUM_SONG_FORMS; f++) {
			CHECK(SONG_FORMS[f].n > 0 && SONG_FORMS[f].n <= MAX_FORM_PARTS, "form %d parts", f);
			for (int k = 0; k < SONG_FORMS[f].n; k++) {
				int b = SONG_FORMS[f].bars[k];
				CHECK(b == 4 || b == 8 || b == 16 || b == 32, "form %d part %d: %d bars", f, k, b);
				CHECK(SONG_FORMS[f].sec[k] >= 0 && SONG_FORMS[f].sec[k] < NUM_SECTIONS, "form %d part %d section", f, k);
			}
		}
		printf("%d genres with a song form\n", mapped);
	}
	// song MIDI: one row the length of the loop renders like the loop; rows in order, markers per row, muted tracks silent
	{
		Rng r(11);
		std::unique_ptr<Slot> a(new Slot), b(new Slot);
		makeSource(*a, 0, 2, r); // 4 bars
		makeSource(*b, 5, 1, r); // 2 bars
		Params Pa = baseP(0, 4), Pb = baseP(5, 2);
		std::vector<MidiNote> loop = renderLoop(a->pat, Pa, 64);
		std::vector<SongPart> parts(1);
		parts[0].pat = &a->pat, parts[0].P = Pa, parts[0].L = 64, parts[0].bars = 4;
		std::vector<MidiNote> one = renderSong(parts);
		bool same = loop.size() == one.size();
		for (size_t i = 0; same && i < loop.size(); i++)
			same = loop[i].startTick == one[i].startTick && loop[i].note == one[i].note && loop[i].len == one[i].len;
		CHECK(same, "song of one loop-long row = the loop (%d vs %d notes)", (int)loop.size(), (int)one.size());
		parts.resize(3);
		parts[0].bars = 8; // the 4-bar loop twice
		parts[0].label = "INTRO 1";
		parts[1].pat = &b->pat, parts[1].P = Pb, parts[1].L = 32, parts[1].bars = 4, parts[1].mute = 1u << T_DRUMS;
		parts[1].label = "DROP 1";
		parts[2].pat = &a->pat, parts[2].P = Pa, parts[2].L = 64, parts[2].bars = 2, parts[2].fill = true;
		parts[2].label = "OUTRO 1";
		std::vector<SongMarker> marks;
		std::vector<MidiNote> song = renderSong(parts, &marks);
		const int TPB = 16 * 24;
		CHECK(marks.size() == 3 && marks[0].tick == 0 && marks[1].tick == 8 * TPB && marks[2].tick == 12 * TPB, "markers");
		CHECK(marks.size() == 3 && marks[1].text == "DROP 1", "marker text");
		bool inRange = true, drumsInDrop = false;
		for (const MidiNote& m : song) {
			inRange &= m.startTick >= 0 && m.startTick + m.len <= 14 * TPB;
			if (m.track == 0 && m.startTick >= 8 * TPB && m.startTick < 12 * TPB)
				drumsInDrop = true;
		}
		CHECK(inRange, "song notes inside the song");
		CHECK(!drumsInDrop, "muted drums in the second row");
		std::vector<uint8_t> smf = writeSmf(song, 124.f, "Song", 14 * 16, nullptr, &marks);
		std::string blob(smf.begin(), smf.end());
		CHECK(blob.find("INTRO 1") != std::string::npos && blob.find("OUTRO 1") != std::string::npos, "markers in the file");
		printf("song MIDI: %d notes, %d bytes\n", (int)song.size(), (int)smf.size());
	}
	// 2.28: a part runs its course over the song row, not over a short loop: a 1-bar BREAK in a 16-bar row has its
	// pickup once at the end and no drums in the first half; a 1-bar BUILD in an 8-bar row rolls once, getting denser
	{
		int gH = 0;
		for (int g = 0; g < numProfiles(); g++)
			if (std::strcmp(prof(g).name, "House") == 0)
				gH = g;
		Rng r(23);
		std::unique_ptr<Slot> src(new Slot), brk(new Slot), bld(new Slot), drop(new Slot);
		makeSource(*src, gH, 0, r); // 1 bar
		Params P = baseP(gH, 1);
		makeSection(*src, *brk, SEC_BREAK, 0xF, P, r);
		makeSection(*src, *bld, SEC_BUILD, 0xF, P, r);
		makeSection(*src, *drop, SEC_DROP, 0xF, P, r);
		CHECK(brk->pat.partTag == SEC_BREAK + 1 && (brk->pat.partMask & 1), "break: tagged and shaped");
		CHECK(drop->pat.partMask == 0, "drop: not shaped");
		const int L = 16, span = 16 * 16;
		int firstHalf = 0, pickups = 0;
		for (int g = 0; g < span; g++) {
			Hit h[NUM_DRUMS];
			playDrums(brk->pat, withMods(P, brk->mods), g % L, L, h, span, g);
			for (int v = 0; v < NUM_DRUMS; v++)
				if (h[v].on && g < span / 2)
					firstHalf++;
			if (h[SNARE].on)
				pickups++;
			if (h[SNARE].on)
				CHECK(g >= span - 4, "break: a snare before the row's last beat (step %d)", g);
		}
		CHECK(firstHalf == 0, "break: drums in the first half of a 16-bar row (%d)", firstHalf);
		CHECK(pickups == 4, "break: one pickup of 4 snares at the row's end (%d)", pickups);
		const int bspan = 8 * 16;
		int seg[4] = {}, r32 = 0, kickLast = 0;
		for (int g = 0; g < bspan; g++) {
			Hit h[NUM_DRUMS];
			playDrums(bld->pat, withMods(P, bld->mods), g % L, L, h, bspan, g);
			if (h[SNARE].on)
				seg[g * 4 / bspan]++;
			if (h[SNARE].on && h[SNARE].ratchet >= 2)
				r32++;
			if (g >= bspan - 4 && h[KICK].on)
				kickLast++;
		}
		for (int q = 1; q < 4; q++)
			CHECK(seg[q] > seg[q - 1], "build: the roll gets denser over the row (%d %d %d %d)", seg[0], seg[1], seg[2], seg[3]);
		CHECK(r32 == 4, "build: 32nds only on the row's last beat (%d)", r32);
		CHECK(kickLast == 0, "build: kick on the row's last beat");
		// MIDI: the song row renders the same course
		std::vector<SongPart> parts(1);
		parts[0].pat = &brk->pat, parts[0].P = withMods(P, brk->mods), parts[0].L = L, parts[0].bars = 16;
		std::vector<MidiNote> song = renderSong(parts);
		int early = 0;
		for (const MidiNote& m : song)
			if (m.track == 0 && m.startTick < 8 * 16 * 24)
				early++;
		CHECK(early == 0, "song MIDI: break drums in the first half (%d)", early);
		// stored and loaded with its part
		Pattern q;
		CHECK(loadPattern(savePattern(brk->pat), q) && q.partTag == brk->pat.partTag && q.partMask == brk->pat.partMask,
		      "part tag round trip");
		// long verse / drop rows: a fill closes every 8-bar phrase inside the row
		CHECK(partPhraseFill(drop->pat, 7, 32) && partPhraseFill(drop->pat, 23, 32) && !partPhraseFill(drop->pat, 31, 32) &&
		          !partPhraseFill(drop->pat, 6, 32) && !partPhraseFill(drop->pat, 7, 8) && !partPhraseFill(brk->pat, 7, 32),
		      "phrase fills");
		printf("parts over the row: break pickups %d, build roll %d/%d/%d/%d\n", pickups, seg[0], seg[1], seg[2], seg[3]);
	}
	printf("%d parts made, %s\n", n, fails ? "FAILED" : "all ABLAUF tests passed");
	return fails != 0;
}
