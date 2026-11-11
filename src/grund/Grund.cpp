// SPDX-License-Identifier: GPL-3.0-only
// GRUND – the bass voice for GENRA's bass track, in the GENRA look (accent: GENRA's bass green).
// Sound engine: GrundDsp.hpp, presets: GrundPresets.hpp, expander: GrundFx.*.
// Panel: tools/panels/grund_panel.py (res/Grund.svg + GrundLayout.hpp).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "GrundDsp.hpp"
#include "GrundFx.hpp"
#include "GrundLayout.hpp"
#include "GrundPresets.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>

namespace genra_grund {

namespace L = glayout;
namespace ui = genra_ui;
using namespace ggrund;

// keep in step with tools/panels/grund_panel.py (ACC)
#define GRUND_RGB 0xb8, 0xf2, 0x3a
static const NVGcolor ACCENT = nvgRGB(GRUND_RGB);
static const int SCOPE = 128;

struct Grund : Module, follow::Host {
	enum ParamId {
		MODEL_PARAM, TIMBRE_PARAM, BITE_PARAM, BODY_PARAM, SUB_PARAM, MUTE_PARAM, GLIDE_PARAM, ACCENT_PARAM,
		VMODE_PARAM, AMP_PARAM, CUTOFF_PARAM, RES_PARAM, FENV_PARAM, FDEC_PARAM, DRIVE_PARAM, COMP_PARAM, KEY_PARAM,
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM, CHORUS_PARAM, OCTAVE_PARAM, WIDTH_PARAM, VERB_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, ACC_INPUT, SLIDE_INPUT, MOD_INPUT,
		MODEL_INPUT, TIMBRE_INPUT, BITE_INPUT, CUT_INPUT, MUTE_INPUT, DRIVE_INPUT, SUB_INPUT, ATK_INPUT, REL_INPUT,
		NUM_INPUTS
	};
	enum OutputId { ENV_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	static_assert(NUM_PRESET_PARAMS == NUM_PARAMS, "presets must cover every knob");

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
	struct ToneQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float f = 20.f * std::pow(1000.f, getValue());
			return f < 1000.f ? string::f("%.0f", f) : string::f("%.2f k", f / 1000.f);
		}
		std::string getUnit() override { return "Hz"; }
	};
	struct HelpQuantity : ParamQuantity {
		std::string getDescription() override {
			Grund* m = dynamic_cast<Grund*>(module);
			return m ? MODEL_HELP[m->modelNow] : "";
		}
	};

	std::unique_ptr<ggrund::Engine> engine{new ggrund::Engine};
	ggrund::Params p;
	FxSettings fxStore;  // GRUND FX's knobs (kept here: the presets and the patch hold them, with or without it)
	bool fxWasLinked = false;
	int ctl = 0;
	int modelNow = FINGER;
	float scope[SCOPE] = {};
	int scopeW = 0, scopeDiv = 0;
	float showScope[SCOPE] = {};

	// ---- presets: the factory list, then the user presets from userDir()
	struct UserPreset {
		std::string name;
		float v[NUM_PARAMS];
		FxSettings fx;
		float gain = 1.f;
	};
	std::vector<UserPreset> user;
	int preset = -1;
	std::string presetName;
	float loaded[NUM_PARAMS];
	float presetGain = 1.f;  // the loaded preset's output level (not a knob)

