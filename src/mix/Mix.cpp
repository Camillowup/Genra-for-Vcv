// SPDX-License-Identifier: GPL-3.0-only
// MIX – 4-track stereo mixer in the GENRA look: one strip per GENRA track (DRUMS, BASS, MELODY, PADS) in its colour,
// a dark master with two send/return loops (A, B), ducking (GENRA's KICK into DUCK), width and drive.
// Engine: MixDsp.hpp. Panel: tools/mix_panel.py (res/Mix.svg + MixLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "MixDsp.hpp"
#include "MixLayout.hpp"

namespace genra_mix {

namespace L = mxlayout;
namespace ui = genra_ui;
using namespace gmix;

// the GENRA track colours (keep in step with tools/mix_panel.py)
static const NVGcolor TRACK[NCH] = {nvgRGB(0x7b, 0x6c, 0xff), nvgRGB(0xb8, 0xf2, 0x3a), nvgRGB(0x3d, 0xc9, 0xff),
                                    nvgRGB(0xe0, 0x77, 0x9e)};
static const NVGcolor METER_GREEN = nvgRGB(0x5d, 0xff, 0x8a), METER_YELLOW = nvgRGB(0xff, 0xc2, 0x4d),
                      METER_RED = nvgRGB(0xff, 0x4f, 0x4f);
static const char* const TRACK_NAMES[NCH] = {"Drums", "Bass", "Melody", "Pads"};

struct Mix : Module {
	// per channel: HIGH, LOW, SEND, DUCK, PAN, LEVEL, MUTE, SOLO
	enum ChParam { HIGH, LOW, SEND, DUCK, PAN, LEVEL, MUTE, SOLO, NCP };
	enum ParamId {
		M_DUCK_PARAM = NCH * NCP, M_TIME_PARAM, M_WIDTH_PARAM, M_RETURN_PARAM, M_DRIVE_PARAM, M_SEND_PARAM,
		M_MASTER_PARAM,
		// added with the second send (kept at the end, so older patches load unchanged)
		SENDB_PARAM, M_SENDB_PARAM = SENDB_PARAM + NCH, M_RETURNB_PARAM, NUM_PARAMS
	};
	// per channel: IN L, IN R, LVL, PAN
	enum ChInput { IN_L, IN_R, IN_LVL, IN_PAN, NCI };
	enum InputId { DUCK_INPUT = NCH * NCI, RET_L_INPUT, RET_R_INPUT, RETB_L_INPUT, RETB_R_INPUT, NUM_INPUTS };
	enum OutputId { SEND_L_OUTPUT, SEND_R_OUTPUT, OUT_L_OUTPUT, OUT_R_OUTPUT, SENDB_L_OUTPUT, SENDB_R_OUTPUT, NUM_OUTPUTS };
	enum LightId { MUTE_LIGHT, SOLO_LIGHT = MUTE_LIGHT + NCH, NUM_LIGHTS = SOLO_LIGHT + NCH };

	static int pid(int ch, int k) { return ch * NCP + k; }
	static int iid(int ch, int k) { return ch * NCI + k; }

