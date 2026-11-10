// SPDX-License-Identifier: GPL-3.0-only
// K-WASUB – the K-WASUB firmware's modern bass synth (Omnipotard, GPL-3.0) as a VCV Rack module, in the GENRA look
// (accent: acid green). The firmware runs unchanged in a private copy of the core library (KwCore); the module turns
// knobs and CV into its parameters, gates into notes, the clock into its tempo, and resamples its 44.1 kHz output.
// The effects expander K-WASUB FX docks to the right: this module reads its knobs too.
// Panels: tools/kwasub_panel.py (res/Kwasub.svg, res/KwasubFx.svg + KwLayout.hpp, KwFxLayout.hpp).
#include "../plugin.hpp"
#include "../Follow.hpp"
#include "../ui/GenraUI.hpp"
#include "../ui/StateBank.hpp"
#include "../OutLimit.hpp"
#include "KwasubLevels.hpp"
#include "KwCore.hpp"
#include "KwIds.hpp"
#include "KwLayout.hpp"
#include "KwasubFx.hpp"
#include <cstring>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace genra_kwasub {

namespace L = kwlayout;
namespace ui = genra_ui;
using namespace kwid;

// keep in step with tools/kwasub_panel.py (ACC)
#define KWASUB_RGB 0xb8, 0xf2, 0x3a
static const NVGcolor ACCENT = nvgRGB(KWASUB_RGB);
static const int MAXCH = 4;  // the firmware's voices

static const char* const ENG_INFO[NENG] = {"SUBTRACTIVE", "SAW + PULSE", "8 TABLES", "4 OPERATORS", "CZ STYLE", "",
                                           "MASTER/SLAVE", "", "KICK BASS", "FORMANT FM", "16 PARTIALS"};

// ---------------------------------------------------------------- what the knobs move
enum { MAIN, FX };
enum Kind { PATCH, ENGINE, LDIV, EQF, EQG, EQON, OTON };
enum { K_EQ = 200, K_EQON = K_EQ + 8, K_OTON, NKEYS };  // keys: 0..NP-1 the patch, then the EQ and the virtual ONs

struct Bind {
	int panel;   // MAIN or FX
	int param;   // −1: no knob (a CV input alone, on the preset's value)
	int input;   // −1: none
	Kind kind;
	int fw;      // the patch id (PATCH, ENGINE, LDIV) or the EQ band (EQF, EQG)
	int key() const {
		switch (kind) {
			case EQF: return K_EQ + fw * 2;
			case EQG: return K_EQ + fw * 2 + 1;
			case EQON: return K_EQON;
			case OTON: return K_OTON;
			default: return fw;
		}
	}
};

struct Kwasub;
/** A knob's value as the firmware writes it ("3.2 kHz", "1/8T", "GROWL 55%"). */
struct FwQuantity : ParamQuantity {
	int fw = 0;
	std::string getDisplayValueString() override;
	std::string getUnit() override { return ""; }
};

struct Kwasub : Module, follow::Host {
	enum ParamId {
		ENG_A, A_P1_PARAM, A_P2_PARAM, A_P3_PARAM, A_P4_PARAM, A_LVL_PARAM, A_OCT_PARAM, A_SEMI_PARAM, A_FINE_PARAM,
		ENG_B, B_P1_PARAM, B_P2_PARAM, B_P3_PARAM, B_P4_PARAM, B_LVL_PARAM, B_OCT_PARAM, B_SEMI_PARAM, B_FINE_PARAM,
		FTYPE_PARAM, CUT_PARAM, RES_PARAM, DRV_PARAM, FENV_PARAM, KEY_PARAM, FEA_PARAM, FED_PARAM,
		SUB_PARAM, NOISE_PARAM, XMOD_PARAM,
		LRATE_PARAM, LDEP_PARAM, LSHP_PARAM, LDST_PARAM,
		AEA_PARAM, AED_PARAM, AES_PARAM, AER_PARAM,
		SAT_PARAM, DLY_PARAM, VERB_PARAM, GLUE_PARAM,
		PREV_PARAM, NEXT_PARAM,
		NUM_PARAMS
	};
	enum InputId {
		VOCT_INPUT, GATE_INPUT, VEL_INPUT, CLOCK_INPUT, RESET_INPUT,
		CUT_INPUT, RES_INPUT, DRV_INPUT, FENV_INPUT, ASHP_INPUT, ACOL_INPUT, BSHP_INPUT, BCOL_INPUT, WOB_INPUT, DEP_INPUT,
		PRESET_INPUT,
		SUB_INPUT, NOISE_INPUT, XMOD_INPUT, ATK_INPUT, REL_INPUT, SAT_INPUT, DLY_INPUT, VERB_INPUT,
		MOD_INPUT, PRESS_INPUT, BEND_INPUT, GLIDE_INPUT,
		ACC_INPUT, SLIDE_INPUT, // 2.13 (GENRA strip)
		NUM_INPUTS
	};
	enum OutputId { ENV_OUTPUT, LFO_OUTPUT, L_OUTPUT, R_OUTPUT, NUM_OUTPUTS };

	KwCore core;
	std::vector<Bind> binds;
	float last[160] = {};    // per bind: the knob's value last seen
	float base[NKEYS] = {};  // per key: the value the knobs (or the preset) set, firmware units
	int written[NKEYS];      // per key: the value last written to the firmware
	bool fxWasThere = false, pushFx = false, pushMain = false;
	int pushFxTicks = 0;  // a saved patch: the expander's saved knobs win if it is there within these blocks
	int presetIdx = 0;
	float presetGain = 1.f;  // the preset's output level (KwasubLevels.hpp; a user preset keeps the one it was saved with)
	std::atomic<int> presetReq{-1};  // a preset picked in the UI, loaded by the engine thread
	// the presets sorted by style, then name (the list, the wheel and PREV / NEXT go in this order)
	std::vector<int> order;
	std::vector<std::string> styleOf;
	std::once_flag orderOnce;
	const std::vector<int>& sorted() {
		if (core.api)
			std::call_once(orderOnce, [&]() {
				int n = (int) core.api->npresets();
				for (int i = 0; i < n; i++) {
					order.push_back(i);
					styleOf.push_back(core.api->preset_style(i));
				}
				std::stable_sort(order.begin(), order.end(), [&](int x, int y) {
					if (styleOf[x] != styleOf[y]) return styleOf[x] < styleOf[y];
					return std::string(core.api->preset_name(x)) < core.api->preset_name(y);
				});
			});
		return order;
	}
	/** The preset d steps away from the current one in the sorted order. */
	int stepPreset(int d) {
		const std::vector<int>& o = sorted();
		if (o.empty()) return presetIdx;
		int pos = (int) (std::find(o.begin(), o.end(), presetIdx) - o.begin());
		int n = (int) o.size();
		return o[((pos + d) % n + n) % n];
	}
	/** Where firmware preset k stands in the sorted order (the field's numbers). */
	int posOf(int k) {
		const std::vector<int>& o = sorted();
		int p = (int) (std::find(o.begin(), o.end(), k) - o.begin());
		return p < (int) o.size() ? p : -1;
	}
	// presets in the header field (StateBank.hpp): the sorted factory list, then the user presets
	genra_states::Bank bank;
	std::atomic<int> loadSerial{0}, loadedPos{-1};  // each preset the engine loads: its place in the sorted list
	dsp::BooleanTrigger prevTrig, nextTrig;
	int ppqn = 4;  // clock pulses a beat (GENRA's CLOCK: 16ths)

