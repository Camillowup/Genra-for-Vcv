// SPDX-License-Identifier: GPL-3.0-only
// JIANT DRUM-X: the JIANT FM firmware's DRUM-X drum synth as a VCV Rack module, its kit of creatures drawn live.
// The engine runs unchanged in private copies of the DRUM-X core (Core.hpp): one plays the mix (with the effects),
// and one per patched lane output plays that lane alone (every other lane silent, no effects), so each output keeps
// the hi-hat's choke and the lane's level. They all get the same knobs, hits and sounds, and render in step.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "DrumCore.hpp"
#include <atomic>
#include <array>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include "JiantUI.hpp"
#include "../ui/Look.hpp"

namespace jiant_drumx {


static const int NL = JDX_NLANE;
static const int NM = 7;                       // the macros after MORPH: TUNE TONE DECAY NOISE FM ENV FOLD DRIVE
static const char* LANE_NAMES[NL] = {"KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL"};
static const char* MACRO_NAMES[NM] = {"TUNE", "TONE", "DECAY", "NOISE", "FM ENV", "FOLD", "DRIVE"};
static const char* SND_NAMES[JDX_NSND] = {"PITCH", "PMOD", "DECAY", "NOISE", "COLOR"};

/** One copy of the core with its own output: rendered a block at a time, resampled to Rack's rate. */
struct Voice {
	enum { BLOCK = 32 };
	DrumCore core;
	jiant::Resampler src;                          // (rests while the core is silent: JiantUI.hpp)
	dsp::DoubleRingBuffer<dsp::Frame<2>, 2048> outBuf;
	float l[BLOCK], r[BLOCK];

	Voice() {}
	const jdx_api* api() const { return core.api; }
	void reset() {
		outBuf.clear();
		src.reset();
	}
	void renderBlock(float sampleRate) {
		const jdx_api* a = core.api;
		a->render(BLOCK, l, r);
		dsp::Frame<2> in[BLOCK];
		for (int i = 0; i < BLOCK; i++) {
			in[i].samples[0] = l[i];
			in[i].samples[1] = r[i];
		}
		src.process(in, BLOCK, (int) a->sample_rate(), (int) sampleRate, outBuf);
	}
	dsp::Frame<2> next() { return outBuf.empty() ? dsp::Frame<2>{} : outBuf.shift(); }
};

// ------------------------------------------------------------------------------------------------ the module
struct DrumX : Module, follow::Host {
	enum ParamId {
		PREV_PARAM, NEXT_PARAM,
		MORPH_PARAM,
		ENUMS(MACRO_PARAM, NM),
		ENUMS(SND_PARAM, JDX_NSND),
		AB_PARAM,
		ENUMS(PAD_PARAM, NL),
		ENUMS(LEVEL_PARAM, NL),
		DLY_PARAM, REV_PARAM, MASTER_PARAM,
		PARAMS_LEN
	};
	enum InputId { MORPH_INPUT, VOCT_INPUT, GATE_INPUT, ENUMS(MACRO_INPUT, NM), ENUMS(TRIG_INPUT, NL), ACC_INPUT, GENRA_INPUT, INPUTS_LEN };
	enum OutputId { ENUMS(LANE_OUTPUT, NL), L_OUTPUT, R_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	Voice mix;
	std::unique_ptr<Voice> lane[NL];
	std::string status;
	bool laneFailed[NL] = {};
	uint32_t seed = 1;

	// 2.37: a lane's core is made and thrown away on a thread of its own. Making one copies the core library to a file
	// and loads it (a file copy, LoadLibrary, memory), throwing one away unloads the library and deletes the file:
	// nothing for the audio thread (it did both in process() when a LANE output was patched or unpatched: dropouts
	// with a small ASIO buffer). The audio thread only asks (laneAsk), takes the finished core (laneReady) and hands
	// back the ones it no longer needs (laneTrash); a freshly patched lane starts a few milliseconds later.
	enum { ASK_IDLE, ASK_WANTED, ASK_DONE };
	std::atomic<int> laneAsk[NL];                // audio: IDLE -> WANTED; worker: WANTED -> DONE; audio: DONE -> IDLE
	std::atomic<Voice*> laneReady[NL];           // worker -> audio (nullptr with DONE: the core could not be loaded)
	std::atomic<Voice*> laneTrash[NL * 2];       // audio -> worker: cores to unload and delete
	std::vector<uint8_t> kitScratch;             // the kit's sounds on their way from the mix to a new lane
	std::thread worker;
	std::mutex workMx;
	std::condition_variable workCv;
	bool workQuit = false;                       // (under workMx)
	std::atomic<bool> workDue{false};

