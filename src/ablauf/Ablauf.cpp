// SPDX-License-Identifier: GPL-3.0-only
// ABLAUF – song arranger for GENRA: 64 pattern places (4 banks of 4 x 4 pads), a song list (pattern, bars, part,
// tracks, fill per row), one button per song part that turns the pattern playing now into an intro, a build, ...
// (Arranger.cpp), connected to GENRA like the grid editor (docked, or chosen with right-click › Verbinden mit…).
//
// Threads: everything that edits the bank or the song runs on the UI thread (ModuleWidget::step -> Ablauf::uiStep),
// like the grid editor. GENRA takes patterns over through its cue mailbox and applies them on its audio thread at a bar
// line, so the timing is exact. The audio thread of ABLAUF only counts button presses and trigger inputs and drives
// the outputs from values the UI thread publishes.
#include "../GenraModule.hpp"
#include "../ui/GenraUI.hpp"
#include "AblaufLayout.hpp"
#include <deque>

namespace genra_ablauf {

namespace L = ablayout;
namespace ui = genra_ui;

static const int NUM_SLOTS = 64, MAX_ROWS = 64, PADS_PER_BANK = 16;
static const int ROW_BARS[6] = {1, 2, 4, 8, 16, 32};
static const NVGcolor ACC = nvgRGB(0xff, 0xd2, 0x3d);
// song parts (keep in step with SECS in tools/ablauf_panel.py)
static const NVGcolor SEC_COL[NUM_SECTIONS] = {nvgRGB(0x6f, 0x86, 0xb8), nvgRGB(0x9b, 0x7b, 0xd4), nvgRGB(0xe0, 0xa5, 0x26),
                                               nvgRGB(0xff, 0x5a, 0x4f), nvgRGB(0x3f, 0xae, 0x9a), nvgRGB(0x8a, 0x8d, 0x99)};
static const char* const TRACK_LETTER[4] = {"D", "B", "M", "P"};

static std::string slotName(int i) {
	if (i < 0 || i >= NUM_SLOTS)
		return "–";
	return string::f("%c%d", 'A' + i / PADS_PER_BANK, i % PADS_PER_BANK + 1);
}

struct Row {
	int8_t slot = -1;
	int8_t barsIdx = 2;   // ROW_BARS
	int8_t sec = SEC_DROP;
	uint8_t on = 0xF;     // bit per track: audible in this row
	bool fill = false;    // fill / drop at the end of the row
	float cvA = 0.f, cvB = 0.f;
	bool operator==(const Row& o) const {
		return slot == o.slot && barsIdx == o.barsIdx && sec == o.sec && on == o.on && fill == o.fill && cvA == o.cvA &&
		       cvB == o.cvB;
	}
};

struct Ablauf;
static Ablauf* findAblauf(int64_t id);

/** One undoable step (also on Rack's Ctrl+Z): the song list and at most one pattern place, before and after. */
struct AblaufAction : history::Action {
	int64_t moduleId = -1;
	std::vector<Row> rowsBefore, rowsAfter;
	int curBefore = 0, curAfter = 0;
	int slot = -1;
	std::shared_ptr<Slot> slotBefore, slotAfter;
	void undo() override;
	void redo() override;
};

struct Ablauf : Module {
	enum ParamId {
		ZEILE_PARAM,
		PATTERN_PARAM,
		TAKTE_PARAM,
		ABSCHNITT_PARAM,
		ENUMS(TRACK_PARAM, 4),
		INS_PARAM,
		DEL_PARAM,
		UNDO_PARAM,
		FILL_PARAM,
		MODE_PARAM,
		PLAY_PARAM,
		STOP_PARAM,
		LOOP_PARAM,
		BANK_PARAM,
		ENUMS(REROLL_PARAM, 4),
		ENUMS(PART_PARAM, NUM_SECTIONS),
		STORE_PARAM,
		COPY_PARAM,
		CLEAR_PARAM,
		PARAMS_LEN
	};
	enum InputId { RUN_INPUT, RESET_INPUT, NEXT_INPUT, PATT_INPUT, ROW_INPUT, INPUTS_LEN };
	enum OutputId { END_OUTPUT, ROW_OUTPUT, BUILD_OUTPUT, DROP_OUTPUT, POS_OUTPUT, CVA_OUTPUT, CVB_OUTPUT, OUTPUTS_LEN };
	enum LightId {
		LINK_LIGHT,
		ENUMS(TRACK_LIGHT, 4),
		INS_LIGHT,
		DEL_LIGHT,
		UNDO_LIGHT,
		FILL_LIGHT,
		PLAY_LIGHT,
		STOP_LIGHT,
		LOOP_LIGHT,
		ENUMS(PART_LIGHT, NUM_SECTIONS),
		STORE_LIGHT,
		COPY_LIGHT,
		CLEAR_LIGHT,
		LIGHTS_LEN
	};
	// momentary buttons: the audio thread counts presses, the UI thread acts on them
	static const int NUM_PRESS = PARAMS_LEN;
	std::atomic<uint32_t> presses[NUM_PRESS];
	uint32_t seen[NUM_PRESS] = {};
	dsp::BooleanTrigger btn[NUM_PRESS];
	dsp::SchmittTrigger runTrig, resetTrig, nextTrig;
	std::atomic<uint32_t> runReq{0}, resetReq{0}, nextReq{0};
	uint32_t runSeen = 0, resetSeen = 0, nextSeen = 0;
	std::atomic<float> pattCv{-1.f}, rowCv{-1.f};
	// published by the UI thread for the outputs
	std::atomic<int> outSec{-1};
	std::atomic<bool> outRunning{false};
	std::atomic<uint32_t> rowTrig{0}, endTrig{0};
	std::atomic<float> outPos{0.f}, outCvA{0.f}, outCvB{0.f};
	uint32_t rowTrigSeen = 0, endTrigSeen = 0;
	dsp::PulseGenerator rowPulse, endPulse;

	// ---- the bank and the song (UI thread)
	std::unique_ptr<Slot[]> slots{new Slot[NUM_SLOTS]};
	uint32_t slotVer[NUM_SLOTS] = {};
	std::vector<Row> rows;
	int curRow = 0;
	int selSlot = -1;
	int armed = 0; // 1 STORE, 2 COPY, 3 CLEAR: waits for a pad
	int copySrc = -1;
	int64_t linkedId = -1;
	Rng rng{random::u64()};
	std::string msg;
	float msgTime = 0.f;
	float blink = 0.f;

	// knobs bound to the selected row
	int lastZ = -1, lastP = -1, lastT = -1, lastA = -1;

	// waiting for GENRA's snapshot
	int pendingStore = -1, pendingPart = -1;
	int pendingPadCue = -1; // pattern mode: cue as soon as the mailbox is free

	// variants: the pattern they were made from, and where each part went
	struct Session {
		bool active = false;
		std::unique_ptr<Slot> source{new Slot};
		Params P;
		int result[NUM_SECTIONS];
		uint32_t ver[NUM_SECTIONS];
		Session() { clear(); }
		void clear() {
			active = false;
			for (int k = 0; k < NUM_SECTIONS; k++)
				result[k] = -1, ver[k] = 0;
		}
	} session;

	// song playback
	struct Song {
		bool playing = false;
		int row = -1;          // row playing now (-1: waiting for the first)
		int64_t rowStart = 0;  // GENRA's bar index where it began
		uint32_t waitId = 0;   // cue handed over, not applied yet
		int cuedRow = -1;
		bool cuedStop = false, cuedWrap = false;
		uint64_t cuedSig = 0;
		int jumpRow = -1;      // NEXT / ZEILE CV: this row at the next bar line
		int rowBars = 0;       // length of the playing row when it began
		int cuedBars = 0;
		int64_t jumpAt = 0;
		int lastRowCv = -1, lastPattCv = -1;
	} song;
	uint32_t cueCounter = 1;
	int pendingStart = -1; // PLAY while GENRA was taking a cue: try again next frame

