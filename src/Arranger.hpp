// GENRA – song parts for the arranger ABLAUF: a stored pattern (slot) and how one is turned into an intro, a build, ...
// Pure C++, no Rack dependency (tested in test/test_ablauf.cpp).
#pragma once
#include "Engine.hpp"

namespace genra {

enum Section { SEC_INTRO, SEC_VERSE, SEC_BUILD, SEC_DROP, SEC_BREAK, SEC_OUTRO, NUM_SECTIONS };
extern const char* const SECTION_NAMES[NUM_SECTIONS];

static const int LENGTH_BARS[4] = {1, 2, 4, 8};

/** Offsets that travel with a stored pattern and are added to GENRA's knobs while it plays (per-track DENSITY,
 * the drums' GHOST and FILL). Tracks a song part bakes into exact notes need none. */
struct ArrMods {
	float dens[NUM_TRACKS] = {0.f, 0.f, 0.f, 0.f};
	float ghost = 0.f, fill = 0.f;
	bool operator==(const ArrMods& o) const {
		for (int t = 0; t < NUM_TRACKS; t++)
			if (dens[t] != o.dens[t])
				return false;
		return ghost == o.ghost && fill == o.fill;
	}
};

/** One pattern place of ABLAUF: GENRA's loop with the genre of every track, its length and the part's offsets. */
struct Slot {
	Pattern pat;
	int8_t sel[NUM_TRACKS] = {-1, -1, -1, -1}; // genre field per track (-1 = follows GENRE)
	int8_t lengthIdx = 2;                      // index into LENGTH_BARS
	ArrMods mods;
	int8_t tag = -1;                           // Section, -1 = none yet
	char name[9] = {};                         // the user's name for it (ABLAUF), "" = none
	bool used = false;
	Slot() { initPattern(pat); }
};

/** How a part's bass is made, when the bass is re-rolled. -1: a style at random on every press. */
enum BreakBass { BB_ROOTS, BB_THIN, BB_PULSE, BB_DUB, BB_OUT, NUM_BREAK_BASS };
enum IntroBass { IB_ROOTS, IB_THIN, IB_PULSE, NUM_INTRO_BASS };
extern const char* const BREAK_BASS_NAMES[NUM_BREAK_BASS];
extern const char* const INTRO_BASS_NAMES[NUM_INTRO_BASS];
struct PartOpts {
	int breakBass = -1;
	int introBass = -1;
};

/** Make a song part from src.
 * reroll: bit per track (1 << T_DRUMS ...): these tracks get new material in the part's style (same genre as in src),
 * the others are copied exactly, offsets included. P: GENRA's knobs without any arranger offsets.
 * The result is tagged with the part. */
void makeSection(const Slot& src, Slot& out, int section, unsigned reroll, const Params& P, Rng& rng,
                 const PartOpts& opts = PartOpts());

/** Params as GENRA plays a slot: P (the knobs) plus the slot's offsets. */
Params withMods(const Params& P, const ArrMods& m);

} // namespace genra
