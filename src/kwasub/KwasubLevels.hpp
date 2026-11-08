// SPDX-License-Identifier: GPL-3.0-only
// Generated from a level measurement of every factory preset (a 16th bass line at 120 BPM): the output level that
// makes the preset loud and even with the others (the limiter in Kwasub.cpp catches the peaks). Index: firmware preset.
#pragma once
namespace kwlevels {
static const float PRESET_GAIN[] = {
	1.346f,  // 0 DEEP PRESSURE
	1.453f,  // 1 SUB TIDE
	0.915f,  // 2 IRON DUB
	1.003f,  // 3 NIGHT BUS
	1.140f,  // 4 CONCRETE
	1.238f,  // 5 REESE CLASSIC
	1.124f,  // 6 WIDE REESE
	1.001f,  // 7 WOBBLE 01
	1.158f,  // 8 YOI TALKER
	0.777f,  // 9 NEURO GROWL
	1.096f,  // 10 METAL FM
	0.974f,  // 11 808 DROP
	2.557f,  // 12 DUB STAB
	0.987f,  // 13 TECHNO RUMBLE
	1.358f,  // 14 ACID LINE
	1.241f,  // 15 PHASE HORN
	1.128f,  // 16 SYNC RIPPER
	1.009f,  // 17 HOLLOW TUBE
	1.270f,  // 18 WUB MACHINE
	1.049f,  // 19 DIGI BITE
	0.893f,  // 20 ROLLER SUB
	0.982f,  // 21 HOOVER DNB
	0.910f,  // 22 STEPPA DUB
	1.167f,  // 23 LIQUID SUB
	0.939f,  // 24 ROLLING REESE
	1.016f,  // 25 AMEN SUB
	0.779f,  // 26 JUNGLE BOOM
	1.102f,  // 27 RIDDIM SQUARE
	0.633f,  // 28 TEAROUT
	1.306f,  // 29 BROSTEP SAW
	1.244f,  // 30 NEURO STAB
	1.247f,  // 31 SPEED BASS
	1.156f,  // 32 UKG ORGAN
	1.021f,  // 33 TRAP 808
	1.370f,  // 34 DEEP HOUSE
	0.984f,  // 35 HOUSE ORGAN
	1.076f,  // 36 BERLIN RUMBLE
	1.685f,  // 37 DUBTECH PULSE
	1.041f,  // 38 SKANK UNIT
	1.612f,  // 39 BREAKS HOOVER
	2.557f,  // 40 MINIMAL CLICK
	1.170f,  // 41 YOI RIDDIM
	2.139f,  // 42 INIT
	0.959f,  // 43 SYNC SWEEP
	1.191f,  // 44 PUMP HOUSE
	1.008f,  // 45 PHASE DUB
	1.234f,  // 46 TAPE TECHNO
	1.066f,  // 47 REESE PHASER
	0.875f,  // 48 TECHNO ROLLER
	0.873f,  // 49 WAVE MORPH
	1.868f,  // 50 PLATE STAB
	1.052f,  // 51 808 BOOMER
	0.926f,  // 52 808 DIST
	1.006f,  // 53 808 KNOCK
	0.743f,  // 54 ACID SQUELCH
	1.226f,  // 55 ACID SQUARE
	1.276f,  // 56 ACID FM ZAP
	1.091f,  // 57 PURE SUB
	1.204f,  // 58 WARM TRI SUB
	1.397f,  // 59 FM SUB GROWL
	1.072f,  // 60 CS BRASS BASS
	1.017f,  // 61 CS SQUELCH
	0.920f,  // 62 CS XMAS BASS
	1.381f,  // 63 CS DRIFT PAD
	1.119f,  // 64 ROLL GROWL
	1.222f,  // 65 TECH ROLL FM
	1.283f,  // 66 CLEAN PICK
	1.333f,  // 67 GLASS BASS
	0.909f,  // 68 BOUNCE SQUARE
	1.874f,  // 69 BOUNCY FM
	0.931f,  // 70 DEEP SUB 30
	1.141f,  // 71 SUB DRAWBAR
	1.085f,  // 72 SNARL GROWL
	1.375f,  // 73 TALK GROWL
	0.886f,  // 74 SCREECH RIP
	1.364f,  // 75 GOSPEL ORGAN
	1.093f,  // 76 FM STACK
	1.050f,  // 77 DX BASS
	1.194f,  // 78 808 SLIDE
	1.514f,  // 79 CS ROLLER
	0.869f,  // 80 VOWEL WOBBLE
	1.143f,  // 81 HOLLOW GROWL
	1.388f,  // 82 SUB GROWL DNB
	0.917f,  // 83 METAL YOY
	1.770f,  // 84 TALKBOX BASS
	1.378f,  // 85 REESE DRAWBAR
	1.049f,  // 86 HOLLOW ORGAN
	1.236f,  // 87 FLUTE SUB
	1.260f,  // 88 CHURCH BASS
	1.319f,  // 89 PERC ORGAN
	2.554f,  // 90 FM KNOCK
	0.963f,  // 91 FM ORGAN BASS
	1.011f,  // 92 NEURO STACK
	1.190f,  // 93 808 GROWL
	0.847f,  // 94 808 FOLDED
	1.211f,  // 95 CS VOX
	1.298f,  // 96 aCiD StUff
};
static const int NUM = (int) (sizeof(PRESET_GAIN) / sizeof(PRESET_GAIN[0]));
inline float gainOf(int i) { return i >= 0 && i < NUM ? PRESET_GAIN[i] : 1.f; }
}  // namespace kwlevels