	Ablauf() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int i = 0; i < NUM_PRESS; i++)
			presses[i] = 0;
		struct RowQuantity : ParamQuantity {
			std::string getDisplayValueString() override { return string::f("%d", (int)std::round(getValue()) + 1); }
		};
		struct SlotQuantity : ParamQuantity {
			std::string getDisplayValueString() override {
				Ablauf* m = dynamic_cast<Ablauf*>(module);
				int i = (int)std::round(getValue());
				std::string s = slotName(i);
				if (m && i >= 0 && i < NUM_SLOTS && !m->slots[i].used)
					s += " (leer)";
				return s;
			}
		};
		configParam<RowQuantity>(ZEILE_PARAM, 0.f, MAX_ROWS - 1, 0.f, "Zeile");
		configParam<SlotQuantity>(PATTERN_PARAM, 0.f, NUM_SLOTS - 1, 0.f, "Pattern der Zeile");
		configSwitch(TAKTE_PARAM, 0.f, 5.f, 2.f, "Takte der Zeile", {"1 Takt", "2 Takte", "4 Takte", "8 Takte", "16 Takte", "32 Takte"});
		configSwitch(ABSCHNITT_PARAM, 0.f, NUM_SECTIONS - 1, SEC_DROP, "Abschnitt der Zeile",
		             {"Intro", "Verse", "Build", "Drop", "Break", "Outro"});
		for (int p : {ZEILE_PARAM, PATTERN_PARAM})
			paramQuantities[p]->snapEnabled = true;
		static const char* TN[4] = {"Drums", "Bass", "Melodie", "Pads"};
		for (int t = 0; t < 4; t++) {
			configButton(TRACK_PARAM + t, std::string(TN[t]) + " in dieser Zeile an / aus");
			configSwitch(REROLL_PARAM + t, 0.f, 1.f, 1.f, std::string("Varianten: ") + TN[t] + " neu würfeln",
			             {"Behalten", "Neu würfeln"});
		}
		configButton(INS_PARAM, "Zeile einfügen (mit dem gewählten Pad)");
		configButton(DEL_PARAM, "Zeile löschen");
		configButton(UNDO_PARAM, "Rückgängig");
		configButton(FILL_PARAM, "Fill am Ende der Zeile an / aus");
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Modus", {"Pattern (Pads live spielen)", "Song (Liste abspielen)"});
		configButton(PLAY_PARAM, "Play (Song ab der gewählten Zeile)");
		configButton(STOP_PARAM, "Stop");
		configSwitch(LOOP_PARAM, 0.f, 1.f, 1.f, "Song wiederholen", {"Aus (am Ende stoppen)", "An"});
		configSwitch(BANK_PARAM, 0.f, 3.f, 0.f, "Bank", {"A", "B", "C", "D"});
		static const char* PN[NUM_SECTIONS] = {"Intro", "Verse", "Build", "Drop", "Break", "Outro"};
		for (int k = 0; k < NUM_SECTIONS; k++)
			configButton(PART_PARAM + k, std::string(PN[k]) + " aus dem laufenden Pattern machen (nochmal: neu würfeln)");
		configButton(STORE_PARAM, "STORE: dann ein Pad – speichert, was GENRA gerade spielt");
		configButton(COPY_PARAM, "COPY: erst Quelle, dann Ziel antippen");
		configButton(CLEAR_PARAM, "CLEAR: dann ein Pad – löscht den Platz");
		for (int i = 0; i < PARAMS_LEN; i++)
			paramQuantities[i]->randomizeEnabled = false;
		configInput(RUN_INPUT, "Run (Trigger: Play / Stop)");
		configInput(RESET_INPUT, "Reset (Song an den Anfang)");
		configInput(NEXT_INPUT, "Nächste Zeile / nächstes Pattern (Trigger, am nächsten Takt)");
		configInput(PATT_INPUT, "Pattern CV (0–10 V über alle 64 Plätze, Pattern-Modus)");
		configInput(ROW_INPUT, "Zeile CV (0–10 V über die Song-Liste, springt am nächsten Takt)");
		configOutput(END_OUTPUT, "Ende (Trigger am Song-Ende und beim Wiederholen)");
		configOutput(ROW_OUTPUT, "Zeile (Trigger bei jeder neuen Zeile)");
		configOutput(BUILD_OUTPUT, "Build (Gate, solange ein Build läuft)");
		configOutput(DROP_OUTPUT, "Drop (Gate, solange ein Drop läuft)");
		configOutput(POS_OUTPUT, "Song-Position (0–10 V)");
		configOutput(CVA_OUTPUT, "CV A der Zeile");
		configOutput(CVB_OUTPUT, "CV B der Zeile");
	}

	// ------------------------------------------------------------ audio thread
	void process(const ProcessArgs& args) override {
		for (int p : {(int)INS_PARAM, (int)DEL_PARAM, (int)UNDO_PARAM, (int)FILL_PARAM, (int)PLAY_PARAM, (int)STOP_PARAM,
		              (int)STORE_PARAM, (int)COPY_PARAM, (int)CLEAR_PARAM})
			if (btn[p].process(params[p].getValue() > 0.5f))
				presses[p]++;
		for (int t = 0; t < 4; t++)
			if (btn[TRACK_PARAM + t].process(params[TRACK_PARAM + t].getValue() > 0.5f))
				presses[TRACK_PARAM + t]++;
		for (int k = 0; k < NUM_SECTIONS; k++)
			if (btn[PART_PARAM + k].process(params[PART_PARAM + k].getValue() > 0.5f))
				presses[PART_PARAM + k]++;
		if (runTrig.process(inputs[RUN_INPUT].getVoltage(), 0.1f, 1.f))
			runReq++;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			resetReq++;
		if (nextTrig.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 1.f))
			nextReq++;
		pattCv.store(inputs[PATT_INPUT].isConnected() ? inputs[PATT_INPUT].getVoltage() : -1.f, std::memory_order_relaxed);
		rowCv.store(inputs[ROW_INPUT].isConnected() ? inputs[ROW_INPUT].getVoltage() : -1.f, std::memory_order_relaxed);

		uint32_t r = rowTrig.load(std::memory_order_relaxed), en = endTrig.load(std::memory_order_relaxed);
		if (r != rowTrigSeen) {
			rowTrigSeen = r;
			rowPulse.trigger(1e-3f);
		}
		if (en != endTrigSeen) {
			endTrigSeen = en;
			endPulse.trigger(1e-3f);
		}
		bool run = outRunning.load(std::memory_order_relaxed);
		int sec = outSec.load(std::memory_order_relaxed);
		outputs[ROW_OUTPUT].setVoltage(rowPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[END_OUTPUT].setVoltage(endPulse.process(args.sampleTime) ? 10.f : 0.f);
		outputs[BUILD_OUTPUT].setVoltage(run && sec == SEC_BUILD ? 10.f : 0.f);
		outputs[DROP_OUTPUT].setVoltage(run && sec == SEC_DROP ? 10.f : 0.f);
		outputs[POS_OUTPUT].setVoltage(outPos.load(std::memory_order_relaxed));
		outputs[CVA_OUTPUT].setVoltage(outCvA.load(std::memory_order_relaxed));
		outputs[CVB_OUTPUT].setVoltage(outCvB.load(std::memory_order_relaxed));
	}

	// ------------------------------------------------------------ UI thread
	Genra* genra() {
		if (linkedId >= 0) {
			Module* m = APP->engine->getModule(linkedId);
			if (m && m->model == modelGenra)
				return static_cast<Genra*>(m);
		}
		Module* side[2] = {leftExpander.module, rightExpander.module};
		for (Module* m : side)
			if (m && m->model == modelGenra)
				return static_cast<Genra*>(m);
		return nullptr;
	}

	bool pressed(int p) {
		uint32_t v = presses[p].load();
		if (v == seen[p])
			return false;
		seen[p] = v;
		return true;
	}

	void say(const std::string& s) {
		msg = s;
		msgTime = 2.5f;
	}

	bool songMode() { return params[MODE_PARAM].getValue() > 0.5f; }
	bool loopOn() { return params[LOOP_PARAM].getValue() > 0.5f; }
	int bank() { return clamp((int)std::round(params[BANK_PARAM].getValue()), 0, 3); }
	unsigned rerollMask() {
		unsigned m = 0;
		for (int t = 0; t < 4; t++)
			if (params[REROLL_PARAM + t].getValue() > 0.5f)
				m |= 1u << t;
		return m;
	}
	bool playable(int r) {
		return r >= 0 && r < (int)rows.size() && rows[r].slot >= 0 && rows[r].slot < NUM_SLOTS && slots[rows[r].slot].used;
	}
	int nextPlayable(int from) {
		for (int r = std::max(0, from); r < (int)rows.size(); r++)
			if (playable(r))
				return r;
		return -1;
	}
	int freeSlot() {
		for (int k = 0; k < NUM_SLOTS; k++) {
			int i = (bank() * PADS_PER_BANK + k) % NUM_SLOTS;
			if (!slots[i].used)
				return i;
		}
		return -1;
	}
	static int barsIdxFor(int lengthIdx) { return clamp(lengthIdx, 0, 3); } // 1/2/4/8 bars -> ROW_BARS index

	// ---- undo
	AblaufAction* beginChange(const char* name, int slot = -1) {
		AblaufAction* a = new AblaufAction;
		a->name = std::string("ABLAUF: ") + name;
		a->moduleId = id;
		a->rowsBefore = rows;
		a->curBefore = curRow;
		a->slot = slot;
		if (slot >= 0)
			a->slotBefore = std::make_shared<Slot>(slots[slot]);
		return a;
	}
	void endChange(AblaufAction* a) {
		a->rowsAfter = rows;
		a->curAfter = curRow;
		if (a->slot >= 0) {
			a->slotAfter = std::make_shared<Slot>(slots[a->slot]);
			slotVer[a->slot]++;
		}
		APP->history->push(a);
	}
	void restore(const std::vector<Row>& r, int cur, int slot, const std::shared_ptr<Slot>& s) {
		bool listChanged = !(r.size() == rows.size() && std::equal(r.begin(), r.end(), rows.begin()));
		rows = r;
		if (listChanged)
			rowsReplaced();
		if (slot >= 0 && s) {
			slots[slot] = *s;
			slotVer[slot]++;
		}
		setRow(cur);
	}
	// rows changed as a whole while the song plays: the playing pattern ends as planned, then the row after it
	void rowsReplaced() {
		if (!song.playing || song.row < 0)
			return;
		song.jumpRow = std::min(song.row + 1, (int)rows.size());
		song.jumpAt = song.rowStart + song.rowBars;
		song.row = -1;
	}

	void undoButton() {
		history::State* h = APP->history;
		if (h->actionIndex > 0) {
			AblaufAction* a = dynamic_cast<AblaufAction*>(h->actions[h->actionIndex - 1]);
			if (a && a->moduleId == id) {
				h->undo();
				say("Rückgängig");
				return;
			}
		}
		say(h->canUndo() ? "Zuletzt kein ABLAUF-Schritt (Strg+Z)" : "Nichts rückgängig zu machen");
	}

	// ---- the selected row and its knobs
	void setRow(int r) {
		curRow = rows.empty() ? 0 : clamp(r, 0, (int)rows.size() - 1);
		params[ZEILE_PARAM].setValue((float)curRow);
		lastZ = curRow;
		syncKnobs();
	}
	void syncKnobs() {
		if (rows.empty())
			return;
		const Row& w = rows[curRow];
		params[PATTERN_PARAM].setValue((float)std::max(0, (int)w.slot));
		params[TAKTE_PARAM].setValue((float)w.barsIdx);
		params[ABSCHNITT_PARAM].setValue((float)w.sec);
		lastP = std::max(0, (int)w.slot);
		lastT = w.barsIdx;
		lastA = w.sec;
	}
	void bindKnobs() {
		int z = (int)std::round(params[ZEILE_PARAM].getValue());
		if (rows.empty()) {
			if (z != 0)
				params[ZEILE_PARAM].setValue(0.f);
			curRow = 0;
			lastZ = 0;
			return;
		}
		if (z != lastZ || z >= (int)rows.size()) {
			setRow(z);
			return;
		}
		Row& w = rows[curRow];
		int p = (int)std::round(params[PATTERN_PARAM].getValue());
		int t = (int)std::round(params[TAKTE_PARAM].getValue());
		int a = (int)std::round(params[ABSCHNITT_PARAM].getValue());
		if (p != lastP) {
			w.slot = (int8_t)clamp(p, 0, NUM_SLOTS - 1);
			if (slots[w.slot].used && slots[w.slot].tag < 0) {
				slots[w.slot].tag = w.sec; // an untagged pattern takes the part of the row it is used in
				slotVer[w.slot]++;
			}
			selSlot = w.slot;
		}
		if (t != lastT)
			w.barsIdx = (int8_t)clamp(t, 0, 5);
		if (a != lastA)
			w.sec = (int8_t)clamp(a, 0, NUM_SECTIONS - 1);
		lastP = p, lastT = t, lastA = a;
	}

	// ---- song list edits
	void insertRow() {
		if ((int)rows.size() >= MAX_ROWS) {
			say("Höchstens 64 Zeilen");
			return;
		}
		int s = selSlot >= 0 ? selSlot : (rows.empty() ? -1 : rows[curRow].slot);
		AblaufAction* a = beginChange("Zeile einfügen", s);
		Row w;
		if (!rows.empty())
			w = rows[curRow], w.fill = false, w.on = 0xF, w.cvA = w.cvB = 0.f;
		w.slot = (int8_t)s;
		if (s >= 0 && slots[s].used) {
			w.barsIdx = (int8_t)barsIdxFor(slots[s].lengthIdx);
			if (slots[s].tag >= 0)
				w.sec = slots[s].tag;
			else
				slots[s].tag = w.sec;
		}
		int at = rows.empty() ? 0 : curRow + 1;
		rows.insert(rows.begin() + at, w);
		if (song.playing && song.row >= at)
			song.row++;
		if (song.playing && song.jumpRow >= at)
			song.jumpRow++;
		setRow(at);
		endChange(a);
		say("Zeile " + std::to_string(at + 1) + ": " + slotName(s));
	}
	void deleteRow() {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("Zeile löschen");
		int at = curRow;
		rows.erase(rows.begin() + at);
		if (song.playing) {
			if (song.row == at) { // go on with what came after it at the next bar line
				song.row = -1;
				song.jumpRow = at;
				song.jumpAt = barNow + 1;
			}
			else if (song.row > at)
				song.row--;
			if (song.jumpRow > at)
				song.jumpRow--;
		}
		setRow(std::min(at, (int)rows.size() - 1));
		endChange(a);
	}
	void toggleTrack(int t) {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("Spur an/aus");
		rows[curRow].on ^= (uint8_t)(1 << t);
		endChange(a);
	}
	void toggleFill() {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("Fill");
		rows[curRow].fill = !rows[curRow].fill;
		endChange(a);
	}
	void setRowCv(int r, bool b, float v) {
		if (r < 0 || r >= (int)rows.size())
			return;
		(b ? rows[r].cvB : rows[r].cvA) = v;
	}

	// ---- pads
	void padTap(int i, Genra* g) {
		if (i < 0 || i >= NUM_SLOTS)
			return;
		if (armed == 1) { // STORE
			if (!g) {
				say("Kein GENRA verbunden");
				return;
			}
			pendingStore = i;
			requestSnap(g);
			armed = 0;
			return;
		}
		if (armed == 2) { // COPY: source, then target
			if (copySrc < 0) {
				if (!slots[i].used) {
					say("Leerer Platz – Quelle wählen");
					return;
				}
				copySrc = i;
				say(slotName(i) + " → Ziel antippen");
				return;
			}
			AblaufAction* a = beginChange("Kopieren", i);
			slots[i] = slots[copySrc];
			endChange(a);
			say(slotName(copySrc) + " → " + slotName(i));
			armed = 0;
			copySrc = -1;
			selSlot = i;
			return;
		}
		if (armed == 3) { // CLEAR
			AblaufAction* a = beginChange("Löschen", i);
			slots[i] = Slot();
			endChange(a);
			say(slotName(i) + " gelöscht");
			armed = 0;
			return;
		}
		selSlot = i;
		if (!songMode() && slots[i].used && g)
			cuePattern(i, g);
	}
	void setTag(int i, int tag) {
		AblaufAction* a = beginChange("Abschnitt", i);
		slots[i].tag = (int8_t)tag;
		endChange(a);
	}
	void clearSlot(int i) {
		AblaufAction* a = beginChange("Löschen", i);
		slots[i] = Slot();
		endChange(a);
	}

	void requestSnap(Genra* g) {
		int e = 0;
		g->snapState.compare_exchange_strong(e, 1);
	}

	// pattern mode: the pad plays from the next bar line (at once if GENRA is stopped)
	bool cuePattern(int i, Genra* g) {
		if (!g->cueBegin()) {
			pendingPadCue = i;
			return false;
		}
		Genra::ArrCue& c = g->cue;
		c.slot = slots[i];
		c.mute = 0;
		c.fill = false;
		c.rowBars = 0;
		c.now = !g->running;
		c.stop = false;
		c.atBar = g->barPub.load() + 1;
		c.row = -1;
		c.tag = slots[i].tag;
		c.slotIdx = i;
		c.id = cueCounter++;
		g->cueCommit();
		pendingPadCue = -1;
		return true;
	}

	// ---- variants
	void part(int k, Genra* g) {
		if (!g) {
			say("Kein GENRA verbunden");
			return;
		}
		// still on one of this session's results (unchanged)? then the same source: "this pattern is great – an intro,
		// a break, ..." always starts from the great one, and a second press re-rolls the same pad
		bool same = false;
		if (session.active) {
			int playing = clamp(g->arrSlotPub.load(), -1, NUM_SLOTS - 1);
			for (int j = 0; j < NUM_SECTIONS; j++)
				if (session.result[j] >= 0 && session.result[j] == playing && slotVer[playing] == session.ver[j])
					same = true;
			if (same && g->patChanges.load() != g->patChangesAtCue.load())
				same = false; // GENRA's pattern was changed since (NEW, MUTATE, grid edits)
		}
		if (same) {
			makePart(k, g);
			return;
		}
		pendingPart = k;
		requestSnap(g);
	}
	void makePart(int k, Genra* g) {
		int target = session.result[k];
		if (target < 0 || slotVer[target] != session.ver[k] || !slots[target].used)
			target = freeSlot();
		if (target < 0) {
			say("Kein freier Platz");
			return;
		}
		AblaufAction* a = beginChange(SECTION_NAMES[k], target);
		makeSection(*session.source, slots[target], k, rerollMask(), session.P, rng);
		endChange(a);
		session.result[k] = target;
		session.ver[k] = slotVer[target];
		selSlot = target;
		params[BANK_PARAM].setValue((float)(target / PADS_PER_BANK));
		say(std::string(SECTION_NAMES[k]) + " → " + slotName(target));
		if (!song.playing)
			cuePattern(target, g);
	}

	// ---- song playback
	uint64_t rowSig(int r, int64_t at) {
		uint64_t h = 1469598103934665603ull;
		auto mix = [&](uint64_t v) { h = (h ^ v) * 1099511628211ull; };
		mix((uint64_t)(r + 1));
		mix((uint64_t)at);
		if (r >= 0) {
			const Row& w = rows[r];
			mix((uint64_t)w.slot + 7);
			mix((uint64_t)w.barsIdx);
			mix((uint64_t)w.sec);
			mix(w.on);
			mix(w.fill);
			if (w.slot >= 0)
				mix(slotVer[w.slot]);
		}
		return h;
	}
	bool cueRow(Genra* g, int r, int64_t at, bool now, bool stop) {
		if (!g->cueBegin())
			return false;
		if (song.waitId && g->cueDone.load() == song.waitId) {
			g->cueState.store(0); // ours was applied meanwhile: handle that first (next frame)
			return false;
		}
		Genra::ArrCue& c = g->cue;
		c.stop = stop;
		c.now = now;
		c.atBar = at;
		c.id = cueCounter++;
		if (!stop) {
			const Row& w = rows[r];
			c.slot = slots[w.slot];
			c.mute = (uint8_t)(~w.on & 0xF);
			c.fill = w.fill;
			c.rowBars = ROW_BARS[clamp((int)w.barsIdx, 0, 5)];
			c.row = (int8_t)r;
			c.tag = w.sec;
			c.slotIdx = w.slot;
		}
		g->cueCommit();
		song.waitId = c.id;
		song.cuedBars = stop ? 0 : c.rowBars;
		song.cuedRow = r;
		song.cuedStop = stop;
		song.cuedSig = rowSig(stop ? -1 : r, at);
		return true;
	}
	void startSong(Genra* g, int from) {
		int r = nextPlayable(from);
		if (r < 0)
			r = nextPlayable(0);
		if (r < 0) {
			say("Song ist leer – Pads mit EINF. eintragen");
			return;
		}
		g->cueCancel();
		song = Song();
		song.playing = true;
		if (!cueRow(g, r, 0, true, false)) {
			song.playing = false;
			pendingStart = from;
			return;
		}
		pendingStart = -1;
		g->arrRunReq = 1;
	}
	void stopSong(Genra* g) {
		if (g) {
			if (song.waitId)
				g->cueCancel();
			g->arrRunReq = 2;
		}
		song.playing = false;
		song.waitId = 0;
		song.row = -1;
	}
	void songStep(Genra* g) {
		if (!song.playing)
			return;
		if (!g->running && !song.waitId) {
			song.playing = false; // stopped on GENRA
			song.row = -1;
			return;
		}
		if (song.waitId && g->cueDone.load() != song.waitId) {
			// our cue gone without being applied (replaced or cancelled elsewhere)? then hand it over again
			int st = g->cueState.load();
			bool lost = st == 0 || (st == 1 && g->cue.id != song.waitId);
			if (lost && g->cueDone.load() != song.waitId)
				song.waitId = 0, song.cuedSig = 0;
		}
		if (song.waitId && g->cueDone.load() == song.waitId) {
			song.waitId = 0;
			if (song.cuedStop) {
				song.playing = false;
				song.row = -1;
				endTrig++;
				return;
			}
			if (song.cuedWrap)
				endTrig++;
			song.row = song.cuedRow;
			song.rowStart = g->rowStartPub.load();
			song.rowBars = song.cuedBars;
			song.jumpRow = -1;
			rowTrig++;
		}
		// what comes next, and when
		int nr;
		int64_t at;
		bool wrap = false;
		if (song.jumpRow >= 0) { // NEXT, ZEILE CV, or the playing row was deleted
			nr = nextPlayable(song.jumpRow);
			if (nr < 0 && loopOn())
				nr = nextPlayable(0), wrap = true;
			at = song.jumpAt;
		}
		else if (song.row >= 0 && song.row < (int)rows.size()) {
			nr = nextPlayable(song.row + 1);
			if (nr < 0 && loopOn())
				nr = nextPlayable(0), wrap = true;
			at = song.rowStart + ROW_BARS[clamp((int)rows[song.row].barsIdx, 0, 5)];
		}
		else
			return; // the first row has not started yet
		bool stop = nr < 0;
		uint64_t sig = rowSig(stop ? -1 : nr, at);
		if (song.waitId && sig == song.cuedSig)
			return; // already handed over
		if (cueRow(g, nr, at, false, stop))
			song.cuedWrap = wrap;
	}

	// the GENRA this ABLAUF holds offsets / mutes in; let go when the link changes or ABLAUF is removed
	Genra* attached = nullptr;
	static void letGo(Genra* g) {
		if (!g)
			return;
		std::lock_guard<std::mutex> lk(genraRegMutex());
		if (genraRegistry().count(g))
			g->arrAlive.store(1e-6f);
	}

	// what the widgets show (refreshed every frame)
	bool genraLinked = false, genraRunning = false;
	int64_t barNow = 0;
	int playingSlot = -1, queuedSlot = -1;
	uint8_t slotSilent[NUM_SLOTS] = {}; // bit per track: nothing plays on it (pad bars)
	uint32_t silentVer[NUM_SLOTS] = {};

	void updateSilent(int i) {
		silentVer[i] = slotVer[i];
		slotSilent[i] = 0;
		const Slot& s = slots[i];
		if (!s.used)
			return;
		Params P;
		P.scale = prof(s.pat.chordGenre).scale;
		P.lengthBars = LENGTH_BARS[clamp((int)s.lengthIdx, 0, 3)];
		P = withMods(P, s.mods);
		int L = P.lengthBars * STEPS_PER_BAR;
		bool any[4] = {false, false, false, false};
		int prev[4] = {60, 64, 67, 72};
		bool hp = false;
		for (int st = 0; st < L; st++) {
			Hit h[NUM_DRUMS];
			drumStep(s.pat, P, st, L, false, h);
			for (int v = 0; v < NUM_DRUMS; v++)
				any[0] |= h[v].on;
			any[1] |= bassAt(s.pat, P, st, L).on;
			any[2] |= melAt(s.pat, P, st, L).on;
			any[3] |= padAt(s.pat, P, st, L, prev, hp).on;
		}
		for (int t = 0; t < 4; t++)
			if (!any[t])
				slotSilent[i] |= (uint8_t)(1 << t);
	}

	void uiFrame(float dt) {
		Genra* g = genra();
		if (g != attached) {
			letGo(attached);
			attached = g;
			if (song.playing) // another GENRA: the song starts over there with PLAY
				song = Song();
			pendingPadCue = -1;
		}
		genraLinked = g != nullptr;
		genraRunning = g && g->running;
		barNow = g ? g->barPub.load() : 0;
		playingSlot = -1;
		queuedSlot = -1;
		if (g) {
			// the pad GENRA plays, as long as its pattern was not changed since (NEW, MUTATE, edits)
			if (g->patChanges.load() == g->patChangesAtCue.load())
				playingSlot = g->arrSlotPub.load();
			if (playingSlot >= NUM_SLOTS)
				playingSlot = -1;
			if (g->cueState.load() == 1 && !g->cue.stop)
				queuedSlot = g->cue.slotIdx;
			if (queuedSlot >= NUM_SLOTS)
				queuedSlot = -1;
		}
		for (int i = 0; i < NUM_SLOTS; i++)
			if (silentVer[i] != slotVer[i])
				updateSilent(i);
		uiStep(dt, g);
	}

	// ---- one frame
	void uiStep(float dt, Genra* g) {
		lights[LINK_LIGHT].setBrightness(g ? 1.f : 0.f);
		blink += dt * 2.5f;
		if (blink >= 1.f)
			blink -= 1.f;
		if (msgTime > 0.f)
			msgTime -= dt;
		if (g)
			g->arrAlive.store(1e9f); // attached; let go explicitly (letGo), not by a timeout: a stalled UI must not unmute

		// GENRA's snapshot arrived
		if (g && (pendingStore >= 0 || pendingPart >= 0) && g->snapState.load(std::memory_order_acquire) == 2) {
			if (pendingStore >= 0) {
				int i = pendingStore;
				AblaufAction* a = beginChange("Speichern", i);
				int8_t tag = slots[i].used ? slots[i].tag : -1;
				slots[i] = g->snap;
				slots[i].tag = tag;
				slots[i].used = true;
				endChange(a);
				selSlot = i;
				say(slotName(i) + " gespeichert");
				session.clear();
			}
			if (pendingPart >= 0) {
				session.clear();
				*session.source = g->snap;
				session.P = g->snapP;
				session.active = true;
				makePart(pendingPart, g);
			}
			pendingStore = pendingPart = -1;
			g->snapState.store(0);
		}
		if (pendingPadCue >= 0 && (songMode() || song.playing))
			pendingPadCue = -1;
		if (g && pendingPadCue >= 0)
			cuePattern(pendingPadCue, g);
		if (g && pendingStart >= 0)
			startSong(g, pendingStart);

		// buttons
		if (pressed(STORE_PARAM))
			armed = armed == 1 ? 0 : 1, copySrc = -1;
		if (pressed(COPY_PARAM))
			armed = armed == 2 ? 0 : 2, copySrc = -1;
		if (pressed(CLEAR_PARAM))
			armed = armed == 3 ? 0 : 3, copySrc = -1;
		for (int k = 0; k < NUM_SECTIONS; k++)
			if (pressed(PART_PARAM + k))
				part(k, g);
		if (pressed(INS_PARAM))
			insertRow();
		if (pressed(DEL_PARAM))
			deleteRow();
		if (pressed(UNDO_PARAM))
			undoButton();
		if (pressed(FILL_PARAM))
			toggleFill();
		for (int t = 0; t < 4; t++)
			if (pressed(TRACK_PARAM + t))
				toggleTrack(t);
		bindKnobs();

		// transport
		uint32_t rq = runReq.load(), rs = resetReq.load(), nx = nextReq.load();
		bool play = pressed(PLAY_PARAM), stop = pressed(STOP_PARAM);
		if (rq != runSeen) {
			runSeen = rq;
			bool on = songMode() ? song.playing : (g && g->running);
			(on ? stop : play) = true;
		}
		if (g) {
			if (play) {
				if (songMode())
					startSong(g, curRow);
				else {
					if (selSlot >= 0 && slots[selSlot].used) {
						g->cueCancel();
						cuePattern(selSlot, g);
					}
					g->arrRunReq = 1;
				}
			}
			if (stop)
				stopSong(g);
			if (rs != resetSeen) {
				resetSeen = rs;
				if (song.playing)
					startSong(g, 0);
				else
					setRow(0);
			}
			if (nx != nextSeen) {
				nextSeen = nx;
				if (song.playing && song.row >= 0) {
					song.jumpRow = song.row + 1 < (int)rows.size() ? song.row + 1 : 0;
					song.jumpAt = g->barPub.load() + 1;
				}
				else if (!songMode()) {
					int from = std::max(0, g->arrSlotPub.load());
					for (int k = 1; k <= NUM_SLOTS; k++) {
						int i = (from + k) % NUM_SLOTS;
						if (slots[i].used) {
							selSlot = i;
							cuePattern(i, g);
							break;
						}
					}
				}
			}
			// CV
			float rc = rowCv.load();
			if (rc >= 0.f && !rows.empty()) {
				int r = clamp((int)std::round(rc / 10.f * (rows.size() - 1)), 0, (int)rows.size() - 1);
				if (r != song.lastRowCv) {
					song.lastRowCv = r;
					if (song.playing) {
						song.jumpRow = r;
						song.jumpAt = g->barPub.load() + 1;
					}
					else
						setRow(r);
				}
			}
			else
				song.lastRowCv = -1;
			float pc = pattCv.load();
			if (pc >= 0.f && !songMode()) {
				int i = clamp((int)std::round(pc / 10.f * (NUM_SLOTS - 1)), 0, NUM_SLOTS - 1);
				if (i != song.lastPattCv) {
					song.lastPattCv = i;
					if (slots[i].used) {
						selSlot = i;
						cuePattern(i, g);
					}
				}
			}
			else
				song.lastPattCv = -1;
			if (song.playing && !songMode())
				stopSongScheduling(g);
			songStep(g);
		}
		else if (song.playing)
			song.playing = false;

		publish(g);
		updateLights();
	}
	// switched to pattern mode while the song ran: GENRA keeps playing, the list stops leading
	void stopSongScheduling(Genra* g) {
		if (song.waitId)
			g->cueCancel();
		song.playing = false;
		song.waitId = 0;
		song.row = -1;
	}

	void publish(Genra* g) {
		bool run = g && g->running;
		outRunning.store(run);
		outSec.store(g ? g->arrSecPub.load() : -1);
		float pos = 0.f, a = 0.f, b = 0.f;
		if (g && song.playing && song.row >= 0 && song.row < (int)rows.size()) {
			int total = 0, before = 0;
			for (int r = 0; r < (int)rows.size(); r++)
				if (playable(r)) {
					int n = ROW_BARS[clamp((int)rows[r].barsIdx, 0, 5)];
					total += n;
					if (r < song.row)
						before += n;
				}
			int64_t into = g->barPub.load() - song.rowStart;
			int n = ROW_BARS[clamp((int)rows[song.row].barsIdx, 0, 5)];
			into = clamp((int)into, 0, n);
			if (total > 0)
				pos = 10.f * (before + (float)into) / total;
			a = rows[song.row].cvA;
			b = rows[song.row].cvB;
		}
		outPos.store(pos);
		outCvA.store(a);
		outCvB.store(b);
	}

	void updateLights() {
		bool hasRow = !rows.empty();
		for (int t = 0; t < 4; t++)
			lights[TRACK_LIGHT + t].setBrightness(hasRow && (rows[curRow].on >> t) & 1 ? 1.f : 0.f);
		lights[FILL_LIGHT].setBrightness(hasRow && rows[curRow].fill ? 1.f : 0.f);
		lights[PLAY_LIGHT].setBrightness(song.playing ? 1.f : 0.f);
		lights[LOOP_LIGHT].setBrightness(loopOn() ? 1.f : 0.f);
		float bl = blink < 0.5f ? 1.f : 0.25f;
		lights[STORE_LIGHT].setBrightness(armed == 1 ? bl : (pendingStore >= 0 ? 1.f : 0.f));
		lights[COPY_LIGHT].setBrightness(armed == 2 ? bl : 0.f);
		lights[CLEAR_LIGHT].setBrightness(armed == 3 ? bl : 0.f);
		int sec = hasRow ? rows[curRow].sec : -1;
		for (int k = 0; k < NUM_SECTIONS; k++) {
			bool mine = session.active && session.result[k] >= 0;
			lights[PART_LIGHT + k].setBrightness(pendingPart == k ? bl : (k == sec ? 0.9f : (mine ? 0.55f : 0.22f)));
		}
		lights[INS_LIGHT].setBrightness(0.f);
		lights[DEL_LIGHT].setBrightness(0.f);
		lights[UNDO_LIGHT].setBrightness(0.f);
		lights[STOP_LIGHT].setBrightness(0.f);
	}

	// ------------------------------------------------------------ persistence
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "linkedId", json_integer(linkedId));
		json_object_set_new(root, "curRow", json_integer(curRow));
		json_object_set_new(root, "selSlot", json_integer(selSlot));
		json_t* sl = json_array();
		for (int i = 0; i < NUM_SLOTS; i++) {
			const Slot& s = slots[i];
			if (!s.used)
				continue;
			json_t* o = json_object();
			json_object_set_new(o, "i", json_integer(i));
			std::vector<uint8_t> b = savePattern(s.pat);
			json_object_set_new(o, "pattern3", json_string(string::toBase64(b.data(), b.size()).c_str()));
			json_t* sel = json_array();
			for (int t = 0; t < 4; t++)
				json_array_append_new(sel, json_integer(s.sel[t]));
			json_object_set_new(o, "trackGenre", sel);
			json_object_set_new(o, "length", json_integer(s.lengthIdx));
			json_t* d = json_array();
			for (int t = 0; t < 4; t++)
				json_array_append_new(d, json_real(s.mods.dens[t]));
			json_object_set_new(o, "dens", d);
			json_object_set_new(o, "ghost", json_real(s.mods.ghost));
			json_object_set_new(o, "fill", json_real(s.mods.fill));
			json_object_set_new(o, "tag", json_integer(s.tag));
			json_array_append_new(sl, o);
		}
		json_object_set_new(root, "slots", sl);
		json_t* rs = json_array();
		for (const Row& w : rows) {
			json_t* o = json_object();
			json_object_set_new(o, "slot", json_integer(w.slot));
			json_object_set_new(o, "bars", json_integer(w.barsIdx));
			json_object_set_new(o, "section", json_integer(w.sec));
			json_object_set_new(o, "tracks", json_integer(w.on));
			json_object_set_new(o, "fill", json_boolean(w.fill));
			json_object_set_new(o, "cvA", json_real(w.cvA));
			json_object_set_new(o, "cvB", json_real(w.cvB));
			json_array_append_new(rs, o);
		}
		json_object_set_new(root, "rows", rs);
		return root;
	}

	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "linkedId")))
			linkedId = json_integer_value(j);
		for (int i = 0; i < NUM_SLOTS; i++) {
			slots[i] = Slot();
			slotVer[i]++;
		}
		json_t* sl = json_object_get(root, "slots");
		for (size_t k = 0; sl && k < json_array_size(sl); k++) {
			json_t* o = json_array_get(sl, k);
			json_t* ji = json_object_get(o, "i");
			json_t* jp = json_object_get(o, "pattern3");
			if (!ji || !jp || !json_is_string(jp))
				continue;
			int i = (int)json_integer_value(ji);
			if (i < 0 || i >= NUM_SLOTS)
				continue;
			Slot& s = slots[i];
			if (!loadPattern(string::fromBase64(json_string_value(jp)), s.pat))
				continue;
			json_t* sel = json_object_get(o, "trackGenre");
			for (int t = 0; t < 4; t++) {
				json_t* v = sel ? json_array_get(sel, t) : nullptr;
				int p = v ? (int)json_integer_value(v) : -1;
				s.sel[t] = (int8_t)(p >= 0 && p < numProfiles() ? p : -1);
			}
			if ((j = json_object_get(o, "length")))
				s.lengthIdx = (int8_t)clamp((int)json_integer_value(j), 0, 3);
			json_t* d = json_object_get(o, "dens");
			for (int t = 0; t < 4; t++)
				if (json_t* v = d ? json_array_get(d, t) : nullptr)
					s.mods.dens[t] = (float)json_number_value(v);
			if ((j = json_object_get(o, "ghost")))
				s.mods.ghost = (float)json_number_value(j);
			if ((j = json_object_get(o, "fill")))
				s.mods.fill = (float)json_number_value(j);
			if ((j = json_object_get(o, "tag")))
				s.tag = (int8_t)clamp((int)json_integer_value(j), -1, NUM_SECTIONS - 1);
			s.used = true;
		}
		rows.clear();
		json_t* rs = json_object_get(root, "rows");
		for (size_t k = 0; rs && k < json_array_size(rs) && (int)rows.size() < MAX_ROWS; k++) {
			json_t* o = json_array_get(rs, k);
			Row w;
			if ((j = json_object_get(o, "slot")))
				w.slot = (int8_t)clamp((int)json_integer_value(j), -1, NUM_SLOTS - 1);
			if ((j = json_object_get(o, "bars")))
				w.barsIdx = (int8_t)clamp((int)json_integer_value(j), 0, 5);
			if ((j = json_object_get(o, "section")))
				w.sec = (int8_t)clamp((int)json_integer_value(j), 0, NUM_SECTIONS - 1);
			if ((j = json_object_get(o, "tracks")))
				w.on = (uint8_t)(json_integer_value(j) & 0xF);
			if ((j = json_object_get(o, "fill")))
				w.fill = json_boolean_value(j);
			if ((j = json_object_get(o, "cvA")))
				w.cvA = (float)json_number_value(j);
			if ((j = json_object_get(o, "cvB")))
				w.cvB = (float)json_number_value(j);
			rows.push_back(w);
		}
		session.clear();
		song = Song();
		if ((j = json_object_get(root, "selSlot")))
			selSlot = clamp((int)json_integer_value(j), -1, NUM_SLOTS - 1);
		curRow = 0;
		if ((j = json_object_get(root, "curRow")))
			curRow = (int)json_integer_value(j);
		lastZ = -1; // the knobs follow the row at the next frame
	}

	// removed from the rack: GENRA drops the offsets and mutes of the pattern it got from here
	void onRemove(const RemoveEvent& e) override {
		letGo(attached); // no engine calls here: Rack holds its lock while removing
		attached = nullptr;
		Module::onRemove(e);
	}

	void onReset(const ResetEvent& e) override {
		Module::onReset(e);
		for (int i = 0; i < NUM_SLOTS; i++) {
			slots[i] = Slot();
			slotVer[i]++;
		}
		rows.clear();
		session.clear();
		song = Song();
		curRow = 0;
		selSlot = -1;
		armed = 0;
		lastZ = -1;
	}
};

