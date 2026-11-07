// GENRA – module definition (shared by the GENRA module and the grid editor)
#pragma once
#include <thread>
#include "plugin.hpp"
#include "Engine.hpp"
#include "Arranger.hpp"
#include "Layout.hpp"
#include "Follow.hpp"
#include <osdialog.h>
#include <atomic>
#include <mutex>
#include <set>
#include <cmath>
#include <cctype>
#include <cstdio>

using namespace genra;

static const int LENGTHS[4] = {1, 2, 4, 8};
static const char* const CLOCK_MODES[4] = {"16ths (4 PPQN)", "8ths (2 PPQN)", "Quarters (1 PPQN)", "24 PPQN"};
static const char* const CHORD_MODES[3] = {"Mixed (templates + free)", "Genre templates only", "Freely generated"};
static const char* const GENRE_CV_MODES[2] = {"0–10 V across all genres", "1/12 V per genre (semitones, 0 V = House)"};
static const char* const TRACK_NAMES[4] = {"Drums", "Bass", "Melody", "Pads"};

static const NVGcolor TRACK_COLORS[4] = {
	nvgRGB(0x7b, 0x6c, 0xff), nvgRGB(0xb8, 0xf2, 0x3a), nvgRGB(0x3d, 0xc9, 0xff), nvgRGB(0xe0, 0x77, 0x9e)};
static const NVGcolor HARMONY_COLOR = nvgRGB(0x2f, 0x9e, 0x8f);

// genre families in GENRE knob order (for the menus)
struct Family {
	const char* name;
	int first, count;
};
static const Family FAMILIES[8] = {
	{"House", 0, 5}, {"Techno", 5, 5}, {"Breaks & Bass", 10, 5}, {"Hip-Hop & Downtempo", 15, 6}, {"Dub · Synthwave · Ambient", 21, 3},
	{"Afro", 24, 4}, {"Techno Styles", 28, 5}, {"Latin & Club", 33, 4}};

inline std::string upper(std::string s) {
	for (char& c : s)
		c = (char)std::toupper((unsigned char)c);
	return s;
}

// "JUNGLE · RAGGA" (variant only if the genre has several)
inline std::string profileLabel(int p) {
	int g = genreOfProfile(p);
	std::string s = upper(prof(p).name);
	if (numVariants(g) > 1)
		s += " · " + upper(prof(p).variant);
	return s;
}

struct Genra;
// every GENRA alive (ABLAUF checks a remembered GENRA against it before touching it)
inline std::mutex& genraRegMutex() {
	static std::mutex m;
	return m;
}
inline std::set<Genra*>& genraRegistry() {
	static std::set<Genra*> s;
	return s;
}
// shows the variant's name of the current genre on hover / right-click
struct VariantQuantity : ParamQuantity {
	std::string getDisplayValueString() override;
};

struct Genra : Module {
	// NOTE: only append to these lists – Rack stores params by index, older patches rely on the order
	enum ParamId {
		GENRE_PARAM,
		KEY_PARAM,
		SCALE_PARAM,
		BPM_PARAM,
		SWING_PARAM,
		COMPLEX_PARAM,
		COLOR_PARAM,
		MUTATE_PARAM,
		LENGTH_PARAM,
		NEW_PARAM,
		RUN_PARAM,
		CHORDLOCK_PARAM,
		ENUMS(DENS_PARAM, 4),
		FILL_PARAM,
		GHOST_PARAM,
		MOVE_PARAM,
		RANGE_PARAM,
		VOICING_PARAM,
		ENUMS(OCT_PARAM, 3),
		ENUMS(MUTE_PARAM, 4),
		ENUMS(LOCK_PARAM, 4),
		VARIANT_PARAM,
		CHORDNEW_PARAM,
		ENUMS(TNEW_PARAM, 4),
		// (v3 had 8 scene buttons and STORE here; removed in 2.11.0 – the song module takes over)
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		RESET_INPUT,
		RUN_INPUT,
		NEW_INPUT,
		GENRE_INPUT,
		MUTATE_INPUT,
		TRANSP_INPUT,
		CHORD_INPUT,
		ENUMS(TNEW_INPUT, 4),
		CHORDNEW_INPUT,
		// v3
		FILL_INPUT,
		VARIANT_INPUT,
		COMPLEX_INPUT,
		COLOR_INPUT,
		ENUMS(TGENRE_INPUT, 4),
		ENUMS(DENS_INPUT, 4),
		UNUSED_SCENE_INPUT, // was SCENE CV (until 2.10.0); kept so the inputs after it keep their ids
		UNUSED_NEXT_INPUT,  // was NEXT (until 2.10.0)
		// v3.1
		SWING_INPUT,
		BPM_INPUT,
		LENGTH_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		ENUMS(DRUM_OUTPUT, 8),
		BASS_VOCT_OUTPUT,
		BASS_GATE_OUTPUT,
		BASS_VEL_OUTPUT,
		BASS_ACC_OUTPUT,
		MEL_VOCT_OUTPUT,
		MEL_GATE_OUTPUT,
		MEL_VEL_OUTPUT,
		MEL_ACC_OUTPUT,
		PAD_VOCT_OUTPUT,
		PAD_GATE_OUTPUT,
		PAD_VEL_OUTPUT,
		PAD_MOD_OUTPUT,
		BASS_SLIDE_OUTPUT,
		MEL_SLIDE_OUTPUT,
		// v3.1
		CLOCK_OUTPUT,
		RESET_OUTPUT,
		RUN_OUTPUT,
		BAR_OUTPUT,
		END_OUTPUT,
		FILL_OUTPUT,
		ROOT_OUTPUT,
		CHORD_OUTPUT,
		// 2.13
		DRUMS_OUTPUT, // all 8 drum tracks on one poly cable (GENRA's order)
		OUTPUTS_LEN
	};
	enum LightId {
		NEW_LIGHT,
		RUN_LIGHT,
		CHORDLOCK_LIGHT,
		ENUMS(MUTE_LIGHT, 4),
		ENUMS(LOCK_LIGHT, 4),
		ENUMS(TNEW_LIGHT, 4),
		CHORDNEW_LIGHT,
		LIGHTS_LEN
	};

	// ---- pattern & engine state
	Pattern pat;
	Rng rng;
	bool running = true;
	int step = -1;
	bool firstPass = true;
	bool fillActive = false;
	bool fillCarry = false; // FILL trigger in the last beat: also fill the next pass
	bool pendingGenre = false;
	int adoptGenre = 0;      // samples left in which the genres are taken as they are (after load / cue)
	int lastTarget[4] = {-1, -1, -1, -1};
	int lastGenre = -1;
	int clockMode = 0;
	int chordMode = CM_MIXED;
	int templateSel = -1;
	int genreCvMode = 0;

	// ---- genre per track: -1 = follows GENRE, otherwise a profile index (written by the UI thread)
	std::atomic<int> trackSel[4];
	std::atomic<bool> trackSelChanged[4];
	bool pendingTrack[4] = {false, false, false, false};
	bool ownPrev[4] = {false, false, false, false};

	// ---- requests from the UI thread
	std::atomic<bool> regenRequested{false};
	std::atomic<bool> chordRegenRequested{false};
	std::atomic<bool> importReady{false};
	std::atomic<bool> resetTracksRequested{false};

	// chord edits from the display (UI thread) -> applied in process() (audio thread)
	enum ChordOp : uint8_t { CE_NEXT, CE_SET, CE_COLOR, CE_COLOR_RESET };
	struct ChordEdit {
		uint8_t op, idx;
		int8_t val;
	};
	static const uint32_t EDIT_QUEUE = 32;
	ChordEdit editBuf[EDIT_QUEUE];
	std::atomic<uint32_t> editHead{0}, editTail{0}; // single producer (UI), single consumer (audio)

