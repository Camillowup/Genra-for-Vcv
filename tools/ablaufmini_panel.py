#!/usr/bin/env python3
"""Panel of ABLAUF MINI: song parts for GENRA BOX, 16 pads (4 x 4), one button per part, re-roll per track.

Usage:  ablaufmini_panel.py real                   -> res/AblaufMini.svg (text as paths) + src/box/AblaufMiniLayout.hpp
        ablaufmini_panel.py preview OUT.png [RACK]   -> preview with Rack components and mock pads
                                                     (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates in mm (16 HP, height 128.5 mm). The C++ side converts with mm2px().
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

HP = 16
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
# the track colours (docs/DESIGN.md); keep in step with src/box/GenraBox.cpp
TRACKS = [("DRUM-X", "#7b6cff", "DR", "FILL"), ("GRUND", "#b8f23a", "BA", "MOVE"),
          ("SCHMELZ", "#e0779e", "PA", "VOICING"), ("FUNKE", "#3dc9ff", "ME", "RANGE")]
MACROS = [("TUNE", "DECAY"), ("TONE", "DRIVE"), ("MOTION", "SPACE"), ("BRIGHT", "SPACE")]
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


YEL = "#ffd23d"
# song parts (keep in step with SECTION_COLORS in src/ablauf/Ablauf.cpp)
SECS = [("INTRO", "#6f86b8"), ("VERSE", "#9b7bd4"), ("BUILD", "#e0a526"), ("DROP", "#ff5a4f"), ("BREAK", "#3fae9a"),
        ("OUTRO", "#8a8d99")]
areas = {}

e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "ABLAUF", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("ABLAUF", 5.1, 800, 1.08) + 2.6, 7.45, "MINI", 1.76, DIM, 600, "start", 0.47)
text(W - 13.6, 7.5, "LINK", 1.6, DIM, 600, "end", 0.5)
comps.append(("light", "LINK", W - 10.6, 6.9))
DISPLAY = (3, 11.5, W - 6, 12)
e(f'<rect x="{DISPLAY[0]}" y="{DISPLAY[1]}" width="{DISPLAY[2]:.2f}" height="{DISPLAY[3]}" rx="1.2" fill="{DARK}" '
  f'stroke="{LINE}" stroke-width="0.4"/>')
plate(3, 25.5, W - 6, 80, YEL, "PARTS")
sp = (W - 10) / 6
for k, (nm, c) in enumerate(SECS):
    x = 5 + sp / 2 + k * sp
    comps.append(("button", f"PART{k}", x, 37.0))
    text(x, 43.4, nm, 1.25, c, 800, "middle", 0.15)
PW, PH, G = (W - 6 - 4.4 - 3 * 1.4) / 4, 10.4, 1.4
PADS = (3 + 2.2, 46.5, PW + G, PH + G, PW, PH)
for n in range(16):
    x, y = PADS[0] + (n % 4) * PADS[2], PADS[1] + (n // 4) * PADS[3]
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{PW:.2f}" height="{PH}" rx="0.8" fill="#1a1b20"/>')
areas["PADS"] = PADS
ry = 96.5
# re-roll per track (which tracks a part button makes new), then UNDO and CLEAR
for t, (lb, c) in enumerate((("DR", "#7b6cff"), ("BA", "#b8f23a"), ("ME", "#3dc9ff"), ("PA", "#e0779e"))):
    comps.append(("button", f"TRACK{t}", 9.5 + t * 8.6, ry))
    text(9.5 + t * 8.6, ry + 6.6, lb, 1.35, c, 800, "middle", 0.1)
comps.append(("button", "SONG", 49.0, ry))  # MAKE SONG: the pads filled with the genre's song form
text(49.0, ry + 6.6, "SONG", 1.3, YEL, 800, "middle", 0.2)
for i, (n, lb) in enumerate((("UNDO", "UNDO"), ("CLEAR", "CLEAR"))):
    comps.append(("button", n, W - 18.6 + i * 9.6, ry))
    text(W - 18.6 + i * 9.6, ry + 6.6, lb, 1.3, TXT, 600, "middle", 0.2)
plate(3, 107.5, W - 6, 18.5)
JY = 114.3
for i, (n, lb) in enumerate((("IN_NEXT", "NEXT"), ("IN_RESET", "RESET"))):
    comps.append(("input", n, 10 + i * 9.2, JY))
    text(10 + i * 9.2, JY + 5.7, lb, 1.45, DIM, 600, "middle", 0.25)
for i, (n, lb) in enumerate((("OUT_DROP", "DROP"), ("OUT_END", "END"))):
    x = W - 19.4 + i * 9.2
    e(f'<rect x="{x - 4.4:.2f}" y="{JY - 4.6:.2f}" width="8.8" height="11.6" rx="0.9" fill="{tint(YEL, 0.25)}"/>')
    comps.append(("output", n, x, JY))
    text(x, JY + 5.7, lb, 1.45, TXT, 600, "middle", 0.25)
e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "AblaufMini.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/ablaufmini_panel.py - do not edit", "#pragma once", "namespace minilayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    lines.append("static const float PADS[6] = {%.3ff, %.3ff, %.3ff, %.3ff, %.3ff, %.3ff};  // x0, y0, step x, step y, w, h" % PADS)
    lines.append("static const float DISPLAY[4] = {%.3ff, %.3ff, %.3ff, %.3ff};" % DISPLAY)
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "box"), exist_ok=True)
    open(os.path.join(ROOT, "src", "box", "AblaufMiniLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/AblaufMini.svg and src/box/AblaufMiniLayout.hpp")
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


SECC = dict(SECS)
LIT = {"PART3": SECC["DROP"], "TRACK0": "#7b6cff", "TRACK1": "#b8f23a", "SONG": YEL}  # re-roll: drums + bass
for k, (nm, c) in enumerate(SECS):
    LIT.setdefault(f"PART{k}", tint(c, 0.55))
for kind, n, x, y in comps:
    if kind == "button":
        img("VCVBezel", x, y)
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.75" fill="{LIT.get(n, "#4a4b52")}"/>')
        if n == "PART3":
            put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4.3" fill="none" stroke="#ffffff" stroke-width="0.35"/>')
    elif kind == "light":
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="1.2" fill="#5dff8a"/>')
    else:
        img("PJ301M", x, y)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        img("ScrewBlack", sx_, sy)
dx, dy, dw, dh = DISPLAY
ptext(dx + 3, dy + 5.4, "DROP 2", 2.6, SECC["DROP"], 800, "start", 0.4)
ptext(dx + 3, dy + 9.6, "NEXT › BREAK 1", 1.45, TXT, 600, "start", 0.3)
ptext(dx + dw - 3, dy + 9.6, "BAR 7/8", 1.45, DIM, 600, "end", 0.3)
X0, Y0, SX, SY, PW_, PH_ = PADS
TAG = {1: "INTRO", 2: "VERSE", 3: "VERSE", 4: "BUILD", 5: "DROP", 6: "DROP", 7: "BREAK"}
cnt = {}
for n in range(16):
    x, y = X0 + (n % 4) * SX, Y0 + (n // 4) * SY
    if n > 7:
        ptext(x + 0.9, y + 3.0, f"{n + 1:02d}", 1.05, "#4a4c57", 700, "start", 0.1)
        continue
    sec = TAG.get(n)
    col = SECC.get(sec, "#9a9caa")
    sel, nxt = n == 6, n == 7
    st = (' stroke="#ffffff" stroke-width="0.55"' if sel else (' stroke="#ffffff" stroke-width="0.4" stroke-dasharray="0.8 0.5"'
          if nxt else (' stroke="#e6e7ec" stroke-width="0.35"' if n == 0 else "")))
    put(f'<rect x="{x:.2f}" y="{y:.2f}" width="{PW_:.2f}" height="{PH_}" rx="0.8" fill="{tint(col, 0.5 if sel else 0.3) if sec else "#2a2b33"}"{st}/>')
    put(f'<rect x="{x + 0.6:.2f}" y="{y + 0.6:.2f}" width="{PW_ - 1.2:.2f}" height="0.7" rx="0.3" fill="{col}"/>')
    ptext(x + 0.9, y + 3.2, f"{n + 1:02d}", 1.05, TXT, 700, "start", 0.1)
    if sec:
        cnt[sec] = cnt.get(sec, 0) + 1
    ptext(x + 0.9, y + 5.7, f"{sec} {cnt[sec]}" if sec else "ROOT", 1.3, "#ffffff", 800, "start", 0.12)
    for t, c in enumerate(("#7b6cff", "#b8f23a", "#3dc9ff", "#e0779e")):
        put(f'<rect x="{x + 0.9 + t * 1.5:.2f}" y="{y + 7.7:.2f}" width="1.05" height="0.6" rx="0.25" fill="{c}"/>')
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1000", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
