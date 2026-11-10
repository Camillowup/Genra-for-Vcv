// SPDX-License-Identifier: GPL-3.0-only
// SCHLAG – a sample drum machine for GENRA's drum track, after the Polyend Play.
// Eight tracks in GENRA's order. Each plays WAV samples from a folder of its own; RANDOM rolls a sample for every step
// (as the Play's randomizer does), BASIC goes back to the track's base sample. Four knobs per track take one of 18
// functions each (dropdowns: level, pan, tune, start/end, attack/decay, sends, filter, drive, bits, repeat, chance,
// action). The hits come from GENRA (Follow GENRA: DRUMS, CLOCK, RESET without cables), the TRIG jacks, V/OCT + GATE
// (General MIDI) or the MIDI input. PRINT keeps the rolled samples with the ABLAUF pad GENRA plays.
// The sound parts: SchlagDsp.hpp. Panel: tools/schlag_panel.py.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../GenraModule.hpp"
#include "../OutLimit.hpp"
#include "../ui/GenraUI.hpp"
#include "SchlagDsp.hpp"
#include "SchlagLayout.hpp"
#include "SchlagLink.hpp"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace schlag {

namespace L = glayout;
namespace gui = genra_ui;

static const NVGcolor ACCENT = nvgRGB(0x7b, 0x6c, 0xff); // the drum track's colour (as DRUM-X)
static const NVGcolor ROLLED = nvgRGB(0xff, 0xd3, 0x4d); // steps with a rolled sample
static const NVGcolor FXCOL = nvgRGB(0x8f, 0xd3, 0xff);  // GENRA FX
static const int NUM_SLOTS = 128;                         // ABLAUF: 4 banks of 32 pads

// ------------------------------------------------------------------------------------------------ settings
// The sample root folder: one per computer (not per patch), in Rack's user folder.
static std::string& rootDir() {
	static std::string r;
	return r;
}
static std::string settingsPath() { return asset::user("Genra/schlag.json"); }
static void loadSettings() {
	static bool done = false;
	if (done)
		return;
	done = true;
	json_t* j = json_load_file(settingsPath().c_str(), 0, nullptr);
	if (!j)
		return;
	if (json_t* r = json_object_get(j, "sampleRoot"))
		rootDir() = json_string_value(r) ? json_string_value(r) : "";
	json_decref(j);
}
static void saveSettings() {
	system::createDirectories(asset::user("Genra"));
	json_t* j = json_object();
	json_object_set_new(j, "sampleRoot", json_string(rootDir().c_str()));
	json_dump_file(j, settingsPath().c_str(), JSON_INDENT(2));
	json_decref(j);
}
static std::string slashes(std::string s) {
	for (char& c : s)
		if (c == '\\')
			c = '/';
	while (s.size() > 1 && s.back() == '/')
		s.pop_back();
	return s;
}
/** A folder relative to the root ("" when it is not inside the root). */
static std::string relOf(const std::string& abs) {
	std::string r = slashes(rootDir()), a = slashes(abs);
	if (r.empty() || a.size() <= r.size() || string::lowercase(a.substr(0, r.size())) != string::lowercase(r) ||
	    a[r.size()] != '/')
		return "";
	return a.substr(r.size() + 1);
}
static std::string kitDir() { return asset::user("Genra/SCHLAG kits"); }
static bool isDir(const std::string& p) {
	try {
		return !p.empty() && system::isDirectory(p);
	}
	catch (std::exception& e) {
		return false;
	}
}
static bool isFileP(const std::string& p) {
	try {
		return system::isFile(p);
	}
	catch (std::exception& e) {
		return false;
	}
}
/** The entries of a folder; none when it cannot be read (no rights, gone, a drive not there). */
static std::vector<std::string> entries(const std::string& dir) {
	try {
		return system::getEntries(dir);
	}
	catch (std::exception& e) {
		return {};
	}
}
static bool isWav(const std::string& p) { return string::lowercase(system::getExtension(p)) == ".wav"; }
static bool lessName(const std::string& a, const std::string& b) {
	return string::lowercase(system::getFilename(a)) < string::lowercase(system::getFilename(b));
}
/** The WAV files of a folder (not its sub folders), sorted by name. */
static std::vector<std::string> wavFiles(const std::string& dir) {
	std::vector<std::string> v;
	if (dir.empty() || !isDir(dir))
		return v;
	for (const std::string& e : entries(dir))
		if (isWav(e) && isFileP(e))
			v.push_back(e);
	std::sort(v.begin(), v.end(), lessName);
	return v;
}

// ------------------------------------------------------------------------------------------------ a track's folder
struct Bank {
	int track = 0;
	uint32_t gen = 0;                              // the request it answers
	std::string dir;                               // absolute
	std::vector<std::shared_ptr<Sample>> files;    // nullptr: could not be read
	std::vector<std::string> names;                // file names (with extension)
	std::string error;                             // "" = fine
	int count() const { return (int) files.size(); }
	int find(const std::string& name) const {
		for (int i = 0; i < count(); i++)
			if (names[i] == name)
				return i;
		std::string lo = string::lowercase(name);
		for (int i = 0; i < count(); i++)
			if (string::lowercase(names[i]) == lo)
				return i;
		return -1;
	}
};
static const int MAX_FILES = 512;

/** Reads folders on a thread of its own (a folder of samples takes a while). */
struct Loader {
	struct Job {
		int track;
		uint32_t gen;
		std::string dir;
	};
	std::thread th;
	std::mutex mx;
	std::condition_variable cv;
	std::deque<Job> jobs;
	std::vector<Bank*> done;
	bool quit = false;
	bool started = false;

	void request(int track, uint32_t gen, const std::string& dir) {
		{
			std::lock_guard<std::mutex> lk(mx);
			for (auto it = jobs.begin(); it != jobs.end();)
				it = it->track == track ? jobs.erase(it) : it + 1;
			jobs.push_back({track, gen, dir});
			if (!started) {
				started = true;
				th = std::thread([this]() { run(); });
			}
		}
		cv.notify_one();
	}
	void run() {
		for (;;) {
			Job j;
			{
				std::unique_lock<std::mutex> lk(mx);
				cv.wait(lk, [&]() { return quit || !jobs.empty(); });
				if (quit)
					return;
				j = jobs.front();
				jobs.pop_front();
			}
			Bank* b = new Bank;
			b->track = j.track;
			b->gen = j.gen;
			b->dir = j.dir;
			if (j.dir.empty())
				b->error = "NO FOLDER";
			else if (!isDir(j.dir))
				b->error = "MISSING";
			else {
				std::vector<std::string> files = wavFiles(j.dir);
				if ((int) files.size() > MAX_FILES)
					files.resize(MAX_FILES);
				for (const std::string& p : files) {
					{
						std::lock_guard<std::mutex> lk(mx);
						if (quit)
							break;
					}
					std::string err;
					std::shared_ptr<Sample> s;
					try {
						// the size first: a multi-GB recording in the folder is skipped, not read into memory
						if (system::getFileSize(p) < 512ull * 1024 * 1024) {
							std::vector<uint8_t> data = system::readFile(p);
							s = readWavData(data, err);
						}
					}
					catch (std::exception& e) {
						s = nullptr;
					}
					if (s)
						s->name = system::getFilename(p);
					b->files.push_back(s);
					b->names.push_back(system::getFilename(p));
				}
				if (b->files.empty())
					b->error = "NO WAV FILES";
			}
			std::lock_guard<std::mutex> lk(mx);
			done.push_back(b);
		}
	}
	std::vector<Bank*> take() {
		std::lock_guard<std::mutex> lk(mx);
		std::vector<Bank*> v;
		v.swap(done);
		return v;
	}
	~Loader() {
		{
			std::lock_guard<std::mutex> lk(mx);
			quit = true;
		}
		cv.notify_one();
		if (th.joinable())
			th.join();
		for (Bank* b : done)
			delete b;
	}
};

// ------------------------------------------------------------------------------------------------ the module
struct Schlag : Module, follow::Host {
	enum ParamId {
		ENUMS(SAMPLE_PARAM, NT),
		ENUMS(RAND_PARAM, NT),
		ENUMS(BASIC_PARAM, NT),
		ENUMS(MUTE_PARAM, NT),
		ENUMS(SOLO_PARAM, NT),
		ENUMS(K_PARAM, NT * NROW),   // track * NROW + row
		ENUMS(FN_PARAM, NT * NROW),  // the knob's function (Fn)
		CHOKE_PARAM,
		ACCENT_PARAM,
		LEVEL_PARAM,
		RNDALL_PARAM,
		UNDO_PARAM,
		BASICALL_PARAM,
		PRINT_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		ENUMS(TRIG_INPUT, NT),
		DRUMS_INPUT, VOCT_INPUT, GATE_INPUT, VEL_INPUT, ACC_INPUT, CLOCK_INPUT, RESET_INPUT,
		RND_INPUT, KIT_INPUT,
		ENUMS(CV_INPUT, NROW),
		INPUTS_LEN
	};
	enum OutputId { ENUMS(OUT_OUTPUT, NT), L_OUTPUT, R_OUTPUT, SENDA_L_OUTPUT, SENDA_R_OUTPUT, SENDB_L_OUTPUT, SENDB_R_OUTPUT, OUTPUTS_LEN };
	enum LightId { LIGHTS_LEN };

	// ---- the folders (UI thread owns these; the audio thread reads bank[])
	std::atomic<Bank*> bank[NT];
	std::string folder[NT];        // absolute path of the track's folder
	uint32_t gen[NT] = {};         // load requests made
	Loader loader;
	struct Retired {
		Bank* b;
		double at;
	};
	std::vector<Retired> retired;
	/** What to set once a folder has been read (from the patch, a kit or the file window): names, not indices. */
	struct Pending {
		bool on = false;
		std::string base;                        // the base sample's file name
		std::vector<std::string> names;          // the names the maps refer to
		std::vector<int16_t> map;                // MAXS entries: index into names, -1 base
		std::vector<std::pair<int, std::vector<int16_t>>> prints; // (slot, map)
	};
	Pending pending[NT];

	// ---- the rolled samples: per track and step an index into the folder, -1 = the base sample
	int16_t map[NT][MAXS];
	// PRINT: the maps kept with ABLAUF pads (pad = GENRA's slot index)
	std::atomic<bool> printHas[NUM_SLOTS];
	int16_t printMap[NUM_SLOTS][NT][MAXS];