static Ablauf* findAblauf(int64_t id) {
	Module* m = APP->engine->getModule(id);
	return m ? dynamic_cast<Ablauf*>(m) : nullptr;
}
void AblaufAction::undo() {
	if (Ablauf* m = findAblauf(moduleId))
		m->restore(rowsBefore, curBefore, slot, slotBefore);
}
void AblaufAction::redo() {
	if (Ablauf* m = findAblauf(moduleId))
		m->restore(rowsAfter, curAfter, slot, slotAfter);
}

// ================================================================= widgets
static void roundRect(NVGcontext* vg, float x, float y, float w, float h, float r, NVGcolor c) {
	nvgBeginPath(vg);
	nvgRoundedRect(vg, x, y, w, h, r);
	nvgFillColor(vg, c);
	nvgFill(vg);
}
static NVGcolor mixc(NVGcolor a, NVGcolor b, float f) { return nvgLerpRGBA(b, a, f); }
static const NVGcolor PLATE = nvgRGB(0x21, 0x23, 0x29), DARK = nvgRGB(0x07, 0x08, 0x0a);

struct AbWidget : Widget {
	Ablauf* module = nullptr;
	void place(const float* a) {
		box.pos = mm2px(Vec(a[0], a[1]));
		box.size = mm2px(Vec(a[2], a[3]));
	}
	static float mm(float v) { return mm2px(v); }
};

