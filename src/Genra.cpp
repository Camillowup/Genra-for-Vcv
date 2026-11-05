// GENRA – generative 4-track sequencer, VCV Rack module (v3)
#include "plugin.hpp"
#include "Engine.hpp"
#include "Layout.hpp"
#include <osdialog.h>
#include <atomic>
#include <cmath>
#include <cctype>
#include <cstdio>

using namespace genra;

static const int LENGTHS[4] = {1, 2, 4, 8};
static const int NUM_SCENES = 8;
static const char* CLOCK_MODES[4] = {"16tel (4 PPQN)", "8tel (2 PPQN)", "Viertel (1 PPQN)", "24 PPQN"};
static const char* CHORD_MODES[3] = {"Gemischt (Vorlagen + frei)", "Nur Genre-Vorlagen", "Frei erzeugt"};
static const char* GENRE_CV_MODES[2] = {"0–10 V über alle Genres", "1/12 V pro Genre (Halbtöne, 0 V = House)"};
static const char* TRACK_NAMES[4] = {"Drums", "Bass", "Melodie", "Pads"};

static const NVGcolor TRACK_COLORS[4] = {
	nvgRGB(0xff, 0x8a, 0x3d), nvgRGB(0xff, 0x4f, 0x6d), nvgRGB(0x3d, 0xc9, 0xff), nvgRGB(0xa9, 0x8b, 0xff)};
static const NVGcolor HARMONY_COLOR = nvgRGB(0x2f, 0x9e, 0x8f);

// genre families in GENRE knob order (for the menus)
struct Family {
	const char* name;
	int first, count;
};
static const Family FAMILIES[5] = {
	{"House", 0, 5}, {"Techno", 5, 5}, {"Breaks & Bass", 10, 5}, {"Hip-Hop & Downtempo", 15, 6}, {"Dub · Synthwave · Ambient", 21, 3}};

static std::string upper(std::string s) {
	for (char& c : s)
		c = (char)std::toupper((unsigned char)c);
	return s;
}

// "JUNGLE · RAGGA" (variant only if the genre has several)
static std::string profileLabel(int p) {
	int g = genreOfProfile(p);
	std::string s = upper(prof(p).name);
	if (numVariants(g) > 1)
		s += " · " + upper(prof(p).variant);
	return s;
}

