// SPDX-License-Identifier: GPL-3.0-only
// GENRA BOX – GENRA's generator with four voices in one module (docs/GENRA_BOX.md): DRUM-X (its core), GENRA MONO,
// FUNKE, SCHMELZ (BoxVoices.hpp). It *is* a GENRA (GRID, ABLAUF, ABLAUF MINI, MIDI and Follow work with it unchanged);
// its panel shows a part of GENRA's controls and adds the voices, NOTES / GATES, DUCK and the mix.
// 2.31: back on 2.30 (it was lost with the old account): GENRA MONO instead of GRUND, NEW / HOLD by the pattern, presets
// of the track's genre by default, MUTATE, CLOCK out, DUCK, the mono sum on L.
#include "../genra/GenraModule.hpp"
#include "BoxLayout.hpp"
#include "BoxVoices.hpp"
#include "../jiant/DrumCore.hpp"
#include "../jiant/JiantUI.hpp"

namespace box {

namespace L = boxlayout;

// the panel's columns: DRUM-X, GRUND, FUNKE, SCHMELZ = GENRA's tracks in GENRA's order
static const int COL_TRACK[4] = {T_DRUMS, T_BASS, T_MELODY, T_PADS};
static const char* const VOICE_NAMES[4] = {"DRUM-X", "GENRA MONO", "FUNKE", "SCHMELZ"}; // by GENRA track
static const char* const MACRO_A[4] = {"Tune", "Cutoff", "Bright", "Motion"};    // by GENRA track
static const char* const MACRO_B[4] = {"Decay", "Reso", "Space", "Space"};
static const char* const SOUND_MODES[3] = {"Like SOUND button", "All genres", "Same genre"};
static const NVGcolor SOUND_COLOR = nvgRGB(0xff, 0xd2, 0x3d);

template <int R, int G, int B>
struct Light : GrayModuleLightWidget {
	Light() { addBaseColor(nvgRGB(R, G, B)); }
};
using DrumLight = Light<0x7b, 0x6c, 0xff>;
using BassLight = Light<0xb8, 0xf2, 0x3a>;
using MelLight = Light<0x3d, 0xc9, 0xff>;
using PadLight = Light<0xe0, 0x77, 0x9e>;
using RunLight = Light<0x5d, 0xff, 0x8a>;
using SoundLight = Light<0xff, 0xd2, 0x3d>;

// ================================================================= DRUM-X: a private copy of the core
struct Drums {
	enum { BLOCK = 32 };
	DrumCore core;
	jiant::Resampler src;
	dsp::DoubleRingBuffer<dsp::Frame<2>, 2048> outBuf;
	float l[BLOCK], r[BLOCK];
	bool failed = false;
	int kit = -1, baseTune = 64, baseDecay = 64;
	float ma = -1.f, mb = -1.f;
	const jdx_api* api() const { return core.api; }
	/** UI thread (onAdd): the core's copy, loaded once */
	void load() {
		if (core.loaded() || failed)
			return;
		if (!core.load()) {
			failed = true;
			WARN("GENRA BOX: DRUM-X core: %s", core.error.c_str());
			return;
		}
		core.api->init(random::u32() | 1u);
		kit = -1;
	}
	void unload() { core.unload(); }
	int nkits() const { return api() ? std::max(1, (int) api()->nkits()) : 1; }
	/** kit and the macros TUNE, DECAY (control rate) */
	void set(int k, float a, float b) {
		const jdx_api* x = api();
		if (!x)
			return;
		int n = nkits();
		k = ((k % n) + n) % n;
		if (k != kit) {
			x->load_kit((uint32_t) k);
			kit = k;
			baseTune = x->get(JDX_TUNE);
			baseDecay = x->get(JDX_DECAY);
			ma = mb = -1.f;
		}
		if (a != ma) {
			ma = a;
			x->set(JDX_TUNE, clamp((int) std::round(baseTune + (a - 0.5f) * 100.f), 0, 127));
		}
		if (b != mb) {
			mb = b;
			x->set(JDX_DECAY, clamp((int) std::round(baseDecay + (b - 0.5f) * 110.f), 0, 127));
		}
	}
	void hit(int lane, int vel) {
		if (api())
			api()->hit((uint32_t) lane, (uint32_t) clamp(vel, 1, 127));
	}
	dsp::Frame<2> next(float sampleRate) {
		const jdx_api* x = api();
		if (!x)
			return dsp::Frame<2>{};
		int guard = 0;
		while (outBuf.empty() && guard++ < 8) {
			x->render(BLOCK, l, r);
			dsp::Frame<2> in[BLOCK];
			for (int i = 0; i < BLOCK; i++)
				in[i].samples[0] = l[i], in[i].samples[1] = r[i];
			src.process(in, BLOCK, (int) x->sample_rate(), (int) sampleRate, outBuf);
		}
		return outBuf.empty() ? dsp::Frame<2>{} : outBuf.shift();
	}
};

struct GenraBox : Genra {
	// NOTE: only append; ids start after GENRA's own lists
	enum BoxParamId {
		SOUND_PARAM = Genra::PARAMS_LEN,
		ENUMS(MACA_PARAM, 4),  // by GENRA track
		ENUMS(MACB_PARAM, 4),
		ENUMS(LEVEL_PARAM, 4),
		ENUMS(HOLD_PARAM, 4),
		MASTER_PARAM,
		DUCK_PARAM,            // the kick pushes bass and pads down
		BOX_PARAMS_END
	};
	enum BoxOutputId {
		TRACK_L_OUTPUT = Genra::OUTPUTS_LEN, // 4, by GENRA track
		TRACK_L_OUTPUT_LAST = TRACK_L_OUTPUT + 3,
		ENUMS(TRACK_R_OUTPUT, 4),
		NOTES_OUTPUT,
		GATES_OUTPUT,
		MAIN_L_OUTPUT,
		MAIN_R_OUTPUT,
		BOX_OUTPUTS_END
	};
	enum BoxLightId {
		SOUND_LIGHT = Genra::LIGHTS_LEN,
		ENUMS(HOLD_LIGHT, 4),
		BOX_LIGHTS_END
	};

	// preset dice per track: 0 like the SOUND button, 1 all genres, 2 same genre; the button's own: 1 or 2
	std::atomic<int> soundMode[4];
	std::atomic<int> soundAll{2}; // 2.31: the track's genre by default

