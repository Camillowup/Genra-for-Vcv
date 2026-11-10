// SPDX-License-Identifier: GPL-3.0-only
// K-WASUB FX – the effects expander of K-WASUB, in the GENRA look: every setting of the firmware's effects rack.
// It holds knobs only: K-WASUB (to its left) reads them and writes the firmware; LINK lights when it does.
// Panel: tools/kwasub_panel.py (res/KwasubFx.svg + KwFxLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "KwFxLayout.hpp"
#include "KwIds.hpp"
#include "KwasubFx.hpp"

namespace genra_kwasubfx {

namespace L = kwfxlayout;
namespace ui = genra_ui;
using namespace kwfx;

#define KWASUB_RGB 0xb8, 0xf2, 0x3a
static const NVGcolor ACCENT = nvgRGB(KWASUB_RGB);

/** The value as the firmware writes it, when K-WASUB is to the left. */
struct FxQuantity : ParamQuantity {
	int fw = -1;
	std::string getDisplayValueString() override {
		char b[48];
		if (fw >= 0 && module && kwasubText(module->leftExpander.module, fw, b, sizeof(b)))
			return b;
		return ParamQuantity::getDisplayValueString();
	}
};

struct KwasubFx : Module {
	KwasubFx() {
		config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
		auto onoff = [&](int p, const char* n) { configSwitch(p, 0.f, 1.f, 0.f, std::string(n) + " on", {"Off", "On"}); };
		auto k = [&](int p, int fw, float lo, float hi, float def, const char* n) {
			FxQuantity* q = configParam<FxQuantity>(p, lo, hi, def, n);
			q->snapEnabled = true;
			q->fw = fw;
		};
		onoff(DR_ON, "Saturation");
		configSwitch(DR_TYPE, 0.f, 3.f, 1.f, "Saturation type", {"Soft", "Tube", "Hard", "Fold"});
		k(DR_AMT, kwid::DR_AMT, 0, 127, 40, "Saturation amount");
		k(DR_MIX, kwid::DR_MIX, 0, 127, 127, "Saturation mix");
		onoff(CR_ON, "Crush");
		k(CR_BITS, kwid::CR_BITS, 0, 127, 40, "Crush bits");
		k(CR_RATE, kwid::CR_RATE, 0, 127, 20, "Crush rate");
		k(CR_MIX, kwid::CR_MIX, 0, 127, 64, "Crush mix");
		onoff(CH_ON, "Chorus");
		k(CH_RATE, kwid::CH_RATE, 0, 127, 30, "Chorus rate");
		k(CH_DEP, kwid::CH_DEP, 0, 127, 60, "Chorus depth");
		k(CH_MIX, kwid::CH_MIX, 0, 127, 60, "Chorus mix");
		onoff(PH_ON, "Phaser");
		k(PH_RATE, kwid::PH_RATE, 0, 127, 40, "Phaser rate");
		k(PH_DEP, kwid::PH_DEP, 0, 127, 80, "Phaser depth");
		k(PH_FB, kwid::PH_FB, 0, 127, 50, "Phaser feedback");
		k(PH_MIX, kwid::PH_MIX, 0, 127, 64, "Phaser mix");
		onoff(DL_ON, "Delay");
		configSwitch(DL_TYPE, 0.f, 3.f, 0.f, "Delay type", {"Ping-pong", "Tape", "Digital", "Dub"});
		k(DL_TIME, kwid::DL_TIME, 3, 13, 8, "Delay time (on the tempo)");
		k(DL_FB, kwid::DL_FB, 0, 127, 60, "Delay feedback");
		k(DL_TONE, kwid::DL_TONE, 0, 127, 50, "Delay tone");
		k(DL_MIX, kwid::DL_MIX, 0, 127, 30, "Delay mix");
		onoff(RV_ON, "Reverb");
		configSwitch(RV_TYPE, 0.f, 3.f, 1.f, "Reverb type", {"Room", "Hall", "Plate", "Spring"});
		k(RV_SIZE, kwid::RV_SIZE, 0, 127, 70, "Reverb size");
		k(RV_DAMP, kwid::RV_DAMP, 0, 127, 60, "Reverb damping");
		k(RV_MIX, kwid::RV_MIX, 0, 127, 25, "Reverb mix");
		onoff(CP_ON, "Compressor");
		k(CP_AMT, kwid::CP_AMT, 0, 127, 50, "Compressor amount");
		k(CP_ATK, kwid::CP_ATK, 0, 127, 20, "Compressor attack");
		k(CP_REL, kwid::CP_REL, 0, 127, 50, "Compressor release");
		k(CP_PUMP, kwid::CP_PUMP, 0, 127, 0, "Pump (the sidechain dip on the tempo)");
		configSwitch(CP_PRATE, 0.f, 3.f, 0.f, "Pump rate", {"1/4", "1/8", "1/2", "1 bar"});
		onoff(OT_ON, "Multiband");
		k(OT_AMT, kwid::OT_AMT, 0, 127, 0, "Multiband amount");
		k(OT_LOW, kwid::OT_LOW, -64, 63, 0, "Multiband low band");
		k(OT_HIGH, kwid::OT_HIGH, -64, 63, 0, "Multiband high band");
		onoff(EQ_ON, "EQ");
		const char* bands[4] = {"EQ low shelf", "EQ bell 1", "EQ bell 2", "EQ high shelf"};
		const float freqDef[4] = {10, 19, 32, 41};
		for (int i = 0; i < 4; i++) {
			configParam(EQ1_F + 2 * i, 0.f, 47.f, freqDef[i], std::string(bands[i]) + " frequency")->snapEnabled = true;
			configParam(EQ1_G + 2 * i, -12.f, 12.f, 0.f, std::string(bands[i]) + " gain", " dB")->snapEnabled = true;
		}
		k(MS_GLUE, kwid::MS_GLUE, 0, 127, 30, "Glue");
		k(MS_SUB, kwid::MS_SUB, 0, 127, 0, "Sub boost");
		k(MB_MONO, kwid::MB_MONO, 0, 6, 4, "Mono bass below");
		const char* in[NUM_INPUTS] = {"Saturation amount CV", "Crush mix CV", "Chorus mix CV", "Phaser mix CV",
		                              "Delay time CV", "Delay feedback CV", "Delay mix CV", "Reverb size CV",
		                              "Reverb mix CV", "Compressor amount CV", "Pump CV", "Multiband amount CV",
		                              "EQ low shelf gain CV", "EQ high shelf gain CV", "Glue CV", "Sub boost CV"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, std::string(in[i]) + " (0–10 V)");
	}
	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 9; i++)
			lights[ON_LIGHT + i].setBrightness(params[ON_PARAMS[i]].getValue() > 0.5f ? 1.f : 0.f);
		Module* l = leftExpander.module;
		lights[LINK_LIGHT].setBrightness(l && l->model == modelKwasub ? 1.f : 0.f);
	}
};

