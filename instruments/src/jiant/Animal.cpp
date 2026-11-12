// SPDX-License-Identifier: GPL-3.0-only
// JIANT ANIMAL: the JIANT FM firmware's ANIMAL engine as a VCV Rack voice, with its creature drawn live.
// The engine runs unchanged in a private copy of the ANIMAL core (Core.hpp); this file is the panel, the jacks and the
// glue: knobs and CV into the part's parameters, V/OCT + GATE into its notes, its 44.1 kHz audio resampled to Rack's
// rate, the creature (the firmware's own drawing) on the screen.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "AnimalCore.hpp"
#include <atomic>
#include <array>
#include <cstring>
#include "JiantUI.hpp"
#include "../ui/Look.hpp"

namespace jiant_animal {


static const int NE = 7;                       // the engine's parameters after MODE (E1..E7)
static const char* MODE_NAMES[5] = {"CMOS", "PULSE", "WHSTL", "WING", "GLOTT"};
static const char* VOICE_NAMES[3] = {"POLY", "MONO", "LEGATO"};

// ------------------------------------------------------------------------------------------------ the module
struct Animal : Module, follow::Host {
	enum ParamId {
		MODE_PARAM,
		ENUMS(E_PARAM, NE),
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM,
		DIST_PARAM, CHOR_PARAM, DLY_PARAM, REV_PARAM,
		LEVEL_PARAM,
		VOICE_PARAM,
		PREV_PARAM, NEXT_PARAM, CALL_PARAM,
		PARAMS_LEN
	};
	enum InputId { ENUMS(E_INPUT, NE), VOCT_INPUT, GATE_INPUT, VEL_INPUT, CALL_INPUT, INPUTS_LEN };
	enum OutputId { L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { CALL_LIGHT, LIGHTS_LEN };
	enum { BLOCK = 32, CALL_NOTE = 72 };

	AnimalCore core;
	std::string status;

	// settings
	int preset = 0;
	bool fromPatch = false;                      // dataFromJson ran: the knobs are the patch's, not a preset's
	std::atomic<int> presetReq{-1};              // a preset asked for from the menu

	// for the UI thread (written by the engine thread)
	std::array<std::array<char, 16>, NE + 1> labels{};
	std::array<std::array<char, 16>, 16> values{};   // JAN_E1.. JAN_LEVEL formatted (index j - 1)
	uint16_t screen[JAN_VIS_W * JAN_VIS_H] = {};
	std::atomic<uint32_t> screenSeq{0};
	std::atomic<int> shownPreset{0};
	std::atomic<int> voices{0};

	// engine thread
	int sent[JAN_NP];
	float devL[BLOCK], devR[BLOCK];
	jiant::Resampler src;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 2048> outBuf;
	uint32_t framesSinceVis = 0;
	bool visDue = true;                          // a frame of the screen owed (drawn when it is seen)
	jiant::ScreenClock screenClock;              // set by the ScreenWidget's draw
	struct CvVoice { bool gate = false; int note = 60; };
	CvVoice cv[16];
	bool callOn = false;
	dsp::BooleanTrigger prevTrig, nextTrig;