	GenraBox() : Genra(BOX_PARAMS_END - Genra::PARAMS_LEN, 0, BOX_OUTPUTS_END - Genra::OUTPUTS_LEN, BOX_LIGHTS_END - Genra::LIGHTS_LEN) {
		for (int t = 0; t < 4; t++)
			soundMode[t] = 0;
		configSwitch(SOUND_PARAM, 0.f, 1.f, 1.f, "SOUND: NEW also re-rolls the presets", {"Off", "On"});
		paramQuantities[SOUND_PARAM]->randomizeEnabled = false;
		for (int t = 0; t < 4; t++) {
			std::string vn = VOICE_NAMES[t];
			configParam(MACA_PARAM + t, 0.f, 1.f, 0.5f, vn + " " + MACRO_A[t], "%", 0.f, 200.f, -100.f);
			configParam(MACB_PARAM + t, 0.f, 1.f, 0.5f, vn + " " + MACRO_B[t], "%", 0.f, 200.f, -100.f);
			configParam(LEVEL_PARAM + t, 0.f, 1.f, 0.75f, vn + " level", "%", 0.f, 133.333f);
			configSwitch(HOLD_PARAM + t, 0.f, 1.f, 0.f, vn + " hold preset (NEW keeps it)", {"Off", "Hold"});
			paramQuantities[HOLD_PARAM + t]->randomizeEnabled = false;
			configOutput(TRACK_L_OUTPUT + t, vn + " left (takes the track out of the mix)");
			configOutput(TRACK_R_OUTPUT + t, vn + " right");
		}
		configParam(MASTER_PARAM, 0.f, 1.f, 0.75f, "Master level", "%", 0.f, 133.333f);
		configParam(DUCK_PARAM, 0.f, 1.f, 0.f, "Duck: the kick pushes bass and pads down", "%", 0.f, 100.f);
		configOutput(NOTES_OUTPUT, "Notes (polyphonic V/Oct: 1 bass, 2 melody, 3–6 pads)");
		configOutput(GATES_OUTPUT, "Gates (polyphonic: 1 bass, 2 melody, 3–6 pads)");
		configOutput(MAIN_L_OUTPUT, "Mix left (alone: the mono sum)");
		configOutput(MAIN_R_OUTPUT, "Mix right");
		// the box's knobs: the names the panel shows
		paramQuantities[FILL_PARAM]->name = "Drum fill";
		paramQuantities[MOVE_PARAM]->name = "Bass movement";
		paramQuantities[VOICING_PARAM]->name = "Pad voicing";
		paramQuantities[RANGE_PARAM]->name = "Melody range";
		paramQuantities[CHORDLOCK_PARAM]->name = "LOCK: keep the chord progression";
		for (int t = 0; t < 4; t++)
			paramQuantities[TNEW_PARAM + t]->name = std::string(VOICE_NAMES[t]) + ": NEW pattern (and a preset with SOUND on)";
		initVoices();
		bass.setSampleRate(sr);
		mel.setSampleRate(sr);
		pads.setSampleRate(sr);
	}

	// ---- the voices (engine thread)
	Drums drums;
	boxv::Bass bass;
	boxv::Melody mel;
	boxv::Pads pads;
	std::atomic<int> preset[4];      // by GENRA track (DRUM-X: the kit)
	std::atomic<int> presetReq[4];   // from the UI thread (-1: none)
	std::atomic<int> followed{0};    // tracks a big instrument plays (Follow GENRA BOX): the box's own voice is quiet
	std::string followName[4];       // (UI thread)
	std::vector<int64_t> followIds[4]; // (UI thread) the modules that follow each track
	std::vector<std::vector<int>> sameGenre[4]; // [track][profile]: the presets that fit the genre (GenreMatch)
	uint32_t rollSeen[4] = {};       // GENRA's presetRoll, as the voices took it (audio thread)
	dsp::SchmittTrigger drumTrig[NUM_DRUMS];
	float duckEnv = 0.f;
	bool duckRise = false;
	int ctlDiv = 0;
	float sr = 44100.f;

	static int presetCount(int t, const Drums& d) {
		switch (t) {
			case T_DRUMS: return d.nkits();
			case T_BASS: return boxv::Bass::count();
			case T_MELODY: return boxv::Melody::count();
			default: return boxv::Pads::count();
		}
	}
	static const char* presetStyle(int t, int i) {
		switch (t) {
			case T_BASS: return boxv::Bass::style(i);
			case T_MELODY: return boxv::Melody::style(i);
			case T_PADS: return boxv::Pads::style(i);
			default: return "";
		}
	}

	void initVoices() {
		for (int t = 0; t < 4; t++) {
			preset[t] = 0;
			presetReq[t] = -1;
		}
		for (int t = 1; t < 4; t++) {
			sameGenre[t].assign(numProfiles(), std::vector<int>());
			for (int p = 0; p < numProfiles(); p++)
				sameGenre[t][p] = genrematch::candidates(presetCount(t, drums), [&](int i) { return presetStyle(t, i); }, prof(p).name);
		}
	}

	/** GENRA's preset mode for track t's followers (and the box's own voice): SOUND off or HOLD: none. */
	int presetModeFor(int t) override {
		if (params[SOUND_PARAM].getValue() < 0.5f || params[HOLD_PARAM + t].getValue() > 0.5f)
			return 0;
		return soundMode[t] == 0 ? soundAll.load() : soundMode[t].load();
	}
	/** A new preset for the box's own voice of track t (audio thread, after NEW). */
	void rollPreset(int t) {
		int mode = presetModeFor(t);
		int n = presetCount(t, drums);
		if (mode <= 0 || n <= 1)
			return;
		int cur = preset[t];
		if (mode == 2 && t != T_DRUMS) {
			int pr = clamp((int) pat.trackGenre[t], 0, numProfiles() - 1);
			if (pr < (int) sameGenre[t].size() && !sameGenre[t][pr].empty()) {
				const std::vector<int>& c = sameGenre[t][pr];
				int k = c[random::u32() % c.size()];
				for (int tries = 0; k == cur && c.size() > 1 && tries < 8; tries++)
					k = c[random::u32() % c.size()];
				preset[t] = k;
				return;
			}
		}
		int k = (int) (random::u32() % (uint32_t) (n - 1));
		preset[t] = k >= cur ? k + 1 : k; // never the same one again
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		sr = e.sampleRate;
		bass.setSampleRate(sr);
		mel.setSampleRate(sr);
		pads.setSampleRate(sr);
		drums.outBuf.clear();
		drums.src.reset();
	}
	void onAdd(const AddEvent& e) override {
		drums.load();
		onSampleRateChange(SampleRateChangeEvent{APP->engine->getSampleRate(), APP->engine->getSampleTime()});
	}
	void onRemove(const RemoveEvent& e) override {
		Genra::onRemove(e);
		drums.unload();
	}

	static float gainOf(float knob) {
		float g = knob / 0.75f; // 0.75 = 0 dB, quadratic as on MIX
		return g * g;
	}

