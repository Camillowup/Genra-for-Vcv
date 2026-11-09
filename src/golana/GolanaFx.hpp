// SPDX-License-Identifier: GPL-3.0-only
// GOLANA FX, the effects expander: its ids, shared with GOLANA (which reads its knobs and writes the firmware).
#pragma once
#include "../plugin.hpp"

namespace glfx {

enum ParamId {
	MFX_ON, MFX_TYPE, CRATE, CDEPTH,
	DLY_ON, DTYPE, DTIME, DFDBK, DCOLOR, DMIX,
	REV_ON, RTYPE, RSIZE, RDAMP,
	ROUTE,
	NUM_PARAMS
};
enum InputId { IN_CRATE, IN_CDEPTH, IN_DFDBK, IN_DCOLOR, IN_DMIX, IN_RSIZE, IN_RDAMP, IN_ROUTE, NUM_INPUTS };
/** The ON buttons' lights, in the order of ON_PARAMS, then LINK. */
static const int ON_PARAMS[3] = {MFX_ON, DLY_ON, REV_ON};
enum LightId { ON_LIGHT, LINK_LIGHT = ON_LIGHT + 3, NUM_LIGHTS };

// the firmware's names (eng_golana.c N_GMFX, N_GDTYPE, N_GDTIME, N_GRTYPE, N_GROUTE)
static const char* const MFX_NAMES[6] = {"OFF", "CHORUS", "FLANGER", "ENSEMBLE", "PHASER", "METAL"};
static const char* const DTYPE_NAMES[3] = {"STEREO", "CROSS", "L/R"};
static const char* const DTIME_NAMES[13] = {"1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/2",
                                            "1/1", "2 BAR", "4 BAR", "1/8D", "1/16D", "1/4D"};
static const char* const RTYPE_NAMES[3] = {"ROOM", "SPRING", "HALL"};
static const char* const ROUTE_NAMES[3] = {"MFX › DLY › REV", "DLY › MFX › REV", "DLY › REV › MFX"};

}  // namespace glfx

extern Model* modelGolanaFx;
extern Model* modelGolana;
/** The firmware's text for the value of the expander's knob fxParam, from a GOLANA module (false: m is none). */
bool golanaFxText(Module* golana, int fxParam, char* out, int n);