	struct LevelQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			float g = levelGain(getValue());
			return g <= 0.f ? "−∞" : string::f("%+.1f", 20.f * std::log10(g));
		}
		std::string getUnit() override { return levelGain(getValue()) <= 0.f ? "" : " dB"; }
	};
	struct EqQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%+.1f", eqDb(getValue())); }
		std::string getUnit() override { return " dB"; }
	};
	struct TimeQuantity : ParamQuantity {
		std::string getDisplayValueString() override { return string::f("%.0f", duckSeconds(getValue()) * 1000.f); }
		std::string getUnit() override { return " ms"; }
	};

	std::unique_ptr<Mixer> mx{new Mixer};
	ChannelParams cp[NCH];
	MasterParams mp;
	int ctl = 0;

	Mix() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int c = 0; c < NCH; c++) {
			std::string n = TRACK_NAMES[c];
			configParam<EqQuantity>(pid(c, HIGH), -1.f, 1.f, 0.f, n + " high (6 kHz shelf)");
			configParam<EqQuantity>(pid(c, LOW), -1.f, 1.f, 0.f, n + " low (120 Hz shelf)");
			configParam(pid(c, SEND), 0.f, 1.f, 0.f, n + " send A (post-fader)", "%", 0.f, 100.f);
			configParam(SENDB_PARAM + c, 0.f, 1.f, 0.f, n + " send B (post-fader)", "%", 0.f, 100.f);
			configParam(pid(c, DUCK), 0.f, 1.f, c == 0 ? 0.f : 0.5f, n + " ducking", "%", 0.f, 100.f);
			paramQuantities[pid(c, DUCK)]->description = "How much this channel dips on a trigger at the DUCK input";
			configParam(pid(c, PAN), -1.f, 1.f, 0.f, n + " pan", "%", 0.f, 100.f);
			configParam<LevelQuantity>(pid(c, LEVEL), 0.f, 1.f, 0.75f, n + " level");
			configSwitch(pid(c, MUTE), 0.f, 1.f, 0.f, n + " mute", {"Off", "On"});
			configSwitch(pid(c, SOLO), 0.f, 1.f, 0.f, n + " solo", {"Off", "On"});
			configInput(iid(c, IN_L), n + " left (or mono)");
			configInput(iid(c, IN_R), n + " right (unplugged: the left)");
			configInput(iid(c, IN_LVL), n + " level CV (0–10 V)");
			configInput(iid(c, IN_PAN), n + " pan CV (±5 V)");
		}
		configParam(M_DUCK_PARAM, 0.f, 1.f, 0.6f, "Ducking depth", "%", 0.f, 100.f);
		configParam<TimeQuantity>(M_TIME_PARAM, 0.f, 1.f, 0.55f, "Ducking time");
		configParam(M_WIDTH_PARAM, 0.f, 1.f, 0.5f, "Stereo width", "%", 0.f, 200.f);
		configParam<LevelQuantity>(M_RETURN_PARAM, 0.f, 1.f, 0.75f, "Return A level");
		configParam(M_DRIVE_PARAM, 0.f, 1.f, 0.f, "Drive", "%", 0.f, 100.f);
		configParam<LevelQuantity>(M_SEND_PARAM, 0.f, 1.f, 0.75f, "Send A level");
		configParam<LevelQuantity>(M_SENDB_PARAM, 0.f, 1.f, 0.75f, "Send B level");
		configParam<LevelQuantity>(M_RETURNB_PARAM, 0.f, 1.f, 0.75f, "Return B level");
		configParam<LevelQuantity>(M_MASTER_PARAM, 0.f, 1.f, 0.75f, "Master level");
		configInput(DUCK_INPUT, "Duck trigger (e.g. GENRA's KICK)");
		configInput(RET_L_INPUT, "Return A left (or mono)");
		configInput(RET_R_INPUT, "Return A right (unplugged: the left)");
		configInput(RETB_L_INPUT, "Return B left (or mono)");
		configInput(RETB_R_INPUT, "Return B right (unplugged: the left)");
		configOutput(SEND_L_OUTPUT, "Send A left");
		configOutput(SEND_R_OUTPUT, "Send A right");
		configOutput(SENDB_L_OUTPUT, "Send B left");
		configOutput(SENDB_R_OUTPUT, "Send B right");
		configOutput(OUT_L_OUTPUT, "Left");
		configOutput(OUT_R_OUTPUT, "Right");
	}

	void onAdd(const AddEvent& e) override { mx->setSampleRate(APP->engine->getSampleRate()); }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { mx->setSampleRate(e.sampleRate); }

	void process(const ProcessArgs& args) override {
		if (--ctl <= 0) {
			ctl = 16;
			for (int c = 0; c < NCH; c++) {
				ChannelParams& p = cp[c];
				p.high = params[pid(c, HIGH)].getValue();
				p.low = params[pid(c, LOW)].getValue();
				p.send = params[pid(c, SEND)].getValue();
				p.sendB = params[SENDB_PARAM + c].getValue();
				p.duck = params[pid(c, DUCK)].getValue();
				p.level = params[pid(c, LEVEL)].getValue();
				p.mute = params[pid(c, MUTE)].getValue() > 0.5f;
				p.solo = params[pid(c, SOLO)].getValue() > 0.5f;
				lights[MUTE_LIGHT + c].setBrightness(p.mute ? 1.f : 0.f);
				lights[SOLO_LIGHT + c].setBrightness(p.solo ? 1.f : 0.f);
			}
			mp.duck = params[M_DUCK_PARAM].getValue();
			mp.time = params[M_TIME_PARAM].getValue();
			mp.width = params[M_WIDTH_PARAM].getValue();
			mp.ret = params[M_RETURN_PARAM].getValue();
			mp.drive = params[M_DRIVE_PARAM].getValue();
			mp.send = params[M_SEND_PARAM].getValue();
			mp.master = params[M_MASTER_PARAM].getValue();
			mp.sendB = params[M_SENDB_PARAM].getValue();
			mp.retB = params[M_RETURNB_PARAM].getValue();
		}
		float in[NCH][2];
		for (int c = 0; c < NCH; c++) {
			// polyphonic inputs are summed; R follows L when unplugged
			float l = inputs[iid(c, IN_L)].getVoltageSum();
			in[c][0] = l;
			in[c][1] = inputs[iid(c, IN_R)].isConnected() ? inputs[iid(c, IN_R)].getVoltageSum() : l;
			cp[c].vca = inputs[iid(c, IN_LVL)].isConnected() ? clamp(inputs[iid(c, IN_LVL)].getVoltage() * 0.1f, 0.f, 1.f) : 1.f;
			cp[c].pan = clamp(params[pid(c, PAN)].getValue() + inputs[iid(c, IN_PAN)].getVoltage() * 0.2f, -1.f, 1.f);
		}
		float rl = inputs[RET_L_INPUT].getVoltageSum();
		float rr = inputs[RET_R_INPUT].isConnected() ? inputs[RET_R_INPUT].getVoltageSum() : rl;
		float rbl = inputs[RETB_L_INPUT].getVoltageSum();
		float rbr = inputs[RETB_R_INPUT].isConnected() ? inputs[RETB_R_INPUT].getVoltageSum() : rbl;
		float ol, orr, sl, sr, sbl, sbr;
		mx->process(cp, mp, in, inputs[DUCK_INPUT].getVoltage(), rl, rr, rbl, rbr, ol, orr, sl, sr, sbl, sbr);
		outputs[OUT_L_OUTPUT].setVoltage(ol);
		outputs[OUT_R_OUTPUT].setVoltage(orr);
		outputs[SEND_L_OUTPUT].setVoltage(sl);
		outputs[SEND_R_OUTPUT].setVoltage(sr);
		outputs[SENDB_L_OUTPUT].setVoltage(sbl);
		outputs[SENDB_R_OUTPUT].setVoltage(sbr);
	}
};

