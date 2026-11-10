#!/usr/bin/env python3
"""Panel of UMLAUF, an orbit vector synth after VADER (open-mpc), in the GENRA look (accent: sith red).

Usage:  umlauf_panel.py real                   -> instruments/res/Umlauf.svg (text as paths) + instruments/src/umlauf/UmlaufLayout.hpp
        umlauf_panel.py preview OUT.png [RACK] -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (32 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display (registration, solo sound, voices).
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

HP = 54
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with instruments/src/umlauf/Umlauf.cpp (ACCENT)
ACC = "#ff2e43"

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


def field(name, x, y, w):
    """A dark selection field (the module draws its text); the chevron marks it as a list."""
    extra_rects[name] = (x, y, w, 5.4)
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="5.4" rx="0.9" fill="{DARK}" stroke="{LINE}" stroke-width="0.4"/>')
    e(f'<polyline points="{x + w - 4.2:.2f},{y + 2.1:.2f} {x + w - 3:.2f},{y + 3.4:.2f} {x + w - 1.8:.2f},{y + 2.1:.2f}" '
      f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')


extra_rects = {}


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.6}" y="{y - 4.75}" width="9.2" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.6, TXT, 600, "middle", 0.25)


# ---------------------------------------------------------------- frame, header, display
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "UMLAUF", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("UMLAUF", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · ORBIT VECTOR SYNTH · AFTER VADER", 1.76, DIM, 600,
     "start", 0.47)
# presets: a field in the header (as on GRUND, FUNKE, SCHMELZ, GENRA MONO), the accent bar after it
field("PRESETF", W - 70, 3.4, 46)
e(f'<rect x="{W - 21.5:.2f}" y="5.55" width="8" height="1.08" rx="0.54" fill="{ACC}"/>')

DISPLAY = (3, 10.5, 100, 36)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]:.2f}" height="{DISPLAY[3]}" rx="1.6" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')

# ---------------------------------------------------------------- orbit + mode
OX = 105
section(OX, 10.5, W - OX - 3, 36, "ORBIT · MODE")
knob("SHAPE", OX + 12, 21.5, "SHAPE", "knob", hot=True)
knob("MODE", OX + 12, 37.6, "MODE", "knob", hot=True)
for i, (n, l) in enumerate([("SPEED", "SPEED"), ("SIZE", "SIZE"), ("ROT", "ROTATE"), ("SPIN", "3D SPIN"),
                            ("TILT", "3D TILT"), ("SUBSIZE", "SUB SIZE"), ("SUBSPD", "SUB SPEED"), ("OENV", "ENV>SIZE")]):
    knob(n, OX + 31 + i * 16.8, 21.5, l, "knob", hot=(n == "SPEED"))
for i in range(8):
    knob(f"M{i + 1}", OX + 31 + i * 16.8, 37.6, f"M{i + 1}", "small")
e(f'<line x1="{OX + 23.5}" y1="31.2" x2="{W - 5}" y2="31.2" stroke="{LINE}" stroke-width="0.3"/>')

# ---------------------------------------------------------------- corners A-D
CW = (W - 6 - 3 * 2) / 4
CORNER = {}
for c, L in enumerate("ABCD"):
    x = 3 + c * (CW + 2)
    section(x, 48.5, CW, 54.5, f"CORNER {L}")
    CORNER[L] = (x + 3, 55.5, CW - 6, 7.5)
    e(f'<rect x="{x + 3}" y="55.5" width="{CW - 6:.2f}" height="7.5" rx="1.0" fill="{DARK}" stroke="{LINE}" '
      f'stroke-width="0.3"/>')
    sp = (CW - 14) / 5
    for i, (n, l) in enumerate([("ENG", "ENGINE"), ("C1", "1"), ("C2", "2"), ("C3", "3"), ("C4", "4"),
                                ("C5", "5")]):
        knob(f"{n}_{L}", x + 7 + i * sp, 72.5, l, "knob" if n == "ENG" else "small", hot=(n == "ENG"))
    for i, (n, l) in enumerate([("FTYPE", "FILTER"), ("FON", "ON"), ("CUT", "CUTOFF"), ("RES", "RES"),
                                ("OCT", "OCT"), ("LVL", "LEVEL")]):
        knob(f"{n}_{L}", x + 7 + i * sp, 91.0, l, "small", hot=(n == "CUT"))

# ---------------------------------------------------------------- in / out
e(f'<rect x="3" y="104.5" width="{W - 6:.2f}" height="23.3" rx="1.2" fill="{PLATE}"/>')
X = [14 + i * (W - 28) / 21 for i in range(22)]
ROW1 = [("IN_VOCT", "V/OCT"), ("IN_GATE", "GATE"), ("IN_VEL", "VEL"), ("IN_CLK", "CLOCK"), ("IN_RST", "RESET"),
        ("IN_X", "X"), ("IN_Y", "Y"), ("IN_SHAPE", "SHAPE"), ("IN_MODE", "MODE"), ("IN_SPEED", "SPEED"),
        ("IN_SIZE", "SIZE"), ("IN_ROT", "ROT"), ("IN_SUB", "SUB"), ("IN_M1", "M1"), ("IN_M2", "M2"), ("IN_M3", "M3"),
        ("IN_M4", "M4"), ("IN_LVL_A", "LVL A"), ("IN_LVL_B", "LVL B"), ("IN_LVL_C", "LVL C"), ("IN_LVL_D", "LVL D"),
        ("IN_VOL", "VOL")]
ROW2_IN = [("IN_CUT_A", "CUT A"), ("IN_CUT_B", "CUT B"), ("IN_CUT_C", "CUT C"), ("IN_CUT_D", "CUT D"),
           ("IN_C1_A", "1 A"), ("IN_C1_B", "1 B"), ("IN_C1_C", "1 C"), ("IN_C1_D", "1 D")]
ROW2_OUT = [("OUT_X", "X"), ("OUT_Y", "Y"), ("OUT_W_A", "WT A"), ("OUT_W_B", "WT B"), ("OUT_W_C", "WT C"),
            ("OUT_W_D", "WT D"), ("OUT_ENV", "ENV"), ("OUT_A", "A"), ("OUT_B", "B"), ("OUT_C", "C"), ("OUT_D", "D"),
            ("OUT_L", "L"), ("OUT_R", "R")]
# the GENRA strip (as on every instrument), then the CV inputs
x = genra_strip(jack, e, 9.3, 109.2, {"V/OCT": "IN_VOCT", "GATE": "IN_GATE", "VEL": "IN_VEL"}, ACC,
                {"CLOCK": "IN_CLK", "RESET": "IN_RST"})
for i, (n, l) in enumerate(ROW1[5:]):
    jack(n, x + 8.1 + i * 11.4, 109.2, l)
for i, (n, l) in enumerate(ROW2_IN):
    jack(n, X[i], 120.2, l)
for i, (n, l) in enumerate(ROW2_OUT):
    jack(n, X[i + 9], 120.2, l, True)


def bar(i0, i1, y, col, op=0.7):
    e(f'<rect x="{X[i0] - 4.2:.2f}" y="{y}" width="{X[i1] - X[i0] + 8.4:.2f}" height="0.5" rx="0.25" fill="{col}" '
      f'fill-opacity="{op}"/>')


# row 1: the strip (its own trays), then the orbit / mode CVs and the corner levels
X1 = [x + 8.1 + i * 11.4 for i in range(len(ROW1) - 5)]
e(f'<rect x="{X1[0] - 4.2:.2f}" y="105.1" width="{X1[11] - X1[0] + 8.4:.2f}" height="0.5" rx="0.25" fill="{DIM}" '
  f'fill-opacity="0.7"/>')
e(f'<rect x="{X1[12] - 4.2:.2f}" y="105.1" width="{X1[16] - X1[12] + 8.4:.2f}" height="0.5" rx="0.25" fill="#6b6e7b" '
  f'fill-opacity="0.7"/>')
for xx, y in (((X1[11] + X1[12]) / 2, 105.7), ((X[8] + X[9]) / 2, 116.4), ((X[3] + X[4]) / 2, 116.4)):
    e(f'<line x1="{xx:.2f}" y1="{y}" x2="{xx:.2f}" y2="{y + 9.8}" stroke="{LINE}" stroke-width="0.3"/>')

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "instruments", "res", "Umlauf.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/umlauf_panel.py - do not edit", "#pragma once", "namespace ulayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for L_, (x_, y_, w_, h_) in CORNER.items():
        lines.append(f"static const float CDISP_{L_}[4] = {{{x_:.3f}f, {y_:.3f}f, {w_:.3f}f, {h_:.3f}f}};")
    lines.append(f"static const float DISPLAY[4] = {{{DISPLAY[0]:.3f}f, {DISPLAY[1]:.3f}f, {DISPLAY[2]:.3f}f, "
                 f"{DISPLAY[3]:.3f}f}};")
    for n, (x_, y_, w_, h_) in extra_rects.items():
        lines.append(f"static const float {n}[4] = {{{x_:.3f}f, {y_:.3f}f, {w_:.3f}f, {h_:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "instruments", "src", "umlauf"), exist_ok=True)
    open(os.path.join(ROOT, "instruments", "src", "umlauf", "UmlaufLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote instruments/res/Umlauf.svg and instruments/src/umlauf/UmlaufLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components + mock display
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


for kind, n, x, y in comps:
    if kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)
import math
dx, dy, dw, dh = DISPLAY
sx, sy, ss = dx + 3.5, dy + 3, 30
out.insert(-1, f'<rect x="{sx}" y="{sy}" width="{ss}" height="{ss}" fill="none" stroke="{LINE}" stroke-width="0.35"/>')
pts = []
for k in range(301):
    t = 2 * math.pi * k / 300
    r = 12 * math.cos(2.5 * t * 2) if False else 12 * abs(math.cos(5 * t / 2)) ** 0.6
    pts.append((sx + ss / 2 + r * math.cos(t * 2), sy + ss / 2 + r * math.sin(t * 2)))
out.insert(-1, '<polyline fill="none" stroke="#5a2a32" stroke-width="0.45" points="' +
           " ".join(f"{a:.2f},{b:.2f}" for a, b in pts) + '"/>')
for k in range(1, 9):
    qx, qy = pts[60 - k * 3]
    out.insert(-1, f'<circle cx="{qx:.2f}" cy="{qy:.2f}" r="{0.75 - k * 0.07:.2f}" fill="{ACC}" fill-opacity="{0.6 - k * 0.06:.2f}"/>')
px, py = pts[60]
out.insert(-1, f'<circle cx="{px:.2f}" cy="{py:.2f}" r="3.2" fill="none" stroke="{ACC}" stroke-width="0.2" stroke-dasharray="0.4 0.5"/>')
out.insert(-1, f'<circle cx="{px:.2f}" cy="{py:.2f}" r="0.8" fill="#ffb3bb"/>')
out.insert(-1, f'<circle cx="{px + 2.4:.2f}" cy="{py + 2.0:.2f}" r="1.4" fill="{ACC}"/>')
for (cx, cy), L in zip(((sx, sy), (sx + ss, sy), (sx, sy + ss), (sx + ss, sy + ss)), "ABCD"):
    out.insert(-1, f'<circle cx="{cx}" cy="{cy}" r="1.9" fill="#2a2b33" stroke="{ACC}" stroke-width="0.3"/>')
    ptext(cx, cy + 0.6, L, 1.6, "#ffffff", 800, "middle", 0)
tx = sx + ss + 5.5
ptext(tx, dy + 6.3, "ROSE 5/2", 3.4, "#ffffff", 800, "start", 0.3)
ptext(tx, dy + 10.2, "MATHS · MODE KEPLER · CLOCK 1/2", 1.6, ACC, 700, "start", 0.3)
for i, n in enumerate(["SPEED", "ECCENTRIC", "PRECESS", "SIZE", "TILT", "WELL", "TURN", "PHASE"]):
    ptext(tx + (i // 4) * 30, dy + 15.3 + (i % 4) * 3.5, f"M{i + 1}", 1.5, DIM, 700, "start", 0.2)
    ptext(tx + 5 + (i // 4) * 30, dy + 15.3 + (i % 4) * 3.5, n, 1.5, TXT, 600, "start", 0.2)
for i, (L, wt) in enumerate(zip("ABCD", (0.48, 0.27, 0.09, 0.16))):
    x = tx + i * 15.5
    ptext(x, dy + 31.6, L, 1.6, "#ffffff", 800, "start", 0)
    out.insert(-1, f'<rect x="{x + 2.5}" y="{dy + 30.2}" width="10" height="1.5" rx="0.3" fill="#2a2b33"/>')
    out.insert(-1, f'<rect x="{x + 2.5}" y="{dy + 30.2}" width="{10 * wt:.2f}" height="1.5" rx="0.3" fill="{ACC}"/>')
for L, eng, names in (("A", "SYNTH", "SAW · GLASS · 42% · +7"), ("B", "VOICE", "VOWEL · FORMANT · BREATH · TONE"),
                      ("C", "METAL", "CHARACTER · SPREAD · FM · BAND"), ("D", "WAV · vader 12", "START · END · LENGTH · LOOP")):
    x, y, w, h = CORNER[L]
    ptext(x + 2, y + 3.3, eng, 2.2, "#ffffff", 800, "start", 0.2)
    ptext(x + 2, y + 6.3, names, 1.4, ACC, 600, "start", 0.15)
# the preset field (the module writes number, name and style)
ptext(W - 68, 7.05, "04  DARK ORBIT", 1.9, "#e6e7ec", 800, "start", 0.5)
ptext(W - 30.5, 6.95, "TECHNO", 1.45, DIM, 600, "end", 0.4)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2000", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])

