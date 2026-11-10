#!/usr/bin/env python3
"""Panel of SCHLAG, the sample drum machine for GENRA's drum track (after the Polyend Play), in the GENRA look (accent: drums violet).

Usage:  grund_panel.py real                   -> res/Schlag.svg (text as paths) + src/schlag/SchlagLayout.hpp
        grund_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (30 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display and the text in the two fields (model, filter).
"""
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import genra_strip, SLOT, dim  # noqa: E402

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 46
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# the drum track colour (as DRUM-X)
ACC = "#7b6cff"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0, "button": 3.6}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}


FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {400: "Inter-Regular.otf", 500: "Inter-Medium.otf", 600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf",
         800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps, extra, fields = [], [], {}, {}


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
    if label:
        text(x0 - 5.0, y + 0.65, label, 1.6, DIM, 700, "end", 0.3)
    for i, n in enumerate(names):
        e(f'<circle cx="{x0 + i * dx:.2f}" cy="{y}" r="1.25" fill="#0d0e11"/>')
        text(x0 + i * dx, y + 4.0, n, 1.45, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.6:.2f}" y="{y - 3.9:.2f}" width="9.2" height="10.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.5, TXT if out_ else DIM, 600, "middle", 0.25)


def field(name, x, y, w):
    """A dark selection field (the module draws its text); the chevron marks it as a list."""
    fields[name] = (x, y, w, 5.4)
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="5.4" rx="0.9" fill="{DARK}" stroke="{LINE}" stroke-width="0.4"/>')
    e(f'<polyline points="{x + w - 4.2:.2f},{y + 2.1:.2f} {x + w - 3:.2f},{y + 3.4:.2f} {x + w - 1.8:.2f},{y + 2.1:.2f}" '
      f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')




def strip(x0, y, items, line):
    """A jack tray like the GENRA strip (panellib), here with DRUMS in front as on DRUM-X."""
    from panellib import TRAY, TRAY_TOP, TRAY_H
    w = len(items) * SLOT
    e(f'<rect x="{x0 + 0.3:.2f}" y="{y + TRAY_TOP:.2f}" width="{w - 0.6:.2f}" height="{TRAY_H}" rx="1" fill="{TRAY}"/>')
    e(f'<rect x="{x0 + 1.2:.2f}" y="{y + TRAY_TOP:.2f}" width="{w - 2.4:.2f}" height="0.55" rx="0.27" fill="{line}"/>')
    for i, (nm, lb) in enumerate(items):
        jack(nm, x0 + SLOT / 2 + i * SLOT, y, lb)
    return x0 + w


# ---------------------------------------------------------------- frame, header, display
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "SCHLAG", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("SCHLAG", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · SAMPLE DRUMS", 1.76, DIM, 600, "start", 0.47)
field("KITF", W - 76, 3.4, 52)
e(f'<rect x="{W - 21.5:.2f}" y="5.55" width="8" height="1.08" rx="0.54" fill="{ACC}"/>')

SW = 24.6                      # strip width
SX = [3 + i * SW for i in range(8)]
MX = SX[-1] + SW + 1.6         # master section
DISPLAY = (3, 10.5, W - 6, 16)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]:.2f}" height="{DISPLAY[3]}" rx="1.6" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')

NAMES = ["KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"]
ROWS = [62, 70.4, 78.8, 87.2]


def small_field(name, x, y, w, h=4.4):
    fields[name] = (x, y, w, h)
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h}" rx="0.7" fill="{DARK}" stroke="{LINE}" stroke-width="0.3"/>')
    e(f'<polyline points="{x + w - 2.6:.2f},{y + 1.75:.2f} {x + w - 1.8:.2f},{y + 2.6:.2f} {x + w - 1.0:.2f},{y + 1.75:.2f}" '
      f'fill="none" stroke="{ACC}" stroke-width="0.3" stroke-linecap="round" stroke-linejoin="round"/>')


