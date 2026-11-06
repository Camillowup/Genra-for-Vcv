// SPDX-License-Identifier: GPL-3.0-only
// MONK: MONK-1 (Charles Vestal's FM-1 firmware: MonkSynth's formant voice, Jonathan Taylor) as a VCV Rack voice in the
// GENRA look. The sound is MONK-1's engine (MonkDsp.hpp), the singers on the screen are drawn by MONK-1's own code
// (monkface.c), and this file is the panel, the jacks and the glue: knobs and CV into the voice's parameters, GATE +
// V/OCT into its note (mono, as the firmware sings), VEL and PRESS as the keys' velocity and aftertouch, the twelve
// singers as presets of all the knobs.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "MonkDsp.hpp"
#include "MonkLayout.hpp"
#include "monkface.h"
#include "../jiant/JiantUI.hpp"
#include <atomic>
#include <array>

namespace monk_mod {

#define MONK_RGB 0xff, 0xb4, 0x3d   // keep in step with tools/monk_panel.py
static const int NK = 8;
static const char* K_NAMES[NK] = {"VOWEL", "HEAD", "BREATH", "GLIDE", "VOICES", "DETUNE", "SPREAD", "TUNE"};
static const char* VOWEL_NAMES[5] = {"OO", "OH", "AH", "EH", "EE"};
static const char* NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

using namespace monkdsp;

// ------------------------------------------------------------------------------------------------ the module
struct Monk : Module, follow::Host {
	enum ParamId {
		ENUMS(K_PARAM, NK),
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM,
		VIBD_PARAM, VIBR_PARAM, ECHO_PARAM, TIME_PARAM, FB_PARAM,
		PDEPTH_PARAM, VELSENS_PARAM, LEVEL_PARAM, ROUTE_PARAM,
		PREV_PARAM, NEXT_PARAM,
		PARAMS_LEN
	};
	enum InputId { ENUMS(K_INPUT, NK), VOCT_INPUT, GATE_INPUT, VEL_INPUT, PRESS_INPUT, SINGER_INPUT, INPUTS_LEN };
	enum OutputId { ENV_OUTPUT, DRY_OUTPUT, L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	/** The panel's knobs in the engine's terms (0..1), and which engine parameter each one is. */
	static int engineId(int param) {
		static const int K_ID[NK] = {VOWEL, HEAD, BREATH, GLIDE, VOICES, DETUNE, SPREAD, TUNE};
		if (param >= K_PARAM && param < K_PARAM + NK)
			return K_ID[param - K_PARAM];
		switch (param) {
			case ATK_PARAM: return ATTACK;
			case DEC_PARAM: return DECAY;
			case SUS_PARAM: return SUSTAIN;
			case REL_PARAM: return RELEASE;
			case VIBD_PARAM: return VIBRATO;
			case VIBR_PARAM: return VIBRATE;
			case ECHO_PARAM: return DELAY;
			case TIME_PARAM: return DELAYRATE;
			case FB_PARAM: return FEEDBACK;
			case LEVEL_PARAM: return LEVEL;
			default: return -1;
		}
	}
	/** TUNE's knob is in semitones (-12..12); the rest are 0..1 as the engine takes them. */
	static float toEngine(int param, float v) { return param == K_PARAM + 7 ? (v + 12.f) / 24.f : v; }
	static float fromEngine(int param, float x) { return param == K_PARAM + 7 ? x * 24.f - 12.f : x; }

	monkdsp::Monk dsp;
	float outL[monkdsp::Monk::BLOCK] = {}, outR[monkdsp::Monk::BLOCK] = {}, outD[monkdsp::Monk::BLOCK] = {};
	uint32_t pos = monkdsp::Monk::BLOCK;
	float env = 0.f;

	int singer = 0;
	std::atomic<int> singerReq{-1};
	int cvSinger = -1;
	bool gate = false;
	float note = 60.f;
	dsp::SchmittTrigger gateTrig;
	dsp::BooleanTrigger prevTrig, nextTrig;

	// for the UI thread: what is sung, and the knobs as they reach the engine (with their CV)
	std::atomic<float> uiVowel{0.5f}, uiAmp{0.f}, uiEnv{0.f}, uiPitch{57.f}, uiPress{0.f};
	std::atomic<bool> uiSinging{false};
	std::array<std::atomic<float>, NK> eff;

