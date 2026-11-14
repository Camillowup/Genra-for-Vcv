// SPDX-License-Identifier: GPL-3.0-only
// FOLLOW GENRA: connections without cables (right-click › Follow GENRA on an instrument, › CH n input from on MIX).
//
// A "virtual cable": while no real cable is in a jack, the module writes the source's voltages into its own input
// (Input::channels / voltages) at the start of process(), so the module's code reads it as if patched. A real cable
// always wins: the engine writes the input after every sample, so a value we did not write means a cable is there
// (Drive). The source is held in a Ref, an atomic pointer that is cleared when the source module is removed
// (follow::gone() in every module's onRemove; Rack holds the engine lock there, no process() runs).
//
// 2.29: also across the two plugins of the family (GENRA and GENRA Instruments): references, the hosts and GENRA's
// outputs and tempo go through the link hub (link/Hub.hpp), never into another plugin's classes directly.
#pragma once
#include "plugin.hpp"
#include "link/Hub.hpp"
#include <atomic>
#include <vector>

namespace follow {

// what an instrument jack takes from GENRA
enum Sig { VOCT, GATE, VEL, ACC, SLIDE, MOD, CLOCK, RESET, DRUMS, NUM_SIGS };
// GENRA's tracks as a follower picks them (-1: off)
enum Track { T_BASS, T_MEL, T_PAD, T_DRUMS, NUM_TRACKS };
extern const char* const TRACK_NAMES[NUM_TRACKS];
NVGcolor trackColor(int track);

// ---------------------------------------------------------------- references to other modules
struct Ref {
	std::atomic<Module*> to{nullptr};
	Module* owner = nullptr;
	genralink::GenraInfo genra; // when `to` is a GENRA: its outputs and tempo (set by point())
	~Ref();
};
/** UI thread: let r point at `to` (nullptr: nowhere). */
void point(Ref& r, Module* owner, Module* to);
/** In onRemove of every module that follows, is followed, or both. */
void gone(Module* m);

/** GENRA's output for a signal of a track: id and channel count (id -1: GENRA has none). */
struct Src {
	int id = -1;
	int channels = 1;
};
Src genraOut(int track, int sig);
bool isGenra(Module* m);
/** GENRA CHOP (the hip-hop GENRA, a plugin of its own, 2.39): it publishes its tracks in GENRA's output layout through
 * the link hub, so instruments can follow it too. Not a Genra class: never cast it. */
bool isGenraChop(Module* m);
/** What an instrument can follow: a GENRA, a GENRA BOX or a GENRA CHOP. */
inline bool isSource(Module* m) { return isGenra(m) || isGenraChop(m); }
/** Audio thread, no locks: the module is a GENRA / GENRA BOX of this family (the Genra class), by its model. */
bool isGenraModel(const Module* m);
/** GENRA's tempo right now (from its clock: the internal BPM or the CLOCK input), for the tempo-synced effects.
 * r: a reference pointing at a GENRA. */
float genraBpm(const Ref& r, float sampleRate);
/** GENRA's outputs as of the last complete frame (one sample later than a cable, never half a frame); nullptr if the
 * reference does not point at a GENRA. */
const float* genraFrame(const Ref& r, int outputId, int64_t frame);
/** GENRA's constructor: it can be followed (also from the other plugin). */
void addGenra(Module* g, const float* followPub, int outputs, const float* stepPeriod);

// ---------------------------------------------------------------- the virtual cable
struct Drive {
	int ch = -1;           // what we wrote last sample (-1: nothing; the jack is not ours)
	float v[PORT_MAX_CHANNELS] = {};
	bool owned = false;    // for the panel: this jack is fed by the link
	bool cable = false;    // a real cable is in the jack (from Module::onPortChange: exact, set under the engine lock)
	/** Feed `in` from n voltages. False if a cable is in the jack (then nothing is written). */
	bool feed(Input& in, const float* src, int n);
	/** Stop feeding (unplugs the virtual cable if it is ours). */
	void release(Input& in);
	bool cableIn(const Input& in) const;
};

// ---------------------------------------------------------------- an instrument that follows a GENRA track
struct Follower {
	struct Bind {
		int input;
		int sig;
	};
	std::vector<Bind> binds;
	std::vector<Drive> drv;
	bool drumsOnly = false;          // DRUM-X: the drums, nothing else
	std::atomic<int> track{-1};      // -1 off, else Track
	std::atomic<int64_t> genraId{-1}; // -1: automatic (the only / first GENRA)
	Ref genra;
	std::atomic<bool> presets{true}; // NEW on the GENRA (or GENRA BOX) may load a preset for the track's genre (2.31)
	float uiTimer = 0.f;
	bool anyDriven = false; // some jack was fed last sample (then process() still has to let go of it)

