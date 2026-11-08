// SPDX-License-Identifier: GPL-3.0-only
// GENRA FX – the send effects for MIX: REVERB on IN A → OUT A (MIX's send A), DELAY on IN B → OUT B (send B).
// Without cables: the tempo comes from GENRA, IN A/B take MIX's SEND A/B (and SCHLAG's) and MIX takes OUT A/B into RETURN A/B
// (Follow.hpp's virtual cables; a real cable always wins). DSP: GenraFxDsp.hpp. Panel: tools/genrafx_panel.py.
#pragma once
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "GenraFxDsp.hpp"
#include <atomic>

namespace genrafx {

// the delay's time field: three ways to divide the beat, or free in ms
enum TimeMode { T_STRAIGHT, T_DOTTED, T_TRIPLET, T_FREE, NUM_TIME_MODES };

struct GenraFx : Module {
	enum ParamId {
		V_TYPE_PARAM, V_DECAY_PARAM, V_SIZE_PARAM, V_PRE_PARAM, V_DAMP_PARAM, V_WIDTH_PARAM, V_SHIM_PARAM, V_LVL_PARAM,
		V_FREEZE_PARAM,
		D_MODE_PARAM, D_TIME_PARAM, D_FB_PARAM, D_TONE_PARAM, D_TAPE_PARAM, D_DUCK_PARAM, D_WIDTH_PARAM, D_LVL_PARAM,
		D_PING_PARAM,
		NUM_PARAMS
	};
	enum InputId { IN_AL, IN_AR, CV_DECAY, CV_FREEZE, IN_BL, IN_BR, CV_TIME, CV_FB, NUM_INPUTS };
	enum OutputId { OUT_AL, OUT_AR, OUT_BL, OUT_BR, NUM_OUTPUTS };
	enum LightId { FREEZE_LIGHT, NUM_LIGHTS };

	gfx::Reverb rv;
	gfx::Delay dl;
	int ctl = 0;
	float sr = 48000.f;

	// ---- without cables
	std::atomic<bool> followGenra{true};
	std::atomic<int> inMode[2] = {{1}, {1}}; // 0 cables only, 1 auto: MIX's SEND A / SEND B
	float freeBpm = 120.f;
	follow::Ref genraRef, mixRef, schlagRef; // (2.26: SCHLAG's SEND A / B add to MIX's)
	follow::Drive drv[4];               // IN_AL, IN_AR, IN_BL, IN_BR
	float uiTimer = 0.f;
	std::atomic<float> bpmNow{120.f};
	std::atomic<bool> synced{false};
	std::atomic<int64_t> returnPulledAt[2] = {{-1000000}, {-1000000}}; // MIX takes OUT A / OUT B (engine frame)
	int64_t frameNow = 0;
	bool returnPulled(int k) const { return frameNow - returnPulledAt[k].load(std::memory_order_relaxed) < 4096; }

	// meters (for the display)
	std::atomic<float> meter[2] = {{0.f}, {0.f}};
	float env[2] = {0.f, 0.f};

	GenraFx();
	void onAdd(const AddEvent& e) override;
	void onSampleRateChange(const SampleRateChangeEvent& e) override;
	void onReset(const ResetEvent& e) override;
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }
	void onPortChange(const PortChangeEvent& e) override;
	void process(const ProcessArgs& args) override;
	json_t* dataToJson() override;
	void dataFromJson(json_t* root) override;

	/** UI thread: find GENRA and MIX. */
	void uiStep(float dt);
	void feedInputs();
	void applyParams();
	int timeMode() { return clamp((int) std::round(params[D_MODE_PARAM].getValue()), 0, NUM_TIME_MODES - 1); }
	/** The delay's division (0..5: 1/32 .. 1/1) or, in FREE, the time in ms, from the TIME knob (+ CV). */
	int divIndex(float v) const { return clamp((int) std::round(v * (gfx::NUM_DIVS - 1)), 0, gfx::NUM_DIVS - 1); }
	static float freeMs(float v) { return 10.f * std::pow(200.f, clamp(v, 0.f, 1.f)); } // 10 ms .. 2 s
	float timeKnob();
};

bool isFx(Module* m);
/** MIX's outputs that feed IN A / IN B (keep in step with src/mix/Mix.cpp: SEND_L/R, SENDB_L/R). */
static const int MIX_SEND_OUT[2][2] = {{0, 1}, {4, 5}};

} // namespace genrafx
