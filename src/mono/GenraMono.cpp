// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – monophonic bass synth after the Moog Minitaur, in the look of MONK and the JIANT voices (accent: turquoise).
// Sound engine: MonoDsp.hpp. Panel: tools/mono_panel.py (res/GenraMono.svg + MonoLayout.hpp).
#include "../plugin.hpp"
#include "MonoDsp.hpp"
#include "MonoLayout.hpp"

namespace genra_mono {

namespace L = mlayout;

// keep in step with tools/mono_panel.py (ACC)
static const NVGcolor ACCENT = nvgRGB(0xb8, 0xf2, 0x3a);
static const NVGcolor TXT = nvgRGB(0xd9, 0xdb, 0xe2), DIM = nvgRGB(0x8a, 0x8d, 0x99);

inline Vec P(const float* p) { return mm2px(Vec(p[0], p[1])); }

struct GenraMono : Module {
	enum ParamId {
		WAVE1_PARAM, WAVE2_PARAM, TUNE1_PARAM, TUNE2_PARAM, LEVEL1_PARAM, LEVEL2_PARAM, GLIDE_PARAM,
		FTYPE_PARAM, CUTOFF_PARAM, RES_PARAM, EGAMT_PARAM, DRIVE_PARAM, KEYTRK_PARAM,
		FA_PARAM, FD_PARAM, FS_PARAM, FR_PARAM, AA_PARAM, AD_PARAM, AS_PARAM, AR_PARAM,
		RATE_PARAM, LFOWAVE_PARAM, LFOPITCH_PARAM, LFOFILT_PARAM,
		NUM_PARAMS
	};
	enum InputId { VOCT_INPUT, GATE_INPUT, VEL_INPUT, CUTOFF_INPUT, RES_INPUT, DRIVE_INPUT, RATE_INPUT, VCA_INPUT,
		           EXT_INPUT, NUM_INPUTS };
	enum OutputId { LFO_OUTPUT, AUDIO_OUTPUT, NUM_OUTPUTS };