	Animal() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		srcName = "ANIMAL";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		configSwitch(MODE_PARAM, 0.f, 4.f, 0.f, "Mode", {"CMOS (bird machine)", "PULSE (cricket, cicada, frog)",
		                                                 "WHSTL (syrinx)", "WING (flight tones)", "GLOTT (larynx)"});
		for (int k = 0; k < NE; k++) {
			configParam<EQuantity>(E_PARAM + k, 0.f, 1.f, 0.5f, string::f("E%d", k + 2));
			configInput(E_INPUT + k, string::f("E%d CV (±5 V: ± half the range)", k + 2));
		}
		const char* envN[4] = {"Attack", "Decay", "Sustain", "Release"};
		const char* fxN[4] = {"Distortion", "Chorus send", "Delay send", "Reverb send"};
		for (int i = 0; i < 4; i++) {
			configParam<RawQuantity>(ATK_PARAM + i, 0.f, 127.f, 0.f, envN[i])->snapEnabled = true;
			configParam<RawQuantity>(DIST_PARAM + i, 0.f, 127.f, 0.f, fxN[i])->snapEnabled = true;
		}
		configParam<RawQuantity>(LEVEL_PARAM, 0.f, 127.f, 104.f, "Level")->snapEnabled = true;
		configSwitch(VOICE_PARAM, 0.f, 2.f, 0.f, "Voice", {"POLY", "MONO", "LEGATO"});
		configButton(PREV_PARAM, "Previous preset");
		configButton(NEXT_PARAM, "Next preset");
		configButton(CALL_PARAM, "Call (plays C5)");
		configInput(VOCT_INPUT, "V/Oct (polyphonic)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(VEL_INPUT, "Velocity (0..10 V, polyphonic)");
		configInput(CALL_INPUT, "Call gate (plays C5)");
		configOutput(L_OUTPUT, "Left (mono when Right is not patched)");
		configOutput(R_OUTPUT, "Right");
		for (int j = ATK_PARAM; j <= VOICE_PARAM; j++)
			paramQuantities[j]->randomizeEnabled = j < DIST_PARAM;
		paramQuantities[MODE_PARAM]->randomizeEnabled = false;
		for (int& s : sent)
			s = INT32_MIN;
		loadCore();
	}

