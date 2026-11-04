// GENRA v2 – genre profiles (see "GENRA v2 – Genre-Spezifikation").
// Grids: one char per 16th, '|' separates beats and is ignored.
//   'X' always (anchor) · 'x' mostly (75 %) · 'o' sometimes (35 %) · 'r' roll/triplet position (75 %)
//   '1'..'9' probability n/9 · '.' never. Drum grids may be 16 or 32 steps (2 bars).
#include "Engine.hpp"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include <initializer_list>
#include <vector>
#include <algorithm>

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
};
enum { S_MAJOR, S_MINOR, S_DORIAN, S_PHRYG, S_LYDIAN, S_MIXO, S_HARM };

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
