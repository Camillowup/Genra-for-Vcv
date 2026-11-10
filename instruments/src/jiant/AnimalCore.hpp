// SPDX-License-Identifier: GPL-3.0-only
// A private copy of the ANIMAL core library (res/animalcore.*), loaded for one module instance. The firmware keeps its
// state in static variables, so a copy of the library is an engine of its own.
#pragma once
#include <string>
#include "../../../jiantcore/animalcore.h"

struct AnimalCore {
	const jan_api* api = nullptr;
	std::string error;

	AnimalCore() {}
	~AnimalCore() { unload(); }
	AnimalCore(const AnimalCore&) = delete;
	AnimalCore& operator=(const AnimalCore&) = delete;

	/** Copies the library to a file of its own and loads it. False (and `error`) when it cannot. */
	bool load();
	void unload();
	bool loaded() const { return api != nullptr; }

private:
	void* handle = nullptr;
	std::string copyPath;
};
