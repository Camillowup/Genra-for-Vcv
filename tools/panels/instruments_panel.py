#!/usr/bin/env python3
"""The panels of K-WASUB, GOLANA and UMLAUF and their expanders in the instruments' look (tools/panels/gpanel.py).

Usage:  instruments_panel.py real               -> instruments/res/<Slug>.svg + the layout headers in mm, names as before
        instruments_panel.py preview OUTDIR     -> OUTDIR/<Slug>.png with stand-in components
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gpanel  # noqa: E402

K, S, L, B, ST, M = "knob", "small", "large", "button", "step", "mini"
SPECS = {}

# ---------------------------------------------------------------- K-WASUB
def osc(t, p):
    return dict(title=t, field=(f"ENG_{p}", "ANALOG", "ENGINE"),
                cols=[[(K, f"{p}_P1", "SHAPE"), (S, f"{p}_LVL", "LEVEL")], [(K, f"{p}_P2", "COLOR"), (S, f"{p}_OCT", "OCT")],
                      [(K, f"{p}_P3", "EDGE"), (S, f"{p}_SEMI", "SEMI")], [(K, f"{p}_P4", "FOLD"), (S, f"{p}_FINE", "FINE")]])
SPECS["Kwasub"] = dict(
    name="K-WASUB", sub="GENRA · MODERN BASS", subshort="MODERN BASS", acc="#b5e61d", preset="12  ACID ROLLER",
    rows=[[osc("OSC A", "A"), osc("OSC B", "B"),
           dict(title="FILTER", field=("FTYPE", "LP 24", "TYPE"),
                cols=[[(L, "F_CUT", "CUTOFF")], [(K, "F_RES", "RES"), (S, "F_KEY", "KEY")], [(K, "F_DRV", "DRIVE"), (S, "FE_A", "F.ATK")],
                      [(K, "F_ENV", "ENV"), (S, "FE_D", "F.DEC")]])],
          [dict(title="SUB · X", cols=[[(M, "S_LVL", "SUB")], [(M, "N_LVL", "NOISE")], [(M, "X_AMT", "X-MOD")]]),
           dict(title="WOBBLE", cols=[[(S, "L_RATE", "RATE")], [(M, "L_DEP", "DEPTH")], [(M, "L_SHP", "SHAPE")], [(M, "L_DST", "DEST")]]),
           dict(title="AMP", cols=[[(M, "AE_A", "A")], [(M, "AE_D", "D")], [(M, "AE_S", "S")], [(M, "AE_R", "R")]]),
           dict(title="FX", cols=[[(M, "FX_SAT", "SAT")], [(M, "FX_DLY", "DELAY")], [(M, "FX_VERB", "VERB")], [(M, "FX_GLUE", "GLUE")]])]],
    strip=[("IN_VOCT", "V/OCT"), ("IN_GATE", "GATE"), ("IN_VEL", "VEL"), ("IN_ACC", "ACC"), ("IN_SLIDE", "SLIDE"), ("IN_MOD", "MOD")],
    sync=[("IN_CLOCK", "CLOCK"), ("IN_RESET", "RESET")],
    cvs=[[("IN_CUT", "CUT"), ("IN_RES", "RES"), ("IN_DRV", "DRIVE"), ("IN_FENV", "ENV")],
         [("IN_ASHP", "A SHP"), ("IN_ACOL", "A COL"), ("IN_BSHP", "B SHP"), ("IN_BCOL", "B COL")],
         [("IN_WOB", "WOB"), ("IN_DEP", "DEPTH")], [("IN_SUB", "SUB"), ("IN_NOISE", "NOISE"), ("IN_XMOD", "X-MOD")],
         [("IN_ATK", "ATK"), ("IN_REL", "REL")], [("IN_SAT", "SAT"), ("IN_DLY", "DELAY"), ("IN_VERB", "VERB")],
         [("IN_PRESS", "PRESS"), ("IN_BEND", "BEND"), ("IN_GLIDE", "GLIDE"), ("IN_PRESET", "PRESET")]],
    outs=[("OUT_ENV", "ENV"), ("OUT_LFO", "LFO"), ("OUT_L", "L"), ("OUT_R", "R")])

# ---------------------------------------------------------------- K-WASUB FX
def fxs(title, on, cols, lamps=None):
    d = dict(title=title, cols=[[(ST, on, "ON")]] + cols)
    if lamps:
        d["lamps"] = [lamps]
    return d
SPECS["KwasubFx"] = dict(
    name="K-WASUB FX", sub="GENRA · EXPANDER", subshort="EXPANDER", acc="#b5e61d", preset=None, display=False, link=True,
    rows=[[fxs("SATURATE", "DR_ON", [[(S, "DR_AMT", "AMOUNT")], [(S, "DR_MIX", "MIX")]], ("DR_TYPE", "", ["SOFT", "TUBE", "HARD", "FOLD"])),
           fxs("DELAY", "DL_ON", [[(S, "DL_TIME", "TIME")], [(S, "DL_FB", "FDBK")], [(S, "DL_TONE", "TONE")], [(S, "DL_MIX", "MIX")]],
               ("DL_TYPE", "", ["PING", "TAPE", "DIGI", "DUB"])),
           fxs("REVERB", "RV_ON", [[(S, "RV_SIZE", "SIZE")], [(S, "RV_DAMP", "DAMP")], [(S, "RV_MIX", "MIX")]],
               ("RV_TYPE", "", ["ROOM", "HALL", "PLATE", "SPRING"])),
           dict(title="MASTER", cols=[[(S, "MS_GLUE", "GLUE")], [(S, "MS_SUB", "SUB")], [(S, "MB_MONO", "MONO")]])],
          [fxs("CRUSH", "CR_ON", [[(S, "CR_BITS", "BITS")], [(S, "CR_RATE", "RATE")], [(S, "CR_MIX", "MIX")]]),
           fxs("CHORUS", "CH_ON", [[(S, "CH_RATE", "RATE")], [(S, "CH_DEP", "DEPTH")], [(S, "CH_MIX", "MIX")]]),
           fxs("PHASER", "PH_ON", [[(S, "PH_RATE", "RATE")], [(S, "PH_DEP", "DEPTH")], [(S, "PH_FB", "FDBK")], [(S, "PH_MIX", "MIX")]])],
          [fxs("COMPRESSOR", "CP_ON", [[(S, "CP_AMT", "AMOUNT")], [(S, "CP_ATK", "ATTACK")], [(S, "CP_REL", "RELEASE")], [(S, "CP_PUMP", "PUMP")]],
               ("CP_PRATE", "", ["1/4", "1/2", "1/1", "BAR"])),
           fxs("MULTIBAND", "OT_ON", [[(S, "OT_AMT", "AMOUNT")], [(S, "OT_LOW", "LOW")], [(S, "OT_HIGH", "HIGH")]]),
           fxs("EQ", "EQ_ON", [[(S, "EQ1_F", "LO F"), (S, "EQ1_G", "GAIN")], [(S, "EQ2_F", "FREQ"), (S, "EQ2_G", "GAIN")],
                               [(S, "EQ3_F", "FREQ"), (S, "EQ3_G", "GAIN")], [(S, "EQ4_F", "HI F"), (S, "EQ4_G", "GAIN")]])]],
    cvs=[[("IN_SAT", "SAT"), ("IN_CRUSH", "CRUSH"), ("IN_CHORUS", "CHORUS"), ("IN_PHASER", "PHASER")],
         [("IN_DTIME", "D.TIME"), ("IN_DFB", "D.FB"), ("IN_DMIX", "D.MIX")], [("IN_VSIZE", "V.SIZE"), ("IN_VMIX", "V.MIX")],
         [("IN_COMP", "COMP"), ("IN_PUMP", "PUMP"), ("IN_OTT", "OTT")], [("IN_EQLO", "EQ LO"), ("IN_EQHI", "EQ HI")],
         [("IN_GLUE", "GLUE"), ("IN_SUBB", "SUB+")]], jackrows=1)

# ---------------------------------------------------------------- GOLANA
SPECS["Golana"] = dict(
    name="GOLANA", sub="GENRA · ANALOG POLY", subshort="ANALOG POLY", acc="#3fc9e8", preset="01  WARM POLY KEYS", screen=34,
    rows=[[dict(title="OSCILLATORS", fieldw=15, fields=[[("F_O1WAVE", "SAW", ""), ("F_O2WAVE", "SAW", ""), ("F_O2MOD", "SYNC", "")]],
                cols=[[(S, "O1CTRL1", "CTRL 1"), (M, "O2SEMI", "SEMI")], [(S, "O1CTRL2", "CTRL 2"), (M, "O2TUNE", "TUNE")], [(S, "DRIFT", "DRIFT")]]),
           dict(title="MIXER", field=("F_SUBTYPE", "-1 SQR", "SUB"),
                cols=[[(S, "MIX1", "OSC 1"), (M, "SUB", "SUB")], [(S, "MIX2", "OSC 2"), (M, "FOLD", "FOLD")], [(S, "NOISE", "NOISE")]]),
           dict(title="FILTER", field=("F_FTYPE", "LPF24", "TYPE"),
                cols=[[(L, "CUTOFF", "CUTOFF")], [(S, "RESO", "RESO"), (M, "KEYTRK", "KEY TRK")], [(S, "EGINT", "EG INT"), (ST, "DIST", "DIST")]]),
           dict(title="ENVELOPES", cols=[[(M, "FA", "F.A"), (M, "AA", "A.A")], [(M, "FD", "F.D"), (M, "AD", "A.D")],
                                        [(M, "FS", "F.S"), (M, "AS", "A.S")], [(M, "FR", "F.R"), (M, "AR", "A.R")]])],
          [dict(title="LFO 1 · 2", fieldw=15, fields=[[("F_L1WAVE", "TRI", ""), ("F_L1KEY", "KEY", ""), ("F_L1SYNC", "OFF", "")],
                                                    [("F_L2WAVE", "SIN", ""), ("F_L2KEY", "KEY", ""), ("F_L2SYNC", "OFF", "")]],
                cols=[[(M, "L1FREQ", "FREQ 1")], [(M, "L2FREQ", "FREQ 2")]]),
           dict(title="PATCH", fieldw=15, fields=[[("F_P1SRC", "LFO 2", ""), ("F_P1DST", "PITCH", ""), ("F_P2SRC", "LFO 1", ""), ("F_P2DST", "CUTOFF", "")],
                                                [("F_P3SRC", "FEG", ""), ("F_P3DST", "PITCH", ""), ("F_P4SRC", "VEL", ""), ("F_P4DST", "AMP", "")]],
                cols=[[(M, "P1INT", "INT 1")], [(M, "P2INT", "INT 2")], [(M, "P3INT", "INT 3")], [(M, "P4INT", "INT 4")]]),
           dict(title="VOICE · ARP", fieldw=15, fields=[[("F_VMODE", "POLY 4", ""), ("F_ARPTYPE", "UP", ""), ("F_ARPRES", "1/16", "")]],
                cols=[[(M, "DETUNE", "DETUNE")], [(M, "PORTA", "PORTA")], [(ST, "ARPON", "ARP")], [(M, "ARPRANGE", "RANGE")], [(M, "ARPSWING", "SWING")]]),
           dict(title="FX · OUT", cols=[[(M, "MFXAMT", "MOD FX"), (M, "SPREAD", "SPREAD")], [(M, "DLYAMT", "DELAY"), (S, "LEVEL", "LEVEL")], [(M, "REVAMT", "REVERB")]])]],
    strip=[("IN_VOCT", "V/OCT"), ("IN_GATE", "GATE"), ("IN_VEL", "VEL"), ("IN_ACC", "ACC"), ("IN_SLIDE", "SLIDE"), ("IN_MODW", "MOD")],
    sync=[("IN_CLOCK", "CLOCK"), ("IN_RESET", "RESET")],
    cvs=[[("IN_CUTOFF", "CUT"), ("IN_RESO", "RESO"), ("IN_EGINT", "EG INT")], [("IN_CTRL1", "CTRL 1"), ("IN_FOLD", "FOLD")],
         [("IN_L1FREQ", "LFO 1"), ("IN_L1RETRIG", "RETRIG"), ("IN_L2FREQ", "LFO 2"), ("IN_L2RETRIG", "RETRIG")], [("IN_AFTER", "AFTER")]],
    outs=[("OUT_L", "L"), ("OUT_R", "R")], jackrows=1)

# ---------------------------------------------------------------- GOLANA FX
SPECS["GolanaFx"] = dict(
    name="GOLANA FX", sub="", acc="#3fc9e8", preset=None, minhp=14, display=False, link=True,
    rows=[[dict(title="MOD FX", field=("F_MFX", "CHORUS", "TYPE"), cols=[[(ST, "MFX_ON", "ON")], [(M, "CRATE", "RATE")], [(M, "CDEPTH", "DEPTH")]])],
          [dict(title="DELAY", fields=[[("F_DTYPE", "STEREO", "TYPE"), ("F_DTIME", "1/8", "TIME")]],
                cols=[[(ST, "DLY_ON", "ON")], [(M, "DFDBK", "FDBK")], [(M, "DCOLOR", "COLOR")], [(M, "DMIX", "MIX")]])],
          [dict(title="REVERB · CHAIN", fields=[[("F_RTYPE", "HALL", "TYPE"), ("F_ROUTE", "M>D>R", "CHAIN")]],
                cols=[[(ST, "REV_ON", "ON")], [(M, "RSIZE", "SIZE")], [(M, "RDAMP", "DAMP")]])]],
    cvs=[[("IN_CRATE", "RATE"), ("IN_CDEPTH", "DEPTH"), ("IN_DFDBK", "FDBK")], [("IN_DCOLOR", "COLOR"), ("IN_DMIX", "MIX"), ("IN_RSIZE", "SIZE")],
         [("IN_RDAMP", "DAMP"), ("IN_ROUTE", "CHAIN")]], jackrows=2)

# ---------------------------------------------------------------- UMLAUF
def corner(c, eng):
    return dict(title=f"CORNER {c}", screen=(f"CDISP_{c}", 7.0),
                cols=[[(M, f"ENG_{c}", "ENGINE"), (M, f"FTYPE_{c}", "FILTER")], [(M, f"C1_{c}", "1"), (M, f"FON_{c}", "ON")],
                      [(M, f"C2_{c}", "2"), (M, f"CUT_{c}", "CUTOFF")], [(M, f"C3_{c}", "3"), (M, f"RES_{c}", "RES")],
                      [(M, f"C4_{c}", "4"), (M, f"OCT_{c}", "OCT")], [(M, f"C5_{c}", "5"), (M, f"LVL_{c}", "LEVEL")]])
SPECS["Umlauf"] = dict(
    name="UMLAUF", sub="GENRA · ORBIT VECTOR SYNTH", subshort="ORBIT VECTOR", acc="#e0303a", preset="04  DARK ORBIT",
    rows=[[dict(title="ORBIT · MODE", cols=[[(L, "SHAPE", "SHAPE")], [(S, "MODE", "MODE")], [(S, "SPEED", "SPEED")], [(S, "SIZE", "SIZE")],
                                          [(S, "ROT", "ROTATE")], [(S, "SPIN", "3D SPIN")], [(S, "TILT", "3D TILT")], [(S, "SUBSIZE", "SUB SIZE")],
                                          [(S, "SUBSPD", "SUB SPD")], [(S, "OENV", "ENV>SIZE")]] +
                                         [[(S, f"M{i}", f"M{i}")] for i in range(1, 9)])],
          [corner("A", "SYNTH"), corner("B", "VOICE"), corner("C", "METAL"), corner("D", "WAV")]],
    strip=[("IN_VOCT", "V/OCT"), ("IN_GATE", "GATE"), ("IN_VEL", "VEL")], sync=[("IN_CLK", "CLOCK"), ("IN_RST", "RESET")],
    cvs=[[("IN_X", "X"), ("IN_Y", "Y"), ("IN_SHAPE", "SHAPE"), ("IN_MODE", "MODE"), ("IN_SPEED", "SPEED"), ("IN_SIZE", "SIZE"),
          ("IN_ROT", "ROT"), ("IN_SUB", "SUB"), ("IN_M1", "M1"), ("IN_M2", "M2"), ("IN_M3", "M3"), ("IN_M4", "M4"),
          ("IN_LVL_A", "LVL A"), ("IN_LVL_B", "LVL B"), ("IN_LVL_C", "LVL C"), ("IN_LVL_D", "LVL D"), ("IN_VOL", "VOL"),
          ("IN_CUT_A", "CUT A"), ("IN_CUT_B", "CUT B"), ("IN_CUT_C", "CUT C"), ("IN_CUT_D", "CUT D"),
          ("IN_C1_A", "C1 A"), ("IN_C1_B", "C1 B"), ("IN_C1_C", "C1 C"), ("IN_C1_D", "C1 D")]],
    outs=[("OUT_X", "X"), ("OUT_Y", "Y"), ("OUT_W_A", "W A"), ("OUT_W_B", "W B"), ("OUT_W_C", "W C"), ("OUT_W_D", "W D"),
          ("OUT_ENV", "ENV"), ("OUT_A", "A"), ("OUT_B", "B"), ("OUT_C", "C"), ("OUT_D", "D"), ("OUT_L", "L"), ("OUT_R", "R")])

# ---------------------------------------------------------------- UMLAUF FX
SPECS["UmlaufFx"] = dict(
    name="UMLAUF FX", sub="GENRA · ENV · VOICE · ARP · FX", subshort="EXPANDER", acc="#e0303a", preset=None, display=False, link=True,
    rows=[[dict(title="ENVELOPE", cols=[[(K, "ATK", "ATTACK")], [(K, "DEC", "DECAY")], [(K, "SUS", "SUSTAIN")], [(K, "REL", "RELEASE")]],
                lamps=[("UNISON", "UNISON", ["1", "2", "4", "8"])]),
           dict(title="VOICE", cols=[[(S, "DETUNE", "DETUNE")], [(S, "GLIDE", "GLIDE")], [(S, "TOCT", "OCTAVE")], [(S, "VELO", "VELOCITY")],
                                    [(S, "VSPD", "VIB RATE")], [(S, "VDEP", "VIBRATO")], [(S, "TSPD", "TREM RATE")], [(S, "TDEP", "TREMOLO")]],
                lamps=[("VOICES", "VOICES", ["8", "16"])])],
          [dict(title="ARPEGGIATOR", cols=[[(ST, "ARPON", "ON")], [(ST, "LATCH", "LATCH")]] +
                [[(ST, f"STEP{i}", str(i))] for i in range(1, 17)] +
                [[(M, "ARATE", "RATE")], [(M, "AOCT", "OCT")], [(M, "ALEN", "LENGTH")], [(M, "ASTEPS", "STEPS")], [(M, "AHUM", "HUMAN")]],
                lamps=[("AMODE", "", ["UP", "DOWN", "U/D", "RND", "ORDER", "CHORD"])])],
          [dict(title="FX · MASTER", cols=[[(S, "DRIVE", "DRIVE")], [(S, "CHORUS", "CHORUS")], [(S, "CHSPD", "CH RATE")], [(S, "DELAY", "DELAY")],
                                          [(S, "DTIME", "TIME")], [(S, "DFB", "FEEDBACK")], [(S, "REVERB", "REVERB")], [(S, "RSIZE", "SIZE")],
                                          [(S, "MRES", "M.RES")], [(K, "VOLUME", "VOLUME")]])]],
    cvs=[[("IN_ATK", "ATTACK"), ("IN_DEC", "DECAY"), ("IN_SUS", "SUSTAIN"), ("IN_REL", "RELEASE")],
         [("IN_DETUNE", "DETUNE"), ("IN_GLIDE", "GLIDE"), ("IN_VIB", "VIBRATO"), ("IN_TREM", "TREMOLO")],
         [("IN_ARPON", "ARP ON"), ("IN_ARATE", "RATE"), ("IN_LATCH", "LATCH"), ("IN_AOCT", "OCT")],
         [("IN_DRIVE", "DRIVE"), ("IN_CHORUS", "CHORUS"), ("IN_DELAY", "DELAY"), ("IN_DTIME", "TIME")],
         [("IN_REVERB", "REVERB"), ("IN_RSIZE", "SIZE"), ("IN_MRES", "M.RES"), ("IN_VOL", "VOLUME")]])


# slug -> (layout header, namespace)
OUT = {"Kwasub": ("kwasub/KwLayout.hpp", "kwlayout"), "KwasubFx": ("kwasub/KwFxLayout.hpp", "kwfxlayout"),
       "Golana": ("golana/GolanaLayout.hpp", "glayout"), "GolanaFx": ("golana/GolanaFxLayout.hpp", "gfxlayout"),
       "Umlauf": ("umlauf/UmlaufLayout.hpp", "ulayout"), "UmlaufFx": ("umlauf/UmlaufFxLayout.hpp", "uflayout")}

if __name__ == "__main__":
    preview = len(sys.argv) > 1 and sys.argv[1] == "preview"
    for slug, (hdr, ns) in OUT.items():
        p = gpanel.build(SPECS[slug])
        if preview:
            svg = os.path.join(sys.argv[2], slug + ".svg")
            open(svg, "w").write("\n".join(p.out + p.mock + gpanel.stand_ins(p) + ["</svg>"]))
            subprocess.run(["rsvg-convert", "-w", str(int(p.W * 15.5)), svg, "-o", svg[:-4] + ".png"], check=True)
            print(f"{slug}: {p.hp} HP -> {svg[:-4]}.png")
            continue
        open(os.path.join(gpanel.ROOT, "instruments", "res", slug + ".svg"), "w").write(gpanel.svg(p))
        lines = ["// Generated by tools/panels/instruments_panel.py - do not edit", "// Millimetres (the code converts with mm2px).",
                 "#pragma once", gpanel.layout(p, None, ns, unit="mm"), ""]
        open(os.path.join(gpanel.ROOT, "instruments", "src", hdr), "w").write("\n".join(lines))
        print(f"{slug}: {p.hp} HP -> instruments/res/{slug}.svg, instruments/src/{hdr}")