	struct PctQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%d", (int) std::round(getValue() * 100.f)); }
		std::string getUnit() override { return "%"; }
	};
	struct VowelQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			return string::f("%s (%d%%)", VOWEL_NAMES[clamp((int) std::round(v * 4.f), 0, 4)], (int) std::round(v * 100.f));
		}
	};
	struct VoicesQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%d", (int) (getValue() * 9.f + 1.5f)); }
	};

	Monk() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		srcName = "MONK";
		outL = L_OUTPUT;
		outR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		configParam<VowelQuantity>(K_PARAM + 0, 0.f, 1.f, 0.5f, "Vowel", "", 0.f, 1.f);
		configParam(K_PARAM + 1, 0.f, 1.f, 0.5f, "Head (vocal tract)", " cm", 0.f, 30.f);
		configParam<PctQuantity>(K_PARAM + 2, 0.f, 1.f, 0.5f, "Breath");
		configParam<PctQuantity>(K_PARAM + 3, 0.f, 1.f, 0.5f, "Glide");
		configParam<VoicesQuantity>(K_PARAM + 4, 0.f, 1.f, 0.f, "Voices (unison)");
		configParam(K_PARAM + 5, 0.f, 1.f, 0.f, "Detune", " ct", 0.f, 50.f);
		configParam<PctQuantity>(K_PARAM + 6, 0.f, 1.f, 0.f, "Spread (head sizes)");
		configParam(K_PARAM + 7, -12.f, 12.f, 0.f, "Tune", " st")->snapEnabled = true;
		configParam(ATK_PARAM, 0.f, 1.f, 0.f, "Attack", " s", 0.f, 5.f);
		configParam(DEC_PARAM, 0.f, 1.f, 0.f, "Decay", " s", 0.f, 5.f);
		configParam<PctQuantity>(SUS_PARAM, 0.f, 1.f, 1.f, "Sustain");
		configParam(REL_PARAM, 0.f, 1.f, 0.f, "Release", " s", 0.f, 5.f);
		configParam<PctQuantity>(VIBD_PARAM, 0.f, 1.f, 0.f, "Vibrato depth");
		configParam<PctQuantity>(VIBR_PARAM, 0.f, 1.f, 0.5f, "Vibrato rate");
		configParam<PctQuantity>(ECHO_PARAM, 0.f, 1.f, 0.8f, "Echo amount");
		configParam<PctQuantity>(TIME_PARAM, 0.f, 1.f, 0.5f, "Echo time");
		configParam(FB_PARAM, 0.f, 1.f, 0.5f / 0.98f, "Echo feedback", "%", 0.f, 98.f);
		configParam<PctQuantity>(PDEPTH_PARAM, 0.f, 1.f, 0.5f, "Pressure depth");
		configParam<PctQuantity>(VELSENS_PARAM, 0.f, 1.f, 0.5f, "Velocity sensitivity");
		configParam<PctQuantity>(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level");
		configSwitch(ROUTE_PARAM, 0.f, 3.f, 0.f, "Pressure goes to", {"Vowel", "Pitch (+2 st)", "Both", "Both, vowel inverted"});
		configButton(PREV_PARAM, "Previous singer");
		configButton(NEXT_PARAM, "Next singer");
		for (int k = 0; k < NK; k++)
			configInput(K_INPUT + k, string::f("%s CV (+-5 V: the knob's whole range)", K_NAMES[k]));
		configInput(VOCT_INPUT, "V/OCT (C4 = 0 V)");
		configInput(GATE_INPUT, "Gate");
		configInput(VEL_INPUT, "Velocity (0..10 V)");
		configInput(PRESS_INPUT, "Pressure / aftertouch (0..10 V)");
		configInput(SINGER_INPUT, "Singer (0..10 V: the twelve)");
		configOutput(ENV_OUTPUT, "Envelope (0..10 V)");
		configOutput(DRY_OUTPUT, "Dry (mono, before the echo)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		for (auto& e : eff)
			e = 0.f;
		dsp.init(44100.f);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		dsp.init(e.sampleRate);
		gate = false;
		pos = monkdsp::Monk::BLOCK;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		singer = 0;
		dsp.panic();
	}

	/** The singer's voice onto all the knobs (as PRESETS does on the FM-1). */
	void loadSinger(int ch) {
		int n = monkface_count();
		singer = (ch % n + n) % n;
		const float* p = monkface_voice(singer);
		for (int i = 0; i < PARAMS_LEN; i++) {
			int id = engineId(i);
			if (id >= 0 && id < 17)
				params[i].setValue(fromEngine(i, p[id]));
		}
		params[FB_PARAM].setValue(0.5f / 0.98f);
	}

	/** Whether the knobs differ from the singer's voice ("edited"). */
	bool edited() {
		const float* p = monkface_voice(singer);
		for (int i = 0; i < PARAMS_LEN; i++) {
			int id = engineId(i);
			if (id >= 0 && id < 17 && std::fabs(toEngine(i, params[i].getValue()) - p[id]) > 0.004f)
				return true;
		}
		return std::fabs(params[FB_PARAM].getValue() - 0.5f / 0.98f) > 0.004f;
	}

	float knob(int param) {
		float x = toEngine(param, params[param].getValue());
		if (param >= K_PARAM && param < K_PARAM + NK && inputs[K_INPUT + param - K_PARAM].isConnected())
			x += inputs[K_INPUT + param - K_PARAM].getVoltage() * 0.1f;
		return clamp(x, 0.f, 1.f);
	}

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void process(const ProcessArgs& args) override {
		fol.process(this); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		// the singer: the buttons, the menu, the CV (on a change of the step it is on)
		int req = singerReq.exchange(-1);
		if (req >= 0)
			loadSinger(req);
		if (prevTrig.process(params[PREV_PARAM].getValue() > 0.f))
			loadSinger(singer - 1);
		if (nextTrig.process(params[NEXT_PARAM].getValue() > 0.f))
			loadSinger(singer + 1);
		if (inputs[SINGER_INPUT].isConnected()) {
			int n = monkface_count();
			int s = clamp((int) (inputs[SINGER_INPUT].getVoltage() / 10.f * n), 0, n - 1);
			if (s != cvSinger) {
				cvSinger = s;
				if (s != singer)
					loadSinger(s);
			}
		} else {
			cvSinger = -1;
		}

		// the note: GATE and V/OCT (channel 1 of a poly cable)
		float g = inputs[GATE_INPUT].getVoltage();
		note = 60.f + 12.f * inputs[VOCT_INPUT].getVoltage();
		if (gateTrig.process(g, 0.1f, 1.f)) {
			float vel = inputs[VEL_INPUT].isConnected() ? inputs[VEL_INPUT].getVoltage() / 10.f : 100.f / 127.f;
			setParams();
			dsp.gateOn(note, vel);
			gate = true;
		} else if (gate && !gateTrig.isHigh()) {
			dsp.gateOff();
			gate = false;
		} else if (gate) {
			dsp.pitch(note);
		}

		if (pos >= monkdsp::Monk::BLOCK) {
			setParams();
			dsp.render(outL, outR, outD);
			pos = 0;
			uiVowel = dsp.uiVowel;
			uiAmp = dsp.uiAmp;
			uiEnv = dsp.uiEnv;
			uiPitch = dsp.uiPitch;
			uiSinging = gate || dsp.uiEnv > 0.001f;
		}
		env += (dsp.uiEnv - env) * 0.02f;
		outputs[L_OUTPUT].setVoltage(5.f * outL[pos]);
		outputs[R_OUTPUT].setVoltage(5.f * outR[pos]);
		outputs[DRY_OUTPUT].setVoltage(5.f * outD[pos]);
		outputs[ENV_OUTPUT].setVoltage(10.f * env);
		pos++;
	}

	void setParams() {
		for (int i = 0; i < PARAMS_LEN; i++) {
			int id = engineId(i);
			if (id < 0)
				continue;
			float x = knob(i);
			dsp.param(id, x);
			if (i >= K_PARAM && i < K_PARAM + NK)
				eff[i - K_PARAM] = x;
		}
		float pr = inputs[PRESS_INPUT].isConnected() ? clamp(inputs[PRESS_INPUT].getVoltage() / 10.f, 0.f, 1.f) : 0.f;
		uiPress = pr;
		dsp.pressure(pr);
		dsp.touch((int) std::round(params[ROUTE_PARAM].getValue()), params[PDEPTH_PARAM].getValue(),
		          params[VELSENS_PARAM].getValue());
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "singer", json_integer(singer));
		fol.toJson(root);
		return root;
	}
	void dataFromJson(json_t* root) override {
		fol.fromJson(root);
		if (json_t* s = json_object_get(root, "singer"))
			singer = clamp((int) json_integer_value(s), 0, monkface_count() - 1);
	}
};

