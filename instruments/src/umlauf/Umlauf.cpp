// SPDX-License-Identifier: GPL-3.0-only
// UMLAUF – an orbit vector synth, the engine of VADER by L'Cronx (github.com/sunskiefer/VADER, MIT) as a VCV Rack
// module in the GENRA look (accent: sith red). A point, the orbiter, travels a shape (or a simulation: a mode) inside a
// square; each of the four corners plays a sound (VADER's overtone generators, one of 14 voices, or a sample), and
// where the orbiter is decides how much of each you hear.
// The engine runs unchanged but for a few marked patches (tools/vader_patch.py) at 44.1 kHz; the module turns knobs
// and CV into its parameters, gates into notes, the clock into its tempo and beat, resamples its output, and puts out
// the orbiter, the corners' weights, the envelope and each corner on its own. UMLAUF FX docks to the right.
// Panels: tools/panels/umlauf_panel.py, tools/panels/umlauf_fx_panel.py.
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "../ui/StateBank.hpp"
#include "UmlaufFx.hpp"
#include "UmlaufLayout.hpp"
#include "UmlaufPresets.h"
#include "VaderApi.hpp"
#include <osdialog.h>
#include <atomic>
#include <cmath>
#include <cstring>
#include <functional>
#include <mutex>

namespace genra_umlauf {

namespace L = ulayout;
namespace ui = genra_ui;
using namespace vader;

// keep in step with tools/panels/umlauf_panel.py (ACC)
#define UMLAUF_RGB 0xff, 0x2e, 0x43
static const NVGcolor ACCENT = nvgRGB(UMLAUF_RGB);
static const int MAXCH = 16;   // the engine's voices
static const int BLOCK = 32;   // frames the engine renders at a time (44.1 kHz)
static const float GAIN = 10.f;

static std::string fwd(std::string s) {
	for (char& c : s)
		if (c == '\\')
			c = '/';
	return s;
}
static std::string baseName(const std::string& p) {
	size_t i = p.find_last_of('/');
	std::string b = i == std::string::npos ? p : p.substr(i + 1);
	size_t d = b.find_last_of('.');
	return d == std::string::npos ? b : b.substr(0, d);
}

// ---------------------------------------------------------------- what a corner's knobs 1–5 move
struct Slot {
	const char* key;  // the parameter after the corner's letter ("og1" → "a_og1")
	const char* name;
	float lo, hi;
	bool snap;
	const char* const* opts;
};
static const Slot SYNTH_SLOTS[5] = {{"og1", "GEN 1", 0, 1, false, nullptr},
                                    {"og2", "GEN 2", 0, 1, false, nullptr},
                                    {"blend", "BLEND", 0, 1, false, nullptr},
                                    {"mode", "B.MODE", 0, 6, true, BLENDS},
                                    {"tune", "G2 TUNE", -24, 24, true, nullptr}};
static const Slot VOICE_SLOTS[5] = {{"v1", "", 0, 1, false, nullptr},
                                    {"v2", "", 0, 1, false, nullptr},
                                    {"v3", "", 0, 1, false, nullptr},
                                    {"v4", "", 0, 1, false, nullptr},
                                    {"lim", "LIMIT", 0, 1, false, nullptr}};
static const Slot WAV_SLOTS[5] = {{"sstart", "START", 0, 1, false, nullptr},
                                  {"send", "END", 0, 1, false, nullptr},
                                  {"slen", "LENGTH", 0, 1, false, nullptr},
                                  {"lim", "LIMIT", 0, 1, false, nullptr},
                                  {nullptr, "", 0, 1, false, nullptr}};  // (the sample's mode: the corner's menu)
static const Slot& slotOf(int engine, int k) {
	return engine == 0 ? SYNTH_SLOTS[k] : engine == NENGINE - 1 ? WAV_SLOTS[k] : VOICE_SLOTS[k];
}
static const char* const SYNTH_NAMES[5] = {"GEN 1", "GEN 2", "BLEND", "B.MODE", "G2 TUNE"};
static const char* const WAV_NAMES[5] = {"START", "END", "LENGTH", "LIMIT", "—"};

struct Umlauf;
struct CornerQuantity : ParamQuantity {
	int corner = 0, slot = 0;
	std::string getLabel() override;
	std::string getDisplayValueString() override;
	std::string getUnit() override { return ""; }
};

// ---------------------------------------------------------------- the module
struct Umlauf : Module, follow::Host {
	enum ParamId {
		SHAPE_PARAM, MODE_PARAM, SPEED_PARAM, SIZE_PARAM, ROT_PARAM, SPIN_PARAM, TILT_PARAM, SUBSIZE_PARAM,
		SUBSPD_PARAM, OENV_PARAM,
		M1_PARAM, M8_PARAM = M1_PARAM + 7,
		CORNER0,  // per corner: ENG, C1..C5, FTYPE, FON, CUT, RES, OCT, LVL
		NUM_PARAMS = CORNER0 + 4 * 12
	};
	enum CornerParam { C_ENG, C_K1, C_K5 = C_K1 + 4, C_FTYPE, C_FON, C_CUT, C_RES, C_OCT, C_LVL, NC };
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, CLK_INPUT, RST_INPUT, X_INPUT, Y_INPUT, SHAPE_INPUT, MODE_INPUT,
		SPEED_INPUT, SIZE_INPUT, ROT_INPUT, SUB_INPUT, M1_INPUT, M4_INPUT = M1_INPUT + 3,
		LVL_A_INPUT, LVL_D_INPUT = LVL_A_INPUT + 3, VOL_INPUT,
		CUT_A_INPUT, CUT_D_INPUT = CUT_A_INPUT + 3, K1_A_INPUT, K1_D_INPUT = K1_A_INPUT + 3,
		NUM_INPUTS
	};
	enum OutputId {
		X_OUTPUT, Y_OUTPUT, W_A_OUTPUT, W_D_OUTPUT = W_A_OUTPUT + 3, ENV_OUTPUT, A_OUTPUT, D_OUTPUT = A_OUTPUT + 3,
		L_OUTPUT, R_OUTPUT, NUM_OUTPUTS
	};
	static int cp(int corner, int k) { return CORNER0 + corner * NC + k; }

	// a parameter of the engine that a knob (main or expander) and/or CV inputs set
	struct Key {
		std::string name;
		float lo, hi;
		bool snap;
		int panel = 0, param = -1;   // the knob (panel 0: this module, 1: the expander), or none
		std::vector<std::pair<int, int>> inputs;  // (panel, input)
		float base = 0.f;            // the knob's value (or the engine's, without a knob)
		float last = NAN;            // the knob's value last seen
		float written = NAN;         // last written to the engine
	};

	const mpc_engine_t* E = mpc_engine();
	void* inst = nullptr;
	overlay_t* bus = nullptr;
	std::vector<Key> keys;
	int kShape = -1, kMode = -1, kDtime = -1, kDsync = -1;
	int kEng[4] = {}, kSmp[4] = {};
	// a corner's knobs 1–5: their key follows the corner's engine
	float cBase[4][5] = {}, cLast[4][5], cWritten[4][5];
	std::string cKey[4][5];
	int engineNow[4] = {-1, -1, -1, -1};
	std::string smpPath[4];          // the sample of each corner (when its engine is WAV)
	int smpWritten[4] = {-1, -1, -1, -1};
	int smpMode[4] = {};             // one shot, loop 1–4 bars
	bool fxWasThere = false;
	bool focusOn = false;
	int ppqn = 4;
	bool delaySync = true;

	// notes
	bool gate[MAXCH] = {};
	int note[MAXCH] = {};
	// clock
	bool clkHigh = false, rstHigh = false, playing = false;
	double clkSamples = 0.0, clkPeriod = 0.0, beats = 0.0, sinceClock = 1e9;
	float tempo = 120.f;
	// resampling: L, R, A, B, C, D at 44.1 kHz
	static const int RING = 1024;
	float ring[6][RING] = {};
	int ringN = 0;
	double pos = 1.0;
	float cbus[4][128];
	ovl_view_t view = {};
	int ctlTick = 0;

	// the lists (the audio thread fills them after a scan; the menus read them)
	std::mutex listMutex;
	std::vector<std::string> shapeFiles, wavFiles;
	std::string userShape;  // the shape file in use (SHAPE: USER)
	bool rescanWanted = true;

	// work for the audio thread (menus, a loaded patch)
	std::mutex cmdMutex;
	std::vector<std::function<void()>> cmds;
	std::atomic<bool> cmdsPending{false};  // (the audio thread looks at this, not the lock, every sample)
	void post(std::function<void()> f) {
		std::lock_guard<std::mutex> lock(cmdMutex);
		cmds.push_back(std::move(f));
		cmdsPending.store(true, std::memory_order_release);
	}

