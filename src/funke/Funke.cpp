// SPDX-License-Identifier: GPL-3.0-only
// FUNKE – the melody voice for GENRA's melody track, in the GENRA look (accent: cyan, GENRA's melody colour).
// Sound engine: FunkeDsp.hpp. Panel: tools/funke_panel.py (res/Funke.svg + FunkeLayout.hpp).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "FunkeDsp.hpp"
#include "FunkeLayout.hpp"
#include "FunkePresets.hpp"
#include "../ui/PresetBank.hpp"
#include <memory>

namespace genra_funke {

namespace L = flayout;
namespace ui = genra_ui;
using namespace gfunke;

// keep in step with tools/funke_panel.py (ACC)
#define FUNKE_RGB 0x3d, 0xc9, 0xff
static const NVGcolor ACCENT = nvgRGB(FUNKE_RGB);
static const int SCOPE = 128;
static const int PPQN_CHOICES[4] = {1, 2, 4, 24};

struct Funke : Module, follow::Host {
	enum ParamId {
		MODEL_PARAM, TIMBRE_PARAM, BITE_PARAM, WIDTH_PARAM, SUB_PARAM, VMODE_PARAM, GLIDE_PARAM, ACCENT_PARAM,
		FTYPE_PARAM, CUTOFF_PARAM, RES_PARAM, FENV_PARAM, KEY_PARAM, FATK_PARAM, FDEC_PARAM, DRIVE_PARAM,
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM,
		VDEPTH_PARAM, VRATE_PARAM, VDELAY_PARAM,
		EDIV_PARAM, CHORUS_PARAM, ECHO_PARAM, FDBK_PARAM, LOFI_PARAM, VERB_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, ACC_INPUT, SLIDE_INPUT, CLOCK_INPUT,
		MODEL_INPUT, TIMBRE_INPUT, BITE_INPUT, CUT_INPUT, RES_INPUT,
		VIB_INPUT, ECHO_INPUT, LOFI_INPUT, ATK_INPUT, REL_INPUT,
		RESET_INPUT, // 2.13 (GENRA strip)
		NUM_INPUTS
	};
	enum OutputId { ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightId { NUM_LIGHTS };

