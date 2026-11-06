// SPDX-License-Identifier: GPL-3.0-only
// UMLAUF FX – the expander of UMLAUF, in the GENRA look: envelope, voice (unison, glide, vibrato, tremolo),
// the arpeggiator with its 16-step pattern, the effects and the master. It holds knobs only: UMLAUF (to its left)
// reads them and writes VADER's engine; when it docks, its knobs take the sound's values.
// Panel: tools/umlauf_fx_panel.py (res/UmlaufFx.svg + UmlaufFxLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "UmlaufFx.hpp"
#include "UmlaufFxLayout.hpp"
#include "VaderApi.hpp"

namespace genra_umlauffx {

namespace L = uflayout;
namespace ui = genra_ui;
using namespace ufx;

#define UMLAUF_RGB 0xff, 0x2e, 0x43
static const NVGcolor ACCENT = nvgRGB(UMLAUF_RGB);

struct UmlaufFx : Module {
	UmlaufFx() {
		config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
		auto pct = [&](int p, float def, const char* n) { configParam(p, 0.f, 1.f, def, n, "%", 0.f, 100.f); };
		pct(ATK, 0.05f, "Attack");
		pct(DEC, 0.4f, "Decay");
		pct(SUS, 0.7f, "Sustain");
		pct(REL, 0.35f, "Release");
		configSwitch(UNISON, 0.f, 3.f, 0.f, "Unison voices per note", {"1", "2", "4", "8"});
		configSwitch(VOICES, 0.f, 1.f, 0.f, "Voices", {"8", "16"});
		pct(DETUNE, 0.2f, "Unison detune");
		pct(GLIDE, 0.f, "Glide");
		configParam(TOCT, -2.f, 2.f, 0.f, "Octave");
		paramQuantities[TOCT]->snapEnabled = true;
		pct(VELO, 0.5f, "Velocity sensitivity");
		pct(VSPD, 0.4f, "Vibrato rate");
		pct(VDEP, 0.f, "Vibrato depth");
		pct(TSPD, 0.4f, "Tremolo rate");
		pct(TDEP, 0.f, "Tremolo depth");
		configSwitch(ARPON, 0.f, 1.f, 0.f, "Arpeggiator", {"Off", "On"});
		configSwitch(LATCH, 0.f, 1.f, 0.f, "Latch (the notes keep playing)", {"Off", "On"});
		configSwitch(AMODE, 0.f, 5.f, 0.f, "Arpeggiator direction",
		             std::vector<std::string>(vader::ARP_MODES, vader::ARP_MODES + 6));
		configSwitch(ARATE, 0.f, 5.f, 3.f, "Arpeggiator rate (on the clock)",
		             std::vector<std::string>(vader::ARP_RATES, vader::ARP_RATES + 6));
		configParam(AOCT, 1.f, 4.f, 1.f, "Arpeggiator octaves");
		paramQuantities[AOCT]->snapEnabled = true;
		pct(ALEN, 0.5f, "Note length");
		configParam(ASTEPS, 1.f, 16.f, 16.f, "Pattern steps");
		paramQuantities[ASTEPS]->snapEnabled = true;
		pct(AHUM, 0.f, "Humanize");
		for (int i = 0; i < 16; i++)
			configSwitch(STEP1 + i, 0.f, 3.f, 1.f, string::f("Step %d", i + 1),
			             std::vector<std::string>(vader::STEPS, vader::STEPS + 4));
		pct(DRIVE, 0.f, "Drive");
		pct(CHORUS, 0.2f, "Chorus mix");
		pct(CHSPD, 0.3f, "Chorus rate");
		pct(DELAY, 0.15f, "Delay mix");
		pct(DTIME, 0.4f, "Delay time (on the clock: the division, 4 bars … 1/16)");
		pct(DFB, 0.35f, "Delay feedback");
		pct(REVERB, 0.25f, "Reverb mix");
		pct(RSIZE, 0.6f, "Reverb size");
		pct(MRES, 0.f, "Master resonance (all corners' filters)");
		pct(VOLUME, 0.75f, "Volume");
		const char* in[NUM_INPUTS] = {"Attack CV",       "Decay CV",       "Sustain CV",       "Release CV",
		                              "Detune CV",       "Glide CV",       "Vibrato depth CV", "Tremolo depth CV",
		                              "Arpeggiator on (gate)", "Arpeggiator rate CV", "Latch (gate)", "Arpeggiator octaves CV",
		                              "Drive CV",        "Chorus mix CV",  "Delay mix CV",     "Delay time CV",
		                              "Reverb mix CV",   "Reverb size CV", "Master resonance CV", "Volume CV"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, in[i]);
	}
	void process(const ProcessArgs& args) override {
		lights[ARPON_LIGHT].setBrightness(params[ARPON].getValue() > 0.5f ? 1.f : 0.f);
		lights[LATCH_LIGHT].setBrightness(params[LATCH].getValue() > 0.5f ? 1.f : 0.f);
	}
};

/** A step of the pattern: a click steps OFF → ON → ACCENT → TIE. */
struct StepButton : app::ParamWidget {
	StepButton() { box.size = mm2px(Vec(6.f, 6.f)); }
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? (int) std::round(pq->getValue()) : 1;
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (engine::ParamQuantity* pq = getParamQuantity())
				pq->setValue((float) ((value() + 1) % 4));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {
		float cx = box.size.x / 2, cy = box.size.y / 2, r = box.size.x / 2 - 1.f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, r);
		nvgFillColor(args.vg, nvgRGB(0x0d, 0x0e, 0x11));
		nvgFill(args.vg);
		nvgStrokeColor(args.vg, ui::EDGE);
		nvgStrokeWidth(args.vg, 1.2f);
		nvgStroke(args.vg);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int v = value();
			float cx = box.size.x / 2, cy = box.size.y / 2, r = box.size.x / 2 - 3.f;
			if (v > 0) {
				NVGcolor c = v == 1 ? ACCENT : v == 2 ? nvgRGB(0xff, 0xb3, 0xbb) : nvgRGB(0x7a, 0x26, 0x32);
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, cx, cy, r);
				nvgFillColor(args.vg, c);
				nvgFill(args.vg);
				if (v == 3) {  // TIE: a bar to the next step
					nvgBeginPath(args.vg);
					nvgRect(args.vg, cx, cy - 1.f, box.size.x, 2.f);
					nvgFillColor(args.vg, c);
					nvgFill(args.vg);
				}
			}
		}
		ParamWidget::drawLayer(args, layer);
	}
};

