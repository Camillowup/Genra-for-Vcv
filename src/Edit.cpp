// GENRA – grid editing: fixed notes, freezing generated tracks, chord follow, snapping.
// Pure C++, no Rack dependency (tested in test/test_edit.cpp).
#include "Engine.hpp"
#include <algorithm>
#include <cmath>

namespace genra {

static inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
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
			}
			else {
				n.force = 2;
				n.absNote = NO_ABS;
				n.flen = n.fvel = n.fflags = n.fgate = 0;
				n.froot = -1;
			}
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
			}
			else {
				ps.force = 2;
				ps.nNotes = 0;
				ps.flen = ps.fvel = ps.fgate = 0;
				ps.froot = -1;
			}
		}
	}
	tile(pat, track, L);
	pat.fixed[track] = 1;
}

std::vector<GridNote> trackNotes(const Pattern& pat, const Params& P, int track, int L) {
	std::vector<GridNote> out;
	L = clampi(L, 1, MAX_STEPS);
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
		for (int s = 0; s < L; s++) {
			PadStep& ps = pat.pads[s];
			ps.force = 2;
			ps.nNotes = 0;
			ps.flen = ps.fvel = ps.fgate = 0;
			ps.froot = -1;
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
		}
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
	pat.fixed[T_DRUMS] = 1;
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
				}
				break;
			case T_BASS:
			case T_MELODY: {
				NoteStep& n = track == T_BASS ? pat.bass[s] : pat.mel[s];
				n.force = 2;
				n.absNote = NO_ABS;
				n.flen = n.fvel = n.fflags = n.fgate = 0;
				n.froot = -1;
				break;
			}
			case T_PADS: {
				PadStep& ps = pat.pads[s];
				ps.force = 2;
				ps.nNotes = 0;
				ps.flen = ps.fvel = ps.fgate = 0;
				ps.froot = -1;
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
				}
				break;
			case T_BASS: pat.bass[s] = clip.notes[i]; break;
			case T_MELODY: pat.mel[s] = clip.notes[i]; break;
			case T_PADS: pat.pads[s] = clip.pads[i]; break;
		}
	}
	return true;
}

} // namespace genra
