// GENRA – song parts for the arranger ABLAUF (see Arranger.hpp).
//
// VERSE and DROP stay generated: they keep following GENRA's knobs and only carry density offsets. INTRO, BUILD,
// BREAK and OUTRO are arranged over the loop (a snare roll, thinning bar by bar, long roots), which needs exact notes:
// the re-rolled track is generated, frozen with the part's settings and then shaped step by step.
#include "Arranger.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace genra {

const char* const SECTION_NAMES[NUM_SECTIONS] = {"INTRO", "VERSE", "BUILD", "DROP", "BREAK", "OUTRO"};
const char* const BREAK_BASS_NAMES[NUM_BREAK_BASS] = {"Long roots", "Thinned line", "Pulse", "Dub offbeats", "Bass out"};
const char* const INTRO_BASS_NAMES[NUM_INTRO_BASS] = {"Legato roots", "Thinned line", "Pulse"};

static inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline int mod12(int x) { return ((x % 12) + 12) % 12; }

Params withMods(const Params& P0, const ArrMods& m) {
	Params P = P0;
	for (int t = 0; t < NUM_TRACKS; t++)
		P.density[t] = clampf(P0.density[t] + m.dens[t], 0.f, 1.f);
	P.ghost = clampf(P0.ghost + m.ghost, 0.f, 1.f);
	P.fill = clampf(P0.fill + m.fill, 0.f, 1.f);
	return P;
}

// ================================================================= helpers on exact (frozen) notes
// steps 0..L-1 repeated over the rest of the pattern (as freezeTrack leaves it)
static void tile(Pattern& pat, int track, int L) {
	for (int s = L; s < MAX_STEPS; s++) {
		int src = s % L;
		switch (track) {
			case T_DRUMS:
				for (int v = 0; v < NUM_DRUMS; v++)
					pat.drums[v][s] = pat.drums[v][src];
				break;
			case T_BASS: pat.bass[s] = pat.bass[src]; break;
			case T_MELODY: pat.mel[s] = pat.mel[src]; break;
			default: pat.pads[s] = pat.pads[src]; break;
		}
	}
}

static void hit(Pattern& p, int v, int s, int vel, int ratchet = 1) {
	DrumStep& d = p.drums[v][s];
	d.fv = (uint8_t)clampi(vel, 0, 127);
	d.frat = (d.fv && ratchet > 1) ? (uint8_t)clampi(ratchet, 2, 4) : 0;
	d.fchance = d.fcond = 0;
}
static void kill(Pattern& p, int v, int s) { hit(p, v, s, 0); }
static void scaleHit(Pattern& p, int v, int s, float k) {
	DrumStep& d = p.drums[v][s];
	if (d.fv)
		d.fv = (uint8_t)clampi((int)std::lround(d.fv * k), 1, 127);
}

static void noteOff(NoteStep& n) {
	n.force = 2;
	n.absNote = NO_ABS;
	n.flen = n.fvel = n.fflags = n.fgate = 0;
	n.froot = -1;
	n.fchance = n.fcond = 0;
}
static void scaleNote(NoteStep& n, float k) {
	if (n.force == 1 && n.fvel)
		n.fvel = (uint8_t)clampi((int)std::lround(n.fvel * k), 1, 127);
}
static bool noteOn(const NoteStep& n) { return n.force == 1 && n.absNote != NO_ABS; }

// the bass root of the chord at step s (plus semis: 7 = its fifth, 12 = an octave up) as an exact note of len steps
static void bassRoot(Pattern& pat, const Params& P, int s, int len, int vel, int semis = 0) {
	Chord ch = chordAt(pat, P, s);
	int keyPc = mod12(P.key + P.transp);
	int base = 36 + keyPc;
	if (base > 41)
		base -= 12;
	int n = base + ch.root + semis;
	while (n > (semis >= 12 ? 67 : 55))
		n -= 12;
	while (n < 28)
		n += 12;
	NoteStep& b = pat.bass[s];
	b.force = 1;
	b.absNote = (int8_t)clampi(n - P.key - P.transp, -127, 127);
	b.flen = (uint8_t)clampi(len, 1, 64);
	b.fvel = (uint8_t)clampi(vel, 1, 127);
	b.fflags = FF_SET;
	b.fgate = 92;
	b.froot = (int8_t)mod12(ch.root);
	b.fchance = b.fcond = 0;
}