	void process(const ProcessArgs& args) override {
		bool sound = params[SOUND_PARAM].getValue() > 0.5f;
		Genra::process(args);

		// NEW (GENRA counts it per track): with SOUND on a new preset for the box's voice (the followers: UI thread)
		for (int t = 0; t < 4; t++) {
			uint32_t r = presetRoll[t].load(std::memory_order_relaxed);
			if (r != rollSeen[t]) {
				rollSeen[t] = r;
				rollPreset(t);
			}
			int rq = presetReq[t].load(std::memory_order_relaxed) >= 0 ? presetReq[t].exchange(-1) : -1;
			if (rq >= 0)
				preset[t] = rq;
		}

		// NOTES / GATES: bass, melody, 4 pad voices on one cable each
		Output& no = outputs[NOTES_OUTPUT];
		Output& ga = outputs[GATES_OUTPUT];
		if (no.isConnected() || ga.isConnected()) {
			no.setChannels(6);
			ga.setChannels(6);
			no.setVoltage(outputs[BASS_VOCT_OUTPUT].getVoltage(), 0);
			ga.setVoltage(outputs[BASS_GATE_OUTPUT].getVoltage(), 0);
			no.setVoltage(outputs[MEL_VOCT_OUTPUT].getVoltage(), 1);
			ga.setVoltage(outputs[MEL_GATE_OUTPUT].getVoltage(), 1);
			for (int k = 0; k < 4; k++) {
				no.setVoltage(outputs[PAD_VOCT_OUTPUT].getVoltage(k), 2 + k);
				ga.setVoltage(outputs[PAD_GATE_OUTPUT].getVoltage(k), 2 + k);
			}
		}

		// ---- the voices
		int fol = followed.load(std::memory_order_relaxed);
		if (--ctlDiv <= 0) {
			ctlDiv = 16;
			drums.set(preset[T_DRUMS], params[MACA_PARAM + T_DRUMS].getValue(), params[MACB_PARAM + T_DRUMS].getValue());
			bass.set(clamp((int) preset[T_BASS], 0, boxv::Bass::count() - 1), params[MACA_PARAM + T_BASS].getValue(), params[MACB_PARAM + T_BASS].getValue());
			mel.set(clamp((int) preset[T_MELODY], 0, boxv::Melody::count() - 1), params[MACA_PARAM + T_MELODY].getValue(), params[MACB_PARAM + T_MELODY].getValue());
			pads.set(clamp((int) preset[T_PADS], 0, boxv::Pads::count() - 1), params[MACA_PARAM + T_PADS].getValue(), params[MACB_PARAM + T_PADS].getValue());
		}
		float tl[4] = {}, tr[4] = {};
		// DRUM-X: GENRA's 8 drum tracks onto its lanes (as DRUM-X's GENRA input does)
		{
			static const int TO_LANE[8] = {0, 1, 2, 6, 3, 4, 5, 7};
			for (int v = 0; v < NUM_DRUMS; v++) {
				float g = outputs[DRUM_OUTPUT + v].getVoltage();
				bool hitNow = drumTrig[v].process(g, 0.1f, 0.5f);
				if (hitNow && v == KICK)
					duckRise = true; // DUCK: the kick (also when a big DRUM-X plays it)
				if (hitNow && !(fol & 1))
					drums.hit(TO_LANE[v], (int) std::round(g * 12.7f));
			}
			dsp::Frame<2> f = drums.next(args.sampleRate);
			tl[T_DRUMS] = f.samples[0] * 5.f, tr[T_DRUMS] = f.samples[1] * 5.f;
		}
		auto monoIn = [&](int voct, int gate, int vel, int acc, int slide) {
			boxv::MonoIn in;
			in.voct = outputs[voct].getVoltage();
			in.gate = outputs[gate].getVoltage() >= 1.f;
			in.vel = clamp(outputs[vel].getVoltage() * 0.1f, 0.f, 1.f);
			in.acc = outputs[acc].getVoltage() >= 1.f;
			in.slide = outputs[slide].getVoltage() >= 1.f;
			return in;
		};
		{
			boxv::MonoIn in = monoIn(BASS_VOCT_OUTPUT, BASS_GATE_OUTPUT, BASS_VEL_OUTPUT, BASS_ACC_OUTPUT, BASS_SLIDE_OUTPUT);
			if (fol & 2)
				in.gate = false;
			bass.process(in, tl[T_BASS], tr[T_BASS]);
		}
		{
			boxv::MonoIn in = monoIn(MEL_VOCT_OUTPUT, MEL_GATE_OUTPUT, MEL_VEL_OUTPUT, MEL_ACC_OUTPUT, MEL_SLIDE_OUTPUT);
			if (fol & 4)
				in.gate = false;
			float bpm = stepPeriod > 0.f ? 60.f * args.sampleRate / (stepPeriod * 4.f) : 120.f;
			mel.process(in, bpm, tl[T_MELODY], tr[T_MELODY]);
		}
		{
			float v[4];
			bool g[4];
			for (int k = 0; k < 4; k++) {
				v[k] = outputs[PAD_VOCT_OUTPUT].getVoltage(k);
				g[k] = outputs[PAD_GATE_OUTPUT].getVoltage(k) >= 1.f && !(fol & 8);
			}
			pads.process(v, g, clamp(outputs[PAD_VEL_OUTPUT].getVoltage() * 0.1f, 0.f, 1.f), tl[T_PADS], tr[T_PADS]);
		}

		// ---- DUCK: 3 ms down after the kick, then back over about 150 ms (bass and pads)
		{
			float dk = params[DUCK_PARAM].getValue();
			if (duckRise) {
				duckEnv += (1.f - duckEnv) * std::min(1.f, args.sampleTime / 0.003f);
				if (duckEnv > 0.97f)
					duckRise = false;
			}
			else
				duckEnv *= std::exp(-args.sampleTime / 0.15f);
			float g = 1.f - 0.85f * dk * duckEnv;
			tl[T_BASS] *= g, tr[T_BASS] *= g;
			tl[T_PADS] *= g, tr[T_PADS] *= g;
		}

		// ---- mix: a patched track output takes the track out of the sum; L alone: the mono sum
		float ml = 0.f, mr = 0.f;
		for (int t = 0; t < 4; t++) {
			float gn = gainOf(params[LEVEL_PARAM + t].getValue());
			float l = tl[t] * gn, r = tr[t] * gn;
			bool oL = outputs[TRACK_L_OUTPUT + t].isConnected(), oR = outputs[TRACK_R_OUTPUT + t].isConnected();
			bool own = oL || oR;
			outputs[TRACK_L_OUTPUT + t].setVoltage(own ? (oR ? l : 0.5f * (l + r)) : 0.f);
			outputs[TRACK_R_OUTPUT + t].setVoltage(own ? r : 0.f);
			if (!own)
				ml += l, mr += r;
		}
		float mg = gainOf(params[MASTER_PARAM].getValue());
		auto soft = [](float x) { return x > 8.f || x < -8.f ? (x > 0.f ? 1.f : -1.f) * (8.f + 2.f * std::tanh((std::fabs(x) - 8.f) * 0.5f)) : x; };
		float oL = soft(ml * mg), oR = soft(mr * mg);
		outputs[MAIN_L_OUTPUT].setVoltage(outputs[MAIN_R_OUTPUT].isConnected() ? oL : 0.5f * (oL + oR));
		outputs[MAIN_R_OUTPUT].setVoltage(oR);

		lights[SOUND_LIGHT].setBrightness(sound ? 1.f : 0.f);
		for (int t = 0; t < 4; t++)
			lights[HOLD_LIGHT + t].setBrightness(params[HOLD_PARAM + t].getValue());
	}

