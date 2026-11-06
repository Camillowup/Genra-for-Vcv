#!/usr/bin/env python3
"""Panel of GRUND, the bass voice for GENRA's bass track (upright, electric, synth basses), in the GENRA look (accent: green).

Usage:  grund_panel.py real                   -> res/Grund.svg (text as paths) + src/grund/GrundLayout.hpp
        grund_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (30 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display and the text in the two fields (model, filter).
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import genra_strip, SLOT  # noqa: E402

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 30
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/grund/Grund.cpp (ACCENT): the bass track's green
ACC = "#b8f23a"

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


# ---------------------------------------------------------------- frame, header, display
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "GRUND", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("GRUND", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · BASS VOICE", 1.76, DIM, 600, "start", 0.47)
# presets: a field in the header (as on GENRA MONO), the accent bar after it
field("PRESETF", W - 70, 3.4, 46)
e(f'<rect x="{W - 21.5:.2f}" y="5.55" width="8" height="1.08" rx="0.54" fill="{ACC}"/>')

DISPLAY = (3, 10.5, W - 6, 15)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]:.2f}" height="{DISPLAY[3]}" rx="1.6" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')

# ---------------------------------------------------------------- voice: which bass, how it is struck
section(3, 27, 72, 47, "VOICE")
field("MODELF", 6, 33.5, 66)
knob("TIMBRE", 16, 51, "TIMBRE", "large", hot=True)
knob("BITE", 33, 48, "BITE")
knob("BODY", 47, 48, "BODY")
knob("SUB", 61, 48, "SUB")
lamps("VMODE", 11, 10, 67.5, ["MONO", "LEGATO"], "")
knob("MUTE", 35, 65.5, "MUTE", "small")
knob("GLIDE", 48, 65.5, "GLIDE", "small")
knob("ACCENT", 61, 65.5, "ACCENT", "small")

# ---------------------------------------------------------------- amp: amp / cab type, tone, drive, compressor
section(77, 27, W - 80, 47, "AMP")
field("AMPF", 80, 33.5, W - 86)
knob("CUTOFF", 91, 51, "TONE", "large", hot=True)
knob("RES", 108, 48, "RES")
knob("FENV", 122, 48, "ENV")
knob("FDEC", 136, 48, "F.DEC")
knob("DRIVE", 108, 65.5, "DRIVE", "small")
knob("COMP", 122, 65.5, "COMP", "small")
knob("KEY", 136, 65.5, "KEY", "small")

# ---------------------------------------------------------------- envelope, string, fx
section(3, 76, 72, 26, "ENVELOPE")
for i, n in enumerate(["ATK", "DEC", "SUS", "REL"]):
    knob(n, 14 + i * 16, 89.5, "ADSR"[i])
section(77, 76, W - 80, 26, "FX")
for i, (n, l) in enumerate([("CHORUS", "CHORUS"), ("OCTAVE", "OCTAVE"), ("WIDTH", "WIDTH"), ("VERB", "VERB")]):
    knob(n, 87 + i * 16, 89.5, l)

# ---------------------------------------------------------------- in / out: jacks grouped on dark strips
e(f'<rect x="3" y="103.6" width="{W - 6:.2f}" height="24.2" rx="1.2" fill="{PLATE}"/>')
G1 = [[("IN_MODEL", "MODEL"), ("IN_TIMBRE", "TIMBRE"), ("IN_BITE", "BITE")], [("IN_CUT", "TONE"), ("IN_MUTE", "MUTE")]]
G2 = [[("IN_DRIVE", "DRIVE"), ("IN_SUB", "SUB")], [("IN_ATK", "ATK"), ("IN_REL", "REL")],
      [("OUT_ENV", "ENV"), ("OUT_L", "L"), ("OUT_R", "R")]]


def row(groups, y, ytop, outs_last, x0=None):
    sl, gap = 10.8, 2.2
    n = sum(len(g) for g in groups)
    gx = (W - (n * sl + (len(groups) - 1) * gap)) / 2 if x0 is None else x0
    for gi, g in enumerate(groups):
        w = len(g) * sl
        e(f'<rect x="{gx + 0.3:.2f}" y="{ytop}" width="{w - 0.6:.2f}" height="11.4" rx="1" fill="#141519"/>')
        for i, (nm, lb) in enumerate(g):
            jack(nm, gx + sl / 2 + i * sl, y, lb, outs_last and gi == len(groups) - 1)
        gx += w + gap


# the GENRA strip (as on every instrument); a bass has no tempo effects, so no CLOCK / RESET
x = genra_strip(jack, e, 6, 108.4, {"V/OCT": "VOCT", "GATE": "GATE", "VEL": "VEL", "ACC": "ACC", "SLIDE": "SLIDE",
                                     "MOD": "IN_MOD"}, ACC)
row(G1, 108.4, 103.5, False, x + 3.2)
row(G2, 120.3, 116.3, True)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "Grund.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/grund_panel.py - do not edit", "#pragma once", "namespace glayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for n, (x, y, w, h) in fields.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};  // field x, y, w, h")
    lines.append(f"static const float DISPLAY[4] = {{{DISPLAY[0]:.3f}f, {DISPLAY[1]:.3f}f, {DISPLAY[2]:.3f}f, "
                 f"{DISPLAY[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "grund"), exist_ok=True)
    open(os.path.join(ROOT, "src", "grund", "GrundLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/Grund.svg and src/grund/GrundLayout.hpp")
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
for n, on in (("VMODE", 0),):
    x0, y0, ddx = extra[n]
    out.insert(-1, f'<circle cx="{x0 + on * ddx}" cy="{y0}" r="1.25" fill="{ACC}"/>')
for n, (a, b) in (("MODELF", ("UPRIGHT", "PLUCKED DOUBLE BASS")), ("AMPF", ("FLIP-TOP", "TUBE AMP · 1×15 CAB")), ("PRESETF", ("07  JAZZ UPRIGHT", "LO-FI · JAZZ"))):
    x, y, w, h = fields[n]
    ptext(x + 2, y + 3.75, a, 1.9, TXT, 700, "start", 0.35)
    ptext(x + w - 6, y + 3.6, b, 1.4, DIM, 600, "end", 0.25)
dx, dy, dw, dh = DISPLAY
ptext(dx + 4, dy + 4.5, "MODEL 1 / 10", 1.5, DIM, 700, "start", 0.35)
ptext(dx + 4, dy + 10.5, "UPRIGHT", 4.4, "#ffffff", 800, "start", 0.3)
ptext(dx + 4, dy + 13.8, "PLUCKED DOUBLE BASS", 1.5, ACC, 700, "start", 0.4)
import math
sx, sw, cy = 60, 46, dy + 7
pts = " ".join(f"{sx + i * sw / 120:.2f},{cy - 4.2 * (math.sin(2 * math.pi * i / 40) * 0.8 + 0.25 * math.sin(4 * math.pi * i / 40 + 0.5)) * math.exp(-i / 240):.2f}" for i in range(121))
out.insert(-1, f'<polyline points="{pts}" fill="none" stroke="{ACC}" stroke-width="0.45"/>')
ptext(sx, dy + 13.8, "FLIP-TOP · COMP · MONO", 1.5, DIM, 700, "start", 0.3)
ptext(dx + dw - 4, dy + 4.5, "BASS", 1.5, DIM, 700, "end", 0.35)
ptext(dx + dw - 4, dy + 10.5, "E1", 4.4, "#ffffff", 800, "end", 0.3)
ptext(dx + dw - 4, dy + 13.8, "GLIDE · ACC", 1.5, ACC, 700, "end", 0.3)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2200", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