# ---------------------------------------------------------------- eight strips in GENRA's order
for i, nm in enumerate(NAMES):
    x0 = SX[i]
    cx = x0 + SW / 2
    e(f'<rect x="{x0 + 0.5:.2f}" y="28" width="{SW - 1:.2f}" height="78.4" rx="1.2" fill="{PLATE}"/>')
    e(f'<rect x="{x0 + 0.5:.2f}" y="28" width="{SW - 1:.2f}" height="0.88" rx="0.44" fill="{ACC}"/>')
    text(cx if nm != "O.HAT" else x0 + 2.6, 32.6, nm, 1.85, ACC, 800, "middle" if nm != "O.HAT" else "start", 0.4)
    # the sample: SAMPLE picks in the folder (right-click: folder), RANDOM = Play-style randomize from the folder
    knob(f"SAMPLE{i}", cx - 5.6, 41, "SAMPLE", "small")
    # RANDOM: an endless encoder (the module draws it: knurled, no pointer, the amount as a number and an arc)
    comps.append(("enc", f"RAND{i}", cx + 5.6, 41))
    text(cx + 5.6, 41 + R["small"] + 3.4, "RANDOM", 1.7, TXT, 600, "middle", 0.3)
    for bx, bn, bl in ((cx - 6.6, "BASIC", "BASIC"), (cx + 1.0, "MUTE", "M"), (cx + 7.6, "SOLO", "S")):
        comps.append(("tl", f"{bn}{i}", bx, 51.4))
        text(bx, 56.1, bl, 1.2, DIM, 700, "middle", 0.2)
    if nm == "O.HAT":
        extra["CHOKE"] = (x0 + SW - 3.4, 31.95, 0)
        e(f'<circle cx="{x0 + SW - 3.4:.2f}" cy="31.95" r="0.95" fill="#0d0e11"/>')
        text(x0 + SW - 4.9, 32.45, "CHOKE", 1.15, DIM, 700, "end", 0.2)
    e(f'<line x1="{x0 + 2.5:.2f}" y1="58.0" x2="{x0 + SW - 2.5:.2f}" y2="58.0" stroke="{LINE}" stroke-width="0.3"/>')
    # four assignable knobs: dropdown chooses the function
    for r, y in enumerate(ROWS):
        small_field(f"FN{i}_{r}", x0 + 2.0, y - 2.2, 11.9)
        knob(f"K{i}_{r}", x0 + SW - 5.6, y, "", "small")
    jack(f"TRIG{i}", cx - 5.6, 98.4, "TRIG")
    jack(f"OUT{i}", cx + 5.6, 98.4, "OUT", True)

# ---------------------------------------------------------------- master
section(MX, 28, W - 3 - MX, 78.4, "MASTER")
mc = MX + (W - 3 - MX) / 2
knob("ACCENT", mc - 6.6, 39.5, "ACCENT", "small")
knob("MLVL", mc + 6.6, 39.5, "LEVEL", "small")
text(mc, 51.0, "MIDI IN", 1.5, TXT, 700, "middle", 0.3)
field("MIDIF", MX + 2.2, 52.3, W - 3 - MX - 4.4)
button("RNDALL", mc - 6.6, 66, "RND ALL")
button("UNDO", mc + 6.6, 66, "UNDO")
button("BASIC", mc - 6.6, 79, "BASIC")
button("PRINT", mc + 6.6, 79, "PRINT")
text(mc + 6.6, 79 + 3.6 + 5.0, "› ABLAUF", 1.35, DIM, 600, "middle", 0.25)
jack("OUT_L", mc - 6.6, 98.4, "L", True)
jack("OUT_R", mc + 6.6, 98.4, "R", True)

# ---------------------------------------------------------------- jacks: GENRA strip, CV
e(f'<rect x="3" y="107.8" width="{W - 6:.2f}" height="17.6" rx="1.2" fill="{PLATE}"/>')
JY = 115.9
x = strip(13, JY, [("IN_DRUMS", "DRUMS"), ("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL"), ("ACC", "ACC")],
          dim(ACC))
x = strip(x + 2.2, JY, [("CLOCK", "CLOCK"), ("RESET", "RESET")], "#5a5c66")
x = strip(x + 4.0, JY, [("IN_RND", "RND"), ("IN_KIT", "KIT")], "#34363f")
cvx = x + 4.0
x = strip(cvx, JY, [("IN_CV1", "CV 1"), ("IN_CV2", "CV 2"), ("IN_CV3", "CV 3"), ("IN_CV4", "CV 4")], "#34363f")
text(cvx + 2 * SLOT, JY - 6.3, "KNOB ROWS · POLY 1–8 = TRACKS", 1.2, DIM, 700, "middle", 0.25)
# sends: what the tracks send to REVERB / DELAY, for outboard effects (internal effect off while patched)
for k, (nm, lb) in enumerate([("SEND_A_L", "A L"), ("SEND_A_R", "A R"), ("SEND_B_L", "B L"), ("SEND_B_R", "B R")]):
    jx = W - 52 + k * 11 + (2.2 if k > 1 else 0)
    jack(nm, jx, JY, lb, True)
