#!/usr/bin/env python3
"""Panel of GRUND FX, the expander that docks right of GRUND: the details behind its effects and amp.

Usage:  grundfx_panel.py real                 -> res/GrundFx.svg (text as paths) + src/grund/GrundFxLayout.hpp
        grundfx_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock display content
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

HP = 14
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


# ---------------------------------------------------------------- frame, header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(10.5, 7.2, "GRUND FX", 3.6, TITLE, 800, "start", 0.8)

text(W - 17.5, 7.0, "LINK", 1.4, TXT, 700, "end", 0.4)
e(f'<rect x="{W - 16.2:.2f}" y="5.3" width="5" height="1.08" rx="0.54" fill="{ACC}"/>')


def fxsec(x, y, w, h, title, name):
    """A section with its on/off button in the corner (as on K-WASUB FX)."""
    section(x, y, w, h, title)
    comps.append(("button", name, x + 4.2, y + 4.4))


# ---------------------------------------------------------------- 14 HP: one section per row, its jacks in it
RH, RG = 22.6, 0.8
Y0 = 10.6
KY = 13.4  # knob / jack row inside a section
SW = W - 6
P = 10.4   # item pitch
X = [9.2 + k * P for k in range(6)]


def cv(name, x, y, label):
    """A CV input inside its section, on a small dark tray (inputs are told apart from knobs at a glance)."""
    e(f'<rect x="{x - 4.4:.2f}" y="{y - 4.5:.2f}" width="8.8" height="11.0" rx="0.9" fill="#141519"/>')
    jack(name, x, y, label)


rows = []
y = Y0
for k in range(5):
    rows.append(y)
    y += RH + RG
Y1, Y2, Y3, Y4, Y5 = rows

fxsec(3, Y1, SW, RH, "COMP", "COMP_ON")
for k, (n, l) in enumerate([("C_THRESH", "THRESH"), ("C_RATIO", "RATIO"), ("C_ATK", "ATTACK"), ("C_REL", "RELEASE")]):
    knob(n, X[k], Y1 + KY, l, "small")
cv("IN_SC", X[4], Y1 + KY, "SC")
cv("IN_COMP", X[5], Y1 + KY, "COMP")
e(f'<rect x="{W - 27:.2f}" y="{Y1 + 3.4:.2f}" width="20" height="1.1" rx="0.55" fill="{DARK}"/>')
text(W - 7, Y1 + 6.9, "GR", 1.2, DIM, 700, "end", 0.3)  # gain reduction

fxsec(3, Y2, SW, RH, "DRIVE", "DRIVE_ON")
field("D_TYPEF", 5.5, Y2 + KY - 2.7, 27)
knob("D_TONE", X[3], Y2 + KY, "TONE", "small")
knob("D_MIX", X[4], Y2 + KY, "MIX", "small")
cv("IN_DRIVE", X[5], Y2 + KY, "DRIVE")

fxsec(3, Y3, SW, RH, "EQ", "EQ_ON")
for k, (n, l) in enumerate([("E_HPF", "HPF"), ("E_LOW", "LOW"), ("E_MID", "MID"), ("E_HIGH", "HIGH")]):
    knob(n, X[k], Y3 + KY, l, "small")
cv("IN_EQLO", X[4], Y3 + KY, "LOW")
cv("IN_EQHI", X[5], Y3 + KY, "HIGH")

fxsec(3, Y4, SW, RH, "CHORUS", "CHO_ON")
knob("CH_RATE", X[1], Y4 + KY, "RATE", "small")
knob("CH_DEPTH", X[2], Y4 + KY, "DEPTH", "small")
cv("IN_CHO", X[4], Y4 + KY, "CHORUS")

fxsec(3, Y5, SW, RH, "REVERB", "VERB_ON")
for k, (n, l) in enumerate([("V_SIZE", "SIZE"), ("V_DAMP", "DAMP"), ("V_PRE", "PRE")]):
    knob(n, X[k + 1], Y5 + KY, l, "small")  # (the corners belong to the screws)
cv("IN_VERB", X[4], Y5 + KY, "VERB")

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GrundFx.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/grundfx_panel.py - do not edit", "#pragma once", "namespace gfxlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for n, (x, y, w, h) in fields.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};  // field x, y, w, h")
    lines.append(f"static const float GR[4] = {{{W - 27:.3f}f, {Y1 + 3.4:.3f}f, 20.000f, 1.100f}};  // gain reduction meter")
    lines.append(f"static const float LINK[4] = {{{W - 16.2:.3f}f, 5.300f, 5.000f, 1.080f}};  // the LINK bar")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "grund"), exist_ok=True)
    open(os.path.join(ROOT, "src", "grund", "GrundFxLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GrundFx.svg and src/grund/GrundFxLayout.hpp")
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
out.insert(-1, f'<rect x="{W - 27:.2f}" y="{Y1 + 3.4:.2f}" width="8" height="1.3" rx="0.65" fill="{ACC}"/>')
x, y, w, h = fields["D_TYPEF"]
ptext(x + 2, y + 3.75, "TUBE", 1.8, TXT, 700, "start", 0.35)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2200", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