	/** E2..E8: the knob 0..1 over the range the current MODE gives it, shown as the FM-1 writes it. */
	struct EQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Animal* m = dynamic_cast<Animal*>(module);
			if (m && m->core.loaded())
				return m->values[paramId - E_PARAM].data();
			return ParamQuantity::getDisplayValueString();
		}
		std::string getLabel() override {
			Animal* m = dynamic_cast<Animal*>(module);
			if (m && m->labels[1 + paramId - E_PARAM][0])
				return m->labels[1 + paramId - E_PARAM].data();
			return ParamQuantity::getLabel();
		}
	};
	/** ATK .. LEVEL: the FM-1's 0..127, shown as it writes them (ms, %, dB). */
	struct RawQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Animal* m = dynamic_cast<Animal*>(module);
			if (m && m->core.loaded())
				return m->values[JAN_ATK + paramId - ATK_PARAM - 1].data();
			return ParamQuantity::getDisplayValueString();
		}
	};

	// ---------------------------------------------------------------------------------- power, presets
	/** The core (a library copied and loaded: file work). From the constructor: Rack holds no engine lock there, so
	 * the audio of a running patch goes on (2.37; before: in onAdd, under the lock). */
	void loadCore() {
		if (!core.loaded() && core.load())
			core.api->init(random::u32() | 1u);
	}
	void powerOn() {
		loadCore();  // (again only if the module was taken out and put back)
		if (!core.loaded()) {
			status = core.error;
			WARN("JIANT ANIMAL: %s", status.c_str());
			return;
		}
		status.clear();
		for (int& s : sent)
			s = INT32_MIN;
		outBuf.clear();
		src.reset();
		for (auto& v : cv)
			v.gate = false;
		callOn = false;
		{ // a patch from a version with more (or fewer) presets: stay in range
			int n = std::max(1, (int) core.api->npresets());
			preset = ((preset % n) + n) % n;
		}
		if (!fromPatch)
			applyPreset(preset);
		else
			core.api->load_preset(preset);       // (the knobs' values follow in the first block)
		shownPreset = preset;
	}

	/** The factory sound into the knobs (and the engine). */
	void applyPreset(int i) {
		const jan_api* a = core.api;
		if (!a)
			return;
		int n = (int) a->npresets();
		preset = ((i % n) + n) % n;
		a->load_preset((uint32_t) preset);
		params[MODE_PARAM].setValue((float) a->get(JAN_MODE));
		for (int k = 0; k < NE; k++) {
			uint32_t j = JAN_E1 + k;
			int lo = a->min(j), hi = a->max(j), v = a->get(j);
			params[E_PARAM + k].setValue(hi > lo ? (float) (v - lo) / (hi - lo) : 0.f);
		}
		for (int i2 = 0; i2 < 4; i2++) {
			params[ATK_PARAM + i2].setValue((float) a->get(JAN_ATK + i2));
			params[DIST_PARAM + i2].setValue((float) a->get(JAN_DIST + i2));
		}
		for (int& s : sent)
			s = INT32_MIN;
		shownPreset = preset;
	}

	void onAdd(const AddEvent& e) override {
		params[PREV_PARAM].setValue(0.f);
		params[NEXT_PARAM].setValue(0.f);
		params[CALL_PARAM].setValue(0.f);
		powerOn();
	}
	void onRemove(const RemoveEvent& e) override { follow::gone(this); core.unload(); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		fromPatch = false;
		presetReq = 0;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "preset", json_integer(preset));
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		if (json_t* j = json_object_get(r, "preset"))
			preset = (int) json_integer_value(j);
		fromPatch = true;
		if (core.api) { // a preset loaded onto (or pasted into) a running module: Rack holds the engine lock here
			int n = std::max(1, (int) core.api->npresets());
			preset = ((preset % n) + n) % n;
			core.api->load_preset(preset);
			for (int& s : sent)
				s = INT32_MIN; // the knobs' values follow in the next block
			shownPreset = preset;
		}
	}

	// ---------------------------------------------------------------------------------- the engine
	void push(const jan_api* a, uint32_t j, int v) {
		if (sent[j] != v) {
			a->set(j, v);
			sent[j] = v;
		}
	}

	void processKnobs(const jan_api* a) {
		push(a, JAN_MODE, (int) std::round(params[MODE_PARAM].getValue()));
		for (int k = 0; k < NE; k++) {
			uint32_t j = JAN_E1 + k;
			float x = params[E_PARAM + k].getValue();
			if (inputs[E_INPUT + k].isConnected())
				x += finiteOr0(inputs[E_INPUT + k].getVoltage()) / 10.f;
			x = clamp(x, 0.f, 1.f);
			int lo = a->min(j), hi = a->max(j);
			push(a, j, lo + (int) std::round(x * (hi - lo)));
		}
		for (int i = 0; i < 4; i++) {
			push(a, JAN_ATK + i, (int) std::round(params[ATK_PARAM + i].getValue()));
			push(a, JAN_DIST + i, (int) std::round(params[DIST_PARAM + i].getValue()));
		}
		push(a, JAN_LEVEL, (int) std::round(params[LEVEL_PARAM].getValue()));
		int vo = (int) std::round(params[VOICE_PARAM].getValue());
		if (sent[JAN_VOICE] != vo)
			a->all_off();
		push(a, JAN_VOICE, vo);
	}

	void processNotes(const jan_api* a) {
		int n = inputs[GATE_INPUT].isConnected() ? std::max(1, inputs[GATE_INPUT].getChannels()) : 0;
		for (int c = 0; c < 16; c++) {
			CvVoice& v = cv[c];
			bool on = false;
			if (c < n) {
				float g = inputs[GATE_INPUT].getPolyVoltage(c);
				on = v.gate ? g > 0.5f : g >= 1.f;
			}
			if (!on && !v.gate)  // a channel at rest: nothing to work out (2.37)
				continue;
			int note = clamp((int) std::round(finiteOr0(inputs[VOCT_INPUT].getPolyVoltage(c)) * 12.f) + 60, 0, 127);
			if (on && !v.gate) {
				int vel = 100;
				if (inputs[VEL_INPUT].isConnected())
					vel = clamp((int) std::round(finiteOr0(inputs[VEL_INPUT].getPolyVoltage(c)) / 10.f * 127.f), 1, 127);
				a->note_on(note, vel);
				v.note = note;
			} else if (!on && v.gate) {
				a->note_off(v.note);
			} else if (on && note != v.note) {      // legato: the new note, then the old one released
				a->note_on(note, 100);
				a->note_off(v.note);
				v.note = note;
			}
			v.gate = on;
		}
		bool call = params[CALL_PARAM].getValue() > 0.5f;
		if (inputs[CALL_INPUT].isConnected()) {
			float g = inputs[CALL_INPUT].getVoltage();
			call = call || (callOn ? g > 0.5f : g >= 1.f);
		}
		if (call && !callOn)
			a->note_on(CALL_NOTE, 100);
		else if (!call && callOn)
			a->note_off(CALL_NOTE);
		callOn = call;
		lights[CALL_LIGHT].setBrightness(call ? 1.f : 0.f);
	}

	void renderBlock(const jan_api* a, float sampleRate) {
		processKnobs(a);
		a->render(BLOCK, devL, devR);
		dsp::Frame<2> in[BLOCK];
		for (int i = 0; i < BLOCK; i++) {
			in[i].samples[0] = devL[i];
			in[i].samples[1] = devR[i];
		}
		int devRate = (int) a->sample_rate();
		src.process(in, BLOCK, devRate, (int) sampleRate, outBuf);
		framesSinceVis += BLOCK;
		if (visDue && screenClock.seen()) {             // (only while the screen is seen: it costs)
			visDue = false;
			std::memcpy(screen, a->vis(JAN_VIS_W, JAN_VIS_H), sizeof screen);
			screenSeq++;
		}
		if (framesSinceVis >= (uint32_t) devRate / 30) {     // the creature, 30 frames a second
			framesSinceVis = 0;
			visDue = true;
			for (int k = 0; k <= NE; k++)
				std::snprintf(labels[k].data(), labels[k].size(), "%s", a->label(JAN_MODE + k));
			for (uint32_t j = JAN_E1; j <= JAN_LEVEL; j++)
				a->format(j, a->get(j), values[j - 1].data(), (uint32_t) values[j - 1].size());
			voices = (int) a->active_voices();
		}
	}

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		const jan_api* a = core.api;
		if (!a) {
			outputs[L_OUTPUT].setVoltage(0.f);
			outputs[R_OUTPUT].setVoltage(0.f);
			return;
		}
		int req = presetReq.exchange(-1);
		bool want = req >= 0;
		if (prevTrig.process(params[PREV_PARAM].getValue() > 0.5f)) {
			req = preset - 1;
			want = true;
		}
		if (nextTrig.process(params[NEXT_PARAM].getValue() > 0.5f)) {
			req = preset + 1;
			want = true;
		}
		if (want) {
			fromPatch = false;
			applyPreset(req);
		}
		processNotes(a);
		int guard = 0;
		while (outBuf.empty() && guard++ < 8)
			renderBlock(a, args.sampleRate);
		dsp::Frame<2> f = outBuf.empty() ? dsp::Frame<2>{} : outBuf.shift();
		float l = f.samples[0] * 5.f, r = f.samples[1] * 5.f;
		if (outputs[R_OUTPUT].isConnected() || pulled(args.frame)) { // (pulled: MIX takes it without a cable)
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		} else {
			outputs[L_OUTPUT].setVoltage(0.5f * (l + r));
			outputs[R_OUTPUT].setVoltage(0.f);
		}
	}
};

