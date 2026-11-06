// SPDX-License-Identifier: GPL-3.0-only
// GENRA MONO – monophonic bass synth after the Moog Minitaur, in the look of MONK and the JIANT voices (accent: turquoise).
// Sound engine: MonoDsp.hpp. Panel: tools/mono_panel.py (res/GenraMono.svg + MonoLayout.hpp).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include <cctype>
#include "MonoDsp.hpp"
#include "MonoPresets.hpp"
#include "MonoLayout.hpp"

namespace genra_mono {

namespace L = mlayout;

// keep in step with tools/mono_panel.py (ACC)
static const NVGcolor ACCENT = nvgRGB(0xb8, 0xf2, 0x3a);
static const NVGcolor TXT = nvgRGB(0xd9, 0xdb, 0xe2), DIM = nvgRGB(0x8a, 0x8d, 0x99);

inline Vec P(const float* p) { return mm2px(Vec(p[0], p[1])); }

struct GenraMono : Module, follow::Host {
	enum ParamId {
		WAVE1_PARAM, WAVE2_PARAM, TUNE1_PARAM, TUNE2_PARAM, LEVEL1_PARAM, LEVEL2_PARAM, GLIDE_PARAM,
		FTYPE_PARAM, CUTOFF_PARAM, RES_PARAM, EGAMT_PARAM, DRIVE_PARAM, KEYTRK_PARAM,
		FA_PARAM, FD_PARAM, FS_PARAM, FR_PARAM, AA_PARAM, AD_PARAM, AS_PARAM, AR_PARAM,
		RATE_PARAM, LFOWAVE_PARAM, LFOPITCH_PARAM, LFOFILT_PARAM,
		ACCENT_PARAM, WIDTH_PARAM,  // 2.10.0
		NUM_PARAMS
	};
	enum InputId { VOCT_INPUT, GATE_INPUT, VEL_INPUT, CUTOFF_INPUT, RES_INPUT, DRIVE_INPUT, RATE_INPUT, VCA_INPUT,
		           EXT_INPUT, ACC_INPUT, SLIDE_INPUT, RETRIG_INPUT, SYNC_INPUT, NUM_INPUTS };
	// the LFO output is gone (2.10.0); its id stays so cables on OUT land on L
	enum OutputId { OLD_LFO_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	static const int OLD_PARAMS = 25;  // presets and patches before 2.10.0
	static_assert(gmono::NUM_PRESET_PARAMS == NUM_PARAMS, "presets must cover every knob");

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

	/** LFO rate: Hz when free, a note value when SYNC has a clock. */
	struct RateQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			GenraMono* m = dynamic_cast<GenraMono*>(module);
			if (m && m->inputs[SYNC_INPUT].isConnected())
				return gmono::DIV_NAMES[gmono::divIndex(getValue(), 0.f)];
			return string::f("%.3g", 0.05f * std::pow(1000.f, getValue()));
		}
		std::string getUnit() override {
			GenraMono* m = dynamic_cast<GenraMono*>(module);
			return m && m->inputs[SYNC_INPUT].isConnected() ? "" : " Hz";
		}
	};

	gmono::Voice voice;
	dsp::SchmittTrigger retrigT, syncT;
	int clockRes = 0;           // SYNC clock: 0 = 16ths, 1 = 8ths, 2 = quarters, 3 = 24 PPQN
	int64_t sinceClock = -1;    // samples since the last clock edge, -1 = none yet
	float beatSec = 0.f;
	gmono::Params p;
	bool gate = false;

	// ---- presets: the factory list (gmono::PRESETS), then the user presets from userDir()
	struct UserPreset {
		std::string name;
		float v[NUM_PARAMS];
	};
	std::vector<UserPreset> user;
	int preset = -1;            // index into factory + user list, -1 = none (INIT)
	std::string presetName;     // a user preset is remembered by name, the list may change
	float loaded[NUM_PARAMS];   // the values as loaded, to show when a knob has moved since

