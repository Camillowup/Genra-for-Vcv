#!/usr/bin/env python3
"""GENRA panel generator.

Usage:  panel.py real    -> res/Genra.svg (text as paths, no components) + src/Layout.hpp
        panel.py mock    -> genra_panel_mock.svg (with drawn components, for previews)

All coordinates in mm. VCV Rack: height 128.5 mm, 1 HP = 5.08 mm.
"""
import os
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

MODE = sys.argv[1] if len(sys.argv) > 1 else "mock"
PREVIEW = MODE == "preview"
REAL = MODE in ("real", "preview")
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HP = 44
W = HP * 5.08
H = 128.5

BG = "#17181d"
PLATE = "#212329"
LINE = "#34363f"
TXT = "#d9dbe2"
DIM = "#8a8d99"
TRACKS = [("DRUMS", "#ff8a3d"), ("BASS", "#ff4f6d"), ("MELODY", "#3dc9ff"), ("PADS", "#a98bff")]

# real component radii (mm)
R_LARGE, R_KNOB, R_SMALL, R_TRIM, R_JACK, R_BEZEL = 6.1, 4.8, 3.84, 3.0, 4.0, 3.6

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_font_cache = {}


def font(weight):
    if weight not in _font_cache:
        f = TTFont(FONT_DIR + FONTS.get(weight, FONTS[600]))
        _font_cache[weight] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _font_cache[weight]


out = []
components = []  # (kind, name, x, y)


def e(s):
    out.append(s)


def text(x, y, s, size=2.3, color=TXT, weight=600, anchor="middle", spacing=0.15):
    if not REAL:
        e(f'<text x="{x:.2f}" y="{y:.2f}" font-family="Inter" font-size="{size}" font-weight="{weight}" '
          f'fill="{color}" text-anchor="{anchor}" letter-spacing="{spacing}">{s}</text>')
        return
    f, gs, cmap, upm, hmtx = font(weight)
    scale = size / upm
    glyphs = [cmap.get(ord(c), "space") for c in s]
    width = sum(hmtx[g][0] * scale + spacing for g in glyphs) - spacing
    cx = x - (width / 2 if anchor == "middle" else (width if anchor == "end" else 0))
    paths = []
    for g in glyphs:
        pen = SVGPathPen(gs)
        tpen = TransformPen(pen, (scale, 0, 0, -scale, cx, y))
        gs[g].draw(tpen)
        d = pen.getCommands()
        if d:
            paths.append(d)
        cx += hmtx[g][0] * scale + spacing
    if paths:
        e(f'<path fill="{color}" d="{" ".join(paths)}"/>')


def knob(name, x, y, r, accent="#e6e7ec", label=None, ly=None):
    components.append(("param", name, x, y))
    if not REAL:
        e(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#2c2e36" stroke="#0d0e11" stroke-width="0.4"/>')
        e(f'<circle cx="{x}" cy="{y}" r="{r*0.78}" fill="#3a3c46"/>')
        e(f'<line x1="{x}" y1="{y-r*0.2}" x2="{x}" y2="{y-r*0.75}" stroke="{accent}" stroke-width="0.7" stroke-linecap="round"/>')
    else:
        e(f'<circle cx="{x}" cy="{y}" r="{r+0.9}" fill="none" stroke="{LINE}" stroke-width="0.35" stroke-dasharray="0.4 0.9"/>')
    if label:
        text(x, ly if ly else y + r + 3.4, label, 2.1, DIM)


def jack(name, x, y, label=None, out_=False, color=None):
    components.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x-4.6}" y="{y-4.6}" width="9.2" height="12.4" rx="1.3" fill="{color}" fill-opacity="0.22"/>')
    if not REAL:
        e(f'<circle cx="{x}" cy="{y}" r="3.15" fill="#b9bcc6" stroke="#0d0e11" stroke-width="0.3"/>')
        e(f'<circle cx="{x}" cy="{y}" r="2.35" fill="#5a5d68"/>')
        e(f'<circle cx="{x}" cy="{y}" r="1.45" fill="#08090b"/>')
    if label:
        text(x, y + 6.3, label, 1.8, TXT if out_ else DIM, 600)


