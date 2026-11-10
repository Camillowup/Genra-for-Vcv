// GENRA – module definition (shared by the GENRA module and the grid editor)
#pragma once
#include <thread>
#include "plugin.hpp"
#include "Engine.hpp"
#include "Arranger.hpp"
#include "Layout.hpp"
#include "Follow.hpp"
#include "link/GenraPorts.hpp"
#include "MidiFolder.hpp"
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
static const Family FAMILIES[14] = {
	{"House", 0, 5}, {"Techno", 5, 5}, {"Breaks & Bass", 10, 5}, {"Hip-Hop & Downtempo", 15, 6}, {"Dub · Synthwave · Ambient", 21, 3},
	{"Afro", 24, 4}, {"Techno Styles", 28, 5}, {"Latin & Club", 33, 4},
	{"House · Funky · Gqom", 37, 5}, {"Breaks & Club", 42, 5}, {"Hard & Fast", 47, 3}, {"Reggae & Latin", 50, 5}, {"Ambient & Drone", 55, 4},
	{"Dark", 59, 5}};

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
		// 2.25: transport pads
		FILLPAD_PARAM,
		STOP_PARAM,
		TAP_PARAM,
		MAKESONG_PARAM,
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
		// 2.25: transport pads
		FILLPAD_LIGHT,
		STOP_LIGHT,
		TAP_LIGHT,
		MAKESONG_LIGHT,
		LIGHTS_LEN
	};

	// ---- pattern & engine state
	Pattern pat;
	Rng rng;
	bool running = false; // GENRA waits for RUN after loading / inserting
	int step = -1;
	bool firstPass = true;
	bool fillActive = false;
	bool fillCarry = false; // FILL trigger in the last beat: also fill the next pass
	bool fillPadReq = false; // FILL pad: a fill over the next bar
	bool fillPadOn = false;  // ... playing now (fillWas: the pass's own fill decision, back after that bar)
	bool fillWas = false;
	// TAP: taps of the last few beats (samples between them)
	float tapSince = 1e9f;
	float tapIv[4] = {};
	int tapN = 0;
	float stopFlash = 0.f, tapFlash = 0.f;
	// genre changes set the tempo of the genre (right-click, off by default)
	bool genreTempo = false;
	// MAKE SONG pad: ABLAUF builds a song from the running loop (ABLAUF reads makeSongReq, the widget noAblauf)
	std::atomic<bool> makeSongReq{false};
	std::atomic<bool> noAblauf{false};
	bool pendingGenre = false;
	int adoptGenre = 0;      // samples left in which the genres are taken as they are (after load / cue)
	int lastTarget[4] = {-1, -1, -1, -1};
	int lastGenre = -1;
	int clockMode = 0;
	int clockOutMode = 0; // CLOCK output: 0 16ths, 1 8ths, 2 quarters, 3 24 PPQN (CLOCK_MODES)
	int ppqnLeft = 0;     // 24 PPQN: pulses still to come within this step
	float ppqnIv = 0.f, ppqnCnt = 0.f;
	int chordMode = CM_MIXED;
	int templateSel = -1;
	int genreCvMode = 0;
	// 0-10 V mode spans this many genres: 37 for patches saved before v2.24 (so their CV keeps hitting the same
	// genres), all genres for new modules
	int genreCvSpan = NUM_GENRES;
	// ---- 2.30: MUTATE around an anchor (after JIANT's MUTATE modes): the anchor is the pattern before mutating (what
	// was made, edited or cued; any change but MUTATE's own sets it anew). Audio thread only, but for the request flag.
	int mutateMode = 0;  // 0 DRIFT (the changes add up, as before), 1 ORBIT (each pass: the anchor, changed), 2 PULSE (changed, back, changed ...)
	int mutateWhat = 0;  // 0 everything, 1 rhythm (drums), 2 notes (bass, melody), 3 harmony (chords, pads)
	Pattern anchor;
	bool anchorValid = false;
	uint32_t anchorStamp = 0;            // patChanges after MUTATE's last change
	std::atomic<bool> anchorBackReq{false}; // the menu: back to the anchor
	bool melPhrase = true;               // new lead / sparse melodies as phrases (Params::melPhrase)
	// ---- 2.31: NEW picks presets for the instruments that follow (Follow GENRA): 0 off, 1 all genres, 2 the track's genre
	int presetMode = 2;
	std::atomic<uint32_t> presetRoll[4]; // NEW (all, or one track) per GENRA track: the UI thread rolls the followers' presets
	uint32_t presetSeen[4] = {};         // (UI thread)
	bool presetSeenInit = false;
	/** The mode for track t's followers (GENRA BOX: its SOUND button and per-track choice). */
	virtual int presetModeFor(int t) { return presetMode; }
	/** UI thread, every frame (the panel's step): the followers of a track NEW rolled load a new preset. */
	void uiPresetRolls() {
		for (int t = 0; t < 4; t++) {
			uint32_t r = presetRoll[t].load();
			if (!presetSeenInit)
				presetSeen[t] = r;
			if (r == presetSeen[t])
				continue;
			presetSeen[t] = r;
			int mode = presetModeFor(t);
			if (mode > 0)
				follow::rollFollowerPresets(this, t, prof(clamp((int) pat.trackGenre[t], 0, numProfiles() - 1)).name, mode);
		}
		presetSeenInit = true;
	}

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
	// ABLAUF <-> GRID (both UI thread): the pad GENRA plays ("A5 · DROP 1", -1 / empty = none known) and a
	// request to store what GENRA plays on that pad (like ABLAUF's "Overwrite with GENRA")
	int arrPadSlot = -1;
	std::string arrPadLabel;
	int padSaveReq = -1;
	// play position in steps (step + fraction of the step), -1 while stopped; read by the GRID (MIDI record)
	std::atomic<float> posPub{-1.f};
	// the step each drum voice's latest hit belongs to (a hit nudged earlier fires in the step before: SCHLAG takes its
	// sample and its PLAY / SKIP count from here)
	std::atomic<int> drumStepPub[NUM_DRUMS];
	float sinceTick = 0.f;
	ArrMods arrMods;
	uint8_t arrMute = 0;
	bool arrFill = false;
	int64_t arrRowEnd = -1;
	int arrRowBars = 0; // the song row's length in bars (with arrRowEnd: where in the row GENRA is)
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
			anchorValid = false; // the cued pattern: MUTATE's new anchor
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
			arrRowBars = cue.rowBars > 0 ? cue.rowBars : 0;
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
	dsp::BooleanTrigger newBtn, runBtn, chordNewBtn, fillPadBtn, stopBtn, tapBtn, makeSongBtn;
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
	// nudged ahead of their step (grid NUDGE < 0): started one tick early
	NoteEv qBassE, qMelE;
	PadEv qPadE;
	float dBassE = -1.f, dMelE = -1.f, dPadE = -1.f;
	bool earlyDone[NUM_TRACKS] = {false, false, false, false};
	bool earlyDrum[NUM_DRUMS] = {};

	// ---- voices
	struct DrumVoice {
		float vel = 0.f;
		float trig = 0.f;
		int ratLeft = 0;
		float ratInterval = 0.f, ratCounter = 0.f;
		float delayCnt = -1.f; // humanize: pending hit
		float pVel = 0.f;
		int pRat = 1;
		int pStep = -1; // the step the pending hit belongs to
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
	static_assert(genraports::PUB_CHANNELS == 4, "followPub channels");
	// instruments and MIX that follow this GENRA without cables let go (Follow.hpp)
	void onRemove(const RemoveEvent& e) override { follow::gone(this); }

	~Genra() {
		follow::gone(this);
		std::lock_guard<std::mutex> lk(genraRegMutex());
		genraRegistry().erase(this);
	}

	// extra: params / inputs / outputs / lights a module built on GENRA (GENRA BOX) appends after GENRA's own
	explicit Genra(int extraParams = 0, int extraInputs = 0, int extraOutputs = 0, int extraLights = 0) {
		// followers of the other plugin (GENRA Instruments) reach GENRA's outputs and tempo through the link hub
		follow::addGenra(this, &followPub[0][0][0], OUTPUTS_LEN, &stepPeriod);
		for (int v = 0; v < NUM_DRUMS; v++)
			drumStepPub[v].store(-1);
		{
			std::lock_guard<std::mutex> lk(genraRegMutex());
			genraRegistry().insert(this);
		}
		initGenres();
		for (int t = 0; t < 4; t++) {
			trackSel[t] = -1;
			trackSelChanged[t] = false;
			gridGen[t] = false;
			presetRoll[t] = 0;
		}
		config(PARAMS_LEN + extraParams, INPUTS_LEN + extraInputs, OUTPUTS_LEN + extraOutputs, LIGHTS_LEN + extraLights);
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
		configButton(RUN_PARAM, "RUN (from the start of the loop)");
		configButton(STOP_PARAM, "STOP");
		configButton(FILLPAD_PARAM, "FILL (a fill over the next bar)");
		configButton(TAP_PARAM, "TAP tempo (internal clock)");
		configButton(MAKESONG_PARAM, "MAKE SONG (in ABLAUF, from the running loop)");
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
		for (int id : std::initializer_list<int>{RUN_PARAM, NEW_PARAM, CHORDLOCK_PARAM, CHORDNEW_PARAM, FILLPAD_PARAM, STOP_PARAM,
		                                         TAP_PARAM, MAKESONG_PARAM})
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
		configOutput(CLOCK_OUTPUT, "Clock (16ths; other resolutions: right-click › Clock output)");
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
		P.melPhrase = melPhrase;
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
	float genreCvSteps(float v) { return genreCvMode == 1 ? v * 12.f : v / 10.f * (genreCvSpan - 1); }

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
	// forStep: the step the hit belongs to (-1: the current one)
	void queueDrum(int v, const Hit& h, float sr, int forStep = -1) {
		if (forStep < 0)
			forStep = step;
		if (h.delay > 0.f) {
			DrumVoice& d = drums[v];
			if (d.delayCnt >= 0.f) { // a hit is still waiting: play it now, it must not get lost
				d.delayCnt = -1.f;
				fireDrum(v, d.pVel, d.pRat, sr, d.pStep);
			}
			d.delayCnt = h.delay * sr;
			d.pVel = h.vel;
			d.pRat = h.ratchet;
			d.pStep = forStep;
			return;
		}
		fireDrum(v, h.vel, h.ratchet, sr, forStep);
	}

	void fireDrum(int v, float vel, int ratchet, float sr, int forStep) {
		DrumVoice& d = drums[v];
		drumStepPub[v].store(forStep >= 0 ? forStep : step, std::memory_order_relaxed);
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
		if (dBassE >= 0.f)
			startMono(bassV, qBassE, sr);
		if (dMelE >= 0.f)
			startMono(melV, qMelE, sr);
		if (dPadE >= 0.f)
			startPad(qPadE, sr);
		dDrums = dBass = dMel = dPad = -1.f;
		dBassE = dMelE = dPadE = -1.f;
	}

	void clearQueued() {
		dDrums = dBass = dMel = dPad = -1.f;
		dBassE = dMelE = dPadE = -1.f;
		for (int t = 0; t < NUM_TRACKS; t++)
			earlyDone[t] = false;
		for (int v = 0; v < NUM_DRUMS; v++)
			earlyDrum[v] = false;
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
		if (dBassE >= 0.f && (dBassE -= 1.f) <= 0.f) {
			dBassE = -1.f;
			startMono(bassV, qBassE, sr);
		}
		if (dMelE >= 0.f && (dMelE -= 1.f) <= 0.f) {
			dMelE = -1.f;
			startMono(melV, qMelE, sr);
		}
		if (dPadE >= 0.f && (dPadE -= 1.f) <= 0.f) {
			dPadE = -1.f;
			startPad(qPadE, sr);
		}
	}

	// swing of one track (or drum voice): odd 16ths are delayed by the swing of the genre the track plays, or by the
	// track's / voice's own swing set in the GRID
	float swingDelay(const Params& P, int t) { return swingAt(P, t, step); }
	float swingAt(const Params& P, int t, int st, int voice = -1) {
		if (st % 2 == 0)
			return 0.f;
		return (swingOf(pat, P, t, voice) - 0.5f) * 2.f * stepPeriod;
	}
	// grid NUDGE in samples (1/24 step each)
	float nudgeSamples(int n) const { return n * stepPeriod / 24.f; }
	bool trackMuted(int t) { return params[MUTE_PARAM + t].getValue() >= 0.5f || ((arrMute >> t) & 1); }

	// one event of a mono / pad track at its offset (swing + nudge) from the step; an event nudged ahead of the
	// step was already started on the previous tick (lookahead) unless that tick did not know it
	template <typename Ev, typename Start>
	void placeEvent(int t, const Ev& ev, float& dq, Ev& q, float off, Start start) {
		if (off < 0.f) {
			if (!earlyDone[t])
				start(ev);
			return;
		}
		q = ev;
		if (off >= 1.f)
			dq = off;
		else
			start(ev);
	}

	// lookahead: events of the next step that are nudged ahead of it start during this step
	void queueEarly(const Params& P, int L, int fillEnd, float sr) {
		const int next = step + 1 >= L ? 0 : step + 1;
		Params Pn = P;
		if (next == 0)
			Pn.pass = passCount + 1;
		for (int t = 0; t < NUM_TRACKS; t++)
			earlyDone[t] = false;
		for (int v = 0; v < NUM_DRUMS; v++)
			earlyDrum[v] = false;
		if (pat.fixed[T_DRUMS] && !trackMuted(T_DRUMS)) {
			Hit hn[NUM_DRUMS];
			drumStep(pat, Pn, next, fillEnd > 0 && next > step ? fillEnd : L, fillActive, hn);
			for (int v = 0; v < NUM_DRUMS; v++) {
				if (!hn[v].on || hn[v].nudge >= 0)
					continue;
				float off = swingAt(Pn, T_DRUMS, next, v) + nudgeSamples(hn[v].nudge);
				if (off >= 0.f)
					continue;
				Hit h = hn[v];
				h.delay += std::max(1.f, stepPeriod + off) / sr;
				queueDrum(v, h, sr, next); // fires in this step, belongs to the next
				earlyDrum[v] = true;
			}
		}
		if (pat.fixed[T_BASS] && !trackMuted(T_BASS)) {
			NoteEv n = bassAt(pat, Pn, next, L);
			float off = n.on && n.nudge < 0 ? swingAt(Pn, T_BASS, next) + nudgeSamples(n.nudge) : 0.f;
			if (off < 0.f) {
				qBassE = n;
				dBassE = std::max(1.f, stepPeriod + off);
				earlyDone[T_BASS] = true;
			}
		}
		if (pat.fixed[T_MELODY] && !trackMuted(T_MELODY)) {
			NoteEv n = melAt(pat, Pn, next, L);
			float off = n.on && n.nudge < 0 ? swingAt(Pn, T_MELODY, next) + nudgeSamples(n.nudge) : 0.f;
			if (off < 0.f) {
				qMelE = n;
				dMelE = std::max(1.f, stepPeriod + off);
				earlyDone[T_MELODY] = true;
			}
		}
		if (pat.fixed[T_PADS] && !trackMuted(T_PADS)) {
			// fixed pads keep their exact notes: evaluating them twice leaves the voicing state unchanged
			PadEv e = padAt(pat, Pn, next, L, padPrev, padHasPrev);
			float off = e.on && e.nudge < 0 ? swingAt(Pn, T_PADS, next) + nudgeSamples(e.nudge) : 0.f;
			if (off < 0.f) {
				qPadE = e;
				dPadE = std::max(1.f, stepPeriod + off);
				earlyDone[T_PADS] = true;
			}
		}
	}

	// ------------------------------------------------------------ tick (one 16th, unswung)
	void doTick(float sr) {
		flushQueued(sr);
		sinceTick = 0.f;
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
			if (fillPadOn) { // the bar of the FILL pad is over: the pass's own decision again
				fillPadOn = false;
				fillActive = fillWas;
			}
			if (arrFill && arrRowEnd >= 0 && barIndex == arrRowEnd - 1) {
				arrFillEnd = step + STEPS_PER_BAR;
				fillActive = true;
			}
			else if (fillPadReq || (arrRowEnd >= 0 && arrRowBars > 0 &&
			                        partPhraseFill(pat, (int)(barIndex - (arrRowEnd - arrRowBars)), arrRowBars))) {
				// FILL pad, or the end of an 8-bar phrase inside a long verse / drop row: this bar is a fill
				fillPadReq = false;
				fillPadOn = true;
				fillWas = fillActive;
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
		// CLOCK output in the chosen resolution (straight, from the first step of the loop)
		if (clockOutMode == 0 || clockOutMode == 3 || (clockOutMode == 1 && step % 2 == 0) || (clockOutMode == 2 && step % 4 == 0))
			clockPulse.trigger(2e-3f);
		if (clockOutMode == 3) { // 24 PPQN: 6 pulses per step, the other 5 between the ticks
			ppqnLeft = 5;
			ppqnIv = ppqnCnt = stepPeriod / 6.f;
		}
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
				bool chLk = chordLocked();
				switch (mutateWhat) { // what MUTATE may change
					case 1: lk[T_BASS] = lk[T_MELODY] = lk[T_PADS] = chLk = true; break;
					case 2: lk[T_DRUMS] = lk[T_PADS] = chLk = true; break;
					case 3: lk[T_DRUMS] = lk[T_BASS] = lk[T_MELODY] = true; break;
					default: break;
				}
				if (m > 0.001f) {
					if (!anchorValid || patChanges.load() != anchorStamp) { // changed by anything else: the new anchor
						anchor = pat;
						anchorValid = true;
					}
					if (mutateMode == 1 || mutateMode == 2)
						pat = anchor; // ORBIT / PULSE: every change starts from the anchor
					if (!(mutateMode == 2 && passCount % 2 == 0)) // PULSE: every other pass the anchor itself
						mutate(pat, clamp(m, 0.f, 1.f), rng, lk, chLk, P);
					patChanges++;
					anchorStamp = patChanges.load();
				}
			}
			firstPass = false;
		}
		// compute this step for every track, play it after the track's swing delay
		drumStep(pat, P, step, arrFillEnd > 0 ? arrFillEnd : L, fillActive, qDrums);
		// a song part runs its course over the row (an intro coming in, a build's roll, a break's breakdown ...)
		const PartPos pp = curPartPos(L);
		if (pp.sec >= 0 && (pat.partMask & (1u << T_DRUMS)))
			shapeDrums(pp, qDrums);
		if (!trackMuted(T_DRUMS)) {
			float d = swingDelay(P, T_DRUMS);
			// nudged hits (edited drums) and voices with their own swing play on their own: swing + nudge per voice
			for (int v = 0; v < NUM_DRUMS; v++) {
				if (!qDrums[v].on || (qDrums[v].nudge == 0 && !pat.vSwing[v]))
					continue;
				Hit h = qDrums[v];
				qDrums[v].on = false;
				float off = swingAt(P, T_DRUMS, step, v) + nudgeSamples(h.nudge);
				if (off < 0.f) {
					if (!earlyDrum[v])
						queueDrum(v, h, sr);
					continue;
				}
				h.delay += off / sr;
				queueDrum(v, h, sr);
			}
			if (d >= 1.f)
				dDrums = d;
			else
				fireDrums(sr);
		}
		NoteEv nb = bassAt(pat, P, step, L);
		if (pp.sec >= 0 && (pat.partMask & (1u << T_BASS)))
			shapeNote(pp, T_BASS, nb);
		qBass.on = false;
		if (nb.on && !trackMuted(T_BASS))
			placeEvent(T_BASS, nb, dBass, qBass, swingDelay(P, T_BASS) + nudgeSamples(nb.nudge),
			           [&](const NoteEv& n) { startMono(bassV, n, sr); });
		NoteEv nm = melAt(pat, P, step, L);
		if (pp.sec >= 0 && (pat.partMask & (1u << T_MELODY)))
			shapeNote(pp, T_MELODY, nm);
		qMel.on = false;
		if (nm.on && !trackMuted(T_MELODY))
			placeEvent(T_MELODY, nm, dMel, qMel, swingDelay(P, T_MELODY) + nudgeSamples(nm.nudge),
			           [&](const NoteEv& n) { startMono(melV, n, sr); });
		PadEv np = padAt(pat, P, step, L, padPrev, padHasPrev);
		qPad.on = false;
		if (np.on && !trackMuted(T_PADS))
			placeEvent(T_PADS, np, dPad, qPad, swingDelay(P, T_PADS) + nudgeSamples(np.nudge),
			           [&](const PadEv& e) { startPad(e, sr); });
		queueEarly(P, L, arrFillEnd, sr);
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

	/** Where a song part is in its course: in a song row the row, otherwise the loop (Pattern::partTag). */
	PartPos curPartPos(int L) const {
		if (arrRowEnd >= 0 && arrRowBars > 0) {
			int64_t bar = barIndex - (arrRowEnd - arrRowBars);
			int b = (int)std::max<int64_t>(0, std::min<int64_t>(arrRowBars - 1, bar));
			return partPos(pat, b * STEPS_PER_BAR + step % STEPS_PER_BAR, arrRowBars * STEPS_PER_BAR);
		}
		return partPos(pat, step, L);
	}

	/** RUN: from the first step of the loop, also while running (audio thread). */
	void restart() {
		step = -1;
		phase = 0.f;
		pulseCount = 0;
		subTicksLeft = 0;
		clearQueued();
		resetArmed = true;
		passCount = 0;
		firstPass = true;
		fillPadReq = fillPadOn = false;
		running = true;
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		step = -1;
		firstPass = true;
		running = false;
		genreTempo = false;
		clockOutMode = 0;
		chordMode = CM_MIXED;
		templateSel = -1;
		genreCvMode = 0;
		genreCvSpan = NUM_GENRES;
		mutateMode = mutateWhat = 0;
		melPhrase = true;
		presetMode = 2;
		anchorValid = false;
		for (int t = 0; t < 4; t++) {
			trackSel[t] = -1;
			trackSelChanged[t] = false;
			pendingTrack[t] = false;
		}
		clearQueued();
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
		if (runBtn.process(params[RUN_PARAM].getValue() > 0.5f))
			restart(); // the RUN pad: always from the start of the loop
		if (runTrig.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
			running = !running; // the RUN input still toggles
		if (stopBtn.process(params[STOP_PARAM].getValue() > 0.5f)) {
			running = false;
			fillPadReq = fillPadOn = false;
			stopFlash = 1.f;
		}
		if (fillPadBtn.process(params[FILLPAD_PARAM].getValue() > 0.5f) && running)
			fillPadReq = true;
		// TAP: the tempo from the last taps (internal clock only; a clock at CLOCK sets the tempo)
		tapSince += 1.f;
		if (tapBtn.process(params[TAP_PARAM].getValue() > 0.5f)) {
			tapFlash = 1.f;
			if (tapSince > args.sampleRate * 2.f)
				tapN = 0; // a pause: a new count
			else {
				for (int i = 3; i > 0; i--)
					tapIv[i] = tapIv[i - 1];
				tapIv[0] = tapSince;
				tapN = std::min(tapN + 1, 4);
				float sum = 0.f;
				for (int i = 0; i < tapN; i++)
					sum += tapIv[i];
				if (!inputs[CLOCK_INPUT].isConnected() && sum > 0.f)
					params[BPM_PARAM].setValue(clamp(std::round(60.f * args.sampleRate * tapN / sum), 40.f, 220.f));
			}
			tapSince = 0.f;
		}
		if (makeSongBtn.process(params[MAKESONG_PARAM].getValue() > 0.5f)) {
			if (arrAlive.load(std::memory_order_relaxed) > 0.f)
				makeSongReq.store(true);
			else
				noAblauf.store(true);
		}
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f)) {
			step = -1;
			phase = 0.f;
			pulseCount = 0;
			subTicksLeft = 0;
			clearQueued();
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
			if (rq == 1) // the song starts from the top
				restart();
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
			bool lk[NUM_TRACKS];
			locks(lk);
			for (int t = 0; t < 4; t++)
				if (!lk[t])
					presetRoll[t]++; // the followers of the tracks NEW made anew get a preset too (UI thread)
		}
		for (int t = 0; t < 4; t++) {
			bool trig = tnewBtn[t].process(params[TNEW_PARAM + t].getValue() > 0.5f);
			trig |= tnewTrig[t].process(inputs[TNEW_INPUT + t].getVoltage(), 0.1f, 1.f);
			if (trig) {
				newTrack(t); // an explicit per-track NEW works even when the track is locked
				presetRoll[t]++;
			}
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
		if (anchorBackReq.load(std::memory_order_relaxed) && anchorBackReq.exchange(false) && anchorValid &&
		    patChanges.load() == anchorStamp) { // back to the anchor (only while MUTATE's changes are what plays)
			pat = anchor;
			padHasPrev = false;
			patChanges++;
			anchorStamp = patChanges.load();
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
			else if (ppqnLeft > 0 && (ppqnCnt -= 1.f) <= 0.f) {
				clockPulse.trigger(2e-3f);
				ppqnLeft--;
				ppqnCnt += ppqnIv;
			}
		}
		else
			ppqnLeft = 0;
		sinceTick += 1.f;
		posPub.store(running && step >= 0 ? step + std::min(0.999f, sinceTick / std::max(1.f, stepPeriod)) : -1.f,
		             std::memory_order_relaxed);

		// ---- outputs
		for (int v = 0; v < NUM_DRUMS; v++) {
			DrumVoice& d = drums[v];
			if (d.delayCnt >= 0.f) {
				d.delayCnt -= 1.f;
				if (d.delayCnt < 0.f)
					fireDrum(v, d.pVel, d.pRat, sr, d.pStep);
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
			lights[FILLPAD_LIGHT].setBrightness(fillPadOn ? 1.f : (fillPadReq && blink < 0.5f ? 0.6f : 0.f));
			stopFlash = std::max(0.f, stopFlash - dt * 4.f);
			lights[STOP_LIGHT].setBrightness(stopFlash);
			tapFlash = std::max(0.f, tapFlash - dt * 6.f);
			// TAP blinks on the beat while running
			float beat = running && step >= 0 && step % 4 == 0 ? std::max(0.f, 1.f - sinceTick / std::max(1.f, stepPeriod * 1.5f)) : 0.f;
			lights[TAP_LIGHT].setBrightness(std::max(tapFlash, beat * 0.7f));
			lights[MAKESONG_LIGHT].setBrightness(arrAlive.load(std::memory_order_relaxed) > 0.f ? 0.85f : 0.f);
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
		json_object_set_new(root, "running", json_boolean(running)); // informative only: GENRA loads stopped
		json_object_set_new(root, "genreTempo", json_boolean(genreTempo));
		json_object_set_new(root, "clockMode", json_integer(clockMode));
		json_object_set_new(root, "clockOutMode", json_integer(clockOutMode));
		json_object_set_new(root, "chordMode", json_integer(chordMode));
		json_object_set_new(root, "templateSel", json_integer(templateSel));
		json_object_set_new(root, "genreCvMode", json_integer(genreCvMode));
		json_object_set_new(root, "genreCvSpan", json_integer(genreCvSpan));
		json_object_set_new(root, "mutateMode", json_integer(mutateMode));
		json_object_set_new(root, "mutateWhat", json_integer(mutateWhat));
		json_object_set_new(root, "melPhrase", json_boolean(melPhrase));
		json_object_set_new(root, "presetMode", json_integer(presetMode));
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
		running = false; // after loading GENRA waits for RUN (or an ABLAUF song)
		genreTempo = false;
		if ((j = json_object_get(root, "genreTempo")))
			genreTempo = json_boolean_value(j);
		if ((j = json_object_get(root, "clockMode")))
			clockMode = clamp((int)json_integer_value(j), 0, 3);
		clockOutMode = 0; // older patches: 16ths
		if ((j = json_object_get(root, "clockOutMode")))
			clockOutMode = clamp((int)json_integer_value(j), 0, 3);
		if ((j = json_object_get(root, "chordMode")))
			chordMode = clamp((int)json_integer_value(j), 0, 2);
		if ((j = json_object_get(root, "templateSel")))
			templateSel = (int)json_integer_value(j);
		if ((j = json_object_get(root, "genreCvMode")))
			genreCvMode = clamp((int)json_integer_value(j), 0, 1);
		genreCvSpan = 37; // pre-v2.24 patch: 0-10 V covered the first 37 genres
		if ((j = json_object_get(root, "genreCvSpan")))
			genreCvSpan = clamp((int)json_integer_value(j), 2, (int)NUM_GENRES);
		if (genreCvSpan != 37) // "all genres" (saved as 59 before 2.30): all of today's
			genreCvSpan = NUM_GENRES;
		mutateMode = mutateWhat = 0;
		if ((j = json_object_get(root, "mutateMode")))
			mutateMode = clamp((int)json_integer_value(j), 0, 2);
		if ((j = json_object_get(root, "mutateWhat")))
			mutateWhat = clamp((int)json_integer_value(j), 0, 3);
		melPhrase = true;
		if ((j = json_object_get(root, "melPhrase")))
			melPhrase = json_boolean_value(j);
		presetMode = 2;
		if ((j = json_object_get(root, "presetMode")))
			presetMode = clamp((int)json_integer_value(j), 0, 2);
		anchorValid = false;
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
		std::string name = std::string("genra-") + prof(curGenre).name;
		if (onlyTrack >= 0 && onlyTrack < 4)
			name += std::string("-") + TRK[onlyTrack];
		Params P = buildParams();
		int L = lengthSteps();
		float bpm = currentBpm();
		std::vector<MidiNote> notes = renderLoop(pat, P, L);
		if (onlyTrack >= 0)
			notes.erase(std::remove_if(notes.begin(), notes.end(), [=](const MidiNote& n) { return n.track != onlyTrack; }),
			            notes.end());
		std::vector<uint8_t> data = writeSmf(notes, bpm, prof(curGenre).name, L, &pat);
		// no save dialog (it can stall the audio driver): straight into the MIDI export folder
		std::string err, file = midifolder::save(name, data, &err);
		showMsg(file.empty() ? "SAVING FAILED: " + err : "SAVED: " + file);
	}
	/** A short message in the display (UI thread), e.g. where a MIDI file went. */
	std::string uiMsg;
	double uiMsgUntil = 0.0;
	void showMsg(const std::string& s) {
		uiMsg = s;
		uiMsgUntil = system::getTime() + 4.0;
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

// the numbers the other plugin (GENRA Instruments) uses for GENRA's outputs (link/GenraPorts.hpp)
static_assert((int) Genra::DRUM_OUTPUT == (int) genraports::DRUM_OUTPUT && (int) Genra::BASS_VOCT_OUTPUT == (int) genraports::BASS_VOCT_OUTPUT &&
                  (int) Genra::BASS_GATE_OUTPUT == (int) genraports::BASS_GATE_OUTPUT && (int) Genra::BASS_VEL_OUTPUT == (int) genraports::BASS_VEL_OUTPUT &&
                  (int) Genra::BASS_ACC_OUTPUT == (int) genraports::BASS_ACC_OUTPUT && (int) Genra::MEL_VOCT_OUTPUT == (int) genraports::MEL_VOCT_OUTPUT &&
                  (int) Genra::MEL_GATE_OUTPUT == (int) genraports::MEL_GATE_OUTPUT && (int) Genra::MEL_VEL_OUTPUT == (int) genraports::MEL_VEL_OUTPUT &&
                  (int) Genra::MEL_ACC_OUTPUT == (int) genraports::MEL_ACC_OUTPUT && (int) Genra::PAD_VOCT_OUTPUT == (int) genraports::PAD_VOCT_OUTPUT &&
                  (int) Genra::PAD_GATE_OUTPUT == (int) genraports::PAD_GATE_OUTPUT && (int) Genra::PAD_VEL_OUTPUT == (int) genraports::PAD_VEL_OUTPUT &&
                  (int) Genra::PAD_MOD_OUTPUT == (int) genraports::PAD_MOD_OUTPUT && (int) Genra::BASS_SLIDE_OUTPUT == (int) genraports::BASS_SLIDE_OUTPUT &&
                  (int) Genra::MEL_SLIDE_OUTPUT == (int) genraports::MEL_SLIDE_OUTPUT && (int) Genra::CLOCK_OUTPUT == (int) genraports::CLOCK_OUTPUT &&
                  (int) Genra::RESET_OUTPUT == (int) genraports::RESET_OUTPUT,
              "GENRA's outputs must keep their numbers (link/GenraPorts.hpp)");