text(W - 52 + 5.5, JY - 6.3, "SEND A", 1.35, DIM, 700, "middle", 0.3)
text(W - 52 + 2.2 + 27.5, JY - 6.3, "SEND B", 1.35, DIM, 700, "middle", 0.3)

e("</svg>")
if not PREVIEW:
    open(os.path.join(ROOT, "res", "Schlag.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/schlag_panel.py - do not edit", "#pragma once", "namespace glayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for n, (x, y, w, h) in fields.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};  // field x, y, w, h")
    lines.append(f"static const float DISPLAY[4] = {{{DISPLAY[0]:.3f}f, {DISPLAY[1]:.3f}f, {DISPLAY[2]:.3f}f, "
                 f"{DISPLAY[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    # the per-track families as tables: NAME_T[track] -> {x, y}
    for fam in ("SAMPLE", "RAND", "BASIC", "MUTE", "SOLO", "TRIG", "OUT"):
        lines.append(f"static const float* const {fam}_T[8] = {{" + ", ".join(f"{fam}{i}" for i in range(8)) + "};")
    lines.append("static const float* const K_T[8][4] = {" + ", ".join("{" + ", ".join(f"K{i}_{r}" for r in range(4)) + "}" for i in range(8)) + "};")
    lines.append("static const float* const FN_T[8][4] = {" + ", ".join("{" + ", ".join(f"FN{i}_{r}" for r in range(4)) + "}" for i in range(8)) + "};")
    lines.append(f"static const float STRIP_W = {SW:.3f}f;  // width of a track strip")
    lines.append(f"static const float STRIP_X0 = {SX[0]:.3f}f;  // left edge of the first strip")
    lines.append(f"static const float MASTER_X = {MX:.3f}f;  // left edge of the master section")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "schlag"), exist_ok=True)
    open(os.path.join(ROOT, "src", "schlag", "SchlagLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/Schlag.svg and src/schlag/SchlagLayout.hpp")
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
    elif kind == "tl":
        img("TL1105_0", x, y)
    elif kind == "enc":  # the encoder as the module draws it, with example amounts
        pct = [35, 60, 0, 20, 75, 0, 45, 100][int(n[4:])]
        rr, ring = R["small"], R["small"] + 1.15
        out.insert(-1, f'<circle cx="{x}" cy="{y}" r="{ring:.2f}" fill="none" stroke="#2a2b36" stroke-width="0.55"/>')
        if pct:
            a0 = math.radians(135); a1 = a0 + math.radians(270 * pct / 100)
            out.insert(-1, f'<path d="M{x + ring * math.cos(a0):.2f},{y + ring * math.sin(a0):.2f} A{ring:.2f},{ring:.2f} 0 '
                           f'{1 if a1 - a0 > math.pi else 0} 1 {x + ring * math.cos(a1):.2f},{y + ring * math.sin(a1):.2f}" '
                           f'fill="none" stroke="{ACC}" stroke-width="0.6" stroke-linecap="round"/>')
        out.insert(-1, f'<circle cx="{x}" cy="{y}" r="{rr:.2f}" fill="#16171d" stroke="#3a3b46" stroke-width="0.25"/>')
        for k in range(28):
            a = 2 * math.pi * k / 28
            out.insert(-1, f'<line x1="{x + (rr - 0.75) * math.cos(a):.2f}" y1="{y + (rr - 0.75) * math.sin(a):.2f}" '
                           f'x2="{x + (rr - 0.1) * math.cos(a):.2f}" y2="{y + (rr - 0.1) * math.sin(a):.2f}" stroke="#4a4b58" stroke-width="0.22"/>')
        out.insert(-1, f'<circle cx="{x}" cy="{y}" r="{rr - 1.1:.2f}" fill="#22232b"/>')
        ptext(x, y + 0.75, f"{pct}%", 1.9, ACC if pct else DIM, 800, "middle", 0.1)
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)
x, y, w, h = fields["MIDIF"]
ptext(x + 1.6, y + 3.75, "MPK MINI · CH 10", 1.45, TXT, 700, "start", 0.15)
cx_, cy_, _ = extra["CHOKE"]
out.insert(-1, f'<circle cx="{cx_}" cy="{cy_}" r="0.95" fill="{ACC}"/>')
for kind, n, x_, y_ in comps:
    if n in ("MUTE4",):
        out.insert(-1, f'<circle cx="{x_}" cy="{y_}" r="1.6" fill="#ffd34d" opacity="0.9"/>')
    if n.startswith("SEND_"):
        out.insert(-1, f'<rect x="{x_ - 2.4:.2f}" y="{y_ - 5.0:.2f}" width="4.8" height="0.7" rx="0.35" fill="#8fd8ff"/>')
