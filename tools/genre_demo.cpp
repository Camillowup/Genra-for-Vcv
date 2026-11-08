// GENRA v2.24 genre demos: 8 bars per new genre, rendered without Rack.
// Pattern: the real GENRA engine (generate + renderSong, so swing, ratchets and the bar-8 fill are GENRA's own).
// Bass: GRUND, melody: FUNKE, pads: SCHMELZ (sustained pads) or FUNKE poly (stabs / skanks). Drums: a small
// synthesized kit. Mastered to about -14 LUFS (BS.1770 K-weighting, gated), peak -1 dBFS, 48 kHz 16-bit stereo.
// Build: g++ -std=gnu++17 -O2 -Isrc tools/genre_demo.cpp src/Engine.cpp src/Genres.cpp src/Edit.cpp src/Store.cpp src/Midi.cpp -o gd && ./gd <dir> [n]
#include "Engine.hpp"
#include "grund/GrundPresets.hpp"
#include "funke/FunkePresets.hpp"
#include "schmelz/SchmelzPresets.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

static const float FS = 48000.f;

// ------------------------------------------------------------------ drum kit
struct Rnd {
	uint32_t s = 0x9e3779b9u;
	float next() {
		s ^= s << 13, s ^= s >> 17, s ^= s << 5;
		return (s >> 8) * (1.f / 8388608.f) * 2.f - 1.f;
	}
};
struct Kit {
	// per genre flavour
	float kickDecay = 0.35f, kickHi = 160.f, kickLo = 48.f, kickDrive = 1.f, hatLen = 1.f, tone = 1.f;
	int perc = 0; // 0 conga, 1 cowbell, 2 metal, 3 shaker
};
struct DrumVoice {
	int type;
	float t = 1e9f, vel = 0.f, ph = 0.f, ph2 = 0.f;
	float lp = 0.f, hp = 0.f, bp1 = 0.f, bp2 = 0.f;
	Rnd r;
	void trig(float v) { t = 0.f, vel = v, ph = 0.f, ph2 = 0.f; }
	float proc(const Kit& k) {
		if (t > 4.f)
			return 0.f;
		float dt = 1.f / FS, out = 0.f;
		switch (type) {
		case 0: { // kick
			float f = k.kickLo + (k.kickHi - k.kickLo) * std::exp(-t * 28.f);
			ph += f * dt;
			float env = std::exp(-t / k.kickDecay);
			float s = std::sin(2.f * (float)M_PI * ph) * env + 0.6f * std::exp(-t * 400.f) * r.next() * 0.3f;
			out = std::tanh(s * k.kickDrive) / std::tanh(std::max(1.f, k.kickDrive) * 0.9f) * 0.95f;
			break;
		}
		case 1: { // snare
			ph += 185.f * dt;
			float n = r.next();
			hp = 0.7f * hp + 0.3f * n;
			out = (0.5f * std::sin(2.f * (float)M_PI * ph) * std::exp(-t * 30.f) + 0.7f * (n - hp) * std::exp(-t * 16.f));
			break;
		}
		case 2: { // clap: three bursts, then a tail
			float n = r.next();
			bp1 += 0.35f * (n - bp1);
			float b = n - bp1;
			float e = 0.f;
			for (int i = 0; i < 3; i++) {
				float ti = t - i * 0.011f;
				if (ti >= 0.f && ti < 0.011f)
					e = std::max(e, std::exp(-ti * 180.f));
			}
			if (t > 0.022f)
				e = std::max(e, std::exp(-(t - 0.022f) * 14.f));
			out = b * e * 0.9f;
			break;
		}
		case 3: { // rim / clave
			ph += 1650.f * dt, ph2 += 820.f * dt;
			out = (std::sin(2.f * (float)M_PI * ph) * 0.5f + std::sin(2.f * (float)M_PI * ph2) * 0.5f) * std::exp(-t * 90.f);
			break;
		}
		case 4: case 5: { // hats
			float n = r.next();
			hp = 0.2f * hp + 0.8f * n;
			lp += 0.6f * ((n - hp) - lp);
			float d = (type == 4 ? 45.f : 6.f) / k.hatLen;
			out = lp * std::exp(-t * d) * 0.45f;
			break;
		}
		case 6: { // tom
			float f = 95.f * k.tone + 60.f * std::exp(-t * 20.f);
			ph += f * dt;
			out = std::sin(2.f * (float)M_PI * ph) * std::exp(-t * 7.f) * 0.8f;
			break;
		}
		case 7: { // perc
			if (k.perc == 0) { // conga
				float f = 330.f + 90.f * std::exp(-t * 60.f);
				ph += f * dt;
				out = std::sin(2.f * (float)M_PI * ph) * std::exp(-t * 14.f) * 0.6f;
			} else if (k.perc == 1) { // cowbell (808-like: two squares, band-passed)
				ph += 540.f * dt, ph2 += 800.f * dt;
				float s = ((ph - std::floor(ph)) < 0.5f ? 1.f : -1.f) + ((ph2 - std::floor(ph2)) < 0.5f ? 1.f : -1.f);
				bp1 += 0.25f * (s - bp1);
				bp2 += 0.05f * (bp1 - bp2);
				out = (bp1 - bp2) * (0.7f * std::exp(-t * 30.f) + 0.3f * std::exp(-t * 6.f)) * 0.5f;
			} else if (k.perc == 2) { // metal
				ph += 1230.f * dt, ph2 += 1747.f * dt;
				float s = std::sin(2.f * (float)M_PI * ph) * std::sin(2.f * (float)M_PI * ph2 * 1.41f);
				out = s * std::exp(-t * 3.f) * 0.45f;
			} else { // shaker
				float n = r.next();
				hp = 0.5f * hp + 0.5f * n;
				float e = std::min(1.f, t * 200.f) * std::exp(-t * 35.f);
				out = (n - hp) * e * 0.35f;
			}
			break;
		}
		}
		t += dt;
		return out * vel;
	}
};