/** The song list: one line per row; click selects, right-click edits CV A / CV B. */
struct SongList : AbWidget {
	int firstVisible = 0;
	static constexpr float ROWH = 4.6f, HEAD = 6.f;
	int visible() { return (int)((box.size.y / mm2px(1.f) - HEAD - 4.f) / ROWH); }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		NVGcontext* vg = args.vg;
		auto bold = ui::boldFont(), mono = ui::monoFont();
		float u = mm2px(1.f);
		float w = box.size.x / u;
		auto t = [&](float x, float y, const std::string& s, float size, NVGcolor c, int al = NVG_ALIGN_LEFT, float sp = 0.3f) {
			ui::text(vg, bold, x * u, y * u, s, size * u, c, al, sp * u);
		};
		t(2.5f, 3.6f, "#", 1.3f, ui::DIM);
		t(8.5f, 3.6f, "PATT", 1.3f, ui::DIM);
		t(21.f, 3.6f, "TAKTE", 1.3f, ui::DIM, NVG_ALIGN_CENTER);
		t(37.f, 3.6f, "SPUREN", 1.3f, ui::DIM, NVG_ALIGN_CENTER);
		t(53.f, 3.6f, "FILL", 1.3f, ui::DIM, NVG_ALIGN_CENTER);
		t(60.f, 3.6f, "ABSCHNITT", 1.3f, ui::DIM);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 1.5f * u, 5.f * u);
		nvgLineTo(vg, (w - 1.5f) * u, 5.f * u);
		nvgStrokeColor(vg, nvgRGB(0x25, 0x26, 0x2d));
		nvgStrokeWidth(vg, 0.25f * u);
		nvgStroke(vg);
		if (!module) {
			t(w / 2, 20.f, "ABLAUF", 2.f, ui::DIM, NVG_ALIGN_CENTER);
			Widget::drawLayer(args, layer);
			return;
		}
		Ablauf* m = module;
		int n = (int)m->rows.size();
		int vis = std::max(1, visible());
		// keep the selected row (and the playing one, if possible) in view
		int want = m->curRow;
		if (want < firstVisible)
			firstVisible = want;
		if (want >= firstVisible + vis)
			firstVisible = want - vis + 1;
		firstVisible = clamp(firstVisible, 0, std::max(0, n - vis));
		if (n == 0) {
			t(w / 2, 18.f, "Noch kein Song", 1.8f, ui::DIM, NVG_ALIGN_CENTER);
			t(w / 2, 23.f, "Pad antippen, dann EINF. – Zeile für Zeile", 1.4f, ui::DIM, NVG_ALIGN_CENTER);
			t(w / 2, 27.f, "Varianten: Pattern läuft, dann INTRO, BUILD, …", 1.4f, ui::DIM, NVG_ALIGN_CENTER);
		}
		for (int i = 0; i < vis && firstVisible + i < n; i++) {
			int r = firstVisible + i;
			const Row& row = m->rows[r];
			float y = HEAD + 1.f + i * ROWH;
			bool sel = r == m->curRow, play = m->song.playing && r == m->song.row;
			bool ok = m->playable(r);
			if (sel) {
				roundRect(vg, 1.f * u, (y - 0.2f) * u, (w - 2.f) * u, (ROWH - 0.2f) * u, 0.5f * u, nvgRGB(0x2e, 0x2a, 0x17));
				roundRect(vg, 1.f * u, (y - 0.2f) * u, 0.7f * u, (ROWH - 0.2f) * u, 0.3f * u, ACC);
			}
			if (play && m->genraRunning) {
				int n2 = ROW_BARS[clamp((int)row.barsIdx, 0, 5)];
				float f = clamp((float)(m->barNow - m->song.rowStart) / n2, 0.f, 1.f);
				roundRect(vg, 1.7f * u, (y + ROWH - 0.9f) * u, (w - 2.7f) * f * u, 0.35f * u, 0.15f * u, ACC);
				nvgBeginPath(vg); // play marker
				nvgMoveTo(vg, (w - 3.6f) * u, (y + 0.7f) * u);
				nvgLineTo(vg, (w - 3.6f) * u, (y + 3.1f) * u);
				nvgLineTo(vg, (w - 1.8f) * u, (y + 1.9f) * u);
				nvgClosePath(vg);
				nvgFillColor(vg, ACC);
				nvgFill(vg);
			}
			NVGcolor c = sel ? ACC : (ok ? ui::TXT : ui::DIM);
			t(2.5f, y + 3.f, string::f("%02d", r + 1), 1.55f, sel ? ACC : ui::DIM, NVG_ALIGN_LEFT, 0.2f);
			t(8.5f, y + 3.f, slotName(row.slot) + (ok ? "" : " –"), 1.7f, c, NVG_ALIGN_LEFT, 0.3f);
			t(21.f, y + 3.f, std::to_string(ROW_BARS[clamp((int)row.barsIdx, 0, 5)]), 1.7f, c, NVG_ALIGN_CENTER, 0.2f);
			for (int k = 0; k < 4; k++) {
				bool on = (row.on >> k) & 1;
				roundRect(vg, (30.4f + k * 3.6f) * u, (y + 0.8f) * u, 3.f * u, 2.6f * u, 0.4f * u,
				          on ? TRACK_COLORS[k] : nvgRGB(0x24, 0x25, 0x2c));
				t(31.9f + k * 3.6f, y + 2.75f, TRACK_LETTER[k], 1.25f, on ? DARK : nvgRGB(0x4a, 0x4c, 0x57), NVG_ALIGN_CENTER, 0.f);
			}
			if (row.fill) {
				nvgBeginPath(vg);
				nvgCircle(vg, 53.f * u, (y + 2.1f) * u, 0.9f * u);
				nvgFillColor(vg, ACC);
				nvgFill(vg);
			}
			int sec = clamp((int)row.sec, 0, NUM_SECTIONS - 1);
			roundRect(vg, 59.5f * u, (y + 0.8f) * u, 1.2f * u, 2.6f * u, 0.3f * u, SEC_COL[sec]);
			t(62.f, y + 3.f, SECTION_NAMES[sec], 1.5f, sel ? SEC_COL[sec] : ui::TXT, NVG_ALIGN_LEFT, 0.3f);
			if (row.cvA != 0.f || row.cvB != 0.f)
				t(w - 5.f, y + 3.f, "CV", 1.2f, ui::DIM, NVG_ALIGN_RIGHT, 0.2f);
		}
		// status line
		std::string st;
		if (m->song.playing && m->song.row >= 0) {
			int total = 0, done = 0;
			for (int r = 0; r < n; r++)
				if (m->playable(r)) {
					int b = ROW_BARS[clamp((int)m->rows[r].barsIdx, 0, 5)];
					total += b;
					if (r < m->song.row)
						done += b;
				}
			int into = clamp((int)(m->barNow - m->song.rowStart), 0, 99) + 1;
			st = string::f("TAKT %d / %d  ·  ZEILE %d / %d", done + into, total, m->song.row + 1, n);
		}
		else if (n)
			st = string::f("ZEILE %d / %d", m->curRow + 1, n);
		if (!st.empty())
			ui::text(vg, mono, (w - 2.5f) * u, (box.size.y / u - 1.2f) * u, st, 2.2f * u, ui::DIM, NVG_ALIGN_RIGHT, 0.f);
		if (n > vis) { // scroll bar
			float h = box.size.y / u - HEAD - 4.f;
			float y0 = HEAD + h * firstVisible / n, y1 = HEAD + h * (firstVisible + vis) / n;
			roundRect(vg, (w - 1.f) * u, y0 * u, 0.5f * u, (y1 - y0) * u, 0.25f * u, nvgRGB(0x3a, 0x3b, 0x44));
		}
		Widget::drawLayer(args, layer);
	}

	int rowAt(Vec pos) {
		float y = pos.y / mm2px(1.f) - HEAD - 1.f;
		if (y < 0.f || !module)
			return -1;
		int r = firstVisible + (int)(y / ROWH);
		return r < (int)module->rows.size() ? r : -1;
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS)
			return;
		int r = rowAt(e.pos);
		if (r < 0)
			return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->setRow(r);
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			module->setRow(r);
			openMenu(r);
			e.consume(this);
		}
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module)
			return;
		int d = e.scrollDelta.y > 0 ? -1 : 1;
		module->setRow(module->curRow + d);
		e.consume(this);
	}
	struct CvQuantity : Quantity {
		Ablauf* m;
		int row;
		bool b;
		float getValue() override {
			if (row >= (int)m->rows.size())
				return 0.f;
			return b ? m->rows[row].cvB : m->rows[row].cvA;
		}
		void setValue(float v) override { m->setRowCv(row, b, clamp(v, -10.f, 10.f)); }
		float getMinValue() override { return -10.f; }
		float getMaxValue() override { return 10.f; }
		float getDefaultValue() override { return 0.f; }
		std::string getLabel() override { return b ? "CV B" : "CV A"; }
		std::string getUnit() override { return " V"; }
		int getDisplayPrecision() override { return 3; }
	};
	void openMenu(int r) {
		Menu* menu = createMenu();
		menu->addChild(createMenuLabel(string::f("Zeile %d", r + 1)));
		for (int k = 0; k < 2; k++) {
			rack::ui::Slider* s = new rack::ui::Slider;
			CvQuantity* q = new CvQuantity;
			q->m = module;
			q->row = r;
			q->b = k == 1;
			s->quantity = q;
			s->box.size.x = 200.f;
			menu->addChild(s);
		}
		Ablauf* m = module;
		menu->addChild(createMenuItem("Zeile duplizieren", "", [=]() {
			int keep = m->selSlot;
			m->selSlot = m->rows[r].slot;
			m->setRow(r);
			m->insertRow();
			m->selSlot = keep;
		}));
		menu->addChild(createMenuItem("Zeile löschen", "", [=]() {
			m->setRow(r);
			m->deleteRow();
		}));
	}
};

