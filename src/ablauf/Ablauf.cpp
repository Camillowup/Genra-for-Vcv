// SPDX-License-Identifier: GPL-3.0-only
// ABLAUF – song arranger for GENRA: 128 pattern places (4 banks of 8 x 4 pads, each with a name and the colour of the
// GENRA original it comes from), a song list (pattern, bars, part, fill per row), one button per song part that turns the
// pattern playing now into an intro, a build, ... (Arranger.cpp), MAKE SONG (a whole song in a genre's typical form), MIDI
// export of pads and of the song; connected to GENRA like the grid editor (docked, or right-click › Connect to…).
//
// Threads: everything that edits the bank or the song runs on the UI thread (ModuleWidget::step -> Ablauf::uiStep),
// like the grid editor. GENRA takes patterns over through its cue mailbox and applies them on its audio thread at a bar
// line, so the timing is exact. The audio thread of ABLAUF only counts button presses and trigger inputs and drives
// the outputs from values the UI thread publishes.
#include "../genra/GenraModule.hpp"
#include "../ui/GenraUI.hpp"
#include "../ui/BigPad.hpp"
#include "AblaufLayout.hpp"
#include <deque>

namespace genra_ablauf {

namespace L = ablayout;
namespace ui = genra_ui;

static const int NUM_SLOTS = 128, MAX_ROWS = 64, PADS_PER_BANK = 32, PAD_COLS = 8;
static const int SAVE_VERSION = 2; // 1 (no field): 4 banks of 16 pads; 2: 4 banks of 32
static const int ROW_BARS[6] = {1, 2, 4, 8, 16, 32};
static const NVGcolor ACC = nvgRGB(0xff, 0xd2, 0x3d);
// song parts (keep in step with SECS in tools/panels/ablauf_panel.py)
static const NVGcolor SEC_COL[NUM_SECTIONS] = {nvgRGB(0x6f, 0x86, 0xb8), nvgRGB(0x9b, 0x7b, 0xd4), nvgRGB(0xe0, 0xa5, 0x26),
                                               nvgRGB(0xff, 0x5a, 0x4f), nvgRGB(0x3f, 0xae, 0x9a), nvgRGB(0x8a, 0x8d, 0x99)};
static const char* const TRACK_LETTER[4] = {"D", "B", "M", "P"};
static const char* const TRACK_FILE[4] = {"drums", "bass", "melody", "pads"};
// family colours: the GENRA original a pad comes from (GENRA n -> colour n-1), apart from part and track colours
static const NVGcolor FAMILY_COL[8] = {nvgRGB(0xff, 0x9d, 0x2e), nvgRGB(0xe8, 0xe8, 0xee), nvgRGB(0x3f, 0xd0, 0xc9),
                                       nvgRGB(0xc8, 0xe6, 0x4a), nvgRGB(0xff, 0x5f, 0xd2), nvgRGB(0x6a, 0xa8, 0xff),
                                       nvgRGB(0xd8, 0xa0, 0x50), nvgRGB(0xb0, 0x8c, 0xff)};
static const NVGcolor NO_FAMILY = nvgRGB(0x4a, 0x4c, 0x57);
static NVGcolor familyColor(int f) { return f > 0 ? FAMILY_COL[(f - 1) % 8] : NO_FAMILY; }
// the number in a name like "INTRO 12" / "GENRA 3" (after base + space), 0 if it has none
static int nameNumber(const char* name, const char* base) {
	size_t n = std::strlen(base);
	if (std::strncmp(name, base, n) != 0 || name[n] != ' ')
		return 0;
	return std::max(0, std::atoi(name + n + 1));
}
static int rowBarsIdx(int bars) {
	for (int k = 5; k >= 0; k--)
		if (ROW_BARS[k] <= bars)
			return k;
	return 0;
}

static std::string slotName(int i) {
	if (i < 0 || i >= NUM_SLOTS)
		return "–";
	return string::f("%c%d", 'A' + i / PADS_PER_BANK, i % PADS_PER_BANK + 1);
}
// an old patch's slot index (4 banks of 16) -> today's (4 banks of 32): A1–A16, B1–B16 … keep their labels
static int slotFromV1(int i) { return i < 0 ? -1 : (i / 16) * PADS_PER_BANK + i % 16; }

// pad names: up to 8 characters, A–Z 0–9 space and '-'
static const int NAME_LEN = 8;
static std::string cleanName(const std::string& in) {
	std::string o;
	for (char c : in) {
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-')
			o += c;
		if ((int)o.size() >= NAME_LEN)
			break;
	}
	while (!o.empty() && o.back() == ' ')
		o.pop_back();
	size_t a = o.find_first_not_of(' ');
	return a == std::string::npos ? "" : o.substr(a);
}
static void setName(Slot& s, const std::string& n) {
	std::string c = cleanName(n);
	std::memset(s.name, 0, sizeof(s.name));
	std::memcpy(s.name, c.data(), std::min(c.size(), sizeof(s.name) - 1));
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
		CLEARALL_PARAM, // (appended: saved patches keep their ids)
		MAKESONG_PARAM,
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
		CLEARALL_LIGHT,
		MAKESONG_LIGHT,
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
	PartOpts partOpts; // the bass styles of BREAK / INTRO (-1: one at random on every press)
	int songForm = -1;  // MAKE SONG: SongFormId, -1 = AUTO (the form typical for the pattern's genre)
	int dragSlot = -1;  // a pad being dragged into the song list
	float bpmNow = 120.f;
	Rng rng{random::u64()};
	std::string msg;
	float msgTime = 0.f;
	float blink = 0.f;
	float stopFlash = 0.f; // STOP pad: a short flash when pressed

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
		int result[NUM_SECTIONS];                  // the last pad each part went to (lights)
		std::vector<std::pair<int, uint32_t>> made; // every pad made from this source, and its version then
		Session() { clear(); }
		void clear() {
			active = false;
			made.clear();
			for (int k = 0; k < NUM_SECTIONS; k++)
				result[k] = -1;
		}
	} session;