	// for the display (audio thread writes, UI reads)
	struct Disp {
		std::string shape = "STAR", mode = "PATH", mk[8], eng[4], names[4][5], sample[4];
		int voices = 0, preset = 0;
		float tempo = 120.f;
		bool clocked = false;
	};
	Disp disp;
	std::mutex dispMutex;
	// when the panel's displays were last drawn (Rack draws nothing off the screen): the texts only while it is seen
	std::atomic<double> shownAt{-1e9};
	void markShown() { shownAt.store(system::getTime(), std::memory_order_relaxed); }
	bool shown() const { return system::getTime() - shownAt.load(std::memory_order_relaxed) < 0.2; }
	int presetIdx = 0;  // VADER's preset last loaded (the patch keeps it, for UMLAUF before 2.18)
	std::vector<std::string> presetNames;  // VADER's, read once, before the engine runs
	// presets in the header field (StateBank.hpp): UMLAUF's own (UmlaufPresets.h), then VADER's, then the user's
	enum { NOWN = UMLAUF_NPRESETS, NVADER = 22 };
	genra_states::Bank bank;
	std::atomic<int> loadSerial{0}, loadedPos{-1};
	// the engine's whole state, as the audio thread last wrote it (dataToJson reads this, never the engine)
	std::mutex stateMutex;
	std::string stateCache;
	int stateTick = 0;
	// refreshes owed: the engine's state changes only through what the module sends it (and a file it loads in the
	// background, collected a little later: those owe more than one)
	int stateOwed = 1;
	std::vector<char> stateBuf;
	void stateChanged(int n = 2) { stateOwed = std::max(stateOwed, n); }
	/** The engine's state into stateCache; false (still owed) when dataToJson holds the lock just now. */
	bool refreshState(bool wait = false) {
		int need = E->get_param(inst, "state", nullptr, 0);
		if (need <= 0)
			return true;
		stateBuf.assign((size_t) need + 1, 0);
		if (E->get_param(inst, "state", stateBuf.data(), need) <= 0)
			return true;
		std::unique_lock<std::mutex> lock(stateMutex, std::defer_lock);
		if (wait)
			lock.lock();
		else if (!lock.try_lock())
			return false;
		stateCache = stateBuf.data();
		return true;
	}

	// ---------------------------------------------------------------- set up
	/** The user's shapes and samples. Genra/Umlauf; one kept from the separate GENRA SYNTHIS plugin stays in use. */
	std::string userDir() {
		std::string now = asset::user("Genra/Umlauf"), old = asset::user("GenraSynthis/Umlauf");
		return fwd(!system::isDirectory(now) && system::isDirectory(old) ? old : now);
	}

	void prepareFolders() {
		std::string base = userDir(), shapes = base + "/Shapes", samples = base + "/Samples";
		system::createDirectories(shapes);
		system::createDirectories(samples);
		system::createDirectories(base + "/User Presets");
		// the example shapes, the first time
		std::string ex = asset::plugin(pluginInstance, "res/umlauf/shapes");
		if (system::getEntries(shapes).empty() && system::isDirectory(ex))
			for (const std::string& f : system::getEntries(ex))
				system::copy(f, shapes + "/" + system::getFilename(f));
	}

	int addKey(const char* name, float lo, float hi, bool snap, int panel = 0, int param = -1) {
		Key k;
		k.name = name;
		k.lo = lo;
		k.hi = hi;
		k.snap = snap;
		k.panel = panel;
		k.param = param;
		keys.push_back(k);
		return (int) keys.size() - 1;
	}
	void addInput(int key, int panel, int input) { keys[key].inputs.push_back({panel, input}); }

	float engGet(const std::string& key, float def = 0.f) {
		char b[64] = "";
		if (inst && E->get_param(inst, key.c_str(), b, sizeof(b)) > 0)
			return std::strtof(b, nullptr);
		return def;
	}
	void engSet(const std::string& key, float v) {
		char b[32];
		std::snprintf(b, sizeof(b), "%.6f", v);
		E->set_param(inst, key.c_str(), b);
		stateChanged();
	}
	std::string engText(const std::string& key) {
		char b[96] = "";
		if (inst && E->get_param(inst, key.c_str(), b, sizeof(b)) > 0)
			return b;
		return "";
	}

	Umlauf() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		srcName = "UMLAUF";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		fol.bind(CLK_INPUT, follow::CLOCK);
		fol.bind(RST_INPUT, follow::RESET);
		prepareFolders();
		{
			// the engine reads numbers in the "C" locale (as Rack runs)
			std::string shapes = userDir() + "/Shapes";
			inst = E->create(shapes.c_str());
		}
		if (inst)
			bus = (overlay_t*) vader_view_bus(inst);
		for (int c = 0; c < 4; c++)
			for (int k = 0; k < 5; k++)
				cLast[c][k] = cWritten[c][k] = NAN;

		std::vector<std::string> shapes(SHAPES, SHAPES + NSHAPE), modes(MODES, MODES + NMODE);
		auto def = [&](const char* key, float d) { return inst ? engGet(key, d) : d; };
		configSwitch(SHAPE_PARAM, 0.f, NSHAPE - 1, def("shape", 2), "Shape", shapes);
		paramQuantities[SHAPE_PARAM]->description = "The path the orbiter travels (USER: a file of yours, see the menu)";
		configSwitch(MODE_PARAM, 0.f, NMODE - 1, 0.f, "Mode", modes);
		paramQuantities[MODE_PARAM]->description = "PATH: the shape. The others move the orbiter by a simulation; M1–M8 set it";
		configParam(SPEED_PARAM, 0.f, 1.f, def("orb_speed", 0.35f), "Orbit speed", "%", 0.f, 100.f);
		configParam(SIZE_PARAM, 0.f, 1.f, def("orb_size", 0.7f), "Orbit size", "%", 0.f, 100.f);
		configParam(ROT_PARAM, 0.f, 1.f, def("rotate", 0.f), "Rotate", "°", 0.f, 360.f);
		configParam(SPIN_PARAM, 0.f, 1.f, def("spin", 0.2f), "3D spin (3D shapes)", "%", 0.f, 100.f);
		configParam(TILT_PARAM, 0.f, 1.f, def("tilt", 0.3f), "3D tilt (3D shapes)", "%", 0.f, 100.f);
		configParam(SUBSIZE_PARAM, 0.f, 1.f, def("sub_size", 0.3f), "Suborbit size", "%", 0.f, 100.f);
		configParam(SUBSPD_PARAM, 0.f, 1.f, def("sub_speed", 0.45f), "Suborbit speed", "%", 0.f, 100.f);
		configParam(OENV_PARAM, -1.f, 1.f, def("orb_env", 0.f), "Envelope → orbit size", "%", 0.f, 100.f);
		for (int i = 0; i < 8; i++)
			configParam(M1_PARAM + i, 0.f, 1.f, 0.5f, string::f("Mode knob %d", i + 1), "%", 0.f, 100.f);
		std::vector<std::string> engs(ENGINES, ENGINES + NENGINE);
		for (int c = 0; c < 4; c++) {
			std::string n = std::string(1, (char) ('A' + c));
			std::string k = std::string(1, (char) ('a' + c)) + "_";
			configSwitch(cp(c, C_ENG), 0.f, NENGINE - 1, def((k + "eng").c_str(), 0), "Corner " + n + " engine", engs);
			for (int s = 0; s < 5; s++) {
				CornerQuantity* q = configParam<CornerQuantity>(cp(c, C_K1 + s), 0.f, 1.f, 0.5f,
				                                                string::f("Corner %s knob %d", n.c_str(), s + 1));
				q->corner = c;
				q->slot = s;
			}
			configSwitch(cp(c, C_FTYPE), 0.f, 4.f, def((k + "ftype").c_str(), 0), "Corner " + n + " filter",
			             std::vector<std::string>(FTYPES, FTYPES + 5));
			configSwitch(cp(c, C_FON), 0.f, 2.f, def((k + "fpos").c_str(), 0), "Corner " + n + " filter on",
			             std::vector<std::string>(FPOS, FPOS + 3));
			configParam(cp(c, C_CUT), 0.f, 1.f, def((k + "cutoff").c_str(), 0.75f), "Corner " + n + " cutoff", "%", 0.f, 100.f);
			configParam(cp(c, C_RES), 0.f, 1.f, def((k + "reso").c_str(), 0.15f), "Corner " + n + " resonance", "%", 0.f, 100.f);
			configParam(cp(c, C_OCT), -2.f, 2.f, def((k + "oct").c_str(), 0), "Corner " + n + " octave");
			paramQuantities[cp(c, C_OCT)]->snapEnabled = true;
			configParam(cp(c, C_LVL), 0.f, 1.f, def((k + "level").c_str(), 0.8f), "Corner " + n + " level", "%", 0.f, 100.f);
		}

