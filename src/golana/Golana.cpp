// SPDX-License-Identifier: GPL-3.0-only
// GOLANA – the Golana firmware's analog-style poly synth (Gadlele, based on Felucca, GPL-3.0) as a VCV Rack module, in
// the GENRA look (accent: the melody track's cyan). The firmware runs unchanged in a private copy of the core library
// (GlCore, glcore/glcore.c); the module turns knobs and CV into its parameters, gates into notes, the clock into its
// tempo, and resamples its 44.1 kHz output. The effects expander GOLANA FX docks to the right: this module reads its
// knobs too. Panels: tools/golana_panel.py, tools/golana_fx_panel.py (res/Golana.svg, res/GolanaFx.svg + layouts).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "../ui/StateBank.hpp"
#include "../OutLimit.hpp"
#include "GlCore.hpp"
#include "GolanaFx.hpp"
#include "GolanaLayout.hpp"
#include "GolanaLevels.hpp"
#include "GolanaUI.hpp"
#include <cstring>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace genra_golana {

namespace L = glayout;
namespace ui = genra_ui;
namespace gu = golana_ui;

// keep in step with tools/golana_panel.py (ACC): the melody track's cyan (= FUNKE)
#define GOLANA_RGB 0x3d, 0xc9, 0xff
static const NVGcolor ACCENT = nvgRGB(GOLANA_RGB);
static const int MAXCH = 4;  // the voices GENRA's melody / pad track sends (Golana plays at most 4 notes)

// the firmware's lists (eng_golana.c N_*, golana_ui.c N_GRES / N_GVOICE)
static const char* const O1WAVE[5] = {"SAW", "PULSE", "TRI", "SINE", "MORPH"};
static const char* const O1INFO[5] = {"CTRL 1: + saw an octave up", "CTRL 1: width", "CTRL 1: fold",
                                      "CTRL 1: cross mod", "CTRL 1: saw to square"};
static const char* const O2WAVE[4] = {"SAW", "SQR", "TRI", "MIX"};
static const char* const O2MOD[4] = {"OFF", "RING", "SYNC", "R+S"};
static const char* const SUBTYPE[8] = {"-1 SQR", "-2 SQR", "-1 SINE", "-2 SINE", "-1 SAW", "-2 SAW", "-1 MIX", "-2 MIX"};
static const char* const FTYPE[5] = {"LPF24", "LPF12", "BPF", "HPF", "MS20"};
static const char* const FINFO[5] = {"2 × SVF · 24 DB", "SVF · 12 DB", "BAND PASS", "HIGH PASS", "MS-20 · 12 DB"};
static const char* const LWAVE[5] = {"SAW", "SQR", "TRI", "SIN", "S&H"};
static const char* const LKEY[3] = {"KEY OFF", "KEY TIMBRE", "KEY VOICE"};
static const char* const LSYNC[11] = {"SYNC OFF", "SYNC 4 BAR", "SYNC 2 BAR", "SYNC 1/1",  "SYNC 1/2", "SYNC 1/4",
                                      "SYNC 1/8", "SYNC 1/8T",  "SYNC 1/16",  "SYNC 1/16T", "SYNC 1/32"};
static const char* const VMODE[4] = {"POLY 4", "MONO", "LEGATO", "UNISON"};
static const char* const PSRC[10] = {"FEG", "AEG", "LFO1", "LFO2", "VELO", "KBD", "MODW", "MW*LFO1", "MW*LFO2", "AFTER"};
static const char* const PDST[7] = {"PITCH", "OSC2", "CTRL1", "NOISE", "CUTOFF", "AMP", "LFO2F"};
static const char* const ARPT[10] = {"UP", "DOWN", "ALT1", "ALT2", "RANDOM", "TRIGGER", "ORDER", "CONVERGE", "PEDAL", "DOUBLE"};
static const char* const ARPRES[6] = {"1/32", "1/24", "1/16", "1/12", "1/8", "1/4"};

enum { MAIN, FX };
enum Kind { PATCH, VIRT };
enum { K_VIRT = 200, NKEYS = K_VIRT + GLV_N };  // keys: the firmware's parameter ids, then GLV_*

struct Bind {
	int panel;   // MAIN or FX
	int param;   // −1: no knob
	int input;   // −1: none
	Kind kind;
	int glk;     // PATCH: GLK_*; VIRT: GLV_*
	int fw;      // PATCH: the firmware's id (from the core)
	int key() const { return kind == VIRT ? K_VIRT + glk : fw; }
};

struct Golana;
/** A knob's value as the firmware writes it ("1.2 kHz", "40 %"). */
struct FwQuantity : ParamQuantity {
	int fw = -1;
	std::string getDisplayValueString() override;
	std::string getUnit() override { return ""; }
};

struct Golana : Module, follow::Host {
	enum ParamId {
		O1WAVE_PARAM, O1C1_PARAM, O1C2_PARAM, DRIFT_PARAM, O2WAVE_PARAM, O2MOD_PARAM, O2SEMI_PARAM, O2TUNE_PARAM,
		MIX1_PARAM, MIX2_PARAM, NOISE_PARAM, SUBTYPE_PARAM, SUB_PARAM, FOLD_PARAM,
		FTYPE_PARAM, CUTOFF_PARAM, RESO_PARAM, EGINT_PARAM, KEYTRK_PARAM, DIST_PARAM,
		FA_PARAM, FD_PARAM, FS_PARAM, FR_PARAM, AA_PARAM, AD_PARAM, AS_PARAM, AR_PARAM,
		L1WAVE_PARAM, L1KEY_PARAM, L1SYNC_PARAM, L1FREQ_PARAM, L2WAVE_PARAM, L2KEY_PARAM, L2SYNC_PARAM, L2FREQ_PARAM,
		VMODE_PARAM, DETUNE_PARAM, PORTA_PARAM,
		P1SRC_PARAM, P1DST_PARAM, P1INT_PARAM, P2SRC_PARAM, P2DST_PARAM, P2INT_PARAM,
		P3SRC_PARAM, P3DST_PARAM, P3INT_PARAM, P4SRC_PARAM, P4DST_PARAM, P4INT_PARAM,
		ARPTYPE_PARAM, ARPRES_PARAM, ARPRANGE_PARAM, ARPSWING_PARAM, ARPON_PARAM,
		MFXAMT_PARAM, DLYAMT_PARAM, REVAMT_PARAM, SPREAD_PARAM, LEVEL_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, ACC_INPUT, SLIDE_INPUT, MOD_INPUT, CLOCK_INPUT, RESET_INPUT,
		CUTOFF_INPUT, RESO_INPUT, EGINT_INPUT, CTRL1_INPUT, FOLD_INPUT, L1FREQ_INPUT, L2FREQ_INPUT, AFTER_INPUT,
		L1RETRIG_INPUT, L2RETRIG_INPUT,
		NUM_INPUTS
	};
	enum OutputId { L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };
	enum LightId { ARPON_LIGHT, NUM_LIGHTS };

