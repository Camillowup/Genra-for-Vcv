// SPDX-License-Identifier: GPL-3.0-only
// GENRA FX – see GenraFx.hpp.
#include "GenraFx.hpp"
#include "../ui/GenraUI.hpp"
#include "GenraFxLayout.hpp"
#include <functional>

namespace genrafx {

namespace L = gfxl;
namespace ui = genra_ui;

#define FX_RGB 0x8f, 0xd3, 0xff
static const NVGcolor ACCENT = nvgRGB(FX_RGB);
static const char* const MODE_NAMES[NUM_TIME_MODES] = {"", "DOTTED", "TRIPLET", "FREE"};

bool isFx(Module* m) { return m && m->model == modelGenraFx; }
static bool isMix(Module* m) { return m && m->model == modelMix; }

struct DecayQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		float s = 0.1f * std::pow(300.f, getValue());
		return s < 10.f ? string::f("%.2f", s) : string::f("%.1f", s);
	}
	std::string getUnit() override { return " s"; }
};
struct TimeQuantity : ParamQuantity {
	std::string getDisplayValueString() override {
		GenraFx* m = dynamic_cast<GenraFx*>(module);
		if (!m)
			return ParamQuantity::getDisplayValueString();
		if (m->timeMode() == T_FREE)
			return string::f("%.0f ms", GenraFx::freeMs(getValue()));
		std::string s = gfx::DIV_NAMES[m->divIndex(getValue())];
		if (m->timeMode() != T_STRAIGHT)
			s += std::string(" ") + MODE_NAMES[m->timeMode()];
		return s;
	}
};

