// GENRA – MIDI export / import (Standard MIDI File, type 1)
#include "Engine.hpp"
#include <algorithm>
#include <cmath>

namespace genra {

static const int TPS = 24;       // ticks per 16th step
static const int PPQ = TPS * 4;  // 96 ticks per quarter
static const int GM_DRUM[NUM_DRUMS] = {36, 38, 39, 37, 42, 46, 45, 70};

static inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static inline int mod12(int x) { return ((x % 12) + 12) % 12; }

std::vector<MidiNote> renderLoop(const Pattern& pat, const Params& P, int L) {
	std::vector<MidiNote> out;
	L = clampi(L, 1, MAX_STEPS);
	float swing = effSwing(P, pat.trackGenre[T_DRUMS]);
	int swingTicks = (int)std::lround((swing - 0.5f) * 2.f * TPS);
	int prev[4] = {0, 0, 0, 0};
	bool hasPrev = false;
	auto add = [&](int track, int step, int tick, int lenTicks, int note, float vel) {
		MidiNote m;
		m.track = track;
		m.start = step;
		m.startTick = tick;
		m.len = std::max(3, lenTicks);
		m.note = clampi(note, 0, 127);
		m.vel = clampi((int)std::lround(vel * 127.f), 1, 127);
		out.push_back(m);
	};
	for (int s = 0; s < L; s++) {
		int t0 = s * TPS + ((s % 2 == 1) ? swingTicks : 0);
		Hit hits[NUM_DRUMS];
		drumStep(pat, P, s, L, false, hits);
		for (int v = 0; v < NUM_DRUMS; v++) {
			if (!hits[v].on)
				continue;
			int n = std::max(1, hits[v].ratchet);
			for (int i = 0; i < n; i++)
				add(0, s, t0 + i * TPS / n, TPS / n / 2, GM_DRUM[v], hits[v].vel);
		}
		NoteEv b = bassAt(pat, P, s, L);
		if (b.on)
			add(1, s, t0, (int)(b.len * TPS * b.gate), b.pitch, b.vel);
		NoteEv m = melAt(pat, P, s, L);
		if (m.on)
			add(2, s, t0, (int)(m.len * TPS * m.gate), m.pitch, m.vel);
		PadEv p = padAt(pat, P, s, L, prev, hasPrev);
		if (p.on) {
			for (int k = 0; k < 4; k++) {
				bool dup = false;
				for (int j = 0; j < k; j++)
					if (p.notes[j] == p.notes[k])
						dup = true;
				if (!dup)
					add(3, s, t0, (int)(p.len * TPS * p.gate), p.notes[k], p.vel);
			}
		}
	}
	return out;
}

// ----------------------------------------------------------------- writer
static void putVlq(std::vector<uint8_t>& v, uint32_t x) {
	uint8_t buf[5];
	int n = 0;
	buf[n++] = x & 0x7f;
	while ((x >>= 7))
		buf[n++] = 0x80 | (x & 0x7f);
	while (n--)
		v.push_back(buf[n]);
}
static void put32(std::vector<uint8_t>& v, uint32_t x) {
	for (int i = 3; i >= 0; i--)
		v.push_back((x >> (8 * i)) & 0xff);
}
static void put16(std::vector<uint8_t>& v, uint16_t x) {
	v.push_back(x >> 8);
	v.push_back(x & 0xff);
}
static void putText(std::vector<uint8_t>& v, uint8_t type, const std::string& s) {
	putVlq(v, 0);
	v.push_back(0xff);
	v.push_back(type);
	putVlq(v, (uint32_t)s.size());
	v.insert(v.end(), s.begin(), s.end());
}
static void putChunk(std::vector<uint8_t>& file, const char* id, const std::vector<uint8_t>& data) {
	file.insert(file.end(), id, id + 4);
	put32(file, (uint32_t)data.size());
	file.insert(file.end(), data.begin(), data.end());
}

std::vector<uint8_t> writeSmf(const std::vector<MidiNote>& notes, float bpm, const char* genreName) {
	std::vector<uint8_t> file;
	std::vector<uint8_t> hdr;
	put16(hdr, 1);
	put16(hdr, 5);
	put16(hdr, PPQ);
	putChunk(file, "MThd", hdr);

	// tempo track
	{
		std::vector<uint8_t> t;
		putText(t, 0x03, std::string("GENRA ") + (genreName ? genreName : ""));
		uint32_t us = (uint32_t)std::lround(60000000.0 / std::max(20.f, bpm));
		putVlq(t, 0);
		t.push_back(0xff);
		t.push_back(0x51);
		t.push_back(3);
		t.push_back((us >> 16) & 0xff);
		t.push_back((us >> 8) & 0xff);
		t.push_back(us & 0xff);
		putVlq(t, 0);
		t.push_back(0xff);
		t.push_back(0x58);
		t.push_back(4);
		t.push_back(4);
		t.push_back(2);
		t.push_back(24);
		t.push_back(8);
		putVlq(t, 0);
		t.push_back(0xff);
		t.push_back(0x2f);
		t.push_back(0);
		putChunk(file, "MTrk", t);
	}
	static const char* NAMES[4] = {"Drums", "Bass", "Melody", "Pads"};
	static const int CH[4] = {9, 0, 1, 2};
	static const int PROG[4] = {-1, 38, 81, 89};
	for (int tr = 0; tr < 4; tr++) {
		struct Ev {
			int tick, type, note, vel;
		};
		std::vector<Ev> evs;
		for (const MidiNote& m : notes) {
			if (m.track != tr)
				continue;
			evs.push_back({m.startTick, 1, m.note, m.vel});
			evs.push_back({m.startTick + m.len, 0, m.note, 0});
		}
		std::stable_sort(evs.begin(), evs.end(), [](const Ev& a, const Ev& b) {
			if (a.tick != b.tick)
				return a.tick < b.tick;
			return a.type < b.type; // note-offs first
		});
		std::vector<uint8_t> t;
		putText(t, 0x03, NAMES[tr]);
		if (PROG[tr] >= 0) {
			putVlq(t, 0);
			t.push_back((uint8_t)(0xC0 | CH[tr]));
			t.push_back((uint8_t)PROG[tr]);
		}
		int last = 0;
		for (const Ev& e : evs) {
			putVlq(t, (uint32_t)std::max(0, e.tick - last));
			last = e.tick;
			t.push_back((uint8_t)((e.type ? 0x90 : 0x80) | CH[tr]));
			t.push_back((uint8_t)e.note);
			t.push_back((uint8_t)(e.type ? e.vel : 64));
		}
		putVlq(t, 0);
		t.push_back(0xff);
		t.push_back(0x2f);
		t.push_back(0);
		putChunk(file, "MTrk", t);
	}
	return file;
}

// ----------------------------------------------------------------- reader
struct RawNote {
	int group, ch, note, start, end, vel;
};
struct GroupInfo {
	int ch = 0;
	std::string name;
	int count = 0;
	double pitchSum = 0;
	int maxPoly = 0;
};

static bool readVlq(const std::vector<uint8_t>& d, size_t& p, size_t end, uint32_t& out) {
	out = 0;
	for (int i = 0; i < 4; i++) {
		if (p >= end)
			return false;
		uint8_t b = d[p++];
		out = (out << 7) | (b & 0x7f);
		if (!(b & 0x80))
			return true;
	}
	return false;
}
static uint32_t rd32(const std::vector<uint8_t>& d, size_t p) {
	return ((uint32_t)d[p] << 24) | ((uint32_t)d[p + 1] << 16) | ((uint32_t)d[p + 2] << 8) | d[p + 3];
}
static std::string lower(std::string s) {
	for (char& c : s)
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	return s;
}

static int drumVoice(int note) {
	switch (note) {
		case 35: case 36: return KICK;
		case 38: case 40: return SNARE;
		case 39: return CLAP;
		case 37: return RIM;
		case 42: case 44: return CHAT;
		case 46: return OHAT;
		case 41: case 43: case 45: case 47: case 48: case 50: return TOM;
		default: return PERC;
	}
}

int importSmf(const std::vector<uint8_t>& d, Pattern& pat, const Params& P, std::string& err) {
	if (d.size() < 14 || std::memcmp(d.data(), "MThd", 4) != 0) {
		err = "Keine gültige MIDI-Datei.";
		return 0;
	}
	uint32_t hl = rd32(d, 4);
	int ntrks = (d[10] << 8) | d[11];
	int division = (d[12] << 8) | d[13];
	if (division & 0x8000 || division == 0) {
		err = "SMPTE-Zeitbasis wird nicht unterstützt.";
		return 0;
	}
	size_t p = 8 + hl;
	std::vector<RawNote> notes;
	std::vector<GroupInfo> groups;
	std::vector<int> groupKey; // trk*16 + ch
	auto groupOf = [&](int trk, int ch, const std::string& name) {
		int key = trk * 16 + ch;
		for (size_t i = 0; i < groupKey.size(); i++)
			if (groupKey[i] == key)
				return (int)i;
		groupKey.push_back(key);
		GroupInfo gi;
		gi.ch = ch;
		gi.name = name;
		groups.push_back(gi);
		return (int)groups.size() - 1;
	};
	for (int trk = 0; trk < ntrks && p + 8 <= d.size(); trk++) {
		if (std::memcmp(&d[p], "MTrk", 4) != 0) {
			p += 8 + rd32(d, p + 4);
			trk--;
			continue;
		}
		size_t len = rd32(d, p + 4);
		size_t q = p + 8, end = std::min(d.size(), q + len);
		p = end;
		uint32_t tick = 0;
		uint8_t status = 0;
		std::string name;
		int onStart[16][128], onVel[16][128];
		for (int c = 0; c < 16; c++)
			for (int n = 0; n < 128; n++)
				onStart[c][n] = -1;
		while (q < end) {
			uint32_t delta;
			if (!readVlq(d, q, end, delta))
				break;
			tick += delta;
			if (q >= end)
				break;
			uint8_t b = d[q];
			if (b == 0xff) {
				if (q + 2 > end)
					break;
				uint8_t type = d[q + 1];
				q += 2;
				uint32_t ml;
				if (!readVlq(d, q, end, ml) || q + ml > end)
					break;
				if (type == 0x03)
					name = std::string((const char*)&d[q], ml);
				q += ml;
				continue;
			}
			if (b == 0xf0 || b == 0xf7) {
				q++;
				uint32_t sl;
				if (!readVlq(d, q, end, sl))
					break;
				q += sl;
				continue;
			}
			if (b & 0x80) {
				status = b;
				q++;
			}
			if (!(status & 0x80))
				break;
			int type = status & 0xf0, ch = status & 0x0f;
			int nData = (type == 0xC0 || type == 0xD0) ? 1 : 2;
			if (q + nData > end)
				break;
			int d1 = d[q], d2 = nData > 1 ? d[q + 1] : 0;
			q += nData;
			if (type == 0x90 && d2 > 0) {
				if (onStart[ch][d1 & 0x7f] >= 0) {
					int g = groupOf(trk, ch, name);
					notes.push_back({g, ch, d1, onStart[ch][d1], (int)tick, onVel[ch][d1]});
				}
				onStart[ch][d1 & 0x7f] = (int)tick;
				onVel[ch][d1 & 0x7f] = d2;
			}
			else if (type == 0x80 || (type == 0x90 && d2 == 0)) {
				int n = d1 & 0x7f;
				if (onStart[ch][n] >= 0) {
					int g = groupOf(trk, ch, name);
					notes.push_back({g, ch, n, onStart[ch][n], (int)tick, onVel[ch][n]});
					onStart[ch][n] = -1;
				}
			}
		}
		for (size_t i = 0; i < groups.size(); i++)
			if (groupKey[i] / 16 == trk && groups[i].name.empty())
				groups[i].name = name;
	}
	if (notes.empty()) {
		err = "Die MIDI-Datei enthält keine Noten.";
		return 0;
	}
	int tps = std::max(1, division / 4);
	// statistics per group
	for (RawNote& n : notes) {
		GroupInfo& g = groups[n.group];
		g.count++;
		g.pitchSum += n.note;
	}
	for (size_t gi = 0; gi < groups.size(); gi++) {
		std::vector<std::pair<int, int>> ev;
		for (RawNote& n : notes)
			if (n.group == (int)gi) {
				ev.push_back({n.start, 1});
				ev.push_back({std::max(n.start + 1, n.end), -1});
			}
		std::sort(ev.begin(), ev.end());
		int cur = 0;
		for (auto& e : ev) {
			cur += e.second;
			groups[gi].maxPoly = std::max(groups[gi].maxPoly, cur);
		}
	}
	// classify
	std::vector<int> role(groups.size());
	for (size_t gi = 0; gi < groups.size(); gi++) {
		GroupInfo& g = groups[gi];
		std::string nm = lower(g.name);
		double avg = g.count ? g.pitchSum / g.count : 60;
		if (g.ch == 9 || nm.find("drum") != std::string::npos)
			role[gi] = T_DRUMS;
		else if (nm.find("bass") != std::string::npos)
			role[gi] = T_BASS;
		else if (nm.find("pad") != std::string::npos || nm.find("chord") != std::string::npos || nm.find("keys") != std::string::npos)
			role[gi] = T_PADS;
		else if (nm.find("mel") != std::string::npos || nm.find("lead") != std::string::npos)
			role[gi] = T_MELODY;
		else if (g.maxPoly >= 3)
			role[gi] = T_PADS;
		else if (avg < 52)
			role[gi] = T_BASS;
		else
			role[gi] = T_MELODY;
	}
	int maxStep = 0;
	for (RawNote& n : notes)
		maxStep = std::max(maxStep, (int)std::lround((double)n.start / tps));
	int bars = (maxStep / 16) + 1;
	int barsQ = bars <= 1 ? 1 : (bars <= 2 ? 2 : (bars <= 4 ? 4 : 8));
	int L = barsQ * 16;
	bool present[NUM_TRACKS] = {false, false, false, false};
	for (size_t gi = 0; gi < groups.size(); gi++)
		present[role[gi]] = true;

	// --- drums
	if (present[T_DRUMS]) {
		pat.fixed[T_DRUMS] = 1;
		for (int v = 0; v < NUM_DRUMS; v++)
			for (int s = 0; s < MAX_STEPS; s++)
				pat.drums[v][s].fv = 0;
		for (RawNote& n : notes) {
			if (role[n.group] != T_DRUMS)
				continue;
			int s = (int)std::lround((double)n.start / tps);
			if (s < 0 || s >= L)
				continue;
			DrumStep& ds = pat.drums[drumVoice(n.note)][s];
			ds.fv = (uint8_t)std::max<int>(ds.fv, clampi(n.vel, 1, 127));
		}
	}
	// --- bass / melody
	for (int tr = T_BASS; tr <= T_MELODY; tr++) {
		if (!present[tr])
			continue;
		NoteStep* arr = tr == T_BASS ? pat.bass : pat.mel;
		pat.fixed[tr] = 1;
		for (int s = 0; s < MAX_STEPS; s++) {
			arr[s].force = 2;
			arr[s].absNote = NO_ABS;
			arr[s].flen = 0;
			arr[s].fvel = 0;
		}
		for (RawNote& n : notes) {
			if (role[n.group] != tr)
				continue;
			int s = (int)std::lround((double)n.start / tps);
			if (s < 0 || s >= L)
				continue;
			NoteStep& ns = arr[s];
			int rel = n.note - P.key;
			if (ns.force == 1 && ns.absNote != NO_ABS) {
				bool keepOld = tr == T_BASS ? (ns.absNote <= rel) : (ns.absNote >= rel);
				if (keepOld)
					continue;
			}
			ns.force = 1;
			ns.absNote = (int8_t)clampi(rel, -127, 127);
			ns.flen = (uint8_t)clampi((int)std::lround((double)(n.end - n.start) / tps), 1, 64);
			ns.fvel = (uint8_t)clampi(n.vel, 1, 127);
		}
	}
	// --- pads + chords
	if (present[T_PADS]) {
		pat.fixed[T_PADS] = 1;
		for (int s = 0; s < MAX_STEPS; s++) {
			pat.pads[s].force = 2;
			pat.pads[s].flen = 0;
			pat.pads[s].fvel = 0;
			pat.pads[s].nNotes = 0;
		}
		// bass note per step (root hint for chord detection)
		int bassAt[MAX_STEPS];
		for (int s = 0; s < MAX_STEPS; s++)
			bassAt[s] = -1;
		for (RawNote& n : notes) {
			if (role[n.group] != T_BASS)
				continue;
			int s = (int)std::lround((double)n.start / tps);
			int e2 = std::max(s + 1, (int)std::lround((double)n.end / tps));
			for (int k = std::max(0, s); k < std::min<int>(e2, MAX_STEPS); k++)
				if (bassAt[k] < 0 || n.note < bassAt[k])
					bassAt[k] = n.note;
		}
		uint16_t maskAt[MAX_STEPS] = {0};
		int lowAt[MAX_STEPS];
		for (int s = 0; s < MAX_STEPS; s++)
			lowAt[s] = 999;
		for (RawNote& n : notes) {
			if (role[n.group] != T_PADS)
				continue;
			int s = (int)std::lround((double)n.start / tps);
			if (s < 0 || s >= L)
				continue;
			maskAt[s] |= (uint16_t)(1 << mod12(n.note - P.key));
			lowAt[s] = std::min(lowAt[s], n.note);
			PadStep& ps = pat.pads[s];
			if (ps.nNotes < 4) {
				bool dup = false;
				for (int k = 0; k < ps.nNotes; k++)
					if (ps.notes[k] == n.note - P.key)
						dup = true;
				if (!dup)
					ps.notes[ps.nNotes++] = (int8_t)clampi(n.note - P.key, -127, 127);
			}
			ps.force = 1;
			ps.flen = (uint8_t)std::max<int>(ps.flen, clampi((int)std::lround((double)(n.end - n.start) / tps), 1, 128));
			ps.fvel = (uint8_t)std::max<int>(ps.fvel, clampi(n.vel, 1, 127));
		}
		// build chord timeline from onsets (single notes are ignored)
		int nProg = 0, nEv = 0;
		uint16_t progMask[MAX_PROG];
		uint16_t lastMask = 0;
		for (int s = 0; s < L; s++) {
			uint16_t m = maskAt[s];
			int bits = 0;
			for (int i = 0; i < 12; i++)
				bits += (m >> i) & 1;
			if (bits < 2 || m == lastMask)
				continue;
			int spec = -1;
			for (int i = 0; i < nProg; i++)
				if (progMask[i] == m)
					spec = i;
			if (spec < 0) {
				if (nProg < MAX_PROG) {
					spec = nProg++;
					progMask[spec] = m;
					Chord ch = chordFromPcs(m, mod12(lowAt[s] - P.key), bassAt[s] >= 0 ? mod12(bassAt[s] - P.key) : -1);
					ChordSpec& cs = pat.prog[spec];
					std::memset(&cs, 0, sizeof(cs));
					cs.root = (int8_t)ch.root;
					cs.quality = Q_MASK;
					cs.explicitQ = 1;
					for (int i = 0; i < ch.n; i++)
						cs.mask |= (uint16_t)(1 << ch.iv[i]);
				}
				else
					spec = nProg - 1;
			}
			if (nEv >= MAX_EVENTS)
				break;
			// stabs often start just after the bar line: snap chord changes to the half-bar grid
			int st = (s % 8 <= 3) ? s - s % 8 : s;
			if (nEv > 0 && st <= pat.ev[nEv - 1].start)
				st = s;
			if (nEv > 0)
				pat.ev[nEv - 1].len = (uint8_t)(st - pat.ev[nEv - 1].start);
			ChordEvent ce;
			ce.spec = (uint8_t)spec;
			ce.start = (uint8_t)(nEv == 0 ? 0 : st);
			ce.len = 0;
			ce.pad = 0;
			pat.ev[nEv++] = ce;
			lastMask = m;
		}
		if (nEv > 0) {
			pat.ev[nEv - 1].len = (uint8_t)(L - pat.ev[nEv - 1].start);
			pat.nProg = (uint8_t)nProg;
			pat.nEvents = (uint8_t)nEv;
			pat.customTimeline = 1;
			pat.layoutLen = (uint8_t)L;
		}
	}
	err.clear();
	return barsQ;
}

} // namespace genra