	GlCore core;
	std::vector<Bind> binds;
	float last[96] = {};     // per bind: the knob's value last seen
	float base[NKEYS] = {};  // per key: the value the knobs (or the preset) set, firmware units
	int written[NKEYS];      // per key: the value last written to the firmware
	bool fxWasThere = false, pushFx = false, pushMain = false;
	int pushFxTicks = 0;
	int presetIdx = 0;
	float presetGain = 1.f;
	std::atomic<int> presetReq{-1};
	genra_states::Bank bank;
	std::atomic<int> loadSerial{0}, loadedPos{-1};
	int ppqn = 4;  // clock pulses a beat (GENRA's CLOCK: 16ths)

	// notes
	bool gate[MAXCH] = {};
	int note[MAXCH] = {};
	int lastMod = -1, lastPress = -1;
	bool clkHigh = false, rstHigh = false;
	double clkSamples = 0.0, clkPeriod = 0.0;
	dsp::SchmittTrigger retrig[2];
	bool retrigDue[2] = {};
	// resampling the core's 44.1 kHz
	float ring[2][2048] = {};
	int ringN = 0;
	double pos = 1.0;
	int ctlTick = 0;

	// for the screen (audio thread writes, UI reads): the output at 44.1 kHz, the notes that sound
	static const int SCOPE = 4096;
	float scope[SCOPE] = {};
	std::atomic<int> scopeW{0};
	struct Disp {
		int notes[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
		int ftype = 0, cutoff = 64, reso = 20;
		int bpm = 120;
		std::string error;
	};
	Disp disp;
	std::mutex dispMutex;

	Golana() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		srcName = "GOLANA";
		audioL = L_OUTPUT;
		audioR = R_OUTPUT;
		fol.bind(VOCT_INPUT, follow::VOCT);
		fol.bind(GATE_INPUT, follow::GATE);
		fol.bind(VEL_INPUT, follow::VEL);
		fol.bind(ACC_INPUT, follow::ACC);
		fol.bind(SLIDE_INPUT, follow::SLIDE);
		fol.bind(MOD_INPUT, follow::MOD);
		fol.bind(CLOCK_INPUT, follow::CLOCK);
		fol.bind(RESET_INPUT, follow::RESET);
		bool ok = core.load();
		const gl_api* a = core.api;
		if (ok)
			a->init();
		auto fwp = [&](int p, int glk, const char* name) {
			int id = ok ? (int) a->id(glk) : -1;
			float lo = ok ? a->lo(id) : 0.f, hi = ok ? a->hi(id) : 127.f;
			FwQuantity* q = configParam<FwQuantity>(p, lo, hi, ok ? (float) a->get(id) : lo, name);
			q->snapEnabled = true;
			q->fw = id;
		};
		auto sw = [&](int p, int glk, const char* name, const char* const* names, int n) {
			int id = ok ? (int) a->id(glk) : -1;
			configSwitch(p, 0.f, (float) (n - 1), ok ? (float) a->get(id) : 0.f, name, gu::list(names, n));
		};
		sw(O1WAVE_PARAM, GLK_O1WAVE, "OSC 1 wave", O1WAVE, 5);
		fwp(O1C1_PARAM, GLK_O1C1, "OSC 1 CTRL 1 (what it does: the wave says)");
		fwp(O1C2_PARAM, GLK_O1C2, "OSC 1 CTRL 2 (LFO 1 on CTRL 1)");
		fwp(DRIFT_PARAM, GLK_DRIFT, "Drift (analog age)");
		sw(O2WAVE_PARAM, GLK_O2WAVE, "OSC 2 wave", O2WAVE, 4);
		sw(O2MOD_PARAM, GLK_O2MOD, "OSC 2 mod (ring, sync)", O2MOD, 4);
		fwp(O2SEMI_PARAM, GLK_O2SEMI, "OSC 2 semitones");
		fwp(O2TUNE_PARAM, GLK_O2TUNE, "OSC 2 tune");
		fwp(MIX1_PARAM, GLK_MIX1, "OSC 1 level");
		fwp(MIX2_PARAM, GLK_MIX2, "OSC 2 level");
		fwp(NOISE_PARAM, GLK_NOISE, "Noise level");
		sw(SUBTYPE_PARAM, GLK_SUBTYPE, "Sub oscillator", SUBTYPE, 8);
		fwp(SUB_PARAM, GLK_SUB, "Sub level");
		fwp(FOLD_PARAM, GLK_FOLD, "Wavefolder");
		sw(FTYPE_PARAM, GLK_FTYPE, "Filter type", FTYPE, 5);
		fwp(CUTOFF_PARAM, GLK_CUTOFF, "Cutoff");
		fwp(RESO_PARAM, GLK_RESO, "Resonance");
		fwp(EGINT_PARAM, GLK_EGINT, "Filter EG intensity");
		fwp(KEYTRK_PARAM, GLK_KTRK, "Key tracking");
		configSwitch(DIST_PARAM, 0.f, 1.f, 0.f, "Distortion", {"Clean", "Dist"});
		fwp(FA_PARAM, GLK_FA, "Filter EG attack");
		fwp(FD_PARAM, GLK_FD, "Filter EG decay");
		fwp(FS_PARAM, GLK_FS, "Filter EG sustain");
		fwp(FR_PARAM, GLK_FR, "Filter EG release");
		fwp(AA_PARAM, GLK_ATK, "Amp EG attack");
		fwp(AD_PARAM, GLK_DEC, "Amp EG decay");
		fwp(AS_PARAM, GLK_SUS, "Amp EG sustain");
		fwp(AR_PARAM, GLK_REL, "Amp EG release");
		sw(L1WAVE_PARAM, GLK_L1WAVE, "LFO 1 wave", LWAVE, 5);
		sw(L1KEY_PARAM, GLK_L1KEY, "LFO 1 key sync", LKEY, 3);
		sw(L1SYNC_PARAM, GLK_L1SYNC, "LFO 1 tempo sync (CLOCK)", LSYNC, 11);
		fwp(L1FREQ_PARAM, GLK_L1FREQ, "LFO 1 frequency");
		sw(L2WAVE_PARAM, GLK_L2WAVE, "LFO 2 wave", LWAVE, 5);
		sw(L2KEY_PARAM, GLK_L2KEY, "LFO 2 key sync", LKEY, 3);
		sw(L2SYNC_PARAM, GLK_L2SYNC, "LFO 2 tempo sync (CLOCK)", LSYNC, 11);
		fwp(L2FREQ_PARAM, GLK_L2FREQ, "LFO 2 frequency");
		sw(VMODE_PARAM, GLK_VOICE, "Voices", VMODE, 4);
		fwp(DETUNE_PARAM, GLK_DETUNE, "Detune (unison)");
		fwp(PORTA_PARAM, GLK_GLIDE, "Portamento");
		for (int k = 0; k < 4; k++) {
			std::string n = "Patch " + std::to_string(k + 1);
			sw(P1SRC_PARAM + 3 * k, GLK_P1SRC + 3 * k, (n + " source").c_str(), PSRC, 10);
			sw(P1DST_PARAM + 3 * k, GLK_P1DST + 3 * k, (n + " destination").c_str(), PDST, 7);
			fwp(P1INT_PARAM + 3 * k, GLK_P1INT + 3 * k, (n + " intensity").c_str());
		}
		sw(ARPTYPE_PARAM, GLK_ARPTYPE, "Arp type", ARPT, 10);
		configSwitch(ARPRES_PARAM, 0.f, 5.f, ok ? (float) a->vget(GLV_ARPRES) : 2.f, "Arp rate (CLOCK)",
		             gu::list(ARPRES, 6));
		fwp(ARPRANGE_PARAM, GLK_AOCT, "Arp range (octaves)");
		fwp(ARPSWING_PARAM, GLK_ASWING, "Arp swing");
		configSwitch(ARPON_PARAM, 0.f, 1.f, 0.f, "Arpeggiator", {"Off", "On"});
		fwp(MFXAMT_PARAM, GLK_IMIX, "MOD FX amount");
		fwp(DLYAMT_PARAM, GLK_DLY, "Delay send");
		fwp(REVAMT_PARAM, GLK_REV, "Reverb send");
		fwp(SPREAD_PARAM, GLK_SPRD, "Stereo spread of the voices");
		configParam(LEVEL_PARAM, 0.f, 2.f, 1.f, "Level", "%", 0.f, 100.f);