		const char* in[NUM_INPUTS] = {
		    "V/Oct (C4 = 0 V, polyphonic: up to 16 voices)", "Gate (polyphonic)", "Velocity (0–10 V)",
		    "Clock (tempo and beat for the arpeggiator, synced orbits and delay; 16ths by default)",
		    "Reset (the beat starts again)", "X: steer the orbiter by hand (±5 V, with Y: FOCUS)",
		    "Y: steer the orbiter by hand (±5 V, +5 V = top)", "Shape CV (0–10 V over all shapes)",
		    "Mode CV (0–10 V over all modes)", "Orbit speed CV", "Orbit size CV", "Rotate CV", "Suborbit size CV",
		    "Mode knob 1 CV", "Mode knob 2 CV", "Mode knob 3 CV", "Mode knob 4 CV", "Corner A level CV",
		    "Corner B level CV", "Corner C level CV", "Corner D level CV", "Volume CV", "Corner A cutoff CV",
		    "Corner B cutoff CV", "Corner C cutoff CV", "Corner D cutoff CV", "Corner A knob 1 CV",
		    "Corner B knob 1 CV", "Corner C knob 1 CV", "Corner D knob 1 CV"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, in[i]);
		configOutput(X_OUTPUT, "Orbiter X (±5 V)");
		configOutput(Y_OUTPUT, "Orbiter Y (±5 V, +5 V = top)");
		for (int c = 0; c < 4; c++) {
			configOutput(W_A_OUTPUT + c, string::f("Corner %c weight (0–10 V)", 'A' + c));
			configOutput(A_OUTPUT + c, string::f("Corner %c alone (dry, before the effects)", 'A' + c));
		}
		configOutput(ENV_OUTPUT, "Envelope of the newest voice (0–10 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		// ---- the keys: main panel
		kShape = addKey("shape", 0, NSHAPE - 1, true, 0, SHAPE_PARAM);
		addInput(kShape, 0, SHAPE_INPUT);
		kMode = addKey("sim_mode", 0, NMODE - 1, true, 0, MODE_PARAM);
		addInput(kMode, 0, MODE_INPUT);
		addInput(addKey("orb_speed", 0, 1, false, 0, SPEED_PARAM), 0, SPEED_INPUT);
		addInput(addKey("orb_size", 0, 1, false, 0, SIZE_PARAM), 0, SIZE_INPUT);
		addInput(addKey("rotate", 0, 1, false, 0, ROT_PARAM), 0, ROT_INPUT);
		addKey("spin", 0, 1, false, 0, SPIN_PARAM);
		addKey("tilt", 0, 1, false, 0, TILT_PARAM);
		addInput(addKey("sub_size", 0, 1, false, 0, SUBSIZE_PARAM), 0, SUB_INPUT);
		addKey("sub_speed", 0, 1, false, 0, SUBSPD_PARAM);
		addKey("orb_env", -1, 1, false, 0, OENV_PARAM);
		for (int i = 0; i < 8; i++) {
			int k = addKey(string::f("mk%d", i + 1).c_str(), 0, 1, false, 0, M1_PARAM + i);
			if (i < 4)
				addInput(k, 0, M1_INPUT + i);
		}
		for (int c = 0; c < 4; c++) {
			std::string p = std::string(1, (char) ('a' + c)) + "_";
			kEng[c] = addKey((p + "eng").c_str(), 0, 14, true);  // written by corner()
			kSmp[c] = addKey(string::f("smp_%c", 'a' + c).c_str(), 0, 499, true);
			addKey((p + "ftype").c_str(), 0, 4, true, 0, cp(c, C_FTYPE));
			addKey((p + "fpos").c_str(), 0, 2, true, 0, cp(c, C_FON));
			addInput(addKey((p + "cutoff").c_str(), 0, 1, false, 0, cp(c, C_CUT)), 0, CUT_A_INPUT + c);
			addKey((p + "reso").c_str(), 0, 1, false, 0, cp(c, C_RES));
			addKey((p + "oct").c_str(), -2, 2, true, 0, cp(c, C_OCT));
			addInput(addKey((p + "level").c_str(), 0, 1, false, 0, cp(c, C_LVL)), 0, LVL_A_INPUT + c);
		}
		// ---- the expander's
		using namespace ufx;
		auto fx = [&](const char* name, float lo, float hi, bool snap, int param, int input = -1) {
			int k = addKey(name, lo, hi, snap, 1, param);
			if (input >= 0)
				addInput(k, 1, input);
			return k;
		};
		fx("attack", 0, 1, false, ATK, IN_ATK);
		fx("decay", 0, 1, false, DEC, IN_DEC);
		fx("sustain", 0, 1, false, SUS, IN_SUS);
		fx("release", 0, 1, false, REL, IN_REL);
		fx("unison", 0, 3, true, UNISON);
		fx("voices", 0, 1, true, VOICES);
		fx("detune", 0, 1, false, DETUNE, IN_DETUNE);
		fx("glide", 0, 1, false, GLIDE, IN_GLIDE);
		fx("octave", -2, 2, true, TOCT);
		fx("velocity", 0, 1, false, VELO);
		fx("vib_speed", 0, 1, false, VSPD);
		fx("vib_depth", 0, 1, false, VDEP, IN_VIB);
		fx("trem_speed", 0, 1, false, TSPD);
		fx("trem_depth", 0, 1, false, TDEP, IN_TREM);
		fx("arp_on", 0, 1, true, ARPON, IN_ARPON);
		fx("latch", 0, 1, true, LATCH, IN_LATCH);
		fx("arp_mode", 0, 5, true, AMODE);
		fx("arp_rate", 0, 5, true, ARATE, IN_ARATE);
		fx("arp_oct", 1, 4, true, AOCT, IN_AOCT);
		fx("note_len", 0, 1, false, ALEN);
		fx("arp_steps", 1, 16, true, ASTEPS);
		fx("arp_human", 0, 1, false, AHUM);
		for (int i = 0; i < 16; i++)
			fx(string::f("step%d", i + 1).c_str(), 0, 3, true, STEP1 + i);
		fx("drive", 0, 1, false, DRIVE, IN_DRIVE);
		fx("mix_ch", 0, 1, false, CHORUS, IN_CHORUS);
		fx("ch_speed", 0, 1, false, CHSPD);
		fx("mix_dly", 0, 1, false, DELAY, IN_DELAY);
		kDtime = fx("dly_time", 0, 1, false, DTIME, IN_DTIME);  // synced: the division (see dtime())
		fx("dly_fb", 0, 1, false, DFB);
		fx("mix_rev", 0, 1, false, REVERB, IN_REVERB);
		fx("rev_size", 0, 1, false, RSIZE, IN_RSIZE);
		fx("mres", 0, 1, false, MRES, IN_MRES);
		addInput(fx("volume", 0, 1, false, VOLUME, IN_VOL), 0, VOL_INPUT);
		kDsync = addKey("dly_sync", 0, 1, true);
		addKey("focus", 0, 1, true);
		addKey("focus_x", 0, 1, false);
		addKey("focus_y", 0, 1, false);

		if (inst) {
			for (int i = 0; i < NVADER; i++)
				presetNames.push_back(engText(string::f("preset:%d", i)));
			genra_states::Factory f;
			f.count = NOWN + NVADER;
			f.style = [](int i) { return i < NOWN ? UMLAUF_PRESETS[i].style : "VADER"; };
			f.name = [this](int i) { return i < NOWN ? UMLAUF_PRESETS[i].name : presetNames[i - NOWN].c_str(); };
			f.info = [](int i) { return i < NOWN ? UMLAUF_PRESETS[i].info : (i - NOWN >= 16 ? "SAMPLES" : ""); };
			bank.init(this, f, asset::user("Genra/UmlaufPresets"), &loadSerial, &loadedPos,
			          [this](int i) { loadFactory(i); });
			bank.preset = NOWN;  // (INIT ORBIT, as the engine starts)
			bank.presetName = bank.nameOf(NOWN);
			refreshState();
			pullAll(true, false);
			for (int i = 0; i < 8; i++)
				paramQuantities[M1_PARAM + i]->defaultValue = params[M1_PARAM + i].getValue();
			for (int c = 0; c < 4; c++)
				for (int s = 0; s < 5; s++)
					paramQuantities[cp(c, C_K1 + s)]->defaultValue = params[cp(c, C_K1 + s)].getValue();
		}
	}

	~Umlauf() {
		if (inst)
			E->destroy(inst);
	}

	// ---------------------------------------------------------------- engine ↔ knobs
	Module* fxModule() {
		Module* m = rightExpander.module;
		return (m && m->model == modelUmlaufFx) ? m : nullptr;
	}
	Module* panelModule(int panel) { return panel == 0 ? this : fxModule(); }
	int findKey(const std::string& n) {
		for (size_t i = 0; i < keys.size(); i++)
			if (keys[i].name == n)
				return (int) i;
		return -1;
	}
	int cornerEngine(int c) { return clamp((int) std::lround(params[cp(c, C_ENG)].getValue()), 0, NENGINE - 1); }

