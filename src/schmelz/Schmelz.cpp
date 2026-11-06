// SPDX-License-Identifier: GPL-3.0-only
// SCHMELZ – string machine with a strings and a solo section, in the GENRA look (accent: rose).
// Sound engine: SchmelzDsp.hpp. Panel: tools/schmelz_panel.py (res/Schmelz.svg + SchmelzLayout.hpp).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "SchmelzDsp.hpp"
#include "SchmelzLayout.hpp"
#include <memory>

namespace genra_schmelz {

namespace L = zlayout;
namespace ui = genra_ui;
using namespace gschmelz;

// keep in step with tools/schmelz_panel.py (ACC)
#define SCHMELZ_RGB 0xe0, 0x77, 0x9e
static const NVGcolor ACCENT = nvgRGB(SCHMELZ_RGB);
static const int MAXV = 16;

struct Schmelz : Module, follow::Host {
	enum ParamId {
		REG_PARAM, CRESC_PARAM, REL_PARAM, TONE_PARAM, ENSDEPTH_PARAM, OCT_PARAM, ENS_PARAM,
		SOUND_PARAM, DECAY_PARAM, SUSTAIN_PARAM, TREM_PARAM, SPLIT_PARAM, ATK_PARAM, TRATE_PARAM, VEL_PARAM, HOLD_PARAM,
		BAL_PARAM, ANIM_PARAM, ARATE_PARAM, PHASER_PARAM, PRATE_PARAM, VERB_PARAM, SIZE_PARAM, VOLUME_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		STR_VOCT_INPUT, STR_GATE_INPUT, STR_VEL_INPUT, SOLO_VOCT_INPUT, SOLO_GATE_INPUT, SOLO_VEL_INPUT,
		REG_INPUT, CRESC_INPUT, REL_INPUT, TONE_INPUT, ENS_INPUT, OCT_INPUT,
		SOUND_INPUT, DECAY_INPUT, TREM_INPUT, BAL_INPUT, ANIM_INPUT, VERB_INPUT,
		MOD_INPUT, // 2.13: GENRA's pad MOD (or any 0–10 V), onto crescendo / animate / tone
		NUM_INPUTS
	};
	enum OutputId { ENV_OUTPUT, STR_OUTPUT, SOLO_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightId { HOLD_LIGHT, NUM_LIGHTS };