	static std::string userDir() { return asset::user("Genra/GrundPresets"); }
	int count() { return NUM_PRESETS + (int) user.size(); }
	std::string nameOf(int i) {
		if (i < 0 || i >= count())
			return "INIT";
		return i < NUM_PRESETS ? PRESETS[i].name : user[i - NUM_PRESETS].name;
	}
	void rememberLoaded() {
		for (int i = 0; i < NUM_PARAMS; i++)
			loaded[i] = params[i].getValue();
	}
	bool edited() {
		if (preset < 0)
			return false;
		for (int i = 0; i < NUM_PARAMS; i++)
			if (std::fabs(params[i].getValue() - loaded[i]) > 1e-4f)
				return true;
		return false;
	}
	gfx::GrundFx* fxModule() {
		Module* m = rightExpander.module;
		return (m && m->model == modelGrundFx) ? static_cast<gfx::GrundFx*>(m) : nullptr;
	}
	/** Loads preset i (undoable, together with the expander's knobs). */
	void load(int i) {
		if (i < 0 || i >= count())
			return;
		gfx::GrundFx* x = fxModule();
		json_t* oldJ = toJson();
		json_t* oldX = x ? x->toJson() : nullptr;
		float v[NUM_PARAMS];
		FxSettings f;
		if (i < NUM_PRESETS) {
			PRESETS[i].values(v);
			f = PRESETS[i].fx;
		}
		else {
			std::copy(user[i - NUM_PRESETS].v, user[i - NUM_PRESETS].v + NUM_PARAMS, v);
			f = user[i - NUM_PRESETS].fx;
		}
		for (int k = 0; k < NUM_PARAMS; k++)
			params[k].setValue(v[k]);
		fxStore = f;
		if (x)
			gfx::writeKnobs(x, f);
		preset = i;
		presetName = nameOf(i);
		presetGain = i < NUM_PRESETS ? PRESETS[i].gain : user[i - NUM_PRESETS].gain;
		rememberLoaded();
		history::ComplexAction* ca = new history::ComplexAction;
		ca->name = "load preset " + presetName;
		history::ModuleChange* h = new history::ModuleChange;
		h->name = ca->name;
		h->moduleId = id;
		h->oldModuleJ = oldJ;
		h->newModuleJ = toJson();
		ca->push(h);
		if (x) {
			history::ModuleChange* hx = new history::ModuleChange;
			hx->name = ca->name;
			hx->moduleId = x->id;
			hx->oldModuleJ = oldX;
			hx->newModuleJ = x->toJson();
			ca->push(hx);
		}
		APP->history->push(ca);
	}
	int stepPreset(int d) {
		int n = count();
		return preset < 0 ? (d > 0 ? 0 : n - 1) : (preset + d + n) % n;
	}
	void scanUser() {
		user.clear();
		std::string dir = userDir();
		if (!system::isDirectory(dir))
			return;
		std::vector<std::string> files = system::getEntries(dir);
		std::sort(files.begin(), files.end());
		for (const std::string& file : files) {
			if (system::getExtension(file) != ".json")
				continue;
			json_t* j = json_load_file(file.c_str(), 0, nullptr);
			if (!j)
				continue;
			json_t* nameJ = json_object_get(j, "name");
			json_t* valsJ = json_object_get(j, "params");
			int n = json_is_array(valsJ) ? (int) json_array_size(valsJ) : 0;
			if (json_is_string(nameJ) && n > 0) {
				UserPreset u;
				u.name = json_string_value(nameJ);
				for (int i = 0; i < NUM_PARAMS; i++)
					u.v[i] = i < n ? (float) json_number_value(json_array_get(valsJ, i)) : paramQuantities[i]->getDefaultValue();
				fxFromJson(json_object_get(j, "fx"), u.fx);
				json_t* gJ = json_object_get(j, "gain");
				u.gain = json_is_number(gJ) ? (float) json_number_value(gJ) : 1.f;
				user.push_back(u);
			}
			json_decref(j);
		}
		if (preset >= NUM_PRESETS) {
			preset = -1;
			for (size_t i = 0; i < user.size(); i++)
				if (user[i].name == presetName)
					preset = NUM_PRESETS + i;
		}
	}
	static json_t* fxToJson(const FxSettings& f) {
		float a[FxSettings::N];
		f.toArray(a);
		json_t* j = json_array();
		for (int i = 0; i < FxSettings::N; i++)
			json_array_append_new(j, json_real(a[i]));
		return j;
	}
	static void fxFromJson(json_t* j, FxSettings& f) {
		if (!json_is_array(j))
			return;
		float a[FxSettings::N];
		int n = std::min((int) json_array_size(j), (int) FxSettings::N);
		for (int i = 0; i < n; i++)
			a[i] = clamp((float) json_number_value(json_array_get(j, i)), 0.f, (float) NUM_DRIVES);
		f.fromArray(a, n);
	}
	void saveUser(std::string name) {
		name = string::trim(name);
		if (name.empty())
			return;
		name = string::uppercase(name);
		std::string file;
		for (char c : name)
			file += std::isalnum((unsigned char) c) ? (char) std::tolower(c) : '_';
		system::createDirectories(userDir());
		json_t* j = json_object();
		json_object_set_new(j, "name", json_string(name.c_str()));
		json_t* a = json_array();
		for (int i = 0; i < NUM_PARAMS; i++)
			json_array_append_new(a, json_real(params[i].getValue()));
		json_object_set_new(j, "params", a);
		json_object_set_new(j, "fx", fxToJson(fxStore));
		json_object_set_new(j, "gain", json_real(presetGain));
		json_dump_file(j, system::join(userDir(), file + ".json").c_str(), JSON_INDENT(2));
		json_decref(j);
		presetName = name;
		preset = NUM_PRESETS;
		scanUser();
		rememberLoaded();
	}