		const char* in[NUM_INPUTS] = {"V/Oct (C4 = 0 V, up to 4 voices)", "Gate (up to 4 voices)", "Velocity (0–10 V)",
		                              "Accent (gate: the note starts louder, GENRA's ACC)",
		                              "Slide (high: glide into the next note, GENRA's SLIDE)",
		                              "Mod wheel (0–10 V, GENRA's MOD; with no patch on it a vibrato)",
		                              "Clock (sets the tempo of the arp, the synced LFOs and the delay; 16ths by default)",
		                              "Reset (the arp and the LFOs start over)", "Cutoff CV", "Resonance CV",
		                              "Filter EG intensity CV", "OSC 1 CTRL 1 CV", "Wavefolder CV", "LFO 1 frequency CV",
		                              "LFO 2 frequency CV", "Aftertouch (0–10 V, a patch source)",
		                              "LFO 1 retrigger (a trigger restarts it)", "LFO 2 retrigger (a trigger restarts it)"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, in[i]);
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		auto b = [&](int panel, int param, int input, Kind k, int g) {
			Bind x{panel, param, input, k, g, 0};
			if (k == PATCH)
				x.fw = ok ? (int) a->id(g) : 0;
			binds.push_back(x);
		};
		b(MAIN, O1WAVE_PARAM, -1, PATCH, GLK_O1WAVE);
		b(MAIN, O1C1_PARAM, CTRL1_INPUT, PATCH, GLK_O1C1);
		b(MAIN, O1C2_PARAM, -1, PATCH, GLK_O1C2);
		b(MAIN, DRIFT_PARAM, -1, PATCH, GLK_DRIFT);
		b(MAIN, O2WAVE_PARAM, -1, PATCH, GLK_O2WAVE);
		b(MAIN, O2MOD_PARAM, -1, PATCH, GLK_O2MOD);
		b(MAIN, O2SEMI_PARAM, -1, PATCH, GLK_O2SEMI);
		b(MAIN, O2TUNE_PARAM, -1, PATCH, GLK_O2TUNE);
		b(MAIN, MIX1_PARAM, -1, PATCH, GLK_MIX1);
		b(MAIN, MIX2_PARAM, -1, PATCH, GLK_MIX2);
		b(MAIN, NOISE_PARAM, -1, PATCH, GLK_NOISE);
		b(MAIN, SUBTYPE_PARAM, -1, PATCH, GLK_SUBTYPE);
		b(MAIN, SUB_PARAM, -1, PATCH, GLK_SUB);
		b(MAIN, FOLD_PARAM, FOLD_INPUT, PATCH, GLK_FOLD);
		b(MAIN, FTYPE_PARAM, -1, PATCH, GLK_FTYPE);
		b(MAIN, CUTOFF_PARAM, CUTOFF_INPUT, PATCH, GLK_CUTOFF);
		b(MAIN, RESO_PARAM, RESO_INPUT, PATCH, GLK_RESO);
		b(MAIN, EGINT_PARAM, EGINT_INPUT, PATCH, GLK_EGINT);
		b(MAIN, KEYTRK_PARAM, -1, PATCH, GLK_KTRK);
		b(MAIN, DIST_PARAM, -1, PATCH, GLK_DIST);
		b(MAIN, FA_PARAM, -1, PATCH, GLK_FA);
		b(MAIN, FD_PARAM, -1, PATCH, GLK_FD);
		b(MAIN, FS_PARAM, -1, PATCH, GLK_FS);
		b(MAIN, FR_PARAM, -1, PATCH, GLK_FR);
		b(MAIN, AA_PARAM, -1, PATCH, GLK_ATK);
		b(MAIN, AD_PARAM, -1, PATCH, GLK_DEC);
		b(MAIN, AS_PARAM, -1, PATCH, GLK_SUS);
		b(MAIN, AR_PARAM, -1, PATCH, GLK_REL);
		b(MAIN, L1WAVE_PARAM, -1, PATCH, GLK_L1WAVE);
		b(MAIN, L1KEY_PARAM, -1, PATCH, GLK_L1KEY);
		b(MAIN, L1SYNC_PARAM, -1, PATCH, GLK_L1SYNC);
		b(MAIN, L1FREQ_PARAM, L1FREQ_INPUT, PATCH, GLK_L1FREQ);
		b(MAIN, L2WAVE_PARAM, -1, PATCH, GLK_L2WAVE);
		b(MAIN, L2KEY_PARAM, -1, PATCH, GLK_L2KEY);
		b(MAIN, L2SYNC_PARAM, -1, PATCH, GLK_L2SYNC);
		b(MAIN, L2FREQ_PARAM, L2FREQ_INPUT, PATCH, GLK_L2FREQ);
		b(MAIN, VMODE_PARAM, -1, PATCH, GLK_VOICE);
		b(MAIN, DETUNE_PARAM, -1, PATCH, GLK_DETUNE);
		b(MAIN, PORTA_PARAM, -1, PATCH, GLK_GLIDE);
		for (int k = 0; k < 4; k++) {
			b(MAIN, P1SRC_PARAM + 3 * k, -1, PATCH, GLK_P1SRC + 3 * k);
			b(MAIN, P1DST_PARAM + 3 * k, -1, PATCH, GLK_P1DST + 3 * k);
			b(MAIN, P1INT_PARAM + 3 * k, -1, PATCH, GLK_P1INT + 3 * k);
		}
		b(MAIN, ARPTYPE_PARAM, -1, PATCH, GLK_ARPTYPE);
		b(MAIN, ARPRES_PARAM, -1, VIRT, GLV_ARPRES);
		b(MAIN, ARPRANGE_PARAM, -1, PATCH, GLK_AOCT);
		b(MAIN, ARPSWING_PARAM, -1, PATCH, GLK_ASWING);
		b(MAIN, ARPON_PARAM, -1, PATCH, GLK_ARPON);
		b(MAIN, MFXAMT_PARAM, -1, PATCH, GLK_IMIX);
		b(MAIN, DLYAMT_PARAM, -1, PATCH, GLK_DLY);
		b(MAIN, REVAMT_PARAM, -1, PATCH, GLK_REV);
		b(MAIN, SPREAD_PARAM, -1, PATCH, GLK_SPRD);
		// the expander's
		b(FX, glfx::MFX_TYPE, -1, PATCH, GLK_MFX);
		b(FX, glfx::CRATE, glfx::IN_CRATE, PATCH, GLK_IA);
		b(FX, glfx::CDEPTH, glfx::IN_CDEPTH, PATCH, GLK_IB);
		b(FX, glfx::DTYPE, -1, PATCH, GLK_DTYPE);
		b(FX, glfx::DTIME, -1, PATCH, GLK_DTIME);
		b(FX, glfx::DFDBK, glfx::IN_DFDBK, PATCH, GLK_DFDBK);
		b(FX, glfx::DCOLOR, glfx::IN_DCOLOR, VIRT, GLV_DCOLOR);
		b(FX, glfx::DMIX, glfx::IN_DMIX, VIRT, GLV_DMIX);
		b(FX, glfx::RTYPE, -1, PATCH, GLK_RTYPE);
		b(FX, glfx::RSIZE, glfx::IN_RSIZE, PATCH, GLK_RSIZE);
		b(FX, glfx::RDAMP, glfx::IN_RDAMP, PATCH, GLK_RDAMP);
		b(FX, glfx::ROUTE, glfx::IN_ROUTE, PATCH, GLK_ROUTE);