	// ---- the knob functions: the value of every function of every track (the knobs show the assigned ones)
	float fnVal[NT][NUM_FN];
	int curFn[NT][NROW];            // audio thread: the function a knob showed last
	float val[NT][NUM_FN];          // audio thread: the values in use (+ CV)

	// ---- MIDI
	midi::InputQueue midiIn;

	// ---- settings kept with the patch
	std::string kitName = "INIT";
	std::atomic<bool> sendAuto{true}; // GENRA FX takes SEND A / B without cables

	// ---- audio thread
	struct Track {
		Voice v, tail;           // the voice and the one fading out after a retrigger
		struct Rep {
			bool on = false;
			int type = 0, n = 0, k = 0, idx = -1;
			float interval = 0.f, timer = 0.f;
			HitParams hp;
			uint32_t seed = 1;
		} rep;
		struct Delayed {
			bool on = false;
			float timer = 0.f;
			int idx = -1;
			HitParams hp;
			int repType = 0, repGrid = 0;
		} delayed;
		int hitCount = 0;
	} tr[NT];
	Rng rng;
	dsp::SchmittTrigger trigIn[NT], drumIn[NT], clockTrig, resetTrig, rndTrig[NT], kitTrig;
	bool gateWas[16] = {};
	int clkStep = -1;
	float clkPeriod = 0.f, sinceClk = 1e9f;
	int lastStep = -1;
	int64_t loopCount = 0;
	int lastSlot = -2;
	int ctl = 0;
	float sr = 48000.f;

	// ---- for the panel (audio -> UI)
	std::atomic<int> stepPub{-1};
	std::atomic<int> lastIdx[NT];       // the sample last played per track (-1 none)
	std::atomic<float> hitFlash[NT];
	uint32_t hitLoop[NT][MAXS];         // loopCount + 1 of the last hit on a step (0 never)
	std::atomic<uint32_t> rndMask{0};   // RND jack: tracks to roll (UI does it)
	std::atomic<int> kitNext{0};        // KIT jack: next kit
	std::atomic<int64_t> sendPulledAt[2];
	int64_t frameNow = 0;
	bool sendPulled(int k) const { return frameNow - sendPulledAt[k].load(std::memory_order_relaxed) < 4096; }

	/** The knob's label and value as the function it has. */
	struct KnobQuantity : ParamQuantity {
		int fnNow() {
			Schlag* m = dynamic_cast<Schlag*>(module);
			if (!m)
				return 0;
			int k = paramId - K_PARAM;
			return clamp((int) std::round(m->params[FN_PARAM + k].getValue()), 0, NUM_FN - 1);
		}
		std::string getLabel() override {
			int k = paramId - K_PARAM;
			return std::string(TRACK_NAMES[k / NROW]) + " · " + FN[fnNow()].name;
		}
		std::string getDisplayValueString() override { return fnText(fnNow(), getValue()); }
		std::string getUnit() override { return ""; }
	};
	/** SAMPLE: the base sample's name. */
	struct SampleQuantity : ParamQuantity {
		std::string getDisplayValueString() override {
			Schlag* m = dynamic_cast<Schlag*>(module);
			if (!m)
				return "";
			int t = paramId - SAMPLE_PARAM;
			Bank* b = m->bank[t].load();
			if (!b || !b->count())
				return b ? b->error : "no folder";
			int i = m->baseIndex(t, b);
			return string::f("%d / %d  %s", i + 1, b->count(), b->names[i].c_str());
		}
		std::string getUnit() override { return ""; }
	};