	template <float (*F)(float)>
	struct TimeQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float t = F(getValue());
			return t < 1.f ? string::f("%.0f", t * 1000.f) : string::f("%.2f", t);
		}
		std::string getUnit() override { return F(getValue()) < 1.f ? " ms" : " s"; }
	};
	template <float (*F)(float)>
	struct RateQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.2f", F(getValue())); }
		std::string getUnit() override { return " Hz"; }
	};
	struct RegQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float x = getValue() * (NUM_REGS - 1);
			int i = std::min((int) x, NUM_REGS - 2);
			int pct = (int) std::lround((x - i) * 100.f);
			if (pct == 0)
				return title(REG_NAMES[i]);
			if (pct == 100)
				return title(REG_NAMES[i + 1]);
			return title(REG_NAMES[i]) + " → " + title(REG_NAMES[i + 1]) + string::f(" %d %%", pct);
		}
		std::string getUnit() override { return ""; }
	};
	struct SoundQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float x = getValue() * (NUM_SOUNDS - 1);
			int i = std::min((int) x, NUM_SOUNDS - 2);
			int pct = (int) std::lround((x - i) * 100.f);
			if (pct == 0)
				return title(SOUND_NAMES[i]);
			if (pct == 100)
				return title(SOUND_NAMES[i + 1]);
			return title(SOUND_NAMES[i]) + " → " + title(SOUND_NAMES[i + 1]) + string::f(" %d %%", pct);
		}
		std::string getUnit() override { return ""; }
	};
	static std::string title(std::string s) {
		for (size_t i = 1; i < s.size(); i++)
			s[i] = std::tolower(s[i]);
		return s;
	}

	std::unique_ptr<StringsVoice[]> sv{new StringsVoice[MAXV]};
	std::unique_ptr<SoloVoice[]> so{new SoloVoice[MAXV]};
	std::unique_ptr<Bus> bus{new Bus};
	bool gate[MAXV] = {}, soloGate[MAXV] = {}, strSide[MAXV] = {}, soloSide[MAXV] = {}, held[MAXV] = {};
	bool soloOwnGate[MAXV] = {};
	int ctl = 0;
	BusParams bp;
	StringsParams sp;
	SoloParams op;

	// for the display
	float dispReg = 0.15f, dispSound = 0.25f;
	int voicesOn = 0;
	bool voiceOn[MAXV] = {};

	Schmelz() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		srcName = "SCHMELZ";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(STR_VOCT_INPUT, follow::VOCT);
		fol.bind(STR_GATE_INPUT, follow::GATE);
		fol.bind(STR_VEL_INPUT, follow::VEL);
		fol.bind(MOD_INPUT, follow::MOD);
		configParam<RegQuantity>(REG_PARAM, 0.f, 1.f, 0.15f, "Registration");
		paramQuantities[REG_PARAM]->description = "Violin, viola, cello, bass, brass, organ, choir, synth – morphing";
		configParam<TimeQuantity<crescSeconds>>(CRESC_PARAM, 0.f, 1.f, 0.35f, "Crescendo");
		configParam<TimeQuantity<releaseSeconds>>(REL_PARAM, 0.f, 1.f, 0.5f, "Release");
		configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "%", 0.f, 200.f, -100.f);
		configParam(ENSDEPTH_PARAM, 0.f, 1.f, 0.7f, "Ensemble depth", "%", 0.f, 100.f);
		configSwitch(OCT_PARAM, 0.f, NUM_OCT - 1, 0.f, "Octave", {"Base", "Both", "8va"});
		configSwitch(ENS_PARAM, 0.f, NUM_ENS - 1, 2.f, "Ensemble", {"Off", "Ensemble", "Ensemble + chorus", "Chorus"});

		configParam<SoundQuantity>(SOUND_PARAM, 0.f, 1.f, 0.25f, "Solo sound");
		paramQuantities[SOUND_PARAM]->description = "Bass, e-piano, clavi, synth, bell – morphing";
		configParam<TimeQuantity<decaySeconds>>(DECAY_PARAM, 0.f, 1.f, 0.55f, "Decay / release");
		configParam(SUSTAIN_PARAM, 0.f, 1.f, 0.4f, "Sustain", "%", 0.f, 100.f);
		configParam(TREM_PARAM, 0.f, 1.f, 0.f, "Tremolo", "%", 0.f, 100.f);
		configSwitch(SPLIT_PARAM, 0.f, NUM_SPLIT - 1, 0.f, "Split",
		             {"Layer", "Low (solo below C4)", "High (solo from C4)"});
		paramQuantities[SPLIT_PARAM]->description = "When the solo plays from the strings inputs (SOLO GATE unplugged)";
		configParam<TimeQuantity<attackSeconds>>(ATK_PARAM, 0.f, 1.f, 0.f, "Solo attack");
		configParam<RateQuantity<tremRate>>(TRATE_PARAM, 0.f, 1.f, 0.4f, "Tremolo rate");
		configParam(VEL_PARAM, 0.f, 1.f, 0.5f, "Velocity sensitivity", "%", 0.f, 100.f);
		configSwitch(HOLD_PARAM, 0.f, 1.f, 0.f, "Sustain (hold the solo notes)", {"Off", "On"});

		configParam(BAL_PARAM, 0.f, 1.f, 0.5f, "Balance strings / solo", "%", 0.f, 200.f, -100.f);
		configParam(ANIM_PARAM, 0.f, 1.f, 0.f, "Animate", "%", 0.f, 100.f);
		paramQuantities[ANIM_PARAM]->description = "An LFO moves the registration";
		configParam<RateQuantity<animRate>>(ARATE_PARAM, 0.f, 1.f, 0.35f, "Animate rate");
		configParam(PHASER_PARAM, 0.f, 1.f, 0.f, "Phaser (strings)", "%", 0.f, 100.f);
		configParam<RateQuantity<phaserRate>>(PRATE_PARAM, 0.f, 1.f, 0.3f, "Phaser rate");
		configParam(VERB_PARAM, 0.f, 1.f, 0.25f, "Reverb", "%", 0.f, 100.f);
		configParam(SIZE_PARAM, 0.f, 1.f, 0.6f, "Reverb size", "%", 0.f, 100.f);
		configParam(VOLUME_PARAM, 0.f, 1.f, 0.7f, "Volume", "%", 0.f, 100.f);

		configInput(STR_VOCT_INPUT, "Strings V/Oct");
		configInput(STR_GATE_INPUT, "Strings gate");
		configInput(STR_VEL_INPUT, "Strings velocity (0–10 V)");
		configInput(SOLO_VOCT_INPUT, "Solo V/Oct (unplugged: the strings notes)");
		configInput(SOLO_GATE_INPUT, "Solo gate (unplugged: the solo plays the strings notes, see SPLIT)");
		configInput(SOLO_VEL_INPUT, "Solo velocity (0–10 V)");
		configInput(REG_INPUT, "Registration CV (0–10 V, polyphonic: per voice)");
		configInput(CRESC_INPUT, "Crescendo CV");
		configInput(REL_INPUT, "Release CV");
		configInput(TONE_INPUT, "Tone CV");
		configInput(ENS_INPUT, "Ensemble CV (0–10 V steps through the modes)");
		configInput(OCT_INPUT, "Octave CV (0–10 V steps through base / both / 8va)");
		configInput(SOUND_INPUT, "Solo sound CV");
		configInput(DECAY_INPUT, "Solo decay CV");
		configInput(TREM_INPUT, "Tremolo CV");
		configInput(BAL_INPUT, "Balance CV");
		configInput(ANIM_INPUT, "Animate CV");
		configInput(MOD_INPUT, "Mod (0–10 V, e.g. GENRA's pad MOD; target in the context menu)");
		configInput(VERB_INPUT, "Reverb CV");
		configOutput(ENV_OUTPUT, "Strings envelope (0–10 V, polyphonic)");
		configOutput(STR_OUTPUT, "Strings (with ensemble, no phaser or reverb)");
		configOutput(SOLO_OUTPUT, "Solo (dry)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		for (int i = 0; i < MAXV; i++) {
			sv[i].seed(0x9e3779b9u * (i + 1));
			so[i].seed(0x85ebca6bu * (i + 7));
		}
	}

	void setRate(float sr) {
		for (int i = 0; i < MAXV; i++) {
			sv[i].setSampleRate(sr);
			so[i].setSampleRate(sr);
		}
		bus->setSampleRate(sr);
	}
	void onAdd(const AddEvent& e) override { setRate(APP->engine->getSampleRate()); }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { setRate(e.sampleRate); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int i = 0; i < MAXV; i++) {
			sv[i].env = so[i].env = 0.f;
			so[i].stage = 0;
			gate[i] = soloGate[i] = held[i] = soloOwnGate[i] = false;
		}
	}

	int modTarget = 0; // MOD onto: 0 crescendo, 1 animate, 2 tone

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "modTarget", json_integer(modTarget));
		fol.toJson(root);
		return root;
	}
	void dataFromJson(json_t* root) override {
		fol.fromJson(root);
		if (json_t* j = json_object_get(root, "modTarget"))
			modTarget = clamp((int) json_integer_value(j), 0, 2);
	}

	float kv(int param, int input, int c = 0) {
		return clamp(params[param].getValue() + inputs[input].getPolyVoltage(c) * 0.1f, 0.f, 1.f);
	}
	int step(int param, int input, int n) {
		int add = clamp((int) (clamp(inputs[input].getVoltage(), 0.f, 10.f) / 10.f * n), 0, n - 1);
		return ((int) std::round(params[param].getValue()) + add) % n;
	}
	static bool schmitt(bool was, float v) { return v >= 1.f ? true : (v <= 0.1f ? false : was); }

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		// controls at a lower rate
		if (--ctl <= 0) {
			ctl = 8;
			sp.oct = step(OCT_PARAM, OCT_INPUT, NUM_OCT);
			sp.cresc = kv(CRESC_PARAM, CRESC_INPUT);
			sp.rel = kv(REL_PARAM, REL_INPUT);
			sp.tone = kv(TONE_PARAM, TONE_INPUT);
			op.sound = kv(SOUND_PARAM, SOUND_INPUT);
			op.atk = params[ATK_PARAM].getValue();
			op.dec = kv(DECAY_PARAM, DECAY_INPUT);
			op.sus = params[SUSTAIN_PARAM].getValue();
			op.velSens = params[VEL_PARAM].getValue();
			bp.ens = step(ENS_PARAM, ENS_INPUT, NUM_ENS);
			bp.ensDepth = params[ENSDEPTH_PARAM].getValue();
			bp.anim = kv(ANIM_PARAM, ANIM_INPUT);
			bp.animRate = params[ARATE_PARAM].getValue();
			bp.phaser = params[PHASER_PARAM].getValue();
			bp.phaserRate = params[PRATE_PARAM].getValue();
			bp.verb = kv(VERB_PARAM, VERB_INPUT);
			bp.size = params[SIZE_PARAM].getValue();
			bp.trem = kv(TREM_PARAM, TREM_INPUT);
			if (inputs[MOD_INPUT].isConnected()) {
				float m = clamp(inputs[MOD_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
				if (modTarget == 0)
					sp.cresc = clamp(sp.cresc + m, 0.f, 1.f);
				else if (modTarget == 1)
					bp.anim = clamp(bp.anim + m, 0.f, 1.f);
				else
					sp.tone = clamp(sp.tone + m, 0.f, 1.f);
			}
			bp.tremRate = params[TRATE_PARAM].getValue();
			bp.bal = kv(BAL_PARAM, BAL_INPUT);
			bp.vol = params[VOLUME_PARAM].getValue();
			dispSound = op.sound;
		}
		bool hold = params[HOLD_PARAM].getValue() > 0.5f;
		lights[HOLD_LIGHT].setBrightness(hold ? 1.f : 0.f);
		int split = (int) std::round(params[SPLIT_PARAM].getValue());
		float anim = bus->animate(bp);

		int sch = std::max(inputs[STR_VOCT_INPUT].getChannels(), inputs[STR_GATE_INPUT].getChannels());
		bool soloOwn = inputs[SOLO_GATE_INPUT].isConnected();
		int och = soloOwn ? std::max(inputs[SOLO_VOCT_INPUT].getChannels(), inputs[SOLO_GATE_INPUT].getChannels()) : sch;
		sch = std::min(sch, MAXV);
		och = std::min(och, MAXV);
		bool sVel = inputs[STR_VEL_INPUT].isConnected(), oVel = inputs[SOLO_VEL_INPUT].isConnected();

		float str = 0.f, solo = 0.f;
		int on = 0;
		// strings (and the solo, when it plays from the strings inputs)
		for (int c = 0; c < sch; c++) {
			float voct = inputs[STR_VOCT_INPUT].getPolyVoltage(c);
			bool was = gate[c];
			gate[c] = schmitt(was, inputs[STR_GATE_INPUT].getPolyVoltage(c));
			bool rise = gate[c] && !was;
			if (rise) {
				strSide[c] = soloOwn || stringsTakes(split, voct);
				soloSide[c] = !soloOwn && soloTakes(split, voct);
			}
			float f = C4 * std::pow(2.f, voct);
			float vel = sVel ? clamp(inputs[STR_VEL_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f) : 1.f;
			StringsParams p = sp;
			p.reg = clamp(params[REG_PARAM].getValue() + inputs[REG_INPUT].getPolyVoltage(c) * 0.1f + anim, 0.f, 1.f);
			if (c == 0)
				dispReg = p.reg;
			bool sg = gate[c] && strSide[c];
			str += sv[c].process(p, f, sg, rise && strSide[c], vel);
			outputs[ENV_OUTPUT].setVoltage(sv[c].env * 10.f, c);
			voiceOn[c] = sv[c].env > 0.002f;
			on += voiceOn[c];
			if (!soloOwn) {
				bool trig = rise && soloSide[c];
				if (trig)
					held[c] = true;
				if (!hold)
					held[c] = false;
				bool g = (gate[c] && soloSide[c]) || (hold && held[c]);
				solo += so[c].process(op, f, g, trig, vel);
			}
		}
		for (int c = sch; c < MAXV; c++)
			voiceOn[c] = false;
		outputs[ENV_OUTPUT].setChannels(std::max(sch, 1));
		// the solo from its own inputs
		if (soloOwn) {
			for (int c = 0; c < och; c++) {
				bool was = soloOwnGate[c];
				soloOwnGate[c] = schmitt(was, inputs[SOLO_GATE_INPUT].getPolyVoltage(c));
				bool trig = soloOwnGate[c] && !was;
				if (trig)
					held[c] = true;
				if (!hold)
					held[c] = false;
				float f = C4 * std::pow(2.f, inputs[SOLO_VOCT_INPUT].getPolyVoltage(c));
				float vel = oVel ? clamp(inputs[SOLO_VEL_INPUT].getPolyVoltage(c) * 0.1f, 0.f, 1.f) : 1.f;
				solo += so[c].process(op, f, soloOwnGate[c] || (hold && held[c]), trig, vel);
			}
		}
		voicesOn = on;

		float l, r, so_, st;
		bus->process(bp, str, solo, l, r, st, so_);
		outputs[L_OUTPUT].setVoltage(l);
		outputs[R_OUTPUT].setVoltage(r);
		outputs[STR_OUTPUT].setVoltage(st);
		outputs[SOLO_OUTPUT].setVoltage(so_);
	}
};

// ---------------------------------------------------------------- the display
struct Display : Widget {
	Schmelz* module = nullptr;

	/** k on a morph of n names: the nearer name, the other one and how far towards it (0..0.5). */
	static void morph(float k, int n, int& main, int& other, float& t) {
		float x = clamp(k, 0.f, 1.f) * (n - 1);
		int i = std::min((int) x, n - 2);
		float f = x - i;
		if (f < 0.5f)
			main = i, other = i + 1, t = f;
		else
			main = i + 1, other = i, t = 1.f - f;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		float reg = module ? module->dispReg : 0.2f;
		float snd = module ? module->dispSound : 0.25f;

		// registration: the nearer name big, the blend towards the other below
		int m, o;
		float t;
		morph(reg, NUM_REGS, m, o, t);
		ui::text(vg, bold, mm2px(4.f), mm2px(7.6f), REG_NAMES[m], 15.5f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);
		int pct = (int) std::lround(t * 100.f);
		if (pct > 0)
			ui::text(vg, bold, mm2px(4.f), mm2px(11.7f), string::f("> %s %d%%", REG_NAMES[o], pct), 6.4f, ACCENT,
			         NVG_ALIGN_LEFT, 0.9f);

		// the morph bar
		float bx = mm2px(45.f), bw = mm2px(56.f), by = mm2px(5.5f);
		nvgBeginPath(vg);
		nvgMoveTo(vg, bx, by);
		nvgLineTo(vg, bx + bw, by);
		nvgStrokeColor(vg, ui::EDGE);
		nvgStrokeWidth(vg, 1.4f);
		nvgStroke(vg);
		for (int i = 0; i < NUM_REGS; i++) {
			float x = bx + i * bw / (NUM_REGS - 1);
			nvgBeginPath(vg);
			nvgCircle(vg, x, by, 2.f);
			nvgFillColor(vg, nvgRGB(0x3a, 0x3c, 0x46));
			nvgFill(vg);
			bool near = i == m || (pct > 0 && i == o);
			ui::text(vg, bold, x, mm2px(10.9f), REG_SHORT[i], 5.6f, near ? ui::TXT : ui::DIM, NVG_ALIGN_CENTER, 0.6f);
		}
		nvgBeginPath(vg);
		nvgCircle(vg, bx + clamp(reg, 0.f, 1.f) * bw, by, 4.f);
		nvgFillColor(vg, ACCENT);
		nvgFill(vg);

		// solo sound
		int sm, so;
		float st;
		morph(snd, NUM_SOUNDS, sm, so, st);
		ui::text(vg, bold, mm2px(109.f), mm2px(5.1f), "SOLO", 5.8f, ui::DIM, NVG_ALIGN_LEFT, 0.9f);
		ui::text(vg, bold, mm2px(109.f), mm2px(10.7f), SOUND_NAMES[sm], 10.f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);

		// voices
		int on = module ? module->voicesOn : 4;
		ui::text(vg, bold, mm2px(133.f), mm2px(5.1f), string::f("VOICES %d", on), 5.6f, ui::DIM, NVG_ALIGN_LEFT, 0.9f);
		for (int i = 0; i < MAXV; i++) {
			bool lit = module ? module->voiceOn[i] : i < 4;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(133.f + i * 1.3f), mm2px(8.5f), mm2px(0.9f), mm2px(2.2f), 0.6f);
			nvgFillColor(vg, lit ? ACCENT : ui::OFF);
			nvgFill(vg);
		}
		Widget::drawLayer(args, layer);
	}
};