// ------------------------------------------------------------------ voices for pads / melody
struct Alloc {
	struct V {
		int note = -1;
		long on = -1, off = -1;
		bool trig = false;
	} v[16];
	int n;
	explicit Alloc(int n_) : n(n_) {}
	int start(int note, long on, long off) {
		int best = 0;
		long bestOff = 1L << 60;
		for (int i = 0; i < n; i++) {
			if (v[i].off < on) { // free
				best = i;
				bestOff = -1;
				break;
			}
			if (v[i].off < bestOff)
				bestOff = v[i].off, best = i;
		}
		v[best].note = note, v[best].on = on, v[best].off = off, v[best].trig = true;
		return best;
	}
};

// ------------------------------------------------------------------ per-genre demo setup
struct Demo {
	int genre;
	const char* file;
	const char* bass;
	const char* mel;
	const char* pad;   // SCHMELZ preset, or FUNKE preset when padFunke
	bool padFunke;
	Kit kit;
	float lDrums, lBass, lMel, lPad;
	int key;
	unsigned seed;
};

static Kit kit(float kd, float hi, float lo, float drive, float hat, float tone, int perc) {
	Kit k;
	k.kickDecay = kd, k.kickHi = hi, k.kickLo = lo, k.kickDrive = drive, k.hatLen = hat, k.tone = tone, k.perc = perc;
	return k;
}