// exact notes: each kept note lasts until the next one (at most cap steps), without slides
static void legato(NoteStep* arr, int L, int cap) {
	for (int s = 0; s < L; s++) {
		if (!noteOn(arr[s]))
			continue;
		int k = 1;
		while (s + k < L && k < cap && !noteOn(arr[s + k]))
			k++;
		arr[s].flen = (uint8_t)k;
		arr[s].fgate = 92;
		arr[s].fflags = (uint8_t)((arr[s].fflags | FF_SET) & ~FF_SLIDE);
	}
}

static int countNotes(const NoteStep* arr, int from, int to) {
	int c = 0;
	for (int s = from; s < to; s++)
		c += noteOn(arr[s]);
	return c;
}

// the settings a track is frozen with
static Params partP(const Params& P, int t, float densOff, float densMin, float ghostK = 1.f, float cxOff = 0.f) {
	Params Q = P;
	Q.density[t] = clampf(P.density[t] + densOff, densMin, 1.f);
	Q.ghost = clampf(P.ghost * ghostK, 0.f, 1.f);
	Q.complexK = clampf(P.complexK + cxOff, 0.f, 1.f);
	Q.fillActive = false;
	return Q;
}

// last bar of the loop (the second half of a one-bar loop)

// ================================================================= bass styles (BREAK, INTRO)
// the frozen line, only on strong beats (1 and 3) and chord changes, shorter and softer; from step `from` on
static void bassThin(Pattern& p, const Params& P, int L, int from) {
	for (int s = 0; s < L; s++) {
		NoteStep& n = p.bass[s];
		bool strong = s % 8 == 0 || isEventStart(p, s);
		if (s < from || !strong || !noteOn(n)) {
			noteOff(n);
			continue;
		}
		n.flen = (uint8_t)clampi(std::min((int)n.flen, 6), 1, 6);
		n.fflags = (uint8_t)((n.fflags | FF_SET) & ~FF_SLIDE);
		scaleNote(n, 0.8f);
	}
	for (int b = (from / 16) * 16; b < L; b += 16) // no empty bar: the root on the one
		if (countNotes(p.bass, b, std::min(L, b + 16)) == 0)
			bassRoot(p, P, std::max(b, from), 4, 74);
}
// short roots on quarters or 8ths, now and then an octave up; from step `from` on
static void bassPulse(Pattern& p, const Params& P, int L, int from, Rng& rng) {
	for (int s = 0; s < L; s++)
		noteOff(p.bass[s]);
	int rate = rng.range(2) ? 4 : 2;
	float oct = 0.08f + 0.12f * rng.uni();
	for (int s = from; s < L; s += rate) {
		bool up = s % 16 != 0 && rng.uni() < oct;
		int vel = s % 8 == 0 ? 90 : 72;
		bassRoot(p, P, s, std::max(1, rate / 2 + (rate == 4 ? 1 : 0)), vel, up ? 12 : 0);
	}
}
// sparse single notes off the beat, roots and fifths, lots of space
static void bassDub(Pattern& p, const Params& P, int L, Rng& rng) {
	for (int s = 0; s < L; s++)
		noteOff(p.bass[s]);
	static const int SPOTS[6] = {6, 14, 3, 11, 7, 15}; // the "and" of 2 and 4 first, then 16th offsets
	for (int b = 0; b < L; b += 16) {
		int want = 1 + rng.range(2) + (rng.uni() < 0.2f ? 1 : 0);
		int placed = 0;
		for (int k = 0; k < 6 && placed < want; k++) {
			int s = b + SPOTS[k];
			if (s >= L || (k >= 2 && rng.uni() < 0.5f))
				continue;
			if (s > b && noteOn(p.bass[s - 1]))
				continue;
			bassRoot(p, P, s, 2 + rng.range(2), 88, rng.uni() < 0.3f ? 7 : 0);
			placed++;
		}
		if (!placed && b + 6 < L)
			bassRoot(p, P, b + 6, 2, 88);
		else if (!placed)
			bassRoot(p, P, b, 2, 88);
	}
}
// the bass is out: only a pickup of one or two short notes on the last beat, into the loop's start
static void bassOut(Pattern& p, const Params& P, int L, Rng& rng) {
	for (int s = 0; s < L; s++)
		noteOff(p.bass[s]);
	int last = L - 2;
	if (rng.range(2) && L >= 4)
		bassRoot(p, P, L - 4, 2, 80, 7);
	bassRoot(p, P, last, 2, 92);
}