// ------------------------------------------------------------------------------------------------ the widgets
namespace L = monk;
static const NVGcolor ACCENT = nvgRGB(MONK_RGB);

static std::string noteName(float n) {
	int k = (int) std::round(n);
	return string::f("%s%d", NOTE_NAMES[(k % 12 + 12) % 12], k / 12 - 1);
}

/** The singer on its stage, drawn by MONK-1's own code: the mouth follows the vowel, the brows the loudness; with
 * nothing sung it blinks and mouths its vowels to itself (the iOS port's idle animation). */
struct ScreenWidget : Widget {
	Monk* module = nullptr;
	jiant::Canvas565 canvas{240, 240};
	std::vector<uint16_t> scene = std::vector<uint16_t>(240 * 240), px = std::vector<uint16_t>(240 * 240);
	int sceneFor = -1;
	uint32_t seq = 0;
	int lastPose = -1;
	monkface_idle_t idle{};
	double idleT = 0.0;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		int ch = module ? module->singer : 0;
		if (ch != sceneFor) {
			monkface_scene(ch, scene.data());
			sceneFor = ch;
			lastPose = -1;
		}
		bool singing = module && module->uiSinging;
		double now = system::getTime();
		if (idleT == 0.0)
			idleT = now;
		while (now - idleT >= 0.1) {   // the idle animation, 100 ms a tick, reset by singing
			idleT += 0.1;
			if (singing)
				monkface_idle_reset(&idle);
			else
				monkface_idle_tick(&idle);
		}
		float vowel, loud = 0.f;
		int blink = 0;
		if (singing) {
			vowel = module->uiVowel;
			loud = clamp(module->uiAmp * module->uiEnv, 0.f, 1.f);
		} else {
			vowel = monkface_idle_vowel(&idle);
			blink = monkface_idle_blink(&idle);
		}
		int pose = (int) (vowel * 23.f + 0.5f) * 100 + (int) (loud * 4.f + 0.5f) * 10 + blink;
		if (pose != lastPose) {
			lastPose = pose;
			monkface_frame(ch, vowel, loud, blink, scene.data(), px.data());
			seq++;
		}
		nvgSave(args.vg);
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, 2.f);
		nvgScissor(args.vg, 0, 0, box.size.x, box.size.y);
		canvas.draw(args.vg, px.data(), seq, box.size.x, box.size.y);
		nvgRestore(args.vg);
	}
};

