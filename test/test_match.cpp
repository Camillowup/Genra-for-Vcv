// SPDX-License-Identifier: GPL-3.0-only
// The "same genre" match (src/GenreMatch.hpp) against GENRA's genres: every genre finds at least one preset in
// GRUND, FUNKE, SCHMELZ and GENRA MONO, by its whole name where a preset carries it.
// g++ -std=gnu++17 -O2 -Isrc test/test_match.cpp src/Engine.cpp src/Genres.cpp src/Edit.cpp src/Store.cpp src/Midi.cpp
#include "Engine.hpp"
#include "GenreMatch.hpp"
#include "grund/GrundPresets.hpp"
#include "funke/FunkePresets.hpp"
#include "schmelz/SchmelzPresets.hpp"
#include "mono/MonoPresets.hpp"
#include <cstdio>

using namespace genra;
static int fails = 0;

template <typename P>
static void cover(const char* inst, const P* presets, int n) {
	for (int g = 0; g < numProfiles(); g++) {
		std::string name = prof(g).name;
		auto c = genrematch::candidates(n, [&](int i) { return presets[i].style; }, name);
		if (c.empty()) {
			fails++;
			std::printf("FAIL %s: no preset for %s\n", inst, name.c_str());
		}
	}
}

int main() {
	initGenres();
	cover("GRUND", ggrund::PRESETS, ggrund::NUM_PRESETS);
	cover("FUNKE", gfunke::PRESETS, gfunke::NUM_PRESETS);
	cover("SCHMELZ", gschmelz::PRESETS, gschmelz::NUM_PRESETS);
	cover("MONO", gmono::PRESETS, gmono::NUM_PRESETS);
	using genrematch::score;
	if (score("WITCH HOUSE", "House") != 0) fails++, std::printf("FAIL Witch House counted as House\n");
	if (score("LO-FI · CHILL", "Lo-Fi") != 2) fails++, std::printf("FAIL Lo-Fi whole name\n");
	if (score("HOUSE · GARAGE", "UK Garage") == 0) fails++, std::printf("FAIL Garage word\n");
	std::printf("%d genres, %d failures\n", numProfiles(), fails);
	return fails ? 1 : 0;
}