	/** Envelope times: 1 ms .. 10 s. */
	struct TimeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float t = gmono::knobTime(getValue());
			return t < 1.f ? string::f("%.0f", t * 1000.f) : string::f("%.2f", t);
		}
		std::string getUnit() override { return gmono::knobTime(getValue()) < 1.f ? " ms" : " s"; }
	};
	/** TUNE: ±12 semitones, fine around the middle. */
	struct TuneQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%+.2f", gmono::tuneSemis(getValue())); }
		std::string getUnit() override { return " st"; }
	};

	gmono::Voice voice;
	gmono::Params p;
	bool gate = false;

	GenraMono() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		configParam(WAVE1_PARAM, 0.f, 1.f, 0.33f, "VCO1 wave", "", 0.f, 1.f);
		configParam(WAVE2_PARAM, 0.f, 1.f, 0.33f, "VCO2 wave", "", 0.f, 1.f);
		paramQuantities[WAVE1_PARAM]->description = paramQuantities[WAVE2_PARAM]->description =
		    "Triangle → saw → square → narrow pulse";
		configParam<TuneQuantity>(TUNE1_PARAM, -1.f, 1.f, 0.f, "VCO1 tune");
		configParam<TuneQuantity>(TUNE2_PARAM, -1.f, 1.f, 0.03f, "VCO2 tune");
		configParam(LEVEL1_PARAM, 0.f, 1.f, 0.8f, "VCO1 level", "%", 0.f, 100.f);
		configParam(LEVEL2_PARAM, 0.f, 1.f, 0.8f, "VCO2 level", "%", 0.f, 100.f);
		paramQuantities[LEVEL1_PARAM]->description = paramQuantities[LEVEL2_PARAM]->description =
		    "High levels drive the filter into saturation";
		configParam(GLIDE_PARAM, 0.f, 1.f, 0.f, "Glide", "%", 0.f, 100.f);
		std::vector<std::string> names;
		for (int i = 0; i < gmono::NUM_FILTERS; i++)
			names.push_back(gmono::FILTER_NAMES[i]);
		configSwitch(FTYPE_PARAM, 0.f, gmono::NUM_FILTERS - 1, 0.f, "Filter type", names);
		configParam(CUTOFF_PARAM, 0.f, 1.f, 0.5f, "Cutoff", " Hz", 1000.f, 20.f);
		configParam(RES_PARAM, 0.f, 1.f, 0.2f, "Resonance", "%", 0.f, 100.f);
		configParam(EGAMT_PARAM, -1.f, 1.f, 0.4f, "Filter EG amount", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam(KEYTRK_PARAM, 0.f, 1.f, 0.f, "Key tracking", "%", 0.f, 100.f);
		const char* st[4] = {"attack", "decay", "sustain", "release"};
		for (int i = 0; i < 4; i++) {
			if (i == 2) {
				configParam(FA_PARAM + i, 0.f, 1.f, 0.f, "Filter EG sustain", "%", 0.f, 100.f);
				configParam(AA_PARAM + i, 0.f, 1.f, 1.f, "Amp EG sustain", "%", 0.f, 100.f);
			} else {
				configParam<TimeQuantity>(FA_PARAM + i, 0.f, 1.f, i == 0 ? 0.f : 0.45f, std::string("Filter EG ") + st[i]);
				configParam<TimeQuantity>(AA_PARAM + i, 0.f, 1.f, i == 0 ? 0.f : 0.45f, std::string("Amp EG ") + st[i]);
			}
		}
		configParam(RATE_PARAM, 0.f, 1.f, 0.45f, "LFO rate", " Hz", 1000.f, 0.05f);
		configSwitch(LFOWAVE_PARAM, 0.f, 1.f, 1.f, "LFO wave", {"Square", "Triangle"});
		configParam(LFOPITCH_PARAM, 0.f, 1.f, 0.f, "LFO → pitch", "%", 0.f, 100.f);
		configParam(LFOFILT_PARAM, 0.f, 1.f, 0.f, "LFO → filter", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(VEL_INPUT, "Velocity (0–10 V)");
		configInput(CUTOFF_INPUT, "Cutoff CV (1 V/oct)");
		configInput(RES_INPUT, "Resonance CV");
		configInput(DRIVE_INPUT, "Drive CV");
		configInput(RATE_INPUT, "LFO rate CV (1 V/oct)");
		configInput(VCA_INPUT, "VCA CV (0–10 V)");
		configInput(EXT_INPUT, "External audio into the filter");
		configOutput(LFO_OUTPUT, "LFO");
		configOutput(AUDIO_OUTPUT, "Audio");
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { voice.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		voice = gmono::Voice();
		voice.setSampleRate(APP->engine->getSampleRate());
	}

	void process(const ProcessArgs& args) override {
		auto v = [&](int id) { return params[id].getValue(); };
		p.wave1 = v(WAVE1_PARAM);
		p.wave2 = v(WAVE2_PARAM);
		p.tune1 = v(TUNE1_PARAM);
		p.tune2 = v(TUNE2_PARAM);
		p.level1 = v(LEVEL1_PARAM);
		p.level2 = v(LEVEL2_PARAM);
		p.glide = v(GLIDE_PARAM);
		p.ftype = clamp((int) std::round(v(FTYPE_PARAM)), 0, gmono::NUM_FILTERS - 1);
		p.cutoff = v(CUTOFF_PARAM);
		p.res = v(RES_PARAM);
		p.egAmt = v(EGAMT_PARAM);
		p.drive = v(DRIVE_PARAM);
		p.keytrk = v(KEYTRK_PARAM);
		p.fA = v(FA_PARAM), p.fD = v(FD_PARAM), p.fS = v(FS_PARAM), p.fR = v(FR_PARAM);
		p.aA = v(AA_PARAM), p.aD = v(AD_PARAM), p.aS = v(AS_PARAM), p.aR = v(AR_PARAM);
		p.rate = v(RATE_PARAM);
		p.lfoSquare = v(LFOWAVE_PARAM) < 0.5f;
		p.lfoPitch = v(LFOPITCH_PARAM);
		p.lfoFilter = v(LFOFILT_PARAM);

		gmono::Inputs in;
		in.voct = inputs[VOCT_INPUT].getVoltage();
		float g = inputs[GATE_INPUT].getVoltage();
		if (g >= 1.f)
			gate = true;
		else if (g <= 0.1f)
			gate = false;
		in.gate = gate;
		in.vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 1.f;
		in.cutoffCv = inputs[CUTOFF_INPUT].getVoltage();
		in.resCv = inputs[RES_INPUT].getVoltage();
		in.driveCv = inputs[DRIVE_INPUT].getVoltage();
		in.rateCv = inputs[RATE_INPUT].getVoltage();
		in.vca = inputs[VCA_INPUT].isConnected() ? clamp(inputs[VCA_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 1.f;
		in.ext = inputs[EXT_INPUT].getVoltage();

		float audio, lfo;
		voice.process(p, in, audio, lfo);
		outputs[AUDIO_OUTPUT].setVoltage(audio);
		outputs[LFO_OUTPUT].setVoltage(lfo);
	}
};

// ---------------------------------------------------------------- widgets
static std::shared_ptr<window::Font> boldFont() {
	return APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
}

/** The filter type field: shows the type, a click opens the list, the wheel steps through it. */
struct FilterField : app::ParamWidget {
	int type() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, gmono::NUM_FILTERS - 1) : 0;
	}
	void setType(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float old = pq->getValue();
		if ((int) std::round(old) == t)
			return;
		pq->setValue(t);
		history::ParamChange* h = new history::ParamChange;
		h->name = "change filter type";
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = t;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Filter type"));
			for (int i = 0; i < gmono::NUM_FILTERS; i++)
				menu->addChild(createCheckMenuItem(gmono::FILTER_NAMES[i], gmono::FILTER_INFO[i],
				                                   [=]() { return type() == i; }, [=]() { setType(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			setType(clamp(type() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, gmono::NUM_FILTERS - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void drawText(NVGcontext* vg, float x, float size, NVGcolor c, const char* s, float spacing) {
		std::shared_ptr<window::Font> f = boldFont();
		if (!f || f->handle < 0)
			return;
		nvgFontFaceId(vg, f->handle);
		nvgFontSize(vg, size);
		nvgTextLetterSpacing(vg, spacing);
		nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(vg, c);
		nvgText(vg, x, box.size.y / 2 + 0.5f, s, nullptr);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int t = type();
			drawText(args.vg, mm2px(2.f), 7.6f, TXT, gmono::FILTER_NAMES[t], 0.9f);
			std::string info = string::uppercase(gmono::FILTER_INFO[t]);
			drawText(args.vg, mm2px(20.5f), 5.8f, DIM, info.c_str(), 0.8f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

struct GenraMonoWidget : ModuleWidget {
	GenraMonoWidget(GenraMono* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GenraMono.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using M = GenraMono;
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<RoundBlackKnob>(P(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<RoundSmallBlackKnob>(P(p), module, id)); };
		knob(L::WAVE1, M::WAVE1_PARAM);
		knob(L::TUNE1, M::TUNE1_PARAM);
		knob(L::LEVEL1, M::LEVEL1_PARAM);
		knob(L::WAVE2, M::WAVE2_PARAM);
		knob(L::TUNE2, M::TUNE2_PARAM);
		knob(L::LEVEL2, M::LEVEL2_PARAM);
		small(L::GLIDE, M::GLIDE_PARAM);

		FilterField* field = createParam<FilterField>(mm2px(Vec(L::FIELD[0], L::FIELD[1])), module, M::FTYPE_PARAM);
		field->box.size = mm2px(Vec(L::FIELD[2], L::FIELD[3]));
		addParam(field);
		addParam(createParamCentered<RoundLargeBlackKnob>(P(L::CUTOFF), module, M::CUTOFF_PARAM));
		knob(L::RES, M::RES_PARAM);
		knob(L::EGAMT, M::EGAMT_PARAM);
		small(L::DRIVE, M::DRIVE_PARAM);
		small(L::KEYTRK, M::KEYTRK_PARAM);

		const float* fenv[4] = {L::FA, L::FD, L::FS, L::FR};
		const float* aenv[4] = {L::AA, L::AD, L::AS, L::AR};
		for (int i = 0; i < 4; i++) {
			small(fenv[i], M::FA_PARAM + i);
			small(aenv[i], M::AA_PARAM + i);
		}
		small(L::RATE, M::RATE_PARAM);
		addParam(createParamCentered<CKSS>(P(L::LFOWAVE), module, M::LFOWAVE_PARAM));
		small(L::LFOPITCH, M::LFOPITCH_PARAM);
		small(L::LFOFILT, M::LFOFILT_PARAM);

		const float* ins[M::NUM_INPUTS] = {L::IN_VOCT, L::IN_GATE, L::IN_VEL, L::IN_CUTOFF, L::IN_RES,
		                                   L::IN_DRIVE, L::IN_RATE, L::IN_VCA, L::IN_EXT};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(P(ins[i]), module, i));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_LFO), module, M::LFO_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_AUDIO), module, M::AUDIO_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		GenraMono* m = getModule<GenraMono>();
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
		    "Filter type",
		    std::vector<std::string>(gmono::FILTER_NAMES, gmono::FILTER_NAMES + gmono::NUM_FILTERS),
		    [=]() { return (size_t) clamp((int) std::round(m->params[GenraMono::FTYPE_PARAM].getValue()), 0, gmono::NUM_FILTERS - 1); },
		    [=](size_t i) { m->params[GenraMono::FTYPE_PARAM].setValue((float) i); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Monophonic bass synth after the Moog Minitaur"));
		menu->addChild(createMenuLabel("Click or scroll the filter field to change the filter type"));
	}
};

}  // namespace genra_mono

Model* modelGenraMono = createModel<genra_mono::GenraMono, genra_mono::GenraMonoWidget>("GenraMono");