ptext(W - 52 - 6.5, JY + 0.5, "GENRA FX ‹", 1.3, "#8fd8ff", 700, "end", 0.2)
x, y, w, h = fields["KITF"]
ptext(x + 2, y + 3.75, "04  GARAGE 2-STEP", 1.9, TXT, 700, "start", 0.35)
ptext(x + w - 6, y + 3.6, "UKG · 2-STEP", 1.4, DIM, 600, "end", 0.25)
FN = [["LEVEL", "DECAY", "DRIVE", "SEND A"], ["LEVEL", "TUNE", "REP TYPE", "REP GRID"],
      ["LEVEL", "DECAY", "SEND A", "SEND B"], ["LEVEL", "TUNE", "BITS", "PAN"],
      ["LEVEL", "DECAY", "PAN", "REP GRID"], ["LEVEL", "DECAY", "CUTOFF", "SEND B"],
      ["LEVEL", "TUNE", "DECAY", "SEND A"], ["LEVEL", "START", "BITS", "CHANCE"]]
for i in range(8):
    for r in range(4):
        x, y, w, h = fields[f"FN{i}_{r}"]
        ptext(x + 1.0, y + 2.85, FN[i][r], 1.3, TXT, 700, "start", 0.1)
for kind, n, x, y in comps:
    if n in ("IN_DRUMS", "CLOCK", "RESET"):
        c = ACC if n == "IN_DRUMS" else "#ffffff"
        out.insert(-1, f'<rect x="{x - 2.4:.2f}" y="{y - 5.6:.2f}" width="4.8" height="0.7" rx="0.35" fill="{c}"/>')
    if n == "OUT_L":
        ptext(x + 6.6, y - 4.7, "› MIX CH 1", 1.3, ACC, 700, "middle", 0.2)
# display: one column per strip — mini waveform, sample name, folder position (random hit flashes)
import math, random
random.seed(4)
dx, dy, dw, dh = DISPLAY
SMP = [("808 DEEP 03", "KICKS 3/24"), ("UKG SNR 11", "SNARES 11/40"), ("CLAP WIDE", "CLAPS ~ 7/18"),
       ("RIM DRY 2", "RIMS 2/9"), ("HAT TIGHT 5", "HATS ~ 14/31"), ("OPEN 909", "OHATS 1/12"),
       ("TOM LO 4", "TOMS 4/10"), ("SHAKER 08", "PERC ~ 8/36")]
DEC = [3.5, 7, 9, 14, 20, 5, 6, 10]
HITS = ["x...x.....x.x...", "....x.......x...", "............x..x", "..x.....x....x..", "x.x.x.x.x.x.x.x.",
        "..x.......x.....", ".......x.....x..", ".x..x.x..x...x.x"]
SWAP = [set(), {12}, set(), {8}, {2, 6, 14}, set(), set(), {4, 9, 15}]
for i in range(8):
    cx = SX[i] + SW / 2
    if i:
        out.insert(-1, f'<line x1="{SX[i]:.2f}" y1="{dy + 2}" x2="{SX[i]:.2f}" y2="{dy + dh - 2}" stroke="#1e2026" stroke-width="0.3"/>')
    wx, ww, cyy = SX[i] + 2.5, SW - 5, dy + 4.6
    tp, bt = [], []
    for k in range(81):
        t = k / 80
        a = 2.3 * math.exp(-t * DEC[i]) * (0.6 + 0.4 * random.random() if i > 0 else 1)
        tp.append(f"{wx + t * ww:.2f},{cyy - a:.2f}")
        bt.insert(0, f"{wx + t * ww:.2f},{cyy + a:.2f}")
    out.insert(-1, f'<polygon points="{" ".join(tp + bt)}" fill="{ACC}" opacity="0.8"/>')
    a, b = SMP[i]
    ptext(cx, dy + 9.5, a, 1.45, "#ffffff", 800, "middle", 0.2)
    ptext(cx, dy + 11.7, b.replace("~ ", ""), 1.2, ACC, 700, "middle", 0.2)
    nw = width(a, 1.45, 800, 0.2)
    out.insert(-1, f'<polyline points="{cx + nw / 2 + 0.8:.2f},{dy + 8.4:.2f} {cx + nw / 2 + 1.5:.2f},{dy + 9.1:.2f} {cx + nw / 2 + 2.2:.2f},{dy + 8.4:.2f}" fill="none" stroke="{ACC}" stroke-width="0.3" stroke-linecap="round" stroke-linejoin="round"/>')
    cw = ww / 16
    for k in range(16):
        hit = HITS[i][k] == "x"
        col = "#ffd34d" if (hit and k in SWAP[i]) else (ACC if hit else "#26282f")
        out.insert(-1, f'<rect x="{wx + k * cw + 0.12:.2f}" y="{dy + 12.9:.2f}" width="{cw - 0.24:.2f}" height="1.5" rx="0.25" fill="{col}"/>')
    out.insert(-1, f'<rect x="{wx + 6 * cw - 0.05:.2f}" y="{dy + 12.65:.2f}" width="{cw + 0.1:.2f}" height="2.0" rx="0.3" fill="none" stroke="#ffffff" stroke-width="0.25"/>')
