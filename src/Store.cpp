// GENRA – pattern storage.
// Version-independent format: tagged records, every step stored field by field with a field count.
// New fields or records can be added later without breaking older patches:
//   - unknown records are skipped,
//   - missing records / fields keep the defaults of initPattern(),
//   - extra fields per step (written by a newer GENRA) are skipped.
// Also loads the raw binary block written by GENRA v2 (2.1.x).
#include "Engine.hpp"
#include <algorithm>

namespace genra {

static const uint8_t STORE_MAGIC[4] = {'G', 'N', 'P', 'T'};
static const uint16_t STORE_VERSION = 1;

enum StoreTag : uint8_t {
	ST_HEADER = 1,
	ST_CHOP = 2,
	ST_PROG = 3,
	ST_EVENTS = 4,
	ST_DRUMS = 5,
	ST_BASS = 6,
	ST_MEL = 7,
	ST_PADS = 8,
};

// ---------------------------------------------------------------- writer
namespace {
struct Writer {
	std::vector<uint8_t> d;
	size_t recStart = 0;
	void u8(uint8_t v) { d.push_back(v); }
	void i8(int8_t v) { d.push_back((uint8_t)v); }
	void u16(uint16_t v) {
		d.push_back(v & 0xff);
		d.push_back(v >> 8);
	}
	void begin(uint8_t tag) {
		u8(tag);
		recStart = d.size();
		for (int i = 0; i < 4; i++)
			u8(0); // length, patched in end()
	}
	void end() {
		uint32_t len = (uint32_t)(d.size() - recStart - 4);
		for (int i = 0; i < 4; i++)
			d[recStart + i] = (uint8_t)(len >> (8 * i));
	}
};

struct Reader {
	const uint8_t* d;
	size_t n, p = 0;
	Reader(const uint8_t* d_, size_t n_) : d(d_), n(n_) {}
	bool ok(size_t k) const { return p + k <= n; }
	uint8_t u8() { return p < n ? d[p++] : 0; }
	int8_t i8() { return (int8_t)u8(); }
	uint16_t u16() {
		uint16_t lo = u8();
		return (uint16_t)(lo | (u8() << 8));
	}
	void skip(size_t k) { p = std::min(n, p + k); }
};
} // namespace

// field counts per step; v3.0 wrote 6 / 15 / 10, v3.1 7 / 18 / 12 – older data simply lacks the newer fields
static const int DRUM_FIELDS = 9, NOTE_FIELDS = 20, PAD_FIELDS = 14, SPEC_FIELDS = 9, EV_FIELDS = 3;

static void putNote(Writer& w, const NoteStep& s) {
	uint8_t f[NOTE_FIELDS] = {s.r, s.rm, s.vj, s.ra, s.rs, (uint8_t)s.a1, (uint8_t)s.b1, (uint8_t)s.c1,
	                          (uint8_t)s.a2, (uint8_t)s.b2, (uint8_t)s.c2, (uint8_t)s.absNote, s.force, s.flen, s.fvel,
	                          s.fflags, s.fgate, (uint8_t)s.froot, s.fchance, s.fcond};
	for (uint8_t b : f)
		w.u8(b);
}

std::vector<uint8_t> savePattern(const Pattern& pat) {
	Writer w;
	for (uint8_t b : STORE_MAGIC)
		w.u8(b);
	w.u16(STORE_VERSION);

	w.begin(ST_HEADER);
	w.u8(NUM_TRACKS);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.i8(pat.trackGenre[t]);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.u8(pat.fixed[t]);
	w.i8(pat.chordGenre);
	w.u8(pat.melMode);
	w.i8(pat.melCenter);
	w.u8(pat.customTimeline);
	w.u8(pat.harm);
	w.u8(pat.layoutLen);
	for (int t = 0; t < NUM_TRACKS; t++)
		w.u8(pat.chordFollow[t]);
	w.end();

	w.begin(ST_CHOP);
	w.u8(MAX_BARS);
	w.u8(16);
	for (int b = 0; b < MAX_BARS; b++) {
		w.u8(pat.chopR[b]);
		for (int s = 0; s < 16; s++)
			w.u8(pat.chop[b][s]);
	}
	w.end();

	w.begin(ST_PROG);
	w.u8(pat.nProg);
	w.u8(SPEC_FIELDS);
	for (int i = 0; i < pat.nProg && i < MAX_PROG; i++) {
		const ChordSpec& c = pat.prog[i];
		uint8_t f[SPEC_FIELDS] = {(uint8_t)c.root, c.quality, c.ext, c.rc, c.rs, (uint8_t)c.extOff, c.explicitQ,
		                          (uint8_t)(c.mask & 0xff), (uint8_t)(c.mask >> 8)};
		for (uint8_t b : f)
			w.u8(b);
	}
	w.end();

	w.begin(ST_EVENTS);
	w.u8(pat.nEvents);
	w.u8(EV_FIELDS);
	for (int i = 0; i < pat.nEvents && i < MAX_EVENTS; i++) {
		w.u8(pat.ev[i].spec);
		w.u8(pat.ev[i].start);
		w.u8(pat.ev[i].len);
	}
	w.end();

	w.begin(ST_DRUMS);
	w.u8(NUM_DRUMS);
	w.u16(MAX_STEPS);
	w.u8(DRUM_FIELDS);
	for (int v = 0; v < NUM_DRUMS; v++)
		for (int s = 0; s < MAX_STEPS; s++) {
			const DrumStep& d = pat.drums[v][s];
			uint8_t f[DRUM_FIELDS] = {d.r, d.rg, d.rf, d.vj, d.rr, d.fv, d.frat, d.fchance, d.fcond};
			for (uint8_t b : f)
				w.u8(b);
		}
	w.end();

	for (int tr = 0; tr < 2; tr++) {
		const NoteStep* arr = tr == 0 ? pat.bass : pat.mel;
		w.begin(tr == 0 ? ST_BASS : ST_MEL);
		w.u16(MAX_STEPS);
		w.u8(NOTE_FIELDS);
		for (int s = 0; s < MAX_STEPS; s++)
			putNote(w, arr[s]);
		w.end();
	}

	w.begin(ST_PADS);
	w.u16(MAX_STEPS);
	w.u8(PAD_FIELDS);
	for (int s = 0; s < MAX_STEPS; s++) {
		const PadStep& p = pat.pads[s];
		uint8_t f[PAD_FIELDS] = {p.r, p.vj, p.force, p.flen, p.fvel, p.nNotes,
		                         (uint8_t)p.notes[0], (uint8_t)p.notes[1], (uint8_t)p.notes[2], (uint8_t)p.notes[3],
		                         p.fgate, (uint8_t)p.froot, p.fchance, p.fcond};
		for (uint8_t b : f)
			w.u8(b);
	}
	w.end();
	return w.d;
}

// ---------------------------------------------------------------- reader
// reads up to `have` fields into f (defaults kept for missing ones), skips extra fields
static void readFields(Reader& r, uint8_t* f, int known, int have) {
	for (int i = 0; i < have; i++) {
		uint8_t b = r.u8();
		if (i < known)
			f[i] = b;
	}
}

static void getNote(Reader& r, NoteStep& s, int have) {
	uint8_t f[NOTE_FIELDS] = {s.r, s.rm, s.vj, s.ra, s.rs, (uint8_t)s.a1, (uint8_t)s.b1, (uint8_t)s.c1,
	                          (uint8_t)s.a2, (uint8_t)s.b2, (uint8_t)s.c2, (uint8_t)s.absNote, s.force, s.flen, s.fvel,
	                          s.fflags, s.fgate, (uint8_t)s.froot, s.fchance, s.fcond};
	readFields(r, f, NOTE_FIELDS, have);
	s.r = f[0], s.rm = f[1], s.vj = f[2], s.ra = f[3], s.rs = f[4];
	s.a1 = (int8_t)f[5], s.b1 = (int8_t)f[6], s.c1 = (int8_t)f[7];
	s.a2 = (int8_t)f[8], s.b2 = (int8_t)f[9], s.c2 = (int8_t)f[10];
	s.absNote = (int8_t)f[11], s.force = f[12], s.flen = f[13], s.fvel = f[14];
	s.fflags = f[15], s.fgate = f[16], s.froot = (int8_t)f[17];
	s.fchance = std::min<uint8_t>(f[18], 99), s.fcond = std::min<uint8_t>(f[19], 1);
	if (s.froot < -1 || s.froot > 11)
		s.froot = -1;
}

static bool loadV2Raw(const std::vector<uint8_t>& data, Pattern& pat);

bool loadPattern(const std::vector<uint8_t>& data, Pattern& out) {
	if (data.size() < 6 || std::memcmp(data.data(), STORE_MAGIC, 4) != 0)
		return loadV2Raw(data, out);
	Pattern pat;
	initPattern(pat);
	Reader r(data.data(), data.size());
	r.skip(4);
	uint16_t ver = r.u16();
	if (ver > STORE_VERSION + 50) // far future major change: refuse instead of misreading
		return false;
	bool gotProg = false, gotEvents = false;
	unsigned seen = 0; // bit per record tag
	while (r.ok(5)) {
		uint8_t tag = r.u8();
		uint32_t len = r.u8() | (r.u8() << 8) | (r.u8() << 16) | ((uint32_t)r.u8() << 24);
		if (!r.ok(len))
			return false;
		Reader q(data.data() + r.p, len);
		r.skip(len);
		if (tag < 32)
			seen |= 1u << tag;
		switch (tag) {
			case ST_HEADER: {
				int nt = q.u8();
				for (int t = 0; t < nt; t++) {
					int8_t g = q.i8();
					if (t < NUM_TRACKS)
						pat.trackGenre[t] = (int8_t)std::max<int>(0, std::min<int>(g, numProfiles() - 1));
				}
				for (int t = 0; t < nt; t++) {
					uint8_t f = q.u8();
					if (t < NUM_TRACKS)
						pat.fixed[t] = f ? 1 : 0;
				}
				pat.chordGenre = (int8_t)std::max<int>(0, std::min<int>(q.i8(), numProfiles() - 1));
				pat.melMode = q.u8();
				pat.melCenter = q.i8();
				pat.customTimeline = q.u8();
				pat.harm = q.u8();
				pat.layoutLen = q.u8();
				for (int t = 0; t < nt; t++) {
					uint8_t f = q.u8(); // absent in v3.0 data: 0
					if (t < NUM_TRACKS)
						pat.chordFollow[t] = f ? 1 : 0;
				}
				break;
			}
			case ST_CHOP: {
				int nb = q.u8(), ns = q.u8();
				for (int b = 0; b < nb; b++) {
					uint8_t cr = q.u8();
					if (b < MAX_BARS)
						pat.chopR[b] = cr;
					for (int s = 0; s < ns; s++) {
						uint8_t c = q.u8();
						if (b < MAX_BARS && s < 16)
							pat.chop[b][s] = (uint8_t)(c % 16);
					}
				}
				break;
			}
			case ST_PROG: {
				int n = q.u8(), have = q.u8();
				if (n < 1)
					break;
				for (int i = 0; i < n; i++) {
					uint8_t f[SPEC_FIELDS] = {0, Q_MIN, 0, 0, 0, 0, 0, 0, 0};
					readFields(q, f, SPEC_FIELDS, have);
					if (i >= MAX_PROG)
						continue;
					ChordSpec& c = pat.prog[i];
					c.root = (int8_t)(f[0] % 12);
					c.quality = std::min<uint8_t>(f[1], Q_MASK);
					c.ext = f[2], c.rc = f[3], c.rs = f[4];
					c.extOff = (int8_t)std::max(-3, std::min(3, (int)(int8_t)f[5]));
					c.explicitQ = f[6];
					c.mask = (uint16_t)(f[7] | (f[8] << 8));
				}
				pat.nProg = (uint8_t)std::min(n, (int)MAX_PROG);
				gotProg = true;
				break;
			}
			case ST_EVENTS: {
				int n = q.u8(), have = q.u8();
				for (int i = 0; i < n; i++) {
					uint8_t f[EV_FIELDS] = {0, 0, 16};
					readFields(q, f, EV_FIELDS, have);
					if (i >= MAX_EVENTS)
						continue;
					pat.ev[i].spec = f[0], pat.ev[i].start = f[1], pat.ev[i].len = f[2], pat.ev[i].pad = 0;
				}
				pat.nEvents = (uint8_t)std::min(n, (int)MAX_EVENTS);
				gotEvents = n > 0;
				break;
			}
			case ST_DRUMS: {
				int nv = q.u8(), ns = q.u16(), have = q.u8();
				for (int v = 0; v < nv; v++)
					for (int s = 0; s < ns; s++) {
						bool in = v < NUM_DRUMS && s < MAX_STEPS;
						DrumStep tmp = {};
						DrumStep& d = in ? pat.drums[v][s] : tmp;
						uint8_t f[DRUM_FIELDS] = {d.r, d.rg, d.rf, d.vj, d.rr, d.fv, d.frat, d.fchance, d.fcond};
						readFields(q, f, DRUM_FIELDS, have);
						d.r = f[0], d.rg = f[1], d.rf = f[2], d.vj = f[3], d.rr = f[4], d.fv = f[5];
						d.frat = std::min<uint8_t>(f[6], 4);
						d.fchance = std::min<uint8_t>(f[7], 99);
						d.fcond = std::min<uint8_t>(f[8], 1);
					}
				break;
			}
			case ST_BASS:
			case ST_MEL: {
				NoteStep* arr = tag == ST_BASS ? pat.bass : pat.mel;
				int ns = q.u16(), have = q.u8();
				for (int s = 0; s < ns; s++) {
					NoteStep tmp = arr[0];
					getNote(q, s < MAX_STEPS ? arr[s] : tmp, have);
				}
				break;
			}
			case ST_PADS: {
				int ns = q.u16(), have = q.u8();
				for (int s = 0; s < ns; s++) {
					PadStep tmp = {};
					PadStep& p = s < MAX_STEPS ? pat.pads[s] : tmp;
					uint8_t f[PAD_FIELDS] = {p.r, p.vj, p.force, p.flen, p.fvel, p.nNotes,
					                         (uint8_t)p.notes[0], (uint8_t)p.notes[1], (uint8_t)p.notes[2], (uint8_t)p.notes[3],
					                         p.fgate, (uint8_t)p.froot, p.fchance, p.fcond};
					readFields(q, f, PAD_FIELDS, have);
					p.r = f[0], p.vj = f[1], p.force = f[2], p.flen = f[3], p.fvel = f[4];
					p.nNotes = std::min<uint8_t>(f[5], 4);
					for (int k = 0; k < 4; k++)
						p.notes[k] = (int8_t)f[6 + k];
					p.fgate = f[10];
					p.froot = (int8_t)f[11];
					p.fchance = std::min<uint8_t>(f[12], 99);
					p.fcond = std::min<uint8_t>(f[13], 1);
					if (p.froot < -1 || p.froot > 11)
						p.froot = -1;
				}
				break;
			}
			default:
				break; // record from a newer GENRA: ignore
		}
	}
	// incomplete data (truncated patch): keep the current pattern instead of loading half of it
	const unsigned need = (1u << ST_HEADER) | (1u << ST_DRUMS) | (1u << ST_BASS) | (1u << ST_MEL) | (1u << ST_PADS);
	if ((seen & need) != need || r.p != r.n)
		return false;
	// keep the chord timeline consistent
	for (int i = 0; i < pat.nEvents; i++)
		if (pat.ev[i].spec >= pat.nProg)
			pat.ev[i].spec = 0;
	if (!gotProg || !gotEvents) {
		pat.customTimeline = 0;
		pat.layoutLen = 0;
	}
	if (pat.layoutLen == 0 || pat.nEvents == 0)
		layoutTimeline(pat, pat.layoutLen ? pat.layoutLen : 64);
	out = pat;
	return true;
}

// ---------------------------------------------------------------- GENRA v2 (2.1.x) raw block
// Frozen copy of the v2 memory layout. Do not change: it describes patches saved by v2.
namespace v2 {
struct DrumStep {
	uint8_t r, rg, rf, vj, rr, fv;
};
struct NoteStep {
	uint8_t r, rm, vj, ra, rs;
	int8_t a1, b1, c1, a2, b2, c2, absNote;
	uint8_t force, flen, fvel;
};
struct PadStep {
	uint8_t r, vj, force, flen, fvel, nNotes;
	int8_t notes[4];
};
struct ChordSpec {
	int8_t root;
	uint8_t quality, ext, rc, rs;
	int8_t extOff;
	uint8_t explicitQ;
	uint16_t mask;
};
struct ChordEvent {
	uint8_t spec, start, len, pad;
};
struct Pattern {
	uint32_t magic, version;
	int8_t trackGenre[4];
	int8_t chordGenre;
	uint8_t melMode;
	int8_t melCenter;
	uint8_t customTimeline, harm, nProg, nEvents, layoutLen;
	uint8_t fixed[4];
	uint8_t reserved[8];
	uint8_t chop[8][16];
	uint8_t chopR[8];
	ChordSpec prog[8];
	ChordEvent ev[32];
	DrumStep drums[8][128];
	NoteStep bass[128];
	NoteStep mel[128];
	PadStep pads[128];
};
} // namespace v2

// for tests: the raw block GENRA v2 would have saved for this pattern
std::vector<uint8_t> v2RawForTest(const Pattern& pat) {
	v2::Pattern o;
	std::memset(&o, 0, sizeof(o));
	o.magic = PATTERN_MAGIC;
	o.version = 2;
	for (int t = 0; t < 4; t++) {
		o.trackGenre[t] = pat.trackGenre[t];
		o.fixed[t] = pat.fixed[t];
	}
	o.chordGenre = pat.chordGenre, o.melMode = pat.melMode, o.melCenter = pat.melCenter;
	o.customTimeline = pat.customTimeline, o.harm = pat.harm, o.nProg = pat.nProg, o.nEvents = pat.nEvents, o.layoutLen = pat.layoutLen;
	std::memcpy(o.chop, pat.chop, sizeof(o.chop));
	std::memcpy(o.chopR, pat.chopR, sizeof(o.chopR));
	for (int i = 0; i < 8; i++) {
		const ChordSpec& c = pat.prog[i];
		o.prog[i] = {c.root, c.quality, c.ext, c.rc, c.rs, c.extOff, c.explicitQ, c.mask};
	}
	for (int i = 0; i < 32; i++)
		o.ev[i] = {pat.ev[i].spec, pat.ev[i].start, pat.ev[i].len, 0};
	for (int v = 0; v < 8; v++)
		for (int s = 0; s < 128; s++) {
			const DrumStep& d = pat.drums[v][s];
			o.drums[v][s] = {d.r, d.rg, d.rf, d.vj, d.rr, d.fv};
		}
	for (int tr = 0; tr < 2; tr++)
		for (int s = 0; s < 128; s++) {
			const NoteStep& n = tr == 0 ? pat.bass[s] : pat.mel[s];
			v2::NoteStep& a = tr == 0 ? o.bass[s] : o.mel[s];
			a = {n.r, n.rm, n.vj, n.ra, n.rs, n.a1, n.b1, n.c1, n.a2, n.b2, n.c2, n.absNote, n.force, n.flen, n.fvel};
		}
	for (int s = 0; s < 128; s++) {
		const PadStep& p = pat.pads[s];
		v2::PadStep& a = o.pads[s];
		a.r = p.r, a.vj = p.vj, a.force = p.force, a.flen = p.flen, a.fvel = p.fvel, a.nNotes = p.nNotes;
		std::memcpy(a.notes, p.notes, 4);
	}
	return std::vector<uint8_t>((const uint8_t*)&o, (const uint8_t*)&o + sizeof(o));
}

static bool loadV2Raw(const std::vector<uint8_t>& data, Pattern& out) {
	if (data.size() != sizeof(v2::Pattern))
		return false;
	v2::Pattern o;
	std::memcpy(&o, data.data(), sizeof(o));
	if (o.magic != PATTERN_MAGIC || o.version != 2)
		return false;
	Pattern pat;
	initPattern(pat);
	for (int t = 0; t < 4; t++) {
		pat.trackGenre[t] = (int8_t)std::max<int>(0, std::min<int>(o.trackGenre[t], numProfiles() - 1));
		pat.fixed[t] = o.fixed[t];
	}
	pat.chordGenre = (int8_t)std::max<int>(0, std::min<int>(o.chordGenre, numProfiles() - 1));
	pat.melMode = o.melMode;
	pat.melCenter = o.melCenter;
	pat.customTimeline = o.customTimeline;
	pat.harm = o.harm;
	pat.nProg = (uint8_t)std::max(1, std::min<int>(o.nProg, MAX_PROG));
	pat.nEvents = (uint8_t)std::min<int>(o.nEvents, MAX_EVENTS);
	pat.layoutLen = o.layoutLen;
	std::memcpy(pat.chop, o.chop, sizeof(o.chop));
	std::memcpy(pat.chopR, o.chopR, sizeof(o.chopR));
	for (int i = 0; i < 8; i++) {
		const v2::ChordSpec& a = o.prog[i];
		ChordSpec& c = pat.prog[i];
		c.root = a.root, c.quality = a.quality, c.ext = a.ext, c.rc = a.rc, c.rs = a.rs;
		c.extOff = a.extOff, c.explicitQ = a.explicitQ, c.mask = a.mask;
	}
	for (int i = 0; i < 32; i++) {
		pat.ev[i].spec = o.ev[i].spec, pat.ev[i].start = o.ev[i].start, pat.ev[i].len = o.ev[i].len, pat.ev[i].pad = 0;
	}
	for (int v = 0; v < 8; v++)
		for (int s = 0; s < 128; s++) {
			const v2::DrumStep& a = o.drums[v][s];
			DrumStep& d = pat.drums[v][s];
			d.r = a.r, d.rg = a.rg, d.rf = a.rf, d.vj = a.vj, d.rr = a.rr, d.fv = a.fv;
		}
	for (int tr = 0; tr < 2; tr++)
		for (int s = 0; s < 128; s++) {
			const v2::NoteStep& a = tr == 0 ? o.bass[s] : o.mel[s];
			NoteStep& n = tr == 0 ? pat.bass[s] : pat.mel[s];
			n.r = a.r, n.rm = a.rm, n.vj = a.vj, n.ra = a.ra, n.rs = a.rs;
			n.a1 = a.a1, n.b1 = a.b1, n.c1 = a.c1, n.a2 = a.a2, n.b2 = a.b2, n.c2 = a.c2;
			n.absNote = a.absNote, n.force = a.force, n.flen = a.flen, n.fvel = a.fvel;
		}
	for (int s = 0; s < 128; s++) {
		const v2::PadStep& a = o.pads[s];
		PadStep& p = pat.pads[s];
		p.r = a.r, p.vj = a.vj, p.force = a.force, p.flen = a.flen, p.fvel = a.fvel, p.nNotes = a.nNotes;
		std::memcpy(p.notes, a.notes, 4);
	}
	out = pat;
	return true;
}

} // namespace genra
