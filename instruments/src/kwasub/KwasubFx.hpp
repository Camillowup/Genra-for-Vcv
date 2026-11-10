// SPDX-License-Identifier: GPL-3.0-only
// K-WASUB FX, the effects expander: its ids, shared with K-WASUB (which reads its knobs and writes the firmware).
#pragma once
#include "../plugin.hpp"

namespace kwfx {

enum ParamId {
	DR_ON, DR_TYPE, DR_AMT, DR_MIX,
	CR_ON, CR_BITS, CR_RATE, CR_MIX,
	CH_ON, CH_RATE, CH_DEP, CH_MIX,
	PH_ON, PH_RATE, PH_DEP, PH_FB, PH_MIX,
	DL_ON, DL_TYPE, DL_TIME, DL_FB, DL_TONE, DL_MIX,
	RV_ON, RV_TYPE, RV_SIZE, RV_DAMP, RV_MIX,
	CP_ON, CP_AMT, CP_ATK, CP_REL, CP_PUMP, CP_PRATE,
	OT_ON, OT_AMT, OT_LOW, OT_HIGH,
	EQ_ON, EQ1_F, EQ1_G, EQ2_F, EQ2_G, EQ3_F, EQ3_G, EQ4_F, EQ4_G,
	MS_GLUE, MS_SUB, MB_MONO,
	NUM_PARAMS
};
enum InputId {
	IN_SAT, IN_CRUSH, IN_CHORUS, IN_PHASER, IN_DTIME, IN_DFB, IN_DMIX, IN_VSIZE, IN_VMIX, IN_COMP, IN_PUMP, IN_OTT,
	IN_EQLO, IN_EQHI, IN_GLUE, IN_SUBB,
	NUM_INPUTS
};
/** The ON buttons' lights, in the order of ON_PARAMS, then LINK. */
static const int ON_PARAMS[9] = {DR_ON, CR_ON, CH_ON, PH_ON, DL_ON, RV_ON, CP_ON, OT_ON, EQ_ON};
enum LightId { ON_LIGHT, LINK_LIGHT = ON_LIGHT + 9, NUM_LIGHTS };

}  // namespace kwfx

extern Model* modelKwasubFx;
extern Model* modelKwasub;
/** The firmware's text for a patch id, from a K-WASUB module (false: m is none). */
bool kwasubText(Module* m, int fw, char* out, int n);
