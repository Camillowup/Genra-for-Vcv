#!/usr/bin/env python3
"""Panels of K-WASUB (the K-WASUB firmware's bass synth) and K-WASUB FX (its effects expander), in the GENRA look
(accent: acid green).

Usage:  kwasub_panel.py real                     -> instruments/res/Kwasub.svg, instruments/res/KwasubFx.svg (text as paths)
                                                    + instruments/src/kwasub/KwLayout.hpp, instruments/src/kwasub/KwFxLayout.hpp
        kwasub_panel.py preview OUT.png [RACK]   -> OUT.png and OUT_fx.png with Rack components
                                                    (RACK: a Rack source tree, for res/ComponentLibrary)

Coordinates in mm (40 HP each, height 128.5 mm). The C++ side converts with mm2px().
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import genra_strip  # noqa: E402

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 40
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with instruments/src/kwasub/Kwasub.cpp (ACCENT)
ACC = "#b8f23a"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0, "button": 3.6}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}


FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {400: "Inter-Regular.otf", 500: "Inter-Medium.otf", 600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf",
         800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps, extra = [], [], {}


def font(weight):
    if weight not in _fonts:
        f = TTFont(FONT_DIR + FONTS[weight])
        _fonts[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _fonts[weight]


def e(s):
    out.append(s)


def text(x, y, s, size=1.8, color=DIM, weight=600, anchor="middle", sp=0.25):
    gs, cmap, upm, hmtx = font(weight)
    scale = size / upm
    glyphs = [cmap.get(ord(c), "space") for c in s]
    w = sum(hmtx[g][0] * scale + sp for g in glyphs) - sp
    cx = x - (w / 2 if anchor == "middle" else (w if anchor == "end" else 0))
    paths = []
    for g in glyphs:
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, cx, y)))
        if pen.getCommands():
            paths.append(pen.getCommands())
        cx += hmtx[g][0] * scale + sp
    if paths:
        e(f'<path fill="{color}" d="{" ".join(paths)}"/>')


def width(s, size, weight, sp):
    gs, cmap, upm, hmtx = font(weight)
    return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


def tint(f):
    """The accent mixed onto the plate (f: the accent's share), as on GENRA MONO, MONK and the JIANT voices."""
    c = [int(ACC[i:i + 2], 16) for i in (1, 3, 5)]
    p = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(p[i] + (c[i] - p[i]) * f):02x}" for i in range(3))


def section(x, y, w, h, title):
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="{h}" rx="1.2" fill="{PLATE}"/>')
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="0.88" rx="0.44" fill="{ACC}"/>')
    text(x + w / 2, y + 5.0, title, 2.03, ACC, 800, "middle", 0.75)


def knob(name, x, y, label, kind="knob", hot=False):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x}" cy="{y}" r="{r:.2f}" fill="none" stroke="{ACC if hot else RING}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    text(x, y + R[kind] + 3.4, label, 1.7, TXT, 600, "middle", 0.3)


def button(name, x, y, label):
    comps.append(("button", name, x, y))
    text(x, y + R["button"] + 3.0, label, 1.7, TXT, 600, "middle", 0.3)


def lamps(name, x0, dx, y, names, label):
    """A lamp row for a switch parameter; the module places a LampRow at (x0, y), dx apart."""
    extra[name] = (x0, y, dx)
    text(x0 - 5.0, y + 0.65, label, 1.6, DIM, 700, "end", 0.3)
    for i, n in enumerate(names):
        e(f'<circle cx="{x0 + i * dx:.2f}" cy="{y}" r="1.35" fill="#0d0e11"/>')
        text(x0 + i * dx, y + 4.4, n, 1.5, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.6}" y="{y - 4.75}" width="9.2" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.6, TXT, 600, "middle", 0.25)


STRIP = "#141519"
OUT_TINT = tint(0.3)


def reset():
    out.clear()
    comps.clear()
    extra.clear()
    rects.clear()


rects = {}


def field(name, x, y, w):
    """An engine / type selector: the panel draws the box, the module the name and the menu."""
    rects[name] = (x, y, w, 5.4)
    e(f'<rect x="{x}" y="{y}" width="{w}" height="5.4" rx="0.9" fill="{DARK}" stroke="{LINE}" stroke-width="0.4"/>')
    e(f'<polyline points="{x + w - 4.2:.2f},{y + 2.1} {x + w - 3:.2f},{y + 3.4} {x + w - 1.8:.2f},{y + 2.1}" fill="none" '
      f'stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')