		for (int k = 0; k < NKEYS; k++)
			written[k] = INT32_MIN;
		if (ok) {
			imix = a->id(GLK_IMIX);
			dly = a->id(GLK_DLY);
			rev = a->id(GLK_REV);
			glide = a->id(GLK_GLIDE);
			loadPreset(0);
			genra_states::Factory f;
			f.count = (int) a->npresets();
			f.style = [this](int i) { return core.api->preset_style(i); };
			f.name = [this](int i) { return core.api->preset_name(i); };
			f.info = [](int) { return ""; };
			bank.init(this, f, asset::user("Genra/GolanaPresets"), &loadSerial, &loadedPos,
			          [this](int i) { presetReq = clamp(i, 0, (int) core.api->npresets() - 1); });
			bank.preset = presetIdx;
			bank.presetName = bank.nameOf(bank.preset);
		} else {
			disp.error = core.error;
		}
	}
	int imix = -1, dly = -1, rev = -1, glide = -1;  // the firmware's ids of the three FX sums and the portamento

	// ---------------------------------------------------------------- firmware values
	int fwLo(const Bind& b) {
		if (b.kind == VIRT)
			return b.glk == GLV_ARPRES ? 0 : 0;
		return core.api->lo(b.fw);
	}
	int fwHi(const Bind& b) {
		if (b.kind == VIRT)
			return b.glk == GLV_ARPRES ? 5 : 127;
		return core.api->hi(b.fw);
	}
	int readKey(const Bind& b) { return b.kind == VIRT ? core.api->vget(b.glk) : core.api->get(b.fw); }
	void writeKey(const Bind& b, int v) {
		if (b.kind == VIRT)
			core.api->vset(b.glk, v);
		else
			core.api->set(b.fw, v);
	}

	Module* fxModule() {
		Module* m = rightExpander.module;
		return (m && m->model == modelGolanaFx) ? m : nullptr;
	}
	/** The expander's ON for an FX sum (true without the expander: the sound as the preset has it). */
	bool fxOn(Module* fx, int key) {
		if (!fx)
			return true;
		int on = key == imix ? glfx::MFX_ON : key == dly ? glfx::DLY_ON : key == rev ? glfx::REV_ON : -1;
		return on < 0 || fx->params[on].getValue() > 0.5f;
	}

	/** The firmware → every knob (after a preset, or when the expander arrives). */
	void pull(bool mainToo, bool fxToo) {
		Module* fx = fxModule();
		for (size_t i = 0; i < binds.size(); i++) {
			const Bind& b = binds[i];
			int v = readKey(b);
			base[b.key()] = (float) v;
			written[b.key()] = v;
			if (b.param < 0)
				continue;
			Module* m = b.panel == MAIN ? (mainToo ? this : nullptr) : (fxToo ? fx : nullptr);
			if (!m)
				continue;
			m->params[b.param].setValue((float) v);
			last[i] = (float) v;
		}
		if (fxToo && fx)
			for (int k = 0; k < 3; k++)
				fx->params[glfx::ON_PARAMS[k]].setValue(1.f);  // the preset's effects as it has them
	}

	void loadPreset(int i) {
		if (!core.api)
			return;
		int n = (int) core.api->npresets();
		presetIdx = ((i % n) + n) % n;
		core.api->load_preset(presetIdx);
		presetGain = gllevels::gainOf(presetIdx);
		pull(true, true);
		for (int c = 0; c < MAXCH; c++)
			gate[c] = false;  // (the notes held now start again on their next gate)
		loadedPos = presetIdx;
		loadSerial++;
	}

