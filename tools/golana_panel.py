#!/usr/bin/env python3
"""Panel of GOLANA, the analog-style poly synth after the FM-1 firmware Golana (Gadlele, GPL-3.0), in the GENRA look.

Usage:  golana_panel.py real                   -> instruments/res/Golana.svg (text as paths) + instruments/src/golana/GolanaLayout.hpp
        golana_panel.py preview OUT.png [RACK]  -> preview with Rack components and mock field/screen content
                                                   (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (42 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the filter name in the dropdown field.
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
MENU = PREVIEW and "menu" in sys.argv[3:]
PMENU = PREVIEW and "presets" in sys.argv[3:]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RACK = sys.argv[3] if PREVIEW and len(sys.argv) > 3 and sys.argv[3] not in ("menu", "presets") else os.path.join(ROOT, "..", "ext", "Rack")
CL = os.path.join(RACK, "res", "ComponentLibrary")

HP = 42
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# keep in step with instruments/src/golana/Golana.cpp (ACCENT): the melody track's cyan (= FUNKE)
ACC, ACC2, INK = "#3dc9ff", "#2687b0", "#0c2a3a"

# component radii (mm)
R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob", "large": "RoundLargeBlackKnob"}

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {400: "Inter-Regular.otf", 500: "Inter-Medium.otf", 600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf",
         800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps = [], []


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


def tint(f):
    """The accent mixed onto the plate (f: the accent's share), as on the JIANT voices and MONK."""
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


def switch(name, x, y, top, bot):
    comps.append(("switch", name, x, y))
    text(x, y - 4.5, top, 1.5, TXT, 600, "middle", 0.25)
    text(x, y + 5.9, bot, 1.5, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x - 4.25}" y="{y - 4.75}" width="8.5" height="11.6" rx="0.9" fill="{tint(0.32)}"/>')
    text(x, y + 5.7, label, 1.63, TXT, 600, "middle", 0.3)


def adsr(x, y, w=10, h=2.6):
    e(f'<polyline points="{x},{y} {x + w * .18:.2f},{y - h} {x + w * .38:.2f},{y - h * .45:.2f} '
      f'{x + w * .75:.2f},{y - h * .45:.2f} {x + w},{y}" fill="none" stroke="{ACC}" stroke-width="0.35" '
      f'stroke-linejoin="round" stroke-opacity="0.7"/>')


def width(s, size, weight, sp):
    gs, cmap, upm, hmtx = font(weight)
    return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


# ---------------------------------------------------------------- helpers of this panel
fields = []   # (name, x, y, w, h, preview text, preview hint)


def field(name, x, y, w, h=5.4, val="", hint=""):
    """A small dropdown field: the module writes the value, click = list, wheel = step."""
    fields.append((name, x, y, w, h, val, hint))
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h}" rx="0.8" fill="{DARK}" stroke="{LINE}" '
      f'stroke-width="0.35"/>')
    cx, cy = x + w - 2.4, y + h / 2
    e(f'<polyline points="{cx - 0.9:.2f},{cy - 0.45:.2f} {cx:.2f},{cy + 0.5:.2f} {cx + 0.9:.2f},{cy - 0.45:.2f}" '
      f'fill="none" stroke="{ACC}" stroke-width="0.35" stroke-linecap="round" stroke-linejoin="round"/>')


def button(name, x, y, label):
    comps.append(("button", name, x, y))
    text(x, y + 4.6, label, 1.5, TXT, 600, "middle", 0.25)


def vline(x, y1, y2):
    e(f'<line x1="{x:.2f}" y1="{y1}" x2="{x:.2f}" y2="{y2}" stroke="{LINE}" stroke-width="0.3"/>')


# ---------------------------------------------------------------- frame and header (as GENRA MONO)
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "GOLANA", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("GOLANA", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · ANALOG POLY · AFTER GOLANA 1.3", 1.76, DIM, 600,
     "start", 0.47)