	void workerRun() {
		for (;;) {
			{
				std::unique_lock<std::mutex> lk(workMx);
				// (the audio thread wakes it without taking the lock, so a wake-up can be missed: it looks again
				// every 50 ms at the latest)
				workCv.wait_for(lk, std::chrono::milliseconds(50), [&]() { return workQuit || workDue.load(); });
				if (workQuit)
					return;
			}
			workDue = false;
			for (auto& t : laneTrash)
				delete t.exchange(nullptr);          // (unloads the library, deletes its file)
			for (int l = 0; l < NL; l++) {
				if (laneAsk[l].load() != ASK_WANTED)
					continue;
				Voice* v = new Voice;
				if (!v->core.load()) {
					WARN("JIANT DRUM-X: lane core: %s", v->core.error.c_str());
					delete v;
					v = nullptr;
				}
				else
					v->api()->init(seed);
				delete laneReady[l].exchange(v);
				laneAsk[l] = ASK_DONE;
			}
		}
	}
	void workerStart() {
		if (worker.joinable())
			return;
		{
			std::lock_guard<std::mutex> lk(workMx);
			workQuit = false;
		}
		worker = std::thread([this]() { workerRun(); });
	}
	/** Ends the thread and frees every core on its way (UI thread; the module leaves the engine or is deleted). */
	void workerStop() {
		if (worker.joinable()) {
			{
				std::lock_guard<std::mutex> lk(workMx);
				workQuit = true;
			}
			workCv.notify_all();
			worker.join();
		}
		for (auto& t : laneTrash)
			delete t.exchange(nullptr);
		for (int l = 0; l < NL; l++) {
			delete laneReady[l].exchange(nullptr);
			laneAsk[l] = ASK_IDLE;
		}
	}
	/** Audio thread: a core it no longer needs goes to the worker. False: no place free right now (try again). */
	bool trash(Voice* v) {
		for (auto& t : laneTrash) {
			Voice* none = nullptr;
			if (t.compare_exchange_strong(none, v)) {
				workDue = true;
				workCv.notify_one();
				return true;
			}
		}
		return false;
	}
	~DrumX() { workerStop(); }

	// settings, kept with the patch
	int kit = 0;
	int sel = 0;                                 // the sound edited
	std::vector<uint8_t> kitData;                // the kit's sounds (as edited) from the patch
	bool fromPatch = false;

	// from the UI thread
	std::atomic<int> selReq{-1}, kitReq{-1};
	std::atomic<uint32_t> padHits{0};            // pads clicked (bit per lane)

	// for the UI thread
	uint16_t screen[JDX_VIS_W * JDX_VIS_H] = {};
	std::atomic<uint32_t> screenSeq{0};
	std::array<std::atomic<float>, NL> env;
	std::array<std::array<char, 12>, JDX_NP> values{};
	std::atomic<int> shownKit{0}, shownSel{0}, shownAB{0};

	// engine thread
	int sent[JDX_NP];
	int sndShown[JDX_NSND];
	int sndLane = -1, sndPatch = -1;
	uint32_t framesSinceVis = 0;
	bool visDue = true;                          // a frame of the screen owed (drawn when it is seen)
	jiant::ScreenClock screenClock;              // set by the ScreenWidget's draw
	dsp::SchmittTrigger trig[NL], gtrig[8];
	bool padDown[NL] = {};
	struct CvVoice { bool gate = false; };
	CvVoice cv[16];