	/** Knobs and CV → the firmware, once per core block. */
	void sync() {
		Module* fx = fxModule();
		if (fx && !fxWasThere) {
			if (pushFx) {
				for (size_t i = 0; i < binds.size(); i++)
					if (binds[i].panel == FX)
						last[i] = NAN;
			} else {
				pull(false, true);
			}
			pushFx = false;
		}
		bool fxChanged = (fx != nullptr) != fxWasThere;
		fxWasThere = fx != nullptr;
		if (pushFx && --pushFxTicks <= 0)
			pushFx = false;
		if (pushMain) {
			for (size_t i = 0; i < binds.size(); i++)
				if (binds[i].panel == MAIN)
					last[i] = NAN;
			pushMain = false;
		}
		// 1. the knobs that moved set their key
		for (size_t i = 0; i < binds.size(); i++) {
			const Bind& b = binds[i];
			Module* m = b.panel == MAIN ? this : fx;
			if (!m || b.param < 0)
				continue;
			float v = m->params[b.param].getValue();
			if (v != last[i]) {
				bool forced = std::isnan(last[i]);
				last[i] = v;
				base[b.key()] = v;
				// a main FX sum turned up switches its effect on in the expander
				if (b.panel == MAIN && fx && v > 0.f && !forced && !fxOn(fx, b.key())) {
					int on = b.key() == imix ? glfx::MFX_ON : b.key() == dly ? glfx::DLY_ON : glfx::REV_ON;
					fx->params[on].setValue(1.f);
				}
			}
		}
		// 2. knob + CV → the firmware (each key once)
		float cv[NKEYS] = {};
		for (const Bind& b : binds) {
			if (b.input < 0)
				continue;
			Module* m = b.panel == MAIN ? this : fx;
			if (!m || !m->inputs[b.input].isConnected())
				continue;
			cv[b.key()] += m->inputs[b.input].getVoltage() * 0.1f * (float) (fwHi(b) - fwLo(b));
		}
		if (glide >= 0 && inputs[SLIDE_INPUT].getVoltage() >= 1.f)
			cv[glide] += 0.35f * 127.f;  // SLIDE high: glide on top of PORTA (GENRA's slides)
		bool done[NKEYS] = {};
		for (const Bind& b : binds) {
			int k = b.key();
			if (done[k])
				continue;
			done[k] = true;
			int v = clamp((int) std::lround(base[k] + cv[k]), fwLo(b), fwHi(b));
			if (!fxOn(fx, k))
				v = 0;  // the expander switched the effect off
			if (v != written[k] || (fxChanged && (k == imix || k == dly || k == rev))) {
				written[k] = v;
				writeKey(b, v);
			}
		}
		// SLIDE without a PORTA knob move: the firmware's value follows the CV too
		if (glide >= 0 && !done[glide]) {
			int v = clamp((int) std::lround(base[glide] + cv[glide]), 0, 127);
			if (v != written[glide]) {
				written[glide] = v;
				core.api->set(glide, v);
			}
		}
	}

	// ---------------------------------------------------------------- notes, clock, MIDI
	void notes(const ProcessArgs& args) {
		const gl_api* a = core.api;
		int ch = std::min(MAXCH, std::max(inputs[VOCT_INPUT].getChannels(), inputs[GATE_INPUT].getChannels()));
		bool velOn = inputs[VEL_INPUT].isConnected();
		for (int c = 0; c < MAXCH; c++) {
			bool g = false;
			if (c < ch) {
				float v = inputs[GATE_INPUT].getPolyVoltage(c);
				g = v >= 1.f ? true : (v <= 0.1f ? false : gate[c]);
			}
			int n = clamp((int) std::lround(60.f + 12.f * inputs[VOCT_INPUT].getPolyVoltage(c)), 0, 127);
			int vel = velOn ? clamp((int) std::lround(inputs[VEL_INPUT].getPolyVoltage(c) * 12.7f), 1, 127) : 100;
			if (g && (!gate[c] || n != note[c]) && inputs[ACC_INPUT].getPolyVoltage(c) >= 1.f)
				vel = std::min(127, vel + 30);  // accent: velocity up
			if (g && !gate[c]) {
				a->midi(0x90, n, vel);
				note[c] = n;
			} else if (!g && gate[c]) {
				a->midi(0x80, note[c], 0);
			} else if (g && n != note[c]) {  // a new pitch under the gate: legato
				a->midi(0x90, n, vel);
				a->midi(0x80, note[c], 0);
				note[c] = n;
			}
			gate[c] = g;
		}
		// reset → the arp and the LFOs start over (before the clock: a reset with a clock edge is no period)
		float rs = inputs[RESET_INPUT].getVoltage();
		bool rh = rs >= 1.f ? true : (rs <= 0.1f ? false : rstHigh);
		if (rh && !rstHigh) {
			a->sync();
			clkSamples = 1e9;
		}
		rstHigh = rh;
		// clock → tempo
		float cl = inputs[CLOCK_INPUT].getVoltage();
		bool h = cl >= 1.f ? true : (cl <= 0.1f ? false : clkHigh);
		clkSamples += 1.0;
		if (h && !clkHigh && inputs[CLOCK_INPUT].isConnected()) {
			if (clkSamples < args.sampleRate * 4.0) {
				clkPeriod = clkPeriod > 0.0 ? 0.6 * clkPeriod + 0.4 * clkSamples : clkSamples;
				double bpm = 60.0 * args.sampleRate / (clkPeriod * ppqn);
				if (bpm >= 40.0 && bpm <= 240.0)
					a->vset(GLV_BPM, (int32_t) std::lround(bpm));
			}
			clkSamples = 0.0;
		}
		clkHigh = h;
		for (int k = 0; k < 2; k++)
			if (retrig[k].process(inputs[L1RETRIG_INPUT + k].getVoltage(), 0.1f, 1.f))
				retrigDue[k] = true;
	}
	void controllers() {
		const gl_api* a = core.api;
		int m = inputs[MOD_INPUT].isConnected() ? clamp((int) std::lround(inputs[MOD_INPUT].getVoltage() * 12.7f), 0, 127) : 0;
		if (m != lastMod) {
			lastMod = m;
			a->midi(0xB0, 1, m);
		}
		int p = inputs[AFTER_INPUT].isConnected() ? clamp((int) std::lround(inputs[AFTER_INPUT].getVoltage() * 12.7f), 0, 127) : 0;
		if (p != lastPress) {
			lastPress = p;
			a->midi(0xD0, p, 0);
		}
		for (int k = 0; k < 2; k++)
			if (retrigDue[k]) {
				retrigDue[k] = false;
				a->retrig(k);
			}
	}