// ------------------------------------------------------------------------------------------------ the panel (GENRA look)
namespace L = jlayout::animal;
static const NVGcolor ACCENT = nvgRGB(JIANT_ANIMAL_RGB);

/** The creature (the firmware's own drawing), drawn in the light layer. */
struct ScreenWidget : Widget {
	Animal* module = nullptr;
	jiant::Canvas565 canvas{JAN_VIS_W, JAN_VIS_H};
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		if (!module) {
			jiant::text(args.vg, jiant::boldFont(), box.size.x / 2, box.size.y / 2 + 5, "ANIMAL", 14.f, ACCENT,
			            NVG_ALIGN_CENTER, 2.f);
			return;
		}
		if (!module->status.empty()) {
			jiant::drawStatus(args.vg, module->status, box.size.x);
			return;
		}
		module->screenClock.mark();
		canvas.draw(args.vg, module->screen, module->screenSeq, box.size.x, box.size.y);
	}
};

/** Right of the creature: the preset, MODE and voice, the voices sounding, the first three knobs' values. */
struct InfoWidget : Widget {
	Animal* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		auto bold = jiant::boldFont(), mono = jiant::monoFont();
		float w = box.size.x;
		const jan_api* a = module ? module->core.api : nullptr;
		int p = module ? (int) module->shownPreset : 0;
		int mode = module ? (int) std::round(module->params[Animal::MODE_PARAM].getValue()) : 0;
		int vo = module ? (int) std::round(module->params[Animal::VOICE_PARAM].getValue()) : 0;
		mode = clamp(mode, 0, 4);
		vo = clamp(vo, 0, 2);
		float h = box.size.y;
		jiant::text(vg, bold, 0, 7.f, string::f("CALL %d / %u", p + 1, a ? a->npresets() : 24u), 5.2f, jiant::DIM,
		            NVG_ALIGN_LEFT, 1.f);
		int nv = module ? (int) module->voices : 0;
		for (int i = 0; i < 8; i++)
			jiant::roundRect(vg, w - (8 - i) * 9.f, 1.5f, 7, 6, 1.2f, i < nv ? ACCENT : jiant::OFF);
		jiant::text(vg, bold, w - 75.f, 7.f, string::f("VOICES %d/8", nv), 5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 1.f);
		jiant::text(vg, bold, 0, h * 0.6f, a ? a->preset_name((uint32_t) p) : "CMOS BIRD", 13.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.6f);
		jiant::text(vg, bold, 0, h - 1.f, string::f("%s \xc2\xb7 %s \xc2\xb7 CALL C5", MODE_NAMES[mode], VOICE_NAMES[vo]), 5.2f,
		            ACCENT, NVG_ALIGN_LEFT, 1.f);
		if (module && module->core.loaded()) {
			std::string s;
			for (int k = 1; k <= 3; k++)
				s += string::f("%s%s %s", k > 1 ? "  " : "", module->labels[k].data(), module->values[k - 1].data());
			jiant::text(vg, bold, w, h - 1.f, s, 5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 0.6f);
		}
		(void) mono;
	}
};