	DrumX() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		srcName = "DRUM-X";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(GENRA_INPUT, follow::DRUMS);
		fol.drumsOnly = true;
		configButton(PREV_PARAM, "Previous kit");
		configButton(NEXT_PARAM, "Next kit");
		configParam<ValueQuantity>(MORPH_PARAM, 0.f, 127.f, 64.f, "Morph A ↔ B")->snapEnabled = true;
		configInput(MORPH_INPUT, "Morph CV (±5 V: ± half the range)");
		for (int i = 0; i < NM; i++) {
			configParam<ValueQuantity>(MACRO_PARAM + i, 0.f, 127.f, i < 4 ? 64.f : 0.f, MACRO_NAMES[i])->snapEnabled = true;
			configInput(MACRO_INPUT + i, std::string(MACRO_NAMES[i]) + " CV (±5 V: ± half the range)");
		}
		for (int k = 0; k < JDX_NSND; k++) {
			configParam(SND_PARAM + k, 0.f, 127.f, 64.f, std::string("Sound ") + SND_NAMES[k])->snapEnabled = true;
			paramQuantities[SND_PARAM + k]->randomizeEnabled = false;
		}
		configSwitch(AB_PARAM, 0.f, 1.f, 0.f, "Sound patch edited", {"A", "B"});
		for (int l = 0; l < NL; l++) {
			configButton(PAD_PARAM + l, std::string(LANE_NAMES[l]) + " (strike and edit)");
			configParam<ValueQuantity>(LEVEL_PARAM + l, 0.f, 127.f, 100.f, std::string(LANE_NAMES[l]) + " level")->snapEnabled = true;
			configInput(TRIG_INPUT + l, std::string(LANE_NAMES[l]) + " trigger (velocity: the trigger's height, 10 V full)");
			configOutput(LANE_OUTPUT + l, std::string(LANE_NAMES[l]) + " alone (dry)");
			env[l] = 0.f;
		}
		configParam<ValueQuantity>(DLY_PARAM, 0.f, 127.f, 0.f, "Delay send")->snapEnabled = true;
		configParam<ValueQuantity>(REV_PARAM, 0.f, 127.f, 20.f, "Reverb send")->snapEnabled = true;
		configParam<ValueQuantity>(MASTER_PARAM, 0.f, 127.f, 104.f, "Level")->snapEnabled = true;
		configInput(VOCT_INPUT, "V/Oct (GM drum notes: C2 kick, D2 snare, F#2 closed hat ..)");
		configInput(GATE_INPUT, "Gate (polyphonic)");
		configInput(ACC_INPUT, "Accent (high: every hit at full velocity)");
		configInput(GENRA_INPUT, "Drums from GENRA (polyphonic, GENRA's order: kick, snare, clap, rim, closed hat, open hat, tom, perc; height = velocity)");
		configOutput(L_OUTPUT, "Mix left (mono when Right is not patched)");
		configOutput(R_OUTPUT, "Mix right");
		for (int j = 0; j < PARAMS_LEN; j++)
			if (j < MORPH_PARAM || j >= SND_PARAM)
				paramQuantities[j]->randomizeEnabled = false;
		for (int& s : sent)
			s = INT32_MIN;
		for (int l = 0; l < NL; l++) {
			laneAsk[l] = ASK_IDLE;
			laneReady[l] = nullptr;
		}
		for (auto& t : laneTrash)
			t = nullptr;
	}

