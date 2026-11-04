// GENRA – generative 4-track sequencer, VCV Rack module
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
static const char* CLOCK_MODES[4] = {"16tel (4 PPQN)", "8tel (2 PPQN)", "Viertel (1 PPQN)", "24 PPQN"};
static const char* CHORD_MODES[3] = {"Gemischt (Vorlagen + frei)", "Nur Genre-Vorlagen", "Frei erzeugt"};

static const NVGcolor TRACK_COLORS[4] = {
	nvgRGB(0xff, 0x8a, 0x3d), nvgRGB(0xff, 0x4f, 0x6d), nvgRGB(0x3d, 0xc9, 0xff), nvgRGB(0xa9, 0x8b, 0xff)};

struct Genra;
// shows the variant's name of the current genre on hover / right-click
struct VariantQuantity : ParamQuantity {
	std::string getDisplayValueString() override;
};

struct Genra : Module {
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
		LIGHTS_LEN
	};

	// ---- pattern & engine state
	Pattern pat;
	Rng rng;
	bool running = true;
	int step = -1;
	bool firstPass = true;
	bool fillActive = false;
	bool pendingGenre = false;
	int lastGenre = -1;
	int clockMode = 0;
	int chordMode = CM_MIXED;
	int templateSel = -1;

	// ---- requests from the UI thread
	std::atomic<bool> regenRequested{false};
	std::atomic<bool> chordRegenRequested{false};
	std::atomic<bool> importReady{false};
	Pattern importBuf;
	int importLengthIndex = 2;

	// ---- clock
	dsp::SchmittTrigger clockTrig, resetTrig, runTrig, newTrig;
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
	float pendingTick = -1.f;
	bool resetArmed = false;

	// ---- live inputs (copied into Params)
	int transpSemis = 0;
	bool extChord = false;
	int extRoot = 0;
	uint16_t extMask = 0;
	int curGenre = 0;

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
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		std::vector<std::string> genreNames;
		for (int g = 0; g < NUM_GENRES; g++)
			genreNames.push_back(prof(profileOf(g, 0)).name);
		configSwitch(GENRE_PARAM, 0.f, NUM_GENRES - 1, 0.f, "Genre", genreNames);
		configParam<VariantQuantity>(VARIANT_PARAM, 0.f, MAX_VARIANTS - 1, 0.f, "Variante");
		paramQuantities[VARIANT_PARAM]->snapEnabled = true;
		configButton(CHORDNEW_PARAM, "Neue Akkordfolge (NEW)");
		configSwitch(KEY_PARAM, 0.f, 11.f, 9.f, "Tonart (Grundton)",
		             {"C", "C#/Db", "D", "D#/Eb", "E", "F", "F#/Gb", "G", "G#/Ab", "A", "A#/Bb", "B/H"});
		std::vector<std::string> scaleNames = {"Auto (Genre)"};
		for (int s = 0; s < NUM_SCALES; s++)
			scaleNames.push_back(SCALES[s].name);
		configSwitch(SCALE_PARAM, 0.f, NUM_SCALES, 0.f, "Tonleiter", scaleNames);
		configParam(BPM_PARAM, 40.f, 220.f, 124.f, "Tempo (interne Clock)", " BPM");
		paramQuantities[BPM_PARAM]->snapEnabled = true;
		configParam(SWING_PARAM, -1.f, 1.f, 0.f, "Swing (relativ zum Genre)", "%", 0.f, 25.f);
		configParam(COMPLEX_PARAM, 0.f, 1.f, 0.5f, "Rhythmische Komplexität (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(COLOR_PARAM, 0.f, 1.f, 0.5f, "Harmonische Farbe (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(MUTATE_PARAM, 0.f, 1.f, 0.f, "Mutation pro Durchlauf", "%", 0.f, 100.f);
		configSwitch(LENGTH_PARAM, 0.f, 3.f, 2.f, "Loop-Länge", {"1 Takt", "2 Takte", "4 Takte", "8 Takte"});
		configButton(NEW_PARAM, "Neu würfeln (NEW)");
		configButton(RUN_PARAM, "Start / Stop");
		configSwitch(CHORDLOCK_PARAM, 0.f, 1.f, 0.f, "Akkordfolge sperren", {"Aus", "Gesperrt"});
		static const char* TN[4] = {"Drums", "Bass", "Melodie", "Pads"};
		for (int t = 0; t < 4; t++) {
			configParam(DENS_PARAM + t, 0.f, 1.f, 0.5f, std::string(TN[t]) + " Dichte", "%", 0.f, 200.f);
			configSwitch(MUTE_PARAM + t, 0.f, 1.f, 0.f, std::string(TN[t]) + " Mute", {"Aus", "Stumm"});
			configSwitch(LOCK_PARAM + t, 0.f, 1.f, 0.f, std::string(TN[t]) + " Lock", {"Aus", "Gesperrt"});
			configButton(TNEW_PARAM + t, std::string(TN[t]) + " neu würfeln (NEW)");
			configInput(TNEW_INPUT + t, std::string(TN[t]) + " neu würfeln (Trigger)");
		}
		configParam(FILL_PARAM, 0.f, 1.f, 0.25f, "Drum-Fill Wahrscheinlichkeit", "%", 0.f, 100.f);
		configParam(GHOST_PARAM, 0.f, 1.f, 0.3f, "Ghost Notes", "%", 0.f, 100.f);
		configParam(MOVE_PARAM, 0.f, 1.f, 0.5f, "Bass Bewegung (relativ zum Genre)", "%", 0.f, 200.f, -100.f);
		configParam(RANGE_PARAM, 0.f, 1.f, 0.5f, "Melodie Tonumfang", "%", 0.f, 100.f);
		configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "Pad Voicing (eng - offen)", "%", 0.f, 100.f);
		static const char* ON[3] = {"Bass", "Melodie", "Pads"};
		for (int i = 0; i < 3; i++)
			configSwitch(OCT_PARAM + i, -2.f, 2.f, 0.f, std::string(ON[i]) + " Oktave", {"-2", "-1", "0", "+1", "+2"});
		for (int id : std::initializer_list<int>{RUN_PARAM, NEW_PARAM, CHORDLOCK_PARAM, CHORDNEW_PARAM, TNEW_PARAM, TNEW_PARAM + 1, TNEW_PARAM + 2, TNEW_PARAM + 3})
			paramQuantities[id]->randomizeEnabled = false;
		for (int t = 0; t < 4; t++) {
			paramQuantities[MUTE_PARAM + t]->randomizeEnabled = false;
			paramQuantities[LOCK_PARAM + t]->randomizeEnabled = false;
		}

		configInput(CLOCK_INPUT, "Clock");
		configInput(RESET_INPUT, "Reset");
		configInput(RUN_INPUT, "Run (Trigger schaltet um)");
		configInput(NEW_INPUT, "Neu würfeln (Trigger)");
		configInput(GENRE_INPUT, "Genre CV (0-10 V)");
		configInput(MUTATE_INPUT, "Mutate CV (0-10 V)");
		configInput(TRANSP_INPUT, "Transpose (V/Oct)");
		configInput(CHORD_INPUT, "Akkord (polyphon, V/Oct)");
		configInput(CHORDNEW_INPUT, "Neue Akkordfolge (Trigger)");
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
		doGenerate();
	}

	// ------------------------------------------------------------ helpers
	int lengthSteps() {
		int idx = clamp((int)std::round(params[LENGTH_PARAM].getValue()), 0, 3);
		return LENGTHS[idx] * STEPS_PER_BAR;
	}

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
		P.complexK = params[COMPLEX_PARAM].getValue();
		P.colorK = params[COLOR_PARAM].getValue();
		P.swingK = params[SWING_PARAM].getValue();
		P.lengthBars = lengthSteps() / STEPS_PER_BAR;
		for (int t = 0; t < 4; t++)
			P.density[t] = params[DENS_PARAM + t].getValue();
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

	void doGenerate() {
		Params P = buildParams();
		if (lastGenre >= 0 && lastGenre != curGenre) {
			templateSel = -1; // template indices are genre specific
			P.templateSel = -1;
		}
		bool lk[NUM_TRACKS];
		locks(lk);
		generate(pat, curGenre, rng, lk, chordLocked(), P);
		lastGenre = curGenre;
		padHasPrev = false;
		newFlash = 1.f;
	}

	void readGenre() {
		curGenre = knobProfile();
		if (lastGenre >= 0 && curGenre != lastGenre && !pendingGenre)
			pendingGenre = true;
	}

	int knobGenre() {
		float g = params[GENRE_PARAM].getValue();
		if (inputs[GENRE_INPUT].isConnected())
			g += inputs[GENRE_INPUT].getVoltage() / 10.f * (NUM_GENRES - 1);
		return clamp((int)std::round(g), 0, NUM_GENRES - 1);
	}
	int knobProfile() {
		return profileOf(knobGenre(), (int)std::round(params[VARIANT_PARAM].getValue()));
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

	// ------------------------------------------------------------ tick
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

	void doTick(float sr) {
		int L = lengthSteps();
		if (pat.layoutLen != L)
			layoutTimeline(pat, L);
		step++;
		if (step >= L)
			step = 0;
		Params P = buildParams();
		if (pendingGenre && step % STEPS_PER_BAR == 0) {
			pendingGenre = false;
			doGenerate();
			P = buildParams();
		}
		if (step == 0) {
			fillActive = rng.uni() < P.fill;
			P.fillActive = fillActive;
			if (!firstPass) {
				float m = params[MUTATE_PARAM].getValue();
				if (inputs[MUTATE_INPUT].isConnected())
					m += inputs[MUTATE_INPUT].getVoltage() / 10.f;
				bool lk[NUM_TRACKS];
				locks(lk);
				mutate(pat, clamp(m, 0.f, 1.f), rng, lk, chordLocked(), P);
			}
			firstPass = false;
		}
		// drums
		Hit hits[NUM_DRUMS];
		drumStep(pat, P, step, L, fillActive, hits);
		if (params[MUTE_PARAM + 0].getValue() < 0.5f)
			for (int v = 0; v < NUM_DRUMS; v++)
				if (hits[v].on)
					queueDrum(v, hits[v], sr);
		// bass / melody
		NoteEv b = bassAt(pat, P, step, L);
		if (b.on && params[MUTE_PARAM + 1].getValue() < 0.5f)
			startMono(bassV, b, sr);
		NoteEv m = melAt(pat, P, step, L);
		if (m.on && params[MUTE_PARAM + 2].getValue() < 0.5f)
			startMono(melV, m, sr);
		// pads
		PadEv pe = padAt(pat, P, step, L, padPrev, padHasPrev);
		if (pe.on && params[MUTE_PARAM + 3].getValue() < 0.5f) {
			if (padV.gate > 0.f)
				padV.retrig = 0.002f * sr;
			for (int k = 0; k < 4; k++)
				padV.pitch[k] = (pe.notes[k] - 60) / 12.f;
			padV.vel = pe.vel;
			padV.gate = std::max(0.004f * sr, pe.len * stepPeriod * pe.gate);
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
		initPattern(pat);
		curGenre = 0;
		doGenerate();
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
			pendingTick = -1.f;
			resetArmed = true;
		}

		if (importReady) {
			pat = importBuf;
			params[LENGTH_PARAM].setValue((float)importLengthIndex);
			importReady = false;
			padHasPrev = false;
		}
		readGenre();
		readChordInput();
		if (regenRequested.exchange(false)) {
			pendingGenre = false;
			doGenerate();
		}
		for (int t = 0; t < 4; t++) {
			bool trig = tnewBtn[t].process(params[TNEW_PARAM + t].getValue() > 0.5f);
			trig |= tnewTrig[t].process(inputs[TNEW_INPUT + t].getVoltage(), 0.1f, 1.f);
			if (trig) {
				// an explicit per-track NEW works even when the track is locked
				Params P = buildParams();
				generateTrack(pat, t, curGenre, rng, P);
				if (t == T_PADS)
					padHasPrev = false;
				tnewFlash[t] = 1.f;
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
			if (pendingTick >= 0.f) {
				pendingTick -= 1.f;
				if (pendingTick <= 0.f || tick) {
					pendingTick = -1.f;
					doTick(sr);
				}
			}
			if (tick) {
				int L = lengthSteps();
				int next = (step + 1 >= L) ? 0 : step + 1;
				Params P = buildParams();
				float swing = effSwing(P, pat.trackGenre[T_DRUMS]);
				float delay = (next % 2 == 1) ? (swing - 0.5f) * 2.f * stepPeriod : 0.f;
				if (delay >= 1.f)
					pendingTick = delay;
				else
					doTick(sr);
			}
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
			newFlash = std::max(0.f, newFlash - dt * 4.f);
			lights[NEW_LIGHT].setBrightness(newFlash);
			lights[RUN_LIGHT].setBrightness(running ? 1.f : 0.f);
			lights[CHORDLOCK_LIGHT].setBrightness(chordLocked() ? 1.f : 0.f);
			chordNewFlash = std::max(0.f, chordNewFlash - dt * 4.f);
			lights[CHORDNEW_LIGHT].setBrightness(chordNewFlash);
			for (int t = 0; t < 4; t++) {
				tnewFlash[t] = std::max(0.f, tnewFlash[t] - dt * 4.f);
				lights[TNEW_LIGHT + t].setBrightness(tnewFlash[t]);
			}
			for (int t = 0; t < 4; t++) {
				lights[MUTE_LIGHT + t].setBrightness(params[MUTE_PARAM + t].getValue());
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
		json_object_set_new(root, "pattern", json_string(string::toBase64((const uint8_t*)&pat, sizeof(Pattern)).c_str()));
		return root;
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
		if ((j = json_object_get(root, "pattern"))) {
			std::vector<uint8_t> data = string::fromBase64(json_string_value(j));
			if (data.size() == sizeof(Pattern)) {
				Pattern p;
				std::memcpy(&p, data.data(), sizeof(Pattern));
				if (p.magic == PATTERN_MAGIC && p.version == PATTERN_VERSION) {
					pat = p;
					lastGenre = curGenre = knobProfile();
					pendingGenre = false;
					padHasPrev = false;
				}
			}
		}
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
		std::vector<uint8_t> data = writeSmf(renderLoop(pat, P, L), bpm, prof(curGenre).name);
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
struct GenraLight : GrayModuleLightWidget {};
template <int R, int G, int B>
struct ColorLight : GrayModuleLightWidget {
	ColorLight() { addBaseColor(nvgRGB(R, G, B)); }
};
using OrangeLight = ColorLight<0xff, 0x8a, 0x3d>;
using PinkLight = ColorLight<0xff, 0x4f, 0x6d>;
using CyanLight = ColorLight<0x3d, 0xc9, 0xff>;
using PurpleLight = ColorLight<0xa9, 0x8b, 0xff>;
using MintLight = ColorLight<0x5d, 0xff, 0x8a>;

// ================================================================= display
// geometry (mm, relative to the display's top-left)
static const float CHORD_X = 64.f;
static const float CHORD_W = 78.f;
static const float CHORD_Y = 2.6f;
static const float CHORD_H = 6.f;

struct GenraDisplay : Widget {
	Genra* module = nullptr;
	int hoverChord = -1;


	int nChordsShown() { return module ? std::max<int>(1, module->pat.nProg) : 4; }
	float chordBoxW() { return std::min(17.f, CHORD_W / nChordsShown()); }

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

	void text(NVGcontext* vg, std::shared_ptr<window::Font> f, float xmm, float ymm, float sizeMm, NVGcolor c, const std::string& s,
	          int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, float spacing = 0.f) {
		nvgFontFaceId(vg, f->handle);
		nvgFontSize(vg, mm2px(sizeMm));
		nvgTextLetterSpacing(vg, mm2px(spacing));
		nvgFillColor(vg, c);
		nvgTextAlign(vg, align);
		nvgText(vg, mm2px(xmm), mm2px(ymm), s.c_str(), NULL);
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

		std::string genre = "TECHNO", variant = "ROLLING", info = "A MINOR  128 BPM  SWING 52%", bar = "BAR 1/4", status;
		std::vector<std::string> chords = {"Am", "F", "C", "G"};
		int curChord = 0;
		bool lanes[4][16] = {};
		int curStep = -1;

		if (module) {
			Genra* m = module;
			Params P = m->buildParams();
			int L = m->lengthSteps();
			int g = m->curGenre;
			genre = prof(g).name;
			for (char& c : genre)
				c = (char)std::toupper((unsigned char)c);
			variant = prof(g).variant;
			for (char& c : variant)
				c = (char)std::toupper((unsigned char)c);
			if (m->pendingGenre)
				variant += " ...";
			float swing = effSwing(P, m->pat.trackGenre[T_DRUMS]);
			char buf[128];
			bool ext = m->inputs[Genra::CLOCK_INPUT].isConnected();
			float bpm = ext ? (m->stepPeriod > 0.f ? 60.f * APP->engine->getSampleRate() / (m->stepPeriod * 4.f) : 0.f)
			                : m->params[Genra::BPM_PARAM].getValue();
			std::string scaleName = SCALES[P.scale].name;
			for (char& c : scaleName)
				c = (char)std::toupper((unsigned char)c);
			snprintf(buf, sizeof(buf), "%s %s  %s%.0f BPM  SWING %.0f%%", NOTE_NAMES[(P.key + P.transp + 120) % 12], scaleName.c_str(),
			         ext ? "EXT " : "", bpm, swing * 100.f);
			info = buf;
			int s = std::max(0, m->step);
			snprintf(buf, sizeof(buf), "BAR %d/%d", s / 16 + 1, L / 16);
			bar = buf;
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
				chords.push_back(chordName(resolveSpec(m->pat.prog[i], P, color), P.key + P.transp));
			if (P.extChord) {
				Chord c = chordAt(m->pat, P, s);
				chords.assign(1, chordName(c, P.key + P.transp));
				curChord = 0;
			}
			else
				curChord = m->pat.ev[eventAt(m->pat, s)].spec;

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
		text(vg, bold, 4.f, 7.2f, 5.2f, white, genre, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		{
			// variant name right after the genre name
			nvgFontFaceId(vg, bold->handle);
			nvgFontSize(vg, mm2px(5.2f));
			nvgTextLetterSpacing(vg, mm2px(0.3f));
			float bounds[4];
			nvgTextBounds(vg, 0, 0, genre.c_str(), NULL, bounds);
			float wmmG = (bounds[2] - bounds[0]) / mm2px(1.f);
			text(vg, bold, 4.f + wmmG + 2.f, 7.2f, 2.6f, dim, std::string("· ") + variant, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		}
		text(vg, mono, 4.f, 12.6f, 2.7f, dim, info);

		// chords
		int n = (int)chords.size();
		float bw = std::min(17.f, CHORD_W / std::max(1, n));
		for (int i = 0; i < n; i++) {
			float x = CHORD_X + i * bw;
			bool active = i == curChord;
			bool hover = i == hoverChord;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(x), mm2px(CHORD_Y), mm2px(bw - 1.2f), mm2px(CHORD_H), mm2px(0.8f));
			nvgFillColor(vg, active ? TRACK_COLORS[3] : (hover ? nvgRGB(0x2a, 0x2b, 0x33) : dark));
			nvgFill(vg);
			float fs = n > 5 ? 2.2f : 2.7f;
			text(vg, bold, x + (bw - 1.2f) / 2.f, CHORD_Y + CHORD_H / 2.f, fs, active ? nvgRGB(0x0a, 0x0a, 0x0d) : dim, chords[i],
			     NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
		}
		text(vg, mono, CHORD_X, 13.f, 2.4f, dim, bar);
		text(vg, mono, CHORD_X + CHORD_W - 1.2f, 13.f, 2.4f, TRACK_COLORS[3], status, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);

		// lanes
		float lx = wmm - 4.f - 16 * 2.3f;
		for (int t = 0; t < 4; t++) {
			for (int i = 0; i < 16; i++) {
				float x = lx + i * 2.3f, y = 2.8f + t * 3.3f;
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
			Params P = m->buildParams();
			ChordSpec& cs = m->pat.prog[i];
			int cur = currentAlternative(cs, P);
			applyAlternative(cs, cur + 1, P);
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
		ChordSpec& cs = module->pat.prog[i];
		int d = e.scrollDelta.y > 0 ? 1 : -1;
		cs.extOff = (int8_t)clamp(cs.extOff + d, -3, 3);
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
			menu->addChild(createCheckMenuItem(name, roman, [=]() { return a == cur; }, [=]() {
				Params P2 = m->buildParams();
				applyAlternative(m->pat.prog[i], a, P2);
			}));
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Mehr Farbe (Mausrad hoch)", "", [=]() {
			m->pat.prog[i].extOff = (int8_t)clamp(m->pat.prog[i].extOff + 1, -3, 3);
		}));
		menu->addChild(createMenuItem("Weniger Farbe (Mausrad runter)", "", [=]() {
			m->pat.prog[i].extOff = (int8_t)clamp(m->pat.prog[i].extOff - 1, -3, 3);
		}));
		menu->addChild(createMenuItem("Farbe zurücksetzen", "", [=]() { m->pat.prog[i].extOff = 0; }));
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

		GenraDisplay* disp = createWidget<GenraDisplay>(mm2px(Vec(layout::DISPLAY[0], layout::DISPLAY[1])));
		disp->box.size = mm2px(Vec(layout::DISPLAY[2], layout::DISPLAY[3]));
		disp->module = module;
		addChild(disp);

		addParam(createParamCentered<RoundLargeBlackKnob>(P(layout::GENRE), module, Genra::GENRE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::KEY), module, Genra::KEY_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::SCALE), module, Genra::SCALE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::BPM), module, Genra::BPM_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::SWING), module, Genra::SWING_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::COMPLEX), module, Genra::COMPLEX_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::COLOR), module, Genra::COLOR_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::MUTATE), module, Genra::MUTATE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::LENGTH), module, Genra::LENGTH_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(layout::NEW), module, Genra::NEW_PARAM, Genra::NEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<MintLight>>(P(layout::RUN), module, Genra::RUN_PARAM, Genra::RUN_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<PurpleLight>>(P(layout::CHORDLOCK), module, Genra::CHORDLOCK_PARAM, Genra::CHORDLOCK_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::VARIANT), module, Genra::VARIANT_PARAM));
		addParam(createLightParamCentered<VCVLightBezel<PurpleLight>>(P(layout::CHORDNEW), module, Genra::CHORDNEW_PARAM, Genra::CHORDNEW_LIGHT));
		addInput(createInputCentered<PJ301MPort>(P(layout::IN_CHORDNEW), module, Genra::CHORDNEW_INPUT));
		const float* tnew[4] = {layout::NEW_0, layout::NEW_1, layout::NEW_2, layout::NEW_3};
		const float* tin[4] = {layout::IN_NEW_0, layout::IN_NEW_1, layout::IN_NEW_2, layout::IN_NEW_3};
		for (int t = 0; t < 4; t++) {
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(tnew[t]), module, Genra::TNEW_PARAM + t, Genra::TNEW_LIGHT + t));
			addInput(createInputCentered<PJ301MPort>(P(tin[t]), module, Genra::TNEW_INPUT + t));
		}
		addOutput(createOutputCentered<PJ301MPort>(P(layout::BASS_SLIDE), module, Genra::BASS_SLIDE_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(layout::MEL_SLIDE), module, Genra::MEL_SLIDE_OUTPUT));

		const float* dens[4] = {layout::DENS_0, layout::DENS_1, layout::DENS_2, layout::DENS_3};
		const float* mute[4] = {layout::MUTE_0, layout::MUTE_1, layout::MUTE_2, layout::MUTE_3};
		const float* lock[4] = {layout::LOCK_0, layout::LOCK_1, layout::LOCK_2, layout::LOCK_3};
		const float* oct[3] = {layout::OCT_0, layout::OCT_1, layout::OCT_2};
		for (int t = 0; t < 4; t++)
			addParam(createParamCentered<RoundBlackKnob>(P(dens[t]), module, Genra::DENS_PARAM + t));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::FILL), module, Genra::FILL_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::GHOST), module, Genra::GHOST_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::MOVE), module, Genra::MOVE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::RANGE), module, Genra::RANGE_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(layout::VOICING), module, Genra::VOICING_PARAM));
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

		const float* ins[8] = {layout::IN_CLOCK, layout::IN_RESET, layout::IN_RUN, layout::IN_NEW,
		                       layout::IN_GENRE, layout::IN_MUTATE, layout::IN_TRANSP, layout::IN_CHORD};
		for (int i = 0; i < 8; i++)
			addInput(createInputCentered<PJ301MPort>(P(ins[i]), module, i));

		const float* drumOut[8] = {layout::DRUM_0, layout::DRUM_1, layout::DRUM_2, layout::DRUM_3,
		                           layout::DRUM_4, layout::DRUM_5, layout::DRUM_6, layout::DRUM_7};
		for (int v = 0; v < 8; v++)
			addOutput(createOutputCentered<PJ301MPort>(P(drumOut[v]), module, Genra::DRUM_OUTPUT + v));
		const float* outs[12] = {layout::BASS_VOCT, layout::BASS_GATE, layout::BASS_VEL, layout::BASS_ACC,
		                         layout::MEL_VOCT, layout::MEL_GATE, layout::MEL_VEL, layout::MEL_ACC,
		                         layout::PAD_VOCT, layout::PAD_GATE, layout::PAD_VEL, layout::PAD_MOD};
		for (int i = 0; i < 12; i++)
			addOutput(createOutputCentered<PJ301MPort>(P(outs[i]), module, Genra::BASS_VOCT_OUTPUT + i));
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
		menu->addChild(createIndexSubmenuItem("Clock-Eingang", {CLOCK_MODES[0], CLOCK_MODES[1], CLOCK_MODES[2], CLOCK_MODES[3]},
		                                      [=]() { return (size_t)m->clockMode; },
		                                      [=](size_t v) { m->clockMode = (int)v; }));
	}
};

Model* modelGenra = createModel<Genra, GenraWidget>("Genra");