	json_t* dataToJson() override {
		json_t* root = Genra::dataToJson();
		json_t* sm = json_array();
		for (int t = 0; t < 4; t++)
			json_array_append_new(sm, json_integer(soundMode[t]));
		json_object_set_new(root, "soundMode", sm);
		json_object_set_new(root, "soundAll", json_integer(soundAll));
		json_t* pr = json_array();
		for (int t = 0; t < 4; t++)
			json_array_append_new(pr, json_integer(preset[t]));
		json_object_set_new(root, "presets", pr);
		return root;
	}
	void dataFromJson(json_t* root) override {
		Genra::dataFromJson(root);
		json_t* sm = json_object_get(root, "soundMode");
		for (int t = 0; t < 4; t++) {
			json_t* v = sm ? json_array_get(sm, t) : nullptr;
			soundMode[t] = v ? clamp((int)json_integer_value(v), 0, 2) : 0;
		}
		if (json_t* j = json_object_get(root, "soundAll"))
			soundAll = clamp((int)json_integer_value(j), 1, 2);
		json_t* pr = json_object_get(root, "presets");
		for (int t = 0; t < 4; t++) {
			json_t* v = pr ? json_array_get(pr, t) : nullptr;
			if (v)
				preset[t] = std::max(0, (int)json_integer_value(v)); // (DRUM-X kits wrap around)
		}
	}
};

// ================================================================= drawing helpers
static std::shared_ptr<window::Font> boldFont() { return APP->window->loadFont(asset::system("res/fonts/Nunito-Bold.ttf")); }
static std::shared_ptr<window::Font> monoFont() { return APP->window->loadFont(asset::system("res/fonts/ShareTechMono-Regular.ttf")); }

static void txt(NVGcontext* vg, std::shared_ptr<window::Font> f, float xmm, float ymm, float sizeMm, NVGcolor c, const std::string& s,
                int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, float spacing = 0.f) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	nvgFillColor(vg, c);
	nvgTextAlign(vg, align);
	nvgText(vg, mm2px(xmm), mm2px(ymm), s.c_str(), NULL);
}
static float txtW(NVGcontext* vg, std::shared_ptr<window::Font> f, float sizeMm, float spacing, const std::string& s) {
	nvgFontFaceId(vg, f->handle);
	nvgFontSize(vg, mm2px(sizeMm));
	nvgTextLetterSpacing(vg, mm2px(spacing));
	float b[4];
	nvgTextBounds(vg, 0, 0, s.c_str(), NULL, b);
	return (b[2] - b[0]) / mm2px(1.f);
}

static const NVGcolor DIM = nvgRGB(0x8a, 0x8d, 0x99);
static const NVGcolor WHITE = nvgRGB(0xf2, 0xf3, 0xf7);

// a field on the panel (dark box with chevron from tools/panels/genrabox_panel.py): text left, small text right
struct Field : OpaqueWidget {
	GenraBox* module = nullptr;
	bool hover = false;
	virtual void texts(std::string& left, NVGcolor& lc, std::string& right) = 0;
	virtual void openMenu() = 0;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		auto bold = boldFont();
		if (!bold)
			return;
		NVGcontext* vg = args.vg;
		float w = box.size.x / mm2px(1.f), h = box.size.y / mm2px(1.f);
		std::string left = "--", right;
		NVGcolor lc = DIM;
		texts(left, lc, right);
		if (hover) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, mm2px(0.9f));
			nvgFillColor(vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
			nvgFill(vg);
		}
		float rw = right.empty() ? 0.f : txtW(vg, bold, 1.5f, 0.15f, right) + 1.2f;
		float avail = w - 6.f - rw, size = h > 5.6f ? 2.4f : 2.1f;
		while (size > 1.4f && txtW(vg, bold, size, 0.15f, left) > avail)
			size -= 0.1f;
		txt(vg, bold, 1.6f, h / 2.f, size, lc, left, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE, 0.15f);
		if (!right.empty())
			txt(vg, bold, w - 5.6f, h / 2.f, 1.5f, DIM, right, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE, 0.15f);
		Widget::drawLayer(args, layer);
	}
	void onHover(const HoverEvent& e) override { e.consume(this); }
	void onEnter(const EnterEvent& e) override { hover = true; }
	void onLeave(const LeaveEvent& e) override { hover = false; }
	void onButton(const ButtonEvent& e) override {
		if (!module || e.action != GLFW_PRESS || (e.button != GLFW_MOUSE_BUTTON_LEFT && e.button != GLFW_MOUSE_BUTTON_RIGHT))
			return;
		openMenu();
		e.consume(this);
	}
};

// families -> genres -> variants; pick(profile) is called with the chosen profile
template <typename IsSel, typename Pick>
static void genreMenu(ui::Menu* menu, IsSel isSel, Pick pick) {
	for (int f = 0; f < (int)(sizeof(FAMILIES) / sizeof(FAMILIES[0])); f++) {
		const Family& fam = FAMILIES[f];
		bool inFam = false;
		for (int g = fam.first; g < fam.first + fam.count; g++)
			for (int v = 0; v < numVariants(g); v++)
				inFam |= isSel(profileOf(g, v));
		menu->addChild(createSubmenuItem(fam.name, inFam ? "✔" : "", [=](ui::Menu* sub) {
			for (int g = fam.first; g < fam.first + fam.count; g++) {
				int nv = numVariants(g);
				std::string gname = prof(profileOf(g, 0)).name;
				if (nv == 1) {
					int p = profileOf(g, 0);
					sub->addChild(createCheckMenuItem(gname, "", [=]() { return isSel(p); }, [=]() { pick(p); }));
					continue;
				}
				bool here = false;
				for (int v = 0; v < nv; v++)
					here |= isSel(profileOf(g, v));
				sub->addChild(createSubmenuItem(gname, here ? "✔" : "", [=](ui::Menu* sub2) {
					for (int v = 0; v < nv; v++) {
						int p = profileOf(g, v);
						sub2->addChild(createCheckMenuItem(prof(p).variant, "", [=]() { return isSel(p); }, [=]() { pick(p); }));
					}
				}));
			}
		}));
	}
}

// the genre in the header: GENRE + VARIANT of GENRA
struct MainGenreField : Field {
	void texts(std::string& left, NVGcolor& lc, std::string& right) override {
		left = "07  UK GARAGE";
		lc = WHITE;
		right = "CLASSIC";
		if (!module)
			return;
		int p = module->curGenre;
		int g = genreOfProfile(p);
		left = string::f("%02d  ", g + 1) + upper(prof(p).name) + (module->pendingGenre ? " …" : "");
		right = numVariants(g) > 1 ? upper(prof(p).variant) : "";
	}
	void openMenu() override {
		GenraBox* m = module;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("Genre (main)"));
		genreMenu(
		    menu, [=](int p) { return m->knobProfile() == p; },
		    [=](int p) {
			    m->params[Genra::GENRE_PARAM].setValue((float)genreOfProfile(p));
			    m->params[Genra::VARIANT_PARAM].setValue((float)variantOfProfile(p));
		    });
	}
};

