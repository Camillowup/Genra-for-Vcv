// SPDX-License-Identifier: GPL-3.0-only
// GOLANA: loads a private copy of the core library (res/glcore.*) for one module instance, as K-WASUB and the JIANT voices do:
// the firmware keeps its state in static variables, so every instance needs its own copy.
#pragma once
#include "../plugin.hpp"
#include "../../glcore/glcore.h"

struct GlCore {
	const gl_api* api = nullptr;
	std::string error;
	bool load();
	void unload();
	~GlCore() { unload(); }

  private:
	void* handle = nullptr;
	std::string copyPath;
};