/** Bank buttons A-D with a bar showing how full each bank is. */
struct BankButtons : AbWidget {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f);
			int cur = module ? module->bank() : 0;
			for (int k = 0; k < 4; k++) {
				float x = 4.f + k * 10.f;
				bool on = k == cur;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, (x - 4.f) * u, 0, 8.f * u, 4.6f * u, 0.7f * u);
				nvgFillColor(vg, on ? nvgRGB(0x3a, 0x35, 0x20) : nvgRGB(0x1a, 0x1b, 0x20));
				nvgFill(vg);
				nvgStrokeColor(vg, on ? ACC : nvgRGB(0x2c, 0x2d, 0x33));
				nvgStrokeWidth(vg, 0.4f * u);
				nvgStroke(vg);
				ui::text(vg, bold, x * u, 3.25f * u, std::string(1, (char)('A' + k)), 2.f * u, on ? ACC : ui::DIM, NVG_ALIGN_CENTER, 0.f);
				int used = 0;
				if (module)
					for (int i = 0; i < PADS_PER_BANK; i++)
						used += module->slots[k * PADS_PER_BANK + i].used;
				if (used)
					roundRect(vg, (x - 2.5f) * u, 5.4f * u, 5.f * used / PADS_PER_BANK * u, 0.5f * u, 0.2f * u, nvgRGBA(0xff, 0xd2, 0x3d, 0xb0));
			}
		}
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		int k = clamp((int)(e.pos.x / mm2px(10.f)), 0, 3);
		module->params[Ablauf::BANK_PARAM].setValue((float)k);
		e.consume(this);
	}
};

