// SPDX-License-Identifier: GPL-3.0-only
// ABLAUF MINI – song parts on 16 pads for GENRA or GENRA BOX (docs/GENRA_BOX.md). The pads keep the notes only
// (pattern, track genres, length), no sounds. Works through GENRA's arranger mailboxes like ABLAUF (snapshot, cue),
// one arranger per GENRA (arrOwner). All of its own logic runs on the UI thread (AblaufMiniWidget::step).
// 2.31: the track buttons Keep / Re-roll / Vary (as ABLAUF's), SONG (MAKE SONG: the pads filled with the genre's song
// form, played one after another over their rows).
// 2.31.1: the rules of the big ABLAUF (it is the host, MINI follows): a part button makes a new variant on the next
// free pad at once, from the pattern that plays (while one of this session's variants plays unchanged: from the same
// source); an empty pad stores what plays; SONG takes the family's variants first; break / intro bass; GENRA's
// MAKE SONG pad; MIDI into the export folder without a dialog.
#include "../genra/GenraModule.hpp"
#include "AblaufMiniLayout.hpp"

namespace amini {

namespace L = minilayout;
static const int NUM_PADS = 16;
static const NVGcolor SEC_COLORS[NUM_SECTIONS] = {nvgRGB(0x6f, 0x86, 0xb8), nvgRGB(0x9b, 0x7b, 0xd4), nvgRGB(0xe0, 0xa5, 0x26),
                                                  nvgRGB(0xff, 0x5a, 0x4f), nvgRGB(0x3f, 0xae, 0x9a), nvgRGB(0x8a, 0x8d, 0x99)};
static const NVGcolor ROOT_COLOR = nvgRGB(0x9a, 0x9c, 0xaa);

template <int R, int G, int B>
struct Light : GrayModuleLightWidget {
	Light() { addBaseColor(nvgRGB(R, G, B)); }
};

static void setName(Slot& s, const std::string& n) {
	std::memset(s.name, 0, sizeof(s.name));
	std::memcpy(s.name, n.data(), std::min(n.size(), sizeof(s.name) - 1));
}

struct AblaufMini : Module {
	enum ParamId { ENUMS(PART_PARAM, NUM_SECTIONS), ENUMS(TRACK_PARAM, 4), UNDO_PARAM, CLEAR_PARAM, SONG_PARAM, PARAMS_LEN };
	enum InputId { NEXT_INPUT, RESET_INPUT, INPUTS_LEN };
	enum OutputId { DROP_OUTPUT, END_OUTPUT, OUTPUTS_LEN };
	enum LightId { ENUMS(PART_LIGHT, NUM_SECTIONS), ENUMS(TRACK_LIGHT, 4), UNDO_LIGHT, CLEAR_LIGHT, LINK_LIGHT, SONG_LIGHT, LIGHTS_LEN };

	// ---- kept with the patch (UI thread)
	Slot slots[NUM_PADS];
	PartOpts partOpts;       // the bass styles of BREAK / INTRO (-1: one at random on every press), as ABLAUF's
	int64_t linkedId = -1;   // -1: the docked GENRA / GENRA BOX

	// ---- UI thread
	struct Undo {
		int pad;        // -1: the whole pad set (MAKE SONG)
		Slot before;
		std::shared_ptr<std::vector<Slot>> all;
	};
	// ---- MAKE SONG (UI thread; the song is kept with the patch)
	struct Part {
		int pad;
		int bars;
		bool fill;
	};
	std::vector<Part> song;
	struct Play {
		bool on = false;
		int pos = -1;          // the part playing
		int64_t start = 0;     // its first bar (GENRA's bar index)
		uint32_t waitId = 0;   // the cue handed to GENRA, not yet applied
		int waitPos = -1;
		bool waitStop = false;
	} play;
	uint32_t cueCounter = 1;
	bool songAfterStore = false; // SONG / GENRA's MAKE SONG with nothing stored: what plays is stored first
	// ---- the part buttons' session (as ABLAUF's): the source the variants of one go come from
	struct Session {
		bool active = false;
		Slot source;
		Params P;
		std::vector<std::pair<int, uint32_t>> made; // pad, its version when made
		void clear() {
			active = false;
			made.clear();
		}
	} session;
	uint32_t slotVer[NUM_PADS] = {};
	int pendingPart = -1;    // a part button waits for GENRA's snapshot
	float partFlash[NUM_SECTIONS] = {};
	std::vector<Undo> undo;
	int pendingPad = -1;     // an empty pad waits for GENRA's snapshot (stores it)
	int pendingCue = -1;     // waits for the cue mailbox
	bool clearArmed = false;
	std::string msg;
	float msgTime = 0.f;
	Rng rng;
	Genra* attached = nullptr;
	follow::Ref genraRef;    // for the audio thread (END, DROP); nulled when GENRA goes (follow::gone)

	// ---- audio thread -> UI thread
	std::atomic<uint32_t> nextCount{0}, resetCount{0};
	dsp::SchmittTrigger nextTrig, resetTrig;

	AblaufMini() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		for (int k = 0; k < NUM_SECTIONS; k++)
			configButton(PART_PARAM + k, std::string(SECTION_NAMES[k]) + " from the playing pattern (each press: a new variant on the next free pad)");
		static const char* TN[4] = {"drums", "bass", "melody", "pads"};
		for (int t = 0; t < 4; t++) {
			configSwitch(TRACK_PARAM + t, 0.f, 2.f, t < 2 ? 1.f : 0.f, std::string("New parts: ") + TN[t],
			             {"Keep", "Re-roll", "Vary (a part that comes again: its last variant, a little changed)"});
			paramQuantities[TRACK_PARAM + t]->randomizeEnabled = false;
		}
		configButton(UNDO_PARAM, "Undo the last pad change");
		configButton(CLEAR_PARAM, "Clear: then tap the pad to clear");
		configButton(SONG_PARAM, "SONG: make a song of the genre's form from the playing pad (or ROOT) and play it (again: stop)");
		configInput(NEXT_INPUT, "Next pad (trigger: the next used pad at the end of the pattern)");
		configInput(RESET_INPUT, "Reset (trigger: back to ROOT at the end of the pattern)");
		configOutput(DROP_OUTPUT, "Drop (gate while a DROP pad plays)");
		configOutput(END_OUTPUT, "End of the pattern (trigger)");
		rng = Rng(random::u64());
		genraRef.owner = this;
	}
	~AblaufMini() { follow::gone(this); }

