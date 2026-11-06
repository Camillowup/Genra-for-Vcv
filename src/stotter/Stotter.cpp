// SPDX-License-Identifier: GPL-3.0-only
// STOTTER – performance FX in the GENRA look: 16 effect pads in four banks (stutter, time, gate and fx rows), played
// in LOOP, ONE SHOT or HOLD mode and launched on a grid, a DJ-isolator EQ with kills, a DJ filter, drive and mix.
// The pads take the mouse, the PADS input (polyphonic gates), MIDI notes (C1–D#2, MIDI learn) and MIDI-Map.
// P1–P3 set the pad that plays (or, with EDIT on, the pad picked); < > change its effect.
// Engine: StotterDsp.hpp. Panel: tools/stotter_panel.py (res/Stotter.svg + StotterLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "StotterDsp.hpp"
#include "StotterLayout.hpp"

#include <atomic>

namespace genra_stotter {

namespace L = stlayout;
namespace ui = genra_ui;
using namespace gstot;

// the pad rows in the GENRA track colours, the global green (keep in step with tools/stotter_panel.py)
static const NVGcolor ROW_COL[4] = {nvgRGB(0x7b, 0x6c, 0xff), nvgRGB(0xb8, 0xf2, 0x3a), nvgRGB(0x3d, 0xc9, 0xff),
                                    nvgRGB(0xe0, 0x77, 0x9e)};
static const NVGcolor DARK = nvgRGB(0x07, 0x08, 0x0a), PLATE = nvgRGB(0x21, 0x23, 0x29);
static const char* const CLOCK_MODES[4] = {"16tel (4 PPQN)", "8tel (2 PPQN)", "Viertel (1 PPQN)", "24 PPQN"};
static const int FIRST_NOTE = 36;  // C1 (MIDI note 36) plays pad 1

static NVGcolor fade(NVGcolor c, float a) {
	c.a = a;
	return c;
}
static NVGcolor mixc(NVGcolor a, NVGcolor b, float t) {
	return nvgRGBf(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t);
}

struct Stotter : Module {
	enum ParamId {
		GAIN_PARAM, HIGH_PARAM, MID_PARAM, LOW_PARAM, FREQ_PARAM, PRE_PARAM, KILL_LO_PARAM, KILL_MID_PARAM,
		KILL_HI_PARAM, FILTER_PARAM, RESO_PARAM, DRIVE_PARAM, INTENSITY_PARAM,
		PAD_PARAM,
		LOOP_PARAM = PAD_PARAM + NPADS, ONE_PARAM, HOLD_PARAM, QUANT_PARAM, MIX_PARAM, LATCH_PARAM, KILL_PARAM,
		BANK_PARAM,
		EDIT_PARAM = BANK_PARAM + NBANKS, PREV_PARAM, NEXT_PARAM, P1_PARAM, P2_PARAM, P3_PARAM, INIT_PARAM, REC_PARAM,
		TAP_PARAM, MODE_PARAM, BANKSEL_PARAM, NUM_PARAMS
	};
	enum InputId { IN_L_INPUT, IN_R_INPUT, CLOCK_INPUT, RESET_INPUT, PADS_INPUT, FILT_CV_INPUT, INT_CV_INPUT,
		           KILLS_INPUT, NUM_INPUTS };
	enum OutputId { OUT_L_OUTPUT, OUT_R_OUTPUT, NUM_OUTPUTS };
	enum LightId {
		PRE_LIGHT, KILL_LO_LIGHT, KILL_MID_LIGHT, KILL_HI_LIGHT, LOOP_LIGHT, ONE_LIGHT, HOLD_LIGHT, LATCH_LIGHT,
		KILL_LIGHT, BANK_LIGHT, EDIT_LIGHT = BANK_LIGHT + NBANKS, REC_LIGHT, TAP_LIGHT, NUM_LIGHTS
	};