/** 4 x 4 pads of the current bank, tinted in the colour of their song part. */
struct Pads : AbWidget {
	float step = 9.7f, size = 8.6f;
	int padAt(Vec p) {
		float u = mm2px(1.f);
		int c = (int)(p.x / u / step), r = (int)(p.y / u / step);
		if (c < 0 || c > 3 || r < 0 || r > 3)
			return -1;
		if (p.x / u - c * step > size || p.y / u - r * step > size)
			return -1;
		return r * 4 + c;
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f);
			int bk = module ? module->bank() : 0;
			int playing = module && module->genraLinked ? module->playingSlot : -1;
			int queued = module ? module->queuedSlot : -1;
			for (int n = 0; n < PADS_PER_BANK; n++) {
				int i = bk * PADS_PER_BANK + n;
				float x = (n % 4) * step, y = (n / 4) * step;
				const Slot* s = module ? &module->slots[i] : nullptr;
				bool used = s && s->used;
				int tag = used ? s->tag : -1;
				NVGcolor fill = !used ? nvgRGB(0x1a, 0x1b, 0x20)
				                      : (tag >= 0 ? mixc(SEC_COL[tag], PLATE, i == playing ? 0.5f : 0.3f) : nvgRGB(0x2a, 0x2b, 0x33));
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x * u, y * u, size * u, size * u, 0.8f * u);
				nvgFillColor(vg, fill);
				nvgFill(vg);
				bool sel = module && i == module->selSlot;
				bool armedSrc = module && module->armed == 2 && i == module->copySrc;
				if (i == playing || i == queued || sel || armedSrc) {
					nvgStrokeColor(vg, i == playing ? nvgRGB(0xff, 0xff, 0xff)
					                                : (armedSrc ? ACC : (i == queued ? nvgRGB(0xff, 0xff, 0xff) : nvgRGBA(0xff, 0xff, 0xff, 0x70))));
					nvgStrokeWidth(vg, (i == playing ? 0.55f : 0.4f) * u);
					if (i == queued && i != playing) {
						// dashed: comes at the next bar line
						for (int d = 0; d < 16; d++) {
							float a = d / 16.f * 4.f, b2 = (d + 0.55f) / 16.f * 4.f;
							auto pt = [&](float f) {
								f = std::fmod(f, 4.f);
								int side = (int)f;
								float t2 = f - side;
								switch (side) {
									case 0: return Vec(x + t2 * size, y);
									case 1: return Vec(x + size, y + t2 * size);
									case 2: return Vec(x + size - t2 * size, y + size);
									default: return Vec(x, y + size - t2 * size);
								}
							};
							Vec p0 = pt(a), p1 = pt(b2);
							nvgBeginPath(vg);
							nvgMoveTo(vg, p0.x * u, p0.y * u);
							nvgLineTo(vg, p1.x * u, p1.y * u);
							nvgStroke(vg);
						}
					}
					else {
						nvgBeginPath(vg);
						nvgRoundedRect(vg, x * u, y * u, size * u, size * u, 0.8f * u);
						nvgStroke(vg);
					}
				}
				if (used && tag >= 0)
					roundRect(vg, (x + 0.6f) * u, (y + 0.6f) * u, (size - 1.2f) * u, 0.8f * u, 0.3f * u, SEC_COL[tag]);
				ui::text(vg, bold, (x + 1.f) * u, (y + 3.6f) * u, slotName(i), 1.35f * u,
				         used ? ui::TXT : nvgRGB(0x4a, 0x4c, 0x57), NVG_ALIGN_LEFT, 0.15f * u);
				if (used && tag >= 0)
					ui::text(vg, bold, (x + 1.f) * u, (y + 5.6f) * u, SECTION_NAMES[tag], 1.f * u, SEC_COL[tag], NVG_ALIGN_LEFT, 0.15f * u);
				float bw = (size - 2.f - 1.5f) / 4.f;
				for (int t = 0; t < 4; t++) {
					bool on = false;
					if (used) // a track with something in it
						on = s->pat.trackGenre[t] >= 0 && !(module->slotSilent[i] & (1 << t));
					roundRect(vg, (x + 1.f + t * (bw + 0.5f)) * u, (y + size - 1.9f) * u, bw * u, 0.8f * u, 0.3f * u,
					          on ? TRACK_COLORS[t] : nvgRGB(0x2f, 0x30, 0x38));
				}
			}
		}
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS)
			return;
		int n = padAt(e.pos);
		if (n < 0)
			return;
		int i = module->bank() * PADS_PER_BANK + n;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->padTap(i, module->genra());
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openMenu(i);
			e.consume(this);
		}
	}
	void openMenu(int i) {
		Ablauf* m = module;
		Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Pad " + slotName(i) + (m->slots[i].used ? "" : " (leer)")));
		if (!m->slots[i].used)
			return;
		menu->addChild(createSubmenuItem("Abschnitt", m->slots[i].tag >= 0 ? SECTION_NAMES[m->slots[i].tag] : "keiner", [=](Menu* sm) {
			sm->addChild(createCheckMenuItem("keiner", "", [=]() { return m->slots[i].tag < 0; }, [=]() { m->setTag(i, -1); }));
			for (int k = 0; k < NUM_SECTIONS; k++)
				sm->addChild(createCheckMenuItem(SECTION_NAMES[k], "", [=]() { return m->slots[i].tag == k; }, [=]() { m->setTag(i, k); }));
		}));
		menu->addChild(createMenuItem("In den Song einfügen (nach der gewählten Zeile)", "", [=]() {
			m->selSlot = i;
			m->insertRow();
		}));
		menu->addChild(createMenuItem("Löschen", "", [=]() { m->clearSlot(i); }));
	}
};

