// SPDX-License-Identifier: GPL-3.0-only
// A private copy of the SPELL core library (res/spellcore.*: SPELLBOX's speech chip, VOICE's fifth language), loaded
// for one module instance. The chip keeps its state in static variables, so a copy of the library is an engine of its own.
#pragma once
#include <string>
#include "../../../jiantcore/spellcore.h"

struct SpellCore {
	const jsp_api* api = nullptr;
	std::string error;

	SpellCore() {}
	~SpellCore() { unload(); }
	SpellCore(const SpellCore&) = delete;
	SpellCore& operator=(const SpellCore&) = delete;

	/** Copies the library to a file of its own and loads it. False (and `error`) when it cannot. */
	bool load();
	void unload();
	bool loaded() const { return api != nullptr; }

private:
	void* handle = nullptr;
	std::string copyPath;
};