// ---------------------------------------------------------------- meters
/** 12 segments per bar, in the channel colour, yellow from −6 dB, red from 0 dB. */
struct MeterWidget : Widget {
	Mix* module = nullptr;
	int ch = 0;  // 0..3, or −1: the master (two bars)
	NVGcolor color = METER_GREEN;
	void bar(NVGcontext* vg, float x, int lit) {
		float seg = box.size.y / 12.f;
		for (int k = 0; k < 12; k++) {
			NVGcolor c = k >= 10 ? METER_RED : (k >= 8 ? METER_YELLOW : color);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, box.size.y - (k + 1) * seg + 0.6f, mm2px(1.8f), seg - 1.2f, 0.8f);
			nvgFillColor(vg, k < lit ? c : ui::OFF);
			nvgFill(vg);
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			float x0 = mm2px(0.3f);
			if (ch >= 0) {
				bar(args.vg, x0, module ? module->mx->ch[ch].meter.segments() : 0);
			} else {
				bar(args.vg, x0, module ? module->mx->mL.segments() : 0);
				bar(args.vg, x0 + mm2px(3.f), module ? module->mx->mR.segments() : 0);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

template <int R, int G, int B>
using Lit = VCVLightBezelLatch<ui::AccentLight<R, G, B>>;

struct MixWidget : ModuleWidget {
	MixWidget(Mix* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Mix.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		using M = Mix;
		auto small = [&](const float* p, int id) { addParam(createParamCentered<RoundSmallBlackKnob>(ui::mm(p), module, id)); };
		auto large = [&](const float* p, int id) { addParam(createParamCentered<RoundLargeBlackKnob>(ui::mm(p), module, id)); };
		auto meter = [&](const float* a, int ch, NVGcolor c) {
			MeterWidget* w = new MeterWidget;
			w->module = module;
			w->ch = ch;
			w->color = c;
			w->box.pos = mm2px(Vec(a[0], a[1]));
			w->box.size = mm2px(Vec(a[2], a[3]));
			addChild(w);
		};

		const float* high[NCH] = {L::HIGH1, L::HIGH2, L::HIGH3, L::HIGH4};
		const float* low[NCH] = {L::LOW1, L::LOW2, L::LOW3, L::LOW4};
		const float* send[NCH] = {L::SEND1, L::SEND2, L::SEND3, L::SEND4};
		const float* sendb[NCH] = {L::SENDB1, L::SENDB2, L::SENDB3, L::SENDB4};
		const float* duck[NCH] = {L::DUCK1, L::DUCK2, L::DUCK3, L::DUCK4};
		const float* pan[NCH] = {L::PAN1, L::PAN2, L::PAN3, L::PAN4};
		const float* level[NCH] = {L::LEVEL1, L::LEVEL2, L::LEVEL3, L::LEVEL4};
		const float* vu[NCH] = {L::VU1, L::VU2, L::VU3, L::VU4};
		const float* mute[NCH] = {L::MUTE1, L::MUTE2, L::MUTE3, L::MUTE4};
		const float* solo[NCH] = {L::SOLO1, L::SOLO2, L::SOLO3, L::SOLO4};
		const float* inl[NCH] = {L::IN_L1, L::IN_L2, L::IN_L3, L::IN_L4};
		const float* inr[NCH] = {L::IN_R1, L::IN_R2, L::IN_R3, L::IN_R4};
		const float* lvl[NCH] = {L::IN_LVL1, L::IN_LVL2, L::IN_LVL3, L::IN_LVL4};
		const float* pcv[NCH] = {L::IN_PAN1, L::IN_PAN2, L::IN_PAN3, L::IN_PAN4};
		for (int c = 0; c < NCH; c++) {
			small(high[c], M::pid(c, M::HIGH));
			small(low[c], M::pid(c, M::LOW));
			small(send[c], M::pid(c, M::SEND));
			small(sendb[c], M::SENDB_PARAM + c);
			small(duck[c], M::pid(c, M::DUCK));
			small(pan[c], M::pid(c, M::PAN));
			large(level[c], M::pid(c, M::LEVEL));
			meter(vu[c], c, TRACK[c]);
			addParam(createLightParamCentered<Lit<0xff, 0x4f, 0x4f>>(ui::mm(mute[c]), module, M::pid(c, M::MUTE),
			                                                          M::MUTE_LIGHT + c));
			Vec sp = ui::mm(solo[c]);
			switch (c) {
				case 0: addParam(createLightParamCentered<Lit<0x7b, 0x6c, 0xff>>(sp, module, M::pid(c, M::SOLO), M::SOLO_LIGHT + c)); break;
				case 1: addParam(createLightParamCentered<Lit<0xb8, 0xf2, 0x3a>>(sp, module, M::pid(c, M::SOLO), M::SOLO_LIGHT + c)); break;
				case 2: addParam(createLightParamCentered<Lit<0x3d, 0xc9, 0xff>>(sp, module, M::pid(c, M::SOLO), M::SOLO_LIGHT + c)); break;
				default: addParam(createLightParamCentered<Lit<0xe0, 0x77, 0x9e>>(sp, module, M::pid(c, M::SOLO), M::SOLO_LIGHT + c)); break;
			}
			addInput(createInputCentered<PJ301MPort>(ui::mm(inl[c]), module, M::iid(c, M::IN_L)));
			addInput(createInputCentered<PJ301MPort>(ui::mm(inr[c]), module, M::iid(c, M::IN_R)));
			addInput(createInputCentered<PJ301MPort>(ui::mm(lvl[c]), module, M::iid(c, M::IN_LVL)));
			addInput(createInputCentered<PJ301MPort>(ui::mm(pcv[c]), module, M::iid(c, M::IN_PAN)));
		}

		small(L::DUCK, M::M_DUCK_PARAM);
		small(L::TIME, M::M_TIME_PARAM);
		small(L::WIDTH, M::M_WIDTH_PARAM);
		small(L::RETURN, M::M_RETURN_PARAM);
		small(L::DRIVE, M::M_DRIVE_PARAM);
		small(L::SENDM, M::M_SEND_PARAM);
		small(L::SENDMB, M::M_SENDB_PARAM);
		small(L::RETURNB, M::M_RETURNB_PARAM);
		large(L::MASTER, M::M_MASTER_PARAM);
		meter(L::VUM, -1, METER_GREEN);
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_DUCK), module, M::DUCK_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::RET_L), module, M::RET_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::RET_R), module, M::RET_R_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::RETB_L), module, M::RETB_L_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::RETB_R), module, M::RETB_R_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::SEND_L), module, M::SEND_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::SEND_R), module, M::SEND_R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::SENDB_L), module, M::SENDB_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::SENDB_R), module, M::SENDB_R_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_L), module, M::OUT_L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_R), module, M::OUT_R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("4-track stereo mixer: polyphonic inputs are summed"));
		menu->addChild(createMenuLabel("GENRA's KICK into DUCK makes the other tracks pump"));
	}
};

}  // namespace genra_mix

Model* modelMix = createModel<genra_mix::Mix, genra_mix::MixWidget>("GenraMix");