// ================================================================= INTRO
// sparse drums that come in over the loop, the bass enters halfway on the chords, a thin melody, pads in front
static void introDrums(Pattern& p, const Params& P, int L) {
	// the material; how it comes in over the row: shapeDrums (Edit.cpp)
	freezeTrack(p, partP(P, T_DRUMS, -0.18f, 0.18f, 0.3f, -0.15f), T_DRUMS, L);
	for (int s = 0; s < L; s++) {
		int gs = s % 16;
		kill(p, TOM, s);
		kill(p, PERC, s);
		scaleHit(p, SNARE, s, 0.8f);
		scaleHit(p, CLAP, s, 0.8f);
		if (gs % 4 != 2)
			kill(p, OHAT, s);
		if (s % 2)
			kill(p, CHAT, s);
		scaleHit(p, OHAT, s, 0.8f);
		scaleHit(p, CHAT, s, 0.85f);
	}
	// never an empty intro: at least the downbeats
	bool any = false;
	for (int s = 0; s < L && !any; s++)
		any = p.drums[KICK][s].fv > 0;
	if (!any)
		for (int s = 0; s < L; s += 16)
			hit(p, KICK, s, 100);
	tile(p, T_DRUMS, L);
}

static void introBass(Pattern& p, const Params& P, int L, int style, Rng& rng) {
	freezeTrack(p, partP(P, T_BASS, style == IB_THIN ? 0.f : -0.25f, 0.12f), T_BASS, L);
	int from = 0; // the entry halfway through the row: shapeNote
	if (style == IB_THIN || style == IB_PULSE) {
		if (style == IB_THIN)
			bassThin(p, P, L, from);
		else
			bassPulse(p, P, L, from, rng);
		for (int s = 0; s < L; s++)
			scaleNote(p.bass[s], 0.9f);
		tile(p, T_BASS, L);
		return;
	}
	for (int s = 0; s < L; s++)
		if (s < from || !(s % 8 == 0 || isEventStart(p, s)))
			noteOff(p.bass[s]);
	// every bar from the entry on has at least its root on the one
	for (int b = from; b < L; b += 16)
		if (countNotes(p.bass, b, std::min(L, b + 16)) == 0 || !noteOn(p.bass[b]))
			bassRoot(p, P, b, 8, 92);
	legato(p.bass, L, 8);
	for (int s = 0; s < L; s++)
		scaleNote(p.bass[s], 0.9f);
	tile(p, T_BASS, L);
}

static void introMelody(Pattern& p, const Params& P, int L) {
	freezeTrack(p, partP(P, T_MELODY, -0.15f, 0.15f), T_MELODY, L);
	for (int s = 0; s < L; s++) {
		if (s % 2)
			noteOff(p.mel[s]);
		else
			scaleNote(p.mel[s], 0.85f);
	}
	tile(p, T_MELODY, L);
}

// ================================================================= BUILD
// a snare roll from quarters to 32nds getting louder, 16th hats in the second half, in the last bar the kick
// goes to quarters and stops on the last beat, the bass drops out: the drop lands in the gap
static void buildDrums(Pattern& p, const Params& P, int L) {
	// the groove without snares, claps, rims and toms; the roll over the row: shapeDrums (Edit.cpp)
	freezeTrack(p, partP(P, T_DRUMS, -0.08f, 0.2f, 0.5f), T_DRUMS, L);
	for (int s = 0; s < L; s++) {
		kill(p, SNARE, s);
		kill(p, CLAP, s);
		kill(p, RIM, s);
		kill(p, TOM, s);
	}
	tile(p, T_DRUMS, L);
}

static void buildBass(Pattern& p, const Params& P, int L) {
	// the gap before the drop at the row's end: shapeNote
	freezeTrack(p, partP(P, T_BASS, 0.f, 0.15f), T_BASS, L);
	tile(p, T_BASS, L);
}

static void buildMelody(Pattern& p, const Params& P, int L) {
	// rising over the row (the last quarter an octave up): shapeNote
	freezeTrack(p, partP(P, T_MELODY, 0.1f, 0.2f), T_MELODY, L);
	tile(p, T_MELODY, L);
}