	Schlag() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		loadSettings();
		srcName = "SCHLAG";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(DRUMS_INPUT, follow::DRUMS);
		fol.bind(CLOCK_INPUT, follow::CLOCK);
		fol.bind(RESET_INPUT, follow::RESET);
		fol.drumsOnly = true;
		fol.track = follow::T_DRUMS; // a new SCHLAG plays GENRA's drums right away (a cable always wins)
		for (int t = 0; t < NT; t++) {
			std::string n = TRACK_NAMES[t];
			configParam<SampleQuantity>(SAMPLE_PARAM + t, 0.f, 1.f, 0.f, n + " sample (in the folder)");
			configParam(RAND_PARAM + t, 0.f, 1.f, 0.f, n + " RANDOM: turn = roll a sample for every step from the folder; how far = how far from the base sample", " %", 0.f, 100.f);
			configButton(BASIC_PARAM + t, n + " BASIC: every step back to the base sample");
			configSwitch(MUTE_PARAM + t, 0.f, 1.f, 0.f, n + " mute", {"Off", "On"});
			configSwitch(SOLO_PARAM + t, 0.f, 1.f, 0.f, n + " solo", {"Off", "On"});
			for (int r = 0; r < NROW; r++) {
				int k = t * NROW + r;
				std::vector<std::string> names;
				for (int f = 0; f < NUM_FN; f++)
					names.push_back(FN[f].name);
				configSwitch(FN_PARAM + k, 0.f, NUM_FN - 1, DEFAULT_FN[r], n + string::f(" knob %d function", r + 1), names);
				configParam<KnobQuantity>(K_PARAM + k, 0.f, 1.f, FN[DEFAULT_FN[r]].def, "");
			}
			configInput(TRIG_INPUT + t, n + " trigger (velocity: the trigger's height, 10 V full)");
			configOutput(OUT_OUTPUT + t, n + " alone (dry: with its level, without pan and sends)");
			for (int f = 0; f < NUM_FN; f++)
				fnVal[t][f] = val[t][f] = FN[f].def;
			for (int r = 0; r < NROW; r++)
				curFn[t][r] = -1;
			bank[t] = nullptr;
			lastIdx[t] = -1;
			hitFlash[t] = 0.f;
			for (int s = 0; s < MAXS; s++)
				map[t][s] = -1, hitLoop[t][s] = 0;
		}
		for (int i = 0; i < NUM_SLOTS; i++)
			printHas[i] = false;
		sendPulledAt[0] = sendPulledAt[1] = -1000000;
		configSwitch(CHOKE_PARAM, 0.f, 1.f, 1.f, "O.HAT CHOKE: the closed hat cuts the open hat", {"Off", "On"});
		configParam(ACCENT_PARAM, 0.f, 1.f, 0.5f, "Accent (how much louder an accented hit is)", " %", 0.f, 100.f);
		configParam(LEVEL_PARAM, 0.f, 1.f, 48.f / 54.f, "Level", " dB", 0.f, 54.f, -48.f);
		configButton(RNDALL_PARAM, "RND ALL: roll every track (each with its RANDOM)");
		configButton(UNDO_PARAM, "UNDO: the last roll / BASIC");
		configButton(BASICALL_PARAM, "BASIC: every track back to its base sample");
		configButton(PRINT_PARAM, "PRINT › ABLAUF: keep the rolled samples with the ABLAUF pad GENRA plays");
		configInput(DRUMS_INPUT, "Drums from GENRA (polyphonic, GENRA's order: kick, snare, clap, rim, closed hat, open hat, tom, perc; height = velocity)");
		configInput(VOCT_INPUT, "V/Oct (General MIDI drum notes: C2 kick, D2 snare, F#2 closed hat …; outside: C kick, C# snare … from C of each octave)");
		configInput(GATE_INPUT, "Gate (polyphonic, with V/Oct)");
		configInput(VEL_INPUT, "Velocity for V/Oct + Gate (0..10 V)");
		configInput(ACC_INPUT, "Accent (high: the hit is louder by ACCENT)");
		configInput(CLOCK_INPUT, "Clock (16ths: counts the steps for the rolled samples and the repeats)");
		configInput(RESET_INPUT, "Reset (back to step 1)");
		configInput(RND_INPUT, "RND trigger: roll (mono: every track; polyphonic: channel n = track n)");
		configInput(KIT_INPUT, "KIT trigger: next kit");
		for (int r = 0; r < NROW; r++)
			configInput(CV_INPUT + r, string::f("CV for the knobs in row %d (polyphonic: channel n = track n; mono: every track; ±5 V = ± half the range)", r + 1));
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		configOutput(SENDA_L_OUTPUT, "SEND A left (what the tracks send to A; GENRA FX takes it without a cable)");
		configOutput(SENDA_R_OUTPUT, "SEND A right");
		configOutput(SENDB_L_OUTPUT, "SEND B left");
		configOutput(SENDB_R_OUTPUT, "SEND B right");
		for (int i = 0; i < PARAMS_LEN; i++)
			paramQuantities[i]->randomizeEnabled = false;
		rng.s = random::u32() | 1u;
	}

	~Schlag() {
		for (int t = 0; t < NT; t++)
			delete bank[t].load();
		for (Retired& r : retired)
			delete r.b;
	}

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }
	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); }
	void onSampleRateChange(const SampleRateChangeEvent& e) override { sr = e.sampleRate; }
	void onAdd(const AddEvent& e) override { sr = APP->engine->getSampleRate(); }

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int t = 0; t < NT; t++) {
			for (int f = 0; f < NUM_FN; f++)
				fnVal[t][f] = FN[f].def;
			for (int r = 0; r < NROW; r++)
				curFn[t][r] = -1;
			for (int s = 0; s < MAXS; s++)
				map[t][s] = -1;
		}
		for (int i = 0; i < NUM_SLOTS; i++)
			printHas[i] = false;
		kitName = "INIT";
	}

	int baseIndex(int t, const Bank* b) {
		int n = b ? b->count() : 0;
		if (n <= 1)
			return 0;
		return clamp((int) std::lround(params[SAMPLE_PARAM + t].getValue() * (n - 1)), 0, n - 1);
	}

	// ============================================================================================ audio thread
	Genra* followed() {
		if (!fol.active())
			return nullptr;
		return static_cast<Genra*>(fol.genra.to.load());
	}
	/** Samples per 16th: GENRA's, the clock's, or 120 BPM. */
	float stepLen() {
		if (Genra* g = followed())
			return std::max(1.f, g->stepPeriod);
		if (clkPeriod > 0.f && sinceClk < 4.f * clkPeriod)
			return clkPeriod;
		return sr * 60.f / 120.f / 4.f;
	}
	bool audible(int t) {
		if (params[MUTE_PARAM + t].getValue() > 0.5f)
			return false;
		bool anySolo = false;
		for (int i = 0; i < NT; i++)
			anySolo |= params[SOLO_PARAM + i].getValue() > 0.5f;
		return !anySolo || params[SOLO_PARAM + t].getValue() > 0.5f;
	}

	void controlTick() {
		for (int t = 0; t < NT; t++) {
			for (int f = 0; f < NUM_FN; f++)
				val[t][f] = fnVal[t][f];
			for (int r = 0; r < NROW; r++) {
				int k = t * NROW + r;
				int fn = clamp((int) std::round(params[FN_PARAM + k].getValue()), 0, NUM_FN - 1);
				if (fn != curFn[t][r]) { // a new function on the knob: it shows that function's value
					curFn[t][r] = fn;
					params[K_PARAM + k].setValue(fnVal[t][fn]);
				}
				float v = params[K_PARAM + k].getValue();
				fnVal[t][fn] = v;
				Input& cv = inputs[CV_INPUT + r];
				if (cv.isConnected()) {
					int ch = cv.getChannels();
					v += (ch > 1 ? cv.getPolyVoltage(t) : cv.getVoltage()) / 10.f;
				}
				val[t][fn] = clamp(v, 0.f, 1.f);
			}
			float res = val[t][F_RES];
			for (Voice* v : {&tr[t].v, &tr[t].tail}) {
				if (!v->active)
					continue;
				float cut = v->hp.cutForce >= 0.f ? v->hp.cutForce : val[t][F_CUTOFF];
				if (v->hp.cutShift > 0.f) // darker repeats: towards the low-pass
					cut = (cut > 0.48f && cut < 0.52f ? 0.48f : std::min(cut, 0.48f)) - v->hp.cutShift;
				v->flt.set(clamp(cut, 0.f, 1.f), res, sr);
			}
		}
	}

	/** The step a hit falls on, and its pass for PLAY / SKIP (loop). fromGenra: the hit came from GENRA's drums, which
	 * says which step it belongs to (a hit nudged earlier fires in the step before; at the loop's start in the pass
	 * before). Without GENRA and a clock every 16 hits of a track are a pass. */
	int stepNow(int t, bool fromGenra, int64_t& loop) {
		loop = loopCount;
		if (Genra* g = followed()) {
			float p = g->posPub.load(std::memory_order_relaxed);
			if (p >= 0.f) {
				int now = (int) p;
				int ds = fromGenra ? g->drumStepPub[t].load(std::memory_order_relaxed) : -1;
				if (ds >= 0 && ds != now) {
					if (ds < now) // the next loop's first steps, fired early at the end of this one
						loop = loopCount + 1;
					return ds % MAXS;
				}
				return now % MAXS;
			}
		}
		if (clkStep >= 0 && inputs[CLOCK_INPUT].isConnected())
			return clkStep % MAXS;
		loop = tr[t].hitCount / 16;
		return tr[t].hitCount % 16;
	}

	void processClock() {
		sinceClk += 1.f;
		Genra* g = followed();
		float gp = g ? g->posPub.load(std::memory_order_relaxed) : -1.f;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			clkStep = -1;
			if (gp < 0.f) // (with GENRA the loop count comes from its play position)
				loopCount++;
		}
		if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
			if (sinceClk < sr * 2.f)
				clkPeriod = sinceClk;
			sinceClk = 0.f;
			clkStep = (clkStep + 1) % MAXS;
			if (clkStep == 0 && gp < 0.f) // a clock without RESET: every 128 steps a new pass
				loopCount++;
		}
		int s = -1;
		if (gp >= 0.f) {
			s = (int) gp;
			if (lastStep >= 0 && s < lastStep - 1) // GENRA's loop started again
				loopCount++;
		}
		else if (inputs[CLOCK_INPUT].isConnected())
			s = clkStep;
		if (s != lastStep && s >= 0)
			stepPub.store(s, std::memory_order_relaxed);
		if (s < 0 && lastStep >= 0)
			stepPub.store(-1, std::memory_order_relaxed);
		lastStep = s;
	}

	/** PRINT: when GENRA starts an ABLAUF pad that has rolled samples kept with it, they come back. */
	void processPrints() {
		Genra* g = followed();
		int slot = g && g->arrOwner.load() >= 0 ? g->arrSlotPub.load(std::memory_order_relaxed) : -1;
		if (slot == lastSlot)
			return;
		lastSlot = slot;
		if (slot >= 0 && slot < NUM_SLOTS && printHas[slot].load())
			std::memcpy(map, printMap[slot], sizeof map);
	}

	/** A hit on track t (velocity 0..1). */
	void hit(int t, float vel, bool accent, bool fromGenra = false) {
		Track& k = tr[t];
		int64_t loop = loopCount;
		int step = stepNow(t, fromGenra, loop);
		k.hitCount++;
		hitLoop[t][step] = (uint32_t) (loopCount + 1); // (the display: hit in the pass it shows)
		if (!audible(t)) {
			k.rep.on = k.delayed.on = false;
			return;
		}
		Bank* b = bank[t].load(std::memory_order_acquire);
		if (!b || !b->count())
			return;
		const float* v = val[t];
		int action = stepOf(F_ACTION, v[F_ACTION]);
		int chance = stepOf(F_CHANCE, v[F_CHANCE]);
		bool pass = chancePasses(chance, rng.uni(), loop);
		if (action == A_PLAY && !pass)
			return;
		bool act = action != A_PLAY && pass;
		int n = b->count();
		int m = map[t][step];
		int idx = m >= 0 && m < n ? m : baseIndex(t, b);
		HitParams hp;
		float acc = accent ? 1.f + params[ACCENT_PARAM].getValue() : 1.f;
		hp.gain = clamp(vel, 0.f, 1.f) * acc;
		hp.semis = tuneSemis(v[F_TUNE]) + microCents(v[F_MICRO]) / 100.f;
		hp.start = v[F_START];
		hp.end = v[F_END];
		hp.attackMs = attackMs(v[F_ATTACK]);
		hp.decayMs = decayMs(v[F_DECAY]);
		int repType = stepOf(F_REPTYPE, v[F_REPTYPE]);
		int repGrid = stepOf(F_REPGRID, v[F_REPGRID]);
		float delay = 0.f;
		if (act) {
			switch (action) {
				case A_SAMPLE: idx = rng.range(0, n - 1); break;
				case A_NOTE: hp.semis += (float) rng.range(-12, 12); break;
				case A_OCTAVE: hp.semis += 12.f; break;
				case A_MICRO: hp.semis += (rng.uni() * 2.f - 1.f); break;
				case A_CUTOFF: hp.cutForce = 0.1f + 0.8f * rng.uni(); break;
				case A_REPEAT: repType = rng.range(1, NUM_REPEAT_TYPES - 1), repGrid = DEFAULT_GRID; break;
				case A_START: hp.start = 0.7f * rng.uni(); break;
				case A_END: hp.end = 0.3f + 0.7f * rng.uni(); break;
				case A_HUMANIZE: delay = rng.uni() * 0.2f * stepLen(); break;
				default: break;
			}
		}
		k.rep.on = false;
		if (delay >= 1.f) { // HUMANIZE: a little later
			if (k.delayed.on)
				start(t, k.delayed.idx, k.delayed.hp, k.delayed.repType, k.delayed.repGrid);
			k.delayed.on = true;
			k.delayed.timer = delay;
			k.delayed.idx = idx;
			k.delayed.hp = hp;
			k.delayed.repType = repType;
			k.delayed.repGrid = repGrid;
			return;
		}
		start(t, idx, hp, repType, repGrid);
	}

	/** Plays the hit and sets up its repeats. */
	void start(int t, int idx, const HitParams& hp, int repType, int repGrid) {
		Track& k = tr[t];
		if (repType != R_OFF) {
			const RepGrid& g = REPEAT_GRIDS[clamp(repGrid, 0, NUM_REPEAT_GRIDS - 1)];
			k.rep.on = g.hits > 1;
			k.rep.type = repType;
			k.rep.n = g.hits;
			k.rep.k = 0;
			k.rep.idx = idx;
			k.rep.interval = stepLen() * g.steps / g.hits;
			k.rep.timer = k.rep.interval;
			k.rep.hp = hp;
			k.rep.seed = rng.next() | 1u;
			fire(t, idx, withRepeat(hp, repType, 0, g.hits, k.rep.seed));
			return;
		}
		k.rep.on = false;
		fire(t, idx, hp);
	}
	static HitParams withRepeat(HitParams hp, int type, int i, int n, uint32_t seed) {
		Rng r;
		r.s = seed + (uint32_t) i * 0x9e3779b9u;
		r.next();
		RepeatMod m = repeatMod(type, i, n, r.uni());
		hp.semis += m.semis;
		hp.gain *= m.gain;
		hp.panOffset += m.pan;
		hp.cutShift += m.cutShift;
		if (m.start >= 0.f)
			hp.start = m.start;
		hp.bitsForce = std::min(hp.bitsForce, m.bits);
		return hp;
	}
	void fire(int t, int idx, const HitParams& hp) {
		Track& k = tr[t];
		Bank* b = bank[t].load(std::memory_order_acquire);
		if (!b || idx < 0 || idx >= b->count() || !b->files[idx])
			return;
		if (t == CHAT && params[CHOKE_PARAM].getValue() > 0.5f) { // the closed hat cuts the open hat
			tr[OHAT].v.fadeOut(sr, 4.f);
			tr[OHAT].tail.fadeOut(sr, 4.f);
			tr[OHAT].rep.on = false;
			tr[OHAT].delayed.on = false; // (an open hat held back by HUMANIZE)
		}
		if (k.v.active) { // the playing one fades out (3 ms) while the new one starts
			std::swap(k.tail, k.v);
			k.tail.fadeOut(sr, 3.f);
		}
		k.v.start(b->files[idx], hp, sr);
		k.v.flt.lastV = -1.f;
		float cut = hp.cutForce >= 0.f ? hp.cutForce : val[t][F_CUTOFF];
		if (hp.cutShift > 0.f)
			cut = (cut > 0.48f && cut < 0.52f ? 0.48f : std::min(cut, 0.48f)) - hp.cutShift;
		k.v.flt.set(clamp(cut, 0.f, 1.f), val[t][F_RES], sr);
		lastIdx[t].store(idx, std::memory_order_relaxed);
		hitFlash[t].store(1.f, std::memory_order_relaxed);
	}

	void processHits() {
		float vel[NT] = {};
		bool fromGenra[NT] = {};
		bool any = false;
		// TRIG jacks
		for (int t = 0; t < NT; t++) {
			float g = inputs[TRIG_INPUT + t].getVoltage();
			if (trigIn[t].process(g, 0.05f, 0.1f))
				vel[t] = std::max(vel[t], clamp(g / 10.f, 0.01f, 1.f)), any = true;
		}
		// GENRA's DRUMS (the order is the tracks' order)
		if (inputs[DRUMS_INPUT].isConnected()) {
			int ch = std::min(NT, inputs[DRUMS_INPUT].getChannels());
			for (int c = 0; c < ch; c++) {
				float g = inputs[DRUMS_INPUT].getPolyVoltage(c);
				if (drumIn[c].process(g, 0.05f, 0.1f))
					vel[c] = std::max(vel[c], clamp(g / 10.f, 0.01f, 1.f)), any = fromGenra[c] = true;
			}
		}
		// V/OCT + GATE: General MIDI drum notes
		if (inputs[GATE_INPUT].isConnected()) {
			int n = std::max(1, inputs[GATE_INPUT].getChannels());
			for (int c = 0; c < 16; c++) {
				bool on = false;
				if (c < n) {
					float g = inputs[GATE_INPUT].getPolyVoltage(c);
					on = gateWas[c] ? g > 0.5f : g >= 1.f;
				}
				if (on && !gateWas[c]) {
					int note = clamp((int) std::round(inputs[VOCT_INPUT].getPolyVoltage(c) * 12.f) + 60, 0, 127);
					int t = clamp(genra::gmDrumVoice(note), 0, NT - 1);
					float v = inputs[VEL_INPUT].isConnected() ? clamp(inputs[VEL_INPUT].getPolyVoltage(c) / 10.f, 0.01f, 1.f) : 1.f;
					vel[t] = std::max(vel[t], v), any = true;
				}
				gateWas[c] = on;
			}
		}
		// MIDI
		midi::Message msg;
		while (midiIn.tryPop(&msg, frameNow)) {
			if (msg.getStatus() != 0x9 || msg.getValue() == 0)
				continue;
			int t = clamp(genra::gmDrumVoice(msg.getNote()), 0, NT - 1);
			vel[t] = std::max(vel[t], msg.getValue() / 127.f), any = true;
		}
		if (!any)
			return;
		bool accent = inputs[ACC_INPUT].getVoltage() >= 1.f;
		// the closed hat first (it cuts an open hat that sounds with it)
		static const int ORDER[NT] = {CHAT, KICK, SNARE, CLAP, RIM, OHAT, TOM, PERC};
		for (int t : ORDER)
			if (vel[t] > 0.f)
				hit(t, vel[t], accent, fromGenra[t] && followed() != nullptr);
	}

	void processTimers() {
		for (int t = 0; t < NT; t++) {
			Track& k = tr[t];
			if (k.delayed.on && (k.delayed.timer -= 1.f) <= 0.f) {
				k.delayed.on = false;
				start(t, k.delayed.idx, k.delayed.hp, k.delayed.repType, k.delayed.repGrid);
			}
			if (k.rep.on && (k.rep.timer -= 1.f) <= 0.f) {
				k.rep.k++;
				if (audible(t))
					fire(t, k.rep.idx, withRepeat(k.rep.hp, k.rep.type, k.rep.k, k.rep.n, k.rep.seed));
				k.rep.timer += k.rep.interval;
				if (k.rep.k >= k.rep.n - 1)
					k.rep.on = false;
			}
		}
	}

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		frameNow = args.frame;
		sr = args.sampleRate;
		if (--ctl <= 0) {
			ctl = 32;
			controlTick();
			// the jacks the UI acts on
			uint32_t mask = 0;
			if (inputs[RND_INPUT].isConnected()) {
				int ch = inputs[RND_INPUT].getChannels();
				for (int c = 0; c < (ch > 1 ? std::min(ch, NT) : 1); c++)
					if (rndTrig[c].process(inputs[RND_INPUT].getPolyVoltage(c), 0.1f, 1.f))
						mask |= ch > 1 ? 1u << c : 0xffu;
			}
			if (mask)
				rndMask.fetch_or(mask);
			if (kitTrig.process(inputs[KIT_INPUT].getVoltage(), 0.1f, 1.f))
				kitNext++;
			// muted tracks fall silent
			for (int t = 0; t < NT; t++)
				if (!audible(t)) {
					tr[t].v.fadeOut(sr, 5.f);
					tr[t].tail.fadeOut(sr, 5.f);
				}
		}
		processClock();
		processPrints();
		processHits();
		processTimers();

		float level = levelGain(params[LEVEL_PARAM].getValue());
		float L = 0.f, R = 0.f, aL = 0.f, aR = 0.f, bL = 0.f, bR = 0.f;
		for (int t = 0; t < NT; t++) {
			Track& k = tr[t];
			float tl = 0.f, trr = 0.f, dry = 0.f;
			const float* v = val[t];
			int bitsK = bitsOf(v[F_BITS]);
			for (Voice* vc : {&k.v, &k.tail}) {
				if (!vc->active)
					continue;
				float l, r;
				vc->next(l, r, v[F_DRIVE], std::min(bitsK, vc->hp.bitsForce));
				dry += 0.5f * (l + r);
				// pan: equal power, on top of a stereo sample's own image
				float p = clamp((v[F_PAN] - 0.5f) * 2.f + vc->hp.panOffset, -1.f, 1.f);
				float pl = std::cos((p + 1.f) * 0.7853982f) * 1.4142136f;
				float pr = std::sin((p + 1.f) * 0.7853982f) * 1.4142136f;
				tl += l * std::min(1.f, pl);
				trr += r * std::min(1.f, pr);
			}
			float g = levelGain(v[F_LEVEL]) * 5.f; // ±1 -> ±5 V
			tl *= g;
			trr *= g;
			// the track's own out: mono, dry (no sends), with its level, before pan
			outputs[OUT_OUTPUT + t].setVoltage(genra_out::limit(dry * g));
			L += tl;
			R += trr;
			aL += tl * v[F_SENDA];
			aR += trr * v[F_SENDA];
			bL += tl * v[F_SENDB];
			bR += trr * v[F_SENDB];
		}
		outputs[L_OUTPUT].setVoltage(genra_out::limit(L * level));
		bool stereo = outputs[R_OUTPUT].isConnected() || pulled(args.frame);
		outputs[R_OUTPUT].setVoltage(genra_out::limit(R * level));
		if (!stereo)
			outputs[L_OUTPUT].setVoltage(genra_out::limit(0.5f * (L + R) * level));
		outputs[SENDA_L_OUTPUT].setVoltage(genra_out::limit(aL));
		outputs[SENDA_R_OUTPUT].setVoltage(genra_out::limit(aR));
		outputs[SENDB_L_OUTPUT].setVoltage(genra_out::limit(bL));
		outputs[SENDB_R_OUTPUT].setVoltage(genra_out::limit(bR));
	}

	// ============================================================================================ UI thread
	/** Read track t's folder (a new folder, or the same again). */
	void requestFolder(int t, const std::string& dir) {
		folder[t] = dir;
		gen[t]++;
		loader.request(t, gen[t], dir);
	}
	/** Called often (from the panel): read folders arrive, old ones go. */
	void uiStep() {
		double now = system::getTime();
		for (Bank* b : loader.take()) {
			int t = b->track;
			if (b->gen != gen[t]) { // an older request
				delete b;
				continue;
			}
			Bank* old = bank[t].exchange(b);
			if (old)
				retired.push_back({old, now});
			resolve(t);
		}
		for (auto it = retired.begin(); it != retired.end();) {
			// after 3 s the audio thread takes nothing new from an old bank; it goes once no voice holds a sample of it
			// any more, so the sample memory is never freed in the audio thread (a voice only drops its reference there)
			bool held = false;
			if (now - it->at > 3.0)
				for (const auto& f : it->b->files)
					if (f && f.use_count() > 1) {
						held = true;
						break;
					}
			if (now - it->at > 3.0 && !held) {
				delete it->b;
				it = retired.erase(it);
			}
			else
				++it;
		}
	}
	/** Names from the patch / kit / file window into indices of the folder now read. */
	void resolve(int t) {
		Pending& p = pending[t];
		if (!p.on)
			return;
		Bank* b = bank[t].load();
		int n = b ? b->count() : 0;
		if (n == 0) // the folder is missing or empty (a drive not plugged in): keep the names, maps and prints as they
			return; // came, so a save writes them back unchanged
		p.on = false;
		if (!p.base.empty() && n > 0) {
			int i = b->find(p.base);
			if (i >= 0)
				params[SAMPLE_PARAM + t].setValue(n > 1 ? i / (float) (n - 1) : 0.f);
		}
		std::vector<int16_t> idx(p.names.size(), -1);
		for (size_t i = 0; i < p.names.size(); i++)
			idx[i] = (int16_t) (b ? b->find(p.names[i]) : -1);
		auto conv = [&](const std::vector<int16_t>& src, int16_t* dst) {
			for (int s = 0; s < MAXS; s++) {
				int k = s < (int) src.size() ? src[s] : -1;
				dst[s] = k >= 0 && k < (int) idx.size() ? idx[k] : -1;
			}
		};
		if (!p.map.empty())
			conv(p.map, map[t]);
		for (auto& pr : p.prints)
			if (pr.first >= 0 && pr.first < NUM_SLOTS)
				conv(pr.second, printMap[pr.first][t]);
		p = Pending();
	}

	/** The file window: a sample chosen for track t. */
	void chooseSample(int t, const std::string& path) {
		std::string dir = system::getDirectory(path);
		Pending& p = pending[t];
		p = Pending();
		p.on = true;
		p.base = system::getFilename(path);
		Bank* b = bank[t].load();
		bool same = b && slashes(b->dir) == slashes(dir) && b->find(p.base) >= 0;
		if (same) { // the same folder: only the base sample
			resolve(t);
			return;
		}
		// a new folder: the rolled samples of this track (and in the prints) point nowhere now, UNDO neither
		undoStack.clear();
		for (int s = 0; s < MAXS; s++)
			map[t][s] = -1;
		for (int i = 0; i < NUM_SLOTS; i++)
			for (int s = 0; s < MAXS; s++)
				printMap[i][t][s] = -1;
		requestFolder(t, dir);
	}

	// ---- the rolls (with UNDO and Ctrl+Z)
	struct Snap {
		int16_t m[NT][MAXS];
	};
	std::vector<Snap> undoStack;
	std::string message;
	double messageAt = -10.0;
	void say(const std::string& s) {
		message = s;
		messageAt = system::getTime();
	}

	struct MapAction : history::ModuleAction {
		Snap before, after;
		void apply(const Snap& s) {
			Schlag* m = dynamic_cast<Schlag*>(APP->engine->getModule(moduleId));
			if (m)
				std::memcpy(m->map, s.m, sizeof s.m);
		}
		void undo() override { apply(before); }
		void redo() override { apply(after); }
	};
	void snapshot(Snap& s) { std::memcpy(s.m, map, sizeof map); }
	// UNDO: the own stack (32) and Rack's Ctrl+Z; rolls from the RND jack only into the own stack (a clock there would
	// fill Ctrl+Z), the moves of a turned RANDOM knob into neither (one entry when the turn ends)
	enum UndoMode { U_ALL, U_OWN, U_NONE };
	void pushUndo(const Snap& before, const char* name, UndoMode um = U_ALL) {
		if (um == U_NONE)
			return;
		undoStack.push_back(before);
		if (undoStack.size() > 32)
			undoStack.erase(undoStack.begin());
		if (um == U_OWN)
			return;
		MapAction* a = new MapAction;
		a->name = name;
		a->moduleId = id;
		a->before = before;
		snapshot(a->after);
		APP->history->push(a);
	}
	/** RANDOM for the tracks in mask, each with its own RANDOM knob. */
	void roll(uint32_t mask, const char* name = "SCHLAG roll", UndoMode um = U_ALL) {
		Snap before;
		snapshot(before);
		Rng r;
		r.s = random::u32() | 1u;
		bool any = false;
		for (int t = 0; t < NT; t++) {
			if (!((mask >> t) & 1u))
				continue;
			Bank* b = bank[t].load();
			if (!b || b->count() < 1)
				continue;
			int16_t tmp[MAXS];
			rollMap(tmp, baseIndex(t, b), b->count(), params[RAND_PARAM + t].getValue(), r);
			std::memcpy(map[t], tmp, sizeof tmp);
			any = true;
		}
		if (any)
			pushUndo(before, name, um);
	}
	void basic(uint32_t mask) {
		Snap before;
		snapshot(before);
		for (int t = 0; t < NT; t++)
			if ((mask >> t) & 1u)
				for (int s = 0; s < MAXS; s++)
					map[t][s] = -1;
		pushUndo(before, "SCHLAG basic");
	}
	void undo() {
		if (undoStack.empty()) {
			say("NOTHING TO UNDO");
			return;
		}
		Snap before;
		snapshot(before);
		std::memcpy(map, undoStack.back().m, sizeof map);
		undoStack.pop_back();
		MapAction* a = new MapAction;
		a->name = "SCHLAG undo";
		a->moduleId = id;
		a->before = before;
		snapshot(a->after);
		APP->history->push(a);
		say("UNDO");
	}
	/** PRINT: the rolled samples into the ABLAUF pad GENRA plays. */
	void print() {
		Genra* g = followed();
		int slot = g && g->arrOwner.load() >= 0 ? g->arrSlotPub.load() : -1;
		if (slot < 0 || slot >= NUM_SLOTS) {
			say("NO ABLAUF PAD");
			return;
		}
		std::memcpy(printMap[slot], map, sizeof map);
		printHas[slot] = true;
		std::string lbl = g->arrPadSlot == slot ? g->arrPadLabel : "";
		size_t k = lbl.find(" · ");
		say("PRINTED › " + (lbl.empty() ? string::f("PAD %d", slot + 1) : lbl.substr(0, k)));
	}

	// ---- kits: the folders, base samples and knobs of all tracks
	json_t* kitToJson() {
		json_t* j = json_object();
		json_t* ts = json_array();
		for (int t = 0; t < NT; t++) {
			json_t* o = json_object();
			json_object_set_new(o, "folder", json_string(relOf(folder[t]).c_str()));
			json_object_set_new(o, "folderAbs", json_string(folder[t].c_str()));
			Bank* b = bank[t].load();
			// (not resolved yet – the folder still loading or missing: the base as it came)
			std::string base = pending[t].on ? pending[t].base : (b && b->count() ? b->names[baseIndex(t, b)] : "");
			json_object_set_new(o, "base", json_string(base.c_str()));
			json_t* fns = json_array();
			for (int r = 0; r < NROW; r++)
				json_array_append_new(fns, json_integer((int) std::round(params[FN_PARAM + t * NROW + r].getValue())));
			json_object_set_new(o, "knobs", fns);
			json_t* vals = json_array();
			for (int f = 0; f < NUM_FN; f++)
				json_array_append_new(vals, json_real(fnVal[t][f]));
			json_object_set_new(o, "values", vals);
			json_object_set_new(o, "random", json_real(params[RAND_PARAM + t].getValue()));
			json_array_append_new(ts, o);
		}
		json_object_set_new(j, "tracks", ts);
		json_object_set_new(j, "choke", json_boolean(params[CHOKE_PARAM].getValue() > 0.5f));
		return j;
	}
	static std::string folderFrom(json_t* o) {
		std::string rel, abs;
		if (json_t* v = json_object_get(o, "folder"))
			rel = json_string_value(v) ? json_string_value(v) : "";
		if (json_t* v = json_object_get(o, "folderAbs"))
			abs = json_string_value(v) ? json_string_value(v) : "";
		if (!rel.empty() && !rootDir().empty()) {
			std::string p = system::join(rootDir(), rel);
			if (isDir(p) || abs.empty() || !isDir(abs))
				return p;
		}
		return abs;
	}
	/** withMaps: from a patch (the maps and prints too); else a kit (the maps start fresh). */
	void kitFromJson(json_t* j, bool withMaps) {
		json_t* ts = json_object_get(j, "tracks");
		for (int t = 0; t < NT && ts; t++) {
			json_t* o = json_array_get(ts, t);
			if (!o)
				continue;
			Pending& p = pending[t];
			p = Pending();
			p.on = true;
			if (json_t* v = json_object_get(o, "base"))
				p.base = json_string_value(v) ? json_string_value(v) : "";
			if (json_t* fns = json_object_get(o, "knobs"))
				for (int r = 0; r < NROW; r++)
					if (json_t* v = json_array_get(fns, r))
						params[FN_PARAM + t * NROW + r].setValue((float) clamp((int) json_integer_value(v), 0, NUM_FN - 1));
			if (json_t* vals = json_object_get(o, "values"))
				for (int f = 0; f < NUM_FN; f++)
					if (json_t* v = json_array_get(vals, f))
						fnVal[t][f] = clamp((float) json_number_value(v), 0.f, 1.f);
			if (json_t* v = json_object_get(o, "random"))
				params[RAND_PARAM + t].setValue(clamp((float) json_number_value(v), 0.f, 1.f));
			for (int r = 0; r < NROW; r++) {
				int fn = clamp((int) std::round(params[FN_PARAM + t * NROW + r].getValue()), 0, NUM_FN - 1);
				params[K_PARAM + t * NROW + r].setValue(fnVal[t][fn]);
				curFn[t][r] = fn;
			}
			if (withMaps) {
				if (json_t* ns = json_object_get(o, "names"))
					for (size_t i = 0; i < json_array_size(ns); i++) {
						const char* s = json_string_value(json_array_get(ns, i));
						p.names.push_back(s ? s : "");
					}
				auto readMap = [](json_t* a, std::vector<int16_t>& out) {
					out.assign(MAXS, -1);
					for (int s = 0; s < MAXS && a; s++)
						if (json_t* v = json_array_get(a, s))
							out[s] = (int16_t) clamp((int) json_integer_value(v), -1, 30000);
				};
				if (json_t* a = json_object_get(o, "map"))
					readMap(a, p.map);
				if (json_t* prs = json_object_get(o, "prints"))
					for (size_t i = 0; i < json_array_size(prs); i++) {
						json_t* e = json_array_get(prs, i);
						int slot = (int) json_integer_value(json_object_get(e, "slot"));
						std::vector<int16_t> m;
						readMap(json_object_get(e, "map"), m);
						p.prints.push_back({slot, m});
					}
			}
			for (int s = 0; s < MAXS; s++)
				map[t][s] = -1;
			std::string dir = folderFrom(o);
			if (dir != folder[t] || !bank[t].load() || withMaps)
				requestFolder(t, dir);
			else
				resolve(t);
		}
		if (json_t* v = json_object_get(j, "choke"))
			params[CHOKE_PARAM].setValue(json_boolean_value(v) ? 1.f : 0.f);
	}

	std::vector<std::string> kitFiles() {
		std::vector<std::string> v;
		if (isDir(kitDir()))
			for (const std::string& e : entries(kitDir()))
				if (string::lowercase(system::getExtension(e)) == ".json")
					v.push_back(e);
		std::sort(v.begin(), v.end(), lessName);
		return v;
	}
	void saveKit(const std::string& name) {
		std::string n = name.empty() ? "KIT" : name;
		for (char& c : n)
			if (std::strchr("\\/:*?\"<>|", c))
				c = '_';
		system::createDirectories(kitDir());
		json_t* j = kitToJson();
		if (json_dump_file(j, system::join(kitDir(), n + ".json").c_str(), JSON_INDENT(2)) == 0) {
			kitName = n;
			say("KIT SAVED");
		}
		json_decref(j);
	}
	void loadKit(const std::string& path) {
		json_t* j = json_load_file(path.c_str(), 0, nullptr);
		if (!j)
			return;
		for (int i = 0; i < NUM_SLOTS; i++)
			printHas[i] = false;
		undoStack.clear(); // other folders: the old rolls mean nothing there
		kitFromJson(j, false);
		json_decref(j);
		kitName = system::getStem(path);
	}
	void initKit() {
		for (int t = 0; t < NT; t++) {
			for (int f = 0; f < NUM_FN; f++)
				fnVal[t][f] = FN[f].def;
			for (int r = 0; r < NROW; r++) {
				params[FN_PARAM + t * NROW + r].setValue(DEFAULT_FN[r]);
				params[K_PARAM + t * NROW + r].setValue(FN[DEFAULT_FN[r]].def);
				curFn[t][r] = DEFAULT_FN[r];
			}
			params[RAND_PARAM + t].setValue(0.f);
		}
		kitName = "INIT";
	}
	void nextKit() {
		std::vector<std::string> v = kitFiles();
		if (v.empty())
			return;
		int i = 0;
		for (int k = 0; k < (int) v.size(); k++)
			if (system::getStem(v[k]) == kitName)
				i = (k + 1) % (int) v.size();
		loadKit(v[i]);
	}

	// ---- the patch
	json_t* dataToJson() override {
		json_t* j = kitToJson();
		json_t* ts = json_object_get(j, "tracks");
		for (int t = 0; t < NT; t++) {
			json_t* o = json_array_get(ts, t);
			Bank* b = bank[t].load();
			// the maps with the names of the samples (a folder may change between sessions)
			std::vector<std::string> names;
			std::vector<int> nameOf(b ? b->count() : 0, -1);
			auto ref = [&](int idx) -> int {
				if (!b || idx < 0 || idx >= b->count())
					return -1;
				if (nameOf[idx] < 0) {
					nameOf[idx] = (int) names.size();
					names.push_back(b->names[idx]);
				}
				return nameOf[idx];
			};
			auto writeMap = [&](const int16_t* m) {
				json_t* a = json_array();
				for (int s = 0; s < MAXS; s++)
					json_array_append_new(a, json_integer(ref(m[s])));
				return a;
			};
			if (pending[t].on) { // not resolved yet (still loading, or the folder missing): as it came
				Pending& p = pending[t];
				for (const std::string& n : p.names)
					names.push_back(n);
				auto rawMap = [](const std::vector<int16_t>& m) {
					json_t* a = json_array();
					for (int s = 0; s < MAXS; s++)
						json_array_append_new(a, json_integer(s < (int) m.size() ? m[s] : -1));
					return a;
				};
				json_object_set_new(o, "map", rawMap(p.map));
				json_t* prs = json_array();
				for (auto& pr : p.prints) {
					json_t* e = json_object();
					json_object_set_new(e, "slot", json_integer(pr.first));
					json_object_set_new(e, "map", rawMap(pr.second));
					json_array_append_new(prs, e);
				}
				json_object_set_new(o, "prints", prs);
			}
			else if (b) {
				json_object_set_new(o, "map", writeMap(map[t]));
				json_t* prs = json_array();
				for (int i = 0; i < NUM_SLOTS; i++)
					if (printHas[i].load()) {
						json_t* e = json_object();
						json_object_set_new(e, "slot", json_integer(i));
						json_object_set_new(e, "map", writeMap(printMap[i][t]));
						json_array_append_new(prs, e);
					}
				json_object_set_new(o, "prints", prs);
			}
			json_t* ns = json_array();
			for (const std::string& n : names)
				json_array_append_new(ns, json_string(n.c_str()));
			json_object_set_new(o, "names", ns);
		}
		json_t* pr = json_array();
		for (int i = 0; i < NUM_SLOTS; i++)
			if (printHas[i].load())
				json_array_append_new(pr, json_integer(i));
		json_object_set_new(j, "printed", pr);
		json_object_set_new(j, "kitName", json_string(kitName.c_str()));
		json_object_set_new(j, "midi", midiIn.toJson());
		json_object_set_new(j, "sendAuto", json_boolean(sendAuto.load()));
		json_object_set_new(j, "version", json_integer(1));
		fol.toJson(j);
		return j;
	}
	void dataFromJson(json_t* j) override {
		fol.fromJson(j);
		if (!json_object_get(j, "followGenra"))
			fol.track = -1;
		for (int i = 0; i < NUM_SLOTS; i++)
			printHas[i] = false;
		if (json_t* pr = json_object_get(j, "printed"))
			for (size_t i = 0; i < json_array_size(pr); i++) {
				int s = (int) json_integer_value(json_array_get(pr, i));
				if (s >= 0 && s < NUM_SLOTS) {
					printHas[s] = true;
					for (int t = 0; t < NT; t++)
						for (int k = 0; k < MAXS; k++)
							printMap[s][t][k] = -1;
				}
			}
		kitFromJson(j, true);
		if (json_t* v = json_object_get(j, "kitName"))
			kitName = json_string_value(v) ? json_string_value(v) : "";
		if (json_t* v = json_object_get(j, "midi"))
			midiIn.fromJson(v);
		if (json_t* v = json_object_get(j, "sendAuto"))
			sendAuto = json_boolean_value(v);
		lastSlot = -2;
	}
};