	void process(const ProcessArgs& args) override {
		if (nextTrig.process(inputs[NEXT_INPUT].getVoltage(), 0.1f, 1.f))
			nextCount++;
		if (resetTrig.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 1.f))
			resetCount++;
		Module* gm = genraRef.to.load(std::memory_order_acquire);
		float drop = 0.f, end = 0.f;
		if (gm) {
			Genra* g = static_cast<Genra*>(gm);
			if (g->arrOwner.load(std::memory_order_relaxed) == id) {
				drop = g->arrSecPub.load(std::memory_order_relaxed) == SEC_DROP && g->running ? 10.f : 0.f;
				end = g->followPub[(args.frame + 1) & 1][Genra::END_OUTPUT][0];
			}
		}
		outputs[DROP_OUTPUT].setVoltage(drop);
		outputs[END_OUTPUT].setVoltage(end);
	}

	void onRemove(const RemoveEvent& e) override {
		letGo();
		follow::gone(this);
	}

	// ------------------------------------------------------------ GENRA (UI thread)
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
	bool busy = false; // the GENRA found is driven by another arranger
	Genra* genra() {
		busy = false;
		Genra* g = findGenra();
		if (!g)
			return nullptr;
		int64_t own = g->arrOwner.load();
		if (own == id)
			return g;
		if (own >= 0) {
			Module* o = APP->engine->getModule(own);
			if (o && (o->model == modelAblauf || o->model == modelAblaufMini)) {
				busy = true;
				return nullptr;
			}
		}
		if (g->arrOwner.compare_exchange_strong(own, id))
			return g;
		busy = true;
		return nullptr;
	}
	void letGo() {
		Genra* g = attached;
		attached = nullptr;
		follow::point(genraRef, this, nullptr);
		if (!g)
			return;
		std::lock_guard<std::mutex> lk(genraRegMutex());
		if (!genraRegistry().count(g))
			return;
		int64_t own = id;
		if (!g->arrOwner.compare_exchange_strong(own, -1) && own >= 0)
			return;
		g->cueCancel();
		g->arrAlive.store(1e-6f);
	}

	void say(const std::string& s) {
		msg = s;
		msgTime = 2.5f;
	}
	unsigned rerollMask() { // Re-roll and Vary (Vary rolls a part that has no variant yet)
		unsigned m = 0;
		for (int t = 0; t < 4; t++)
			if (params[TRACK_PARAM + t].getValue() > 0.5f)
				m |= 1u << t;
		return m;
	}
	unsigned varyMask() {
		unsigned m = 0;
		for (int t = 0; t < 4; t++)
			if (params[TRACK_PARAM + t].getValue() > 1.5f)
				m |= 1u << t;
		return m;
	}
	static int nameNumber(const char* name, const std::string& base) {
		std::string n = name;
		if (n.size() > base.size() + 1 && n.compare(0, base.size() + 1, base + " ") == 0)
			return std::atoi(n.c_str() + base.size() + 1);
		return 0;
	}
	/** The latest variant of part k in a family (the highest number), skip excluded; -1 none (as ABLAUF's). */
	int lastVariant(int k, int family, int skip) {
		int best = -1, top = -1;
		if (family < 0)
			return -1;
		for (int i = 0; i < NUM_PADS; i++) {
			if (i == skip || !slots[i].used || slots[i].family != family || slots[i].tag != k)
				continue;
			int n = nameNumber(slots[i].name, SECTION_NAMES[k]);
			if (n > top)
				top = n, best = i;
		}
		return best;
	}
	/** base + the next free number in a family (every family counts on its own, as ABLAUF's). */
	std::string nextName(const std::string& base, int skip, int family) {
		int top = 0;
		for (int i = 0; i < NUM_PADS; i++)
			if (i != skip && slots[i].used && slots[i].family == family)
				top = std::max(top, nameNumber(slots[i].name, base));
		return base + " " + std::to_string(top + 1);
	}
	/** A new family for a pattern stored from GENRA: one above all families (ROOT included). */
	int newFamily() {
		int top = 0;
		for (int i = 0; i < NUM_PADS; i++)
			if (slots[i].used)
				top = std::max(top, (int) slots[i].family);
		return top + 1;
	}
	/** The next free pad: 02-16 first, ROOT (01) last. */
	int freePad() {
		for (int i = 1; i < NUM_PADS; i++)
			if (!slots[i].used)
				return i;
		return slots[0].used ? -1 : 0;
	}
	/** The pad a family started with (stored from GENRA), or i itself. */
	int familySource(int i) {
		if (i < 0 || i >= NUM_PADS || !slots[i].used)
			return -1;
		int f = slots[i].family;
		if (f >= 0)
			for (int k = 0; k < NUM_PADS; k++)
				if (slots[k].used && slots[k].family == f && slots[k].tag < 0)
					return k;
		return i;
	}
	/** The pad GENRA plays unchanged (no NEW, MUTATE or edit since), -1 none. */
	int playingPad(Genra* g) {
		int pl = g->arrSlotPub.load();
		if (pl >= 0 && pl < NUM_PADS && slots[pl].used && g->patChanges.load() == g->patChangesAtCue.load())
			return pl;
		return -1;
	}
	void remember(int i) {
		undo.push_back({i, slots[i], nullptr});
		if (undo.size() > 32)
			undo.erase(undo.begin());
		slotVer[i]++;
	}
	void rememberAll() {
		Undo u;
		u.pad = -1;
		u.all = std::make_shared<std::vector<Slot>>(slots, slots + NUM_PADS);
		undo.push_back(u);
		if (undo.size() > 32)
			undo.erase(undo.begin());
		for (int i = 0; i < NUM_PADS; i++)
			slotVer[i]++;
	}

	/** The bar line where GENRA's loop starts again. */
	int64_t loopEndBar(Genra* g) {
		int L = std::max(16, g->lengthSteps());
		int s = std::max(0, g->step);
		int barIn = (s % L) / 16;
		return g->barPub.load() + (L / 16 - barIn);
	}
	bool cue(int i, Genra* g) {
		if (!g->cueBegin()) {
			pendingCue = i;
			return false;
		}
		Genra::ArrCue& c = g->cue;
		c.slot = slots[i];
		c.mute = 0;
		c.fill = false;
		c.rowBars = 0;
		c.now = !g->running;
		c.stop = false;
		c.atBar = loopEndBar(g);
		c.row = -1;
		c.tag = slots[i].tag;
		c.slotIdx = i;
		c.id = cueCounter++;
		g->cueCommit();
		pendingCue = -1;
		return true;
	}

	void requestSnap(Genra* g) {
		int e = 0;
		if (!g->snapState.compare_exchange_strong(e, 1) && e == 2) {
			e = 2;
			g->snapState.compare_exchange_strong(e, 1);
		}
	}
	/** A pad tapped (UI thread): a used pad plays at the end of the pattern, an empty one stores what plays. */
	void tap(int i) {
		Genra* g = attached;
		if (clearArmed) {
			clearArmed = false;
			if (slots[i].used) {
				remember(i);
				slots[i] = Slot();
				say(std::string(i == 0 ? "ROOT" : "Pad") + " cleared");
			}
			return;
		}
		if (!g) {
			say(busy ? "GENRA is driven by another ABLAUF" : "No GENRA connected");
			return;
		}
		if (play.on)
			stopSong(g, false); // a pad by hand: the song lets go
		if (slots[i].used) {
			cue(i, g);
			return;
		}
		pendingPad = i;
		requestSnap(g);
	}
	/** GENRA's pattern stored on pad i: a family of its own (ROOT on pad 01, else GENRA n). */
	void storeSnap(Genra* g, int i) {
		remember(i);
		slots[i] = g->snap;
		slots[i].used = true;
		slots[i].tag = -1;
		int f = newFamily();
		slots[i].family = (int8_t) f;
		setName(slots[i], i == 0 ? std::string("ROOT") : "GENRA " + std::to_string(f));
	}
	void snapArrived(Genra* g) {
		if (pendingPad >= 0) {
			int i = pendingPad;
			pendingPad = -1;
			if (!slots[i].used) {
				storeSnap(g, i);
				session.clear();
				say(std::string(slots[i].name) + " stored");
				if (songAfterStore) { // what plays is this pad now: the song comes from it
					songAfterStore = false;
					g->arrSlotPub.store(i);
					g->patChangesAtCue.store(g->patChanges.load());
					makeSong(g, i);
				}
			}
		}
		if (pendingPart >= 0) {
			int k = pendingPart;
			pendingPart = -1;
			int pl = playingPad(g);
			session.clear();
			if (!slots[0].used && pl < 0) { // nothing stored yet: what plays becomes ROOT, the parts its family
				storeSnap(g, 0);
				session.source = slots[0];
			}
			else {
				session.source = g->snap;
				setName(session.source, pl >= 0 ? std::string(slots[pl].name) : "");
				session.source.family = pl >= 0 ? slots[pl].family : (int8_t) -1; // the family of what it comes from
			}
			session.P = g->snapP;
			session.active = true;
			makePart(k, g);
		}
	}
	// ------------------------------------------------------------ the part buttons (ABLAUF's rules)
	void partButton(int k, Genra* g) {
		partFlash[k] = 0.3f;
		if (!g) {
			say(busy ? "GENRA is driven by another ABLAUF" : "No GENRA connected");
			return;
		}
		// still on one of this session's variants (unchanged)? then the same source: "this pattern is great – an
		// intro, a break, ..." always starts from the great one; every press writes a new variant on the next free pad
		bool same = false;
		if (session.active) {
			int pl = playingPad(g);
			for (auto& mv : session.made)
				if (pl >= 0 && mv.first == pl && slotVer[pl] == mv.second)
					same = true;
		}
		if (same) {
			makePart(k, g);
			return;
		}
		pendingPart = k;
		requestSnap(g);
	}
	void makePart(int k, Genra* g) {
		int t = freePad();
		if (t < 0) {
			say("All 16 pads are full");
			return;
		}
		remember(t);
		makeSection(session.source, slots[t], k, rerollMask(), session.P, rng, partOpts);
		// VARY: a part that has a variant already comes again from that one, a little changed
		int prev = varyMask() ? lastVariant(k, slots[t].family, t) : -1;
		if (prev >= 0)
			varySection(slots[prev], slots[t], varyMask(), session.P, rng);
		slots[t].used = true;
		setName(slots[t], nextName(SECTION_NAMES[k], t, slots[t].family));
		slotVer[t]++;
		session.made.push_back({t, slotVer[t]});
		say(std::string(slots[t].name) + string::f(" \xe2\x86\x92 pad %02d", t + 1));
		if (!play.on)
			cue(t, g);
	}
	// ------------------------------------------------------------ MAKE SONG
	/** SONG: a song of the genre's form from the playing pad's family (or ROOT's), played from the top. Nothing
	 * stored: what GENRA plays is stored first. Again: stop. */
	void songButton(Genra* g) {
		if (!g) {
			say(busy ? "GENRA is driven by another ABLAUF" : "No GENRA connected");
			return;
		}
		if (play.on) {
			stopSong(g, true);
			say("Song stopped");
			return;
		}
		songFromGenra(g, true);
	}
	/** The source of a song: the pad that plays unchanged, else ROOT; nothing (ROOT empty, or GENRA's MAKE SONG pad
	 * with a changed loop): what plays is stored on a free pad first (as ABLAUF's makeSongFromGenra). */
	void songFromGenra(Genra* g, bool rootFallback) {
		if (pendingPad >= 0 || pendingPart >= 0)
			return; // a snapshot is on its way
		int pl = playingPad(g);
		if (pl >= 0) {
			makeSong(g, pl);
			return;
		}
		if (rootFallback && slots[0].used) {
			makeSong(g, 0);
			return;
		}
		int t = slots[0].used ? freePad() : 0;
		if (t < 0) {
			say("Pads full \xe2\x80\x93 clear a pad for SONG");
			return;
		}
		pendingPad = t;
		songAfterStore = true;
		requestSnap(g);
	}
	/** As ABLAUF's MAKE SONG: every part of the form from the family's variants (lowest number first; a part that
	 * comes again takes the next one) or made new like the part buttons on a free pad; pads full: the original. */
	void makeSong(Genra* g, int sel) {
		int src = familySource(sel);
		if (src < 0) {
			say("Tap a pad with a pattern first");
			return;
		}
		int fam = slots[src].family;
		const Genre& G = prof(clamp((int) slots[src].pat.trackGenre[T_DRUMS], 0, numProfiles() - 1));
		int form = clamp(songFormForGenre(G.name), 0, NUM_SONG_FORMS - 1);
		const SongForm& F = SONG_FORMS[form];
		std::vector<int> have[NUM_SECTIONS];
		if (fam >= 0)
			for (int i = 0; i < NUM_PADS; i++)
				if (i != src && slots[i].used && slots[i].family == fam && slots[i].tag >= 0 && slots[i].tag < NUM_SECTIONS)
					have[slots[i].tag].push_back(i);
		for (int k = 0; k < NUM_SECTIONS; k++)
			std::stable_sort(have[k].begin(), have[k].end(), [&](int a, int b) {
				return nameNumber(slots[a].name, SECTION_NAMES[k]) < nameNumber(slots[b].name, SECTION_NAMES[k]);
			});
		rememberAll();
		Params P = g->buildParams(false);
		P.lengthBars = LENGTH_BARS[clamp((int) slots[src].lengthIdx, 0, 3)];
		P = withMods(P, slots[src].mods);
		int taken[NUM_SECTIONS] = {}, fallback = 0;
		song.clear();
		for (int k = 0; k < F.n; k++) {
			int sec = clamp(F.sec[k], 0, NUM_SECTIONS - 1);
			int pad = -1;
			if (taken[sec] < (int) have[sec].size())
				pad = have[sec][taken[sec]];
			else {
				int t = freePad();
				if (t >= 0) { // a new variant, like the part button, on the next free pad
					makeSection(slots[src], slots[t], sec, rerollMask(), P, rng, partOpts);
					// VARY: the part's second time comes from its first, a little changed (not a new roll)
					if (varyMask() && !have[sec].empty())
						varySection(slots[have[sec].back()], slots[t], varyMask(), P, rng);
					slots[t].used = true;
					setName(slots[t], nextName(SECTION_NAMES[sec], t, slots[t].family));
					have[sec].push_back(t);
					pad = t;
				}
			}
			if (pad < 0)
				pad = src, fallback++; // the pads are full: the original plays there
			taken[sec]++;
			int bars = F.bars[k];
			if (sec == SEC_BUILD && G.buildBars > 0) // the genre's build length
				bars = G.buildBars;
			song.push_back({pad, bars, false});
		}
		// a fill leads out of a verse or a drop into another part
		for (size_t k = 0; k + 1 < song.size(); k++) {
			int a = F.sec[k], b = F.sec[k + 1];
			song[k].fill = (a == SEC_VERSE || a == SEC_DROP) && b != a;
		}
		session.clear();
		int total = 0;
		for (const Part& p : song)
			total += p.bars;
		std::string m = string::f("Song: %s, %d bars", F.name, total);
		if (fallback)
			m += string::f(" \xe2\x80\x93 pads full: %d parts play the original", fallback);
		say(m);
		msgTime = 4.f;
		startSong(g);
	}
	bool cueSong(Genra* g, int pos, int64_t at, bool now) {
		if (!g->cueBegin())
			return false;
		Genra::ArrCue& c = g->cue;
		bool stop = pos >= (int) song.size();
		c.stop = stop;
		c.now = now;
		c.atBar = at;
		c.id = cueCounter++;
		if (!stop) {
			const Part& p = song[pos];
			c.slot = slots[p.pad];
			c.mute = 0;
			c.fill = p.fill;
			c.rowBars = p.bars; // the part runs its course over its row (build, break, intro, outro)
			c.row = (int8_t) pos;
			c.tag = slots[p.pad].tag;
			c.slotIdx = p.pad;
		}
		g->cueCommit();
		play.waitId = c.id;
		play.waitPos = pos;
		play.waitStop = stop;
		return true;
	}
	void startSong(Genra* g) {
		if (song.empty())
			return;
		g->cueCancel();
		play = Play();
		play.on = true;
		if (cueSong(g, 0, 0, true))
			g->arrRunReq = 1;
		else
			play.on = false;
	}
	void stopSong(Genra* g, bool stopGenra) {
		if (g && play.waitId)
			g->cueCancel();
		if (g && stopGenra)
			g->arrRunReq = 2;
		play = Play();
	}
	/** UI thread, every frame while the song plays. */
	void songStep(Genra* g) {
		if (!play.on)
			return;
		if (play.waitId && g->cueDone.load() == play.waitId) { // the part we handed over plays now
			play.waitId = 0;
			if (play.waitStop) {
				play = Play();
				return;
			}
			play.pos = play.waitPos;
			play.start = g->rowStartPub.load();
		}
		if (!g->running && play.pos >= 0) { // stopped on GENRA itself
			stopSong(g, false);
			return;
		}
		if (play.waitId || play.pos < 0)
			return;
		// hand over the next part (or the end) for the bar where this one ends
		cueSong(g, play.pos + 1, play.start + song[play.pos].bars, false);
	}

	/** NEXT: the next used pad after the one playing (or queued). */
	void next(Genra* g) {
		int from = g->arrSlotPub.load();
		if (g->cueState.load() == 1)
			from = g->cue.slotIdx;
		for (int k = 1; k <= NUM_PADS; k++) {
			int i = ((from < 0 ? -1 : from) + k + NUM_PADS) % NUM_PADS;
			if (slots[i].used) {
				cue(i, g);
				return;
			}
		}
	}

	// MIDI: one pad, all used pads one after another, or the song (its parts over their rows), with a marker per part
	void exportMidi(int only, bool asSong = false) {
		Genra* g = attached ? attached : findGenra();
		if (!g)
			return;
		std::string name = only >= 0 ? std::string("ablauf-mini-") + slots[only].name : (asSong ? "ablauf-mini-song" : "ablauf-mini-pads");
		Params P = g->buildParams(false);
		std::vector<SongPart> parts;
		auto add = [&](int i, int bars, bool fill) {
			SongPart sp;
			sp.pat = &slots[i].pat;
			sp.P = withMods(P, slots[i].mods);
			sp.P.lengthBars = LENGTH_BARS[clamp((int) slots[i].lengthIdx, 0, 3)];
			sp.L = sp.P.lengthBars * STEPS_PER_BAR;
			sp.bars = bars > 0 ? bars : sp.P.lengthBars;
			sp.fill = fill;
			sp.label = slots[i].name;
			parts.push_back(sp);
		};
		if (asSong)
			for (const Part& p : song) {
				if (slots[p.pad].used)
					add(p.pad, p.bars, p.fill);
			}
		else
			for (int i = 0; i < NUM_PADS; i++)
				if (slots[i].used && (only < 0 || i == only))
					add(i, 0, false);
		if (parts.empty())
			return;
		std::vector<SongMarker> marks;
		std::vector<MidiNote> notes = renderSong(parts, &marks);
		int total = 0;
		for (const SongPart& sp : parts)
			total += sp.bars * STEPS_PER_BAR;
		float bpm = g->params[Genra::BPM_PARAM].getValue();
		std::vector<uint8_t> data = writeSmf(notes, bpm, prof(g->curGenre).name, total, only >= 0 ? &slots[only].pat : nullptr, &marks);
		// into the MIDI export folder, no dialog (a save dialog can stall the audio driver), as ABLAUF's
		std::string err, file = midifolder::save(name, data, &err);
		say(file.empty() ? "Saving failed: " + err : "Saved: " + file);
		msgTime = 4.f;
	}

	// ------------------------------------------------------------ the UI thread's frame
	bool partBtn[NUM_SECTIONS] = {}, undoBtn = false, clearBtn = false, songBtn = false;
	uint32_t seenNext = 0, seenReset = 0;
	void uiFrame(float dt) {
		Genra* g = genra();
		if (g != attached) {
			if (attached)
				letGo();
			attached = g;
			if (g)
				follow::point(genraRef, this, g);
		}
		if (g)
			g->arrAlive.store(1e9f);
		if (msgTime > 0.f)
			msgTime -= dt;
		// buttons
		for (int k = 0; k < NUM_SECTIONS; k++) {
			bool d = params[PART_PARAM + k].getValue() > 0.5f;
			if (d && !partBtn[k])
				partButton(k, g);
			partBtn[k] = d;
			partFlash[k] = std::max(0.f, partFlash[k] - dt);
		}
		bool u = params[UNDO_PARAM].getValue() > 0.5f;
		if (u && !undoBtn && !undo.empty()) {
			if (undo.back().pad < 0 && undo.back().all) {
				for (int i = 0; i < NUM_PADS && i < (int) undo.back().all->size(); i++)
					slots[i] = (*undo.back().all)[i], slotVer[i]++;
				if (play.on && g)
					stopSong(g, false);
				song.clear();
			}
			else
				slots[undo.back().pad] = undo.back().before, slotVer[undo.back().pad]++;
			session.clear();
			say("Undo");
			undo.pop_back();
		}
		undoBtn = u;
		bool c = params[CLEAR_PARAM].getValue() > 0.5f;
		if (c && !clearBtn) {
			clearArmed = !clearArmed;
			if (clearArmed)
				say("Tap the pad to clear");
		}
		clearBtn = c;
		bool sb = params[SONG_PARAM].getValue() > 0.5f;
		if (sb && !songBtn)
			songButton(g);
		songBtn = sb;
		if (g)
			songStep(g);
		// snapshot, waiting cue, NEXT / RESET
		if (g && g->makeSongReq.exchange(false)) // GENRA's MAKE SONG pad: the running loop becomes a song
			songFromGenra(g, false);
		if (g && (pendingPad >= 0 || pendingPart >= 0) && g->snapState.load(std::memory_order_acquire) == 2) {
			snapArrived(g);
			g->snapState.store(0);
		}
		if (g && pendingCue >= 0 && slots[pendingCue].used)
			cue(pendingCue, g);
		uint32_t n = nextCount.load(), r = resetCount.load();
		if (g && n != seenNext) {
			if (play.on)
				stopSong(g, false);
			next(g);
		}
		if (g && r != seenReset) {
			if (!song.empty() && play.on)
				startSong(g); // RESET: the song from the top
			else if (slots[0].used)
				cue(0, g);
		}
		seenNext = n, seenReset = r;
		// lights
		for (int k = 0; k < NUM_SECTIONS; k++)
			lights[PART_LIGHT + k].setBrightness(partFlash[k] > 0.f ? 1.f : 0.25f);
		static float breath = 0.f;
		breath += dt;
		for (int t = 0; t < 4; t++) { // Keep dark, Re-roll lit, Vary breathing
			float v = params[TRACK_PARAM + t].getValue();
			lights[TRACK_LIGHT + t].setBrightness(v > 1.5f ? 0.35f + 0.65f * (0.5f + 0.5f * std::sin(breath * 4.f)) : (v > 0.5f ? 1.f : 0.f));
		}
		lights[SONG_LIGHT].setBrightness(play.on ? 1.f : (song.empty() ? 0.f : 0.25f));
		lights[CLEAR_LIGHT].setBrightness(clearArmed ? 1.f : 0.f);
		lights[UNDO_LIGHT].setBrightness(0.f);
		lights[LINK_LIGHT].setBrightness(g ? 1.f : 0.f);
	}

	// ------------------------------------------------------------ patch
	json_t* dataToJson() override {
		json_t* r = json_object();
		json_object_set_new(r, "breakBass", json_integer(partOpts.breakBass));
		json_object_set_new(r, "introBass", json_integer(partOpts.introBass));
		json_object_set_new(r, "linkedId", json_integer(linkedId));
		json_t* a = json_array();
		for (int i = 0; i < NUM_PADS; i++) {
			json_t* s = json_object();
			if (slots[i].used) {
				std::vector<uint8_t> blob = savePattern(slots[i].pat);
				json_object_set_new(s, "pattern", json_string(string::toBase64(blob.data(), blob.size()).c_str()));
				json_t* sel = json_array();
				for (int t = 0; t < 4; t++)
					json_array_append_new(sel, json_integer(slots[i].sel[t]));
				json_object_set_new(s, "sel", sel);
				json_object_set_new(s, "length", json_integer(slots[i].lengthIdx));
				json_object_set_new(s, "tag", json_integer(slots[i].tag));
				json_object_set_new(s, "name", json_string(slots[i].name));
				json_object_set_new(s, "family", json_integer(slots[i].family));
				json_t* d = json_array();
				for (int t = 0; t < 4; t++)
					json_array_append_new(d, json_real(slots[i].mods.dens[t]));
				json_object_set_new(s, "dens", d);
				json_object_set_new(s, "ghost", json_real(slots[i].mods.ghost));
				json_object_set_new(s, "fill", json_real(slots[i].mods.fill));
			}
			json_array_append_new(a, s);
		}
		json_object_set_new(r, "pads", a);
		json_t* sg = json_array();
		for (const Part& p : song) {
			json_t* o = json_object();
			json_object_set_new(o, "pad", json_integer(p.pad));
			json_object_set_new(o, "bars", json_integer(p.bars));
			json_object_set_new(o, "fill", json_boolean(p.fill));
			json_array_append_new(sg, o);
		}
		json_object_set_new(r, "song", sg);
		return r;
	}
	void dataFromJson(json_t* r) override {
		if (json_t* j = json_object_get(r, "breakBass"))
			partOpts.breakBass = clamp((int) json_integer_value(j), -1, NUM_BREAK_BASS - 1);
		if (json_t* j = json_object_get(r, "introBass"))
			partOpts.introBass = clamp((int) json_integer_value(j), -1, NUM_INTRO_BASS - 1);
		if (json_t* j = json_object_get(r, "linkedId"))
			linkedId = json_integer_value(j);
		json_t* a = json_object_get(r, "pads");
		for (int i = 0; i < NUM_PADS; i++) {
			slots[i] = Slot();
			json_t* s = a ? json_array_get(a, i) : nullptr;
			json_t* p = s ? json_object_get(s, "pattern") : nullptr;
			if (!p || !json_string_value(p))
				continue;
			std::vector<uint8_t> blob = string::fromBase64(json_string_value(p));
			if (!loadPattern(blob, slots[i].pat))
				continue;
			slots[i].used = true;
			json_t* sel = json_object_get(s, "sel");
			for (int t = 0; t < 4; t++) {
				json_t* v = sel ? json_array_get(sel, t) : nullptr;
				int pr = v ? (int)json_integer_value(v) : -1;
				slots[i].sel[t] = (int8_t)(pr >= 0 && pr < numProfiles() ? pr : -1);
			}
			if (json_t* j = json_object_get(s, "length"))
				slots[i].lengthIdx = (int8_t)clamp((int)json_integer_value(j), 0, 3);
			if (json_t* j = json_object_get(s, "tag"))
				slots[i].tag = (int8_t)clamp((int)json_integer_value(j), -1, NUM_SECTIONS - 1);
			if (json_t* j = json_object_get(s, "name"))
				setName(slots[i], json_string_value(j) ? json_string_value(j) : "");
			if (json_t* j = json_object_get(s, "family"))
				slots[i].family = (int8_t) clamp((int) json_integer_value(j), -1, 127);
			json_t* d = json_object_get(s, "dens");
			for (int t = 0; t < 4; t++) {
				json_t* v = d ? json_array_get(d, t) : nullptr;
				slots[i].mods.dens[t] = v ? (float)json_number_value(v) : 0.f;
			}
			if (json_t* j = json_object_get(s, "ghost"))
				slots[i].mods.ghost = (float)json_number_value(j);
			if (json_t* j = json_object_get(s, "fill"))
				slots[i].mods.fill = (float)json_number_value(j);
		}
		undo.clear();
		session.clear();
		song.clear();
		play = Play();
		if (json_t* sg = json_object_get(r, "song")) {
			for (size_t k = 0; k < json_array_size(sg); k++) {
				json_t* o = json_array_get(sg, k);
				Part p;
				json_t* jp = json_object_get(o, "pad");
				json_t* jb = json_object_get(o, "bars");
				json_t* jf = json_object_get(o, "fill");
				p.pad = jp ? clamp((int) json_integer_value(jp), 0, NUM_PADS - 1) : 0;
				p.bars = jb ? clamp((int) json_integer_value(jb), 1, 64) : 4;
				p.fill = jf && json_boolean_value(jf);
				song.push_back(p);
			}
		}
	}
};

