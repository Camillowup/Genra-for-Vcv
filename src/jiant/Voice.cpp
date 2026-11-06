// SPDX-License-Identifier: GPL-3.0-only
// JIANT VOICE: the JIANT FM firmware's VOICE engine (a Klatt formant singer: vowels, and words in Spanish, English and
// Japanese) as a VCV Rack voice, its vocal tract drawn live. The engine runs unchanged in a private copy of the VOICE
// core (Core.hpp); this file is the panel, the jacks and the glue: knobs and CV into the part's parameters, V/OCT + GATE
// into its notes (each note sings the next syllable), the lyric typed on the panel, its 44.1 kHz audio resampled.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "VoiceCore.hpp"
#include <atomic>
#include <array>
#include <cstring>
#include <mutex>
#include "JiantUI.hpp"

namespace jiant_voice {


static const int NE = 8;

// ------------------------------------------------------------------------------------------------ the module
struct Voice : Module, follow::Host {
	enum ParamId {
		ENUMS(E_PARAM, NE),
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM,
		DIST_PARAM, CHOR_PARAM, DLY_PARAM, REV_PARAM,
		LEVEL_PARAM,
		VOICE_PARAM, LANG_PARAM, LYRIC_PARAM,
		PREV_PARAM, NEXT_PARAM, LPREV_PARAM, LNEXT_PARAM, CALL_PARAM,
		PARAMS_LEN
	};
	enum InputId { ENUMS(E_INPUT, NE), VOCT_INPUT, GATE_INPUT, VEL_INPUT, CALL_INPUT, WORD_INPUT, INPUTS_LEN };
	enum OutputId { L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { CALL_LIGHT, LIGHTS_LEN };
	enum { BLOCK = 32, CALL_NOTE = 60 };

	VoiceCore core;
	std::string status;

	// settings
	int preset = 0;
	bool fromPatch = false;
	std::atomic<int> presetReq{-1};
	std::mutex textMutex;
	std::string text;                            // the lyric (LYRIC = TEXT), the panel's field
	std::atomic<bool> textDirty{false};
	std::atomic<uint32_t> textSeq{0};            // the field shows it again when it changes from here

	// for the UI thread
	std::array<std::array<char, 16>, NE> labels{};
	std::array<std::array<char, 16>, JVO_NP> values{};
	uint16_t screen[JVO_VIS_W * JVO_VIS_H] = {};
	std::atomic<uint32_t> screenSeq{0};
	std::atomic<int> shownPreset{0};
	std::atomic<int> voices{0};
	std::array<char, 24> word{};
	std::atomic<int> syl{0}, nsyl{0};

	// engine thread
	int sent[JVO_NP];
	float devL[BLOCK], devR[BLOCK];
	dsp::SampleRateConverter<2> src;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 2048> outBuf;
	uint32_t framesSinceVis = 0;
	struct CvVoice { bool gate = false; int note = 60; };
	CvVoice cv[16];
	bool callOn = false;
	dsp::BooleanTrigger prevTrig, nextTrig, lprevTrig, lnextTrig;
	dsp::SchmittTrigger wordTrig;

