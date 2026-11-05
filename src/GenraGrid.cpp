// GENRA GRID – editor for GENRA's loop: drum grid, piano roll, velocity lane.
// Connects to a GENRA module docked on the left or right, or to one chosen in the context menu.
// All editing happens on the UI thread on a copy of the pattern; finished tracks are handed to
// GENRA with Genra::submitTrack() and applied on its audio thread.
#include "GenraModule.hpp"
#include "LayoutGrid.hpp"
#include <deque>

static const int ZOOMS[4] = {1, 2, 4, 8};
static const int PIANO_ROWS = 25;
static const char* const DRUM_SHORT[NUM_DRUMS] = {"KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"};
static const char* const NOTE_SHARP[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char* const NOTE_FLAT[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};

struct GenraGrid : Module {
	enum ParamId {
		ENUMS(TRACK_PARAM, 4),
		BAR_PARAM,
		ZOOM_PARAM,
		PITCH_PARAM,
		AUTO_PARAM,
		VEL_PARAM,
		LEN_PARAM,
		SNAP_PARAM,
		FOLLOW_PARAM,
		UNDO_PARAM,
		REDO_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		CLEAR_PARAM,
		GEN_PARAM,
		PARAMS_LEN
	};
	enum LightId {
		ENUMS(TRACK_LIGHT, 4),
		AUTO_LIGHT,
		ENUMS(SNAP_LIGHT, 3),
		FOLLOW_LIGHT,
		LINK_LIGHT,
		UNDO_LIGHT,
		REDO_LIGHT,
		COPY_LIGHT,
		PASTE_LIGHT,
		CLEAR_LIGHT,
		GEN_LIGHT,
		LIGHTS_LEN
	};

	// editor state (UI thread only)
	int track = T_BASS;
	int snapMode = 1; // 0 off, 1 scale, 2 chord
	int drumRow = KICK;
	int64_t linkedId = -1; // -1: automatic (docked GENRA)

	GenraGrid() {
		config(PARAMS_LEN, 0, 0, LIGHTS_LEN);
		static const char* TN[4] = {"Drums", "Bass", "Melodie", "Pads"};
		for (int t = 0; t < 4; t++)
			configButton(TRACK_PARAM + t, std::string("Spur ") + TN[t] + " bearbeiten");
		configSwitch(BAR_PARAM, 0.f, 7.f, 0.f, "Erster sichtbarer Takt", {"1", "2", "3", "4", "5", "6", "7", "8"});
		configSwitch(ZOOM_PARAM, 0.f, 3.f, 1.f, "Sichtbare Takte", {"1 Takt", "2 Takte", "4 Takte", "8 Takte"});
		configParam(PITCH_PARAM, -36.f, 36.f, 0.f, "Tonbereich verschieben", " Halbtöne");
		paramQuantities[PITCH_PARAM]->snapEnabled = true;
		configSwitch(AUTO_PARAM, 0.f, 1.f, 1.f, "Ansicht läuft beim Abspielen mit (AUTO SCROLL)", {"Aus", "An"});
		configParam(VEL_PARAM, 1.f, 127.f, 100.f, "Velocity neuer Noten");
		paramQuantities[VEL_PARAM]->snapEnabled = true;
		configParam(LEN_PARAM, 1.f, 16.f, 2.f, "Länge neuer Noten", " 16tel");
		paramQuantities[LEN_PARAM]->snapEnabled = true;
		configButton(SNAP_PARAM, "SNAP: Aus / Tonleiter / Akkordtöne (Shift: alle Noten einrasten)");
		configButton(FOLLOW_PARAM, "CHORD FOLLOW: bearbeitete Noten folgen den Akkorden");
		configButton(UNDO_PARAM, "Rückgängig");
		configButton(REDO_PARAM, "Wiederholen");
		configButton(COPY_PARAM, "Takt kopieren (erster sichtbarer Takt)");
		configButton(PASTE_PARAM, "Takt einfügen (erster sichtbarer Takt)");
		configButton(CLEAR_PARAM, "Spur leeren");
		configButton(GEN_PARAM, "Spur wieder generieren (GEN)");
		for (int i = 0; i < PARAMS_LEN; i++)
			paramQuantities[i]->randomizeEnabled = false;
	}

	json_t* dataToJson() override {
		json_t* root = json_object();
		json_object_set_new(root, "track", json_integer(track));
		json_object_set_new(root, "snapMode", json_integer(snapMode));
		json_object_set_new(root, "drumRow", json_integer(drumRow));
		json_object_set_new(root, "linkedId", json_integer(linkedId));
		return root;
	}
	void dataFromJson(json_t* root) override {
		json_t* j;
		if ((j = json_object_get(root, "track")))
			track = clamp((int)json_integer_value(j), 0, 3);
		if ((j = json_object_get(root, "snapMode")))
			snapMode = clamp((int)json_integer_value(j), 0, 2);
		if ((j = json_object_get(root, "drumRow")))
			drumRow = clamp((int)json_integer_value(j), 0, NUM_DRUMS - 1);
		if ((j = json_object_get(root, "linkedId")))
			linkedId = json_integer_value(j);
	}

	// the GENRA this editor works on (UI thread)
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
};

// ================================================================= lights
template <int R, int G, int B>
struct GridColorLight : GrayModuleLightWidget {
	GridColorLight() { addBaseColor(nvgRGB(R, G, B)); }
};
using GOrange = GridColorLight<0xff, 0x8a, 0x3d>;
using GPink = GridColorLight<0xff, 0x4f, 0x6d>;
using GCyan = GridColorLight<0x3d, 0xc9, 0xff>;
using GPurple = GridColorLight<0xa9, 0x8b, 0xff>;
using GTeal = GridColorLight<0x4f, 0xd1, 0xbd>;
using GMint = GridColorLight<0x5d, 0xff, 0x8a>;

// ================================================================= editor display
struct GridDisplay : Widget {
	GenraGrid* module = nullptr;

	// ---- view (recomputed every frame in step())
	Genra* g = nullptr;
	Params P;
	int L = 64;
	int vs = 0, vis = 32; // first visible step, visible steps
	int low = 36;         // lowest visible pitch (piano roll)
	float center[4] = {0.f, 0.f, 0.f, 0.f};
	bool centerSet[4] = {false, false, false, false};
	int lastTrack = -1;

	// ---- editing
	enum Drag { D_NONE, D_MOVE, D_RESIZE, D_PAINT, D_ERASE, D_VEL };
	int drag = D_NONE;
	Pattern work;        // pattern being edited (copy)
	Pattern before;      // state before the current gesture (undo)
	bool editing = false;
	bool changed = false; // the current gesture really changed something
	bool dirty = false;  // work has to be handed to GENRA
	int showWork = 0;    // frames to keep drawing `work` after a hand-off
	std::vector<GridNote> notes;
	int dragIdx = -1, anchor = 0;
	Vec pressPos;
	int hoverStep = -1, hoverRow = -1;
	struct Snapshot {
		Pattern pat;
		int track;
	};
	std::deque<Snapshot> undo, redo;
	BarClip clip;

	// ---------------------------------------------------------------- geometry (mm, display-relative)
	float gx0() { return glayout::GRID[0]; }
	float gy0() { return glayout::GRID[1]; }
	float gx1() { return glayout::GRID[2]; }
	float gy1() { return glayout::GRID[3]; }
	float sw() { return (gx1() - gx0()) / vis; }
	int rows() { return module && module->track == T_DRUMS ? NUM_DRUMS : PIANO_ROWS; }
	float rh() { return (gy1() - gy0()) / rows(); }
	Vec mm(Vec px) { return px.div(mm2px(Vec(1.f, 1.f))); }
	int stepAt(float xmm) { return vs + (int)std::floor((xmm - gx0()) / sw()); }
	int rowAt(float ymm) { return (int)std::floor((ymm - gy0()) / rh()); }
	int pitchOfRow(int r) { return low + PIANO_ROWS - 1 - r; }
	int rowOfPitch(int p) { return low + PIANO_ROWS - 1 - p; }
	bool inGrid(Vec m) { return m.x >= gx0() && m.x < gx1() && m.y >= gy0() && m.y < gy1(); }
	bool inVel(Vec m) { return m.x >= gx0() && m.x < gx1() && m.y >= glayout::VELLANE[0] && m.y < glayout::VELLANE[1]; }

	const Pattern& src() { return (editing || dirty || showWork > 0) ? work : g->pat; }

	// ---------------------------------------------------------------- frame update
	void step() override {
		Widget::step();
		if (!module)
			return;
		g = module->genra();
		if (!g) {
			editing = false;
			drag = D_NONE;
			return;
		}
		P = g->buildParams();
		L = g->lengthSteps();
		int t = module->track;
		int zoomBars = ZOOMS[clamp((int)std::round(module->params[GenraGrid::ZOOM_PARAM].getValue()), 0, 3)];
		vis = zoomBars * 16;
		int startBar = clamp((int)std::round(module->params[GenraGrid::BAR_PARAM].getValue()), 0, MAX_BARS - 1);
		bool autoScroll = module->params[GenraGrid::AUTO_PARAM].getValue() > 0.5f;
		if (autoScroll && g->running && g->step >= 0 && !editing)
			startBar = (g->step / 16 / zoomBars) * zoomBars;
		startBar = clamp(startBar, 0, std::max(0, MAX_BARS - zoomBars));
		vs = startBar * 16;
		// pitch window: centred on the track's notes (once per track), PITCH shifts it
		if (t != T_DRUMS && (!centerSet[t] || t != lastTrack)) {
			if (!centerSet[t]) {
				std::vector<GridNote> ns = trackNotes(g->pat, P, t, L);
				float sum = 0.f;
				for (const GridNote& n : ns)
					sum += n.pitch;
				static const int DEF[4] = {0, 36, 67, 60};
				center[t] = ns.empty() ? DEF[t] + P.key : sum / ns.size();
				centerSet[t] = true;
			}
		}
		lastTrack = t;
		low = clamp((int)std::round(center[t] + module->params[GenraGrid::PITCH_PARAM].getValue()) - PIANO_ROWS / 2, 0, 127 - PIANO_ROWS + 1);
		// hand edited tracks to GENRA
		if (dirty && g->submitTrack(work, t)) {
			dirty = false;
			showWork = 3;
		}
		else if (!dirty && showWork > 0 && g->gridState.load() == 0)
			showWork--;
	}

	// ---------------------------------------------------------------- edit helpers
	bool begin() {
		if (!g)
			return false;
		work = g->pat;
		before = g->pat;
		int t = module->track;
		freezeTrack(work, P, t, L);
		editing = true;
		changed = false;
		return true;
	}
	void apply() { // the note list was changed during a gesture
		int t = module->track;
		if (t != T_DRUMS)
			writeTrackNotes(work, P, t, notes, L);
		changed = true;
		dirty = true;
	}
	void touched() { // drum / direct pattern edits
		changed = true;
		dirty = true;
	}
	void commit() {
		if (!editing)
			return;
		editing = false;
		if (!changed)
			return; // e.g. a click on a note without moving it: the track stays as it is
		pushUndo(before, module->track);
		dirty = true;
	}
	void pushUndo(const Pattern& p, int t) {
		undo.push_back({p, t});
		if (undo.size() > 64)
			undo.pop_front();
		redo.clear();
	}
	void handOver(const Pattern& p) {
		work = p;
		dirty = true;
	}

	int findNote(int s, int pitch) {
		for (int i = (int)notes.size() - 1; i >= 0; i--)
			if (notes[i].pitch == pitch && s >= notes[i].start && s < notes[i].start + notes[i].len)
				return i;
		return -1;
	}
	// mono tracks: only one note may start on a step
	void removeStartsAt(int s, int keep) {
		if (module->track == T_PADS)
			return;
		for (int i = (int)notes.size() - 1; i >= 0; i--)
			if (i != keep && notes[i].start == s) {
				notes.erase(notes.begin() + i);
				if (dragIdx > i)
					dragIdx--;
			}
	}

	int newVel() { return clamp((int)std::round(module->params[GenraGrid::VEL_PARAM].getValue()), 1, 127); }
	int newLen() { return clamp((int)std::round(module->params[GenraGrid::LEN_PARAM].getValue()), 1, 16); }

	// ---------------------------------------------------------------- mouse
	Vec mousePosMm() {
		Vec p = APP->scene->getMousePos().minus(getAbsoluteOffset(Vec(0, 0))).div(getAbsoluteZoom());
		return mm(p);
	}

	void onHover(const HoverEvent& e) override {
		Vec m = mm(e.pos);
		hoverStep = inGrid(m) || inVel(m) ? stepAt(m.x) : -1;
		hoverRow = inGrid(m) ? rowAt(m.y) : -1;
		e.consume(this);
	}
	void onLeave(const LeaveEvent& e) override {
		hoverStep = hoverRow = -1;
	}

	void onButton(const ButtonEvent& e) override {
		if (!module || !g || e.action != GLFW_PRESS)
			return;
		Vec m = mm(e.pos);
		pressPos = e.pos;
		int t = module->track;
		if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			if (inGrid(m))
				openMenu(stepAt(m.x), rowAt(m.y));
			e.consume(this);
			return;
		}
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		e.consume(this);
		// drum row labels: choose the voice shown in the velocity lane
		if (t == T_DRUMS && m.x < gx0() && m.y >= gy0() && m.y < gy1()) {
			module->drumRow = clamp(rowAt(m.y), 0, NUM_DRUMS - 1);
			return;
		}
		if (inVel(m)) {
			if (!begin())
				return;
			notes = trackNotes(work, P, t, L);
			drag = D_VEL;
			velAt(m);
			return;
		}
		if (!inGrid(m))
			return;
		int s = stepAt(m.x), r = rowAt(m.y);
		if (s < 0 || s >= L || !begin())
			return;
		if (t == T_DRUMS) {
			int v = clamp(r, 0, NUM_DRUMS - 1);
			bool on = work.drums[v][s].fv > 0;
			drag = on ? D_ERASE : D_PAINT;
			setDrum(work, v, s, on ? 0 : newVel(), 1);
			touched();
			return;
		}
		notes = trackNotes(work, P, t, L);
		int pitch = pitchOfRow(r);
		int i = findNote(s, pitch);
		if (i >= 0) {
			const GridNote& n = notes[i];
			float endMm = gx0() + (n.start + n.len - vs) * sw();
			drag = (m.x > endMm - std::max(1.2f, sw() * 0.35f)) ? D_RESIZE : D_MOVE;
			dragIdx = i;
			anchor = s - n.start;
			return;
		}
		GridNote n;
		n.start = s;
		n.len = std::min(newLen(), L - s);
		n.pitch = snapPitch(work, P, s, pitch, module->snapMode);
		n.vel = newVel();
		notes.push_back(n);
		dragIdx = (int)notes.size() - 1;
		removeStartsAt(s, dragIdx);
		drag = D_RESIZE;
		anchor = 0;
		apply();
	}

	void velAt(Vec m) {
		int s = stepAt(m.x);
		if (s < 0 || s >= L)
			return;
		float y0 = glayout::VELLANE[0], y1 = glayout::VELLANE[1];
		int v = clamp((int)std::round((y1 - m.y) / (y1 - y0) * 127.f), 1, 127);
		int t = module->track;
		if (t == T_DRUMS) {
			DrumStep& d = work.drums[module->drumRow][s];
			if (d.fv && d.fv != v) {
				d.fv = (uint8_t)v;
				touched();
			}
			return;
		}
		bool any = false;
		for (GridNote& n : notes)
			if (n.start == s) {
				n.vel = v;
				any = true;
			}
		if (any)
			apply();
	}

	void onDragStart(const DragStartEvent& e) override { e.consume(this); }

	void onDragMove(const DragMoveEvent& e) override {
		if (!module || !g || !editing || drag == D_NONE)
			return;
		Vec m = mousePosMm();
		int t = module->track;
		if (drag == D_VEL) {
			velAt(m);
			return;
		}
		int s = clamp(stepAt(m.x), 0, L - 1);
		if (t == T_DRUMS) {
			int v = clamp(rowAt(m.y), 0, NUM_DRUMS - 1);
			int want = drag == D_PAINT ? newVel() : 0;
			if ((work.drums[v][s].fv > 0) != (want > 0)) {
				setDrum(work, v, s, want, 1);
				touched();
			}
			return;
		}
		if (dragIdx < 0 || dragIdx >= (int)notes.size())
			return;
		GridNote& n = notes[dragIdx];
		if (drag == D_RESIZE) {
			int len = clamp(s - n.start + 1, 1, L - n.start);
			if (len != n.len) {
				n.len = len;
				apply();
			}
		}
		else if (drag == D_MOVE) {
			int ns = clamp(s - anchor, 0, L - 1);
			int np = snapPitch(work, P, ns, pitchOfRow(clamp(rowAt(m.y), 0, PIANO_ROWS - 1)), module->snapMode);
			if (ns != n.start || np != n.pitch) {
				n.start = ns;
				n.pitch = np;
				n.len = std::min(n.len, L - ns);
				removeStartsAt(ns, dragIdx);
				apply();
			}
		}
	}

	void onDragEnd(const DragEndEvent& e) override {
		if (drag != D_NONE)
			commit();
		drag = D_NONE;
		dragIdx = -1;
	}

	void onDoubleClick(const DoubleClickEvent& e) override {
		if (!module || !g || module->track == T_DRUMS)
			return;
		Vec m = mm(pressPos);
		if (!inGrid(m))
			return;
		if (!begin())
			return;
		notes = trackNotes(work, P, module->track, L);
		int i = findNote(stepAt(m.x), pitchOfRow(rowAt(m.y)));
		if (i >= 0) {
			notes.erase(notes.begin() + i);
			apply();
			commit();
		}
		else
			editing = false;
		e.consume(this);
	}

	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module || !g)
			return;
		Vec m = mm(e.pos);
		if (!inGrid(m))
			return;
		int s = stepAt(m.x), r = rowAt(m.y), t = module->track;
		int d = e.scrollDelta.y > 0 ? 6 : -6;
		if (s < 0 || s >= L)
			return;
		if (t == T_DRUMS) {
			int v = clamp(r, 0, NUM_DRUMS - 1);
			if (!g->pat.fixed[T_DRUMS] && !drumHit(g->pat, P, v, s, L, false).on)
				return;
			if (!begin())
				return;
			DrumStep& ds = work.drums[v][s];
			if (ds.fv) {
				ds.fv = (uint8_t)clamp(ds.fv + d, 1, 127);
				touched();
				commit();
			}
			else
				editing = false;
		}
		else {
			if (!begin())
				return;
			notes = trackNotes(work, P, t, L);
			int i = findNote(s, pitchOfRow(r));
			if (i >= 0) {
				notes[i].vel = clamp(notes[i].vel + d, 1, 127);
				apply();
				commit();
			}
			else
				editing = false;
		}
		e.consume(this);
	}

	// ---------------------------------------------------------------- context menu on a note / hit
	void openMenu(int s, int r) {
		if (!g || s < 0 || s >= L)
			return;
		int t = module->track;
		ui::Menu* menu = createMenu();
		GridDisplay* self = this;
		auto edit = [=](std::function<void()> f) {
			if (!self->g || !self->begin())
				return;
			int tt = self->module->track;
			if (tt != T_DRUMS)
				self->notes = trackNotes(self->work, self->P, tt, self->L);
			f();
			if (tt != T_DRUMS)
				self->apply();
			else
				self->touched();
			self->commit();
		};
		if (t == T_DRUMS) {
			int v = clamp(r, 0, NUM_DRUMS - 1);
			Hit h = drumHit(g->pat, P, v, s, L, false);
			menu->addChild(createMenuLabel(string::f("%s · Step %d", DRUM_SHORT[v], s + 1)));
			if (!h.on) {
				menu->addChild(createMenuItem("Schlag setzen", "", [=]() { edit([=]() { setDrum(self->work, v, s, self->newVel(), 1); }); }));
				return;
			}
			for (int rat = 1; rat <= 4; rat++)
				menu->addChild(createCheckMenuItem(rat == 1 ? "Einzelschlag" : string::f("Roll %d×", rat), "", [=]() { return h.ratchet == rat; },
				                                   [=]() { edit([=]() { setDrum(self->work, v, s, std::max<int>(1, self->work.drums[v][s].fv), rat); }); }));
			menu->addChild(new ui::MenuSeparator);
			for (int vv : {32, 64, 96, 127})
				menu->addChild(createMenuItem(string::f("Velocity %d", vv), "", [=]() {
					edit([=]() { self->work.drums[v][s].fv = (uint8_t)vv; });
				}));
			menu->addChild(new ui::MenuSeparator);
			menu->addChild(createMenuItem("Löschen", "", [=]() { edit([=]() { setDrum(self->work, v, s, 0, 1); }); }));
			return;
		}
		std::vector<GridNote> ns = trackNotes(g->pat, P, t, L);
		int pitch = pitchOfRow(r);
		int idx = -1;
		for (int i = (int)ns.size() - 1; i >= 0; i--)
			if (ns[i].pitch == pitch && s >= ns[i].start && s < ns[i].start + ns[i].len)
				idx = i;
		if (idx < 0) {
			menu->addChild(createMenuLabel("Keine Note"));
			return;
		}
		GridNote n = ns[idx];
		auto find = [=]() {
			for (int i = (int)self->notes.size() - 1; i >= 0; i--)
				if (self->notes[i].start == n.start && self->notes[i].pitch == n.pitch)
					return i;
			return -1;
		};
		bool sharp = sharpKey(P.key + P.transp, P.scale);
		menu->addChild(createMenuLabel(string::f("%s%d · Step %d · Länge %d", (sharp ? NOTE_SHARP : NOTE_FLAT)[n.pitch % 12], n.pitch / 12 - 1, n.start + 1, n.len)));
		if (t != T_PADS) {
			menu->addChild(createCheckMenuItem("Akzent", "", [=]() { return n.acc; }, [=]() {
				edit([=]() { int i = find(); if (i >= 0) self->notes[i].acc = !self->notes[i].acc; });
			}));
			menu->addChild(createCheckMenuItem("Slide zur nächsten Note", "", [=]() { return n.slide; }, [=]() {
				edit([=]() { int i = find(); if (i >= 0) self->notes[i].slide = !self->notes[i].slide; });
			}));
			menu->addChild(new ui::MenuSeparator);
		}
		for (int vv : {32, 64, 96, 127})
			menu->addChild(createMenuItem(string::f("Velocity %d", vv), "", [=]() {
				edit([=]() { int i = find(); if (i >= 0) self->notes[i].vel = vv; });
			}));
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuItem("Löschen", "Doppelklick", [=]() {
			edit([=]() { int i = find(); if (i >= 0) self->notes.erase(self->notes.begin() + i); });
		}));
	}

	// ---------------------------------------------------------------- buttons (polled by the widget)
	void doUndo() {
		if (!g || undo.empty())
			return;
		Snapshot s = undo.back();
		undo.pop_back();
		redo.push_back({g->pat, s.track});
		Pattern p = g->pat;
		copyTrack(s.pat, p, s.track);
		module->track = s.track;
		handOver(p);
	}
	void doRedo() {
		if (!g || redo.empty())
			return;
		Snapshot s = redo.back();
		redo.pop_back();
		undo.push_back({g->pat, s.track});
		Pattern p = g->pat;
		copyTrack(s.pat, p, s.track);
		module->track = s.track;
		handOver(p);
	}
	int firstBar() { return vs / 16; }
	void doCopy() {
		if (!g)
			return;
		Pattern p = g->pat;
		freezeTrack(p, P, module->track, L);
		copyBar(p, module->track, firstBar(), clip);
	}
	bool doPaste() {
		if (!g || clip.track != module->track || !begin())
			return false;
		bool ok = pasteBar(work, module->track, firstBar(), clip);
		if (ok) {
			touched();
			commit();
		}
		else
			editing = false;
		return ok;
	}
	void doClear() {
		if (!begin())
			return;
		clearTrack(work, module->track);
		touched();
		commit();
	}
	void doGen() {
		if (!g)
			return;
		pushUndo(g->pat, module->track);
		g->gridGen[module->track] = true;
		centerSet[module->track] = false;
	}
	void doFollow() {
		int t = module->track;
		if (!g || t == T_DRUMS)
			return;
		Pattern p = g->pat;
		pushUndo(p, t);
		setChordFollow(p, P, t, !p.chordFollow[t], L);
		handOver(p);
	}
	void doSnapAll() {
		int t = module->track;
		if (t == T_DRUMS || module->snapMode == 0 || !begin())
			return;
		notes = trackNotes(work, P, t, L);
		for (GridNote& n : notes)
			n.pitch = snapPitch(work, P, n.start, n.pitch, module->snapMode);
		apply();
		commit();
	}

	// ---------------------------------------------------------------- drawing
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1) {
			Widget::drawLayer(args, layer);
			return;
		}
		std::shared_ptr<window::Font> bold = APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf"));
		std::shared_ptr<window::Font> mono = APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf"));
		if (!bold || !mono)
			return;
		NVGcontext* vg = args.vg;
		NVGcolor dim = nvgRGB(0x8a, 0x8d, 0x99), line = nvgRGB(0x34, 0x36, 0x3f);
		float wmm = box.size.x / mm2px(1.f), hmm = box.size.y / mm2px(1.f);
		auto txt = [&](std::shared_ptr<window::Font> f, float x, float y, float size, NVGcolor c, const std::string& s, int align, float sp = 0.f) {
			nvgFontFaceId(vg, f->handle);
			nvgFontSize(vg, mm2px(size));
			nvgTextLetterSpacing(vg, mm2px(sp));
			nvgFillColor(vg, c);
			nvgTextAlign(vg, align);
			nvgText(vg, mm2px(x), mm2px(y), s.c_str(), NULL);
		};
		auto rect = [&](float x, float y, float w, float h, NVGcolor c, float r = 0.f) {
			nvgBeginPath(vg);
			if (r > 0.f)
				nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(w), mm2px(h), mm2px(r));
			else
				nvgRect(vg, mm2px(x), mm2px(y), mm2px(w), mm2px(h));
			nvgFillColor(vg, c);
			nvgFill(vg);
		};
		if (!module || !g) {
			txt(bold, wmm / 2.f, hmm / 2.f - 2.f, 3.4f, dim, module ? "NICHT VERBUNDEN" : "GENRA GRID EDITOR", NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE, 0.4f);
			if (module)
				txt(bold, wmm / 2.f, hmm / 2.f + 3.5f, 2.2f, dim, "Editor links oder rechts neben GENRA setzen – oder per Rechtsklick › Verbinden mit…",
				    NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			Widget::drawLayer(args, layer);
			return;
		}
		const Pattern& pat = src();
		int t = module->track;
		NVGcolor col = TRACK_COLORS[t];
		float x0 = gx0(), x1 = gx1(), y0 = gy0(), y1 = gy1(), cw = sw(), rhh = rh();
		int keyPc = ((P.key + P.transp) % 12 + 12) % 12;
		bool sharp = sharpKey(P.key + P.transp, P.scale);

		// ---- header
		static const char* TN[4] = {"DRUMS", "BASS", "MELODY", "PADS"};
		txt(bold, 3.5f, 5.2f, 3.2f, col, TN[t], NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.5f);
		nvgFontSize(vg, mm2px(3.2f));
		nvgTextLetterSpacing(vg, mm2px(0.5f));
		float b4[4];
		nvgTextBounds(vg, 0, 0, TN[t], NULL, b4);
		std::string gl = "· " + profileLabel(pat.trackGenre[t]);
		if (g->trackSel[t] < 0 && !g->trackHasCv(t))
			gl += "  · GLOBAL";
		txt(bold, 3.5f + (b4[2] - b4[0]) / mm2px(1.f) + 2.f, 5.2f, 2.2f, dim, gl, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		static const char* SNAPS[3] = {"SNAP OFF", "SNAP SCALE", "SNAP CHORD"};
		std::string right = pat.fixed[t] ? "BEARBEITET" : "GENERIERT";
		if (t != T_DRUMS)
			right += std::string("  ·  ") + SNAPS[module->snapMode];
		if (t != T_DRUMS && pat.fixed[t] && pat.chordFollow[t])
			right += "  ·  CHORD FOLLOW";
		int b0 = vs / 16 + 1, b1 = (vs + vis) / 16;
		right += b0 == b1 ? string::f("  ·  BAR %d / %d", b0, L / 16) : string::f("  ·  BAR %d-%d / %d", b0, b1, L / 16);
		right += std::string("  ·  ") + noteName(P.key + P.transp, P.key + P.transp, P.scale) + " " + upper(SCALES[P.scale].name);
		txt(mono, wmm - 3.5f, 5.2f, 2.3f, dim, right, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE, 0.2f);

		// ---- chord ruler
		int playStep = g->running && g->step >= 0 ? g->step : -1;
		int curEv = playStep >= 0 ? eventAt(pat, playStep) : -1;
		float color = effColor(P, pat.chordGenre);
		for (int s = vs; s < vs + vis;) {
			int e = clamp(eventAt(pat, s % std::max(1, L)), 0, MAX_EVENTS - 1);
			int e0 = s;
			while (s < vs + vis && clamp(eventAt(pat, s % std::max(1, L)), 0, MAX_EVENTS - 1) == e && (s == e0 || s % std::max(1, L) != 0))
				s++;
			float xa = x0 + (e0 - vs) * cw, xb = x0 + (s - vs) * cw;
			bool act = e == curEv && playStep >= e0 && playStep < s;
			Chord ch = P.extChord ? chordAt(pat, P, e0 % std::max(1, L)) : resolveSpec(pat.prog[pat.ev[e].spec], P, color);
			rect(xa + 0.2f, 7.6f, xb - xa - 0.4f, 4.2f, act ? HARMONY_COLOR : nvgRGB(0x1a, 0x1b, 0x21), 0.6f);
			nvgScissor(vg, mm2px(xa), mm2px(7.6f), mm2px(std::max(0.f, xb - xa - 0.6f)), mm2px(4.2f));
			txt(bold, xa + 1.4f, 10.5f, 2.2f, act ? nvgRGB(0x0a, 0x0a, 0x0d) : dim, chordName(ch, P.key + P.transp, P.scale), NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE);
			nvgResetScissor(vg);
		}

		// ---- rows
		int nr = rows();
		uint16_t colMask[MAX_STEPS * 2] = {0}; // chord tones (pitch classes relative to the key) per visible column
		if (t != T_DRUMS)
			for (int s = vs; s < vs + vis && s < L; s++) {
				Chord ch = chordAt(pat, P, s);
				for (int i = 0; i < ch.n; i++)
					colMask[s - vs] |= (uint16_t)(1 << (((ch.root + ch.iv[i]) % 12 + 12) % 12));
			}
		for (int r = 0; r < nr; r++) {
			float y = y0 + r * rhh;
			if (t == T_DRUMS) {
				rect(x0, y, x1 - x0, rhh, r % 2 ? nvgRGB(0x0c, 0x0d, 0x11) : nvgRGB(0x10, 0x11, 0x16));
				txt(bold, x0 - 1.6f, y + rhh / 2.f, 2.0f, r == module->drumRow ? col : dim, DRUM_SHORT[r], NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, 0.2f);
				continue;
			}
			int p = pitchOfRow(r);
			int pc = ((p - keyPc) % 12 + 12) % 12;
			bool inSc = false;
			for (int i = 0; i < 7; i++)
				inSc |= SCALES[P.scale].iv[i] == pc;
			rect(x0, y, x1 - x0, rhh, inSc ? nvgRGB(0x13, 0x14, 0x1a) : nvgRGB(0x0a, 0x0a, 0x0d));
			// chord tones of the chord at each column
			NVGcolor hc = HARMONY_COLOR;
			hc.a = 0.11f;
			for (int s = vs; s < vs + vis && s < L; s++)
				if (colMask[s - vs] & (1 << pc))
					rect(x0 + (s - vs) * cw, y, cw, rhh, hc);
			// keyboard
			bool black = (p % 12 == 1 || p % 12 == 3 || p % 12 == 6 || p % 12 == 8 || p % 12 == 10);
			rect(x0 - 13.2f, y + 0.1f, black ? 8.f : 12.6f, rhh - 0.2f, black ? nvgRGB(0x2a, 0x2b, 0x33) : nvgRGB(0xc9, 0xcc, 0xd6), 0.3f);
			if (!black || pc == 0)
				txt(bold, x0 - 1.4f, y + rhh / 2.f, std::min(1.7f, rhh * 0.8f), pc == 0 ? TRACK_COLORS[1] : nvgRGB(0x3a, 0x3c, 0x46),
				    string::f("%s%d", (sharp ? NOTE_SHARP : NOTE_FLAT)[p % 12], p / 12 - 1), NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
		}
		// steps beyond the loop
		if (vs + vis > L) {
			float xa = x0 + std::max(0, L - vs) * cw;
			rect(xa, y0, x1 - xa, y1 - y0, nvgRGBA(0, 0, 0, 0x90));
		}
		// vertical lines
		for (int s = 0; s <= vis; s++) {
			int gs = vs + s;
			NVGcolor c = line;
			float w = 0.12f;
			if (gs % 16 == 0) {
				c = nvgRGB(0x7a, 0x7e, 0x8c);
				w = 0.45f;
			}
			else if (gs % 4 == 0) {
				c = nvgRGB(0x4a, 0x4d, 0x58);
				w = vis > 64 ? 0.2f : 0.3f;
			}
			else if (vis > 64)
				continue;
			nvgBeginPath(vg);
			nvgMoveTo(vg, mm2px(x0 + s * cw), mm2px(y0));
			nvgLineTo(vg, mm2px(x0 + s * cw), mm2px(y1));
			nvgStrokeColor(vg, c);
			nvgStrokeWidth(vg, mm2px(w));
			nvgStroke(vg);
		}
		for (int b = vs / 16; b < (vs + vis) / 16; b++)
			txt(mono, x0 + (b * 16 - vs) * cw + 1.f, y1 + 3.f, 1.8f, dim, string::f("BAR %d", b + 1), NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.2f);
		// hover
		if (hoverStep >= vs && hoverStep < vs + vis && hoverRow >= 0 && hoverRow < nr && drag == D_NONE) {
			NVGcolor hc = nvgRGBA(0xff, 0xff, 0xff, 0x14);
			rect(x0 + (hoverStep - vs) * cw, y0 + hoverRow * rhh, cw, rhh, hc);
		}

		// ---- notes / hits + velocity lane
		float vy0 = glayout::VELLANE[0], vy1 = glayout::VELLANE[1], vh = vy1 - vy0;
		rect(x0, vy0, x1 - x0, vh, nvgRGB(0x0c, 0x0d, 0x11));
		txt(bold, x0 - 1.6f, vy0 + vh / 2.f - (t == T_DRUMS ? 1.2f : 0.f), 1.8f, dim, "VEL", NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, 0.3f);
		auto velBar = [&](int s, int vel) {
			float x = x0 + (s - vs) * cw + cw * 0.3f, h = vel / 127.f * (vh - 1.2f);
			rect(x, vy1 - h - 0.4f, std::max(0.5f, cw * 0.4f), h, col, 0.3f);
		};
		if (t == T_DRUMS) {
			txt(bold, x0 - 1.6f, vy0 + vh / 2.f + 1.6f, 1.6f, col, DRUM_SHORT[module->drumRow], NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, 0.3f);
			NVGcolor sc = col;
			sc.a = 0.5f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(x0 - 14.5f), mm2px(y0 + module->drumRow * rhh + 0.4f), mm2px(x1 - x0 + 14.5f), mm2px(rhh - 0.8f), mm2px(0.6f));
			nvgStrokeColor(vg, sc);
			nvgStrokeWidth(vg, mm2px(0.3f));
			nvgStroke(vg);
			for (int v = 0; v < NUM_DRUMS; v++)
				for (int s = vs; s < vs + vis && s < L; s++) {
					Hit h = drumHit(pat, P, v, s, L, false);
					if (!h.on)
						continue;
					float x = x0 + (s - vs) * cw + std::min(0.45f, cw * 0.08f), y = y0 + v * rhh + 0.9f;
					float w = cw - 2.f * std::min(0.45f, cw * 0.08f), hh = rhh - 1.8f;
					NVGcolor c = col;
					c.a = 0.35f + 0.65f * h.vel;
					rect(x, y, w, hh, c, std::min(0.7f, w * 0.3f));
					if (h.vel < 0.45f) {
						nvgBeginPath(vg);
						nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(w), mm2px(hh), mm2px(std::min(0.7f, w * 0.3f)));
						nvgStrokeColor(vg, col);
						nvgStrokeWidth(vg, mm2px(0.25f));
						nvgStroke(vg);
					}
					for (int k = 1; k < h.ratchet; k++) {
						float xx = x + k * w / h.ratchet;
						nvgBeginPath(vg);
						nvgMoveTo(vg, mm2px(xx), mm2px(y + 0.6f));
						nvgLineTo(vg, mm2px(xx), mm2px(y + hh - 0.6f));
						nvgStrokeColor(vg, nvgRGB(0x07, 0x08, 0x0a));
						nvgStrokeWidth(vg, mm2px(0.35f));
						nvgStroke(vg);
					}
					if (v == module->drumRow)
						velBar(s, (int)std::round(h.vel * 127.f));
				}
		}
		else {
			std::vector<GridNote> ns = editing ? notes : trackNotes(pat, P, t, L);
			nvgScissor(vg, mm2px(x0), mm2px(y0), mm2px(x1 - x0), mm2px(y1 - y0));
			for (size_t i = 0; i < ns.size(); i++) {
				const GridNote& n = ns[i];
				if (n.start + n.len <= vs || n.start >= vs + vis)
					continue;
				int r = rowOfPitch(n.pitch);
				float x = x0 + (n.start - vs) * cw + 0.3f, w = n.len * cw - 0.6f;
				NVGcolor c = col;
				c.a = 0.4f + 0.6f * n.vel / 127.f;
				if (r < 0 || r >= PIANO_ROWS) {
					// outside the window: small marker at the edge
					float y = r < 0 ? y0 : y1 - 0.8f;
					rect(x, y, w, 0.8f, c);
					continue;
				}
				float y = y0 + r * rhh + 0.25f;
				bool sel = editing && (int)i == dragIdx;
				rect(x, y, w, rhh - 0.5f, c, 0.6f);
				if (sel) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(w), mm2px(rhh - 0.5f), mm2px(0.6f));
					nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
					nvgStrokeWidth(vg, mm2px(0.35f));
					nvgStroke(vg);
				}
				rect(x + w - 1.2f, y + 0.5f, 0.5f, rhh - 1.5f, nvgRGBA(0x07, 0x08, 0x0a, 0x80));
				if (n.acc)
					txt(bold, x + 0.9f, y + (rhh - 0.5f) / 2.f, std::min(1.8f, rhh), nvgRGB(0x07, 0x08, 0x0a), ">", NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				if (n.slide && i + 1 < ns.size()) {
					const GridNote& nx = ns[i + 1];
					float xa = x + w, ya = y + (rhh - 0.5f) / 2.f;
					float xb = x0 + (nx.start - vs) * cw + 0.6f, yb = y0 + rowOfPitch(nx.pitch) * rhh + rhh / 2.f;
					nvgBeginPath(vg);
					nvgMoveTo(vg, mm2px(xa), mm2px(ya));
					nvgBezierTo(vg, mm2px(xa + 1.5f), mm2px(ya), mm2px(xb - 1.5f), mm2px(yb), mm2px(xb), mm2px(yb));
					nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
					nvgStrokeWidth(vg, mm2px(0.35f));
					nvgStroke(vg);
				}
			}
			nvgResetScissor(vg);
			int lastVelStep = -1;
			for (const GridNote& n : ns)
				if (n.start >= vs && n.start < vs + vis && n.start != lastVelStep) {
					velBar(n.start, n.vel);
					lastVelStep = n.start;
				}
		}
		// ---- playhead
		if (playStep >= vs && playStep < vs + vis) {
			float px = x0 + (playStep - vs) * cw;
			rect(px, y0, cw, y1 - y0, nvgRGBA(0xff, 0xff, 0xff, 0x0f));
			nvgBeginPath(vg);
			nvgMoveTo(vg, mm2px(px), mm2px(y0 - 1.f));
			nvgLineTo(vg, mm2px(px), mm2px(y1));
			nvgStrokeColor(vg, nvgRGB(0xff, 0xff, 0xff));
			nvgStrokeWidth(vg, mm2px(0.3f));
			nvgStroke(vg);
		}
		Widget::drawLayer(args, layer);
	}
};

