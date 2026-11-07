// SPDX-License-Identifier: GPL-3.0-only
// GRUND FX, the expander of GRUND: its ids and the module (shared with GRUND, which reads its knobs each control
// block, writes them when a preset loads, and shows the compressor's gain reduction on it).
#pragma once
#include "../plugin.hpp"
#include "GrundDsp.hpp"
#include <atomic>

namespace gfx {

enum ParamId {
	COMP_ON, C_THRESH, C_RATIO, C_ATK, C_REL,
	DRIVE_ON, D_TYPE, D_TONE, D_MIX,
	EQ_ON, E_HPF, E_LOW, E_MID, E_HIGH,
	CHO_ON, CH_RATE, CH_DEPTH,
	VERB_ON, V_SIZE, V_DAMP, V_PRE,
	NUM_PARAMS
};
enum InputId { IN_SC, IN_COMP, IN_DRIVE, IN_EQLO, IN_EQHI, IN_CHO, IN_VERB, NUM_INPUTS };
static const int NUM_ON = 5;
static const int ON_PARAMS[NUM_ON] = {COMP_ON, DRIVE_ON, EQ_ON, CHO_ON, VERB_ON};
enum LightId { ON_LIGHT, LINK_LIGHT = ON_LIGHT + NUM_ON, NUM_LIGHTS };

struct GrundFx : Module {
	std::atomic<float> gr{0.f};       // the compressor's gain reduction 0..1, from GRUND
	std::atomic<bool> linked{false};  // GRUND reads it
	bool choHi = true, verbHi = true;  // only the highs into chorus / reverb (context menu)

	GrundFx();
	void process(const ProcessArgs& args) override;
	json_t* dataToJson() override;
	void dataFromJson(json_t* j) override;
};

/** The knobs (without CV) into f. */
inline void readKnobs(GrundFx* x, ggrund::FxSettings& f) {
	auto v = [&](int p) { return x->params[p].getValue(); };
	f.compOn = v(COMP_ON) > 0.5f, f.thresh = v(C_THRESH), f.ratio = v(C_RATIO), f.cAtk = v(C_ATK), f.cRel = v(C_REL);
	f.driveOn = v(DRIVE_ON) > 0.5f, f.dType = clamp((int) std::round(v(D_TYPE)), 0, ggrund::NUM_DRIVES - 1);
	f.dTone = v(D_TONE), f.dMix = v(D_MIX);
	f.eqOn = v(EQ_ON) > 0.5f, f.hpf = v(E_HPF), f.low = v(E_LOW), f.mid = v(E_MID), f.high = v(E_HIGH);
	f.choOn = v(CHO_ON) > 0.5f, f.choRate = v(CH_RATE), f.choDepth = v(CH_DEPTH);
	f.verbOn = v(VERB_ON) > 0.5f, f.vSize = v(V_SIZE), f.vDamp = v(V_DAMP), f.vPre = v(V_PRE);
	f.choHi = x->choHi, f.verbHi = x->verbHi;
}
/** The CV inputs on top (0–10 V = the whole range; EQ ±5 V = ±12 dB). */
inline void addCv(GrundFx* x, ggrund::FxSettings& f) {
	auto cv = [&](int i) { return x->inputs[i].getVoltage() * 0.1f; };
	f.thresh = clamp(f.thresh + cv(IN_COMP), 0.f, 1.f);
	f.dMix = clamp(f.dMix + cv(IN_DRIVE), 0.f, 1.f);
	f.low = clamp(f.low + cv(IN_EQLO), 0.f, 1.f);
	f.high = clamp(f.high + cv(IN_EQHI), 0.f, 1.f);
	f.choDepth = clamp(f.choDepth + cv(IN_CHO), 0.f, 1.f);
	f.vSize = clamp(f.vSize + cv(IN_VERB), 0.f, 1.f);
}
/** f onto the knobs (a preset loads). */
inline void writeKnobs(GrundFx* x, const ggrund::FxSettings& f) {
	auto s = [&](int p, float v) { x->params[p].setValue(v); };
	s(COMP_ON, f.compOn), s(C_THRESH, f.thresh), s(C_RATIO, f.ratio), s(C_ATK, f.cAtk), s(C_REL, f.cRel);
	s(DRIVE_ON, f.driveOn), s(D_TYPE, (float) f.dType), s(D_TONE, f.dTone), s(D_MIX, f.dMix);
	s(EQ_ON, f.eqOn), s(E_HPF, f.hpf), s(E_LOW, f.low), s(E_MID, f.mid), s(E_HIGH, f.high);
	s(CHO_ON, f.choOn), s(CH_RATE, f.choRate), s(CH_DEPTH, f.choDepth);
	s(VERB_ON, f.verbOn), s(V_SIZE, f.vSize), s(V_DAMP, f.vDamp), s(V_PRE, f.vPre);
	x->choHi = f.choHi, x->verbHi = f.verbHi;
}

}  // namespace gfx

extern Model* modelGrundFx;
extern Model* modelGrund;
