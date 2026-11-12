// SPDX-License-Identifier: GPL-3.0-only
// MIX – 4-track stereo mixer in the GENRA look: one strip per GENRA track (DRUMS, BASS, MELODY, PADS) in its colour,
// a dark master with two send/return loops (A, B), ducking (GENRA's KICK into DUCK), width and drive.
// Engine: MixDsp.hpp. Panel: tools/panels/mix_panel.py (res/Mix.svg + MixLayout.hpp).
#include "../plugin.hpp"
#include "../ui/GenraUI.hpp"
#include "../Follow.hpp"
#include <map>
#include "MixDsp.hpp"
#include "MixLayout.hpp"
#include "../genrafx/GenraFx.hpp"

namespace genra_mix {

namespace L = mxlayout;
namespace ui = genra_ui;
using namespace gmix;

// the GENRA track colours (keep in step with tools/panels/mix_panel.py)
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

	// ---- inputs without cables (right-click › CH n input from / DUCK from; see Follow.hpp)
	// a channel's source: 0 off, 1 auto (the instrument that follows the channel's GENRA track), 2 a chosen module
	struct ChSrc {
		std::atomic<int> mode{0};
		std::atomic<int64_t> id{-1};
		std::atomic<int> outL{-1}, outR{-1};
		std::atomic<follow::Host*> host{nullptr};
		follow::Ref ref;
		follow::Drive dl, dr;
		float timer = 0.f;
	} src[NCH];
	// RETURN A / B without cables: 0 cables only, 1 auto (GENRA FX's OUT A / OUT B)
	std::atomic<int> retMode[2] = {{1}, {1}};
	follow::Ref fxRef;
	follow::Drive dret[2][2]; // [A/B][L/R]
	float fxTimer = 0.f;
	std::atomic<int> duckMode{0}; // 0 off, 1 GENRA's KICK
	follow::Ref duckRef;
	follow::Drive dd;
	float duckTimer = 0.f;
	static int chTrack(int c) {
		static const int T[NCH] = {follow::T_DRUMS, follow::T_BASS, follow::T_MEL, follow::T_PAD};
		return T[c];
	}

	void feedSources(int64_t frame) {
		for (int c = 0; c < NCH; c++) {
			ChSrc& s = src[c];
			Module* m = s.ref.to.load();
			int ol = s.outL.load(), orr = s.outR.load();
			Input& il = inputs[iid(c, IN_L)];
			Input& ir = inputs[iid(c, IN_R)];
			follow::Host* h = s.host.load();
			// (bounds: for one sample at a change of source, the module and the ids may not belong together)
			if (!m || !h || ol < 0 || ol >= (int) m->outputs.size() || orr >= (int) m->outputs.size()) {
				if (s.dl.ch >= 0)
					s.dl.release(il);
				if (s.dr.ch >= 0)
					s.dr.release(ir);
				continue;
			}
			// it plays in stereo, as with two cables (written now and then: the source reads it on another thread)
			if (frame - h->pulledAt.load(std::memory_order_relaxed) > 1024) {
				h->pulledAt.store(frame, std::memory_order_relaxed);
				h->pulledBy.store(c, std::memory_order_relaxed);
			}
			Output& oL = m->outputs[ol];
			s.dl.feed(il, oL.voltages, std::max(1, (int) oL.channels));
			if (orr >= 0) {
				Output& oR = m->outputs[orr];
				s.dr.feed(ir, oR.voltages, std::max(1, (int) oR.channels));
			}
		}
		{ // the returns from GENRA FX
			Module* fx = fxRef.to.load();
			static const int RIN[2][2] = {{RET_L_INPUT, RET_R_INPUT}, {RETB_L_INPUT, RETB_R_INPUT}};
			for (int k = 0; k < 2; k++)
				for (int lr = 0; lr < 2; lr++) {
					Input& in = inputs[RIN[k][lr]];
					int oid = k * 2 + lr; // genrafx: OUT_AL, OUT_AR, OUT_BL, OUT_BR
					if (fx && retMode[k].load() && oid < (int) fx->outputs.size()) {
						if (dret[k][lr].feed(in, fx->outputs[oid].voltages, 1) && lr == 0)
							static_cast<genrafx::GenraFx*>(fx)->returnPulledAt[k].store(frame, std::memory_order_relaxed);
					}
					else if (dret[k][lr].ch >= 0)
						dret[k][lr].release(in);
				}
		}
		Module* g = duckMode.load() ? duckRef.to.load() : nullptr;
		if (g) {
			follow::Src k = follow::genraOut(follow::T_DRUMS, follow::DRUMS); // output 0 of the 8: the kick
			if (const float* f = follow::genraFrame(duckRef, k.id, frame))
				dd.feed(inputs[DUCK_INPUT], f, 1);
			else if (dd.ch >= 0)
				dd.release(inputs[DUCK_INPUT]);
		}
		else if (dd.ch >= 0)
			dd.release(inputs[DUCK_INPUT]);
	}