// PATTERN: the genre a track plays ("= MAIN" follows the header)
struct TrackGenreField : Field {
	int track = 0;
	void texts(std::string& left, NVGcolor& lc, std::string& right) override {
		left = "UK GARAGE";
		right = "= MAIN";
		if (!module)
			return;
		GenraBox* m = module;
		bool own = m->trackSel[track] >= 0;
		left = upper(prof(own ? (int)m->trackSel[track] : m->curGenre).name);
		if (m->pendingTrack[track])
			left += " …";
		lc = own ? TRACK_COLORS[track] : DIM;
		right = own ? "OWN" : "= MAIN";
	}
	void openMenu() override {
		GenraBox* m = module;
		int t = track;
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel(std::string(VOICE_NAMES[t]) + " pattern genre"));
		menu->addChild(createCheckMenuItem("= MAIN (follows the header)", "", [=]() { return m->trackSel[t] < 0; }, [=]() { m->setTrackSel(t, -1); }));
		menu->addChild(new ui::MenuSeparator);
		genreMenu(
		    menu, [=](int p) { return m->trackSel[t] == p; }, [=](int p) { m->setTrackSel(t, p); });
	}
};

// SOUND: the preset of a voice (DRUM-X: the kit). Click = list by style, wheel = next / previous
struct PresetField : Field {
	int track = 0;
	std::string name(int i) {
		GenraBox* m = module;
		switch (track) {
			case T_DRUMS: {
				const jdx_api* a = m->drums.api();
				return a && i < (int) a->nkits() ? upper(a->kit_name((uint32_t) i)) : string::f("KIT %d", i + 1);
			}
			case T_BASS: return boxv::Bass::name(i);
			case T_MELODY: return boxv::Melody::name(i);
			default: return boxv::Pads::name(i);
		}
	}
	int count() { return GenraBox::presetCount(track, module->drums); }
	static std::string hname(follow::Host* h, int i) {
		char n[64], st[96];
		h->hostPresetInfo(i, n, sizeof(n), st, sizeof(st));
		return n;
	}
	static std::string hstyle(follow::Host* h, int i) {
		char n[64], st[96];
		h->hostPresetInfo(i, n, sizeof(n), st, sizeof(st));
		return st[0] ? std::string(st) : std::string("USER");
	}
	/** the first big instrument that follows this track (UI thread) */
	follow::Host* followHost() {
		if (!module || module->followIds[track].empty())
			return nullptr;
		return follow::host(APP->engine->getModule(module->followIds[track][0]));
	}
	int current() {
		int n = std::max(1, count());
		return ((module->preset[track] % n) + n) % n;
	}
	void texts(std::string& left, NVGcolor& lc, std::string& right) override {
		static const char* DEMO[4] = {"808 GARAGE", "BOOM BAP", "VOCAL CHOP", "ORGAN STAB"};
		left = DEMO[track];
		lc = WHITE;
		right = "01/30";
		if (!module)
			return;
		if (follow::Host* h = followHost()) {
			int c = h->hostPresetCurrent();
			left = "\xe2\x80\xba " + std::string(h->srcName) + "  " + (c >= 0 ? upper(hname(h, c)) : std::string("INIT"));
			lc = TRACK_COLORS[track];
			right = h->hostPresetCount() > 0 ? string::f("%02d/%02d", c + 1, h->hostPresetCount()) : "";
			return;
		}
		left = name(current());
		right = string::f("%02d/%02d", current() + 1, count());
	}
	void openMenu() override {
		GenraBox* m = module;
		int t = track;
		ui::Menu* menu = createMenu();
		if (follow::Host* h = followHost()) { // the big instrument's presets
			menu->addChild(createMenuLabel(std::string(h->srcName) + " plays this track: its presets"));
			int64_t hid = module->followIds[t][0];
			int hn = h->hostPresetCount();
			std::vector<std::string> styles;
			for (int i = 0; i < hn; i++) {
				std::string st = hstyle(h, i);
				if (std::find(styles.begin(), styles.end(), st) == styles.end())
					styles.push_back(st);
			}
			auto item = [=](int i) {
				follow::Host* y = follow::host(APP->engine->getModule(hid));
				std::string nm = string::f("%02d  ", i + 1) + (y ? hname(y, i) : std::string());
				return createCheckMenuItem(nm, "", [=]() {
					follow::Host* x = follow::host(APP->engine->getModule(hid));
					return x && x->hostPresetCurrent() == i;
				}, [=]() {
					follow::Host* x = follow::host(APP->engine->getModule(hid));
					if (x)
						x->hostPresetLoad(i);
				});
			};
			if (styles.size() <= 1)
				for (int i = 0; i < hn; i++)
					menu->addChild(item(i));
			else
				for (const std::string& st : styles)
					menu->addChild(createSubmenuItem(st, "", [=](ui::Menu* sub) {
						for (int i = 0; i < hn; i++) {
							follow::Host* x = follow::host(APP->engine->getModule(hid));
							if (x && hstyle(x, i) == st)
								sub->addChild(item(i));
						}
					}));
			menu->addChild(new ui::MenuSeparator);
			menu->addChild(createMenuLabel("NEW with SOUND on picks from"));
			for (int k = 0; k < 3; k++)
				menu->addChild(createCheckMenuItem(SOUND_MODES[k], "", [=]() { return m->soundMode[t] == k; }, [=]() { m->soundMode[t] = k; }));
			return;
		}
		menu->addChild(createMenuLabel(std::string(VOICE_NAMES[t]) + (t == T_DRUMS ? " kit" : " preset")));
		int n = count();
		if (t == T_DRUMS) {
			for (int i = 0; i < n; i++)
				menu->addChild(createCheckMenuItem(string::f("%02d  ", i + 1) + name(i), "", [=]() { return current() == i; }, [=]() { m->presetReq[t] = i; }));
		}
		else {
			// grouped by style, in the list's order
			std::vector<std::string> styles;
			for (int i = 0; i < n; i++) {
				std::string st = GenraBox::presetStyle(t, i);
				if (std::find(styles.begin(), styles.end(), st) == styles.end())
					styles.push_back(st);
			}
			for (const std::string& st : styles) {
				bool here = GenraBox::presetStyle(t, current()) == st;
				menu->addChild(createSubmenuItem(st, here ? "\xe2\x9c\x94" : "", [=](ui::Menu* sub) {
					for (int i = 0; i < n; i++)
						if (GenraBox::presetStyle(t, i) == st)
							sub->addChild(createCheckMenuItem(string::f("%02d  ", i + 1) + name(i), "", [=]() { return current() == i; },
							                                  [=]() { m->presetReq[t] = i; }));
				}));
			}
		}
		menu->addChild(new ui::MenuSeparator);
		menu->addChild(createMenuLabel("NEW with SOUND on picks from"));
		for (int k = 0; k < 3; k++)
			menu->addChild(createCheckMenuItem(SOUND_MODES[k], "", [=]() { return m->soundMode[t] == k; }, [=]() { m->soundMode[t] = k; }));
		if (t == T_DRUMS)
			menu->addChild(createMenuLabel("(kits have no style: always all of them)"));
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module || e.scrollDelta.y == 0.f)
			return;
		if (follow::Host* h = followHost()) {
			int hn = h->hostPresetCount();
			if (hn > 0)
				h->hostPresetLoad((std::max(0, h->hostPresetCurrent()) + (e.scrollDelta.y > 0.f ? -1 : 1) + hn) % hn);
			e.consume(this);
			return;
		}
		int n = std::max(1, count());
		module->presetReq[track] = (current() + (e.scrollDelta.y > 0.f ? -1 : 1) + n) % n;
		e.consume(this);
	}
};