struct Genra;
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
		// v3
		ENUMS(SCENE_PARAM, NUM_SCENES),
		STORE_PARAM,
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
		SCENE_INPUT,
		NEXT_INPUT,
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
		// v3
		ENUMS(SCENE_LIGHT, NUM_SCENES),
		STORE_LIGHT,
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

	// ---- scenes
	struct Scene {
		bool used = false;
		Pattern pat;
		int8_t sel[4] = {-1, -1, -1, -1};
		int8_t lengthIdx = 2;
	};
	Scene scenes[NUM_SCENES];
	int activeScene = -1;
	int pendingScene = -1;
	int lastSceneCv = -1;

	// ---- requests from the UI thread
	std::atomic<bool> regenRequested{false};
	std::atomic<bool> chordRegenRequested{false};
	std::atomic<bool> importReady{false};
	std::atomic<bool> resetTracksRequested{false};
	std::atomic<bool> clearScenesRequested{false};

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

	// ---- clock & triggers
	dsp::SchmittTrigger clockTrig, resetTrig, runTrig, newTrig, fillTrig, nextTrig;
	dsp::BooleanTrigger newBtn, runBtn, chordNewBtn;
	dsp::BooleanTrigger tnewBtn[4], sceneBtn[NUM_SCENES];
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
	float newFlash = 0.f;
	dsp::ClockDivider lightDivider;

	Genra() {
		initGenres();
		for (int t = 0; t < 4; t++) {
			trackSel[t] = -1;
			trackSelChanged[t] = false;
		}
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> genreNames;
		for (int g = 0; g < NUM_GENRES; g++)
			genreNames.push_back(prof(profileOf(g, 0)).name);
		configSwitch(GENRE_PARAM, 0.f, NUM_GENRES - 1, 0.f, "Genre (global)", genreNames);
		configParam<VariantQuantity>(VARIANT_PARAM, 0.f, MAX_VARIANTS - 1, 0.f, "Variante");
		paramQuantities[VARIANT_PARAM]->snapEnabled = true;
		configButton(CHORDNEW_PARAM, "Harmony: neue Akkordfolge (NEW)");
		configSwitch(KEY_PARAM, 0.f, 11.f, 9.f, "Tonart (Grundton)",
		             {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B/H"});
		std::vector<std::string> scaleNames = {"Auto (Genre)"};
		for (int s = 0; s < NUM_SCALES; s++)
			scaleNames.push_back(SCALES[s].name);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES, 0.f, "Tonleiter", scaleNames);
		configParam(BPM_PARAM, 40.f, 220.f, 124.f, "Tempo (interne Clock)", " BPM");
		paramQuantities[BPM_PARAM]->snapEnabled = true;
		configParam(SWING_PARAM, -1.f, 1.f, 0.f, "Swing (relativ zum Genre jeder Spur)", "%", 0.f, 25.f);
		configParam(COMPLEX_PARAM, 0.f, 1.f, 0.5f, "Rhythmische Komplexität (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(COLOR_PARAM, 0.f, 1.f, 0.5f, "Harmonische Farbe (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutation pro Durchlauf", "%", 0.f, 100.f);
		configSwitch(LENGTH_PARAM, 0.f, 3.f, 2.f, "Loop-Länge", {"1 Takt", "2 Takte", "4 Takte", "8 Takte"});
		configButton(NEW_PARAM, "Alles neu würfeln (NEW ALL)");
		configButton(RUN_PARAM, "Start / Stop");
		configSwitch(CHORDLOCK_PARAM, 0.f, 1.f, 0.f, "Harmony: Akkordfolge sperren", {"Aus", "Gesperrt"});
		for (int t = 0; t < 4; t++) {
			std::string tn = TRACK_NAMES[t];
			configParam(DENS_PARAM + t, 0.f, 1.f, 0.5f, tn + " Dichte", "%", 0.f, 200.f);
			configSwitch(MUTE_PARAM + t, 0.f, 1.f, 0.f, tn + " Mute", {"Aus", "Stumm"});
			configSwitch(LOCK_PARAM + t, 0.f, 1.f, 0.f, tn + " Lock", {"Aus", "Gesperrt"});
			configButton(TNEW_PARAM + t, tn + " neu würfeln (NEW)");
			configInput(TNEW_INPUT + t, tn + " neu würfeln (Trigger)");
			configInput(TGENRE_INPUT + t, tn + " Genre CV (überschreibt das Genre-Feld)");
			configInput(DENS_INPUT + t, tn + " Dichte CV (0-10 V, addiert)");
		}
		configParam(FILL_PARAM, 0.f, 1.f, 0.25f, "Drum-Fill / Drop Wahrscheinlichkeit", "%", 0.f, 100.f);
		configParam(GHOST_PARAM, 0.f, 1.f, 0.3f, "Ghost Notes", "%", 0.f, 100.f);
		configParam(MOVE_PARAM, 0.f, 1.f, 0.5f, "Bass Bewegung (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(RANGE_PARAM, 0.f, 1.f, 0.5f, "Melodie Tonumfang", "%", 0.f, 100.f);
		configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "Pad Voicing (eng - offen)", "%", 0.f, 100.f);
		static const char* ON[3] = {"Bass", "Melodie", "Pads"};
		for (int i = 0; i < 3; i++)
			configSwitch(OCT_PARAM + i, -2.f, 2.f, 0.f, std::string(ON[i]) + " Oktave", {"-2", "-1", "0", "+1", "+2"});
		for (int i = 0; i < NUM_SCENES; i++)
			configButton(SCENE_PARAM + i, string::f("Szene %d (abrufen; leer oder nach STORE: speichern)", i + 1));
		configSwitch(STORE_PARAM, 0.f, 1.f, 0.f, "Szene speichern (STORE, dann Szenen-Taster)", {"Aus", "Bereit"});
		for (int id : std::initializer_list<int>{RUN_PARAM, NEW_PARAM, CHORDLOCK_PARAM, CHORDNEW_PARAM, STORE_PARAM})
			paramQuantities[id]->randomizeEnabled = false;
		for (int t = 0; t < 4; t++) {
			paramQuantities[TNEW_PARAM + t]->randomizeEnabled = false;
			paramQuantities[MUTE_PARAM + t]->randomizeEnabled = false;
			paramQuantities[LOCK_PARAM + t]->randomizeEnabled = false;
		}
		for (int i = 0; i < NUM_SCENES; i++)
			paramQuantities[SCENE_PARAM + i]->randomizeEnabled = false;

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run (Trigger schaltet um)");
		configInput(NEW_INPUT, "Alles neu würfeln (Trigger)");
		configInput(GENRE_INPUT, "Genre CV (global)");
		configInput(MUTATE_INPUT, "Mutate CV (0-10 V, addiert)");
		configInput(TRANSP_INPUT, "Transpose (V/Oct)");
		configInput(CHORD_INPUT, "Akkord (polyphon, V/Oct)");
		configInput(CHORDNEW_INPUT, "Harmony: neue Akkordfolge (Trigger)");
		configInput(FILL_INPUT, "Fill / Drop am Loop-Ende auslösen (Trigger)");
		configInput(VARIANT_INPUT, "Variante CV (0-10 V, addiert)");
		configInput(COMPLEX_INPUT, "Complex CV (0-10 V, addiert)");
		configInput(COLOR_INPUT, "Color CV (0-10 V, addiert)");
		configInput(SCENE_INPUT, "Szene CV (0-10 V = Szene 1-8)");
		configInput(NEXT_INPUT, "Nächste Szene (Trigger)");
		for (int v = 0; v < NUM_DRUMS; v++)
			configOutput(DRUM_OUTPUT + v, std::string(DRUM_NAMES[v]) + " Trigger (Höhe = Velocity)");
		configOutput(BASS_VOCT_OUTPUT, "Bass V/Oct");
		configOutput(BASS_GATE_OUTPUT, "Bass Gate");
		configOutput(BASS_VEL_OUTPUT, "Bass Velocity");
		configOutput(BASS_ACC_OUTPUT, "Bass Akzent");
		configOutput(MEL_VOCT_OUTPUT, "Melodie V/Oct");
		configOutput(MEL_GATE_OUTPUT, "Melodie Gate");
		configOutput(MEL_VEL_OUTPUT, "Melodie Velocity");
		configOutput(MEL_ACC_OUTPUT, "Melodie Akzent");
		configOutput(PAD_VOCT_OUTPUT, "Pads V/Oct (polyphon, 4)");
		configOutput(PAD_GATE_OUTPUT, "Pads Gate (polyphon, 4)");
		configOutput(PAD_VEL_OUTPUT, "Pads Velocity");
		configOutput(PAD_MOD_OUTPUT, "Pads Modulation (0-10 V)");
		configOutput(BASS_SLIDE_OUTPUT, "Bass Slide (10 V während des Glides)");
		configOutput(MEL_SLIDE_OUTPUT, "Melodie Slide (10 V während des Glides)");

		lightDivider.setDivision(64);
		rng = Rng(random::u64());
		initPattern(pat);
		curGenre = 0;
		doGenerate(true);
	}

	// ------------------------------------------------------------ helpers
	int lengthSteps() {
		int idx = clamp((int)std::round(params[LENGTH_PARAM].getValue()), 0, 3);
		return LENGTHS[idx] * STEPS_PER_BAR;
	}

	static float cv01(Input& in) { return in.isConnected() ? in.getVoltage() / 10.f : 0.f; }

	Params buildParams() {
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
		P.swingK = params[SWING_PARAM].getValue();
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
		return P;
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
	}

	void newTrack(int t) {
		Params P = buildParams();
		generateTrack(pat, t, trackTarget(t), rng, P);
		pendingTrack[t] = false;
		if (t == T_PADS)
			padHasPrev = false;
		tnewFlash[t] = 1.f;
	}

	void readGenre() {
		curGenre = knobProfile();
		if (lastGenre >= 0 && curGenre != lastGenre && !pendingGenre)
			pendingGenre = true;
		for (int t = 0; t < 4; t++) {
			bool own = ownGenre(t);
			if (trackSelChanged[t].exchange(false))
				pendingTrack[t] = true; // new choice in the genre field (also back to Global)
			if (ownPrev[t] && !own)
				pendingTrack[t] = true; // CV unplugged -> back to the global genre
			if (own && trackTarget(t) != pat.trackGenre[t])
				pendingTrack[t] = true;
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

	// ------------------------------------------------------------ scenes
	void storeScene(int i) {
		Scene& s = scenes[i];
		s.used = true;
		s.pat = pat;
		for (int t = 0; t < 4; t++)
			s.sel[t] = (int8_t)trackSel[t];
		s.lengthIdx = (int8_t)clamp((int)std::round(params[LENGTH_PARAM].getValue()), 0, 3);
		activeScene = i;
		pendingScene = -1;
	}

	void recallScene(int i) {
		const Scene& s = scenes[i];
		if (!s.used)
			return;
		pat = s.pat;
		for (int t = 0; t < 4; t++) {
			trackSel[t] = s.sel[t];
			trackSelChanged[t] = false;
			pendingTrack[t] = false;
			ownPrev[t] = ownGenre(t);
		}
		params[LENGTH_PARAM].setValue((float)s.lengthIdx);
		pendingGenre = false;
		lastGenre = curGenre; // the scene's pattern stands, even if the GENRE knob differs
		padHasPrev = false;
		activeScene = i;
	}

	void requestScene(int i) {
		if (i < 0 || i >= NUM_SCENES || !scenes[i].used)
			return;
		if (!running || step < 0)
			recallScene(i); // stopped: switch right away
		else
			pendingScene = i; // playing: at the next bar line
	}

	void processScenes() {
		bool armed = params[STORE_PARAM].getValue() > 0.5f;
		for (int i = 0; i < NUM_SCENES; i++) {
			if (!sceneBtn[i].process(params[SCENE_PARAM + i].getValue() > 0.5f))
				continue;
			if (armed || !scenes[i].used) {
				storeScene(i);
				params[STORE_PARAM].setValue(0.f);
				armed = false;
			}
			else
				requestScene(i);
		}
		if (nextTrig.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 1.f)) {
			int from = pendingScene >= 0 ? pendingScene : activeScene;
			for (int k = 1; k <= NUM_SCENES; k++) {
				int i = ((from < 0 ? -1 : from) + k) % NUM_SCENES;
				if (scenes[i].used) {
					requestScene(i);
					break;
				}
			}
		}
		if (inputs[SCENE_INPUT].isConnected()) {
			int s = clamp((int)std::round(inputs[SCENE_INPUT].getVoltage() / 10.f * (NUM_SCENES - 1)), 0, NUM_SCENES - 1);
			if (s != lastSceneCv) {
				lastSceneCv = s;
				requestScene(s);
			}
		}
		else
			lastSceneCv = -1;
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
		Params P = buildParams();
		if (step % STEPS_PER_BAR == 0) {
			bool changed = false;
			if (pendingScene >= 0) {
				recallScene(pendingScene);
				pendingScene = -1;
				L = lengthSteps();
				layoutTimeline(pat, L);
				if (step >= L)
					step = 0;
				changed = true;
			}
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
		if (step == 0) {
			fillActive = fillCarry || rng.uni() < P.fill;
			fillCarry = false;
			P.fillActive = fillActive;
			if (!firstPass) {
				float m = params[MUTATE_PARAM].getValue() + cv01(inputs[MUTATE_INPUT]);
				bool lk[NUM_TRACKS];
				locks(lk);
				mutate(pat, clamp(m, 0.f, 1.f), rng, lk, chordLocked(), P);
			}
			firstPass = false;
		}
		// compute this step for every track, play it after the track's swing delay
		drumStep(pat, P, step, L, fillActive, qDrums);
		if (params[MUTE_PARAM + 0].getValue() < 0.5f) {
			float d = swingDelay(P, T_DRUMS);
			if (d >= 1.f)
				dDrums = d;
			else
				fireDrums(sr);
		}
		qBass = bassAt(pat, P, step, L);
		if (qBass.on && params[MUTE_PARAM + 1].getValue() < 0.5f) {
			float d = swingDelay(P, T_BASS);
			if (d >= 1.f)
				dBass = d;
			else
				startMono(bassV, qBass, sr);
		}
		qMel = melAt(pat, P, step, L);
		if (qMel.on && params[MUTE_PARAM + 2].getValue() < 0.5f) {
			float d = swingDelay(P, T_MELODY);
			if (d >= 1.f)
				dMel = d;
			else
				startMono(melV, qMel, sr);
		}
		qPad = padAt(pat, P, step, L, padPrev, padHasPrev);
		if (qPad.on && params[MUTE_PARAM + 3].getValue() < 0.5f) {
			float d = swingDelay(P, T_PADS);
			if (d >= 1.f)
				dPad = d;
			else
				startPad(qPad, sr);
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
		for (int i = 0; i < NUM_SCENES; i++)
			scenes[i].used = false;
		activeScene = pendingScene = -1;
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
		}
		if (fillTrig.process(inputs[FILL_INPUT].getVoltage(), 0.1f, 1.f)) {
			fillActive = true; // this pass ends with a fill / drop
			if (step >= lengthSteps() - 4)
				fillCarry = true; // too late for this pass: the next one
		}

		applyChordEdits();
		if (importReady) {
			pat = importBuf;
			params[LENGTH_PARAM].setValue((float)importLengthIndex);
			importReady = false;
			padHasPrev = false;
		}
		if (resetTracksRequested.exchange(false))
			for (int t = 0; t < 4; t++)
				setTrackSel(t, -1);
		if (clearScenesRequested.exchange(false)) {
			for (int i = 0; i < NUM_SCENES; i++)
				scenes[i].used = false;
			activeScene = pendingScene = -1;
		}
		processScenes();
		readGenre();
		readChordInput();
		if (regenRequested.exchange(false)) {
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
			}
		}
		if (chordRegenRequested.exchange(false) && !chordLocked()) {
			Params P = buildParams();
			generateChords(pat, curGenre, rng, P);
			layoutTimeline(pat, lengthSteps());
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
				lights[MUTE_LIGHT + t].setBrightness(params[MUTE_PARAM + t].getValue());
				lights[LOCK_LIGHT + t].setBrightness(params[LOCK_PARAM + t].getValue());
			}
			for (int i = 0; i < NUM_SCENES; i++) {
				float b = scenes[i].used ? 0.18f : 0.f;
				if (i == activeScene)
					b = 1.f;
				if (i == pendingScene)
					b = blink < 0.5f ? 1.f : 0.25f;
				lights[SCENE_LIGHT + i].setBrightness(b);
			}
			lights[STORE_LIGHT].setBrightness(params[STORE_PARAM].getValue() > 0.5f ? (blink < 0.5f ? 1.f : 0.4f) : 0.f);
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
		json_t* sc = json_array();
		for (int i = 0; i < NUM_SCENES; i++) {
			if (!scenes[i].used)
				continue;
			json_t* o = json_object();
			json_object_set_new(o, "slot", json_integer(i));
			std::vector<uint8_t> b = savePattern(scenes[i].pat);
			json_object_set_new(o, "pattern3", json_string(string::toBase64(b.data(), b.size()).c_str()));
			json_t* s = json_array();
			for (int t = 0; t < 4; t++)
				json_array_append_new(s, json_integer(scenes[i].sel[t]));
			json_object_set_new(o, "trackGenre", s);
			json_object_set_new(o, "length", json_integer(scenes[i].lengthIdx));
			json_array_append_new(sc, o);
		}
		json_object_set_new(root, "scenes", sc);
		json_object_set_new(root, "activeScene", json_integer(activeScene));
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
		for (int i = 0; i < NUM_SCENES; i++)
			scenes[i].used = false;
		json_t* sc = json_object_get(root, "scenes");
		for (size_t k = 0; sc && k < json_array_size(sc); k++) {
			json_t* o = json_array_get(sc, k);
			json_t* js = json_object_get(o, "slot");
			json_t* jpat = json_object_get(o, "pattern3");
			if (!js || !jpat || !json_is_string(jpat))
				continue;
			int i = (int)json_integer_value(js);
			if (i < 0 || i >= NUM_SCENES)
				continue;
			Scene& s = scenes[i];
			if (!loadPattern(string::fromBase64(json_string_value(jpat)), s.pat))
				continue;
			readSel(json_object_get(o, "trackGenre"), s.sel);
			json_t* jl = json_object_get(o, "length");
			s.lengthIdx = (int8_t)clamp(jl ? (int)json_integer_value(jl) : 2, 0, 3);
			s.used = true;
		}
		activeScene = -1;
		if ((j = json_object_get(root, "activeScene"))) {
			int a = (int)json_integer_value(j);
			activeScene = (a >= 0 && a < NUM_SCENES && scenes[a].used) ? a : -1;
		}
		pendingScene = -1;
	}

	// ------------------------------------------------------------ MIDI file actions (UI thread)
	void exportMidi() {
		osdialog_filters* filters = osdialog_filters_parse("MIDI-Datei:mid,midi");
		std::string name = std::string("genra-") + prof(curGenre).name + ".mid";
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
		float bpm = inputs[CLOCK_INPUT].isConnected() && stepPeriod > 0.f
		                ? 60.f * APP->engine->getSampleRate() / (stepPeriod * 4.f)
		                : params[BPM_PARAM].getValue();
		std::vector<uint8_t> data = writeSmf(renderLoop(pat, P, L), bpm, prof(curGenre).name, L, &pat);
		try {
			system::writeFile(path, data);
		}
		catch (Exception& e) {
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, ("Speichern fehlgeschlagen: " + std::string(e.what())).c_str());
		}
	}

	void importMidi() {
		osdialog_filters* filters = osdialog_filters_parse("MIDI-Datei:mid,midi");
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
			osdialog_message(OSDIALOG_ERROR, OSDIALOG_OK, "Datei konnte nicht gelesen werden.");
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

std::string VariantQuantity::getDisplayValueString() {
	Genra* m = dynamic_cast<Genra*>(module);
	if (!m)
		return ParamQuantity::getDisplayValueString();
	int g = m->knobGenre();
	int v = (int)std::round(getValue());
	int n = numVariants(g);
	std::string name = prof(profileOf(g, v)).variant;
	if (v >= n)
		name += " (nur " + std::to_string(n) + ")";
	return name;
}

// ================================================================= lights
template <int R, int G, int B>
struct ColorLight : GrayModuleLightWidget {
	ColorLight() { addBaseColor(nvgRGB(R, G, B)); }
};
using OrangeLight = ColorLight<0xff, 0x8a, 0x3d>;
using PinkLight = ColorLight<0xff, 0x4f, 0x6d>;
using CyanLight = ColorLight<0x3d, 0xc9, 0xff>;
using PurpleLight = ColorLight<0xa9, 0x8b, 0xff>;
using MintLight = ColorLight<0x5d, 0xff, 0x8a>;
using TealLight = ColorLight<0x4f, 0xd1, 0xbd>;

// ================================================================= shared drawing
static void drawText(NVGcontext* vg, std::shared_ptr<window::Font> f, float xmm, float ymm, float sizeMm, NVGcolor c, const std::string& s,
                     int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, float spacing = 0.f) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	nvgFillColor(vg, c);
	nvgTextAlign(vg, align);
	nvgText(vg, mm2px(xmm), mm2px(ymm), s.c_str(), NULL);
}

static float textWidthMm(NVGcontext* vg, std::shared_ptr<window::Font> f, float sizeMm, float spacing, const std::string& s) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	float b[4];
	nvgTextBounds(vg, 0, 0, s.c_str(), NULL, b);
	return (b[2] - b[0]) / mm2px(1.f);
}

// ================================================================= genre field (one per track)
struct GenreField : Widget {
	Genra* module = nullptr;
	int track = 0;
	bool hover = false;

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> bold = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		if (!bold)
			return;
		NVGcontext* vg = args.vg;
		float w = box.size.x / mm2px(1.f), h = box.size.y / mm2px(1.f);
		NVGcolor col = TRACK_COLORS[track];
		NVGcolor dim = nvgRGB(0x8a, 0x8d, 0x99);
		std::string label = "GLOBAL";
		bool own = false, pending = false;
		if (module) {
			Genra* m = module;
			pending = m->pendingTrack[track];
			if (m->trackHasCv(track)) {
				label = "CV  " + profileLabel(m->trackTarget(track));
				own = true;
			}
			else if (m->trackSel[track] >= 0) {
				label = profileLabel(m->trackSel[track]);
				own = true;
			}
		}
		if (pending)
			label += " …";
		if (hover) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, mm2px(0.9f));
			nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
			nvgFill(vg);
		}
		// shrink long names to fit
		float avail = w - 5.f, size = 2.1f;
		while (size > 1.4f && textWidthMm(vg, bold, size, 0.15f, label) > avail)
			size -= 0.1f;
		NVGcolor tc = own ? col : dim;
		if (pending)
			tc.a = 0.7f;
		drawText(vg, bold, 1.4f, h / 2.f, size, tc, label, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, 0.15f);
		// arrow
		float ax = w - 2.4f, ay = h / 2.f;
		nvgBeginPath(vg);
		nvgMoveTo(vg, mm2px(ax - 1.f), mm2px(ay - 0.5f));
		nvgLineTo(vg, mm2px(ax + 1.f), mm2px(ay - 0.5f));
		nvgLineTo(vg, mm2px(ax), mm2px(ay + 0.7f));
		nvgClosePath(vg);
		nvgFillColor(vg, own ? col : dim);
		nvgFill(vg);
		Widget::drawLayer(args, layer);
	}

	void onHover(const HoverEvent& e) override { e.consume(this); } // needed to receive Enter / Leave
	void onEnter(const EnterEvent& e) override { hover = true; }
	void onLeave(const LeaveEvent& e) override { hover = false; }

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || (e.button != GLFW_MOUSE_BUTTON_LEFT && e.button != GLFW_MOUSE_BUTTON_RIGHT))
			return;
		openMenu();
		e.consume(this);
	}

	void openMenu() {
		Genra* m = module;
		int t = track;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(std::string("Genre der Spur ") + TRACK_NAMES[t]));
		if (m->trackHasCv(t))
			menu->addChild(createMenuLabel("CV-Eingang verbunden: die Spannung wählt das Genre"));
		menu->addChild(createCheckMenuItem("Global (folgt GENRE)", "", [=]() { return m->trackSel[t] < 0; }, [=]() { m->setTrackSel(t, -1); }));
		menu->addChild(new ui::MenuSeparator);
		for (int f = 0; f < 5; f++) {
			const Family& fam = FAMILIES[f];
			int sel = m->trackSel[t];
			int sg = sel >= 0 ? genreOfProfile(sel) : -1;
			bool inFam = sg >= fam.first && sg < fam.first + fam.count;
			menu->addChild(createSubmenuItem(fam.name, inFam ? "✔" : "", [=](ui::Menu* sub) {
				for (int g = fam.first; g < fam.first + fam.count; g++) {
					int nv = numVariants(g);
					std::string gname = prof(profileOf(g, 0)).name;
					if (nv == 1) {
						int p = profileOf(g, 0);
						sub->addChild(createCheckMenuItem(gname, "", [=]() { return m->trackSel[t] == p; }, [=]() { m->setTrackSel(t, p); }));
						continue;
					}
					int cur = m->trackSel[t];
					bool here = cur >= 0 && genreOfProfile(cur) == g;
					sub->addChild(createSubmenuItem(gname, here ? "✔" : "", [=](ui::Menu* sub2) {
						for (int v = 0; v < nv; v++) {
							int p = profileOf(g, v);
							sub2->addChild(createCheckMenuItem(prof(p).variant, "", [=]() { return m->trackSel[t] == p; }, [=]() { m->setTrackSel(t, p); }));
						}
					}));
				}
			}));
		}
	}
};