struct SchmelzWidget : ModuleWidget {
	SchmelzWidget(Schmelz* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Schmelz.svg")));
		follow::addStrip(this, module, {mm2px(Vec(L::STR_VOCT[0], L::STR_VOCT[1])), mm2px(Vec(L::STR_GATE[0], L::STR_GATE[1])), mm2px(Vec(L::STR_VEL[0], L::STR_VEL[1])), mm2px(Vec(L::IN_MOD[0], L::IN_MOD[1]))}); // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);

		using M = Schmelz;
		auto large = [&](const float* p, int id) { addParam(createParamCentered<GLargeKnob>(ui::mm(p), module, id)); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto lamps = [&](const float* p, int id, int n) {
			ui::LampRow* w = createParam<ui::LampRow>(Vec(0, 0), module, id);
			w->place(p, n);
			w->accent = ACCENT;
			addParam(w);
		};

		large(L::REG, M::REG_PARAM);
		knob(L::CRESC, M::CRESC_PARAM);
		knob(L::REL, M::REL_PARAM);
		knob(L::TONE, M::TONE_PARAM);
		small(L::ENSDEPTH, M::ENSDEPTH_PARAM);
		lamps(L::OCT, M::OCT_PARAM, NUM_OCT);
		lamps(L::ENS, M::ENS_PARAM, NUM_ENS);

		large(L::SOUND, M::SOUND_PARAM);
		knob(L::DECAY, M::DECAY_PARAM);
		knob(L::SUSTAIN, M::SUSTAIN_PARAM);
		knob(L::TREM, M::TREM_PARAM);
		lamps(L::SPLIT, M::SPLIT_PARAM, NUM_SPLIT);
		small(L::ATK, M::ATK_PARAM);
		small(L::TRATE, M::TRATE_PARAM);
		small(L::VEL, M::VEL_PARAM);
		addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<SCHMELZ_RGB>>>(
		    ui::mm(L::HOLD), module, M::HOLD_PARAM, M::HOLD_LIGHT));

		knob(L::BAL, M::BAL_PARAM);
		knob(L::ANIM, M::ANIM_PARAM);
		small(L::ARATE, M::ARATE_PARAM);
		knob(L::PHASER, M::PHASER_PARAM);
		small(L::PRATE, M::PRATE_PARAM);
		knob(L::VERB, M::VERB_PARAM);
		small(L::SIZE, M::SIZE_PARAM);
		knob(L::VOLUME, M::VOLUME_PARAM);

		const float* ins[M::NUM_INPUTS] = {L::STR_VOCT, L::STR_GATE, L::STR_VEL, L::SOLO_VOCT, L::SOLO_GATE,
		                                   L::SOLO_VEL, L::IN_REG,   L::IN_CRESC, L::IN_REL,   L::IN_TONE,
		                                   L::IN_ENS,   L::IN_OCT,   L::IN_SOUND, L::IN_DECAY, L::IN_TREM,
		                                   L::IN_BAL,   L::IN_ANIM,  L::IN_VERB,  L::IN_MOD};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_ENV, L::OUT_STR, L::OUT_SOLO, L::OUT_L, L::OUT_R};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("String machine: strings + solo, up to 16 voices each"));
		menu->addChild(createMenuLabel("SOLO GATE unplugged: the solo plays the strings notes (SPLIT at C4)"));
		Schmelz* m = dynamic_cast<Schmelz*>(module);
		if (m)
			menu->addChild(createIndexSubmenuItem("MOD input goes to", {"Crescendo", "Animate", "Tone"},
			                                      [=]() { return (size_t) m->modTarget; },
			                                      [=](size_t v) { m->modTarget = (int) v; }));
	}
};

}  // namespace genra_schmelz

Model* modelSchmelz = createModel<genra_schmelz::Schmelz, genra_schmelz::SchmelzWidget>("Schmelz");
