// SPDX-License-Identifier: GPL-3.0-only
// GENRA Instruments: the instruments of the GENRA family as a plugin of their own (same brand "Genra" in Rack's
// browser). The knobs, the GENRA strip and the cable-free links come from the GENRA plugin's sources (../../src).
#pragma once
#include "../../src/plugin.hpp"

extern Model* modelKwasub;
extern Model* modelKwasubFx;
extern Model* modelGolana;
extern Model* modelGolanaFx;
extern Model* modelUmlauf;
extern Model* modelUmlaufFx;
extern Model* modelMonk;
extern Model* modelVoice;
extern Model* modelVoiceBend;
extern Model* modelAnimal;
// from GENRA Test (2.38)
extern Model* modelMath;
extern Model* modelKurbel;
extern Model* modelSwarm;
extern Model* modelLied;
extern Model* modelCanta;
extern Model* modelMiaou;
extern Model* modelSaurus;
extern Model* modelFumi;
extern Model* modelKessel;
extern Model* modelKesselFx;

/** A voltage as it goes into lround / floor / a parameter: a NaN or an infinite one (a broken module upstream) counts
 * as 0 V (2.37). */
inline float finiteOr0(float v) { return std::isfinite(v) ? v : 0.f; }