	// song playback
	struct Song {
		bool playing = false;
		int row = -1;          // row playing now (-1: waiting for the first)
		int64_t rowStart = 0;  // GENRA's bar index where it began
		uint32_t waitId = 0;   // cue handed over, not applied yet
		int cuedRow = -1;
		bool cuedStop = false, cuedWrap = false, cuedJump = false;
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
					s += " (empty)";
				return s;
			}
		};
		configParam<RowQuantity>(ZEILE_PARAM, 0.f, MAX_ROWS - 1, 0.f, "Row");
		configParam<SlotQuantity>(PATTERN_PARAM, 0.f, NUM_SLOTS - 1, 0.f, "Row pattern");
		configSwitch(TAKTE_PARAM, 0.f, 5.f, 2.f, "Row length", {"1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars"});
		configSwitch(ABSCHNITT_PARAM, 0.f, NUM_SECTIONS - 1, SEC_DROP, "Row part",
		             {"Intro", "Verse", "Build", "Drop", "Break", "Outro"});
		for (int p : {ZEILE_PARAM, PATTERN_PARAM})
			paramQuantities[p]->snapEnabled = true;
		static const char* TN[4] = {"Drums", "Bass", "Melody", "Pads"};
		for (int t = 0; t < 4; t++) {
			configButton(TRACK_PARAM + t, std::string(TN[t]) + " on / off in this row");
			configSwitch(REROLL_PARAM + t, 0.f, 2.f, 1.f, std::string("Variations: ") + TN[t],
			             {"Keep", "Re-roll", "Vary (a part that comes again: its last variant, a little changed)"});
		}
		configButton(INS_PARAM, "Insert row (with the selected pad)");
		configButton(DEL_PARAM, "Delete row");
		configButton(UNDO_PARAM, "Undo");
		configButton(FILL_PARAM, "Fill at the end of the row on / off");
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Mode", {"Pattern (play pads live)", "Song (play the list)"});
		configButton(PLAY_PARAM, "Play (from the selected row; while playing: start again from there)");
		configButton(STOP_PARAM, "Stop (press again: back to the start)");
		configSwitch(LOOP_PARAM, 0.f, 1.f, 1.f, "Loop song", {"Off (stop at the end)", "On"});
		configSwitch(BANK_PARAM, 0.f, 3.f, 0.f, "Bank", {"A", "B", "C", "D"});
		static const char* PN[NUM_SECTIONS] = {"Intro", "Verse", "Build", "Drop", "Break", "Outro"};
		for (int k = 0; k < NUM_SECTIONS; k++)
			configButton(PART_PARAM + k, std::string(PN[k]) + " from the playing pattern (each press: a new variant on the next free pad)");
		configButton(STORE_PARAM, "Store (right-click a pad › Overwrite with GENRA)");
		configButton(COPY_PARAM, "COPY: tap source, then target");
		configButton(CLEAR_PARAM, "CLEAR: then a pad – clears the slot");
		configButton(CLEARALL_PARAM, "CLR ALL: press twice – clears all 128 pads (UNDO brings them back)");
		configButton(MAKESONG_PARAM, "Make song from the selected pad (in the SONG FORM next to it; UNDO takes it back)");
		for (int i = 0; i < PARAMS_LEN; i++)
			paramQuantities[i]->randomizeEnabled = false;
		configInput(RUN_INPUT, "Run (trigger: play / stop)");
		configInput(RESET_INPUT, "Reset (song to the start)");
		configInput(NEXT_INPUT, "Next row / next pattern (trigger, at the next bar)");
		configInput(PATT_INPUT, "Pattern CV (0–10 V across all 128 pads, pattern mode)");
		configInput(ROW_INPUT, "Row CV (0–10 V across the song list, jumps at the next bar)");
		configOutput(END_OUTPUT, "End (trigger at song end and on loop)");
		configOutput(ROW_OUTPUT, "Row (trigger on every new row)");
		configOutput(BUILD_OUTPUT, "Build (gate while a build plays)");
		configOutput(DROP_OUTPUT, "Drop (gate while a drop plays)");
		configOutput(POS_OUTPUT, "Song position (0–10 V)");
		configOutput(CVA_OUTPUT, "Row CV A");
		configOutput(CVB_OUTPUT, "Row CV B");
	}

	// ------------------------------------------------------------ audio thread
	void process(const ProcessArgs& args) override {
		for (int p : {(int)INS_PARAM, (int)DEL_PARAM, (int)UNDO_PARAM, (int)FILL_PARAM, (int)PLAY_PARAM, (int)STOP_PARAM,
		              (int)STORE_PARAM, (int)COPY_PARAM, (int)CLEAR_PARAM, (int)CLEARALL_PARAM, (int)MAKESONG_PARAM})
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
	bool genraBusy = false; // the GENRA found is driven by another ABLAUF
	Genra* findGenra() {
		if (linkedId >= 0) {
			Module* m = APP->engine->getModule(linkedId);
			if (m && isGenraModel(m->model))
				return static_cast<Genra*>(m);
		}
		Module* side[2] = {leftExpander.module, rightExpander.module};
		for (Module* m : side)
			if (m && isGenraModel(m->model))
				return static_cast<Genra*>(m);
		return nullptr;
	}
	// UI thread. One ABLAUF per GENRA: two would take each other's cues and snapshots
	Genra* genra() {
		genraBusy = false;
		Genra* g = findGenra();
		if (!g)
			return nullptr;
		int64_t own = g->arrOwner.load();
		if (own == id)
			return g;
		if (own >= 0) {
			Module* o = APP->engine->getModule(own);
			if (o && (o->model == modelAblauf || o->model == modelAblaufMini)) {
				genraBusy = true;
				return nullptr;
			}
		}
		if (g->arrOwner.compare_exchange_strong(own, id))
			return g;
		genraBusy = true;
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
	unsigned rerollMask() { // Re-roll and Vary (Vary rolls a part that has no variant yet)
		unsigned m = 0;
		for (int t = 0; t < 4; t++)
			if (params[REROLL_PARAM + t].getValue() > 0.5f)
				m |= 1u << t;
		return m;
	}
	unsigned varyMask() {
		unsigned m = 0;
		for (int t = 0; t < 4; t++)
			if (params[REROLL_PARAM + t].getValue() > 1.5f)
				m |= 1u << t;
		return m;
	}
	/** The latest variant of part k in a family (the highest number: BREAK 2 before BREAK 1), skip excluded; -1 none. */
	int lastVariant(int k, int family, int skip) {
		int best = -1, top = -1;
		if (family < 0)
			return -1;
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (i == skip || !slots[i].used || slots[i].family != family || slots[i].tag != k)
				continue;
			int n = nameNumber(slots[i].name, SECTION_NAMES[k]);
			if (n > top) {
				top = n;
				best = i;
			}
		}
		return best;
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
	bool allEmpty() {
		for (int k = 0; k < NUM_SLOTS; k++)
			if (slots[k].used)
				return false;
		return true;
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

	// empty the pads [from, to) as one undo step; the song list stays (rows on empty pads are skipped)
	float clrAllArm = 0.f; // CLR ALL pressed once: a second press within this time clears
	void clearSlots(int from, int to, const char* name) {
		history::ComplexAction* ca = new history::ComplexAction;
		ca->name = std::string("ABLAUF: ") + name;
		for (int i = std::max(0, from); i < std::min(to, (int)NUM_SLOTS); i++) {
			if (!slots[i].used)
				continue;
			AblaufAction* a = new AblaufAction;
			a->name = ca->name;
			a->moduleId = id;
			a->rowsBefore = a->rowsAfter = rows;
			a->curBefore = a->curAfter = curRow;
			a->slot = i;
			a->slotBefore = std::make_shared<Slot>(slots[i]);
			slots[i] = Slot();
			a->slotAfter = std::make_shared<Slot>(slots[i]);
			slotVer[i]++;
			ca->push(a);
		}
		if (ca->actions.empty()) {
			delete ca;
			say("No pads to clear");
			return;
		}
		if (selSlot >= from && selSlot < to)
			selSlot = -1;
		session.clear();
		APP->history->push(ca);
		say(std::to_string(ca->actions.size()) + " pads cleared – UNDO brings them back");
	}

	void undoButton() {
		history::State* h = APP->history;
		if (h->actionIndex > 0) {
			history::Action* last = h->actions[h->actionIndex - 1];
			if (history::ComplexAction* ca = dynamic_cast<history::ComplexAction*>(last))
				if (!ca->actions.empty())
					last = ca->actions.front();
			AblaufAction* a = dynamic_cast<AblaufAction*>(last);
			if (a && a->moduleId == id) {
				h->undo();
				say("Undo");
				return;
			}
		}
		say(h->canUndo() ? "Last step was not ABLAUF (Ctrl+Z)" : "Nothing to undo");
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
		int s = selSlot >= 0 ? selSlot : (rows.empty() ? -1 : rows[curRow].slot);
		insertRowAt(rows.empty() ? 0 : curRow + 1, s);
	}
	/** A new row at position at (0..rows) with pad s: length and part from the pad. */
	void insertRowAt(int at, int s) {
		if ((int)rows.size() >= MAX_ROWS) {
			say("64 rows at most");
			return;
		}
		at = clamp(at, 0, (int)rows.size());
		AblaufAction* a = beginChange("insert row", s);
		Row w;
		if (!rows.empty())
			w = rows[clamp(at - 1, 0, (int)rows.size() - 1)], w.fill = false, w.on = 0xF, w.cvA = w.cvB = 0.f;
		w.slot = (int8_t)s;
		if (s >= 0 && slots[s].used) {
			w.barsIdx = (int8_t)barsIdxFor(slots[s].lengthIdx);
			if (slots[s].tag >= 0)
				w.sec = slots[s].tag;
			else
				slots[s].tag = w.sec;
		}
		rows.insert(rows.begin() + at, w);
		if (song.playing && song.row >= at)
			song.row++;
		if (song.playing && song.jumpRow >= at)
			song.jumpRow++;
		if (song.playing && song.waitId && !song.cuedStop && song.cuedRow >= at)
			song.cuedRow++;
		setRow(at);
		endChange(a);
		say("Row " + std::to_string(at + 1) + ": " + slotName(s));
	}
	/** Row r plays pad s from now on (dropped onto the row). */
	void replaceRowSlot(int r, int s) {
		if (r < 0 || r >= (int)rows.size() || s < 0 || s >= NUM_SLOTS || !slots[s].used)
			return;
		AblaufAction* a = beginChange("replace pattern", s);
		rows[r].slot = (int8_t)s;
		if (slots[s].tag < 0)
			slots[s].tag = rows[r].sec;
		setRow(r);
		endChange(a);
		say("Row " + std::to_string(r + 1) + ": " + slotName(s));
	}
	void deleteRow() {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("delete row");
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
			if (song.waitId && !song.cuedStop && song.cuedRow > at)
				song.cuedRow--;
		}
		setRow(std::min(at, (int)rows.size() - 1));
		endChange(a);
	}
	void toggleTrack(int t) {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("track on/off");
		rows[curRow].on ^= (uint8_t)(1 << t);
		endChange(a);
	}
	void toggleFill() {
		if (rows.empty())
			return;
		AblaufAction* a = beginChange("fill");
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
				say("No GENRA connected");
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
					say("Empty slot – pick a source");
					return;
				}
				copySrc = i;
				say(slotName(i) + " → tap target");
				return;
			}
			AblaufAction* a = beginChange("copy", i);
			slots[i] = slots[copySrc];
			endChange(a);
			say(slotName(copySrc) + " → " + slotName(i));
			armed = 0;
			copySrc = -1;
			selSlot = i;
			return;
		}
		if (armed == 3) { // CLEAR
			AblaufAction* a = beginChange("clear", i);
			slots[i] = Slot();
			endChange(a);
			say(slotName(i) + " cleared");
			armed = 0;
			return;
		}
		if (!slots[i].used) { // empty pad: takes what GENRA is playing right now (no STORE needed)
			if (!g) {
				say("No GENRA connected");
				return;
			}
			if (pendingStore >= 0 || pendingPart >= 0)
				return; // a snapshot is already on its way
			selSlot = i;
			pendingStore = i;
			requestSnap(g);
			return;
		}
		selSlot = i;
		if (!songMode() && g)
			cuePattern(i, g);
	}
	/** Right-click › Overwrite with GENRA: the pad takes what GENRA plays now, keeps its name, part and family. */
	void overwrite(int i, Genra* g) {
		if (i < 0 || i >= NUM_SLOTS)
			return;
		if (!g) {
			say("No GENRA connected");
			return;
		}
		if (pendingStore >= 0 || pendingPart >= 0)
			return;
		selSlot = i;
		pendingStore = i;
		requestSnap(g);
	}
	/** A new family number: one past every family and every "GENRA n" name in use. */
	int newFamily() {
		int top = 0;
		for (int i = 0; i < NUM_SLOTS; i++)
			if (slots[i].used)
				top = std::max(top, std::max((int)slots[i].family, nameNumber(slots[i].name, "GENRA")));
		return std::min(top + 1, 127);
	}
	void rename(int i, const std::string& n) {
		if (i < 0 || i >= NUM_SLOTS || !slots[i].used)
			return;
		std::string c = cleanName(n);
		if (c == slots[i].name)
			return;
		AblaufAction* a = beginChange(c.empty() ? "clear name" : "rename", i);
		setName(slots[i], c);
		endChange(a);
		say(slotName(i) + (c.empty() ? ": name cleared" : " → " + c));
	}
	void setTag(int i, int tag) {
		AblaufAction* a = beginChange("part", i);
		slots[i].tag = (int8_t)tag;
		endChange(a);
	}
	void clearSlot(int i) {
		AblaufAction* a = beginChange("clear", i);
		slots[i] = Slot();
		endChange(a);
	}

	void requestSnap(Genra* g) {
		int e = 0;
		if (!g->snapState.compare_exchange_strong(e, 1) && e == 2) {
			e = 2; // a finished snapshot nobody took: ask for a fresh one
			g->snapState.compare_exchange_strong(e, 1);
		}
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
			say("No GENRA connected");
			return;
		}
		// still on one of this session's variants (unchanged)? then the same source: "this pattern is great – an intro,
		// a break, ..." always starts from the great one; every press writes a new variant on the next free pad
		bool same = false;
		if (session.active) {
			int playing = clamp(g->arrSlotPub.load(), -1, NUM_SLOTS - 1);
			for (auto& mv : session.made)
				if (playing >= 0 && mv.first == playing && slotVer[playing] == mv.second)
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
	/** A variant's name: the part and the next free number of that part (INTRO 1, INTRO 2, BREAK 1 ...). */
	std::string partName(int k, int skip, int family) { return nextName(SECTION_NAMES[k], skip, family); }
	/** base + the next free number among the pads of one family (skip: the pad being written). Every family counts
	 * on its own: INTRO 1, INTRO 2 from GENRA 1, again INTRO 1 from GENRA 2. */
	std::string nextName(const std::string& base, int skip, int family) {
		int top = 0;
		for (int i = 0; i < NUM_SLOTS; i++) {
			if (i == skip || !slots[i].used || slots[i].family != family)
				continue;
			top = std::max(top, nameNumber(slots[i].name, base.c_str()));
		}
		return base + " " + std::to_string(top + 1);
	}
	void makePart(int k, Genra* g) {
		int target = freeSlot(); // the next free pad: from this bank on, then the following banks
		if (target < 0) {
			say("All 128 pads are full");
			return;
		}
		AblaufAction* a = beginChange(SECTION_NAMES[k], target);
		makeSection(*session.source, slots[target], k, rerollMask(), session.P, rng, partOpts);
		// VARY: a part that has a variant already comes again from that one, a little changed
		int prev = varyMask() ? lastVariant(k, slots[target].family, target) : -1;
		if (prev >= 0)
			varySection(slots[prev], slots[target], varyMask(), session.P, rng);
		setName(slots[target], partName(k, target, slots[target].family));
		endChange(a);
		session.result[k] = target;
		session.made.push_back({target, slotVer[target]});
		selSlot = target;
		params[BANK_PARAM].setValue((float)(target / PADS_PER_BANK));
		say(std::string(SECTION_NAMES[k]) + " → " + slotName(target));
		if (!song.playing)
			cuePattern(target, g);
	}

	// ---- MAKE SONG
	/** The pad a family started with (the GENRA original), or i itself. */
	int familySource(int i) {
		if (i < 0 || i >= NUM_SLOTS || !slots[i].used)
			return -1;
		int f = slots[i].family;
		if (f >= 0)
			for (int k = 0; k < NUM_SLOTS; k++)
				if (slots[k].used && slots[k].family == f && slots[k].tag < 0)
					return k;
		return i;
	}
	static int slotGenre(const Slot& s) {
		int g = s.pat.trackGenre[T_DRUMS];
		return clamp(g, 0, numProfiles() - 1);
	}
	/** The form MAKE SONG uses now: the chosen one, or (AUTO) the one typical for the selected pad's genre; -1: none yet. */
	int effectiveForm() {
		if (songForm >= 0 && songForm < NUM_SONG_FORMS)
			return songForm;
		int src = familySource(selSlot);
		return src < 0 ? -1 : songFormForGenre(prof(slotGenre(slots[src])).name);
	}
	/** GENRA's knobs as a slot plays them (offsets, length, its native scale unless GENRA's SCALE overrides it). */
	static Params slotParams(Params P, const Slot& s) {
		P.lengthBars = LENGTH_BARS[clamp((int)s.lengthIdx, 0, 3)];
		if (!P.scaleOverride)
			P.scale = prof(s.pat.chordGenre).scale;
		P.fillActive = false;
		P.pass = 0;
		return withMods(P, s.mods);
	}
	/** The selected pad becomes a whole song: every part of the form from the family's existing variants (a part that
	 * comes twice takes another one) or made new like the part buttons; replaces the song list, one undo step. */
	void makeSong(Genra* g) {
		int sel = selSlot;
		if (sel < 0 || sel >= NUM_SLOTS || !slots[sel].used) {
			say("Tap a pad with a pattern first");
			return;
		}
		int src = familySource(sel);
		int fam = slots[src].family;
		int form = clamp(effectiveForm(), 0, NUM_SONG_FORMS - 1);
		const SongForm& F = SONG_FORMS[form];
		// the family's variants, part by part, lowest number first
		std::vector<int> have[NUM_SECTIONS];
		if (fam >= 0)
			for (int i = 0; i < NUM_SLOTS; i++)
				if (i != src && slots[i].used && slots[i].family == fam && slots[i].tag >= 0 && slots[i].tag < NUM_SECTIONS)
					have[slots[i].tag].push_back(i);
		for (int k = 0; k < NUM_SECTIONS; k++)
			std::stable_sort(have[k].begin(), have[k].end(), [&](int a, int b) {
				return nameNumber(slots[a].name, SECTION_NAMES[k]) < nameNumber(slots[b].name, SECTION_NAMES[k]);
			});
		// GENRA's knobs (density, complexity ...) as the part buttons use them; parts are made from the stored original
		Params P;
		if (g)
			P = g->buildParams(false);
		history::ComplexAction* ca = new history::ComplexAction;
		ca->name = "ABLAUF: make song";
		int taken[NUM_SECTIONS] = {};
		int fallback = 0;
		bool full = false;
		std::vector<Row> made;
		for (int k = 0; k < F.n; k++) {
			int sec = clamp(F.sec[k], 0, NUM_SECTIONS - 1);
			int slot = -1;
			if (taken[sec] < (int)have[sec].size())
				slot = have[sec][taken[sec]];
			else if (g && !full) {
				int t = freeSlot();
				if (t < 0)
					full = true;
				else { // a new variant, like the part button, on the next free pad
					AblaufAction* a = new AblaufAction;
					a->name = ca->name;
					a->moduleId = id;
					a->rowsBefore = a->rowsAfter = rows;
					a->curBefore = a->curAfter = curRow;
					a->slot = t;
					a->slotBefore = std::make_shared<Slot>(slots[t]);
					makeSection(slots[src], slots[t], sec, rerollMask(), slotParams(P, slots[src]), rng, partOpts);
					// VARY: the part's second time comes from its first, a little changed (not a new roll)
					if (varyMask() && !have[sec].empty())
						varySection(slots[have[sec].back()], slots[t], varyMask(), slotParams(P, slots[src]), rng);
					setName(slots[t], partName(sec, t, slots[t].family));
					a->slotAfter = std::make_shared<Slot>(slots[t]);
					slotVer[t]++;
					ca->push(a);
					have[sec].push_back(t);
					slot = t;
				}
			}
			if (slot < 0)
				slot = src, fallback++; // no GENRA to make it, or the pads are full: the original plays there
			taken[sec]++;
			Row w;
			w.slot = (int8_t)slot;
			w.sec = (int8_t)sec;
			int bars = F.bars[k];
			if (sec == SEC_BUILD && prof(slotGenre(slots[src])).buildBars > 0) // 2.30: the genre's build length
				bars = prof(slotGenre(slots[src])).buildBars;
			w.barsIdx = (int8_t)rowBarsIdx(bars);
			w.on = 0xF;
			made.push_back(w);
		}
		// a fill leads out of a verse or a drop into the next, different part (the build and the break have their own
		// endings: the roll, the pickup)
		for (size_t k = 0; k + 1 < made.size(); k++)
			if ((made[k].sec == SEC_VERSE || made[k].sec == SEC_DROP) && made[k + 1].sec != made[k].sec)
				made[k].fill = true;
		AblaufAction* ra = new AblaufAction;
		ra->name = ca->name;
		ra->moduleId = id;
		ra->rowsBefore = rows;
		ra->curBefore = curRow;
		ra->slot = -1;
		rows = made;
		rowsReplaced();
		setRow(0);
		ra->rowsAfter = rows;
		ra->curAfter = curRow;
		ca->push(ra);
		APP->history->push(ca);
		session.clear();
		std::string m = string::f("Song made: %s, %d bars", F.name, songFormBars(form));
		if (fallback)
			m += full ? string::f(" – pads full: %d parts play the original", fallback)
			          : string::f(" – no GENRA: %d parts play the original", fallback);
		say(m);
		msgTime = 4.f;
	}

	/** GENRA's MAKE SONG pad: the running loop becomes a song. A loop that is a pad unchanged is taken from there;
	 * otherwise it is stored on the next free pad first (as "GENRA n"), then MAKE SONG runs. */
	bool songAfterStore = false;
	void makeSongFromGenra(Genra* g) {
		if (!g)
			return;
		if (pendingStore >= 0 || pendingPart >= 0)
			return; // a snapshot is on its way
		int pl = g->arrSlotPub.load();
		if (pl >= 0 && pl < NUM_SLOTS && slots[pl].used && g->patChanges.load() == g->patChangesAtCue.load()) {
			selSlot = pl;
			makeSong(g);
			return;
		}
		int t = freeSlot();
		if (t < 0) {
			say("Pads full – clear a pad for MAKE SONG");
			return;
		}
		selSlot = t;
		pendingStore = t;
		songAfterStore = true;
		requestSnap(g);
	}

	// ---- MIDI files (UI thread)
	float bpmFor(Genra* g) { return g ? clamp(g->currentBpm(), 20.f, 400.f) : 120.f; }
	/** Into the MIDI export folder, no dialog (a save dialog can stall the audio driver). */
	bool saveMidi(std::string name, const std::vector<uint8_t>& data) {
		std::string err, file = midifolder::save(name, data, &err);
		say(file.empty() ? "Saving failed: " + err : "Saved: " + file);
		return !file.empty();
	}
	/** Pad i as a MIDI file: all four tracks (track -1) or one (0 drums ... 3 pads). */
	void exportPad(int i, int track) {
		if (i < 0 || i >= NUM_SLOTS || !slots[i].used)
			return;
		Genra* g = genra();
		const Slot& s = slots[i];
		Params P = slotParams(g ? g->buildParams(false) : Params(), s);
		int L = P.lengthBars * STEPS_PER_BAR;
		std::vector<MidiNote> notes = renderLoop(s.pat, P, L);
		if (track >= 0)
			notes.erase(std::remove_if(notes.begin(), notes.end(), [=](const MidiNote& n) { return n.track != track; }), notes.end());
		std::string name = slotName(i);
		if (s.name[0])
			name += std::string("-") + s.name;
		if (track >= 0 && track < 4)
			name += std::string("-") + TRACK_FILE[track];
		saveMidi(name, writeSmf(notes, bpmFor(g), prof(slotGenre(s)).name, L, &s.pat));
	}
	/** The whole song as one MIDI file: every row in order, its length, fill and silent tracks; a marker per row. */
	void exportSong(int track = -1) {
		Genra* g = genra();
		Params base = g ? g->buildParams(false) : Params();
		std::vector<SongPart> parts;
		for (int r = 0; r < (int)rows.size(); r++) {
			if (!playable(r))
				continue;
			const Row& w = rows[r];
			const Slot& s = slots[w.slot];
			SongPart sp;
			sp.pat = &s.pat;
			sp.P = slotParams(base, s);
			sp.L = sp.P.lengthBars * STEPS_PER_BAR;
			sp.bars = ROW_BARS[clamp((int)w.barsIdx, 0, 5)];
			sp.mute = (uint8_t)(~w.on & 0xF);
			sp.fill = w.fill;
			sp.label = s.name[0] ? std::string(s.name) : slotName(w.slot) + " " + SECTION_NAMES[clamp((int)w.sec, 0, NUM_SECTIONS - 1)];
			parts.push_back(sp);
		}
		if (parts.empty()) {
			say("Song is empty – nothing to export");
			return;
		}
		std::vector<SongMarker> marks;
		std::vector<MidiNote> notes = renderSong(parts, &marks);
		if (track >= 0)
			notes.erase(std::remove_if(notes.begin(), notes.end(), [=](const MidiNote& n) { return n.track != track; }), notes.end());
		int total = 0;
		for (const SongPart& sp : parts)
			total += sp.bars * STEPS_PER_BAR;
		std::string name = "song";
		if (track >= 0 && track < 4)
			name += std::string("-") + TRACK_FILE[track];
		saveMidi(name, writeSmf(notes, bpmFor(g), "Song", total, nullptr, &marks));
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
			say("Song is empty – add pads with INSERT");
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
		if (!g->running && song.row >= 0 && !(song.waitId && g->cueDone.load() == song.waitId)) { // stopped on GENRA itself: the marker stays on the row that played
			if (song.waitId)
				g->cueCancel();
			setRow(song.row);
			song.playing = false;
			song.waitId = 0;
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
			if (song.cuedJump || song.jumpRow == song.cuedRow)
				song.jumpRow = -1; // (a regular next row does not cancel a NEXT / ROW CV jump asked for meanwhile)
			if (song.row >= (int)rows.size()) { // rows deleted while it waited: go on after it
				song.row = -1;
				song.jumpRow = (int)rows.size();
				song.jumpAt = song.rowStart + song.rowBars;
			}
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
		bool jump = song.jumpRow >= 0;
		if (cueRow(g, nr, at, false, stop))
			song.cuedWrap = wrap, song.cuedJump = jump;
	}

	// the GENRA this ABLAUF holds offsets / mutes in; let go when the link changes or ABLAUF is removed
	Genra* attached = nullptr;
	static void letGo(Genra* g, int64_t me) {
		if (!g)
			return;
		std::lock_guard<std::mutex> lk(genraRegMutex());
		if (!genraRegistry().count(g))
			return;
		int64_t own = me;
		if (!g->arrOwner.compare_exchange_strong(own, -1) && own >= 0)
			return; // another ABLAUF's GENRA: leave it alone
		g->cueCancel(); // a cue left waiting would still mute tracks or stop GENRA at its bar line
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

	bool remapGenraSlot = false; // loaded from an old patch: GENRA still names the pad it plays the old way
	void uiFrame(float dt) {
		Genra* g = genra();
		if (g && remapGenraSlot) {
			remapGenraSlot = false;
			int o = g->arrSlotPub.load();
			if (o >= 0 && o < 64)
				g->arrSlotPub.store(slotFromV1(o));
		}
		if (g != attached) {
			letGo(attached, id);
			attached = g;
			if (song.playing) // another GENRA: the song starts over there with PLAY
				song = Song();
			pendingPadCue = -1;
			pendingStore = pendingPart = -1; // a snapshot asked of the old GENRA
		}
		genraLinked = g != nullptr;
		genraRunning = g && g->running;
		bpmNow = bpmFor(g);
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
		if (g) {
			// tell the GRID which pad GENRA plays; it may ask to store GENRA there (Overwrite with GENRA)
			int pl = g->arrSlotPub.load();
			bool known = pl >= 0 && pl < NUM_SLOTS && slots[pl].used;
			g->arrPadSlot = known ? pl : -1;
			g->arrPadLabel = known ? slotName(pl) + (slots[pl].name[0] ? std::string(" · ") + slots[pl].name : std::string()) : std::string();
			if (g->padSaveReq >= 0) {
				int rq = g->padSaveReq;
				g->padSaveReq = -1;
				if (known && rq == pl)
					overwrite(rq, g);
			}
		}

		// GENRA's snapshot arrived
		if (g && (pendingStore >= 0 || pendingPart >= 0) && g->snapState.load(std::memory_order_acquire) == 2) {
			if (pendingStore >= 0) {
				int i = pendingStore;
				AblaufAction* a = beginChange("store", i);
				bool was = slots[i].used;
				int8_t tag = was ? slots[i].tag : -1;
				// a pattern from GENRA on an empty pad starts a family: GENRA 1, GENRA 2 ... in their own colour;
				// a pad stored over (Overwrite with GENRA) keeps its name, part and family
				int8_t fam = was ? slots[i].family : (int8_t)newFamily();
				std::string keep = was ? std::string(slots[i].name) : "GENRA " + std::to_string(fam);
				slots[i] = g->snap;
				slots[i].tag = tag;
				slots[i].family = fam;
				slots[i].used = true;
				setName(slots[i], keep);
				endChange(a);
				selSlot = i;
				say(slotName(i) + " stored");
				session.clear();
				if (songAfterStore) {
					// GENRA plays this pad now: a second MAKE SONG takes it from here instead of storing it again
					g->arrSlotPub.store(i);
					g->patChangesAtCue.store(g->patChanges.load());
				}
			}
			if (pendingPart >= 0) {
				std::string src;
				int8_t fam = -1;
				int pl = g->patChanges.load() == g->patChangesAtCue.load() ? g->arrSlotPub.load() : -1;
				if (pl >= 0 && pl < NUM_SLOTS && slots[pl].used)
					src = slots[pl].name, fam = slots[pl].family;
				session.clear();
				*session.source = g->snap;
				setName(*session.source, src);
				session.source->family = fam; // variants belong to the family of the pattern they come from
				session.P = g->snapP;
				session.active = true;
				makePart(pendingPart, g);
			}
			pendingStore = pendingPart = -1;
			g->snapState.store(0);
			if (songAfterStore) {
				songAfterStore = false;
				makeSong(g);
			}
		}
		if (g && g->makeSongReq.exchange(false))
			makeSongFromGenra(g);
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
		if (clrAllArm > 0.f)
			clrAllArm -= dt;
		if (pressed(CLEARALL_PARAM)) {
			if (clrAllArm > 0.f) {
				clrAllArm = 0.f;
				clearSlots(0, NUM_SLOTS, "clear all pads");
			}
			else {
				clrAllArm = 3.f;
				armed = 0, copySrc = -1;
				say("CLR ALL: press again");
			}
		}
		for (int k = 0; k < NUM_SECTIONS; k++)
			if (pressed(PART_PARAM + k))
				part(k, g);
		if (pressed(MAKESONG_PARAM))
			makeSong(g);
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
		if (stop)
			stopFlash = 1.f;
		if (stop) {
			// STOP: the marker stays on the row that was playing; STOP again (already stopped): back to the start
			bool was = song.playing || (g && g->running);
			int at = song.playing ? song.row : -1;
			if (g)
				stopSong(g);
			else
				song.playing = false, song.row = -1;
			if (!was) {
				setRow(0);
				say("Back to the start");
			}
			else if (at >= 0)
				setRow(at);
			stop = false;
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
					song.jumpRow = song.row + 1; // past the last row: the end (LOOP decides, END fires)
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
		updateLights(g);
	}
	// switched to pattern mode while the song ran: GENRA keeps playing, the list stops leading
	void stopSongScheduling(Genra* g) {
		if (song.waitId)
			g->cueCancel();
		song.playing = false;
		song.waitId = 0;
		song.row = -1;
	}

	float heldPos = 0.f, heldA = 0.f, heldB = 0.f;
	bool heldOut = false;
	void publish(Genra* g) {
		bool run = g && g->running;
		outRunning.store(run);
		outSec.store(g ? g->arrSecPub.load() : -1);
		float pos = 0.f, a = 0.f, b = 0.f;
		if (g && song.playing && song.row < 0 && heldOut) { // between an undo and the next row: keep what played
			pos = heldPos, a = heldA, b = heldB;
		}
		else if (g && song.playing && song.row >= 0 && song.row < (int)rows.size()) {
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
			heldPos = pos, heldA = a, heldB = b, heldOut = true;
		}
		if (!song.playing)
			heldOut = false;
		outPos.store(pos);
		outCvA.store(a);
		outCvB.store(b);
	}

	void updateLights(Genra* g) {
		bool hasRow = !rows.empty();
		for (int t = 0; t < 4; t++)
			lights[TRACK_LIGHT + t].setBrightness(hasRow && (rows[curRow].on >> t) & 1 ? 1.f : 0.f);
		lights[FILL_LIGHT].setBrightness(hasRow && rows[curRow].fill ? 1.f : 0.f);
		// PLAY: lit while the song runs (SONG) or GENRA plays (PATT)
		lights[PLAY_LIGHT].setBrightness(song.playing || (!songMode() && g && g->running) ? 1.f : 0.f);
		lights[LOOP_LIGHT].setBrightness(loopOn() ? 1.f : 0.f);
		float bl = blink < 0.5f ? 1.f : 0.25f;
		lights[STORE_LIGHT].setBrightness(armed == 1 ? bl : (pendingStore >= 0 ? 1.f : 0.f));
		lights[COPY_LIGHT].setBrightness(armed == 2 ? bl : 0.f);
		lights[CLEAR_LIGHT].setBrightness(armed == 3 ? bl : 0.f);
		lights[CLEARALL_LIGHT].setBrightness(clrAllArm > 0.f ? bl : 0.f);
		lights[MAKESONG_LIGHT].setBrightness(selSlot >= 0 && selSlot < NUM_SLOTS && slots[selSlot].used ? 0.9f : 0.f);
		int sec = hasRow ? rows[curRow].sec : -1;
		for (int k = 0; k < NUM_SECTIONS; k++) {
			bool mine = session.active && session.result[k] >= 0;
			lights[PART_LIGHT + k].setBrightness(pendingPart == k ? bl : (k == sec ? 0.9f : (mine ? 0.55f : 0.22f)));
		}
		lights[INS_LIGHT].setBrightness(0.f);
		lights[DEL_LIGHT].setBrightness(0.f);
		lights[UNDO_LIGHT].setBrightness(0.f);
		stopFlash = std::max(0.f, stopFlash - 0.08f);
		lights[STOP_LIGHT].setBrightness(stopFlash);
	}

	// ------------------------------------------------------------ persistence
	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "version", json_integer(SAVE_VERSION));
		json_object_set_new(root, "linkedId", json_integer(linkedId));
		json_object_set_new(root, "breakBass", json_integer(partOpts.breakBass));
		json_object_set_new(root, "introBass", json_integer(partOpts.introBass));
		json_object_set_new(root, "songForm", json_integer(songForm));
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
			if (s.name[0])
				json_object_set_new(o, "name", json_string(s.name));
			if (s.family >= 0)
				json_object_set_new(o, "family", json_integer(s.family));
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
		partOpts = PartOpts();
		if ((j = json_object_get(root, "breakBass")))
			partOpts.breakBass = clamp((int)json_integer_value(j), -1, NUM_BREAK_BASS - 1);
		if ((j = json_object_get(root, "introBass")))
			partOpts.introBass = clamp((int)json_integer_value(j), -1, NUM_INTRO_BASS - 1);
		songForm = -1;
		if ((j = json_object_get(root, "songForm")))
			songForm = clamp((int)json_integer_value(j), -1, NUM_SONG_FORMS - 1);
		json_t* jv = json_object_get(root, "version");
		bool v1 = !jv || json_integer_value(jv) < 2; // 4 banks of 16 pads
		auto fromSaved = [&](int i) { return v1 ? slotFromV1(i) : i; };
		remapGenraSlot = v1;
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
			if (i < 0 || i >= (v1 ? 64 : NUM_SLOTS))
				continue;
			i = fromSaved(i);
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
			if ((j = json_object_get(o, "name")) && json_is_string(j))
				setName(s, json_string_value(j));
			s.family = -1;
			if ((j = json_object_get(o, "family")))
				s.family = (int8_t)clamp((int)json_integer_value(j), -1, 127);
			else if (int n = nameNumber(s.name, "GENRA")) // older patches: a "GENRA n" pad starts family n
				s.family = (int8_t)std::min(n, 127);
			s.used = true;
		}
		rows.clear();
		json_t* rs = json_object_get(root, "rows");
		for (size_t k = 0; rs && k < json_array_size(rs) && (int)rows.size() < MAX_ROWS; k++) {
			json_t* o = json_array_get(rs, k);
			Row w;
			if ((j = json_object_get(o, "slot")))
				w.slot = (int8_t)clamp(fromSaved(clamp((int)json_integer_value(j), -1, v1 ? 63 : NUM_SLOTS - 1)), -1, NUM_SLOTS - 1);
			if ((j = json_object_get(o, "bars")))
				w.barsIdx = (int8_t)clamp((int)json_integer_value(j), 0, 5);
			if ((j = json_object_get(o, "section")))
				w.sec = (int8_t)clamp((int)json_integer_value(j), 0, NUM_SECTIONS - 1);
			if ((j = json_object_get(o, "tracks")))
				w.on = (uint8_t)(json_integer_value(j) & 0xF);
			if ((j = json_object_get(o, "fill")))
				w.fill = json_boolean_value(j);
			if ((j = json_object_get(o, "cvA")))
				w.cvA = clamp((float)json_number_value(j), -10.f, 10.f);
			if ((j = json_object_get(o, "cvB")))
				w.cvB = clamp((float)json_number_value(j), -10.f, 10.f);
			rows.push_back(w);
		}
		session.clear();
		song = Song();
		if ((j = json_object_get(root, "selSlot")))
			selSlot = clamp(fromSaved(clamp((int)json_integer_value(j), -1, v1 ? 63 : NUM_SLOTS - 1)), -1, NUM_SLOTS - 1);
		curRow = 0;
		if ((j = json_object_get(root, "curRow")))
			curRow = clamp((int)json_integer_value(j), 0, std::max(0, (int)rows.size() - 1));
		lastZ = -1; // the knobs follow the row at the next frame
	}

	// removed from the rack: GENRA drops the offsets and mutes of the pattern it got from here
	void onRemove(const RemoveEvent& e) override {
		letGo(attached, id); // no engine calls here: Rack holds its lock while removing
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
		partOpts = PartOpts();
		songForm = -1;
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

/** The song list: one line per row; click selects, right-click edits CV A / CV B; a pad dragged in is inserted between
 * rows or replaces the pattern of the row it is dropped on. */
struct SongList : AbWidget {
	int firstVisible = 0;
	bool dragOver = false;
	float dragY = 0.f; // mm, in the list
	/** Where a pad dropped at y (mm) goes: insert at pos (0..rows) or replace row pos. */
	void dropTarget(float y, bool& insert, int& pos) {
		int n = module ? (int)module->rows.size() : 0;
		float f = (y - HEAD - 1.f) / ROWH;
		int idx = firstVisible + (int)std::floor(f);
		float frac = f - std::floor(f);
		insert = true;
		if (n == 0 || idx >= n)
			pos = n;
		else if (f < 0.f || idx < firstVisible)
			pos = firstVisible;
		else if (frac < 0.28f)
			pos = idx;
		else if (frac > 0.72f)
			pos = idx + 1;
		else
			insert = false, pos = idx;
	}
	static constexpr float ROWH = 4.3f, HEAD = 5.2f;
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
		t(2.2f, 3.4f, "#", 1.2f, ui::DIM);
		t(7.f, 3.4f, "PATTERN", 1.2f, ui::DIM);
		t(36.f, 3.4f, "BARS", 1.2f, ui::DIM, NVG_ALIGN_CENTER);
		t(46.f, 3.4f, "PART", 1.2f, ui::DIM);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 1.5f * u, 4.6f * u);
		nvgLineTo(vg, (w - 1.5f) * u, 4.6f * u);
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
			t(w / 2, 17.f, "No song yet", 1.8f, ui::DIM, NVG_ALIGN_CENTER);
			t(w / 2, 22.f, "Drag pads in here, or tap a pad, then INSERT", 1.3f, ui::DIM, NVG_ALIGN_CENTER);
			t(w / 2, 26.f, "MAKE SONG: a whole song from the selected pad", 1.3f, ui::DIM, NVG_ALIGN_CENTER);
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
			if (play && !sel)
				roundRect(vg, 1.f * u, (y - 0.2f) * u, 0.7f * u, (ROWH - 0.2f) * u, 0.3f * u, ACC);
			if (play && m->genraRunning) {
				int n2 = ROW_BARS[clamp((int)row.barsIdx, 0, 5)];
				float f = clamp((float)(m->barNow - m->song.rowStart) / n2, 0.f, 1.f);
				roundRect(vg, 1.7f * u, (y + ROWH - 0.85f) * u, (w - 2.7f) * f * u, 0.35f * u, 0.15f * u, ACC);
			}
			NVGcolor c = sel ? ACC : (ok ? ui::TXT : ui::DIM);
			t(2.2f, y + 2.8f, string::f("%02d", r + 1), 1.4f, sel ? ACC : ui::DIM, NVG_ALIGN_LEFT, 0.2f);
			t(7.f, y + 2.8f, slotName(row.slot), 1.5f, c, NVG_ALIGN_LEFT, 0.25f);
			if (ok) // the colour of the GENRA original it comes from
				roundRect(vg, 13.2f * u, (y + 0.7f) * u, 1.f * u, 2.4f * u, 0.3f * u, familyColor(m->slots[row.slot].family));
			if (ok && m->slots[row.slot].name[0])
				ui::text(vg, ui::boldFont(), 15.2f * u, (y + 2.8f) * u, m->slots[row.slot].name, 1.5f * u, c, NVG_ALIGN_LEFT, 0.25f * u);
			else if (!ok)
				t(15.2f, y + 2.8f, "–", 1.5f, ui::DIM);
			t(36.f, y + 2.8f, std::to_string(ROW_BARS[clamp((int)row.barsIdx, 0, 5)]), 1.5f, c, NVG_ALIGN_CENTER, 0.2f);
			if ((row.on & 0xF) != 0xF) // an older row with silent tracks (they stay silent)
				t(39.6f, y + 2.6f, "M", 1.0f, ui::DIM, NVG_ALIGN_CENTER, 0.f);
			if (row.fill)
				roundRect(vg, 42.4f * u, (y + 1.5f) * u, 0.8f * u, 0.8f * u, 0.2f * u, ACC);
			int sec = clamp((int)row.sec, 0, NUM_SECTIONS - 1);
			roundRect(vg, 45.5f * u, (y + 0.7f) * u, 1.f * u, 2.4f * u, 0.3f * u, SEC_COL[sec]);
			t(47.4f, y + 2.8f, SECTION_NAMES[sec], 1.35f, sel ? SEC_COL[sec] : ui::TXT, NVG_ALIGN_LEFT, 0.25f);
			if (row.cvA != 0.f || row.cvB != 0.f) { // the row carries CV A / CV B
				nvgBeginPath(vg);
				nvgCircle(vg, (w - 2.4f) * u, (y + 1.9f) * u, 0.45f * u);
				nvgStrokeColor(vg, ui::DIM);
				nvgStrokeWidth(vg, 0.2f * u);
				nvgStroke(vg);
			}
		}
		// status line
		std::string st;
		int total = 0, done = 0;
		for (int r = 0; r < n; r++)
			if (m->playable(r)) {
				int b = ROW_BARS[clamp((int)m->rows[r].barsIdx, 0, 5)];
				total += b;
				if (m->song.playing && r < m->song.row)
					done += b;
			}
		float secPerBar = 240.f / std::max(20.f, m->bpmNow); // 4/4: a bar is four beats
		auto mmss = [](float sec) {
			int v = (int)std::lround(std::max(0.f, sec));
			return string::f("%d:%02d", v / 60, v % 60);
		};
		if (m->song.playing && m->song.row >= 0) {
			int into = clamp((int)(m->barNow - m->song.rowStart), 0, 99) + 1;
			st = string::f("BAR %d / %d  ·  %s / %s", done + into, total, mmss((done + into - 1) * secPerBar).c_str(),
			               mmss(total * secPerBar).c_str());
		}
		else if (n)
			st = string::f("ROW %d / %d  ·  %d BARS  ·  %s", m->curRow + 1, n, total, mmss(total * secPerBar).c_str());
		if (!st.empty())
			ui::text(vg, mono, (w - 2.2f) * u, (box.size.y / u - 1.2f) * u, st, 2.f * u, ui::DIM, NVG_ALIGN_RIGHT, 0.f);
		if (m->dragSlot < 0)
			dragOver = false;
		if (dragOver && m->dragSlot >= 0) { // where the dragged pad lands: a line between rows, or a frame round a row
			bool ins;
			int pos;
			dropTarget(dragY, ins, pos);
			float yy = HEAD + 1.f + (pos - firstVisible) * ROWH;
			if (ins) {
				yy = clamp(yy - 0.2f, HEAD + 0.6f, box.size.y / u - 3.6f);
				roundRect(vg, 1.f * u, (yy - 0.25f) * u, (w - 2.f) * u, 0.5f * u, 0.25f * u, ACC);
			}
			else {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, 1.f * u, (yy - 0.2f) * u, (w - 2.f) * u, (ROWH - 0.2f) * u, 0.5f * u);
				nvgStrokeColor(vg, ACC);
				nvgStrokeWidth(vg, 0.4f * u);
				nvgStroke(vg);
			}
			// the pad itself, next to the pointer
			const Slot& ds = m->slots[clamp(m->dragSlot, 0, NUM_SLOTS - 1)];
			std::string lab = slotName(m->dragSlot) + (ds.name[0] ? std::string("  ") + ds.name : "");
			float gy = clamp(dragY - 2.4f, 0.f, box.size.y / u - 4.4f);
			roundRect(vg, (w - 25.f) * u, gy * u, 23.f * u, 4.2f * u, 0.7f * u, nvgRGBA(0x3a, 0x35, 0x20, 0xf0));
			roundRect(vg, (w - 24.4f) * u, (gy + 0.5f) * u, 0.9f * u, 3.2f * u, 0.3f * u, familyColor(ds.family));
			t(w - 22.6f, gy + 2.9f, lab, 1.4f, ACC, NVG_ALIGN_LEFT, 0.2f);
		}
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
		if (module && e.action == GLFW_RELEASE && e.button == GLFW_MOUSE_BUTTON_LEFT && module->dragSlot >= 0) {
			e.consume(this); // a pad dragged here: the drop follows (onDragDrop)
			return;
		}
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
	void onDragHover(const DragHoverEvent& e) override {
		if (!module || module->dragSlot < 0)
			return;
		dragOver = true;
		dragY = e.pos.y / mm2px(1.f);
		e.consume(this);
	}
	void onDragLeave(const DragLeaveEvent& e) override { dragOver = false; }
	void onDragDrop(const DragDropEvent& e) override {
		if (!module || module->dragSlot < 0)
			return;
		bool ins;
		int pos;
		dropTarget(dragY, ins, pos);
		int s = module->dragSlot;
		if (ins)
			module->insertRowAt(pos, s);
		else
			module->replaceRowSlot(pos, s);
		dragOver = false;
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
		menu->addChild(createMenuLabel(string::f("Row %d", r + 1)));
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
		menu->addChild(createMenuItem("Duplicate row", "", [=]() {
			int keep = m->selSlot;
			m->selSlot = m->rows[r].slot;
			m->setRow(r);
			m->insertRow();
			m->selSlot = keep;
		}));
		menu->addChild(createMenuItem("Delete row", "", [=]() {
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

/** 8 x 4 pads of the current bank, tinted in the colour of their song part, a bar on top in the colour of the GENRA
 * original they come from, with their names. Tap: play (or store, if empty); drag into the song list; double-click:
 * rename. */
struct Pads : AbWidget {
	float stepX = L::PADSTEP[0], stepY = L::PADSTEP[1], pw = L::PADSTEP[2], ph = L::PADSTEP[3];
	int pressPad = -1;   // a filled pad pressed: tap on release, unless it was dragged
	Vec lastPress;       // where the last press was (double-click)
	float dragDist = 0.f;
	bool dragging = false;
	int padAt(Vec p) {
		float u = mm2px(1.f);
		int c = (int)(p.x / u / stepX), r = (int)(p.y / u / stepY);
		if (c < 0 || c >= PAD_COLS || r < 0 || r >= PADS_PER_BANK / PAD_COLS)
			return -1;
		if (p.x / u - c * stepX > pw || p.y / u - r * stepY > ph)
			return -1;
		return r * PAD_COLS + c;
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
				float x = (n % PAD_COLS) * stepX, y = (n / PAD_COLS) * stepY;
				const Slot* s = module ? &module->slots[i] : nullptr;
				bool used = s && s->used;
				int tag = used ? s->tag : -1;
				NVGcolor fill = !used ? nvgRGB(0x1a, 0x1b, 0x20)
				                      : (tag >= 0 ? mixc(SEC_COL[tag], PLATE, i == playing ? 0.5f : 0.3f) : nvgRGB(0x2a, 0x2b, 0x33));
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x * u, y * u, pw * u, ph * u, 0.8f * u);
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
									case 0: return Vec(x + t2 * pw, y);
									case 1: return Vec(x + pw, y + t2 * ph);
									case 2: return Vec(x + pw - t2 * pw, y + ph);
									default: return Vec(x, y + ph - t2 * ph);
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
						nvgRoundedRect(vg, x * u, y * u, pw * u, ph * u, 0.8f * u);
						nvgStroke(vg);
					}
				}
				if (used) // the GENRA original it comes from
					roundRect(vg, (x + 0.6f) * u, (y + 0.6f) * u, (pw - 1.2f) * u, 1.f * u, 0.4f * u, familyColor(s->family));
				ui::text(vg, bold, (x + 0.9f) * u, (y + 3.2f) * u, slotName(i), 1.05f * u,
				         used ? ui::TXT : nvgRGB(0x4a, 0x4c, 0x57), NVG_ALIGN_LEFT, 0.1f * u);
				if (used && s->name[0]) {
					// the name, as much of it as fits the pad
					std::string nm = s->name;
					nvgFontFaceId(vg, bold->handle);
					nvgFontSize(vg, 1.35f * u);
					nvgTextLetterSpacing(vg, 0.15f * u);
					while (nm.size() > 1 && nvgTextBounds(vg, 0, 0, nm.c_str(), NULL, NULL) > (pw - 1.4f) * u)
						nm.pop_back();
					ui::text(vg, bold, (x + 0.9f) * u, (y + 5.3f) * u, nm, 1.35f * u, nvgRGB(0xff, 0xff, 0xff), NVG_ALIGN_LEFT, 0.15f * u);
				}
				for (int t = 0; t < 4; t++) {
					bool on = false;
					if (used) // a track with something in it
						on = !(module->slotSilent[i] & (1 << t));
					roundRect(vg, (x + 0.9f + t * 1.5f) * u, (y + ph - 1.3f) * u, 1.05f * u, 0.6f * u, 0.25f * u,
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
		lastPress = e.pos;
		int i = module->bank() * PADS_PER_BANK + n;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (!module->slots[i].used) // empty: stores at once
				module->padTap(i, module->genra());
			else // filled: tap on release (it may become a drag into the song list)
				pressPad = i, dragDist = 0.f, dragging = false;
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			openMenu(i);
			e.consume(this);
		}
	}
	void onDragMove(const DragMoveEvent& e) override {
		if (!module || pressPad < 0)
			return;
		dragDist += e.mouseDelta.norm();
		if (!dragging && dragDist > mm2px(1.5f)) {
			dragging = true;
			module->dragSlot = pressPad;
		}
	}
	void onDragEnd(const DragEndEvent& e) override {
		if (module && pressPad >= 0 && !dragging)
			module->padTap(pressPad, module->genra());
		pressPad = -1;
		dragging = false;
		if (module)
			module->dragSlot = -1;
	}
	void onDoubleClick(const DoubleClickEvent& e) override {
		if (!module)
			return;
		int n = padAt(lastPress);
		if (n < 0)
			return;
		int i = module->bank() * PADS_PER_BANK + n;
		if (module->slots[i].used)
			openRename(i);
		e.consume(this);
	}
	/** The name field in the pad's menu: Enter renames (one undo step). */
	struct NameField : rack::ui::TextField {
		Ablauf* m = nullptr;
		int i = -1;
		void onSelectKey(const SelectKeyEvent& e) override {
			if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
				m->rename(i, getText());
				if (rack::ui::MenuOverlay* o = getAncestorOfType<rack::ui::MenuOverlay>())
					o->requestDelete();
				e.consume(this);
				return;
			}
			rack::ui::TextField::onSelectKey(e);
			if (!e.getTarget())
				e.consume(this);
		}
		void onChange(const ChangeEvent& e) override {
			std::string t = getText(), c;
			for (char ch : t) { // as you type: capitals, at most 8 characters
				if (ch >= 'a' && ch <= 'z')
					ch = (char)(ch - 'a' + 'A');
				if (((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == ' ' || ch == '-') && (int)c.size() < NAME_LEN)
					c += ch;
			}
			if (c != t) {
				int cur = cursor;
				setText(c);
				cursor = selection = std::min(cur, (int)c.size());
			}
			rack::ui::TextField::onChange(e);
		}
	};
	void openRename(int i) {
		Ablauf* m = module;
		Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Rename " + slotName(i) + " (Enter):"));
		NameField* f = new NameField;
		f->m = m;
		f->i = i;
		f->box.size.x = 140.f;
		f->setText(m->slots[i].name);
		f->placeholder = "NAME";
		f->selectAll();
		menu->addChild(f);
		APP->event->setSelectedWidget(f);
	}
	void openMenu(int i) {
		Ablauf* m = module;
		Menu* menu = createMenu();
		const Slot& sl = m->slots[i];
		menu->addChild(createMenuLabel("Pad " + slotName(i) + (sl.used ? (sl.name[0] ? std::string(" · ") + sl.name : "") : " (empty)")));
		if (!sl.used)
			return;
		menu->addChild(createMenuLabel("Rename (Enter):"));
		NameField* f = new NameField;
		f->m = m;
		f->i = i;
		f->box.size.x = 140.f;
		f->setText(sl.name);
		f->placeholder = "NAME";
		f->selectAll();
		menu->addChild(f);
		APP->event->setSelectedWidget(f);
		if (sl.name[0])
			menu->addChild(createMenuItem("Clear name", "", [=]() { m->rename(i, ""); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createSubmenuItem("Part", sl.tag >= 0 ? SECTION_NAMES[sl.tag] : "none", [=](Menu* sm) {
			sm->addChild(createCheckMenuItem("None", "", [=]() { return m->slots[i].tag < 0; }, [=]() { m->setTag(i, -1); }));
			for (int k = 0; k < NUM_SECTIONS; k++)
				sm->addChild(createCheckMenuItem(SECTION_NAMES[k], "", [=]() { return m->slots[i].tag == k; }, [=]() { m->setTag(i, k); }));
		}));
		menu->addChild(createMenuItem("Insert into song (after the selected row)", "", [=]() {
			m->selSlot = i;
			m->insertRow();
		}));
		menu->addChild(createMenuItem("Overwrite with GENRA", "what GENRA plays now", [=]() { m->overwrite(i, m->genra()); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Export as MIDI", "all tracks", [=]() { m->exportPad(i, -1); }));
		static const char* TN[4] = {"drums", "bass", "melody", "pads"};
		for (int t = 0; t < 4; t++)
			menu->addChild(createMenuItem(std::string("Export ") + TN[t] + " only", "", [=]() { m->exportPad(i, t); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Clear", "", [=]() { m->clearSlot(i); }));
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
				ui::text(vg, bold, 1.6f * u, 3.7f * u,
				         m->genraBusy ? "This GENRA is driven by another ABLAUF" : "Place next to GENRA or right-click › Connect",
				         1.3f * u, ui::DIM, NVG_ALIGN_LEFT, 0.15f * u);
			else {
				int p = m->playingSlot;
				if (p >= 0) {
					std::string lab = slotName(p);
					if (m->slots[p].used && m->slots[p].name[0])
						lab += std::string("  ") + m->slots[p].name;
					float x = 1.6f + ui::text(vg, bold, 1.6f * u, 3.7f * u, lab, 1.45f * u, ui::TXT, NVG_ALIGN_LEFT, 0.25f * u) / u;
					int tag = m->slots[p].used ? m->slots[p].tag : -1;
					if (tag >= 0)
						ui::text(vg, bold, (x + 2.5f) * u, 3.7f * u, SECTION_NAMES[tag], 1.45f * u, SEC_COL[tag], NVG_ALIGN_LEFT, 0.25f * u);
				}
				else
					ui::text(vg, bold, 1.6f * u, 3.7f * u, "GENRA plays freely", 1.4f * u, ui::DIM, NVG_ALIGN_LEFT, 0.25f * u);
				if (m->armed == 2)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, m->copySrc < 0 ? "COPY: source" : "COPY: target", 1.4f * u, ACC, NVG_ALIGN_RIGHT, 0.25f * u);
				else if (m->armed == 3)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, "CLEAR: tap a pad", 1.4f * u, ACC, NVG_ALIGN_RIGHT, 0.25f * u);
				else if (m->queuedSlot >= 0)
					ui::text(vg, bold, (w - 1.6f) * u, 3.7f * u, "> " + slotName(m->queuedSlot), 1.45f * u, ui::TXT, NVG_ALIGN_RIGHT, 0.25f * u);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

/** MODE: a large two-way switch PATT | SONG – the chosen half is lit, a click on a half picks it. */
struct ModeSwitch : app::ParamWidget {
	int value() {
		engine::ParamQuantity* pq = getParamQuantity();
		return pq && pq->getValue() > 0.5f ? 1 : 0;
	}
	void onButton(const ButtonEvent& e) override {
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
			if (engine::ParamQuantity* pq = getParamQuantity())
				pq->setValue(e.pos.x < box.size.x / 2 ? 0.f : 1.f);
			e.consume(this);
			return;
		}
		ParamWidget::onButton(e);
	}
	void draw(const DrawArgs& args) override {}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f), hw = box.size.x / 2, h = box.size.y;
			int v = value();
			nvgBeginPath(vg);
			nvgRoundedRect(vg, v * hw + 0.4f * u, 0.4f * u, hw - 0.8f * u, h - 0.8f * u, 0.9f * u);
			nvgFillColor(vg, ACC);
			nvgFill(vg);
			nvgBeginPath(vg);
			nvgMoveTo(vg, hw, 1.f * u);
			nvgLineTo(vg, hw, h - 1.f * u);
			nvgStrokeColor(vg, nvgRGB(0x34, 0x36, 0x3f));
			nvgStrokeWidth(vg, 0.3f * u);
			nvgStroke(vg);
			const char* lab[2] = {"PATT", "SONG"};
			for (int i = 0; i < 2; i++)
				ui::text(vg, bold, (i + 0.5f) * hw, h / 2 + 0.6f * u, lab[i], 1.55f * u,
				         i == v ? nvgRGB(0x14, 0x15, 0x19) : nvgRGB(0x8a, 0x8c, 0x96), NVG_ALIGN_CENTER, 0.25f * u);
		}
		ParamWidget::drawLayer(args, layer);
	}
};

/** SONG FORM: the structure MAKE SONG builds – AUTO (typical for the pattern's genre) or one chosen. Click: the list,
 * wheel: steps. */
struct SongFormField : AbWidget {
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			NVGcontext* vg = args.vg;
			auto bold = ui::boldFont();
			float u = mm2px(1.f), w = box.size.x / u;
			int sf = module ? module->songForm : -1;
			float x = 1.6f;
			if (sf < 0)
				x += ui::text(vg, bold, x * u, 3.65f * u, "AUTO", 1.7f * u, ACC, NVG_ALIGN_LEFT, 0.4f * u) / u + 2.f;
			int f = module ? module->effectiveForm() : SF_HOUSE;
			std::string nm = f >= 0 ? SONG_FORMS[f].name : "by the pattern's genre";
			float size = 1.6f;
			nvgFontFaceId(vg, bold->handle);
			while (size > 1.1f) { // long names get smaller
				nvgFontSize(vg, size * u);
				nvgTextLetterSpacing(vg, 0.3f * u);
				if (nvgTextBounds(vg, 0, 0, nm.c_str(), NULL, NULL) <= (w - x - 6.f) * u)
					break;
				size -= 0.1f;
			}
			ui::text(vg, bold, x * u, 3.65f * u, nm, size * u, f >= 0 ? ui::TXT : ui::DIM, NVG_ALIGN_LEFT, 0.3f * u);
		}
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		Ablauf* m = module;
		Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Song form for MAKE SONG"));
		menu->addChild(createCheckMenuItem("AUTO (typical for the pattern's genre)", "", [=]() { return m->songForm < 0; },
		                                   [=]() { m->songForm = -1; }));
		for (int k = 0; k < NUM_SONG_FORMS; k++)
			menu->addChild(createCheckMenuItem(SONG_FORMS[k].name, string::f("%d bars", songFormBars(k)),
			                                   [=]() { return m->songForm == k; }, [=]() { m->songForm = k; }));
		e.consume(this);
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module)
			return;
		int d = e.scrollDelta.y > 0 ? -1 : 1;
		module->songForm = clamp(module->songForm + d, -1, NUM_SONG_FORMS - 1);
		e.consume(this);
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
				float v = module ? module->params[Ablauf::REROLL_PARAM + t].getValue() : 1.f;
				bool on = v > 0.5f && v < 1.5f, vary = v > 1.5f;
				roundRect(vg, t * 4.6f * u, 0, 3.6f * u, 3.2f * u, 0.5f * u, on ? TRACK_COLORS[t] : nvgRGB(0x24, 0x25, 0x2c));
				if (vary) { // VARY: the track's colour as a frame, the letter in it
					nvgBeginPath(vg);
					nvgRoundedRect(vg, t * 4.6f * u + 0.3f * u, 0.3f * u, 3.0f * u, 2.6f * u, 0.4f * u);
					nvgStrokeColor(vg, TRACK_COLORS[t]);
					nvgStrokeWidth(vg, 0.5f * u);
					nvgStroke(vg);
				}
				ui::text(vg, bold, (t * 4.6f + 1.8f) * u, 2.25f * u, TRACK_LETTER[t], 1.3f * u,
				         on ? DARK : (vary ? TRACK_COLORS[t] : nvgRGB(0x4a, 0x4c, 0x57)), NVG_ALIGN_CENTER, 0.f);
			}
		}
		Widget::drawLayer(args, layer);
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		int t = clamp((int)(e.pos.x / mm2px(4.6f)), 0, 3);
		Param& p = module->params[Ablauf::REROLL_PARAM + t];
		int v = (int)std::round(p.getValue()); // Keep → Re-roll → Vary → Keep
		p.setValue((float)((v + 1) % 3));
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
			pending = m->beginChange("edit row", m->rows.empty() ? -1 : m->rows[m->curRow].slot);
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
		add(new SongFormField, L::SONGFORM);
		SectionRing* ring = new SectionRing;
		add(ring, L::SECRING);
		SectionRing* nm = new SectionRing;
		nm->name = true;
		add(nm, L::SECNAME);

		addParam(createParamCentered<GKnob>(ui::mm(L::ZEILE), module, A::ZEILE_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::PATTERN), module, A::PATTERN_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::TAKTE), module, A::TAKTE_PARAM));
		addParam(createParamCentered<RowKnob<GSmallKnob>>(ui::mm(L::ABSCHNITT), module, A::ABSCHNITT_PARAM));

		// (the track buttons D B M P and STORE are gone from the panel; their ids stay for old patches)

		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::INS), module, A::INS_PARAM, A::INS_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::DEL), module, A::DEL_PARAM, A::DEL_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::UNDO), module, A::UNDO_PARAM, A::UNDO_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xff, 0xd2, 0x3d>>>(ui::mm(L::MAKESONG), module, A::MAKESONG_PARAM, A::MAKESONG_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xff, 0xd2, 0x3d>>>(ui::mm(L::FILL), module, A::FILL_PARAM, A::FILL_LIGHT));
		ModeSwitch* mode = createParam<ModeSwitch>(mm2px(Vec(L::MODESW[0], L::MODESW[1])), module, A::MODE_PARAM);
		mode->box.size = mm2px(Vec(L::MODESW[2], L::MODESW[3]));
		addParam(mode);
		// PLAY / STOP / LOOP: large pads like GENRA's transport
		auto pad = [&](const float* p, int id, int light, int icon, NVGcolor c, bool latch) {
			genraui::BigPad* w = createParamCentered<genraui::BigPad>(ui::mm(p), module, id);
			w->setSize(8.f);
			w->lightId = light;
			w->icon = icon;
			w->col = c;
			w->latch = latch;
			addParam(w);
		};
		pad(L::PLAY, A::PLAY_PARAM, A::PLAY_LIGHT, genraui::BigPad::I_PLAY, nvgRGB(0x5d, 0xff, 0x8a), false);
		pad(L::STOP, A::STOP_PARAM, A::STOP_LIGHT, genraui::BigPad::I_STOP, nvgRGB(0xff, 0x5f, 0x5f), false);
		pad(L::LOOP, A::LOOP_PARAM, A::LOOP_LIGHT, genraui::BigPad::I_LOOP, nvgRGB(0xff, 0xd8, 0x4a), true);

		const float* pp[NUM_SECTIONS] = {L::PART0, L::PART1, L::PART2, L::PART3, L::PART4, L::PART5};
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x6f, 0x86, 0xb8>>>(ui::mm(pp[0]), module, A::PART_PARAM + 0, A::PART_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x9b, 0x7b, 0xd4>>>(ui::mm(pp[1]), module, A::PART_PARAM + 1, A::PART_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xe0, 0xa5, 0x26>>>(ui::mm(pp[2]), module, A::PART_PARAM + 2, A::PART_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0xff, 0x5a, 0x4f>>>(ui::mm(pp[3]), module, A::PART_PARAM + 3, A::PART_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x3f, 0xae, 0x9a>>>(ui::mm(pp[4]), module, A::PART_PARAM + 4, A::PART_LIGHT + 4));
		addParam(createLightParamCentered<VCVLightBezel<ALight<0x8a, 0x8d, 0x99>>>(ui::mm(pp[5]), module, A::PART_PARAM + 5, A::PART_LIGHT + 5));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::COPY), module, A::COPY_PARAM, A::COPY_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(ui::mm(L::CLEAR), module, A::CLEAR_PARAM, A::CLEAR_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(ui::mm(L::CLEARALL), module, A::CLEARALL_PARAM, A::CLEARALL_LIGHT));
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
		menu->addChild(createMenuLabel("Connect to…"));
		menu->addChild(createCheckMenuItem("Automatic (docked GENRA)", "", [=]() { return m->linkedId < 0; }, [=]() { m->linkedId = -1; }));
		int n = 0;
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* mm = APP->engine->getModule(id);
			if (!mm || !isGenraModel(mm->model))
				continue;
			n++;
			std::string label = string::f("GENRA #%d", n);
			menu->addChild(createCheckMenuItem(label, "", [=]() { return m->linkedId == id; }, [=]() { m->linkedId = id; }));
		}
		menu->addChild(new MenuSeparator);
		auto styleMenu = [=](const char* label, int* opt, const char* const* names, int n) {
			menu->addChild(createSubmenuItem(label, *opt < 0 ? "Random" : names[*opt], [=](Menu* sm) {
				sm->addChild(createCheckMenuItem("Random (each press)", "", [=]() { return *opt < 0; }, [=]() { *opt = -1; }));
				for (int k = 0; k < n; k++)
					sm->addChild(createCheckMenuItem(names[k], "", [=]() { return *opt == k; }, [=]() { *opt = k; }));
			}));
		};
		styleMenu("Break bass", &m->partOpts.breakBass, BREAK_BASS_NAMES, NUM_BREAK_BASS);
		styleMenu("Intro bass", &m->partOpts.introBass, INTRO_BASS_NAMES, NUM_INTRO_BASS);
		menu->addChild(new MenuSeparator);
		for (int b = 0; b < 4; b++)
			menu->addChild(createMenuItem(string::f("Clear bank %c", 'A' + b), "", [=]() {
				m->clearSlots(b * PADS_PER_BANK, (b + 1) * PADS_PER_BANK, string::f("clear bank %c", 'A' + b).c_str());
			}));
		menu->addChild(createMenuItem("Export song as MIDI", "all rows, a marker per part", [=]() { m->exportSong(); }));
		{
			static const char* TN[4] = {"drums", "bass", "melody", "pads"};
			for (int t = 0; t < 4; t++)
				menu->addChild(createMenuItem(std::string("Export song – ") + TN[t] + " only", "", [=]() { m->exportSong(t); }));
		}
		midifolder::appendMenu(menu);
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuItem("Clear song", "", [=]() {
			AblaufAction* a = m->beginChange("clear song");
			m->rows.clear();
			m->rowsReplaced();
			m->setRow(0);
			m->endChange(a);
		}));
		menu->addChild(createMenuLabel("Store: tap an empty pad (right-click a pad › Overwrite with GENRA)"));
		menu->addChild(createMenuLabel("Song: drag a pad into the list, or tap a pad, then INSERT · MAKE SONG builds it all"));
		menu->addChild(createMenuLabel("Variations: pattern playing, then INTRO … OUTRO (each press: a new pad)"));
		menu->addChild(createMenuLabel("Names: double-click a pad (GENRA 1, 2 … and per family INTRO 1, 2 …)"));
	}
};

} // namespace genra_ablauf

Model* modelAblauf = createModel<genra_ablauf::Ablauf, genra_ablauf::AblaufWidget>("GenraAblauf");
