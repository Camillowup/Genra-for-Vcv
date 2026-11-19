#pragma once
// Preset select for MIDI-MAP (2.41.1): every instrument with a preset field has one more param after its knobs
// (PRESETSEL_PARAM = NUM_PARAMS, so the presets and the loops over the knobs never see it). The preset field is a
// ParamWidget for it: MIDI-MAP learns it with a click on the field. A knob mapped to it goes through the list (in the
// order of the mouse wheel); the field shows the name at once, the preset loads when the knob rests for 0.3 s.
// Values the code sets (a patch, undo, a user preset: the module's JSON) do not load anything: the module bumps a
// counter in dataFromJson.
#include <rack.hpp>
#include <cmath>

namespace genra_presel {
using namespace rack;

inline void config(engine::Module* m, int pid) {
	m->configParam(pid, 0.f, 1.f, 0.f,
	               "Preset select (for MIDI-MAP: a knob goes through the list, the preset loads when it rests)", " %", 0.f,
	               100.f);
	m->paramQuantities[pid]->randomizeEnabled = false;
	m->paramQuantities[pid]->resetEnabled = false;
}

/** UI side: watches the param, says which preset to load (and which one to show while the knob still moves). */
struct Watch {
	float seen = -1.f;
	uint32_t seenCode = 0;
	double movedAt = 0.0;
	int want = -1;  // the preset the knob points at, not loaded yet
	/** The preset to load now, or -1. count: the presets there are; code: the module's JSON-load counter. */
	int step(engine::Module* m, int pid, uint32_t code, int count) {
		if (!m || pid < 0 || pid >= (int) m->params.size() || count <= 0)
			return -1;
		float v = m->params[pid].getValue();
		if (code != seenCode || seen < 0.f) {  // set by the code: take it as it is
			seenCode = code;
			seen = v;
			want = -1;
			return -1;
		}
		double now = system::getTime();
		if (std::fabs(v - seen) > 1e-5f) {
			seen = v;
			want = clamp((int) std::round(v * (count - 1)), 0, count - 1);
			movedAt = now;
		}
		if (want >= 0 && now - movedAt >= 0.3) {
			int w = want;
			want = -1;
			return w;
		}
		return -1;
	}
};

}  // namespace genra_presel
