// SPDX-License-Identifier: GPL-3.0-only
// The VADER engine's doors as UMLAUF uses them (vader/, built into the plugin by the Makefile).
#pragma once
#include <cstdint>

extern "C" {
#include "../../vader/third_party/mpc-vst-plugins/wrapper/engine.h"
#include "../../vader/src/overlay.h"

void vader_render_f(void* inst, float* l, float* r, int frames);  // 44.1 kHz, frames <= 128
void* vader_view_bus(void* inst);                                 // overlay_t*: what the screen draws
void vader_set_corner_bus(void* inst, float (*bus)[128]);         // each corner's share, added per block (or NULL)
int vader_list_count(void* inst, int wav);                        // shape files (0) / samples (1) found
const char* vader_list_path(void* inst, int wav, int i);
int vader_voices_on(void* inst);
}

namespace vader {

static const int NSHAPE = 51, NMODE = 27, NENGINE = 16;  // engines: SYNTH, the 14 voices, WAV (a sample)
static const int SHAPE_USER = 15;
static const char* const SHAPES[NSHAPE] = {
    "CIRCLE", "FIGURE 8", "STAR", "SQUARE", "TRIANGLE", "LISSA 3:2", "LISSA 5:4", "ROSE 3", "ROSE 5", "SPIRAL",
    "HEART", "ZIGZAG", "SPIRO", "CUBE", "ICOSA", "USER", "N-GON", "BUTTERFLY", "SUPERFORMULA", "LISSAJOUS", "ROSE",
    "LIMACON", "CARDIOID", "SPIROGRAPH", "ARCHIMEDES", "FERMAT", "HYPERBOLIC", "LITUUS", "LOG SPIRAL", "GOLDEN SPIRAL",
    "EULER SPIRAL", "TORUS KNOT", "MOBIUS", "HARMONOGRAPH", "PI LISSAJOUS", "EPICYCLES", "PULSE", "ADSR PATH",
    "POLYHEDRA", "HILBERT", "KOCH", "FOURIER", "SQUEEZE", "DE BROGLIE", "COTES", "POINSOT", "ATOMIC", "MANDALA",
    "STACKED SPIRO", "TIMES TABLE", "TOY GEARS"};
static const char* const MODES[NMODE] = {
    "PATH", "CHASE", "RECAMAN", "METEOR RAIN", "KEPLER", "ROSETTE", "FIGURE 8", "LORENZ", "ROSSLER", "THOMAS",
    "AIZAWA", "PENDULUM", "BOUNCE", "RIPPLE", "MAGNET", "BOIDS", "PARTICLE WAVE", "OPTIMIZE", "TRANSFER ORBIT",
    "GRAVITY WELLS", "SERPENTINE DRAGON", "PARAMECIA", "MEDUSA", "FLATWORMS", "ETHEREAL RAY", "CLUMP SWARM", "THUNDER"};
static const char* const ENGINES[NENGINE] = {"SYNTH", "ANALOG", "TRIO",  "WAVE", "CHIP",  "MACRO",   "WHEEL", "VOICE",
                                             "NOISE", "TONE",   "METAL", "BAZZ", "DRONE", "TEXTURE", "FX",    "WAV"};
static const char* const ENGINE_INFO[NENGINE] = {
    "two overtone generators, blended", "analog oscillators, sync, PWM", "three oscillators", "wavetables (Plaits)",
    "8-bit chip voice", "macro oscillator (Braids)", "drawbar organ", "formant voice", "filtered noise",
    "LXR tone", "LXR metal", "BAZZ", "drone stack", "textures", "effect voice", "a sample (WAV)"};
static const char* const BLENDS[7] = {"MIX", "AM", "PM", "AM+PM", "PD", "RING", "FOLD"};
static const char* const FTYPES[5] = {"LP12", "LP24", "BP", "HP", "LADDER"};
static const char* const FPOS[3] = {"POST", "GEN 1", "GEN 2"};
static const char* const SMP_MODES[5] = {"ONE SHOT", "LOOP 1 BAR", "LOOP 2 BARS", "LOOP 3 BARS", "LOOP 4 BARS"};
static const char* const ARP_MODES[6] = {"UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER", "CHORD"};
static const char* const ARP_RATES[6] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const char* const DIVS[7] = {"4 BARS", "2 BARS", "1 BAR", "1/2", "1/4", "1/8", "1/16"};
static const char* const STEPS[4] = {"OFF", "ON", "ACCENT", "TIE"};

}  // namespace vader