/** The line under the pads: the pattern playing, the one coming, or a short message. */
struct InfoLine : AbWidget {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1 && module) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f), w = box.size.x / u;
			Ablauf* m = module;
			if (m->msgTime > 0.f)
				ui::text(vg, bold, 1.6f * u, 3.7f * u, m->msg, 1.45f * u, ACC, NVG_ALIGN_LEFT, 0.25f * u);
			else if (!m->genraLinked)
				ui::text(vg, bold, 1.6f * u, 3.7f * u, "Neben GENRA setzen oder Rechtsklick › Verbinden", 1.3f * u, ui::DIM, NVG_ALIGN_LEFT, 0.15f * u);
			else {
				int p = m->playingSlot;
				if (p >= 0) {
					float x = 1.6f + ui::text(vg, bold, 1.6f * u, 3.7f * u, slotName(p), 1.45f * u, ui::TXT, NVG_ALIGN_LEFT, 0.25f * u) / u;
					int tag = m->slots[p].used ? m->slots[p].tag : -1;
					if (tag >= 0)
						ui::text(vg, bold, (x + 1.5f) * u, 3.7f * u, SECTION_NAMES[tag], 1.45f * u, SEC_COL[tag], NVG_ALIGN_LEFT, 0.25f * u);
				}
				else
					ui::text(vg, bold, 1.6f * u, 3.7f * u, m->armed == 1 ? "STORE: Pad antippen" : "GENRA spielt frei", 1.4f * u,
					         m->armed == 1 ? ACC : ui::DIM, NVG_ALIGN_LEFT, 0.25f * u);
				if (m->armed == 2)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, m->copySrc < 0 ? "COPY: Quelle" : "COPY: Ziel", 1.4f * u, ACC, NVG_ALIGN_RIGHT, 0.25f * u);
				else if (m->armed == 3)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, "CLEAR: Pad", 1.4f * u, ACC, NVG_ALIGN_RIGHT, 0.25f * u);
				else if (m->queuedSlot >= 0)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, "> " + slotName(m->queuedSlot), 1.45f * u, ui::TXT, NVG_ALIGN_RIGHT, 0.25f * u);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

