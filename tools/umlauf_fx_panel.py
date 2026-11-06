#!/usr/bin/env python3
"""Panel of UMLAUF FX, the expander of UMLAUF: envelope, voice, arpeggiator with 16 steps, effects (accent: sith red).

Usage:  umlauf_fx_panel.py real                   -> res/UmlaufFx.svg (text as paths) + src/umlauf/UmlaufFxLayout.hpp
        umlauf_fx_panel.py preview OUT.png [RACK] -> preview with Rack components and mock display content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (32 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display (registration, solo sound, voices).
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

HP = 36
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with src/umlauf/Umlauf.cpp (ACCENT)
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


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.6}" y="{y - 4.75}" width="9.2" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.6, TXT, 600, "middle", 0.25)


# ---------------------------------------------------------------- frame, header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "UMLAUF FX", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("UMLAUF FX", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · ENV · VOICE · ARP · FX", 1.76, DIM, 600, "start",
     0.47)
e(f'<rect x="{W - 27.1:.2f}" y="5.55" width="11.5" height="1.08" rx="0.54" fill="{ACC}"/>')

# ---------------------------------------------------------------- envelope + voice
section(3, 10.5, 58, 33, "ENVELOPE")
for i, (n, l) in enumerate([("ATK", "ATTACK"), ("DEC", "DECAY"), ("SUS", "SUSTAIN"), ("REL", "RELEASE")]):
    knob(n, 11 + i * 14, 25.0, l, "knob", hot=(n == "ATK"))
knob("VELO", 18, 36.8, "VELOCITY", "small") if False else None
section(63, 10.5, W - 66, 33, "VOICE")
lamps("UNISON", 79, 7.5, 21.0, ["1", "2", "4", "8"], "UNISON")
for i, (n, l) in enumerate([("DETUNE", "DETUNE"), ("GLIDE", "GLIDE"), ("TOCT", "OCTAVE"), ("VELO", "VELOCITY")]):
    knob(n, 70 + i * 13, 34.0, l, "small", hot=(n == "DETUNE"))
for i, (n, l) in enumerate([("VSPD", "VIB RATE"), ("VDEP", "VIBRATO"), ("TSPD", "TREM RATE"), ("TDEP", "TREMOLO")]):
    knob(n, 124 + i * 14, 34.0, l, "small")
lamps("VOICES", 136, 9, 21.0, ["8", "16"], "VOICES")
button("RETRIG", 168, 21.0, "") if False else None

# ---------------------------------------------------------------- arpeggiator
section(3, 45.5, W - 6, 35, "ARPEGGIATOR")
button("ARPON", 13, 57.0, "ON")
button("LATCH", 13, 72.5, "LATCH")
lamps("AMODE", 35, 9.2, 54.5, ["UP", "DOWN", "U/D", "RND", "ORDER", "CHORD"], "")
for i, (n, l) in enumerate([("ARATE", "RATE"), ("AOCT", "OCT"), ("ALEN", "LENGTH"), ("ASTEPS", "STEPS"),
                            ("AHUM", "HUMAN")]):
    knob(n, 99 + i * 0 if False else 98.0 + i * 15.6, 57.5, l, "small", hot=(n == "ARATE"))
STEPX = [27 + i * (W - 36 - 27) / 15 for i in range(16)]
for i, x in enumerate(STEPX):
    comps.append(("step", f"STEP{i + 1}", x, 73.0))
    text(x, 78.6, str(i + 1), 1.5, DIM if i % 4 else TXT, 700, "middle", 0.2)
for k in range(1, 4):
    xx = (STEPX[4 * k - 1] + STEPX[4 * k]) / 2
    e(f'<line x1="{xx:.2f}" y1="69.5" x2="{xx:.2f}" y2="79" stroke="{LINE}" stroke-width="0.3"/>')
text(W - 9.5, 74.0, "OFF ON", 1.3, DIM, 600, "middle", 0.15)
text(W - 9.5, 76.4, "ACC TIE", 1.3, DIM, 600, "middle", 0.15)

# ---------------------------------------------------------------- effects
section(3, 82.5, W - 6, 21, "FX · MASTER")
for i, (n, l) in enumerate([("DRIVE", "DRIVE"), ("CHORUS", "CHORUS"), ("CHSPD", "CH RATE"), ("DELAY", "DELAY"),
                            ("DTIME", "TIME"), ("DFB", "FEEDBACK"), ("REVERB", "REVERB"), ("RSIZE", "SIZE"),
                            ("MRES", "M.RES"), ("VOLUME", "VOLUME")]):
    knob(n, 11 + i * 18.0, 95.0, l, "small", hot=(n == "VOLUME"))

# ---------------------------------------------------------------- in
e(f'<rect x="3" y="104.5" width="{W - 6:.2f}" height="23.3" rx="1.2" fill="{PLATE}"/>')
X = [17 + i * (W - 34) / 9 for i in range(10)]
ROW1 = [("IN_ATK", "ATTACK"), ("IN_DEC", "DECAY"), ("IN_SUS", "SUSTAIN"), ("IN_REL", "RELEASE"), ("IN_DETUNE", "DETUNE"),
        ("IN_GLIDE", "GLIDE"), ("IN_VIB", "VIBRATO"), ("IN_TREM", "TREMOLO"), ("IN_ARPON", "ARP ON"),
        ("IN_ARATE", "ARP RATE")]
ROW2 = [("IN_LATCH", "LATCH"), ("IN_AOCT", "ARP OCT"), ("IN_DRIVE", "DRIVE"), ("IN_CHORUS", "CHORUS"),
        ("IN_DELAY", "DELAY"), ("IN_DTIME", "TIME"), ("IN_REVERB", "REVERB"), ("IN_RSIZE", "SIZE"), ("IN_MRES", "M.RES"),
        ("IN_VOL", "VOLUME")]
for i, (n, l) in enumerate(ROW1):
    jack(n, X[i], 109.2, l)
for i, (n, l) in enumerate(ROW2):
    jack(n, X[i], 120.2, l)
e(f'<rect x="{X[0] - 4.2:.2f}" y="105.1" width="{X[7] - X[0] + 8.4:.2f}" height="0.5" rx="0.25" fill="{DIM}" fill-opacity="0.7"/>')
e(f'<rect x="{X[8] - 4.2:.2f}" y="105.1" width="{X[9] - X[8] + 8.4:.2f}" height="0.5" rx="0.25" fill="{ACC}" fill-opacity="0.7"/>')

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "UmlaufFx.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/umlauf_fx_panel.py - do not edit", "#pragma once", "namespace uflayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, dx) in extra.items():
        lines.append(f"static const float {n}[3] = {{{x:.3f}f, {y:.3f}f, {dx:.3f}f}};  // first lamp x, y, distance")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "umlauf"), exist_ok=True)
    open(os.path.join(ROOT, "src", "umlauf", "UmlaufFxLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/UmlaufFx.svg and src/umlauf/UmlaufFxLayout.hpp")
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
    elif kind == "step":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)
for n, on in (("UNISON", 1), ("VOICES", 1), ("AMODE", 0)):
    x0, y0, ddx = extra[n]
    out.insert(-1, f'<circle cx="{x0 + on * ddx}" cy="{y0}" r="1.35" fill="{ACC}"/>')
for i, x in enumerate(STEPX):
    st = [1, 0, 2, 1, 1, 3, 1, 0, 2, 1, 0, 1, 1, 3, 2, 1][i]
    col = {0: None, 1: ACC, 2: "#ffb3bb", 3: "#7a2632"}[st]
    if col:
        out.insert(-1, f'<circle cx="{x:.2f}" cy="73.0" r="2.3" fill="{col}" fill-opacity="0.85"/>')
for x, y in ((13, 57.0),):
    out.insert(-1, f'<circle cx="{x}" cy="{y}" r="2.3" fill="{ACC}" fill-opacity="0.85"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1400", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
