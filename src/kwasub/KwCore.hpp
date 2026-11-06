// SPDX-License-Identifier: GPL-3.0-only
// K-WASUB: loads a private copy of the core library (res/kwcore.*) for one module instance, as GENRA's JIANT voices do:
// the firmware keeps its state in static variables, so every instance needs its own copy.
#pragma once
#include "../plugin.hpp"
#include "../../kwcore/kwcore.h"

struct KwCore {
	const kw_api* api = nullptr;
	std::string error;
	bool load();
	void unload();
	~KwCore() { unload(); }

  private:
	void* handle = nullptr;
	std::string copyPath;
};