	/** The engine's value of a corner knob → the knob (0..1). */
	void pullCorner(int c) {
		int e = cornerEngine(c);
		for (int s = 0; s < 5; s++) {
			const Slot& sl = slotOf(e, s);
			if (!sl.key) {
				cKey[c][s].clear();
				continue;
			}
			std::string key = std::string(1, (char) ('a' + c)) + "_" + sl.key;
			float v = engGet(key, sl.lo);
			float n = sl.hi > sl.lo ? (v - sl.lo) / (sl.hi - sl.lo) : 0.f;
			n = clamp(n, 0.f, 1.f);
			params[cp(c, C_K1 + s)].setValue(n);
			cBase[c][s] = cLast[c][s] = n;
			cKey[c][s] = key;
			cWritten[c][s] = v;
		}
	}
	/** The engine → the knobs (after a preset or a restored state, or when the expander docks). */
	void pullAll(bool mainToo, bool fxToo) {
		Module* fx = fxModule();
		for (Key& k : keys) {
			float v = engGet(k.name, k.base);
			if (k.name == "dly_time" && delaySync)
				v = engGet("dly_div", 5) / 6.f;  // the TIME knob picks the division
			k.base = v;
			k.written = (k.name == "dly_time" && delaySync) ? NAN : v;
			if (k.param < 0)
				continue;
			Module* m = k.panel == 0 ? (mainToo ? this : nullptr) : (fxToo ? fx : nullptr);
			if (!m)
				continue;
			m->params[k.param].setValue(v);
			k.last = v;
		}
		if (mainToo) {
			for (int c = 0; c < 4; c++) {
				int e = (int) engGet(string::f("%c_eng", 'a' + c), 0);
				int smp = (int) engGet(string::f("smp_%c", 'a' + c), 0);
				params[cp(c, C_ENG)].setValue(smp > 0 ? NENGINE - 1 : e);
				engineNow[c] = cornerEngine(c);
				smpWritten[c] = smp;
				smpMode[c] = (int) engGet(string::f("smp_len_%c", 'a' + c), 0);
				keys[kEng[c]].written = e;
				pullCorner(c);
			}
		}
	}

	float focusX = NAN, focusY = NAN;  // last written (focus_x, focus_y)
	/** A value moved by CV: sent when it moved more than 1e-4 of its range (CV's noise does not reach the engine
	 * every block: inaudible); a value never written (NAN) always. A knob alone is sent at every change, as before. */
	static bool cvMoved(float v, float written, float range) {
		return !(std::fabs(v - written) <= 1e-4f * std::fabs(range));
	}