def onbtn(name, x, y):
    comps.append(("lit", name, x, y))


def lamps(name, x0, dx, y, names, size=1.45):
    extra[name] = (x0, y, dx)
    for i, n in enumerate(names):
        e(f'<circle cx="{x0 + i * dx:.2f}" cy="{y}" r="1.2" fill="#0d0e11"/>')
        text(x0 + i * dx, y + 3.9, n, size, TXT, 600, "middle", 0.25)


def row(names, x0, dx, y, kind):
    for i, (n, l) in enumerate(names):
        knob(n, x0 + i * dx, y, l, kind)


def strips(groups, y, ytop, h, x_of):
    """Dark strips behind each group of jacks."""
    for g in groups:
        xs = [x_of(n) for n, *_ in g]
        e(f'<rect x="{min(xs) - 5.2:.2f}" y="{ytop}" width="{max(xs) - min(xs) + 10.4:.2f}" height="{h}" rx="1" '
          f'fill="{STRIP}"/>')


def header(title, sub, preset=False):
    e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
    e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
    text(12.9, 7.6, title, 5.1, TITLE, 800, "start", 1.08)
    text(12.9 + width(title, 5.1, 800, 1.08) + 6.0, 7.45, sub, 1.76, DIM, 600, "start", 0.47)
    if preset:  # presets: a field in the header (as on GRUND, FUNKE, SCHMELZ, GENRA MONO), the accent bar after it
        field("PRESETF", W - 70, 3.4, 46)
        e(f'<rect x="{W - 21.5:.2f}" y="5.55" width="8" height="1.08" rx="0.54" fill="{ACC}"/>')
    else:
        e(f'<rect x="{W - 27.1:.2f}" y="5.55" width="11.5" height="1.08" rx="0.54" fill="{ACC}"/>')


def jack_groups(rowsets, y, ytop, sh=14.3, SL=11.0, x0=None):
    """rowsets: groups of (name, label, out); laid out as one row with gaps between the groups, each on a strip
    (centred, or from x0)."""
    GAP = 2.2
    n = sum(len(g) for g in rowsets)
    tot = n * SL + (len(rowsets) - 1) * GAP
    gx = (W - tot) / 2 if x0 is None else x0
    for g in rowsets:
        w = len(g) * SL
        e(f'<rect x="{gx + 0.3:.2f}" y="{ytop}" width="{w - 0.6:.2f}" height="{sh}" rx="1" fill="{STRIP}"/>')
        for i, (nm, lab, o) in enumerate(g):
            x = gx + SL / 2 + i * SL
            if o:
                e(f'<rect x="{x - 4.6:.2f}" y="{y - 4.75:.2f}" width="9.2" height="11.3" rx="0.9" fill="{OUT_TINT}"/>')
            comps.append(("output" if o else "input", nm, x, y))
            text(x, y + 5.7, lab, 1.5, TXT if o else DIM, 600, "middle", 0.25)
        gx += w + GAP


def write(svgname, hdr, ns):
    open(os.path.join(ROOT, "instruments", "res", svgname), "w").write("\n".join(out))
    lines = [f"// Generated by tools/kwasub_panel.py - do not edit", "#pragma once", f"namespace {ns} {{"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for n, (x, y, w, h) in rects.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "instruments", "src", "kwasub"), exist_ok=True)
    open(os.path.join(ROOT, "instruments", "src", "kwasub", hdr), "w").write("\n".join(lines) + "\n")