// ================================================================= display
// geometry (mm, relative to the display's top-left)
static const float CHORD_X = 78.f;
static const float CHORD_W = 136.f;
static const float CHORD_Y = 2.6f;
static const float CHORD_H = 6.f;

struct GenraDisplay : Widget {
	Genra* module = nullptr;
	int hoverChord = -1;

	int nChordsShown() { return module ? std::max<int>(1, module->pat.nProg) : 4; }
	float chordBoxW() { return std::min(18.f, CHORD_W / nChordsShown()); }

	int chordAtPos(Vec pos) {
		Vec mm = pos.div(mm2px(Vec(1.f, 1.f)));
		if (mm.y < CHORD_Y || mm.y > CHORD_Y + CHORD_H)
			return -1;
		float x = mm.x - CHORD_X;
		if (x < 0.f)
			return -1;
		int i = (int)(x / chordBoxW());
		return i < nChordsShown() ? i : -1;
	}

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		std::shared_ptr<window::Font> bold = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		std::shared_ptr<window::Font> mono = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!bold || !mono)
			return;
		NVGcontext* vg = args.vg;
		NVGcolor white = nvgRGB(0xf2, 0xf3, 0xf7), dim = nvgRGB(0x8a, 0x8d, 0x99), dark = nvgRGB(0x1a, 0x1b, 0x21);
		float wmm = box.size.x / mm2px(1.f);

		std::string genre = "JUNGLE", variant = "RAGGA", info = "F MINOR  170 BPM  SWING 54%", bar = "BAR 1/4", status;
		std::vector<std::string> chords = {"Fm", "Db", "Fm", "Gb"};
		std::string laneName[4] = {"JUNGLE", "DUB", "LO-FI", "AMBIENT"};
		bool lanePending[4] = {false, false, false, false};
		int curChord = 0;
		bool lanes[4][16] = {};
		int curStep = -1;

		if (module) {
			Genra* m = module;
			Params P = m->buildParams();
			int L = m->lengthSteps();
			int g = m->curGenre;
			genre = upper(prof(g).name);
			variant = upper(prof(g).variant);
			if (m->pendingGenre)
				variant += " ...";
			float swing = effSwing(P, m->pat.trackGenre[T_DRUMS]);
			char buf[128];
			bool ext = m->inputs[Genra::CLOCK_INPUT].isConnected();
			float bpm = ext ? (m->stepPeriod > 0.f ? 60.f * APP->engine->getSampleRate() / (m->stepPeriod * 4.f) : 0.f)
			                : m->params[Genra::BPM_PARAM].getValue();
			std::string scaleName = upper(SCALES[P.scale].name);
			snprintf(buf, sizeof(buf), "%s %s  %s%.0f BPM  SWING %.0f%%", noteName(P.key + P.transp, P.key + P.transp, P.scale), scaleName.c_str(),
			         ext ? "EXT " : "", bpm, swing * 100.f);
			info = buf;
			int s = std::max(0, m->step);
			snprintf(buf, sizeof(buf), "BAR %d/%d", s / 16 + 1, L / 16);
			bar = buf;
			if (m->activeScene >= 0 || m->pendingScene >= 0) {
				if (m->pendingScene >= 0 && m->activeScene >= 0)
					snprintf(buf, sizeof(buf), "   SCENE %d > %d", m->activeScene + 1, m->pendingScene + 1);
				else if (m->pendingScene >= 0)
					snprintf(buf, sizeof(buf), "   SCENE > %d", m->pendingScene + 1);
				else
					snprintf(buf, sizeof(buf), "   SCENE %d", m->activeScene + 1);
				bar += buf;
			}
			std::vector<std::string> st;
			if (!m->running)
				st.push_back("STOP");
			if (m->chordLocked())
				st.push_back("CHORDS LOCKED");
			if (P.transp != 0) {
				snprintf(buf, sizeof(buf), "TRANSP %+d", P.transp);
				st.push_back(buf);
			}
			if (P.extChord)
				st.push_back("EXT CHORD");
			for (int t = 0; t < 4; t++)
				if (m->pat.fixed[t]) {
					st.push_back("MIDI");
					break;
				}
			for (size_t i = 0; i < st.size(); i++)
				status += (i ? "  " : "") + st[i];

			chords.clear();
			float color = effColor(P, m->pat.chordGenre);
			for (int i = 0; i < m->pat.nProg; i++)
				chords.push_back(chordName(resolveSpec(m->pat.prog[i], P, color), P.key + P.transp, P.scale));
			if (P.extChord) {
				Chord c = chordAt(m->pat, P, s);
				chords.assign(1, chordName(c, P.key + P.transp, P.scale));
				curChord = 0;
			}
			else
				curChord = m->pat.ev[eventAt(m->pat, s)].spec;

			for (int t = 0; t < 4; t++) {
				laneName[t] = upper(prof(m->pat.trackGenre[t]).name);
				lanePending[t] = m->pendingTrack[t] || (m->pendingGenre && !m->ownGenre(t) && m->params[Genra::LOCK_PARAM + t].getValue() < 0.5f);
			}

			int barStart = (s / 16) * 16;
			curStep = m->step >= 0 ? s % 16 : -1;
			int prev[4];
			std::memcpy(prev, m->padPrev, sizeof(prev));
			bool hp = false;
			for (int i = 0; i < 16; i++) {
				int st2 = barStart + i;
				if (st2 >= L)
					break;
				Hit hits[NUM_DRUMS];
				drumStep(m->pat, P, st2, L, false, hits);
				for (int v = 0; v < NUM_DRUMS; v++)
					lanes[0][i] |= hits[v].on;
				lanes[1][i] = bassAt(m->pat, P, st2, L).on;
				lanes[2][i] = melAt(m->pat, P, st2, L).on;
				lanes[3][i] = padAt(m->pat, P, st2, L, prev, hp).on;
			}
		}

		// left: genre + info
		drawText(vg, bold, 4.f, 7.2f, 5.2f, white, genre, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		float wmmG = textWidthMm(vg, bold, 5.2f, 0.3f, genre);
		drawText(vg, bold, 4.f + wmmG + 2.f, 7.2f, 2.6f, dim, std::string("· ") + variant, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		drawText(vg, mono, 4.f, 12.6f, 2.7f, dim, info);

		// chords
		int n = (int)chords.size();
		float bw = std::min(18.f, CHORD_W / std::max(1, n));
		for (int i = 0; i < n; i++) {
			float x = CHORD_X + i * bw;
			bool active = i == curChord;
			bool hover = i == hoverChord;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(x), mm2px(CHORD_Y), mm2px(bw - 1.2f), mm2px(CHORD_H), mm2px(0.8f));
			nvgFillColor(vg, active ? HARMONY_COLOR : (hover ? nvgRGB(0x2a, 0x2b, 0x33) : dark));
			nvgFill(vg);
			float fs = n > 5 ? 2.3f : 2.7f;
			drawText(vg, bold, x + (bw - 1.2f) / 2.f, CHORD_Y + CHORD_H / 2.f, fs, active ? nvgRGB(0x0a, 0x0a, 0x0d) : dim, chords[i],
			         NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		}
		drawText(vg, mono, CHORD_X, 13.f, 2.4f, dim, bar);
		drawText(vg, mono, CHORD_X + CHORD_W - 1.2f, 13.f, 2.4f, HARMONY_COLOR, status, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);

		// lanes with the genre each track plays
		float lx = wmm - 4.f - 16 * 2.3f;
		for (int t = 0; t < 4; t++) {
			float y = 2.8f + t * 3.3f;
			NVGcolor lc = TRACK_COLORS[t];
			if (lanePending[t])
				lc.a = 0.55f;
			drawText(vg, mono, lx - 1.6f, y + 2.0f, 2.1f, lc, laneName[t] + (lanePending[t] ? " …" : ""), NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
			for (int i = 0; i < 16; i++) {
				float x = lx + i * 2.3f;
				bool on = module ? lanes[t][i] : ((i * 7 + t * 5) % 11 < (5 - t));
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(1.8f), mm2px(2.4f), mm2px(0.3f));
				nvgFillColor(vg, on ? TRACK_COLORS[t] : nvgRGB(0x22, 0x23, 0x2a));
				nvgFill(vg);
				if (i == curStep) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, mm2px(x - 0.25f), mm2px(y - 0.25f), mm2px(2.3f), mm2px(2.9f), mm2px(0.4f));
					nvgStrokeColor(vg, white);
					nvgStrokeWidth(vg, mm2px(0.25f));
					nvgStroke(vg);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}

	void onHover(const HoverEvent& e) override {
		hoverChord = chordAtPos(e.pos);
		Widget::onHover(e);
	}
	void onLeave(const LeaveEvent& e) override {
		hoverChord = -1;
		Widget::onLeave(e);
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || module->extChord)
			return;
		int i = chordAtPos(e.pos);
		if (i < 0 || i >= module->pat.nProg)
			return;
		Genra* m = module;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			m->pushChordEdit(Genra::CE_NEXT, i);
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openChordMenu(i);
			e.consume(this);
		}
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module || module->extChord)
			return;
		int i = chordAtPos(e.pos);
		if (i < 0 || i >= module->pat.nProg)
			return;
		module->pushChordEdit(Genra::CE_COLOR, i, e.scrollDelta.y > 0 ? 1 : -1);
		e.consume(this);
	}

	void openChordMenu(int i) {
		Genra* m = module;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(string::f("Akkord %d ändern", i + 1)));
		Params P = m->buildParams();
		float color = effColor(P, m->pat.chordGenre);
		int cur = currentAlternative(m->pat.prog[i], P);
		for (int a = 0; a < numAlternatives(); a++) {
			std::string name = alternativeName(a, P, color);
			std::string roman = a < 7 ? string::f("Stufe %d", a + 1) : "geborgt";
			menu->addChild(createCheckMenuItem(name, roman, [=]() { return a == cur; }, [=]() { m->pushChordEdit(Genra::CE_SET, i, a); }));
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Mehr Farbe (Mausrad hoch)", "", [=]() { m->pushChordEdit(Genra::CE_COLOR, i, 1); }));
		menu->addChild(createMenuItem("Weniger Farbe (Mausrad runter)", "", [=]() { m->pushChordEdit(Genra::CE_COLOR, i, -1); }));
		menu->addChild(createMenuItem("Farbe zurücksetzen", "", [=]() { m->pushChordEdit(Genra::CE_COLOR_RESET, i); }));
	}
};