GenraFx::GenraFx() {
	config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	configSwitch(V_TYPE_PARAM, 0.f, gfx::NUM_REVERB_TYPES - 1, 0.f, "Reverb type", {"Hall", "Room", "Plate", "Space"});
	configParam<DecayQuantity>(V_DECAY_PARAM, 0.f, 1.f, 0.564f, "Reverb decay (RT60)");
	configParam(V_SIZE_PARAM, 0.f, 1.f, 0.5f, "Reverb size", "%", 0.f, 100.f);
	configParam(V_PRE_PARAM, 0.f, 250.f, 10.f, "Reverb pre-delay", " ms");
	configParam(V_DAMP_PARAM, 0.f, 1.f, 0.4f, "Reverb damping (bright .. dark)", "%", 0.f, 100.f);
	configParam(V_WIDTH_PARAM, 0.f, 1.5f, 1.f, "Reverb width", "%", 0.f, 100.f);
	configParam(V_SHIM_PARAM, 0.f, 1.f, 0.f, "Shimmer (an octave up in the tail)", "%", 0.f, 100.f);
	configParam(V_LVL_PARAM, 0.f, 2.f, 1.f, "Reverb level", "%", 0.f, 100.f);
	configSwitch(V_FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze (holds the tail, the input closed)", {"Off", "On"});
	configSwitch(D_MODE_PARAM, 0.f, NUM_TIME_MODES - 1, 0.f, "Delay time", {"Straight", "Dotted", "Triplet", "Free (ms)"});
	configParam<TimeQuantity>(D_TIME_PARAM, 0.f, 1.f, 0.6f, "Delay time");
	configParam(D_FB_PARAM, 0.f, 1.1f, 0.4f, "Delay feedback", "%", 0.f, 100.f);
	configParam(D_TONE_PARAM, -1.f, 1.f, 0.f, "Delay tone (dark .. thin)", "%", 0.f, 100.f);
	configParam(D_TAPE_PARAM, 0.f, 1.f, 0.f, "Tape (wow, flutter, saturation)", "%", 0.f, 100.f);
	configParam(D_DUCK_PARAM, 0.f, 1.f, 0.f, "Duck (the echoes step back while the input plays)", "%", 0.f, 100.f);
	configParam(D_WIDTH_PARAM, 0.f, 1.5f, 1.f, "Delay width", "%", 0.f, 100.f);
	configParam(D_LVL_PARAM, 0.f, 2.f, 1.f, "Delay level", "%", 0.f, 100.f);
	configSwitch(D_PING_PARAM, 0.f, 1.f, 0.f, "Delay mode", {"Stereo", "Ping-pong"});
	configInput(IN_AL, "Reverb in left (or mono) – without a cable: MIX's SEND A");
	configInput(IN_AR, "Reverb in right (unplugged: the left)");
	configInput(CV_DECAY, "Reverb decay CV (±5 V)");
	configInput(CV_FREEZE, "Freeze gate");
	configInput(IN_BL, "Delay in left (or mono) – without a cable: MIX's SEND B");
	configInput(IN_BR, "Delay in right (unplugged: the left)");
	configInput(CV_TIME, "Delay time CV (±5 V)");
	configInput(CV_FB, "Delay feedback CV (±5 V)");
	configOutput(OUT_AL, "Reverb left");
	configOutput(OUT_AR, "Reverb right");
	configOutput(OUT_BL, "Delay left");
	configOutput(OUT_BR, "Delay right");
	rv.setSampleRate(sr);
	dl.setSampleRate(sr);
}

void GenraFx::onAdd(const AddEvent& e) {
	sr = APP->engine->getSampleRate();
	rv.setSampleRate(sr);
	dl.setSampleRate(sr);
}
void GenraFx::onSampleRateChange(const SampleRateChangeEvent& e) {
	sr = e.sampleRate;
	rv.setSampleRate(sr);
	dl.setSampleRate(sr);
}
void GenraFx::onReset(const ResetEvent& e) {
	Module::onReset(e);
	rv.reset();
	dl.reset();
	followGenra = true;
	inMode[0] = 1;
	inMode[1] = 1;
	freeBpm = 120.f;
}
void GenraFx::onPortChange(const PortChangeEvent& e) {
	if (e.type != Port::INPUT)
		return;
	static const int IDS[4] = {IN_AL, IN_AR, IN_BL, IN_BR};
	for (int k = 0; k < 4; k++)
		if (e.portId == IDS[k])
			drv[k].cable = e.connecting;
}

float GenraFx::timeKnob() {
	return clamp(params[D_TIME_PARAM].getValue() + inputs[CV_TIME].getVoltage() * 0.1f, 0.f, 1.f);
}

void GenraFx::applyParams() {
	Module* g = followGenra.load() ? genraRef.to.load() : nullptr;
	float bpm = g ? follow::genraBpm(g, sr) : freeBpm;
	synced.store(g != nullptr, std::memory_order_relaxed);
	bpmNow.store(bpm, std::memory_order_relaxed);

	gfx::ReverbParams r;
	r.type = clamp((int) std::round(params[V_TYPE_PARAM].getValue()), 0, gfx::NUM_REVERB_TYPES - 1);
	float dv = clamp(params[V_DECAY_PARAM].getValue() + inputs[CV_DECAY].getVoltage() * 0.1f, 0.f, 1.f);
	r.decay = 0.1f * std::pow(300.f, dv);
	r.size = params[V_SIZE_PARAM].getValue();
	r.predelayMs = params[V_PRE_PARAM].getValue();
	r.damp = params[V_DAMP_PARAM].getValue();
	r.width = params[V_WIDTH_PARAM].getValue();
	r.shimmer = params[V_SHIM_PARAM].getValue();
	r.level = params[V_LVL_PARAM].getValue();
	r.freeze = params[V_FREEZE_PARAM].getValue() > 0.5f || inputs[CV_FREEZE].getVoltage() > 1.f;
	lights[FREEZE_LIGHT].setBrightness(r.freeze ? 1.f : 0.f);
	rv.set(r);

	gfx::DelayParams d;
	int tm = timeMode();
	float tk = timeKnob();
	d.sync = tm != T_FREE;
	d.div = divIndex(tk);
	d.mode = tm == T_DOTTED ? gfx::DOTTED : (tm == T_TRIPLET ? gfx::TRIPLET : gfx::STRAIGHT);
	d.bpm = bpm;
	d.ms = freeMs(tk);
	d.feedback = clamp(params[D_FB_PARAM].getValue() + inputs[CV_FB].getVoltage() * 0.11f, 0.f, 1.1f);
	d.tone = params[D_TONE_PARAM].getValue();
	d.tape = params[D_TAPE_PARAM].getValue();
	d.duck = params[D_DUCK_PARAM].getValue();
	d.width = params[D_WIDTH_PARAM].getValue();
	d.pingpong = params[D_PING_PARAM].getValue() > 0.5f;
	d.level = params[D_LVL_PARAM].getValue();
	dl.set(d);
}

void GenraFx::feedInputs() {
	static const int IDS[4] = {IN_AL, IN_AR, IN_BL, IN_BR};
	Module* m = mixRef.to.load();
	for (int k = 0; k < 4; k++) {
		int side = k / 2, lr = k % 2;
		Input& in = inputs[IDS[k]];
		int oid = MIX_SEND_OUT[side][lr];
		if (m && inMode[side].load() && oid < (int) m->outputs.size()) {
			Output& o = m->outputs[oid];
			drv[k].feed(in, o.voltages, 1);
		}
		else if (drv[k].ch >= 0)
			drv[k].release(in);
	}
}

void GenraFx::process(const ProcessArgs& args) {
	frameNow = args.frame;
	feedInputs();
	if (--ctl <= 0) {
		ctl = 32;
		applyParams();
	}
	float al = inputs[IN_AL].getVoltageSum();
	float ar = inputs[IN_AR].isConnected() ? inputs[IN_AR].getVoltageSum() : al;
	float bl = inputs[IN_BL].getVoltageSum();
	float br = inputs[IN_BR].isConnected() ? inputs[IN_BR].getVoltageSum() : bl;
	float oal, oar, obl, obr;
	rv.process(al, ar, oal, oar);
	dl.process(bl, br, obl, obr);
	outputs[OUT_AL].setVoltage(oal);
	outputs[OUT_AR].setVoltage(oar);
	outputs[OUT_BL].setVoltage(obl);
	outputs[OUT_BR].setVoltage(obr);
	// meters: a peak follower, 300 ms release
	float a = 0.5f * (std::fabs(oal) + std::fabs(oar)), b = 0.5f * (std::fabs(obl) + std::fabs(obr));
	float rel = 1.f - std::exp(-1.f / (0.3f * args.sampleRate));
	env[0] = a > env[0] ? a : env[0] + (a - env[0]) * rel;
	env[1] = b > env[1] ? b : env[1] + (b - env[1]) * rel;
	if ((args.frame & 255) == 0) {
		meter[0].store(env[0], std::memory_order_relaxed);
		meter[1].store(env[1], std::memory_order_relaxed);
	}
}

void GenraFx::uiStep(float dt) {
	uiTimer -= dt;
	if (uiTimer > 0.f)
		return;
	uiTimer = 0.5f;
	// GENRA (the tempo)
	Module* g = genraRef.to.load();
	if (!followGenra.load()) {
		if (g)
			follow::point(genraRef, this, nullptr);
	}
	else if (!g) {
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* m = APP->engine->getModule(id);
			if (follow::isGenra(m)) {
				follow::point(genraRef, this, m);
				break;
			}
		}
	}
	// MIX (the sends)
	Module* x = mixRef.to.load();
	bool want = inMode[0].load() || inMode[1].load();
	if (!want) {
		if (x)
			follow::point(mixRef, this, nullptr);
	}
	else if (!x) {
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* m = APP->engine->getModule(id);
			if (isMix(m)) {
				follow::point(mixRef, this, m);
				break;
			}
		}
	}
}