// ------------------------------------------------------------------------------------------------ the panel
void pickSample(Schlag* m, int t);

/** The display: per track the sample (waveform, name, place in the folder) and its steps; the link at the right.
 * A click on a track: the Windows file window for its sample. On the right: Follow GENRA. */
struct Display : OpaqueWidget {
	Schlag* module = nullptr;

	float colX(int t) { return mm2px(L::STRIP_X0 - L::DISPLAY[0] + t * L::STRIP_W); }
	float colW() { return mm2px(L::STRIP_W); }

	int pickTrack = -1; // a track clicked: its file window opens in the next frame
	void linkMenu();

	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			float x = e.pos.x;
			if (x >= mm2px(L::MASTER_X - L::DISPLAY[0]))
				linkMenu();
			else {
				pickTrack = clamp((int) ((x - colX(0)) / colW()), 0, NT - 1);
			}
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void step() override {
		if (module) {
			module->uiStep();
			if (pickTrack >= 0) {
				int t = pickTrack;
				pickTrack = -1;
				pickSample(module, t);
			}
		}
		OpaqueWidget::step();
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override;
};

/** The window to choose the sample root folder (once per computer; any time again from the right-click menu). */
static bool chooseRoot() {
	std::string start = isDir(rootDir()) ? rootDir() : "";
	char* p = osdialog_file(OSDIALOG_OPEN_DIR, start.empty() ? nullptr : start.c_str(), nullptr, nullptr);
	if (!p)
		return false;
	std::string r = p;
	std::free(p);
	while (r.size() > 3 && (r.back() == '\\' || r.back() == '/'))
		r.pop_back();
	rootDir() = r;
	saveSettings();
	return true;
}

/** The window to choose a track's sample: click through the folders, pick a WAV. Its folder becomes the track's
 * folder, the file its base sample. The first time (no root folder yet) the root folder comes first. */
void pickSample(Schlag* m, int t) {
	if (!isDir(rootDir()) && !chooseRoot())
		return;
	Bank* b = m->bank[t].load();
	std::string start = b && isDir(b->dir) ? b->dir : (isDir(m->folder[t]) ? m->folder[t] : rootDir());
	std::string name = b && b->count() ? b->names[m->baseIndex(t, b)] : "";
	osdialog_filters* f = osdialog_filters_parse("WAV:wav,WAV");
	char* p = osdialog_file(OSDIALOG_OPEN, start.empty() ? nullptr : start.c_str(), name.empty() ? nullptr : name.c_str(), f);
	osdialog_filters_free(f);
	if (!p)
		return;
	std::string path = p;
	std::free(p);
	if (isWav(path))
		m->chooseSample(t, path);
}

/** A name typed into a menu: Enter applies (saving a kit). */
struct PathField : ui::TextField {
	std::function<void(const std::string&)> apply;
	void onAction(const ActionEvent& e) override {
		if (apply)
			apply(text);
		if (ui::MenuOverlay* o = getAncestorOfType<ui::MenuOverlay>())
			o->requestDelete();
		e.consume(this);
	}
};

void Display::linkMenu() {
	Schlag* m = module;
	Menu* menu = createMenu();
	menu->addChild(createMenuLabel("Where the hits come from"));
	m->fol.appendMenu(menu, m);
	menu->addChild(createMenuLabel("Also: the TRIG jacks, DRUMS, V/OCT + GATE and MIDI IN"));
}

void Display::drawLayer(const DrawArgs& args, int layer) {
	OpaqueWidget::drawLayer(args, layer);
	if (layer != 1 || !module)
		return;
	NVGcontext* vg = args.vg;
	auto font = gui::boldFont();
	float H = box.size.y;
	int step = module->stepPub.load(std::memory_order_relaxed);
	uint32_t loopNow = (uint32_t) (module->loopCount + 1);
	for (int t = 0; t < NT; t++) {
		float x0 = colX(t), w = colW();
		if (t) {
			nvgBeginPath(vg);
			nvgRect(vg, x0, mm2px(2.f), 1.f, H - mm2px(4.f));
			nvgFillColor(vg, nvgRGB(0x1e, 0x20, 0x26));
			nvgFill(vg);
		}
		Bank* b = module->bank[t].load();
		float cx = x0 + w / 2;
		// waveform of the sample last played (or the base sample)
		std::shared_ptr<Sample> s;
		int base = b ? module->baseIndex(t, b) : 0;
		if (b && b->count()) {
			int li = module->lastIdx[t].load();
			int i = li >= 0 && li < b->count() ? li : base;
			s = b->files[i];
		}
		float wx = x0 + mm2px(2.5f), ww = w - mm2px(5.f), wy = mm2px(4.6f), wa = mm2px(2.4f);
		float fl = module->hitFlash[t].load();
		module->hitFlash[t].store(std::max(0.f, fl - 0.08f));
		if (s) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, wx, wy);
			for (int i = 0; i < 64; i++)
				nvgLineTo(vg, wx + ww * (i + 0.5f) / 64.f, wy - wa * s->peaks[i]);
			for (int i = 63; i >= 0; i--)
				nvgLineTo(vg, wx + ww * (i + 0.5f) / 64.f, wy + wa * s->peaks[i]);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgTransRGBAf(ACCENT, 0.7f + 0.3f * fl));
			nvgFill(vg);
		}
		// name and place
		std::string name, place;
		NVGcolor pc = ACCENT;
		if (!b) {
			name = module->folder[t].empty() ? "CHOOSE SAMPLE" : "LOADING …";
			place = module->folder[t].empty() ? "CLICK HERE" : "";
			if (module->folder[t].empty())
				pc = gui::DIM;
		}
		else if (!b->count()) {
			name = b->error;
			place = b->dir.empty() ? "CLICK HERE" : string::uppercase(system::getFilename(b->dir));
			pc = nvgRGB(0xff, 0x6b, 0x6b);
		}
		else {
			name = string::uppercase(system::getStem(b->names[base]));
			place = string::uppercase(system::getFilename(b->dir)) + string::f("  %d/%d", base + 1, b->count());
		}
		nvgSave(vg);
		nvgScissor(vg, x0 + 2, 0, w - 4, H);
		gui::text(vg, font, cx, mm2px(9.5f), name, 7.2f, gui::WHITE, NVG_ALIGN_CENTER, 0.4f);
		gui::text(vg, font, cx, mm2px(11.7f), place, 6.f, pc, NVG_ALIGN_CENTER, 0.4f);
		nvgRestore(vg);
		// the steps of the bar playing: rolled (yellow), hit (accent), playing (frame)
		int page = step >= 0 ? (step / 16) * 16 : 0;
		float cw = ww / 16.f, sy = mm2px(12.9f), sh = mm2px(1.5f);
		for (int k = 0; k < 16; k++) {
			int s2 = page + k;
			bool rolled = module->map[t][s2] >= 0;
			uint32_t hl = module->hitLoop[t][s2];
			bool hit = hl && hl + 1 >= loopNow;
			NVGcolor c = hit ? (rolled ? ROLLED : ACCENT) : (rolled ? nvgTransRGBAf(ROLLED, 0.35f) : nvgRGB(0x26, 0x28, 0x2f));
			nvgBeginPath(vg);
			nvgRoundedRect(vg, wx + k * cw + 0.4f, sy, cw - 0.8f, sh, 0.8f);
			nvgFillColor(vg, c);
			nvgFill(vg);
			if (s2 == step) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, wx + k * cw - 0.2f, sy - 0.8f, cw + 0.4f, sh + 1.6f, 1.f);
				nvgStrokeColor(vg, gui::WHITE);
				nvgStrokeWidth(vg, 0.8f);
				nvgStroke(vg);
			}
		}
	}
	// the right: the link, the tempo, a message
	float mx = mm2px(L::MASTER_X - L::DISPLAY[0]), mw = box.size.x - mx, mc = mx + mw / 2;
	nvgBeginPath(vg);
	nvgRect(vg, mx - mm2px(0.8f), mm2px(2.f), 1.f, H - mm2px(4.f));
	nvgFillColor(vg, gui::EDGE);
	nvgFill(vg);
	bool linked = module->fol.active();
	gui::text(vg, font, mc, mm2px(5.5f), linked ? "\xe2\x97\x8f GENRA" : (module->inputs[Schlag::CLOCK_INPUT].isConnected() ? "CLOCK" : "NO LINK"),
	          6.6f, linked ? ACCENT : gui::DIM, NVG_ALIGN_CENTER, 0.6f);
	double age = system::getTime() - module->messageAt;
	if (age < 2.5)
		gui::text(vg, font, mc, mm2px(10.3f), module->message, 7.f, ROLLED, NVG_ALIGN_CENTER, 0.3f);
	else {
		std::string big = step >= 0 ? string::f("STEP %d", step + 1) : "STOP";
		gui::text(vg, font, mc, mm2px(10.3f), big, 9.f, gui::WHITE, NVG_ALIGN_CENTER, 0.5f);
	}
	float bpm = module->sr * 60.f / std::max(1.f, module->stepLen()) / 4.f;
	gui::text(vg, font, mc, mm2px(14.0f), string::f("%.0f BPM", bpm), 6.f, gui::DIM, NVG_ALIGN_CENTER, 0.6f);
}