// ================================================================= widget
struct GenraWidget : ModuleWidget {
	GenraWidget(Genra* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Genra.svg")));

		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		auto knob = [&](const float* p, int id) { addParam(createParamCentered<RoundSmallBlackKnob>(P(p), module, id)); };
		auto in = [&](const float* p, int id) { addInput(createInputCentered<PJ301MPort>(P(p), module, id)); };
		auto out = [&](const float* p, int id) { addOutput(createOutputCentered<PJ301MPort>(P(p), module, id)); };

		GenraDisplay* disp = createWidget<GenraDisplay>(mm2px(Vec(layout::DISPLAY[0], layout::DISPLAY[1])));
		disp->box.size = mm2px(Vec(layout::DISPLAY[2], layout::DISPLAY[3]));
		disp->module = module;
		addChild(disp);

		// global row
		addParam(createParamCentered<RoundLargeBlackKnob>(P(layout::GENRE), module, Genra::GENRE_PARAM));
		knob(layout::BPM, Genra::BPM_PARAM);
		knob(layout::KEY, Genra::KEY_PARAM);
		knob(layout::SCALE, Genra::SCALE_PARAM);
		knob(layout::VARIANT, Genra::VARIANT_PARAM);
		knob(layout::COMPLEX, Genra::COMPLEX_PARAM);
		knob(layout::COLOR, Genra::COLOR_PARAM);
		knob(layout::MUTATE, Genra::MUTATE_PARAM);
		knob(layout::SWING, Genra::SWING_PARAM);
		knob(layout::LENGTH, Genra::LENGTH_PARAM);
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(layout::NEW), module, Genra::NEW_PARAM, Genra::NEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<MintLight>>(P(layout::RUN), module, Genra::RUN_PARAM, Genra::RUN_LIGHT));
		// harmony
		addParam(createLightParamCentered<VCVLightBezel<TealLight>>(P(layout::CHORDNEW), module, Genra::CHORDNEW_PARAM, Genra::CHORDNEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<TealLight>>(P(layout::CHORDLOCK), module, Genra::CHORDLOCK_PARAM, Genra::CHORDLOCK_LIGHT));
		in(layout::IN_CHORDNEW, Genra::CHORDNEW_INPUT);

		// inputs
		in(layout::IN_CLOCK, Genra::CLOCK_INPUT);
		in(layout::IN_RESET, Genra::RESET_INPUT);
		in(layout::IN_RUN, Genra::RUN_INPUT);
		in(layout::IN_NEW, Genra::NEW_INPUT);
		in(layout::IN_FILL, Genra::FILL_INPUT);
		in(layout::IN_MUTATE, Genra::MUTATE_INPUT);
		in(layout::IN_GENRE, Genra::GENRE_INPUT);
		in(layout::IN_VARIANT, Genra::VARIANT_INPUT);
		in(layout::IN_TRANSP, Genra::TRANSP_INPUT);
		in(layout::IN_COMPLEX, Genra::COMPLEX_INPUT);
		in(layout::IN_COLOR, Genra::COLOR_INPUT);
		in(layout::IN_CHORD, Genra::CHORD_INPUT);

		// scenes
		const float* scene[NUM_SCENES] = {layout::SCENE_0, layout::SCENE_1, layout::SCENE_2, layout::SCENE_3,
		                                  layout::SCENE_4, layout::SCENE_5, layout::SCENE_6, layout::SCENE_7};
		for (int i = 0; i < NUM_SCENES; i++)
			addParam(createLightParamCentered<VCVLightBezel<MintLight>>(P(scene[i]), module, Genra::SCENE_PARAM + i, Genra::SCENE_LIGHT + i));
		addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(P(layout::STORE), module, Genra::STORE_PARAM, Genra::STORE_LIGHT));
		in(layout::IN_SCENE, Genra::SCENE_INPUT);
		in(layout::IN_NEXT, Genra::NEXT_INPUT);