json_t* GenraFx::dataToJson() {
	json_t* j = json_object();
	json_object_set_new(j, "followGenra", json_boolean(followGenra.load()));
	json_object_set_new(j, "inputA", json_integer(inMode[0].load()));
	json_object_set_new(j, "inputB", json_integer(inMode[1].load()));
	json_object_set_new(j, "freeBpm", json_real(freeBpm));
	return j;
}
void GenraFx::dataFromJson(json_t* j) {
	if (json_t* v = json_object_get(j, "followGenra"))
		followGenra = json_boolean_value(v);
	if (json_t* v = json_object_get(j, "inputA"))
		inMode[0] = clamp((int) json_integer_value(v), 0, 1);
	if (json_t* v = json_object_get(j, "inputB"))
		inMode[1] = clamp((int) json_integer_value(v), 0, 1);
	if (json_t* v = json_object_get(j, "freeBpm"))
		freeBpm = clamp((float) json_number_value(v), 20.f, 400.f);
	uiTimer = 0.f;
}

// ---------------------------------------------------------------- panel parts
/** A field for a switch parameter: click = list, wheel = step; the text comes from text(). */
struct ChoiceField : app::ParamWidget {
	std::string title;
	std::vector<std::string> names;
	std::function<std::string()> left, right;
	std::function<void(Menu*)> extraMenu;
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? (int) std::round(pq->getValue()) : 0;
	}
	void set(int t) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq || (int) std::round(pq->getValue()) == t)
			return;
		float old = pq->getValue();
		pq->setValue(t);
		history::ParamChange* h = new history::ParamChange;
		h->name = "change " + title;
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
			if (extraMenu)
				extraMenu(menu);
			else
				for (int i = 0; i < (int) names.size(); i++)
					menu->addChild(createCheckMenuItem(names[i], "", [=]() { return value() == i; }, [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f && !names.empty()) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, (int) names.size() - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			auto f = ui::boldFont();
			float y = box.size.y / 2 + 2.6f;
			if (left)
				ui::text(args.vg, f, mm2px(2.f), y, left(), 7.2f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			if (right)
				ui::text(args.vg, f, box.size.x - mm2px(6.f), y, right(), 6.2f, ui::DIM, NVG_ALIGN_RIGHT, 0.6f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** The display: tempo and its source, where A and B come from, two level meters; the SYNC bar; the lit segments. */
struct FxOverlay : widget::TransparentWidget {
	GenraFx* module = nullptr;
	Vec jIn[4], jOut[4];
	bool found = false;
	void step() override {
		Widget::step();
		if (module)
			module->uiStep(APP->window->getLastFrameDuration());
		if (found)
			return;
		ModuleWidget* mw = getAncestorOfType<ModuleWidget>();
		if (!mw)
			return;
		static const int IN[4] = {GenraFx::IN_AL, GenraFx::IN_AR, GenraFx::IN_BL, GenraFx::IN_BR};
		for (int k = 0; k < 4; k++) {
			PortWidget* i = mw->getInput(IN[k]);
			PortWidget* o = mw->getOutput(k);
			if (!i || !o)
				return;
			jIn[k] = i->box.getCenter();
			jOut[k] = o->box.getCenter();
		}
		found = true;
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		Widget::drawLayer(args, layer);
		if (layer != 1 || !module)
			return;
		NVGcontext* vg = args.vg;
		auto f = ui::boldFont();
		bool sync = module->synced.load();
		// SYNC bar
		{
			Rect b(mm2px(Vec(L::SYNC[0], L::SYNC[1])), mm2px(Vec(L::SYNC[2], L::SYNC[3])));
			if (sync) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, b.pos.x, b.pos.y, b.size.x, b.size.y, b.size.y / 2);
				nvgFillColor(vg, ACCENT);
				nvgFill(vg);
			}
		}
		// display
		float dx = mm2px(L::DISP[0]), dy = mm2px(L::DISP[1]), dw = mm2px(L::DISP[2]);
		std::string t = string::f("%.0f BPM", module->bpmNow.load());
		t += sync ? "  \xc2\xb7  FROM GENRA" : "  \xc2\xb7  FREE";
		ui::text(vg, f, dx + mm2px(3.f), dy + mm2px(5.2f), t, 9.f, ACCENT, NVG_ALIGN_LEFT, 1.2f);
		auto src = [&](int side) -> std::string {
			bool cable = module->drv[side * 2].cable;
			if (cable)
				return "CABLE";
			if (module->drv[side * 2].owned)
				return side ? "MIX SEND B" : "MIX SEND A";
			return "\xe2\x80\x93";
		};
		std::string s = "A \xe2\x80\xb9 " + src(0) + "      B \xe2\x80\xb9 " + src(1);
		ui::text(vg, f, dx + mm2px(3.f), dy + mm2px(9.8f), s, 7.4f, ui::TXT, NVG_ALIGN_LEFT, 1.0f);
		// meters
		for (int k = 0; k < 2; k++) {
			float x = dx + dw - mm2px(40.f), y = dy + mm2px(3.f + k * 4.5f), w = mm2px(36.f), h = mm2px(1.4f);
			ui::text(vg, f, x - mm2px(2.f), y + h, k ? "B" : "A", 6.5f, ui::DIM, NVG_ALIGN_RIGHT, 0.5f);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x, y, w, h, h / 2);
			nvgFillColor(vg, nvgRGB(0x1d, 0x1f, 0x26));
			nvgFill(vg);
			float v = module->meter[k].load();
			float lvl = v <= 0.001f ? 0.f : clamp((20.f * std::log10(v / 5.f) + 48.f) / 54.f, 0.f, 1.f); // -48 .. +6 dB
			if (lvl > 0.f) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x, y, w * lvl, h, h / 2);
				nvgFillColor(vg, lvl > 0.9f ? nvgRGB(0xff, 0x4f, 0x4f) : ACCENT);
				nvgFill(vg);
			}
		}
		// lit segments: the inputs MIX's sends feed, the outputs MIX takes back
		if (found) {
			for (int k = 0; k < 4; k++) {
				if (module->drv[k].owned)
					follow::segment(vg, jIn[k], ACCENT);
				if (module->returnPulled(k / 2))
					follow::segment(vg, jOut[k], ACCENT);
			}
		}
	}
};