/** Right of the singer: its name, what it sings, the vowel (chips: a click sets VOWEL), pressure and envelope. */
struct InfoWidget : Widget {
	Monk* module = nullptr;
	static constexpr float CHIP_Y = 41.f, CHIP_H = 14.f, GAP = 3.f;

	float chipW() const { return (box.size.x - GAP * 4) / 5; }

	void onButton(const ButtonEvent& e) override {
		if (module && e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && e.pos.y >= CHIP_Y &&
		    e.pos.y <= CHIP_Y + CHIP_H) {
			int i = clamp((int) (e.pos.x / (chipW() + GAP)), 0, 4);
			module->params[Monk::K_PARAM + 0].setValue(i / 4.f);
			e.consume(this);
			return;
		}
		Widget::onButton(e);
	}

	void bar(NVGcontext* vg, float y, const char* name, float v, NVGcolor c) {
		float w = box.size.x;
		jiant::text(vg, jiant::monoFont(), 0, y + 5.5f, name, 7.f, jiant::DIM, NVG_ALIGN_LEFT, 0.f);
		jiant::roundRect(vg, 30, y, w - 30, 6, 1.5f, jiant::OFF);
		if (v > 0.002f)
			jiant::roundRect(vg, 30, y, std::max(3.f, (w - 30) * clamp(v, 0.f, 1.f)), 6, 1.5f, c);
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		auto bold = jiant::boldFont(), mono = jiant::monoFont();
		float w = box.size.x;
		int ch = module ? module->singer : 0, n = monkface_count();
		std::string name = monkface_name(ch);
		for (auto& c : name)
			c = (char) std::toupper((unsigned char) c);
		jiant::text(vg, bold, 0, 20, name, 15.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.6f);
		jiant::text(vg, bold, w, 20, string::f("\xc2\xb7 %d/%d", ch + 1, n), 6.5f, jiant::DIM, NVG_ALIGN_RIGHT, 0.8f);
		std::string line;
		bool singing = module && module->uiSinging;
		if (module) {
			int vo = (int) (module->eff[4] * 9.f + 1.5f);
			line = string::f("%s \xc2\xb7 %d VOICE%s \xc2\xb7 %s", singing ? noteName(module->uiPitch).c_str() : "--", vo,
			                 vo == 1 ? "" : "S", module->edited() ? "EDITED" : "FACTORY VOICE");
		} else {
			line = "A3 \xc2\xb7 1 VOICE \xc2\xb7 FACTORY VOICE";
		}
		jiant::text(vg, mono, 0, 33, line, 7.f, jiant::DIM, NVG_ALIGN_LEFT, 0.f);
		float vw = module ? (singing ? (float) module->uiVowel : (float) module->eff[0]) : 0.5f;
		int lit = clamp((int) std::round(vw * 4.f), 0, 4);
		float cw = chipW();
		for (int i = 0; i < 5; i++)
			jiant::chip(vg, i * (cw + GAP), CHIP_Y, cw, CHIP_H, VOWEL_NAMES[i], i == lit, ACCENT);
		NVGcolor envC = ACCENT;
		envC.a = 0.7f;
		bar(vg, 65.5f, "PRESS", module ? (float) module->uiPress : 0.35f, ACCENT);
		bar(vg, 80.5f, "ENV", module ? (float) module->uiEnv : 0.8f, envC);
	}
};