e(f'<rect x="{W - 27.1:.2f}" y="5.55" width="11.5" height="1.08" rx="0.54" fill="{ACC}"/>')
PFIELD = (W - 66.0, 3.3, 34.0, 5.4)
e(f'<rect x="{PFIELD[0]:.2f}" y="{PFIELD[1]}" width="{PFIELD[2]}" height="{PFIELD[3]}" rx="0.9" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')
pcx = PFIELD[0] + PFIELD[2] - 3
e(f'<polyline points="{pcx - 1.2:.2f},{PFIELD[1] + 2.1} {pcx:.2f},{PFIELD[1] + 3.4} {pcx + 1.2:.2f},{PFIELD[1] + 2.1}" '
  f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')

# ---------------------------------------------------------------- row A: oscillators, mixer, filter (+ screen)
YA, HA = 13.5, 43.5
r1, r2 = 26.5, 44.0
ow = 74
section(3, YA, ow, HA, "OSCILLATORS")
text(5.6, r1 + 0.7, "OSC1", 1.7, ACC, 800, "start", 0.3)
field("F_O1WAVE", 13.5, r1 - 2.7, 18, val="SAW")
knob("O1CTRL1", 41.5, r1, "CTRL 1")
knob("O1CTRL2", 54, r1, "CTRL 2")
vline(61.5, 19.5, 52)
knob("DRIFT", 69.5, r1, "DRIFT", "small")
text(5.6, r2 + 0.7, "OSC2", 1.7, ACC, 800, "start", 0.3)
field("F_O2WAVE", 13.5, r2 - 2.7, 18, val="SAW")
knob("O2SEMI", 41.5, r2, "SEMI")
knob("O2TUNE", 54, r2, "TUNE")
field("F_O2MOD", 63, r2 - 2.7, 13, val="SYNC")
text(69.5, r2 + 6.4, "MOD", 1.7, TXT, 600, "middle", 0.3)

mx = 3 + ow + 2
mw = 42
section(mx, YA, mw, HA, "MIXER")
knob("MIX1", mx + 8, r1, "OSC 1")
knob("MIX2", mx + 21, r1, "OSC 2")
knob("NOISE", mx + 34, r1, "NOISE")
field("F_SUBTYPE", mx + 2, r2 - 2.7, 15, val="-1 SQR")
knob("SUB", mx + 24, r2, "SUB", "small")
knob("FOLD", mx + 35, r2, "FOLD", "small")

fx = mx + mw + 2
fw = W - 3 - fx
section(fx, YA, fw, HA, "FILTER")
FIELD = (fx + 3, 20.3, 50.0, 5.4)
field("F_FTYPE", *FIELD, val="LPF24", hint="2 × SVF · 24 DB")
knob("CUTOFF", fx + 15, 38.5, "CUTOFF", "large", hot=True)
knob("RESO", fx + 33.5, 31.8, "RESO")
knob("EGINT", fx + 48.5, 31.8, "EG INT")
knob("KEYTRK", fx + 33.5, 48.3, "KEY TRK", "small")
switch("DIST", fx + 48.5, 48.6, "DIST", "CLEAN")
# the screen: Golana draws what it does - here the sound's trail and the filter's curve (drawn by the module)
SCREEN = (fx + 56.5, 20.3, fw - 59.5, 32.6)
e(f'<rect x="{SCREEN[0]:.2f}" y="{SCREEN[1]}" width="{SCREEN[2]:.2f}" height="{SCREEN[3]}" rx="1.0" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')

# ---------------------------------------------------------------- row B: envelopes, LFO 1/2, voice
YB, HB = YA + HA + 2, 25
kb = YB + 15.3
ew = 92
section(3, YB, ew, HB, "ENVELOPES")
emid = 3 + ew / 2
text(3 + 20, YB + 9.0, "FILTER EG", 1.5, TXT, 800, "middle", 0.4)
adsr(3 + 29, YB + 9.2)
text(emid + 20, YB + 9.0, "AMP EG", 1.5, TXT, 800, "middle", 0.4)
adsr(emid + 29, YB + 9.2)
for i, l in enumerate("ADSR"):
    knob(f"F{l}", 9 + i * 10.4, kb, l, "small")
    knob(f"A{l}", emid + 6 + i * 10.4, kb, l, "small")
vline(emid, YB + 7, YB + 22)

lw = 42
for n in (1, 2):
    lx = 3 + ew + 2 + (n - 1) * (lw + 2)
    section(lx, YB, lw, HB, f"LFO {n}")
    field(f"F_L{n}WAVE", lx + 2.5, YB + 7.6, 19, 4.6, val="TRI" if n == 1 else "SIN")
    field(f"F_L{n}KEY", lx + 2.5, YB + 12.9, 19, 4.6, val="KEY TIMBRE")
    field(f"F_L{n}SYNC", lx + 2.5, YB + 18.2, 19, 4.6, val="SYNC OFF")
    knob(f"L{n}FREQ", lx + 26.6, kb - 0.6, "FREQ", "small")
    # a trigger restarts the LFO (all voices), whatever KEY says
    jack(f"IN_L{n}RETRIG", lx + 36.6, kb - 1.2, "RETRIG")

vx = 3 + ew + 2 + 2 * (lw + 2)
vw = W - 3 - vx
section(vx, YB, vw, HB, "VOICE")
field("F_VMODE", vx + 2.5, YB + 7.6, vw - 5, 4.6, val="POLY 4")
knob("DETUNE", vx + vw / 2 - 6.2, kb + 1.6, "DETUNE", "small")
knob("PORTA", vx + vw / 2 + 6.2, kb + 1.6, "PORTA", "small")

# ---------------------------------------------------------------- row C: patch bay, arpeggiator, FX sums
YC, HC = YB + HB + 2, 20
kc = YC + 12
pw = 108
section(3, YC, pw, HC, "PATCH")
for k in range(4):
    px = 4.5 + k * 26.5
    text(px + 1.0, YC + 9.3, str(k + 1), 1.5, ACC, 800, "middle", 0.2)
    field(f"F_P{k + 1}SRC", px + 2.6, YC + 6.6, 14, 4.4, val=["LFO2", "FEG", "LFO1", "LFO1"][k])
    field(f"F_P{k + 1}DST", px + 2.6, YC + 11.6, 14, 4.4, val=["PITCH", "PITCH", "CUTOFF", "AMP"][k])
    knob(f"P{k + 1}INT", px + 21.6, kc, "INT", "small")
    if k:
        vline(px - 0.9, YC + 6.5, YC + 17.5)

ax = 3 + pw + 2
aw = 52
section(ax, YC, aw, HC, "ARP")
field("F_ARPTYPE", ax + 2.5, YC + 6.6, 18, 4.4, val="UP")
field("F_ARPRES", ax + 2.5, YC + 11.6, 18, 4.4, val="1/16")
knob("ARPRANGE", ax + 26.8, kc, "RANGE", "small")
knob("ARPSWING", ax + 37.6, kc, "SWING", "small")
button("ARPON", ax + 47.2, kc - 1.0, "ON")

ox = ax + aw + 2
ow2 = W - 3 - ox
section(ox, YC, ow2, HC, "FX")
for i, (n, l) in enumerate([("MFXAMT", "MOD FX"), ("DLYAMT", "DELAY"), ("REVAMT", "REVERB")]):
    knob(n, ox + 8.5 + i * (ow2 - 17) / 2, kc, l, "small")

# ---------------------------------------------------------------- row D: in / out
YD = YC + HC + 2
jy = YD + 7.1
e(f'<rect x="3" y="{YD}" width="{W - 6:.2f}" height="{H - 2.5 - YD:.2f}" rx="1.2" fill="{PLATE}"/>')
# the GENRA strip (docs/DESIGN.md): V/OCT and GATE poly up to 4 voices, MOD = GENRA's MOD (Golana's mod wheel)
x = genra_strip(jack, e, 5.5, jy, {"V/OCT": "IN_VOCT", "GATE": "IN_GATE", "VEL": "IN_VEL", "ACC": "IN_ACC",
                                    "SLIDE": "IN_SLIDE", "MOD": "IN_MODW"}, ACC,
                {"CLOCK": "IN_CLOCK", "RESET": "IN_RESET"})
cvx = x + 3.2 + 4.7
for i, (n, l) in enumerate([("IN_CUTOFF", "CUT"), ("IN_RESO", "RESO"), ("IN_EGINT", "EG INT"), ("IN_CTRL1", "CTRL 1"),
                            ("IN_FOLD", "FOLD"), ("IN_L1FREQ", "LFO 1"), ("IN_L2FREQ", "LFO 2"), ("IN_AFTER", "AFTER")]):
    jack(n, cvx + i * 9.4, jy, l)
vline(W - 48.5, YD + 2, YD + 16)
knob("SPREAD", W - 42, jy - 0.3, "SPREAD", "small")
knob("LEVEL", W - 31.5, jy - 0.3, "LEVEL", "small")
jack("OUT_L", W - 20.5, jy, "L", True)
jack("OUT_R", W - 10, jy, "R", True)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "instruments", "res", "Golana.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/golana_panel.py - do not edit", "#pragma once", "namespace glayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, x, y, w, h, _, _ in fields:
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const float PFIELD[4] = {{{PFIELD[0]:.3f}f, {PFIELD[1]:.3f}f, {PFIELD[2]:.3f}f, {PFIELD[3]:.3f}f}};")
    lines.append(f"static const float SCREEN[4] = {{{SCREEN[0]:.3f}f, {SCREEN[1]:.3f}f, {SCREEN[2]:.3f}f, {SCREEN[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "instruments", "src", "golana"), exist_ok=True)
    open(os.path.join(ROOT, "instruments", "src", "golana", "GolanaLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote instruments/res/Golana.svg and instruments/src/golana/GolanaLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components, field texts, screen
_cache = {}


def img(name, x, y, rot=0.0):
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
        body = re.sub(r"</?(sodipodi|inkscape):[^>]*>", "", body)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w / 2.9528, h / 2.9528, body)
    vbs, sw, sh, body = _cache[fn]
    g = f'<g transform="rotate({rot} {x} {y})">' if rot else "<g>"
    out.insert(-1, f'{g}<svg x="{x - sw / 2:.3f}" y="{y - sh / 2:.3f}" width="{sw:.3f}" height="{sh:.3f}" '
                   f'viewBox="{vbs}" overflow="visible">{body}</svg></g>')


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


for kind, n, x, y in comps:
    if kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "switch":
        img("CKSS_1", x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
    else:
        img("PJ301M", x, y)
for sx in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx, sy)
for n, x, y, w, h, val, hint in fields:
    big = h > 5
    ptext(x + 1.8, y + h / 2 + (0.68 if big else 0.6), val, 1.9 if big else 1.6, TXT, 700, "start", 0.3)
    if hint:
        ptext(x + w - 4.6, y + h / 2 + 0.55, hint, 1.4, DIM, 600, "end", 0.25)
ptext(PFIELD[0] + 2, PFIELD[1] + 3.55, "01", 1.6, ACC, 700, "start", 0.3)
ptext(PFIELD[0] + 6.2, PFIELD[1] + 3.55, "WARM POLY KEYS", 1.9, TXT, 700, "start", 0.35)
# the screen: the trail of the last few cycles (fading) and the filter curve
import math
sx0, sy0, sw_, sh_ = SCREEN
cy = sy0 + sh_ * 0.42
for k, op in ((4, 0.12), (3, 0.2), (2, 0.32), (1, 0.5), (0, 0.95)):
    pts = []
    for i in range(121):
        t = i / 120
        ph = (t * 2.0 + k * 0.035) % 1.0
        v = (1 - 2 * ph) * (0.75 + 0.05 * k) + 0.18 * math.sin(2 * math.pi * 6 * t)
        pts.append(f"{sx0 + 2 + t * (sw_ - 4):.2f},{cy - v * sh_ * 0.24 - k * 0.5:.2f}")
    col = ACC if k == 0 else "#5dd39e"
    out.insert(-1, f'<polyline points="{" ".join(pts)}" fill="none" stroke="{col}" stroke-opacity="{op}" '
                   f'stroke-width="0.32" stroke-linejoin="round"/>')
base = sy0 + sh_ - 3.2
pts = []
for i in range(81):
    t = i / 80
    f = 10 ** (t * 3)
    fc, q = 120, 2.4
    r = f / fc
    mag = 1 / math.sqrt((1 - r * r) ** 2 + (r / q) ** 2) ** 2
    db = max(-30, min(10, 20 * math.log10(mag)))
    pts.append(f"{sx0 + 2 + t * (sw_ - 4):.2f},{base - (db + 30) / 40 * sh_ * 0.32:.2f}")
out.insert(-1, f'<polyline points="{" ".join(pts)}" fill="none" stroke="{TXT}" stroke-opacity="0.55" '
               f'stroke-width="0.28"/>')
ptext(sx0 + 2, sy0 + 3.2, "TRAIL", 1.3, DIM, 700, "start", 0.4)
ptext(sx0 + sw_ - 2, sy0 + 3.2, "C3 E3 G3", 1.3, ACC, 700, "end", 0.3)
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
out[0] = out[0].replace("<svg ", '<svg xmlns:xlink="http://www.w3.org/1999/xlink" ', 1)
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2000", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
