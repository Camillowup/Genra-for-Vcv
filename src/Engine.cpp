// GENRA – engine implementation
#include "Engine.hpp"
#include <cmath>
#include <algorithm>
#include <cstdlib>

namespace genra {

// ================================================================= helpers
static inline float clampf(float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
static inline int clampi(int x, int a, int b) { return x < a ? a : (x > b ? b : x); }
static inline int mod12(int x) { return ((x % 12) + 12) % 12; }
static inline int floordiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
static inline int modn(int a, int b) { return ((a % b) + b) % b; }

static bool inScale(const Params& P, int pcRelKey) {
	const int* sc = SCALES[P.scale].iv;
	int pc = mod12(pcRelKey);
	for (int i = 0; i < 7; i++)
		if (sc[i] == pc)
			return true;
	return false;
}

static int degreeOf(const int* sc, int pc) {
	for (int i = 0; i < 7; i++)
		if (sc[i] == mod12(pc))
			return i;
	return -1;
}

static int degreeSemi(const int* sc, int d) {
	return sc[modn(d, 7)] + 12 * floordiv(d, 7);
}

static int diatonicQuality(const int* sc, int deg) {
	int third = mod12(sc[(deg + 2) % 7] - sc[deg]);
	int fifth = mod12(sc[(deg + 4) % 7] - sc[deg]);
	if (third == 4)
		return fifth == 8 ? Q_AUG : Q_MAJ;
	return fifth == 6 ? Q_DIM : Q_MIN;
}

static int pickWeighted(Rng& rng, const float* w, int n) {
	float sum = 0.f;
	for (int i = 0; i < n; i++)
		sum += std::max(0.f, w[i]);
	if (sum <= 0.f)
		return rng.range(n);
	float x = rng.uni() * sum;
	for (int i = 0; i < n; i++) {
		x -= std::max(0.f, w[i]);
		if (x <= 0.f)
			return i;
	}
	return n - 1;
}

// functional harmony transition matrix between scale degrees
static const float TRANS[7][7] = {
	{0.0f, 0.3f, 0.8f, 1.2f, 0.8f, 1.2f, 1.0f},
	{0.3f, 0.0f, 0.3f, 0.5f, 1.5f, 0.3f, 0.6f},
	{0.5f, 0.3f, 0.0f, 0.8f, 0.3f, 1.2f, 1.0f},
	{1.0f, 0.4f, 0.4f, 0.0f, 1.0f, 0.6f, 1.2f},
	{1.5f, 0.2f, 0.3f, 0.5f, 0.0f, 1.0f, 0.4f},
	{0.8f, 0.5f, 1.0f, 1.0f, 0.6f, 0.0f, 1.3f},
	{1.3f, 0.2f, 1.0f, 0.6f, 0.4f, 0.8f, 0.0f},
};

// ================================================================= effective values
float effComplex(const Params& P, int g) {
	return clampf(prof(g).complexity + (P.complexK - 0.5f) * 1.2f, 0.f, 1.f);
}
float effColor(const Params& P, int g) {
	return clampf(prof(g).color + (P.colorK - 0.5f) * 1.3f, 0.f, 1.f);
}
float effSwing(const Params& P, int g) {
	return clampf(prof(g).swing + P.swingK * 0.25f, 0.5f, 0.75f);
}

static float syncF(int s, float cx) {
	if (s % 2 == 1)
		return 0.3f + 1.4f * cx;
	if (s % 4 == 2)
		return 0.7f + 0.6f * cx;
	return 1.f;
}

// density for drums: anchors (p ~ 1) survive until density is almost zero
static float densDrum(float p, float d) {
	float f = d < 0.5f ? d * 2.f : 1.f + (d - 0.5f) * 3.f;
	return clampf(p * f, 0.f, 1.f);
}
// density for tonal tracks
static float densTonal(float p, float d) {
	if (p >= 0.95f)
		return clampf(d * 2.f, 0.f, 1.f);
	float f = d < 0.5f ? d * 2.f : 1.f + (d - 0.5f) * 2.4f;
	return clampf(p * f, 0.f, 1.f);
}

// ================================================================= chords
bool parseChordToken(const char* tok, ChordSpec& cs) {
	const char* p = tok;
	int acc = 0;
	if (*p == 'b' && (p[1] == 'I' || p[1] == 'V' || p[1] == 'i' || p[1] == 'v')) {
		acc = -1;
		p++;
	}
	else if (*p == '#') {
		acc = 1;
		p++;
	}
	bool upper = (*p == 'I' || *p == 'V');
	char rn[8] = {0};
	int k = 0;
	while (k < 4 && (*p == 'I' || *p == 'V' || *p == 'i' || *p == 'v')) {
		rn[k++] = (char)(*p == 'i' ? 'I' : (*p == 'v' ? 'V' : *p));
		p++;
	}
	static const char* ROMAN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
	static const int SEMI[7] = {0, 2, 4, 5, 7, 9, 11};
	int val = -1;
	for (int i = 0; i < 7; i++)
		if (std::strcmp(rn, ROMAN[i]) == 0)
			val = i;
	if (val < 0)
		return false;
	std::memset(&cs, 0, sizeof(cs));
	cs.root = (int8_t)mod12(SEMI[val] + acc);
	cs.quality = upper ? Q_MAJ : Q_MIN;
	cs.explicitQ = 1;
	std::string rest(p);
	if (!rest.empty() && rest[0] == 'o') {
		cs.quality = Q_DIM;
		rest = rest.substr(1);
	}
	else if (!rest.empty() && rest[0] == '+') {
		cs.quality = Q_AUG;
		rest = rest.substr(1);
	}
	if (rest == "maj7")
		cs.ext = EXT_7 | EXT_MAJ7;
	else if (rest == "maj9")
		cs.ext = EXT_7 | EXT_MAJ7 | EXT_9;
	else if (rest == "7")
		cs.ext = upper ? (EXT_7 | EXT_DOM7) : EXT_7;
	else if (rest == "9")
		cs.ext = upper ? (EXT_7 | EXT_DOM7 | EXT_9) : (EXT_7 | EXT_9);
	else if (rest == "sus2")
		cs.quality = Q_SUS2;
	else if (rest == "sus4")
		cs.quality = Q_SUS4;
	else if (rest == "add9")
		cs.ext = EXT_9;
	return true;
}

static Chord chordFromMask(int root, uint16_t mask) {
	Chord ch;
	ch.root = mod12(root);
	ch.n = 0;
	for (int i = 0; i < 16 && ch.n < 6; i++)
		if (mask & (1 << i))
			ch.iv[ch.n++] = i;
	if (ch.n == 0) {
		ch.n = 1;
		ch.iv[0] = 0;
	}
	return ch;
}

Chord resolveSpec(const ChordSpec& cs, const Params& P, float color) {
	if (cs.quality == Q_MASK)
		return chordFromMask(cs.root, cs.mask);
	const int* sc = SCALES[P.scale].iv;
	int root = mod12(cs.root);
	int q = cs.quality;
	int ext = cs.ext;
	int deg = degreeOf(sc, root);
	if (deg >= 0 && q != Q_SUS2 && q != Q_SUS4 && (!cs.explicitQ || P.scaleOverride)) {
		q = diatonicQuality(sc, deg);
		if (P.scaleOverride)
			ext &= ~(EXT_DOM7 | EXT_MAJ7);
	}
	float c = clampf(color + cs.extOff * 0.22f, 0.f, 1.f);
	float r = cs.rc / 255.f;
	float rs = cs.rs / 255.f;
	bool want7 = (ext & EXT_7) ? (c > 0.18f) : (r < (c - 0.3f) * 1.8f);
	bool want9 = (ext & EXT_9) ? (c > 0.3f) : (want7 && r < (c - 0.6f) * 2.2f);
	if ((q == Q_MAJ || q == Q_MIN) && !(ext & EXT_DOM7) && !cs.explicitQ && rs < (c - 0.55f) * 0.35f)
		q = (cs.rs & 1) ? Q_SUS2 : Q_SUS4;

	int third = 4, fifth = 7;
	switch (q) {
		case Q_MIN: third = 3; break;
		case Q_DIM: third = 3; fifth = 6; break;
		case Q_AUG: fifth = 8; break;
		case Q_SUS2: third = 2; break;
		case Q_SUS4: third = 5; break;
		default: break;
	}
	Chord ch;
	ch.root = root;
	ch.n = 0;
	ch.iv[ch.n++] = 0;
	ch.iv[ch.n++] = third;
	ch.iv[ch.n++] = fifth;
	if (want7) {
		int s7;
		if (ext & EXT_DOM7)
			s7 = 10;
		else if (ext & EXT_MAJ7)
			s7 = 11;
		else if (q == Q_DIM)
			s7 = inScale(P, root + 9) ? 9 : 10;
		else if (q == Q_MIN || q == Q_SUS2 || q == Q_SUS4)
			s7 = (!inScale(P, root + 10) && inScale(P, root + 11)) ? 11 : 10;
		else
			s7 = inScale(P, root + 11) ? 11 : 10;
		ch.iv[ch.n++] = s7;
	}
	if (want9 && q != Q_SUS2) {
		if (inScale(P, root + 2) || deg < 0 || (ext & EXT_9))
			ch.iv[ch.n++] = 14;
	}
	return ch;
}

int eventAt(const Pattern& pat, int step) {
	for (int e = 0; e < pat.nEvents; e++) {
		const ChordEvent& ev = pat.ev[e];
		if (step >= ev.start && step < ev.start + ev.len)
			return e;
	}
	if (pat.nEvents > 0 && pat.layoutLen > 0)
		for (int e = 0; e < pat.nEvents; e++) {
			int s = step % pat.layoutLen;
			if (s >= pat.ev[e].start && s < pat.ev[e].start + pat.ev[e].len)
				return e;
		}
	return 0;
}

bool isEventStart(const Pattern& pat, int step) {
	for (int e = 0; e < pat.nEvents; e++)
		if (pat.ev[e].start == step)
			return true;
	return false;
}

Chord resolveEvent(const Pattern& pat, const Params& P, int e) {
	if (pat.nEvents == 0 || pat.nProg == 0) {
		Chord ch;
		ch.n = 3;
		ch.iv[1] = 3;
		ch.iv[2] = 7;
		return ch;
	}
	e = clampi(e, 0, pat.nEvents - 1);
	const ChordSpec& cs = pat.prog[clampi(pat.ev[e].spec, 0, pat.nProg - 1)];
	int g = pat.chordGenre;
	float color = effColor(P, g);
	Chord ch = resolveSpec(cs, P, color);
	if (cs.quality != Q_MASK && pat.nEvents > 1 && !pat.customTimeline) {
		const ChordSpec& nx = pat.prog[clampi(pat.ev[(e + 1) % pat.nEvents].spec, 0, pat.nProg - 1)];
		float c = clampf(color + cs.extOff * 0.22f, 0.f, 1.f);
		float rs = cs.rs / 255.f;
		bool tonicStart = (e == 0 && cs.root == 0);
		if (!tonicStart && c > 0.62f && rs < (c - 0.62f) * 1.1f && nx.root != cs.root) {
			// secondary dominant (or tritone substitute) leading into the next chord
			Chord d;
			d.root = prof(g).jazzy ? mod12(nx.root + 1) : mod12(nx.root + 7);
			d.n = 4;
			d.iv[0] = 0;
			d.iv[1] = 4;
			d.iv[2] = 7;
			d.iv[3] = 10;
			if (c > 0.85f) {
				d.iv[4] = 14;
				d.n = 5;
			}
			return d;
		}
		if (!prof(g).jazzy && cs.root != 0 && c > 0.5f && rs > 0.6f && rs < 0.6f + (c - 0.5f) * 0.5f) {
			// modal interchange: flip major/minor third
			for (int i = 1; i < ch.n; i++) {
				if (ch.iv[i] == 3) {
					ch.iv[i] = 4;
					break;
				}
				if (ch.iv[i] == 4) {
					ch.iv[i] = 3;
					break;
				}
			}
		}
	}
	return ch;
}

Chord chordAt(const Pattern& pat, const Params& P, int step) {
	if (P.extChord && P.extMask)
		return chordFromMask(P.extRoot, P.extMask);
	return resolveEvent(pat, P, eventAt(pat, step));
}

Chord chordFromPcs(uint16_t pcMask, int lowestPc, int bassPc) {
	int best = -1;
	float bestScore = -1e9f;
	for (int r = 0; r < 12; r++) {
		if (!(pcMask & (1 << r)))
			continue;
		auto has = [&](int i) { return (pcMask >> mod12(r + i)) & 1; };
		float score = 0.f;
		if (has(3) || has(4))
			score += 2.f;
		if (has(7))
			score += 1.f;
		if (has(10) || has(11))
			score += 0.4f;
		if (has(9) && !has(10) && !has(11))
			score -= 0.6f; // a "6th" usually means another root (m7 inversion)
		if (has(1) || has(6))
			score -= 0.5f;
		if (r == mod12(lowestPc))
			score += 0.8f;
		if (bassPc >= 0 && r == mod12(bassPc))
			score += 2.5f;
		if (score > bestScore) {
			bestScore = score;
			best = r;
		}
	}
	if (best < 0)
		best = mod12(lowestPc);
	uint16_t m = 0;
	bool hasThird = ((pcMask >> mod12(best + 3)) & 1) || ((pcMask >> mod12(best + 4)) & 1);
	for (int i = 0; i < 12; i++) {
		if ((pcMask >> mod12(best + i)) & 1) {
			int iv = i;
			if (i == 2 && hasThird)
				iv = 14;
			m |= (uint16_t)(1 << iv);
		}
	}
	return chordFromMask(best, m);
}

std::string chordName(const Chord& ch, int key) {
	bool s[15] = {false};
	for (int i = 0; i < ch.n; i++) {
		int v = ch.iv[i];
		if (v >= 0 && v < 15)
			s[v] = true;
		if (v >= 15)
			s[v % 12] = true;
	}
	bool m3 = s[3], M3 = s[4], p5 = s[7], d5 = s[6], a5 = s[8];
	bool b7 = s[10], M7 = s[11], d7 = s[9];
	bool nine = s[14] || (s[2] && (m3 || M3));
	bool sus2 = s[2] && !m3 && !M3, sus4 = s[5] && !m3 && !M3;
	std::string q;
	if (m3 && d5 && !p5)
		q = b7 ? "m7b5" : (d7 ? "dim7" : "dim");
	else if (M3 && a5 && !p5)
		q = "aug";
	else if (m3) {
		q = "m";
		if (M7)
			q += nine ? "maj9" : "maj7";
		else if (b7)
			q += nine ? "9" : "7";
		else if (nine)
			q += "add9";
	}
	else if (M3) {
		if (M7)
			q = nine ? "maj9" : "maj7";
		else if (b7)
			q = nine ? "9" : "7";
		else if (nine)
			q = "add9";
	}
	else if (sus4)
		q = b7 ? "7sus4" : "sus4";
	else if (sus2)
		q = b7 ? "7sus2" : "sus2";
	else if (p5)
		q = "5";
	return std::string(NOTE_NAMES[mod12(key + ch.root)]) + q;
}

void layoutTimeline(Pattern& pat, int L) {
	L = clampi(L, 1, MAX_STEPS);
	if (pat.customTimeline) {
		if (pat.layoutLen == L || pat.layoutLen == 0 || pat.nEvents == 0)
			return;
		ChordEvent old[MAX_EVENTS];
		int nOld = pat.nEvents, oldLen = pat.layoutLen;
		std::memcpy(old, pat.ev, sizeof(old));
		int n = 0;
		for (int base = 0; base < L && n < MAX_EVENTS; base += oldLen) {
			for (int e = 0; e < nOld && n < MAX_EVENTS; e++) {
				int st = base + old[e].start;
				if (st >= L)
					break;
				ChordEvent ce = old[e];
				ce.start = (uint8_t)st;
				ce.len = (uint8_t)std::min<int>(old[e].len, L - st);
				pat.ev[n++] = ce;
			}
		}
		pat.nEvents = (uint8_t)n;
		pat.layoutLen = (uint8_t)L;
		return;
	}
	int H = std::max<int>(1, pat.harm);
	int nProg = std::max<int>(1, pat.nProg);
	// shorten the harmonic rhythm so the whole progression fits (not below one bar)
	while (L / H < nProg && H > 16)
		H /= 2;
	int nEff = std::max(1, std::min(nProg, L / H));
	int n = 0;
	for (int s = 0, k = 0; s < L && n < MAX_EVENTS; s += H, k++) {
		ChordEvent ce;
		ce.spec = (uint8_t)(k % nEff);
		ce.start = (uint8_t)s;
		ce.len = (uint8_t)std::min(H, L - s);
		ce.pad = 0;
		pat.ev[n++] = ce;
	}
	pat.nEvents = (uint8_t)n;
	pat.layoutLen = (uint8_t)L;
}

// ----------------------------------------------------------------- alternatives (display editing)
struct Alt {
	int root;     // -1: diatonic degree given in deg
	int deg;
	int quality;
	int ext;
};
static const Alt ALTS[] = {
	{-1, 0, 0, 0}, {-1, 1, 0, 0}, {-1, 2, 0, 0}, {-1, 3, 0, 0}, {-1, 4, 0, 0}, {-1, 5, 0, 0}, {-1, 6, 0, 0},
	{3, 0, Q_MAJ, 0}, {8, 0, Q_MAJ, 0}, {10, 0, Q_MAJ, 0}, {5, 0, Q_MAJ, 0}, {5, 0, Q_MIN, 0},
	{7, 0, Q_MAJ, EXT_7 | EXT_DOM7}, {1, 0, Q_MAJ, 0}, {0, 0, Q_SUS2, 0}, {0, 0, Q_SUS4, 0},
};
static const int NUM_ALTS = sizeof(ALTS) / sizeof(ALTS[0]);

int numAlternatives() { return NUM_ALTS; }

void applyAlternative(ChordSpec& cs, int idx, const Params& P) {
	idx = modn(idx, NUM_ALTS);
	const Alt& a = ALTS[idx];
	const int* sc = SCALES[P.scale].iv;
	if (a.root < 0) {
		cs.root = (int8_t)sc[a.deg];
		cs.quality = (uint8_t)diatonicQuality(sc, a.deg);
		cs.explicitQ = 0;
		cs.ext = 0;
	}
	else {
		cs.root = (int8_t)a.root;
		cs.quality = (uint8_t)a.quality;
		cs.explicitQ = 1;
		cs.ext = (uint8_t)a.ext;
	}
	cs.mask = 0;
}

std::string alternativeName(int idx, const Params& P, float color) {
	ChordSpec cs;
	std::memset(&cs, 0, sizeof(cs));
	cs.rc = 128;
	cs.rs = 255;
	applyAlternative(cs, idx, P);
	return chordName(resolveSpec(cs, P, color), P.key + P.transp);
}

int currentAlternative(const ChordSpec& cs, const Params& P) {
	const int* sc = SCALES[P.scale].iv;
	for (int i = 0; i < NUM_ALTS; i++) {
		const Alt& a = ALTS[i];
		if (a.root < 0) {
			if (sc[a.deg] == mod12(cs.root) && cs.quality != Q_SUS2 && cs.quality != Q_SUS4 &&
			    (!cs.explicitQ || cs.quality == diatonicQuality(sc, a.deg)))
				return i;
		}
		else if (a.root == mod12(cs.root) && a.quality == cs.quality)
			return i;
	}
	return -1;
}

// ================================================================= generation
void initPattern(Pattern& pat) {
	std::memset(&pat, 0, sizeof(pat));
	pat.magic = PATTERN_MAGIC;
	pat.version = PATTERN_VERSION;
	for (int s = 0; s < MAX_STEPS; s++) {
		pat.bass[s].absNote = NO_ABS;
		pat.mel[s].absNote = NO_ABS;
	}
	pat.nProg = 1;
	pat.prog[0].root = 0;
	pat.prog[0].quality = Q_MIN;
	pat.harm = 16;
	pat.melCenter = 4;
	layoutTimeline(pat, 64);
}

void generateChords(Pattern& pat, int g, Rng& rng, const Params& P) {
	const Genre& G = prof(g);
	const int* sc = SCALES[G.scale].iv;
	pat.harm = (uint8_t)G.harm[pickWeighted(rng, G.harmW, 3)];
	bool useTemplate;
	if (P.templateSel >= 0 && P.templateSel < G.nTemplates)
		useTemplate = true;
	else if (P.chordMode == CM_TEMPLATES)
		useTemplate = true;
	else if (P.chordMode == CM_FREE)
		useTemplate = false;
	else
		useTemplate = rng.uni() < 0.6f;
	if (G.nTemplates == 0)
		useTemplate = false;

	int n = 0;
	if (useTemplate) {
		int t = (P.templateSel >= 0 && P.templateSel < G.nTemplates) ? P.templateSel : rng.range(G.nTemplates);
		std::string s = G.templates[t];
		size_t pos = 0;
		while (pos < s.size() && n < MAX_PROG) {
			size_t e = s.find(' ', pos);
			if (e == std::string::npos)
				e = s.size();
			std::string tok = s.substr(pos, e - pos);
			if (!tok.empty() && parseChordToken(tok.c_str(), pat.prog[n]))
				n++;
			pos = e + 1;
		}
	}
	if (n == 0) {
		int H = pat.harm;
		if (H >= 64)
			n = 1 + rng.range(2);
		else if (H >= 32)
			n = 2 + rng.range(3);
		else {
			static const int NS[4] = {2, 3, 4, 4};
			n = NS[rng.range(4)];
		}
		int degs[MAX_PROG];
		degs[0] = rng.uni() < 0.75f ? 0 : pickWeighted(rng, G.degW, 7);
		for (int i = 1; i < n; i++) {
			float w[7];
			for (int d = 0; d < 7; d++) {
				w[d] = TRANS[degs[i - 1]][d] * G.degW[d];
				if (i == n - 1 && n > 1)
					w[d] *= 0.3f + TRANS[d][degs[0]];
			}
			degs[i] = pickWeighted(rng, w, 7);
		}
		for (int i = 0; i < n; i++) {
			ChordSpec& cs = pat.prog[i];
			std::memset(&cs, 0, sizeof(cs));
			cs.root = (int8_t)sc[degs[i]];
			cs.quality = (uint8_t)diatonicQuality(sc, degs[i]);
			cs.explicitQ = 0;
		}
	}
	for (int i = 0; i < n; i++) {
		pat.prog[i].rc = rng.byte();
		pat.prog[i].rs = rng.byte();
		pat.prog[i].extOff = 0;
	}
	pat.nProg = (uint8_t)n;
	pat.chordGenre = (int8_t)g;
	pat.customTimeline = 0;
}

static void genDrums(Pattern& pat, int g, Rng& rng) {
	const Genre& G = prof(g);
	uint8_t baseR[NUM_DRUMS][32], baseG[NUM_DRUMS][32];
	for (int v = 0; v < NUM_DRUMS; v++)
		for (int s = 0; s < 32; s++) {
			baseR[v][s] = rng.byte();
			baseG[v][s] = rng.byte();
		}
	int gl = G.drumLen; // 16 or 32: the base pattern repeats every gl steps
	for (int b = 0; b < MAX_BARS; b++) {
		float var = (b * 16 < gl) ? 0.f : ((b % 4 == 3) ? 0.3f : 0.1f);
		for (int v = 0; v < NUM_DRUMS; v++)
			for (int s = 0; s < 16; s++) {
				int idx = b * 16 + s;
				DrumStep& d = pat.drums[v][idx];
				d.r = (rng.uni() < var) ? rng.byte() : baseR[v][idx % gl];
				d.rg = (rng.uni() < var) ? rng.byte() : baseG[v][idx % gl];
				d.rf = rng.byte();
				d.vj = rng.byte();
				d.rr = rng.byte();
				d.fv = 0;
			}
	}
	// break chops: per bar a re-ordering of the 16 steps (repeat a beat, stutter, swap)
	for (int b = 0; b < MAX_BARS; b++) {
		uint8_t* m = pat.chop[b];
		for (int s = 0; s < 16; s++)
			m[s] = (uint8_t)s;
		int ops = 1 + rng.range(3);
		for (int o = 0; o < ops; o++) {
			int kind = rng.range(3);
			if (kind == 0) { // repeat one beat over another
				int from = rng.range(4), to = rng.range(4);
				for (int k = 0; k < 4; k++)
					m[to * 4 + k] = m[from * 4 + k];
			}
			else if (kind == 1) { // stutter: a 16th repeated 2-4 times
				int at = rng.range(14), len = 2 + rng.range(3);
				for (int k = 1; k < len && at + k < 16; k++)
					m[at + k] = m[at];
			}
			else { // swap two half-beats
				int a = rng.range(8) * 2, c = rng.range(8) * 2;
				for (int k = 0; k < 2; k++) {
					uint8_t t = m[a + k];
					m[a + k] = m[c + k];
					m[c + k] = t;
				}
			}
		}
		m[0] = 0; // the downbeat always stays
		pat.chopR[b] = rng.byte();
	}
	pat.trackGenre[T_DRUMS] = (int8_t)g;
	pat.fixed[T_DRUMS] = 0;
}

static void rollBassPitch(Rng& rng, const Genre& G, int s, NoteStep& n) {
	n.a1 = 0;
	n.b1 = 0;
	n.c1 = 0;
	if (s % 4 != 0 && rng.uni() < G.bassMove * 0.3f)
		n.c1 = 1;
	static const float W[5] = {0.35f, 0.2f, 0.2f, 0.1f, 0.15f};
	int k = pickWeighted(rng, W, 5);
	n.a2 = 0;
	n.b2 = 0;
	n.c2 = 0;
	switch (k) {
		case 0: n.a2 = 2; break;              // fifth
		case 1: n.c2 = 1; break;              // octave
		case 2: n.a2 = 1; break;              // third
		case 3: n.a2 = 3; break;              // seventh
		default: n.b2 = (int8_t)(rng.uni() < 0.5f ? -1 : 1); break; // passing
	}
}

static void genBass(Pattern& pat, int g, Rng& rng) {
	const Genre& G = prof(g);
	NoteStep base[16];
	for (int s = 0; s < 16; s++) {
		std::memset(&base[s], 0, sizeof(NoteStep));
		base[s].r = rng.byte();
		base[s].rm = rng.byte();
		rollBassPitch(rng, G, s, base[s]);
	}
	for (int b = 0; b < MAX_BARS; b++) {
		float var = (b == 0) ? 0.f : ((b % 4 == 3) ? 0.25f : 0.1f);
		for (int s = 0; s < 16; s++) {
			NoteStep& n = pat.bass[b * 16 + s];
			n = base[s];
			if (rng.uni() < var) {
				n.r = rng.byte();
				n.rm = rng.byte();
				rollBassPitch(rng, G, s, n);
			}
			n.vj = rng.byte();
			n.ra = rng.byte();
			n.rs = rng.byte();
			n.absNote = NO_ABS;
			n.force = 0;
			n.flen = 0;
			n.fvel = 0;
		}
	}
	pat.trackGenre[T_BASS] = (int8_t)g;
	pat.fixed[T_BASS] = 0;
}

static int melodyMask(const Genre& G) {
	if (!G.melPenta)
		return 0x7f;
	return SCALES[G.scale].iv[2] == 3 ? 0x5d : 0x37;
}

static int nudgeAllowed(int d, int mask, int dir) {
	for (int i = 0; i < 3; i++) {
		if (mask & (1 << modn(d, 7)))
			return d;
		d += dir >= 0 ? 1 : -1;
	}
	return d;
}

static int walkStep(Rng& rng, int cur, int s, int lo, int hi, int mask) {
	int next;
	if (s % 4 == 0) {
		// strong beat: prefer tonic-triad degrees near the current note
		static const int OFFS[5] = {0, -1, 1, -2, 2};
		next = cur;
		for (int i = 0; i < 5; i++) {
			int c = cur + OFFS[i];
			int m = modn(c, 7);
			if ((m == 0 || m == 2 || m == 4) && c >= lo && c <= hi) {
				next = c;
				break;
			}
		}
		if (rng.uni() < 0.25f)
			next += (rng.uni() < 0.5f) ? 2 : -2;
	}
	else {
		static const float W[9] = {0.04f, 0.08f, 0.12f, 0.28f, 0.06f, 0.28f, 0.12f, 0.08f, 0.04f};
		int k = pickWeighted(rng, W, 9) - 4; // -4..4
		next = cur + k;
	}
	if (next < lo)
		next = lo + (lo - next);
	if (next > hi)
		next = hi - (next - hi);
	next = clampi(next, lo, hi);
	return nudgeAllowed(next, mask, next >= cur ? 1 : -1);
}

static void genMelody(Pattern& pat, int g, Rng& rng) {
	const Genre& G = prof(g);
	int style = G.melStyle;
	for (int i = 0; i < MAX_STEPS; i++) {
		NoteStep& n = pat.mel[i];
		std::memset(&n, 0, sizeof(n));
		n.absNote = NO_ABS;
		n.vj = rng.byte();
		n.ra = rng.byte();
		n.rs = rng.byte();
	}
	if (style == MS_ACID) {
		// a second 303 line: chord tones in two octaves, repeated per bar with small variations
		pat.melMode = 1;
		static const float W[5] = {0.35f, 0.2f, 0.2f, 0.12f, 0.13f};
		int8_t tone[16], oct[16];
		uint8_t baseR[16];
		for (int s = 0; s < 16; s++) {
			int k = pickWeighted(rng, W, 5);
			tone[s] = (int8_t)(k == 1 ? 2 : (k == 3 ? 1 : (k == 4 ? 3 : 0)));
			oct[s] = (int8_t)(k == 2 ? 1 : (rng.uni() < 0.2f ? 1 : 0));
			baseR[s] = rng.byte();
		}
		for (int i = 0; i < MAX_STEPS; i++) {
			NoteStep& n = pat.mel[i];
			bool var = i >= 16 && rng.uni() < 0.1f;
			n.a1 = var ? (int8_t)rng.range(3) : tone[i % 16];
			n.c1 = var ? (int8_t)rng.range(2) : oct[i % 16];
			n.r = var ? rng.byte() : baseR[i % 16];
		}
	}
	else if (style == MS_ARP) {
		pat.melMode = 1;
		int type = rng.range(4);
		int len = rng.uni() < 0.5f ? 4 : 8;
		int tone[8], oct[8];
		for (int i = 0; i < len; i++) {
			int up = i % 3, upOct = (i / 3) % 2;
			switch (type) {
				case 0: tone[i] = up; oct[i] = upOct; break;
				case 1: tone[i] = 2 - up; oct[i] = 1 - upOct; break;
				case 2: {
					int j = i < len / 2 ? i : len - 1 - i;
					tone[i] = j % 3;
					oct[i] = (j / 3) % 2;
				} break;
				default: tone[i] = rng.range(4); oct[i] = rng.range(2); break;
			}
		}
		uint8_t baseR[16];
		for (int s = 0; s < 16; s++)
			baseR[s] = rng.byte();
		for (int i = 0; i < MAX_STEPS; i++) {
			NoteStep& n = pat.mel[i];
			n.a1 = (int8_t)tone[i % len];
			n.c1 = (int8_t)oct[i % len];
			n.r = (rng.uni() < 0.1f) ? rng.byte() : baseR[i % 16];
		}
	}
	else {
		pat.melMode = 0;
		int mask = melodyMask(G);
		int motifLen = (style == MS_RIFF) ? 16 : 32;
		int lo = 0, hi = (style == MS_RIFF) ? 7 : 10;
		static const int START[4] = {0, 2, 4, 7};
		int cur = nudgeAllowed(START[rng.range(4)], mask, 1);
		int deg[32];
		uint8_t rr[32];
		for (int s = 0; s < motifLen; s++) {
			cur = walkStep(rng, cur, s, lo, hi, mask);
			deg[s] = cur;
			rr[s] = rng.byte();
		}
		for (int i = 0; i < MAX_STEPS; i++) {
			pat.mel[i].a1 = (int8_t)deg[i % motifLen];
			pat.mel[i].r = rr[i % motifLen];
		}
		// variations: re-walk a segment from the preceding note
		auto rewalk = [&](int from, int to) {
			int c = pat.mel[std::max(0, from - 1)].a1;
			for (int i = from; i < to && i < MAX_STEPS; i++) {
				c = walkStep(rng, c, i % 16, lo, hi, mask);
				pat.mel[i].a1 = (int8_t)c;
				pat.mel[i].r = rng.byte();
			}
		};
		if (motifLen == 16) {
			rewalk(3 * 16 + 8, 4 * 16);       // bar 4 second half
			rewalk(7 * 16 + 4, 8 * 16);       // bar 8 bigger variation
		}
		else {
			rewalk(3 * 16 + 8, 4 * 16);       // answer phrase
			rewalk(6 * 16 + 8, 8 * 16);
		}
		pat.melCenter = (int8_t)((lo + hi) / 2);
	}
	pat.trackGenre[T_MELODY] = (int8_t)g;
	pat.fixed[T_MELODY] = 0;
}

static void genPads(Pattern& pat, int g, Rng& rng) {
	uint8_t base[16];
	for (int s = 0; s < 16; s++)
		base[s] = rng.byte();
	for (int i = 0; i < MAX_STEPS; i++) {
		PadStep& p = pat.pads[i];
		p.r = (i >= 16 && rng.uni() < 0.1f) ? rng.byte() : base[i % 16];
		p.vj = rng.byte();
		p.force = 0;
		p.flen = 0;
		p.fvel = 0;
		p.nNotes = 0;
	}
	pat.trackGenre[T_PADS] = (int8_t)g;
	pat.fixed[T_PADS] = 0;
}

void generateTrack(Pattern& pat, int track, int g, Rng& rng, const Params& P) {
	g = clampi(g, 0, numProfiles() - 1);
	switch (track) {
		case T_DRUMS: genDrums(pat, g, rng); break;
		case T_BASS: genBass(pat, g, rng); break;
		case T_MELODY: genMelody(pat, g, rng); break;
		case T_PADS: genPads(pat, g, rng); break;
		default: break;
	}
	(void)P;
}

void generate(Pattern& pat, int g, Rng& rng, const bool lock[NUM_TRACKS], bool chordLock, const Params& P) {
	g = clampi(g, 0, numProfiles() - 1);
	if (!chordLock)
		generateChords(pat, g, rng, P);
	for (int t = 0; t < NUM_TRACKS; t++)
		if (!lock[t])
			generateTrack(pat, t, g, rng, P);
	layoutTimeline(pat, P.lengthBars * STEPS_PER_BAR);
}

static int8_t shiftInScale(const Params& P, int absNote, int dir) {
	int n = absNote;
	for (int i = 0; i < 12; i++) {
		n += dir;
		if (inScale(P, n))
			break;
	}
	return (int8_t)clampi(n, -127, 127);
}

void mutate(Pattern& pat, float amount, Rng& rng, const bool lock[NUM_TRACKS], bool chordLock, const Params& P) {
	if (amount <= 0.005f)
		return;
	int L = clampi(P.lengthBars * STEPS_PER_BAR, 16, MAX_STEPS);
	if (!lock[T_DRUMS]) {
		const Genre& G = prof(pat.trackGenre[T_DRUMS]);
		int k = (int)std::round(amount * (4.f + 20.f * rng.uni()));
		for (int i = 0; i < k; i++) {
			int v = rng.range(NUM_DRUMS);
			if (!G.drumUsed[v])
				continue;
			int s = rng.range(L);
			DrumStep& d = pat.drums[v][s];
			if (pat.fixed[T_DRUMS]) {
				float p = G.pDrum[v][s % G.drumLen];
				if (d.fv == 0 && p >= 0.3f)
					d.fv = (uint8_t)(80 + rng.range(40));
				else if (d.fv > 0 && p < 0.5f)
					d.fv = 0;
			}
			else {
				d.r = rng.byte();
				d.rg = rng.byte();
			}
		}
	}
	if (!lock[T_BASS]) {
		const Genre& G = prof(pat.trackGenre[T_BASS]);
		int k = (int)std::round(amount * 8.f * rng.uni());
		for (int i = 0; i < k; i++) {
			int s = rng.range(L);
			NoteStep& n = pat.bass[s];
			if (pat.fixed[T_BASS]) {
				if (n.force == 1 && n.absNote != NO_ABS)
					n.absNote = shiftInScale(P, n.absNote, rng.uni() < 0.5f ? -1 : 1);
			}
			else {
				n.r = rng.byte();
				n.rm = rng.byte();
				if (rng.uni() < 0.5f)
					rollBassPitch(rng, G, s % 16, n);
			}
		}
	}
	if (!lock[T_MELODY]) {
		const Genre& G = prof(pat.trackGenre[T_MELODY]);
		int mask = melodyMask(G);
		int k = (int)std::round(amount * 6.f * rng.uni());
		for (int i = 0; i < k; i++) {
			int s = rng.range(L);
			NoteStep& n = pat.mel[s];
			if (pat.fixed[T_MELODY]) {
				if (n.force == 1 && n.absNote != NO_ABS)
					n.absNote = shiftInScale(P, n.absNote, rng.uni() < 0.5f ? -1 : 1);
			}
			else {
				n.r = rng.byte();
				if (pat.melMode == 0) {
					int d = n.a1 + (rng.uni() < 0.5f ? -1 : 1) * (1 + rng.range(2));
					n.a1 = (int8_t)nudgeAllowed(clampi(d, 0, 10), mask, 1);
				}
			}
		}
	}
	if (!lock[T_PADS] && !pat.fixed[T_PADS]) {
		int k = (int)std::round(amount * 3.f * rng.uni());
		for (int i = 0; i < k; i++)
			pat.pads[rng.range(L)].r = rng.byte();
	}
	if (!chordLock && !pat.customTimeline && pat.nProg > 1 && rng.uni() < amount * 0.3f) {
		const Genre& G = prof(pat.chordGenre);
		const int* sc = SCALES[G.scale].iv;
		int i = 1 + rng.range(pat.nProg - 1);
		int prevDeg = std::max(0, degreeOf(sc, pat.prog[i - 1].root));
		float w[7];
		for (int d = 0; d < 7; d++)
			w[d] = TRANS[prevDeg][d] * G.degW[d];
		int d = pickWeighted(rng, w, 7);
		ChordSpec& cs = pat.prog[i];
		cs.root = (int8_t)sc[d];
		cs.quality = (uint8_t)diatonicQuality(sc, d);
		cs.explicitQ = 0;
		cs.ext = 0;
		cs.rc = rng.byte();
		cs.rs = rng.byte();
	}
}

// ================================================================= evaluation
// is a part silenced by the genre's drop/break mode at this step?
static bool dropped(const Genre& G, const Params& P, int track, int voice, int step, int L) {
	if (G.dropMode == DM_NONE || L < 32 || !P.fillActive)
		return false;
	int lastBar = L - 16;
	switch (G.dropMode) {
		case DM_KICKBASS: // last two beats: kick and bass out
			return step >= L - 8 && ((track == T_DRUMS && voice == KICK) || track == T_BASS);
		case DM_BREAK: // last bar: the break drops out, bass and perc stay
			return step >= lastBar && track == T_DRUMS && voice != PERC;
		case DM_EIGHT: // second half of a long loop: melody every other bar, hats out in the last bar
			if (L < 64 || step < L / 2)
				return false;
			if (track == T_MELODY)
				return (step / 16) % 2 == 1;
			return track == T_DRUMS && (voice == CHAT || voice == OHAT) && step >= lastBar;
		default:
			return false;
	}
}

Hit drumHit(const Pattern& pat, const Params& P, int v, int step, int L, bool fill) {
	Hit h;
	if (step < 0 || step >= MAX_STEPS || v < 0 || v >= NUM_DRUMS)
		return h;
	int g = pat.trackGenre[T_DRUMS];
	const Genre& G = prof(g);
	int gs = step % 16;
	if (pat.fixed[T_DRUMS]) {
		const DrumStep& d = pat.drums[v][step];
		if (d.fv) {
			h.on = true;
			h.vel = d.fv / 127.f;
		}
		return h;
	}
	if (dropped(G, P, T_DRUMS, v, step, L))
		return h;
	float cx = effComplex(P, g);
	// break chops: kick, snare, rim and hats read from another step of the bar
	int src = step;
	int bar = step / 16;
	if (G.chop > 0.f && (v == KICK || v == SNARE || v == RIM || v == CHAT) &&
	    pat.chopR[bar] / 255.f < G.chop * (0.3f + cx * 1.2f))
		src = bar * 16 + pat.chop[bar][gs];
	const DrumStep& d = pat.drums[v][src];
	int sgs = src % 16;
	float p = G.pDrum[v][src % G.drumLen];
	float dn = P.density[T_DRUMS];
	float pe = 0.f;
	if (p >= 0.95f)
		pe = dn < 0.08f ? 0.f : 1.f;
	else if (p > 0.f)
		pe = densDrum(p * syncF(sgs, cx), dn);
	else if (dn > 0.5f && G.drumUsed[v] && v != KICK)
		pe = (dn - 0.5f) * 0.22f * syncF(sgs, cx);

	if (d.r / 255.f < pe) {
		h.on = true;
		float vel = 0.82f;
		if (gs % 4 == 0)
			vel = 1.f;
		else if (gs % 2 == 1)
			vel = 0.68f;
		if (v == CHAT)
			vel *= 0.85f;
		h.vel = vel + (d.vj / 255.f - 0.5f) * 0.18f;
	}
	else if ((v == SNARE || v == RIM || v == CHAT) && G.drumUsed[v] && P.ghost > 0.01f) {
		float gp = P.ghost * (v == CHAT ? 0.3f : (gs % 2 == 1 ? 0.45f : 0.12f));
		if (d.rg / 255.f < gp) {
			h.on = true;
			h.vel = 0.28f + d.vj / 255.f * 0.15f;
		}
	}
	// genre-specific fills over the end of the loop
	if (fill && L >= 16) {
		int fs = L - 8;
		if (step >= fs) {
			float pos = (step - fs) / 8.f;
			float r = d.rf / 255.f;
			switch (G.fillStyle) {
				case FS_TOMS:
					if (v == TOM && r < 0.45f + 0.3f * pos) {
						h.on = true;
						h.vel = 0.6f + 0.4f * pos;
					}
					if (v == SNARE && step >= L - 2 && r < 0.6f) {
						h.on = true;
						h.vel = 0.9f;
					}
					if (v == CHAT && step >= L - 4)
						h.on = false;
					break;
				case FS_CHOP:
					if (step >= L - 4 && (v == SNARE || v == KICK)) {
						h.on = r < (v == SNARE ? 0.7f : 0.35f);
						h.vel = 0.7f + 0.3f * pos;
						if (v == SNARE && h.on)
							h.ratchet = (d.rr & 1) ? 2 : 3;
					}
					break;
				case FS_DROP:
					// silence the last half bar, one big snare on the final 16th
					if (step >= L - 4) {
						h.on = (v == SNARE && step == L - 1);
						h.vel = 1.f;
					}
					break;
				case FS_ROLL:
					if (step >= L - 4 && v == CHAT) {
						h.on = true;
						h.vel = 0.5f + 0.5f * pos;
						h.ratchet = (d.rr & 1) ? 3 : 4;
					}
					if (v == SNARE && step == L - 2 && r < 0.5f) {
						h.on = true;
						h.vel = 0.8f;
					}
					break;
				default: // FS_SNARE
					if (v == SNARE || v == TOM) {
						float fp = (v == SNARE ? 0.25f + 0.55f * pos : 0.35f) * (0.6f + cx * 0.6f);
						if (r < fp) {
							h.on = true;
							h.vel = 0.55f + 0.45f * pos;
						}
					}
					if ((v == CHAT || v == OHAT) && step >= L - 2)
						h.on = false;
					break;
			}
		}
	}
	if (h.on && h.ratchet == 1 && G.pRoll[v][src % G.drumLen] && (d.rr >> 1) / 127.f < 0.3f + cx * 0.7f)
		h.ratchet = (d.rr & 1) ? 3 : 2;
	if (h.on && h.ratchet == 1 && v == CHAT) {
		float rp = G.rolls * cx * 0.55f + cx * cx * 0.06f;
		if (d.rr / 255.f < rp)
			h.ratchet = (G.rolls > 0.7f && (d.rr & 2)) ? 4 : ((d.rr & 1) ? 3 : 2);
	}
	if (h.on && G.humanize > 0.f && (v == SNARE || v == CLAP || v == RIM || v == CHAT || v == OHAT))
		h.delay = G.humanize * 0.02f * (d.vj / 255.f);
	h.vel = clampf(h.vel, 0.1f, 1.f);
	return h;
}

void drumStep(const Pattern& pat, const Params& P, int step, int L, bool fill, Hit out[NUM_DRUMS]) {
	for (int v = 0; v < NUM_DRUMS; v++)
		out[v] = drumHit(pat, P, v, step, L, fill);
	if (out[OHAT].on)
		out[CHAT].on = false;
}

static int bassIndex(const Genre& G, int step) {
	return (G.bassCycle > 0 && G.bassCycle != 16) ? step % G.bassCycle : step;
}

static bool bassActive(const Pattern& pat, const Params& P, int step, int L) {
	int g = pat.trackGenre[T_BASS];
	const Genre& G = prof(g);
	int bi = pat.fixed[T_BASS] ? step : bassIndex(G, step);
	const NoteStep& n = pat.bass[bi];
	if (pat.fixed[T_BASS])
		return n.force == 1;
	if (n.force == 1)
		return true;
	if (n.force == 2)
		return false;
	if (dropped(G, P, T_BASS, 0, step, L))
		return false;
	int gs = bi % 16;
	float cx = effComplex(P, g);
	float p = G.pBass[gs];
	if (p < 0.95f)
		p *= syncF(gs, cx);
	if (G.bassKick != 0) {
		bool k = drumHit(pat, P, KICK, step, L, false).on;
		if (G.bassKick > 0)
			p = k ? std::min(1.f, p * 1.5f + 0.25f) : p * 0.55f;
		else
			p = k ? p * 0.1f : p;
	}
	if (p > 0.f && G.bassKick >= 0 && isEventStart(pat, step))
		p = std::max(p, 0.96f);
	return n.r / 255.f < densTonal(p, P.density[T_BASS]);
}

static int bassPitch(const Chord& ch, const Params& P, int tone, int b, int c) {
	int keyPc = mod12(P.key + P.transp);
	int base = 36 + keyPc;
	if (base > 41)
		base -= 12;
	int iv = 0;
	auto find = [&](int a, int bb, int cc, int dflt) {
		for (int i = 1; i < ch.n; i++) {
			int x = ch.iv[i] % 12;
			if (x == a || x == bb || x == cc)
				return x;
		}
		return dflt;
	};
	switch (tone) {
		case 1: iv = find(3, 4, 2, 0); break;
		case 2: iv = find(7, 6, 8, 7); break;
		case 3: iv = find(10, 11, 9, 12); break;
		default: iv = 0; break;
	}
	int n = base + ch.root + iv;
	if (b != 0) {
		int dir = b > 0 ? 1 : -1;
		for (int k = 0; k < std::abs(b); k++) {
			for (int i = 0; i < 3; i++) {
				n += dir;
				if (inScale(P, n - keyPc))
					break;
			}
		}
	}
	while (n > 55)
		n -= 12;
	while (n < 28)
		n += 12;
	return n + 12 * c;
}

NoteEv bassAt(const Pattern& pat, const Params& P, int step, int L) {
	NoteEv e;
	if (step < 0 || step >= MAX_STEPS)
		return e;
	if (!bassActive(pat, P, step, L))
		return e;
	int g = pat.trackGenre[T_BASS];
	const Genre& G = prof(g);
	const NoteStep& n = pat.bass[pat.fixed[T_BASS] ? step : bassIndex(G, step)];
	int gs = step % 16;
	float cx = effComplex(P, g);
	e.on = true;
	float mv = clampf(G.bassMove + (P.move - 0.5f) * 1.2f, 0.f, 1.f);
	if (pat.fixed[T_BASS] || n.flen) {
		e.len = std::max(1, (int)n.flen);
		e.gate = 0.95f;
	}
	else {
		int cap = G.bassLen == 0 ? 1 : (G.bassLen == 1 ? 2 : 16);
		int k = 1;
		while (k < 16 && step + k < L && !bassActive(pat, P, step + k, L))
			k++;
		bool next = step + k < L && k <= 4;
		// slide into the next note: MOVE scales the genre's slide amount
		float sp = G.slide * (0.4f + 1.2f * mv);
		if (next && sp > 0.f && n.rs / 255.f < sp) {
			e.slide = true;
			e.len = k;
			e.gate = 1.05f;
		}
		else {
			e.len = std::min(k, cap);
			e.gate = G.bassLen == 0 ? 0.45f : (G.bassLen == 1 ? 0.85f : 0.97f);
		}
	}
	if (n.absNote != NO_ABS)
		e.pitch = n.absNote + P.key + P.transp + 12 * P.oct[0];
	else {
		Chord ch = chordAt(pat, P, step);
		bool alt = n.rm / 255.f < mv * 0.8f;
		if (gs == 0 || isEventStart(pat, step))
			alt = false; // downbeats and chord changes stay on the root
		e.pitch = alt ? bassPitch(ch, P, n.a2, n.b2, n.c2) : bassPitch(ch, P, n.a1, n.b1, n.c1);
		e.pitch += 12 * P.oct[0];
	}
	if (n.fvel)
		e.vel = n.fvel / 127.f;
	else
		e.vel = clampf(0.78f + (gs % 4 == 0 ? 0.12f : 0.f) + (n.vj / 255.f - 0.5f) * 0.14f, 0.f, 1.f);
	e.acc = n.fvel ? (n.fvel > 100) : (n.ra / 255.f < 0.18f + cx * 0.15f);
	e.pitch = clampi(e.pitch, 0, 127);
	return e;
}

static bool melActive(const Pattern& pat, const Params& P, int step, int L) {
	const NoteStep& n = pat.mel[step];
	if (pat.fixed[T_MELODY])
		return n.force == 1;
	if (n.force)
		return n.force == 1;
	int g = pat.trackGenre[T_MELODY];
	const Genre& G = prof(g);
	if (dropped(G, P, T_MELODY, 0, step, L))
		return false;
	int gs = step % 16;
	float p = G.pMel[gs];
	if (p < 0.95f)
		p *= syncF(gs, effComplex(P, g));
	return n.r / 255.f < densTonal(p, P.density[T_MELODY]);
}

static int nearestChordTone(const Chord& ch, int keyPc, int pitch) {
	for (int d = 0; d <= 6; d++) {
		for (int sgn = -1; sgn <= 1; sgn += 2) {
			int p = pitch + d * sgn;
			int pc = mod12(p - keyPc - ch.root);
			for (int i = 0; i < ch.n; i++)
				if (mod12(ch.iv[i]) == pc)
					return p;
			if (d == 0)
				break;
		}
	}
	return pitch;
}

NoteEv melAt(const Pattern& pat, const Params& P, int step, int L) {
	NoteEv e;
	if (step < 0 || step >= MAX_STEPS)
		return e;
	if (!melActive(pat, P, step, L))
		return e;
	const NoteStep& n = pat.mel[step];
	int g = pat.trackGenre[T_MELODY];
	const Genre& G = prof(g);
	int gs = step % 16;
	float cx = effComplex(P, g);
	e.on = true;
	if (pat.fixed[T_MELODY] || n.flen) {
		e.len = std::max(1, (int)n.flen);
		e.gate = 0.95f;
	}
	else {
		static const int CAP[5] = {2, 4, 1, 8, 1};
		static const float GATE[5] = {0.55f, 0.92f, 0.6f, 0.97f, 0.5f};
		int st = clampi(G.melStyle, 0, 4);
		int k = 1;
		while (k < 8 && step + k < L && !melActive(pat, P, step + k, L))
			k++;
		bool next = step + k < L && k <= 4;
		float sp = G.melSlide * (0.4f + 1.2f * effComplex(P, g));
		if (next && sp > 0.f && n.rs / 255.f < sp) {
			e.slide = true;
			e.len = k;
			e.gate = 1.05f;
		}
		else {
			e.len = std::min(k, CAP[st]);
			e.gate = GATE[st];
		}
	}
	int keyPc = mod12(P.key + P.transp);
	int base = 60 + keyPc;
	if (base > 66)
		base -= 12;
	if (n.absNote != NO_ABS)
		e.pitch = n.absNote + P.key + P.transp + 12 * P.oct[1];
	else {
		Chord ch = chordAt(pat, P, step);
		if (pat.melMode == 1) {
			int r = ch.root > 6 ? ch.root - 12 : ch.root;
			int t = std::max(0, (int)n.a1);
			int cnt = std::max(1, ch.n);
			e.pitch = base + r + ch.iv[t % cnt] + 12 * (t / cnt) + 12 * n.c1;
		}
		else {
			float k = 0.35f + P.range * 1.3f;
			int c = pat.melCenter;
			int d = c + (int)std::lround((n.a1 - c) * k);
			int mask = SCALES[P.scale].pentaMask;
			if (mask == 0x7f && G.melPenta)
				mask = SCALES[P.scale].iv[2] == 3 ? 0x5d : 0x37;
			d = nudgeAllowed(d, mask, 1);
			e.pitch = base + degreeSemi(SCALES[P.scale].iv, d);
			if (gs % 4 == 0)
				e.pitch = nearestChordTone(ch, keyPc, e.pitch);
		}
		e.pitch += 12 * P.oct[1];
	}
	if (n.fvel)
		e.vel = n.fvel / 127.f;
	else
		e.vel = clampf(0.74f + (gs % 4 == 0 ? 0.1f : 0.f) + (n.vj / 255.f - 0.5f) * 0.16f, 0.f, 1.f);
	e.acc = n.fvel ? (n.fvel > 100) : (n.ra / 255.f < 0.15f + cx * 0.15f);
	e.pitch = clampi(e.pitch, 0, 127);
	return e;
}

static bool padActive(const Pattern& pat, const Params& P, int step) {
	const PadStep& ps = pat.pads[step];
	if (pat.fixed[T_PADS])
		return ps.force == 1;
	int g = pat.trackGenre[T_PADS];
	const Genre& G = prof(g);
	float d = P.density[T_PADS];
	// padBase: presence at DENSITY centre; above the centre DENSITY brings pads in even when the genre has none
	float base = G.padBase;
	float pres = d >= 0.5f ? clampf(base + (d - 0.5f) * 2.f, 0.f, 1.f) : base * d * 2.f;
	if (G.padStyle == PS_SUSTAIN) {
		if (!isEventStart(pat, step))
			return false;
		return ps.r / 255.f < pres;
	}
	int gs = step % 16;
	float p = G.pPad[gs];
	if (p < 0.95f)
		p *= syncF(gs, effComplex(P, g));
	float dd = base > 0.f ? d : (d - 0.5f);
	if (dd <= 0.f)
		return false;
	return ps.r / 255.f < densTonal(p, base > 0.f ? d : dd * 2.f);
}

static void voiceChord(const Chord& ch, const Params& P, int prev[4], bool& hasPrev, int out[4]) {
	int keyPc = mod12(P.key + P.transp);
	float v = P.voicing;
	// pick chord tones by priority
	int third = -1, fifth = -1, sev = -1, nine = -1;
	for (int i = 1; i < ch.n; i++) {
		int x = ch.iv[i];
		if (x == 2 || x == 3 || x == 4 || x == 5)
			third = x;
		else if (x == 6 || x == 7 || x == 8)
			fifth = x;
		else if (x == 9 || x == 10 || x == 11)
			sev = x;
		else if (x == 14 || x == 13)
			nine = x % 12;
	}
	int tones[4];
	int n = 0;
	tones[n++] = 0;
	if (third >= 0)
		tones[n++] = third;
	if (v < 0.33f) {
		if (fifth >= 0)
			tones[n++] = fifth;
	}
	else if (v < 0.66f) {
		if (fifth >= 0)
			tones[n++] = fifth;
		if (sev >= 0)
			tones[n++] = sev;
		else if (nine >= 0)
			tones[n++] = nine;
	}
	else {
		if (sev >= 0)
			tones[n++] = sev;
		else if (fifth >= 0)
			tones[n++] = fifth;
		if (nine >= 0)
			tones[n++] = nine;
		else if (fifth >= 0 && n < 4)
			tones[n++] = fifth;
	}
	while (n < 3)
		tones[n++] = 7;
	int target = 60 + 12 * P.oct[2];
	int best[4] = {0, 0, 0, 0};
	int bestN = n;
	float bestCost = 1e9f;
	for (int inv = 0; inv < n; inv++) {
		int cand[4];
		int last = -100;
		for (int k = 0; k < n; k++) {
			int t = tones[(inv + k) % n];
			int pitch = keyPc + ch.root + t;
			while (pitch <= last)
				pitch += 12;
			cand[k] = pitch;
			last = pitch;
		}
		if (v >= 0.66f && n >= 4) {
			// drop-2: second highest voice down an octave -> open voicing
			cand[n - 2] -= 12;
			std::sort(cand, cand + n);
		}
		for (int o = -1; o <= 7; o++) {
			int c2[4];
			float mean = 0.f;
			for (int k = 0; k < n; k++) {
				c2[k] = cand[k] + 12 * o;
				mean += c2[k];
			}
			mean /= n;
			float cost = std::fabs(mean - target) * 0.6f;
			for (int k = 1; k < n; k++)
				if (c2[k] - c2[k - 1] == 1)
					cost += 6.f; // avoid semitone clusters
			if (hasPrev) {
				for (int k = 0; k < n; k++) {
					int bd = 99;
					for (int j = 0; j < 4; j++)
						bd = std::min(bd, std::abs(c2[k] - prev[j]));
					cost += bd;
				}
			}
			if (cost < bestCost) {
				bestCost = cost;
				for (int k = 0; k < n; k++)
					best[k] = c2[k];
				bestN = n;
			}
		}
	}
	for (int k = 0; k < 4; k++) {
		if (k < bestN)
			out[k] = best[k];
		else
			out[k] = best[0] + 12; // double the lowest note an octave up
		out[k] = clampi(out[k], 0, 127);
		prev[k] = out[k];
	}
	hasPrev = true;
}

PadEv padAt(const Pattern& pat, const Params& P, int step, int L, int prev[4], bool& hasPrev) {
	PadEv e;
	if (step < 0 || step >= MAX_STEPS)
		return e;
	if (!padActive(pat, P, step))
		return e;
	const PadStep& ps = pat.pads[step];
	int g = pat.trackGenre[T_PADS];
	const Genre& G = prof(g);
	e.on = true;
	if (pat.fixed[T_PADS] || ps.flen) {
		e.len = std::max(1, (int)ps.flen);
		e.gate = 0.97f;
	}
	else if (G.padStyle == PS_SUSTAIN) {
		int ev = eventAt(pat, step);
		const ChordEvent& ce = pat.ev[ev];
		e.len = std::max(1, ce.start + ce.len - step);
		e.len = std::min(e.len, L - step);
		e.gate = 0.97f;
	}
	else if (G.padStyle == PS_STABS) {
		int k = 1;
		while (k < 2 && step + k < L && !padActive(pat, P, step + k))
			k++;
		e.len = k;
		e.gate = 0.7f;
	}
	else {
		e.len = 1;
		e.gate = 0.45f;
	}
	e.vel = ps.fvel ? ps.fvel / 127.f : clampf(0.7f + (ps.vj / 255.f - 0.5f) * 0.14f, 0.f, 1.f);
	if (pat.fixed[T_PADS] && ps.nNotes > 0) {
		for (int k = 0; k < 4; k++) {
			int idx = k < ps.nNotes ? k : 0;
			int n = ps.notes[idx] + P.key + P.transp + 12 * P.oct[2] + (k < ps.nNotes ? 0 : 12);
			e.notes[k] = clampi(n, 0, 127);
			prev[k] = e.notes[k];
		}
		hasPrev = true;
		return e;
	}
	Chord ch = chordAt(pat, P, step);
	voiceChord(ch, P, prev, hasPrev, e.notes);
	return e;
}

} // namespace genra