// ================================================================= display: BPM, key, lanes
struct Display : Widget {
	GenraBox* module = nullptr;
	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer != 1)
			return;
		auto bold = boldFont();
		auto mono = monoFont();
		if (!bold || !mono)
			return;
		NVGcontext* vg = args.vg;
		float wmm = box.size.x / mm2px(1.f);
		// the BPM field sits in the display's top-left (L::BPMF, relative to the display)
		float bx = L::BPMF[0] - L::DISPLAY[0], by = L::BPMF[1] - L::DISPLAY[1], bw = L::BPMF[2], bh = L::BPMF[3];

		float bpm = 124.f;
		bool ext = false;
		std::string info = "A MINOR  4 BARS  1/4", status = "SOUND: ALL GENRES";
		NVGcolor stc = SOUND_COLOR;
		bool lanes[4][16] = {};
		int curStep = -1;
		bool runningNow = true;
		if (module) {
			GenraBox* m = module;
			Params P = m->buildParams();
			int L_ = m->lengthSteps();
			ext = m->inputs[Genra::CLOCK_INPUT].isConnected();
			bpm = ext ? (m->stepPeriod > 0.f ? 60.f * APP->engine->getSampleRate() / (m->stepPeriod * 4.f) : 0.f)
			          : m->params[Genra::BPM_PARAM].getValue();
			int s = std::max(0, m->step);
			std::string scaleName = upper(SCALES[P.scale].name);
			info = string::f("%s %s  %d BAR%s  %d/%d", noteName(P.key + P.transp, P.key + P.transp, P.scale), scaleName.c_str(),
			                 L_ / 16, L_ > 16 ? "S" : "", s / 16 + 1, L_ / 16);
			runningNow = m->running;
			if (!m->running) {
				status = "STOP";
				stc = DIM;
			}
			else if (m->chordLocked()) {
				status = "CHORDS LOCKED";
				stc = HARMONY_COLOR;
			}
			else if (m->params[GenraBox::SOUND_PARAM].getValue() > 0.5f)
				status = m->soundAll == 2 ? "SOUND: SAME GENRE" : "SOUND: ALL GENRES";
			else {
				status = "SOUND OFF";
				stc = DIM;
			}
			int barStart = (s / 16) * 16;
			curStep = m->step >= 0 ? s % 16 : -1;
			int prev[4];
			std::memcpy(prev, m->padPrev, sizeof(prev));
			bool hp = false;
			for (int i = 0; i < 16; i++) {
				int st2 = barStart + i;
				if (st2 >= L_)
					break;
				Hit hits[NUM_DRUMS];
				drumStep(m->pat, P, st2, L_, false, hits);
				for (int v = 0; v < NUM_DRUMS; v++)
					lanes[0][i] |= hits[v].on;
				lanes[1][i] = bassAt(m->pat, P, st2, L_).on;
				lanes[2][i] = melAt(m->pat, P, st2, L_).on;
				lanes[3][i] = padAt(m->pat, P, st2, L_, prev, hp).on;
			}
		}

		// BPM field
		nvgBeginPath(vg);
		nvgRoundedRect(vg, mm2px(bx), mm2px(by), mm2px(bw), mm2px(bh), mm2px(0.6f));
		nvgFillColor(vg, nvgRGB(0x1a, 0x1b, 0x21));
		nvgFill(vg);
		nvgStrokeColor(vg, nvgRGB(0x5a, 0x5c, 0x66));
		nvgStrokeWidth(vg, mm2px(0.25f));
		nvgStroke(vg);
		txt(vg, bold, bx + 1.0f, by + 4.6f, 3.8f, ext ? DIM : WHITE, string::f("%.0f", bpm), NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.1f);
		txt(vg, bold, bx + bw - 0.8f, by + 4.6f, 1.5f, DIM, "BPM", NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE, 0.1f);
		NVGcolor clk = ext ? nvgRGB(0x3d, 0xc9, 0xff) : nvgRGB(0x5d, 0xff, 0x8a);
		nvgBeginPath(vg);
		nvgCircle(vg, mm2px(bx + bw + 2.0f), mm2px(by + 1.6f), mm2px(0.7f));
		nvgFillColor(vg, clk);
		nvgFill(vg);
		txt(vg, bold, bx + bw + 3.2f, by + 2.2f, 1.6f, clk, ext ? "EXT" : "INT", NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.1f);
		// key · length · bar, status
		txt(vg, mono, bx + 0.6f, by + 9.4f, 2.1f, DIM, info);
		txt(vg, bold, bx + 0.6f, by + 12.9f, 1.7f, stc, status, NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE, 0.2f);

		// lanes (GENRA's track order: drums, bass, melody, pads)
		float lx = wmm - 2.f - 16 * 2.05f;
		for (int t = 0; t < 4; t++) {
			float y = 1.8f + t * 3.1f;
			for (int i = 0; i < 16; i++) {
				float x = lx + i * 2.05f;
				bool on = module ? lanes[t][i] : ((i * 7 + t * 5) % 11 < (5 - t));
				nvgBeginPath(vg);
				nvgRoundedRect(vg, mm2px(x), mm2px(y), mm2px(1.6f), mm2px(2.4f), mm2px(0.3f));
				nvgFillColor(vg, on ? TRACK_COLORS[t] : nvgRGB(0x1d, 0x1e, 0x24));
				nvgFill(vg);
			}
		}
		if (curStep >= 0 && runningNow) {
			nvgBeginPath(vg);
			nvgRoundedRect(vg, mm2px(lx + curStep * 2.05f - 0.3f), mm2px(1.4f), mm2px(2.2f), mm2px(12.6f), mm2px(0.4f));
			nvgStrokeColor(vg, WHITE);
			nvgStrokeWidth(vg, mm2px(0.28f));
			nvgStroke(vg);
		}
		Widget::drawLayer(args, layer);
	}
};