	/** The parameter's value as the FM-1 writes it (%, dB). */
	struct ValueQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			DrumX* m = dynamic_cast<DrumX*>(module);
			int j = m ? m->jOf(paramId) : -1;
			if (m && j >= 0 && m->mix.core.loaded() && m->values[j][0])
				return m->values[j].data();
			return ParamQuantity::getDisplayValueString();
		}
	};
	/** A knob's parameter in the core (JDX_*), or -1. */
	static int jOf(int paramId) {
		if (paramId == MORPH_PARAM)
			return JDX_MORPH;
		if (paramId >= MACRO_PARAM && paramId < MACRO_PARAM + NM)
			return JDX_TUNE + paramId - MACRO_PARAM;
		if (paramId >= LEVEL_PARAM && paramId < LEVEL_PARAM + NL)
			return JDX_LN0 + paramId - LEVEL_PARAM;
		if (paramId == DLY_PARAM)
			return JDX_DLY;
		if (paramId == REV_PARAM)
			return JDX_REV;
		if (paramId == MASTER_PARAM)
			return JDX_LEVEL;
		return -1;
	}

	// ---------------------------------------------------------------------------------- the cores
	template <class F>
	void forAll(F f) {
		if (mix.api())
			f(mix.api());
		for (auto& e : lane)
			if (e && e->api())
				f(e->api());
	}

	void powerOn() {
		for (auto& e : lane)
			e.reset();
		if (!mix.core.load()) {
			status = mix.core.error;
			WARN("JIANT DRUM-X: %s", status.c_str());
			return;
		}
		status.clear();
		seed = random::u32() | 1u;
		mix.api()->init(seed);
		mix.reset();
		kitScratch.assign(mix.api()->kit_size(), 0);
		workerStart();
		for (int& s : sent)
			s = INT32_MIN;
		{ // a patch from a version with more (or fewer) kits: stay in range
			int n = std::max(1, (int) mix.api()->nkits());
			kit = ((kit % n) + n) % n;
		}
		if (!fromPatch) {
			applyKit(kit);
		} else {
			mix.api()->load_kit((uint32_t) kit);
			if (kitData.size() == mix.api()->kit_size())
				mix.api()->kit_import(kitData.data());
		}
		sndLane = sndPatch = -1;
		shownKit = kit;
	}

	/** A lane's own core, loaded by the worker (laneReady): a copy of the mix's state, that lane alone. Audio thread;
	 * no file, no library and no memory here. */
	void laneOn(int l, Voice* v) {
		std::unique_ptr<Voice> e(v);
		const jdx_api* a = e->api();
		const jdx_api* m = mix.api();
		a->load_kit((uint32_t) kit);                  // (a->init(seed): the worker did it)
		if (kitScratch.size() == m->kit_size()) {
			m->kit_export(kitScratch.data());
			a->kit_import(kitScratch.data());
		}
		for (int j = 0; j < JDX_NP; j++)
			a->set(j, m->get(j));
		a->isolate(l);
		// in step with the mix: as many frames waiting as the mix has
		size_t have = mix.outBuf.size();
		while (e->outBuf.size() < have)
			e->outBuf.push(dsp::Frame<2>{});
		lane[l] = std::move(e);
	}
	/** The lane outputs' cores: on when patched, off when not (audio thread). */
	void processLanes() {
		for (int l = 0; l < NL; l++) {
			bool on = outputs[LANE_OUTPUT + l].isConnected();
			int ask = laneAsk[l].load(std::memory_order_acquire);
			if (ask == ASK_DONE) {                    // the worker is through: take the core, or note that it failed
				Voice* v = laneReady[l].exchange(nullptr);
				if (v && on && !lane[l])
					laneOn(l, v);
				else if (v && !trash(v)) {            // unpatched meanwhile: back to the worker
					laneReady[l] = v;                 // (no place free: again in the next sample)
					continue;
				}
				else if (!v)
					laneFailed[l] = true;             // not again every sample; retried after unplugging
				laneAsk[l].store(ASK_IDLE, std::memory_order_release);
				ask = ASK_IDLE;
			}
			if (on) {
				if (!lane[l] && !laneFailed[l] && ask == ASK_IDLE) {
					laneAsk[l].store(ASK_WANTED, std::memory_order_release);
					workDue = true;
					workCv.notify_one();
				}
			}
			else {
				laneFailed[l] = false;
				if (lane[l] && trash(lane[l].get()))  // (no place free: it plays on unheard, again in the next sample)
					lane[l].release();
			}
		}
	}

	/** The factory kit into the knobs (and every core). */
	void applyKit(int i) {
		const jdx_api* m = mix.api();
		if (!m)
			return;
		int n = (int) m->nkits();
		kit = ((i % n) + n) % n;
		forAll([&](const jdx_api* a) { a->load_kit((uint32_t) kit); });
		params[MORPH_PARAM].setValue((float) m->get(JDX_MORPH));
		for (int i2 = 0; i2 < NM; i2++)
			params[MACRO_PARAM + i2].setValue((float) m->get(JDX_TUNE + i2));
		for (int l = 0; l < NL; l++)
			params[LEVEL_PARAM + l].setValue((float) m->get(JDX_LN0 + l));
		params[DLY_PARAM].setValue((float) m->get(JDX_DLY));
		params[REV_PARAM].setValue((float) m->get(JDX_REV));
		for (int& s : sent)
			s = INT32_MIN;
		sndLane = sndPatch = -1;                 // the sound knobs read the new kit
		shownKit = kit;
	}

	void onAdd(const AddEvent& e) override {
		params[PREV_PARAM].setValue(0.f);
		params[NEXT_PARAM].setValue(0.f);
		for (int l = 0; l < NL; l++)
			params[PAD_PARAM + l].setValue(0.f);
		powerOn();
	}
	void onRemove(const RemoveEvent& e) override { follow::gone(this);
		workerStop();
		for (auto& l : lane)
			l.reset();
		mix.core.unload();
	}
	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		fromPatch = false;
		sel = 0;
		kitReq = 0;
	}

	// kits the followed GENRA may switch (NEW: kits have no style, any other one; UI thread)
	int hostPresetCount() override { return mix.api() ? (int) mix.api()->nkits() : 0; }
	void hostPresetInfo(int i, char* name, int nl, char* style, int sl) override {
		follow::putStr(name, nl, mix.api() ? mix.api()->kit_name((uint32_t) i) : "");
		follow::putStr(style, sl, "");
	}
	int hostPresetCurrent() override { return shownKit.load(); }
	void hostPresetLoad(int i) override { kitReq = i; }

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "kit", json_integer(kit));
		json_object_set_new(r, "sel", json_integer(sel));
		if (mix.api()) {                         // the sounds as edited, hex
			std::vector<uint8_t> k(mix.api()->kit_size());
			mix.api()->kit_export(k.data());
			std::string hx;
			for (uint8_t b : k)
				hx += string::f("%02x", b);
			json_object_set_new(r, "sounds", json_string(hx.c_str()));
		}
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		if (json_t* j = json_object_get(r, "kit"))
			kit = (int) json_integer_value(j);
		if (json_t* j = json_object_get(r, "sel"))
			sel = clamp((int) json_integer_value(j), 0, NL - 1);
		kitData.clear();
		if (json_t* j = json_object_get(r, "sounds")) {
			const char* s = json_string_value(j);
			for (size_t i = 0; s && s[i] && s[i + 1]; i += 2) {
				unsigned v = 0;
				std::sscanf(s + i, "%2x", &v);
				kitData.push_back((uint8_t) v);
			}
		}
		fromPatch = true;
		if (mix.api()) { // a preset loaded onto (or pasted into) a running module: Rack holds the engine lock here
			int n = std::max(1, (int) mix.api()->nkits());
			kit = ((kit % n) + n) % n;
			mix.api()->load_kit((uint32_t) kit);
			if (kitData.size() == mix.api()->kit_size())
				mix.api()->kit_import(kitData.data());
			for (auto& e : lane) // the lanes are copies of the mix: made anew
				e.reset();
			for (int& s : sent)
				s = INT32_MIN;   // the knobs' values follow in the next block
			sndLane = sndPatch = -1;
			shownKit = kit;
		}
	}

	// ---------------------------------------------------------------------------------- the engine
	void push(int j, int v) {
		if (sent[j] != v) {
			forAll([&](const jdx_api* a) { a->set(j, v); });
			sent[j] = v;
		}
	}
	int knobCv(int param, int input) {
		float v = params[param].getValue();
		if (inputs[input].isConnected())
			v += inputs[input].getVoltage() / 10.f * 127.f;
		return clamp((int) std::round(v), 0, 127);
	}

	void processKnobs() {
		push(JDX_MORPH, knobCv(MORPH_PARAM, MORPH_INPUT));
		for (int i = 0; i < NM; i++)
			push(JDX_TUNE + i, knobCv(MACRO_PARAM + i, MACRO_INPUT + i));
		for (int l = 0; l < NL; l++)
			push(JDX_LN0 + l, (int) std::round(params[LEVEL_PARAM + l].getValue()));
		push(JDX_DLY, (int) std::round(params[DLY_PARAM].getValue()));
		push(JDX_REV, (int) std::round(params[REV_PARAM].getValue()));
		push(JDX_LEVEL, (int) std::round(params[MASTER_PARAM].getValue()));

		// the sound edited: the knobs show it when it changes, and write it when they move
		const jdx_api* m = mix.api();
		int patch = params[AB_PARAM].getValue() > 0.5f ? 1 : 0;
		if (sel != sndLane || patch != sndPatch) {
			sndLane = sel;
			sndPatch = patch;
			for (int k = 0; k < JDX_NSND; k++) {
				sndShown[k] = m->snd_get(sel, patch, k);
				params[SND_PARAM + k].setValue((float) sndShown[k]);
			}
		}
		for (int k = 0; k < JDX_NSND; k++) {
			int v = (int) std::round(params[SND_PARAM + k].getValue());
			if (v != sndShown[k]) {
				sndShown[k] = v;
				forAll([&](const jdx_api* a) { a->snd_set(sel, patch, k, v); });
			}
		}
		shownSel = sel;
		shownAB = patch;
	}

	int accent(int vel) {
		if (inputs[ACC_INPUT].isConnected() && inputs[ACC_INPUT].getVoltage() >= 1.f)
			return 127;
		return vel;
	}

	void processHits() {
		uint32_t pads = padHits.exchange(0);
		int cvHit[NL] = {}; // TRIG jack and GENRA's DRUMS cable on the same lane in the same sample: one hit (the louder)
		for (int l = 0; l < NL; l++) {
			bool down = params[PAD_PARAM + l].getValue() > 0.5f;
			bool hit = (down && !padDown[l]) || ((pads >> l) & 1u);
			padDown[l] = down;
			if (hit) {
				int v = accent(100);
				forAll([&](const jdx_api* a) { a->hit(l, v); });
			}
			float g = inputs[TRIG_INPUT + l].getVoltage();    // (0 V when unpatched: the trigger sees it low)
			if (trig[l].process(g, 0.1f, 0.5f))
				cvHit[l] = std::max(cvHit[l], clamp((int) std::round(g * 12.7f), 1, 127));
		}
		// GENRA's DRUMS cable: its 8 tracks in GENRA's order onto the lanes (rim and the hats sit elsewhere here)
		if (inputs[GENRA_INPUT].isConnected()) {
			static const int TO_LANE[8] = {0, 1, 2, 6, 3, 4, 5, 7};
			int gc = std::min(8, inputs[GENRA_INPUT].getChannels());
			for (int c = 0; c < gc; c++) {
				float g = inputs[GENRA_INPUT].getPolyVoltage(c);
				if (gtrig[c].process(g, 0.1f, 0.5f)) {
					int l = TO_LANE[c];
					cvHit[l] = std::max(cvHit[l], clamp((int) std::round(g * 12.7f), 1, 127));
				}
			}
		}
		for (int l = 0; l < NL; l++)
			if (cvHit[l]) {
				int v = accent(cvHit[l]);
				forAll([&](const jdx_api* a) { a->hit(l, v); });
			}
		int n = inputs[GATE_INPUT].isConnected() ? std::max(1, inputs[GATE_INPUT].getChannels()) : 0;
		for (int c = 0; c < 16; c++) {
			bool on = false;
			if (c < n) {
				float g = inputs[GATE_INPUT].getPolyVoltage(c);
				on = cv[c].gate ? g > 0.5f : g >= 1.f;
			}
			if (on && !cv[c].gate) {
				int note = clamp((int) std::round(inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f) + 60, 0, 127);
				int v = accent(100);
				forAll([&](const jdx_api* a) { a->note_on(note, v); });
			}
			cv[c].gate = on;
		}
	}

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		const jdx_api* m = mix.api();
		if (!m) {
			for (int o = 0; o < OUTPUTS_LEN; o++)
				outputs[o].setVoltage(0.f);
			return;
		}
		int s = selReq.exchange(-1);
		if (s >= 0)
			sel = s;
		int req = kitReq.exchange(-1);
		bool want = req >= 0;
		if (prevTrig.process(params[PREV_PARAM].getValue() > 0.5f)) {
			req = kit - 1;
			want = true;
		}
		if (nextTrig.process(params[NEXT_PARAM].getValue() > 0.5f)) {
			req = kit + 1;
			want = true;
		}
		if (want) {
			fromPatch = false;
			applyKit(req);
		}
		processLanes(); // the lane outputs' cores: on when patched, off when not (made and freed on the worker thread)
		processHits();
		if (mix.outBuf.empty()) {
			processKnobs();
			int guard = 0;
			while (mix.outBuf.empty() && guard++ < 8) {
				mix.renderBlock(args.sampleRate);
				for (auto& e : lane)
					if (e && e->api())
						e->renderBlock(args.sampleRate);
			}
			framesSinceVis += Voice::BLOCK * guard;
			if (framesSinceVis >= m->sample_rate() / 30) {   // the kit, 30 frames a second
				framesSinceVis = 0;
				visDue = true;
				for (int l = 0; l < NL; l++)
					env[l] = m->lane_env(l);
				for (int j = 0; j < JDX_NP; j++)
					m->format(j, m->get(j), values[j].data(), (uint32_t) values[j].size());
			}
			if (visDue && screenClock.seen()) {         // (only while the screen is seen: it costs)
				visDue = false;
				std::memcpy(screen, m->vis(JDX_VIS_W, JDX_VIS_H, sel), sizeof screen);
				screenSeq++;
			}
		}
		dsp::Frame<2> f = mix.next();
		float l = f.samples[0] * 5.f, r = f.samples[1] * 5.f;
		if (outputs[R_OUTPUT].isConnected() || pulled(args.frame)) { // (pulled: MIX takes it without a cable)
			outputs[L_OUTPUT].setVoltage(l);
			outputs[R_OUTPUT].setVoltage(r);
		} else {
			outputs[L_OUTPUT].setVoltage(0.5f * (l + r));
			outputs[R_OUTPUT].setVoltage(0.f);
		}
		for (int i = 0; i < NL; i++) {
			if (lane[i] && lane[i]->api()) {
				dsp::Frame<2> g = lane[i]->next();
				outputs[LANE_OUTPUT + i].setVoltage(2.5f * (g.samples[0] + g.samples[1]));
			} else {
				outputs[LANE_OUTPUT + i].setVoltage(0.f);
			}
		}
	}

	dsp::BooleanTrigger prevTrig, nextTrig;
};