	static std::string userDir() { return asset::user("Genra/MonoPresets"); }
	int count() { return gmono::NUM_PRESETS + (int) user.size(); }
	std::string nameOf(int i) {
		if (i < 0 || i >= count())
			return "INIT";
		return i < gmono::NUM_PRESETS ? gmono::PRESETS[i].name : user[i - gmono::NUM_PRESETS].name;
	}
	void valuesOf(int i, float* v) {
		if (i < gmono::NUM_PRESETS)
			gmono::PRESETS[i].values(v);
		else
			std::copy(user[i - gmono::NUM_PRESETS].v, user[i - gmono::NUM_PRESETS].v + NUM_PARAMS, v);
	}
	void rememberLoaded() {
		for (int i = 0; i < NUM_PARAMS; i++)
			loaded[i] = params[i].getValue();
	}
	/** True when a knob was moved since the preset was loaded. */
	bool edited() {
		if (preset < 0)
			return false;
		for (int i = 0; i < NUM_PARAMS; i++)
			if (std::fabs(params[i].getValue() - loaded[i]) > 1e-4f)
				return true;
		return false;
	}
	/** Loads preset i (undoable). */
	void load(int i) {
		if (i < 0 || i >= count())
			return;
		json_t* oldJ = toJson();
		float v[NUM_PARAMS];
		valuesOf(i, v);
		for (int k = 0; k < NUM_PARAMS; k++)
			params[k].setValue(v[k]);
		preset = i;
		presetName = nameOf(i);
		rememberLoaded();
		history::ModuleChange* h = new history::ModuleChange;
		h->name = "load preset " + presetName;
		h->moduleId = id;
		h->oldModuleJ = oldJ;
		h->newModuleJ = toJson();
		APP->history->push(h);
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
		for (const std::string& f : files) {
			if (system::getExtension(f) != ".json")
				continue;
			json_t* j = json_load_file(f.c_str(), 0, nullptr);
			if (!j)
				continue;
			json_t* nameJ = json_object_get(j, "name");
			json_t* valsJ = json_object_get(j, "params");
			int n = json_is_array(valsJ) ? (int) json_array_size(valsJ) : 0;
			if (json_is_string(nameJ) && n >= OLD_PARAMS) {
				UserPreset u;
				u.name = json_string_value(nameJ);
				for (int i = 0; i < NUM_PARAMS; i++)
					u.v[i] = i < n ? (float) json_number_value(json_array_get(valsJ, i))
					               : paramQuantities[i]->getDefaultValue();
				user.push_back(u);
			}
			json_decref(j);
		}
		// keep pointing at the same user preset
		if (preset >= gmono::NUM_PRESETS) {
			preset = -1;
			for (size_t i = 0; i < user.size(); i++)
				if (user[i].name == presetName)
					preset = gmono::NUM_PRESETS + i;
		}
	}
	/** Saves the knobs as a user preset (an existing one with the same name is replaced) and selects it. */
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
		json_dump_file(j, system::join(userDir(), file + ".json").c_str(), JSON_INDENT(2));
		json_decref(j);
		presetName = name;
		preset = gmono::NUM_PRESETS;  // so scanUser looks the name up
		scanUser();
		rememberLoaded();
	}

	GenraMono() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		srcName = "MONO";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		fol.bind(ACC_INPUT, follow::ACC);
		fol.bind(SLIDE_INPUT, follow::SLIDE);
		fol.bind(SYNC_INPUT, follow::CLOCK);
		fol.bind(RETRIG_INPUT, follow::RESET);
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
		configParam<RateQuantity>(RATE_PARAM, 0.f, 1.f, 0.45f, "LFO rate");
		paramQuantities[RATE_PARAM]->description = "With a clock at SYNC: the note value";
		configSwitch(LFOWAVE_PARAM, 0.f, 1.f, 1.f, "LFO wave", {"Square", "Triangle"});
		configParam(LFOPITCH_PARAM, 0.f, 1.f, 0.f, "LFO → pitch", "%", 0.f, 100.f);
		configParam(LFOFILT_PARAM, 0.f, 1.f, 0.f, "LFO → filter", "%", 0.f, 100.f);
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accent", "%", 0.f, 100.f);
		paramQuantities[ACCENT_PARAM]->description = "How strongly ACC makes a note louder and brighter";
		configParam(WIDTH_PARAM, 0.f, 1.f, 0.f, "Stereo width", "%", 0.f, 100.f);
		paramQuantities[WIDTH_PARAM]->description = "0 = mono; the lows below 150 Hz always stay mono";

		configInput(VOCT_INPUT, "V/Oct");
		configInput(GATE_INPUT, "Gate");
		configInput(VEL_INPUT, "Velocity (0–10 V)");
		configInput(CUTOFF_INPUT, "Cutoff CV (1 V/oct)");
		configInput(RES_INPUT, "Resonance CV");
		configInput(DRIVE_INPUT, "Drive CV");
		configInput(RATE_INPUT, "LFO rate CV (1 V/oct)");
		configInput(VCA_INPUT, "VCA CV (0–10 V)");
		configInput(EXT_INPUT, "External audio into the filter");
		configInput(ACC_INPUT, "Accent (gate, taken at the start of a note)");
		configInput(SLIDE_INPUT, "Slide (glides while high; patched: glide only on slides)");
		configInput(RETRIG_INPUT, "Reset (restarts the LFO)");
		configInput(SYNC_INPUT, "Clock (the LFO follows the tempo; RATE then picks the note value)");
		configOutput(OLD_LFO_OUTPUT, "(unused)");
		configOutput(L_OUTPUT, "Left (mono when R is not patched)");
		configOutput(R_OUTPUT, "Right");
		rememberLoaded();
		scanUser();
	}

	json_t* dataToJson() override {
		json_t* j = json_object();
		json_object_set_new(j, "preset", json_string(preset < 0 ? "" : presetName.c_str()));
		json_t* a = json_array();
		for (int i = 0; i < NUM_PARAMS; i++)
			json_array_append_new(a, json_real(loaded[i]));
		json_object_set_new(j, "loaded", a);
		json_object_set_new(j, "clockRes", json_integer(clockRes));
		fol.toJson(j);
		return j;
	}
	void dataFromJson(json_t* j) override {
		fol.fromJson(j);
		json_t* pJ = json_object_get(j, "preset");
		preset = -1;
		presetName = json_is_string(pJ) ? json_string_value(pJ) : "";
		for (int i = 0; i < count() && !presetName.empty(); i++)
			if (nameOf(i) == presetName) {
				preset = i;
				break;
			}
		json_t* a = json_object_get(j, "loaded");
		int n = json_is_array(a) ? (int) json_array_size(a) : 0;
		rememberLoaded();
		for (int i = 0; i < n && i < NUM_PARAMS; i++)
			loaded[i] = (float) json_number_value(json_array_get(a, i));
		json_t* cJ = json_object_get(j, "clockRes");
		if (json_is_integer(cJ))
			clockRes = clamp((int) json_integer_value(cJ), 0, 3);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override { voice.setSampleRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		voice = gmono::Voice();
		voice.setSampleRate(APP->engine->getSampleRate());
		preset = -1;
		presetName.clear();
		rememberLoaded();
	}

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
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
		p.accent = v(ACCENT_PARAM);
		p.width = outputs[R_OUTPUT].isConnected() || pulled() ? v(WIDTH_PARAM) : 0.f;

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
		in.acc = inputs[ACC_INPUT].getVoltage() >= 1.f;
		in.slideConnected = inputs[SLIDE_INPUT].isConnected();
		in.slide = inputs[SLIDE_INPUT].getVoltage() >= 1.f;
		in.retrig = retrigT.process(inputs[RETRIG_INPUT].getVoltage(), 0.1f, 1.f);
		if (in.retrig && sinceClock > 0)
			sinceClock = -1; // RESET: the gap before it is not a clock period

		// SYNC: the clock period → one beat; no clock for 2 s → free again
		if (inputs[SYNC_INPUT].isConnected()) {
			if (sinceClock >= 0)
				sinceClock++;
			if (syncT.process(inputs[SYNC_INPUT].getVoltage(), 0.1f, 1.f)) {
				static const float PER_BEAT[4] = {4.f, 2.f, 1.f, 24.f};
				if (sinceClock > 0)
					beatSec = sinceClock * args.sampleTime * PER_BEAT[clamp(clockRes, 0, 3)];
				sinceClock = 0;
			}
			if (sinceClock > (int64_t) (2.f * args.sampleRate))
				beatSec = 0.f, sinceClock = -1;
		} else {
			beatSec = 0.f;
			sinceClock = -1;
		}
		in.beatSec = beatSec;

		float l, r;
		voice.process(p, in, l, r);
		if (outputs[R_OUTPUT].isConnected() || pulled()) {
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		} else {
			outputs[L_OUTPUT].setVoltage(l);
		}
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

/** The preset list: one submenu per style, the user presets at the end; the one playing ticked. */
static void appendPresets(Menu* menu, GenraMono* m) {
	for (int i = 0; i < gmono::NUM_PRESETS;) {
		std::string st = gmono::PRESETS[i].style;
		int j = i;
		while (j < gmono::NUM_PRESETS && st == gmono::PRESETS[j].style)
			j++;
		bool here = m->preset >= i && m->preset < j;
		menu->addChild(createSubmenuItem(st, here ? "✔" : "", [=](Menu* sub) {
			for (int k = i; k < j; k++)
				sub->addChild(createCheckMenuItem(string::f("%02d  %s", k + 1, gmono::PRESETS[k].name), "",
				                                  [=]() { return m->preset == k; }, [=]() { m->load(k); }));
		}));
		i = j;
	}
	bool here = m->preset >= gmono::NUM_PRESETS;
	menu->addChild(createSubmenuItem("USER", here ? "✔" : string::f("%d", (int) m->user.size()), [=](Menu* sub) {
		if (m->user.empty())
			sub->addChild(createMenuLabel("Save one with right-click → Save preset as …"));
		for (int k = gmono::NUM_PRESETS; k < m->count(); k++)
			sub->addChild(createCheckMenuItem(m->nameOf(k), "", [=]() { return m->preset == k; }, [=]() { m->load(k); }));
	}));
}

/** The preset field in the header: number and name (a dot when a knob has moved), click = list, wheel = step. */
struct PresetField : OpaqueWidget {
	GenraMono* module = nullptr;
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
			std::shared_ptr<window::Font> f = boldFont();
			if (f && f->handle >= 0) {
				NVGcontext* vg = args.vg;
				int idx = module ? module->preset : 0;
				std::string name = module ? module->nameOf(idx) : gmono::PRESETS[0].name;
				std::string num = idx < 0 ? "--" : idx >= gmono::NUM_PRESETS ? "U" : string::f("%02d", idx + 1);
				float y = box.size.y / 2 + 0.5f;
				nvgFontFaceId(vg, f->handle);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFontSize(vg, 6.4f);
				nvgTextLetterSpacing(vg, 0.8f);
				nvgFillColor(vg, ACCENT);
				nvgText(vg, mm2px(2.f), y, num.c_str(), nullptr);
				nvgFontSize(vg, 7.6f);
				nvgTextLetterSpacing(vg, 0.9f);
				nvgFillColor(vg, TXT);
				float end = nvgText(vg, mm2px(6.2f), y, name.c_str(), nullptr);
				if (module && module->edited()) {
					nvgBeginPath(vg);
					nvgCircle(vg, end + mm2px(1.4f), y, mm2px(0.55f));
					nvgFillColor(vg, ACCENT);
					nvgFill(vg);
				}
			}
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** The name field of "Save preset as …": Enter saves. */
struct SaveField : ui::TextField {
	GenraMono* module = nullptr;
	void onAction(const ActionEvent& e) override {
		if (module)
			module->saveUser(text);
		if (ui::MenuOverlay* o = getAncestorOfType<ui::MenuOverlay>())
			o->requestDelete();
		e.consume(this);
	}
};

struct GenraMonoWidget : ModuleWidget {
	GenraMonoWidget(GenraMono* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GenraMono.svg")));
		follow::addStrip(this, module, {mm2px(Vec(L::IN_VOCT[0], L::IN_VOCT[1])), mm2px(Vec(L::IN_GATE[0], L::IN_GATE[1])), mm2px(Vec(L::IN_VEL[0], L::IN_VEL[1])), mm2px(Vec(L::IN_ACC[0], L::IN_ACC[1])), mm2px(Vec(L::IN_SLIDE[0], L::IN_SLIDE[1])), mm2px(Vec(L::IN_SYNC[0], L::IN_SYNC[1])), mm2px(Vec(L::IN_RETRIG[0], L::IN_RETRIG[1]))}); // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using M = GenraMono;
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(P(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(P(p), module, id)); };
		knob(L::WAVE1, M::WAVE1_PARAM);
		knob(L::TUNE1, M::TUNE1_PARAM);
		knob(L::LEVEL1, M::LEVEL1_PARAM);
		knob(L::WAVE2, M::WAVE2_PARAM);
		knob(L::TUNE2, M::TUNE2_PARAM);
		knob(L::LEVEL2, M::LEVEL2_PARAM);
		small(L::GLIDE, M::GLIDE_PARAM);
		small(L::ACCENT, M::ACCENT_PARAM);
		small(L::WIDTH, M::WIDTH_PARAM);

		FilterField* field = createParam<FilterField>(mm2px(Vec(L::FIELD[0], L::FIELD[1])), module, M::FTYPE_PARAM);
		field->box.size = mm2px(Vec(L::FIELD[2], L::FIELD[3]));
		addParam(field);

		PresetField* pf = createWidget<PresetField>(mm2px(Vec(L::PFIELD[0], L::PFIELD[1])));
		pf->box.size = mm2px(Vec(L::PFIELD[2], L::PFIELD[3]));
		pf->module = module;
		addChild(pf);
		addParam(createParamCentered<GLargeKnob>(P(L::CUTOFF), module, M::CUTOFF_PARAM));
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

		const float* ins[M::NUM_INPUTS] = {L::IN_VOCT, L::IN_GATE,  L::IN_VEL,   L::IN_CUTOFF, L::IN_RES,
		                                   L::IN_DRIVE, L::IN_RATE, L::IN_VCA,   L::IN_EXT,    L::IN_ACC,
		                                   L::IN_SLIDE, L::IN_RETRIG, L::IN_SYNC};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(P(ins[i]), module, i));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_L), module, M::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_R), module, M::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		GenraMono* m = getModule<GenraMono>();
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
			tf->text = m->preset >= gmono::NUM_PRESETS ? m->presetName : "";
			sub->addChild(tf);
			APP->event->setSelectedWidget(tf);
		}));
		menu->addChild(createMenuItem("Open user preset folder", "", [=]() {
			system::createDirectories(GenraMono::userDir());
			system::openDirectory(GenraMono::userDir());
		}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
		    "Filter type",
		    std::vector<std::string>(gmono::FILTER_NAMES, gmono::FILTER_NAMES + gmono::NUM_FILTERS),
		    [=]() { return (size_t) clamp((int) std::round(m->params[GenraMono::FTYPE_PARAM].getValue()), 0, gmono::NUM_FILTERS - 1); },
		    [=](size_t i) { m->params[GenraMono::FTYPE_PARAM].setValue((float) i); }));
		menu->addChild(createIndexPtrSubmenuItem("LFO sync clock", {"16ths (as GENRA)", "8ths", "Quarters", "24 PPQN"},
		                                         &m->clockRes));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Monophonic bass synth after the Moog Minitaur"));
		menu->addChild(createMenuLabel("Click or scroll the filter field to change the filter type"));
		menu->addChild(createMenuLabel("Click or scroll the preset field to change the preset"));
	}
};

}  // namespace genra_mono

Model* modelGenraMono = createModel<genra_mono::GenraMono, genra_mono::GenraMonoWidget>("GenraMono");