	/** Knobs and CV → the engine, once a block. */
	void sync() {
		Module* fx = fxModule();
		if (fx && !fxWasThere)
			pullAll(false, true);
		fxWasThere = fx != nullptr;
		if (fx)
			fx->lights[ufx::LINK_LIGHT].setBrightness(1.f);

		// focus: X / Y steer the orbiter
		bool foc = inputs[X_INPUT].isConnected() || inputs[Y_INPUT].isConnected();
		if (foc != focusOn) {
			focusOn = foc;
			engSet("focus", foc ? 1.f : 0.f);
			focusX = focusY = NAN;
		}
		if (foc) {
			float fx = clamp(0.5f + inputs[X_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			float fy = clamp(0.5f - inputs[Y_INPUT].getVoltage() / 10.f, 0.f, 1.f);
			if (cvMoved(fx, focusX, 1.f)) {
				focusX = fx;
				engSet("focus_x", fx);
			}
			if (cvMoved(fy, focusY, 1.f)) {
				focusY = fy;
				engSet("focus_y", fy);
			}
		}
		if (delaySync != (keys[kDsync].written > 0.5f)) {
			keys[kDsync].written = delaySync ? 1.f : 0.f;
			engSet("dly_sync", keys[kDsync].written);
			keys[kDtime].written = NAN;
		}

		for (size_t i = 0; i < keys.size(); i++) {
			Key& k = keys[i];
			if ((int) i == kEng[0] || (int) i == kEng[1] || (int) i == kEng[2] || (int) i == kEng[3])
				continue;
			if ((int) i == kSmp[0] || (int) i == kSmp[1] || (int) i == kSmp[2] || (int) i == kSmp[3])
				continue;
			if (k.param >= 0) {
				// (the expander not there: its keys keep their value, CV from this panel still adds)
				if (Module* m = panelModule(k.panel)) {
					float v = m->params[k.param].getValue();
					if (v != k.last) {
						k.last = v;
						k.base = v;
					}
				}
			} else if (k.inputs.empty()) {
				continue;
			}
			float cv = 0.f;
			bool byCv = false;
			for (auto& in : k.inputs) {
				Module* m = panelModule(in.first);
				if (m && m->inputs[in.second].isConnected()) {
					cv += m->inputs[in.second].getVoltage() * 0.1f * (k.hi - k.lo);
					byCv = true;
				}
			}
			float v = clamp(k.base + cv, k.lo, k.hi);
			if (k.snap)
				v = std::round(v);
			if ((int) i == kDtime && delaySync) {
				int d = clamp((int) std::lround(clamp(k.base + cv, 0.f, 1.f) * 6.f), 0, 6);
				if ((float) d != k.written) {
					k.written = (float) d;
					engSet("dly_div", (float) d);
				}
				continue;
			}
			if (byCv && !k.snap ? cvMoved(v, k.written, k.hi - k.lo) : v != k.written) {
				k.written = v;
				engSet(k.name, v);
				if ((int) i == kMode) {  // a new mode: its own knob values come back
					for (int j = 0; j < 8; j++) {
						int mk = findKey(string::f("mk%d", j + 1));
						float x = engGet(keys[mk].name, 0.5f);
						params[M1_PARAM + j].setValue(x);
						keys[mk].base = keys[mk].last = keys[mk].written = x;
					}
				}
			}
		}
		// the corners: engine, sample, knobs 1–5
		for (int c = 0; c < 4; c++) {
			int e = cornerEngine(c);
			if (e != engineNow[c]) {
				engineNow[c] = e;
				int ve = e == NENGINE - 1 ? 0 : e;
				if ((float) ve != keys[kEng[c]].written) {
					keys[kEng[c]].written = (float) ve;
					engSet(keys[kEng[c]].name, (float) ve);  // the engine sets a voice's own knob values
				}
				pullCorner(c);
			}
			int smp = 0;
			if (e == NENGINE - 1) {
				std::lock_guard<std::mutex> lock(listMutex);
				for (size_t i = 0; i < wavFiles.size(); i++)
					if (wavFiles[i] == smpPath[c])
						smp = (int) i + 1;
				if (!smp && !wavFiles.empty()) {
					smpPath[c] = wavFiles[0];
					smp = 1;
				}
			}
			if (smp != smpWritten[c]) {
				smpWritten[c] = smp;
				engSet(keys[kSmp[c]].name, (float) smp);
			}
			for (int s = 0; s < 5; s++) {
				const Slot& sl = slotOf(e, s);
				if (!sl.key)
					continue;
				float v = params[cp(c, C_K1 + s)].getValue();
				if (v != cLast[c][s]) {
					cLast[c][s] = v;
					cBase[c][s] = v;
				}
				float n = cBase[c][s];
				bool byCv = s == 0 && inputs[K1_A_INPUT + c].isConnected();
				if (byCv)
					n += inputs[K1_A_INPUT + c].getVoltage() * 0.1f;
				n = clamp(n, 0.f, 1.f);
				float x = sl.lo + n * (sl.hi - sl.lo);
				if (sl.snap)
					x = std::round(x);
				bool moved = byCv && !sl.snap ? cvMoved(x, cWritten[c][s], sl.hi - sl.lo) : x != cWritten[c][s];
				if (moved && !cKey[c][s].empty()) {  // (cKey follows the engine: pullCorner)
					cWritten[c][s] = x;
					engSet(cKey[c][s], x);
				}
			}
		}
	}

	// ---------------------------------------------------------------- notes and clock
	void midi3(int a, int b, int c) {
		uint8_t m[3] = {(uint8_t) a, (uint8_t) b, (uint8_t) c};
		E->midi(inst, m, 3);
	}
	void notes(const ProcessArgs& args) {
		int ch = std::min(MAXCH, std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels()));
		bool velOn = inputs[VEL_INPUT].isConnected();
		for (int c = 0; c < MAXCH; c++) {
			bool g = false;
			if (c < ch) {
				float v = inputs[GATE_INPUT].getPolyVoltage(c);
				g = v >= 1.f ? true : (v <= 0.1f ? false : gate[c]);
			}
			if (!g && !gate[c])
				continue;
			int n = clamp((int) std::lround(60.f + 12.f * inputs[VOCT_INPUT].getPolyVoltage(c)), 0, 127);
			int vel = velOn ? clamp((int) std::lround(inputs[VEL_INPUT].getPolyVoltage(c) * 12.7f), 1, 127) : 100;
			if (g && !gate[c]) {
				midi3(0x90, n, vel);
				note[c] = n;
			} else if (!g && gate[c]) {
				midi3(0x80, note[c], 0);
			} else if (g && n != note[c]) {  // a new pitch under the gate: legato
				midi3(0x90, n, vel);
				midi3(0x80, note[c], 0);
				note[c] = n;
			}
			gate[c] = g;
		}
		// clock → tempo and beat (the arpeggiator, synced orbits, the delay)
		float cl = inputs[CLK_INPUT].getVoltage();
		bool h = cl >= 1.f ? true : (cl <= 0.1f ? false : clkHigh);
		clkSamples += 1.0;
		sinceClock += 1.0;
		bool edge = false;
		if (h && !clkHigh && inputs[CLK_INPUT].isConnected()) {
			if (clkSamples < args.sampleRate * 4.0) {
				clkPeriod = clkPeriod > 0.0 ? 0.6 * clkPeriod + 0.4 * clkSamples : clkSamples;
				double bpm = 60.0 * args.sampleRate / (clkPeriod * ppqn);
				if (bpm >= 20.0 && bpm <= 400.0)
					tempo = (float) bpm;
			}
			clkSamples = 0.0;
			sinceClock = 0.0;
			if (playing)
				beats += 1.0 / ppqn;
			playing = true;
			edge = true;
		}
		clkHigh = h;
		float rs = inputs[RST_INPUT].getVoltage();
		bool rh = rs >= 1.f ? true : (rs <= 0.1f ? false : rstHigh);
		if (rh && !rstHigh) {
			beats = 0.0;
			edge = true;
		}
		rstHigh = rh;
		if (playing && (sinceClock > args.sampleRate * 2.0 || !inputs[CLK_INPUT].isConnected())) {
			playing = false;  // the clock stopped: the engine runs on its own (at the last tempo)
			edge = true;
		}
		if (edge)
			E->set_param(inst, "host_time", string::f("%d %.9f %.6f", playing ? 1 : 0, beats, tempo).c_str());
	}

	// ---------------------------------------------------------------- the display's texts
	void updateDisplay() {
		Disp d;
		d.shape = SHAPES[clamp((int) keys[kShape].written, 0, NSHAPE - 1)];
		if ((int) keys[kShape].written == SHAPE_USER)
			d.shape = userShape.empty() ? "USER (NO FILE)" : baseName(userShape);
		int mode = clamp((int) keys[kMode].written, 0, NMODE - 1);
		d.mode = MODES[mode];
		for (int i = 0; i < 8; i++)
			d.mk[i] = mode ? engText(string::f("mk%d_name", i + 1)) : "";
		for (int c = 0; c < 4; c++) {
			int e = cornerEngine(c);
			d.eng[c] = ENGINES[e];
			for (int s = 0; s < 5; s++) {
				if (e == 0)
					d.names[c][s] = SYNTH_NAMES[s];
				else if (e == NENGINE - 1)
					d.names[c][s] = WAV_NAMES[s];
				else
					d.names[c][s] = s == 4 ? "LIMIT" : engText(string::f("%c_v%d_name", 'a' + c, s + 1));
			}
			d.sample[c] = e == NENGINE - 1 ? (smpPath[c].empty() ? "NO SAMPLE" : baseName(smpPath[c])) : "";
		}
		d.voices = vader_voices_on(inst);
		d.tempo = tempo;
		d.clocked = playing;
		d.preset = presetIdx;
		std::unique_lock<std::mutex> lock(dispMutex, std::try_to_lock);
		if (lock.owns_lock())
			disp = d;
	}

	void readLists() {
		std::vector<std::string> sh, wv;
		for (int i = 0; i < vader_list_count(inst, 0); i++)
			sh.push_back(fwd(vader_list_path(inst, 0, i)));
		for (int i = 0; i < vader_list_count(inst, 1); i++)
			wv.push_back(fwd(vader_list_path(inst, 1, i)));
		std::lock_guard<std::mutex> lock(listMutex);
		shapeFiles = sh;
		wavFiles = wv;
		for (int c = 0; c < 4; c++)
			smpWritten[c] = -1;  // the list's order may have changed
	}

	// ---------------------------------------------------------------- audio
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		if (!inst)
			return;
		if (cmdsPending.load(std::memory_order_acquire)) {
			std::vector<std::function<void()>> todo;
			{
				std::unique_lock<std::mutex> lock(cmdMutex, std::try_to_lock);
				if (lock.owns_lock()) {
					todo.swap(cmds);
					cmdsPending.store(false, std::memory_order_relaxed);
				}
			}
			for (auto& f : todo)
				f();
			if (!todo.empty()) {
				stateTick = 0;
				stateChanged(4);  // (a file may still be loading: looked at again a few times)
			}
		}
		if (rescanWanted) {
			rescanWanted = false;
			engSet("rescan", 1.f);
			readLists();
		}
		notes(args);

		bool corners = false;
		for (int c = 0; c < 4; c++)
			corners |= outputs[A_OUTPUT + c].isConnected();
		double step = 44100.0 / args.sampleRate;
		while ((int) pos + 3 >= ringN) {
			if (ringN > RING - BLOCK) {  // keep the last frames
				int drop = (int) pos - 1;
				for (int c = 0; c < 6; c++)
					std::memmove(ring[c], ring[c] + drop, (ringN - drop) * sizeof(float));
				ringN -= drop;
				pos -= drop;
			}
			sync();
			if (corners) {
				std::memset(cbus, 0, sizeof(cbus));
				vader_set_corner_bus(inst, cbus);
			} else {
				vader_set_corner_bus(inst, nullptr);
			}
			vader_render_f(inst, ring[0] + ringN, ring[1] + ringN, BLOCK);
			for (int c = 0; c < 4; c++) {
				if (corners)
					std::memcpy(ring[2 + c] + ringN, cbus[c], BLOCK * sizeof(float));
				else
					std::memset(ring[2 + c] + ringN, 0, BLOCK * sizeof(float));
			}
			ringN += BLOCK;
			overlay_read_view(bus, &view);
			if (ctlTick < 40)
				ctlTick++;
			if (ctlTick >= 40 && shown()) {  // ~35 times a second, while the panel is seen (at once when it is again)
				ctlTick = 0;
				updateDisplay();
			}
			if (--stateTick <= 0) {  // the state for saving: now and then, when it changed (sooner after a preset, a file)
				stateTick = 44100 * 2 / BLOCK;
				if (stateOwed > 0) {
					if (refreshState())
						stateOwed--;
					else
						stateTick = 8;  // (dataToJson reading it: soon again)
				}
			}
		}
		int i = (int) pos;
		float t = (float) (pos - i);
		auto cubic = [&](const float* y) {
			float y0 = y[i - 1 < 0 ? 0 : i - 1], y1 = y[i], y2 = y[i + 1], y3 = y[i + 2];
			float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
			return ((c3 * t + c2) * t + c1) * t + y1;
		};
		auto clip = [](float v) { return 10.f * std::tanh(v * 0.1f); };
		outputs[L_OUTPUT].setVoltage(clip(GAIN * cubic(ring[0])));
		outputs[R_OUTPUT].setVoltage(clip(GAIN * cubic(ring[1])));
		if (!outputs[R_OUTPUT].isConnected() && !pulled(args.frame)) // L alone: the mono sum (as FUNKE, GRUND, MONO)
			outputs[L_OUTPUT].setVoltage(0.5f * (outputs[L_OUTPUT].getVoltage() + outputs[R_OUTPUT].getVoltage()));
		if (corners)
			for (int c = 0; c < 4; c++)
				outputs[A_OUTPUT + c].setVoltage(clip(GAIN * cubic(ring[2 + c])));
		pos += step;

		outputs[X_OUTPUT].setVoltage(5.f * view.ox);
		outputs[Y_OUTPUT].setVoltage(-5.f * view.oy);
		for (int c = 0; c < 4; c++)
			outputs[W_A_OUTPUT + c].setVoltage(10.f * view.w[c]);
		outputs[ENV_OUTPUT].setVoltage(10.f * view.env);
	}