struct AnimalWidget : ModuleWidget {
	AnimalWidget(Animal* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/JiantAnimal.svg")));
		follow::addStrip(this, module, {jiant::at(L::VOCT), jiant::at(L::GATE), jiant::at(L::VEL)}); // lit while GENRA feeds the strip
		jiant::addScrews(this);

		ScreenWidget* scr = new ScreenWidget;
		scr->module = module;
		scr->box = jiant::area(L::SCREEN);
		addChild(scr);
		InfoWidget* info = new InfoWidget;
		info->module = module;
		info->box = jiant::area(L::INFO);
		addChild(info);
		genra_look::PickField* pf = createWidget<genra_look::PickField>(Vec(L::PRESETF[0], L::PRESETF[1]));
		pf->box.size = Vec(L::PRESETF[2], L::PRESETF[3]);
		pf->accent = ACCENT;
		pf->count = [=]() { return module && module->core.api ? (int) module->core.api->npresets() : 1; };
		pf->name = [=](int i) -> std::string {
			return module && module->core.api ? module->core.api->preset_name((uint32_t) i) : "CMOS BIRD";
		};
		pf->current = [=]() { return module ? (int) module->shownPreset : 0; };
		pf->choose = [=](int i) {
			if (module)
				module->presetReq = i;
		};
		addChild(pf);

		genra_look::ParamField* mf = createParam<genra_look::ParamField>(Vec(L::MODEF[0], L::MODEF[1]), module, Animal::MODE_PARAM);
		mf->box.size = Vec(L::MODEF[2], L::MODEF[3]);
		mf->count = 5;
		mf->nameOf = [](int i) -> std::string { return MODE_NAMES[clamp(i, 0, 4)]; };
		mf->infoOf = [](int i) -> std::string {
			static const char* I[5] = {"bird machine", "cricket, cicada, frog", "whistle", "wing", "glottal"};
			return I[clamp(i, 0, 4)];
		};
		mf->title = "Mode";
		mf->undo = "change mode";
		addParam(mf);
		addParam(createLightParamCentered<VCVLightBezel<jiant::AccentLight<JIANT_ANIMAL_RGB>>>(
		    jiant::at(L::CALL), module, Animal::CALL_PARAM, Animal::CALL_LIGHT));
		jiant::LampRow* vs = createParam<jiant::LampRow>(Vec(0, 0), module, Animal::VOICE_PARAM);
		vs->place(L::VOICE, 3);
		vs->accent = ACCENT;
		addParam(vs);
		addParam(createParamCentered<GKnob>(jiant::at(L::LEVEL), module, Animal::LEVEL_PARAM));

		static const char* defLab[NE] = {"TOUCH", "COUPLE", "CAP", "HYST", "RATE", "GROUP", "FM"};
		for (int k = 0; k < NE; k++) {
			Vec c = jiant::at(L::E[k]);
			if (k == 0)
				addParam(createParamCentered<GLargeKnob>(c, module, Animal::E_PARAM + k));
			else if (k < 4)
				addParam(createParamCentered<GKnob>(c, module, Animal::E_PARAM + k));
			else
				addParam(createParamCentered<GSmallKnob>(c, module, Animal::E_PARAM + k));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::E_IN[k]), module, Animal::E_INPUT + k));
			auto label = [=]() -> std::string {
				if (module && module->labels[k + 1][0])
					return module->labels[k + 1].data();
				return defLab[k];
			};
			genra_look::PanelText* t = new genra_look::PanelText;
			t->box = jiant::area(L::E_TEXT[k]);
			t->text = label;
			addChild(t);
			genra_look::PanelText* j = new genra_look::PanelText;
			j->box = jiant::area(L::E_INL[k]);
			j->text = label;
			j->color = jiant::DIM;
			j->size = 4.4f;
			j->spacing = 0.7f;
			addChild(j);
		}
		const float* env[4] = {L::ATK, L::DEC, L::SUS, L::REL};
		const float* fx[4] = {L::DIST, L::CHOR, L::DLY, L::REV};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(env[i]), module, Animal::ATK_PARAM + i));
			addParam(createParamCentered<GSmallKnob>(jiant::at(fx[i]), module, Animal::DIST_PARAM + i));
		}
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VOCT), module, Animal::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::GATE), module, Animal::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VEL), module, Animal::VEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::CALL_IN), module, Animal::CALL_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_L), module, Animal::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_R), module, Animal::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Animal* m = getModule<Animal>();
		if (!m || !m->core.loaded())
			return;
		menu->addChild(new MenuSeparator);
		std::vector<std::string> names;
		for (uint32_t i = 0; i < m->core.api->npresets(); i++)
			names.push_back(m->core.api->preset_name(i));
		menu->addChild(createIndexSubmenuItem("Preset", names, [=]() { return (size_t) m->shownPreset; },
		                                      [=](size_t i) { m->presetReq = (int) i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Engine: ANIMAL from JIANT FM by Juan Jiant (a Felucca fork), GPL-3.0"));
	}
};

}  // namespace jiant_animal

Model* modelAnimal = createModel<jiant_animal::Animal, jiant_animal::AnimalWidget>("Animal");