	void updateDisplay() {
		const gl_api* a = core.api;
		std::unique_lock<std::mutex> lock(dispMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		for (int v = 0; v < 8; v++)
			disp.notes[v] = a->voice_note(v);
		disp.ftype = a->get(a->id(GLK_FTYPE));
		disp.cutoff = a->get(a->id(GLK_CUTOFF));
		disp.reso = a->get(a->id(GLK_RESO));
		disp.bpm = a->vget(GLV_BPM);
	}

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }
	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); }

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame);  // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		const gl_api* a = core.api;
		lights[ARPON_LIGHT].setBrightness(params[ARPON_PARAM].getValue() > 0.5f ? 1.f : 0.f);
		if (!a)
			return;
		int req = presetReq.exchange(-1);
		if (req >= 0)
			loadPreset(req);
		notes(args);

		// resample: read the core's frames at 44100 / sampleRate per sample (cubic)
		double step = 44100.0 / args.sampleRate;
		while ((int) pos + 3 >= ringN) {
			if (ringN > 2048 - GL_FRAMES) {  // keep the last frames
				int drop = (int) pos - 1;
				for (int c = 0; c < 2; c++)
					std::memmove(ring[c], ring[c] + drop, (ringN - drop) * sizeof(float));
				ringN -= drop;
				pos -= drop;
			}
			controllers();
			sync();
			const float* f = a->render();
			int w = scopeW.load(std::memory_order_relaxed);
			for (int i = 0; i < GL_FRAMES; i++) {
				ring[0][ringN + i] = f[2 * i];
				ring[1][ringN + i] = f[2 * i + 1];
				scope[(w + i) & (SCOPE - 1)] = 0.5f * (f[2 * i] + f[2 * i + 1]);
			}
			scopeW.store((w + GL_FRAMES) & (SCOPE - 1), std::memory_order_release);
			ringN += GL_FRAMES;
			if (++ctlTick >= 12) {
				ctlTick = 0;
				updateDisplay();
			}
		}
		int i = (int) pos;
		float t = (float) (pos - i);
		float lvl = presetGain * params[LEVEL_PARAM].getValue();
		for (int c = 0; c < 2; c++) {
			float y0 = ring[c][i - 1 < 0 ? 0 : i - 1], y1 = ring[c][i], y2 = ring[c][i + 1], y3 = ring[c][i + 2];
			float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
			float y = ((c3 * t + c2) * t + c1) * t + y1;
			outputs[c ? R_OUTPUT : L_OUTPUT].setVoltage(genra_out::limit(5.f * y * lvl));  // level + safety limiter
		}
		pos += step;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		loadPreset(0);
	}

	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "preset", json_integer(presetIdx));
		json_object_set_new(r, "presetGain", json_real(presetGain));
		json_object_set_new(r, "ppqn", json_integer(ppqn));
		if (core.api) {
			// the whole sound: knob-bound values from the knobs (base: a save while a CV is up must not bake it in),
			// the rest as the firmware has it
			json_t* p = json_array();
			std::vector<char> fromKnob(core.api->np(), 0);
			for (const Bind& b : binds)
				if (b.kind == PATCH && b.fw >= 0 && b.fw < (int) fromKnob.size())
					fromKnob[b.fw] = 1;
			for (uint32_t id = 0; id < core.api->np(); id++)
				json_array_append_new(p, json_integer(fromKnob[id] && std::isfinite(base[id]) ? (int) std::lround(base[id])
				                                                                          : core.api->get(id)));
			json_object_set_new(r, "patch", p);
			json_t* v = json_array();
			for (int k = 0; k < GLV_N; k++)
				json_array_append_new(v, json_integer(std::isfinite(base[K_VIRT + k]) && k != GLV_BPM
				                                          ? (int) std::lround(base[K_VIRT + k])
				                                          : core.api->vget(k)));
			json_object_set_new(r, "virt", v);
			bank.toJson(r);
		}
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		if (json_t* j = json_object_get(r, "preset"))
			presetIdx = std::max(0, (int) json_integer_value(j));
		json_t* gJ = json_object_get(r, "presetGain");
		presetGain = json_is_number(gJ) ? (float) json_number_value(gJ) : gllevels::gainOf(presetIdx);
		bool engineWins = json_is_true(json_object_get(r, "engineWins"));
		if (json_t* j = json_object_get(r, "ppqn"))
			ppqn = clamp((int) json_integer_value(j), 1, 24);
		if (!core.api)
			return;
		core.api->load_preset(presetIdx);  // (a clean start: the sound below replaces it whole)
		if (json_t* p = json_object_get(r, "patch")) {
			for (size_t id = 0; id < json_array_size(p) && id < core.api->np(); id++)
				core.api->set((uint32_t) id, (int32_t) json_integer_value(json_array_get(p, id)));
		}
		if (json_t* v = json_object_get(r, "virt")) {
			for (size_t k = 0; k < json_array_size(v) && k < GLV_N; k++)
				if (k != GLV_BPM)
					core.api->vset((uint32_t) k, (int32_t) json_integer_value(json_array_get(v, k)));
		}
		pull(false, false);
		pushMain = true;
		if (engineWins) {
			pull(false, true);
			pushFx = false;
			fxWasThere = fxModule() != nullptr;
		} else {
			pushFx = true;
			pushFxTicks = 40;
			fxWasThere = false;
		}
		bank.fromJson(r, presetIdx);
	}
};

std::string FwQuantity::getDisplayValueString() {
	Golana* m = dynamic_cast<Golana*>(module);
	if (!m || !m->core.api || fw < 0)
		return ParamQuantity::getDisplayValueString();
	char b[48];
	m->core.api->text(fw, b, sizeof(b));
	return b;
}

// ---------------------------------------------------------------- the screen: the sound's trail and the filter
struct Screen : Widget {
	Golana* module = nullptr;
	static const int N = 256;      // points of a trace
	float trail[5][N] = {};        // the last traces, newest first
	int traces = 0;
	int frame = 0;