	Grund() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		srcName = "GRUND";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		fol.bind(ACC_INPUT, follow::ACC);
		fol.bind(SLIDE_INPUT, follow::SLIDE);
		fol.bind(MOD_INPUT, follow::MOD);
		std::vector<std::string> models, amps;
		for (int i = 0; i < NUM_MODELS; i++)
			models.push_back(MODEL_NAMES[i]);
		for (int i = 0; i < NUM_AMPS; i++)
			amps.push_back(AMP_NAMES[i]);
		ggrund::Params d;
		configSwitch(MODEL_PARAM, 0.f, NUM_MODELS - 1, d.model, "Model", models);
		configParam<HelpQuantity>(TIMBRE_PARAM, 0.f, 1.f, d.timbre, "Timbre", "%", 0.f, 100.f);
		configParam<HelpQuantity>(BITE_PARAM, 0.f, 1.f, d.bite, "Bite", "%", 0.f, 100.f);
		configParam(BODY_PARAM, 0.f, 1.f, d.body, "Body (wood, cabinet)", "%", 0.f, 100.f);
		configParam(SUB_PARAM, 0.f, 1.f, d.sub, "Sub (an octave down)", "%", 0.f, 100.f);
		configParam(MUTE_PARAM, 0.f, 1.f, d.mute, "Mute (the palm on the strings: shorter, drier)", "%", 0.f, 100.f);
		configParam<GlideQuantity>(GLIDE_PARAM, 0.f, 1.f, d.glide, "Glide time");
		paramQuantities[GLIDE_PARAM]->description = "On GENRA's SLIDE; with SLIDE unplugged on every note";
		configParam(ACCENT_PARAM, 0.f, 1.f, d.accent, "Accent amount", "%", 0.f, 100.f);
		paramQuantities[ACCENT_PARAM]->description = "What ACC does: louder, more tone envelope, more bite (SLAP: pop)";
		configSwitch(VMODE_PARAM, 0.f, NUM_MODES - 1, d.mode, "Voice mode", {"Mono", "Legato"});
		paramQuantities[VMODE_PARAM]->description = "Mono: every note restarts. Legato: overlapping notes glide without a new attack";
		configSwitch(AMP_PARAM, 0.f, NUM_AMPS - 1, d.amp, "Amp", amps);
		configParam<ToneQuantity>(CUTOFF_PARAM, 0.f, 1.f, d.tone, "Tone (the amp's low-pass)");
		configParam(RES_PARAM, 0.f, 1.f, d.res, "Resonance", "%", 0.f, 100.f);
		configParam(FENV_PARAM, 0.f, 1.f, d.fenv, "Tone envelope amount", "%", 0.f, 100.f);
		configParam<TimeQuantity>(FDEC_PARAM, 0.f, 1.f, d.fdec, "Tone envelope decay");
		configParam(DRIVE_PARAM, 0.f, 1.f, d.drive, "Drive (the amp)", "%", 0.f, 100.f);
		configParam(COMP_PARAM, 0.f, 1.f, d.comp, "Compressor (details on GRUND FX)", "%", 0.f, 100.f);
		configParam(KEY_PARAM, 0.f, 1.f, d.key, "Key tracking of the tone", "%", 0.f, 100.f);
		configParam<TimeQuantity>(ATK_PARAM, 0.f, 1.f, d.atk, "Attack");
		configParam<TimeQuantity>(DEC_PARAM, 0.f, 1.f, d.dec, "Decay");
		configParam(SUS_PARAM, 0.f, 1.f, d.sus, "Sustain", "%", 0.f, 100.f);
		configParam<TimeQuantity>(REL_PARAM, 0.f, 1.f, d.rel, "Release");
		configParam(CHORUS_PARAM, 0.f, 1.f, d.chorus, "Chorus", "%", 0.f, 100.f);
		configParam(OCTAVE_PARAM, 0.f, 1.f, d.octave, "Octave (an octaver: an octave down mixed in)", "%", 0.f, 100.f);
		configParam(WIDTH_PARAM, 0.f, 1.f, d.width, "Width (the highs only)", "%", 0.f, 100.f);
		configParam(VERB_PARAM, 0.f, 1.f, d.verb, "Reverb", "%", 0.f, 100.f);

		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(VEL_INPUT, "Velocity (0–10 V)");
		configInput(ACC_INPUT, "Accent (gate, from GENRA's ACC)");
		configInput(SLIDE_INPUT, "Slide (from GENRA's SLIDE: glide into the next note, without a new attack)");
		configInput(MOD_INPUT, "Mod: timbre (like a mod wheel; GENRA's MOD)");
		configInput(MODEL_INPUT, "Model CV (0–10 V steps through the models)");
		configInput(TIMBRE_INPUT, "Timbre CV");
		configInput(BITE_INPUT, "Bite CV");
		configInput(CUT_INPUT, "Tone CV (1 V/oct)");
		configInput(MUTE_INPUT, "Mute CV");
		configInput(DRIVE_INPUT, "Drive CV");
		configInput(SUB_INPUT, "Sub CV");
		configInput(ATK_INPUT, "Attack CV");
		configInput(REL_INPUT, "Release CV");
		configOutput(ENV_OUTPUT, "Envelope (0–10 V)");
		configOutput(L_OUTPUT, "Left (mono when R is not patched)");
		configOutput(R_OUTPUT, "Right");
		rememberLoaded();
		scanUser();
	}

	void onAdd(const AddEvent& e) override { engine->setSampleRate(APP->engine->getSampleRate()); }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { engine->setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		engine->allOff();
		fxStore = FxSettings();
		if (gfx::GrundFx* x = fxModule())
			gfx::writeKnobs(x, fxStore);
		preset = -1;
		presetName.clear();
		presetGain = 1.f;
		rememberLoaded();
	}
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }
	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); }  // a real cable wins (Follow.hpp)

	float kv(int param, int input) { return clamp(params[param].getValue() + inputs[input].getVoltage() * 0.1f, 0.f, 1.f); }

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame);  // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		gfx::GrundFx* x = fxModule();
		if (--ctl <= 0) {
			ctl = 16;
			int mAdd = clamp((int) (clamp(inputs[MODEL_INPUT].getVoltage(), 0.f, 10.f) / 10.f * NUM_MODELS), 0, NUM_MODELS - 1);
			p.model = ((int) std::round(params[MODEL_PARAM].getValue()) + mAdd) % NUM_MODELS;
			modelNow = p.model;
			p.timbre = clamp(params[TIMBRE_PARAM].getValue() + (inputs[TIMBRE_INPUT].getVoltage() + inputs[MOD_INPUT].getVoltage()) * 0.1f, 0.f, 1.f);
			p.bite = kv(BITE_PARAM, BITE_INPUT);
			p.body = params[BODY_PARAM].getValue();
			p.sub = kv(SUB_PARAM, SUB_INPUT);
			p.mute = kv(MUTE_PARAM, MUTE_INPUT);
			p.glide = params[GLIDE_PARAM].getValue();
			p.accent = params[ACCENT_PARAM].getValue();
			p.mode = clamp((int) std::round(params[VMODE_PARAM].getValue()), 0, NUM_MODES - 1);
			p.amp = clamp((int) std::round(params[AMP_PARAM].getValue()), 0, NUM_AMPS - 1);
			p.tone = params[CUTOFF_PARAM].getValue();
			p.cutCv = inputs[CUT_INPUT].getVoltage();
			p.res = params[RES_PARAM].getValue();
			p.fenv = params[FENV_PARAM].getValue();
			p.fdec = params[FDEC_PARAM].getValue();
			p.drive = kv(DRIVE_PARAM, DRIVE_INPUT);
			p.comp = params[COMP_PARAM].getValue();
			p.key = params[KEY_PARAM].getValue();
			p.atk = kv(ATK_PARAM, ATK_INPUT);
			p.dec = params[DEC_PARAM].getValue();
			p.sus = params[SUS_PARAM].getValue();
			p.rel = kv(REL_PARAM, REL_INPUT);
			p.chorus = params[CHORUS_PARAM].getValue();
			p.octave = params[OCTAVE_PARAM].getValue();
			p.width = params[WIDTH_PARAM].getValue();
			p.verb = params[VERB_PARAM].getValue();
			p.outGain = presetGain;
			// GRUND FX: just docked → it takes GRUND's settings; docked → GRUND takes its knobs (+ CV)
			if (x && !fxWasLinked)
				gfx::writeKnobs(x, fxStore);
			fxWasLinked = x != nullptr;
			if (x) {
				gfx::readKnobs(x, fxStore);
				p.fx = fxStore;
				gfx::addCv(x, p.fx);
			}
			else
				p.fx = fxStore;
		}
		ggrund::Input in;
		in.voct = inputs[VOCT_INPUT].getVoltage();
		float g = inputs[GATE_INPUT].getVoltage();
		in.gate = g >= 1.f ? true : (g <= 0.1f ? false : lastGate);
		lastGate = in.gate;
		in.vel = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getVoltage() * 0.1f, 0.f, 1.f) : 0.8f;
		in.acc = inputs[ACC_INPUT].getVoltage() >= 1.f;
		in.slideConnected = inputs[SLIDE_INPUT].isConnected();
		in.slide = inputs[SLIDE_INPUT].getVoltage() >= 1.f;
		if (x) {
			in.scConnected = x->inputs[gfx::IN_SC].isConnected();
			in.sc = x->inputs[gfx::IN_SC].getVoltage();
		}
		float l, r, env;
		engine->process(p, in, l, r, env);
		if (x)
			x->gr.store(engine->bus->gr, std::memory_order_relaxed);
		outputs[L_OUTPUT].setVoltage(outputs[R_OUTPUT].isConnected() || pulled(args.frame) ? l : 0.5f * (l + r));
		outputs[R_OUTPUT].setVoltage(r);
		outputs[ENV_OUTPUT].setVoltage(10.f * env);
		if (++scopeDiv >= 24) {
			scopeDiv = 0;
			scope[scopeW] = 0.5f * (l + r);
			if (++scopeW >= SCOPE) {
				scopeW = 0;
				std::memcpy(showScope, scope, sizeof(scope));
			}
		}
	}
	bool lastGate = false;

	// presets the followed GENRA may switch (NEW picks one for the genre; UI thread)
	int hostPresetCount() override { return count(); }
	void hostPresetInfo(int i, char* name, int nl, char* style, int sl) override {
		follow::putStr(name, nl, nameOf(i).c_str());
		follow::putStr(style, sl, i >= 0 && i < NUM_PRESETS ? PRESETS[i].style : "");
	}
	int hostPresetCurrent() override { return preset; }
	void hostPresetLoad(int i) override { load(i); }

	json_t* dataToJson() override {
		json_t* j = json_object();
		json_object_set_new(j, "preset", json_string(preset < 0 ? "" : presetName.c_str()));
		json_t* a = json_array();
		for (int i = 0; i < NUM_PARAMS; i++)
			json_array_append_new(a, json_real(loaded[i]));
		json_object_set_new(j, "loaded", a);
		json_object_set_new(j, "fx", fxToJson(fxStore));
		json_object_set_new(j, "presetGain", json_real(presetGain));
		fol.toJson(j);
		return j;
	}
	void dataFromJson(json_t* j) override {
		fol.fromJson(j);
		fxFromJson(json_object_get(j, "fx"), fxStore);
		fxWasLinked = true;  // a loaded expander keeps its own knobs (they were saved with the same values)
		json_t* pJ = json_object_get(j, "preset");
		preset = -1;
		presetName = json_is_string(pJ) ? json_string_value(pJ) : "";
		for (int i = 0; i < count() && !presetName.empty(); i++)
			if (nameOf(i) == presetName) {
				preset = i;
				break;
			}
		rememberLoaded();
		json_t* a = json_object_get(j, "loaded");
		int n = json_is_array(a) ? (int) json_array_size(a) : 0;
		for (int i = 0; i < n && i < NUM_PARAMS; i++)
			loaded[i] = (float) json_number_value(json_array_get(a, i));
		// the output level: as saved; a patch from before keeps the level of its factory preset
		json_t* gJ = json_object_get(j, "presetGain");
		presetGain = json_is_number(gJ) ? (float) json_number_value(gJ) : (preset >= 0 && preset < NUM_PRESETS ? PRESETS[preset].gain : 1.f);
	}
};