/** The field above a knob: its function. Click = the list; while the knob turns, its value. */
struct FnField : app::ParamWidget {
	int track = 0, row = 0;
	float lastV = -1.f;
	double movedAt = -10.0;
	int fnNow() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, NUM_FN - 1) : 0;
	}
	void set(int fn) {
		Schlag* m = dynamic_cast<Schlag*>(module);
		if (!m)
			return;
		int old = fnNow();
		if (fn == old)
			return;
		history::ComplexAction* ca = new history::ComplexAction;
		ca->name = "change knob function";
		auto change = [&](int r, int from, int to) {
			int id = Schlag::FN_PARAM + track * NROW + r;
			m->params[id].setValue((float) to);
			history::ParamChange* h = new history::ParamChange;
			h->moduleId = m->id;
			h->paramId = id;
			h->oldValue = (float) from;
			h->newValue = (float) to;
			ca->push(h);
		};
		// the function is on another knob of the track: the two swap
		for (int r = 0; r < NROW; r++)
			if (r != row && (int) std::round(m->params[Schlag::FN_PARAM + track * NROW + r].getValue()) == fn)
				change(r, fn, old);
		change(row, old, fn);
		APP->history->push(ca);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel(string::f("%s · KNOB %d", TRACK_NAMES[track], row + 1)));
			for (int g = 0; g < NUM_GROUPS; g++) {
				menu->addChild(new MenuSeparator);
				menu->addChild(createMenuLabel(GROUP_NAMES[g]));
				for (int f = 0; f < NUM_FN; f++)
					if (FN[f].group == g)
						menu->addChild(createCheckMenuItem(FN[f].name, FN[f].hint, [=]() { return fnNow() == f; }, [=]() { set(f); }));
			}
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			int fn = fnNow();
			float v = module->params[Schlag::K_PARAM + track * NROW + row].getValue();
			double now = system::getTime();
			if (lastV >= 0.f && v != lastV)
				movedAt = now;
			lastV = v;
			bool showVal = now - movedAt < 1.2;
			std::string s = showVal ? fnText(fn, v) : std::string(FN[fn].name);
			auto font = gui::boldFont();
			nvgSave(args.vg);
			nvgScissor(args.vg, 0, 0, box.size.x - mm2px(3.f), box.size.y);
			gui::text(args.vg, font, mm2px(1.f), box.size.y / 2 + 2.3f, s, showVal ? 6.4f : 6.2f, showVal ? ROLLED : gui::TXT,
			          NVG_ALIGN_LEFT, 0.2f);
			nvgRestore(args.vg);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** The kit field in the header. */
