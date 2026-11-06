// SPDX-License-Identifier: GPL-3.0-only
// UMLAUF FX's parameters and inputs, shared with UMLAUF (which reads them when the expander is docked to its right).
#pragma once
#include "../plugin.hpp"

namespace ufx {

enum ParamId {
	ATK, DEC, SUS, REL,
	UNISON, VOICES, DETUNE, GLIDE, TOCT, VELO, VSPD, VDEP, TSPD, TDEP,
	ARPON, LATCH, AMODE, ARATE, AOCT, ALEN, ASTEPS, AHUM,
	STEP1, STEP16 = STEP1 + 15,
	DRIVE, CHORUS, CHSPD, DELAY, DTIME, DFB, REVERB, RSIZE, MRES, VOLUME,
	NUM_PARAMS
};
enum InputId {
	IN_ATK, IN_DEC, IN_SUS, IN_REL, IN_DETUNE, IN_GLIDE, IN_VIB, IN_TREM, IN_ARPON, IN_ARATE,
	IN_LATCH, IN_AOCT, IN_DRIVE, IN_CHORUS, IN_DELAY, IN_DTIME, IN_REVERB, IN_RSIZE, IN_MRES, IN_VOL,
	NUM_INPUTS
};
enum LightId { ARPON_LIGHT, LATCH_LIGHT, LINK_LIGHT, NUM_LIGHTS };

}  // namespace ufx

