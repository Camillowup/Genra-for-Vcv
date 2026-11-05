// GENRA – generative 4-track sequencer for VCV Rack
// Engine: music theory, genre profiles, pattern generation and step evaluation.
// Pure C++11, no Rack dependency (so it can be tested stand-alone).
#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace genra {

enum {
	NUM_DRUMS = 8,
	STEPS_PER_BAR = 16,
	MAX_BARS = 8,
	MAX_STEPS = MAX_BARS * STEPS_PER_BAR,
	MAX_PROG = 8,
	MAX_EVENTS = 32,
	NUM_GENRES = 24,
	MAX_VARIANTS = 3,
	MAX_PROFILES = 64,
	NUM_SCALES = 9,
	NO_ABS = -128,
};

enum Drum { KICK, SNARE, CLAP, RIM, CHAT, OHAT, TOM, PERC };
enum Track { T_DRUMS, T_BASS, T_MELODY, T_PADS, NUM_TRACKS };
enum ChordMode { CM_MIXED, CM_TEMPLATES, CM_FREE };
enum Quality { Q_MAJ, Q_MIN, Q_DIM, Q_AUG, Q_SUS2, Q_SUS4, Q_MASK };
enum ExtBits { EXT_7 = 1, EXT_9 = 2, EXT_DOM7 = 4, EXT_MAJ7 = 8 };
enum MelStyle { MS_RIFF, MS_LEAD, MS_ARP, MS_SPARSE, MS_ACID };
enum PadStyle { PS_SUSTAIN, PS_STABS, PS_SKANK };
enum FillStyle { FS_SNARE, FS_TOMS, FS_CHOP, FS_DROP, FS_ROLL };
enum DropMode { DM_NONE, DM_KICKBASS, DM_BREAK, DM_EIGHT };

extern const char* DRUM_NAMES[NUM_DRUMS];
extern const char* NOTE_NAMES[12];

// ---------------------------------------------------------------- random
struct Rng {
	uint64_t s;
	explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ULL) : s(seed ? seed : 1) {}
	uint32_t next() {
		s ^= s << 13;
		s ^= s >> 7;
		s ^= s << 17;
		return (uint32_t)(s >> 32);
	}
	float uni() { return (next() >> 8) * (1.f / 16777216.f); }
	int range(int n) { return n <= 0 ? 0 : (int)(next() % (uint32_t)n); }
	uint8_t byte() { return (uint8_t)(next() >> 24); }
};

// ---------------------------------------------------------------- scales
struct Scale {
	const char* name;
	int iv[7];
	int pentaMask; // bit per degree allowed for melodies, 0x7f = all
};
extern const Scale SCALES[NUM_SCALES];

// ---------------------------------------------------------------- genres
// A "profile" is one variant of a genre. The GENRE knob picks the genre, VARIANT one of its profiles.
struct Genre {
	const char* name = "";       // genre name
	const char* variant = "";    // variant name
	int scale = 1;               // native scale index
	float bpm = 120.f;           // typical tempo (display hint)
	float swing = 0.5f;          // 0.5 = straight, 0.75 = max
	float complexity = 0.4f;     // typical rhythmic complexity 0..1
	float color = 0.4f;          // typical harmonic colour 0..1
	bool jazzy = false;          // tritone substitutions instead of borrowed chords
	float rolls = 0.1f;          // hi-hat roll tendency
	int harm[3] = {16, 32, 8};   // steps per chord options
	float harmW[3] = {0.5f, 0.4f, 0.1f};
	const char* templates[8] = {};
	float degW[7] = {6, 0.3f, 1.0f, 1.5f, 0.8f, 1.6f, 1.6f};
	const char* drum[NUM_DRUMS] = {};  // 16 or 32 step grids ('X' 'x' 'o' 1-9 '.', '|' ignored)
	int bassKick = 0;            // +1 follow kick, -1 avoid kick, 0 independent
	int bassLen = 1;             // 0 short, 1 medium, 2 long
	float bassMove = 0.4f;
	float slide = 0.f;           // bass slide probability (scaled by MOVE)
	int bassCycle = 16;          // bass pattern length in steps (12/15 = polymeter)
	const char* bass = "X...";
	int melStyle = MS_LEAD;
	bool melPenta = false;
	float melSlide = 0.f;
	const char* mel = "X...";
	int padStyle = PS_SUSTAIN;
	float padBase = 1.f;         // pad presence at DENSITY centre (0 = off by default)
	const char* pad = "X...";
	int fillStyle = FS_SNARE;
	int dropMode = DM_NONE;
	float humanize = 0.f;        // timing spread (0..1 -> 0..20 ms) on snare, clap, rim, hats
	float chop = 0.f;            // break-chop amount (jungle)

	// parsed (filled by initGenres)
	float pDrum[NUM_DRUMS][32];
	bool pRoll[NUM_DRUMS][32];   // 'r' in the grid: roll / triplet position
	int drumLen = 16;
	bool drumUsed[NUM_DRUMS];
	float pBass[16], pMel[16], pPad[16];
	int nTemplates = 0;
};
void initGenres();
int numProfiles();
const Genre& prof(int idx);
int numVariants(int genre);
int profileOf(int genre, int variant);
int genreOfProfile(int idx);
int variantOfProfile(int idx);