	void pushChordEdit(uint8_t op, int idx, int val = 0) {
		uint32_t h = editHead.load(std::memory_order_relaxed);
		if (h - editTail.load(std::memory_order_acquire) >= EDIT_QUEUE)
			return; // queue full: drop (only happens with absurdly fast clicking)
		editBuf[h % EDIT_QUEUE] = {op, (uint8_t)idx, (int8_t)val};
		editHead.store(h + 1, std::memory_order_release);
	}

	void applyChordEdits() {
		uint32_t t = editTail.load(std::memory_order_relaxed);
		uint32_t h = editHead.load(std::memory_order_acquire);
		if (t == h)
			return;
		Params P = buildParams();
		for (; t != h; t++) {
			const ChordEdit& ce = editBuf[t % EDIT_QUEUE];
			if (ce.idx >= pat.nProg)
				continue;
			ChordSpec& cs = pat.prog[ce.idx];
			switch (ce.op) {
				case CE_NEXT: applyAlternative(cs, currentAlternative(cs, P) + 1, P); break;
				case CE_SET: applyAlternative(cs, ce.val, P); break;
				case CE_COLOR: cs.extOff = (int8_t)clamp(cs.extOff + ce.val, -3, 3); break;
				case CE_COLOR_RESET: cs.extOff = 0; break;
			}
		}
		editTail.store(t, std::memory_order_release);
	}
	Pattern importBuf;
	int importLengthIndex = 2;

	// ---- grid editor: a whole edited track is handed over (editor UI thread -> audio thread)
	Pattern gridBuf;
	int gridTrack = 0;
	std::atomic<int> gridState{0}; // 0 = free, 1 = written by the editor
	std::atomic<bool> gridGen[4];  // GEN TRACK: back to generated material

	bool submitTrack(const Pattern& p, int t) {
		if (t < 0 || t >= 4 || gridState.load(std::memory_order_acquire) != 0)
			return false;
		gridBuf = p;
		gridTrack = t;
		gridState.store(1, std::memory_order_release);
		return true;
	}

	// ---- arranger ABLAUF: it works on its UI thread and hands patterns over through two mailboxes
	// snapshot (ABLAUF asks for what plays now): 0 idle, 1 asked, 2 ready
	std::atomic<int> snapState{0};
	Slot snap;
	Params snapP;
	// cue (ABLAUF -> audio thread): 0 free, 1 waiting, 2 being written, 3 being applied
	struct ArrCue {
		Slot slot;
		uint8_t mute = 0;     // bit per track: muted while this pattern plays
		bool fill = false;    // fill / drop at the end of the row
		int rowBars = 0;      // length of the row in bars (0: open end)
		int64_t atBar = 0;    // apply at the bar line where barIndex reaches this
		bool now = false;     // apply at once (GENRA stopped, or the song starts)
		bool stop = false;    // no pattern: stop at that bar line (end of the song)
		int8_t row = -1, tag = -1;
		int slotIdx = -1;
		uint32_t id = 0;
	} cue;
	std::atomic<int> cueState{0};
	std::atomic<uint32_t> cueDone{0};       // id of the last cue applied
	std::atomic<int64_t> barPub{0};         // bar index (counts the bar lines while running)
	std::atomic<int64_t> rowStartPub{0};    // bar index where the last cue began
	std::atomic<int> arrRunReq{0};          // 1 start from the top, 2 stop
	std::atomic<int> arrSecPub{-1}, arrRowPub{-1}, arrSlotPub{-1};
	std::atomic<uint32_t> patChanges{0};    // counts changes of the pattern (generate, mutate, edits)
	std::atomic<uint32_t> patChangesAtCue{0};
	std::atomic<float> arrAlive{0.f};       // > 0 while ABLAUF is attached (it sets a huge value, a tiny one to let go)
	std::atomic<int64_t> arrOwner{-1};      // module id of the ABLAUF that drives this GENRA (one at a time)
	ArrMods arrMods;
	uint8_t arrMute = 0;
	bool arrFill = false;
	int64_t arrRowEnd = -1;
	int arrFillEnd = -1; // the row ends inside the loop: the drums play the fill up to this step
	int64_t barIndex = 0;

	// writer side (ABLAUF): acquire the cue buffer (also replaces a cue still waiting)
	bool cueBegin() {
		int e = 0;
		if (cueState.compare_exchange_strong(e, 2))
			return true;
		e = 1;
		return cueState.compare_exchange_strong(e, 2);
	}
	void cueCommit() { cueState.store(1, std::memory_order_release); }
	// drop a waiting cue (UI thread). The audio thread holds a waiting cue (3) for a moment every sample to check it:
	// wait that out instead of giving up, or a cancelled cue could still play later. False only while another
	// writer holds it (2).
	bool cueCancel() {
		for (int i = 0; i < 200000; i++) {
			int e = 1;
			if (cueState.compare_exchange_strong(e, 0))
				return true;
			if (e == 0)
				return true;
			if (e == 2)
				return false;
			std::this_thread::yield();
		}
		return cueState.load() == 0;
	}

	void applyCue(bool atBarLine) {
		if (cue.stop) {
			running = false;
			step = -1;
			firstPass = true;
			resetArmed = true;
			arrSecPub = -1;
			arrRowPub = -1;
			arrMute = 0;
			arrFill = false;
			arrRowEnd = -1;
			arrFillEnd = -1;
			arrSlotPub = -1;
		}
		else {
			const Slot& s = cue.slot;
			pat = s.pat;
			for (int t = 0; t < 4; t++) {
				trackSel[t] = s.sel[t];
				trackSelChanged[t] = false;
				pendingTrack[t] = false;
				ownPrev[t] = ownGenre(t);
			}
			params[LENGTH_PARAM].setValue((float)clamp((int)s.lengthIdx, 0, 3));
			pendingGenre = false;
			lastGenre = curGenre; // the stored pattern stands, even if the GENRE knob differs
			adoptGenre = 1;       // and its track genres as well (no re-roll at the next bar)
			padHasPrev = false;
			arrMods = s.mods;
			arrMute = cue.mute;
			arrFill = cue.fill;
			int64_t start = atBarLine ? barIndex : barIndex + 1;
			arrRowEnd = cue.rowBars > 0 ? start + cue.rowBars : -1;
			rowStartPub.store(start);
			arrSecPub = cue.tag;
			arrRowPub = cue.row;
			arrSlotPub = cue.slotIdx;
			arrFillEnd = -1;
			// the stored pattern is heard as stored: no MUTATE in its first pass, CHANCE / every-2nd count anew
			firstPass = true;
			passCount = 0;
			if (!atBarLine) { // GENRA stopped (or the song starts): from the top at the next start
				step = -1;
				resetArmed = true;
			}
			patChangesAtCue.store(patChanges.load());
		}
		cueDone.store(cue.id, std::memory_order_release);
	}

	// the cue for this bar line (audio thread); true if a pattern was applied or GENRA stopped
	bool takeCue(bool atBarLine) {
		int e = 1;
		if (!cueState.compare_exchange_strong(e, 3))
			return false;
		bool due = cue.now || (atBarLine && cue.atBar <= barIndex);
		if (due)
			applyCue(atBarLine);
		cueState.store(due ? 0 : 1, std::memory_order_release);
		return due;
	}

