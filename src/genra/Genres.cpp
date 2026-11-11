// GENRA v2 – genre profiles (see "GENRA v2 – Genre-Spezifikation").
// Grids: one char per 16th, '|' separates beats and is ignored.
//   'X' always (anchor) · 'x' mostly (75 %) · 'o' sometimes (35 %) · 'r' roll/triplet position (75 %)
//   '1'..'9' probability n/9 · '.' never. Drum grids may be 16 or 32 steps (2 bars).
#include "Engine.hpp"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <initializer_list>
#include <vector>
#include <algorithm>
#include <cstring>
#include "../GenreMatch.hpp"

namespace genra {

const char* DRUM_NAMES[NUM_DRUMS] = {"Kick", "Snare", "Clap", "Rim", "Closed hat", "Open hat", "Tom", "Perc"};
const char* NOTE_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

const Scale SCALES[NUM_SCALES] = {
	{"Major", {0, 2, 4, 5, 7, 9, 11}, 0x7f},
	{"Minor", {0, 2, 3, 5, 7, 8, 10}, 0x7f},
	{"Dorian", {0, 2, 3, 5, 7, 9, 10}, 0x7f},
	{"Phrygian", {0, 1, 3, 5, 7, 8, 10}, 0x7f},
	{"Lydian", {0, 2, 4, 6, 7, 9, 11}, 0x7f},
	{"Mixolydian", {0, 2, 4, 5, 7, 9, 10}, 0x7f},
	{"Harm. Minor", {0, 2, 3, 5, 7, 8, 11}, 0x7f},
	{"Major Penta", {0, 2, 4, 5, 7, 9, 11}, 0x37},
	{"Minor Penta", {0, 2, 3, 5, 7, 8, 10}, 0x5d},
	// 2.30 (appended: the SCALE knob keeps its old positions)
	{"Locrian", {0, 1, 3, 5, 6, 8, 10}, 0x7f},
	{"Phryg. Dom.", {0, 1, 4, 5, 7, 8, 10}, 0x7f},
	{"Dbl. Harmonic", {0, 1, 4, 5, 7, 8, 11}, 0x7f},
};
enum { S_MAJOR, S_MINOR, S_DORIAN, S_PHRYG, S_LYDIAN, S_MIXO, S_HARM, S_MAJPENTA, S_MINPENTA, S_LOCRIAN, S_PHRYGDOM, S_DBLHARM };

static std::vector<Genre> PROFILES;
static std::vector<int> VARIANTS[NUM_GENRES];
static std::vector<int> PROFILE_GENRE;

// ----------------------------------------------------------------- builders
static void tpl(Genre& g, std::initializer_list<const char*> l) {
	int i = 0;
	for (const char* t : l)
		if (i < 8)
			g.templates[i++] = t;
	for (; i < 8; i++)
		g.templates[i] = nullptr;
}
static void drums(Genre& g, std::initializer_list<const char*> l) {
	int i = 0;
	for (const char* t : l)
		if (i < NUM_DRUMS)
			g.drum[i++] = t;
}
static void degw(Genre& g, std::initializer_list<float> l) {
	int i = 0;
	for (float f : l)
		if (i < 7)
			g.degW[i++] = f;
}
static void minorW(Genre& g) { degw(g, {6, 0.3f, 1.0f, 1.5f, 0.8f, 1.6f, 1.6f}); }
static void majorW(Genre& g) { degw(g, {6, 1.2f, 0.6f, 1.6f, 1.4f, 1.6f, 0.2f}); }
static void harm(Genre& g, int a, float wa, int b, float wb, int c, float wc) {
	g.harm[0] = a;
	g.harm[1] = b;
	g.harm[2] = c;
	g.harmW[0] = wa;
	g.harmW[1] = wb;
	g.harmW[2] = wc;
}
static void add(int genre, const Genre& g) {
	if ((int)PROFILES.size() >= MAX_PROFILES || (int)VARIANTS[genre].size() >= MAX_VARIANTS)
		return;
	VARIANTS[genre].push_back((int)PROFILES.size());
	PROFILE_GENRE.push_back(genre);
	PROFILES.push_back(g);
}

// parse a grid into probabilities; returns its length (16 or 32)
static int parseGrid(const char* s, float* out, bool* roll, int maxLen) {
	int n = 0;
	for (int i = 0; i < maxLen; i++) {
		out[i] = 0.f;
		if (roll)
			roll[i] = false;
	}
	if (!s)
		return 16;
	for (const char* c = s; *c && n < maxLen; c++) {
		float p = -1.f;
		bool r = false;
		switch (*c) {
			case 'X': p = 1.f; break;
			case 'x': p = 0.75f; break;
			case 'o': p = 0.35f; break;
			case 'r': p = 0.75f; r = true; break;
			case '.': p = 0.f; break;
			default:
				if (*c >= '1' && *c <= '9')
					p = (*c - '0') / 9.f;
				break; // '|' and anything else: separator
		}
		if (p < 0.f)
			continue;
		out[n] = p;
		if (roll)
			roll[n] = r;
		n++;
	}
	return n > 16 ? 32 : 16;
}


// ================================================== 2.30: each genre's habits (chord moves, accents, song parts)
// After DARK-1 (Polystellarflow, GPL-3.0-only): its chord habits, step tricks and sections per genre, on GENRA's
// genres. Degrees in a minor key: 0 i, 1 ii(o) / bII, 2 III, 3 iv, 4 v / V, 5 VI, 6 VII.
static void habit(Genre& g, int cadTo, bool noBack, bool raiseV, std::initializer_list<Habit> l) {
	g.nHabit = 0;
	for (const Habit& h : l)
		if (g.nHabit < 8)
			g.habit[g.nHabit++] = h;
	g.cadTo = cadTo;
	g.noBack = noBack;
	g.raiseV = raiseV;
}
static bool named(const Genre& g, std::initializer_list<const char*> l) {
	for (const char* n : l)
		if (std::strcmp(g.name, n) == 0)
			return true;
	return false;
}
static void applyFeel() {
	for (Genre& g : PROFILES) {
		// ---- chord habits
		if (named(g, {"House", "Deep House", "Tech House", "Afro House", "Organic House", "UK Garage", "2-Step",
		              "Speed Garage", "UK Funky", "Bassline", "Jersey Club", "Baltimore Club", "Gqom"}))
			habit(g, 6, false, false, {{0, 3, 4}, {3, 0, 4}, {0, 5, 3}, {5, 6, 4}, {6, 0, 4}});
		else if (named(g, {"Trance", "Psytrance", "Melodic Techno", "Progressive House"}))
			habit(g, 6, true, false, {{0, 5, 9}, {5, 2, 9}, {2, 6, 9}, {6, 0, 9}});
		else if (named(g, {"Techno", "Minimal", "Acid", "Detroit Techno", "Hard Techno", "Dub Techno", "EBM", "Hardcore"}))
			habit(g, 5, false, false, {{0, 5, 3}, {5, 0, 5}, {0, 6, 3}, {6, 0, 5}, {0, 3, 2}});
		else if (named(g, {"Drum & Bass", "Jungle", "Breakcore", "Breakbeat", "Breaks", "Big Beat", "Footwork", "Grime"}))
			habit(g, 4, false, true, {{0, 5, 8}, {5, 6, 6}, {6, 0, 8}, {0, 3, 5}, {5, 4, 3}});
		else if (named(g, {"Dubstep"}))
			habit(g, 5, false, false, {{0, 5, 6}, {5, 0, 8}, {0, 3, 3}, {3, 0, 4}});
		else if (named(g, {"Hip-Hop", "Lo-Fi", "Trip-Hop", "Downtempo", "Phonk"}))
			habit(g, 6, true, false, {{0, 3, 6}, {3, 6, 6}, {6, 2, 6}, {2, 5, 4}, {5, 0, 4}});
		else if (named(g, {"Trap", "Drill"}))
			habit(g, named(g, {"Drill"}) ? 4 : 6, false, named(g, {"Drill"}), {{0, 5, 6}, {5, 0, 6}, {0, 6, 3}, {6, 0, 4}});
		else if (named(g, {"Afrobeat", "Afrobeats", "Amapiano", "Reggaeton", "Dancehall", "Moombahton", "Baile Funk"}))
			habit(g, 6, false, false, {{0, 5, 6}, {5, 6, 6}, {6, 0, 6}, {0, 3, 4}, {3, 0, 4}});
		else if (named(g, {"Cumbia"}))
			habit(g, 4, false, true, {{0, 3, 5}, {3, 4, 5}, {4, 0, 8}, {0, 6, 3}});
		else if (named(g, {"Dub", "Roots Reggae"}))
			habit(g, 6, false, false, {{0, 6, 6}, {6, 0, 6}, {0, 3, 4}, {3, 0, 4}});
		else if (named(g, {"Synthwave"}))
			habit(g, 4, true, true, {{0, 6, 9}, {6, 5, 9}, {5, 4, 9}, {4, 0, 9}, {0, 5, 3}});
		else if (named(g, {"Electro"}))
			habit(g, 6, false, false, {{0, 3, 6}, {3, 0, 6}, {0, 6, 5}, {6, 0, 5}});
		else if (named(g, {"Ambient", "Drone", "Dark Ambient", "Ambient Techno", "Berlin School"}))
			habit(g, -1, false, false, {{0, 5, 6}, {5, 0, 6}, {0, 3, 4}, {3, 0, 4}});
		else if (named(g, {"Witch House"}))
			habit(g, 5, false, false, {{0, 5, 6}, {5, 0, 6}, {0, 3, 3}, {3, 0, 4}});
		else if (named(g, {"Darkwave"}))
			habit(g, 6, false, false, {{0, 6, 9}, {6, 5, 8}, {5, 6, 8}, {6, 0, 9}, {0, 3, 4}, {3, 0, 5}});
		else if (named(g, {"Dungeon Synth"}))
			habit(g, 4, false, true, {{0, 5, 7}, {5, 2, 6}, {2, 6, 6}, {6, 0, 8}, {0, 3, 5}, {3, 0, 5}, {5, 4, 3}, {4, 0, 4}});
		else if (named(g, {"Industrial"}))
			habit(g, 1, false, false, {{0, 1, 6}, {1, 0, 6}, {0, 6, 4}, {6, 0, 4}, {0, 3, 2}});
		else if (named(g, {"Horror"}))
			habit(g, 1, false, false, {{0, 1, 6}, {1, 0, 6}, {0, 4, 4}, {4, 0, 6}, {0, 5, 3}, {5, 4, 4}});

		// ---- accents
		if (named(g, {"House", "Deep House", "Afro House", "Organic House", "Progressive House", "Speed Garage", "Trance", "Psytrance"}))
			g.accents = AC_OFFHAT | AC_ONE;
		else if (named(g, {"Tech House", "Techno", "Detroit Techno", "Melodic Techno", "Acid", "Dub Techno"}))
			g.accents = AC_ONE | AC_SOFT_OFFHAT;
		else if (named(g, {"Minimal"}))
			g.accents = AC_SOFT_OFFHAT | AC_GHOST_HATS;
		else if (named(g, {"Hard Techno", "Hardcore", "EBM", "Industrial"}))
			g.accents = AC_ONE | AC_BACKBEAT;
		else if (named(g, {"Drum & Bass", "Jungle", "Breakcore", "UK Garage", "2-Step", "Footwork"}))
			g.accents = AC_BACKBEAT | AC_GHOST_HATS;
		else if (named(g, {"Breakbeat", "Breaks", "Big Beat", "Grime", "Bassline", "UK Funky", "Electro", "Synthwave", "Darkwave",
		                   "Trap", "Drill", "Phonk", "Reggaeton", "Dancehall", "Moombahton", "Jersey Club", "Baltimore Club"}))
			g.accents = AC_BACKBEAT;
		else if (named(g, {"Hip-Hop", "Lo-Fi", "Trip-Hop", "Downtempo"}))
			g.accents = AC_BACKBEAT | AC_LATE_SNARE | (named(g, {"Lo-Fi"}) ? AC_GHOST_HATS : 0);
		else if (named(g, {"Dub", "Roots Reggae", "Witch House"}))
			g.accents = AC_LATE_SNARE | AC_SOFT_OFFHAT;
		else if (named(g, {"Afrobeat", "Afrobeats", "Amapiano", "Gqom", "Baile Funk", "Cumbia", "Bossa Nova", "Horror", "Dungeon Synth"}))
			g.accents = AC_FLAM_PERC | (named(g, {"Horror", "Dungeon Synth"}) ? 0 : AC_SOFT_OFFHAT);

		// ---- song parts: what a break keeps, the kick before the drop, the pads' rise, a build's bars
		if (named(g, {"Techno", "Minimal", "Acid", "Detroit Techno", "Hard Techno", "Dub Techno", "EBM", "Tech House"}))
			g.breakKeep = 1;
		else if (named(g, {"Industrial"}))
			g.breakKeep = 3;
		else if (named(g, {"Drum & Bass", "Jungle", "Breakcore", "Footwork"}))
			g.breakKeep = 2;
		if (named(g, {"Dubstep", "Trap", "Drill", "Phonk", "Hip-Hop", "Drum & Bass", "Jungle", "Grime", "Big Beat", "Breakbeat",
		              "Witch House", "Horror"}))
			g.buildKick = false;
		if (named(g, {"Trance", "Melodic Techno", "Ambient Techno"}))
			g.padRise = 0.45f;
		else if (named(g, {"Psytrance", "Progressive House", "Synthwave", "Darkwave", "Witch House"}))
			g.padRise = 0.4f;
		else if (named(g, {"Techno", "Hard Techno", "EBM", "Hardcore", "Industrial"}))
			g.padRise = 0.12f;
		else if (named(g, {"Dungeon Synth", "Horror"}))
			g.padRise = 0.2f;
		if (named(g, {"Trance", "Psytrance", "Melodic Techno", "Progressive House"}))
			g.buildBars = 16;
		else if (named(g, {"UK Garage", "2-Step", "Speed Garage", "Bassline", "Grime", "Jersey Club", "Baltimore Club", "Footwork"}))
			g.buildBars = 4;
		else if (named(g, {"Dubstep", "Drum & Bass", "Jungle"}))
			g.buildBars = 8;
	}
}

// ----------------------------------------------------------------- the genres
static void defineGenres() {
	// ================================================== HOUSE FAMILY
	{ // 0 HOUSE
		Genre g;
		g.name = "House"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 124; g.swing = 0.54f; g.complexity = 0.4f; g.color = 0.6f; g.rolls = 0.1f;
		harm(g, 16, 0.5f, 32, 0.4f, 8, 0.1f);
		tpl(g, {"i7 iv7", "i7 bVImaj7 bVII", "iv7 v7 i7", "bVImaj7 bVII7 i7", "i7 bIIImaj7 bVImaj7 bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "....|....|....|o...", "....|X...|....|X...", "....|..o.|....|o..o",
		          "x.o.|x.o.|x.o.|x.o.", "..X.|..X.|..X.|..X.", "................", ".o.x|.o..|.o.x|..o."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.45f; g.bass = "..X...X...X..xX.";
		g.melStyle = MS_LEAD; g.mel = "x..o..x...o.x...";
		g.padStyle = PS_STABS; g.pad = "...x..o....x..o.";
		add(0, g);
		g.variant = "Piano"; g.pad = "..x.x.o...x.x.o."; g.color = 0.7f;
		add(0, g);
	}
	{ // 1 DEEP HOUSE
		Genre g;
		g.name = "Deep House"; g.variant = "Stabs";
		g.scale = S_MINOR; g.bpm = 120; g.swing = 0.6f; g.complexity = 0.4f; g.color = 0.85f;
		harm(g, 16, 0.5f, 32, 0.5f, 8, 0.0f);
		tpl(g, {"i9 iv9", "i7 bVImaj7 bVII", "i9 v7", "i7 v7 iv7", "i7 iv7"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "....|....|....|....", "....|x...|....|x...", "...o|..o.|...o|..o.",
		          "..o.|..o.|..o.|..o.", "..x.|..x.|..x.|..x.", "....|....|....|o...", "xoxo|xoxo|xoxo|xoxo"});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.5f; g.bass = "X..x..x.X..o.x..";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "o.....o...o.....";
		g.padStyle = PS_STABS; g.pad = "..X.............";
		add(1, g);
		g.variant = "Pads"; g.padStyle = PS_SUSTAIN; g.pad = "X...............";
		add(1, g);
	}
	{ // 2 SPEED GARAGE
		Genre g;
		g.name = "Speed Garage"; g.variant = "Warp";
		g.scale = S_MINOR; g.bpm = 132; g.swing = 0.6f; g.complexity = 0.5f; g.color = 0.25f; g.rolls = 0.15f;
		harm(g, 32, 0.5f, 16, 0.3f, 64, 0.2f);
		tpl(g, {"i", "i bVI", "i bVII", "i iv"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "....|X..o|....|X.o.", "....|x...|....|x...", "..o.|...o|..o.|.o.o",
		          "x.x.|x.xx|x.x.|x.xx", "..X.|..X.|..X.|..X.", "................", ".o..|..o.|.o..|o..."});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.55f; g.slide = 0.3f; g.bass = "X..x..x.X.x..x.o";
		g.melStyle = MS_RIFF; g.mel = "x..x..x...x..x..";
		g.padStyle = PS_STABS; g.pad = "..x...o...x...o.";
		g.fillStyle = FS_DROP; g.dropMode = DM_KICKBASS;
		add(2, g);
		g.variant = "Organ"; g.bassLen = 0; g.slide = 0.05f; g.bass = "x.x..x.xx.x..x.x"; g.pad = "x.x...x.x.x...x.";
		add(2, g);
	}
	{ // 3 UK GARAGE (4x4)
		Genre g;
		g.name = "UK Garage"; g.variant = "4x4";
		g.scale = S_MINOR; g.bpm = 132; g.swing = 0.64f; g.complexity = 0.55f; g.color = 0.75f; g.rolls = 0.15f;
		harm(g, 16, 0.5f, 32, 0.4f, 8, 0.1f);
		tpl(g, {"i9 bVImaj7 bIIImaj7 bVII", "i7 iv7 bVII7 bIIImaj7", "iv9 v7 i9", "i7 bVImaj7", "i9 bVII bVImaj7 v7"});
		minorW(g);
		drums(g, {"X...|X...|X...|X..o", "....|X...|....|X...", "....|x...|....|x...", "..o.|..x.|..o.|.x.o",
		          "x.xo|x.xo|x.xo|x.xo", "..x.|..x.|..x.|..x.", "................", "...o|....|..o.|...o"});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.55f; g.slide = 0.05f; g.bass = "X..o..x...x.o...";
		g.melStyle = MS_LEAD; g.mel = "x..o.x...o..x.o.";
		g.padStyle = PS_STABS; g.pad = "..x...o..x...o..";
		add(3, g);
	}
	{ // 4 2-STEP
		Genre g;
		g.name = "2-Step"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 134; g.swing = 0.67f; g.complexity = 0.65f; g.color = 0.8f; g.rolls = 0.15f;
		harm(g, 16, 0.5f, 32, 0.4f, 8, 0.1f);
		tpl(g, {"i9 bVImaj7 bIIImaj7 bVII", "i7 iv9 bVII7 bIIImaj7", "iv9 bVII7 i9", "i9 v7 bVImaj7 bVII"});
		minorW(g);
		drums(g, {"X...|...o|..X.|...o", "....|X...|....|X...", "....|o...|....|o...", "...o|..o.|.o..|..o.",
		          "x.xx|x.x.|x.xx|x.x.", "..x.|..x.|....|..x.", "....|....|....|..o.", "...o|....|..o.|...."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.6f; g.bass = "X.....X.X.......";
		g.melStyle = MS_LEAD; g.mel = "x..o..x..o..x...";
		g.padStyle = PS_STABS; g.pad = "...x......x..o..";
		add(4, g);
	}
	// ================================================== TECHNO FAMILY
	{ // 5 TECHNO
		Genre g;
		g.name = "Techno"; g.variant = "Rolling";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.52f; g.complexity = 0.35f; g.color = 0.2f; g.rolls = 0.25f;
		harm(g, 64, 0.4f, 32, 0.4f, 16, 0.2f);
		tpl(g, {"i", "i bVI", "i bII", "i iv", "i i bVI bVII"});
		degw(g, {6, 0.2f, 0.6f, 1.6f, 0.8f, 1.6f, 1.4f});
		drums(g, {"X...|X...|X...|X...", "................", "....|x...|....|x...", "...o|..o.|...o|.o..",
		          "x.xx|x.xx|x.xx|x.xx", "..X.|..X.|..X.|..X.", "....|....|....|..o.", ".o..|..o.|.o..|o..o"});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.2f; g.bass = ".xxx.xxx.xxx.xxx";
		g.melStyle = MS_RIFF; g.mel = "x..o..x..o..x...";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.7f;
		add(5, g);
		g.variant = "Peak"; g.bass = ".x.x.x.x.x.x.x.x"; g.complexity = 0.45f;
		g.drum[2] = "....|X...|....|X..."; g.drum[7] = "x.x.|x.x.|x.x.|x.x.";
		add(5, g);
		g.variant = "Hypnotic"; g.bass = "..x...x...x...o."; g.complexity = 0.3f;
		g.drum[2] = "....|o...|....|x..."; g.drum[4] = "..x.|..x.|..x.|..x."; g.drum[7] = "o.xo|.ox.|o.xo|.o.x";
		g.mel = "x.......o......."; g.padBase = 0.5f;
		add(5, g);
	}
	{ // 6 MINIMAL
		Genre g;
		g.name = "Minimal"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 127; g.swing = 0.54f; g.complexity = 0.4f; g.color = 0.3f; g.rolls = 0.05f;
		harm(g, 64, 0.7f, 32, 0.3f, 64, 0.0f);
		tpl(g, {"i7", "i", "i bII"});
		degw(g, {6, 0.6f, 0.4f, 0.8f, 0.6f, 0.6f, 0.8f});
		drums(g, {"X...|X...|X...|X...", "................", "....|o...|....|x...", "..o.|...o|.o..|..o.",
		          "..x.|..x.|..x.|..x.", "....|....|....|..o.", "................", "o..o|.o..|..o.|.o.o"});
		g.bassKick = -1; g.bassLen = 1; g.bassMove = 0.25f; g.bass = "..x.....o.x.....";
		g.melStyle = MS_SPARSE; g.mel = "........o.......";
		g.padStyle = PS_STABS; g.padBase = 0.6f; g.pad = "..........o.....";
		g.fillStyle = FS_DROP;
		add(6, g);
	}
	{ // 7 ACID
		Genre g;
		g.name = "Acid"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.53f; g.complexity = 0.45f; g.color = 0.15f; g.rolls = 0.1f;
		harm(g, 64, 0.6f, 32, 0.4f, 64, 0.0f);
		tpl(g, {"i", "i bVII", "i bII"});
		degw(g, {6, 0.3f, 0.3f, 0.6f, 0.4f, 0.6f, 1.2f});
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "....|...o|....|..o.",
		          "x.x.|x.x.|x.x.|x.x.", "..X.|..X.|..X.|..X.", "................", "....|..o.|....|o..."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.6f; g.slide = 0.25f; g.bass = "xxox|xoxx|xxox|oxxo";
		g.melStyle = MS_ACID; g.melSlide = 0.2f; g.mel = "o..o|.o..|o..o|..o.";
		g.padStyle = PS_STABS; g.padBase = 0.4f; g.pad = "........o.......";
		add(7, g);
		g.variant = "Double"; g.mel = "x.xx|xoxx|x.xx|oxxo"; g.melSlide = 0.25f;
		add(7, g);
		g.variant = "Poly 12"; g.bassCycle = 12; g.mel = "o..o|.o..|o..o|..o."; g.melSlide = 0.2f;
		add(7, g);
	}
	{ // 8 ELECTRO
		Genre g;
		g.name = "Electro"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 128; g.swing = 0.52f; g.complexity = 0.5f; g.color = 0.35f;
		harm(g, 32, 0.5f, 16, 0.4f, 64, 0.1f);
		tpl(g, {"i iv", "i bVI bVII", "i V", "i bVI iv V"});
		minorW(g);
		drums(g, {"X..x|..x.|X...|..xo", "....|X...|....|X...", "....|x...|....|x...", "................",
		          "x.x.|x.x.|x.x.|x.x.", "....|....|....|..o.", "....|....|....|.o.o", "..x.|..x.|..x.|..x."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.6f; g.bass = "..x..x.x..x.x..x";
		g.melStyle = MS_RIFF; g.mel = "x.x.x..x.x..x.x.";
		g.padStyle = PS_STABS; g.pad = "..x.....o...x...";
		g.fillStyle = FS_TOMS;
		add(8, g);
		g.variant = "Funk"; g.bpm = 116; g.swing = 0.57f; g.color = 0.45f;
		add(8, g);
		g.variant = "Detroit"; g.bpm = 133; g.swing = 0.5f; g.complexity = 0.6f; g.color = 0.25f;
		g.drum[4] = "xxxx|xxxx|xxxx|xxxx"; g.mel = "x.xx.x.xx.xx.x.x";
		add(8, g);
	}
	{ // 9 TRANCE
		Genre g;
		g.name = "Trance"; g.variant = "Uplifting";
		g.scale = S_MINOR; g.bpm = 138; g.swing = 0.5f; g.complexity = 0.3f; g.color = 0.3f;
		harm(g, 32, 0.6f, 16, 0.3f, 64, 0.1f);
		tpl(g, {"i bVI bIII bVII", "i bVII bVI bVII", "bVI bVII i i", "i iv bVI bVII", "i bVI iv bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "................", "....|x...|....|x...", "................",
		          "o.o.|o.o.|o.o.|o.o.", "..X.|..X.|..X.|..X.", "................", "...o|...o|...o|...o"});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.2f; g.bass = ".xxx.xxx.xxx.xxx";
		g.melStyle = MS_ARP; g.mel = "XXXXXXXXXXXXXXXX";
		g.padStyle = PS_SUSTAIN;
		add(9, g);
		g.variant = "Psy"; g.bpm = 142; g.bass = ".XXX.XXX.XXX.XXX"; g.color = 0.1f;
		tpl(g, {"i", "i bII", "i i bVII i"});
		harm(g, 64, 0.8f, 32, 0.2f, 64, 0.0f);
		g.padBase = 0.5f;
		add(9, g);
	}
	// ================================================== BREAKS & BASS
	{ // 10 BREAKBEAT
		Genre g;
		g.name = "Breakbeat"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.54f; g.complexity = 0.6f; g.color = 0.4f; g.rolls = 0.2f;
		harm(g, 16, 0.5f, 32, 0.4f, 8, 0.1f);
		tpl(g, {"i bVII", "i iv bVII", "i bVI bVII", "i iv", "i bIII bVII i"});
		minorW(g);
		drums(g, {"X...|....|..X.|.x..", "....|X...|....|X...", "................", "....|...o|....|...o",
		          "x.x.|x.x.|x.x.|x.x.", "....|....|..o.|....", "....|....|....|..o.", "...o|....|..o.|...."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.5f; g.bass = "X..o....X.o...o.";
		g.melStyle = MS_RIFF; g.mel = "x..o..x.....o...";
		g.padStyle = PS_STABS; g.pad = "..x.......x.....";
		g.fillStyle = FS_CHOP; g.chop = 0.15f;
		add(10, g);
		g.variant = "Nu-Skool"; g.bpm = 133; g.bassMove = 0.65f; g.slide = 0.1f; g.bass = "X..x..x.X.x..x..";
		add(10, g);
		g.variant = "Big Beat"; g.bpm = 110; g.complexity = 0.5f; g.slide = 0.0f; g.bass = "X.......X.x.....";
		add(10, g);
	}
	{ // 11 JUNGLE
		Genre g;
		g.name = "Jungle"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 168; g.swing = 0.54f; g.complexity = 0.6f; g.color = 0.35f; g.rolls = 0.2f;
		harm(g, 64, 0.5f, 128, 0.3f, 32, 0.2f);
		tpl(g, {"i", "i bVI", "i bII", "i iv"});
		degw(g, {6, 0.4f, 0.6f, 1.2f, 0.6f, 1.4f, 1.0f});
		drums(g, {"X.x.|....|..xx|....", "....|X..o|.o..|X..o", "................", "..o.|.o..|o...|.o.o",
		          "x.x.|x.x.|x.x.|x.x.", "....|....|....|..o.", "................", "....|..o.|....|...."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.35f; g.bass = "X.......o.....x.";
		g.melStyle = MS_SPARSE; g.mel = "x.......o.......";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.8f;
		g.fillStyle = FS_CHOP; g.chop = 0.6f; g.dropMode = DM_BREAK;
		add(11, g);
		g.variant = "Ragga"; g.bassLen = 1; g.bassMove = 0.6f; g.bass = "X..x..x.X..o.x..";
		g.padStyle = PS_SKANK; g.pad = "....X.......X...";
		add(11, g);
		g.variant = "Dark"; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.3f; g.bass = "X.......x...o...";
		g.padStyle = PS_SUSTAIN; g.color = 0.2f; g.scale = S_PHRYG;
		add(11, g);
	}
	{ // 12 DRUM & BASS
		Genre g;
		g.name = "Drum & Bass"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 174; g.swing = 0.5f; g.complexity = 0.6f; g.color = 0.5f; g.rolls = 0.35f;
		harm(g, 32, 0.4f, 64, 0.3f, 16, 0.3f);
		tpl(g, {"i bVI", "i bVII bVI", "i7 iv7", "i bIII bVII iv", "i9 bVImaj7"});
		minorW(g);
		drums(g, {"X...|....|..X.|....", "....|X...|....|X...", "................", "..o.|...o|..o.|...o",
		          "x.x.|x.x.|x.x.|x.x.", "..o.|....|..o.|....", "................", "....|...o|....|...o"});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.5f; g.bass = "X..x..X...x..x..";
		g.melStyle = MS_SPARSE; g.mel = "x.......o...x...";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_CHOP; g.chop = 0.25f;
		add(12, g);
		g.variant = "Liquid"; g.color = 0.85f; g.bassMove = 0.65f; g.chop = 0.1f; g.bassLen = 2;
		tpl(g, {"i9 bVImaj7", "i9 iv9 bVII7 bIIImaj7", "i7 iv7", "bVImaj7 v7 i9"});
		g.melStyle = MS_LEAD; g.melPenta = true;
		add(12, g);
		g.variant = "Neuro"; g.color = 0.1f; g.bassMove = 0.4f; g.slide = 0.35f; g.chop = 0.3f; g.bassLen = 2;
		tpl(g, {"i", "i bII", "i bVI"});
		g.melStyle = MS_SPARSE; g.melPenta = false; g.padBase = 0.5f; g.scale = S_PHRYG;
		add(12, g);
	}
	{ // 13 DUBSTEP
		Genre g;
		g.name = "Dubstep"; g.variant = "Deep";
		g.scale = S_MINOR; g.bpm = 140; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.3f; g.rolls = 0.35f;
		harm(g, 32, 0.4f, 64, 0.4f, 16, 0.2f);
		tpl(g, {"i", "i bII", "i bVI", "i bVII bVI", "i iv"});
		degw(g, {6, 0.2f, 0.6f, 1.4f, 0.6f, 1.6f, 1.2f});
		drums(g, {"X...|....|..o.|....", "....|....|X...|....", "....|....|x...|....", "....|....|..o.|..o.",
		          "x.r.|x.r.|x.r.|x.r.", "....|....|....|..o.", "....|....|....|.o.o", "....|..o.|....|.o.."});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.35f; g.slide = 0.1f; g.bass = "X.....o...x.....";
		g.melStyle = MS_SPARSE; g.mel = "x.......o.....o.";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_TOMS;
		add(13, g);
		g.variant = "Heavy"; g.complexity = 0.6f; g.rolls = 0.6f;
		g.drum[4] = "x.r.|xrr.|x.r.|xrrr"; g.bass = "X..x..o.x.x..o..";
		add(13, g);
	}
	{ // 14 GRIME
		Genre g;
		g.name = "Grime"; g.variant = "Eski";
		g.scale = S_MINOR; g.bpm = 140; g.swing = 0.53f; g.complexity = 0.5f; g.color = 0.1f; g.rolls = 0.2f;
		harm(g, 64, 0.6f, 32, 0.4f, 64, 0.0f);
		tpl(g, {"i", "i bII", "i i bVI i"});
		degw(g, {6, 0.6f, 0.3f, 0.6f, 0.4f, 0.6f, 0.6f});
		drums(g, {"X...|..o.|.x..|..o.", "....|X...|....|X...", "....|x...|....|x...", "...o|....|..o.|....",
		          "..x.|..x.|..x.|..x.", "................", "................", "....|.o..|....|..o."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.3f; g.bass = "X..x...xX..x....";
		g.melStyle = MS_RIFF; g.mel = "x..x....x..x....";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.f;
		g.fillStyle = FS_DROP; g.dropMode = DM_EIGHT;
		add(14, g);
		g.variant = "Sino"; g.melPenta = true; g.mel = "x..x..x.x..x..o.";
		add(14, g);
	}
	// ================================================== HIP-HOP & DOWNTEMPO
	{ // 15 HIP-HOP
		Genre g;
		g.name = "Hip-Hop"; g.variant = "Boom Bap";
		g.scale = S_MINOR; g.bpm = 90; g.swing = 0.58f; g.complexity = 0.5f; g.color = 0.7f; g.jazzy = true; g.rolls = 0.1f;
		harm(g, 16, 0.4f, 32, 0.5f, 8, 0.1f);
		tpl(g, {"i7 iv7", "i9 iv9", "bVImaj7 v7", "i7 bVII7 bVImaj7 v7", "iv7 bVII7 bIIImaj7 bVImaj7", "i7 i7 iv7 v7"});
		minorW(g);
		drums(g, {"X...|....|X.x.|....", "....|X...|....|X...", "....|o...|....|o...", "....|..o.|....|o...",
		          "x.x.|x.x.|x.x.|x.x.", "....|....|....|..o.", "................", "....|...o|....|..o."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.6f; g.bass = "X.....o.X.x.....";
		g.melStyle = MS_LEAD; g.melPenta = true; g.mel = "x..o...x.o....o.";
		g.padStyle = PS_SUSTAIN;
		g.humanize = 0.3f;
		add(15, g);
	}
	{ // 16 LO-FI
		Genre g;
		g.name = "Lo-Fi"; g.variant = "Jazzy";
		g.scale = S_MAJOR; g.bpm = 80; g.swing = 0.62f; g.complexity = 0.45f; g.color = 0.9f; g.jazzy = true; g.rolls = 0.05f;
		harm(g, 16, 0.45f, 32, 0.45f, 8, 0.1f);
		tpl(g, {"IVmaj7 iii7 vi7", "ii7 V7 Imaj7", "Imaj7 vi7 ii7 V7", "IVmaj7 V7 iii7 vi7", "Imaj7 IVmaj7", "ii9 V9 Imaj9 vi7"});
		majorW(g);
		drums(g, {"X...|...o|..X.|....", "....|X...|....|X..o", "....|o...|....|o...", "..o.|...o|..o.|..o.",
		          "x.o.|x.o.|x.o.|x.o.", "....|....|....|..o.", "................", "...o|....|..o.|...."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.55f; g.bass = "X.....o...X..o..";
		g.melStyle = MS_LEAD; g.melPenta = true; g.mel = "x...o..x...o..o.";
		g.padStyle = PS_SUSTAIN;
		g.humanize = 0.8f;
		add(16, g);
	}
	{ // 17 TRAP
		Genre g;
		g.name = "Trap"; g.variant = "Classic";
		g.scale = S_HARM; g.bpm = 140; g.swing = 0.5f; g.complexity = 0.6f; g.color = 0.3f; g.rolls = 0.9f;
		harm(g, 32, 0.5f, 16, 0.3f, 64, 0.2f);
		tpl(g, {"i bVI", "i bVI bIII bVII", "i iv bVI V", "i V", "i bII", "i bVI iv V"});
		degw(g, {6, 0.3f, 0.8f, 1.4f, 1.2f, 1.8f, 0.6f});
		drums(g, {"X...|...o|..X.|....", "....|....|X...|....", "....|....|X...|....", "................",
		          "xxxx|xxxx|xrxr|xxxx", "....|....|....|..o.", "................", "....|..o.|....|..o."});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.2f; g.bass = "X.....x...X.....";
		g.melStyle = MS_RIFF; g.mel = "x..x..x...o..x..";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_ROLL;
		add(17, g);
	}
	{ // 18 DRILL (2-bar grid)
		Genre g;
		g.name = "Drill"; g.variant = "UK";
		g.scale = S_MINOR; g.bpm = 142; g.swing = 0.5f; g.complexity = 0.55f; g.color = 0.25f; g.rolls = 0.6f;
		harm(g, 32, 0.6f, 16, 0.3f, 64, 0.1f);
		tpl(g, {"i bVI", "i iv", "i bII", "i bVI V", "i bVI iv V"});
		degw(g, {6, 0.3f, 0.6f, 1.4f, 1.0f, 1.8f, 0.8f});
		drums(g, {"X...|...o|..x.|....|..x.|....|o...|..x.", "....|....|X...|....|....|....|....|X...",
		          "....|....|x...|....|....|....|....|x...", "....|..o.|....|....|....|.o..|....|....",
		          "x..x|..x.|x..x|..x.|x..x|..x.|xr.x|..x.", "....|....|....|..o.|....|....|....|....",
		          "................................", "....|....|....|....|....|....|..o.|...."});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.6f; g.slide = 0.4f; g.bass = "X..x....x..o..x.";
		g.melStyle = MS_RIFF; g.mel = "x..x..x...x.....";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_ROLL;
		add(18, g);
	}
	{ // 19 TRIP-HOP
		Genre g;
		g.name = "Trip-Hop"; g.variant = "Bristol";
		g.scale = S_MINOR; g.bpm = 86; g.swing = 0.58f; g.complexity = 0.45f; g.color = 0.5f; g.rolls = 0.05f;
		harm(g, 32, 0.6f, 16, 0.3f, 64, 0.1f);
		tpl(g, {"i bVII bVI bVII", "i7 iv7", "i bVI V", "i iv bVI V"});
		minorW(g);
		drums(g, {"X...|....|..x.|....", "....|X...|....|X...", "................", "....|..o.|....|....",
		          "x.x.|x.x.|x.x.|x.x.", "....|..o.|....|....", "....|....|....|..o.", ".o..|.o..|.o..|.o.."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.1f; g.bass = "X.......x.....o.";
		g.melStyle = MS_SPARSE; g.mel = "x.......o.......";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_TOMS; g.humanize = 0.4f;
		add(19, g);
	}
	{ // 20 DOWNTEMPO
		Genre g;
		g.name = "Downtempo"; g.variant = "Chillout";
		g.scale = S_MAJOR; g.bpm = 96; g.swing = 0.55f; g.complexity = 0.35f; g.color = 0.8f; g.rolls = 0.0f;
		harm(g, 32, 0.7f, 16, 0.3f, 64, 0.0f);
		tpl(g, {"Imaj7 IVmaj7", "vi7 IVmaj7 Imaj7", "Imaj7 iii7 IVmaj7", "Imaj9 vi9"});
		majorW(g);
		drums(g, {"X...|....|X.o.|....", "................", "................", "....|X...|....|X...",
		          "..o.|..o.|..o.|..o.", "................", "....|....|....|..o.", "xoxo|xoxo|xoxo|xoxo"});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.08f; g.bass = "X.....x...x..o..";
		g.melStyle = MS_ARP; g.mel = "x.o.x.o.x.o.x.o.";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_TOMS; g.humanize = 0.3f;
		add(20, g);
		g.variant = "Organic"; g.scale = S_MINOR; minorW(g);
		tpl(g, {"i9 bVImaj7", "i7 iv9", "i7 bVImaj7 bVII", "i9 iv9"});
		g.melStyle = MS_LEAD; g.melPenta = true; g.mel = "x...o...x...o...";
		g.drum[7] = "xoxo|xoxx|xoxo|xoxx"; g.drum[6] = "....|..o.|....|o.o.";
		add(20, g);
	}
	// ================================================== OTHERS
	{ // 21 DUB
		Genre g;
		g.name = "Dub"; g.variant = "Roots";
		g.scale = S_MINOR; g.bpm = 76; g.swing = 0.58f; g.complexity = 0.4f; g.color = 0.3f; g.rolls = 0.0f;
		harm(g, 32, 0.5f, 16, 0.3f, 64, 0.2f);
		tpl(g, {"i bVII", "i iv", "i bVII bVI bVII", "i v", "i", "i iv bVII i"});
		degw(g, {6, 0.2f, 0.8f, 1.6f, 1.0f, 1.0f, 1.8f});
		drums(g, {"....|....|X...|....", "................", "................", "....|....|X...|....",
		          "x.o.|x.o.|x.o.|x.o.", "....|..o.|....|..o.", "....|....|....|o.o.", "..o.|....|o..o|...."});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.75f; g.bass = "X..x..x.x..o.x..";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "x.......o.....o.";
		g.padStyle = PS_SKANK; g.pad = "....X.......X...";
		g.fillStyle = FS_TOMS;
		add(21, g);
		g.variant = "Steppers"; g.drum[0] = "X...|X...|X...|X..."; g.bpm = 140; g.swing = 0.52f;
		add(21, g);
	}
	{ // 22 SYNTHWAVE
		Genre g;
		g.name = "Synthwave"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 100; g.swing = 0.5f; g.complexity = 0.25f; g.color = 0.4f;
		harm(g, 32, 0.6f, 16, 0.4f, 64, 0.0f);
		tpl(g, {"i bVI bIII bVII", "bVI bVII i", "i bVII bVI bVII", "i iv bVI bVII", "bVI bIII bVII i"});
		minorW(g);
		drums(g, {"X...|....|X...|..o.", "....|X...|....|X...", "....|x...|....|x...", "................",
		          "x.x.|x.x.|x.x.|x.x.", "................", "....|....|....|o.o.", "................"});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.3f; g.bass = "x.x.x.x.x.x.x.x.";
		g.melStyle = MS_LEAD; g.mel = "x..o..x...o.o...";
		g.padStyle = PS_SUSTAIN;
		g.fillStyle = FS_TOMS;
		add(22, g);
		g.variant = "Outrun"; g.bpm = 112; g.melStyle = MS_ARP; g.mel = "xxxxxxxxxxxxxxxx"; g.bass = "xxxxxxxxxxxxxxxx";
		add(22, g);
	}
	{ // 23 AMBIENT
		Genre g;
		g.name = "Ambient"; g.variant = "Classic";
		g.scale = S_MAJOR; g.bpm = 80; g.swing = 0.5f; g.complexity = 0.15f; g.color = 0.8f;
		harm(g, 64, 0.6f, 32, 0.4f, 64, 0.0f);
		tpl(g, {"Imaj7 IVmaj7", "Isus2 vi7 IVmaj7", "vi7 IVmaj7 Imaj7 V", "Imaj9 iii7 IVmaj7 IVmaj7", "IVmaj7 Imaj7"});
		majorW(g);
		drums(g, {"o...|....|....|....", "................", "................", "....|....|o...|....",
		          "................", "................", "................", "..o.|....|o..o|...."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.2f; g.bass = "X...............";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "o.......o.......";
		g.padStyle = PS_SUSTAIN;
		add(23, g);
		g.variant = "Bright"; g.scale = S_LYDIAN;
		add(23, g);
	}

	// ================================================== v3: AFRO FAMILY (24-27)
	{ // 24 AFROBEAT
		Genre g;
		g.name = "Afrobeat"; g.variant = "Classic";
		g.scale = S_DORIAN; g.bpm = 110; g.swing = 0.56f; g.complexity = 0.55f; g.color = 0.5f; g.rolls = 0.05f;
		harm(g, 32, 0.5f, 64, 0.4f, 16, 0.1f);
		tpl(g, {"i7", "i7 IV7", "i7 iv7", "i bVII"});
		degw(g, {6, 0.4f, 0.6f, 1.4f, 0.8f, 1.0f, 1.4f});
		drums(g, {"X...|..x.|o...|..x.", "....|X..o|....|X..o", "................", "..x.|.x..|x..x|..x.",
		          "xoxo|xoxo|xoxo|xoxo", "....|..o.|....|..o.", "..o.|..x.|..o.|.xx.", "x.xx|.x.x|.xx.|x.x."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.5f; g.bassCycle = 16; g.bass = "X..x|..x.|X.x.|.x.o";
		g.melStyle = MS_RIFF; g.melPenta = true; g.mel = "....|....|..x.|x.x.";
		g.padStyle = PS_STABS; g.padBase = 0.8f; g.pad = "..x.|..x.|..x.|..x.";
		g.fillStyle = FS_TOMS; g.humanize = 0.35f;
		add(24, g);
		g.variant = "Tony Allen"; g.complexity = 0.7f; g.color = 0.65f; g.rolls = 0.12f;
		g.drum[1] = "...o|X..o|.o..|X.oo"; g.drum[4] = "xxxo|xxxo|xxxo|xxxx"; g.humanize = 0.45f;
		add(24, g);
		g.variant = "Afro-Funk"; g.bpm = 118; g.complexity = 0.6f; g.color = 0.45f;
		g.drum[1] = "....|X..o|....|X..o"; g.drum[4] = "xoxo|xoxo|xoxo|xoxo"; g.humanize = 0.3f;
		g.bassMove = 0.65f; g.bass = "X.xx|..x.|X.xx|.xx."; g.mel = "..x.|x...|..x.|x.x."; g.melStyle = MS_RIFF;
		add(24, g);
	}
	{ // 25 AFROBEATS
		Genre g;
		g.name = "Afrobeats"; g.variant = "Afropop";
		g.scale = S_MAJOR; g.bpm = 105; g.swing = 0.58f; g.complexity = 0.45f; g.color = 0.6f; g.rolls = 0.1f;
		harm(g, 16, 0.7f, 32, 0.3f, 16, 0.0f);
		tpl(g, {"Imaj7 vi7 IVmaj7 V", "vi IV I V", "IVmaj7 V iii7 vi7", "Imaj7 IVmaj7"});
		majorW(g);
		drums(g, {"X...|...o|..X.|....", "....|X...|....|X...", "....|o...|....|o...", "x..x|..x.|x..x|..x.",
		          "X.X.|.X.X|..X.|.X..", "....|..o.|....|..o.", "..o.|..x.|..o.|.x..", "xoxo|xoxo|xoxo|xoxo"});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.55f; g.bass = "X..x|..x.|..X.|.x..";
		g.melStyle = MS_RIFF; g.melPenta = true; g.mel = "x.x.|.x..|x.x.|..x.";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.5f;
		g.fillStyle = FS_SNARE; g.humanize = 0.2f;
		add(25, g);
		g.variant = "Afro-Swing"; g.bpm = 98; g.swing = 0.62f; g.scale = S_MINOR; g.color = 0.55f;
		tpl(g, {"i7 iv7 bVII bIII", "i7 bVImaj7 bVII", "i7 iv7"});
		minorW(g);
		add(25, g);
		g.variant = "Afro-Piano"; g.bpm = 108; g.swing = 0.6f; g.scale = S_MAJOR; g.color = 0.7f;
		tpl(g, {"Imaj7 vi7 ii7 V", "IVmaj7 iii7 vi7", "Imaj9 vi9"});
		majorW(g);
		g.bassLen = 1; g.bassMove = 0.7f; g.slide = 0.35f; g.bass = "X..x|..x.|x..x|..x.";
		g.padStyle = PS_STABS; g.pad = "..x.|..x.|..x.|..x.";
		add(25, g);
	}
	{ // 26 AMAPIANO
		Genre g;
		g.name = "Amapiano"; g.variant = "Classic";
		g.scale = S_MAJOR; g.bpm = 113; g.swing = 0.6f; g.complexity = 0.45f; g.color = 0.85f; g.rolls = 0.08f;
		harm(g, 16, 0.6f, 32, 0.4f, 16, 0.0f);
		tpl(g, {"Imaj9 vi9 ii9 V", "IVmaj7 iii7 ii7 Imaj7", "ii9 V9 Imaj9", "Imaj9 IVmaj9"});
		majorW(g);
		drums(g, {"X...|X...|X...|X...", "................", "....|x...|....|x...", "..x.|..x.|..x.|.x.x",
		          "xxxx|xxxx|xxxx|xxxx", "..x.|..x.|..x.|..x.", "................", "....|..o.|.o..|..o."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.7f; g.slide = 0.4f; g.bass = "X..x|..x.|x..x|..x.";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "....|x...|....|..o.";
		g.padStyle = PS_STABS; g.padBase = 0.9f; g.pad = "..x.|..x.|..x.|..o.";
		g.fillStyle = FS_DROP; g.dropMode = DM_KICKBASS;
		add(26, g);
		g.variant = "Soulful"; g.color = 0.95f; g.complexity = 0.4f;
		g.drum[0] = "X...|....|X...|...."; g.padStyle = PS_SUSTAIN; g.padBase = 1.1f; g.mel = "x...|..o.|....|o...";
		add(26, g);
		g.variant = "Log Drum"; g.color = 0.6f; g.complexity = 0.55f;
		g.drum[0] = "X...|X...|X...|X...";
		tpl(g, {"Imaj7 vi7", "vi7 IVmaj7", "Imaj7"});
		harm(g, 32, 0.6f, 64, 0.4f, 16, 0.0f);
		g.bassMove = 0.85f; g.slide = 0.5f; g.bass = "X.xx|..x.|x.xx|.xx.";
		g.padStyle = PS_STABS; g.padBase = 0.6f; g.pad = "..x.|....|..x.|....";
		add(26, g);
	}
	{ // 27 AFRO HOUSE
		Genre g;
		g.name = "Afro House"; g.variant = "Classic";
		g.scale = S_DORIAN; g.bpm = 122; g.swing = 0.57f; g.complexity = 0.5f; g.color = 0.6f; g.rolls = 0.08f;
		harm(g, 32, 0.6f, 64, 0.3f, 16, 0.1f);
		tpl(g, {"i7 IV7", "i9 iv9", "i7 bVII", "i9"});
		degw(g, {6, 0.4f, 0.6f, 1.4f, 0.8f, 1.0f, 1.4f});
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "..x.|.x..|..x.|.x..",
		          "xoxo|xoxo|xoxo|xoxo", "..x.|..x.|..x.|..x.", "..x.|o..x|..x.|o.x.", "o..x|..o.|..x.|.o.."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.35f; g.bass = "..x.|..xo|..x.|.xx.";
		g.melStyle = MS_RIFF; g.melPenta = true; g.mel = "x..x|..x.|.x..|x...";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.9f;
		g.fillStyle = FS_TOMS; g.humanize = 0.2f;
		add(27, g);
		g.variant = "Organic"; g.bpm = 119; g.complexity = 0.6f; g.color = 0.7f;
		g.drum[7] = "o.xx|..ox|.ox.|xo.o"; g.drum[6] = "..x.|o.xx|..x.|o.xo";
		add(27, g);
		g.variant = "3-Step"; g.bpm = 122; g.complexity = 0.5f;
		g.drum[0] = "X...|..X.|..X.|...."; g.drum[6] = "..x.|o..x|..x.|o.x."; g.drum[7] = "o..x|..o.|..x.|.o..";
		g.bassKick = 0; g.bass = "X...|..x.|..x.|.x..";
		add(27, g);
	}

	// ================================================== v3: TECHNO STYLES (28-32)
	{ // 28 DUB TECHNO
		Genre g;
		g.name = "Dub Techno"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 122; g.swing = 0.52f; g.complexity = 0.3f; g.color = 0.55f; g.rolls = 0.0f;
		harm(g, 128, 0.6f, 64, 0.4f, 32, 0.0f);
		tpl(g, {"i7", "i7 i7 bVII i7", "i7 iv7", "isus2"});
		degw(g, {6, 0.2f, 0.4f, 1.0f, 0.4f, 1.2f, 1.0f});
		drums(g, {"X...|X...|X...|X...", "................", "....|o...|....|o...", "....|..o.|....|...3",
		          "..X.|..X.|..X.|..X.", "................", "................", "...3|....|.3..|...."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.1f; g.bass = "X...|X...|X...|X...";
		g.melStyle = MS_SPARSE; g.mel = "................"; g.melPenta = true;
		g.padStyle = PS_STABS; g.padBase = 1.0f; g.pad = "..X.|.3.x|..o.|.2..";  // stab + quieter echoes 3 steps later
		g.fillStyle = FS_DROP;
		add(28, g);
		g.variant = "Deep Chord"; g.padStyle = PS_SUSTAIN; g.padBase = 1.1f; g.color = 0.7f;
		g.drum[7] = "...2|..2.|.2..|...2";
		add(28, g);
	}
	{ // 29 MELODIC TECHNO
		Genre g;
		g.name = "Melodic Techno"; g.variant = "Afterlife";
		g.scale = S_MINOR; g.bpm = 124; g.swing = 0.5f; g.complexity = 0.4f; g.color = 0.45f; g.rolls = 0.15f;
		harm(g, 32, 0.6f, 16, 0.3f, 64, 0.1f);
		tpl(g, {"i bVI bIII bVII", "i bVI iv v", "i iv bVI v", "bVI bVII i i"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "....|....|..o.|....",
		          "xxXx|xxXx|xxXx|xxXx", "..x.|..x.|..x.|..x.", "....|....|....|..o.", "....|..3.|....|3..."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.25f; g.bass = ".XXX|.XXX|.XXX|.XXX";
		g.melStyle = MS_ARP; g.mel = "xxxx|xxxx|xxxx|xxxx";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.8f;
		g.fillStyle = FS_ROLL; g.dropMode = DM_KICKBASS;
		add(29, g);
		g.variant = "House"; g.bpm = 122; g.color = 0.6f; g.swing = 0.53f;
		tpl(g, {"i7 bVImaj7 bIIImaj7 bVII", "i7 iv7", "bVImaj7 bVII i7"});
		g.drum[7] = "xoxo|xoxo|xoxo|xoxo";
		g.melStyle = MS_LEAD; g.melSlide = 0.2f; g.mel = "x...|..o.|x...|o...";
		g.bass = "..x.|..x.|..x.|.xx.";
		add(29, g);
	}
	{ // 30 HARD TECHNO
		Genre g;
		g.name = "Hard Techno"; g.variant = "Industrial";
		g.scale = S_PHRYG; g.bpm = 145; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.05f; g.rolls = 0.2f;
		harm(g, 128, 0.7f, 64, 0.3f, 32, 0.0f);
		tpl(g, {"i", "i bII"});
		degw(g, {6, 1.2f, 0.4f, 0.6f, 0.4f, 0.6f, 0.6f});
		drums(g, {"X...|X...|X...|X...", "................", "....|x...|....|x...", "..o.|....|..o.|.o..",
		          "xXxX|xXxX|xXxX|xXxX", "..X.|..X.|..X.|..X.", ".xx.|.xx.|.xx.|.xxx", "...o|..o.|...o|o..."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.15f; g.bass = ".xx.|.xx.|.xx.|.xx.";
		g.melStyle = MS_RIFF; g.mel = "..o.|....|..o.|...o";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.0f;
		g.fillStyle = FS_ROLL; g.dropMode = DM_KICKBASS;
		add(30, g);
		g.variant = "Schranz"; g.bpm = 155; g.complexity = 0.5f;
		g.drum[2] = "X...|X...|X...|X..."; g.drum[5] = "..X.|..X.|..X.|..Xx"; g.drum[7] = "x.xo|x.xo|x.xo|x.xx";
		add(30, g);
		g.variant = "Rave"; g.bpm = 150; g.scale = S_MINOR; g.color = 0.2f;
		tpl(g, {"i", "i bVI", "i bVII"});
		minorW(g);
		g.drum[2] = "....|x...|....|x..."; g.drum[5] = "..X.|..X.|..X.|..X.";
		g.mel = "x..x|..x.|x..x|..x."; g.padBase = 0.3f;
		add(30, g);
	}
	{ // 31 TECH HOUSE
		Genre g;
		g.name = "Tech House"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 126; g.swing = 0.56f; g.complexity = 0.45f; g.color = 0.3f; g.rolls = 0.1f;
		harm(g, 64, 0.6f, 32, 0.4f, 16, 0.0f);
		tpl(g, {"i7", "i7 bVII", "i7 iv7"});
		degw(g, {6, 0.3f, 0.6f, 1.2f, 0.6f, 1.0f, 1.6f});
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "...o|....|.o..|..o.",
		          "..X.|..X.|..X.|..X.", "................", "................", "xoxo|xoxo|xoxo|xoxo"});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.5f; g.slide = 0.15f; g.bass = "..X.|..Xo|..X.|.xX.";
		g.melStyle = MS_RIFF; g.melPenta = true; g.mel = "..x.|....|.x..|..o.";
		g.padStyle = PS_STABS; g.padBase = 0.3f; g.pad = "........o.......";
		g.fillStyle = FS_DROP; g.dropMode = DM_KICKBASS;
		add(31, g);
		g.variant = "Rolling"; g.bass = ".XXX|.XXX|.XXX|.XXX"; g.bassMove = 0.4f; g.slide = 0.1f;
		add(31, g);
		g.variant = "Minimal"; g.bpm = 122; g.complexity = 0.35f;
		g.drum[3] = "..x.|...x|.x..|..x."; g.drum[7] = "o.o.|o.o.|o.o.|o.oo";
		g.bass = "..x.|....|..x.|.x..";
		add(31, g);
	}
	{ // 32 DETROIT TECHNO
		Genre g;
		g.name = "Detroit Techno"; g.variant = "Strings";
		g.scale = S_MINOR; g.bpm = 128; g.swing = 0.53f; g.complexity = 0.5f; g.color = 0.6f; g.rolls = 0.1f;
		harm(g, 16, 0.5f, 32, 0.5f, 64, 0.0f);
		tpl(g, {"i7 bVII7 bVImaj7 bVII7", "i7 iv7 i7 v7", "i9 bIIImaj7 iv7 bVImaj7", "i9 bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "....|X...|....|X...", "....|x...|....|x..o", "...o|..x.|...o|.o..",
		          "x.x.|x.x.|x.x.|x.xo", "..X.|..X.|..X.|..X.", "....|....|....|..oo", "................"});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.5f; g.bass = "X.xX|.xX.|X.xX|.x.o";
		g.melStyle = MS_RIFF; g.mel = "x..x|..x.|..x.|x...";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.0f;
		g.fillStyle = FS_TOMS;
		add(32, g);
		g.variant = "Electro"; g.drum[0] = "X..X|..X.|X...|..X."; g.bassKick = 1;
		g.bass = "X..x|..x.|X...|..x."; g.padBase = 0.7f;
		add(32, g);
		g.variant = "Deep"; g.bpm = 125; g.drum[0] = "X...|X...|X...|X..."; g.drum[4] = "..x.|..x.|..x.|..x.";
		g.bassKick = 0; g.bass = "X.x.|..x.|X.x.|...o"; g.padBase = 1.2f; g.color = 0.75f; g.complexity = 0.4f;
		add(32, g);
	}

	// ================================================== v3: LATIN & CLUB (33-36)
	{ // 33 BAILE FUNK
		Genre g;
		g.name = "Baile Funk"; g.variant = "Tamborzao";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.54f; g.complexity = 0.5f; g.color = 0.15f; g.rolls = 0.15f;
		harm(g, 64, 0.6f, 32, 0.4f, 16, 0.0f);
		tpl(g, {"i v bVI bVII", "i bVII", "i"});
		minorW(g);
		drums(g, {"X..x|..X.|..X.|.x..", "................", "...X|....|...X|..o.", "................",
		          "..x.|..x.|..x.|..x.", "................", "..r.|X..r|..X.|r.X.", ".o..|.o..|.o..|.o.."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.15f; g.slide = 0.1f; g.bass = "X..x|..X.|..X.|.x..";
		g.melStyle = MS_RIFF; g.melPenta = true; g.mel = "x...|..x.|....|x...";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.0f;
		g.fillStyle = FS_DROP; g.dropMode = DM_BREAK;
		add(33, g);
		g.variant = "150 BPM"; g.bpm = 150; g.complexity = 0.55f;
		g.drum[0] = "X..x|..X.|X..x|..X."; g.drum[4] = "xxxx|xxxx|xxxx|xxxx"; g.drum[6] = "....|X...|....|..X.";
		g.bass = "X..x|..X.|X..x|..X.";
		add(33, g);
		g.variant = "Volt Mix"; g.bpm = 127; g.complexity = 0.45f;
		g.drum[0] = "X..X|..X.|X..X|..X."; g.drum[2] = "....|X...|....|X..."; g.drum[4] = "x.x.|x.x.|x.x.|x.x.";
		g.drum[6] = "................"; g.bass = "X..X|..X.|X..X|..X.";
		add(33, g);
	}
	{ // 34 REGGAETON
		Genre g;
		g.name = "Reggaeton"; g.variant = "Perreo";
		g.scale = S_MINOR; g.bpm = 94; g.swing = 0.52f; g.complexity = 0.4f; g.color = 0.25f; g.rolls = 0.1f;
		harm(g, 16, 0.6f, 32, 0.4f, 16, 0.0f);
		tpl(g, {"i bVI bIII bVII", "bVI bVII i i", "i iv bVII bIII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "...X|..X.|...X|..X.", "................", "....|....|.o..|....",
		          "x.x.|x.x.|x.x.|x.x.", "....|...o|....|...o", "................", "2525|2525|2525|2525"});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.25f; g.bass = "X..x|X...|X..x|X...";
		g.melStyle = MS_LEAD; g.mel = "..X.|.X..|..X.|X...";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.5f;
		g.fillStyle = FS_SNARE;
		add(34, g);
		g.variant = "Clasico"; g.bpm = 92; g.complexity = 0.35f;
		g.drum[1] = "................"; g.drum[3] = "...X|..X.|...X|..X."; g.drum[4] = "................";
		g.drum[7] = "................"; g.padBase = 0.3f;
		add(34, g);
		g.variant = "Dominicano"; g.bpm = 122; g.complexity = 0.5f;
		g.drum[1] = "...X|..X.|...X|..X."; g.drum[3] = "................"; g.drum[4] = "x.x.|x.x.|x.x.|x.x.";
		g.drum[7] = "x5x5|x5x5|x5x5|x5x5"; g.padBase = 0.0f; g.melStyle = MS_SPARSE;
		add(34, g);
	}
	{ // 35 FOOTWORK
		Genre g;
		g.name = "Footwork"; g.variant = "Battle";
		g.scale = S_MINOR; g.bpm = 160; g.swing = 0.5f; g.complexity = 0.6f; g.color = 0.45f; g.rolls = 0.3f;
		harm(g, 64, 0.6f, 32, 0.4f, 128, 0.0f);
		tpl(g, {"i7", "i7 bVImaj7", "i9"});
		minorW(g);
		drums(g, {"X..X|..X.|....|X..x|X..X|..X.|..X.|X..X", "................", "....|....|X...|....|....|....|X..o|....",
		          "..o.|....|..o.|....|..o.|....|....|.o..", "rrXr|rXrr|Xrrr|Xrr.|rrXr|rXrr|Xrrr|X.rr", "................",
		          "....|..o.|...o|....|....|..o.|...o|..oo", "................"});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.3f; g.bass = "X..X|..X.|....|X..x";
		g.melStyle = MS_RIFF; g.mel = "x.x.|x...|..x.|x.x.";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.0f;
		g.fillStyle = FS_ROLL;
		add(35, g);
		g.variant = "Soul"; g.color = 0.7f; g.padBase = 0.7f; g.melStyle = MS_LEAD; g.mel = "x...|..x.|x...|....";
		add(35, g);
		g.variant = "Juke"; g.bpm = 150; g.complexity = 0.5f; g.padBase = 0.0f; g.melStyle = MS_RIFF;
		g.drum[0] = "X...|X..X|X...|X.x."; g.drum[2] = "....|X...|....|X..."; g.drum[4] = "x.x.|x.x.|x.x.|x.xx";
		g.drum[3] = "................"; g.bass = "X...|X..X|X...|X.x.";
		add(35, g);
	}
	{ // 36 BREAKCORE
		Genre g;
		g.name = "Breakcore"; g.variant = "Amen";
		g.scale = S_MINOR; g.bpm = 175; g.swing = 0.5f; g.complexity = 0.75f; g.color = 0.3f; g.rolls = 0.6f;
		harm(g, 64, 0.5f, 32, 0.4f, 128, 0.1f);
		tpl(g, {"i", "i bVI", "i bII", "i iv"});
		degw(g, {6, 0.6f, 0.6f, 1.2f, 0.6f, 1.4f, 1.0f});
		drums(g, {"X.X.|....|..XX|....", "....|X..x|.x..|X..x", "................", "....|..o.|....|..o.",
		          "X.X.|X.X.|X.X.|X.X.", "....|....|....|...o", "....|....|....|.rrr", "................"});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.6f; g.slide = 0.3f; g.bass = "X.......x...o...";
		g.melStyle = MS_LEAD; g.mel = "x.......o.......";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.4f;
		g.fillStyle = FS_CHOP; g.chop = 0.8f; g.dropMode = DM_BREAK;
		add(36, g);
		g.variant = "Gabber"; g.bpm = 188; g.complexity = 0.85f; g.scale = S_PHRYG; g.color = 0.1f;
		g.drum[0] = "X.X.|X...|X.XX|X...";  // amen kicks + straight gabber kick on every beat
		g.drum[1] = "....|X.xx|.x..|X.xx";
		add(36, g);
		g.variant = "Melodic"; g.bpm = 165; g.complexity = 0.65f; g.scale = S_MINOR; g.color = 0.8f; g.chop = 0.5f;
		harm(g, 16, 0.5f, 32, 0.5f, 64, 0.0f);
		tpl(g, {"i9 bVImaj7 bIIImaj7 bVII", "bVImaj7 bVII i9", "i7 iv7 bVImaj7 v7"});
		minorW(g);
		g.drum[0] = "X.X.|....|..XX|...."; g.drum[1] = "....|X..x|.x..|X..x";
		g.padBase = 1.2f; g.melStyle = MS_LEAD; g.melPenta = true; g.mel = "x...|..o.|x...|o...";
		add(36, g);
	}

	// ================================================== v2.24: 22 NEW GENRES (37-58)
	{ // 37 PROGRESSIVE HOUSE
		Genre g;
		g.name = "Progressive House"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 124; g.swing = 0.5f; g.complexity = 0.35f; g.color = 0.6f; g.rolls = 0.1f;
		harm(g, 32, 0.5f, 64, 0.4f, 16, 0.1f);
		tpl(g, {"i bVI bIII bVII", "i bVImaj7", "bVI bVII i", "i9 bVImaj7 bIIImaj7 bVII", "i iv bVI bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "................",
		          "..x.|..x.|..x.|..xo", "..X.|..X.|..X.|..X.", "................", "...o|....|...o|..o."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.25f; g.bass = ".xXx|.xXx|.xXx|.xXx";
		g.melStyle = MS_ARP; g.mel = "x.x.|x.x.|x.x.|x.x.";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.9f;
		g.fillStyle = FS_ROLL;
		add(37, g);
		g.variant = "Melodic"; g.color = 0.8f; g.melStyle = MS_LEAD; g.mel = "x...|..x.|..o.|x...";
		g.bpm = 122; g.padBase = 1.1f;
		add(37, g);
	}
	{ // 38 ORGANIC HOUSE
		Genre g;
		g.name = "Organic House"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 118; g.swing = 0.53f; g.complexity = 0.4f; g.color = 0.65f; g.rolls = 0.0f;
		harm(g, 32, 0.6f, 64, 0.4f, 16, 0.0f);
		tpl(g, {"i7 bVImaj7", "i9 iv7", "i7 bVII bVImaj7", "i9"});
		degw(g, {6, 0.3f, 1.0f, 1.5f, 0.8f, 1.6f, 1.4f});
		drums(g, {"X...|X...|X...|X...", "................", "....|o...|....|x...", "..o.|...o|..o.|....",
		          "x.x.|x.x.|x.x.|x.x.", "................", "....|..x.|...o|..x.", "x.ox|.xo.|x.ox|.x.o"});
		g.bassKick = -1; g.bassLen = 1; g.bassMove = 0.35f; g.bass = "..X.|..x.|..X.|.x..";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "x..o|..x.|...o|x...";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.0f;
		g.fillStyle = FS_TOMS; g.humanize = 0.3f;
		add(38, g);
		g.variant = "Downtempo"; g.bpm = 110; g.drum[0] = "X...|....|X.x.|...."; g.complexity = 0.3f;
		add(38, g);
	}
	{ // 39 UK FUNKY
		Genre g;
		g.name = "UK Funky"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.55f; g.complexity = 0.55f; g.color = 0.55f; g.rolls = 0.1f;
		harm(g, 32, 0.5f, 16, 0.4f, 64, 0.1f);
		tpl(g, {"i7 iv7", "i7 bVII", "i9 bVImaj7", "i7"});
		minorW(g);
		drums(g, {"X..x|..X.|X...|..x.", "................", "....|X...|....|X...", "..x.|.x..|x..x|.x..",
		          "x.xx|x.x.|x.xx|x.xo", "..o.|....|..o.|....", "...o|..x.|.o..|x.o.", "x.ox|..x.|.xo.|x..x"});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.45f; g.bass = "X..x|..X.|X...|..x.";
		g.melStyle = MS_RIFF; g.mel = "..x.|x...|..x.|.x..";
		g.padStyle = PS_STABS; g.pad = "...x|..o.|...x|.o..";
		g.fillStyle = FS_TOMS; g.humanize = 0.2f;
		add(39, g);
		g.variant = "Tribal"; g.drum[6] = "..x.|.xx.|..x.|x.xx"; g.drum[7] = "xoxx|.xox|xoxx|.x.x"; g.padBase = 0.5f;
		g.complexity = 0.65f;
		add(39, g);
	}
	{ // 40 BASSLINE
		Genre g;
		g.name = "Bassline"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 138; g.swing = 0.52f; g.complexity = 0.45f; g.color = 0.4f; g.rolls = 0.1f;
		harm(g, 16, 0.5f, 32, 0.5f, 8, 0.0f);
		tpl(g, {"i bVI", "i bVII bVI bVII", "i iv", "i bIII bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "....|X...|....|X...", "....|x...|....|x...", "................",
		          "..x.|..x.|..x.|..x.", "..o.|....|..o.|..o.", "................", "...o|....|....|.o.."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.6f; g.slide = 0.4f; g.bass = "X.xX|.x.X|x.xX|.xX.";
		g.melStyle = MS_RIFF; g.mel = "x..x|..x.|x..x|....";
		g.padStyle = PS_STABS; g.pad = "x..x|..x.|....|x...";
		g.fillStyle = FS_SNARE;
		add(40, g);
		g.variant = "Niche"; g.bpm = 136; g.color = 0.6f; g.pad = "...x|..x.|...x|..x."; g.padBase = 1.0f;
		add(40, g);
	}
	{ // 41 GQOM
		Genre g;
		g.name = "Gqom"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 126; g.swing = 0.5f; g.complexity = 0.5f; g.color = 0.15f; g.rolls = 0.0f;
		harm(g, 64, 0.6f, 32, 0.4f, 128, 0.0f);
		tpl(g, {"i", "i bII", "i bVI"});
		degw(g, {6, 0.8f, 0.4f, 1.0f, 0.6f, 1.2f, 0.8f});
		drums(g, {"X..X|..X.|.X..|X...", "................", "....|....|....|X...", "..x.|....|..x.|..o.",
		          "................", "................", "..X.|.x.x|x..x|.xX.", "...x|.o..|..x.|o..x"});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.2f; g.bass = "X...|....|.X..|....";
		g.melStyle = MS_SPARSE; g.mel = "x...|....|....|..o.";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.3f;
		g.fillStyle = FS_TOMS;
		add(41, g);
		g.variant = "Sgubhu"; g.bpm = 120; g.drum[0] = "X..x|..X.|X...|..X."; g.drum[4] = "..o.|..o.|..o.|..o.";
		g.padBase = 0.5f; g.color = 0.25f;
		add(41, g);
	}
	{ // 42 BIG BEAT
		Genre g;
		g.name = "Big Beat"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 120; g.swing = 0.53f; g.complexity = 0.5f; g.color = 0.25f; g.rolls = 0.1f;
		harm(g, 32, 0.5f, 16, 0.4f, 64, 0.1f);
		tpl(g, {"i", "i bVII", "i bIII bVII", "i iv"});
		degw(g, {6, 0.3f, 0.8f, 1.4f, 0.8f, 1.2f, 1.6f});
		drums(g, {"X.x.|....|..X.|....", "....|X...|....|X..x", "................", "................",
		          "x.x.|x.x.|x.x.|x.xx", "....|....|....|..o.", "................", "x...|..o.|....|x..."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.4f; g.bass = "X.x.|....|..X.|.x..";
		g.melStyle = MS_RIFF; g.mel = "x.x.|..x.|x...|x.o.";
		g.padStyle = PS_STABS; g.pad = "X...|....|....|..x."; g.padBase = 0.5f;
		g.fillStyle = FS_CHOP; g.chop = 0.3f; g.dropMode = DM_BREAK;
		add(42, g);
		g.variant = "Acid"; g.melStyle = MS_ACID; g.mel = "xxox|xoxx|xxox|x.xx"; g.bpm = 126;
		add(42, g);
	}
	{ // 43 BREAKS
		Genre g;
		g.name = "Breaks"; g.variant = "Nu-Skool";
		g.scale = S_MINOR; g.bpm = 132; g.swing = 0.5f; g.complexity = 0.5f; g.color = 0.35f; g.rolls = 0.15f;
		harm(g, 32, 0.5f, 64, 0.4f, 16, 0.1f);
		tpl(g, {"i", "i bVI", "i bVII bVI bVII", "i iv"});
		minorW(g);
		drums(g, {"X...|...x|..X.|....", "....|X...|....|X...", "................", "................",
		          "x.x.|x.x.|x.x.|x.xx", "..o.|....|..o.|....", "................", "...o|....|....|..o."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.5f; g.slide = 0.2f; g.bass = "X.xx|.xX.|x.xx|.x.x";
		g.melStyle = MS_RIFF; g.mel = "x...|..x.|x...|.x..";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.6f;
		g.fillStyle = FS_CHOP; g.chop = 0.2f; g.dropMode = DM_BREAK;
		add(43, g);
		g.variant = "Progressive"; g.bpm = 130; g.color = 0.6f; g.melStyle = MS_ARP; g.mel = "x.x.|x.x.|x.x.|x.x.";
		g.padBase = 1.0f;
		add(43, g);
	}
	{ // 44 JERSEY CLUB
		Genre g;
		g.name = "Jersey Club"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 140; g.swing = 0.5f; g.complexity = 0.55f; g.color = 0.5f; g.rolls = 0.3f;
		harm(g, 32, 0.6f, 16, 0.4f, 64, 0.0f);
		tpl(g, {"i bVI", "i7 iv7", "bVImaj7 bVII i", "i bIII bVII"});
		minorW(g);
		drums(g, {"X...|X...|X..X|..X.", "....|X...|....|X...", "....|X...|....|X...", "................",
		          "x.x.|x.x.|x.x.|x.x.", "................", "................", "rr.r|....|rr.r|...."});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.35f; g.bass = "X...|X...|X..X|..X.";
		g.melStyle = MS_RIFF; g.mel = "x.x.|..x.|x.x.|....";
		g.padStyle = PS_STABS; g.pad = "x...|....|x..x|...."; g.padBase = 0.7f;
		g.fillStyle = FS_ROLL;
		add(44, g);
		g.variant = "Philly"; g.swing = 0.55f; g.bpm = 135; g.drum[0] = "X..X|..X.|X..X|..X.";
		add(44, g);
	}
	{ // 45 BALTIMORE CLUB
		Genre g;
		g.name = "Baltimore Club"; g.variant = "Think";
		g.scale = S_MINOR; g.bpm = 130; g.swing = 0.5f; g.complexity = 0.55f; g.color = 0.35f; g.rolls = 0.2f;
		harm(g, 32, 0.6f, 64, 0.4f, 16, 0.0f);
		tpl(g, {"i", "i bVII", "i iv", "i bVI"});
		minorW(g);
		drums(g, {"X...|X...|X..X|..X.", "....|X..x|....|X...", "....|....|....|X...", "..x.|....|..x.|....",
		          "x.xx|x.xx|x.xx|x.xx", "..o.|....|..o.|....", "................", "................"});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.3f; g.bass = "X...|X...|X..X|..X.";
		g.melStyle = MS_RIFF; g.mel = "x...|x...|..x.|x...";
		g.padStyle = PS_STABS; g.pad = "X...|....|....|...."; g.padBase = 0.4f;
		g.fillStyle = FS_CHOP; g.chop = 0.5f; g.dropMode = DM_BREAK;
		add(45, g);
		g.variant = "Bmore Gutter"; g.bpm = 128; g.chop = 0.7f; g.complexity = 0.65f;
		add(45, g);
	}
	{ // 46 PHONK
		Genre g;
		g.name = "Phonk"; g.variant = "Memphis";
		g.scale = S_PHRYG; g.bpm = 140; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.3f; g.rolls = 0.5f;
		harm(g, 32, 0.5f, 64, 0.5f, 16, 0.0f);
		tpl(g, {"i", "i bII", "i bVI", "i iv bII"});
		degw(g, {6, 1.2f, 0.4f, 1.2f, 0.6f, 1.4f, 0.8f});
		drums(g, {"X...|....|..X.|....|X..x|....|..X.|..x.", "........|X.......|........|X.......", "................",
		          "................", "x.x.|x.rr|x.x.|xrrr", "................", "................",
		          "x..x|..x.|x..x|..x."});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.4f; g.slide = 0.6f; g.bass = "X...|....|..X.|....";
		g.melStyle = MS_RIFF; g.mel = "x..x|..x.|x..x|..x.";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.4f;
		g.fillStyle = FS_ROLL;
		add(46, g);
		g.variant = "Drift"; g.bpm = 130; g.drum[0] = "X...|..X.|X...|..X."; g.drum[1] = "....|X...|....|X...";
		g.complexity = 0.5f;
		add(46, g);
	}
	{ // 47 HARDCORE
		Genre g;
		g.name = "Hardcore"; g.variant = "Gabber";
		g.scale = S_PHRYG; g.bpm = 180; g.swing = 0.5f; g.complexity = 0.4f; g.color = 0.1f; g.rolls = 0.1f;
		harm(g, 64, 0.5f, 32, 0.5f, 128, 0.0f);
		tpl(g, {"i", "i bII", "i bVI", "i iv"});
		degw(g, {6, 1.0f, 0.4f, 1.2f, 0.6f, 1.2f, 0.8f});
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "................",
		          "................", "..X.|..X.|..X.|..X.", "................", "...o|....|...o|...."});
		g.bassKick = 1; g.bassLen = 0; g.bassMove = 0.2f; g.bass = "X...|X...|X...|X...";
		g.melStyle = MS_RIFF; g.mel = "x.x.|x...|x.x.|..x.";
		g.padStyle = PS_STABS; g.pad = "x...|....|x...|...."; g.padBase = 0.5f;
		g.fillStyle = FS_SNARE; g.dropMode = DM_KICKBASS;
		add(47, g);
		g.variant = "Happy Hardcore"; g.scale = S_MAJOR; g.bpm = 170; g.color = 0.6f;
		harm(g, 16, 0.6f, 32, 0.4f, 8, 0.0f);
		tpl(g, {"I V vi IV", "vi IV I V", "IV V I", "I iii IV V"});
		majorW(g);
		g.melStyle = MS_LEAD; g.mel = "x.x.|x.x.|x.x.|x.x."; g.padStyle = PS_SUSTAIN; g.padBase = 1.0f;
		g.drum[1] = "....|X...|....|X..."; g.drum[4] = "x.x.|x.x.|x.x.|x.x.";
		add(47, g);
	}
	{ // 48 PSYTRANCE
		Genre g;
		g.name = "Psytrance"; g.variant = "Full-On";
		g.scale = S_PHRYG; g.bpm = 142; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.2f; g.rolls = 0.1f;
		harm(g, 64, 0.6f, 128, 0.4f, 32, 0.0f);
		tpl(g, {"i", "i bII", "i bVII"});
		degw(g, {6, 1.2f, 0.4f, 1.0f, 0.6f, 1.0f, 0.8f});
		drums(g, {"X...|X...|X...|X...", "................", "....|X...|....|X...", "................",
		          "x.xx|x.xx|x.xx|x.xx", "..X.|..X.|..X.|..X.", "................", "...o|..o.|...o|.o.."});
		g.bassKick = -1; g.bassLen = 0; g.bassMove = 0.1f; g.bass = ".XXX|.XXX|.XXX|.XXX";
		g.melStyle = MS_ACID; g.mel = "x.xx|xx.x|x.xx|.xxx";
		g.padStyle = PS_SUSTAIN; g.padBase = 0.3f;
		g.fillStyle = FS_ROLL; g.dropMode = DM_KICKBASS;
		add(48, g);
		g.variant = "Progressive Psy"; g.bpm = 136; g.scale = S_MINOR; g.color = 0.5f; g.padBase = 0.8f;
		g.melStyle = MS_ARP; g.mel = "x.x.|x.x.|x.x.|x.x.";
		add(48, g);
	}
	{ // 49 EBM
		Genre g;
		g.name = "EBM"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 120; g.swing = 0.5f; g.complexity = 0.3f; g.color = 0.15f; g.rolls = 0.0f;
		harm(g, 64, 0.5f, 32, 0.5f, 128, 0.0f);
		tpl(g, {"i", "i bVII", "i iv", "i bVI bVII i"});
		degw(g, {6, 0.6f, 0.6f, 1.2f, 0.8f, 1.2f, 1.4f});
		drums(g, {"X...|X...|X...|X...", "....|X...|....|X...", "....|X...|....|X...", "................",
		          "x.x.|x.x.|x.x.|x.x.", "................", "................", "................"});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.15f; g.bass = "X.X.|X.X.|X.X.|X.X.";
		g.melStyle = MS_SPARSE; g.mel = "x...|....|x...|....";
		g.padStyle = PS_STABS; g.pad = "X...|....|....|...."; g.padBase = 0.3f;
		g.fillStyle = FS_SNARE;
		add(49, g);
		g.variant = "Industrial"; g.bpm = 128; g.scale = S_PHRYG; g.complexity = 0.45f;
		g.drum[6] = "..o.|....|..x.|...o"; g.drum[7] = "...x|..x.|...x|.x.x"; g.drum[4] = "x.xx|x.xx|x.xx|x.xx";
		add(49, g);
	}
	{ // 50 DANCEHALL
		Genre g;
		g.name = "Dancehall"; g.variant = "Modern";
		g.scale = S_MINOR; g.bpm = 100; g.swing = 0.52f; g.complexity = 0.45f; g.color = 0.4f; g.rolls = 0.1f;
		harm(g, 32, 0.6f, 16, 0.4f, 64, 0.0f);
		tpl(g, {"i iv", "i bVII", "i bVI bVII", "i7 iv7"});
		minorW(g);
		drums(g, {"X...|..X.|X...|..X.", "...x|..X.|...x|..X.", "................", "...x|..x.|...x|..x.",
		          "x.x.|x.x.|x.x.|x.x.", "................", "................", "..o.|....|..o.|...."});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.4f; g.bass = "X..x|..X.|X...|..X.";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "x...|..x.|....|x...";
		g.padStyle = PS_SKANK; g.pad = "..x.|..x.|..x.|..x."; g.padBase = 0.7f;
		g.fillStyle = FS_SNARE;
		add(50, g);
		g.variant = "Steppers"; g.drum[0] = "X...|X...|X...|X..."; g.bass = "X...|X...|X...|X..."; g.bpm = 96;
		add(50, g);
	}
	{ // 51 ROOTS REGGAE
		Genre g;
		g.name = "Roots Reggae"; g.variant = "One Drop";
		g.scale = S_MINOR; g.bpm = 75; g.swing = 0.55f; g.complexity = 0.35f; g.color = 0.4f; g.rolls = 0.0f;
		harm(g, 32, 0.5f, 16, 0.4f, 64, 0.1f);
		tpl(g, {"i iv", "i bVII", "i bVII bVI bVII", "i iv bVII i", "I IV V IV"});
		degw(g, {6, 0.3f, 0.8f, 1.6f, 1.0f, 1.0f, 1.8f});
		drums(g, {"....|....|X...|....", "................", "................", "....|....|X...|....",
		          "x.x.|x.x.|x.x.|x.x.", "....|..o.|....|..o.", "................", "..o.|....|..o.|...."});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.7f; g.bass = "X.x.|..x.|x.xo|.x..";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "x...|....|..o.|....";
		g.padStyle = PS_SKANK; g.pad = "....|X...|....|X..."; g.padBase = 1.0f;
		g.fillStyle = FS_TOMS;
		add(51, g);
		g.variant = "Rockers"; g.drum[0] = "X...|....|X...|...."; g.drum[3] = "....|X...|....|X...";
		g.bpm = 80;
		add(51, g);
	}
	{ // 52 MOOMBAHTON
		Genre g;
		g.name = "Moombahton"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 108; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.35f; g.rolls = 0.15f;
		harm(g, 32, 0.6f, 16, 0.4f, 64, 0.0f);
		tpl(g, {"i bVI", "i bVII", "i iv", "i bVI bIII bVII"});
		minorW(g);
		drums(g, {"X...|X...|X...|X...", "...x|..X.|...x|..X.", "................", "...x|..x.|...x|..x.",
		          "x.x.|x.x.|x.x.|x.x.", "................", "................", "..o.|...o|..o.|...."});
		g.bassKick = 0; g.bassLen = 1; g.bassMove = 0.4f; g.slide = 0.4f; g.bass = "X..x|..X.|X..x|..X.";
		g.melStyle = MS_RIFF; g.mel = "x..x|..x.|x...|....";
		g.padStyle = PS_STABS; g.pad = "...x|..x.|...x|..x."; g.padBase = 0.6f;
		g.fillStyle = FS_ROLL; g.dropMode = DM_KICKBASS;
		add(52, g);
		g.variant = "Moombahcore"; g.bpm = 112; g.color = 0.15f; g.scale = S_PHRYG; g.complexity = 0.55f;
		add(52, g);
	}
	{ // 53 CUMBIA
		Genre g;
		g.name = "Cumbia"; g.variant = "Digital";
		g.scale = S_HARM; g.bpm = 95; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.4f; g.rolls = 0.0f;
		harm(g, 32, 0.6f, 16, 0.4f, 64, 0.0f);
		tpl(g, {"i V", "i iv V i", "i bVII bVI V", "iv i V i"});
		degw(g, {6, 0.3f, 0.6f, 1.4f, 1.8f, 1.0f, 0.8f});
		drums(g, {"X...|....|X...|....", "....|..x.|....|..x.", "................", "....|X...|....|X...",
		          "X.xx|X.xx|X.xx|X.xx", "................", "................", "..x.|.xx.|..x.|.x.x"});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.6f; g.bass = "X...|..x.|X...|..x.";
		g.melStyle = MS_LEAD; g.mel = "x.x.|..x.|x...|x.o.";
		g.padStyle = PS_SKANK; g.pad = "..x.|..x.|..x.|..x."; g.padBase = 0.7f;
		g.fillStyle = FS_TOMS; g.humanize = 0.2f;
		add(53, g);
		g.variant = "Villera"; g.bpm = 100; g.scale = S_MINOR; g.color = 0.25f; g.melStyle = MS_RIFF;
		add(53, g);
	}
	{ // 54 BOSSA NOVA
		Genre g;
		g.name = "Bossa Nova"; g.variant = "Classic";
		g.scale = S_MAJOR; g.bpm = 130; g.swing = 0.5f; g.complexity = 0.4f; g.color = 0.9f; g.jazzy = true; g.rolls = 0.0f;
		harm(g, 16, 0.5f, 32, 0.5f, 8, 0.0f);
		tpl(g, {"Imaj7 vi7 ii7 V7", "ii7 V7 Imaj7", "Imaj9 IVmaj7", "iii7 vi7 ii7 V7", "Imaj7 bIIImaj7 ii7 V7"});
		majorW(g);
		drums(g, {"X..x|....|X..x|....", "................", "................", "X..X|..X.|..X.|X...",
		          "x.x.|x.x.|x.x.|x.x.", "................", "................", "................"});
		g.bassKick = 1; g.bassLen = 1; g.bassMove = 0.2f; g.bass = "X..x|....|X..x|....";
		g.melStyle = MS_LEAD; g.mel = "x..x|..x.|....|x...";
		g.padStyle = PS_STABS; g.pad = "x..x|..x.|..x.|x..."; g.padBase = 1.0f;
		g.fillStyle = FS_SNARE; g.humanize = 0.35f;
		add(54, g);
		g.variant = "Samba"; g.bpm = 100; g.complexity = 0.55f; g.drum[7] = "x.xx|x.xx|x.xx|x.xx";
		g.drum[4] = "xxxx|xxxx|xxxx|xxxx"; g.drum[6] = "....|....|...x|.x..";
		add(54, g);
	}
	{ // 55 DRONE
		Genre g;
		g.name = "Drone"; g.variant = "Dark";
		g.scale = S_MINOR; g.bpm = 70; g.swing = 0.5f; g.complexity = 0.05f; g.color = 0.5f; g.rolls = 0.0f;
		harm(g, 128, 0.7f, 64, 0.3f, 128, 0.0f);
		tpl(g, {"i", "i bVI", "isus2 bVImaj7", "i9"});
		degw(g, {6, 0.3f, 0.6f, 1.2f, 0.8f, 1.4f, 1.0f});
		drums(g, {"................", "................", "................", "................",
		          "................", "................", "o...|....|....|....", "....|....|....|..o."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.05f; g.bass = "X...|....|....|....";
		g.melStyle = MS_SPARSE; g.mel = "o...|....|....|....";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.3f;
		add(55, g);
		g.variant = "Bright"; g.scale = S_LYDIAN; g.color = 0.8f;
		tpl(g, {"I", "Imaj7 II", "Isus2", "Imaj9"});
		majorW(g);
		add(55, g);
	}
	{ // 56 DARK AMBIENT
		Genre g;
		g.name = "Dark Ambient"; g.variant = "Classic";
		g.scale = S_PHRYG; g.bpm = 80; g.swing = 0.5f; g.complexity = 0.1f; g.color = 0.35f; g.rolls = 0.0f;
		harm(g, 64, 0.6f, 128, 0.4f, 32, 0.0f);
		tpl(g, {"i", "i bII", "i bVI", "i bII bVI"});
		degw(g, {6, 1.2f, 0.4f, 1.0f, 0.6f, 1.0f, 0.6f});
		drums(g, {"o...|....|....|....", "................", "................", "....|....|..o.|....",
		          "................", "................", "....|....|o...|....", "..o.|....|....|.o.."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.1f; g.bass = "X...|....|....|....";
		g.melStyle = MS_SPARSE; g.mel = "o...|....|..o.|....";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.2f;
		add(56, g);
		g.variant = "Ritual"; g.drum[6] = "x...|..o.|x...|...."; g.drum[7] = "..x.|.o..|..x.|o..o"; g.bpm = 85;
		g.complexity = 0.2f;
		add(56, g);
	}
	{ // 57 AMBIENT TECHNO
		Genre g;
		g.name = "Ambient Techno"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 118; g.swing = 0.5f; g.complexity = 0.25f; g.color = 0.7f; g.rolls = 0.0f;
		harm(g, 64, 0.6f, 32, 0.4f, 128, 0.0f);
		tpl(g, {"i9 bVImaj7", "i7 iv9", "bVImaj7 bIIImaj7", "i9"});
		minorW(g);
		drums(g, {"x...|x...|x...|x...", "................", "................", "....|....|..o.|....",
		          "..x.|..x.|..x.|..x.", "................", "................", "...o|....|....|.o.."});
		g.bassKick = -1; g.bassLen = 1; g.bassMove = 0.2f; g.bass = "..x.|....|..x.|....";
		g.melStyle = MS_SPARSE; g.melPenta = true; g.mel = "x...|....|..o.|....";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.2f;
		g.fillStyle = FS_ROLL;
		add(57, g);
		g.variant = "Dub"; g.padStyle = PS_STABS; g.pad = "..x.|....|.o..|...."; g.bpm = 122;
		add(57, g);
	}
	{ // 58 BERLIN SCHOOL
		Genre g;
		g.name = "Berlin School"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 110; g.swing = 0.5f; g.complexity = 0.3f; g.color = 0.5f; g.rolls = 0.0f;
		harm(g, 64, 0.6f, 128, 0.4f, 32, 0.0f);
		tpl(g, {"i", "i bVI", "i bVII", "i iv", "i bVI bVII i"});
		minorW(g);
		drums(g, {"................", "................", "................", "................",
		          "................", "................", "....|....|....|..o.", "................"});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.35f; g.bass = "XxXx|XxXx|XxXx|XxXx";
		g.melStyle = MS_ARP; g.mel = "x.xx|x.xx|x.xx|x.xx";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.1f;
		g.fillStyle = FS_TOMS;
		add(58, g);
		g.variant = "Kosmische"; g.drum[0] = "X...|X...|X...|X..."; g.drum[1] = "....|X...|....|X...";
		g.drum[4] = "x.x.|x.x.|x.x.|x.x."; g.bpm = 120; g.complexity = 0.35f;
		add(58, g);
	}
	// ================================================== DARK (2.30, after DARK-1 by Polystellarflow, GPL-3.0-only)
	{ // 59 WITCH HOUSE: slow half-time drag, rolling trap hats, a long sub, tolling chords
		Genre g;
		g.name = "Witch House"; g.variant = "Classic";
		g.scale = S_MINOR; g.bpm = 70; g.swing = 0.5f; g.complexity = 0.35f; g.color = 0.3f; g.rolls = 0.65f;
		harm(g, 64, 0.6f, 32, 0.4f, 128, 0.0f);
		tpl(g, {"i bVI", "i bII", "i bVI bVII", "i iv bII"});
		degw(g, {6, 0.3f, 0.6f, 1.2f, 0.6f, 1.6f, 1.0f});
		drums(g, {"X...|....|..x.|....", "....|....|X...|....", "................", "................",
		          "x.x.|x.xr|x.x.|xrx.", "................", "................", "....|..o.|....|o..."});
		g.bassKick = 1; g.bassLen = 2; g.bassMove = 0.1f; g.slide = 0.3f; g.bass = "X...|....|..x.|....";
		g.melStyle = MS_SPARSE; g.mel = "x...|....|....|o...";
		g.padStyle = PS_SUSTAIN; g.pad = "X..............."; g.padBase = 1.1f;
		g.fillStyle = FS_ROLL; g.humanize = 0.1f;
		add(59, g);
		g.variant = "Drag"; g.scale = S_PHRYG; g.bpm = 64; g.rolls = 0.4f; g.complexity = 0.25f;
		g.drum[0] = "X...|....|....|..x."; g.drum[4] = "x...|x...|x..r|x...";
		add(59, g);
	}
	{ // 60 DARKWAVE: post-punk drive, a running bass, gothic minor chords, a dry drum machine
		Genre g;
		g.name = "Darkwave"; g.variant = "Post-Punk";
		g.scale = S_MINOR; g.bpm = 118; g.swing = 0.5f; g.complexity = 0.3f; g.color = 0.35f; g.rolls = 0.05f;
		harm(g, 16, 0.4f, 32, 0.6f, 64, 0.0f);
		tpl(g, {"i bVII bVI bVII", "i bVI bIII bVII", "i iv", "i bVII"});
		minorW(g);
		drums(g, {"X...|..x.|X...|....", "....|X...|....|X...", "................", "................",
		          "x.x.|x.x.|x.x.|x.x.", "................", "....|....|....|..o.", "................"});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.35f; g.bass = "X.x.|X.x.|X.x.|X.x.";
		g.melStyle = MS_LEAD; g.mel = "x...|x.o.|x...|..o.";
		g.padStyle = PS_SUSTAIN; g.pad = "X..............."; g.padBase = 0.9f;
		g.fillStyle = FS_TOMS;
		add(60, g);
		g.variant = "Coldwave"; g.bpm = 126; g.color = 0.25f; g.padStyle = PS_STABS; g.pad = "x...|....|x...|...."; g.padBase = 0.7f;
		g.drum[0] = "X...|X...|X...|X..."; g.melStyle = MS_ARP; g.mel = "x.x.|x.x.|x.x.|x.x.";
		add(60, g);
	}
	{ // 61 DUNGEON SYNTH: slow minor harps over organs and choirs, almost no drums, the dominant raised
		Genre g;
		g.name = "Dungeon Synth"; g.variant = "Torchlit";
		g.scale = S_MINOR; g.bpm = 72; g.swing = 0.5f; g.complexity = 0.05f; g.color = 0.2f; g.rolls = 0.0f;
		harm(g, 64, 0.6f, 32, 0.4f, 128, 0.0f);
		tpl(g, {"i bVI bIII bVII", "i iv i V", "i bVII bVI V", "i iv bVI V"});
		minorW(g);
		drums(g, {"o...|....|....|....", "................", "................", "................",
		          "................", "................", "....|....|o...|....", "....|....|....|..o."});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.1f; g.bass = "X...|....|....|....";
		g.melStyle = MS_LEAD; g.mel = "x...|x...|x...|x.o.";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.3f;
		g.fillStyle = FS_TOMS; g.humanize = 0.25f;
		add(61, g);
		g.variant = "Winter"; g.scale = S_DORIAN; g.bpm = 64; g.color = 0.3f; g.mel = "x...|....|x...|..o.";
		add(61, g);
	}
	{ // 62 INDUSTRIAL: machine rhythms, metal hits, a driven bass, the flat second
		Genre g;
		g.name = "Industrial"; g.variant = "Machine";
		g.scale = S_PHRYG; g.bpm = 120; g.swing = 0.5f; g.complexity = 0.45f; g.color = 0.1f; g.rolls = 0.1f;
		harm(g, 64, 0.6f, 128, 0.4f, 32, 0.0f);
		tpl(g, {"i", "i bII", "i bVII", "i bII i bVII"});
		degw(g, {6, 1.4f, 0.4f, 0.8f, 0.6f, 0.8f, 1.2f});
		drums(g, {"X...|X...|X..x|X...", "....|X...|....|X...", "....|X...|....|X..x", "..x.|....|..x.|.x..",
		          "xxxx|xxxx|xxxx|xxxx", "................", "...o|....|..o.|....", "x..x|..x.|x..x|..x."});
		g.bassKick = 0; g.bassLen = 0; g.bassMove = 0.2f; g.bass = "X.X.|X.X.|X.X.|X.XX";
		g.melStyle = MS_RIFF; g.mel = "x..x|..x.|x...|x...";
		g.padStyle = PS_STABS; g.pad = "X...|....|....|...."; g.padBase = 0.4f;
		g.fillStyle = FS_CHOP;
		add(62, g);
		g.variant = "Rhythmic Noise"; g.bpm = 135; g.complexity = 0.6f; g.drum[7] = "xx.x|x.xx|.xx.|x.xx";
		g.drum[0] = "X..x|X...|X.x.|X..."; g.padBase = 0.2f;
		add(62, g);
	}
	{ // 63 HORROR: no steady pulse, dissonant scales, a tritone, sparse dry hits under a drone
		Genre g;
		g.name = "Horror"; g.variant = "Suspense";
		g.scale = S_HARM; g.bpm = 80; g.swing = 0.5f; g.complexity = 0.1f; g.color = 0.3f; g.rolls = 0.0f;
		harm(g, 64, 0.7f, 128, 0.3f, 32, 0.0f);
		tpl(g, {"i bII", "i bV", "i bVI bII", "i iv bII i"});
		degw(g, {6, 1.2f, 0.4f, 0.8f, 1.0f, 1.0f, 0.4f});
		drums(g, {"x...|....|..o.|....", "................", "................", "....|....|o...|....",
		          "................", "................", "....|..o.|....|o...", "..o.|....|.o..|...o"});
		g.bassKick = 0; g.bassLen = 2; g.bassMove = 0.1f; g.slide = 0.2f; g.bass = "X...|....|....|....";
		g.melStyle = MS_SPARSE; g.mel = "o...|..o.|....|o...";
		g.padStyle = PS_SUSTAIN; g.padBase = 1.2f;
		g.humanize = 0.2f;
		add(63, g);
		g.variant = "Cult"; g.scale = S_PHRYGDOM; g.bpm = 90; g.complexity = 0.2f;
		g.drum[6] = "x...|..o.|x...|...."; g.drum[7] = "..x.|.o..|..x.|o..o";
		add(63, g);
		g.variant = "Locrian"; g.scale = S_LOCRIAN; g.bpm = 66; g.complexity = 0.05f;
		tpl(g, {"io", "io bII", "io bV", "io bVI"});
		add(63, g);
	}
	applyFeel();
}

void initGenres() {
	static bool done = false;
	if (done)
		return;
	done = true;
	PROFILES.reserve(MAX_PROFILES);
	defineGenres();
	for (Genre& G : PROFILES) {
		G.drumLen = 16;
		for (int v = 0; v < NUM_DRUMS; v++) {
			int len = parseGrid(G.drum[v], G.pDrum[v], G.pRoll[v], 32);
			if (len == 32)
				G.drumLen = 32;
		}
		// 16-step grids repeat when another voice uses 32 steps
		for (int v = 0; v < NUM_DRUMS; v++) {
			if (G.drumLen == 32 && G.drum[v]) {
				float tmp[32];
				bool tr[32];
				if (parseGrid(G.drum[v], tmp, tr, 32) == 16)
					for (int s = 16; s < 32; s++) {
						G.pDrum[v][s] = G.pDrum[v][s - 16];
						G.pRoll[v][s] = G.pRoll[v][s - 16];
					}
			}
			G.drumUsed[v] = false;
			for (int s = 0; s < 32; s++)
				if (G.pDrum[v][s] > 0.f)
					G.drumUsed[v] = true;
		}
		parseGrid(G.bass, G.pBass, nullptr, 16);
		parseGrid(G.mel, G.pMel, nullptr, 16);
		parseGrid(G.pad, G.pPad, nullptr, 16);
		G.nTemplates = 0;
		while (G.nTemplates < 8 && G.templates[G.nTemplates])
			G.nTemplates++;
	}
	// the genres' names for the preset match (src/GenreMatch.hpp: a multi-word genre name is that genre's own)
	std::vector<std::string> names;
	for (const Genre& G : PROFILES)
		if (names.empty() || names.back() != G.name)
			names.push_back(G.name);
	genrematch::setGenreNames(names);
}

int numProfiles() { return (int)PROFILES.size(); }

const Genre& prof(int idx) {
	static Genre fallback;
	if (PROFILES.empty())
		return fallback;
	if (idx < 0 || idx >= (int)PROFILES.size())
		idx = 0;
	return PROFILES[idx];
}

int numVariants(int genre) {
	if (genre < 0 || genre >= NUM_GENRES)
		return 1;
	return std::max<int>(1, (int)VARIANTS[genre].size());
}

int profileOf(int genre, int variant) {
	if (genre < 0 || genre >= NUM_GENRES || VARIANTS[genre].empty())
		return 0;
	if (variant < 0)
		variant = 0;
	if (variant >= (int)VARIANTS[genre].size())
		variant = (int)VARIANTS[genre].size() - 1;
	return VARIANTS[genre][variant];
}

int genreOfProfile(int idx) {
	if (idx < 0 || idx >= (int)PROFILE_GENRE.size())
		return 0;
	return PROFILE_GENRE[idx];
}

int variantOfProfile(int idx) {
	int g = genreOfProfile(idx);
	for (size_t i = 0; i < VARIANTS[g].size(); i++)
		if (VARIANTS[g][i] == idx)
			return (int)i;
	return 0;
}

} // namespace genra