// ------------------------------------------------------------------------------------------------ the panel (GENRA look)
namespace L = jlayout::drumx;
static const NVGcolor ACCENT = nvgRGB(JIANT_DRUMX_RGB);

/** The kit's creatures (the firmware's own drawing), drawn in the light layer. */
struct ScreenWidget : Widget {
	DrumX* module = nullptr;
	jiant::Canvas565 canvas{JDX_VIS_W, JDX_VIS_H};
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		if (!module) {
			jiant::text(args.vg, jiant::boldFont(), box.size.x / 2, box.size.y / 2 + 5, "DRUM-X", 14.f, ACCENT,
			            NVG_ALIGN_CENTER, 2.f);
			return;
		}
		if (!module->status.empty()) {
			jiant::drawStatus(args.vg, module->status, box.size.x);
			return;
		}
		module->screenClock.mark();
		canvas.draw(args.vg, module->screen, module->screenSeq, box.size.x, box.size.y);
	}
};

/** Right of the creatures: the kit, MORPH and the sound edited, the lanes (chips, their own widget), the macros. */
struct InfoWidget : Widget {
	DrumX* module = nullptr;
	std::string v(int j) { return module && module->mix.core.loaded() ? module->values[j].data() : "-"; }
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		NVGcontext* vg = args.vg;
		auto bold = jiant::boldFont(), mono = jiant::monoFont();
		float w = box.size.x;
		const jdx_api* a = module ? module->mix.api() : nullptr;
		int k = module ? (int) module->shownKit : 0;
		int s = module ? (int) module->shownSel : 0, ab = module ? (int) module->shownAB : 0;
		float h = box.size.y;
		jiant::text(vg, bold, 0, 7.f, string::f("KIT %d / %u", k + 1, a ? a->nkits() : 9u), 5.2f, jiant::DIM, NVG_ALIGN_LEFT, 1.f);
		jiant::text(vg, bold, w, 7.f, string::f("MORPH %s \xc2\xb7 SOUND %s %s", v(JDX_MORPH).c_str(), LANE_NAMES[s], ab ? "B" : "A"),
		            5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 1.f);
		jiant::text(vg, bold, 0, h * 0.64f, a ? a->kit_name((uint32_t) k) : "DRUM-X", 12.f, jiant::WHITE, NVG_ALIGN_LEFT, 0.6f);
		jiant::text(vg, bold, 0, h - 1.f, string::f("TUNE %s \xc2\xb7 TONE %s \xc2\xb7 DECAY %s", v(JDX_TUNE).c_str(),
		                                           v(JDX_TONE).c_str(), v(JDX_DECAY).c_str()),
		            5.2f, ACCENT, NVG_ALIGN_LEFT, 0.8f);
		jiant::text(vg, bold, w, h - 1.f, string::f("DLY %s \xc2\xb7 REV %s \xc2\xb7 LVL %s", v(JDX_DLY).c_str(), v(JDX_REV).c_str(),
		                                           v(JDX_LEVEL).c_str()),
		            5.2f, jiant::DIM, NVG_ALIGN_RIGHT, 0.8f);
		(void) mono;
	}
};

