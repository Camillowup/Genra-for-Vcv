#!/usr/bin/env python3
"""Panel of GENRA FX, the send effects for MIX: REVERB (send A) and DELAY (send B).

Usage:  genrafx_panel.py real                 -> res/GenraFx.svg (text as paths) + src/genrafx/GenraFxLayout.hpp
        genrafx_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (24 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display, the meters and the text in the two fields.
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
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 24
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/genrafx/GenraFx.cpp (ACCENT): an ice blue, the colour of the send effects
ACC = "#8fd3ff"

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
text(10.5, 7.2, "GENRA FX", 3.6, TITLE, 800, "start", 0.8)
text(10.5 + width("GENRA FX", 3.6, 800, 0.8) + 4, 7.0, "SEND EFFECTS · FOR MIX A / B", 1.5, DIM, 600, "start", 0.4)
text(W - 17.5, 7.0, "SYNC", 1.4, TXT, 700, "end", 0.4)
e(f'<rect x="{W - 16.2:.2f}" y="5.3" width="5" height="1.08" rx="0.54" fill="#2a2b33"/>')  # lit by the module while synced

# display: tempo and what both effects do
e(f'<rect x="3" y="10.5" width="{W - 6:.2f}" height="13" rx="1.2" fill="{DARK}" stroke="{LINE}" stroke-width="0.4"/>')
extra_disp = (3, 10.5, W - 6, 13)

SW_ = (W - 6 - 2) / 2
XA, XB = 3, 3 + SW_ + 2
Y = 26
SH = 66

def sec(x, title, sub):
    section(x, Y, SW_, SH, title)
    text(x + SW_ / 2, Y + 8.2, sub, 1.3, DIM, 700, "middle", 0.35)

# ---- REVERB (send A)
sec(XA, "REVERB", "RETURN A")
cx = XA + SW_ / 2
field("V_TYPEF", XA + 4, Y + 11, SW_ - 8)
knob("V_DECAY", cx, Y + 27, "DECAY", "large", hot=True)
knob("V_SIZE", XA + 9, Y + 27, "SIZE", "small")
knob("V_PRE", XA + SW_ - 9, Y + 27, "PRE-DELAY", "small")
for k, (n, l) in enumerate([("V_DAMP", "DAMP"), ("V_WIDTH", "WIDTH"), ("V_MOD", "SHIMMER"), ("V_LVL", "LEVEL")]):
    knob(n, XA + 8 + k * (SW_ - 16) / 3, Y + 44, l, "small")
button("V_FREEZE", cx, Y + 57.5, "FREEZE")

# ---- DELAY (send B)
sec(XB, "DELAY", "RETURN B")
cx = XB + SW_ / 2
field("D_TIMEF", XB + 4, Y + 11, SW_ - 8)
knob("D_TIME", cx, Y + 27, "TIME", "large", hot=True)
knob("D_FB", XB + 9, Y + 27, "FEEDBACK", "small")
knob("D_TONE", XB + SW_ - 9, Y + 27, "TONE", "small")
for k, (n, l) in enumerate([("D_TAPE", "TAPE"), ("D_DUCK", "DUCK"), ("D_WIDTH", "WIDTH"), ("D_LVL", "LEVEL")]):
    knob(n, XB + 8 + k * (SW_ - 16) / 3, Y + 44, l, "small")
lamps("D_MODE", cx - 6, 12, Y + 56, ["STEREO", "PING-PONG"], None)

# ---------------------------------------------------------------- jacks: one tray per effect
TY = Y + SH + 2.5
TH = H - TY - 4
for x0, pre, cvs in ((XA, "A", [("CV_DECAY", "DECAY"), ("CV_FREEZE", "FREEZE")]),
                     (XB, "B", [("CV_TIME", "TIME"), ("CV_FB", "FDBK")])):
    e(f'<rect x="{x0}" y="{TY:.2f}" width="{SW_:.2f}" height="{TH:.2f}" rx="1.2" fill="#141519"/>')
    text(x0 + 3, TY + 4, "IN  ·  CV", 1.3, DIM, 700, "start", 0.35)
    text(x0 + SW_ - 3, TY + 4, "OUT", 1.3, DIM, 700, "end", 0.35)
    p = (SW_ - 6) / 5
    xs = [x0 + 3 + p * (k + 0.5) for k in range(5)]
    jack(f"IN_{pre}L", xs[0], TY + 11, f"IN {pre} L")
    jack(f"IN_{pre}R", xs[1], TY + 11, "R")
    jack(cvs[0][0], xs[2], TY + 11, cvs[0][1])
    jack(cvs[1][0], xs[2], TY + 23, cvs[1][1])
    jack(f"OUT_{pre}L", xs[3], TY + 11, "L", True)
    jack(f"OUT_{pre}R", xs[4], TY + 11, "R", True)
e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GenraFx.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/panels/genrafx_panel.py - do not edit", "#pragma once", "namespace gfxl {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    for n, (x, y, w, h) in fields.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};  // field x, y, w, h")
    dx, dy, dw, dh = extra_disp
    lines.append(f"static const float DISP[4] = {{{dx:.3f}f, {dy:.3f}f, {dw:.3f}f, {dh:.3f}f}};  // the display")
    lines.append(f"static const float SYNC[4] = {{{W - 16.2:.3f}f, 5.300f, 5.000f, 1.080f}};  // the SYNC bar")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "genrafx"), exist_ok=True)
    open(os.path.join(ROOT, "src", "genrafx", "GenraFxLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GenraFx.svg and src/genrafx/GenraFxLayout.hpp")
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
x, y, w, h = fields["V_TYPEF"]
ptext(x + 2, y + 3.75, "HALL", 1.8, TXT, 700, "start", 0.35)
ptext(x + w - 6, y + 3.65, "3.4 s", 1.5, DIM, 700, "end", 0.3)
x, y, w, h = fields["D_TIMEF"]
ptext(x + 2, y + 3.75, "3/16  DOTTED", 1.8, TXT, 700, "start", 0.35)
ptext(x + w - 6, y + 3.65, "363 ms", 1.5, DIM, 700, "end", 0.3)
dx, dy, dw, dh = extra_disp
ptext(dx + 3, dy + 5, "124 BPM  ·  FROM GENRA", 1.7, ACC, 800, "start", 0.4)
ptext(dx + 3, dy + 9.6, "A  ‹ MIX SEND A      B  ‹ MIX SEND B", 1.45, TXT, 700, "start", 0.35)
# little meters
for k, lvl in enumerate([0.6, 0.45]):
    out.insert(-1, f'<rect x="{dx + dw - 40:.2f}" y="{dy + 3 + k * 4.5:.2f}" width="36" height="1.4" rx="0.7" fill="#1d1f26"/>')
    out.insert(-1, f'<rect x="{dx + dw - 40:.2f}" y="{dy + 3 + k * 4.5:.2f}" width="{36 * lvl:.1f}" height="1.4" rx="0.7" fill="{ACC}"/>')
    ptext(dx + dw - 42, dy + 4.3 + k * 4.5, "AB"[k], 1.3, DIM, 800, "end", 0.3)
# lit lamp
x0_, y0_, ddx = extra["D_MODE"]
out.insert(-1, f'<circle cx="{x0_ + ddx:.2f}" cy="{y0_}" r="1.25" fill="{ACC}"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2200", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