static const Demo DEMOS[22] = {
	{37, "01_progressive_house", "TECHNO ROLL", "HYPNOTIC BLIP", "TRANCE SWELL", false, kit(.32f, 160, 48, 1.4f, 1.f, 1.f, 3), 1.f, .55f, .40f, .45f, 9, 11},
	{38, "02_organic_house", "DEEP HOUSE", "KALIMBA", "AMBIENT CLOUD", false, kit(.30f, 140, 50, 1.f, 1.f, 1.2f, 0), 1.f, .55f, .55f, .40f, 2, 12},
	{39, "03_uk_funky", "2-STEP SUB", "GARAGE BELL", "M1 ORGAN", true, kit(.28f, 170, 52, 1.2f, 1.f, 1.3f, 0), 1.f, .60f, .65f, .40f, 7, 13},
	{40, "04_bassline", "ORGAN BASS", "GRIME PULSE", "M1 ORGAN", true, kit(.28f, 170, 50, 1.3f, 1.f, 1.f, 3), 1.f, .70f, .40f, .40f, 4, 14},
	{41, "05_gqom", "RUMBLE", "DUBSTEP PLUCK", "DARK DRONE", false, kit(.45f, 150, 44, 2.0f, 1.f, 0.9f, 2), 1.f, .55f, .35f, .35f, 5, 15},
	{42, "06_big_beat", "GROWL", "ELECTRO ZAP", "STRING + BRASS", false, kit(.30f, 180, 50, 2.2f, 1.2f, 1.f, 3), 1.f, .60f, .45f, .35f, 4, 16},
	{43, "07_breaks", "REESE", "HYPNOTIC BLIP", "PHASED STRINGS", false, kit(.28f, 175, 50, 1.5f, 1.f, 1.f, 3), 1.f, .60f, .40f, .35f, 7, 17},
	{44, "08_jersey_club", "808 BOOM", "DRILL BELL", "PIANO HOUSE STAB", false, kit(.25f, 180, 55, 1.5f, 0.8f, 1.f, 3), 1.f, .55f, .40f, .70f, 1, 18},
	{45, "09_baltimore_club", "BOOM BAP", "GRIME PULSE", "PIANO HOUSE STAB", false, kit(.25f, 175, 52, 1.6f, 1.f, 1.f, 3), 1.f, .60f, .40f, .80f, 9, 19},
	{46, "10_phonk", "DRILL SLIDE", "DRILL BELL", "DRILL CHOIR", false, kit(.40f, 160, 42, 2.5f, 0.6f, 1.f, 1), 1.f, .70f, .45f, .30f, 1, 20},
	{47, "11_hardcore_gabber", "HARD RUMBLE", "SUPERSAW", "CINEMATIC", false, kit(.22f, 230, 55, 6.0f, 1.f, 1.f, 2), .75f, .80f, .90f, .60f, 9, 21},
	{48, "12_psytrance", "TECHNO ROLL", "ACID LEAD", "HYPNOTIC SWEEP", false, kit(.20f, 190, 52, 1.6f, 0.8f, 1.f, 3), 1.f, .65f, .45f, .45f, 4, 22},
	{49, "13_ebm_industrial", "INDUSTRIAL", "ELECTRO ZAP", "ANALOG BRASS", false, kit(.30f, 160, 48, 2.0f, 1.f, 1.f, 2), 1.f, .65f, .22f, .45f, 9, 23},
	{50, "14_dancehall", "STEPPERS SUB", "KALIMBA", "DUB SKANK", true, kit(.30f, 150, 48, 1.2f, 1.f, 1.f, 3), 1.f, .60f, .85f, .40f, 2, 24},
	{51, "15_roots_reggae", "DUB ROOTS", "DUB MELODICA", "DUB SKANK", true, kit(.30f, 120, 50, 1.f, 1.f, 1.f, 0), 1.f, .65f, .45f, .45f, 7, 25},
	{52, "16_moombahton", "GROWL", "GRIME PULSE", "FILTER HOUSE STAB", true, kit(.30f, 160, 48, 1.6f, 1.f, 1.f, 3), 1.f, .60f, .40f, .40f, 4, 26},
	{53, "17_cumbia_digital", "MOTOWN P-BASS", "FLUTE SOLO", "JAZZ ORGAN", true, kit(.25f, 140, 55, 1.f, 1.4f, 1.3f, 0), 1.f, .65f, .45f, .40f, 9, 27},
	{54, "18_bossa_nova", "JAZZ UPRIGHT", "FLUTE SOLO", "CLASSIC RHODES", true, kit(.30f, 110, 52, 1.f, 0.8f, 1.f, 3), .8f, .65f, .45f, .35f, 2, 28},
	{55, "19_drone", "ARCO DARK", "AMBIENT BELLS", "DARK DRONE", false, kit(.50f, 90, 40, 1.f, 1.f, 0.7f, 2), .7f, .30f, .35f, .70f, 2, 29},
	{56, "20_dark_ambient", "ARCO DARK", "AMBIENT BELLS", "GLASS CHOIR", false, kit(.60f, 90, 38, 1.f, 1.f, 0.6f, 2), .8f, .45f, .35f, .70f, 4, 30},
	{57, "21_ambient_techno", "DUB TECHNO", "SPACE RHODES", "DETROIT PAD", false, kit(.30f, 110, 48, 0.8f, 0.7f, 1.f, 3), .7f, .50f, .40f, .60f, 9, 31},
	{58, "22_berlin_school", "HYPNOTIC PLUCK", "80S POLY", "SOLINA DREAM", false, kit(.30f, 140, 48, 1.f, 1.f, 0.8f, 2), .7f, .55f, .45f, .55f, 9, 32},
};