ptext(MX + (W - 3 - MX) / 2, dy + 5.5, "● GENRA", 1.5, ACC, 700, "middle", 0.3)
ptext(MX + (W - 3 - MX) / 2, dy + 10.3, "2-STEP", 2.4, "#ffffff", 800, "middle", 0.3)
ptext(MX + (W - 3 - MX) / 2, dy + 14.0, "132 BPM", 1.4, DIM, 700, "middle", 0.3)
out.insert(-1, f'<line x1="{MX - 0.8:.2f}" y1="{dy + 2}" x2="{MX - 0.8:.2f}" y2="{dy + dh - 2}" stroke="{LINE}" stroke-width="0.3"/>')
if os.environ.get("MENU"):
    # the function dropdown of a strip knob, opened on SNARE, knob 3
    fx, fy, fw, fh = fields["FN1_2"]
    out.insert(-1, f'<rect x="{fx - 0.2:.2f}" y="{fy - 0.2:.2f}" width="{fw + 0.4:.2f}" height="{fh + 0.4:.2f}" rx="0.8" fill="none" stroke="{ACC}" stroke-width="0.35"/>')
    COLS = [[("SOUND", None), ("LEVEL", "volume"), ("PAN", "L · C · R"), ("TUNE", "semitones"), ("MICROTUNE", "±100 cents"),
             ("START", "sample start"), ("END", "end < start = reverse"), ("ATTACK", "fade-in"), ("DECAY", "fade-out"),
             ("SPACE", None), ("REV SEND", "to REVERB"), ("DLY SEND", "to DELAY")],
            [("TONE", None), ("CUTOFF", "LP · off · HP"), ("RESONANCE", "filter peak"), ("DRIVE", "overdrive"),
             ("BITS", "bit depth"),
             ("PLAY", None), ("REPEAT TYPE", "18 types"), ("REPEAT GRID", "hits | steps"), ("CHANCE", "always · % · play/skip"),
             ("ACTION", "random X · humanize")]]
    RH, CW, PAD = 3.5, 36.0, 1.6
    mx, my = fx, fy + fh + 0.8
    mw = 2 * CW + 3 * PAD
    mh = max(len(c) for c in COLS) * RH + 2 * PAD
    out.insert(-1, f'<rect x="{mx + 0.6:.2f}" y="{my + 0.8:.2f}" width="{mw:.2f}" height="{mh:.2f}" rx="1.0" fill="#000000" opacity="0.45"/>')
    out.insert(-1, f'<rect x="{mx:.2f}" y="{my:.2f}" width="{mw:.2f}" height="{mh:.2f}" rx="1.0" fill="#0d0e12" stroke="{ACC}" stroke-width="0.35"/>')
    out.insert(-1, f'<line x1="{mx + PAD + CW + PAD / 2:.2f}" y1="{my + PAD:.2f}" x2="{mx + PAD + CW + PAD / 2:.2f}" y2="{my + mh - PAD:.2f}" stroke="{LINE}" stroke-width="0.25"/>')
    for ci, col in enumerate(COLS):
        cx0 = mx + PAD + ci * (CW + PAD)
        for ri, (nm, sub) in enumerate(col):
            yy = my + PAD + ri * RH
            if sub is None:
                ptext(cx0 + 1.2, yy + 2.5, nm, 1.25, ACC, 800, "start", 0.45)
                continue
            if nm == "REPEAT TYPE":
                out.insert(-1, f'<rect x="{cx0:.2f}" y="{yy + 0.15:.2f}" width="{CW:.2f}" height="{RH - 0.3:.2f}" rx="0.6" fill="{tint(0.45)}"/>')
            ptext(cx0 + 1.2, yy + 2.45, nm, 1.45, "#ffffff" if nm == "REPEAT TYPE" else TXT, 700, "start", 0.2)
            ptext(cx0 + CW - 1.2, yy + 2.4, sub, 1.15, DIM, 500, "end", 0.1)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2400", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