// ---------------------------------------------------------------- pattern
struct DrumStep {
	uint8_t r, rg, rf, vj, rr, fv; // fv: fixed velocity (imported / edited), 0 = none
	uint8_t frat;                  // fixed: roll / ratchet count (0/1 = single hit, 2-4)
};

// flags of fixed notes (edited in the grid editor)
enum NoteFlags { FF_SLIDE = 1, FF_ACC = 2, FF_SET = 0x80 /* flags are valid (otherwise: legacy import) */ };

struct NoteStep {
	uint8_t r, rm, vj, ra, rs; // rs: slide random
	int8_t a1, b1, c1; // primary pitch
	int8_t a2, b2, c2; // alternative pitch (bass MOVE)
	int8_t absNote;    // imported: midi note minus key, NO_ABS otherwise
	uint8_t force;     // 0 probabilistic, 1 on, 2 off
	uint8_t flen, fvel;
	uint8_t fflags;    // fixed: NoteFlags
	uint8_t fgate;     // fixed: gate in % of the length (0 = default)
	int8_t froot;      // fixed: chord root (semitones above key) the note was written against, -1 = unknown
};

struct PadStep {
	uint8_t r, vj, force, flen, fvel;
	uint8_t nNotes;   // imported: exact notes (midi minus key)
	int8_t notes[4];
	uint8_t fgate;    // fixed: gate in % (0 = default)
	int8_t froot;     // fixed: chord root the notes were written against, -1 = unknown
};

struct ChordSpec {
	int8_t root;      // semitones above key root, 0..11
	uint8_t quality;  // Quality
	uint8_t ext;      // ExtBits requested by template
	uint8_t rc, rs;   // random values for colour / substitution
	int8_t extOff;    // user offset for extensions (-3..3)
	uint8_t explicitQ;
	uint16_t mask;    // interval mask for Q_MASK (bit n = n semitones above root)
};

struct ChordEvent {
	uint8_t spec, start, len, pad;
};

struct Pattern {
	uint32_t magic;
	uint32_t version;
	int8_t trackGenre[NUM_TRACKS];
	int8_t chordGenre;
	uint8_t melMode;  // 0 degrees, 1 arpeggio (chord tones)
	int8_t melCenter;
	uint8_t customTimeline;
	uint8_t harm;
	uint8_t nProg, nEvents, layoutLen;
	uint8_t fixed[NUM_TRACKS]; // track plays imported / edited notes exactly
	uint8_t chordFollow[NUM_TRACKS]; // fixed notes move with the chords (diatonically)
	uint8_t reserved[8];
	uint8_t chop[MAX_BARS][16]; // break-chop source step per bar
	uint8_t chopR[MAX_BARS];
	ChordSpec prog[MAX_PROG];
	ChordEvent ev[MAX_EVENTS];
	DrumStep drums[NUM_DRUMS][MAX_STEPS];
	NoteStep bass[MAX_STEPS];
	NoteStep mel[MAX_STEPS];
	PadStep pads[MAX_STEPS];
};

static const uint32_t PATTERN_MAGIC = 0x47454e52; // "GENR"
static const uint32_t PATTERN_VERSION = 2;

// version-independent storage (Store.cpp); loadPattern also accepts the raw block of GENRA v2
std::vector<uint8_t> savePattern(const Pattern& pat);
bool loadPattern(const std::vector<uint8_t>& data, Pattern& pat);
std::vector<uint8_t> v2RawForTest(const Pattern& pat);

// ---------------------------------------------------------------- live parameters
struct Params {
	int genre = 0;          // current knob genre
	int key = 9;            // 0 = C
	int transp = 0;         // semitones
	int scale = 1;          // resolved scale index
	bool scaleOverride = false;
	float complexK = 0.5f;  // 0.5 = genre typical
	float colorK = 0.5f;
	float swingK = 0.f;     // -1..1 offset
	int lengthBars = 4;
	float density[NUM_TRACKS] = {0.5f, 0.5f, 0.5f, 0.5f};
	float fill = 0.3f, ghost = 0.3f, move = 0.5f, range = 0.5f, voicing = 0.5f;
	int oct[3] = {0, 0, 0}; // bass, melody, pads
	bool extChord = false;
	int extRoot = 0;
	uint16_t extMask = 0;
	int chordMode = CM_MIXED;
	int templateSel = -1;   // -1 random
	bool fillActive = false; // this pass ends with a fill / drop (decided per loop pass)
};

// ---------------------------------------------------------------- chords
struct Chord {
	int root = 0; // semitones above key
	int n = 0;
	int iv[6] = {0, 0, 0, 0, 0, 0}; // ascending, iv[0] = 0
};