struct KitField : OpaqueWidget {
	Schlag* module = nullptr;
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Schlag* m = module;
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Kit: folders, samples and knobs of all tracks"));
			std::vector<std::string> v = m->kitFiles();
			if (v.empty())
				menu->addChild(createMenuLabel("(no kits saved yet)"));
			for (const std::string& f : v)
				menu->addChild(createCheckMenuItem(system::getStem(f), "", [=]() { return m->kitName == system::getStem(f); },
				                                   [=]() { m->loadKit(f); }));
			menu->addChild(new MenuSeparator);
			menu->addChild(createSubmenuItem("Save kit as …", "", [=](Menu* sub) {
				PathField* tf = new PathField;
				tf->box.size.x = 180.f;
				tf->placeholder = "Name, Enter saves";
				tf->text = m->kitName == "INIT" ? "" : m->kitName;
				tf->apply = [=](const std::string& s) { m->saveKit(s); };
				sub->addChild(tf);
				APP->event->setSelectedWidget(tf);
			}));
			menu->addChild(createMenuItem("Init knobs (keeps the samples)", "", [=]() { m->initKit(); }));
			menu->addChild(createMenuItem("Open the kit folder", "", [=]() {
				system::createDirectories(kitDir());
				system::openDirectory(kitDir());
			}));
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			auto font = gui::boldFont();
			float y = box.size.y / 2 + 2.6f;
			gui::text(args.vg, font, mm2px(2.f), y, "KIT", 6.4f, ACCENT, NVG_ALIGN_LEFT, 0.8f);
			gui::text(args.vg, font, mm2px(9.f), y, string::uppercase(module->kitName), 7.6f, gui::TXT, NVG_ALIGN_LEFT, 0.9f);
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** MIDI IN: device and channel. */
struct MidiField : OpaqueWidget {
	Schlag* module = nullptr;
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Menu* menu = createMenu();
			menu->addChild(createMenuLabel("MIDI IN: notes play the tracks (General MIDI drums)"));
			app::appendMidiMenu(menu, &module->midiIn);
			e.consume(this);
			return;
		}
		OpaqueWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			int d = module->midiIn.getDeviceId();
			std::string s = d >= 0 ? module->midiIn.getDeviceName(d) : "OFF";
			if (d >= 0) {
				int c = module->midiIn.getChannel();
				s += c < 0 ? " · ALL" : string::f(" · CH %d", c + 1);
			}
			auto font = gui::boldFont();
			nvgSave(args.vg);
			nvgScissor(args.vg, 0, 0, box.size.x - mm2px(4.f), box.size.y);
			gui::text(args.vg, font, mm2px(1.6f), box.size.y / 2 + 2.4f, string::uppercase(s), 6.2f, d >= 0 ? gui::TXT : gui::DIM,
			          NVG_ALIGN_LEFT, 0.3f);
			nvgRestore(args.vg);
		}
		OpaqueWidget::drawLayer(args, layer);
	}
};