	void bind(int input, int sig) {
		binds.push_back({input, sig});
		drv.resize(binds.size());
	}
	bool active() const { return track.load() >= 0 && genra.to.load() != nullptr; }
	/** Audio thread, first thing in process(). */
	void process(Module* self, int64_t frame);
	/** From the module's onPortChange: a cable plugged into / pulled from one of the bound jacks. */
	void portChange(const Module::PortChangeEvent& e);
	/** UI thread: find the GENRA. */
	void uiStep(Module* self, float dt);
	void toJson(json_t* root);
	void fromJson(json_t* root);
	void appendMenu(Menu* menu, Module* self);
	/** For the panel: the gate (or any drum) is high right now. */
	bool gateHigh(Module* self);
};

/** Every module that MIX can take audio from (instruments, STOTTER); instruments also follow GENRA. */
struct Host {
	Follower fol;
	bool follows = true;      // false: a source for MIX only (STOTTER)
	int audioL = -1, audioR = -1; // the stereo out MIX takes
	const char* srcName = "";
	std::atomic<int64_t> pulledAt{-1000000}; // MIX takes the stereo out (engine frame of the last time)
	std::atomic<int> pulledBy{-1};           // … on this channel (0..3), for the panel
	/** The R out counts as patched while MIX takes it (the module then plays in stereo, as with two cables). */
	bool pulled(int64_t frame) const { return frame - pulledAt.load(std::memory_order_relaxed) < 4096; }
	virtual ~Host() {}
	// ---- presets the GENRA it follows may switch (NEW picks one for the genre; UI thread, 2.31). Plain buffers, no
	// std::string: the host may live in the other plugin (each has its own C++ runtime).
	virtual int hostPresetCount() { return 0; }
	/** Preset i's name and style (as "HOUSE · GARAGE"; "" for a user preset) into the buffers. */
	virtual void hostPresetInfo(int i, char* name, int nameLen, char* style, int styleLen) {
		if (nameLen > 0)
			name[0] = 0;
		if (styleLen > 0)
			style[0] = 0;
	}
	virtual int hostPresetCurrent() { return -1; }
	virtual void hostPresetLoad(int i) {}
};
/** Copy s into a host buffer (hostPresetInfo). */
inline void putStr(char* dst, int len, const char* s) {
	if (len <= 0)
		return;
	int i = 0;
	for (; s && s[i] && i < len - 1; i++)
		dst[i] = s[i];
	dst[i] = 0;
}
/** The host behind a module (also one of the other plugin, once its panel has made it known: knownHost()). */
Host* host(Module* m);
/** UI thread, from the module's panel: make the host known to the other plugin (MIX there takes its audio). */
void knownHost(Module* m);
/** UI thread: the instruments that follow `genra`'s track (GENRA's track numbering: 0 drums, 1 bass, 2 melody, 3 pads)
 * and allow it load a new preset. mode 1: all genres, 2: the genre's (genreName; none fits: all). Returns how many. */
int rollFollowerPresets(Module* genra, int genraTrack, const char* genreName, int mode);

// ---------------------------------------------------------------- panel and menu
/** The lit strip: one segment over each jack the link feeds. jacks: (centre in px, index into fol.binds). */
struct StripLight : widget::TransparentWidget {
	Module* module = nullptr;
	std::vector<std::pair<Vec, int>> jacks;
	float flash = 0.f;
	bool lastGate = false;
	void step() override;
	void drawLayer(const DrawArgs& args, int layer) override;
};
/** Add the lit strip to a module widget. pos: jack centres in px, in the order of fol.binds. (Adds OutLight too.) */
StripLight* addStrip(ModuleWidget* mw, Module* module, const std::vector<Vec>& pos);

/** OUT L/R glow in the MIX channel's colour while MIX takes them without a cable; "› MIX CH n" above. */
struct OutLight : widget::TransparentWidget {
	Module* module = nullptr;
	Vec l, r;
	bool found = false;
	bool known = false;
	void step() override;
	void drawLayer(const DrawArgs& args, int layer) override;
};
OutLight* addOutLight(ModuleWidget* mw, Module* module);
/** The MIX channel colours (CH 1..4). */
NVGcolor mixColor(int ch);
/** A glowing ring around a jack (layer 1). */
void ring(NVGcontext* vg, Vec c, NVGcolor col, float a = 1.f);
/** The lit segment over a jack, as on the GENRA strip (layer 1). */
void segment(NVGcontext* vg, Vec jack, NVGcolor col, float a = 0.85f);

} // namespace follow