	/** E1..E8: the knob 0..1 over the range the current LANG gives it, shown as the FM-1 writes it. */
	struct EQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Voice* m = dynamic_cast<Voice*>(module);
			if (m && m->core.loaded())
				return m->values[paramId - E_PARAM].data();
			return ParamQuantity::getDisplayValueString();
		}
		std::string getLabel() override {
			Voice* m = dynamic_cast<Voice*>(module);
			if (m && m->labels[paramId - E_PARAM][0])
				return m->labels[paramId - E_PARAM].data();
			return ParamQuantity::getLabel();
		}
	};
	/** ATK .. LEVEL: the FM-1's 0..127, shown as it writes them. */
	struct RawQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Voice* m = dynamic_cast<Voice*>(module);
			if (m && m->core.loaded())
				return m->values[JVO_ATK + paramId - ATK_PARAM].data();
			return ParamQuantity::getDisplayValueString();
		}
	};

	Voice() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		srcName = "VOICE";
		outL = L_OUTPUT;
		outR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		for (int k = 0; k < NE; k++) {
			configParam<EQuantity>(E_PARAM + k, 0.f, 1.f, 0.5f, string::f("E%d", k + 1));
			configInput(E_INPUT + k, string::f("E%d CV (±5 V: ± half the range)", k + 1));
		}
		const char* envN[4] = {"Attack", "Decay", "Sustain", "Release"};
		const char* fxN[4] = {"Distortion", "Chorus send", "Delay send", "Reverb send"};
		for (int i = 0; i < 4; i++) {
			configParam<RawQuantity>(ATK_PARAM + i, 0.f, 127.f, 0.f, envN[i])->snapEnabled = true;
			configParam<RawQuantity>(DIST_PARAM + i, 0.f, 127.f, 0.f, fxN[i])->snapEnabled = true;
		}
		configParam<RawQuantity>(LEVEL_PARAM, 0.f, 127.f, 104.f, "Level")->snapEnabled = true;
		configSwitch(VOICE_PARAM, 0.f, 2.f, 0.f, "Voice", {"POLY", "MONO", "LEGATO"});
		configSwitch(LANG_PARAM, 0.f, 3.f, 0.f, "Language", {"OFF (vowels)", "ES (Spanish)", "EN (English)", "JA (Japanese)"});
		std::vector<std::string> lyr = {"TEXT (the panel's lyric)"};
		for (int i = 1; i <= 16; i++)
			lyr.push_back(string::f("W%d (words)", i));
		for (int i = 1; i <= 8; i++)
			lyr.push_back(string::f("S%d (scat)", i));
		configSwitch(LYRIC_PARAM, 0.f, 24.f, 0.f, "Lyric", lyr);
		configButton(PREV_PARAM, "Previous preset");
		configButton(NEXT_PARAM, "Next preset");
		configButton(LPREV_PARAM, "Previous lyric");
		configButton(LNEXT_PARAM, "Next lyric");
		configButton(CALL_PARAM, "Call (plays C4: the next syllable)");
		configInput(VOCT_INPUT, "V/Oct (polyphonic, 4 voices)");
		configInput(GATE_INPUT, "Gate (polyphonic; each note sings the next syllable)");
		configInput(VEL_INPUT, "Velocity (0..10 V, polyphonic)");
		configInput(CALL_INPUT, "Call gate (plays C4)");
		configInput(WORD_INPUT, "Restart the lyric (trigger)");
		configOutput(L_OUTPUT, "Left (mono when Right is not patched)");
		configOutput(R_OUTPUT, "Right");
		for (int j = ATK_PARAM; j < PARAMS_LEN; j++)
			paramQuantities[j]->randomizeEnabled = j < DIST_PARAM;
		src.setQuality(6);
		for (int& s : sent)
			s = INT32_MIN;
	}

	// ---------------------------------------------------------------------------------- power, presets
	void powerOn() {
		if (!core.load()) {
			status = core.error;
			WARN("JIANT VOICE: %s", status.c_str());
			return;
		}
		status.clear();
		core.api->init(random::u32() | 1u);
		for (int& s : sent)
			s = INT32_MIN;
		outBuf.clear();
		src.refreshState();
		for (auto& v : cv)
			v.gate = false;
		callOn = false;
		{
			std::lock_guard<std::mutex> lock(textMutex);
			core.api->set_text(text.c_str());
		}
		{ // a patch from a version with more (or fewer) presets: stay in range
			int n = std::max(1, (int) core.api->npresets());
			preset = ((preset % n) + n) % n;
		}
		if (!fromPatch)
			applyPreset(preset);
		else
			core.api->load_preset(preset);       // (the knobs follow in the first block)
		shownPreset = preset;
	}

	/** The factory sound into the knobs (and the engine). */
	void applyPreset(int i) {
		const jvo_api* a = core.api;
		if (!a)
			return;
		int n = (int) a->npresets();
		preset = ((i % n) + n) % n;
		a->all_off();
		a->load_preset((uint32_t) preset);
		params[LANG_PARAM].setValue((float) a->get(JVO_LANG));
		params[LYRIC_PARAM].setValue((float) a->get(JVO_LYRIC));
		int vo = a->get(JVO_VOICE);
		params[VOICE_PARAM].setValue((float) clamp(vo, 0, 2));
		for (int k = 0; k < NE; k++) {
			uint32_t j = JVO_E1 + k;
			int lo = a->min(j), hi = a->max(j), v = a->get(j);
			params[E_PARAM + k].setValue(hi > lo ? (float) (v - lo) / (hi - lo) : 0.f);
		}
		for (int i2 = 0; i2 < 4; i2++) {
			params[ATK_PARAM + i2].setValue((float) a->get(JVO_ATK + i2));
			params[DIST_PARAM + i2].setValue((float) a->get(JVO_DIST + i2));
		}
		for (int& s : sent)
			s = INT32_MIN;
		shownPreset = preset;
	}

	void onAdd(const AddEvent& e) override {
		for (int j = PREV_PARAM; j <= CALL_PARAM; j++)
			params[j].setValue(0.f);
		powerOn();
	}
	void onRemove(const RemoveEvent& e) override { follow::gone(this); core.unload(); }
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		fromPatch = false;
		presetReq = 0;
	}

	void setText(const std::string& s) {
		std::lock_guard<std::mutex> lock(textMutex);
		text = s.substr(0, JVO_TEXT_MAX);
		textDirty = true;
	}
	std::string getText() {
		std::lock_guard<std::mutex> lock(textMutex);
		return text;
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "preset", json_integer(preset));
		json_object_set_new(r, "text", json_string(getText().c_str()));
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		if (json_t* j = json_object_get(r, "preset"))
			preset = (int) json_integer_value(j);
		if (json_t* j = json_object_get(r, "text")) {
			std::lock_guard<std::mutex> lock(textMutex);
			text = json_string_value(j) ? json_string_value(j) : "";
		}
		textSeq++;
		fromPatch = true;
		if (core.api) { // a preset loaded onto (or pasted into) a running module: Rack holds the engine lock here
			int n = std::max(1, (int) core.api->npresets());
			preset = ((preset % n) + n) % n;
			core.api->load_preset(preset);
			textDirty = true; // the text loaded with it, not the preset's
			for (int& s : sent)
				s = INT32_MIN; // the knobs' values follow in the next block
			shownPreset = preset;
		}
	}

	// ---------------------------------------------------------------------------------- the engine
	void push(const jvo_api* a, uint32_t j, int v) {
		if (sent[j] != v) {
			a->set(j, v);
			sent[j] = v;
		}
	}

	void processKnobs(const jvo_api* a) {
		int vo = (int) std::round(params[VOICE_PARAM].getValue());
		if (sent[JVO_VOICE] != INT32_MIN && sent[JVO_VOICE] != vo)
			a->all_off();
		push(a, JVO_VOICE, vo);
		push(a, JVO_LANG, (int) std::round(params[LANG_PARAM].getValue()));
		push(a, JVO_LYRIC, (int) std::round(params[LYRIC_PARAM].getValue()));
		for (int k = 0; k < NE; k++) {          // (after LANG: its ranges)
			uint32_t j = JVO_E1 + k;
			float x = params[E_PARAM + k].getValue();
			if (inputs[E_INPUT + k].isConnected())
				x += inputs[E_INPUT + k].getVoltage() / 10.f;
			x = clamp(x, 0.f, 1.f);
			int lo = a->min(j), hi = a->max(j);
			push(a, j, lo + (int) std::round(x * (hi - lo)));
		}
		for (int i = 0; i < 4; i++) {
			push(a, JVO_ATK + i, (int) std::round(params[ATK_PARAM + i].getValue()));
			push(a, JVO_DIST + i, (int) std::round(params[DIST_PARAM + i].getValue()));
		}
		push(a, JVO_LEVEL, (int) std::round(params[LEVEL_PARAM].getValue()));
		if (textDirty.exchange(false)) {
			a->set_text(getText().c_str());
			a->restart();
		}
	}

	void processNotes(const jvo_api* a) {
		int n = inputs[GATE_INPUT].isConnected() ? std::max(1, inputs[GATE_INPUT].getChannels()) : 0;
		for (int c = 0; c < 16; c++) {
			CvVoice& v = cv[c];
			bool on = false;
			if (c < n) {
				float g = inputs[GATE_INPUT].getPolyVoltage(c);
				on = v.gate ? g > 0.5f : g >= 1.f;
			}
			int note = clamp((int) std::round(inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f) + 60, 0, 127);
			if (on && !v.gate) {
				int vel = 100;
				if (inputs[VEL_INPUT].isConnected())
					vel = clamp((int) std::round(inputs[VEL_INPUT].getPolyVoltage(c) / 10.f * 127.f), 1, 127);
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
		if (wordTrig.process(inputs[WORD_INPUT].getVoltage(), 0.1f, 1.f))
			a->restart();
	}

	void renderBlock(const jvo_api* a, float sampleRate) {
		processKnobs(a);
		a->render(BLOCK, devL, devR);
		dsp::Frame<2> in[BLOCK];
		for (int i = 0; i < BLOCK; i++) {
			in[i].samples[0] = devL[i];
			in[i].samples[1] = devR[i];
		}
		int devRate = (int) a->sample_rate();
		if ((int) sampleRate == devRate) {
			for (int i = 0; i < BLOCK && !outBuf.full(); i++)
				outBuf.push(in[i]);
		} else {
			src.setRates(devRate, (int) sampleRate);
			int inLen = BLOCK, outLen = (int) outBuf.capacity();
			src.process(in, &inLen, outBuf.endData(), &outLen);
			outBuf.endIncr(outLen);
		}
		framesSinceVis += BLOCK;
		if (framesSinceVis >= (uint32_t) devRate / 30) {     // the tract, 30 frames a second
			framesSinceVis = 0;
			std::memcpy(screen, a->vis(JVO_VIS_W, JVO_VIS_H), sizeof screen);
			screenSeq++;
			for (int k = 0; k < NE; k++)
				std::snprintf(labels[k].data(), labels[k].size(), "%s", a->label(JVO_E1 + k));
			for (uint32_t j = 0; j < JVO_NP; j++)
				a->format(j, a->get(j), values[j].data(), (uint32_t) values[j].size());
			uint32_t s = 0, ns = 0;
			a->word(word.data(), (uint32_t) word.size(), &s, &ns);
			syl = (int) s;
			nsyl = (int) ns;
			voices = (int) a->active_voices();
		}
	}

	void process(const ProcessArgs& args) override {
		fol.process(this); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		const jvo_api* a = core.api;
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
		int ly = (int) std::round(params[LYRIC_PARAM].getValue());
		if (lprevTrig.process(params[LPREV_PARAM].getValue() > 0.5f))
			params[LYRIC_PARAM].setValue((float) ((ly + 24) % 25));
		if (lnextTrig.process(params[LNEXT_PARAM].getValue() > 0.5f))
			params[LYRIC_PARAM].setValue((float) ((ly + 1) % 25));
		processNotes(a);
		int guard = 0;
		while (outBuf.empty() && guard++ < 8)
			renderBlock(a, args.sampleRate);
		dsp::Frame<2> f = outBuf.empty() ? dsp::Frame<2>{} : outBuf.shift();
		float l = f.samples[0] * 5.f, r = f.samples[1] * 5.f;
		if (outputs[R_OUTPUT].isConnected()) {
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		} else {
			outputs[L_OUTPUT].setVoltage(0.5f * (l + r));
			outputs[R_OUTPUT].setVoltage(0.f);
		}
	}
};

// ------------------------------------------------------------------------------------------------ the panel (GENRA look)
namespace L = jlayout::voice;
static const NVGcolor ACCENT = nvgRGB(JIANT_VOICE_RGB);
static const char* LANG_NAMES[4] = {"OFF", "ES", "EN", "JA"};
static const char* VOICE_NAMES[3] = {"POLY", "MONO", "LEGATO"};

static std::string lyricName(int v) {
	return v == 0 ? "TEXT" : v <= 16 ? string::f("W%d", v) : string::f("S%d", v - 16);
}

/** The vocal tract (the firmware's own drawing), drawn in the light layer. */
struct ScreenWidget : Widget {
	Voice* module = nullptr;
	jiant::Canvas565 canvas{JVO_VIS_W, JVO_VIS_H};
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		if (!module) {
			jiant::text(args.vg, jiant::boldFont(), box.size.x / 2, box.size.y / 2 + 5, "VOICE", 14.f, ACCENT,
			            NVG_ALIGN_CENTER, 2.f);
			return;
		}
		if (!module->status.empty()) {
			jiant::drawStatus(args.vg, module->status, box.size.x);
			return;
		}
		canvas.draw(args.vg, module->screen, module->screenSeq, box.size.x, box.size.y);
	}
};