	// notes
	bool gate[MAXCH] = {};
	int note[MAXCH] = {};
	int lastBend = 8192, lastMod = -1, lastPress = -1;
	// clock
	bool clkHigh = false, rstHigh = false;
	double clkSamples = 0.0, clkPeriod = 0.0;
	int presetCvLast = -1;
	// resampling the core's 44.1 kHz
	float ring[2][2048] = {};
	int ringN = 0;     // frames in the ring (from ring start)
	double pos = 1.0;  // read position in frames
	int ctlTick = 0;

	// for the display (audio thread writes, UI reads)
	struct Disp {
		char preset[24] = "", style[24] = "", engA[24] = "", engB[24] = "", pa[3][40] = {}, pb[3][40] = {};
		char wob[48] = "", mode[16] = "";
		int idx = 0, n = 97, voices = 0, bpm10 = 1400, lshape = 0;
		float phase = 0.f;
		std::string error;
	};
	Disp disp;
	std::mutex dispMutex;

	Kwasub() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, 0);
		srcName = "K-WASUB";
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
		const kw_api* a = core.api;
		if (ok)
			a->init();
		auto fwp = [&](int p, int id, const char* name, const char* unit = "") {
			float lo = ok ? a->lo(id) : 0.f, hi = ok ? a->hi(id) : 127.f;
			FwQuantity* q = configParam<FwQuantity>(p, lo, hi, ok ? (float) a->get(id) : lo, name, unit);
			q->snapEnabled = true;
			q->fw = id;
		};
		std::vector<std::string> engs;
		for (int i = 0; i < 9; i++)
			engs.push_back(ENG_NAMES[ENG_ORDER[i]]);
		for (int o = 0; o < 2; o++) {
			int p0 = o ? ENG_B : ENG_A, f0 = o ? B_ENG : A_ENG;
			std::string n = o ? "B " : "A ";
			configSwitch(p0, 0.f, 8.f, o ? 0.f : 2.f, "Osc " + n + "engine", engs);
			fwp(p0 + 1, f0 + 2, ("Osc " + n + "shape (P1)").c_str());
			fwp(p0 + 2, f0 + 3, ("Osc " + n + "color (P2)").c_str());
			fwp(p0 + 3, f0 + 4, ("Osc " + n + "edge (P3)").c_str());
			fwp(p0 + 4, o ? (int) B_P4 : (int) A_P4, ("Osc " + n + "fold / ring / algo (P4)").c_str());
			fwp(p0 + 5, f0 + 1, ("Osc " + n + "level").c_str());
			fwp(p0 + 6, f0 + 5, ("Osc " + n + "octave").c_str());
			fwp(p0 + 7, f0 + 6, ("Osc " + n + "semitones").c_str());
			fwp(p0 + 8, f0 + 7, ("Osc " + n + "fine").c_str());
		}
		configSwitch(FTYPE_PARAM, 0.f, 6.f, 0.f, "Filter type",
		             std::vector<std::string>(FILTER_NAMES, FILTER_NAMES + 7));
		fwp(CUT_PARAM, F_CUT, "Cutoff");
		fwp(RES_PARAM, F_RES, "Resonance");
		fwp(DRV_PARAM, F_DRV, "Filter drive");
		fwp(FENV_PARAM, F_ENV, "Filter envelope amount");
		fwp(KEY_PARAM, F_KEY, "Key tracking");
		fwp(FEA_PARAM, FE_A, "Filter envelope attack");
		fwp(FED_PARAM, FE_D, "Filter envelope decay");
		fwp(SUB_PARAM, S_LVL, "Sub level");
		fwp(NOISE_PARAM, N_LVL, "Noise level");
		fwp(XMOD_PARAM, X_AMT, "X-mod amount");
		fwp(LRATE_PARAM, L1_DIV, "Wobble rate (on the tempo)");
		fwp(LDEP_PARAM, L1_DEP, "Wobble depth");
		fwp(LSHP_PARAM, L1_SHP, "Wobble shape");
		fwp(LDST_PARAM, L1_DST, "Wobble destination");
		fwp(AEA_PARAM, AE_A, "Amp attack");
		fwp(AED_PARAM, AE_D, "Amp decay");
		fwp(AES_PARAM, AE_S, "Amp sustain");
		fwp(AER_PARAM, AE_R, "Amp release");
		fwp(SAT_PARAM, DR_AMT, "Saturation amount");
		fwp(DLY_PARAM, DL_MIX, "Delay mix");
		fwp(VERB_PARAM, RV_MIX, "Reverb mix");
		fwp(GLUE_PARAM, MS_GLUE, "Glue");
		configButton(PREV_PARAM, "Previous preset");
		configButton(NEXT_PARAM, "Next preset");

		const char* in[NUM_INPUTS] = {"V/Oct (C4 = 0 V, up to 4 voices)", "Gate", "Velocity (0–10 V)",
		                              "Clock (sets the tempo; 16ths by default)", "Reset (the bar starts)", "Cutoff CV",
		                              "Resonance CV", "Filter drive CV", "Filter envelope CV", "Osc A shape CV",
		                              "Osc A color CV", "Osc B shape CV", "Osc B color CV", "Wobble rate CV",
		                              "Wobble depth CV", "Preset CV (0–10 V over all presets)", "Sub CV", "Noise CV",
		                              "X-mod CV", "Amp attack CV", "Amp release CV", "Saturation CV", "Delay mix CV",
		                              "Reverb mix CV", "Mod wheel (0–10 V)", "Pressure (0–10 V)", "Pitch bend (±5 V)",
		                              "Glide CV", "Accent (gate: the note starts louder and brighter, GENRA's ACC)",
		                              "Slide (high: glide into the next note, GENRA's SLIDE)"};
		for (int i = 0; i < NUM_INPUTS; i++)
			configInput(i, in[i]);
		configOutput(ENV_OUTPUT, "Amp envelope of the newest voice (0–10 V)");
		configOutput(LFO_OUTPUT, "Wobble LFO (±5 V)");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");

		// the main panel's binds
		auto b = [&](int panel, int param, int input, Kind k, int fw) { binds.push_back({panel, param, input, k, fw}); };
		b(MAIN, ENG_A, -1, ENGINE, A_ENG);
		b(MAIN, A_P1_PARAM, ASHP_INPUT, PATCH, A_P1);
		b(MAIN, A_P2_PARAM, ACOL_INPUT, PATCH, A_P2);
		b(MAIN, A_P3_PARAM, -1, PATCH, A_P3);
		b(MAIN, A_P4_PARAM, -1, PATCH, A_P4);
		b(MAIN, A_LVL_PARAM, -1, PATCH, A_LVL);
		b(MAIN, A_OCT_PARAM, -1, PATCH, A_OCT);
		b(MAIN, A_SEMI_PARAM, -1, PATCH, A_SEMI);
		b(MAIN, A_FINE_PARAM, -1, PATCH, A_FINE);
		b(MAIN, ENG_B, -1, ENGINE, B_ENG);
		b(MAIN, B_P1_PARAM, BSHP_INPUT, PATCH, B_P1);
		b(MAIN, B_P2_PARAM, BCOL_INPUT, PATCH, B_P2);
		b(MAIN, B_P3_PARAM, -1, PATCH, B_P3);
		b(MAIN, B_P4_PARAM, -1, PATCH, B_P4);
		b(MAIN, B_LVL_PARAM, -1, PATCH, B_LVL);
		b(MAIN, B_OCT_PARAM, -1, PATCH, B_OCT);
		b(MAIN, B_SEMI_PARAM, -1, PATCH, B_SEMI);
		b(MAIN, B_FINE_PARAM, -1, PATCH, B_FINE);
		b(MAIN, FTYPE_PARAM, -1, PATCH, F_TYPE);
		b(MAIN, CUT_PARAM, CUT_INPUT, PATCH, F_CUT);
		b(MAIN, RES_PARAM, RES_INPUT, PATCH, F_RES);
		b(MAIN, DRV_PARAM, DRV_INPUT, PATCH, F_DRV);
		b(MAIN, FENV_PARAM, FENV_INPUT, PATCH, F_ENV);
		b(MAIN, KEY_PARAM, -1, PATCH, F_KEY);
		b(MAIN, FEA_PARAM, -1, PATCH, FE_A);
		b(MAIN, FED_PARAM, -1, PATCH, FE_D);
		b(MAIN, SUB_PARAM, SUB_INPUT, PATCH, S_LVL);
		b(MAIN, NOISE_PARAM, NOISE_INPUT, PATCH, N_LVL);
		b(MAIN, XMOD_PARAM, XMOD_INPUT, PATCH, X_AMT);
		b(MAIN, LRATE_PARAM, WOB_INPUT, LDIV, L1_DIV);
		b(MAIN, LDEP_PARAM, DEP_INPUT, PATCH, L1_DEP);
		b(MAIN, LSHP_PARAM, -1, PATCH, L1_SHP);
		b(MAIN, LDST_PARAM, -1, PATCH, L1_DST);
		b(MAIN, AEA_PARAM, ATK_INPUT, PATCH, AE_A);
		b(MAIN, AED_PARAM, -1, PATCH, AE_D);
		b(MAIN, AES_PARAM, -1, PATCH, AE_S);
		b(MAIN, AER_PARAM, REL_INPUT, PATCH, AE_R);
		b(MAIN, SAT_PARAM, SAT_INPUT, PATCH, DR_AMT);
		b(MAIN, DLY_PARAM, DLY_INPUT, PATCH, DL_MIX);
		b(MAIN, VERB_PARAM, VERB_INPUT, PATCH, RV_MIX);
		b(MAIN, GLUE_PARAM, -1, PATCH, MS_GLUE);
		b(MAIN, -1, GLIDE_INPUT, PATCH, V_GLIDE);
		// the expander's
		using namespace kwfx;
		b(FX, kwfx::DR_ON, -1, PATCH, kwid::DR_ON);
		b(FX, kwfx::DR_TYPE, -1, PATCH, kwid::DR_TYPE);
		b(FX, kwfx::DR_AMT, IN_SAT, PATCH, kwid::DR_AMT);
		b(FX, kwfx::DR_MIX, -1, PATCH, kwid::DR_MIX);
		b(FX, kwfx::CR_ON, -1, PATCH, kwid::CR_ON);
		b(FX, kwfx::CR_BITS, -1, PATCH, kwid::CR_BITS);
		b(FX, kwfx::CR_RATE, -1, PATCH, kwid::CR_RATE);
		b(FX, kwfx::CR_MIX, IN_CRUSH, PATCH, kwid::CR_MIX);
		b(FX, kwfx::CH_ON, -1, PATCH, kwid::CH_ON);
		b(FX, kwfx::CH_RATE, -1, PATCH, kwid::CH_RATE);
		b(FX, kwfx::CH_DEP, -1, PATCH, kwid::CH_DEP);
		b(FX, kwfx::CH_MIX, IN_CHORUS, PATCH, kwid::CH_MIX);
		b(FX, kwfx::PH_ON, -1, PATCH, kwid::PH_ON);
		b(FX, kwfx::PH_RATE, -1, PATCH, kwid::PH_RATE);
		b(FX, kwfx::PH_DEP, -1, PATCH, kwid::PH_DEP);
		b(FX, kwfx::PH_FB, -1, PATCH, kwid::PH_FB);
		b(FX, kwfx::PH_MIX, IN_PHASER, PATCH, kwid::PH_MIX);
		b(FX, kwfx::DL_ON, -1, PATCH, kwid::DL_ON);
		b(FX, kwfx::DL_TYPE, -1, PATCH, kwid::DL_TYPE);
		b(FX, kwfx::DL_TIME, IN_DTIME, PATCH, kwid::DL_TIME);
		b(FX, kwfx::DL_FB, IN_DFB, PATCH, kwid::DL_FB);
		b(FX, kwfx::DL_TONE, -1, PATCH, kwid::DL_TONE);
		b(FX, kwfx::DL_MIX, IN_DMIX, PATCH, kwid::DL_MIX);
		b(FX, kwfx::RV_ON, -1, PATCH, kwid::RV_ON);
		b(FX, kwfx::RV_TYPE, -1, PATCH, kwid::RV_TYPE);
		b(FX, kwfx::RV_SIZE, IN_VSIZE, PATCH, kwid::RV_SIZE);
		b(FX, kwfx::RV_DAMP, -1, PATCH, kwid::RV_DAMP);
		b(FX, kwfx::RV_MIX, IN_VMIX, PATCH, kwid::RV_MIX);
		b(FX, kwfx::CP_ON, -1, PATCH, kwid::CP_ON);
		b(FX, kwfx::CP_AMT, IN_COMP, PATCH, kwid::CP_AMT);
		b(FX, kwfx::CP_ATK, -1, PATCH, kwid::CP_ATK);
		b(FX, kwfx::CP_REL, -1, PATCH, kwid::CP_REL);
		b(FX, kwfx::CP_PUMP, IN_PUMP, PATCH, kwid::CP_PUMP);
		b(FX, kwfx::CP_PRATE, -1, PATCH, kwid::CP_PRATE);
		b(FX, kwfx::OT_ON, -1, OTON, 0);
		b(FX, kwfx::OT_AMT, IN_OTT, PATCH, kwid::OT_AMT);
		b(FX, kwfx::OT_LOW, -1, PATCH, kwid::OT_LOW);
		b(FX, kwfx::OT_HIGH, -1, PATCH, kwid::OT_HIGH);
		b(FX, kwfx::EQ_ON, -1, EQON, 0);
		for (int k = 0; k < 4; k++) {
			b(FX, kwfx::EQ1_F + 2 * k, -1, EQF, k);
			b(FX, kwfx::EQ1_G + 2 * k, k == 0 ? IN_EQLO : (k == 3 ? IN_EQHI : -1), EQG, k);
		}
		b(FX, kwfx::MS_GLUE, IN_GLUE, PATCH, kwid::MS_GLUE);
		b(FX, kwfx::MS_SUB, IN_SUBB, PATCH, kwid::MS_SUB);
		b(FX, kwfx::MB_MONO, -1, PATCH, kwid::MB_MONO);

		for (int k = 0; k < NKEYS; k++)
			written[k] = INT32_MIN;
		if (ok) {
			loadPreset(0);
			sorted();
			genra_states::Factory f;
			f.count = (int) order.size();
			f.style = [this](int i) { return styleOf[order[i]].c_str(); };
			f.name = [this](int i) { return core.api->preset_name(order[i]); };
			f.info = [](int) { return ""; };
			bank.init(this, f, asset::user("Genra/KwasubPresets"), &loadSerial, &loadedPos,
			          [this](int i) { presetReq = order[clamp(i, 0, (int) order.size() - 1)]; });
			bank.preset = posOf(presetIdx);
			bank.presetName = bank.nameOf(bank.preset);
		} else {
			disp.error = core.error;
		}
	}

	// ---------------------------------------------------------------- firmware values
	int fwLo(const Bind& b) {
		switch (b.kind) {
			case ENGINE: return 0;
			case EQF: return 0;
			case EQG: return -12;
			case EQON: case OTON: return 0;
			default: return core.api->lo(b.fw);
		}
	}
	int fwHi(const Bind& b) {
		switch (b.kind) {
			case ENGINE: return 8;
			case EQF: return 47;
			case EQG: return 12;
			case EQON: case OTON: return 1;
			default: return core.api->hi(b.fw);
		}
	}
	/** The key's value in the firmware now (ENGINE: as the field's index). */
	int readKey(const Bind& b) {
		const kw_api* a = core.api;
		switch (b.kind) {
			case ENGINE: {
				int e = a->get(b.fw);
				for (int i = 0; i < 9; i++)
					if (ENG_ORDER[i] == e)
						return i;
				return 0;
			}
			case EQF: return a->eq_get(b.fw, KW_EQ_FREQ);
			case EQG: return a->eq_get(b.fw, KW_EQ_GAIN);
			case EQON: {
				for (int k = 0; k < 4; k++)
					if (a->eq_get(k, KW_EQ_TYPE) != EQ_OFF)
						return 1;
				return 0;
			}
			case OTON: return a->get(kwid::OT_AMT) > 0 ? 1 : 0;
			default: return a->get(b.fw);
		}
	}
	void writeKey(const Bind& b, int v) {
		const kw_api* a = core.api;
		switch (b.kind) {
			case ENGINE: a->set(b.fw, ENG_ORDER[clamp(v, 0, 8)]); break;
			case EQF: a->eq_set(b.fw, KW_EQ_FREQ, v); break;
			case EQG: a->eq_set(b.fw, KW_EQ_GAIN, v); break;
			case EQON: {
				static const int T[4] = {EQ_LOSH, EQ_BELL, EQ_BELL, EQ_HISH};
				for (int k = 0; k < 4; k++)
					a->eq_set(k, KW_EQ_TYPE, v ? T[k] : EQ_OFF);
			} break;
			case OTON: break;  // (OT_AMT reads it)
			case LDIV:
				a->set(b.fw, v);
				a->set(kwid::L1_SYNC, 1);
				break;
			default: a->set(b.fw, v); break;
		}
	}

	Module* fxModule() {
		Module* m = rightExpander.module;
		return (m && m->model == modelKwasubFx) ? m : nullptr;
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
	}

	void loadPreset(int i) {
		if (!core.api)
			return;
		int n = (int) core.api->npresets();
		presetIdx = ((i % n) + n) % n;
		core.api->load_preset(presetIdx);
		presetGain = kwlevels::gainOf(presetIdx);
		pull(true, true);
		loadedPos = posOf(presetIdx);
		loadSerial++;
	}

	/** Knobs and CV → the firmware, once per core block. */
	void sync() {
		Module* fx = fxModule();
		if (fx && !fxWasThere) {
			if (pushFx) {
				for (size_t i = 0; i < binds.size(); i++)
					if (binds[i].panel == FX)
						last[i] = NAN;  // every knob of the expander counts as moved
			} else {
				pull(false, true);
			}
			pushFx = false;
		}
		fxWasThere = fx != nullptr;
		if (pushFx && --pushFxTicks <= 0)
			pushFx = false;  // (no expander in the patch: one placed later takes the sound's values)
		if (pushMain) {
			for (size_t i = 0; i < binds.size(); i++)
				if (binds[i].panel == MAIN)
					last[i] = NAN;
			pushMain = false;
		}
		// 1. the knobs that moved set their key; the other knobs on the same key follow
		bool moved[NKEYS] = {};
		int mover[NKEYS];
		for (size_t i = 0; i < binds.size(); i++) {
			const Bind& b = binds[i];
			if (b.param < 0)
				continue;
			Module* m = b.panel == MAIN ? this : fx;
			if (!m)
				continue;
			float v = m->params[b.param].getValue();
			if (v != last[i]) {
				bool forced = std::isnan(last[i]); // pushed after loading a patch: not a turn of the knob
				last[i] = v;
				base[b.key()] = v;
				moved[b.key()] = true;
				mover[b.key()] = (int) i;
				// the main panel's FX knobs switch their effect on
				if (b.panel == MAIN && v > 0.f && !forced) {
					int on = b.fw == DR_AMT ? (int) DR_ON : b.fw == DL_MIX ? (int) DL_ON : b.fw == RV_MIX ? (int) RV_ON : -1;
					if (on >= 0 && base[on] < 0.5f) {
						base[on] = 1.f;
						moved[on] = true;
						mover[on] = -1;
					}
				}
			}
		}
		for (size_t i = 0; i < binds.size(); i++) {
			const Bind& b = binds[i];
			int k = b.key();
			if (!moved[k] || mover[k] == (int) i || b.param < 0)
				continue;
			Module* m = b.panel == MAIN ? this : fx;
			if (!m)
				continue;
			m->params[b.param].setValue(base[k]);
			last[i] = base[k];
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
		// SLIDE high: glide on top of the GLIDE knob (GENRA's slides)
		if (inputs[SLIDE_INPUT].getVoltage() >= 1.f)
			for (const Bind& b : binds)
				if (b.kind == PATCH && b.fw == kwid::V_GLIDE) {
					cv[b.key()] += 0.35f * (float) (fwHi(b) - fwLo(b));
					break;
				}
		bool done[NKEYS] = {};
		for (const Bind& b : binds) {
			int k = b.key();
			if (done[k])
				continue;
			done[k] = true;
			int v = clamp((int) std::lround(base[k] + cv[k]), fwLo(b), fwHi(b));
			if (b.kind == PATCH && b.fw == kwid::OT_AMT && base[K_OTON] < 0.5f)
				v = 0;  // MULTIBAND off
			if (v != written[k]) {
				written[k] = v;
				writeKey(b, v);
			}
			if (b.kind == OTON && moved[k]) {  // MULTIBAND switched: OT_AMT again
				written[kwid::OT_AMT] = INT32_MIN;
			}
		}
	}

	// ---------------------------------------------------------------- notes, clock, MIDI
	void notes(const ProcessArgs& args) {
		const kw_api* a = core.api;
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
				vel = std::min(127, vel + 30); // accent: velocity up (louder, the filter envelope opens further)
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
		// reset → the bar (first: with a clock edge in the same sample, the gap before the reset is no period)
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
				if (bpm >= 30.0 && bpm <= 300.0)
					a->set_bpm10((uint32_t) std::lround(bpm * 10.0));
			}
			clkSamples = 0.0;
		}
		clkHigh = h;
	}
	void controllers() {
		const kw_api* a = core.api;
		if (inputs[BEND_INPUT].isConnected()) {
			int b = clamp((int) std::lround(8192.f + inputs[BEND_INPUT].getVoltage() / 5.f * 8191.f), 0, 16383);
			if (std::abs(b - lastBend) > 4) {
				lastBend = b;
				a->midi(0xE0, b & 127, b >> 7);
			}
		} else if (lastBend != 8192) {
			lastBend = 8192;
			a->midi(0xE0, 0, 64);
		}
		int m = inputs[MOD_INPUT].isConnected() ? clamp((int) std::lround(inputs[MOD_INPUT].getVoltage() * 12.7f), 0, 127) : 0;
		if (m != lastMod) {
			lastMod = m;
			a->midi(0xB0, 1, m);
		}
		int p = inputs[PRESS_INPUT].isConnected() ? clamp((int) std::lround(inputs[PRESS_INPUT].getVoltage() * 12.7f), 0, 127) : 0;
		if (p != lastPress) {
			lastPress = p;
			a->midi(0xD0, p, 0);
		}
		if (inputs[PRESET_INPUT].isConnected()) {
			int n = (int) a->npresets();
			int k = clamp((int) std::lround(inputs[PRESET_INPUT].getVoltage() / 10.f * (n - 1)), 0, n - 1);
			if (k != presetCvLast) {
				presetCvLast = k;
				loadPreset(k);
			}
		} else {
			presetCvLast = -1;
		}
	}

	void updateDisplay() {
		const kw_api* a = core.api;
		std::unique_lock<std::mutex> lock(dispMutex, std::try_to_lock);
		if (!lock.owns_lock())
			return;
		auto cpy = [](char* d, const char* s, size_t n) {
			std::strncpy(d, s ? s : "", n - 1);
			d[n - 1] = 0;
		};
		disp.idx = presetIdx;
		disp.n = (int) a->npresets();
		cpy(disp.preset, a->preset_name(presetIdx), sizeof(disp.preset));
		a->text(P_STYLE, disp.style, sizeof(disp.style));
		cpy(disp.engA, ENG_NAMES[a->get(A_ENG) % NENG], sizeof(disp.engA));
		cpy(disp.engB, ENG_NAMES[a->get(B_ENG) % NENG], sizeof(disp.engB));
		for (int k = 0; k < 3; k++) {
			char v[24];
			int ida = A_P1 + k, idb = B_P1 + k;
			a->text(ida, v, sizeof(v));
			snprintf(disp.pa[k], sizeof(disp.pa[k]), "%s %s", a->sname(ida), v);
			a->text(idb, v, sizeof(v));
			snprintf(disp.pb[k], sizeof(disp.pb[k]), "%s %s", a->sname(idb), v);
		}
		char d[16], sh[16];
		a->text(L1_DIV, d, sizeof(d));
		a->text(L1_SHP, sh, sizeof(sh));
		snprintf(disp.wob, sizeof(disp.wob), "WOB %s · %s", a->get(L1_SYNC) ? d : "FREE", sh);
		a->text(V_MODE, disp.mode, sizeof(disp.mode));
		disp.voices = (int) a->voices();
		disp.bpm10 = (int) a->bpm10();
		disp.lshape = a->get(L1_SHP);
		disp.phase = a->lfo_phase();
	}

	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	void onPortChange(const PortChangeEvent& e) override { fol.portChange(e); } // a real cable wins (Follow.hpp)

	void process(const ProcessArgs& args) override {
		fol.process(this, args.frame); // FOLLOW GENRA: the jacks without a cable (Follow.hpp)
		const kw_api* a = core.api;
		if (!a)
			return;
		int req = presetReq.exchange(-1);
		if (req >= 0)
			loadPreset(req);
		// PREV / NEXT have no buttons any more (the preset field took their place), but MIDI-Map can still use them
		if (prevTrig.process(params[PREV_PARAM].getValue() > 0.f))
			loadPreset(stepPreset(-1));
		if (nextTrig.process(params[NEXT_PARAM].getValue() > 0.f))
			loadPreset(stepPreset(1));
		notes(args);

		// resample: read the core's frames at 44100 / sampleRate per sample (cubic)
		double step = 44100.0 / args.sampleRate;
		while ((int) pos + 3 >= ringN) {
			if (ringN > 2048 - KW_FRAMES) {  // keep the last 4 frames
				int drop = (int) pos - 1;
				for (int c = 0; c < 2; c++)
					std::memmove(ring[c], ring[c] + drop, (ringN - drop) * sizeof(float));
				ringN -= drop;
				pos -= drop;
			}
			controllers();
			sync();
			const float* f = a->render();
			for (int i = 0; i < KW_FRAMES; i++) {
				ring[0][ringN + i] = f[2 * i];
				ring[1][ringN + i] = f[2 * i + 1];
			}
			ringN += KW_FRAMES;
			if (++ctlTick >= 6) {
				ctlTick = 0;
				updateDisplay();
			}
		}
		int i = (int) pos;
		float t = (float) (pos - i);
		for (int c = 0; c < 2; c++) {
			float y0 = ring[c][i - 1 < 0 ? 0 : i - 1], y1 = ring[c][i], y2 = ring[c][i + 1], y3 = ring[c][i + 2];
			float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
			float y = ((c3 * t + c2) * t + c1) * t + y1;
			outputs[c ? R_OUTPUT : L_OUTPUT].setVoltage(genra_out::limit(5.f * y * presetGain));  // level + safety limiter
		}
		if (!outputs[R_OUTPUT].isConnected() && !pulled(args.frame)) // L alone: the mono sum (as FUNKE, GRUND, MONO)
			outputs[L_OUTPUT].setVoltage(0.5f * (outputs[L_OUTPUT].getVoltage() + outputs[R_OUTPUT].getVoltage()));
		pos += step;
		outputs[ENV_OUTPUT].setVoltage(10.f * a->env());
		outputs[LFO_OUTPUT].setVoltage(5.f * a->lfo());
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
			json_t* p = json_array();
			// knob-bound values from the knobs (base), not the firmware's knob + CV (SLIDE, GLIDE CV …): a save while
			// a CV is up must not bake it into the sound
			std::vector<char> fromKnob(core.api->np(), 0);
			for (const Bind& b : binds)
				if (b.kind == PATCH && b.fw >= 0 && b.fw < (int) fromKnob.size() && b.fw != kwid::OT_AMT) // (OT_AMT: its 0 = MULTIBAND off)
					fromKnob[b.fw] = 1;
			for (uint32_t id = 0; id < core.api->np(); id++)
				json_array_append_new(p, json_integer(fromKnob[id] && std::isfinite(base[id]) ? (int) std::lround(base[id])
				                                                                          : core.api->get(id)));
			json_object_set_new(r, "patch", p);
			json_t* eq = json_array();
			for (int k = 0; k < 4; k++)
				for (int w = 0; w < 4; w++)
					json_array_append_new(eq, json_integer(core.api->eq_get(k, w)));
			json_object_set_new(r, "eq", eq);
		}
		if (core.api)
			bank.toJson(r);
		fol.toJson(r);
		return r;
	}
	void dataFromJson(json_t* r) override {
		fol.fromJson(r);
		if (json_t* j = json_object_get(r, "preset"))
			presetIdx = std::max(0, (int) json_integer_value(j));
		{
			// the output level: as saved; a patch from before takes its preset's level
			json_t* gJ = json_object_get(r, "presetGain");
			presetGain = json_is_number(gJ) ? (float) json_number_value(gJ) : kwlevels::gainOf(presetIdx);
		}
		// a preset or an undo step (StateBank.hpp): the sound in the patch is the whole sound, the expander follows
		bool engineWins = json_is_true(json_object_get(r, "engineWins"));
		if (json_t* j = json_object_get(r, "ppqn"))
			ppqn = clamp((int) json_integer_value(j), 1, 24);
		if (!core.api)
			return;
		// the whole sound, the parts no knob shows included (LFO 2, the matrix, OSC C …)
		if (json_t* p = json_object_get(r, "patch")) {
			for (size_t id = 0; id < json_array_size(p) && id < core.api->np(); id++)
				core.api->set((uint32_t) id, (int32_t) json_integer_value(json_array_get(p, id)));
		}
		if (json_t* eq = json_object_get(r, "eq")) {
			for (size_t i = 0; i < json_array_size(eq) && i < 16; i++)
				core.api->eq_set((uint32_t) (i / 4), (uint32_t) (i % 4), (int32_t) json_integer_value(json_array_get(eq, i)));
		}
		pull(false, false);  // (base and written from the firmware; the knobs then push their saved values)
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
		bank.fromJson(r, posOf(presetIdx));
	}
};

