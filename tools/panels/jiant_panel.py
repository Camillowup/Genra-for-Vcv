#!/usr/bin/env python3
"""The panels of the JIANT FM engines in the instruments' look (tools/panels/gpanel.py): VOICE, ANIMAL and BEND
(GENRA Instruments), DRUM-X (GENRA).

Usage:  jiant_panel.py real               -> instruments/res/JiantVoice.svg, JiantAnimal.svg, JiantVoiceBend.svg,
                                             res/JiantDrumX.svg and src/jiant/JiantLayout.hpp (Rack px, names as before)
        jiant_panel.py preview OUTDIR     -> OUTDIR/<Slug>.png with stand-in components
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gpanel  # noqa: E402

ENV = [[("small", "ATK", "A")], [("small", "DEC", "D")], [("small", "SUS", "S")], [("small", "REL", "R")]]
FX = [[("small", "DIST", "DIST")], [("small", "CHOR", "CHORUS")], [("small", "DLY", "DELAY")], [("small", "REV", "REVERB")]]
POLY3 = ("VOICE", "", ["POLY", "MONO", "LEGATO"])

# ---------------------------------------------------------------- VOICE (the names follow the language / SPELL)
VE = ["VOWEL", "VOWEL2", "TALK", "SHIFT", "BUZZ", "BREATH", "Q", "RAND"]
VOICE = dict(
    name="VOICE", sub="GENRA · SINGING VOICE", subshort="SINGING VOICE", acc="#c42f3d", preset="16  TALKING TOY", dh=21,
    screen=24,
    rows=[
        [dict(title="ENGINE", fieldw=24, fields=[[("LANGF", "EN", "LANG"), ("SPMF", "SING", "SPELL")]],
              cols=[[("large", "E0", VE[0], "E_TEXT0")], [("knob", "E1", VE[1], "E_TEXT1"), ("small", "E5", VE[5], "E_TEXT5")],
                    [("knob", "E2", VE[2], "E_TEXT2"), ("small", "E6", VE[6], "E_TEXT6")],
                    [("knob", "E3", VE[3], "E_TEXT3"), ("small", "E7", VE[7], "E_TEXT7")], [("knob", "E4", VE[4], "E_TEXT4")]]),
         dict(title="PLAY", field=("LYRF", "TEXT", "LYRIC"), cols=[[("button", "CALL", "CALL")], [("knob", "LEVEL", "LEVEL")]],
              lamps=[POLY3])],
        [dict(title="ENVELOPE", cols=ENV), dict(title="FX", cols=FX)],
    ],
    strip=[("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL")], sync=[("CALL_IN", "CALL"), ("WORD", "WORD ↺")],
    cvs=[[(f"E_IN{k}", f"~E_INL{k}:{VE[k]}") for k in range(8)]], outs=[("OUT_L", "L"), ("OUT_R", "R")])

# ---------------------------------------------------------------- ANIMAL (the names follow MODE)
AE = ["TOUCH", "COUPLE", "CAP", "HYST", "RATE", "GROUP", "FM"]
ANIMAL = dict(
    name="ANIMAL", sub="GENRA · CREATURE VOICE", subshort="CREATURE VOICE", acc="#4fe08a", preset="01  CMOS BIRD", dh=20,
    screen=24,
    rows=[
        [dict(title="ENGINE", cols=[[("large", "E0", AE[0], "E_TEXT0")], [("knob", "E1", AE[1], "E_TEXT1"), ("small", "E5", AE[5], "E_TEXT5")],
                                   [("knob", "E2", AE[2], "E_TEXT2"), ("small", "E6", AE[6], "E_TEXT6")],
                                   [("knob", "E3", AE[3], "E_TEXT3"), ("small", "E4", AE[4], "E_TEXT4")]]),
         dict(title="PLAY", field=("MODEF", "CMOS", "MODE"), cols=[[("button", "CALL", "CALL")], [("knob", "LEVEL", "LEVEL")]],
              lamps=[POLY3])],
        [dict(title="ENVELOPE", cols=ENV), dict(title="FX", cols=FX)],
    ],
    strip=[("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL")], sync=[("CALL_IN", "CALL")],
    cvs=[[(f"E_IN{k}", f"~E_INL{k}:{AE[k]}") for k in range(7)]], outs=[("OUT_L", "L"), ("OUT_R", "R")])

# ---------------------------------------------------------------- BEND (VOICE's expander: SPELL's circuit bending)
BEND = dict(
    name="BEND", sub="", acc="#c42f3d", preset=None, dh=41, minhp=10,
    rows=[[dict(title="BEND", cols=[[("button", "BEND", "BEND")], [("knob", "B0", "CLOCK")]])],
          [dict(title="GLITCH", cols=[[("knob", "B1", "CORRUPT")], [("knob", "B2", "STUTTER")], [("knob", "B3", "CRUSH")]])]],
    cvs=[[("ON", "ON"), ("B_IN0", "CLOCK"), ("B_IN1", "CORR")], [("B_IN2", "STUTTER"), ("B_IN3", "CRUSH")]],
    nosplit=True, jackrows=3)

# ---------------------------------------------------------------- DRUM-X (GENRA): eight lanes, a level and a trigger each
LANES = ["KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL"]
MACROS = ["TUNE", "TONE", "DECAY", "NOISE", "FM ENV", "FOLD", "DRIVE"]
DRUMX = dict(
    name="DRUM-X", sub="GENRA · 8-LANE DRUM SYNTH", subshort="8-LANE DRUMS", acc="#7b6cff", preset="02  909 CLUB", dh=14,
    screen=30, lanes=dict(names=LANES, pad="PAD", knob="LEVEL", jack="TRIG"),
    rows=[
        [dict(title="SOUND", dyntitle="SOUND_HEAD", cols=[[("large", "SND0", "PITCH")], [("knob", "SND1", "PMOD")],
                                                       [("knob", "SND2", "DECAY")], [("knob", "SND3", "NOISE")],
                                                       [("knob", "SND4", "COLOR")]]),
         dict(title="MASTER", cols=[[("large", "MORPH", "MORPH")], [("knob", "DLY", "DLY")], [("knob", "REV", "REV")],
                                    [("knob", "MASTER", "LVL")]])],
        [dict(title="KIT MACROS", cols=[[("small", f"MACRO{i}", MACROS[i])] for i in range(7)])],
    ],
    strip=[("GENRA_IN", "DRUMS"), ("VOCT", "V/OCT"), ("GATE", "GATE"), ("ACC", "ACC")],
    cvs=[[("MORPH_IN", "MORPH")] + [(f"MACRO_IN{i}", MACROS[i]) for i in range(7)]],
    outs=[(f"LANE_OUT{i}", LANES[i]) for i in range(8)] + [("OUT_L", "L"), ("OUT_R", "R")], nosplit=True)

PANELS = [("Voice", "voice", VOICE, "instruments/res"), ("DrumX", "drumx", DRUMX, "res"),
          ("Animal", "animal", ANIMAL, "instruments/res"), ("VoiceBend", "voicebend", BEND, "instruments/res")]

built = [(slug, ns, gpanel.build(spec), res) for slug, ns, spec, res in PANELS]
if len(sys.argv) > 1 and sys.argv[1] == "preview":
    out = sys.argv[2]
    for slug, ns, p, res in built:
        svg = os.path.join(out, slug + ".svg")
        open(svg, "w").write("\n".join(p.out + p.mock + gpanel.stand_ins(p) + ["</svg>"]))
        subprocess.run(["rsvg-convert", "-w", str(int(p.W * 15.5)), svg, "-o", svg[:-4] + ".png"], check=True)
        print(f"{slug}: {p.hp} HP -> {svg[:-4]}.png")
    sys.exit(0)
hdr = ["// Generated by tools/panels/jiant_panel.py - do not edit", "// Rack pixels (1 HP = 15 px).", "#pragma once",
       "namespace jlayout {"]
for slug, ns, p, res in built:
    open(os.path.join(gpanel.ROOT, res, f"Jiant{slug}.svg"), "w").write(gpanel.svg(p))
    hdr.append(gpanel.layout(p, "jlayout", ns))
    print(f"Jiant{slug}: {p.hp} HP -> {res}/Jiant{slug}.svg")
hdr += ["}  // namespace jlayout", ""]
open(os.path.join(gpanel.ROOT, "src", "jiant", "JiantLayout.hpp"), "w").write("\n".join(hdr))
print("written: src/jiant/JiantLayout.hpp")