def button(name, x, y, color, label=None, lit=False, label2=None):
    components.append(("button", name, x, y))
    if not REAL:
        e(f'<rect x="{x-R_BEZEL}" y="{y-R_BEZEL}" width="{2*R_BEZEL}" height="{2*R_BEZEL}" rx="1.4" fill="#2c2e36" stroke="#0d0e11" stroke-width="0.35"/>')
        e(f'<circle cx="{x}" cy="{y}" r="2.2" fill="{color if lit else "#3d3f48"}"/>')
    if label:
        text(x, y + R_BEZEL + 2.6, label, 1.8, DIM)
    if label2:
        text(x, y + R_BEZEL + 4.9, label2, 1.8, DIM)


# ---------------------------------------------------------------- panel
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
if not REAL:
    for sx in (7.5, W - 7.5):
        for sy in (3, H - 3):
            e(f'<circle cx="{sx}" cy="{sy}" r="1.6" fill="#3a3c45"/>')

text(14, 9.2, "GENRA", 5.2, TXT, 800, "start", 0.9)
text(40, 9.0, "GENERATIVE 4-TRACK SEQUENCER", 2.0, DIM, 600, "start", 0.5)
for i, (_, c) in enumerate(TRACKS):
    e(f'<rect x="{W-42+i*7.5:.2f}" y="6.4" width="5.5" height="1.1" rx="0.5" fill="{c}"/>')

# display area (content is drawn by the module widget)
DISP = (6, 12.5, W - 12, 17)
dx, dy, dw, dh = DISP
e(f'<rect x="{dx}" y="{dy}" width="{dw:.2f}" height="{dh}" rx="1.6" fill="#07080a" stroke="{LINE}" stroke-width="0.4"/>')

# global row
gy = 40.6
e(f'<rect x="6" y="32.5" width="{W-12:.2f}" height="18" rx="1.6" fill="{PLATE}"/>')
knob("GENRE", 19, gy, R_LARGE, "#ffffff", "GENRE", gy + 9.3)
for i, lab in enumerate(["VARIANT", "KEY", "SCALE", "BPM", "SWING", "COMPLEX", "COLOR", "MUTATE", "LENGTH"]):
    knob(lab, 35.5 + i * 13.4, gy, R_SMALL, label=lab, ly=gy + 7.3)
e(f'<line x1="148.6" y1="35" x2="148.6" y2="48" stroke="{LINE}" stroke-width="0.3"/>')
button("NEW", 156.5, gy - 1, "#ffffff", "NEW", label2="ALL")
button("RUN", 168.5, gy - 1, "#5dff8a", "RUN", True)
# chord group
e(f'<rect x="176.5" y="34.2" width="{W-6-176.5-1:.2f}" height="14.6" rx="1.3" fill="#a98bff" fill-opacity="0.10"/>')
text(197.5, 36.9, "CHORDS", 1.7, "#a98bff", 800, "middle", 0.5)
button("CHORDNEW", 184, gy, "#a98bff", "NEW")
button("CHORDLOCK", 196.5, gy, "#a98bff", "LOCK", True)
jack("IN_CHORDNEW", 209.5, gy, "TRIG")