// ================================================================= BREAK
// no kick, hats only on 8ths and quiet, long bass roots on the chords, pads in front; a short snare pickup at the end
static void breakDrums(Pattern& p, const Params& P, int L) {
	// no kick, hats only on 8ths and quiet; the breakdown and the pickup over the row: shapeDrums (Edit.cpp)
	freezeTrack(p, partP(P, T_DRUMS, -0.25f, 0.12f, 0.6f, -0.1f), T_DRUMS, L);
	for (int s = 0; s < L; s++) {
		kill(p, KICK, s);
		kill(p, OHAT, s);
		kill(p, TOM, s);
		kill(p, SNARE, s);
		kill(p, CLAP, s);
		if (s % 2)
			kill(p, CHAT, s);
		else
			scaleHit(p, CHAT, s, 0.7f);
		scaleHit(p, RIM, s, 0.8f);
		scaleHit(p, PERC, s, 0.8f);
	}
	tile(p, T_DRUMS, L);
}

static void breakBass(Pattern& p, const Params& P, int L, int style, Rng& rng) {
	freezeTrack(p, partP(P, T_BASS, 0.f, 0.1f), T_BASS, L);
	if (style != BB_ROOTS) {
		switch (style) {
			case BB_THIN: bassThin(p, P, L, 0); break;
			case BB_PULSE: bassPulse(p, P, L, 0, rng); break;
			case BB_DUB: bassDub(p, P, L, rng); break;
			default: bassOut(p, P, L, rng); break;
		}
		tile(p, T_BASS, L);
		return;
	}
	for (int s = 0; s < L; s++)
		noteOff(p.bass[s]);
	for (int s = 0; s < L; s++) {
		if (!isEventStart(p, s) && s != 0)
			continue;
		int e = s + 1;
		while (e < L && !isEventStart(p, e))
			e++;
		for (int q = s; q < e; q += 32) // a chord longer than two bars: the root again, no silent bar
			bassRoot(p, P, q, std::min(e - q, 32), q == s ? 84 : 76);
	}
	tile(p, T_BASS, L);
}

static void breakMelody(Pattern& p, const Params& P, int L) {
	freezeTrack(p, partP(P, T_MELODY, -0.15f, 0.12f), T_MELODY, L);
	for (int s = 0; s < L; s++)
		scaleNote(p.mel[s], 0.8f);
	tile(p, T_MELODY, L);
}

// ================================================================= OUTRO
// thins out bar by bar: toms/perc/open hat go first, then claps and ghosts, then the snare; the last bar is the kick
static void outroDrums(Pattern& p, const Params& P, int L) {
	// thinning out over the row: shapeDrums / shapeNote (Edit.cpp)
	freezeTrack(p, partP(P, T_DRUMS, -0.05f, 0.2f, 0.6f), T_DRUMS, L);
	tile(p, T_DRUMS, L);
}

static void outroBass(Pattern& p, const Params& P, int L) {
	// thinning out over the row: shapeDrums / shapeNote (Edit.cpp)
	freezeTrack(p, partP(P, T_BASS, -0.1f, 0.12f), T_BASS, L);
	tile(p, T_BASS, L);
}

static void outroMelody(Pattern& p, const Params& P, int L) {
	// thinning out over the row: shapeDrums / shapeNote (Edit.cpp)
	freezeTrack(p, partP(P, T_MELODY, -0.1f, 0.12f), T_MELODY, L);
	tile(p, T_MELODY, L);
}

// ================================================================= the parts
// density offsets of the generated tracks (drums, bass, melody, pads), the drums' ghost and fill offsets
struct Live {
	float dens[NUM_TRACKS];
	float ghost, fill;
};
static const Live LIVE[NUM_SECTIONS] = {
	/* INTRO */ {{0.f, 0.f, 0.f, 0.12f}, 0.f, -1.f},
	/* VERSE */ {{-0.06f, -0.05f, -0.2f, -0.08f}, 0.1f, -0.15f},
	/* BUILD */ {{0.f, 0.f, 0.f, 0.15f}, 0.f, -1.f},
	/* DROP  */ {{0.12f, 0.12f, 0.05f, -0.05f}, 0.08f, 0.f},
	/* BREAK */ {{0.f, 0.f, 0.f, 0.3f}, 0.f, -1.f},
	/* OUTRO */ {{0.f, 0.f, 0.f, 0.1f}, 0.f, -1.f},
};