// ---------------------------------------------------------------- widgets
/** A selection field (model, amp): name and short info, click = list, wheel = step. */
struct NameField : app::ParamWidget {
	const char* const* names = nullptr;
	const char* const* info = nullptr;
	int count = 1;
	std::string title, undo;

	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, count - 1) : 0;
	}
	void set(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq || (int) std::round(pq->getValue()) == t)
			return;
		float old = pq->getValue();
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
			ui::text(args.vg, f, box.size.x - mm2px(6.f), y - 0.3f, string::uppercase(info[t]), 5.6f, ui::DIM,
			         NVG_ALIGN_RIGHT, 0.7f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** The preset list: one submenu per style, the user presets at the end. */
static void appendPresets(Menu* menu, Grund* m) {
	for (int i = 0; i < NUM_PRESETS;) {
		std::string st = PRESETS[i].style;
		int j = i;
		while (j < NUM_PRESETS && st == PRESETS[j].style)
			j++;
		bool here = m->preset >= i && m->preset < j;
		menu->addChild(createSubmenuItem(st, here ? "✔" : "", [=](Menu* sub) {
			for (int k = i; k < j; k++)
				sub->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, PRESETS[k].name), MODEL_NAMES[PRESETS[k].model],
				                                  [=]() { return m->preset == k; }, [=]() { m->load(k); }));
		}));
		i = j;
	}
	bool here = m->preset >= NUM_PRESETS;
	menu->addChild(createSubmenuItem("USER", here ? "✔" : string::f("%d", (int) m->user.size()), [=](Menu* sub) {
		if (m->user.empty())
			sub->addChild(createMenuLabel("Save one with right-click → Save preset as …"));
		for (int k = NUM_PRESETS; k < m->count(); k++)
			sub->addChild(createCheckMenuItem(m->nameOf(k), "", [=]() { return m->preset == k; }, [=]() { m->load(k); }));
	}));
}