// ================================================================= widgets
static std::shared_ptr<window::Font> bold() { return APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf")); }
static void txt(NVGcontext* vg, std::shared_ptr<window::Font> f, float xmm, float ymm, float size, NVGcolor c, const std::string& s,
                int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, float sp = 0.15f) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(size));
	nvgTextLetterSpacing(vg, mm2px(sp));
	nvgFillColor(vg, c);
	nvgTextAlign(vg, align);
	nvgText(vg, mm2px(xmm), mm2px(ymm), s.c_str(), NULL);
}
static NVGcolor tint(NVGcolor c, float f) {
	NVGcolor p = nvgRGB(0x21, 0x23, 0x29);
	return nvgRGBf(p.r + (c.r - p.r) * f, p.g + (c.g - p.g) * f, p.b + (c.b - p.b) * f);
}

struct Display : Widget {
	AblaufMini* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		auto f = bold();
		if (!f)
			return;
		NVGcontext* vg = args.vg;
		float w = box.size.x / mm2px(1.f);
		std::string now = "DROP 2", nxt = "NEXT \xe2\x80\xba BREAK 1", bar = "BAR 7/8";
		NVGcolor nc = SEC_COLORS[SEC_DROP];
		if (module) {
			AblaufMini* m = module;
			Genra* g = m->attached;
			now = "--";
			nc = nvgRGB(0x8a, 0x8d, 0x99);
			nxt = m->msgTime > 0.f ? m->msg : "";
			bar = "";
			if (!g) {
				now = m->busy ? "BUSY" : "NO GENRA";
				if (nxt.empty())
					nxt = "dock or right-click \xe2\x80\xba Connect to";
			}
			else {
				int pl = g->arrSlotPub.load();
				if (pl >= 0 && pl < NUM_PADS && m->slots[pl].used) {
					now = m->slots[pl].name;
					nc = m->slots[pl].tag >= 0 ? SEC_COLORS[m->slots[pl].tag] : ROOT_COLOR;
				}
				else
					now = g->running ? "LIVE" : "STOP";
				if (nxt.empty() && m->play.on && m->play.pos >= 0 && m->play.pos + 1 < (int) m->song.size())
					nxt = string::f("SONG %d/%d \xe2\x80\xba ", m->play.pos + 1, (int) m->song.size()) + m->slots[m->song[m->play.pos + 1].pad].name;
				else if (nxt.empty() && m->play.on && m->play.pos >= 0)
					nxt = string::f("SONG %d/%d \xe2\x80\xba END", m->play.pos + 1, (int) m->song.size());
				if (nxt.empty()) {
					if (g->cueState.load() == 1 && g->cue.slotIdx >= 0 && g->cue.slotIdx < NUM_PADS)
						nxt = std::string("NEXT \xe2\x80\xba ") + m->slots[g->cue.slotIdx].name;
					else if (m->clearArmed)
						nxt = "CLEAR: tap a pad";
					else
						nxt = "PART: a new variant \xc2\xb7 empty pad: store";
				}
				int L = std::max(16, g->lengthSteps());
				int s = std::max(0, g->step) % L;
				bar = string::f("BAR %d/%d", s / 16 + 1, L / 16);
				if (m->play.on && m->play.pos >= 0 && m->play.pos < (int) m->song.size()) {
					int64_t in = g->barPub.load() - m->play.start;
					bar = string::f("BAR %d/%d", clamp((int) in + 1, 1, m->song[m->play.pos].bars), m->song[m->play.pos].bars);
				}
			}
		}
		txt(vg, f, 3.f, 5.6f, 2.8f, nc, now, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.3f);
		txt(vg, f, 3.f, 9.8f, 1.6f, nvgRGB(0xd9, 0xdb, 0xe2), nxt);
		txt(vg, f, w - 3.f, 9.8f, 1.6f, nvgRGB(0x8a, 0x8d, 0x99), bar, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
		Widget::drawLayer(args, layer);
	}
};