// ================================================================= widget
struct GenraGridWidget : ModuleWidget {
	GridDisplay* display = nullptr;
	bool prev[GenraGrid::PARAMS_LEN] = {};
	float flash[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};

	GenraGridWidget(GenraGrid* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GenraGrid.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };

		display = createWidget<GridDisplay>(mm2px(Vec(glayout::DISPLAY[0], glayout::DISPLAY[1])));
		display->box.size = mm2px(Vec(glayout::DISPLAY[2], glayout::DISPLAY[3]));
		display->module = module;
		addChild(display);

		addParam(createLightParamCentered<VCVLightBezel<GOrange>>(P(glayout::TRACK_0), module, GenraGrid::TRACK_PARAM + 0, GenraGrid::TRACK_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<GPink>>(P(glayout::TRACK_1), module, GenraGrid::TRACK_PARAM + 1, GenraGrid::TRACK_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<GCyan>>(P(glayout::TRACK_2), module, GenraGrid::TRACK_PARAM + 2, GenraGrid::TRACK_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<GPurple>>(P(glayout::TRACK_3), module, GenraGrid::TRACK_PARAM + 3, GenraGrid::TRACK_LIGHT + 3));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(glayout::BAR), module, GenraGrid::BAR_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(glayout::ZOOM), module, GenraGrid::ZOOM_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(glayout::PITCH), module, GenraGrid::PITCH_PARAM));
		addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(P(glayout::AUTO), module, GenraGrid::AUTO_PARAM, GenraGrid::AUTO_LIGHT));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(glayout::VEL), module, GenraGrid::VEL_PARAM));
		addParam(createParamCentered<RoundSmallBlackKnob>(P(glayout::LEN), module, GenraGrid::LEN_PARAM));
		addParam(createParamCentered<VCVBezel>(P(glayout::SNAP), module, GenraGrid::SNAP_PARAM));
		const float* sl[3] = {glayout::SNAPLED_0, glayout::SNAPLED_1, glayout::SNAPLED_2};
		for (int i = 0; i < 3; i++)
			addChild(createLightCentered<TinyLight<WhiteLight>>(P(sl[i]), module, GenraGrid::SNAP_LIGHT + i));
		addParam(createLightParamCentered<VCVLightBezel<GTeal>>(P(glayout::FOLLOW), module, GenraGrid::FOLLOW_PARAM, GenraGrid::FOLLOW_LIGHT));
		const float* ed[6] = {glayout::UNDO, glayout::REDO, glayout::COPY, glayout::PASTE, glayout::CLEAR, glayout::GEN};
		for (int i = 0; i < 6; i++)
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(ed[i]), module, GenraGrid::UNDO_PARAM + i, GenraGrid::UNDO_LIGHT + i));
		addChild(createLightCentered<SmallLight<GMint>>(P(glayout::LINKLED), module, GenraGrid::LINK_LIGHT));
	}

	bool pressed(int id) {
		bool on = module->params[id].getValue() > 0.5f;
		bool p = on && !prev[id];
		prev[id] = on;
		return p;
	}

	void step() override {
		ModuleWidget::step();
		GenraGrid* m = dynamic_cast<GenraGrid*>(module);
		if (!m || !display)
			return;
		GridDisplay* d = display;
		for (int t = 0; t < 4; t++)
			if (pressed(GenraGrid::TRACK_PARAM + t) && !d->editing)
				m->track = t;
		if (pressed(GenraGrid::SNAP_PARAM)) {
			if (APP->window->getMods() & GLFW_MOD_SHIFT)
				d->doSnapAll();
			else
				m->snapMode = (m->snapMode + 1) % 3;
		}
		if (pressed(GenraGrid::FOLLOW_PARAM))
			d->doFollow();
		static const int EDIT_IDS[6] = {GenraGrid::UNDO_PARAM, GenraGrid::REDO_PARAM, GenraGrid::COPY_PARAM,
		                                GenraGrid::PASTE_PARAM, GenraGrid::CLEAR_PARAM, GenraGrid::GEN_PARAM};
		for (int i = 0; i < 6; i++) {
			if (!pressed(EDIT_IDS[i]) || d->editing)
				continue;
			bool ok = true;
			switch (i) {
				case 0: ok = !d->undo.empty(); d->doUndo(); break;
				case 1: ok = !d->redo.empty(); d->doRedo(); break;
				case 2: d->doCopy(); break;
				case 3: ok = d->doPaste(); break;
				case 4: d->doClear(); break;
				case 5: d->doGen(); break;
			}
			if (ok && d->g)
				flash[i] = 1.f;
		}
		// lights
		float dt = APP->window->getLastFrameDuration();
		if (!std::isfinite(dt) || dt <= 0.f || dt > 0.5f)
			dt = 1.f / 60.f;
		for (int t = 0; t < 4; t++)
			m->lights[GenraGrid::TRACK_LIGHT + t].setBrightness(m->track == t ? 1.f : 0.f);
		m->lights[GenraGrid::AUTO_LIGHT].setBrightness(m->params[GenraGrid::AUTO_PARAM].getValue());
		for (int i = 0; i < 3; i++)
			m->lights[GenraGrid::SNAP_LIGHT + i].setBrightness(m->snapMode == i ? 1.f : 0.f);
		float fl = 0.f;
		if (d->g && m->track != T_DRUMS)
			fl = d->g->pat.fixed[m->track] ? (d->g->pat.chordFollow[m->track] ? 1.f : 0.f) : 0.25f;
		m->lights[GenraGrid::FOLLOW_LIGHT].setBrightness(fl);
		m->lights[GenraGrid::LINK_LIGHT].setBrightness(d->g ? 1.f : 0.f);
		for (int i = 0; i < 6; i++) {
			flash[i] = std::max(0.f, flash[i] - dt * 4.f);
			m->lights[GenraGrid::UNDO_LIGHT + i].setBrightness(flash[i]);
		}
	}

	void appendContextMenu(Menu* menu) override {
		GenraGrid* m = dynamic_cast<GenraGrid*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Verbinden mit…"));
		menu->addChild(createCheckMenuItem("Automatisch (angedocktes GENRA)", "", [=]() { return m->linkedId < 0; }, [=]() { m->linkedId = -1; }));
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* mm = APP->engine->getModule(id);
			if (!mm || mm->model != modelGenra)
				continue;
			Genra* gm = static_cast<Genra*>(mm);
			std::string label = "GENRA · " + profileLabel(gm->curGenre) + string::f("  (#%lld)", (long long)id);
			menu->addChild(createCheckMenuItem(label, "", [=]() { return m->linkedId == id; }, [=]() { m->linkedId = id; }));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Bedienung"));
		menu->addChild(createMenuLabel("Klick: Note / Schlag setzen · Ziehen: verschieben, Länge am rechten Rand"));
		menu->addChild(createMenuLabel("Doppelklick: löschen · Mausrad: Velocity · Rechtsklick: Akzent, Slide, Roll"));
	}
};

Model* modelGenraGrid = createModel<GenraGrid, GenraGridWidget>("GenraGrid");