struct UmlaufFxWidget : ModuleWidget {
	UmlaufFxWidget(UmlaufFx* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/UmlaufFx.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto lamps = [&](const float* p, int id, int n) {
			ui::LampRow* w = createParam<ui::LampRow>(Vec(0, 0), module, id);
			w->place(p, n);
			w->accent = ACCENT;
			addParam(w);
		};
		knob(L::ATK, ATK);
		knob(L::DEC, DEC);
		knob(L::SUS, SUS);
		knob(L::REL, REL);
		lamps(L::UNISON, UNISON, 4);
		lamps(L::VOICES, VOICES, 2);
		small(L::DETUNE, DETUNE);
		small(L::GLIDE, GLIDE);
		small(L::TOCT, TOCT);
		small(L::VELO, VELO);
		small(L::VSPD, VSPD);
		small(L::VDEP, VDEP);
		small(L::TSPD, TSPD);
		small(L::TDEP, TDEP);
		addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<UMLAUF_RGB>>>(ui::mm(L::ARPON), module,
		                                                                                   ARPON, ARPON_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<UMLAUF_RGB>>>(ui::mm(L::LATCH), module,
		                                                                                   LATCH, LATCH_LIGHT));
		lamps(L::AMODE, AMODE, 6);
		small(L::ARATE, ARATE);
		small(L::AOCT, AOCT);
		small(L::ALEN, ALEN);
		small(L::ASTEPS, ASTEPS);
		small(L::AHUM, AHUM);
		const float* st[16] = {L::STEP1,  L::STEP2,  L::STEP3,  L::STEP4,  L::STEP5,  L::STEP6,  L::STEP7,  L::STEP8,
		                       L::STEP9,  L::STEP10, L::STEP11, L::STEP12, L::STEP13, L::STEP14, L::STEP15, L::STEP16};
		for (int i = 0; i < 16; i++)
			addParam(createParamCentered<StepButton>(ui::mm(st[i]), module, STEP1 + i));
		small(L::DRIVE, DRIVE);
		small(L::CHORUS, CHORUS);
		small(L::CHSPD, CHSPD);
		small(L::DELAY, DELAY);
		small(L::DTIME, DTIME);
		small(L::DFB, DFB);
		small(L::REVERB, REVERB);
		small(L::RSIZE, RSIZE);
		small(L::MRES, MRES);
		small(L::VOLUME, VOLUME);
		const float* ins[NUM_INPUTS] = {L::IN_ATK,    L::IN_DEC,   L::IN_SUS,    L::IN_REL,    L::IN_DETUNE,
		                                L::IN_GLIDE,  L::IN_VIB,   L::IN_TREM,   L::IN_ARPON,  L::IN_ARATE,
		                                L::IN_LATCH,  L::IN_AOCT,  L::IN_DRIVE,  L::IN_CHORUS, L::IN_DELAY,
		                                L::IN_DTIME,  L::IN_REVERB, L::IN_RSIZE, L::IN_MRES,   L::IN_VOL};
		for (int i = 0; i < NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
	}
	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Place it to the right of UMLAUF: it reads these knobs"));
	}
};

}  // namespace genra_umlauffx

Model* modelUmlaufFx = createModel<genra_umlauffx::UmlaufFx, genra_umlauffx::UmlaufFxWidget>("UmlaufFx");