/** SOUND · the lane edited, and A | B (a click flips it). */
struct SoundHead : OpaqueWidget {
	DrumX* module = nullptr;
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module && e.pos.x > box.size.x - 42 && e.pos.x < box.size.x - 8) {
			engine::ParamQuantity* pq = module->paramQuantities[DrumX::AB_PARAM];
			pq->setValue(e.pos.x > box.size.x - 25 ? 1.f : 0.f);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {
		NVGcontext* vg = args.vg;
		int s = module ? (int) module->shownSel : 0, ab = module ? (int) module->shownAB : 0;
		jiant::text(vg, jiant::boldFont(), box.size.x / 2, box.size.y / 2 + 2.6f, string::f("SOUND \xc2\xb7 %s", LANE_NAMES[s]),
		            6.f, ACCENT, NVG_ALIGN_CENTER, 2.2f);
		jiant::chip(vg, box.size.x - 40, 0, 14, box.size.y, "A", ab == 0, ACCENT, 5.f);
		jiant::chip(vg, box.size.x - 24, 0, 14, box.size.y, "B", ab == 1, ACCENT, 5.f);
	}
};

/** A lane's pad: strikes it (and picks it for SOUND); lit by its hit. */
struct Pad : app::Switch {
	DrumX* module = nullptr;
	int l = 0;
	Pad() { momentary = true; }
	void onDragStart(const DragStartEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT && module)
			module->selReq = l;
		Switch::onDragStart(e);
	}
	void face(NVGcontext* vg, float lit) {
		bool sel = module ? module->shownSel == l : l == 0;
		jiant::roundRect(vg, 0, 0, box.size.x, box.size.y, 2.2f, nvgLerpRGBA(nvgRGB(0x33, 0x35, 0x3e), ACCENT, lit));
		nvgBeginPath(vg);
		nvgRoundedRect(vg, 0.4f, 0.4f, box.size.x - 0.8f, box.size.y - 0.8f, 2.f);
		nvgStrokeColor(vg, sel ? jiant::WHITE : nvgRGB(0x0d, 0x0e, 0x11));
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		jiant::text(vg, jiant::boldFont(), box.size.x / 2, box.size.y / 2 + 2.1f, LANE_NAMES[l], 5.6f,
		            lit > 0.5f ? jiant::INK : (sel ? jiant::WHITE : jiant::TXT), NVG_ALIGN_CENTER, 0.4f);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1)
			face(args.vg, module ? std::sqrt(clamp((float) module->env[l], 0.f, 1.f)) : 0.f);
		Switch::drawLayer(args, layer);
	}
};