// the BPM field: drag up / down or scroll; double-click = the genre's typical tempo; right-click = type it
struct BpmField : OpaqueWidget {
	GenraBox* module = nullptr;
	float acc = 0.f;
	bool ext() { return module && module->inputs[Genra::CLOCK_INPUT].isConnected(); }
	void nudge(float d) {
		ParamQuantity* q = module->paramQuantities[Genra::BPM_PARAM];
		q->setValue(clamp(std::round(q->getValue() + d), q->getMinValue(), q->getMaxValue()));
	}
	void onButton(const ButtonEvent& e) override {
		OpaqueWidget::onButton(e);
		if (!module || ext())
			return;
		if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_RIGHT) {
			ui::Menu* menu = createMenu();
			menu->addChild(createMenuLabel("Tempo (internal clock)"));
			struct BpmEdit : ui::TextField {
				GenraBox* m;
				void onSelectKey(const SelectKeyEvent& e) override {
					if (e.action == GLFW_PRESS && (e.key == GLFW_KEY_ENTER || e.key == GLFW_KEY_KP_ENTER)) {
						float v = std::atof(text.c_str());
						if (v >= 40.f && v <= 220.f)
							m->params[Genra::BPM_PARAM].setValue(std::round(v));
						ui::MenuOverlay* o = getAncestorOfType<ui::MenuOverlay>();
						if (o)
							o->requestDelete();
						e.consume(this);
					}
					if (!e.getTarget())
						ui::TextField::onSelectKey(e);
				}
			};
			BpmEdit* f = new BpmEdit;
			f->m = module;
			f->box.size.x = 80.f;
			f->text = string::f("%.0f", module->params[Genra::BPM_PARAM].getValue());
			f->selectAll();
			menu->addChild(f);
			APP->event->setSelectedWidget(f);
			e.consume(this);
		}
	}
	void onDoubleClick(const DoubleClickEvent& e) override {
		if (!module || ext())
			return;
		module->params[Genra::BPM_PARAM].setValue(std::round(clamp(prof(module->curGenre).bpm, 40.f, 220.f)));
		e.consume(this);
	}
	void onDragStart(const DragStartEvent& e) override {
		if (e.button == GLFW_MOUSE_BUTTON_LEFT)
			acc = 0.f;
	}
	void onDragMove(const DragMoveEvent& e) override {
		if (!module || ext() || e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		acc -= e.mouseDelta.y / mm2px(1.2f); // one BPM per 1.2 mm
		float d = std::trunc(acc);
		if (d != 0.f) {
			nudge(d);
			acc -= d;
		}
	}
	void onHoverScroll(const HoverScrollEvent& e) override {
		if (!module || ext())
			return;
		nudge(e.scrollDelta.y > 0.f ? 1.f : (e.scrollDelta.y < 0.f ? -1.f : 0.f));
		e.consume(this);
	}
};

// ================================================================= panel
struct GenraBoxWidget : ModuleWidget {
	GenraBoxWidget(GenraBox* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/GenraBox.svg")));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		auto P = [](const float* p) { return mm2px(Vec(p[0], p[1])); };
		auto small = [&](const float* p, int id) { addParam(createParamCentered<GSmallKnob>(P(p), module, id)); };
		auto in = [&](const float* p, int id) { addInput(createInputCentered<PJ301MPort>(P(p), module, id)); };
		auto out = [&](const float* p, int id) { addOutput(createOutputCentered<PJ301MPort>(P(p), module, id)); };
		auto place = [&](Widget* w, const float* f) {
			w->box.pos = mm2px(Vec(f[0], f[1]));
			w->box.size = mm2px(Vec(f[2], f[3]));
			addChild(w);
		};

		Display* disp = new Display;
		disp->module = module;
		place(disp, L::DISPLAY);
		BpmField* bpm = new BpmField;
		bpm->module = module;
		place(bpm, L::BPMF);
		MainGenreField* gf = new MainGenreField;
		gf->module = module;
		place(gf, L::GENREF);

		// generator
		small(L::KEY, Genra::KEY_PARAM);
		small(L::COLOR, Genra::COLOR_PARAM);
		small(L::SWING, Genra::SWING_PARAM);
		small(L::LENGTH, Genra::LENGTH_PARAM);
		small(L::MUTATE, Genra::MUTATE_PARAM);
		addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(L::NEWALL), module, Genra::NEW_PARAM, Genra::NEW_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<WhiteLight>>(P(L::CHLOCK), module, Genra::CHORDLOCK_PARAM, Genra::CHORDLOCK_LIGHT));
		addParam(createLightParamCentered<VCVLightBezel<RunLight>>(P(L::RUN), module, Genra::RUN_PARAM, Genra::RUN_LIGHT));
		addParam(createLightParamCentered<VCVLightBezelLatch<SoundLight>>(P(L::SOUND), module, GenraBox::SOUND_PARAM, GenraBox::SOUND_LIGHT));

		// the four voices (panel column c plays GENRA track COL_TRACK[c])
		const float* tg[4] = {L::TGENRE0, L::TGENRE1, L::TGENRE2, L::TGENRE3};
		const float* pr[4] = {L::PRESET0, L::PRESET1, L::PRESET2, L::PRESET3};
		const float* dens[4] = {L::DENS0, L::DENS1, L::DENS2, L::DENS3};
		const float* shape[4] = {L::SHAPE0, L::SHAPE1, L::SHAPE2, L::SHAPE3};
		const float* ma[4] = {L::MACA0, L::MACA1, L::MACA2, L::MACA3};
		const float* mb[4] = {L::MACB0, L::MACB1, L::MACB2, L::MACB3};
		const float* lv[4] = {L::LEVEL0, L::LEVEL1, L::LEVEL2, L::LEVEL3};
		const float* mu[4] = {L::MUTE0, L::MUTE1, L::MUTE2, L::MUTE3};
		const float* nw[4] = {L::NEW0, L::NEW1, L::NEW2, L::NEW3};
		const float* ho[4] = {L::HOLD0, L::HOLD1, L::HOLD2, L::HOLD3};
		const int SHAPE_PARAM[4] = {Genra::FILL_PARAM, Genra::MOVE_PARAM, Genra::RANGE_PARAM, Genra::VOICING_PARAM}; // by track
		for (int c = 0; c < 4; c++) {
			int t = COL_TRACK[c];
			TrackGenreField* f = new TrackGenreField;
			f->module = module;
			f->track = t;
			place(f, tg[c]);
			PresetField* pf = new PresetField;
			pf->module = module;
			pf->track = t;
			place(pf, pr[c]);
			small(dens[c], Genra::DENS_PARAM + t);
			small(shape[c], SHAPE_PARAM[t]);
			addParam(createParamCentered<GKnob>(P(ma[c]), module, GenraBox::MACA_PARAM + t));
			addParam(createParamCentered<GKnob>(P(mb[c]), module, GenraBox::MACB_PARAM + t));
			small(lv[c], GenraBox::LEVEL_PARAM + t);
			addParam(createLightParamCentered<VCVLightBezel<WhiteLight>>(P(nw[c]), module, Genra::TNEW_PARAM + t, Genra::TNEW_LIGHT + t));
			switch (t) {
				case T_DRUMS:
					addParam(createLightParamCentered<VCVLightBezelLatch<DrumLight>>(P(mu[c]), module, Genra::MUTE_PARAM + t, Genra::MUTE_LIGHT + t));
					addParam(createLightParamCentered<VCVLightBezelLatch<DrumLight>>(P(ho[c]), module, GenraBox::HOLD_PARAM + t, GenraBox::HOLD_LIGHT + t));
					break;
				case T_BASS:
					addParam(createLightParamCentered<VCVLightBezelLatch<BassLight>>(P(mu[c]), module, Genra::MUTE_PARAM + t, Genra::MUTE_LIGHT + t));
					addParam(createLightParamCentered<VCVLightBezelLatch<BassLight>>(P(ho[c]), module, GenraBox::HOLD_PARAM + t, GenraBox::HOLD_LIGHT + t));
					break;
				case T_MELODY:
					addParam(createLightParamCentered<VCVLightBezelLatch<MelLight>>(P(mu[c]), module, Genra::MUTE_PARAM + t, Genra::MUTE_LIGHT + t));
					addParam(createLightParamCentered<VCVLightBezelLatch<MelLight>>(P(ho[c]), module, GenraBox::HOLD_PARAM + t, GenraBox::HOLD_LIGHT + t));
					break;
				default:
					addParam(createLightParamCentered<VCVLightBezelLatch<PadLight>>(P(mu[c]), module, Genra::MUTE_PARAM + t, Genra::MUTE_LIGHT + t));
					addParam(createLightParamCentered<VCVLightBezelLatch<PadLight>>(P(ho[c]), module, GenraBox::HOLD_PARAM + t, GenraBox::HOLD_LIGHT + t));
			}
		}

