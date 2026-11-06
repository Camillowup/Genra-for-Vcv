#!/usr/bin/env python3
"""Panel of MIX, the 4-track stereo mixer in the GENRA look: one strip per GENRA track in its colour, a dark master.

Usage:  mix_panel.py real                   -> res/Mix.svg (text as paths) + src/mix/MixLayout.hpp
        mix_panel.py preview OUT.png [RACK] -> preview with Rack components and mock meters
                                               (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (28 HP, height 128.5 mm). The C++ side converts with mm2px().
"""
import os
import re
import subprocess
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 28
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
ACC = "#8a8d99"  # MIX has no accent of its own: the strips carry the track colours

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


# ---------------------------------------------------------------- helpers for the strips
TRACKS = [("DRUMS", "#7b6cff"), ("BASS", "#b8f23a"), ("MELODY", "#3dc9ff"), ("PADS", "#e0779e")]
MASTER_BAR, MASTER_TXT = "#4a4c57", "#b4b7c2"  # the master strip stays dark
OUT_TINT = "#2c4a37"                              # green, as the meters


def mixc(col, f):
    c = [int(col[i:i + 2], 16) for i in (1, 3, 5)]
    p = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(p[i] + (c[i] - p[i]) * f):02x}" for i in range(3))


def rknob(name, x, y, label, kind="small", ring=None):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x:.2f}" cy="{y}" r="{r:.2f}" fill="none" stroke="{ring or RING}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    text(x, y + R[kind] + 3.3, label, 1.7, TXT, 600, "middle", 0.3)


def ljack(name, x, y, label, out_=False, tint=None):
    comps.append(("output" if out_ else "input", name, x, y))
    if tint:
        e(f'<rect x="{x - 4.6:.2f}" y="{y - 4.75}" width="9.2" height="11.4" rx="0.9" fill="{tint}"/>')
    text(x, y + 5.7, label, 1.55, TXT if tint else DIM, 600, "middle", 0.25)


def meter(name, x, y0, y1, n=1):
    """A slot for the meter segments the module draws (n bars, 3 mm apart)."""
    areas[name] = (x - 1.2, y0, 1.2 * 2 + (n - 1) * 3.0, y1 - y0)
    for i in range(n):
        e(f'<rect x="{x - 1.2 + i * 3.0:.2f}" y="{y0}" width="2.4" height="{y1 - y0}" rx="0.4" fill="#111215"/>')


areas = {}

# ---------------------------------------------------------------- frame and header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "MIX", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("MIX", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · 4-TRACK STEREO MIXER", 1.76, DIM, 600, "start", 0.47)
for i, (_, c) in enumerate(TRACKS):
    e(f'<rect x="{W - 42 + i * 8:.2f}" y="5.55" width="6.4" height="1.08" rx="0.54" fill="{c}"/>')

# ---------------------------------------------------------------- four channel strips
SW, SP = 23, 24  # strip width, strip pitch
for i, (nm, col) in enumerate(TRACKS):
    n = i + 1
    x0 = 3 + i * SP
    cx = x0 + SW / 2
    e(f'<rect x="{x0}" y="11" width="{SW}" height="114.5" rx="1.2" fill="{PLATE}"/>')
    e(f'<rect x="{x0}" y="11" width="{SW}" height="0.88" rx="0.44" fill="{col}"/>')
    text(cx, 16.6, nm, 2.03, col, 800, "middle", 0.6)
    text(cx, 20.2, f"CH {n}", 1.45, DIM, 600, "middle", 0.4)
    rknob(f"HIGH{n}", cx - 5.5, 29, "HIGH")
    rknob(f"LOW{n}", cx + 5.5, 29, "LOW")
    rknob(f"SEND{n}", cx - 5.5, 43, "SEND A")
    rknob(f"SENDB{n}", cx + 5.5, 43, "SEND B")
    rknob(f"DUCK{n}", cx - 5.5, 57, "DUCK")
    rknob(f"PAN{n}", cx + 5.5, 57, "PAN")
    rknob(f"LEVEL{n}", cx - 3, 75, "LEVEL", "large", col)
    meter(f"VU{n}", cx + 8, 67, 83.5)
    button(f"MUTE{n}", cx - 5.5, 92, "MUTE")
    button(f"SOLO{n}", cx + 5.5, 92, "SOLO")
    ljack(f"IN_L{n}", cx - 5.5, 105, "IN L")
    ljack(f"IN_R{n}", cx + 5.5, 105, "IN R")
    ljack(f"IN_LVL{n}", cx - 5.5, 117.5, "LVL")
    ljack(f"IN_PAN{n}", cx + 5.5, 117.5, "PAN")