// is the track baked into exact notes in this part?
static bool baked(int sec, int t) {
	if (t == T_PADS)
		return false; // pads stay generated: their voicing follows the chords, DENSITY sets their presence
	return sec == SEC_INTRO || sec == SEC_BUILD || sec == SEC_BREAK || sec == SEC_OUTRO;
}

void makeSection(const Slot& src, Slot& out, int sec, unsigned reroll, const Params& P0, Rng& rng, const PartOpts& opts) {
	sec = clampi(sec, 0, NUM_SECTIONS - 1);
	if (&out != &src)
		out = src;
	out.used = true;
	out.tag = (int8_t)sec;
	out.pat.partTag = (uint8_t)(sec + 1); // a part: rerolled tracks run its course over the song row
	out.pat.partMask = 0;
	int L = LENGTH_BARS[clampi(src.lengthIdx, 0, 3)] * STEPS_PER_BAR;
	Params P = P0;
	P.lengthBars = L / STEPS_PER_BAR;
	P.fillActive = false;
	P.ignoreChance = false;
	const Live& lv = LIVE[sec];
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (!(reroll & (1u << t)))
			continue; // kept exactly, its offsets too
		Pattern& p = out.pat;
		// the bass style of a break / an intro: fixed in the menu, or one at random on every press
		int bassStyle = 0;
		if (t == T_BASS && sec == SEC_BREAK)
			bassStyle = opts.breakBass >= 0 && opts.breakBass < NUM_BREAK_BASS ? opts.breakBass : rng.range(NUM_BREAK_BASS);
		if (t == T_BASS && sec == SEC_INTRO)
			bassStyle = opts.introBass >= 0 && opts.introBass < NUM_INTRO_BASS ? opts.introBass : rng.range(NUM_INTRO_BASS);
		bool thin = t == T_BASS && ((sec == SEC_BREAK && bassStyle == BB_THIN) || (sec == SEC_INTRO && bassStyle == IB_THIN));
		if (!thin) // a thinned line comes from the source's own bass, not a new one
			generateTrack(p, t, p.trackGenre[t], rng, P);
		if (baked(sec, t)) {
			out.mods.dens[t] = 0.f;
			p.partMask |= (uint8_t)(1u << t); // runs the part's course over the row (shapeDrums / shapeNote)
			switch (sec) {
				case SEC_INTRO: t == T_DRUMS ? introDrums(p, P, L) : (t == T_BASS ? introBass(p, P, L, bassStyle, rng) : introMelody(p, P, L)); break;
				case SEC_BUILD: t == T_DRUMS ? buildDrums(p, P, L) : (t == T_BASS ? buildBass(p, P, L) : buildMelody(p, P, L)); break;
				case SEC_BREAK: t == T_DRUMS ? breakDrums(p, P, L) : (t == T_BASS ? breakBass(p, P, L, bassStyle, rng) : breakMelody(p, P, L)); break;
				default: t == T_DRUMS ? outroDrums(p, P, L) : (t == T_BASS ? outroBass(p, P, L) : outroMelody(p, P, L)); break;
			}
		}
		else {
			out.mods.dens[t] = lv.dens[t];
			// a drop opens with a bass note on the one
			if (sec == SEC_DROP && t == T_BASS)
				p.bass[0].force = 1;
		}
		if (t == T_DRUMS) {
			out.mods.ghost = lv.ghost;
			out.mods.fill = lv.fill;
		}
	}
}