struct MonkWidget : ModuleWidget {
	MonkWidget(Monk* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Monk.svg")));
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

		addParam(createParamCentered<VCVBezel>(jiant::at(L::PREV), module, Monk::PREV_PARAM));
		addParam(createParamCentered<VCVBezel>(jiant::at(L::NEXT), module, Monk::NEXT_PARAM));
		jiant::LampRow* route = createParam<jiant::LampRow>(Vec(0, 0), module, Monk::ROUTE_PARAM);
		route->place(L::ROUTE, 4);
		route->accent = ACCENT;
		addParam(route);
		addParam(createParamCentered<GTrimpot>(jiant::at(L::PDEPTH), module, Monk::PDEPTH_PARAM));
		addParam(createParamCentered<GTrimpot>(jiant::at(L::VELSENS), module, Monk::VELSENS_PARAM));
		addParam(createParamCentered<GSmallKnob>(jiant::at(L::LEVEL), module, Monk::LEVEL_PARAM));

		for (int k = 0; k < NK; k++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(L::K[k]), module, Monk::K_PARAM + k));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::K_IN[k]), module, Monk::K_INPUT + k));
			jiant::KnobText* t = new jiant::KnobText;
			t->box = jiant::area(L::K_TEXT[k]);
			t->accent = ACCENT;
			t->label = [=]() -> std::string { return K_NAMES[k]; };
			t->value = [=]() -> std::string {
				if (!module)
					return "";
				float x = module->eff[k];
				switch (k) {
					case 0: return VOWEL_NAMES[clamp((int) std::round(x * 4.f), 0, 4)];
					case 1: return string::f("%dcm", (int) (x * 30.f + 0.5f));
					case 4: return string::f("%d", (int) (x * 9.f + 1.5f));
					case 5: return string::f("%dct", (int) (x * 50.f + 0.5f));
					case 7: {
						int st = (int) std::round(x * 24.f - 12.f);
						return string::f("%s%dst", st > 0 ? "+" : "", st);
					}
					default: return string::f("%d%%", (int) std::round(x * 100.f));
				}
			};
			addChild(t);
		}
		const float* small[9] = {L::ATK, L::DEC, L::SUS, L::REL, L::VIBD, L::VIBR, L::ECHO, L::TIME, L::FB};
		for (int i = 0; i < 9; i++)
			addParam(createParamCentered<GTrimpot>(jiant::at(small[i]), module, Monk::ATK_PARAM + i));

		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VOCT), module, Monk::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::GATE), module, Monk::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VEL), module, Monk::VEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::PRESS), module, Monk::PRESS_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::SINGER), module, Monk::SINGER_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_ENV), module, Monk::ENV_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_DRY), module, Monk::DRY_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_L), module, Monk::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_R), module, Monk::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Monk* m = dynamic_cast<Monk*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Singer", monkface_name(m->singer), [=](Menu* sub) {
			for (int i = 0; i < monkface_count(); i++)
				sub->addChild(createCheckMenuItem(monkface_name(i), "", [=]() { return m->singer == i; },
				                                  [=]() { m->singerReq = i; }));
		}));
		menu->addChild(createMenuItem("Singer's voice as it came", "", [=]() { m->singerReq = m->singer; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("MONK-1 by Charles Vestal (GPL-3.0)"));
		menu->addChild(createMenuLabel("MonkSynth by Jonathan Taylor (MIT)"));
		menu->addChild(createMenuLabel("If you enjoy it: savetibet.org"));
	}
};

}  // namespace monk_mod

Model* modelMonk = createModel<monk_mod::Monk, monk_mod::MonkWidget>("Monk");