	/** A trace of the newest output: one window from a rising zero crossing. */
	bool capture(float* out) {
		if (!module)
			return false;
		const int S = Golana::SCOPE, W = 1024;
		int w = module->scopeW.load(std::memory_order_acquire);
		int start = (w - W - 2048 + 2 * S) & (S - 1);
		int trig = -1;
		for (int i = 1; i < 2048; i++) {
			float a = module->scope[(start + i - 1) & (S - 1)], b = module->scope[(start + i) & (S - 1)];
			if (a <= 0.f && b > 0.f) {
				trig = i;
				break;
			}
		}
		if (trig < 0)
			trig = 2048;
		float peak = 0.02f;
		for (int i = 0; i < W; i++)
			peak = std::max(peak, std::fabs(module->scope[(start + trig + i) & (S - 1)]));
		for (int p = 0; p < N; p++)
			out[p] = module->scope[(start + trig + p * W / N) & (S - 1)] / peak;
		return true;
	}
	void step() override {
		Widget::step();
		if (++frame % 2)
			return;
		float t[N];
		if (capture(t)) {
			for (int k = 4; k > 0; k--)
				std::memcpy(trail[k], trail[k - 1], sizeof(trail[0]));
			std::memcpy(trail[0], t, sizeof(t));
			traces = std::min(5, traces + 1);
		}
	}
	static float response(int type, float f, float fc, float q) {
		float r = f / fc, x = 1.f - r * r, y = r / q;
		float lp2 = 1.f / std::sqrt(x * x + y * y);
		switch (type) {
			case 0: return lp2 * lp2;               // LPF24
			case 2: return y * lp2;                 // BPF
			case 3: return r * r * lp2;             // HPF
			default: return lp2;                    // LPF12, MS20
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		Golana::Disp d;
		if (module) {
			std::lock_guard<std::mutex> lock(module->dispMutex);
			d = module->disp;
		} else {
			d.notes[0] = 48, d.notes[1] = 52, d.notes[2] = 55;
			d.cutoff = 70, d.reso = 60;
		}
		float W = box.size.x, H = box.size.y;
		if (!d.error.empty()) {
			ui::text(vg, bold, mm2px(1.5f), mm2px(5.f), "NO CORE", 6.f, nvgRGB(0xff, 0x45, 0x3a));
			Widget::drawLayer(args, layer);
			return;
		}
		nvgScissor(vg, 0, 0, W, H);
		// the trail: the newest trace bright, older ones fading into green
		float cy = H * 0.42f, amp = H * 0.24f, x0 = mm2px(1.f), w = W - mm2px(2.f);
		int n = module ? traces : 1;
		for (int k = n - 1; k >= 0; k--) {
			nvgBeginPath(vg);
			for (int p = 0; p < N; p++) {
				float v = module ? trail[k][p] : (1.f - 2.f * std::fmod(p / 64.f, 1.f)) * 0.8f;
				float x = x0 + w * p / (N - 1), y = cy - amp * clamp(v, -1.2f, 1.2f) - k * 0.8f;
				if (p == 0)
					nvgMoveTo(vg, x, y);
				else
					nvgLineTo(vg, x, y);
			}
			NVGcolor c = k == 0 ? ACCENT : nvgRGB(0x5d, 0xd3, 0x9e);
			c.a = k == 0 ? 0.95f : 0.45f - 0.08f * k;
			nvgStrokeColor(vg, c);
			nvgStrokeWidth(vg, k == 0 ? 1.1f : 0.8f);
			nvgLineJoin(vg, NVG_ROUND);
			nvgStroke(vg);
		}
		// the filter's curve (cutoff 0..127 over 20 Hz .. 20 kHz, the resonance its peak)
		float fc = 20.f * std::pow(1000.f, d.cutoff / 127.f), q = 0.6f + d.reso / 127.f * (d.ftype == 4 ? 12.f : 8.f);
		float base = H - mm2px(2.6f), span = H * 0.3f;
		nvgBeginPath(vg);
		for (int p = 0; p <= 80; p++) {
			float f = 20.f * std::pow(1000.f, p / 80.f);
			float db = 20.f * std::log10(std::max(1e-4f, response(d.ftype, f, fc, q)));
			float y = base - (clamp(db, -30.f, 12.f) + 30.f) / 42.f * span;
			float x = x0 + w * p / 80.f;
			if (p == 0)
				nvgMoveTo(vg, x, y);
			else
				nvgLineTo(vg, x, y);
		}
		nvgStrokeColor(vg, nvgTransRGBA(ui::TXT, 140));
		nvgStrokeWidth(vg, 0.9f);
		nvgStroke(vg);
		// TRAIL, and the notes that sound
		ui::text(vg, bold, mm2px(1.6f), mm2px(3.2f), "TRAIL", 4.6f, ui::DIM, NVG_ALIGN_LEFT, 1.2f);
		static const char* const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		std::string notes;
		int shown = 0;
		for (int v = 0; v < 8 && shown < 4; v++) {
			int nt = d.notes[v];
			if (nt < 0)
				continue;
			bool dup = false;
			for (int u = 0; u < v; u++)
				dup |= d.notes[u] == nt;
			if (dup)
				continue;
			notes += (notes.empty() ? "" : " ") + std::string(NN[nt % 12]) + std::to_string(nt / 12 - 1);
			shown++;
		}
		ui::text(vg, bold, W - mm2px(1.6f), mm2px(3.2f), notes, 4.6f, ACCENT, NVG_ALIGN_RIGHT, 0.6f);
		nvgResetScissor(vg);
		Widget::drawLayer(args, layer);
	}
};

static void presetMenu(Golana* m) {
	if (!m || !m->core.api)
		return;
	m->bank.scan();
	Menu* menu = createMenu();
	menu->addChild(createMenuLabel("Preset"));
	genra_states::appendList(menu, &m->bank);
}

struct GolanaWidget : ModuleWidget {
	GolanaWidget(Golana* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Golana.svg")));
		auto mmv = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		follow::addStrip(this, module, {mmv(L::IN_VOCT), mmv(L::IN_GATE), mmv(L::IN_VEL), mmv(L::IN_ACC), mmv(L::IN_SLIDE),
		                                mmv(L::IN_MODW), mmv(L::IN_CLOCK), mmv(L::IN_RESET)});  // lit while GENRA feeds it
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Screen* s = new Screen;
		s->module = module;
		s->box.pos = mm2px(Vec(L::SCREEN[0], L::SCREEN[1]));
		s->box.size = mm2px(Vec(L::SCREEN[2], L::SCREEN[3]));
		addChild(s);

		using M = Golana;
		std::vector<std::string> fhints(FINFO, FINFO + 5), o1hints(O1INFO, O1INFO + 5);
		gu::addField(this, module, L::F_O1WAVE, M::O1WAVE_PARAM, gu::list(O1WAVE, 5), o1hints)->hints.clear();
		gu::addField(this, module, L::F_O2WAVE, M::O2WAVE_PARAM, gu::list(O2WAVE, 4));
		gu::addField(this, module, L::F_O2MOD, M::O2MOD_PARAM, gu::list(O2MOD, 4));
		gu::addField(this, module, L::F_SUBTYPE, M::SUBTYPE_PARAM, gu::list(SUBTYPE, 8));
		gu::addField(this, module, L::F_FTYPE, M::FTYPE_PARAM, gu::list(FTYPE, 5), fhints);
		gu::addField(this, module, L::F_L1WAVE, M::L1WAVE_PARAM, gu::list(LWAVE, 5));
		gu::addField(this, module, L::F_L1KEY, M::L1KEY_PARAM, gu::list(LKEY, 3));
		gu::addField(this, module, L::F_L1SYNC, M::L1SYNC_PARAM, gu::list(LSYNC, 11));
		gu::addField(this, module, L::F_L2WAVE, M::L2WAVE_PARAM, gu::list(LWAVE, 5));
		gu::addField(this, module, L::F_L2KEY, M::L2KEY_PARAM, gu::list(LKEY, 3));
		gu::addField(this, module, L::F_L2SYNC, M::L2SYNC_PARAM, gu::list(LSYNC, 11));
		gu::addField(this, module, L::F_VMODE, M::VMODE_PARAM, gu::list(VMODE, 4));
		const float* psrc[4] = {L::F_P1SRC, L::F_P2SRC, L::F_P3SRC, L::F_P4SRC};
		const float* pdst[4] = {L::F_P1DST, L::F_P2DST, L::F_P3DST, L::F_P4DST};
		const float* pint[4] = {L::P1INT, L::P2INT, L::P3INT, L::P4INT};
		for (int k = 0; k < 4; k++) {
			gu::addField(this, module, psrc[k], M::P1SRC_PARAM + 3 * k, gu::list(PSRC, 10));
			gu::addField(this, module, pdst[k], M::P1DST_PARAM + 3 * k, gu::list(PDST, 7));
		}
		gu::addField(this, module, L::F_ARPTYPE, M::ARPTYPE_PARAM, gu::list(ARPT, 10));
		gu::addField(this, module, L::F_ARPRES, M::ARPRES_PARAM, gu::list(ARPRES, 6));

		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		knob(L::O1CTRL1, M::O1C1_PARAM);
		knob(L::O1CTRL2, M::O1C2_PARAM);
		small(L::DRIFT, M::DRIFT_PARAM);
		knob(L::O2SEMI, M::O2SEMI_PARAM);
		knob(L::O2TUNE, M::O2TUNE_PARAM);
		knob(L::MIX1, M::MIX1_PARAM);
		knob(L::MIX2, M::MIX2_PARAM);
		knob(L::NOISE, M::NOISE_PARAM);
		small(L::SUB, M::SUB_PARAM);
		small(L::FOLD, M::FOLD_PARAM);
		addParam(createParamCentered<GLargeKnob>(ui::mm(L::CUTOFF), module, M::CUTOFF_PARAM));
		knob(L::RESO, M::RESO_PARAM);
		knob(L::EGINT, M::EGINT_PARAM);
		small(L::KEYTRK, M::KEYTRK_PARAM);
		addParam(createParamCentered<CKSS>(ui::mm(L::DIST), module, M::DIST_PARAM));
		const float* env[8] = {L::FA, L::FD, L::FS, L::FR, L::AA, L::AD, L::AS, L::AR};
		for (int k = 0; k < 8; k++)
			small(env[k], M::FA_PARAM + k);
		small(L::L1FREQ, M::L1FREQ_PARAM);
		small(L::L2FREQ, M::L2FREQ_PARAM);
		small(L::DETUNE, M::DETUNE_PARAM);
		small(L::PORTA, M::PORTA_PARAM);
		for (int k = 0; k < 4; k++)
			small(pint[k], M::P1INT_PARAM + 3 * k);
		small(L::ARPRANGE, M::ARPRANGE_PARAM);
		small(L::ARPSWING, M::ARPSWING_PARAM);
		addParam(createLightParamCentered<VCVLightBezelLatch<ui::AccentLight<GOLANA_RGB>>>(ui::mm(L::ARPON), module,
		                                                                                  M::ARPON_PARAM, M::ARPON_LIGHT));
		small(L::MFXAMT, M::MFXAMT_PARAM);
		small(L::DLYAMT, M::DLYAMT_PARAM);
		small(L::REVAMT, M::REVAMT_PARAM);
		small(L::SPREAD, M::SPREAD_PARAM);
		small(L::LEVEL, M::LEVEL_PARAM);

		genra_states::PresetField* pf = createWidget<genra_states::PresetField>(mm2px(Vec(L::PFIELD[0], L::PFIELD[1])));
		pf->box.size = mm2px(Vec(L::PFIELD[2], L::PFIELD[3]));
		pf->bank = module && module->core.api ? &module->bank : nullptr;
		pf->accent = ACCENT;
		pf->sample = "WARM POLY KEYS";
		pf->sampleStyle = "SOUL · JAZZ · LO-FI";
		addChild(pf);

		const float* ins[M::NUM_INPUTS] = {L::IN_VOCT,   L::IN_GATE,  L::IN_VEL,   L::IN_ACC,    L::IN_SLIDE,
		                                   L::IN_MODW,   L::IN_CLOCK, L::IN_RESET, L::IN_CUTOFF, L::IN_RESO,
		                                   L::IN_EGINT,  L::IN_CTRL1, L::IN_FOLD,  L::IN_L1FREQ, L::IN_L2FREQ,
		                                   L::IN_AFTER,  L::IN_L1RETRIG, L::IN_L2RETRIG};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_L), module, M::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_R), module, M::R_OUTPUT));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Golana* m = getModule<Golana>();
		if (!m || !m->core.api)
			return;
		const gl_api* a = m->core.api;
		menu->addChild(new MenuSeparator);
		genra_states::appendMenu(menu, &m->bank);
		auto toggle = [&](const char* name, int glk) {
			int id = (int) a->id(glk);
			menu->addChild(createBoolMenuItem(name, "", [=]() { return a->get(id) != 0; },
			                                  [=](bool on) { a->set(id, on ? 1 : 0); }));
		};
		toggle("EG reset (a retrigger starts both envelopes from 0)", GLK_EGRESET);
		toggle("Arp latch (the chord plays on after the keys)", GLK_AHOLD);
		toggle("Arp key sync (the pattern starts with the first key)", GLK_ARPKSYNC);
		menu->addChild(createIndexSubmenuItem(
		    "Arp gate", {"25 %", "50 %", "75 %", "90 %", "100 %"},
		    [=]() {
			    int g = a->get(a->id(GLK_AGATE));
			    return (size_t) (g < 48 ? 0 : g < 80 ? 1 : g < 105 ? 2 : g < 120 ? 3 : 4);
		    },
		    [=](size_t i) {
			    static const int G[5] = {32, 64, 96, 115, 127};
			    a->set(a->id(GLK_AGATE), G[i]);
		    }));
		menu->addChild(createIndexSubmenuItem(
		    "Clock input", {"16ths (GENRA)", "8ths", "Quarter notes", "24 PPQN"},
		    [=]() { return (size_t) (m->ppqn == 4 ? 0 : m->ppqn == 2 ? 1 : m->ppqn == 1 ? 2 : 3); },
		    [=](size_t i) { m->ppqn = i == 0 ? 4 : i == 1 ? 2 : i == 2 ? 1 : 24; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Golana firmware by Gadlele, based on Felucca by Leo Kuroshita (GPL-3.0)"));
		menu->addChild(createMenuLabel("The effects in detail: GOLANA FX, placed to the right"));
	}
};

}  // namespace genra_golana

/** For the expander's tooltips: the firmware's text for the value of its knob fxParam, from the GOLANA module m. */
bool golanaFxText(Module* m, int fxParam, char* out, int n) {
	genra_golana::Golana* g = dynamic_cast<genra_golana::Golana*>(m);
	if (!g || !g->core.api)
		return false;
	for (const genra_golana::Bind& b : g->binds)
		if (b.panel == genra_golana::FX && b.param == fxParam) {
			if (b.kind == genra_golana::PATCH)
				g->core.api->text((uint32_t) b.fw, out, (uint32_t) n);
			else
				snprintf(out, n, "%d", (int) g->core.api->vget((uint32_t) b.glk));
			return true;
		}
	return false;
}

Model* modelGolana = createModel<genra_golana::Golana, genra_golana::GolanaWidget>("Golana");