/** The preset field in the header: number and name (a dot when a knob has moved), click = list, wheel = step. */
struct PresetField : OpaqueWidget {
	Grund* module = nullptr;
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			module->scanUser();
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Preset"));
			appendPresets(menu, module);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			module->load(module->stepPreset(e.scrollDelta.y < 0.f ? 1 : -1));
			e.consume(this);
			return;
		}
		OpaqueWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int idx = module ? module->preset : 0;
			std::string name = module ? module->nameOf(idx) : PRESETS[0].name;
			std::string num = idx < 0 ? "--" : idx >= NUM_PRESETS ? "U" : string::f("%02d", idx + 1);
			std::string style = idx >= 0 && idx < NUM_PRESETS ? PRESETS[idx].style : "";
			float y = box.size.y / 2 + 2.6f;
			auto f = ui::boldFont();
			ui::text(args.vg, f, mm2px(2.f), y, num, 6.4f, ACCENT, NVG_ALIGN_LEFT, 0.8f);
			float w = ui::text(args.vg, f, mm2px(6.6f), y, name, 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			if (module && module->edited()) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, mm2px(6.6f) + w + mm2px(1.4f), y - 2.6f, mm2px(0.55f));
				nvgFillColor(args.vg, ACCENT);
				nvgFill(args.vg);
			}
			if (!style.empty())
				ui::text(args.vg, f, box.size.x - mm2px(6.f), y - 0.3f, style, 5.2f, ui::DIM, NVG_ALIGN_RIGHT, 0.6f);
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** The name field of "Save preset as …": Enter saves. */
struct SaveField : rack::ui::TextField {
	Grund* module = nullptr;
	void onAction(const ActionEvent& e) override {
		if (module)
			module->saveUser(text);
		if (rack::ui::MenuOverlay* o = getAncestorOfType<rack::ui::MenuOverlay>())
			o->requestDelete();
		e.consume(this);
	}
};

