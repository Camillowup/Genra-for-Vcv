// SPDX-License-Identifier: GPL-3.0-only
// FOLLOW GENRA: connections without cables (right-click › Follow GENRA on an instrument, › CH n input from on MIX).
//
// A "virtual cable": while no real cable is in a jack, the module writes the source's voltages into its own input
// (Input::channels / voltages) at the start of process(), so the module's code reads it as if patched. A real cable
// always wins: the engine writes the input after every sample, so a value we did not write means a cable is there
// (Drive). The source is held in a Ref, an atomic pointer that is cleared when the source module is removed
// (follow::gone() in every module's onRemove; Rack holds the engine lock there, no process() runs).
#pragma once
#include "plugin.hpp"
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

// ---------------------------------------------------------------- the virtual cable
struct Drive {
	int ch = -1;           // what we wrote last sample (-1: nothing; the jack is not ours)
	float v[PORT_MAX_CHANNELS] = {};
	bool owned = false;    // for the panel: this jack is fed by the link
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
	float uiTimer = 0.f;

	void bind(int input, int sig) {
		binds.push_back({input, sig});
		drv.resize(binds.size());
	}
	bool active() const { return track.load() >= 0 && genra.to.load() != nullptr; }
	/** Audio thread, first thing in process(). */
	void process(Module* self);
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
	int outL = -1, outR = -1;
	const char* srcName = "";
	virtual ~Host() {}
};
Host* host(Module* m);

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
/** Add the lit strip to a module widget. pos: jack centres in px, in the order of fol.binds. */
StripLight* addStrip(ModuleWidget* mw, Module* module, const std::vector<Vec>& pos);

} // namespace follow