	// ---------------------------------------------------------------- presets, files
	/** VADER's preset n. */
	void loadPreset(int n) { loadFactory(NOWN + clamp(n, 0, NVADER - 1)); }
	/** Factory preset i of the field's list (on the audio thread): UMLAUF's own are INIT ORBIT and their values. */
	void loadFactory(int i) {
		post([=]() {
			if (i < NOWN) {
				presetIdx = 0;
				engSet("preset", 1.f);  // (the engine loads only a preset other than its own: INIT afresh)
				engSet("preset", 0.f);
				for (const ukv_t* kv = UMLAUF_PRESETS[i].kv; kv->key; kv++)
					engSet(kv->key, kv->v);
			} else {
				presetIdx = i - NOWN;
				engSet("preset", (float) presetIdx);
			}
			pullAll(true, true);
			refreshState(true);  // (the patch, and the undo step, have it at once)
			loadedPos = i;
			loadSerial++;
		});
	}
	void pickShapeFile(const std::string& path) {
		post([=]() {
			std::vector<std::string> files;
			{
				std::lock_guard<std::mutex> lock(listMutex);
				files = shapeFiles;
			}
			for (size_t i = 0; i < files.size(); i++)
				if (files[i] == path) {
					engSet("file", (float) i);
					engSet("load", 1.f);
					userShape = path;
					params[SHAPE_PARAM].setValue(SHAPE_USER);
					return;
				}
		});
	}
	/** A file from anywhere: copied into the Shapes (or Samples) folder, then picked. */
	void importFile(const std::string& src, bool wav, int corner) {
		std::string dir = userDir() + (wav ? "/Samples" : "/Shapes");
		std::string dst = dir + "/" + system::getFilename(src);
		if (fwd(src) != dst)
			system::copy(src, dst);
		post([=]() {
			engSet("rescan", 1.f);
			readLists();
		});
		if (wav)
			post([=]() {
				smpPath[corner] = dst;
				params[cp(corner, C_ENG)].setValue(NENGINE - 1);
			});
		else
			pickShapeFile(dst);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		loadPreset(0);
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "ppqn", json_integer(ppqn));
		json_object_set_new(r, "delaySync", json_boolean(delaySync));
		json_object_set_new(r, "preset", json_integer(presetIdx));
		json_object_set_new(r, "userShape", json_string(userShape.c_str()));
		json_t* s = json_array();
		for (int c = 0; c < 4; c++)
			json_array_append_new(s, json_string(smpPath[c].c_str()));
		json_object_set_new(r, "samples", s);
		{
			std::lock_guard<std::mutex> lock(stateMutex);
			if (!stateCache.empty())
				json_object_set_new(r, "state", json_string(stateCache.c_str()));
		}
		if (inst)
			bank.toJson(r);
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		// a preset or an undo step (StateBank.hpp): the engine's state is the whole sound, the expander follows it
		bool engineWins = json_is_true(json_object_get(r, "engineWins"));
		if (json_t* j = json_object_get(r, "ppqn"))
			ppqn = clamp((int) json_integer_value(j), 1, 24);
		if (json_t* j = json_object_get(r, "delaySync"))
			delaySync = json_boolean_value(j);
		if (json_t* j = json_object_get(r, "preset"))
			presetIdx = clamp((int) json_integer_value(j), 0, NVADER - 1);
		if (inst)  // (a patch from before 2.18 has VADER's number only)
			bank.fromJson(r, json_object_get(r, "preset") ? NOWN + presetIdx : -1);
		if (json_t* j = json_object_get(r, "userShape"))
			if (const char* v = json_string_value(j))
				userShape = v;
		if (json_t* s = json_object_get(r, "samples"))
			for (int c = 0; c < 4 && c < (int) json_array_size(s); c++)
				if (const char* v = json_string_value(json_array_get(s, c)))
					smpPath[c] = v;
		std::string state;
		if (json_t* j = json_object_get(r, "state"))
			if (const char* v = json_string_value(j))
				state = v;
		post([=]() {
			if (!state.empty())
				E->set_param(inst, "state", state.c_str());
			stateChanged(4);
			engSet("rescan", 1.f);
			readLists();
			// the knobs (Rack restored them) are written again over the state
			for (Key& k : keys) {
				k.written = NAN;
				k.last = NAN;
			}
			keys[kDsync].written = NAN;
			for (int c = 0; c < 4; c++) {
				// the corner's engine is in the state already: writing it again would reset a voice's knobs
				keys[kEng[c]].written = engGet(keys[kEng[c]].name, 0);
				engineNow[c] = cornerEngine(c);
				smpMode[c] = (int) engGet(string::f("smp_len_%c", 'a' + c), 0);
				smpWritten[c] = -1;
				for (int s = 0; s < 5; s++) {
					cLast[c][s] = cWritten[c][s] = NAN;
					const Slot& sl = slotOf(engineNow[c], s);
					cKey[c][s] = sl.key ? std::string(1, (char) ('a' + c)) + "_" + sl.key : "";
				}
			}
			fxWasThere = fxModule() != nullptr;  // an expander saved with the patch keeps its own knobs
			if (engineWins && fxWasThere)
				pullAll(false, true);              // … a preset's sound: the expander shows it
			refreshState();
			if (!userShape.empty() && (int) std::lround(params[SHAPE_PARAM].getValue()) == SHAPE_USER)
				pickShapeFile(userShape);
		});
	}
};

std::string CornerQuantity::getLabel() {
	Umlauf* m = dynamic_cast<Umlauf*>(module);
	std::string n = string::f("%c %d", 'A' + corner, slot + 1);
	if (m) {
		m->markShown();  // (asked for: the texts kept up)
		std::lock_guard<std::mutex> lock(m->dispMutex);
		const std::string& s = m->disp.names[corner][slot];
		if (!s.empty())
			return string::f("Corner %c ", 'A' + corner) + s;
	}
	return "Corner " + n;
}
std::string CornerQuantity::getDisplayValueString() {
	Umlauf* m = dynamic_cast<Umlauf*>(module);
	if (!m)
		return ParamQuantity::getDisplayValueString();
	const Slot& sl = slotOf(m->cornerEngine(corner), slot);
	float x = sl.lo + getValue() * (sl.hi - sl.lo);
	if (sl.snap) {
		int i = (int) std::lround(x);
		if (sl.opts)
			return sl.opts[clamp(i, (int) sl.lo, (int) sl.hi)];
		return string::f("%+d", i);
	}
	return string::f("%.0f %%", 100.f * getValue());
}

// ---------------------------------------------------------------- the display
static void glowLine(NVGcontext* vg, const float* xs, const float* ys, int n, bool closed, float w) {
	const float ws[3] = {w * 5.f, w * 2.6f, w};
	const float al[3] = {0.10f, 0.22f, 1.f};
	for (int pass = 0; pass < 3; pass++) {
		nvgBeginPath(vg);
		for (int i = 0; i < n; i++) {
			if (i == 0)
				nvgMoveTo(vg, xs[i], ys[i]);
			else
				nvgLineTo(vg, xs[i], ys[i]);
		}
		if (closed)
			nvgClosePath(vg);
		NVGcolor c = pass == 2 ? nvgRGB(0xff, 0xa8, 0xb2) : ACCENT;
		c.a = al[pass];
		nvgStrokeColor(vg, c);
		nvgStrokeWidth(vg, ws[pass]);
		nvgLineJoin(vg, NVG_ROUND);
		nvgLineCap(vg, NVG_ROUND);
		nvgStroke(vg);
	}
}