	struct TimeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float t = gmono::knobTime(getValue());
			return t < 1.f ? string::f("%.0f", t * 1000.f) : string::f("%.2f", t);
		}
		std::string getUnit() override { return gmono::knobTime(getValue()) < 1.f ? " ms" : " s"; }
	};
	struct GlideQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.0f", glideSeconds(getValue()) * 1000.f); }
		std::string getUnit() override { return " ms"; }
	};
	struct VRateQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.2f", vibRate(getValue())); }
		std::string getUnit() override { return " Hz"; }
	};
	struct VDelayQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.2f", vibDelay(getValue())); }
		std::string getUnit() override { return " s"; }
	};
	struct CutoffQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float f = 20.f * std::pow(1000.f, getValue());
			return f < 1000.f ? string::f("%.0f", f) : string::f("%.2f k", f / 1000.f);
		}
		std::string getUnit() override { return "Hz"; }
	};
	struct TimbreQuantity : ParamQuantity {
		std::string getDescription() override {
			Funke* m = dynamic_cast<Funke*>(module);
			return m ? MODEL_HELP[m->modelNow] : "";
		}
	};

	std::unique_ptr<gfunke::Engine> engine{new gfunke::Engine};
	Params p;
	Channel ch[MAXCH];
	int ctl = 0;
	int ppqn = 4;  // GENRA's CLOCK: 16ths
	dsp::SchmittTrigger clockTrig, resetTrig;
	float sinceClock = 0.f, bpm = 120.f;
	bool clockSeen = false;

	// for the display
	int modelNow = RHODES;
	float scope[SCOPE] = {};
	int scopeW = 0, scopeDiv = 0;
	float showScope[SCOPE] = {};

	static_assert(NUM_PRESET_PARAMS == NUM_PARAMS, "presets must cover every knob");
	genra_presets::Bank bank;  // presets: the factory list (FunkePresets.hpp), then the user presets
	void initPresets() {
		genra_presets::Factory f;
		f.count = NUM_PRESETS;
		f.style = [](int i) { return PRESETS[i].style; };
		f.name = [](int i) { return PRESETS[i].name; };
		f.info = [](int i) { return MODEL_NAMES[PRESETS[i].model]; };
		f.values = [](int i, float* v) { PRESETS[i].values(v); };
		f.gain = [](int i) { return PRESETS[i].gain; };
		bank.init(this, f, asset::user("Genra/FunkePresets"));
	}

	Funke() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		srcName = "FUNKE";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		fol.bind(ACC_INPUT, follow::ACC);
		fol.bind(SLIDE_INPUT, follow::SLIDE);
		fol.bind(VIB_INPUT, follow::MOD);
		fol.bind(CLOCK_INPUT, follow::CLOCK);
		fol.bind(RESET_INPUT, follow::RESET);
		std::vector<std::string> models, filters;
		for (int i = 0; i < NUM_MODELS; i++)
			models.push_back(MODEL_NAMES[i]);
		for (int i = 0; i < gmono::NUM_FILTERS; i++)
			filters.push_back(gmono::FILTER_NAMES[i]);
		configSwitch(MODEL_PARAM, 0.f, NUM_MODELS - 1, 0.f, "Model", models);
		configParam<TimbreQuantity>(TIMBRE_PARAM, 0.f, 1.f, 0.5f, "Timbre", "%", 0.f, 100.f);
		configParam<TimbreQuantity>(BITE_PARAM, 0.f, 1.f, 0.4f, "Bite", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.4f, "Width", "%", 0.f, 100.f);
		configParam(SUB_PARAM, 0.f, 1.f, 0.f, "Sub (an octave down)", "%", 0.f, 100.f);
		configSwitch(VMODE_PARAM, 0.f, NUM_MODES - 1, MODE_POLY, "Voice mode", {"Mono", "Legato", "Poly (8 voices)"});
		paramQuantities[VMODE_PARAM]->description =
		    "Mono: every note restarts. Legato: overlapping notes glide without a new attack. Poly: release tails overlap";
		configParam<GlideQuantity>(GLIDE_PARAM, 0.f, 1.f, 0.3f, "Glide time");
		paramQuantities[GLIDE_PARAM]->description = "On GENRA's SLIDE; with SLIDE unplugged in Mono/Legato on every note";
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accent amount", "%", 0.f, 100.f);
		paramQuantities[ACCENT_PARAM]->description = "What ACC does: louder, more filter envelope, more bite";
		configSwitch(FTYPE_PARAM, 0.f, gmono::NUM_FILTERS - 1, 0.f, "Filter type", filters);
		configParam<CutoffQuantity>(CUTOFF_PARAM, 0.f, 1.f, 0.7f, "Cutoff");
		configParam(RES_PARAM, 0.f, 1.f, 0.15f, "Resonance", "%", 0.f, 100.f);
		configParam(FENV_PARAM, -1.f, 1.f, 0.3f, "Filter envelope amount", "%", 0.f, 100.f);
		configParam(KEY_PARAM, 0.f, 1.f, 0.5f, "Key tracking", "%", 0.f, 100.f);
		configParam<TimeQuantity>(FATK_PARAM, 0.f, 1.f, 0.f, "Filter envelope attack");
		configParam<TimeQuantity>(FDEC_PARAM, 0.f, 1.f, 0.45f, "Filter envelope decay");
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam<TimeQuantity>(ATK_PARAM, 0.f, 1.f, 0.f, "Attack");
		configParam<TimeQuantity>(DEC_PARAM, 0.f, 1.f, 0.5f, "Decay");
		configParam(SUS_PARAM, 0.f, 1.f, 1.f, "Sustain", "%", 0.f, 100.f);
		configParam<TimeQuantity>(REL_PARAM, 0.f, 1.f, 0.55f, "Release");
		configParam(VDEPTH_PARAM, 0.f, 1.f, 0.f, "Vibrato depth", "%", 0.f, 100.f);
		configParam<VRateQuantity>(VRATE_PARAM, 0.f, 1.f, 0.5f, "Vibrato rate");
		configParam<VDelayQuantity>(VDELAY_PARAM, 0.f, 1.f, 0.4f, "Vibrato delay");
		paramQuantities[VDELAY_PARAM]->description = "Long notes start to swing after this time";
		configSwitch(EDIV_PARAM, 0.f, NUM_DIVS - 1, DIV_8D, "Echo time", {"1/16", "1/8", "1/8 dotted", "1/4"});
		paramQuantities[EDIV_PARAM]->description = "In time with CLOCK (120 BPM without)";
		configParam(CHORUS_PARAM, 0.f, 1.f, 0.2f, "Chorus", "%", 0.f, 100.f);
		configParam(ECHO_PARAM, 0.f, 1.f, 0.2f, "Echo level", "%", 0.f, 100.f);
		configParam(FDBK_PARAM, 0.f, 1.f, 0.35f, "Echo feedback", "%", 0.f, 100.f);
		configParam(LOFI_PARAM, 0.f, 1.f, 0.f, "Lo-fi (tape wow, fewer bits and samples)", "%", 0.f, 100.f);
		configParam(VERB_PARAM, 0.f, 1.f, 0.2f, "Reverb", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/Oct (polyphonic)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(VEL_INPUT, "Velocity (0–10 V)");
		configInput(ACC_INPUT, "Accent (gate, from GENRA's ACC)");
		configInput(SLIDE_INPUT, "Slide (from GENRA's SLIDE: glide into the next note)");
		configInput(CLOCK_INPUT, "Clock for the echo time (GENRA: 16ths, see the context menu)");
		configInput(MODEL_INPUT, "Model CV (0–10 V steps through the models, taken at each new note)");
		configInput(TIMBRE_INPUT, "Timbre CV");
		configInput(BITE_INPUT, "Bite CV");
		configInput(CUT_INPUT, "Cutoff CV (1 V/oct)");
		configInput(RES_INPUT, "Resonance CV");
		configInput(VIB_INPUT, "Mod: vibrato depth (like a mod wheel; GENRA's MOD)");
		configInput(RESET_INPUT, "Reset (the tempo is measured anew from the next clock)");
		configInput(ECHO_INPUT, "Echo level CV");
		configInput(LOFI_INPUT, "Lo-fi CV");
		configInput(ATK_INPUT, "Attack CV");
		configInput(REL_INPUT, "Release CV");
		configOutput(ENV_OUTPUT, "Envelope of the newest note (0–10 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		initPresets();
	}

	void onAdd(const AddEvent& e) override { engine->setSampleRate(APP->engine->getSampleRate()); }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine->setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine->allOff();
		bank.reset();
	}

	float kv(int param, int input) { return clamp(params[param].getValue() + inputs[input].getVoltage() * 0.1f, 0.f, 1.f); }

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		// the clock: BPM from the time between pulses
		sinceClock += args.sampleTime;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			// a restart: the pause before it is no clock interval
			clockSeen = false;
			sinceClock = 0.f;
		}
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (clockSeen && sinceClock > 0.005f && sinceClock < 4.f)
				bpm = clamp(60.f / (sinceClock * ppqn), 20.f, 400.f);
			clockSeen = true;
			sinceClock = 0.f;
		}
		if (!inputs[CLOCK_INPUT].isConnected() || sinceClock > 4.f) {
			clockSeen = inputs[CLOCK_INPUT].isConnected() && sinceClock <= 4.f;
			if (!inputs[CLOCK_INPUT].isConnected())
				bpm = 120.f;
		}

		if (--ctl <= 0) {
			ctl = 16;
			int mAdd = clamp((int) (clamp(inputs[MODEL_INPUT].getVoltage(), 0.f, 10.f) / 10.f * NUM_MODELS), 0, NUM_MODELS - 1);
			p.model = ((int) std::round(params[MODEL_PARAM].getValue()) + mAdd) % NUM_MODELS;
			modelNow = p.model;
			p.timbre = kv(TIMBRE_PARAM, TIMBRE_INPUT);
			p.bite = kv(BITE_PARAM, BITE_INPUT);
			p.width = params[WIDTH_PARAM].getValue();
			p.sub = params[SUB_PARAM].getValue();
			p.mode = (int) std::round(params[VMODE_PARAM].getValue());
			p.glide = params[GLIDE_PARAM].getValue();
			p.accent = params[ACCENT_PARAM].getValue();
			p.ftype = clamp((int) std::round(params[FTYPE_PARAM].getValue()), 0, gmono::NUM_FILTERS - 1);
			p.cutoff = params[CUTOFF_PARAM].getValue();
			p.cutCv = inputs[CUT_INPUT].getVoltage();
			p.res = params[RES_PARAM].getValue();
			p.resCv = inputs[RES_INPUT].getVoltage() * 0.1f;
			p.fenv = params[FENV_PARAM].getValue();
			p.key = params[KEY_PARAM].getValue();
			p.fatk = params[FATK_PARAM].getValue();
			p.fdec = params[FDEC_PARAM].getValue();
			p.drive = params[DRIVE_PARAM].getValue();
			p.atk = kv(ATK_PARAM, ATK_INPUT);
			p.dec = params[DEC_PARAM].getValue();
			p.sus = params[SUS_PARAM].getValue();
			p.rel = kv(REL_PARAM, REL_INPUT);
			p.vdepth = params[VDEPTH_PARAM].getValue();
			p.vibCv = inputs[VIB_INPUT].getVoltage() * 0.1f;
			p.vrate = params[VRATE_PARAM].getValue();
			p.vdelay = params[VDELAY_PARAM].getValue();
			p.div = clamp((int) std::round(params[EDIV_PARAM].getValue()), 0, NUM_DIVS - 1);
			p.chorus = params[CHORUS_PARAM].getValue();
			p.echo = kv(ECHO_PARAM, ECHO_INPUT);
			p.fdbk = params[FDBK_PARAM].getValue();
			p.lofi = kv(LOFI_PARAM, LOFI_INPUT);
			p.verb = params[VERB_PARAM].getValue();
			p.outGain = bank.gain;
		}

		int n = std::min(std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels()), MAXCH);
		bool vel = inputs[VEL_INPUT].isConnected();
		for (int c = 0; c < n; c++) {
			Channel& x = ch[c];
			x.voct = inputs[VOCT_INPUT].getPolyVoltage(c);
			float g = inputs[GATE_INPUT].getPolyVoltage(c);
			x.gate = g >= 1.f ? true : (g <= 0.1f ? false : x.gate);
			x.vel = vel ? clamp(inputs[VEL_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f) : 0.8f;
			x.acc = inputs[ACC_INPUT].getPolyVoltage(c) >= 1.f;
			x.slide = inputs[SLIDE_INPUT].getPolyVoltage(c) >= 1.f;
		}
		float l, r, env;
		float echoS = 60.f / bpm * DIV_BEATS[p.div];
		engine->process(p, ch, n, inputs[SLIDE_INPUT].isConnected(), echoS, l, r, env);
		// L alone: the mono sum
		outputs[L_OUTPUT].setVoltage(outputs[R_OUTPUT].isConnected() || pulled(args.frame) ? l : 0.5f * (l + r));
		outputs[R_OUTPUT].setVoltage(r);
		outputs[ENV_OUTPUT].setVoltage(10.f * env);

		// the scope: every 24th sample
		if (++scopeDiv >= 24) {
			scopeDiv = 0;
			scope[scopeW] = 0.5f * (l + r);
			if (++scopeW >= SCOPE) {
				scopeW = 0;
				std::memcpy(showScope, scope, sizeof(scope));
			}
		}
	}

	json_t* dataToJson() override {
		json_t* j = json_object();
		json_object_set_new(j, "ppqn", json_integer(ppqn));
		bank.toJson(j);
		fol.toJson(j);
		return j;
	}
	void dataFromJson(json_t* j) override {
		fol.fromJson(j);
		if (json_t* v = json_object_get(j, "ppqn"))
			ppqn = std::max(1, (int) json_integer_value(v));
		bank.fromJson(j);
	}
};