	struct EqQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float g = eqGain(getValue());
			return g <= 0.f ? "kill" : string::f("%+.1f", 20.f * std::log10(g));
		}
		std::string getUnit() override { return eqGain(getValue()) <= 0.f ? "" : " dB"; }
	};
	struct GainQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%+.1f", 12.f * getValue()); }
		std::string getUnit() override { return " dB"; }
	};
	struct FreqQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float f = midFreq(getValue());
			return f >= 1000.f ? string::f("%.2f k", f / 1000.f) : string::f("%.0f ", f);
		}
		std::string getUnit() override { return "Hz"; }
	};
	struct CutQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float v = getValue();
			if (std::fabs(v) < 0.02f) return "off";
			float a = clampf((std::fabs(v) - 0.02f) / 0.98f, 0.f, 1.f);
			float f = v < 0.f ? 20000.f * std::pow(40.f / 20000.f, a) : 20.f * std::pow(10000.f / 20.f, a);
			return string::f("%s %.0f Hz", v < 0.f ? "LP" : "HP", f);
		}
	};
	/** P1–P3: the value as the pad they set reads it. */
	struct PQuantity : ParamQuantity {
		int k = 0;
		std::string getDisplayValueString() override {
			Stotter* m = dynamic_cast<Stotter*>(module);
			if (!m) return ParamQuantity::getDisplayValueString();
			const PadCfg& c = m->e.pads[m->target];
			return string::f("%s %s", PARAM_NAMES[c.fx][k], paramText(c.fx, k, getValue()).c_str());
		}
	};

	gstot::Engine e;
	dsp::SchmittTrigger clockT, resetT;
	bool down[NPADS] = {};       // pad held (button, PADS input or MIDI)
	bool btn[NPADS] = {}, gate[NPADS] = {}, note[NPADS] = {};
	float prevBtn[NUM_PARAMS] = {};
	int target = 0;              // the slot P1–P3 set
	float loaded[3] = {-1, -1, -1};
	int editPad = 0, lastPad = 0;
	int ctl = 0;
	float killFlash = 0.f;
	// MIDI
	midi::InputQueue midiIn;
	int notes[NPADS];
	std::atomic<int> learn{-1};

	Stotter() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam<GainQuantity>(GAIN_PARAM, -1.f, 1.f, 0.f, "Input gain");
		configParam<EqQuantity>(HIGH_PARAM, -1.f, 1.f, 0.f, "High");
		configParam<EqQuantity>(MID_PARAM, -1.f, 1.f, 0.f, "Mid");
		configParam<EqQuantity>(LOW_PARAM, -1.f, 1.f, 0.f, "Low");
		configParam<FreqQuantity>(FREQ_PARAM, 0.f, 1.f, 0.484f, "Mid band centre");
		paramQuantities[FREQ_PARAM]->description = "Moves the mid band; the crossovers sit a decade apart around it";
		configSwitch(PRE_PARAM, 0.f, 1.f, 1.f, "EQ position", {"After the effects", "Before the effects"});
		configSwitch(KILL_LO_PARAM, 0.f, 1.f, 0.f, "Kill low", {"Off", "On"});
		configSwitch(KILL_MID_PARAM, 0.f, 1.f, 0.f, "Kill mid", {"Off", "On"});
		configSwitch(KILL_HI_PARAM, 0.f, 1.f, 0.f, "Kill high", {"Off", "On"});
		configParam<CutQuantity>(FILTER_PARAM, -1.f, 1.f, 0.f, "DJ filter (left LP, right HP)");
		configParam(RESO_PARAM, 0.f, 1.f, 0.2f, "Resonance", "%", 0.f, 100.f);
		configParam(DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam(INTENSITY_PARAM, 0.f, 1.f, 0.5f, "Intensity", "%", 0.f, 200.f);
		paramQuantities[INTENSITY_PARAM]->description = "100 %: the pads as set · less: softer · more: harder";
		for (int i = 0; i < NPADS; i++) {
			configButton(PAD_PARAM + i, string::f("Pad %d", i + 1));
			notes[i] = FIRST_NOTE + i;
		}
		configButton(LOOP_PARAM, "Mode: loop (the pad plays until another pad or the same again)");
		configButton(ONE_PARAM, "Mode: one shot (one bar, then back)");
		configButton(HOLD_PARAM, "Mode: hold (plays while held)");
		configSwitch(QUANT_PARAM, 0.f, NQUANT - 1, 3.f, "Launch on",
		             {"Off (at once)", "1/16", "1/8", "1/4", "1/2", "1 bar"});
		configParam(MIX_PARAM, 0.f, 1.f, 1.f, "Dry/wet", "%", 0.f, 100.f);
		configSwitch(LATCH_PARAM, 0.f, 1.f, 0.f, "Latch (hold mode: a press starts, the next stops)", {"Off", "On"});
		configButton(KILL_PARAM, "Kill: stop the effect now");
		for (int b = 0; b < NBANKS; b++) configButton(BANK_PARAM + b, string::f("Bank %c", 'A' + b));
		configSwitch(EDIT_PARAM, 0.f, 1.f, 0.f, "Edit (a pad picks the pad P1–P3 set, without playing it)",
		             {"Off", "On"});
		configButton(PREV_PARAM, "Previous effect for this pad");
		configButton(NEXT_PARAM, "Next effect for this pad");
		for (int k = 0; k < 3; k++) {
			PQuantity* q = configParam<PQuantity>(P1_PARAM + k, 0.f, 1.f, 0.5f, string::f("P%d", k + 1));
			q->k = k;
			q->resetEnabled = false;
			q->randomizeEnabled = false;
		}
		configButton(INIT_PARAM, "Init: this pad back to the factory setting");
		configButton(REC_PARAM, "Record 1 bar: loops the next bar instead of the input (again: off)");
		configButton(TAP_PARAM, "Tap tempo (without a clock)");
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Pad mode", {"Loop", "One shot", "Hold"});
		configSwitch(BANKSEL_PARAM, 0.f, NBANKS - 1, 0.f, "Bank", {"A", "B", "C", "D"});
		configInput(IN_L_INPUT, "Left (or mono)");
		configInput(IN_R_INPUT, "Right (unplugged: the left)");
		configInput(CLOCK_INPUT, "Clock (resolution in the menu, as GENRA)");
		configInput(RESET_INPUT, "Reset");
		configInput(PADS_INPUT, "Pads (polyphonic gates: channel 1 = pad 1 … 16)");
		configInput(FILT_CV_INPUT, "Filter CV (±5 V)");
		configInput(INT_CV_INPUT, "Intensity CV (±5 V)");
		configInput(KILLS_INPUT, "Kills (polyphonic gates: low, mid, high)");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
		configBypass(IN_L_INPUT, OUT_L_OUTPUT);
		configBypass(IN_R_INPUT, OUT_R_OUTPUT);
	}

	void onAdd(const AddEvent& ev) override { e.setSampleRate(APP->engine->getSampleRate()); }
	void onSampleRateChange(const SampleRateChangeEvent& ev) override { e.setSampleRate(ev.sampleRate); }
	void onReset(const ResetEvent& ev) override {
		Module::onReset(ev);
		for (int i = 0; i < NBANKS * NPADS; i++) e.pads[i] = defaultPad(i);
		for (int i = 0; i < NPADS; i++) notes[i] = FIRST_NOTE + i;
		e.stop();
		loaded[0] = loaded[1] = loaded[2] = -1.f;
	}

	bool pressed(int id) {
		float v = params[id].getValue();
		bool p = v > 0.5f && prevBtn[id] <= 0.5f;
		prevBtn[id] = v;
		return p;
	}

	/** P1–P3 follow the pad they set: on a change of pad they take its values. */
	void syncKnobs() {
		int t = params[EDIT_PARAM].getValue() > 0.5f ? e.slot(editPad) : (e.act >= 0 ? e.act : e.slot(lastPad));
		if (t != target) {
			target = t;
			loaded[0] = loaded[1] = loaded[2] = -1.f;
		}
		PadCfg& c = e.pads[target];
		for (int k = 0; k < 3; k++) {
			if (loaded[k] < 0.f) {
				params[P1_PARAM + k].setValue(c.p[k]);
				loaded[k] = c.p[k];
				continue;
			}
			float v = params[P1_PARAM + k].getValue();
			if (v != loaded[k]) {
				c.p[k] = v;
				loaded[k] = v;
			}
		}
	}

	void padDown(int i, bool on) {
		if (on == down[i]) return;
		down[i] = on;
		if (on) {
			if (params[EDIT_PARAM].getValue() > 0.5f) {
				editPad = i;
				return;
			}
			lastPad = i;
			e.press(i);
		}
		else if (params[EDIT_PARAM].getValue() <= 0.5f)
			e.release(i);
	}

	void process(const ProcessArgs& args) override {
		// MIDI notes (every sample: timing)
		midi::Message msg;
		while (midiIn.tryPop(&msg, args.frame)) {
			uint8_t st = msg.getStatus();
			if (st != 0x9 && st != 0x8) continue;
			int n = msg.getNote();
			bool on = st == 0x9 && msg.getValue() > 0;
			int l = learn.load();
			if (on && l >= 0) {
				notes[l] = n;
				learn = -1;
				continue;
			}
			for (int i = 0; i < NPADS; i++)
				if (notes[i] == n) note[i] = on;
		}

		if (--ctl <= 0) {
			ctl = 16;
			Controls& c = e.c;
			c.gain = params[GAIN_PARAM].getValue();
			c.high = params[HIGH_PARAM].getValue();
			c.mid = params[MID_PARAM].getValue();
			c.low = params[LOW_PARAM].getValue();
			c.freq = params[FREQ_PARAM].getValue();
			c.pre = params[PRE_PARAM].getValue() > 0.5f;
			auto kg = [&](int ch) { return inputs[KILLS_INPUT].getChannels() > ch && inputs[KILLS_INPUT].getVoltage(ch) > 1.f; };
			c.killLo = params[KILL_LO_PARAM].getValue() > 0.5f || kg(0);
			c.killMid = params[KILL_MID_PARAM].getValue() > 0.5f || kg(1);
			c.killHi = params[KILL_HI_PARAM].getValue() > 0.5f || kg(2);
			c.reso = params[RESO_PARAM].getValue();
			c.drive = params[DRIVE_PARAM].getValue();
			c.intensity = clamp(params[INTENSITY_PARAM].getValue() + inputs[INT_CV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
			c.quant = (int) std::round(params[QUANT_PARAM].getValue());
			c.mix = params[MIX_PARAM].getValue();
			c.latch = params[LATCH_PARAM].getValue() > 0.5f;

			// buttons
			for (int m = 0; m < 3; m++)
				if (pressed(LOOP_PARAM + m)) params[MODE_PARAM].setValue((float) m);
			c.mode = (int) std::round(params[MODE_PARAM].getValue());
			for (int b = 0; b < NBANKS; b++)
				if (pressed(BANK_PARAM + b)) params[BANKSEL_PARAM].setValue((float) b);
			e.bank = (int) std::round(params[BANKSEL_PARAM].getValue());
			if (pressed(KILL_PARAM)) {
				e.kill();
				killFlash = 0.15f;
			}
			if (pressed(REC_PARAM)) e.recToggle();
			if (pressed(TAP_PARAM)) e.tap();
			syncKnobs();
			// < > and INIT act on the pad P1–P3 set
			bool prev = pressed(PREV_PARAM), next = pressed(NEXT_PARAM), init = pressed(INIT_PARAM);
			if (prev || next || init) {
				PadCfg& pc = e.pads[target];
				if (init) pc = defaultPad(target % NPADS);
				else pc.fx = (pc.fx + (next ? 1 : NUM_FX - 1)) % NUM_FX;
				loaded[0] = loaded[1] = loaded[2] = -1.f;
				syncKnobs();
			}

			// pads: the button, the PADS input, MIDI
			int pc = inputs[PADS_INPUT].getChannels();
			for (int i = 0; i < NPADS; i++) {
				btn[i] = params[PAD_PARAM + i].getValue() > 0.5f;
				gate[i] = i < pc && inputs[PADS_INPUT].getVoltage(i) > 1.f;
				padDown(i, btn[i] || gate[i] || note[i]);
			}

			float dt = args.sampleTime * 16.f;
			killFlash = std::max(0.f, killFlash - dt);
			lights[PRE_LIGHT].setBrightness(c.pre ? 1.f : 0.f);
			lights[KILL_LO_LIGHT].setBrightness(c.killLo ? 1.f : 0.f);
			lights[KILL_MID_LIGHT].setBrightness(c.killMid ? 1.f : 0.f);
			lights[KILL_HI_LIGHT].setBrightness(c.killHi ? 1.f : 0.f);
			for (int m = 0; m < 3; m++) lights[LOOP_LIGHT + m].setBrightness(c.mode == m ? 1.f : 0.f);
			lights[LATCH_LIGHT].setBrightness(c.latch ? 1.f : 0.f);
			lights[KILL_LIGHT].setBrightness(killFlash > 0.f ? 1.f : 0.f);
			for (int b = 0; b < NBANKS; b++) lights[BANK_LIGHT + b].setBrightness(e.bank == b ? 1.f : 0.f);
			lights[EDIT_LIGHT].setBrightness(params[EDIT_PARAM].getValue() > 0.5f ? 1.f : 0.f);
			float frac = (float) (e.beat - std::floor(e.beat));
			float rec = 0.f;
			if (e.cap == gstot::Engine::CAP_ARMED) rec = frac < 0.5f ? 1.f : 0.f;
			else if (e.cap == gstot::Engine::CAP_REC) rec = 1.f;
			else if (e.cap == gstot::Engine::CAP_PLAY) rec = 0.45f;
			lights[REC_LIGHT].setBrightness(rec);
			lights[TAP_LIGHT].setBrightness(frac < 0.12f ? 1.f : 0.f);
		}

		// clock and reset
		if (inputs[RESET_INPUT].isConnected() && resetT.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			e.resetPulse();
		if (inputs[CLOCK_INPUT].isConnected() && clockT.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f))
			e.clockPulse();

		float l = inputs[IN_L_INPUT].getVoltageSum();
		float r = inputs[IN_R_INPUT].isConnected() ? inputs[IN_R_INPUT].getVoltageSum() : l;
		e.c.cut = clamp(params[FILTER_PARAM].getValue() + inputs[FILT_CV_INPUT].getVoltage() * 0.2f, -1.f, 1.f);
		float ol, orr;
		e.process(l, r, ol, orr);
		outputs[OUT_L_OUTPUT].setVoltage(ol);
		outputs[OUT_R_OUTPUT].setVoltage(orr);
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_t* pa = json_array();
		for (int i = 0; i < NBANKS * NPADS; i++) {
			json_t* p = json_array();
			json_array_append_new(p, json_integer(e.pads[i].fx));
			for (int k = 0; k < 3; k++) json_array_append_new(p, json_real(e.pads[i].p[k]));
			json_array_append_new(pa, p);
		}
		json_object_set_new(root, "pads", pa);
		json_t* na = json_array();
		for (int i = 0; i < NPADS; i++) json_array_append_new(na, json_integer(notes[i]));
		json_object_set_new(root, "notes", na);
		json_object_set_new(root, "clockMode", json_integer(e.c.clockMode));
		json_object_set_new(root, "editPad", json_integer(editPad));
		json_object_set_new(root, "lastPad", json_integer(lastPad));
		json_object_set_new(root, "bpm", json_real(e.bpm));
		json_object_set_new(root, "midi", midiIn.toJson());
		return root;
	}
	void dataFromJson(json_t* root) override {
		if (json_t* pa = json_object_get(root, "pads")) {
			for (int i = 0; i < NBANKS * NPADS && i < (int) json_array_size(pa); i++) {
				json_t* p = json_array_get(pa, i);
				if (json_array_size(p) < 4) continue;
				e.pads[i].fx = clamp((int) json_integer_value(json_array_get(p, 0)), 0, NUM_FX - 1);
				for (int k = 0; k < 3; k++)
					e.pads[i].p[k] = clamp((float) json_number_value(json_array_get(p, k + 1)), 0.f, 1.f);
			}
		}
		if (json_t* na = json_object_get(root, "notes"))
			for (int i = 0; i < NPADS && i < (int) json_array_size(na); i++)
				notes[i] = clamp((int) json_integer_value(json_array_get(na, i)), 0, 127);
		if (json_t* j = json_object_get(root, "clockMode")) e.c.clockMode = clamp((int) json_integer_value(j), 0, 3);
		if (json_t* j = json_object_get(root, "editPad")) editPad = clamp((int) json_integer_value(j), 0, NPADS - 1);
		if (json_t* j = json_object_get(root, "lastPad")) lastPad = clamp((int) json_integer_value(j), 0, NPADS - 1);
		if (json_t* j = json_object_get(root, "bpm")) e.setBpm((float) json_number_value(j));
		if (json_t* j = json_object_get(root, "midi")) midiIn.fromJson(j);
		loaded[0] = loaded[1] = loaded[2] = -1.f;
	}
};

// ---------------------------------------------------------------- the pads
struct PadButton : app::Switch {
	Stotter* st = nullptr;
	int pad = 0;
	std::shared_ptr<window::Font> font;
	PadButton() { momentary = true; }
	void draw(const DrawArgs& args) override {
		if (!font) font = ui::boldFont();
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y;
		PadCfg c = st ? st->e.pads[st->e.slot(pad)] : defaultPad(pad);
		int row = FX_ROW[c.fx];
		NVGcolor col = ROW_COL[row];
		int state = st ? st->e.padState(pad) : (pad == 2 ? 1 : 0);
		bool on = state == 1 || (state == 2 && std::fmod(system::getTime(), 0.25) < 0.125);
		bool down = st && st->down[pad];
		bool edit = st && st->params[Stotter::EDIT_PARAM].getValue() > 0.5f && st->editPad == pad;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.5f, 0.5f, w - 1.f, h - 1.f, 3.5f);
		nvgFillColor(vg, on ? col : mixc(PLATE, col, down ? 0.4f : 0.22f));
		nvgFill(vg);
		nvgStrokeWidth(vg, edit ? 1.6f : 0.8f);
		nvgStrokeColor(vg, edit ? ui::WHITE : fade(col, 0.75f));
		nvgStroke(vg);
		ui::text(vg, font, w / 2, h * 0.38f, ROW_NAMES[row], 6.f, on ? DARK : col, NVG_ALIGN_CENTER, 0.6f);
		std::string lab = padLabel(c);
		ui::text(vg, font, w / 2, h * 0.68f, lab, lab.size() > 5 ? 7.4f : 9.5f, on ? DARK : ui::TXT, NVG_ALIGN_CENTER, 0.4f);
		if (state == 1 && st) {
			float p = st->e.progress();
			if (p >= 0.f) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, 3.f, h - 5.f, (w - 6.f) * p, 1.8f, 0.9f);
				nvgFillColor(vg, DARK);
				nvgFill(vg);
			}
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && st && st->e.padState(pad) == 1) {
			// a soft glow around a playing pad
			NVGcolor col = ROW_COL[FX_ROW[st->e.pads[st->e.slot(pad)].fx]];
			float w = box.size.x, h = box.size.y;
			nvgBeginPath(args.vg);
			nvgRect(args.vg, -6, -6, w + 12, h + 12);
			nvgFillPaint(args.vg, nvgBoxGradient(args.vg, 0, 0, w, h, 4.f, 10.f, fade(col, 0.35f), fade(col, 0.f)));
			nvgFill(args.vg);
		}
		Switch::drawLayer(args, layer);
	}
};