struct OrbitDisplay : Widget {
	Umlauf* module = nullptr;
	ovl_shape_t shape;
	unsigned shapeVer = ~0u;
	float px[256], py[256];

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		auto mm = [](float v) { return mm2px(v); };
		Umlauf::Disp d;
		ovl_view_t v = {};
		bool have = false;
		if (module && module->bus) {
			module->markShown();
			{
				std::lock_guard<std::mutex> lock(module->dispMutex);
				d = module->disp;
			}
			overlay_read_view(module->bus, &v);
			unsigned ver = overlay_shape_version(module->bus);
			if (ver != shapeVer && !(ver & 1)) {
				overlay_read_shape(module->bus, &shape);
				shapeVer = ver;
			}
			have = shapeVer != ~0u;
		} else {
			// the module browser: a star
			for (int i = 0; i < 256; i++) {
				float a = 6.2831853f * i / 256.f, r = 0.55f + 0.4f * std::cos(5.f * a);
				shape.px[i] = r * std::cos(a);
				shape.py[i] = r * std::sin(a);
				shape.pz[i] = 0;
			}
			shape.is3d = 0;
			v.size = 0.9f;
			v.ox = 0.35f;
			v.oy = -0.42f;
			v.fx = 0.28f;
			v.fy = -0.3f;
			v.w[0] = 0.4f; v.w[1] = 0.3f; v.w[2] = 0.2f; v.w[3] = 0.1f;
			have = true;
			for (int c = 0; c < 4; c++)
				d.eng[c] = "SYNTH";
		}
		// the square
		float S = mm(30.f), X0 = mm(3.5f), Y0 = mm(3.f);
		auto sx = [&](float x) { return X0 + (x + 1.f) * 0.5f * S; };
		auto sy = [&](float y) { return Y0 + (y + 1.f) * 0.5f * S; };
		nvgBeginPath(vg);
		nvgRect(vg, X0, Y0, S, S);
		nvgStrokeColor(vg, ui::EDGE);
		nvgStrokeWidth(vg, 0.8f);
		nvgStroke(vg);
		nvgScissor(vg, X0 - 2, Y0 - 2, S + 4, S + 4);
		if (have) {
			// the shape, turned (in 3D: spun and tilted), at its size
			float cr = std::cos(v.rot * 6.2831853f), sr = std::sin(v.rot * 6.2831853f), m = 0.f;
			float cy = std::cos(v.yaw), syw = std::sin(v.yaw), cpt = std::cos(v.tilt), spt = std::sin(v.tilt);
			for (int i = 0; i < 256; i++) {
				float x = shape.px[i], y = shape.py[i], z = shape.pz[i], a = x, b = y;
				if (shape.is3d) {
					float x1 = x * cy + z * syw, z1 = -x * syw + z * cy;
					float y1 = y * cpt - z1 * spt, zz = y * spt + z1 * cpt;
					float persp = 1.f / (1.f + 0.25f * zz);
					a = x1 * persp;
					b = y1 * persp;
				}
				px[i] = a * cr - b * sr;
				py[i] = a * sr + b * cr;
				m = std::max(m, std::max(std::fabs(px[i]), std::fabs(py[i])));
			}
			float k = (m > 1.f ? 1.f / m : 1.f) * v.size;
			bool sim = module && d.mode != "PATH";
			if (!sim) {
				float xs[256], ys[256];
				for (int i = 0; i < 256; i++) {
					xs[i] = sx(px[i] * k);
					ys[i] = sy(py[i] * k);
				}
				glowLine(vg, xs, ys, 256, true, 0.9f);
			}
			// the extra balls (a mode's, the kaleidoscope's)
			for (int b = 0; b < v.nballs && b < 16; b++) {
				nvgBeginPath(vg);
				nvgCircle(vg, sx(v.bx[b]), sy(v.by[b]), 1.6f);
				nvgFillColor(vg, nvgRGBA(0xff, 0x8a, 0x96, 0xb0));
				nvgFill(vg);
			}
			// the path point and the orbiter
			if (!sim) {
				nvgBeginPath(vg);
				nvgCircle(vg, sx(v.fx), sy(v.fy), 2.f);
				nvgFillColor(vg, nvgRGB(0xff, 0xb3, 0xbb));
				nvgFill(vg);
			}
			float ox = sx(v.ox), oy = sy(v.oy);
			NVGcolor glow = ACCENT, none = ACCENT;
			glow.a = 0.5f;
			none.a = 0.f;
			nvgBeginPath(vg);
			nvgCircle(vg, ox, oy, 9.f);
			nvgFillPaint(vg, nvgRadialGradient(vg, ox, oy, 2.f, 9.f, glow, none));
			nvgFill(vg);
			nvgBeginPath(vg);
			nvgCircle(vg, ox, oy, 3.4f);
			nvgFillColor(vg, ACCENT);
			nvgFill(vg);
		}
		nvgResetScissor(vg);
		// the corners
		const char* LT = "ABCD";
		float cx[4] = {sx(-1), sx(1), sx(-1), sx(1)}, cyy[4] = {sy(-1), sy(-1), sy(1), sy(1)};
		for (int c = 0; c < 4; c++) {
			nvgBeginPath(vg);
			nvgCircle(vg, cx[c], cyy[c], mm(1.9f));
			nvgFillColor(vg, ui::OFF);
			nvgFill(vg);
			nvgStrokeColor(vg, ACCENT);
			nvgStrokeWidth(vg, 0.9f);
			nvgStroke(vg);
			ui::text(vg, bold, cx[c], cyy[c] + mm(0.6f), std::string(1, LT[c]), 5.f, ui::WHITE, NVG_ALIGN_CENTER, 0.f);
		}
		// the texts
		float tx = X0 + S + mm(5.5f);
		ui::text(vg, bold, tx, mm(6.3f), d.shape, 10.f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);
		std::string sub = "MODE " + d.mode + (d.clocked ? string::f(" · CLOCK %.0f BPM", d.tempo) : "");
		ui::text(vg, bold, tx, mm(10.2f), sub, 5.f, ACCENT, NVG_ALIGN_LEFT, 0.8f);
		bool anyMk = false;
		for (int i = 0; i < 8; i++)
			anyMk |= !d.mk[i].empty();
		if (anyMk) {
			for (int i = 0; i < 8; i++) {
				float x = tx + (i / 4) * mm(30.f), y = mm(15.3f) + (i % 4) * mm(3.5f);
				ui::text(vg, bold, x, y, string::f("M%d", i + 1), 4.4f, ui::DIM, NVG_ALIGN_LEFT, 0.5f);
				ui::text(vg, bold, x + mm(5.f), y, d.mk[i].empty() ? "—" : d.mk[i], 4.4f, ui::TXT, NVG_ALIGN_LEFT, 0.5f);
			}
		} else {
			for (int c = 0; c < 4; c++) {
				float y = mm(15.3f) + c * mm(3.5f);
				ui::text(vg, bold, tx, y, std::string(1, LT[c]), 4.6f, ui::WHITE, NVG_ALIGN_LEFT, 0.f);
				ui::text(vg, bold, tx + mm(3.f), y, d.eng[c] + (d.sample[c].empty() ? "" : " · " + d.sample[c]), 4.4f,
				         ui::TXT, NVG_ALIGN_LEFT, 0.5f);
			}
		}
		for (int c = 0; c < 4; c++) {
			float x = tx + c * mm(15.5f), y = mm(31.6f);
			ui::text(vg, bold, x, y, std::string(1, LT[c]), 4.6f, ui::WHITE, NVG_ALIGN_LEFT, 0.f);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + mm(2.5f), y - mm(1.4f), mm(10.f), mm(1.5f), 1.f);
			nvgFillColor(vg, ui::OFF);
			nvgFill(vg);
			nvgBeginPath(vg);
			nvgRoundedRect(vg, x + mm(2.5f), y - mm(1.4f), mm(10.f) * clamp(v.w[c], 0.f, 1.f), mm(1.5f), 1.f);
			nvgFillColor(vg, ACCENT);
			nvgFill(vg);
		}
		ui::text(vg, bold, box.size.x - mm(3.f), mm(4.6f), string::f("VOICES %d", d.voices), 4.4f, ui::DIM,
		         NVG_ALIGN_RIGHT, 0.6f);
		Widget::drawLayer(args, layer);
	}
};

struct CornerDisplay : Widget {
	Umlauf* module = nullptr;
	int corner = 0;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		auto mm = [](float v) { return mm2px(v); };
		std::string eng = "SYNTH", names[5] = {"GEN 1", "GEN 2", "BLEND", "B.MODE", "G2 TUNE"}, smp;
		if (module) {
			module->markShown();
			std::lock_guard<std::mutex> lock(module->dispMutex);
			eng = module->disp.eng[corner];
			for (int s = 0; s < 5; s++)
				names[s] = module->disp.names[corner][s];
			smp = module->disp.sample[corner];
		}
		if (eng.empty())
			eng = "SYNTH";
		ui::text(vg, bold, mm(2.f), mm(3.3f), eng + (smp.empty() ? "" : " · " + smp), 6.4f, ui::WHITE, NVG_ALIGN_LEFT,
		         0.6f);
		std::string line;
		for (int s = 0; s < 5; s++) {
			if (s)
				line += " · ";
			line += names[s].empty() ? "—" : names[s];
		}
		ui::text(vg, bold, mm(2.f), mm(6.3f), line, 4.1f, ACCENT, NVG_ALIGN_LEFT, 0.4f);
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override;
};

static void cornerMenu(Umlauf* m, Menu* menu, int c) {
	menu->addChild(createMenuLabel(string::f("Corner %c", 'A' + c)));
	for (int e = 0; e < NENGINE; e++)
		menu->addChild(createCheckMenuItem(string::f("%s  —  %s", ENGINES[e], ENGINE_INFO[e]), "",
		                                   [=]() { return m->cornerEngine(c) == e; },
		                                   [=]() { m->params[Umlauf::cp(c, Umlauf::C_ENG)].setValue((float) e); }));
	menu->addChild(new MenuSeparator);
	std::vector<std::string> wavs;
	{
		std::lock_guard<std::mutex> lock(m->listMutex);
		wavs = m->wavFiles;
	}
	menu->addChild(createSubmenuItem("Sample", m->smpPath[c].empty() ? "none" : baseName(m->smpPath[c]), [=](Menu* sub) {
		if (wavs.empty())
			sub->addChild(createMenuLabel("No WAV files in the Samples folder yet"));
		for (const std::string& w : wavs) {
			std::string rel = w;
			std::string root = m->userDir() + "/Samples/";
			if (rel.compare(0, root.size(), root) == 0)
				rel = rel.substr(root.size());
			sub->addChild(createCheckMenuItem(rel, "", [=]() { return m->smpPath[c] == w; }, [=]() {
				m->post([=]() { m->smpPath[c] = w; });
				m->params[Umlauf::cp(c, Umlauf::C_ENG)].setValue((float) (NENGINE - 1));
			}));
		}
	}));
	menu->addChild(createIndexSubmenuItem(
	    "Sample plays", std::vector<std::string>(SMP_MODES, SMP_MODES + 5), [=]() { return (size_t) m->smpMode[c]; },
	    [=](size_t i) {
		    m->smpMode[c] = (int) i;
		    m->post([=]() { m->engSet(string::f("smp_len_%c", 'a' + c), (float) i); });
	    }));
	menu->addChild(createMenuItem("Load a WAV file…", "", [=]() {
		osdialog_filters* f = osdialog_filters_parse("WAV:wav,WAV");
		char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, f);
		osdialog_filters_free(f);
		if (path) {
			m->importFile(path, true, c);
			std::free(path);
		}
	}));
}