	// UI thread: find the sources
	void uiSources(float dt) {
		for (int c = 0; c < NCH; c++) {
			ChSrc& s = src[c];
			s.timer -= dt;
			int mode = s.mode.load();
			Module* now = s.ref.to.load();
			if (mode == 0) {
				if (now)
					follow::point(s.ref, this, nullptr);
				continue;
			}
			if (s.timer > 0.f) // (2.37: also while nothing is found: no search through every module on every frame)
				continue;
			s.timer = 0.5f;
			Module* want = nullptr;
			if (mode == 2) {
				Module* m = APP->engine->getModule(s.id.load());
				follow::Host* h = follow::host(m);
				if (h && h->audioL >= 0)
					want = m;
			}
			if (!want) { // auto, or the chosen module is not there (e.g. a pasted copy): who follows the channel's track
				for (int64_t id : APP->engine->getModuleIds()) {
					Module* m = APP->engine->getModule(id);
					follow::Host* h = follow::host(m);
					if (h && h->follows && h->audioL >= 0 && h->fol.track.load() == chTrack(c)) {
						want = m;
						break;
					}
				}
			}
			if (want != now) {
				follow::point(s.ref, this, nullptr);
				follow::Host* h = follow::host(want);
				s.host = h;
				s.outL = h ? h->audioL : -1;
				s.outR = h ? h->audioR : -1;
				follow::point(s.ref, this, want);
			}
		}
		fxTimer -= dt;
		Module* fx = fxRef.to.load();
		if (!retMode[0].load() && !retMode[1].load()) {
			if (fx)
				follow::point(fxRef, this, nullptr);
		}
		else if (!fx && fxTimer <= 0.f) {
			fxTimer = 0.5f;
			for (int64_t id : APP->engine->getModuleIds()) {
				Module* m = APP->engine->getModule(id);
				if (genrafx::isFx(m)) {
					follow::point(fxRef, this, m);
					break;
				}
			}
		}
		duckTimer -= dt;
		Module* g = duckRef.to.load();
		if (!duckMode.load()) {
			if (g)
				follow::point(duckRef, this, nullptr);
		}
		else if (!g && duckTimer <= 0.f) {
			duckTimer = 0.5f;
			for (int64_t id : APP->engine->getModuleIds()) {
				Module* m = APP->engine->getModule(id);
				if (follow::isGenra(m)) {
					follow::point(duckRef, this, m);
					break;
				}
			}
		}
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_t* a = json_array();
		for (int c = 0; c < NCH; c++) {
			json_t* o = json_object();
			json_object_set_new(o, "mode", json_integer(src[c].mode.load()));
			json_object_set_new(o, "id", json_integer(src[c].id.load()));
			json_array_append_new(a, o);
		}
		json_object_set_new(r, "inputFrom", a);
		json_object_set_new(r, "duckFrom", json_integer(duckMode.load()));
		json_t* rf = json_array();
		for (int k = 0; k < 2; k++)
			json_array_append_new(rf, json_integer(retMode[k].load()));
		json_object_set_new(r, "returnFrom", rf);
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* a = json_object_get(r, "inputFrom"))
			for (int c = 0; c < NCH && c < (int) json_array_size(a); c++) {
				json_t* o = json_array_get(a, c);
				if (json_t* j = json_object_get(o, "mode"))
					src[c].mode = clamp((int) json_integer_value(j), 0, 2);
				if (json_t* j = json_object_get(o, "id"))
					src[c].id = (int64_t) json_integer_value(j);
				src[c].timer = 0.f;
			}
		if (json_t* j = json_object_get(r, "duckFrom"))
			duckMode = clamp((int) json_integer_value(j), 0, 1);
		if (json_t* a = json_object_get(r, "returnFrom"))
			for (int k = 0; k < 2 && k < (int) json_array_size(a); k++)
				retMode[k] = clamp((int) json_integer_value(json_array_get(a, k)), 0, 1);
		fxTimer = 0.f;
	}
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }
	// a real cable wins (exact, from the engine; see Follow.hpp)
	void onPortChange(const PortChangeEvent& e) override {
		if (e.type != Port::INPUT)
			return;
		for (int c = 0; c < NCH; c++) {
			if (e.portId == iid(c, IN_L))
				src[c].dl.cable = e.connecting;
			if (e.portId == iid(c, IN_R))
				src[c].dr.cable = e.connecting;
		}
		if (e.portId == DUCK_INPUT)
			dd.cable = e.connecting;
		if (e.portId == RET_L_INPUT)
			dret[0][0].cable = e.connecting;
		if (e.portId == RET_R_INPUT)
			dret[0][1].cable = e.connecting;
		if (e.portId == RETB_L_INPUT)
			dret[1][0].cable = e.connecting;
		if (e.portId == RETB_R_INPUT)
			dret[1][1].cable = e.connecting;
	}

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
		feedSources(args.frame);
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