template <int R, int G, int B>
using Lit = VCVLightBezelLatch<ui::AccentLight<R, G, B>>;

struct GenraFxWidget : ModuleWidget {
	GenraFxWidget(GenraFx* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GenraFx.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		using M = GenraFx;
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		auto large = [&](const float* p, int id) { addParam(createParamCentered<GLargeKnob>(ui::mm(p), module, id)); };

		// reverb
		ChoiceField* tf = createParam<ChoiceField>(mm2px(Vec(L::V_TYPEF[0], L::V_TYPEF[1])), module, M::V_TYPE_PARAM);
		tf->box.size = mm2px(Vec(L::V_TYPEF[2], L::V_TYPEF[3]));
		tf->title = "Reverb type";
		for (int i = 0; i < gfx::NUM_REVERB_TYPES; i++)
			tf->names.push_back(gfx::REVERB_TYPE_NAMES[i]);
		tf->left = [=]() { return std::string(gfx::REVERB_TYPE_NAMES[clamp(tf->value(), 0, gfx::NUM_REVERB_TYPES - 1)]); };
		tf->right = [=]() {
			if (!module)
				return std::string();
			if (module->params[M::V_FREEZE_PARAM].getValue() > 0.5f)
				return std::string("FROZEN");
			float s = 0.1f * std::pow(300.f, module->params[M::V_DECAY_PARAM].getValue());
			return s < 10.f ? string::f("%.1f s", s) : string::f("%.0f s", s);
		};
		addParam(tf);
		large(L::V_DECAY, M::V_DECAY_PARAM);
		small(L::V_SIZE, M::V_SIZE_PARAM);
		small(L::V_PRE, M::V_PRE_PARAM);
		small(L::V_DAMP, M::V_DAMP_PARAM);
		small(L::V_WIDTH, M::V_WIDTH_PARAM);
		small(L::V_MOD, M::V_SHIM_PARAM);
		small(L::V_LVL, M::V_LVL_PARAM);
		addParam(createLightParamCentered<Lit<FX_RGB>>(ui::mm(L::V_FREEZE), module, M::V_FREEZE_PARAM, M::FREEZE_LIGHT));

		// delay
		ChoiceField* df = createParam<ChoiceField>(mm2px(Vec(L::D_TIMEF[0], L::D_TIMEF[1])), module, M::D_MODE_PARAM);
		df->box.size = mm2px(Vec(L::D_TIMEF[2], L::D_TIMEF[3]));
		df->title = "Delay time";
		df->names = {"STRAIGHT", "DOTTED", "TRIPLET", "FREE"};
		df->left = [=]() {
			if (!module)
				return std::string("1/8");
			int tm = module->timeMode();
			float tk = module->timeKnob();
			if (tm == T_FREE)
				return string::f("FREE  %.0f ms", GenraFx::freeMs(tk));
			int d = module->divIndex(tk);
			if (tm == T_DOTTED) { // a dotted value reads as its sum: 1/8 dotted = 3/16
				static const char* const DOT[gfx::NUM_DIVS] = {"3/64", "3/32", "3/16", "3/8", "3/4", "3/2"};
				return std::string(DOT[d]) + "  DOTTED";
			}
			return std::string(gfx::DIV_NAMES[d]) + (tm == T_TRIPLET ? "  TRIPLET" : "");
		};
		df->right = [=]() {
			if (!module || module->timeMode() == T_FREE)
				return std::string();
			gfx::DelayParams p;
			p.sync = true;
			p.div = module->divIndex(module->timeKnob());
			int tm = module->timeMode();
			p.mode = tm == T_DOTTED ? gfx::DOTTED : (tm == T_TRIPLET ? gfx::TRIPLET : gfx::STRAIGHT);
			p.bpm = module->bpmNow.load();
			return string::f("%.0f ms", gfx::delaySeconds(p) * 1000.f);
		};
		df->extraMenu = [=](Menu* menu) {
			// every division in every mode sets mode and knob at once; FREE keeps the knob
			static const char* const MN[3] = {"", " dotted", " triplet"};
			for (int tm = 0; tm < 3; tm++) {
				for (int d = 0; d < gfx::NUM_DIVS; d++) {
					std::string n = std::string(gfx::DIV_NAMES[d]) + MN[tm];
					menu->addChild(createCheckMenuItem(n, "",
					                                   [=]() { return module && module->timeMode() == tm && module->divIndex(module->params[M::D_TIME_PARAM].getValue()) == d; },
					                                   [=]() {
						                                   if (!module)
							                                   return;
						                                   module->params[M::D_MODE_PARAM].setValue((float) tm);
						                                   module->params[M::D_TIME_PARAM].setValue((float) d / (gfx::NUM_DIVS - 1));
					                                   }));
				}
				menu->addChild(new MenuSeparator);
			}
			menu->addChild(createCheckMenuItem("Free (ms, the TIME knob)", "", [=]() { return module && module->timeMode() == T_FREE; },
			                                   [=]() {
				                                   if (module)
					                                   module->params[M::D_MODE_PARAM].setValue((float) T_FREE);
			                                   }));
		};
		addParam(df);
		large(L::D_TIME, M::D_TIME_PARAM);
		small(L::D_FB, M::D_FB_PARAM);
		small(L::D_TONE, M::D_TONE_PARAM);
		small(L::D_TAPE, M::D_TAPE_PARAM);
		small(L::D_DUCK, M::D_DUCK_PARAM);
		small(L::D_WIDTH, M::D_WIDTH_PARAM);
		small(L::D_LVL, M::D_LVL_PARAM);
		ui::LampRow* lr = createParam<ui::LampRow>(Vec(), module, M::D_PING_PARAM);
		lr->accent = ACCENT;
		lr->place(L::D_MODE, 2);
		addParam(lr);

		const float* ins[M::NUM_INPUTS] = {L::IN_AL, L::IN_AR, L::CV_DECAY, L::CV_FREEZE, L::IN_BL, L::IN_BR, L::CV_TIME, L::CV_FB};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_AL, L::OUT_AR, L::OUT_BL, L::OUT_BR};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));

		FxOverlay* ov = new FxOverlay;
		ov->module = module;
		ov->box.size = box.size;
		addChild(ov);
	}

	void appendContextMenu(Menu* menu) override {
		GenraFx* m = dynamic_cast<GenraFx*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createCheckMenuItem("Follow GENRA (tempo)", "", [=]() { return m->followGenra.load(); }, [=]() {
			m->followGenra = !m->followGenra.load();
			m->uiTimer = 0.f;
		}));
		menu->addChild(createSubmenuItem("Free tempo", string::f("%.0f BPM", m->freeBpm), [=](Menu* sub) {
			sub->addChild(createMenuLabel("Without GENRA (or Follow off)"));
			for (int b : {70, 85, 90, 100, 110, 120, 124, 128, 130, 135, 140, 150, 160, 170, 174})
				sub->addChild(createCheckMenuItem(string::f("%d BPM", b), "", [=]() { return std::fabs(m->freeBpm - b) < 0.5f; },
				                                  [=]() { m->freeBpm = (float) b; }));
		}));
		for (int k = 0; k < 2; k++) {
			const char* sendName = k ? "MIX send B" : "MIX send A";
			menu->addChild(createSubmenuItem(k ? "Input B from" : "Input A from", m->inMode[k].load() ? sendName : "Cables only",
			                                 [=](Menu* sub) {
				                                 sub->addChild(createCheckMenuItem(std::string("Auto: ") + sendName, "",
				                                                                   [=]() { return m->inMode[k].load() == 1; },
				                                                                   [=]() { m->inMode[k] = 1, m->uiTimer = 0.f; }));
				                                 sub->addChild(createCheckMenuItem("Off (cables only)", "", [=]() { return m->inMode[k].load() == 0; },
				                                                                   [=]() { m->inMode[k] = 0; }));
				                                 sub->addChild(new MenuSeparator);
				                                 sub->addChild(createMenuLabel("A cable in IN L / IN R always wins"));
			                                 }));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("MIX takes OUT A / OUT B back into its returns (MIX › Return from)"));
	}
};

} // namespace genrafx

Model* modelGenraFx = createModel<genrafx::GenraFx, genrafx::GenraFxWidget>("GenraFx");