struct KwasubFxWidget : ModuleWidget {
	KwasubFxWidget(KwasubFx* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/KwasubFx.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto lamps = [&](const float* p, int id) {
			ui::LampRow* w = createParam<ui::LampRow>(Vec(0, 0), module, id);
			w->place(p, 4);
			w->accent = ACCENT;
			addParam(w);
		};
		const float* on[9] = {L::DR_ON, L::CR_ON, L::CH_ON, L::PH_ON, L::DL_ON, L::RV_ON, L::CP_ON, L::OT_ON, L::EQ_ON};
		for (int i = 0; i < 9; i++)
			addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<KWASUB_RGB>>>(ui::mm(on[i]), module,
			                                                                                  ON_PARAMS[i], ON_LIGHT + i));
		Vec link = mm2px(Vec(L::LINK[0] + L::LINK[2] / 2, L::LINK[1] + L::LINK[3] / 2));
		addChild(createLightCentered<MediumLight<ui::AccentLight<0x5d, 0xff, 0x8a>>>(link, module, LINK_LIGHT));

		lamps(L::DR_TYPE, DR_TYPE);
		small(L::DR_AMT, DR_AMT);
		small(L::DR_MIX, DR_MIX);
		knob(L::CR_BITS, CR_BITS);
		knob(L::CR_RATE, CR_RATE);
		knob(L::CR_MIX, CR_MIX);
		knob(L::CH_RATE, CH_RATE);
		knob(L::CH_DEP, CH_DEP);
		knob(L::CH_MIX, CH_MIX);
		knob(L::PH_RATE, PH_RATE);
		knob(L::PH_DEP, PH_DEP);
		knob(L::PH_FB, PH_FB);
		knob(L::PH_MIX, PH_MIX);
		lamps(L::DL_TYPE, DL_TYPE);
		small(L::DL_TIME, DL_TIME);
		small(L::DL_FB, DL_FB);
		small(L::DL_TONE, DL_TONE);
		small(L::DL_MIX, DL_MIX);
		lamps(L::RV_TYPE, RV_TYPE);
		small(L::RV_SIZE, RV_SIZE);
		small(L::RV_DAMP, RV_DAMP);
		small(L::RV_MIX, RV_MIX);
		knob(L::CP_AMT, CP_AMT);
		knob(L::CP_ATK, CP_ATK);
		knob(L::CP_REL, CP_REL);
		knob(L::CP_PUMP, CP_PUMP);
		lamps(L::CP_PRATE, CP_PRATE);
		small(L::OT_AMT, OT_AMT);
		small(L::OT_LOW, OT_LOW);
		small(L::OT_HIGH, OT_HIGH);
		const float* eq[8] = {L::EQ1_F, L::EQ1_G, L::EQ2_F, L::EQ2_G, L::EQ3_F, L::EQ3_G, L::EQ4_F, L::EQ4_G};
		for (int i = 0; i < 8; i++)
			small(eq[i], EQ1_F + i);
		small(L::MS_GLUE, MS_GLUE);
		small(L::MS_SUB, MS_SUB);
		small(L::MB_MONO, MB_MONO);
		const float* ins[NUM_INPUTS] = {L::IN_SAT,  L::IN_CRUSH, L::IN_CHORUS, L::IN_PHASER, L::IN_DTIME, L::IN_DFB,
		                                L::IN_DMIX, L::IN_VSIZE, L::IN_VMIX,   L::IN_COMP,   L::IN_PUMP,  L::IN_OTT,
		                                L::IN_EQLO, L::IN_EQHI,  L::IN_GLUE,   L::IN_SUBB};
		for (int i = 0; i < NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
	}
	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Place it right of K-WASUB (LINK lights)"));
		menu->addChild(createMenuLabel("Without K-WASUB it does nothing"));
	}
};

}  // namespace genra_kwasubfx

Model* modelKwasubFx = createModel<genra_kwasubfx::KwasubFx, genra_kwasubfx::KwasubFxWidget>("KwasubFx");