# ================================================================ K-WASUB
def build_main():
    reset()
    header("K-WASUB", "GENRA · MODERN BASS · FIRMWARE BY OMNIPOTARD", preset=True)
    rects["DISPLAY"] = (3, 10.5, W - 6, 16)
    e(f'<rect x="3" y="10.5" width="{W - 6:.2f}" height="16" rx="1.6" fill="{DARK}" stroke="{LINE}" stroke-width="0.4"/>')
    for i, o in enumerate(("A", "B")):
        x = 3 + i * 60
        section(x, 28.5, 58, 47, f"OSC {o}")
        field(f"ENG_{o}", x + 3, 35, 52)
        row([(f"{o}_P1", "SHAPE"), (f"{o}_P2", "COLOR"), (f"{o}_P3", "EDGE"), (f"{o}_P4", "FOLD")], x + 9, 13.3, 49.5, "knob")
        row([(f"{o}_LVL", "LEVEL"), (f"{o}_OCT", "OCT"), (f"{o}_SEMI", "SEMI"), (f"{o}_FINE", "FINE")], x + 9, 13.3, 65.5,
            "small")
    section(123, 28.5, W - 126, 47, "FILTER")
    field("FTYPE", 126, 35, W - 132)
    knob("F_CUT", 140, 54, "CUTOFF", "large", hot=True)
    row([("F_RES", "RES"), ("F_DRV", "DRIVE"), ("F_ENV", "ENV")], 158, 14, 49.5, "knob")
    row([("F_KEY", "KEY"), ("FE_A", "F.ATK"), ("FE_D", "F.DEC")], 158, 14, 65.5, "small")
    # the bottom row: four sections over the full width (the preset moved into the header)
    SW = (W - 6 - 3 * 2) / 4
    def sx(i): return 3 + i * (SW + 2)
    def spread(names, i, hot=None):
        n = len(names) + (1 if hot else 0)
        dx = min(11.5, (SW - 8) / max(1, n - 1))
        x0 = sx(i) + SW / 2 - dx * (n - 1) / 2
        if hot:
            knob(hot[0], x0, 88.5, hot[1], "knob", hot=True)
            x0 += dx
        row(names, x0, dx, 88.5, "small")
    section(sx(0), 77.5, SW, 23, "SUB · X")
    spread([("S_LVL", "SUB"), ("N_LVL", "NOISE"), ("X_AMT", "X-MOD")], 0)
    section(sx(1), 77.5, SW, 23, "WOBBLE")
    spread([("L_DEP", "DEPTH"), ("L_SHP", "SHAPE"), ("L_DST", "DEST")], 1, hot=("L_RATE", "RATE"))
    section(sx(2), 77.5, SW, 23, "AMP")
    spread([("AE_A", "A"), ("AE_D", "D"), ("AE_S", "S"), ("AE_R", "R")], 2)
    section(sx(3), 77.5, SW, 23, "FX")
    spread([("FX_SAT", "SAT"), ("FX_DLY", "DELAY"), ("FX_VERB", "VERB"), ("FX_GLUE", "GLUE")], 3)
    e(f'<rect x="3" y="102.2" width="{W - 6:.2f}" height="25" rx="1.2" fill="{PLATE}"/>')
    # the GENRA strip (as on every instrument): ACC and SLIDE are new, MOD moved here (mod wheel)
    def jk(name, x, y, label):
        comps.append(("input", name, x, y))
        text(x, y + 5.7, label, 1.5, DIM, 600, "middle", 0.25)
    x = genra_strip(jk, e, 5, 106.8, {"V/OCT": "IN_VOCT", "GATE": "IN_GATE", "VEL": "IN_VEL", "ACC": "IN_ACC",
                                       "SLIDE": "IN_SLIDE", "MOD": "IN_MOD"}, ACC,
                    {"CLOCK": "IN_CLOCK", "RESET": "IN_RESET"})
    jack_groups([[("IN_CUT", "CUT", 0), ("IN_RES", "RES", 0), ("IN_DRV", "DRIVE", 0), ("IN_FENV", "ENV", 0)],
                 [("IN_ASHP", "A SHP", 0), ("IN_ACOL", "A COL", 0), ("IN_BSHP", "B SHP", 0), ("IN_BCOL", "B COL", 0)],
                 [("IN_WOB", "WOB", 0), ("IN_DEP", "DEPTH", 0)]], 106.8, 102.9, 11.8, 10.8, x + 3.2)
    jack_groups([[("IN_SUB", "SUB", 0), ("IN_NOISE", "NOISE", 0), ("IN_XMOD", "X-MOD", 0)],
                 [("IN_ATK", "ATK", 0), ("IN_REL", "REL", 0)],
                 [("IN_SAT", "SAT", 0), ("IN_DLY", "DELAY", 0), ("IN_VERB", "VERB", 0)],
                 [("IN_PRESS", "PRESS", 0), ("IN_BEND", "BEND", 0), ("IN_GLIDE", "GLIDE", 0), ("IN_PRESET", "PRESET", 0)],
                 [("OUT_ENV", "ENV", 1), ("OUT_LFO", "LFO", 1), ("OUT_L", "L", 1), ("OUT_R", "R", 1)]], 118.9, 115.0, 11.8, 10.8)
    e("</svg>")