struct DrumXWidget : ModuleWidget {
	DrumXWidget(DrumX* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/JiantDrumX.svg")));
		follow::addStrip(this, module, {jiant::at(L::GENRA_IN)}); // lit while GENRA feeds the strip
		jiant::addScrews(this);

		ScreenWidget* scr = new ScreenWidget;
		scr->module = module;
		scr->box = jiant::area(L::SCREEN);
		addChild(scr);
		InfoWidget* info = new InfoWidget;
		info->module = module;
		info->box = jiant::area(L::INFO);
		addChild(info);
		genra_look::PickField* pf = createWidget<genra_look::PickField>(Vec(L::PRESETF[0], L::PRESETF[1]));
		pf->box.size = Vec(L::PRESETF[2], L::PRESETF[3]);
		pf->accent = ACCENT;
		pf->title = "Kit";
		pf->count = [=]() { return module && module->mix.api() ? (int) module->mix.api()->nkits() : 1; };
		pf->name = [=](int i) -> std::string {
			return module && module->mix.api() ? module->mix.api()->kit_name((uint32_t) i) : "909 CLUB";
		};
		pf->current = [=]() { return module ? (int) module->shownKit : 1; };
		pf->choose = [=](int i) {
			if (module)
				module->kitReq = i;
		};
		addChild(pf);

		SoundHead* sh = new SoundHead;
		sh->module = module;
		sh->box = jiant::area(L::SOUND_HEAD);
		addChild(sh);
		for (int k = 0; k < JDX_NSND; k++) {
			if (k == 0)
				addParam(createParamCentered<GLargeKnob>(jiant::at(L::SND[k]), module, DrumX::SND_PARAM + k));
			else
				addParam(createParamCentered<GKnob>(jiant::at(L::SND[k]), module, DrumX::SND_PARAM + k));
		}
		addParam(createParamCentered<GLargeKnob>(jiant::at(L::MORPH), module, DrumX::MORPH_PARAM));
		addParam(createParamCentered<GKnob>(jiant::at(L::DLY), module, DrumX::DLY_PARAM));
		addParam(createParamCentered<GKnob>(jiant::at(L::REV), module, DrumX::REV_PARAM));
		addParam(createParamCentered<GKnob>(jiant::at(L::MASTER), module, DrumX::MASTER_PARAM));
		for (int i = 0; i < NM; i++) {
			addParam(createParamCentered<GSmallKnob>(jiant::at(L::MACRO[i]), module, DrumX::MACRO_PARAM + i));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::MACRO_IN[i]), module, DrumX::MACRO_INPUT + i));
		}
		// the lanes: a pad (strike, pick for SOUND), its level and its trigger; the outputs at the bottom
		for (int l = 0; l < NL; l++) {
			Pad* p = createParam<Pad>(Vec(L::PAD[l][0], L::PAD[l][1]), module, DrumX::PAD_PARAM + l);
			p->box.size = Vec(L::PAD[l][2], L::PAD[l][3]);
			p->module = module;
			p->l = l;
			addParam(p);
			addParam(createParamCentered<GTrimpot>(jiant::at(L::LEVEL[l]), module, DrumX::LEVEL_PARAM + l));
			addInput(createInputCentered<PJ301MPort>(jiant::at(L::TRIG[l]), module, DrumX::TRIG_INPUT + l));
			addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::LANE_OUT[l]), module, DrumX::LANE_OUTPUT + l));
		}
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::GENRA_IN), module, DrumX::GENRA_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::VOCT), module, DrumX::VOCT_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::GATE), module, DrumX::GATE_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::ACC), module, DrumX::ACC_INPUT));
		addInput(createInputCentered<PJ301MPort>(jiant::at(L::MORPH_IN), module, DrumX::MORPH_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_L), module, DrumX::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(jiant::at(L::OUT_R), module, DrumX::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		DrumX* m = getModule<DrumX>();
		if (!m || !m->mix.core.loaded())
			return;
		menu->addChild(new MenuSeparator);
		std::vector<std::string> names;
		for (uint32_t i = 0; i < m->mix.api()->nkits(); i++)
			names.push_back(m->mix.api()->kit_name(i));
		menu->addChild(createIndexSubmenuItem("Kit", names, [=]() { return (size_t) m->shownKit; },
		                                      [=](size_t i) { m->kitReq = (int) i; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Engine: DRUM-X from JIANT FM by Juan Jiant (a Felucca fork), GPL-3.0"));
	}
};

}  // namespace jiant_drumx

Model* modelDrumX = createModel<jiant_drumx::DrumX, jiant_drumx::DrumXWidget>("DrumX");