struct Display : Widget {
	Grund* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		float W = box.size.x;
		int m = module ? module->modelNow : FINGER;
		ui::text(vg, bold, mm2px(4.f), mm2px(4.5f), string::f("MODEL %d / %d", m + 1, NUM_MODELS), 5.6f, ui::DIM,
		         NVG_ALIGN_LEFT, 1.1f);
		ui::text(vg, bold, mm2px(4.f), mm2px(10.5f), MODEL_NAMES[m], 15.5f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);
		ui::text(vg, bold, mm2px(4.f), mm2px(13.8f), string::uppercase(MODEL_INFO[m]), 5.6f, ACCENT, NVG_ALIGN_LEFT, 1.2f);

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
			const ggrund::Params& p = module->p;
			std::string line = std::string(AMP_NAMES[clamp(p.amp, 0, NUM_AMPS - 1)]);
			if (p.comp > 0.01f && p.fx.compOn)
				line += " · COMP";
			if (module->fxModule())
				line += " · FX";
			line += p.mode == MODE_LEGATO ? " · LEGATO" : " · MONO";
			ui::text(vg, bold, sx, mm2px(13.8f), line, 5.6f, ui::DIM, NVG_ALIGN_LEFT, 1.f);
		}
		ui::text(vg, bold, W - mm2px(4.f), mm2px(4.5f), "BASS", 5.6f, ui::DIM, NVG_ALIGN_RIGHT, 1.1f);
		char nn[8] = "E1";
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

