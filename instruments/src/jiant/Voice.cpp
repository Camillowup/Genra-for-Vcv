// SPDX-License-Identifier: GPL-3.0-only
// JIANT VOICE: the JIANT FM firmware's VOICE engine (a Klatt formant singer: vowels, and words in Spanish, English and
// Japanese) as a VCV Rack voice, its vocal tract drawn live. The engine runs unchanged in a private copy of the VOICE
// core (Core.hpp); this file is the panel, the jacks and the glue: knobs and CV into the part's parameters, V/OCT + GATE
// into its notes (each note sings the next syllable), the lyric typed on the panel, its 44.1 kHz audio resampled.
// LANG SPELL (2.35.0) is a fifth language of its own: SPELLBOX's LPC talking-toy voice (Spell.hpp, SpellCore), which
// sings a word or each key's letter, or speaks the lyric, bent by the expander VOICE BEND on its right.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "VoiceCore.hpp"
#include "Spell.hpp"
#include "VoiceBend.hpp"
#include <atomic>
#include <array>
#include <cstring>
#include <mutex>
#include "JiantUI.hpp"
#include "../ui/Look.hpp"

namespace jiant_voice {


static const int NE = 8;

// ------------------------------------------------------------------------------------------------ the module
struct Voice : Module, follow::Host, SpellSink {
	enum ParamId {
		ENUMS(E_PARAM, NE),
		ATK_PARAM, DEC_PARAM, SUS_PARAM, REL_PARAM,
		DIST_PARAM, CHOR_PARAM, DLY_PARAM, REV_PARAM,
		LEVEL_PARAM,
		VOICE_PARAM, LANG_PARAM, LYRIC_PARAM,
		PREV_PARAM, NEXT_PARAM, LPREV_PARAM, LNEXT_PARAM, CALL_PARAM,
		SPELL_MODE_PARAM,  // (2.35.0, appended: old patches load)
		PARAMS_LEN
	};
	enum InputId { ENUMS(E_INPUT, NE), VOCT_INPUT, GATE_INPUT, VEL_INPUT, CALL_INPUT, WORD_INPUT, INPUTS_LEN };
	enum OutputId { L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { CALL_LIGHT, LIGHTS_LEN };
	enum { BLOCK = 32, CALL_NOTE = 60, LANG_SPELL = 4 };

	VoiceCore core;
	SpellCore spell;
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
	// SPELL, for the UI thread
	std::array<char, 24> spWord{}, spNote{};
	std::atomic<int> spLit{0}, spLen{0}, spWordI{0}, spWords{0};
	std::array<std::atomic<int>, 10> spK{};
	std::atomic<int> spE{0}, spClock{64};

	// engine thread
	int sent[JVO_NP];
	float devL[BLOCK], devR[BLOCK];
	jiant::Resampler src;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 2048> outBuf;
	uint32_t framesSinceVis = 0;
	bool visDue = true;                          // a frame of the screen owed (drawn when it is seen)
	jiant::ScreenClock screenClock;              // set by the ScreenWidget's draw
	struct CvVoice { bool gate = false; int note = 60; };
	CvVoice cv[16];
	bool callOn = false;
	dsp::BooleanTrigger prevTrig, nextTrig, lprevTrig, lnextTrig;
	dsp::SchmittTrigger wordTrig;
	// SPELL, engine thread
	struct SpellState {
		bool on = false;           // LANG was SPELL in the last sample
		int ch = -2;               // the gate channel it sings (-1: CALL, -2: none)
		float vel = 1.f;
		int note = 60;
		int pos = 0, notes = 0;    // SPEAK: the next word, notes since the lyric's start
		std::string said;          // SPEAK: the words of the last note
		int mode = SPM_SING;
		SpellKnobs k;
	} sp;
	SpellEnv spEnv;
	SpellFx spFx;
	float spBuf[BLOCK];

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
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
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
		configSwitch(LANG_PARAM, 0.f, 4.f, 0.f, "Language", {"OFF (vowels)", "ES (Spanish)", "EN (English)", "JA (Japanese)",
		                                                     "SPELL (talking toy, English)"});
		configSwitch(SPELL_MODE_PARAM, 0.f, 2.f, 0.f, "SPELL plays",
		             {"SING (a word at each note's pitch)", "LETTER (each key its letter)", "SPEAK (the lyric, word by word)"});
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
		for (auto& k : spK)
			k = 0;
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
		if (spell.load())
			spell.api->init(random::u32() | 1u);
		else
			WARN("JIANT VOICE: %s", spell.error.c_str());
		sp = SpellState();
		spEnv = SpellEnv();
		for (int& s : sent)
			s = INT32_MIN;
		outBuf.clear();
		src.reset();
		for (auto& v : cv)
			v.gate = false;
		callOn = false;
		{
			std::lock_guard<std::mutex> lock(textMutex);
			core.api->set_text(text.c_str());
		}
		{ // a patch from a version with more (or fewer) presets: stay in range
			int n = npresets();
			preset = ((preset % n) + n) % n;
		}
		if (!fromPatch)
			applyPreset(preset);
		else if (preset < (int) core.api->npresets())
			core.api->load_preset(preset);       // (the knobs follow in the first block)
		shownPreset = preset;
	}