/** NEU WÜRFELN D B M P: which tracks a variant may change. */
struct RerollBoxes : AbWidget {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f);
			for (int t = 0; t < 4; t++) {
				bool on = !module || module->params[Ablauf::REROLL_PARAM + t].getValue() > 0.5f;
				roundRect(vg, t * 4.6f * u, 0, 3.6f * u, 3.2f * u, 0.5f * u, on ? TRACK_COLORS[t] : nvgRGB(0x24, 0x25, 0x2c));
				ui::text(vg, bold, (t * 4.6f + 1.8f) * u, 2.25f * u, TRACK_LETTER[t], 1.3f * u, on ? DARK : nvgRGB(0x4a, 0x4c, 0x57),
				         NVG_ALIGN_CENTER, 0.f);
			}
		}
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		int t = clamp((int)(e.pos.x / mm2px(4.6f)), 0, 3);
		Param& p = module->params[Ablauf::REROLL_PARAM + t];
		p.setValue(p.getValue() > 0.5f ? 0.f : 1.f);
		e.consume(this);
	}
};

/** The six colours around the ABSCHNITT knob and the part's name below it. */
struct SectionRing : AbWidget {
	bool name = false;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			float u = mm2px(1.f);
			int cur = module ? clamp((int)std::round(module->params[Ablauf::ABSCHNITT_PARAM].getValue()), 0, NUM_SECTIONS - 1) : SEC_DROP;
			if (name) {
				ui::text(vg, ui::boldFont(), box.size.x / 2, 2.55f * u, SECTION_NAMES[cur], 1.6f * u, SEC_COL[cur], NVG_ALIGN_CENTER, 0.35f * u);
			}
			else {
				float cx = box.size.x / 2, cy = box.size.y / 2;
				for (int k = 0; k < NUM_SECTIONS; k++) {
					float a = (-135.f + k * 54.f) * (float)M_PI / 180.f;
					nvgBeginPath(vg);
					nvgCircle(vg, cx + 6.6f * u * std::sin(a), cy - 6.6f * u * std::cos(a), (k == cur ? 0.95f : 0.7f) * u);
					NVGcolor c = SEC_COL[k];
					c.a = k == cur ? 1.f : 0.4f;
					nvgFillColor(vg, c);
					nvgFill(vg);
				}
				// the knob's ring in the part's colour
				nvgBeginPath(vg);
				nvgCircle(vg, cx, cy, 4.72f * u);
				NVGcolor c = SEC_COL[cur];
				c.a = 0.85f;
				nvgStrokeColor(vg, c);
				nvgStrokeWidth(vg, 0.3f * u);
				nvgStroke(vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

template <int R, int G, int B>
using ALight = ui::AccentLight<R, G, B>;

/** PATTERN / TAKTE / ABSCHNITT: a turn is one undo step of the row it changed (not Rack's ParamChange, whose undo
 * would land on whatever row is selected then). */
template <class K>
struct RowKnob : K {
	AblaufAction* pending = nullptr;
	void onDragStart(const event::DragStart& e) override {
		Ablauf* m = dynamic_cast<Ablauf*>(this->module);
		if (m && e.button == GLFW_MOUSE_BUTTON_LEFT && !pending)
			pending = m->beginChange("Zeile ändern", m->rows.empty() ? -1 : m->rows[m->curRow].slot);
		K::onDragStart(e);
	}
	void onDragEnd(const event::DragEnd& e) override {
		K::onDragEnd(e);
		Ablauf* m = dynamic_cast<Ablauf*>(this->module);
		if (!m || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		history::State* h = APP->history;
		if (h->actionIndex > 0 && h->actionIndex == (int)h->actions.size()) {
			history::ParamChange* pc = dynamic_cast<history::ParamChange*>(h->actions.back());
			if (pc && pc->moduleId == m->id && pc->paramId == this->paramId) {
				delete pc;
				h->actions.pop_back();
				h->actionIndex--;
			}
		}
		if (pending) {
			m->bindKnobs(); // the last value into the row before the snapshot
			bool changed = !(pending->rowsBefore.size() == m->rows.size() &&
			                 std::equal(m->rows.begin(), m->rows.end(), pending->rowsBefore.begin()));
			if (changed)
				m->endChange(pending);
			else
				delete pending;
			pending = nullptr;
		}
	}
};

struct AblaufWidget : ModuleWidget {
	AblaufWidget(Ablauf* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Ablauf.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		using A = Ablauf;

		auto add = [&](AbWidget* w, const float* a) {
			w->module = module;
			w->place(a);
			addChild(w);
		};
		add(new SongList, L::SONGLIST);
		add(new BankButtons, L::BANKS);
		add(new Pads, L::PADS);
		add(new InfoLine, L::INFO);
		add(new RerollBoxes, L::REROLL);
		SectionRing* ring = new SectionRing;
		add(ring, L::SECRING);
		SectionRing* nm = new SectionRing;
		nm->name = true;
		add(nm, L::SECNAME);

		addParam(createParamCentered<GKnob>(ui::mm(L::ZEILE), module, A::ZEILE_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::PATTERN), module, A::PATTERN_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::TAKTE), module, A::TAKTE_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::ABSCHNITT), module, A::ABSCHNITT_PARAM));

		const float* tr[4] = {L::TRACK0, L::TRACK1, L::TRACK2, L::TRACK3};
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x7b, 0x6c, 0xff>>>(ui::mm(tr[0]), module, A::TRACK_PARAM + 0, A::TRACK_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xb8, 0xf2, 0x3a>>>(ui::mm(tr[1]), module, A::TRACK_PARAM + 1, A::TRACK_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x3d, 0xc9, 0xff>>>(ui::mm(tr[2]), module, A::TRACK_PARAM + 2, A::TRACK_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xe0, 0x77, 0x9e>>>(ui::mm(tr[3]), module, A::TRACK_PARAM + 3, A::TRACK_LIGHT + 3));

		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::INS), module, A::INS_PARAM, A::INS_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::DEL), module, A::DEL_PARAM, A::DEL_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::UNDO), module, A::UNDO_PARAM, A::UNDO_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xff, 0xd2, 0x3d>>>(ui::mm(L::FILL), module, A::FILL_PARAM, A::FILL_LIGHT));
		ui::LampRow* mode = createParam<ui::LampRow>(Vec(), module, A::MODE_PARAM);
		mode->accent = ACC;
		const float mp[3] = {L::MODE[0], L::MODE[1], 7.5f};
		mode->place(mp, 2);
		addParam(mode);
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x5d, 0xff, 0x8a>>>(ui::mm(L::PLAY), module, A::PLAY_PARAM, A::PLAY_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::STOP), module, A::STOP_PARAM, A::STOP_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<ALight<0xff, 0xd2, 0x3d>>>(ui::mm(L::LOOP), module, A::LOOP_PARAM, A::LOOP_LIGHT));

		const float* pp[NUM_SECTIONS] = {L::PART0, L::PART1, L::PART2, L::PART3, L::PART4, L::PART5};
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x6f, 0x86, 0xb8>>>(ui::mm(pp[0]), module, A::PART_PARAM + 0, A::PART_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x9b, 0x7b, 0xd4>>>(ui::mm(pp[1]), module, A::PART_PARAM + 1, A::PART_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xe0, 0xa5, 0x26>>>(ui::mm(pp[2]), module, A::PART_PARAM + 2, A::PART_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xff, 0x5a, 0x4f>>>(ui::mm(pp[3]), module, A::PART_PARAM + 3, A::PART_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x3f, 0xae, 0x9a>>>(ui::mm(pp[4]), module, A::PART_PARAM + 4, A::PART_LIGHT + 4));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x8a, 0x8d, 0x99>>>(ui::mm(pp[5]), module, A::PART_PARAM + 5, A::PART_LIGHT + 5));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(ui::mm(L::STORE), module, A::STORE_PARAM, A::STORE_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::COPY), module, A::COPY_PARAM, A::COPY_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::CLEAR), module, A::CLEAR_PARAM, A::CLEAR_LIGHT));
		addChild(createLightCentered<SmallLight<ALight<0x5d, 0xff, 0x8a>>>(ui::mm(L::LINKLED), module, A::LINK_LIGHT));

		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_RUN), module, A::RUN_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_RESET), module, A::RESET_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_NEXT), module, A::NEXT_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_PATT), module, A::PATT_INPUT));
		addInput(createInputCentered<PJ301MPort>(ui::mm(L::IN_ROW), module, A::ROW_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_END), module, A::END_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_ROW), module, A::ROW_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_BUILD), module, A::BUILD_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_DROP), module, A::DROP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_POS), module, A::POS_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_CVA), module, A::CVA_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(ui::mm(L::OUT_CVB), module, A::CVB_OUTPUT));
	}

	void step() override {
		if (Ablauf* m = dynamic_cast<Ablauf*>(module))
			m->uiFrame(APP->window->getLastFrameDuration());
		ModuleWidget::step();
	}

	void appendContextMenu(Menu* menu) override {
		Ablauf* m = dynamic_cast<Ablauf*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Verbinden mit…"));
		menu->addChild(createCheckMenuItem("Automatisch (angedocktes GENRA)", "", [=]() { return m->linkedId < 0; }, [=]() { m->linkedId = -1; }));
		int n = 0;
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* mm = APP->engine->getModule(id);
			if (!mm || mm->model != modelGenra)
				continue;
			n++;
			std::string label = string::f("GENRA #%d", n);
			menu->addChild(createCheckMenuItem(label, "", [=]() { return m->linkedId == id; }, [=]() { m->linkedId = id; }));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Song leeren", "", [=]() {
			AblaufAction* a = m->beginChange("Song leeren");
			m->rows.clear();
			m->rowsReplaced();
			m->setRow(0);
			m->endChange(a);
		}));
		menu->addChild(createMenuLabel("Pattern speichern: STORE, dann Pad · Song: Pad antippen, dann EINF."));
		menu->addChild(createMenuLabel("Varianten: Pattern läuft, dann INTRO … OUTRO (nochmal = neu würfeln)"));
	}
};

} // namespace genra_ablauf

Model* modelAblauf = createModel<genra_ablauf::Ablauf, genra_ablauf::AblaufWidget>("GenraAblauf");