	// ---- clock & triggers
	dsp::SchmittTrigger clockTrig, resetTrig, runTrig, newTrig, fillTrig;
	dsp::BooleanTrigger newBtn, runBtn, chordNewBtn;
	dsp::BooleanTrigger tnewBtn[4];
	dsp::SchmittTrigger tnewTrig[4], chordNewTrig;
	float tnewFlash[4] = {0.f, 0.f, 0.f, 0.f};
	float chordNewFlash = 0.f;
	float phase = 0.f;
	float samplesSinceClock = 0.f;
	float clockPeriod = 0.f;
	float stepPeriod = 5000.f;
	int pulseCount = 0;
	int subTicksLeft = 0;
	float subInterval = 0.f, subCounter = 0.f;
	bool resetArmed = false;
	float blink = 0.f;
	int passCount = 0; // loop passes since start (CHANCE of edited notes)
	dsp::PulseGenerator clockPulse, resetPulse, barPulse, endPulse;
	float rootV = 0.f;
	float chordV[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
	int chordN = 0;

	// ---- live inputs (copied into Params)
	int transpSemis = 0;
	bool extChord = false;
	int extRoot = 0;
	uint16_t extMask = 0;
	int curGenre = 0;

	// ---- events waiting for their track's swing delay
	Hit qDrums[NUM_DRUMS];
	NoteEv qBass, qMel;
	PadEv qPad;
	float dDrums = -1.f, dBass = -1.f, dMel = -1.f, dPad = -1.f;

	// ---- voices
	struct DrumVoice {
		float vel = 0.f;
		float trig = 0.f;
		int ratLeft = 0;
		float ratInterval = 0.f, ratCounter = 0.f;
		float delayCnt = -1.f; // humanize: pending hit
		float pVel = 0.f;
		int pRat = 1;
	} drums[NUM_DRUMS];
	struct MonoVoice {
		float pitch = 0.f, vel = 0.f;
		bool acc = false;
		float gate = 0.f, retrig = 0.f;
		bool slidePending = false; // the last note slides into the next one
		float slideOut = 0.f;
	} bassV, melV;
	struct PadVoice {
		float pitch[4] = {0.f, 0.f, 0.f, 0.f};
		float vel = 0.f;
		float gate = 0.f, retrig = 0.f;
	} padV;
	int padPrev[4] = {60, 64, 67, 72};
	bool padHasPrev = false;
	float modTarget = 0.f, modValue = 0.f;
	float drumOut[NUM_DRUMS] = {};
	float newFlash = 0.f;
	dsp::ClockDivider lightDivider;

	float followPub[2][OUTPUTS_LEN][4] = {}; // the outputs per frame (even / odd) for Follow.cpp
	// instruments and MIX that follow this GENRA without cables let go (Follow.hpp)
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	~Genra() {
		follow::gone(this);
		std::lock_guard<std::mutex> lk(genraRegMutex());
		genraRegistry().erase(this);
	}

	Genra() {
		{
			std::lock_guard<std::mutex> lk(genraRegMutex());
			genraRegistry().insert(this);
		}
		initGenres();
		for (int t = 0; t < 4; t++) {
			trackSel[t] = -1;
			trackSelChanged[t] = false;
			gridGen[t] = false;
		}
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> genreNames;
		for (int g = 0; g < NUM_GENRES; g++)
			genreNames.push_back(prof(profileOf(g, 0)).name);
		configSwitch(GENRE_PARAM, 0.f, NUM_GENRES - 1, 0.f, "Genre (global)", genreNames);
		configParam<VariantQuantity>(VARIANT_PARAM, 0.f, MAX_VARIANTS - 1, 0.f, "Variant");
		paramQuantities[VARIANT_PARAM]->snapEnabled = true;
		configButton(CHORDNEW_PARAM, "Harmony: new chord progression (NEW)");
		configSwitch(KEY_PARAM, 0.f, 11.f, 9.f, "Key (root)",
		             {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B/H"});
		std::vector<std::string> scaleNames = {"Auto (Genre)"};
		for (int s = 0; s < NUM_SCALES; s++)
			scaleNames.push_back(SCALES[s].name);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES, 0.f, "Scale", scaleNames);
		configParam(BPM_PARAM, 40.f, 220.f, 124.f, "Tempo (internal clock)", " BPM");
		paramQuantities[BPM_PARAM]->snapEnabled = true;
		configParam(SWING_PARAM, -1.f, 1.f, 0.f, "Swing (relative to each track's genre)", "%", 0.f, 25.f);
		configParam(COMPLEX_PARAM, 0.f, 1.f, 0.5f, "Rhythmic complexity (relative to genre)", "%", 0.f, 200.f, -100.f);
		configParam(COLOR_PARAM, 0.f, 1.f, 0.5f, "Harmonic color (relative to genre)", "%", 0.f, 200.f, -100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutation per loop", "%", 0.f, 100.f);
		configSwitch(LENGTH_PARAM, 0.f, 3.f, 2.f, "Loop length", {"1 bar", "2 bars", "4 bars", "8 bars"});
		configButton(NEW_PARAM, "Re-roll everything (NEW ALL)");
		configButton(RUN_PARAM, "Start / Stop");
		configSwitch(CHORDLOCK_PARAM, 0.f, 1.f, 0.f, "Harmony: lock chord progression", {"Off", "Locked"});
		for (int t = 0; t < 4; t++) {
			std::string tn = TRACK_NAMES[t];
			configParam(DENS_PARAM + t, 0.f, 1.f, 0.5f, tn + " density", "%", 0.f, 200.f);
			configSwitch(MUTE_PARAM + t, 0.f, 1.f, 0.f, tn + " mute", {"Off", "Muted"});
			configSwitch(LOCK_PARAM + t, 0.f, 1.f, 0.f, tn + " lock", {"Off", "Locked"});
			configButton(TNEW_PARAM + t, tn + " re-roll (NEW)");
			configInput(TNEW_INPUT + t, tn + " re-roll (trigger)");
			configInput(TGENRE_INPUT + t, tn + " genre CV (overrides the genre field)");
			configInput(DENS_INPUT + t, tn + " density CV (0–10 V, added)");
		}
		configParam(FILL_PARAM, 0.f, 1.f, 0.25f, "Drum fill / drop probability", "%", 0.f, 100.f);
		configParam(GHOST_PARAM, 0.f, 1.f, 0.3f, "Ghost notes", "%", 0.f, 100.f);
		configParam(MOVE_PARAM, 0.f, 1.f, 0.5f, "Bass movement (relative to genre)", "%", 0.f, 200.f, -100.f);
		configParam(RANGE_PARAM, 0.f, 1.f, 0.5f, "Melody range", "%", 0.f, 100.f);
		configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "Pad voicing (close – open)", "%", 0.f, 100.f);
		static const char* ON[3] = {"Bass", "Melody", "Pads"};
		for (int i = 0; i < 3; i++)
			configSwitch(OCT_PARAM + i, -2.f, 2.f, 0.f, std::string(ON[i]) + " octave", {"-2", "-1", "0", "+1", "+2"});
		for (int id : std::initializer_list<int>{RUN_PARAM, NEW_PARAM, CHORDLOCK_PARAM, CHORDNEW_PARAM})
			paramQuantities[id]->randomizeEnabled = false;
		for (int t = 0; t < 4; t++) {
			paramQuantities[TNEW_PARAM + t]->randomizeEnabled = false;
			paramQuantities[MUTE_PARAM + t]->randomizeEnabled = false;
			paramQuantities[LOCK_PARAM + t]->randomizeEnabled = false;
		}

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run (trigger toggles)");
		configInput(NEW_INPUT, "Re-roll everything (trigger)");
		configInput(GENRE_INPUT, "Genre CV (global)");
		configInput(MUTATE_INPUT, "Mutate CV (0–10 V, added)");
		configInput(TRANSP_INPUT, "Transpose (V/Oct)");
		configInput(CHORD_INPUT, "Chord (polyphonic, V/Oct)");
		configInput(CHORDNEW_INPUT, "Harmony: new chord progression (trigger)");
		configInput(FILL_INPUT, "Fill / drop at loop end (trigger)");
		configInput(VARIANT_INPUT, "Variant CV (0–10 V, added)");
		configInput(COMPLEX_INPUT, "Complex CV (0–10 V, added)");
		configInput(COLOR_INPUT, "Color CV (0–10 V, added)");
		configInput(SWING_INPUT, "Swing CV (0–10 V, added)");
		configInput(BPM_INPUT, "Tempo CV (internal clock, 10 BPM per volt, added)");
		configInput(LENGTH_INPUT, "Loop length CV (0–10 V, added)");
		configOutput(CLOCK_OUTPUT, "Clock (16ths)");
		configOutput(RESET_OUTPUT, "Reset (loop start)");
		configOutput(RUN_OUTPUT, "Run (gate while GENRA is running)");
		configOutput(BAR_OUTPUT, "Bar (pulse per bar)");
		configOutput(END_OUTPUT, "Loop end (pulse on the last step)");
		configOutput(FILL_OUTPUT, "Fill / drop (gate)");
		configOutput(ROOT_OUTPUT, "Root of the current chord (V/Oct)");
		configOutput(CHORD_OUTPUT, "Current chord (polyphonic, V/Oct)");
		configOutput(DRUMS_OUTPUT, "All 8 drum tracks (polyphonic, GENRA's order: kick, snare, clap, rim, closed hat, open hat, tom, perc; height = velocity)");
		for (int v = 0; v < NUM_DRUMS; v++)
			configOutput(DRUM_OUTPUT + v, std::string(DRUM_NAMES[v]) + " trigger (level = velocity)");
		configOutput(BASS_VOCT_OUTPUT, "Bass V/Oct");
		configOutput(BASS_GATE_OUTPUT, "Bass gate");
		configOutput(BASS_VEL_OUTPUT, "Bass velocity");
		configOutput(BASS_ACC_OUTPUT, "Bass accent");
		configOutput(MEL_VOCT_OUTPUT, "Melody V/Oct");
		configOutput(MEL_GATE_OUTPUT, "Melody gate");
		configOutput(MEL_VEL_OUTPUT, "Melody velocity");
		configOutput(MEL_ACC_OUTPUT, "Melody accent");
		configOutput(PAD_VOCT_OUTPUT, "Pads V/Oct (polyphonic, 4)");
		configOutput(PAD_GATE_OUTPUT, "Pads gate (polyphonic, 4)");
		configOutput(PAD_VEL_OUTPUT, "Pads velocity");
		configOutput(PAD_MOD_OUTPUT, "Pads modulation (0–10 V)");
		configOutput(BASS_SLIDE_OUTPUT, "Bass slide (10 V during glide)");
		configOutput(MEL_SLIDE_OUTPUT, "Melody slide (10 V during glide)");

		lightDivider.setDivision(64);
		rng = Rng(random::u64());
		initPattern(pat);
		curGenre = 0;
		doGenerate(true);
	}

	// ------------------------------------------------------------ helpers
	static float cv01(Input& in) { return in.isConnected() ? in.getVoltage() / 10.f : 0.f; }

	int lengthSteps() {
		int idx = clamp((int)std::round(params[LENGTH_PARAM].getValue() + cv01(inputs[LENGTH_INPUT]) * 3.f), 0, 3);
		return LENGTHS[idx] * STEPS_PER_BAR;
	}

	Params buildParams(bool withArr = true) {
		Params P;
		P.genre = curGenre;
		P.key = clamp((int)std::round(params[KEY_PARAM].getValue()), 0, 11);
		P.transp = transpSemis;
		int sc = clamp((int)std::round(params[SCALE_PARAM].getValue()), 0, NUM_SCALES);
		if (sc == 0) {
			P.scale = prof(pat.chordGenre).scale;
			P.scaleOverride = false;
		}
		else {
			P.scale = sc - 1;
			P.scaleOverride = true;
		}
		P.complexK = clamp(params[COMPLEX_PARAM].getValue() + cv01(inputs[COMPLEX_INPUT]), 0.f, 1.f);
		P.colorK = clamp(params[COLOR_PARAM].getValue() + cv01(inputs[COLOR_INPUT]), 0.f, 1.f);
		P.swingK = clamp(params[SWING_PARAM].getValue() + cv01(inputs[SWING_INPUT]) * 2.f, -1.f, 1.f);
		P.lengthBars = lengthSteps() / STEPS_PER_BAR;
		for (int t = 0; t < 4; t++)
			P.density[t] = clamp(params[DENS_PARAM + t].getValue() + cv01(inputs[DENS_INPUT + t]), 0.f, 1.f);
		P.fill = params[FILL_PARAM].getValue();
		P.ghost = params[GHOST_PARAM].getValue();
		P.move = params[MOVE_PARAM].getValue();
		P.range = params[RANGE_PARAM].getValue();
		P.voicing = params[VOICING_PARAM].getValue();
		for (int i = 0; i < 3; i++)
			P.oct[i] = (int)std::round(params[OCT_PARAM + i].getValue());
		P.extChord = extChord;
		P.extRoot = extRoot;
		P.extMask = extMask;
		P.chordMode = chordMode;
		P.templateSel = templateSel;
		P.fillActive = fillActive;
		P.pass = passCount;
		return withArr ? withMods(P, arrMods) : P;
	}

	void locks(bool out[NUM_TRACKS]) {
		for (int t = 0; t < 4; t++)
			out[t] = params[LOCK_PARAM + t].getValue() > 0.5f;
	}
	bool chordLocked() { return params[CHORDLOCK_PARAM].getValue() > 0.5f; }

	// ---- genre selection
	// genre CV: 0-10 V over all genres, or 1/12 V per genre
	float genreCvSteps(float v) { return genreCvMode == 1 ? v * 12.f : v / 10.f * (NUM_GENRES - 1); }

	int knobGenre() {
		float g = params[GENRE_PARAM].getValue();
		if (inputs[GENRE_INPUT].isConnected())
			g += genreCvSteps(inputs[GENRE_INPUT].getVoltage());
		return clamp((int)std::round(g), 0, NUM_GENRES - 1);
	}
	int knobVariant() {
		float v = params[VARIANT_PARAM].getValue() + cv01(inputs[VARIANT_INPUT]) * (MAX_VARIANTS - 1);
		return clamp((int)std::round(v), 0, MAX_VARIANTS - 1);
	}
	int knobProfile() { return profileOf(knobGenre(), knobVariant()); }

	bool trackHasCv(int t) { return inputs[TGENRE_INPUT + t].isConnected(); }
	bool ownGenre(int t) { return trackSel[t] >= 0 || trackHasCv(t); }

	// profile a track should play: CV > genre field > global GENRE
	int trackTarget(int t) {
		int sel = trackSel[t];
		if (trackHasCv(t)) {
			int g = clamp((int)std::round(genreCvSteps(inputs[TGENRE_INPUT + t].getVoltage())), 0, NUM_GENRES - 1);
			int v = (sel >= 0 && genreOfProfile(sel) == g) ? variantOfProfile(sel) : knobVariant();
			return profileOf(g, v);
		}
		if (sel >= 0)
			return clamp(sel, 0, numProfiles() - 1);
		return curGenre;
	}

	void setTrackSel(int t, int profile) {
		trackSel[t] = profile;
		trackSelChanged[t] = true;
	}

	// all: NEW ALL (every unlocked track in its own genre); otherwise a global genre change
	// (only unlocked tracks that follow GENRE)
	void doGenerate(bool all) {
		Params P = buildParams();
		if (lastGenre >= 0 && lastGenre != curGenre) {
			templateSel = -1; // template indices are genre specific
			P.templateSel = -1;
		}
		bool lk[NUM_TRACKS];
		locks(lk);
		if (!chordLocked())
			generateChords(pat, curGenre, rng, P);
		for (int t = 0; t < NUM_TRACKS; t++) {
			if (lk[t] || (!all && ownGenre(t)))
				continue;
			generateTrack(pat, t, trackTarget(t), rng, P);
			pendingTrack[t] = false;
		}
		layoutTimeline(pat, lengthSteps());
		lastGenre = curGenre;
		padHasPrev = false;
		newFlash = 1.f;
		patChanges++;
	}

	void newTrack(int t) {
		Params P = buildParams();
		generateTrack(pat, t, trackTarget(t), rng, P);
		pendingTrack[t] = false;
		if (t == T_PADS)
			padHasPrev = false;
		tnewFlash[t] = 1.f;
		patChanges++;
	}

	void readGenre() {
		curGenre = knobProfile();
		// just loaded (or a pattern just cued): the stored pattern stands. CVs at GENRE / track genre inputs only
		// arrive with the cables and a sample later, so for a moment the genres are taken as they are
		bool adopting = adoptGenre > 0;
		if (adopting) {
			adoptGenre--;
			lastGenre = curGenre;
		}
		if (lastGenre >= 0 && curGenre != lastGenre && !pendingGenre)
			pendingGenre = true;
		for (int t = 0; t < 4; t++) {
			bool own = ownGenre(t);
			int tgt = trackTarget(t);
			bool sel = trackSelChanged[t].load(std::memory_order_relaxed) && trackSelChanged[t].exchange(false);
			if (adopting) {
				ownPrev[t] = own;
				lastTarget[t] = tgt;
				continue;
			}
			if (sel)
				pendingTrack[t] = true; // new choice in the genre field (also back to Global)
			if (params[LOCK_PARAM + t].getValue() > 0.5f)
				continue; // locked: CV changes wait; unlocked, they are caught up
			if (ownPrev[t] && !own)
				pendingTrack[t] = true; // CV unplugged -> back to the global genre
			if (own && tgt != lastTarget[t])
				pendingTrack[t] = true; // the track's genre CV moved to another genre
			lastTarget[t] = tgt;
			ownPrev[t] = own;
		}
	}

	void readChordInput() {
		transpSemis = inputs[TRANSP_INPUT].isConnected() ? (int)std::round(inputs[TRANSP_INPUT].getVoltage() * 12.f) : 0;
		int ch = inputs[CHORD_INPUT].getChannels();
		if (ch <= 0) {
			extChord = false;
			return;
		}
		int key = clamp((int)std::round(params[KEY_PARAM].getValue()), 0, 11) + transpSemis;
		uint16_t mask = 0;
		int lowest = 9999;
		for (int c = 0; c < ch; c++) {
			int note = (int)std::round(inputs[CHORD_INPUT].getVoltage(c) * 12.f) + 60;
			mask |= (uint16_t)(1 << (((note - key) % 12 + 12) % 12));
			lowest = std::min(lowest, note);
		}
		Chord c = chordFromPcs(mask, ((lowest - key) % 12 + 12) % 12);
		uint16_t m = 0;
		for (int i = 0; i < c.n; i++)
			m |= (uint16_t)(1 << c.iv[i]);
		extRoot = c.root;
		extMask = m;
		extChord = true;
	}

	// ------------------------------------------------------------ voices
	void queueDrum(int v, const Hit& h, float sr) {
		if (h.delay > 0.f) {
			DrumVoice& d = drums[v];
			d.delayCnt = h.delay * sr;
			d.pVel = h.vel;
			d.pRat = h.ratchet;
			return;
		}
		fireDrum(v, h.vel, h.ratchet, sr);
	}

	void fireDrum(int v, float vel, int ratchet, float sr) {
		DrumVoice& d = drums[v];
		d.vel = vel;
		float interval = stepPeriod / std::max(1, ratchet);
		d.trig = std::min(0.005f * sr, interval * 0.5f);
		d.ratLeft = ratchet - 1;
		d.ratInterval = interval;
		d.ratCounter = interval;
	}

	void startMono(MonoVoice& m, const NoteEv& n, float sr) {
		if (m.slidePending && m.gate > 0.f) {
			// glide: no retrigger, SLIDE high at the start of the target note
			m.slideOut = std::min(stepPeriod, 0.25f * sr);
		}
		else if (m.gate > 0.f)
			m.retrig = 0.002f * sr;
		m.slidePending = n.slide;
		m.pitch = (n.pitch - 60) / 12.f;
		m.vel = n.vel;
		m.acc = n.acc;
		m.gate = std::max(0.004f * sr, n.len * stepPeriod * n.gate);
	}

	void startPad(const PadEv& pe, float sr) {
		if (padV.gate > 0.f)
			padV.retrig = 0.002f * sr;
		for (int k = 0; k < 4; k++)
			padV.pitch[k] = (pe.notes[k] - 60) / 12.f;
		padV.vel = pe.vel;
		padV.gate = std::max(0.004f * sr, pe.len * stepPeriod * pe.gate);
	}

	void fireDrums(float sr) {
		for (int v = 0; v < NUM_DRUMS; v++)
			if (qDrums[v].on)
				queueDrum(v, qDrums[v], sr);
	}

	// events still waiting for their swing delay are played now (before the next step)
	void flushQueued(float sr) {
		if (dDrums >= 0.f)
			fireDrums(sr);
		if (dBass >= 0.f)
			startMono(bassV, qBass, sr);
		if (dMel >= 0.f)
			startMono(melV, qMel, sr);
		if (dPad >= 0.f)
			startPad(qPad, sr);
		dDrums = dBass = dMel = dPad = -1.f;
	}

	void processQueued(float sr) {
		if (dDrums >= 0.f && (dDrums -= 1.f) <= 0.f) {
			dDrums = -1.f;
			fireDrums(sr);
		}
		if (dBass >= 0.f && (dBass -= 1.f) <= 0.f) {
			dBass = -1.f;
			startMono(bassV, qBass, sr);
		}
		if (dMel >= 0.f && (dMel -= 1.f) <= 0.f) {
			dMel = -1.f;
			startMono(melV, qMel, sr);
		}
		if (dPad >= 0.f && (dPad -= 1.f) <= 0.f) {
			dPad = -1.f;
			startPad(qPad, sr);
		}
	}

	// swing of one track: odd 16ths are delayed by the swing of the genre the track plays
	float swingDelay(const Params& P, int t) {
		if (step % 2 == 0)
			return 0.f;
		return (effSwing(P, pat.trackGenre[t]) - 0.5f) * 2.f * stepPeriod;
	}

	// ------------------------------------------------------------ tick (one 16th, unswung)
	void doTick(float sr) {
		flushQueued(sr);
		int L = lengthSteps();
		if (pat.layoutLen != L)
			layoutTimeline(pat, L);
		step++;
		if (step >= L)
			step = 0;
		if (step % STEPS_PER_BAR == 0) {
			barIndex++;
			barPub.store(barIndex);
			if (takeCue(true)) {
				if (!running)
					return; // the song ended at this bar line
				step = 0; // a new pattern starts at its beginning
				L = lengthSteps();
				layoutTimeline(pat, L);
			}
		}
		if (step % STEPS_PER_BAR == 0) {
			// ABLAUF: fill / drop over the last bar of a song row, also when the row ends inside the loop
			arrFillEnd = -1;
			if (arrFill && arrRowEnd >= 0 && barIndex == arrRowEnd - 1) {
				arrFillEnd = step + STEPS_PER_BAR;
				fillActive = true;
			}
		}
		Params P = buildParams();
		if (step % STEPS_PER_BAR == 0) {
			bool changed = false;
			if (pendingGenre) {
				pendingGenre = false;
				doGenerate(false);
				changed = true;
			}
			for (int t = 0; t < 4; t++)
				if (pendingTrack[t]) {
					newTrack(t);
					changed = true;
				}
			if (changed)
				P = buildParams();
		}
		if (step == 0 && !firstPass)
			passCount++;
		P.pass = passCount;
		// sync outputs (unswung grid)
		clockPulse.trigger(2e-3f);
		if (step == 0)
			resetPulse.trigger(2e-3f);
		if (step % STEPS_PER_BAR == 0)
			barPulse.trigger(2e-3f);
		if (step == L - 1)
			endPulse.trigger(2e-3f);
		if (step == 0) {
			fillActive = fillCarry || rng.uni() < P.fill || arrFillEnd > 0;
			fillCarry = false;

			P.fillActive = fillActive;
			if (!firstPass) {
				float m = params[MUTATE_PARAM].getValue() + cv01(inputs[MUTATE_INPUT]);
				bool lk[NUM_TRACKS];
				locks(lk);
				if (m > 0.001f) {
					mutate(pat, clamp(m, 0.f, 1.f), rng, lk, chordLocked(), P);
					patChanges++;
				}
			}
			firstPass = false;
		}
		// compute this step for every track, play it after the track's swing delay
		drumStep(pat, P, step, arrFillEnd > 0 ? arrFillEnd : L, fillActive, qDrums);
		if (params[MUTE_PARAM + 0].getValue() < 0.5f && !(arrMute & 1)) {
			float d = swingDelay(P, T_DRUMS);
			if (d >= 1.f)
				dDrums = d;
			else
				fireDrums(sr);
		}
		qBass = bassAt(pat, P, step, L);
		if (qBass.on && params[MUTE_PARAM + 1].getValue() < 0.5f && !(arrMute & 2)) {
			float d = swingDelay(P, T_BASS);
			if (d >= 1.f)
				dBass = d;
			else
				startMono(bassV, qBass, sr);
		}
		qMel = melAt(pat, P, step, L);
		if (qMel.on && params[MUTE_PARAM + 2].getValue() < 0.5f && !(arrMute & 4)) {
			float d = swingDelay(P, T_MELODY);
			if (d >= 1.f)
				dMel = d;
			else
				startMono(melV, qMel, sr);
		}
		qPad = padAt(pat, P, step, L, padPrev, padHasPrev);
		if (qPad.on && params[MUTE_PARAM + 3].getValue() < 0.5f && !(arrMute & 8)) {
			float d = swingDelay(P, T_PADS);
			if (d >= 1.f)
				dPad = d;
			else
				startPad(qPad, sr);
		}
		// current chord on ROOT / CHORD (V/Oct, root between C4 and B4)
		{
			Chord ch = chordAt(pat, P, step);
			int rootPc = ((P.key + P.transp + ch.root) % 12 + 12) % 12;
			rootV = rootPc / 12.f;
			chordN = std::min(ch.n, 6);
			for (int i = 0; i < chordN; i++)
				chordV[i] = rootV + ch.iv[i] / 12.f;
		}
		// modulation: smooth rise over each chord
		int e = eventAt(pat, step);
		const ChordEvent& ce = pat.ev[clamp(e, 0, MAX_EVENTS - 1)];
		float prog = ce.len > 0 ? (step - ce.start + 0.5f) / ce.len : 0.f;
		modTarget = 5.f * (1.f - std::cos(3.14159265f * clamp(prog, 0.f, 1.f)));
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		step = -1;
		firstPass = true;
		running = true;
		chordMode = CM_MIXED;
		templateSel = -1;
		genreCvMode = 0;
		for (int t = 0; t < 4; t++) {
			trackSel[t] = -1;
			trackSelChanged[t] = false;
			pendingTrack[t] = false;
		}
		dDrums = dBass = dMel = dPad = -1.f;
		initPattern(pat);
		lastGenre = -1;
		curGenre = 0;
		doGenerate(true);
	}

	void onRandomize(const RandomizeEvent& e) override {
		Module::onRandomize(e);
		regenRequested = true;
	}

	// ------------------------------------------------------------ process
	void process(const ProcessArgs& args) override {
		float sr = args.sampleRate;

		if (newBtn.process(params[NEW_PARAM].getValue() > 0.5f) | newTrig.process(inputs[NEW_INPUT].getVoltage(), 0.1f, 1.f))
			regenRequested = true;
		if (runBtn.process(params[RUN_PARAM].getValue() > 0.5f) | runTrig.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
			running = !running;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			step = -1;
			phase = 0.f;
			pulseCount = 0;
			subTicksLeft = 0;
			dDrums = dBass = dMel = dPad = -1.f;
			resetArmed = true;
			passCount = 0;
			firstPass = true;
		}
		if (fillTrig.process(inputs[FILL_INPUT].getVoltage(), 0.1f, 1.f)) {
			fillActive = true; // this pass ends with a fill / drop
			if (step >= lengthSteps() - 4)
				fillCarry = true; // too late for this pass: the next one
		}

		// ---- arranger ABLAUF
		{
			int rq = arrRunReq.load(std::memory_order_relaxed) ? arrRunReq.exchange(0) : 0;
			if (rq == 1) { // the song starts from the top
				step = -1;
				phase = 0.f;
				pulseCount = 0;
				subTicksLeft = 0;
				dDrums = dBass = dMel = dPad = -1.f;
				resetArmed = true;
				passCount = 0;
				firstPass = true;
				running = true;
			}
			else if (rq == 2)
				running = false;
			if (cueState.load(std::memory_order_acquire) == 1)
				takeCue(false); // only a cue marked "now" is taken here
			if (snapState.load(std::memory_order_acquire) == 1) {
				snap.pat = pat;
				for (int t = 0; t < 4; t++)
					snap.sel[t] = (int8_t)trackSel[t];
				snap.lengthIdx = (int8_t)clamp((int)std::round(params[LENGTH_PARAM].getValue()), 0, 3);
				snap.mods = arrMods;
				snap.tag = -1;
				snap.used = true;
				snapP = buildParams(false);
				snapState.store(2, std::memory_order_release);
			}
			float alive = arrAlive.load(std::memory_order_relaxed);
			if (alive > 0.f) {
				alive -= args.sampleTime;
				arrAlive.store(alive, std::memory_order_relaxed);
				if (alive <= 0.f) { // ABLAUF is gone: its offsets and mutes go too, and a cue it left waiting
					int e = 1;
					cueState.compare_exchange_strong(e, 0);
					arrMods = ArrMods();
					arrMute = 0;
					arrFill = false;
					arrRowEnd = -1;
					arrSecPub = -1;
					arrRowPub = -1;
					arrSlotPub = -1;
				}
			}
		}

		{
			uint32_t h0 = editHead.load(std::memory_order_acquire);
			if (h0 != editTail.load(std::memory_order_relaxed))
				patChanges++;
		}
		applyChordEdits();
		if (gridState.load(std::memory_order_acquire) == 1) {
			copyTrack(gridBuf, pat, gridTrack);
			if (gridTrack == T_PADS)
				padHasPrev = false;
			gridState.store(0, std::memory_order_release);
			patChanges++;
		}
		for (int t = 0; t < 4; t++)
			if (gridGen[t].load(std::memory_order_relaxed) && gridGen[t].exchange(false))
				newTrack(t);
		if (importReady) {
			patChanges++;
			pat = importBuf;
			params[LENGTH_PARAM].setValue((float)importLengthIndex);
			importReady = false;
			padHasPrev = false;
		}
		if (resetTracksRequested.load(std::memory_order_relaxed) && resetTracksRequested.exchange(false))
			for (int t = 0; t < 4; t++)
				setTrackSel(t, -1);
		readGenre();
		readChordInput();
		if (regenRequested.load(std::memory_order_relaxed) && regenRequested.exchange(false)) {
			pendingGenre = false;
			doGenerate(true);
		}
		for (int t = 0; t < 4; t++) {
			bool trig = tnewBtn[t].process(params[TNEW_PARAM + t].getValue() > 0.5f);
			trig |= tnewTrig[t].process(inputs[TNEW_INPUT + t].getVoltage(), 0.1f, 1.f);
			if (trig)
				newTrack(t); // an explicit per-track NEW works even when the track is locked
		}
		{
			bool trig = chordNewBtn.process(params[CHORDNEW_PARAM].getValue() > 0.5f);
			trig |= chordNewTrig.process(inputs[CHORDNEW_INPUT].getVoltage(), 0.1f, 1.f);
			if (trig) {
				Params P = buildParams();
				generateChords(pat, curGenre, rng, P);
				layoutTimeline(pat, lengthSteps());
				chordNewFlash = 1.f;
				patChanges++;
			}
		}
		if (chordRegenRequested.load(std::memory_order_relaxed) && chordRegenRequested.exchange(false) && !chordLocked()) {
			Params P = buildParams();
			generateChords(pat, curGenre, rng, P);
			layoutTimeline(pat, lengthSteps());
			patChanges++;
		}

		// ---- clock
		bool tick = false;
		samplesSinceClock += 1.f;
		if (inputs[CLOCK_INPUT].isConnected()) {
			if (clockTrig.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 1.f)) {
				if (samplesSinceClock < sr * 4.f)
					clockPeriod = samplesSinceClock;
				samplesSinceClock = 0.f;
				static const float PPS[4] = {1.f, 0.5f, 0.25f, 6.f};
				float pps = PPS[clamp(clockMode, 0, 3)];
				if (pps >= 1.f) {
					if (resetArmed)
						pulseCount = (int)pps - 1; // first pulse after reset plays step 0
					if (++pulseCount >= (int)pps) {
						pulseCount = 0;
						tick = true;
					}
					if (clockPeriod > 0.f)
						stepPeriod = clockPeriod * pps;
				}
				else {
					tick = true;
					int sub = (int)std::round(1.f / pps);
					if (clockPeriod > 0.f) {
						stepPeriod = clockPeriod / sub;
						subTicksLeft = sub - 1;
						subInterval = stepPeriod;
						subCounter = subInterval;
					}
				}
				resetArmed = false;
			}
			if (subTicksLeft > 0) {
				subCounter -= 1.f;
				if (subCounter <= 0.f) {
					tick = true;
					subTicksLeft--;
					subCounter += subInterval;
				}
			}
		}
		else {
			float bpm = params[BPM_PARAM].getValue();
			if (inputs[BPM_INPUT].isConnected())
				bpm = clamp(bpm + inputs[BPM_INPUT].getVoltage() * 10.f, 40.f, 220.f);
			stepPeriod = sr * 60.f / bpm / 4.f;
			phase += 1.f / stepPeriod;
			if (phase >= 1.f || resetArmed) {
				phase = resetArmed ? 0.f : phase - 1.f;
				resetArmed = false;
				tick = true;
			}
		}

		if (running) {
			processQueued(sr);
			if (tick)
				doTick(sr);
		}

		// ---- outputs
		for (int v = 0; v < NUM_DRUMS; v++) {
			DrumVoice& d = drums[v];
			if (d.delayCnt >= 0.f) {
				d.delayCnt -= 1.f;
				if (d.delayCnt < 0.f)
					fireDrum(v, d.pVel, d.pRat, sr);
			}
			if (d.ratLeft > 0) {
				d.ratCounter -= 1.f;
				if (d.ratCounter <= 0.f) {
					d.ratLeft--;
					d.ratCounter += d.ratInterval;
					d.trig = std::min(0.005f * sr, d.ratInterval * 0.5f);
				}
			}
			float out = 0.f;
			if (d.trig > 0.f) {
				d.trig -= 1.f;
				out = 10.f * d.vel;
			}
			outputs[DRUM_OUTPUT + v].setVoltage(out);
			drumOut[v] = out;
		}
		if (outputs[DRUMS_OUTPUT].isConnected()) {
			outputs[DRUMS_OUTPUT].setChannels(NUM_DRUMS);
			for (int v = 0; v < NUM_DRUMS; v++)
				outputs[DRUMS_OUTPUT].setVoltage(drumOut[v], v);
		}
		auto mono = [&](MonoVoice& m, int voct, int gate, int vel, int acc, int slide) {
			bool g = m.gate > 0.f && m.retrig <= 0.f;
			if (m.gate > 0.f)
				m.gate -= 1.f;
			else
				m.slidePending = false;
			if (m.retrig > 0.f)
				m.retrig -= 1.f;
			outputs[voct].setVoltage(m.pitch);
			outputs[gate].setVoltage(g ? 10.f : 0.f);
			outputs[vel].setVoltage(10.f * m.vel);
			outputs[acc].setVoltage(g && m.acc ? 10.f : 0.f);
			outputs[slide].setVoltage(m.slideOut > 0.f ? 10.f : 0.f);
			if (m.slideOut > 0.f)
				m.slideOut -= 1.f;
		};
		mono(bassV, BASS_VOCT_OUTPUT, BASS_GATE_OUTPUT, BASS_VEL_OUTPUT, BASS_ACC_OUTPUT, BASS_SLIDE_OUTPUT);
		mono(melV, MEL_VOCT_OUTPUT, MEL_GATE_OUTPUT, MEL_VEL_OUTPUT, MEL_ACC_OUTPUT, MEL_SLIDE_OUTPUT);
		{
			bool g = padV.gate > 0.f && padV.retrig <= 0.f;
			if (padV.gate > 0.f)
				padV.gate -= 1.f;
			if (padV.retrig > 0.f)
				padV.retrig -= 1.f;
			outputs[PAD_VOCT_OUTPUT].setChannels(4);
			outputs[PAD_GATE_OUTPUT].setChannels(4);
			for (int k = 0; k < 4; k++) {
				outputs[PAD_VOCT_OUTPUT].setVoltage(padV.pitch[k], k);
				outputs[PAD_GATE_OUTPUT].setVoltage(g ? 10.f : 0.f, k);
			}
			outputs[PAD_VEL_OUTPUT].setVoltage(10.f * padV.vel);
			modValue += (modTarget - modValue) * std::min(1.f, 20.f / sr);
			outputs[PAD_MOD_OUTPUT].setVoltage(modValue);
		}

		// ---- sync / harmony outputs
		{
			float st = args.sampleTime;
			bool c = clockPulse.process(st), r = resetPulse.process(st), b = barPulse.process(st), en = endPulse.process(st);
			outputs[CLOCK_OUTPUT].setVoltage(c ? 10.f : 0.f);
			outputs[RESET_OUTPUT].setVoltage(r ? 10.f : 0.f);
			outputs[BAR_OUTPUT].setVoltage(b ? 10.f : 0.f);
			outputs[END_OUTPUT].setVoltage(en ? 10.f : 0.f);
			outputs[RUN_OUTPUT].setVoltage(running ? 10.f : 0.f);
			int L = lengthSteps();
			outputs[FILL_OUTPUT].setVoltage(running && fillActive && step >= L - 8 && L >= 16 ? 10.f : 0.f);
			outputs[ROOT_OUTPUT].setVoltage(rootV);
			outputs[CHORD_OUTPUT].setChannels(std::max(1, chordN));
			for (int i = 0; i < std::max(1, chordN); i++)
				outputs[CHORD_OUTPUT].setVoltage(chordN ? chordV[i] : rootV, i);
		}

		// for instruments / MIX that follow without cables: this frame complete, read in the next one (Follow.cpp)
		{
			float(*p)[4] = followPub[args.frame & 1];
			for (int o = 0; o < OUTPUTS_LEN; o++)
				for (int c = 0; c < 4; c++)
					p[o][c] = outputs[o].voltages[c];
		}

		// ---- lights
		if (lightDivider.process()) {
			float dt = args.sampleTime * lightDivider.getDivision();
			blink += dt * 3.f;
			if (blink >= 1.f)
				blink -= 1.f;
			newFlash = std::max(0.f, newFlash - dt * 4.f);
			lights[NEW_LIGHT].setBrightness(newFlash);
			lights[RUN_LIGHT].setBrightness(running ? 1.f : 0.f);
			lights[CHORDLOCK_LIGHT].setBrightness(chordLocked() ? 1.f : 0.f);
			chordNewFlash = std::max(0.f, chordNewFlash - dt * 4.f);
			lights[CHORDNEW_LIGHT].setBrightness(chordNewFlash);
			for (int t = 0; t < 4; t++) {
				tnewFlash[t] = std::max(0.f, tnewFlash[t] - dt * 4.f);
				lights[TNEW_LIGHT + t].setBrightness(std::max(tnewFlash[t], pendingTrack[t] && blink < 0.5f ? 0.4f : 0.f));
				lights[MUTE_LIGHT + t].setBrightness(std::max(params[MUTE_PARAM + t].getValue(), (arrMute >> t) & 1 ? 0.35f : 0.f));
				lights[LOCK_LIGHT + t].setBrightness(params[LOCK_PARAM + t].getValue());
			}
		}
	}

	// ------------------------------------------------------------ persistence
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "running", json_boolean(running));
		json_object_set_new(root, "clockMode", json_integer(clockMode));
		json_object_set_new(root, "chordMode", json_integer(chordMode));
		json_object_set_new(root, "templateSel", json_integer(templateSel));
		json_object_set_new(root, "genreCvMode", json_integer(genreCvMode));
		std::vector<uint8_t> blob = savePattern(pat);
		json_object_set_new(root, "pattern3", json_string(string::toBase64(blob.data(), blob.size()).c_str()));
		json_t* sel = json_array();
		for (int t = 0; t < 4; t++)
			json_array_append_new(sel, json_integer(trackSel[t]));
		json_object_set_new(root, "trackGenre", sel);
		if (arrAlive.load() > 0.f) {
			// the offsets and mutes of the pattern ABLAUF loaded (ABLAUF picks up again after loading)
			json_t* a = json_object();
			json_t* d = json_array();
			for (int t = 0; t < 4; t++)
				json_array_append_new(d, json_real(arrMods.dens[t]));
			json_object_set_new(a, "dens", d);
			json_object_set_new(a, "ghost", json_real(arrMods.ghost));
			json_object_set_new(a, "fill", json_real(arrMods.fill));
			json_object_set_new(a, "mute", json_integer(arrMute));
			json_object_set_new(a, "section", json_integer(arrSecPub.load()));
			json_object_set_new(a, "slot", json_integer(arrSlotPub.load()));
			json_object_set_new(root, "arranger", a);
		}
		return root;
	}

	static void readSel(json_t* arr, int8_t out[4]) {
		for (int t = 0; t < 4; t++) {
			json_t* v = arr ? json_array_get(arr, t) : nullptr;
			int p = v ? (int)json_integer_value(v) : -1;
			out[t] = (int8_t)(p >= 0 && p < numProfiles() ? p : -1);
		}
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "running")))
			running = json_boolean_value(j);
		if ((j = json_object_get(root, "clockMode")))
			clockMode = clamp((int)json_integer_value(j), 0, 3);
		if ((j = json_object_get(root, "chordMode")))
			chordMode = clamp((int)json_integer_value(j), 0, 2);
		if ((j = json_object_get(root, "templateSel")))
			templateSel = (int)json_integer_value(j);
		if ((j = json_object_get(root, "genreCvMode")))
			genreCvMode = clamp((int)json_integer_value(j), 0, 1);
		// "pattern3": version-independent format (v3+); "pattern": raw block of GENRA v2
		json_t* jp = json_object_get(root, "pattern3");
		if (!jp)
			jp = json_object_get(root, "pattern");
		if (jp && json_is_string(jp)) {
			Pattern p;
			if (loadPattern(string::fromBase64(json_string_value(jp)), p)) {
				pat = p;
				lastGenre = curGenre = knobProfile();
				pendingGenre = false;
				padHasPrev = false;
				adoptGenre = 4800; // ~0.1 s: until the cables (and their CVs) are there
			}
		}
		int8_t sel[4];
		readSel(json_object_get(root, "trackGenre"), sel); // missing (v2 patch): all tracks follow GENRE
		for (int t = 0; t < 4; t++) {
			trackSel[t] = sel[t];
			trackSelChanged[t] = false;
			pendingTrack[t] = false;
			ownPrev[t] = sel[t] >= 0;
		}
		arrMods = ArrMods();
		arrMute = 0;
		if (json_t* a = json_object_get(root, "arranger")) {
			json_t* d = json_object_get(a, "dens");
			for (int t = 0; t < 4; t++)
				if (json_t* v = d ? json_array_get(d, t) : nullptr)
					arrMods.dens[t] = (float)json_number_value(v);
			if ((j = json_object_get(a, "ghost")))
				arrMods.ghost = (float)json_number_value(j);
			if ((j = json_object_get(a, "fill")))
				arrMods.fill = (float)json_number_value(j);
			if ((j = json_object_get(a, "mute")))
				arrMute = (uint8_t)json_integer_value(j);
			if ((j = json_object_get(a, "section")))
				arrSecPub = (int)json_integer_value(j);
			if ((j = json_object_get(a, "slot")))
				arrSlotPub = clamp((int)json_integer_value(j), -1, 127); // ABLAUF: 4 banks of 32
			arrAlive = 3.f; // ABLAUF has a few seconds to show up
		}
	}

	/** The tempo: from the clock input when patched, else the BPM knob (UI thread: an estimate is fine). */
	float currentBpm() {
		if (inputs[CLOCK_INPUT].isConnected() && stepPeriod > 0.f)
			return 60.f * APP->engine->getSampleRate() / (stepPeriod * 4.f);
		return params[BPM_PARAM].getValue();
	}

	// ------------------------------------------------------------ MIDI file actions (UI thread)
	// onlyTrack: -1 = all four tracks, 0..3 = just drums / bass / melody / pads
	void exportMidi(int onlyTrack = -1) {
		static const char* TRK[4] = {"drums", "bass", "melody", "pads"};
		osdialog_filters* filters = osdialog_filters_parse("MIDI file:mid,midi");
		std::string name = std::string("genra-") + prof(curGenre).name;
		if (onlyTrack >= 0 && onlyTrack < 4)
			name += std::string("-") + TRK[onlyTrack];
		name += ".mid";
		for (char& c : name)
			if (c == ' ' || c == '&')
				c = '_';
		char* pathC = osdialog_file(OSDIALOG_SAVE, NULL, name.c_str(), filters);
		osdialog_filters_free(filters);
		if (!pathC)
			return;
		std::string path = pathC;
		std::free(pathC);
		if (system::getExtension(path) != ".mid" && system::getExtension(path) != ".midi")
			path += ".mid";
		Params P = buildParams();
		int L = lengthSteps();
		float bpm = currentBpm();
		std::vector<MidiNote> notes = renderLoop(pat, P, L);
		if (onlyTrack >= 0)
			notes.erase(std::remove_if(notes.begin(), notes.end(), [=](const MidiNote& n) { return n.track != onlyTrack; }),
			            notes.end());
		std::vector<uint8_t> data = writeSmf(notes, bpm, prof(curGenre).name, L, &pat);
		try {
			system::writeFile(path, data);
		}
		catch (Exception& e) {
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, ("Saving failed: " + std::string(e.what())).c_str());
		}
	}

	void importMidi() {
		osdialog_filters* filters = osdialog_filters_parse("MIDI file:mid,midi");
		char* pathC = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
		osdialog_filters_free(filters);
		if (!pathC)
			return;
		std::string path = pathC;
		std::free(pathC);
		std::vector<uint8_t> data;
		try {
			data = system::readFile(path);
		}
		catch (Exception& e) {
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, "Could not read the file.");
			return;
		}
		if (importReady)
			return;
		Pattern p = pat;
		Params P = buildParams();
		std::string err;
		int bars = importSmf(data, p, P, err);
		if (bars <= 0) {
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, err.c_str());
			return;
		}
		importBuf = p;
		importLengthIndex = bars <= 1 ? 0 : (bars <= 2 ? 1 : (bars <= 4 ? 2 : 3));
		importReady = true;
	}
};

