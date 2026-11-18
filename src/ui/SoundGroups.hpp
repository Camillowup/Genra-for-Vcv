// SPDX-License-Identifier: GPL-3.0-only
// The sound groups of the preset lists (FUNKE, SCHMELZ, UMLAUF, GOLANA): BASS, LEAD, PAD, KEYS, FX. The menus show the
// factory presets by group instead of by style; the style (genre) stays in the preset lists for Follow GENRA (NEW).
// Plain C++ without Rack (test/test_groups.cpp checks that every factory preset has a group).
#pragma once
#include <cstring>

namespace genra_groups {

static const int NUM = 5;
static const char* const ORDER[NUM] = {"BASS", "LEAD", "PAD", "KEYS", "FX"};

struct Entry {
	const char* name;
	const char* group;
};

static const Entry FUNKE[] = {
    {"CLASSIC RHODES", "KEYS"},
    {"NEO SOUL KEYS", "KEYS"},
    {"LO-FI KEYS", "KEYS"},
    {"JAZZ ORGAN", "KEYS"},
    {"VIBES", "KEYS"},
    {"M1 ORGAN", "KEYS"},
    {"GARAGE BELL", "KEYS"},
    {"DEEP CHORD STAB", "KEYS"},
    {"FILTER HOUSE STAB", "KEYS"},
    {"DUB SKANK", "KEYS"},
    {"DUB MELODICA", "LEAD"},
    {"ACID LEAD", "LEAD"},
    {"HYPNOTIC BLIP", "KEYS"},
    {"DETROIT STRINGS", "PAD"},
    {"DUB TECHNO STAB", "KEYS"},
    {"ELECTRO ZAP", "LEAD"},
    {"GRIME PULSE", "LEAD"},
    {"DRILL BELL", "KEYS"},
    {"DUBSTEP PLUCK", "KEYS"},
    {"SUPERSAW", "LEAD"},
    {"80S POLY", "PAD"},
    {"KALIMBA", "KEYS"},
    {"FLUTE SOLO", "LEAD"},
    {"AMBIENT BELLS", "KEYS"},
    {"SPACE RHODES", "KEYS"},
    {"AIR PAD", "PAD"},
    {"2-STEP STAB", "KEYS"},
    {"MINIMAL DUB BLIP", "KEYS"},
    {"ACID SQUELCH LEAD", "LEAD"},
    {"BREAKBEAT HOOVER", "LEAD"},
    {"JUNGLE RAGGA STAB", "KEYS"},
    {"LIQUID RHODES", "KEYS"},
    {"BOOM BAP KEYS", "KEYS"},
    {"TRAP BELLS", "KEYS"},
    {"TRIP-HOP RHODES", "KEYS"},
    {"DOWNTEMPO RHODES", "KEYS"},
    {"AFROBEAT ORGAN", "KEYS"},
    {"AFROBEATS PLUCK", "KEYS"},
    {"AMAPIANO KEYS", "KEYS"},
    {"BAILE STAB", "KEYS"},
    {"REGGAETON PLUCK", "KEYS"},
    {"FOOTWORK STAB", "LEAD"},
    {"BREAKCORE GLITCH", "LEAD"},
    {"UK FUNKY ORGAN", "KEYS"},
    {"BASSLINE ORGAN STAB", "KEYS"},
    {"GQOM SYNTH", "KEYS"},
    {"BIG BEAT HOOVER", "LEAD"},
    {"BREAKS STAB", "KEYS"},
    {"JERSEY BELL", "KEYS"},
    {"BALTIMORE STAB", "KEYS"},
    {"PHONK COWBELL", "KEYS"},
    {"HARDCORE HOOVER", "LEAD"},
    {"PSY ZAP", "LEAD"},
    {"EBM SEQUENCE", "LEAD"},
    {"DANCEHALL SKANK", "KEYS"},
    {"MOOMBAH LEAD", "LEAD"},
    {"CUMBIA ORGAN", "KEYS"},
    {"BOSSA VIBES", "KEYS"},
    {"DRONE FLUTE", "PAD"},
    {"BERLIN SEQUENCE", "LEAD"},
    {"DARKWAVE LEAD", "LEAD"},
    {"DUNGEON FLUTE", "LEAD"},
    {"INDUSTRIAL CLANG", "KEYS"},
    {"HORROR MUSIC BOX", "KEYS"},
    {"WITCH CHIME", "KEYS"},
};
static const int N_FUNKE = (int) (sizeof(FUNKE) / sizeof(FUNKE[0]));

static const Entry SCHMELZ[] = {
    {"SOLINA DREAM", "PAD"},
    {"CRUMAR STRINGS", "PAD"},
    {"ANALOG CELLO", "PAD"},
    {"STRING + BRASS", "PAD"},
    {"PHASED STRINGS", "PAD"},
    {"DISCO STRINGS", "PAD"},
    {"DEEP HOUSE PAD", "PAD"},
    {"GARAGE CHORDS", "KEYS"},
    {"PIANO HOUSE STAB", "KEYS"},
    {"AMBIENT CLOUD", "PAD"},
    {"LO-FI TAPE PAD", "PAD"},
    {"TRIP-HOP NOIR", "FX"},
    {"EP + STRINGS", "KEYS"},
    {"GLASS CHOIR", "PAD"},
    {"DUB TECHNO CHORD", "KEYS"},
    {"DETROIT PAD", "PAD"},
    {"DARK DRONE", "FX"},
    {"HYPNOTIC SWEEP", "PAD"},
    {"DUB ORGAN PAD", "KEYS"},
    {"GRIME STRINGS", "PAD"},
    {"DRILL CHOIR", "PAD"},
    {"SYNTHWAVE PAD", "PAD"},
    {"TRANCE SWELL", "PAD"},
    {"CINEMATIC", "PAD"},
    {"ANALOG BRASS", "PAD"},
    {"2-STEP ORGAN", "KEYS"},
    {"MINIMAL CHORD", "KEYS"},
    {"ACID SWEEP PAD", "PAD"},
    {"ELECTRO PAD", "PAD"},
    {"BREAKBEAT STRINGS", "PAD"},
    {"JUNGLE STRINGS", "PAD"},
    {"LIQUID PAD", "PAD"},
    {"BOOM BAP STRINGS", "PAD"},
    {"TRAP CHOIR", "PAD"},
    {"DOWNTEMPO PAD", "PAD"},
    {"AFROBEAT HORNS", "PAD"},
    {"AFROBEATS PAD", "PAD"},
    {"AMAPIANO PIANO", "KEYS"},
    {"BAILE BRASS STAB", "PAD"},
    {"REGGAETON PAD", "PAD"},
    {"FOOTWORK VOX PAD", "PAD"},
    {"BREAKCORE DREAD", "FX"},
    {"UK FUNKY CHORDS", "KEYS"},
    {"BASSLINE STAB", "KEYS"},
    {"GQOM DREAD", "FX"},
    {"BIG BEAT BRASS", "PAD"},
    {"BREAKS PAD", "PAD"},
    {"JERSEY VOX", "PAD"},
    {"BALTIMORE STAB", "KEYS"},
    {"PHONK CHOIR", "PAD"},
    {"HARDCORE SWELL", "PAD"},
    {"PSY SWEEP", "PAD"},
    {"EBM BRASS", "PAD"},
    {"DANCEHALL ORGAN", "KEYS"},
    {"MOOMBAH PAD", "PAD"},
    {"CUMBIA ORGAN", "KEYS"},
    {"BOSSA STRINGS", "PAD"},
    {"DRONE CHOIR", "PAD"},
    {"BERLIN STRINGS", "PAD"},
    {"DARKWAVE STRINGS", "PAD"},
    {"DUNGEON CHOIR", "PAD"},
    {"INDUSTRIAL HUM", "FX"},
    {"HORROR STRINGS", "PAD"},
    {"WITCH CHOIR", "PAD"},
};
static const int N_SCHMELZ = (int) (sizeof(SCHMELZ) / sizeof(SCHMELZ[0]));

static const Entry UMLAUF[] = {
    {"HYPNO STAB", "KEYS"},
    {"WAREHOUSE DRONE", "FX"},
    {"ACID ORBIT", "LEAD"},
    {"RAVE CHORD", "KEYS"},
    {"DEEP CHORD", "KEYS"},
    {"ORGAN HOUSE", "KEYS"},
    {"GARAGE PLUCK", "KEYS"},
    {"DISCO STRINGS", "KEYS"},
    {"DUB TECHNO CHORD", "KEYS"},
    {"DUB SKANK", "KEYS"},
    {"MELODICA", "KEYS"},
    {"SPACE ECHO", "FX"},
    {"GLACIER PAD", "PAD"},
    {"TIDAL DRONE", "FX"},
    {"CHOIR CLOUD", "PAD"},
    {"TEXTURE FIELD", "FX"},
    {"SUB ORBIT", "BASS"},
    {"REESE ORBIT", "BASS"},
    {"FM BASS", "BASS"},
    {"WOBBLE ORBIT", "BASS"},
    {"ORBIT E-PIANO", "KEYS"},
    {"SYNC LEAD", "LEAD"},
    {"CHIP ARP", "LEAD"},
    {"BELL TREE", "KEYS"},
    {"LORENZ STORM", "FX"},
    {"BOIDS SWARM", "FX"},
    {"THUNDER RISER", "FX"},
    {"DARK HORIZON", "FX"},
};
static const int N_UMLAUF = (int) (sizeof(UMLAUF) / sizeof(UMLAUF[0]));

static const Entry GOLANA[] = {
    {"WARM POLY KEYS", "KEYS"},
    {"LO-FI CHORDS", "KEYS"},
    {"SOFT SQUARE LEAD", "LEAD"},
    {"DUSTY BELLS", "KEYS"},
    {"HOUSE ORGAN", "KEYS"},
    {"DEEP CHORD", "KEYS"},
    {"GARAGE STAB", "KEYS"},
    {"DISCO BRASS", "PAD"},
    {"DUB SIREN", "FX"},
    {"DUB CHORD ECHO", "KEYS"},
    {"MELODICA", "KEYS"},
    {"ACID LINE", "LEAD"},
    {"HYPNOTIC ARP", "LEAD"},
    {"DUB TECHNO CHORD", "KEYS"},
    {"ELECTRO SYNC LEAD", "LEAD"},
    {"MINIMAL BLIP", "LEAD"},
    {"GRIME SQUARE", "LEAD"},
    {"REESE", "BASS"},
    {"DRILL GLIDE", "LEAD"},
    {"WOBBLE LEAD", "LEAD"},
    {"SUPERSAW", "LEAD"},
    {"80S BRASS", "PAD"},
    {"SYNTHWAVE POLY", "KEYS"},
    {"GLIDE LEAD", "LEAD"},
    {"TRANCE PLUCK", "KEYS"},
    {"UNISON LEAD", "LEAD"},
    {"SOFT PAD", "PAD"},
    {"PHASE PAD", "PAD"},
    {"SLOW ARP", "LEAD"},
    {"DRONE", "FX"},
    {"FAT BASS", "BASS"},
    {"UNISON SAW", "LEAD"},
    {"SYNC PAD", "PAD"},
    {"BRIGHT ARP", "LEAD"},
    {"ORGAN", "KEYS"},
    {"SOFT BELLS", "KEYS"},
    {"WARM LEAD", "LEAD"},
    {"DARK UNISON", "LEAD"},
    {"S&H TEXTURE", "FX"},
    {"SUB BASS", "BASS"},
    {"POLY CHORUS", "KEYS"},
    {"OLD STRINGS", "PAD"},
    {"WORN TAPE", "FX"},
    {"DRIFT LEAD", "LEAD"},
    {"SUB ARP", "BASS"},
    {"AGED PAD", "PAD"},
};
static const int N_GOLANA = (int) (sizeof(GOLANA) / sizeof(GOLANA[0]));

/** The group of a preset by its name in one of the tables above, or nullptr. */
inline const char* find(const Entry* t, int n, const char* name) {
	for (int i = 0; i < n; i++)
		if (!std::strcmp(t[i].name, name))
			return t[i].group;
	return nullptr;
}

/** VADER's presets in UMLAUF (their names come from the engine): by words in the name, PAD when none fits. */
inline const char* vader(const char* name) {
	static const char* const RULES[][2] = {{"DRONE", "FX"}, {"ATMOS", "FX"}, {"TEXTURE", "FX"}, {"SCOPE", "FX"},
	                                     {"ARP", "LEAD"}, {"PWM", "LEAD"}, {"FOLD", "LEAD"}, {"BELL", "KEYS"},
	                                     {"ORGAN", "KEYS"}, {"METAL", "KEYS"}, {"FM ", "KEYS"}, {"PLAY", "KEYS"}};
	for (auto& r : RULES)
		if (std::strstr(name, r[0]))
			return r[1];
	return "PAD";
}

/** The group of a preset, with a fallback (KEYS) for a name not in the table. */
inline const char* of(const Entry* t, int n, const char* name) {
	const char* g = find(t, n, name);
	return g ? g : "KEYS";
}

}  // namespace genra_groups
