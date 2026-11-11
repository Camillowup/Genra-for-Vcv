// SPDX-License-Identifier: GPL-3.0-only
// JIANT VOICE BEND: the circuit bending of VOICE's SPELL voice (SPELLBOX's speech chip), an expander placed right of
// VOICE: the chip's clock (pitch, speed and voice together), corrupted coefficients, frames played again, bits off its
// DAC. BEND (or a gate at ON) switches them all in at once, as SPELLBOX's GLO does.
#include "../plugin.hpp"
#include "VoiceBend.hpp"
#include "JiantUI.hpp"

namespace jiant_voice {

static const int NB = 4;

struct VoiceBend : Module, BendSource {
	enum ParamId { BEND_PARAM, ENUMS(B_PARAM, NB), PARAMS_LEN };
	enum InputId { ON_INPUT, ENUMS(B_INPUT, NB), INPUTS_LEN };
	enum OutputId { OUTPUTS_LEN };
	enum LightId { BEND_LIGHT, LIGHTS_LEN };

	bool latch = false;
	dsp::BooleanTrigger press;
	dsp::ClockDivider div;
	std::atomic<int> vals[NB];  // for the panel
	std::atomic<int> linked{0}, spellNow{0};

	struct BendQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			VoiceBend* m = dynamic_cast<VoiceBend*>(module);
			int k = paramId - B_PARAM;
			int v = m ? m->vals[k].load() : 0;
			return valueText(k, v);
		}
	};

	static std::string valueText(int k, int v) {
		switch (k) {
		case 0: return string::f("%.1f kHz", v <= 64 ? 4.f + v * 4.f / 64.f : 8.f + (v - 64) * 6.f / 63.f);
		case 3: return string::f("%d bit", 8 - v);
		default: return string::f("%d%%", (int) std::round(v * 100.f / 127.f));
		}
	}

	VoiceBend() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configButton(BEND_PARAM, "Bend (on / off; also on while ON is high)");
		configInput(ON_INPUT, "Bend on (gate)");
		const char* names[NB] = {"Clock (the chip's, 4..14 kHz)", "Corrupt (coefficients knocked off)", "Stutter (frames played again)",
		                         "Crush (bits off the 8-bit DAC)"};
		const float defs[NB] = {0.5f, 0.f, 0.f, 0.f};
		for (int k = 0; k < NB; k++) {
			configParam<BendQuantity>(B_PARAM + k, 0.f, 1.f, defs[k], names[k]);
			configInput(B_INPUT + k, string::f("%s CV (±5 V: ± half the range)", names[k]));
			vals[k] = 0;
		}
		div.setDivision(32);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		latch = false;
	}
	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "bend", json_boolean(latch));
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "bend"))
			latch = json_boolean_value(j);
	}

	void process(const ProcessArgs& args) override {
		if (press.process(params[BEND_PARAM].getValue() > 0.5f))
			latch = !latch;
		bool o = latch || inputs[ON_INPUT].getVoltage() >= 1.f;
		on = o ? 1 : 0;
		lights[BEND_LIGHT].setBrightness(o ? 1.f : 0.f);
		if (!div.process())
			return;
		const int hi[NB] = {127, 127, 127, 5};
		int v[NB];
		for (int k = 0; k < NB; k++) {
			float x = params[B_PARAM + k].getValue();
			if (inputs[B_INPUT + k].isConnected())
				x += inputs[B_INPUT + k].getVoltage() / 10.f;
			v[k] = (int) std::round(clamp(x, 0.f, 1.f) * hi[k]);
			vals[k] = v[k];
		}
		clock = v[0];
		bend = v[1];
		stutter = v[2];
		crush = v[3];
		Module* l = leftExpander.module;
		SpellSink* s = l && l->model == modelVoice ? dynamic_cast<SpellSink*>(l) : nullptr;
		linked = s ? 1 : 0;
		spellNow = s && s->spellOn ? 1 : 0;
	}
};

// ------------------------------------------------------------------------------------------------ the panel
namespace LB = jlayout::voicebend;
static const NVGcolor BEND_ACCENT = nvgRGB(JIANT_VOICE_RGB);

/** Linked to a VOICE on the left, and whether it is in SPELL. */
struct BendStatus : Widget {
	VoiceBend* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		bool lk = !module || module->linked, spl = !module || module->spellNow;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, 3, 6, 2.6f);
		nvgFillColor(args.vg, lk && spl ? BEND_ACCENT : lk ? jiant::DIM : nvgRGB(0x3d, 0x3f, 0x48));
		nvgFill(args.vg);
		jiant::text(args.vg, jiant::monoFont(), 9, 8.2f, !lk ? "no VOICE left" : spl ? "linked \xc2\xb7 SPELL" : "linked \xc2\xb7 not SPELL",
		            5.f, jiant::DIM, NVG_ALIGN_LEFT, 0.f);
	}
};

struct VoiceBendWidget : ModuleWidget {
	VoiceBendWidget(VoiceBend* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/JiantVoiceBend.svg")));
		jiant::addScrews(this);
		addParam(createLightParamCentered<VCVLightBezel<jiant::AccentLight<JIANT_VOICE_RGB>>>(
		    jiant::at(LB::BEND), module, VoiceBend::BEND_PARAM, VoiceBend::BEND_LIGHT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(LB::ON), module, VoiceBend::ON_INPUT));
		static const char* names[NB] = {"CLOCK", "CORRUPT", "STUTTER", "CRUSH"};
		for (int k = 0; k < NB; k++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(LB::B[k]), module, VoiceBend::B_PARAM + k));
			addInput(createInputCentered<PJ301MPort>(jiant::at(LB::B_IN[k]), module, VoiceBend::B_INPUT + k));
			jiant::KnobText* t = new jiant::KnobText;
			t->box = jiant::area(LB::B_TEXT[k]);
			t->accent = BEND_ACCENT;
			t->label = [=]() -> std::string { return names[k]; };
			t->value = [=]() -> std::string {
				static const int hi[NB] = {127, 127, 127, 5};
				int v = module ? module->vals[k].load() : (int) std::round((k == 0 ? 0.5f : 0.f) * hi[k]);
				return VoiceBend::valueText(k, v);
			};
			addChild(t);
		}
		BendStatus* st = new BendStatus;
		st->module = module;
		st->box = jiant::area(LB::STATUS);
		addChild(st);
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Place it right of VOICE; it bends VOICE's SPELL only"));
		menu->addChild(createMenuLabel("SPELL: SPELLBOX by pjac1810 (its circuit-bending knobs), GPL-3.0"));
	}
};

}  // namespace jiant_voice

Model* modelVoiceBend = createModel<jiant_voice::VoiceBend, jiant_voice::VoiceBendWidget>("VoiceBend");