struct Pads : OpaqueWidget {
	AblaufMini* module = nullptr;
	int padAt(Vec pos) {
		float x = pos.x / mm2px(1.f), y = pos.y / mm2px(1.f);
		int c = (int)(x / L::PADS[2]), r = (int)(y / L::PADS[3]);
		if (c < 0 || c > 3 || r < 0 || r > 3)
			return -1;
		if (x - c * L::PADS[2] > L::PADS[4] || y - r * L::PADS[3] > L::PADS[5])
			return -1;
		return r * 4 + c;
	}
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS)
			return;
		int i = padAt(e.pos);
		if (i < 0)
			return;
		if (e.button == GLFW_MOUSE_BUTTON_LEFT) {
			module->tap(i);
			e.consume(this);
		}
		else if (e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			AblaufMini* m = module;
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel(m->slots[i].used ? std::string(m->slots[i].name) : string::f("Pad %02d (empty)", i + 1)));
			if (m->slots[i].used) {
				menu->addChild(createMenuItem("Export pad as MIDI…", "", [=]() { m->exportMidi(i); }));
				menu->addChild(createMenuItem("Clear pad", "", [=]() {
					m->remember(i);
					m->slots[i] = Slot();
				}));
			}
			e.consume(this);
		}
	}
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		auto f = bold();
		if (!f)
			return;
		NVGcontext* vg = args.vg;
		Genra* g = module ? module->attached : nullptr;
		int playing = g ? g->arrSlotPub.load() : -1;
		int queued = g && g->cueState.load() == 1 ? g->cue.slotIdx : -1;
		for (int i = 0; i < NUM_PADS; i++) {
			float x = (i % 4) * L::PADS[2], y = (i / 4) * L::PADS[3], pw = L::PADS[4], ph = L::PADS[5];
			bool used = module ? module->slots[i].used : i < 8;
			int tag = module ? module->slots[i].tag : (i == 0 ? -1 : (i < 8 ? (i * 5) % 6 : -1));
			NVGcolor col = tag >= 0 ? SEC_COLORS[tag] : ROOT_COLOR;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(pw), mm2px(ph), mm2px(0.8f));
			nvgFillColor(vg, used ? tint(col, i == playing ? 0.5f : 0.3f) : nvgRGB(0x1a, 0x1b, 0x20));
			nvgFill(vg);
			if (used) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(x + 0.6f), mm2px(y + 0.6f), mm2px(pw - 1.2f), mm2px(0.7f), mm2px(0.3f));
				nvgFillColor(vg, col);
				nvgFill(vg);
				std::string nm = module ? std::string(module->slots[i].name) : (i == 0 ? "ROOT" : "PART");
				txt(vg, f, x + 0.9f, y + 5.8f, 1.5f, nvgRGB(0xff, 0xff, 0xff), nm, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.1f);
				for (int t = 0; t < 4; t++) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, mm2px(x + 0.9f + t * 1.5f), mm2px(y + 7.7f), mm2px(1.05f), mm2px(0.6f), mm2px(0.25f));
					nvgFillColor(vg, TRACK_COLORS[t]);
					nvgFill(vg);
				}
			}
			txt(vg, f, x + 0.9f, y + 3.2f, 1.1f, used ? nvgRGB(0xd9, 0xdb, 0xe2) : nvgRGB(0x4a, 0x4c, 0x57), string::f("%02d", i + 1));
			if (i == playing || i == queued || (i == 0 && used)) {
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(pw), mm2px(ph), mm2px(0.8f));
				nvgStrokeColor(vg, i == playing || i == queued ? nvgRGB(0xff, 0xff, 0xff) : nvgRGB(0xe6, 0xe7, 0xec));
				nvgStrokeWidth(vg, mm2px(i == playing ? 0.55f : 0.35f));
				if (i == queued && i != playing)
					nvgStrokeWidth(vg, mm2px(0.4f)); // (NanoVG has no dashes: queued = a thinner white frame + blink)
				nvgStroke(vg);
			}
		}
		Widget::drawLayer(args, layer);
	}
};