// ---------------------------------------------------------------- where a channel's sound comes from
/** Over "CH n": the source's name in the channel colour (dim with "CABLE" when a cable wins); a ring at DUCK. */
struct SourceTags : widget::TransparentWidget {
	Mix* module = nullptr;
	Vec duck;
	Vec ret[2][2], snd[2][2];
	Vec inL[NCH], inR[NCH];
	bool found = false;
	void step() override {
		Widget::step();
		if (found)
			return;
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		if (!mw)
			return;
		for (int c = 0; c < NCH; c++) {
			PortWidget* l = mw->getInput(Mix::iid(c, Mix::IN_L));
			PortWidget* r = mw->getInput(Mix::iid(c, Mix::IN_R));
			if (!l || !r)
				return;
			inL[c] = l->box.getCenter();
			inR[c] = r->box.getCenter();
		}
		static const int RIN[2][2] = {{Mix::RET_L_INPUT, Mix::RET_R_INPUT}, {Mix::RETB_L_INPUT, Mix::RETB_R_INPUT}};
		static const int SOUT[2][2] = {{Mix::SEND_L_OUTPUT, Mix::SEND_R_OUTPUT}, {Mix::SENDB_L_OUTPUT, Mix::SENDB_R_OUTPUT}};
		for (int k = 0; k < 2; k++)
			for (int lr = 0; lr < 2; lr++) {
				PortWidget* p = mw->getInput(RIN[k][lr]);
				PortWidget* o = mw->getOutput(SOUT[k][lr]);
				if (!p || !o)
					return;
				ret[k][lr] = p->box.getCenter();
				snd[k][lr] = o->box.getCenter();
			}
		found = true;
	}
	static Rect field(int c) {
		float cx = mm2px(3.f + c * 24.f + 11.5f), cy = mm2px(19.7f); // (tools/panels/mix_panel.py: strips at 3 + i·24, 23 wide)
		float w = mm2px(20.f), hh = mm2px(3.6f);
		return Rect(Vec(cx - w / 2, cy - hh / 2), Vec(w, hh));
	}
	// the dark field over "CH n" belongs to the panel (a light cannot darken anything)
	void draw(const DrawArgs& args) override {
		Widget::draw(args);
		if (!module)
			return;
		for (int c = 0; c < NCH; c++) {
			if (!follow::host(module->src[c].ref.to.load()))
				continue;
			Rect f = field(c);
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, f.pos.x, f.pos.y, f.size.x, f.size.y, f.size.y / 3);
			nvgFillColor(args.vg, nvgRGB(0x07, 0x08, 0x0a));
			nvgFill(args.vg);
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		Widget::drawLayer(args, layer);
		if (layer != 1 || !module)
			return;
		NVGcontext* vg = args.vg;
		auto font = ui::boldFont();
		for (int c = 0; c < NCH; c++) {
			Mix::ChSrc& s = module->src[c];
			follow::Host* h = follow::host(s.ref.to.load());
			if (!h)
				continue;
			bool cable = !s.dl.owned;
			float cx = mm2px(3.f + c * 24.f + 11.5f), cy = mm2px(19.7f); // (tools/panels/mix_panel.py: strips at 3 + i·24, 23 wide)
			float w = mm2px(20.f), hh = mm2px(3.6f);
			NVGcolor col = cable ? nvgLerpRGBA(TRACK[c], nvgRGB(0x20, 0x21, 0x26), 0.6f) : TRACK[c];
			if (!cable) {
				NVGpaint glow = nvgBoxGradient(vg, cx - w / 2, cy - hh / 2, w, hh, hh / 3, mm2px(1.4f),
				                               nvgTransRGBAf(TRACK[c], 0.35f), nvgTransRGBAf(TRACK[c], 0.f));
				nvgBeginPath(vg);
				nvgRect(vg, cx - w / 2 - 10, cy - hh / 2 - 10, w + 20, hh + 20);
				nvgFillPaint(vg, glow);
				nvgFill(vg);
			}
			nvgBeginPath(vg);
			nvgRoundedRect(vg, cx - w / 2, cy - hh / 2, w, hh, hh / 3);
			nvgStrokeColor(vg, col);
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
			// over IN L / IN R: the lit segment, as on the instruments' strip (not where a cable is in)
			if (found) {
				if (s.dl.owned)
					follow::segment(vg, inL[c], TRACK[c]);
				if (s.dr.owned)
					follow::segment(vg, inR[c], TRACK[c]);
			}
			std::string t = std::string("\xe2\x80\xb9 ") + h->srcName + (cable ? " \xc2\xb7 CABLE" : "");
			ui::text(vg, font, cx, cy + hh * 0.22f, t, cable ? 6.5f : 7.5f, col, NVG_ALIGN_CENTER, 0.4f);
		}
		// the send effects (GENRA FX): its ice blue over RETURN A/B it feeds and over SEND A/B it takes
		if (found) {
			genrafx::GenraFx* fx = static_cast<genrafx::GenraFx*>(module->fxRef.to.load());
			NVGcolor ice = nvgRGB(0x8f, 0xd3, 0xff);
			for (int k = 0; k < 2; k++) {
				bool any = false;
				for (int lr = 0; lr < 2; lr++)
					if (module->dret[k][lr].owned) {
						follow::segment(vg, ret[k][lr], ice);
						any = true;
					}
				if (any)
					ui::text(vg, font, 0.5f * (ret[k][0].x + ret[k][1].x), ret[k][0].y - mm2px(6.2f), "\xe2\x80\xb9 GENRA FX", 6.5f,
					         ice, NVG_ALIGN_CENTER, 0.4f);
				if (fx)
					for (int lr = 0; lr < 2; lr++)
						if (fx->drv[k * 2 + lr].owned)
							follow::segment(vg, snd[k][lr], ice);
			}
		}
		if (module->duckMode.load() && module->duckRef.to.load() && module->dd.owned) {
			NVGcolor k = TRACK[0];
			follow::segment(vg, duck, k); // (as over the channel inputs)
			ui::text(vg, font, duck.x + mm2px(5.4f), duck.y + mm2px(0.7f), "\xe2\x80\xb9 KICK", 7.f, k, NVG_ALIGN_LEFT, 0.4f);
		}
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
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto large = [&](const float* p, int id) { addParam(createParamCentered<GLargeKnob>(ui::mm(p), module, id)); };
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
		SourceTags* tags = new SourceTags;
		tags->module = module;
		tags->box.size = box.size;
		tags->duck = ui::mm(L::IN_DUCK);
		addChild(tags);
	}