void CornerDisplay::onButton(const ButtonEvent& e) {
	if (module && e.action == GLFW_PRESS && (e.button == GLFW_MOUSE_BUTTON_LEFT || e.button == GLFW_MOUSE_BUTTON_RIGHT)) {
		Menu* menu = createMenu();
		cornerMenu(module, menu, corner);
		e.consume(this);
		return;
	}
	Widget::onButton(e);
}

struct UmlaufWidget : ModuleWidget {
	UmlaufWidget(Umlauf* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Umlauf.svg")));
		follow::addStrip(this, module, {mm2px(Vec(L::IN_VOCT[0], L::IN_VOCT[1])), mm2px(Vec(L::IN_GATE[0], L::IN_GATE[1])), mm2px(Vec(L::IN_VEL[0], L::IN_VEL[1])), mm2px(Vec(L::IN_CLK[0], L::IN_CLK[1])), mm2px(Vec(L::IN_RST[0], L::IN_RST[1]))}); // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		genra_states::PresetField* pf = createWidget<genra_states::PresetField>(mm2px(Vec(L::PRESETF[0], L::PRESETF[1])));
		pf->box.size = mm2px(Vec(L::PRESETF[2], L::PRESETF[3]));
		pf->bank = module && module->inst ? &module->bank : nullptr;
		pf->accent = ACCENT;
		pf->sample = UMLAUF_PRESETS[0].name;
		pf->sampleStyle = UMLAUF_PRESETS[0].style;
		addChild(pf);

		OrbitDisplay* d = new OrbitDisplay;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);
		const float* cd[4] = {L::CDISP_A, L::CDISP_B, L::CDISP_C, L::CDISP_D};
		for (int c = 0; c < 4; c++) {
			CornerDisplay* w = new CornerDisplay;
			w->module = module;
			w->corner = c;
			w->box.pos = mm2px(Vec(cd[c][0], cd[c][1]));
			w->box.size = mm2px(Vec(cd[c][2], cd[c][3]));
			addChild(w);
		}

		using M = Umlauf;
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		knob(L::SHAPE, M::SHAPE_PARAM);
		knob(L::MODE, M::MODE_PARAM);
		knob(L::SPEED, M::SPEED_PARAM);
		knob(L::SIZE, M::SIZE_PARAM);
		knob(L::ROT, M::ROT_PARAM);
		knob(L::SPIN, M::SPIN_PARAM);
		knob(L::TILT, M::TILT_PARAM);
		knob(L::SUBSIZE, M::SUBSIZE_PARAM);
		knob(L::SUBSPD, M::SUBSPD_PARAM);
		knob(L::OENV, M::OENV_PARAM);
		const float* mk[8] = {L::M1, L::M2, L::M3, L::M4, L::M5, L::M6, L::M7, L::M8};
		for (int i = 0; i < 8; i++)
			small(mk[i], M::M1_PARAM + i);
		const float* cpos[4][12] = {
		    {L::ENG_A, L::C1_A, L::C2_A, L::C3_A, L::C4_A, L::C5_A, L::FTYPE_A, L::FON_A, L::CUT_A, L::RES_A, L::OCT_A, L::LVL_A},
		    {L::ENG_B, L::C1_B, L::C2_B, L::C3_B, L::C4_B, L::C5_B, L::FTYPE_B, L::FON_B, L::CUT_B, L::RES_B, L::OCT_B, L::LVL_B},
		    {L::ENG_C, L::C1_C, L::C2_C, L::C3_C, L::C4_C, L::C5_C, L::FTYPE_C, L::FON_C, L::CUT_C, L::RES_C, L::OCT_C, L::LVL_C},
		    {L::ENG_D, L::C1_D, L::C2_D, L::C3_D, L::C4_D, L::C5_D, L::FTYPE_D, L::FON_D, L::CUT_D, L::RES_D, L::OCT_D, L::LVL_D}};
		for (int c = 0; c < 4; c++)
			for (int k = 0; k < 12; k++) {
				if (k == 0)
					knob(cpos[c][k], M::cp(c, k));
				else
					small(cpos[c][k], M::cp(c, k));
			}

		const float* ins[M::NUM_INPUTS] = {
		    L::IN_VOCT,  L::IN_GATE,  L::IN_VEL,   L::IN_CLK,   L::IN_RST,   L::IN_X,     L::IN_Y,     L::IN_SHAPE,
		    L::IN_MODE,  L::IN_SPEED, L::IN_SIZE,  L::IN_ROT,   L::IN_SUB,   L::IN_M1,    L::IN_M2,    L::IN_M3,
		    L::IN_M4,    L::IN_LVL_A, L::IN_LVL_B, L::IN_LVL_C, L::IN_LVL_D, L::IN_VOL,   L::IN_CUT_A, L::IN_CUT_B,
		    L::IN_CUT_C, L::IN_CUT_D, L::IN_C1_A,  L::IN_C1_B,  L::IN_C1_C,  L::IN_C1_D};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_X,   L::OUT_Y, L::OUT_W_A, L::OUT_W_B, L::OUT_W_C, L::OUT_W_D,
		                                     L::OUT_ENV, L::OUT_A, L::OUT_B,   L::OUT_C,   L::OUT_D,   L::OUT_L,
		                                     L::OUT_R};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Umlauf* m = getModule<Umlauf>();
		if (!m || !m->inst)
			return;
		menu->addChild(new MenuSeparator);
		genra_states::appendMenu(menu, &m->bank);
		menu->addChild(new MenuSeparator);
		std::vector<std::string> files;
		{
			std::lock_guard<std::mutex> lock(m->listMutex);
			files = m->shapeFiles;
		}
		menu->addChild(createSubmenuItem("Shape file (USER)", m->userShape.empty() ? "none" : baseName(m->userShape),
		                                 [=](Menu* sub) {
			                                 for (const std::string& f : files)
				                                 sub->addChild(createCheckMenuItem(
				                                     system::getFilename(f), "", [=]() { return m->userShape == f; },
				                                     [=]() { m->pickShapeFile(f); }));
			                                 if (files.empty())
				                                 sub->addChild(createMenuLabel("No shape files found"));
		                                 }));
		menu->addChild(createMenuItem("Load a shape file (SVG, OBJ, PNG, JPG)…", "", [=]() {
			osdialog_filters* f = osdialog_filters_parse("Shapes:svg,obj,png,jpg,jpeg");
			char* path = osdialog_file(OSDIALOG_OPEN, nullptr, nullptr, f);
			osdialog_filters_free(f);
			if (path) {
				m->importFile(path, false, 0);
				std::free(path);
			}
		}));
		for (int c = 0; c < 4; c++)
			menu->addChild(createSubmenuItem(string::f("Corner %c", 'A' + c), ENGINES[m->cornerEngine(c)],
			                                 [=](Menu* sub) { cornerMenu(m, sub, c); }));
		menu->addChild(createMenuItem("Open the UMLAUF folder (Shapes, Samples)", "",
		                              [=]() { system::openDirectory(m->userDir()); }));
		menu->addChild(createMenuItem("Scan the folders again", "", [=]() { m->rescanWanted = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem(
		    "Clock input", {"16ths (GENRA)", "8ths", "Quarter notes", "24 PPQN"},
		    [=]() { return (size_t) (m->ppqn == 4 ? 0 : m->ppqn == 2 ? 1 : m->ppqn == 1 ? 2 : 3); },
		    [=](size_t i) { m->ppqn = i == 0 ? 4 : i == 1 ? 2 : i == 2 ? 1 : 24; }));
		menu->addChild(createBoolPtrMenuItem("Delay on the clock (TIME picks the division)", "", &m->delaySync));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Engine: VADER by L'Cronx (MIT) · voices: LXRXXL ZYNTH, Mutable Instruments"));
		menu->addChild(createMenuLabel("Envelope, voice, arpeggiator, effects: UMLAUF FX, placed to the right"));
	}
};

}  // namespace genra_umlauf

Model* modelUmlauf = createModel<genra_umlauf::Umlauf, genra_umlauf::UmlaufWidget>("Umlauf");
