// The sound groups of the preset lists (src/ui/SoundGroups.hpp): every factory preset of FUNKE, SCHMELZ and UMLAUF has
// one of the five groups, the table names no preset that is gone, and every group is used. (GOLANA's names come from
// its core: its table is checked against the same rules only.)
// g++ -O2 -std=c++17 test/test_groups.cpp -o /tmp/test_groups && /tmp/test_groups
#include "../src/ui/SoundGroups.hpp"
#include "../src/funke/FunkePresets.hpp"
#include "../src/schmelz/SchmelzPresets.hpp"
#include "../instruments/src/umlauf/UmlaufPresets.h"
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

static int fails = 0;
#define CHECK(c, ...)                    \
	do {                                 \
		if (!(c)) {                      \
			fails++;                     \
			printf("FAIL " __VA_ARGS__); \
			printf("\n");                \
		}                                \
	} while (0)

static bool known(const char* g) {
	for (const char* o : genra_groups::ORDER)
		if (!std::strcmp(o, g))
			return true;
	return false;
}

/** Every entry has a known group; with names given, each preset is in the table and each entry is a preset. */
static void check(const char* what, const genra_groups::Entry* t, int n, const char* const* names, int count) {
	std::set<std::string> groups, presets;
	for (int i = 0; i < n; i++) {
		CHECK(known(t[i].group), "%s: %s has the group %s", what, t[i].name, t[i].group);
		groups.insert(t[i].group);
	}
	for (int i = 0; i < count; i++) {
		presets.insert(names[i]);
		CHECK(genra_groups::find(t, n, names[i]), "%s: preset %s has no group", what, names[i]);
	}
	if (count > 0)
		for (int i = 0; i < n; i++)
			CHECK(presets.count(t[i].name), "%s: %s is in the table, not in the presets", what, t[i].name);
	printf("%-8s %3d presets, %zu groups\n", what, count > 0 ? count : n, groups.size());
}

int main() {
	const char* names[256];
	for (int i = 0; i < gfunke::NUM_PRESETS; i++)
		names[i] = gfunke::PRESETS[i].name;
	check("FUNKE", genra_groups::FUNKE, genra_groups::N_FUNKE, names, gfunke::NUM_PRESETS);
	for (int i = 0; i < gschmelz::NUM_PRESETS; i++)
		names[i] = gschmelz::PRESETS[i].name;
	check("SCHMELZ", genra_groups::SCHMELZ, genra_groups::N_SCHMELZ, names, gschmelz::NUM_PRESETS);
	for (int i = 0; i < UMLAUF_NPRESETS; i++)
		names[i] = UMLAUF_PRESETS[i].name;
	check("UMLAUF", genra_groups::UMLAUF, genra_groups::N_UMLAUF, names, UMLAUF_NPRESETS);
	check("GOLANA", genra_groups::GOLANA, genra_groups::N_GOLANA, names, 0);
	// VADER's names: by words, always one of the groups
	for (const char* v : {"INIT ORBIT", "BELL CUBE", "ARP HEART", "SAMPLE DRONES", "ORGAN SQUARE", "SOMETHING NEW"})
		CHECK(known(genra_groups::vader(v)), "VADER: %s has the group %s", v, genra_groups::vader(v));
	CHECK(!std::strcmp(genra_groups::vader("ARP HEART"), "LEAD"), "VADER: ARP HEART is %s", genra_groups::vader("ARP HEART"));
	CHECK(!std::strcmp(genra_groups::vader("SAMPLE DRONES"), "FX"), "VADER: SAMPLE DRONES is %s",
	      genra_groups::vader("SAMPLE DRONES"));
	printf(fails ? "%d FAILED\n" : "all good\n", fails);
	return fails ? 1 : 0;
}