# ================================================================ K-WASUB FX
def fxsec(x, y, w, h, title, on=None):
    section(x, y, w, h, title)
    if on:
        onbtn(on, x + 4.6, y + 4.4)


def build_fx():
    reset()
    header("K-WASUB FX", "GENRA · EXPANDER · DOCKS RIGHT OF K-WASUB")
    rects["LINK"] = (W - 40 - 1.1, 6.1 - 1.1, 2.2, 2.2)
    text(W - 37.5, 6.75, "LINK", 1.7, TXT, 700, "start", 0.4)
    y, h = 11, 32
    fxsec(3, y, 46, h, "SATURATE", "DR_ON")
    lamps("DR_TYPE", 10, 10.3, y + 10.5, ["SOFT", "TUBE", "HARD", "FOLD"])
    row([("DR_AMT", "AMOUNT"), ("DR_MIX", "MIX")], 16, 14, y + 22.5, "small")
    fxsec(51, y, 44, h, "CRUSH", "CR_ON")
    row([("CR_BITS", "BITS"), ("CR_RATE", "RATE"), ("CR_MIX", "MIX")], 59, 13.5, y + 18, "knob")
    fxsec(97, y, 44, h, "CHORUS", "CH_ON")
    row([("CH_RATE", "RATE"), ("CH_DEP", "DEPTH"), ("CH_MIX", "MIX")], 105, 13.5, y + 18, "knob")
    fxsec(143, y, W - 146, h, "PHASER", "PH_ON")
    row([("PH_RATE", "RATE"), ("PH_DEP", "DEPTH"), ("PH_FB", "FDBK"), ("PH_MIX", "MIX")], 150.5, 13.6, y + 18, "knob")
    y = 45
    fxsec(3, y, 60, h, "DELAY", "DL_ON")
    lamps("DL_TYPE", 11, 12.6, y + 10.5, ["PING-PONG", "TAPE", "DIGITAL", "DUB"])
    row([("DL_TIME", "TIME"), ("DL_FB", "FDBK"), ("DL_TONE", "TONE"), ("DL_MIX", "MIX")], 11, 12.6, y + 22.5, "small")
    fxsec(65, y, 52, h, "REVERB", "RV_ON")
    lamps("RV_TYPE", 73, 11.8, y + 10.5, ["ROOM", "HALL", "PLATE", "SPRING"])
    row([("RV_SIZE", "SIZE"), ("RV_DAMP", "DAMP"), ("RV_MIX", "MIX")], 76, 15, y + 22.5, "small")
    fxsec(119, y, W - 122, h, "COMPRESSOR", "CP_ON")
    row([("CP_AMT", "AMOUNT"), ("CP_ATK", "ATTACK"), ("CP_REL", "RELEASE"), ("CP_PUMP", "PUMP")], 127, 12.5, y + 18, "knob")
    lamps("CP_PRATE", 175.5, 5.9, y + 14.5, ["1/4", "1/8", "1/2", "BAR"])
    text(184.35, y + 25.6, "PUMP RATE", 1.45, DIM, 700, "middle", 0.3)
    y, h = 79, 29
    fxsec(3, y, 42, h, "MULTIBAND", "OT_ON")
    row([("OT_AMT", "AMOUNT"), ("OT_LOW", "LOW"), ("OT_HIGH", "HIGH")], 11, 13, y + 16.5, "small")
    fxsec(47, y, 113, h, "EQ", "EQ_ON")
    for i, n in enumerate(["LO SHELF", "BELL", "BELL", "HI SHELF"]):
        cx = 56 + i * 27.5
        text(cx + 5.5, y + 10.6, n, 1.45, DIM, 700, "middle", 0.3)
        knob(f"EQ{i + 1}_F", cx, y + 18.5, "FREQ", "small")
        knob(f"EQ{i + 1}_G", cx + 11, y + 18.5, "GAIN", "small")
        if i < 3:
            e(f'<line x1="{cx + 18.2:.2f}" y1="{y + 8.5}" x2="{cx + 18.2:.2f}" y2="{y + 26}" stroke="{LINE}" stroke-width="0.3"/>')
    fxsec(162, y, W - 165, h, "MASTER")
    row([("MS_GLUE", "GLUE"), ("MS_SUB", "SUB"), ("MB_MONO", "MONO")], 170, 12.5, y + 16.5, "small")
    e(f'<rect x="3" y="110" width="{W - 6:.2f}" height="16.5" rx="1.2" fill="{PLATE}"/>')
    jack_groups([[("IN_SAT", "SAT", 0)], [("IN_CRUSH", "CRUSH", 0)], [("IN_CHORUS", "CHORUS", 0)], [("IN_PHASER", "PHASER", 0)],
                 [("IN_DTIME", "D.TIME", 0), ("IN_DFB", "D.FDBK", 0), ("IN_DMIX", "D.MIX", 0)],
                 [("IN_VSIZE", "V.SIZE", 0), ("IN_VMIX", "V.MIX", 0)], [("IN_COMP", "COMP", 0), ("IN_PUMP", "PUMP", 0)],
                 [("IN_OTT", "OTT", 0)], [("IN_EQLO", "EQ LO", 0), ("IN_EQHI", "EQ HI", 0)],
                 [("IN_GLUE", "GLUE", 0), ("IN_SUBB", "SUB", 0)]], 115.6, 111.2)
    e("</svg>")