/** Right of the vocal tract: the preset, LANG (chips, their own widget), LYRIC, the lyric's field (its own widget) and
 * the word sung with its syllables. */
struct InfoWidget : Widget {
	Voice* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		auto bold = jiant::boldFont(), mono = jiant::monoFont();
		float w = box.size.x;
		const jvo_api* a = module ? module->core.api : nullptr;
		int p = module ? (int) module->shownPreset : 0;
		int lang = module ? clamp((int) std::round(module->params[Voice::LANG_PARAM].getValue()), 0, 3) : 1;
		int lyr = module ? (int) std::round(module->params[Voice::LYRIC_PARAM].getValue()) : 1;
		int vo = module ? clamp((int) std::round(module->params[Voice::VOICE_PARAM].getValue()), 0, 2) : 0;
		jiant::text(vg, bold, 0, 20, a ? a->preset_name((uint32_t) p) : "CHOIR AAH", 15.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.6f);
		jiant::text(vg, bold, w, 20, string::f("\xc2\xb7 %d/%u", p + 1, a ? a->npresets() : 15u), 6.5f, jiant::DIM,
		            NVG_ALIGN_RIGHT, 0.8f);
		jiant::text(vg, mono, 0, 33, string::f("LANG %s \xc2\xb7 LYRIC %s \xc2\xb7 %s", LANG_NAMES[lang], lyricName(lyr).c_str(),
		                                       VOICE_NAMES[vo]), 7.f, jiant::DIM, NVG_ALIGN_LEFT, 0.f);
		// LANG chips at x 0..125; the lyric chosen right of them
		float lx = 128;
		nvgBeginPath(vg);
		nvgRoundedRect(vg, lx, 41, w - lx, 14, 2.2f);
		nvgFillColor(vg, jiant::FIELD);
		nvgFill(vg);
		nvgStrokeColor(vg, jiant::EDGE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		jiant::text(vg, mono, lx + (w - lx) / 2, 51, lyricName(lyr), 8.f, ACCENT, NVG_ALIGN_CENTER, 0.f);
		// the lyric's field
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, 60, w, 14, 2.2f);
		nvgFillColor(vg, jiant::FIELD);
		nvgFill(vg);
		NVGcolor edge = ACCENT;
		edge.a = lyr == 0 ? 0.6f : 0.25f;
		nvgStrokeColor(vg, edge);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		// the word sung, its syllables as dots (lit: sung)
		if (module && module->word[0]) {
			std::string wd = module->word.data();
			jiant::text(vg, bold, 0, 92, wd, 11.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.4f);
			float tw = nvgTextBounds(vg, 0, 0, wd.c_str(), nullptr, nullptr);
			int ns = std::min((int) module->nsyl, 8), sy = module->syl;
			for (int i = 0; i < ns; i++) {
				nvgBeginPath(vg);
				nvgCircle(vg, tw + 8 + i * 8, 88.5f, 2.3f);
				nvgFillColor(vg, i < sy ? ACCENT : nvgRGB(0x33, 0x34, 0x3c));
				nvgFill(vg);
			}
			jiant::text(vg, mono, w, 92, string::f("SYL %d/%d", sy, (int) module->nsyl), 6.6f, jiant::DIM, NVG_ALIGN_RIGHT, 0.f);
		} else if (module) {
			int nv = module->voices;
			jiant::text(vg, mono, 0, 92, string::f("%d VOICE%s", nv, nv == 1 ? "" : "S"), 7.f, jiant::DIM, NVG_ALIGN_LEFT, 0.f);
		}
	}
};