	/** The JIANT engine's factory sounds, then SPELL's. */
	int npresets() const { return core.api ? std::max(1, (int) core.api->npresets()) + N_SPELL_PRESETS : 1; }
	std::string presetName(int i) const {
		if (!core.api)
			return "";
		int n = (int) core.api->npresets();
		return i < n ? core.api->preset_name((uint32_t) i) : SPELL_PRESETS[clamp(i - n, 0, N_SPELL_PRESETS - 1)].name;
	}

	/** A SPELL factory sound into the knobs. */
	void applySpellPreset(const SpellPreset& q) {
		params[LANG_PARAM].setValue((float) LANG_SPELL);
		params[SPELL_MODE_PARAM].setValue((float) q.mode);
		params[VOICE_PARAM].setValue((float) q.voice);
		for (int k = 0; k < NE; k++)
			params[E_PARAM + k].setValue(q.e[k]);
		if (q.word && spell.api) { // WORD: the knob on that word
			int n = (int) spell.api->count();
			for (int i = 0; i < n; i++)
				if (!std::strcmp(spell.api->text((uint32_t) i), q.word))
					params[E_PARAM + 1].setValue((float) i / (n - 1));
		}
		for (int i = 0; i < 4; i++) {
			params[ATK_PARAM + i].setValue((float) q.env[i]);
			params[DIST_PARAM + i].setValue((float) q.fx[i]);
		}
		if (q.mode == SPM_SPEAK && getText().empty()) {
			setText("hello I am a robot");
			textSeq++;
		}
	}