# ---------------------------------------------------------------- preview: Rack components
_cache = {}


def img(name, x, y):
    fn = os.path.join(CL, name + ".svg")
    if fn not in _cache:
        src = open(fn).read()
        src = re.sub(r"<\?xml[^>]*\?>|<!DOCTYPE[^>]*>|<!--.*?-->", "", src, flags=re.S)
        m = re.search(r"<svg[^>]*>", src, re.S)
        tag = m.group(0)
        w = float(re.search(r'\swidth="([0-9.]+)', tag).group(1))
        h = float(re.search(r'\sheight="([0-9.]+)', tag).group(1))
        vb = re.search(r'viewBox="([^"]+)"', tag)
        body = src[m.end():src.rindex("</svg>")]
        body = re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)|<metadata.*?</metadata>", "", body, flags=re.S)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w / 2.9528, h / 2.9528, body)
    vbs, sw, sh, body = _cache[fn]
    out.insert(-1, f'<svg x="{x - sw / 2:.3f}" y="{y - sh / 2:.3f}" width="{sw:.3f}" height="{sh:.3f}" '
                   f'viewBox="{vbs}" overflow="visible">{body}</svg>')


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


def place():
    for kind, n, x, y in comps:
        if kind in KIND:
            img(KIND[kind] + "_bg", x, y)
            img(KIND[kind], x, y)
        elif kind in ("button", "lit"):
            img("VCVBezel", x, y)
        else:
            img("PJ301M", x, y)
    for sx_ in (7.62, W - 7.62):
        for sy in (2.54, H - 2.54):
            img("ScrewBlack", sx_, sy)


def render(png):
    _cache.clear()
    place()
    if "PRESETF" in rects:  # the preset field (the module writes number, name and style)
        x, y, w, h = rects["PRESETF"]
        ptext(x + 2, y + 3.65, "12  ACID ROLLER", 1.9, "#e6e7ec", 800, "start", 0.5)
        ptext(x + w - 6, y + 3.55, "TECHNO", 1.45, DIM, 600, "end", 0.4)
    svg = png.rsplit(".", 1)[0] + ".svg"
    open(svg, "w").write("\n".join(out))
    subprocess.run(["rsvg-convert", "-w", "1600", svg, "-o", png], check=True)
    print("wrote", png)


for build, svgname, hdr, ns, sfx in ((build_main, "Kwasub.svg", "KwLayout.hpp", "kwlayout", ""),
                                     (build_fx, "KwasubFx.svg", "KwFxLayout.hpp", "kwfxlayout", "_fx")):
    build()
    if PREVIEW:
        p = sys.argv[2].rsplit(".", 1)
        render(p[0] + sfx + "." + p[1])
    else:
        write(svgname, hdr, ns)
if not PREVIEW:
    print("wrote instruments/res/Kwasub.svg, instruments/res/KwasubFx.svg, instruments/src/kwasub/KwLayout.hpp, instruments/src/kwasub/KwFxLayout.hpp")
