// SPDX-License-Identifier: GPL-3.0-only
// GRUND FX – the expander of GRUND, in the GENRA look: the details behind GRUND's amp and FX knobs (compressor,
// drive, EQ, chorus, reverb). It holds knobs only: GRUND (to its left) reads them; LINK lights when it does.
// Panel: tools/panels/grundfx_panel.py (res/GrundFx.svg + GrundFxLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "GrundFx.hpp"
#include "GrundFxLayout.hpp"

namespace gfx {

namespace L = gfxlayout;
namespace ui = genra_ui;

#define GRUND_RGB 0xb8, 0xf2, 0x3a
static const NVGcolor ACCENT = nvgRGB(GRUND_RGB);

GrundFx::GrundFx() {
	config(NUM_PARAMS, NUM_INPUTS, 0, NUM_LIGHTS);
	auto onoff = [&](int p, const char* n, bool on) {
		configSwitch(p, 0.f, 1.f, on ? 1.f : 0.f, std::string(n) + " on", {"Off", "On"});
	};
	ggrund::FxSettings d;
	onoff(COMP_ON, "Compressor", d.compOn);
	configParam(C_THRESH, 0.f, 1.f, d.thresh, "Compressor threshold (with COMP on GRUND: how deep)", "%", 0.f, 100.f);
	configParam(C_RATIO, 0.f, 1.f, d.ratio, "Compressor ratio", "%", 0.f, 100.f);
	configParam(C_ATK, 0.f, 1.f, d.cAtk, "Compressor attack", "%", 0.f, 100.f);
	configParam(C_REL, 0.f, 1.f, d.cRel, "Compressor release", "%", 0.f, 100.f);
	onoff(DRIVE_ON, "Drive", d.driveOn);
	configSwitch(D_TYPE, 0.f, ggrund::NUM_DRIVES - 1, (float) d.dType, "Drive type", {"Tube", "FET", "Fuzz", "Crush"});
	configParam(D_TONE, 0.f, 1.f, d.dTone, "Drive tone (Crush: bits and rate)", "%", 0.f, 100.f);
	configParam(D_MIX, 0.f, 1.f, d.dMix, "Drive mix (parallel: the bass stays underneath)", "%", 0.f, 100.f);
	onoff(EQ_ON, "EQ", d.eqOn);
	configParam(E_HPF, 0.f, 1.f, d.hpf, "Low cut", " Hz", 10.f, 20.f);
	configParam(E_LOW, 0.f, 1.f, d.low, "Low (110 Hz)", " dB", 0.f, 24.f, -12.f);
	configParam(E_MID, 0.f, 1.f, d.mid, "Mid (700 Hz)", " dB", 0.f, 24.f, -12.f);
	configParam(E_HIGH, 0.f, 1.f, d.high, "High (3.2 kHz)", " dB", 0.f, 24.f, -12.f);
	onoff(CHO_ON, "Chorus", d.choOn);
	configParam(CH_RATE, 0.f, 1.f, d.choRate, "Chorus rate", "%", 0.f, 100.f);
	configParam(CH_DEPTH, 0.f, 1.f, d.choDepth, "Chorus depth", "%", 0.f, 100.f);
	onoff(VERB_ON, "Reverb", d.verbOn);
	configParam(V_SIZE, 0.f, 1.f, d.vSize, "Reverb size", "%", 0.f, 100.f);
	configParam(V_DAMP, 0.f, 1.f, d.vDamp, "Reverb damping", "%", 0.f, 100.f);
	configParam(V_PRE, 0.f, 1.f, d.vPre, "Reverb pre-delay", " ms", 0.f, 100.f, 1.f);
	configInput(IN_SC, "Sidechain: a kick here ducks the bass (the compressor's key)");
	configInput(IN_COMP, "Compressor threshold CV (0–10 V)");
	configInput(IN_DRIVE, "Drive mix CV (0–10 V)");
	configInput(IN_EQLO, "EQ low CV (±5 V = ±12 dB)");
	configInput(IN_EQHI, "EQ high CV (±5 V = ±12 dB)");
	configInput(IN_CHO, "Chorus depth CV (0–10 V)");
	configInput(IN_VERB, "Reverb size CV (0–10 V)");
}

void GrundFx::process(const ProcessArgs& args) {
	for (int i = 0; i < NUM_ON; i++)
		lights[ON_LIGHT + i].setBrightness(params[ON_PARAMS[i]].getValue() > 0.5f ? 1.f : 0.f);
	Module* l = leftExpander.module;
	bool link = l && l->model == modelGrund;
	lights[LINK_LIGHT].setBrightness(link ? 1.f : 0.f);
	if (!link)
		gr = 0.f;
}

json_t* GrundFx::dataToJson() {
	json_t* j = json_object();
	json_object_set_new(j, "choHi", json_boolean(choHi));
	json_object_set_new(j, "verbHi", json_boolean(verbHi));
	return j;
}
void GrundFx::dataFromJson(json_t* j) {
	if (json_t* v = json_object_get(j, "choHi"))
		choHi = json_boolean_value(v);
	if (json_t* v = json_object_get(j, "verbHi"))
		verbHi = json_boolean_value(v);
}

/** The drive type field: name and info, click = list, wheel = step. */
struct TypeField : app::ParamWidget {
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, ggrund::NUM_DRIVES - 1) : 0;
	}
	void set(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq || (int) std::round(pq->getValue()) == t)
			return;
		float old = pq->getValue();
		pq->setValue(t);
		history::ParamChange* h = new history::ParamChange;
		h->name = "change drive type";
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = t;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			rack::ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Drive type"));
			for (int i = 0; i < ggrund::NUM_DRIVES; i++)
				menu->addChild(createCheckMenuItem(ggrund::DRIVE_NAMES[i], ggrund::DRIVE_INFO[i],
				                                   [=]() { return value() == i; }, [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, ggrund::NUM_DRIVES - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			ui::text(args.vg, ui::boldFont(), mm2px(2.f), box.size.y / 2 + 2.6f, ggrund::DRIVE_NAMES[module ? value() : 0],
			         7.2f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
		ParamWidget::drawLayer(args, layer);
	}
};

/** The compressor's gain reduction. */
struct GrMeter : Widget {
	GrundFx* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			float g = clamp(module->gr.load(), 0.f, 1.f);
			if (g > 0.002f) {
				nvgBeginPath(args.vg);
				nvgRoundedRect(args.vg, 0, 0, box.size.x * g, box.size.y, box.size.y / 2);
				nvgFillColor(args.vg, ACCENT);
				nvgFill(args.vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct GrundFxWidget : ModuleWidget {
	GrundFxWidget(GrundFx* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GrundFx.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		const float* on[NUM_ON] = {L::COMP_ON, L::DRIVE_ON, L::EQ_ON, L::CHO_ON, L::VERB_ON};
		for (int i = 0; i < NUM_ON; i++)
			addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<GRUND_RGB>>>(ui::mm(on[i]), module,
			                                                                                 ON_PARAMS[i], ON_LIGHT + i));
		Vec link = mm2px(Vec(L::LINK[0] + L::LINK[2] / 2, L::LINK[1] + L::LINK[3] / 2));
		addChild(createLightCentered<MediumLight<ui::AccentLight<0x5d, 0xff, 0x8a>>>(link, module, LINK_LIGHT));

		small(L::C_THRESH, C_THRESH);
		small(L::C_RATIO, C_RATIO);
		small(L::C_ATK, C_ATK);
		small(L::C_REL, C_REL);
		GrMeter* gm = new GrMeter;
		gm->module = module;
		gm->box.pos = mm2px(Vec(L::GR[0], L::GR[1]));
		gm->box.size = mm2px(Vec(L::GR[2], L::GR[3]));
		addChild(gm);

		TypeField* tf = createParam<TypeField>(mm2px(Vec(L::D_TYPEF[0], L::D_TYPEF[1])), module, D_TYPE);
		tf->box.size = mm2px(Vec(L::D_TYPEF[2], L::D_TYPEF[3]));
		addParam(tf);
		small(L::D_TONE, D_TONE);
		small(L::D_MIX, D_MIX);
		small(L::E_HPF, E_HPF);
		small(L::E_LOW, E_LOW);
		small(L::E_MID, E_MID);
		small(L::E_HIGH, E_HIGH);
		small(L::CH_RATE, CH_RATE);
		small(L::CH_DEPTH, CH_DEPTH);
		small(L::V_SIZE, V_SIZE);
		small(L::V_DAMP, V_DAMP);
		small(L::V_PRE, V_PRE);
		const float* ins[NUM_INPUTS] = {L::IN_SC, L::IN_COMP, L::IN_DRIVE, L::IN_EQLO, L::IN_EQHI, L::IN_CHO, L::IN_VERB};
		for (int i = 0; i < NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
	}
	void appendContextMenu(Menu* menu) override {
		GrundFx* m = dynamic_cast<GrundFx*>(module);
		menu->addChild(new MenuSeparator);
		if (m) {
			menu->addChild(createBoolPtrMenuItem("Chorus: highs only (the bass stays mono)", "", &m->choHi));
			menu->addChild(createBoolPtrMenuItem("Reverb: highs only (the bass stays dry)", "", &m->verbHi));
			menu->addChild(new MenuSeparator);
		}
		menu->addChild(createMenuLabel("Place it right of GRUND (LINK lights)"));
		menu->addChild(createMenuLabel("GRUND's presets set these knobs too"));
	}
};

}  // namespace gfx

Model* modelGrundFx = createModel<gfx::GrundFx, gfx::GrundFxWidget>("GrundFx");
