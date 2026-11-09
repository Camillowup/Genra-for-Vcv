// SPDX-License-Identifier: GPL-3.0-only
// GOLANA FX – the effects expander of GOLANA, in the GENRA look: the details of Golana's three effects (MOD FX, delay,
// reverb) and their order. It holds knobs only: GOLANA (to its left) reads them and writes the firmware; LINK lights
// when it does. Panel: tools/golana_fx_panel.py (res/GolanaFx.svg + GolanaFxLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "GolanaFx.hpp"
#include "GolanaFxLayout.hpp"
#include "GolanaUI.hpp"

namespace genra_golanafx {

namespace L = gfxlayout;
namespace ui = genra_ui;
namespace gu = golana_ui;
using namespace glfx;

// keep in step with tools/golana_panel.py (ACC): the melody track's cyan
#define GOLANA_RGB 0x3d, 0xc9, 0xff
static const NVGcolor ACCENT = nvgRGB(GOLANA_RGB);

/** The value as the firmware writes it, when GOLANA is to the left. */
struct FxQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		char b[48];
		if (module && golanaFxText(module->leftExpander.module, paramId, b, sizeof(b)))
			return b;
		return ParamQuantity::getDisplayValueString();
	}
};

struct GolanaFx : Module {
	GolanaFx() {
		config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
		auto onoff = [&](int p, const char* n) { configSwitch(p, 0.f, 1.f, 1.f, std::string(n) + " on", {"Off", "On"}); };
		auto k = [&](int p, int, float lo, float hi, float def, const char* n) {
			configParam<FxQuantity>(p, lo, hi, def, n)->snapEnabled = true;
		};
		onoff(MFX_ON, "MOD FX");
		configSwitch(MFX_TYPE, 0.f, 5.f, 0.f, "MOD FX type", gu::list(MFX_NAMES, 6));
		k(CRATE, -1, 0, 127, 40, "MOD FX rate");
		k(CDEPTH, -1, 0, 127, 70, "MOD FX depth");
		onoff(DLY_ON, "Delay");
		configSwitch(DTYPE, 0.f, 2.f, 0.f, "Delay type", gu::list(DTYPE_NAMES, 3));
		configSwitch(DTIME, 0.f, 12.f, 1.f, "Delay time (on the tempo)", gu::list(DTIME_NAMES, 13));
		k(DFDBK, -1, 0, 120, 55, "Delay feedback");
		k(DCOLOR, -1, 0, 127, 70, "Delay color (the echoes' tone)");
		k(DMIX, -1, 0, 127, 110, "Delay level (the echoes)");
		onoff(REV_ON, "Reverb");
		configSwitch(RTYPE, 0.f, 2.f, 2.f, "Reverb type", gu::list(RTYPE_NAMES, 3));
		k(RSIZE, -1, 0, 127, 95, "Reverb size");
		k(RDAMP, -1, 0, 127, 60, "Reverb damping");
		configSwitch(ROUTE, 0.f, 2.f, 0.f, "Effects order", gu::list(ROUTE_NAMES, 3));
		const char* in[NUM_INPUTS] = {"MOD FX rate", "MOD FX depth", "Delay feedback", "Delay color", "Delay level",
		                              "Reverb size", "Reverb damping", "Effects order"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, std::string(in[i]) + " CV (0–10 V)");
	}
	void process(const ProcessArgs& args) override {
		for (int i = 0; i < 3; i++)
			lights[ON_LIGHT + i].setBrightness(params[ON_PARAMS[i]].getValue() > 0.5f ? 1.f : 0.f);
		Module* l = leftExpander.module;
		lights[LINK_LIGHT].setBrightness(l && l->model == modelGolana ? 1.f : 0.f);
	}
};

struct GolanaFxWidget : ModuleWidget {
	GolanaFxWidget(GolanaFx* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GolanaFx.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		const float* on[3] = {L::MFX_ON, L::DLY_ON, L::REV_ON};
		for (int i = 0; i < 3; i++)
			addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<GOLANA_RGB>>>(ui::mm(on[i]), module,
			                                                                                  ON_PARAMS[i], ON_LIGHT + i));
		addChild(createLightCentered<MediumLight<ui::AccentLight<GOLANA_RGB>>>(ui::mm(L::LINK), module, LINK_LIGHT));

		gu::addField(this, module, L::F_MFX, MFX_TYPE, gu::list(MFX_NAMES, 6));
		gu::addField(this, module, L::F_DTYPE, DTYPE, gu::list(DTYPE_NAMES, 3));
		gu::addField(this, module, L::F_DTIME, DTIME, gu::list(DTIME_NAMES, 13));
		gu::addField(this, module, L::F_RTYPE, RTYPE, gu::list(RTYPE_NAMES, 3));
		gu::addField(this, module, L::F_ROUTE, ROUTE, gu::list(ROUTE_NAMES, 3));
		small(L::CRATE, CRATE);
		small(L::CDEPTH, CDEPTH);
		small(L::DFDBK, DFDBK);
		small(L::DCOLOR, DCOLOR);
		small(L::DMIX, DMIX);
		small(L::RSIZE, RSIZE);
		small(L::RDAMP, RDAMP);
		const float* ins[NUM_INPUTS] = {L::IN_CRATE, L::IN_CDEPTH, L::IN_DFDBK, L::IN_DCOLOR,
		                                L::IN_DMIX,  L::IN_RSIZE,  L::IN_RDAMP, L::IN_ROUTE};
		for (int i = 0; i < NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
	}
	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Place it right of GOLANA (LINK lights)"));
		menu->addChild(createMenuLabel("Without GOLANA it does nothing"));
	}
};

}  // namespace genra_golanafx

Model* modelGolanaFx = createModel<genra_golanafx::GolanaFx, genra_golanafx::GolanaFxWidget>("GolanaFx");
