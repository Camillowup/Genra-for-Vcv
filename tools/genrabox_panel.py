#!/usr/bin/env python3
"""Panel of GENRA BOX: GENRA's generator with four built-in voices (DRUM-X, GRUND, SCHMELZ, FUNKE) in one module.

Usage:  genrabox_panel.py real                   -> res/GenraBox.svg (text as paths) + src/box/BoxLayout.hpp
        genrabox_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock display content
                                                     (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (36 HP, height 128.5 mm). The C++ side converts with mm2px().
The panel holds what never changes; the module widget draws the display, the BPM field and the text in the fields
(genre in the header, genre and preset per track).
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

HP = 40  # 2.30: 40 HP (MUTATE, CLOCK OUT, DUCK)
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# the track colours (docs/DESIGN.md); keep in step with src/box/GenraBox.cpp
# in GENRA's track order: drums, bass, melody, pads
TRACKS = [("DRUM-X", "#7b6cff", "DR", "FILL"), ("GENRA MONO", "#b8f23a", "BA", "MOVE"),
          ("FUNKE", "#3dc9ff", "ME", "RANGE"), ("SCHMELZ", "#e0779e", "PA", "VOICING")]
MACROS = [("TUNE", "DECAY"), ("CUTOFF", "RESO"), ("BRIGHT", "SPACE"), ("MOTION", "SPACE")]
SOUND = "#ffd23d"  # the SOUND button (presets follow NEW)

R = {"small": 3.84, "knob": 4.8, "jack": 4.0, "button": 3.6}
KIND = {"small": "RoundSmallBlackKnob", "knob": "RoundBlackKnob"}

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps, fields = [], [], {}


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


def tint(col, f):
    c = [int(col[i:i + 2], 16) for i in (1, 3, 5)]
    p = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(p[i] + (c[i] - p[i]) * f):02x}" for i in range(3))


def plate(x, y, w, h, col=None, title=None):
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="1.2" fill="{PLATE}"/>')
    if col:
        e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="0.88" rx="0.44" fill="{col}"/>')
    if title:
        text(x + w / 2, y + 5.0, title, 2.03, col, 800, "middle", 0.75)


def knob(name, x, y, label, kind="small", ring=RING, ly=None):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="none" stroke="{ring}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    if label:
        text(x, ly if ly else y + R[kind] + 3.3, label, 1.55, TXT, 600, "middle", 0.3)


def button(name, x, y, label, color=TXT, ly=None):
    comps.append(("button", name, x, y))
    text(x, ly if ly else y + R["button"] + 3.0, label, 1.45, color, 600 if color == TXT else 800, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    text(x, y + 5.7, label, 1.45, TXT if out_ else DIM, 600, "middle", 0.25)


def field(name, x, y, w, h=5.4, chev="#ffffff"):
    """A dark selection field (the module draws its text); the chevron marks it as a list."""
    fields[name] = (x, y, w, h)
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h}" rx="0.9" fill="{DARK}" stroke="{LINE}" '
      f'stroke-width="0.4"/>')
    if chev:
        e(f'<polyline points="{x + w - 4.2:.2f},{y + h / 2 - 0.6:.2f} {x + w - 3:.2f},{y + h / 2 + 0.7:.2f} '
          f'{x + w - 1.8:.2f},{y + h / 2 - 0.6:.2f}" fill="none" stroke="{chev}" stroke-width="0.4" '
          f'stroke-linecap="round" stroke-linejoin="round"/>')


# ---------------------------------------------------------------- frame, header with the genre field
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "GENRA BOX", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("GENRA BOX", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · GROOVEBOX", 1.76, DIM, 600, "start", 0.47)
field("GENREF", W - 70, 3.2, 56, 5.8)

# ---------------------------------------------------------------- generator band: display, global knobs, buttons
plate(3, 11.5, W - 6, 20)
DISPLAY = (5.5, 13.5, 67, 16)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]}" height="{DISPLAY[3]}" rx="1.2" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')
BPMF = (7.5, 15.0, 13.6, 6.0)  # BPM: drag / scroll / double-click (drawn by the module)
for i, (n, lb) in enumerate((("KEY", "KEY"), ("COLOR", "COLOR"), ("SWING", "SWING"), ("LENGTH", "LENGTH"),
                             ("MUTATE", "MUTATE"))):
    knob(n, 82 + i * 11.6, 19.8, lb, ly=28.6)
x0 = W - 53.4
e(f'<rect x="{x0 - 3.2:.2f}" y="14.5" width="0.35" height="14" fill="{LINE}"/>')
for i, (n, lb, c) in enumerate((("NEWALL", "NEW ALL", TXT), ("CHLOCK", "LOCK", TXT), ("RUN", "RUN", TXT),
                               ("SOUND", "SOUND", SOUND))):
    button(n, x0 + 4.4 + i * 11.6, 19.8, lb, c, ly=28.6)

# ---------------------------------------------------------------- the four voices
CW = (W - 6 - 3 * 2) / 4
Y = 33.5
for t, (name, col, short, k2) in enumerate(TRACKS):
    cx = 3 + t * (CW + 2)
    m = cx + CW / 2
    plate(cx, Y, CW, 72, col, name)
    text(cx + 2.4, Y + 9.4, "PATTERN", 1.2, DIM, 700, "start", 0.4)
    field(f"TGENRE{t}", cx + 2.2, Y + 10.6, CW - 4.4, 5.4, col)
    knob(f"DENS{t}", m - 16.2, Y + 22.4, "DENSITY", ly=Y + 29.6)
    knob(f"SHAPE{t}", m - 5.2, Y + 22.4, k2, ly=Y + 29.6)
    button(f"NEW{t}", m + 5.8, Y + 22.4, "NEW", ly=Y + 29.6)    # pattern and a preset of the track's genre
    button(f"HOLD{t}", m + 16.2, Y + 22.4, "HOLD", ly=Y + 29.6)  # the preset stays
    e(f'<rect x="{cx + 2.4:.2f}" y="{Y + 31.8:.2f}" width="{CW - 4.8:.2f}" height="0.3" fill="{LINE}"/>')
    text(cx + 2.4, Y + 35.4, "SOUND", 1.2, DIM, 700, "start", 0.4)
    field(f"PRESET{t}", cx + 2.2, Y + 36.6, CW - 4.4, 5.4, col)
    knob(f"MACA{t}", m - 10, Y + 49.6, MACROS[t][0], "knob", col, ly=Y + 58.0)
    knob(f"MACB{t}", m + 10, Y + 49.6, MACROS[t][1], "knob", col, ly=Y + 58.0)
    knob(f"LEVEL{t}", m - 10, Y + 64.0, "LEVEL", ly=Y + 70.4)
    button(f"MUTE{t}", m + 10, Y + 64.0, "MUTE", ly=Y + 70.4)

# ---------------------------------------------------------------- jacks: inputs, track outs (stereo), notes, master
plate(3, 107.5, W - 6, 18.5)
JY = 114.3
for i, (n, lb) in enumerate((("IN_CLOCK", "CLOCK"), ("IN_RESET", "RESET"), ("IN_RUN", "RUN"), ("IN_NEW", "NEW"))):
    jack(n, 9.5 + i * 9.0, JY, lb)
e(f'<rect x="{45.5 - 4.4:.2f}" y="{JY - 4.6:.2f}" width="8.8" height="11.6" rx="0.9" fill="{tint("#ffffff", 0.18)}"/>')
jack("OUT_CLOCK", 45.5, JY, "CLK OUT", True)
for t, (name, col, short, _) in enumerate(TRACKS):
    px = 57 + t * 19.0
    e(f'<rect x="{px - 4.6:.2f}" y="{JY - 4.6:.2f}" width="17.6" height="11.6" rx="0.9" fill="{tint(col, 0.32)}"/>')
    jack(f"OUT_{short}_L", px, JY, f"{short} L", True)
    jack(f"OUT_{short}_R", px + 8.4, JY, f"{short} R", True)
for i, (n, lb, c) in enumerate((("OUT_NOTES", "NOTES", "#c9ccd6"), ("OUT_GATES", "GATES", "#c9ccd6"),
                               ("OUT_DRUMS", "DRUMS", TRACKS[0][1]))):
    x = 134 + i * 9.0
    e(f'<rect x="{x - 4.4:.2f}" y="{JY - 4.6:.2f}" width="8.8" height="11.6" rx="0.9" fill="{tint(c, 0.22)}"/>')
    jack(n, x, JY, lb, True)
knob("DUCK", 162.6, JY - 0.5, "DUCK", ly=JY + 7.5)
knob("MASTER", 173.0, JY - 0.5, "MASTER", ly=JY + 7.5)
for i, (n, lb) in enumerate((("OUT_L", "L"), ("OUT_R", "R"))):
    x = W - 19.8 + i * 9.0
    e(f'<rect x="{x - 4.4:.2f}" y="{JY - 4.6:.2f}" width="8.8" height="11.6" rx="0.9" fill="{tint("#ffffff", 0.18)}"/>')
    jack(n, x, JY, lb, True)

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "GenraBox.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/genrabox_panel.py - do not edit", "#pragma once", "namespace boxlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, w, h) in fields.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};  // field x, y, w, h")
    for n, v in (("DISPLAY", DISPLAY), ("BPMF", BPMF)):
        lines.append(f"static const float {n}[4] = {{{v[0]:.3f}f, {v[1]:.3f}f, {v[2]:.3f}f, {v[3]:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "box"), exist_ok=True)
    open(os.path.join(ROOT, "src", "box", "BoxLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GenraBox.svg and src/box/BoxLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: Rack components + mock display
_cache = {}


def img(name, x, y):
    fn = os.path.join(CL, name + ".svg")
    if fn not in _cache:
        src = open(fn).read()
        src = re.sub(r"<\?xml[^>]*\?>|<!DOCTYPE[^>]*>|<!--.*?-->", "", src, flags=re.S)
        mm = re.search(r"<svg[^>]*>", src, re.S)
        tag = mm.group(0)
        w = float(re.search(r'\swidth="([0-9.]+)', tag).group(1))
        h = float(re.search(r'\sheight="([0-9.]+)', tag).group(1))
        vb = re.search(r'viewBox="([^"]+)"', tag)
        body = src[mm.end():src.rindex("</svg>")]
        body = re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)|<metadata.*?</metadata>", "", body, flags=re.S)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w / 2.9528, h / 2.9528, body)
    vbs, sw, sh, body = _cache[fn]
    out.insert(-1, f'<svg x="{x - sw / 2:.3f}" y="{y - sh / 2:.3f}" width="{sw:.3f}" height="{sh:.3f}" '
                   f'viewBox="{vbs}" overflow="visible">{body}</svg>')


def put(s):
    out.insert(-1, s)


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


LIT = {"RUN": "#5dff8a", "SOUND": SOUND, "NEW1": TRACKS[1][1], "HOLD3": TRACKS[3][1]}
for kind, n, x, y in comps:
    if kind in KIND:
        img(KIND[kind] + "_bg", x, y)
        img(KIND[kind], x, y)
    elif kind == "button":
        img("VCVBezel", x, y)
        if n in LIT:
            put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.75" fill="{LIT[n]}"/>')
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)


def ftext(n, a, b, ca=TXT, cb=DIM, size=1.6):
    x, y, w, h = fields[n]
    ptext(x + 1.6, y + h / 2 + 0.6, a, size, ca, 800, "start", 0.3)
    ptext(x + w - 5.6, y + h / 2 + 0.5, b, 1.25, cb, 600, "end", 0.2)


ftext("GENREF", "07  UK GARAGE", "CLASSIC", "#ffffff", DIM, 1.9)
GEN = [("UK GARAGE", "= MAIN"), ("HIP-HOP", "OWN"), ("UK GARAGE", "= MAIN"), ("UK GARAGE", "= MAIN")]
PRE = [("808 GARAGE", "12/30"), ("BOOM BAP", "14/23"), ("VOCAL CHOP", "06/26"), ("ORGAN STAB", "14/25")]
for t in range(4):
    own = GEN[t][1] == "OWN"
    ftext(f"TGENRE{t}", GEN[t][0], GEN[t][1], TRACKS[t][1] if own else DIM)
    ftext(f"PRESET{t}", PRE[t][0], PRE[t][1])
bx, by, bw, bh = BPMF
put(f'<rect x="{bx}" y="{by}" width="{bw}" height="{bh}" rx="0.6" fill="#1a1b21" stroke="#5a5c66" stroke-width="0.25"/>')
ptext(bx + 1.0, by + 4.5, "132", 3.4, "#ffffff", 800, "start", 0.2)
ptext(bx + 8.8, by + 4.5, "BPM", 1.25, DIM, 700, "start", 0.2)
put(f'<circle cx="{bx + bw + 2.2:.2f}" cy="{by + 1.7:.2f}" r="0.7" fill="#5dff8a"/>')
ptext(bx + bw + 3.4, by + 2.25, "INT", 1.15, "#5dff8a", 700, "start", 0.2)
ptext(bx + 1.0, by + 9.0, "A MINOR · 4 BARS · 3/4", 1.35, DIM, 600, "start", 0.3)
ptext(bx + 1.0, by + 12.6, "SOUND: SAME GENRE", 1.3, SOUND, 700, "start", 0.3)
gx = DISPLAY[0] + DISPLAY[2] - 2 - 16 * 2.05
pat = ["1000101000100010", "0000100000001000", "1010001010000010", "1000000010000000"]
for row, bits in enumerate(pat):
    c = TRACKS[row][1]
    for i, bb in enumerate(bits):
        put(f'<rect x="{gx + i * 2.05:.2f}" y="{15.3 + row * 3.1:.2f}" width="1.6" height="2.4" rx="0.3" '
            f'fill="{c if bb == "1" else "#1d1e24"}"/>')
put(f'<rect x="{gx + 4 * 2.05 - 0.3:.2f}" y="14.9" width="2.2" height="12.6" rx="0.4" fill="none" stroke="#ffffff" '
    f'stroke-width="0.28"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "2200", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