struct GrundWidget : ModuleWidget {
	GrundWidget(Grund* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Grund.svg")));
		follow::addStrip(this, module,
		                 {mm2px(Vec(L::VOCT[0], L::VOCT[1])), mm2px(Vec(L::GATE[0], L::GATE[1])), mm2px(Vec(L::VEL[0], L::VEL[1])),
		                  mm2px(Vec(L::ACC[0], L::ACC[1])), mm2px(Vec(L::SLIDE[0], L::SLIDE[1])),
		                  mm2px(Vec(L::IN_MOD[0], L::IN_MOD[1]))});  // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);

		using M = Grund;
		auto field = [&](const float* r, int id, const char* const* names, const char* const* info, int count,
		                 const char* title, const char* undo) {
			NameField* f = createParam<NameField>(mm2px(Vec(r[0], r[1])), module, id);
			f->box.size = mm2px(Vec(r[2], r[3]));
			f->names = names, f->info = info, f->count = count, f->title = title, f->undo = undo;
			addParam(f);
		};
		field(L::MODELF, M::MODEL_PARAM, MODEL_NAMES, MODEL_INFO, NUM_MODELS, "Model", "change model");
		field(L::AMPF, M::AMP_PARAM, AMP_NAMES, AMP_INFO, NUM_AMPS, "Amp", "change amp");
		PresetField* pf = createWidget<PresetField>(mm2px(Vec(L::PRESETF[0], L::PRESETF[1])));
		pf->box.size = mm2px(Vec(L::PRESETF[2], L::PRESETF[3]));
		pf->module = module;
		addChild(pf);

		auto large = [&](const float* p, int id) { addParam(createParamCentered<GLargeKnob>(ui::mm(p), module, id)); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		large(L::TIMBRE, M::TIMBRE_PARAM);
		knob(L::BITE, M::BITE_PARAM);
		knob(L::BODY, M::BODY_PARAM);
		knob(L::SUB, M::SUB_PARAM);
		ui::LampRow* lr = createParam<ui::LampRow>(Vec(0, 0), module, M::VMODE_PARAM);
		lr->place(L::VMODE, NUM_MODES);
		lr->accent = ACCENT;
		addParam(lr);
		small(L::MUTE, M::MUTE_PARAM);
		small(L::GLIDE, M::GLIDE_PARAM);
		small(L::ACCENT, M::ACCENT_PARAM);
		large(L::CUTOFF, M::CUTOFF_PARAM);
		knob(L::RES, M::RES_PARAM);
		knob(L::FENV, M::FENV_PARAM);
		knob(L::FDEC, M::FDEC_PARAM);
		small(L::DRIVE, M::DRIVE_PARAM);
		small(L::COMP, M::COMP_PARAM);
		small(L::KEY, M::KEY_PARAM);
		knob(L::ATK, M::ATK_PARAM);
		knob(L::DEC, M::DEC_PARAM);
		knob(L::SUS, M::SUS_PARAM);
		knob(L::REL, M::REL_PARAM);
		knob(L::CHORUS, M::CHORUS_PARAM);
		knob(L::OCTAVE, M::OCTAVE_PARAM);
		knob(L::WIDTH, M::WIDTH_PARAM);
		knob(L::VERB, M::VERB_PARAM);

		const float* ins[M::NUM_INPUTS] = {L::VOCT,     L::GATE,      L::VEL,     L::ACC,    L::SLIDE,
		                                   L::IN_MOD,   L::IN_MODEL,  L::IN_TIMBRE, L::IN_BITE, L::IN_CUT,
		                                   L::IN_MUTE,  L::IN_DRIVE,  L::IN_SUB,  L::IN_ATK, L::IN_REL};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_ENV, L::OUT_L, L::OUT_R};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Grund* m = dynamic_cast<Grund*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		m->scanUser();
		menu->addChild(createSubmenuItem("Preset", m->nameOf(m->preset), [=](Menu* sub) { appendPresets(sub, m); }));
		menu->addChild(createSubmenuItem("Save preset as …", "", [=](Menu* sub) {
			SaveField* tf = new SaveField;
			tf->module = m;
			tf->box.size.x = 160.f;
			tf->placeholder = "Name, Enter saves";
			tf->text = m->preset >= NUM_PRESETS ? m->presetName : "";
			sub->addChild(tf);
			APP->event->setSelectedWidget(tf);
		}));
		menu->addChild(createMenuItem("Open user preset folder", "", [=]() {
			system::createDirectories(Grund::userDir());
			system::openDirectory(Grund::userDir());
		}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Patch GENRA's bass outputs, or right-click → Follow GENRA → Bass"));
		menu->addChild(createMenuLabel("Dock GRUND FX on the right for the effects in detail"));
		menu->addChild(createMenuLabel(MODEL_HELP[m->modelNow]));
	}
};

}  // namespace genra_grund

Model* modelGrund = createModel<genra_grund::Grund, genra_grund::GrundWidget>("Grund");