# ---------------------------------------------------------------- master (dark)
mx = 3 + 4 * SP
mw = W - 3 - mx
mc = mx + mw / 2
c4 = [mc - 14.25, mc - 4.75, mc + 4.75, mc + 14.25]  # four columns
e(f'<rect x="{mx}" y="11" width="{mw:.2f}" height="114.5" rx="1.2" fill="{PLATE}"/>')
e(f'<rect x="{mx}" y="11" width="{mw:.2f}" height="0.88" rx="0.44" fill="{MASTER_BAR}"/>')
text(mc, 16.6, "MASTER", 2.03, MASTER_TXT, 800, "middle", 0.6)
text(mc, 20.2, "STEREO", 1.45, DIM, 600, "middle", 0.4)
rknob("DUCK", c4[0], 29, "DUCK")
rknob("TIME", c4[1], 29, "TIME")
rknob("WIDTH", c4[2], 29, "WIDTH")
rknob("DRIVE", c4[3], 29, "DRIVE")
rknob("SENDM", c4[0], 43, "SEND A")
rknob("RETURN", c4[1], 43, "RET A")
rknob("SENDMB", c4[2], 43, "SEND B")
rknob("RETURNB", c4[3], 43, "RET B")
rknob("MASTER", mc - 5, 66, "MASTER", "large")
meter("VUM", mc + 8, 57, 78, 2)
text(mc + 8, 81, "L", 1.4, DIM, 600, "middle", 0.2)
text(mc + 11, 81, "R", 1.4, DIM, 600, "middle", 0.2)
ljack("RET_L", c4[0], 92, "RET A L")
ljack("RET_R", c4[1], 92, "RET A R")
ljack("RETB_L", c4[2], 92, "RET B L")
ljack("RETB_R", c4[3], 92, "RET B R")
e(f'<line x1="{mx + 3}" y1="98.6" x2="{mx + mw - 3:.2f}" y2="98.6" stroke="{LINE}" stroke-width="0.3"/>')
ST = mixc("#8a8d99", 0.25)
ljack("SEND_L", c4[0], 105, "SND A L", True, ST)
ljack("SEND_R", c4[1], 105, "SND A R", True, ST)
ljack("SENDB_L", c4[2], 105, "SND B L", True, ST)
ljack("SENDB_R", c4[3], 105, "SND B R", True, ST)
ljack("IN_DUCK", c4[0], 117.5, "DUCK")
ljack("OUT_L", c4[2], 117.5, "OUT L", True, OUT_TINT)
ljack("OUT_R", c4[3], 117.5, "OUT R", True, OUT_TINT)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "Mix.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/mix_panel.py - do not edit", "#pragma once", "namespace mxlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, w, h) in areas.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "mix"), exist_ok=True)
    open(os.path.join(ROOT, "src", "mix", "MixLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/Mix.svg and src/mix/MixLayout.hpp")
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


def fake(kind, x, y):
    """Stand-ins for the Rack components when no Rack source tree is at hand."""
    if kind in KIND:
        r = R[kind]
        out.insert(-1, f'<circle cx="{x:.2f}" cy="{y}" r="{r:.2f}" fill="#141417" stroke="#2c2d33" stroke-width="0.4"/>'
                       f'<line x1="{x:.2f}" y1="{y - r * 0.9:.2f}" x2="{x:.2f}" y2="{y - r * 0.35:.2f}" stroke="#eee" '
                       f'stroke-width="0.6" stroke-linecap="round"/>')
    elif kind == "button":
        out.insert(-1, f'<circle cx="{x:.2f}" cy="{y}" r="3.2" fill="#0d0e11"/><circle cx="{x:.2f}" cy="{y}" r="2.4" fill="#555"/>')
    else:
        out.insert(-1, f'<circle cx="{x:.2f}" cy="{y}" r="3.1" fill="#c9cbd1"/><circle cx="{x:.2f}" cy="{y}" r="2.2" '
                       f'fill="#8d9097"/><circle cx="{x:.2f}" cy="{y}" r="1.45" fill="#050505"/>')


HAVE_CL = os.path.isdir(CL)
for kind, n, x, y in comps:
    if not HAVE_CL:
        fake(kind, x, y)
    elif kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        if HAVE_CL:
            img("ScrewBlack", sx_, sy)
        else:
            out.insert(-1, f'<circle cx="{sx_}" cy="{sy}" r="1.6" fill="#3a3b42"/>')
LVLS = [9, 7, 6, 8]
for i, (_, col) in enumerate(TRACKS):
    x, y, w, h = areas[f"VU{i + 1}"]
    seg = h / 12
    for k in range(12):
        c = "#ff4f4f" if k >= 10 else ("#ffc24d" if k >= 8 else col)
        out.insert(-1, f'<rect x="{x + 0.3:.2f}" y="{y + h - (k + 1) * seg + 0.25:.2f}" width="1.8" height="{seg - 0.5:.2f}" '
                       f'rx="0.25" fill="{c if k < LVLS[i] else "#2a2b33"}"/>')
x, y, w, h = areas["VUM"]
seg = h / 12
for b, lv in ((0, 8), (1, 7)):
    for k in range(12):
        c = "#ff4f4f" if k >= 10 else ("#ffc24d" if k >= 8 else "#5dff8a")
        out.insert(-1, f'<rect x="{x + 0.3 + b * 3:.2f}" y="{y + h - (k + 1) * seg + 0.25:.2f}" width="1.8" '
                       f'height="{seg - 0.5:.2f}" rx="0.25" fill="{c if k < lv else "#2a2b33"}"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1400", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