/** O.HAT CHOKE: a lamp to click. */
struct ChokeLamp : app::ParamWidget {
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (engine::ParamQuantity* pq = getParamQuantity())
				pq->setValue(pq->getValue() > 0.5f ? 0.f : 1.f);
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			engine::ParamQuantity* pq = getParamQuantity();
			bool on = pq && pq->getValue() > 0.5f;
			Vec c = box.size.div(2);
			if (on) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, c.x, c.y, 7.f);
				nvgFillPaint(args.vg, nvgRadialGradient(args.vg, c.x, c.y, 1.f, 7.f, nvgTransRGBAf(ACCENT, 0.4f), nvgTransRGBAf(ACCENT, 0.f)));
				nvgFill(args.vg);
			}
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c.x, c.y, mm2px(0.95f));
			nvgFillColor(args.vg, on ? ACCENT : gui::LAMP);
			nvgFill(args.vg);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** A small latching button (MUTE, SOLO) with its lamp. */
struct LatchButton : TL1105 {
	NVGcolor col = ROLLED;
	LatchButton() { momentary = false; }
	void drawLayer(const DrawArgs& args, int layer) override {
		TL1105::drawLayer(args, layer);
		if (layer != 1)
			return;
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq || pq->getValue() < 0.5f)
			return;
		Vec c = box.size.div(2);
		float r = box.size.x * 0.5f;
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r * 1.6f);
		nvgFillPaint(args.vg, nvgRadialGradient(args.vg, c.x, c.y, r * 0.5f, r * 1.6f, nvgTransRGBAf(col, 0.45f), nvgTransRGBAf(col, 0.f)));
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c.x, c.y, r * 0.62f);
		nvgFillColor(args.vg, col);
		nvgFill(args.vg);
	}
};

/** RANDOM: turning it rolls (a turn of the knob, not a value set by undo or a preset). */
struct RandKnob : GSmallKnob {
	int track = 0;
	double last = -1.0;
	bool turning = false, rolled = false;
	Schlag::Snap atStart; // the samples before the turn: one UNDO / Ctrl+Z entry for the whole turn
	void rollNow(Schlag::UndoMode um) {
		Schlag* m = dynamic_cast<Schlag*>(module);
		if (!m)
			return;
		double now = system::getTime();
		if (now - last < 0.04) // at most 25 rolls a second while turning
			return;
		last = now;
		m->roll(1u << track, "SCHLAG RANDOM", um);
		rolled = true;
	}
	void onDragStart(const DragStartEvent& e) override {
		GSmallKnob::onDragStart(e);
		if (Schlag* m = dynamic_cast<Schlag*>(module)) {
			m->snapshot(atStart);
			turning = true;
			rolled = false;
		}
	}
	void onDragEnd(const DragEndEvent& e) override {
		GSmallKnob::onDragEnd(e);
		Schlag* m = dynamic_cast<Schlag*>(module);
		if (m && turning && rolled)
			m->pushUndo(atStart, "SCHLAG RANDOM");
		turning = false;
	}
	void onDragMove(const DragMoveEvent& e) override {
		float before = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
		GSmallKnob::onDragMove(e);
		if (getParamQuantity() && getParamQuantity()->getValue() != before)
			rollNow(turning ? Schlag::U_NONE : Schlag::U_ALL);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		float before = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
		GSmallKnob::onHoverScroll(e);
		if (getParamQuantity() && getParamQuantity()->getValue() != before)
			rollNow(Schlag::U_ALL);
	}
};