// ---------------------------------------------------------------- the display and the edit bar
struct Display : TransparentWidget {
	Stotter* module = nullptr;
	std::shared_ptr<window::Font> bold, mono;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) return;
		if (!bold) bold = ui::boldFont();
		if (!mono) mono = ui::monoFont();
		NVGcontext* vg = args.vg;
		float w = box.size.x, h = box.size.y;
		bool editing = module && module->params[Stotter::EDIT_PARAM].getValue() > 0.5f;
		gstot::Engine* e = module ? &module->e : nullptr;
		int shown = e ? (e->act >= 0 ? e->act : module->target) : 2;
		PadCfg c = e ? e->pads[shown] : defaultPad(2);
		NVGcolor col = ROW_COL[FX_ROW[c.fx]];
		if (editing) {
			int pad = module->editPad;
			ui::text(vg, bold, 10, 14, string::f("EDIT · PAD %d · BANK %c", pad + 1, 'A' + e->bank), 8.5f, col);
			ui::text(vg, bold, w - 10, 14, string::f("<  %s  >", FX_NAMES[c.fx]), 8.5f, ui::TXT, NVG_ALIGN_RIGHT);
			for (int k = 0; k < 3; k++) {
				float x = w * (0.2f + 0.3f * k);
				ui::text(vg, bold, x, 27, string::f("P%d %s", k + 1, PARAM_NAMES[c.fx][k]), 7.f, ui::DIM, NVG_ALIGN_CENTER);
				ui::text(vg, bold, x, 41, paramText(c.fx, k, c.p[k]), 11.5f, ui::TXT, NVG_ALIGN_CENTER);
			}
			return;
		}
		std::string title = "STOTTER";
		if (e && e->act >= 0) title = string::f("%s %s", FX_NAMES[c.fx], paramText(c.fx, 0, c.p[0]).c_str());
		else if (e && e->pending >= 0) title = string::f("WAIT · %s", FX_NAMES[e->pads[e->pending].fx]);
		ui::text(vg, bold, 10, 14, title, 8.5f, e && (e->act >= 0 || e->pending >= 0) ? col : ui::DIM);
		std::string info = "LOOP · QUANT 1/4 · 120 BPM · EQ PRE";
		if (e) {
			info = string::f("%s · BANK %c · QUANT %s · %s%.0f BPM · EQ %s", MODE_NAMES[e->c.mode], 'A' + e->bank,
			                 QUANT_NAMES[clamp(e->c.quant, 0, NQUANT - 1)], e->ext ? "EXT " : "", e->bpm,
			                 e->c.pre ? "PRE" : "POST");
			if (e->cap == gstot::Engine::CAP_ARMED) info = "REC WAITS FOR THE BAR · " + info;
			else if (e->cap == gstot::Engine::CAP_REC) info = "RECORDING · " + info;
			else if (e->cap == gstot::Engine::CAP_PLAY) info = "LOOP PLAYS · " + info;
		}
		ui::text(vg, bold, w - 10, 14, info, 6.8f, ui::DIM, NVG_ALIGN_RIGHT, 0.3f);
		// the pattern of one bar
		float x0 = 10, x1 = w - 10, y0 = 20, y1 = h - 6;
		for (int q = 0; q <= 4; q++) {
			float x = x0 + (x1 - x0) * q / 4.f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, x, y0 - 1);
			nvgLineTo(vg, x, y1 + 1);
			nvgStrokeColor(vg, PLATE);
			nvgStrokeWidth(vg, 1.f);
			nvgStroke(vg);
		}
		bool live = e && e->act >= 0;
		nvgBeginPath(vg);
		const int N = 160;
		for (int i = 0; i <= N; i++) {
			float x = (float) i / N;
			float v = clampf(displayCurve(c, std::min(x, 0.9999f)), 0.f, 1.f);
			float px = x0 + (x1 - x0) * x, py = y1 - (y1 - y0) * v;
			if (i == 0) nvgMoveTo(vg, px, py);
			else nvgLineTo(vg, px, py);
		}
		nvgStrokeColor(vg, live ? col : fade(col, 0.35f));
		nvgStrokeWidth(vg, 1.3f);
		nvgStroke(vg);
		if (live) {
			float p = e->progress();
			float px = x0 + (x1 - x0) * p;
			nvgBeginPath(vg);
			nvgMoveTo(vg, px, y0 - 2);
			nvgLineTo(vg, px, y1 + 2);
			nvgStrokeColor(vg, ui::TXT);
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		}
	}
};