struct AblaufMiniWidget : ModuleWidget {
	AblaufMiniWidget(AblaufMini* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/AblaufMini.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		using A = AblaufMini;
		addParam(createLightParamCentered<VCVLightBezel<Light<0x6f, 0x86, 0xb8>>>(P(L::PART0), module, A::PART_PARAM + 0, A::PART_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezel<Light<0x9b, 0x7b, 0xd4>>>(P(L::PART1), module, A::PART_PARAM + 1, A::PART_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezel<Light<0xe0, 0xa5, 0x26>>>(P(L::PART2), module, A::PART_PARAM + 2, A::PART_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezel<Light<0xff, 0x5a, 0x4f>>>(P(L::PART3), module, A::PART_PARAM + 3, A::PART_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezel<Light<0x3f, 0xae, 0x9a>>>(P(L::PART4), module, A::PART_PARAM + 4, A::PART_LIGHT + 4));
		addParam(createLightParamCentered<VCVLightBezel<Light<0x8a, 0x8d, 0x99>>>(P(L::PART5), module, A::PART_PARAM + 5, A::PART_LIGHT + 5));
		addParam(createLightParamCentered<VCVLightBezelLatch<Light<0x7b, 0x6c, 0xff>>>(P(L::TRACK0), module, A::TRACK_PARAM + 0, A::TRACK_LIGHT + 0));
		addParam(createLightParamCentered<VCVLightBezelLatch<Light<0xb8, 0xf2, 0x3a>>>(P(L::TRACK1), module, A::TRACK_PARAM + 1, A::TRACK_LIGHT + 1));
		addParam(createLightParamCentered<VCVLightBezelLatch<Light<0x3d, 0xc9, 0xff>>>(P(L::TRACK2), module, A::TRACK_PARAM + 2, A::TRACK_LIGHT + 2));
		addParam(createLightParamCentered<VCVLightBezelLatch<Light<0xe0, 0x77, 0x9e>>>(P(L::TRACK3), module, A::TRACK_PARAM + 3, A::TRACK_LIGHT + 3));
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(L::UNDO), module, A::UNDO_PARAM, A::UNDO_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RedLight>>(P(L::CLEAR), module, A::CLEAR_PARAM, A::CLEAR_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<Light<0xff, 0xd2, 0x3d>>>(P(L::SONG), module, A::SONG_PARAM, A::SONG_LIGHT));
		addChild(createLightCentered<MediumLight<GreenLight>>(P(L::LINK), module, A::LINK_LIGHT));
		addInput(createInputCentered<PJ301MPort>(P(L::IN_NEXT), module, A::NEXT_INPUT));
		addInput(createInputCentered<PJ301MPort>(P(L::IN_RESET), module, A::RESET_INPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_DROP), module, A::DROP_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(P(L::OUT_END), module, A::END_OUTPUT));
		Display* d = new Display;
		d->module = module;
		d->box.pos = mm2px(Vec(L::DISPLAY[0], L::DISPLAY[1]));
		d->box.size = mm2px(Vec(L::DISPLAY[2], L::DISPLAY[3]));
		addChild(d);
		Pads* p = new Pads;
		p->module = module;
		p->box.pos = mm2px(Vec(L::PADS[0], L::PADS[1]));
		p->box.size = mm2px(Vec(3 * L::PADS[2] + L::PADS[4], 3 * L::PADS[3] + L::PADS[5]));
		addChild(p);
	}
	void step() override {
		ModuleWidget::step();
		if (AblaufMini* m = dynamic_cast<AblaufMini*>(module))
			m->uiFrame(APP->window->getLastFrameDuration());
	}
	void appendContextMenu(Menu* menu) override {
		AblaufMini* m = dynamic_cast<AblaufMini*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Connect to…"));
		menu->addChild(createCheckMenuItem("Automatic (docked GENRA / GENRA BOX)", "", [=]() { return m->linkedId < 0; }, [=]() { m->linkedId = -1; }));
		int n = 0;
		for (int64_t id : APP->engine->getModuleIds()) {
			Module* mm = APP->engine->getModule(id);
			if (!mm || !isGenraModel(mm->model))
				continue;
			n++;
			std::string label = string::f(mm->model == modelGenraBox ? "GENRA BOX #%d" : "GENRA #%d", n);
			menu->addChild(createCheckMenuItem(label, "", [=]() { return m->linkedId == id; }, [=]() { m->linkedId = id; }));
		}
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("New parts (as ABLAUF)"));
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
		menu->addChild(createMenuLabel("MIDI (into the MIDI export folder)"));
		menu->addChild(createMenuItem("Export all pads as MIDI…", "one after another, a marker per pad", [=]() { m->exportMidi(-1); }));
		if (!m->song.empty())
			menu->addChild(createMenuItem("Export the song as MIDI…", "its parts over their rows, a marker per part", [=]() { m->exportMidi(-1, true); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("Song (SONG button)"));
		if (!m->song.empty())
			menu->addChild(createMenuItem(m->play.on ? "Stop the song" : "Play the song from the top", "", [=]() {
				if (Genra* g = m->attached) {
					if (m->play.on)
						m->stopSong(g, true);
					else
						m->startSong(g);
				}
			}));
		else
			menu->addChild(createMenuLabel("(SONG makes one from the playing pad or ROOT)"));
		menu->addChild(createMenuLabel("(one pad: right-click the pad)"));
	}
};

} // namespace amini

Model* modelAblaufMini = createModel<amini::AblaufMini, amini::AblaufMiniWidget>("GenraAblaufMini");