/** The SEND jacks glow while GENRA FX takes them; "› GENRA FX" beside them. */
struct SendLight : TransparentWidget {
	Schlag* module = nullptr;
	Vec jack[4];
	void drawLayer(const DrawArgs& args, int layer) override {
		TransparentWidget::drawLayer(args, layer);
		if (layer != 1 || !module)
			return;
		bool any = false;
		for (int k = 0; k < 4; k++)
			if (module->sendPulled(k / 2)) {
				follow::segment(args.vg, jack[k], FXCOL);
				any = true;
			}
		if (any)
			gui::text(args.vg, gui::boldFont(), jack[0].x - mm2px(6.5f), jack[0].y + 2.f, "GENRA FX \xe2\x80\xb9", 6.2f, FXCOL,
			          NVG_ALIGN_RIGHT, 0.4f);
	}
};

struct SchlagWidget : ModuleWidget {
	dsp::BooleanTrigger basicT[NT], rndAllT, undoT, basicAllT, printT;
	int kitSeen = 0;

	SchlagWidget(Schlag* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Schlag.svg")));
		using M = Schlag;
		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		follow::addStrip(this, module, {P(L::IN_DRUMS), P(L::CLOCK), P(L::RESET)});
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);

		KitField* kf = createWidget<KitField>(mm2px(Vec(L::KITF[0], L::KITF[1])));
		kf->box.size = mm2px(Vec(L::KITF[2], L::KITF[3]));
		kf->module = module;
		addChild(kf);
		MidiField* mf = createWidget<MidiField>(mm2px(Vec(L::MIDIF[0], L::MIDIF[1])));
		mf->box.size = mm2px(Vec(L::MIDIF[2], L::MIDIF[3]));
		mf->module = module;
		addChild(mf);

		for (int t = 0; t < NT; t++) {
			addParam(createParamCentered<GSmallKnob>(P(L::SAMPLE_T[t]), module, M::SAMPLE_PARAM + t));
			RandKnob* rk = createParamCentered<RandKnob>(P(L::RAND_T[t]), module, M::RAND_PARAM + t);
			rk->track = t;
			addParam(rk);
			addParam(createParamCentered<TL1105>(P(L::BASIC_T[t]), module, M::BASIC_PARAM + t));
			LatchButton* mb = createParamCentered<LatchButton>(P(L::MUTE_T[t]), module, M::MUTE_PARAM + t);
			mb->col = ROLLED;
			addParam(mb);
			LatchButton* sb = createParamCentered<LatchButton>(P(L::SOLO_T[t]), module, M::SOLO_PARAM + t);
			sb->col = nvgRGB(0x7d, 0xe0, 0x8f);
			addParam(sb);
			for (int r = 0; r < NROW; r++) {
				const float* f = L::FN_T[t][r];
				FnField* ff = createParam<FnField>(mm2px(Vec(f[0], f[1])), module, M::FN_PARAM + t * NROW + r);
				ff->box.size = mm2px(Vec(f[2], f[3]));
				ff->track = t, ff->row = r;
				addParam(ff);
				addParam(createParamCentered<GSmallKnob>(P(L::K_T[t][r]), module, M::K_PARAM + t * NROW + r));
			}
			addInput(createInputCentered<PJ301MPort>(P(L::TRIG_T[t]), module, M::TRIG_INPUT + t));
			addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_T[t]), module, M::OUT_OUTPUT + t));
		}
		ChokeLamp* cl = createParam<ChokeLamp>(Vec(0, 0), module, M::CHOKE_PARAM);
		cl->box.size = Vec(mm2px(3.f), mm2px(3.f));
		cl->box.pos = mm2px(Vec(L::CHOKE[0], L::CHOKE[1])).minus(cl->box.size.div(2));
		addParam(cl);
		addParam(createParamCentered<GSmallKnob>(P(L::ACCENT), module, M::ACCENT_PARAM));
		addParam(createParamCentered<GSmallKnob>(P(L::MLVL), module, M::LEVEL_PARAM));
		addParam(createParamCentered<VCVBezel>(P(L::RNDALL), module, M::RNDALL_PARAM));
		addParam(createParamCentered<VCVBezel>(P(L::UNDO), module, M::UNDO_PARAM));
		addParam(createParamCentered<VCVBezel>(P(L::BASIC), module, M::BASICALL_PARAM));
		addParam(createParamCentered<VCVBezel>(P(L::PRINT), module, M::PRINT_PARAM));

		const float* ins[] = {L::IN_DRUMS, L::VOCT, L::GATE, L::VEL, L::ACC, L::CLOCK, L::RESET, L::IN_RND, L::IN_KIT};
		const int inIds[] = {M::DRUMS_INPUT, M::VOCT_INPUT, M::GATE_INPUT, M::VEL_INPUT, M::ACC_INPUT, M::CLOCK_INPUT,
		                     M::RESET_INPUT, M::RND_INPUT, M::KIT_INPUT};
		for (int i = 0; i < 9; i++)
			addInput(createInputCentered<PJ301MPort>(P(ins[i]), module, inIds[i]));
		const float* cvs[NROW] = {L::IN_CV1, L::IN_CV2, L::IN_CV3, L::IN_CV4};
		for (int r = 0; r < NROW; r++)
			addInput(createInputCentered<PJ301MPort>(P(cvs[r]), module, M::CV_INPUT + r));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_L), module, M::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_R), module, M::R_OUTPUT));
		const float* sends[4] = {L::SEND_A_L, L::SEND_A_R, L::SEND_B_L, L::SEND_B_R};
		SendLight* sl = new SendLight;
		sl->module = module;
		sl->box.size = box.size;
		for (int k = 0; k < 4; k++) {
			addOutput(createOutputCentered<PJ301MPort>(P(sends[k]), module, M::SENDA_L_OUTPUT + k));
			sl->jack[k] = P(sends[k]);
		}
		addChild(sl);
	}

	void step() override {
		ModuleWidget::step();
		Schlag* m = dynamic_cast<Schlag*>(module);
		if (!m)
			return;
		auto pressed = [&](dsp::BooleanTrigger& tr, int id) { return tr.process(m->params[id].getValue() > 0.5f); };
		uint32_t basicMask = 0;
		for (int t = 0; t < NT; t++)
			if (pressed(basicT[t], Schlag::BASIC_PARAM + t))
				basicMask |= 1u << t;
		if (pressed(basicAllT, Schlag::BASICALL_PARAM))
			basicMask = 0xffu;
		if (basicMask)
			m->basic(basicMask);
		uint32_t rnd = m->rndMask.exchange(0);
		if (pressed(rndAllT, Schlag::RNDALL_PARAM))
			m->roll(0xffu, "SCHLAG RND ALL");
		else if (rnd) // the RND jack: UNDO yes, Ctrl+Z no (a clock there would fill it)
			m->roll(rnd, "SCHLAG RND", Schlag::U_OWN);
		if (pressed(undoT, Schlag::UNDO_PARAM))
			m->undo();
		if (pressed(printT, Schlag::PRINT_PARAM))
			m->print();
		int k = m->kitNext.load();
		if (k != kitSeen) {
			kitSeen = k;
			m->nextKit();
		}
	}

	void appendContextMenu(Menu* menu) override {
		Schlag* m = dynamic_cast<Schlag*>(module);
		if (!m)
			return;
		m->fol.appendMenu(menu, m);
		menu->addChild(createCheckMenuItem("Sends to GENRA FX (without cables)", "", [=]() { return m->sendAuto.load(); },
		                                   [=]() { m->sendAuto = !m->sendAuto.load(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("SAMPLE ROOT FOLDER"));
		menu->addChild(createMenuLabel(rootDir().empty() ? "Not chosen yet" : rootDir()));
		menu->addChild(createMenuItem(rootDir().empty() ? "Choose root folder …" : "Choose another root folder …", "",
		                              []() { chooseRoot(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("SAMPLES"));
		for (int t = 0; t < NT; t++) {
			Bank* b = m->bank[t].load();
			std::string cur = b && b->count() ? b->names[m->baseIndex(t, b)] : (b ? b->error : "");
			menu->addChild(createMenuItem(string::f("%d %s …", t + 1, TRACK_NAMES[t]), cur, [=]() { pickSample(m, t); }));
		}
		menu->addChild(createMenuLabel("(or click a track in the display)"));
	}
};

}  // namespace schlag

// ------------------------------------------------------------------------------------------------ for GRID, GENRA FX
namespace schlag_link {

bool midiSource(int& driver, int& device, int& channel, std::string& label) {
	for (int64_t id : APP->engine->getModuleIds()) {
		Module* m = APP->engine->getModule(id);
		if (!isSchlag(m))
			continue;
		schlag::Schlag* s = static_cast<schlag::Schlag*>(m);
		int d = s->midiIn.getDeviceId();
		if (d < 0)
			continue;
		driver = s->midiIn.getDriverId();
		device = d;
		channel = s->midiIn.getChannel();
		label = s->midiIn.getDeviceName(d);
		return true;
	}
	return false;
}

bool sendFrame(Module* m, int side, float& l, float& r, int64_t frame) {
	if (!isSchlag(m))
		return false;
	schlag::Schlag* s = static_cast<schlag::Schlag*>(m);
	int ol = side ? schlag::Schlag::SENDB_L_OUTPUT : schlag::Schlag::SENDA_L_OUTPUT;
	if (!s->sendAuto.load() || s->outputs[ol].isConnected() || s->outputs[ol + 1].isConnected())
		return false;
	l = s->outputs[ol].getVoltage();
	r = s->outputs[ol + 1].getVoltage();
	s->sendPulledAt[side].store(frame, std::memory_order_relaxed);
	return true;
}

}  // namespace schlag_link

Model* modelSchlag = createModel<schlag::Schlag, schlag::SchlagWidget>("Schlag");