# inputs
e(f'<rect x="6" y="53" width="28" height="{H-53-7}" rx="1.6" fill="{PLATE}"/>')
text(20, 57.6, "IN", 2.0, TXT, 700)
ins = ["CLOCK", "RESET", "RUN", "NEW", "GENRE", "MUTATE", "TRANSP", "CHORD"]
for i, lab in enumerate(ins):
    jack("IN_" + lab, 13 + (i % 2) * 14, 64.5 + (i // 2) * 14.5, lab)
text(27, 118.2, "POLY", 1.4, "#a98bff", 700)

# tracks
x0 = 37
for t, (name, col) in enumerate(TRACKS):
    cw = 58 if name == "DRUMS" else (W - 6 - 37 - 58) / 3
    xc = x0 + cw / 2
    e(f'<rect x="{x0+0.6:.2f}" y="53" width="{cw-1.2:.2f}" height="{H-53-7}" rx="1.6" fill="{PLATE}"/>')
    e(f'<rect x="{x0+0.6:.2f}" y="53" width="{cw-1.2:.2f}" height="1.3" rx="0.6" fill="{col}"/>')
    text(xc, 59, name, 2.6, col, 800, "middle", 0.6)
    if name == "DRUMS":
        knob("DENS_0", xc - 17, 67.5, R_KNOB, col, "DENSITY", 76.3)
        knob("FILL", xc, 67.5, R_SMALL, col, "FILL", 76.3)
        knob("GHOST", xc + 17, 67.5, R_SMALL, col, "GHOST", 76.3)
    else:
        second = {"BASS": "MOVE", "MELODY": "RANGE", "PADS": "VOICING"}[name]
        knob(f"DENS_{t}", xc - 12.5, 67.5, R_KNOB, col, "DENSITY", 76.3)
        knob(second, xc + 1, 67.5, R_SMALL, col, second, 76.3)
        knob(f"OCT_{t-1}", xc + 13.5, 67.5, R_TRIM, "#ffffff", "OCT", 76.3)
    sp = 11.5 if name == "DRUMS" else 9.6
    bx = [xc - 1.5 * sp, xc - 0.5 * sp, xc + 0.5 * sp, xc + 1.5 * sp]
    button(f"MUTE_{t}", bx[0], 83.0, col, "MUTE")
    button(f"LOCK_{t}", bx[1], 83.0, col, "LOCK", t == 0)
    button(f"NEW_{t}", bx[2], 83.0, "#ffffff", "NEW")
    jack(f"IN_NEW_{t}", bx[3], 83.0, "TRIG")
    e(f'<line x1="{x0+4:.2f}" y1="92.6" x2="{x0+cw-4:.2f}" y2="92.6" stroke="{LINE}" stroke-width="0.3"/>')
    text(xc, 95.6, "OUT  ·  POLY 4" if name == "PADS" else "OUT", 1.7, col if name == "PADS" else DIM, 700, "middle", 0.4)
    if name == "DRUMS":
        names = ["KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"]
        for i, n in enumerate(names):
            jack(f"DRUM_{i}", xc - 19.5 + (i % 4) * 13, 101.4 + (i // 4) * 13.2, n, True, col)
    else:
        pre = {"BASS": "BASS", "MELODY": "MEL", "PADS": "PAD"}[name]
        outs = ["VOCT", "GATE", "VEL", "ACC", "SLIDE"] if name != "PADS" else ["VOCT", "GATE", "VEL", "MOD"]
        labels = {"VOCT": "V/OCT", "GATE": "GATE", "VEL": "VEL", "ACC": "ACC", "SLIDE": "SLIDE", "MOD": "MOD"}
        for i, o in enumerate(outs):
            if i < 3:
                x, y = xc - 11.8 + i * 11.8, 101.4
            else:
                x, y = (xc - 5.9 + (i - 3) * 11.8 if len(outs) == 5 else xc), 114.6
            jack(f"{pre}_{o}", x, y, labels[o], True, col)
    x0 += cw

e("</svg>")

if PREVIEW:
    CL = os.path.abspath(os.path.join(ROOT, "..", "ext", "Rack", "res", "ComponentLibrary"))
    PX = 2.9528
    SIZES = {"RoundLargeBlackKnob": 36.0, "RoundBlackKnob": 28.35, "RoundSmallBlackKnob": 22.68,
             "Trimpot": 17.86, "PJ301M": 23.7, "VCVBezel": 21.26}
    import re as _re
    _cache = {}
    def img(name, x, y, layer=""):
        fn = f"{CL}/{name}{layer}.svg"
        if fn not in _cache:
            src = open(fn).read()
            src = _re.sub(r"<\?xml[^>]*\?>", "", src)
            src = _re.sub(r"<!DOCTYPE[^>]*>", "", src)
            m = _re.search(r"<svg[^>]*>", src)
            tag = m.group(0)
            vb = _re.search(r'viewBox="([^"]+)"', tag)
            w = float(_re.search(r'width="([0-9.]+)', tag).group(1))
            h = float(_re.search(r'height="([0-9.]+)', tag).group(1))
            body = src[m.end():src.rindex("</svg>")]
            body = _re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)", "", body, flags=_re.S)
            body = _re.sub(r"<metadata.*?</metadata>", "", body, flags=_re.S)
            body = _re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
            _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w, h, body)
        vbs, w, h, body = _cache[fn]
        sw, sh = w / PX, h / PX
        out.insert(-1, f'<svg x="{x-sw/2:.3f}" y="{y-sh/2:.3f}" width="{sw:.3f}" height="{sh:.3f}" viewBox="{vbs}">{body}</svg>')
    lit = {"RUN": "#5dff8a", "LOCK_0": "#ff8a3d", "CHORDLOCK": "#a98bff"}
    for kind, name, x, y in components:
        if kind in ("input", "output"):
            img("PJ301M", x, y)
        elif kind == "button":
            img("VCVBezel", x, y)
            c = lit.get(name)
            out.insert(-1, f'<circle cx="{x}" cy="{y}" r="2.6" fill="{c or "#2a2b30"}" fill-opacity="{0.95 if c else 0.6}"/>')
        else:
            k = "RoundLargeBlackKnob" if name == "GENRE" else ("RoundBlackKnob" if name.startswith("DENS") else ("Trimpot" if name.startswith("OCT") else "RoundSmallBlackKnob"))
            img(k, x, y, "_bg")
            img(k, x, y)
    for sx in (7.62, W - 7.62):
        for sy in (2.54, H - 2.54):
            out.insert(-1, f'<circle cx="{sx:.2f}" cy="{sy}" r="1.7" fill="#2b2c31"/><path d="M{sx-1:.2f} {sy-1} L{sx+1:.2f} {sy+1}" stroke="#111" stroke-width="0.35"/>')
    # display content (approximation of what the module draws)
    D = []
    D.append(f'<text x="{dx+4}" y="{dy+7.2}" font-family="Nunito, Inter" font-weight="800" font-size="5.2" fill="#f2f3f7" letter-spacing="0.3">2-STEP</text><text x="{dx+27.5}" y="{dy+7.2}" font-family="Inter" font-weight="700" font-size="2.6" fill="#8a8d99" letter-spacing="0.4">· CLASSIC</text>')
    D.append(f'<text x="{dx+4}" y="{dy+12.6}" font-family="DejaVu Sans Mono" font-size="2.6" fill="#8a8d99">F MINOR  132 BPM  SWING 64%</text>')
    chords = ["Fm9", "Dbmaj7", "Abmaj7", "Eb"]
    for i, ch in enumerate(chords):
        x = dx + 64 + i * 17
        act = i == 1
        D.append(f'<rect x="{x}" y="{dy+2.6}" width="15.8" height="6" rx="0.8" fill="{"#a98bff" if act else "#1a1b21"}"/>')
        D.append(f'<text x="{x+7.9}" y="{dy+6.5}" text-anchor="middle" font-family="Nunito, Inter" font-weight="800" font-size="2.7" fill="{"#0a0a0d" if act else "#8a8d99"}">{ch}</text>')
    D.append(f'<text x="{dx+64}" y="{dy+13}" font-family="DejaVu Sans Mono" font-size="2.4" fill="#8a8d99">BAR 2/4</text>')
    D.append(f'<text x="{dx+64+78-1.2}" y="{dy+13}" text-anchor="end" font-family="DejaVu Sans Mono" font-size="2.4" fill="#a98bff">CHORDS LOCKED</text>')
    lanes = ["X......x..X....x", "X.....X...X..X..", "......X..X..X...", "...X......X..X.."]
    for t, (_, c) in enumerate(TRACKS):
        for i in range(16):
            x = dx + dw - 4 - 16 * 2.3 + i * 2.3
            y = dy + 2.8 + t * 3.3
            on = lanes[t][i] != "."
            D.append(f'<rect x="{x:.2f}" y="{y:.2f}" width="1.8" height="2.4" rx="0.3" fill="{c if on else "#22232a"}"/>')
            if i == 6:
                D.append(f'<rect x="{x-0.25:.2f}" y="{y-0.25:.2f}" width="2.3" height="2.9" rx="0.4" fill="none" stroke="#fff" stroke-width="0.25"/>')
    for d in D:
        out.insert(-1, d)
    open(sys.argv[2], "w").write("\n".join(out))
elif REAL:
    os.makedirs(os.path.join(ROOT, "res"), exist_ok=True)
    open(os.path.join(ROOT, "res", "Genra.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/panel.py - do not edit", "#pragma once", "namespace layout {"]
    for kind, name, x, y in components:
        lines.append(f"static const float {name}[2] = {{{x:.3f}f, {y:.3f}f}};")
    lines.append(f"static const float DISPLAY[4] = {{{dx:.3f}f, {dy:.3f}f, {dw:.3f}f, {dh:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    open(os.path.join(ROOT, "src", "Layout.hpp"), "w").write("\n".join(lines) + "\n")
    print(f"wrote res/Genra.svg and src/Layout.hpp ({len(components)} components)")
else:
    open(sys.argv[2] if len(sys.argv) > 2 else "genra_panel_mock.svg", "w").write("\n".join(out))