std::string FwQuantity::getDisplayValueString() {
	Kwasub* m = dynamic_cast<Kwasub*>(module);
	if (!m || !m->core.api)
		return ParamQuantity::getDisplayValueString();
	char b[48];
	m->core.api->text(fw, b, sizeof(b));
	return b;
}

// ---------------------------------------------------------------- widgets
/** A selector field (engine, filter type): the name, a click opens the list, the wheel steps through it. */
struct Field : app::ParamWidget {
	std::vector<std::string> names, infos;
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq ? clamp((int) std::round(pq->getValue()), 0, (int) names.size() - 1) : 0;
	}
	void set(int v) {
		engine::ParamQuantity* pq = getParamQuantity();
		if (!pq)
			return;
		float old = pq->getValue();
		if ((int) std::round(old) == v)
			return;
		pq->setValue((float) v);
		history::ParamChange* h = new history::ParamChange;
		h->name = "change " + pq->name;
		h->moduleId = module->id;
		h->paramId = paramId;
		h->oldValue = old;
		h->newValue = (float) v;
		APP->history->push(h);
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module) {
			Menu* menu = createMenu();
			for (int i = 0; i < (int) names.size(); i++)
				menu->addChild(createCheckMenuItem(names[i], infos[i], [=]() { return value() == i; }, [=]() { set(i); }));
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (module && e.scrollDelta.y != 0.f) {
			set(clamp(value() + (e.scrollDelta.y < 0.f ? 1 : -1), 0, (int) names.size() - 1));
			e.consume(this);
			return;
		}
		ParamWidget::onHoverScroll(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			int v = module ? value() : 0;
			auto f = ui::boldFont();
			float y = box.size.y / 2 + 2.6f;
			ui::text(args.vg, f, mm2px(2.f), y, names[v], 7.6f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
			ui::text(args.vg, f, box.size.x - mm2px(6.f), y, infos[v], 5.6f, ui::DIM, NVG_ALIGN_RIGHT, 0.8f);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** The preset list (StateBank.hpp), opened by a click on the display's name too. */
static void presetMenu(Kwasub* m) {
	if (!m || !m->core.api)
		return;
	m->bank.scan();
	Menu* menu = createMenu();
	menu->addChild(createMenuLabel("Preset"));
	genra_states::appendList(menu, &m->bank);
}

struct Display : Widget {
	Kwasub* module = nullptr;
	/** A click on the preset name (the left part) opens the preset list too. */
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT && module && e.pos.x < mm2px(55.f)) {
			presetMenu(module);
			e.consume(this);
			return;
		}
		Widget::onButton(e);
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont();
		Kwasub::Disp d;
		if (module) {
			std::lock_guard<std::mutex> lock(module->dispMutex);
			d = module->disp;
		} else {
			std::strcpy(d.preset, "NEURO GROWL");
			std::strcpy(d.style, "NEUROFUNK");
			std::strcpy(d.engA, "GROWL");
			std::strcpy(d.engB, "ANALOG");
			std::strcpy(d.wob, "WOB 1/8T · WUB");
			std::strcpy(d.mode, "POLY");
			d.idx = 9;
		}
		auto mm = [](float v) { return mm2px(v); };
		if (!d.error.empty()) {
			ui::text(vg, bold, mm(4), mm(9), d.error, 7.f, nvgRGB(0xff, 0x45, 0x3a));
			Widget::drawLayer(args, layer);
			return;
		}
		// the preset as the header field has it (the sorted list's number; a user preset by its name)
		std::string pname = d.preset, pstyle = d.style, pnum = string::f("PRESET %d / %d", d.idx + 1, d.n);
		if (module) {
			const genra_states::Bank& b = module->bank;
			if (b.preset >= b.f.count) {
				pname = b.nameOf(b.preset);
				pstyle = "USER PRESET";
				pnum = "PRESET U";
			} else if (b.preset >= 0) {
				pnum = string::f("PRESET %d / %d", b.preset + 1, b.f.count);
			}
		}
		ui::text(vg, bold, mm(4), mm(4.7f), pnum, 5.4f, ui::DIM, NVG_ALIGN_LEFT, 1.f);
		ui::text(vg, bold, mm(4), mm(11.1f), pname, 15.f, ui::WHITE, NVG_ALIGN_LEFT, 0.8f);
		ui::text(vg, bold, mm(4), mm(14.5f), pstyle, 5.4f, ACCENT, NVG_ALIGN_LEFT, 1.2f);
		// the oscillators
		ui::text(vg, bold, mm(59), mm(4.7f), "A", 5.4f, ui::DIM);
		ui::text(vg, bold, mm(63), mm(4.7f), d.engA, 6.2f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
		ui::text(vg, bold, mm(59), mm(8.7f), string::f("%s   %s   %s", d.pa[0], d.pa[1], d.pa[2]), 4.9f, ui::DIM,
		         NVG_ALIGN_LEFT, 0.6f);
		ui::text(vg, bold, mm(59), mm(12.5f), "B", 5.4f, ui::DIM);
		ui::text(vg, bold, mm(63), mm(12.5f), d.engB, 6.2f, ui::TXT, NVG_ALIGN_LEFT, 0.9f);
		ui::text(vg, bold, mm(59), mm(15.9f), string::f("%s   %s   %s", d.pb[0], d.pb[1], d.pb[2]), 4.9f, ui::DIM,
		         NVG_ALIGN_LEFT, 0.6f);
		// the wobble: three cycles of its shape, a dot where it is
		float x0 = mm(125), w = mm(46), cy = mm(8), amp = mm(4.5f);
		auto shape = [&](float ph) -> float {
			switch (d.lshape) {
				case 0: return std::sin(6.2831853f * ph);
				case 1: return 1.f - 4.f * std::fabs(ph - 0.5f);
				case 2: return 1.f - 2.f * ph;
				case 3: return 2.f * ph - 1.f;
				case 4: return ph < 0.5f ? 1.f : -1.f;
				case 6: { float t = 1.f - ph; return 2.f * t * t - 1.f; }
				case 7: return std::floor(ph * 4.f) / 1.5f - 1.f;
				default: return std::sin(6.2831853f * ph) * 0.6f + std::sin(18.849556f * ph) * 0.3f;
			}
		};
		nvgBeginPath(vg);
		nvgMoveTo(vg, x0, cy);
		nvgLineTo(vg, x0 + w, cy);
		nvgStrokeColor(vg, ui::EDGE);
		nvgStrokeWidth(vg, 0.6f);
		nvgStroke(vg);
		nvgBeginPath(vg);
		for (int i = 0; i <= 120; i++) {
			float ph = std::fmod(i / 120.f * 3.f, 1.f);
			float y = cy - amp * clamp((float) shape(ph), -1.f, 1.f);
			if (i == 0)
				nvgMoveTo(vg, x0, y);
			else
				nvgLineTo(vg, x0 + w * i / 120.f, y);
		}
		nvgStrokeColor(vg, ACCENT);
		nvgStrokeWidth(vg, 1.3f);
		nvgLineJoin(vg, NVG_ROUND);
		nvgStroke(vg);
		float px = x0 + w * d.phase / 3.f;
		nvgBeginPath(vg);
		nvgCircle(vg, px, cy - amp * clamp((float) shape(d.phase), -1.f, 1.f), 2.4f);
		nvgFillColor(vg, ui::WHITE);
		nvgFill(vg);
		ui::text(vg, bold, x0, mm(14.8f), d.wob, 5.f, ui::DIM, NVG_ALIGN_LEFT, 0.9f);
		// tempo, voice mode, voices
		float xr = box.size.x - mm(4);
		ui::text(vg, bold, xr, mm(4.7f), string::f("%d BPM", (int) std::lround(d.bpm10 / 10.f)), 5.4f, ui::DIM,
		         NVG_ALIGN_RIGHT, 1.f);
		ui::text(vg, bold, xr, mm(11.1f), d.mode, 9.f, ui::WHITE, NVG_ALIGN_RIGHT, 0.8f);
		ui::text(vg, bold, xr, mm(14.5f), string::f("%d VOICE%s", d.voices, d.voices == 1 ? "" : "S"), 5.f, ui::DIM,
		         NVG_ALIGN_RIGHT, 1.f);
		Widget::drawLayer(args, layer);
	}
};

struct KwasubWidget : ModuleWidget {
	KwasubWidget(Kwasub* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Kwasub.svg")));
		follow::addStrip(this, module, {mm2px(Vec(L::IN_VOCT[0], L::IN_VOCT[1])), mm2px(Vec(L::IN_GATE[0], L::IN_GATE[1])), mm2px(Vec(L::IN_VEL[0], L::IN_VEL[1])), mm2px(Vec(L::IN_ACC[0], L::IN_ACC[1])), mm2px(Vec(L::IN_SLIDE[0], L::IN_SLIDE[1])), mm2px(Vec(L::IN_MOD[0], L::IN_MOD[1])), mm2px(Vec(L::IN_CLOCK[0], L::IN_CLOCK[1])), mm2px(Vec(L::IN_RESET[0], L::IN_RESET[1]))}); // lit while GENRA feeds the strip
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);

		using M = Kwasub;
		auto field = [&](const float* r, int id, std::vector<std::string> names, std::vector<std::string> infos) {
			Field* f = createParam<Field>(mm2px(Vec(r[0], r[1])), module, id);
			f->box.size = mm2px(Vec(r[2], r[3]));
			f->names = names;
			f->infos = infos;
			addParam(f);
		};
		std::vector<std::string> en, ei;
		for (int i = 0; i < 9; i++) {
			en.push_back(ENG_NAMES[ENG_ORDER[i]]);
			ei.push_back(ENG_INFO[ENG_ORDER[i]]);
		}
		field(L::ENG_A, M::ENG_A, en, ei);
		field(L::ENG_B, M::ENG_B, en, ei);
		field(L::FTYPE, M::FTYPE_PARAM, std::vector<std::string>(FILTER_NAMES, FILTER_NAMES + 7),
		      std::vector<std::string>(FILTER_INFO, FILTER_INFO + 7));

		auto knob = [&](const float* p, int id) { addParam(createParamCentered<GKnob>(ui::mm(p), module, id)); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(ui::mm(p), module, id)); };
		const float* oscK[2][4] = {{L::A_P1, L::A_P2, L::A_P3, L::A_P4}, {L::B_P1, L::B_P2, L::B_P3, L::B_P4}};
		const float* oscS[2][4] = {{L::A_LVL, L::A_OCT, L::A_SEMI, L::A_FINE}, {L::B_LVL, L::B_OCT, L::B_SEMI, L::B_FINE}};
		for (int o = 0; o < 2; o++) {
			int p0 = o ? M::ENG_B : M::ENG_A;
			for (int k = 0; k < 4; k++) {
				knob(oscK[o][k], p0 + 1 + k);
				small(oscS[o][k], p0 + 5 + k);
			}
		}
		addParam(createParamCentered<GLargeKnob>(ui::mm(L::F_CUT), module, M::CUT_PARAM));
		knob(L::F_RES, M::RES_PARAM);
		knob(L::F_DRV, M::DRV_PARAM);
		knob(L::F_ENV, M::FENV_PARAM);
		small(L::F_KEY, M::KEY_PARAM);
		small(L::FE_A, M::FEA_PARAM);
		small(L::FE_D, M::FED_PARAM);
		small(L::S_LVL, M::SUB_PARAM);
		small(L::N_LVL, M::NOISE_PARAM);
		small(L::X_AMT, M::XMOD_PARAM);
		knob(L::L_RATE, M::LRATE_PARAM);
		small(L::L_DEP, M::LDEP_PARAM);
		small(L::L_SHP, M::LSHP_PARAM);
		small(L::L_DST, M::LDST_PARAM);
		small(L::AE_A, M::AEA_PARAM);
		small(L::AE_D, M::AED_PARAM);
		small(L::AE_S, M::AES_PARAM);
		small(L::AE_R, M::AER_PARAM);
		small(L::FX_SAT, M::SAT_PARAM);
		small(L::FX_DLY, M::DLY_PARAM);
		small(L::FX_VERB, M::VERB_PARAM);
		small(L::FX_GLUE, M::GLUE_PARAM);
		genra_states::PresetField* pf = createWidget<genra_states::PresetField>(mm2px(Vec(L::PRESETF[0], L::PRESETF[1])));
		pf->box.size = mm2px(Vec(L::PRESETF[2], L::PRESETF[3]));
		pf->bank = module && module->core.api ? &module->bank : nullptr;
		pf->accent = ACCENT;
		pf->sample = "NEURO GROWL";
		pf->sampleStyle = "NEUROFUNK";
		addChild(pf);

		const float* ins[M::NUM_INPUTS] = {L::IN_VOCT, L::IN_GATE,  L::IN_VEL,   L::IN_CLOCK, L::IN_RESET, L::IN_CUT,
		                                   L::IN_RES,  L::IN_DRV,   L::IN_FENV,  L::IN_ASHP,  L::IN_ACOL,  L::IN_BSHP,
		                                   L::IN_BCOL, L::IN_WOB,   L::IN_DEP,   L::IN_PRESET, L::IN_SUB,   L::IN_NOISE,
		                                   L::IN_XMOD, L::IN_ATK,   L::IN_REL,   L::IN_SAT,   L::IN_DLY,   L::IN_VERB,
		                                   L::IN_MOD,  L::IN_PRESS, L::IN_BEND,  L::IN_GLIDE, L::IN_ACC, L::IN_SLIDE};
		for (int i = 0; i < M::NUM_INPUTS; i++)
			addInput(createInputCentered<PJ301MPort>(ui::mm(ins[i]), module, i));
		const float* outs[M::NUM_OUTPUTS] = {L::OUT_ENV, L::OUT_LFO, L::OUT_L, L::OUT_R};
		for (int i = 0; i < M::NUM_OUTPUTS; i++)
			addOutput(createOutputCentered<PJ301MPort>(ui::mm(outs[i]), module, i));
	}

	void appendContextMenu(Menu* menu) override {
		if (follow::Host* h = follow::host(module))
			h->fol.appendMenu(menu, module);
		Kwasub* m = getModule<Kwasub>();
		if (!m || !m->core.api)
			return;
		menu->addChild(new MenuSeparator);
		genra_states::appendMenu(menu, &m->bank);
		menu->addChild(createIndexSubmenuItem(
		    "Voices", {"Mono", "Legato", "Poly (4)"}, [=]() { return (size_t) m->core.api->get(V_MODE); },
		    [=](size_t i) { m->core.api->set(V_MODE, (int32_t) i); }));
		menu->addChild(createIndexSubmenuItem(
		    "Clock input", {"16ths (GENRA)", "8ths", "Quarter notes", "24 PPQN"},
		    [=]() { return (size_t) (m->ppqn == 4 ? 0 : m->ppqn == 2 ? 1 : m->ppqn == 1 ? 2 : 3); },
		    [=](size_t i) { m->ppqn = i == 0 ? 4 : i == 1 ? 2 : i == 2 ? 1 : 24; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("K-WASUB firmware by Omnipotard (GPL-3.0), born from KAWA"));
		menu->addChild(createMenuLabel("The effects in detail: K-WASUB FX, placed to the right"));
	}
};

}  // namespace genra_kwasub

/** For the expander's tooltips: the firmware's text for a patch id, from the K-WASUB module m. */
bool kwasubText(Module* m, int fw, char* out, int n) {
	genra_kwasub::Kwasub* k = dynamic_cast<genra_kwasub::Kwasub*>(m);
	if (!k || !k->core.api)
		return false;
	k->core.api->text((uint32_t) fw, out, (uint32_t) n);
	return true;
}

Model* modelKwasub = createModel<genra_kwasub::Kwasub, genra_kwasub::KwasubWidget>("Kwasub");