/** The lyric typed on the panel (LYRIC = TEXT sings it). */
struct LyricField : jiant::DisplayTextField {
	Voice* module = nullptr;
	uint32_t seq = ~0u;
	LyricField() {
		color = ACCENT;
		placeholder = "type a lyric \xe2\x80\xa6";
		multiline = false;
	}
	void step() override {
		if (module && module->textSeq != seq && APP->event->selectedWidget != this) {
			seq = module->textSeq;
			setText(module->getText());
		}
		DisplayTextField::step();
	}
	void onChange(const ChangeEvent& e) override {
		if (module) {
			std::string t = getText();
			if ((int) t.size() > JVO_TEXT_MAX) {
				t.resize(JVO_TEXT_MAX);
				setText(t);
				return;
			}
			if (t != module->getText()) {
				module->setText(t);
				// typing sings it: LYRIC to TEXT, a language if none
				module->paramQuantities[Voice::LYRIC_PARAM]->setValue(0.f);
				if (module->params[Voice::LANG_PARAM].getValue() < 0.5f)
					module->paramQuantities[Voice::LANG_PARAM]->setValue(2.f);
			}
		}
		DisplayTextField::onChange(e);
	}
};

struct VoiceWidget : ModuleWidget {
	VoiceWidget(Voice* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/JiantVoice.svg")));
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
		jiant::ChipRow* lang = createParam<jiant::ChipRow>(Vec(L::INFO[0], L::INFO[1] + 41), module, Voice::LANG_PARAM);
		lang->box.size = Vec(125, 14);
		lang->names = {"OFF", "ES", "EN", "JA"};
		lang->accent = ACCENT;
		addParam(lang);
		LyricField* lf = createWidget<LyricField>(Vec(L::INFO[0], L::INFO[1] + 60));
		lf->box.size = Vec(L::INFO[2], 14);
		lf->module = module;
		addChild(lf);

		addParam(createParamCentered<VCVBezel>(jiant::at(L::PREV), module, Voice::PREV_PARAM));
		addParam(createParamCentered<VCVBezel>(jiant::at(L::NEXT), module, Voice::NEXT_PARAM));
		addParam(createParamCentered<VCVBezel>(jiant::at(L::LPREV), module, Voice::LPREV_PARAM));
		addParam(createParamCentered<VCVBezel>(jiant::at(L::LNEXT), module, Voice::LNEXT_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<jiant::AccentLight<JIANT_VOICE_RGB>>>(
		    jiant::at(L::CALL), module, Voice::CALL_PARAM, Voice::CALL_LIGHT));
		jiant::LampRow* vs = createParam<jiant::LampRow>(Vec(0, 0), module, Voice::VOICE_PARAM);
		vs->place(L::VOICE, 3);
		vs->accent = ACCENT;
		addParam(vs);
		addParam(createParamCentered<GSmallKnob>(jiant::at(L::LEVEL), module, Voice::LEVEL_PARAM));

		static const char* defLab[NE] = {"VOWEL", "VOWEL2", "TALK", "SHIFT", "BUZZ", "BREATH", "Q", "RAND"};
		for (int k = 0; k < NE; k++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(L::E[k]), module, Voice::E_PARAM + k));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::E_IN[k]), module, Voice::E_INPUT + k));
			jiant::KnobText* t = new jiant::KnobText;
			t->box = jiant::area(L::E_TEXT[k]);
			t->accent = ACCENT;
			t->label = [=]() -> std::string {
				if (module && module->labels[k][0])
					return module->labels[k].data();
				return defLab[k];
			};
			t->value = [=]() -> std::string { return module && module->core.loaded() ? module->values[k].data() : ""; };
			addChild(t);
		}
		const float* env[4] = {L::ATK, L::DEC, L::SUS, L::REL};
		const float* fx[4] = {L::DIST, L::CHOR, L::DLY, L::REV};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<GTrimpot>(jiant::at(env[i]), module, Voice::ATK_PARAM + i));
			addParam(createParamCentered<GTrimpot>(jiant::at(fx[i]), module, Voice::DIST_PARAM + i));
		}
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VOCT), module, Voice::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::GATE), module, Voice::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VEL), module, Voice::VEL_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::CALL_IN), module, Voice::CALL_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::WORD), module, Voice::WORD_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_L), module, Voice::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_R), module, Voice::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Voice* m = getModule<Voice>();
		if (!m || !m->core.loaded())
			return;
		menu->addChild(new MenuSeparator);
		std::vector<std::string> names;
		for (uint32_t i = 0; i < m->core.api->npresets(); i++)
			names.push_back(m->core.api->preset_name(i));
		menu->addChild(createIndexSubmenuItem("Preset", names, [=]() { return (size_t) m->shownPreset; },
		                                      [=](size_t i) { m->presetReq = (int) i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Each note sings the next syllable; WORD \xe2\x86\xba starts the lyric over"));
		menu->addChild(createMenuLabel("Engine: VOICE from JIANT FM by Juan Jiant (a Felucca fork), GPL-3.0"));
	}
};

}  // namespace jiant_voice

Model* modelVoice = createModel<jiant_voice::Voice, jiant_voice::VoiceWidget>("Voice");