		// tracks
		const float* dens[4] = {layout::DENS_0, layout::DENS_1, layout::DENS_2, layout::DENS_3};
		const float* densIn[4] = {layout::IN_DENS_0, layout::IN_DENS_1, layout::IN_DENS_2, layout::IN_DENS_3};
		const float* field[4] = {layout::FIELD_0, layout::FIELD_1, layout::FIELD_2, layout::FIELD_3};
		const float* genreIn[4] = {layout::IN_GENRE_0, layout::IN_GENRE_1, layout::IN_GENRE_2, layout::IN_GENRE_3};
		const float* mute[4] = {layout::MUTE_0, layout::MUTE_1, layout::MUTE_2, layout::MUTE_3};
		const float* lock[4] = {layout::LOCK_0, layout::LOCK_1, layout::LOCK_2, layout::LOCK_3};
		const float* tnew[4] = {layout::NEW_0, layout::NEW_1, layout::NEW_2, layout::NEW_3};
		const float* tin[4] = {layout::IN_NEW_0, layout::IN_NEW_1, layout::IN_NEW_2, layout::IN_NEW_3};
		for (int t = 0; t < 4; t++) {
			addParam(createParamCentered<RoundBlackKnob>(P(dens[t]), module, Genra::DENS_PARAM + t));
			in(densIn[t], Genra::DENS_INPUT + t);
			GenreField* gf = createWidget<GenreField>(mm2px(Vec(field[t][0], field[t][1])));
			gf->box.size = mm2px(Vec(field[t][2], field[t][3]));
			gf->module = module;
			gf->track = t;
			addChild(gf);
			in(genreIn[t], Genra::TGENRE_INPUT + t);
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(tnew[t]), module, Genra::TNEW_PARAM + t, Genra::TNEW_LIGHT + t));
			in(tin[t], Genra::TNEW_INPUT + t);
		}
		knob(layout::FILL, Genra::FILL_PARAM);
		knob(layout::GHOST, Genra::GHOST_PARAM);
		knob(layout::MOVE, Genra::MOVE_PARAM);
		knob(layout::RANGE, Genra::RANGE_PARAM);
		knob(layout::VOICING, Genra::VOICING_PARAM);
		const float* oct[3] = {layout::OCT_0, layout::OCT_1, layout::OCT_2};
		for (int i = 0; i < 3; i++)
			addParam(createParamCentered<Trimpot>(P(oct[i]), module, Genra::OCT_PARAM + i));

		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(P(mute[0]), module, Genra::MUTE_PARAM + 0, Genra::MUTE_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezelLatch<PinkLight>>(P(mute[1]), module, Genra::MUTE_PARAM + 1, Genra::MUTE_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezelLatch<CyanLight>>(P(mute[2]), module, Genra::MUTE_PARAM + 2, Genra::MUTE_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(P(mute[3]), module, Genra::MUTE_PARAM + 3, Genra::MUTE_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezelLatch<OrangeLight>>(P(lock[0]), module, Genra::LOCK_PARAM + 0, Genra::LOCK_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezelLatch<PinkLight>>(P(lock[1]), module, Genra::LOCK_PARAM + 1, Genra::LOCK_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezelLatch<CyanLight>>(P(lock[2]), module, Genra::LOCK_PARAM + 2, Genra::LOCK_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(P(lock[3]), module, Genra::LOCK_PARAM + 3, Genra::LOCK_LIGHT + 3));

		// outputs
		const float* drumOut[8] = {layout::DRUM_0, layout::DRUM_1, layout::DRUM_2, layout::DRUM_3,
		                           layout::DRUM_4, layout::DRUM_5, layout::DRUM_6, layout::DRUM_7};
		for (int v = 0; v < 8; v++)
			out(drumOut[v], Genra::DRUM_OUTPUT + v);
		out(layout::BASS_VOCT, Genra::BASS_VOCT_OUTPUT);
		out(layout::BASS_GATE, Genra::BASS_GATE_OUTPUT);
		out(layout::BASS_VEL, Genra::BASS_VEL_OUTPUT);
		out(layout::BASS_ACC, Genra::BASS_ACC_OUTPUT);
		out(layout::BASS_SLIDE, Genra::BASS_SLIDE_OUTPUT);
		out(layout::MEL_VOCT, Genra::MEL_VOCT_OUTPUT);
		out(layout::MEL_GATE, Genra::MEL_GATE_OUTPUT);
		out(layout::MEL_VEL, Genra::MEL_VEL_OUTPUT);
		out(layout::MEL_ACC, Genra::MEL_ACC_OUTPUT);
		out(layout::MEL_SLIDE, Genra::MEL_SLIDE_OUTPUT);
		out(layout::PAD_VOCT, Genra::PAD_VOCT_OUTPUT);
		out(layout::PAD_GATE, Genra::PAD_GATE_OUTPUT);
		out(layout::PAD_VEL, Genra::PAD_VEL_OUTPUT);
		out(layout::PAD_MOD, Genra::PAD_MOD_OUTPUT);
	}

	void appendContextMenu(Menu* menu) override {
		Genra* m = dynamic_cast<Genra*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("GENRA"));
		menu->addChild(createMenuItem("Als MIDI-Datei exportieren…", "", [=]() { m->exportMidi(); }));
		menu->addChild(createMenuItem("MIDI-Datei importieren…", "", [=]() { m->importMidi(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Akkord-Modus", {CHORD_MODES[0], CHORD_MODES[1], CHORD_MODES[2]},
		                                      [=]() { return (size_t)m->chordMode; },
		                                      [=](size_t v) { m->chordMode = (int)v; }));
		menu->addChild(createSubmenuItem("Akkord-Vorlage", "", [=](Menu* sub) {
			const Genre& G = prof(m->curGenre);
			sub->addChild(createCheckMenuItem("Zufällig", "", [=]() { return m->templateSel < 0; }, [=]() {
				m->templateSel = -1;
				m->chordRegenRequested = true;
			}));
			for (int t = 0; t < G.nTemplates; t++) {
				sub->addChild(createCheckMenuItem(G.templates[t], "", [=]() { return m->templateSel == t; }, [=]() {
					m->templateSel = t;
					m->chordRegenRequested = true;
				}));
			}
		}));
		menu->addChild(createMenuItem("Nur Akkorde neu würfeln", "", [=]() { m->chordRegenRequested = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Genre-CV-Eingänge", {GENRE_CV_MODES[0], GENRE_CV_MODES[1]},
		                                      [=]() { return (size_t)m->genreCvMode; },
		                                      [=](size_t v) { m->genreCvMode = (int)v; }));
		menu->addChild(createMenuItem("Alle Spuren auf Global (folgen GENRE)", "", [=]() { m->resetTracksRequested = true; }));
		menu->addChild(createMenuItem("Alle Szenen löschen", "", [=]() { m->clearScenesRequested = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock-Eingang", {CLOCK_MODES[0], CLOCK_MODES[1], CLOCK_MODES[2], CLOCK_MODES[3]},
		                                      [=]() { return (size_t)m->clockMode; },
		                                      [=](size_t v) { m->clockMode = (int)v; }));
	}
};

Model* modelGenra = createModel<Genra, GenraWidget>("Genra");