		// jacks
		in(L::IN_CLOCK, Genra::CLOCK_INPUT);
		in(L::IN_RESET, Genra::RESET_INPUT);
		in(L::IN_RUN, Genra::RUN_INPUT);
		in(L::IN_NEW, Genra::NEW_INPUT);
		out(L::OUT_CLOCK, Genra::CLOCK_OUTPUT);
		const float* ol[4] = {L::OUT_DR_L, L::OUT_BA_L, L::OUT_ME_L, L::OUT_PA_L};
		const float* orr[4] = {L::OUT_DR_R, L::OUT_BA_R, L::OUT_ME_R, L::OUT_PA_R};
		for (int c = 0; c < 4; c++) {
			out(ol[c], GenraBox::TRACK_L_OUTPUT + COL_TRACK[c]);
			out(orr[c], GenraBox::TRACK_R_OUTPUT + COL_TRACK[c]);
		}
		out(L::OUT_NOTES, GenraBox::NOTES_OUTPUT);
		out(L::OUT_GATES, GenraBox::GATES_OUTPUT);
		out(L::OUT_DRUMS, Genra::DRUMS_OUTPUT);
		small(L::DUCK, GenraBox::DUCK_PARAM);
		small(L::MASTER, GenraBox::MASTER_PARAM);
		out(L::OUT_L, GenraBox::MAIN_L_OUTPUT);
		out(L::OUT_R, GenraBox::MAIN_R_OUTPUT);
	}

	float followTimer = 0.f;
	void step() override {
		ModuleWidget::step();
		GenraBox* m = dynamic_cast<GenraBox*>(module);
		if (!m)
			return;
		// NEW rolled a track: the big instruments that follow it (and allow it) load a preset (GENRA's uiPresetRolls)
		m->uiPresetRolls();
		followTimer -= APP->window->getLastFrameDuration();
		if (followTimer > 0.f)
			return;
		followTimer = 0.5f;
		// big instruments that follow a track of this box (Follow GENRA BOX): the box's own voice of that track is quiet
		int bits = 0;
		std::string names[4];
		std::vector<int64_t> ids[4];
		for (int64_t id : APP->engine->getModuleIds()) {
			follow::Host* h = follow::host(APP->engine->getModule(id));
			if (!h || !h->follows || h->fol.genra.to.load() != static_cast<Module*>(m))
				continue;
			static const int FROM_FOLLOW[4] = {T_BASS, T_MELODY, T_PADS, T_DRUMS}; // follow::Track -> GENRA track
			int ft = h->fol.track.load();
			if (ft < 0 || ft > 3)
				continue;
			int t = FROM_FOLLOW[ft];
			bits |= 1 << t;
			names[t] += (names[t].empty() ? "" : ", ") + std::string(h->srcName);
			ids[t].push_back(id);
		}
		m->followed = bits;
		for (int t = 0; t < 4; t++) {
			m->followName[t] = names[t];
			m->followIds[t] = ids[t];
		}
	}

	void appendContextMenu(Menu* menu) override {
		GenraBox* m = dynamic_cast<GenraBox*>(module);
		if (!m)
			return;
		menu->addChild(new MenuSeparator);
		menu->addChild(createMenuLabel("GENRA BOX"));
		menu->addChild(createMenuItem("Export as MIDI file…", "all tracks", [=]() { m->exportMidi(); }));
		static const char* TN[4] = {"drums", "bass", "melody", "pads"};
		for (int t = 0; t < 4; t++)
			menu->addChild(createMenuItem(std::string("Export ") + TN[t] + " only…", "", [=]() { m->exportMidi(t); }));
		menu->addChild(createMenuItem("Import MIDI file…", "", [=]() { m->importMidi(); }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("SOUND button picks presets from", {"All genres", "Same genre"},
		                                      [=]() { return (size_t)(m->soundAll == 2 ? 1 : 0); }, [=](size_t i) { m->soundAll = i == 1 ? 2 : 1; }));
		std::vector<std::string> scales = {"Auto (genre)"};
		for (int s = 0; s < NUM_SCALES; s++)
			scales.push_back(SCALES[s].name);
		menu->addChild(createIndexSubmenuItem("Scale", scales, [=]() { return (size_t)clamp((int)std::round(m->params[Genra::SCALE_PARAM].getValue()), 0, NUM_SCALES); },
		                                      [=](size_t i) { m->params[Genra::SCALE_PARAM].setValue((float)i); }));
		menu->addChild(createIndexSubmenuItem("Chord mode", {CHORD_MODES[0], CHORD_MODES[1], CHORD_MODES[2]},
		                                      [=]() { return (size_t)m->chordMode; }, [=](size_t i) { m->chordMode = (int)i; }));
		menu->addChild(createMenuItem("Re-roll chords only", "", [=]() { m->chordRegenRequested = true; }));
		menu->addChild(createMenuItem("All tracks to the main genre", "", [=]() { m->resetTracksRequested = true; }));
		menu->addChild(createIndexSubmenuItem("Melody", {"Phrases (motif, answer, close)", "Free walk (before 2.30)"},
		                                      [=]() { return (size_t)(m->melPhrase ? 0 : 1); }, [=](size_t v) { m->melPhrase = v == 0; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("MUTATE mode",
		                                      {"Drift (the changes add up)", "Orbit (around the anchor)", "Pulse (changed, back, changed …)"},
		                                      [=]() { return (size_t)m->mutateMode; }, [=](size_t v) { m->mutateMode = (int)v; }));
		menu->addChild(createIndexSubmenuItem("MUTATE changes",
		                                      {"Everything", "Rhythm (drums)", "Notes (bass, melody)", "Harmony (chords, pads)"},
		                                      [=]() { return (size_t)m->mutateWhat; }, [=](size_t v) { m->mutateWhat = (int)v; }));
		menu->addChild(createMenuItem("Back to the anchor", "the pattern before MUTATE", [=]() { m->anchorBackReq = true; }));
		menu->addChild(new MenuSeparator);
		menu->addChild(createIndexSubmenuItem("Clock input", {CLOCK_MODES[0], CLOCK_MODES[1], CLOCK_MODES[2], CLOCK_MODES[3]},
		                                      [=]() { return (size_t)m->clockMode; }, [=](size_t i) { m->clockMode = (int)i; }));
		menu->addChild(createIndexSubmenuItem("Clock output", {CLOCK_MODES[0], CLOCK_MODES[1], CLOCK_MODES[2], CLOCK_MODES[3]},
		                                      [=]() { return (size_t)m->clockOutMode; }, [=](size_t i) { m->clockOutMode = (int)i; }));
	}
};

} // namespace box

Model* modelGenraBox = createModel<box::GenraBox, box::GenraBoxWidget>("GenraBox");