	/** The factory sound into the knobs (and the engine). */
	void applyPreset(int i) {
		const jvo_api* a = core.api;
		if (!a)
			return;
		int n = (int) a->npresets(), all = npresets();
		preset = ((i % all) + all) % all;
		a->all_off();
		if (spell.api)
			spell.api->stop();
		if (preset >= n) {
			applySpellPreset(SPELL_PRESETS[preset - n]);
			for (int& s : sent)
				s = INT32_MIN;
			shownPreset = preset;
			return;
		}
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
			int n = npresets();
			preset = ((preset % n) + n) % n;
			if (preset < (int) core.api->npresets())
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
		int lang = (int) std::round(params[LANG_PARAM].getValue());
		push(a, JVO_LANG, lang == LANG_SPELL ? 0 : lang);  // (SPELL: the JIANT engine rests on its vowels, silent)
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

	bool spellLang() { return (int) std::round(params[LANG_PARAM].getValue()) == LANG_SPELL; }

	/** SPELL's knobs: the ENGINE eight (CV added) as SPELL reads them, into its chip; the bends from VOICE BEND. */
	void spellKnobs(const jsp_api* s) {
		float x[NE];
		for (int k = 0; k < NE; k++) {
			x[k] = params[E_PARAM + k].getValue();
			if (inputs[E_INPUT + k].isConnected())
				x[k] += inputs[E_INPUT + k].getVoltage() / 10.f;
		}
		sp.mode = clamp((int) std::round(params[SPELL_MODE_PARAM].getValue()), 0, SPM_N - 1);
		sp.k.read(x, sp.mode, (int) s->count());
		s->set(JSP_ARTIC, sp.k.artic);
		s->set(JSP_PITCH, sp.k.shift);
		s->set(JSP_SPEED, sp.k.speed);
		s->set(JSP_TONE, sp.k.tone);
		s->set(JSP_MONO, sp.k.mono);
		BendSource* b = nullptr;
		if (rightExpander.module && rightExpander.module->model == modelVoiceBend)
			b = dynamic_cast<BendSource*>(rightExpander.module);
		bendSeen = b ? 1 : 0;
		bool on = b && b->on;
		s->set(JSP_CLOCK, on ? b->clock.load() : 64);
		s->set(JSP_BEND, on ? b->bend.load() : 0);
		s->set(JSP_STUTTER, on ? b->stutter.load() : 0);
		s->set(JSP_CRUSH, on ? b->crush.load() : 0);
		spClock = on ? b->clock.load() : 64;
	}

	/** The lyric's words in capitals (SPEAK). */
	std::vector<std::string> lyricWords() {
		std::vector<std::string> w;
		std::string t = getText(), cur;
		for (char c : t) {
			if (std::isalpha((unsigned char) c) || (c == '\'' && !cur.empty()))
				cur += (char) std::toupper((unsigned char) c);
			else if (!cur.empty()) {
				w.push_back(cur);
				cur.clear();
			}
		}
		if (!cur.empty())
			w.push_back(cur);
		return w;
	}

	/** A note in SPELL: SING its word, LETTER its letter (held while the gate is), SPEAK the lyric's next words. */
	void spellNoteOn(const jsp_api* s, int note, int ch) {
		sp.ch = ch;
		sp.note = note;
		int sung = clamp(note + sp.k.shift, 0, 127);
		if (sp.mode == SPM_SING) {
			s->say((uint32_t) sp.k.sel, (uint32_t) sung, 1);
		} else if (sp.mode == SPM_LETTER) {
			int L = sp.k.sel ? sp.k.sel - 1 : ((note - 53) % 26 + 26) % 26;  // (A on F3, as SPELLBOX's keys)
			s->say(s->letter_base() + (uint32_t) L, (uint32_t) sung, 1);
		} else {
			std::vector<std::string> w = lyricWords();
			if (w.empty())
				return;
			int nw = (int) w.size();
			int loop = SPELL_LOOPS[sp.k.loop];
			if (loop && sp.notes >= loop)
				sp.pos = 0, sp.notes = 0;
			int at = sp.k.pos ? (sp.k.pos - 1) % nw : sp.pos % nw;
			int take = sp.k.sel >= SPELL_KWORDS ? nw : sp.k.sel + 1;
			std::string say;
			for (int i = 0; i < take && i < nw; i++)
				say += (i ? " " : "") + w[(at + i) % nw];
			s->speak(say.c_str());
			sp.said = say;
			sp.pos = (at + take) % nw;
			sp.notes++;
			spWordI = at + 1;
			spWords = nw;
		}
		spEnv.gate(true);
	}
	void spellNoteOff(const jsp_api* s) {
		if (sp.mode != SPM_SPEAK)
			s->release();
		spEnv.gate(false);
		sp.ch = -2;
	}

	void processSpellNotes(const jsp_api* s) {
		int n = inputs[GATE_INPUT].isConnected() ? std::max(1, inputs[GATE_INPUT].getChannels()) : 0;
		int legato = (int) std::round(params[VOICE_PARAM].getValue()) == 2;
		int rose = -1;
		for (int c = 0; c < 16; c++) {  // one voice: the newest gate wins
			CvVoice& v = cv[c];
			bool on = false;
			if (c < n) {
				float g = inputs[GATE_INPUT].getPolyVoltage(c);
				on = v.gate ? g > 0.5f : g >= 1.f;
			}
			int note = clamp((int) std::round(inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f) + 60, 0, 127);
			if (on && !v.gate)
				rose = c;
			else if (on && c == sp.ch && note != v.note && sp.mode != SPM_SPEAK) {  // the note it sings moved
				if (legato)
					s->sing_note((uint32_t) clamp(note + sp.k.shift, 0, 127)), sp.note = note;
				else
					rose = c;
			} else if (!on && v.gate && c == sp.ch)
				spellNoteOff(s);
			v.gate = on;
			v.note = note;
		}
		if (rose >= 0) {
			int vel = 100;
			if (inputs[VEL_INPUT].isConnected())
				vel = clamp((int) std::round(inputs[VEL_INPUT].getPolyVoltage(rose) / 10.f * 127.f), 1, 127);
			sp.vel = vel / 127.f;
			spellNoteOn(s, cv[rose].note, rose);
		}
		bool call = params[CALL_PARAM].getValue() > 0.5f;
		if (inputs[CALL_INPUT].isConnected()) {
			float g = inputs[CALL_INPUT].getVoltage();
			call = call || (callOn ? g > 0.5f : g >= 1.f);
		}
		if (call && !callOn) {
			sp.vel = 100.f / 127.f;
			spellNoteOn(s, CALL_NOTE, -1);
		} else if (!call && callOn && sp.ch == -1)
			spellNoteOff(s);
		callOn = call;
		lights[CALL_LIGHT].setBrightness(call ? 1.f : 0.f);
		if (wordTrig.process(inputs[WORD_INPUT].getVoltage(), 0.1f, 1.f))
			sp.pos = 0, sp.notes = 0;
	}

	void processNotes(const jvo_api* a) {
		bool spl = spellLang();
		if (spl != sp.on) {  // into SPELL or out of it: the other engine let go
			sp.on = spl;
			spellOn = spl ? 1 : 0;
			if (spl)
				a->all_off();
			else if (spell.api) {
				spell.api->stop();
				spEnv.gate(false);
			}
			sp.ch = -2;
		}
		if (spl) {
			if (spell.api)
				processSpellNotes(spell.api);
			return;
		}
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

	/** SPELL's block (44.1 kHz, as the JIANT engine's): the chip, the envelope, the velocity, LEVEL, the effects. */
	void renderSpell(const jsp_api* s) {
		spellKnobs(s);
		s->render(BLOCK, spBuf);
		int adsr[4], fx[4];
		for (int i = 0; i < 4; i++) {
			adsr[i] = (int) std::round(params[ATK_PARAM + i].getValue());
			fx[i] = (int) std::round(params[DIST_PARAM + i].getValue());
		}
		float lvl = params[LEVEL_PARAM].getValue() / 127.f;
		for (int i = 0; i < BLOCK; i++) {
			float e = spEnv.process(adsr, 44100.f);
			if (sp.mode == SPM_SPEAK)
				e = 1.f;  // (it speaks to its end, whatever the gate)
			float l, r;
			spFx.process(spBuf[i] * e * sp.vel * lvl, fx, l, r);
			devL[i] += l;
			devR[i] += r;
		}
	}

	/** What the display shows of SPELL: the knobs' names and values, the word and how far it is, the lattice. */
	void spellView(const jsp_api* s) {
		for (int k = 0; k < NE; k++) {
			std::snprintf(labels[k].data(), labels[k].size(), "%s", spellLabel(k, sp.mode));
			std::snprintf(values[k].data(), values[k].size(), "%s", spellValue(k, sp.k, sp.mode, s).c_str());
		}
		bool busy = s->busy();
		std::string w;
		if (sp.mode == SPM_SING)
			w = s->text((uint32_t) sp.k.sel);
		else if (sp.mode == SPM_LETTER) {
			static const char* NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
			int L = sp.k.sel ? sp.k.sel - 1 : ((sp.note - 53) % 26 + 26) % 26;
			w = std::string(1, (char) ('A' + L));
			std::snprintf(spNote.data(), spNote.size(), "%s%d", NN[sp.note % 12], sp.note / 12 - 1);
		} else { // SPEAK: the word it says now (its marks)
			int m = (int) s->mark(), i = 0;
			std::string cur;
			for (size_t j = 0; j <= sp.said.size(); j++) {
				if (j == sp.said.size() || sp.said[j] == ' ') {
					if (++i == std::max(1, m)) {
						w = cur;
						break;
					}
					cur.clear();
				} else
					cur += sp.said[j];
			}
		}
		std::snprintf(spWord.data(), spWord.size(), "%s", w.c_str());
		int len = (int) w.size();
		spLen = len;
		spLit = !busy ? (sp.ch == -2 ? 0 : len) : sp.mode == SPM_SING ? (int) ((s->progress() * len + 128) / 256) : len;
		int32_t k[10], e = 0;
		s->frame(k, &e);
		for (int i = 0; i < 10; i++)
			spK[i] = k[i];
		spE = e;
	}

	void renderBlock(const jvo_api* a, float sampleRate) {
		processKnobs(a);
		a->render(BLOCK, devL, devR);
		if (sp.on && spell.api)
			renderSpell(spell.api);
		dsp::Frame<2> in[BLOCK];
		for (int i = 0; i < BLOCK; i++) {
			in[i].samples[0] = devL[i];
			in[i].samples[1] = devR[i];
		}
		int devRate = (int) a->sample_rate();
		src.process(in, BLOCK, devRate, (int) sampleRate, outBuf);
		framesSinceVis += BLOCK;
		if (visDue && screenClock.seen()) {             // (only while the screen is seen: it costs)
			visDue = false;
			std::memcpy(screen, a->vis(JVO_VIS_W, JVO_VIS_H), sizeof screen);
			screenSeq++;
		}
		if (framesSinceVis >= (uint32_t) devRate / 30) {     // the tract, 30 frames a second
			framesSinceVis = 0;
			visDue = true;
			for (int k = 0; k < NE; k++)
				std::snprintf(labels[k].data(), labels[k].size(), "%s", a->label(JVO_E1 + k));
			for (uint32_t j = 0; j < JVO_NP; j++)
				a->format(j, a->get(j), values[j].data(), (uint32_t) values[j].size());
			uint32_t s = 0, ns = 0;
			a->word(word.data(), (uint32_t) word.size(), &s, &ns);
			syl = (int) s;
			nsyl = (int) ns;
			voices = (int) a->active_voices();
			if (sp.on && spell.api)
				spellView(spell.api);
		}
	}

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
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
		if (outputs[R_OUTPUT].isConnected() || pulled(args.frame)) { // (pulled: MIX takes it without a cable)
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
static const char* LANG_NAMES[5] = {"OFF", "ES", "EN", "JA", "SPELL"};
static const NVGcolor VFD = nvgRGB(0x5f, 0xe0, 0xd0), VFD_OFF = nvgRGB(0x24, 0x40, 0x3d);  // SPELL's word, its lattice
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
		module->screenClock.mark();
		if (module->spellOn) { // SPELL: the chip's lattice now, ten coefficients and the energy, as bars
			NVGcontext* vg = args.vg;
			if (!module->spell.loaded()) {
				jiant::drawStatus(vg, module->spell.error, box.size.x);
				return;
			}
			int c = module->spClock;
			float khz = c <= 64 ? 4.f + c * 4.f / 64.f : 8.f + (c - 64) * 6.f / 63.f;
			jiant::text(vg, jiant::boldFont(), 4, 11, string::f("LPC \xc2\xb7 %.1f kHz", khz), 6.f, VFD, NVG_ALIGN_LEFT, 0.8f);
			float bw = (box.size.x - 12) / 11, top = 18, bot = box.size.y - 4;
			for (int i = 0; i < 11; i++) {
				float v = i < 10 ? std::fabs((float) module->spK[i]) / 16384.f : std::min(1.f, module->spE / 1600.f);
				float h = std::max(1.f, std::min(1.f, v) * (bot - top));
				jiant::roundRect(vg, 6 + i * bw, bot - h, bw - 2.5f, h, 1.f, i == 10 ? jiant::WHITE : VFD);
			}
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
		int lang = module ? clamp((int) std::round(module->params[Voice::LANG_PARAM].getValue()), 0, 4) : 1;
		bool spl = lang == Voice::LANG_SPELL;
		int spm = module ? clamp((int) std::round(module->params[Voice::SPELL_MODE_PARAM].getValue()), 0, SPM_N - 1) : 0;
		int lyr = module ? (int) std::round(module->params[Voice::LYRIC_PARAM].getValue()) : 1;
		int vo = module ? clamp((int) std::round(module->params[Voice::VOICE_PARAM].getValue()), 0, 2) : 0;
		float h = box.size.y, fy = h - 14.f;   // the lyric's field (its own widget) at the bottom
		jiant::text(vg, bold, 0, 7.f, string::f("PRESET %d / %d", p + 1, a ? module->npresets() : 18), 5.2f, jiant::DIM,
		            NVG_ALIGN_LEFT, 1.f);
		std::string mode = spl ? string::f("SPELL \xc2\xb7 %s \xc2\xb7 %s", SPM_NAMES[spm], VOICE_NAMES[vo])
		                       : string::f("LANG %s \xc2\xb7 LYRIC %s \xc2\xb7 %s", LANG_NAMES[lang], lyricName(lyr).c_str(), VOICE_NAMES[vo]);
		jiant::text(vg, bold, w, 7.f, mode, 5.2f, ACCENT, NVG_ALIGN_RIGHT, 1.f);
		jiant::text(vg, bold, 0, 19.f, a ? module->presetName(p) : "TALKING TOY", 11.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.6f);
		float ly = fy - 3.f;   // the word sung
		if (module && spl) {
			// SPELL: the word in the chip's display colour, letter by letter as it is said
			std::string wd = module->spWord.data();
			int lit = module->spLit;
			float x = 0;
			for (size_t i = 0; i < wd.size() && i < 14; i++) {
				std::string ch(1, wd[i]);
				jiant::text(vg, bold, x, ly, ch, 10.f, (int) i < lit ? VFD : VFD_OFF, NVG_ALIGN_LEFT, 0.f);
				x += nvgTextBounds(vg, 0, 0, ch.c_str(), nullptr, nullptr) + 2.f;
			}
			int cnt = module->spell.api ? (int) module->spell.api->count() : 162;
			int idx = (int) std::round(clamp(module->params[Voice::E_PARAM + 1].getValue(), 0.f, 1.f) * (cnt - 1)) + 1;
			std::string right = spm == SPM_LETTER  ? std::string(module->spNote.data())
			                    : spm == SPM_SPEAK ? string::f("WORD %d/%d", (int) module->spWordI, (int) module->spWords)
			                                       : string::f("WORD %d/%d", idx, cnt);
			jiant::text(vg, bold, w, ly, right, 5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 0.6f);
		} else if (module && module->word[0]) {
			// the word sung, its syllables as dots (lit: sung)
			std::string wd = module->word.data();
			jiant::text(vg, bold, 0, ly, wd, 9.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.4f);
			float tw = nvgTextBounds(vg, 0, 0, wd.c_str(), nullptr, nullptr);
			int ns = std::min((int) module->nsyl, 8), sy = module->syl;
			for (int i = 0; i < ns; i++) {
				nvgBeginPath(vg);
				nvgCircle(vg, tw + 7 + i * 7, ly - 3.f, 2.f);
				nvgFillColor(vg, i < sy ? ACCENT : nvgRGB(0x33, 0x34, 0x3c));
				nvgFill(vg);
			}
			jiant::text(vg, bold, w, ly, string::f("SYL %d/%d", sy, (int) module->nsyl), 5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 0.6f);
		} else if (module) {
			int nv = module->voices;
			jiant::text(vg, bold, 0, ly, string::f("%d VOICE%s", nv, nv == 1 ? "" : "S"), 5.2f, jiant::DIM, NVG_ALIGN_LEFT, 0.6f);
		}
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0, fy, w, 14, 2.2f);
		nvgFillColor(vg, jiant::FIELD);
		nvgFill(vg);
		NVGcolor edge = ACCENT;
		edge.a = lyr == 0 ? 0.6f : 0.25f;
		nvgStrokeColor(vg, edge);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		(void) mono;
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
				if (module->spellOn)  // SPELL: typing reads it out
					module->paramQuantities[Voice::SPELL_MODE_PARAM]->setValue((float) SPM_SPEAK);
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
		LyricField* lf = createWidget<LyricField>(Vec(L::INFO[0], L::INFO[1] + L::INFO[3] - 14));
		lf->box.size = Vec(L::INFO[2], 14);
		lf->module = module;
		addChild(lf);

		genra_look::PickField* pf = createWidget<genra_look::PickField>(Vec(L::PRESETF[0], L::PRESETF[1]));
		pf->box.size = Vec(L::PRESETF[2], L::PRESETF[3]);
		pf->accent = ACCENT;
		pf->count = [=]() { return module && module->core.loaded() ? module->npresets() : 1; };
		pf->name = [=](int i) -> std::string { return module && module->core.loaded() ? module->presetName(i) : "TALKING TOY"; };
		pf->current = [=]() { return module ? (int) module->shownPreset : 0; };
		pf->choose = [=](int i) {
			if (module)
				module->presetReq = i;
		};
		addChild(pf);

		genra_look::ParamField* lang = createParam<genra_look::ParamField>(Vec(L::LANGF[0], L::LANGF[1]), module, Voice::LANG_PARAM);
		lang->box.size = Vec(L::LANGF[2], L::LANGF[3]);
		lang->count = 5;
		lang->nameOf = [](int i) -> std::string { return LANG_NAMES[clamp(i, 0, 4)]; };
		lang->infoOf = [](int i) -> std::string {
			static const char* I[5] = {"vowels", "Spanish", "English", "Japanese", "LPC speech"};
			return I[clamp(i, 0, 4)];
		};
		lang->title = "Language";
		lang->undo = "change language";
		addParam(lang);
		genra_look::ParamField* spm = createParam<genra_look::ParamField>(Vec(L::SPMF[0], L::SPMF[1]), module, Voice::SPELL_MODE_PARAM);
		spm->box.size = Vec(L::SPMF[2], L::SPMF[3]);
		spm->count = SPM_N;
		spm->nameOf = [](int i) -> std::string { return SPM_NAMES[clamp(i, 0, SPM_N - 1)]; };
		spm->infoOf = [](int) -> std::string { return "LANG SPELL"; };
		spm->title = "SPELL plays";
		spm->undo = "change SPELL mode";
		addParam(spm);
		genra_look::ParamField* lyr = createParam<genra_look::ParamField>(Vec(L::LYRF[0], L::LYRF[1]), module, Voice::LYRIC_PARAM);
		lyr->box.size = Vec(L::LYRF[2], L::LYRF[3]);
		lyr->count = 25;
		lyr->nameOf = [](int i) -> std::string { return lyricName(i); };
		lyr->infoOf = [](int i) -> std::string { return i == 0 ? "the field in the display" : i <= 16 ? "word list" : "song"; };
		lyr->title = "Lyric";
		lyr->undo = "change lyric";
		addParam(lyr);

		addParam(createLightParamCentered<VCVLightBezel<jiant::AccentLight<JIANT_VOICE_RGB>>>(
		    jiant::at(L::CALL), module, Voice::CALL_PARAM, Voice::CALL_LIGHT));
		jiant::LampRow* vs = createParam<jiant::LampRow>(Vec(0, 0), module, Voice::VOICE_PARAM);
		vs->place(L::VOICE, 3);
		vs->accent = ACCENT;
		addParam(vs);
		addParam(createParamCentered<GKnob>(jiant::at(L::LEVEL), module, Voice::LEVEL_PARAM));

		static const char* defLab[NE] = {"VOWEL", "VOWEL2", "TALK", "SHIFT", "BUZZ", "BREATH", "Q", "RAND"};
		for (int k = 0; k < NE; k++) {
			Vec c = jiant::at(L::E[k]);
			if (k == 0)
				addParam(createParamCentered<GLargeKnob>(c, module, Voice::E_PARAM + k));
			else if (k < 5)
				addParam(createParamCentered<GKnob>(c, module, Voice::E_PARAM + k));
			else
				addParam(createParamCentered<GSmallKnob>(c, module, Voice::E_PARAM + k));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::E_IN[k]), module, Voice::E_INPUT + k));
			auto label = [=]() -> std::string {
				if (module && module->labels[k][0])
					return module->labels[k].data();
				return defLab[k];
			};
			genra_look::PanelText* t = new genra_look::PanelText;
			t->box = jiant::area(L::E_TEXT[k]);
			t->text = label;
			addChild(t);
			genra_look::PanelText* j = new genra_look::PanelText;
			j->box = jiant::area(L::E_INL[k]);
			j->text = label;
			j->color = jiant::DIM;
			j->size = 4.4f;
			j->spacing = 0.7f;
			addChild(j);
		}
		const float* env[4] = {L::ATK, L::DEC, L::SUS, L::REL};
		const float* fx[4] = {L::DIST, L::CHOR, L::DLY, L::REV};
		for (int i = 0; i < 4; i++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(env[i]), module, Voice::ATK_PARAM + i));
			addParam(createParamCentered<GSmallKnob>(jiant::at(fx[i]), module, Voice::DIST_PARAM + i));
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
		for (int i = 0; i < m->npresets(); i++)
			names.push_back(m->presetName(i));
		menu->addChild(createIndexSubmenuItem("Preset", names, [=]() { return (size_t) m->shownPreset; },
		                                      [=](size_t i) { m->presetReq = (int) i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Each note sings the next syllable; WORD \xe2\x86\xba starts the lyric over"));
		menu->addChild(createMenuLabel("SPELL: one voice; SING and LETTER hold while the gate does, SPEAK says the lyric's next words"));
		menu->addChild(createMenuLabel("VOICE BEND on the right bends SPELL (clock, corrupt, stutter, crush)"));
		menu->addChild(createMenuLabel("Engine: VOICE from JIANT FM by Juan Jiant (a Felucca fork), GPL-3.0"));
		menu->addChild(createMenuLabel("SPELL: SPELLBOX by pjac1810 (LPC speech, espeak-ng data), GPL-3.0"));
	}
};

}  // namespace jiant_voice

Model* modelVoice = createModel<jiant_voice::Voice, jiant_voice::VoiceWidget>("Voice");