// ---------------------------------------------------------------- song forms (MAKE SONG)
#define I SEC_INTRO
#define V SEC_VERSE
#define B SEC_BUILD
#define D SEC_DROP
#define K SEC_BREAK
#define O SEC_OUTRO
const SongForm SONG_FORMS[NUM_SONG_FORMS] = {
	{"HOUSE · TECHNO", 8, {I, V, B, D, K, B, D, O}, {16, 16, 8, 32, 16, 8, 32, 16}},
	{"TRANCE · SYNTHWAVE", 8, {I, V, B, D, K, B, D, O}, {16, 16, 8, 16, 16, 16, 32, 16}},
	{"GARAGE", 9, {I, V, B, D, K, V, B, D, O}, {16, 16, 8, 16, 8, 16, 8, 16, 16}},
	{"BASS MUSIC", 7, {I, B, D, K, B, D, O}, {16, 8, 16, 16, 8, 16, 8}},
	{"JUNGLE · DNB", 7, {I, B, D, K, B, D, O}, {32, 8, 32, 16, 8, 32, 16}},
	{"HIP-HOP", 8, {I, V, D, V, D, K, D, O}, {4, 16, 8, 16, 8, 4, 8, 4}},
	{"AFRO · LATIN", 7, {I, V, D, V, K, D, O}, {8, 16, 16, 16, 8, 16, 8}},
	{"DUB · DOWNTEMPO · AMBIENT", 6, {I, V, K, V, K, O}, {8, 16, 16, 16, 16, 8}},
};
#undef I
#undef V
#undef B
#undef D
#undef K
#undef O

int songFormForGenre(const char* name) {
	if (!name)
		return SF_HOUSE;
	static const struct {
		const char* genre;
		int form;
	} MAP[] = {
	    {"House", SF_HOUSE}, {"Deep House", SF_HOUSE}, {"Tech House", SF_HOUSE}, {"Afro House", SF_HOUSE},
	    {"Techno", SF_HOUSE}, {"Minimal", SF_HOUSE}, {"Acid", SF_HOUSE}, {"Detroit Techno", SF_HOUSE},
	    {"Melodic Techno", SF_HOUSE}, {"Hard Techno", SF_HOUSE}, {"Dub Techno", SF_HOUSE},
	    {"Trance", SF_TRANCE}, {"Synthwave", SF_TRANCE}, {"Electro", SF_TRANCE},
	    {"UK Garage", SF_GARAGE}, {"2-Step", SF_GARAGE}, {"Speed Garage", SF_GARAGE},
	    {"Dubstep", SF_BASS}, {"Grime", SF_BASS}, {"Breakbeat", SF_BASS},
	    {"Jungle", SF_JUNGLE}, {"Drum & Bass", SF_JUNGLE}, {"Footwork", SF_JUNGLE}, {"Breakcore", SF_JUNGLE},
	    {"Hip-Hop", SF_HIPHOP}, {"Lo-Fi", SF_HIPHOP}, {"Trap", SF_HIPHOP}, {"Drill", SF_HIPHOP}, {"Trip-Hop", SF_HIPHOP},
	    {"Afrobeat", SF_AFRO}, {"Afrobeats", SF_AFRO}, {"Amapiano", SF_AFRO}, {"Baile Funk", SF_AFRO}, {"Reggaeton", SF_AFRO},
	    {"Dub", SF_DUB}, {"Downtempo", SF_DUB}, {"Ambient", SF_DUB},
	    // v2.24
	    {"Progressive House", SF_HOUSE}, {"Organic House", SF_HOUSE}, {"Hardcore", SF_HOUSE}, {"EBM", SF_HOUSE},
	    {"Psytrance", SF_TRANCE},
	    {"UK Funky", SF_GARAGE}, {"Bassline", SF_GARAGE},
	    {"Big Beat", SF_BASS}, {"Breaks", SF_BASS}, {"Jersey Club", SF_BASS}, {"Baltimore Club", SF_BASS},
	    {"Phonk", SF_HIPHOP},
	    {"Gqom", SF_AFRO}, {"Dancehall", SF_AFRO}, {"Moombahton", SF_AFRO}, {"Cumbia", SF_AFRO}, {"Bossa Nova", SF_AFRO},
	    {"Roots Reggae", SF_DUB}, {"Drone", SF_DUB}, {"Dark Ambient", SF_DUB}, {"Ambient Techno", SF_DUB}, {"Berlin School", SF_DUB},
	};
	for (const auto& m : MAP)
		if (std::strcmp(m.genre, name) == 0)
			return m.form;
	return SF_HOUSE;
}

int songFormBars(int form) {
	if (form < 0 || form >= NUM_SONG_FORMS)
		return 0;
	int n = 0;
	for (int k = 0; k < SONG_FORMS[form].n; k++)
		n += SONG_FORMS[form].bars[k];
	return n;
}

} // namespace genra