/** The bar on top of the edit strip takes the colour of the pad P1–P3 set. */
struct EditBar : TransparentWidget {
	Stotter* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1 || !module) return;
		NVGcolor col = ROW_COL[FX_ROW[module->e.pads[module->target].fx]];
		nvgBeginPath(args.vg);
		nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, box.size.y / 2);
		nvgFillColor(args.vg, col);
		nvgFill(args.vg);
	}
};

// ---------------------------------------------------------------- widget
template <int R, int G, int B>
using Bez = VCVLightBezel<ui::AccentLight<R, G, B>>;
template <int R, int G, int B>
using BezLatch = VCVLightBezelLatch<ui::AccentLight<R, G, B>>;
template <int R, int G, int B>
using Small = VCVLightButton<MediumSimpleLight<ui::AccentLight<R, G, B>>>;
template <int R, int G, int B>
using SmallLatch = VCVLightLatch<MediumSimpleLight<ui::AccentLight<R, G, B>>>;
#define WHITE_ 0xf2, 0xf3, 0xf7
#define RED_ 0xff, 0x4f, 0x6d

struct StotterWidget : ModuleWidget {
	StotterWidget(Stotter* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Stotter.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using M = Stotter;
		auto small = [&](const float* p, int id) { addParam(createParamCentered<RoundSmallBlackKnob>(ui::mm(p), module, id)); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<RoundBlackKnob>(ui::mm(p), module, id)); };
		auto in = [&](const float* p, int id) { addInput(createInputCentered<PJ301MPort>(ui::mm(p), module, id)); };

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);
		EditBar* eb = new EditBar;
		eb->module = module;
		eb->box.pos = mm2px(Vec(L::EDITBAR[0], L::EDITBAR[1]));
		eb->box.size = mm2px(Vec(L::EDITBAR[2], L::EDITBAR[3]));
		addChild(eb);

		// edit strip
		addParam(createLightParamCentered<BezLatch<WHITE_>>(ui::mm(L::EDIT), module, M::EDIT_PARAM, M::EDIT_LIGHT));
		addParam(createParamCentered<VCVBezel>(ui::mm(L::PREV), module, M::PREV_PARAM));
		addParam(createParamCentered<VCVBezel>(ui::mm(L::NEXT), module, M::NEXT_PARAM));
		small(L::P1, M::P1_PARAM);
		small(L::P2, M::P2_PARAM);
		small(L::P3, M::P3_PARAM);
		addParam(createParamCentered<VCVBezel>(ui::mm(L::INIT), module, M::INIT_PARAM));
		addParam(createLightParamCentered<Bez<RED_>>(ui::mm(L::REC), module, M::REC_PARAM, M::REC_LIGHT));
		addParam(createLightParamCentered<Bez<WHITE_>>(ui::mm(L::TAP), module, M::TAP_PARAM, M::TAP_LIGHT));

		// EQ
		small(L::GAIN, M::GAIN_PARAM);
		addParam(createLightParamCentered<BezLatch<WHITE_>>(ui::mm(L::PRE), module, M::PRE_PARAM, M::PRE_LIGHT));
		knob(L::HIGH, M::HIGH_PARAM);
		knob(L::MID, M::MID_PARAM);
		knob(L::LOW, M::LOW_PARAM);
		small(L::FREQ, M::FREQ_PARAM);
		addParam(createLightParamCentered<SmallLatch<RED_>>(ui::mm(L::KILL_LO), module, M::KILL_LO_PARAM, M::KILL_LO_LIGHT));
		addParam(createLightParamCentered<SmallLatch<RED_>>(ui::mm(L::KILL_MID), module, M::KILL_MID_PARAM, M::KILL_MID_LIGHT));
		addParam(createLightParamCentered<SmallLatch<RED_>>(ui::mm(L::KILL_HI), module, M::KILL_HI_PARAM, M::KILL_HI_LIGHT));

		// filter
		addParam(createParamCentered<RoundLargeBlackKnob>(ui::mm(L::FILTER), module, M::FILTER_PARAM));
		small(L::RESO, M::RESO_PARAM);
		small(L::DRIVE, M::DRIVE_PARAM);
		knob(L::INTENSITY, M::INTENSITY_PARAM);

		// pads
		const float* padA[NPADS] = {L::PAD1, L::PAD2, L::PAD3, L::PAD4, L::PAD5, L::PAD6, L::PAD7, L::PAD8,
		                            L::PAD9, L::PAD10, L::PAD11, L::PAD12, L::PAD13, L::PAD14, L::PAD15, L::PAD16};
		for (int i = 0; i < NPADS; i++) {
			PadButton* p = createParam<PadButton>(mm2px(Vec(padA[i][0], padA[i][1])), module, M::PAD_PARAM + i);
			p->box.size = mm2px(Vec(padA[i][2], padA[i][3]));
			p->st = module;
			p->pad = i;
			addParam(p);
		}

		// perform
		addParam(createLightParamCentered<Bez<WHITE_>>(ui::mm(L::M_LOOP), module, M::LOOP_PARAM, M::LOOP_LIGHT));
		addParam(createLightParamCentered<Bez<WHITE_>>(ui::mm(L::M_ONE), module, M::ONE_PARAM, M::ONE_LIGHT));
		addParam(createLightParamCentered<Bez<WHITE_>>(ui::mm(L::M_HOLD), module, M::HOLD_PARAM, M::HOLD_LIGHT));
		small(L::QUANT, M::QUANT_PARAM);
		small(L::MIX, M::MIX_PARAM);
		addParam(createLightParamCentered<BezLatch<WHITE_>>(ui::mm(L::LATCH), module, M::LATCH_PARAM, M::LATCH_LIGHT));
		addParam(createLightParamCentered<Bez<RED_>>(ui::mm(L::KILL), module, M::KILL_PARAM, M::KILL_LIGHT));
		const float* bank[NBANKS] = {L::BANK_A, L::BANK_B, L::BANK_C, L::BANK_D};
		for (int b = 0; b < NBANKS; b++)
			addParam(createLightParamCentered<Small<WHITE_>>(ui::mm(bank[b]), module, M::BANK_PARAM + b, M::BANK_LIGHT + b));

		// jacks
		in(L::IN_L, M::IN_L_INPUT);
		in(L::IN_R, M::IN_R_INPUT);
		in(L::CLOCK, M::CLOCK_INPUT);
		in(L::RESET, M::RESET_INPUT);
		in(L::PADS, M::PADS_INPUT);
		in(L::FILT_CV, M::FILT_CV_INPUT);
		in(L::INT_CV, M::INT_CV_INPUT);
		in(L::KILLS, M::KILLS_INPUT);
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_L), module, M::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_R), module, M::OUT_R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		Stotter* m = dynamic_cast<Stotter*>(module);
		if (!m) return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
		    "Clock resolution", std::vector<std::string>(CLOCK_MODES, CLOCK_MODES + 4),
		    [=]() -> size_t { return (size_t) m->e.c.clockMode; }, [=](size_t i) { m->e.c.clockMode = (int) i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("MIDI: notes play the pads"));
		app::appendMidiMenu(menu, &m->midiIn);
		menu->addChild(createSubmenuItem("MIDI learn", "", [=](Menu* sub) {
			for (int i = 0; i < NPADS; i++) {
				int n = m->notes[i];
				static const char* const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
				std::string cur = string::f("%s%d", NN[n % 12], n / 12 - 2);
				sub->addChild(createMenuItem(string::f("Pad %d (now %s)", i + 1, cur.c_str()),
				                             m->learn.load() == i ? "waiting…" : "", [=]() { m->learn = i; }));
			}
			sub->addChild(new MenuSeparator);
			sub->addChild(createMenuItem("Back to C1 … D#2", "", [=]() {
				for (int i = 0; i < NPADS; i++) m->notes[i] = FIRST_NOTE + i;
			}));
		}));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem(string::f("Factory pads for bank %c", 'A' + m->e.bank), "", [=]() {
			for (int i = 0; i < NPADS; i++) m->e.pads[m->e.slot(i)] = defaultPad(i);
			m->loaded[0] = m->loaded[1] = m->loaded[2] = -1.f;
		}));
	}
};

}  // namespace genra_stotter

Model* modelStotter = createModel<genra_stotter::Stotter, genra_stotter::StotterWidget>("GenraStotter");