	void step() override {
		if (Mix* m = dynamic_cast<Mix*>(module))
			m->uiSources(APP->window->getLastFrameDuration());
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Mix* m = dynamic_cast<Mix*>(module);
		if (m) {
			menu->addChild(new MenuSeparator);
			for (int c = 0; c < NCH; c++) {
				int mode = m->src[c].mode.load();
				Module* now = m->src[c].ref.to.load();
				follow::Host* hn = follow::host(now);
				std::string right = mode == 0 ? "Off" : (hn ? hn->srcName : (mode == 1 ? "Auto" : "–"));
				menu->addChild(createSubmenuItem(string::f("CH %d input from", c + 1), right, [=](Menu* sub) {
					sub->addChild(createCheckMenuItem("Off (cables only)", "", [=]() { return m->src[c].mode.load() == 0; },
					                                  [=]() { m->src[c].mode = 0; }));
					sub->addChild(createCheckMenuItem(string::f("Auto: who follows GENRA %s", follow::TRACK_NAMES[Mix::chTrack(c)]),
					                                  "", [=]() { return m->src[c].mode.load() == 1; }, [=]() {
						                                  m->src[c].mode = 1;
						                                  m->src[c].timer = 0.f;
						                                  follow::point(m->src[c].ref, m, nullptr);
					                                  }));
					sub->addChild(new MenuSeparator);
					// every instrument (and STOTTER) in the patch, numbered when there are several of a kind
					std::map<std::string, int> count, seen;
					std::vector<std::pair<int64_t, std::string>> list;
					for (int64_t id : APP->engine->getModuleIds()) {
						follow::Host* h = follow::host(APP->engine->getModule(id));
						if (h && h->audioL >= 0) {
							list.push_back({id, h->srcName});
							count[h->srcName]++;
						}
					}
					for (auto& it : list) {
						int64_t id = it.first;
						std::string name = it.second;
						if (count[name] > 1)
							name += string::f(" #%d", ++seen[name]);
						sub->addChild(createCheckMenuItem(name, "", [=]() { return m->src[c].mode.load() == 2 && m->src[c].id.load() == id; },
						                                  [=]() {
							                                  m->src[c].id = id;
							                                  m->src[c].mode = 2;
							                                  m->src[c].timer = 0.f;
							                                  follow::point(m->src[c].ref, m, nullptr);
						                                  }));
					}
					if (list.empty())
						sub->addChild(createMenuLabel("(no GENRA instrument in the patch)"));
					sub->addChild(new MenuSeparator);
					sub->addChild(createMenuLabel("A cable in IN L / IN R always wins"));
				}));
			}
			for (int k = 0; k < 2; k++) {
				const char* rn = k ? "RETURN B from" : "RETURN A from";
				menu->addChild(createSubmenuItem(rn, m->retMode[k].load() ? "GENRA FX" : "Cables only", [=](Menu* sub) {
					sub->addChild(createCheckMenuItem(k ? "Auto: GENRA FX's OUT B (delay)" : "Auto: GENRA FX's OUT A (reverb)", "",
					                                  [=]() { return m->retMode[k].load() == 1; },
					                                  [=]() { m->retMode[k] = 1, m->fxTimer = 0.f; }));
					sub->addChild(createCheckMenuItem("Off (cables only)", "", [=]() { return m->retMode[k].load() == 0; },
					                                  [=]() { m->retMode[k] = 0; }));
					sub->addChild(new MenuSeparator);
					sub->addChild(createMenuLabel("A cable in RET L / RET R always wins"));
				}));
			}
			menu->addChild(createSubmenuItem("DUCK from", m->duckMode.load() ? "GENRA KICK" : "Off", [=](Menu* sub) {
				sub->addChild(createCheckMenuItem("Off (cable only)", "", [=]() { return m->duckMode.load() == 0; },
				                                  [=]() { m->duckMode = 0; }));
				sub->addChild(createCheckMenuItem("GENRA KICK", "", [=]() { return m->duckMode.load() == 1; },
				                                  [=]() { m->duckMode = 1; }));
			}));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("4-track stereo mixer: polyphonic inputs are summed"));
		menu->addChild(createMenuLabel("GENRA's KICK into DUCK makes the other tracks pump"));
	}
};

}  // namespace genra_mix

Model* modelMix = createModel<genra_mix::Mix, genra_mix::MixWidget>("GenraMix");
