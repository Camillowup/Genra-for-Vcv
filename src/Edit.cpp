// GENRA – grid editing: fixed notes, freezing generated tracks, chord follow, snapping.
// Pure C++, no Rack dependency (tested in test/test_edit.cpp).
#include "Engine.hpp"
#include "Arranger.hpp"
#include <algorithm>
#include <cmath>

namespace genra {

static inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline int mod12(int x) { return ((x % 12) + 12) % 12; }
static inline int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

// nearest scale degree at or below a pitch class
static int degreeBelow(const int* sc, int pc) {
	pc = mod12(pc);
	int best = 0;
	for (int i = 0; i < 7; i++)
		if (sc[i] <= pc)
			best = i;
	return best;
}

// diatonic transposition by k scale degrees (chromatic alterations are kept)
static int diatonicShift(const int* sc, int n, int k) {
	int oct = floordiv(n, 12);
	int pc = n - oct * 12;
	int d = degreeBelow(sc, pc);
	int alt = pc - sc[d];
	int idx = oct * 7 + d + k;
	int o2 = floordiv(idx, 7);
	return o2 * 12 + sc[idx - o2 * 7] + alt;
}

bool chancePlays(int chance, int cond, int track, int voice, int step, const Params& P) {
	if (P.ignoreChance)
		return true;
	if (cond == NC_EVERY2 && (P.pass & 1))
		return false;
	if (chance <= 0 || chance >= 100)
		return true;
	// deterministic per pass: the display, playback and MIDI export agree
	uint32_t h = (uint32_t)(track * 977 + voice * 131 + step * 7919) ^ ((uint32_t)P.pass * 2654435761u);
	h ^= h >> 15;
	h *= 2246822519u;
	h ^= h >> 13;
	h *= 3266489917u;
	h ^= h >> 16;
	return (int)(h % 100u) < chance;
}

int followAbs(const Pattern& pat, const Params& P, int track, int step, int absNote, int froot) {
	if (track < 0 || track >= NUM_TRACKS || !pat.chordFollow[track] || froot < 0)
		return absNote;
	int root = chordAt(pat, P, step).root;
	if (root == froot)
		return absNote;
	int n;
	if (mod12(absNote) == mod12(froot)) {
		// the chord root itself: becomes the new root (shortest way)
		int d = mod12(root - froot);
		if (d > 6)
			d -= 12;
		n = absNote + d;
	}
	else {
		// any other note moves by scale degrees, so scale notes stay in the scale
		const int* sc = SCALES[clampi(P.scale, 0, NUM_SCALES - 1)].iv;
		int k = degreeBelow(sc, root) - degreeBelow(sc, froot);
		if (k > 3) // move the shortest way
			k -= 7;
		if (k < -3)
			k += 7;
		n = diatonicShift(sc, absNote, k);
	}
	return clampi(n, -127, 127);
}

static int octOf(const Params& P, int track) {
	return track == T_BASS ? P.oct[0] : (track == T_MELODY ? P.oct[1] : P.oct[2]);
}
static int toAbs(const Params& P, int track, int pitch) { return pitch - P.key - P.transp - 12 * octOf(P, track); }

static void tile(Pattern& pat, int track, int L) {
	L = clampi(L, 1, MAX_STEPS);
	for (int s = L; s < MAX_STEPS; s++) {
		int src = s % L;
		switch (track) {
			case T_DRUMS:
				for (int v = 0; v < NUM_DRUMS; v++)
					pat.drums[v][s] = pat.drums[v][src];
				break;
			case T_BASS: pat.bass[s] = pat.bass[src]; break;
			case T_MELODY: pat.mel[s] = pat.mel[src]; break;
			case T_PADS: pat.pads[s] = pat.pads[src]; break;
		}
	}
}

static uint8_t vel127(float v) { return (uint8_t)clampi((int)std::lround(v * 127.f), 1, 127); }

void freezeTrack(Pattern& pat, const Params& P0, int track, int L) {
	L = clampi(L, 1, MAX_STEPS);
	Params P = P0;
	P.fillActive = false; // fills and drops stay live, they are not baked into the notes
	if (track < 0 || track >= NUM_TRACKS || pat.fixed[track])
		return;
	if (track == T_DRUMS) {
		static Hit hits[MAX_STEPS][NUM_DRUMS];
		for (int s = 0; s < L; s++)
			drumStep(pat, P, s, L, false, hits[s]);
		for (int s = 0; s < L; s++)
			for (int v = 0; v < NUM_DRUMS; v++) {
				DrumStep& d = pat.drums[v][s];
				d.fv = hits[s][v].on ? vel127(hits[s][v].vel) : 0;
				d.frat = hits[s][v].on && hits[s][v].ratchet > 1 ? (uint8_t)hits[s][v].ratchet : 0;
				d.fchance = d.fcond = 0;
				d.fnudge = 0;
			}
	}
	else if (track == T_BASS || track == T_MELODY) {
		static NoteEv ev[MAX_STEPS];
		static int roots[MAX_STEPS];
		for (int s = 0; s < L; s++) {
			ev[s] = track == T_BASS ? bassAt(pat, P, s, L) : melAt(pat, P, s, L);
			roots[s] = chordAt(pat, P, s).root;
		}
		NoteStep* arr = track == T_BASS ? pat.bass : pat.mel;
		for (int s = 0; s < L; s++) {
			NoteStep& n = arr[s];
			const NoteEv& e = ev[s];
			if (e.on) {
				n.force = 1;
				n.absNote = (int8_t)clampi(toAbs(P, track, e.pitch), -127, 127);
				n.flen = (uint8_t)clampi(e.len, 1, 64);
				n.fvel = vel127(e.vel);
				n.fflags = (uint8_t)(FF_SET | (e.slide ? FF_SLIDE : 0) | (e.acc ? FF_ACC : 0));
				n.fgate = (uint8_t)clampi((int)std::lround(e.gate * 100.f), 1, 200);
				n.froot = (int8_t)mod12(roots[s]);
				n.fchance = n.fcond = 0;
			}
			else {
				n.force = 2;
				n.absNote = NO_ABS;
				n.flen = n.fvel = n.fflags = n.fgate = 0;
				n.froot = -1;
			}
			n.fnudge = 0;
		}
		if (track == T_MELODY)
			pat.melMode = 0;
	}
	else {
		static PadEv ev[MAX_STEPS];
		static int roots[MAX_STEPS];
		int prev[4] = {60, 64, 67, 72};
		bool hasPrev = false;
		for (int s = 0; s < L; s++) {
			ev[s] = padAt(pat, P, s, L, prev, hasPrev);
			roots[s] = chordAt(pat, P, s).root;
		}
		int o = 12 * P.oct[2];
		for (int s = 0; s < L; s++) {
			PadStep& ps = pat.pads[s];
			const PadEv& e = ev[s];
			if (e.on) {
				ps.force = 1;
				ps.nNotes = 4; // keep the voicing exactly (doubled notes included)
				for (int k = 0; k < 4; k++)
					ps.notes[k] = (int8_t)clampi(e.notes[k] - P.key - P.transp - o, -127, 127);
				ps.flen = (uint8_t)clampi(e.len, 1, 128);
				ps.fvel = vel127(e.vel);
				ps.fgate = (uint8_t)clampi((int)std::lround(e.gate * 100.f), 1, 200);
				ps.froot = (int8_t)mod12(roots[s]);
				ps.fchance = ps.fcond = 0;
			}
			else {
				ps.force = 2;
				ps.nNotes = 0;
				ps.flen = ps.fvel = ps.fgate = 0;
				ps.froot = -1;
			}
			ps.fnudge = 0;
		}
	}
	tile(pat, track, L);
	pat.fixed[track] = 1;
}

std::vector<GridNote> trackNotes(const Pattern& pat, const Params& P0, int track, int L) {
	std::vector<GridNote> out;
	Params P = P0;
	P.ignoreChance = true; // the editor shows every note, also those skipped in this pass
	L = clampi(L, 1, MAX_STEPS);
	// only the track's own cycle: the rest of the loop repeats it
	if (track > T_DRUMS && track < NUM_TRACKS && pat.tLen[track] > 0)
		L = std::min(L, (int)pat.tLen[track]);
	if (track == T_BASS || track == T_MELODY) {
		for (int s = 0; s < L; s++) {
			NoteEv e = track == T_BASS ? bassAt(pat, P, s, L) : melAt(pat, P, s, L);
			if (!e.on)
				continue;
			GridNote g;
			g.start = s;
			g.len = std::max(1, e.len);
			g.pitch = e.pitch;
			g.vel = vel127(e.vel);
			g.slide = e.slide;
			g.acc = e.acc;
			if (pat.fixed[track]) {
				const NoteStep& n = track == T_BASS ? pat.bass[s] : pat.mel[s];
				g.chance = n.fchance ? n.fchance : 100;
				g.cond = n.fcond;
			}
			g.nudge = e.nudge;
			out.push_back(g);
		}
		// mono: a note ends where the next one starts
		for (size_t i = 0; i + 1 < out.size(); i++)
			out[i].len = std::max(1, std::min(out[i].len, out[i + 1].start - out[i].start));
		if (!out.empty())
			out.back().len = std::max(1, std::min(out.back().len, L - out.back().start));
	}
	else if (track == T_PADS) {
		int prev[4] = {60, 64, 67, 72};
		bool hasPrev = false;
		for (int s = 0; s < L; s++) {
			PadEv e = padAt(pat, P, s, L, prev, hasPrev);
			if (!e.on)
				continue;
			for (int k = 0; k < 4; k++) {
				bool dup = false;
				for (int j = 0; j < k; j++)
					dup |= e.notes[j] == e.notes[k];
				if (dup)
					continue;
				GridNote g;
				g.start = s;
				g.len = std::max(1, std::min(e.len, L - s));
				g.pitch = e.notes[k];
				g.vel = vel127(e.vel);
				if (pat.fixed[T_PADS]) {
					g.chance = pat.pads[s].fchance ? pat.pads[s].fchance : 100;
					g.cond = pat.pads[s].fcond;
				}
				g.nudge = e.nudge;
				out.push_back(g);
			}
		}
	}
	return out;
}

void writeTrackNotes(Pattern& pat, const Params& P, int track, const std::vector<GridNote>& notes, int L) {
	if (track != T_BASS && track != T_MELODY && track != T_PADS)
		return;
	L = clampi(L, 1, MAX_STEPS);
	freezeTrack(pat, P, track, L);
	if (track == T_PADS) {
		int chanceMax[MAX_STEPS] = {0};
		for (int s = 0; s < L; s++) {
			PadStep& ps = pat.pads[s];
			ps.force = 2;
			ps.nNotes = 0;
			ps.flen = ps.fvel = ps.fgate = 0;
			ps.froot = -1;
			ps.fchance = ps.fcond = 0;
			ps.fnudge = 0;
		}
		for (const GridNote& g : notes) {
			if (g.start < 0 || g.start >= L)
				continue;
			PadStep& ps = pat.pads[g.start];
			int a = clampi(g.pitch - P.key - P.transp - 12 * P.oct[2], -127, 127);
			bool dup = false;
			for (int k = 0; k < ps.nNotes; k++)
				dup |= ps.notes[k] == a;
			if (!dup && ps.nNotes < 4)
				ps.notes[ps.nNotes++] = (int8_t)a;
			ps.force = 1;
			ps.flen = (uint8_t)std::max<int>(ps.flen, clampi(g.len, 1, 128));
			ps.fvel = (uint8_t)std::max<int>(ps.fvel, clampi(g.vel, 1, 127));
			ps.froot = (int8_t)mod12(chordAt(pat, P, g.start).root);
			// one chance per chord onset: the most likely note decides
			chanceMax[g.start] = std::max(chanceMax[g.start], clampi(g.chance, 1, 100));
			if (g.cond)
				ps.fcond = (uint8_t)clampi(g.cond, 0, 1);
			if (g.nudge) // one timing per chord onset
				ps.fnudge = (int8_t)clampi(g.nudge, -NUDGE_MAX, NUDGE_MAX);
		}
		for (int s = 0; s < L; s++)
			if (chanceMax[s] > 0)
				pat.pads[s].fchance = (uint8_t)(chanceMax[s] >= 100 ? 0 : chanceMax[s]);
		// fill up to 4 voices with doublings so nothing is added an octave up
		for (int s = 0; s < L; s++) {
			PadStep& ps = pat.pads[s];
			if (ps.force != 1 || ps.nNotes == 0)
				continue;
			std::sort(ps.notes, ps.notes + ps.nNotes);
			for (int k = ps.nNotes; k < 4; k++)
				ps.notes[k] = ps.notes[k - 1];
			ps.nNotes = 4;
		}
		return;
	}
	NoteStep* arr = track == T_BASS ? pat.bass : pat.mel;
	for (int s = 0; s < L; s++) {
		NoteStep& n = arr[s];
		n.force = 2;
		n.absNote = NO_ABS;
		n.flen = n.fvel = n.fflags = n.fgate = 0;
		n.froot = -1;
		n.fchance = n.fcond = 0;
		n.fnudge = 0;
	}
	for (const GridNote& g : notes) {
		if (g.start < 0 || g.start >= L)
			continue;
		NoteStep& n = arr[g.start];
		n.force = 1;
		n.absNote = (int8_t)clampi(toAbs(P, track, g.pitch), -127, 127);
		n.flen = (uint8_t)clampi(g.len, 1, 64);
		n.fvel = (uint8_t)clampi(g.vel, 1, 127);
		n.fflags = (uint8_t)(FF_SET | (g.slide ? FF_SLIDE : 0) | (g.acc ? FF_ACC : 0));
		n.fgate = 0;
		n.froot = (int8_t)mod12(chordAt(pat, P, g.start).root);
		n.fchance = (uint8_t)(g.chance >= 100 ? 0 : clampi(g.chance, 1, 99));
		n.fcond = (uint8_t)clampi(g.cond, 0, 1);
		n.fnudge = (int8_t)clampi(g.nudge, -NUDGE_MAX, NUDGE_MAX);
	}
	if (track == T_MELODY)
		pat.melMode = 0;
}

void setDrum(Pattern& pat, int voice, int step, int vel, int ratchet) {
	if (voice < 0 || voice >= NUM_DRUMS || step < 0 || step >= MAX_STEPS)
		return;
	DrumStep& d = pat.drums[voice][step];
	d.fv = (uint8_t)clampi(vel, 0, 127);
	d.frat = vel > 0 && ratchet > 1 ? (uint8_t)clampi(ratchet, 2, 4) : 0;
	if (vel <= 0) {
		d.fchance = d.fcond = 0;
		d.fnudge = 0;
	}
	pat.fixed[T_DRUMS] = 1;
}

void setDrumNudge(Pattern& pat, int voice, int step, int nudge) {
	if (voice < 0 || voice >= NUM_DRUMS || step < 0 || step >= MAX_STEPS)
		return;
	pat.drums[voice][step].fnudge = (int8_t)clampi(nudge, -NUDGE_MAX, NUDGE_MAX);
}

void setCycleLen(Pattern& pat, int track, int voice, int len) {
	uint8_t v = (uint8_t)clampi(len, 0, MAX_STEPS);
	if (track == T_DRUMS) {
		if (voice >= 0 && voice < NUM_DRUMS)
			pat.vLen[voice] = v;
	}
	else if (track > T_DRUMS && track < NUM_TRACKS)
		pat.tLen[track] = v;
}

void setSwing(Pattern& pat, int track, int voice, int pct) {
	uint8_t v = pct <= 0 ? 0 : (uint8_t)clampi(pct, SWING_MIN, SWING_MAX);
	if (track == T_DRUMS) {
		if (voice >= 0 && voice < NUM_DRUMS)
			pat.vSwing[voice] = v;
	}
	else if (track > T_DRUMS && track < NUM_TRACKS)
		pat.tSwing[track] = v;
}

void setDrumChance(Pattern& pat, int voice, int step, int chance, int cond) {
	if (voice < 0 || voice >= NUM_DRUMS || step < 0 || step >= MAX_STEPS)
		return;
	DrumStep& d = pat.drums[voice][step];
	d.fchance = (uint8_t)(chance >= 100 ? 0 : clampi(chance, 1, 99));
	d.fcond = (uint8_t)clampi(cond, 0, 1);
}

void setChordFollow(Pattern& pat, const Params& P, int track, bool on, int L) {
	if (track < T_BASS || track > T_PADS)
		return;
	L = clampi(L, 1, MAX_STEPS);
	if (on && pat.fixed[track]) {
		// notes without a known chord (e.g. MIDI import) are written against the current chords
		for (int s = 0; s < MAX_STEPS; s++) {
			int root = mod12(chordAt(pat, P, s % L).root);
			if (track == T_PADS) {
				if (pat.pads[s].force == 1 && pat.pads[s].froot < 0)
					pat.pads[s].froot = (int8_t)root;
			}
			else {
				NoteStep& n = track == T_BASS ? pat.bass[s] : pat.mel[s];
				if (n.force == 1 && n.absNote != NO_ABS && n.froot < 0)
					n.froot = (int8_t)root;
			}
		}
	}
	else if (!on && pat.fixed[track] && pat.chordFollow[track]) {
		// keep the notes where they sound now
		for (int s = 0; s < MAX_STEPS; s++) {
			int st = s % L;
			int root = mod12(chordAt(pat, P, st).root);
			if (track == T_PADS) {
				PadStep& ps = pat.pads[s];
				if (ps.force == 1 && ps.froot >= 0) {
					for (int k = 0; k < 4; k++)
						ps.notes[k] = (int8_t)followAbs(pat, P, T_PADS, st, ps.notes[k], ps.froot);
					ps.froot = (int8_t)root;
				}
			}
			else {
				NoteStep& n = track == T_BASS ? pat.bass[s] : pat.mel[s];
				if (n.force == 1 && n.absNote != NO_ABS && n.froot >= 0) {
					n.absNote = (int8_t)followAbs(pat, P, track, st, n.absNote, n.froot);
					n.froot = (int8_t)root;
				}
			}
		}
	}
	pat.chordFollow[track] = on ? 1 : 0;
}

int snapPitch(const Pattern& pat, const Params& P, int step, int pitch, int mode) {
	if (mode <= 0)
		return pitch;
	int keyPc = mod12(P.key + P.transp);
	uint16_t mask = 0;
	if (mode == 1) {
		const int* sc = SCALES[clampi(P.scale, 0, NUM_SCALES - 1)].iv;
		for (int i = 0; i < 7; i++)
			mask |= (uint16_t)(1 << mod12(sc[i] + keyPc));
	}
	else {
		Chord ch = chordAt(pat, P, clampi(step, 0, MAX_STEPS - 1));
		for (int i = 0; i < ch.n; i++)
			mask |= (uint16_t)(1 << mod12(ch.root + ch.iv[i] + keyPc));
	}
	if (!mask)
		return pitch;
	for (int d = 0; d <= 6; d++) {
		if (mask & (1 << mod12(pitch - d)))
			return clampi(pitch - d, 0, 127); // prefer the lower neighbour on ties
		if (mask & (1 << mod12(pitch + d)))
			return clampi(pitch + d, 0, 127);
	}
	return pitch;
}

void copyTrack(const Pattern& from, Pattern& to, int track) {
	if (track < 0 || track >= NUM_TRACKS)
		return;
	to.fixed[track] = from.fixed[track];
	to.chordFollow[track] = from.chordFollow[track];
	to.trackGenre[track] = from.trackGenre[track];
	to.tLen[track] = from.tLen[track];
	to.tSwing[track] = from.tSwing[track];
	to.partMask = (uint8_t)((to.partMask & ~(1u << track)) | (from.partMask & (1u << track)));
	if (from.partMask & (1u << track))
		to.partTag = from.partTag;
	if (track == T_DRUMS) {
		std::memcpy(to.vLen, from.vLen, sizeof(to.vLen));
		std::memcpy(to.vSwing, from.vSwing, sizeof(to.vSwing));
	}
	switch (track) {
		case T_DRUMS:
			std::memcpy(to.drums, from.drums, sizeof(to.drums));
			std::memcpy(to.chop, from.chop, sizeof(to.chop));
			std::memcpy(to.chopR, from.chopR, sizeof(to.chopR));
			break;
		case T_BASS: std::memcpy(to.bass, from.bass, sizeof(to.bass)); break;
		case T_MELODY:
			std::memcpy(to.mel, from.mel, sizeof(to.mel));
			to.melMode = from.melMode;
			to.melCenter = from.melCenter;
			break;
		case T_PADS: std::memcpy(to.pads, from.pads, sizeof(to.pads)); break;
	}
}

void clearTrack(Pattern& pat, int track) {
	for (int s = 0; s < MAX_STEPS; s++) {
		switch (track) {
			case T_DRUMS:
				for (int v = 0; v < NUM_DRUMS; v++) {
					pat.drums[v][s].fv = 0;
					pat.drums[v][s].frat = 0;
					pat.drums[v][s].fchance = pat.drums[v][s].fcond = 0;
					pat.drums[v][s].fnudge = 0;
				}
				break;
			case T_BASS:
			case T_MELODY: {
				NoteStep& n = track == T_BASS ? pat.bass[s] : pat.mel[s];
				n.force = 2;
				n.absNote = NO_ABS;
				n.flen = n.fvel = n.fflags = n.fgate = 0;
				n.froot = -1;
				n.fchance = n.fcond = 0;
				n.fnudge = 0;
				break;
			}
			case T_PADS: {
				PadStep& ps = pat.pads[s];
				ps.force = 2;
				ps.nNotes = 0;
				ps.flen = ps.fvel = ps.fgate = 0;
				ps.froot = -1;
				ps.fchance = ps.fcond = 0;
				ps.fnudge = 0;
				break;
			}
		}
	}
	if (track >= 0 && track < NUM_TRACKS)
		pat.fixed[track] = 1;
}

void copyBar(const Pattern& pat, int track, int bar, BarClip& clip) {
	bar = clampi(bar, 0, MAX_BARS - 1);
	clip.track = track;
	for (int i = 0; i < 16; i++) {
		int s = bar * 16 + i;
		for (int v = 0; v < NUM_DRUMS; v++)
			clip.drums[v][i] = pat.drums[v][s];
		clip.notes[i] = track == T_MELODY ? pat.mel[s] : pat.bass[s];
		clip.pads[i] = pat.pads[s];
	}
}

bool pasteBar(Pattern& pat, int track, int bar, const BarClip& clip) {
	if (clip.track != track || !pat.fixed[track])
		return false;
	bar = clampi(bar, 0, MAX_BARS - 1);
	for (int i = 0; i < 16; i++) {
		int s = bar * 16 + i;
		switch (track) {
			case T_DRUMS:
				for (int v = 0; v < NUM_DRUMS; v++) {
					pat.drums[v][s].fv = clip.drums[v][i].fv;
					pat.drums[v][s].frat = clip.drums[v][i].frat;
					pat.drums[v][s].fchance = clip.drums[v][i].fchance;
					pat.drums[v][s].fcond = clip.drums[v][i].fcond;
					pat.drums[v][s].fnudge = clip.drums[v][i].fnudge;
				}
				break;
			case T_BASS: pat.bass[s] = clip.notes[i]; break;
			case T_MELODY: pat.mel[s] = clip.notes[i]; break;
			case T_PADS: pat.pads[s] = clip.pads[i]; break;
		}
	}
	return true;
}

// ================================================================= song parts over their row
// The parts made by ABLAUF (INTRO, BUILD, BREAK, OUTRO) used to be shaped over the loop: a 1-bar break in a 16-bar row
// played its pickup 16 times. Now the part's pattern holds the material and these run its course over the row.

PartPos partPos(const Pattern& pat, int pos, int span) {
	PartPos pp;
	if (!pat.partTag || !pat.partMask || span <= 0)
		return pp;
	pp.sec = pat.partTag - 1;
	pp.span = span;
	pp.pos = clampi(pos, 0, span - 1);
	return pp;
}

static void hitOff(Hit& h) { h.on = false; }
static void hitPut(Hit& h, int vel, int ratchet = 1) {
	h.on = true;
	h.vel = clampf(vel / 127.f, 0.f, 1.f);
	h.ratchet = ratchet;
	h.delay = 0.f;
	h.nudge = 0;
}
static void hitScale(Hit& h, float k) {
	if (h.on)
		h.vel = clampf(h.vel * k, 0.f, 1.f);
}

void shapeDrums(const PartPos& pp, Hit h[NUM_DRUMS]) {
	if (pp.sec < 0 || pp.span <= 0)
		return;
	const int span = pp.span, pos = pp.pos, sb = pos % STEPS_PER_BAR, left = span - pos;
	const float x = (float)pos / span;
	switch (pp.sec) {
		case SEC_INTRO: // the drums come in: first the kick on the one, then the backbeat, getting louder
			if (span >= 32 && x < 0.5f) {
				hitOff(h[SNARE]);
				hitOff(h[CLAP]);
				hitOff(h[RIM]);
			}
			if (span >= 64 && x < 0.25f && sb != 0)
				hitOff(h[KICK]);
			for (int v = 0; v < NUM_DRUMS; v++)
				hitScale(h[v], 0.72f + 0.28f * x);
			break;
		case SEC_BUILD: { // one roll over the whole row: quarters, 8ths, 16ths, 32nds on the last beat, getting louder
			int lb = span >= 32 ? span - 16 : span / 2;
			float q0, q1, e1;
			if (span >= 128)
				q0 = 0.25f, q1 = 0.5f, e1 = 0.75f;
			else if (span >= 64)
				q0 = 0.f, q1 = 0.5f, e1 = 0.75f;
			else if (span >= 32)
				q0 = 0.f, q1 = 0.25f, e1 = 0.5f;
			else
				q0 = 0.f, q1 = 0.f, e1 = 0.5f;
			if (pos >= lb) { // the last bar: kick on quarters, stops on the last beat: the drop lands in the gap
				hitOff(h[OHAT]);
				hitOff(h[PERC]);
				if (sb % 4 == 0 && left > 4)
					hitPut(h[KICK], 118);
				else
					hitOff(h[KICK]);
			}
			if (x >= 0.5f)
				hitPut(h[CHAT], (int)(42 + 58 * x));
			if (left <= 2)
				hitOff(h[CHAT]);
			bool on = false;
			if (x >= q0)
				on = x < q1 ? sb % 4 == 0 : (x < e1 ? sb % 2 == 0 : true);
			if (on) {
				int vel = (int)(36 + 91 * std::pow(x, 1.4f));
				hitPut(h[SNARE], vel, left <= 4 ? 2 : 1);
				if (left <= 4)
					hitPut(h[CLAP], vel * 3 / 4);
			}
			break;
		}
		case SEC_BREAK: // a long break: the first half without drums, then hats and percussion come back
			if (span >= 128 && x < 0.5f)
				for (int v = 0; v < NUM_DRUMS; v++)
					hitOff(h[v]);
			else if (span >= 64 && x < 0.25f)
				hitOff(h[CHAT]);
			if (span >= 128 && x >= 0.5f)
				for (int v = 0; v < NUM_DRUMS; v++)
					hitScale(h[v], 0.6f + 0.8f * (x - 0.5f));
			if (span >= 32 && left <= 4) // the snare pickup: once, into the next part
				hitPut(h[SNARE], 50 + (4 - left) * 20);
			break;
		case SEC_OUTRO: { // thins out: toms/perc/open hat, then claps and ghosts, then the snare; the last bar the kick
			int lb = span >= 32 ? span - 16 : span;
			if (x >= 0.25f) {
				hitOff(h[TOM]);
				hitOff(h[PERC]);
				hitOff(h[OHAT]);
			}
			if (x >= 0.5f) {
				hitOff(h[CLAP]);
				hitOff(h[RIM]);
				for (int v = 0; v < NUM_DRUMS; v++)
					if (v != KICK && h[v].on && h[v].vel < 55.f / 127.f)
						hitOff(h[v]);
			}
			if (x >= 0.75f) {
				hitOff(h[SNARE]);
				if (pos % 2)
					hitOff(h[CHAT]);
			}
			if (pos >= lb) {
				for (int v = 0; v < NUM_DRUMS; v++)
					if (v != KICK)
						hitOff(h[v]);
				if (sb % 8)
					hitOff(h[KICK]);
				else if (pos == lb && !h[KICK].on)
					hitPut(h[KICK], 90);
			}
			for (int v = 0; v < NUM_DRUMS; v++)
				hitScale(h[v], 1.f - 0.35f * x);
			break;
		}
		default: break;
	}
}

void shapeNote(const PartPos& pp, int track, NoteEv& n) {
	if (!n.on || pp.sec < 0 || pp.span <= 0)
		return;
	const int span = pp.span, pos = pp.pos;
	const float x = (float)pos / span;
	const bool bass = track == T_BASS;
	switch (pp.sec) {
		case SEC_INTRO: // the bass and the melody enter halfway
			if (span >= 32 && x < 0.5f)
				n.on = false;
			break;
		case SEC_BUILD:
			if (bass) { // out in the last bar: the gap before the drop
				int lb = span >= 32 ? span - 16 : span - 4;
				if (pos >= lb)
					n.on = false;
				else if (pos + n.len > lb) {
					n.len = std::max(1, lb - pos);
					n.slide = false;
				}
			}
			else {
				if (span >= 32 && x >= 0.75f && n.pitch <= 115)
					n.pitch += 12; // the last quarter an octave up
				n.vel = clampf(n.vel * (0.8f + 0.25f * x), 0.f, 1.f);
			}
			break;
		case SEC_OUTRO:
			if (bass) {
				if ((x >= 0.5f && pos % 8) || (span >= 32 && pos >= span - 16))
					n.on = false;
				else
					n.vel = clampf(n.vel * (1.f - 0.3f * x), 0.f, 1.f);
			}
			else {
				if (span >= 32 && x >= 0.5f)
					n.on = false;
				else
					n.vel = clampf(n.vel * (1.f - 0.4f * x), 0.f, 1.f);
			}
			break;
		default: break;
	}
}

bool partPhraseFill(const Pattern& pat, int bar, int bars) {
	int sec = (int)pat.partTag - 1;
	if ((sec != SEC_VERSE && sec != SEC_DROP) || bars < 16)
		return false;
	return bar % 8 == 7 && bar < bars - 1;
}

} // namespace genra