template <typename P>
static int findPreset(const P* list, int n, const char* name) {
	for (int i = 0; i < n; i++)
		if (std::strcmp(list[i].name, name) == 0)
			return i;
	fprintf(stderr, "preset %s not found\n", name);
	exit(2);
}

// ------------------------------------------------------------------ loudness (BS.1770-4, 48 kHz)
struct Biquad {
	double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
	double p(double x) {
		double y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};
static double lufs(const std::vector<float>& L, const std::vector<float>& R) {
	Biquad s[2] = {{1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585},
	               {1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621}};
	Biquad sl[2] = {s[0], s[1]}, sr[2] = {s[0], s[1]};
	size_t n = L.size();
	std::vector<double> e(n);
	for (size_t i = 0; i < n; i++) {
		double l = sl[1].p(sl[0].p(L[i])), r = sr[1].p(sr[0].p(R[i]));
		e[i] = l * l + r * r;
	}
	size_t blk = (size_t)(0.4 * FS), hop = blk / 4;
	std::vector<double> z;
	for (size_t a = 0; a + blk <= n; a += hop) {
		double m = 0;
		for (size_t i = a; i < a + blk; i++)
			m += e[i];
		z.push_back(m / blk);
	}
	auto loud = [](double m) { return -0.691 + 10.0 * std::log10(std::max(m, 1e-12)); };
	double sum = 0;
	int c = 0;
	for (double m : z)
		if (loud(m) > -70.0)
			sum += m, c++;
	if (!c)
		return -100.0;
	double rel = loud(sum / c) - 10.0;
	sum = 0, c = 0;
	for (double m : z)
		if (loud(m) > -70.0 && loud(m) > rel)
			sum += m, c++;
	return c ? loud(sum / c) : -100.0;
}
// gain, then a look-ahead peak limiter (ceiling), oversampled peak check by linear interpolation x4
static void master(const std::vector<float>& inL, const std::vector<float>& inR, float gain, float ceiling,
                   std::vector<float>& L, std::vector<float>& R) {
	size_t n = inL.size();
	const int la = (int)(0.003f * FS);
	std::vector<float> need(n, 1.f);
	for (size_t i = 0; i < n; i++) {
		float pk = std::max(std::fabs(inL[i]), std::fabs(inR[i]));
		if (i + 1 < n)
			for (int k = 1; k < 4; k++) {
				float f = k / 4.f;
				pk = std::max(pk, std::fabs(inL[i] + (inL[i + 1] - inL[i]) * f));
				pk = std::max(pk, std::fabs(inR[i] + (inR[i + 1] - inR[i]) * f));
			}
		pk *= gain;
		need[i] = pk > ceiling ? ceiling / pk : 1.f;
	}
	// min over the look-ahead window, then smooth release
	std::vector<float> g(n, 1.f);
	float cur = 1.f;
	const float rel = std::exp(-1.f / (0.08f * FS));
	std::vector<float> win(n);
	for (size_t i = 0; i < n; i++) {
		float m = 1.f;
		for (int k = 0; k <= la && i + k < n; k++)
			m = std::min(m, need[i + k]);
		win[i] = m;
	}
	for (size_t i = 0; i < n; i++) {
		if (win[i] < cur)
			cur = cur + (win[i] - cur) * 0.5f; // fast attack within the look-ahead
		else
			cur = win[i] + (cur - win[i]) * rel;
		g[i] = std::min(cur, need[i]);
	}
	L.resize(n), R.resize(n);
	for (size_t i = 0; i < n; i++)
		L[i] = inL[i] * gain * g[i], R[i] = inR[i] * gain * g[i];
}
static void writeWav(const char* fn, const std::vector<float>& L, const std::vector<float>& R) {
	FILE* f = fopen(fn, "wb");
	if (!f) {
		perror(fn);
		exit(1);
	}
	uint32_t n = (uint32_t)L.size(), bytes = n * 4;
	auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
	auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
	fwrite("RIFF", 1, 4, f), u32(36 + bytes), fwrite("WAVEfmt ", 1, 8, f);
	u32(16), u16(1), u16(2), u32(48000), u32(48000 * 4), u16(4), u16(16);
	fwrite("data", 1, 4, f), u32(bytes);
	Rnd r;
	for (uint32_t i = 0; i < n; i++) {
		for (int c = 0; c < 2; c++) {
			float x = (c ? R[i] : L[i]) * 32767.f + (r.next() + r.next()) * 0.5f; // TPDF dither
			int v = (int)std::lround(std::max(-32768.f, std::min(32767.f, x)));
			int16_t s = (int16_t)v;
			fwrite(&s, 2, 1, f);
		}
	}
	fclose(f);
}

// ------------------------------------------------------------------ render one genre
static void render(const Demo& d, const char* dir) {
	using namespace genra;
	int pi = profileOf(d.genre, 0);
	const Genre& G = prof(pi);
	Params P;
	P.genre = pi;
	P.scale = G.scale;
	P.key = d.key;
	P.lengthBars = 4;
	P.fill = 0.5f;
	Pattern pat;
	initPattern(pat);
	Rng rng(d.seed * 7919u + 17u);
	bool lock[4] = {false, false, false, false};
	generate(pat, pi, rng, lock, false, P);
	std::vector<SongPart> parts(2);
	parts[0].pat = &pat, parts[0].P = P, parts[0].L = 64, parts[0].bars = 4, parts[0].fill = false;
	parts[1] = parts[0];
	parts[1].P.pass = 1;
	parts[1].fill = true;
	std::vector<MidiNote> notes = renderSong(parts);

	const float bpm = G.bpm;
	const double spt = 60.0 / bpm / 4.0 / 24.0 * FS; // samples per tick (24 ticks per 16th)
	const long len = (long)(8 * 16 * 24 * spt);
	const long total = len + (long)(2.5 * FS);

	// ---- engines
	int gb = findPreset(ggrund::PRESETS, ggrund::NUM_PRESETS, d.bass);
	ggrund::Params bp = ggrund::PRESETS[gb].params();
	auto bass = std::make_unique<ggrund::Engine>();
	bass->setSampleRate(FS);
	int fm = findPreset(gfunke::PRESETS, gfunke::NUM_PRESETS, d.mel);
	gfunke::Params mp = gfunke::PRESETS[fm].params();
	auto mel = std::make_unique<gfunke::Engine>();
	mel->setSampleRate(FS);
	// pads
	std::unique_ptr<gfunke::Engine> padF;
	gfunke::Params pfp;
	std::unique_ptr<gschmelz::StringsVoice[]> sv;
	std::unique_ptr<gschmelz::SoloVoice[]> so;
	std::unique_ptr<gschmelz::Bus> sbus;
	gschmelz::StringsParams ssp;
	gschmelz::SoloParams sop;
	gschmelz::BusParams sbp;
	const gschmelz::Preset* spr = nullptr;
	const int NPAD = 8;
	if (d.padFunke) {
		int fp = findPreset(gfunke::PRESETS, gfunke::NUM_PRESETS, d.pad);
		pfp = gfunke::PRESETS[fp].params();
		padF.reset(new gfunke::Engine);
		padF->setSampleRate(FS);
	} else {
		int sp = findPreset(gschmelz::PRESETS, gschmelz::NUM_PRESETS, d.pad);
		spr = &gschmelz::PRESETS[sp];
		spr->apply(ssp, sop, sbp);
		sv.reset(new gschmelz::StringsVoice[NPAD]);
		so.reset(new gschmelz::SoloVoice[NPAD]);
		sbus.reset(new gschmelz::Bus);
		sbus->setSampleRate(FS);
		for (int c = 0; c < NPAD; c++)
			sv[c].setSampleRate(FS), so[c].setSampleRate(FS), sv[c].seed(0x1234u * (c + 1)), so[c].seed(0x777u * (c + 3));
	}
	DrumVoice dv[NUM_DRUMS];
	for (int v = 0; v < NUM_DRUMS; v++)
		dv[v].type = v, dv[v].r.s = 0x1000u + v * 7777u;

	// ---- events in samples
	struct Ev {
		long on, off;
		int note;
		float vel;
	};
	std::vector<Ev> ev[4];
	for (const MidiNote& m : notes) {
		Ev e;
		e.on = (long)(m.startTick * spt);
		e.off = (long)((m.startTick + m.len) * spt);
		e.note = m.note;
		e.vel = m.vel / 127.f;
		ev[m.track].push_back(e);
	}
	for (auto& v : ev)
		std::sort(v.begin(), v.end(), [](const Ev& a, const Ev& b) { return a.on < b.on; });
	size_t di = 0, bi = 0, mi = 0, pi2 = 0;
	Ev curB{-1, -1, 48, 0.8f}, curM{-1, -1, 60, 0.8f};
	Alloc padA(d.padFunke ? 8 : NPAD);
	const float echoS = (float)(60.0 / bpm * 0.75);
	const float padOct = 0.f; // GENRA pad notes are already in a pad register
	std::vector<float> L(total), R(total);
	double st4[4] = {0, 0, 0, 0};
	gfunke::Channel pch[16];
	for (long i = 0; i < total; i++) {
		float dl = 0.f;
		while (di < ev[0].size() && ev[0][di].on <= i) {
			int v = gmDrumVoice(ev[0][di].note);
			if (v >= 0 && v < NUM_DRUMS)
				dv[v].trig(ev[0][di].vel);
			if (v == OHAT)
				; // open hat keeps ringing
			if (v == CHAT)
				dv[OHAT].t = std::max(dv[OHAT].t, 0.5f); // closed hat chokes the open one
			di++;
		}
		for (int v = 0; v < NUM_DRUMS; v++)
			dl += dv[v].proc(d.kit);
		// bass (mono, last note wins)
		while (bi < ev[1].size() && ev[1][bi].on <= i)
			curB = ev[1][bi++];
		ggrund::Input bin;
		bin.voct = (curB.note - 60) / 12.f;
		bin.gate = i >= curB.on && i < curB.off && i < len;
		bin.vel = curB.vel;
		bin.acc = curB.vel > 0.9f;
		float bl, br, benv;
		bass->process(bp, bin, bl, br, benv);
		// melody (mono)
		while (mi < ev[2].size() && ev[2][mi].on <= i)
			curM = ev[2][mi++];
		gfunke::Channel mc;
		mc.voct = (curM.note - 60) / 12.f;
		mc.gate = i >= curM.on && i < curM.off && i < len;
		mc.vel = curM.vel;
		float ml, mr, menv;
		mel->process(mp, &mc, 1, false, echoS, ml, mr, menv);
		// pads
		while (pi2 < ev[3].size() && ev[3][pi2].on <= i) {
			const Ev& e = ev[3][pi2++];
			padA.start(e.note, e.on, std::min<long>(e.off, len));
		}
		float pl = 0.f, pr = 0.f;
		if (padF) {
			for (int c = 0; c < padA.n; c++) {
				pch[c].voct = (padA.v[c].note - 60) / 12.f + padOct;
				pch[c].gate = padA.v[c].note >= 0 && i >= padA.v[c].on && i < padA.v[c].off;
				pch[c].vel = 0.75f;
			}
			float env;
			padF->process(pfp, pch, padA.n, false, echoS, pl, pr, env);
		} else {
			float an = sbus->animate(sbp);
			float s = 0.f, o = 0.f;
			gschmelz::StringsParams p = ssp;
			p.reg = gschmelz::clampf(spr->reg + an, 0.f, 1.f);
			for (int c = 0; c < NPAD; c++) {
				auto& v = padA.v[c];
				if (v.note < 0)
					continue;
				bool g = i >= v.on && i < v.off;
				float vo = (v.note - 60) / 12.f;
				float f = gschmelz::C4 * std::pow(2.f, vo);
				bool tr = v.trig;
				v.trig = false;
				s += sv[c].process(p, f, g, tr, 0.8f);
				if (gschmelz::stringsTakes(spr->split, vo) || gschmelz::soloTakes(spr->split, vo))
					o += so[c].process(sop, f, g || spr->hold, tr, 0.8f);
			}
			float st, so_;
			sbus->process(sbp, s, o, pl, pr, st, so_);
		}
		// mix (synth outputs are in volts, ~±5 V)
		const float k = d.padFunke ? 0.25f : 0.1f, kb = 0.2f, km = 0.25f;
		st4[0] += (double)(d.lDrums * 0.5f * dl) * (d.lDrums * 0.5f * dl);
		st4[1] += (double)(d.lBass * kb * bl) * (d.lBass * kb * bl);
		st4[2] += (double)(d.lMel * km * ml) * (d.lMel * km * ml);
		st4[3] += (double)(d.lPad * k * pl) * (d.lPad * k * pl);
		L[i] = d.lDrums * 0.5f * dl + d.lBass * kb * bl + d.lMel * km * ml + d.lPad * k * pl;
		R[i] = d.lDrums * 0.5f * dl + d.lBass * kb * br + d.lMel * km * mr + d.lPad * k * pr;
	}
	// fade the last 0.3 s of the tail
	long fade = (long)(0.3 * FS);
	for (long i = 0; i < fade; i++) {
		float g = 1.f - (float)i / fade;
		L[total - fade + i] *= g, R[total - fade + i] *= g;
	}
	// ---- master: -14 LUFS, -1 dBFS peak
	const float ceiling = std::pow(10.f, -1.05f / 20.f);
	double l0 = lufs(L, R);
	float gain = (float)std::pow(10.0, (-14.0 - l0) / 20.0);
	std::vector<float> oL, oR;
	for (int it = 0; it < 4; it++) {
		master(L, R, gain, ceiling, oL, oR);
		double l1 = lufs(oL, oR);
		if (std::fabs(l1 + 14.0) < 0.1)
			break;
		gain *= (float)std::pow(10.0, (-14.0 - l1) / 20.0);
	}
	double lf = lufs(oL, oR);
	float pk = 0.f;
	for (long i = 0; i < total; i++)
		pk = std::max(pk, std::max(std::fabs(oL[i]), std::fabs(oR[i])));
	printf("   stems dB  drums %6.1f  bass %6.1f  mel %6.1f  pad %6.1f   notes d%zu b%zu m%zu p%zu\n", 10 * std::log10(st4[0] / total + 1e-12),
	       10 * std::log10(st4[1] / total + 1e-12), 10 * std::log10(st4[2] / total + 1e-12), 10 * std::log10(st4[3] / total + 1e-12),
	       ev[0].size(), ev[1].size(), ev[2].size(), ev[3].size());
	char fn[512];
	snprintf(fn, sizeof fn, "%s/%s.wav", dir, d.file);
	writeWav(fn, oL, oR);
	printf("%-24s %-18s %-14s %5.0f BPM  %5.1fs  %6.2f LUFS  peak %6.2f dBFS  (pre %5.1f LUFS, gain %+5.1f dB)\n", d.file, G.name,
	       G.variant, bpm, total / FS, lf, 20.f * std::log10(pk), l0, 20.f * std::log10(gain));
}

int main(int argc, char** argv) {
	const char* dir = argc > 1 ? argv[1] : ".";
	int only = argc > 2 ? atoi(argv[2]) : 0;
	genra::initGenres();
	for (int i = 0; i < 22; i++)
		if (!only || only == i + 1)
			render(DEMOS[i], dir);
}