// ---------------------------------------------------------------- widgets
/** A selection field (model, filter type): shows the name and a short info, a click opens the list, the wheel steps. */
struct NameField : app::ParamWidget {
	const char* const* names = nullptr;
	const char* const* info = nullptr;
	int count = 1;
	std::string title, undo;
	float infoRight = 6.f;  // mm from the right edge

	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, count - 1) : 0;
	}
	void set(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float old = pq->getValue();
		if ((int) std::round(old) == t)
			return;
		pq->setValue(t);
		history::ParamChange* h = new history::ParamChange;
		h->name = undo;
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = t;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			rack::ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel(title));
			for (int i = 0; i < count; i++)
				menu->addChild(createCheckMenuItem(names[i], info[i], [=]() { return value() == i; }, [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, count - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int t = module ? value() : 0;
			auto f = ui::boldFont();
			float y = box.size.y / 2 + 2.6f;
			ui::text(args.vg, f, mm2px(2.f), y, names[t], 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			ui::text(args.vg, f, box.size.x - mm2px(infoRight), y - 0.3f, string::uppercase(info[t]), 5.6f, ui::DIM,
			         NVG_ALIGN_RIGHT, 0.7f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

struct Display : Widget {
	Funke* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		float W = box.size.x;
		int m = module ? module->modelNow : RHODES;
		ui::text(vg, bold, mm2px(4.f), mm2px(4.5f), string::f("MODEL %d / %d", m + 1, NUM_MODELS), 5.6f, ui::DIM,
		         NVG_ALIGN_LEFT, 1.1f);
		ui::text(vg, bold, mm2px(4.f), mm2px(10.5f), MODEL_NAMES[m], 15.5f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);
		ui::text(vg, bold, mm2px(4.f), mm2px(13.8f), string::uppercase(MODEL_INFO[m]), 5.6f, ACCENT, NVG_ALIGN_LEFT, 1.2f);

		// scope
		float sx = mm2px(57.f), sw = mm2px(46.f), cy = mm2px(7.f), sh = mm2px(4.6f);
		nvgBeginPath(vg);
		nvgMoveTo(vg, sx, cy);
		nvgLineTo(vg, sx + sw, cy);
		nvgStrokeColor(vg, ui::EDGE);
		nvgStrokeWidth(vg, 0.6f);
		nvgStroke(vg);
		nvgBeginPath(vg);
		for (int i = 0; i < SCOPE; i++) {
			float v = module ? module->showScope[i] / 5.f : 0.6f * std::sin(i * 0.15f) * std::exp(-i / 200.f);
			float x = sx + i * sw / (SCOPE - 1), y = cy - clamp(v, -1.f, 1.f) * sh;
			if (i == 0)
				nvgMoveTo(vg, x, y);
			else
				nvgLineTo(vg, x, y);
		}
		nvgStrokeColor(vg, ACCENT);
		nvgStrokeWidth(vg, 1.3f);
		nvgLineJoin(vg, NVG_ROUND);
		nvgStroke(vg);
		if (module) {
			const Params& p = module->p;
			static const char* const MODES[NUM_MODES] = {"MONO", "LEGATO", "POLY"};
			std::string line = std::string(gmono::FILTER_NAMES[p.ftype]) + " · ECHO " + DIV_NAMES[p.div] + " · "
			                   + MODES[clamp(p.mode, 0, NUM_MODES - 1)];
			ui::text(vg, bold, sx, mm2px(13.8f), line, 5.6f, ui::DIM, NVG_ALIGN_LEFT, 1.f);
		}

		// tempo, note, glide / accent
		float bpm = module ? module->bpm : 120.f;
		bool clocked = module && module->inputs[Funke::CLOCK_INPUT].isConnected();
		ui::text(vg, bold, W - mm2px(4.f), mm2px(4.5f), string::f(clocked ? "%.0f BPM" : "%.0f BPM FREE", bpm), 5.6f,
		         ui::DIM, NVG_ALIGN_RIGHT, 1.1f);
		char nn[8] = "C4";
		if (module)
			noteName(module->engine->lastNote, nn);
		ui::text(vg, bold, W - mm2px(4.f), mm2px(10.5f), nn, 15.5f, ui::WHITE, NVG_ALIGN_RIGHT, 0.8f);
		bool g = module && module->engine->showGlide, a = module && module->engine->showAcc;
		float x = W - mm2px(4.f);
		ui::text(vg, bold, x, mm2px(13.8f), "ACC", 5.6f, a ? ACCENT : ui::LAMP, NVG_ALIGN_RIGHT, 1.1f);
		ui::text(vg, bold, x - mm2px(7.5f), mm2px(13.8f), "GLIDE", 5.6f, g ? ACCENT : ui::LAMP, NVG_ALIGN_RIGHT, 1.1f);
		Widget::drawLayer(args, layer);
	}
};

struct FunkeWidget : ModuleWidget {
	FunkeWidget(Funke* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Funke.svg")));
		follow::addStrip(this, module, {mm2px(Vec(L::VOCT[0], L::VOCT[1])), mm2px(Vec(L::GATE[0], L::GATE[1])), mm2px(Vec(L::VEL[0], L::VEL[1])), mm2px(Vec(L::ACC[0], L::ACC[1])), mm2px(Vec(L::SLIDE[0], L::SLIDE[1])), mm2px(Vec(L::IN_VIB[0], L::IN_VIB[1])), mm2px(Vec(L::CLOCK[0], L::CLOCK[1])), mm2px(Vec(L::IN_RESET[0], L::IN_RESET[1]))}); // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);

		using M = Funke;
		auto field = [&](const float* r, int id, const char* const* names, const char* const* info, int count,
		                 const char* title, const char* undo) {
			NameField* f = createParam<NameField>(mm2px(Vec(r[0], r[1])), module, id);
			f->box.size = mm2px(Vec(r[2], r[3]));
			f->names = names, f->info = info, f->count = count, f->title = title, f->undo = undo;
			addParam(f);
		};
		field(L::MODELF, M::MODEL_PARAM, MODEL_NAMES, MODEL_INFO, NUM_MODELS, "Model", "change model");
		field(L::FTYPEF, M::FTYPE_PARAM, gmono::FILTER_NAMES, gmono::FILTER_INFO, gmono::NUM_FILTERS, "Filter type",
		      "change filter type");
		genra_presets::PresetField* pf = createWidget<genra_presets::PresetField>(mm2px(Vec(L::PRESETF[0], L::PRESETF[1])));
		pf->box.size = mm2px(Vec(L::PRESETF[2], L::PRESETF[3]));
		pf->bank = module ? &module->bank : nullptr;
		pf->accent = ACCENT;
		pf->sample = PRESETS[0].name;
		addChild(pf);

		auto large = [&](const float* p, int id) { addParam(createParamCentered<GLargeKnob>(ui::mm(p), module, id)); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto lamps = [&](const float* p, int id, int n) {
			ui::LampRow* w = createParam<ui::LampRow>(Vec(0, 0), module, id);
			w->place(p, n);
			w->accent = ACCENT;
			addParam(w);
		};

		large(L::TIMBRE, M::TIMBRE_PARAM);
		knob(L::BITE, M::BITE_PARAM);
		knob(L::WIDTH, M::WIDTH_PARAM);
		knob(L::SUB, M::SUB_PARAM);
		lamps(L::VMODE, M::VMODE_PARAM, NUM_MODES);
		small(L::GLIDE, M::GLIDE_PARAM);
		small(L::ACCENT, M::ACCENT_PARAM);

		large(L::CUTOFF, M::CUTOFF_PARAM);
		knob(L::RES, M::RES_PARAM);
		knob(L::FENV, M::FENV_PARAM);
		knob(L::KEY, M::KEY_PARAM);
		small(L::FATK, M::FATK_PARAM);
		small(L::FDEC, M::FDEC_PARAM);
		small(L::DRIVE, M::DRIVE_PARAM);

		small(L::ATK, M::ATK_PARAM);
		small(L::DEC, M::DEC_PARAM);
		small(L::SUS, M::SUS_PARAM);
		small(L::REL, M::REL_PARAM);
		small(L::VDEPTH, M::VDEPTH_PARAM);
		small(L::VRATE, M::VRATE_PARAM);
		small(L::VDELAY, M::VDELAY_PARAM);
		lamps(L::EDIV, M::EDIV_PARAM, NUM_DIVS);
		small(L::CHORUS, M::CHORUS_PARAM);
		small(L::ECHO, M::ECHO_PARAM);
		small(L::FDBK, M::FDBK_PARAM);
		small(L::LOFI, M::LOFI_PARAM);
		small(L::VERB, M::VERB_PARAM);

		const float* ins[M::NUM_INPUTS] = {L::VOCT,    L::GATE,      L::VEL,     L::ACC,    L::SLIDE,  L::CLOCK,
		                                   L::IN_MODEL, L::IN_TIMBRE, L::IN_BITE, L::IN_CUT, L::IN_RES, L::IN_VIB,
		                                   L::IN_ECHO, L::IN_LOFI,   L::IN_ATK,  L::IN_REL,  L::IN_RESET};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_ENV, L::OUT_L, L::OUT_R};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Funke* m = dynamic_cast<Funke*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		genra_presets::appendMenu(menu, &m->bank);
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
		    "CLOCK input", {"Quarters (1 PPQN)", "8ths (2 PPQN)", "16ths (4 PPQN, GENRA)", "24 PPQN"},
		    [=]() {
			    for (int i = 0; i < 4; i++)
				    if (PPQN_CHOICES[i] == m->ppqn)
					    return (size_t) i;
			    return (size_t) 2;
		    },
		    [=](size_t i) { m->ppqn = PPQN_CHOICES[i]; }));
		menu->addChild(createMenuLabel("Patch GENRA's melody outputs: V/OCT, GATE, VEL, ACC, SLIDE, and CLOCK"));
		menu->addChild(createMenuLabel(MODEL_HELP[m->modelNow]));
	}
};

}  // namespace genra_funke

Model* modelFunke = createModel<genra_funke::Funke, genra_funke::FunkeWidget>("Funke");