bool parseChordToken(const char* tok, ChordSpec& cs);
Chord resolveSpec(const ChordSpec& cs, const Params& P, float color);
Chord resolveEvent(const Pattern& pat, const Params& P, int e);
int eventAt(const Pattern& pat, int step);
bool isEventStart(const Pattern& pat, int step);
Chord chordAt(const Pattern& pat, const Params& P, int step);
// scale: SCALES index for # / b spelling (-1 = always flats)
std::string chordName(const Chord& ch, int key, int scale = -1);
bool sharpKey(int key, int scale);
const char* noteName(int pc, int key, int scale);
Chord chordFromPcs(uint16_t pcMask, int lowestPc, int bassPc = -1);
void layoutTimeline(Pattern& pat, int lengthSteps);

// alternatives for display editing
int numAlternatives();
void applyAlternative(ChordSpec& cs, int idx, const Params& P);
std::string alternativeName(int idx, const Params& P, float color);
int currentAlternative(const ChordSpec& cs, const Params& P);

// ---------------------------------------------------------------- generation
void initPattern(Pattern& pat);
void generate(Pattern& pat, int genre, Rng& rng, const bool lock[NUM_TRACKS], bool chordLock, const Params& P);
void generateChords(Pattern& pat, int genre, Rng& rng, const Params& P);
void generateTrack(Pattern& pat, int track, int genre, Rng& rng, const Params& P);
void mutate(Pattern& pat, float amount, Rng& rng, const bool lock[NUM_TRACKS], bool chordLock, const Params& P);

// ---------------------------------------------------------------- evaluation
float effComplex(const Params& P, int genre);
float effColor(const Params& P, int genre);
float effSwing(const Params& P, int genre);

struct Hit {
	bool on = false;
	float vel = 0.f;
	int ratchet = 1;
	float delay = 0.f; // humanize, seconds
};
Hit drumHit(const Pattern& pat, const Params& P, int voice, int step, int L, bool fill);
void drumStep(const Pattern& pat, const Params& P, int step, int L, bool fill, Hit out[NUM_DRUMS]);

struct NoteEv {
	bool on = false;
	int pitch = 60; // midi
	float vel = 0.8f;
	int len = 1;    // steps
	float gate = 0.9f; // fraction of len
	bool acc = false;
	bool slide = false; // glides into the next note (gate held)
};
NoteEv bassAt(const Pattern& pat, const Params& P, int step, int L);
NoteEv melAt(const Pattern& pat, const Params& P, int step, int L);

struct PadEv {
	bool on = false;
	float vel = 0.7f;
	int len = 1;
	float gate = 0.95f;
	int notes[4] = {60, 64, 67, 72};
};
// prev: previous voicing (voice leading), updated in place
PadEv padAt(const Pattern& pat, const Params& P, int step, int L, int prev[4], bool& hasPrev);

// ---------------------------------------------------------------- grid editing (Edit.cpp)
// a note as it sounds (midi pitch incl. key, transpose and octave)
struct GridNote {
	int start = 0, len = 1, pitch = 60, vel = 100;
	bool slide = false, acc = false;
};
// fixed note (relative to key) after chord follow
int followAbs(const Pattern& pat, const Params& P, int track, int step, int absNote, int froot);
// turn a generated track into fixed notes that sound the same (loop of L steps, repeated up to MAX_STEPS)
void freezeTrack(Pattern& pat, const Params& P, int track, int L);
// notes of bass / melody / pads as they sound in the first L steps
std::vector<GridNote> trackNotes(const Pattern& pat, const Params& P, int track, int L);
// write notes into a fixed track (steps 0..L-1 are replaced, the rest is kept)
void writeTrackNotes(Pattern& pat, const Params& P, int track, const std::vector<GridNote>& notes, int L);
// drums (fixed track): vel 0 = no hit
void setDrum(Pattern& pat, int voice, int step, int vel, int ratchet);
void setChordFollow(Pattern& pat, const Params& P, int track, bool on, int L);
// snap: 0 off, 1 scale, 2 chord tones
int snapPitch(const Pattern& pat, const Params& P, int step, int pitch, int mode);
void copyTrack(const Pattern& from, Pattern& to, int track);
void clearTrack(Pattern& pat, int track);
struct BarClip {
	int track = -1;
	DrumStep drums[NUM_DRUMS][16];
	NoteStep notes[16];
	PadStep pads[16];
};
void copyBar(const Pattern& pat, int track, int bar, BarClip& clip);
bool pasteBar(Pattern& pat, int track, int bar, const BarClip& clip);

// ---------------------------------------------------------------- MIDI
struct MidiNote {
	int track; // 0 drums, 1 bass, 2 melody, 3 pads
	int start; // steps
	int len;   // ticks (24 per step)
	int startTick;
	int note;
	int vel;
};
std::vector<MidiNote> renderLoop(const Pattern& pat, const Params& P, int L);
// loopSteps: track end is written at the loop end; pat: chord progression stored as GENRA metadata
std::vector<uint8_t> writeSmf(const std::vector<MidiNote>& notes, float bpm, const char* genreName, int loopSteps = 0,
                              const Pattern* pat = nullptr);
// returns number of bars imported (0 on failure); err receives a message
int importSmf(const std::vector<uint8_t>& data, Pattern& pat, const Params& P, std::string& err);

} // namespace genra
