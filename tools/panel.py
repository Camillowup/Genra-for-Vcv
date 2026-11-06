#!/usr/bin/env python3
"""GENRA panel generator (v3).

Usage:  panel.py real              -> res/Genra.svg (text as paths, no components) + src/Layout.hpp
        panel.py preview out.svg   -> preview with VCV components and mock display content
        panel.py mock out.svg      -> quick sketch with drawn components

All coordinates in mm. VCV Rack: height 128.5 mm, 1 HP = 5.08 mm.
Preview needs the Rack ComponentLibrary SVGs in ../ext/Rack/res/ComponentLibrary.
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

HP = 64
W = HP * 5.08
H = 128.5

BG = "#17181d"
PLATE = "#212329"
LINE = "#34363f"
TXT = "#d9dbe2"
DIM = "#8a8d99"
CHORDCOL = "#2f9e8f"  # harmony: dark teal
SCENECOL = "#ffd23d"
TRACKS = [("DRUMS", "#7b6cff"), ("BASS", "#b8f23a"), ("MELODY", "#3dc9ff"), ("PADS", "#e0779e")]

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
components = []  # (kind, name, x, y[, w, h])


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


def jack(name, x, y, label=None, out_=False, color=None, lsize=1.8, ldy=6.3):
    components.append(("output" if out_ else "input", name, x, y))
    if out_:
        e(f'<rect x="{x-4.6}" y="{y-4.6}" width="9.2" height="11.0" rx="1.3" fill="{color}" fill-opacity="0.22"/>')
    if not REAL:
        e(f'<circle cx="{x}" cy="{y}" r="3.15" fill="#b9bcc6" stroke="#0d0e11" stroke-width="0.3"/>')
        e(f'<circle cx="{x}" cy="{y}" r="2.35" fill="#5a5d68"/>')
        e(f'<circle cx="{x}" cy="{y}" r="1.45" fill="#08090b"/>')
    if label:
        text(x, y + ldy, label, lsize, TXT if out_ else DIM, 600)


def button(name, x, y, color, label=None, lit=False, label2=None, lsize=1.8):
    components.append(("button", name, x, y))
    if not REAL:
        e(f'<rect x="{x-R_BEZEL}" y="{y-R_BEZEL}" width="{2*R_BEZEL}" height="{2*R_BEZEL}" rx="1.4" fill="#2c2e36" stroke="#0d0e11" stroke-width="0.35"/>')
        e(f'<circle cx="{x}" cy="{y}" r="2.2" fill="{color if lit else "#3d3f48"}"/>')
    if label:
        text(x, y + R_BEZEL + 2.6, label, lsize, DIM)
    if label2:
        text(x, y + R_BEZEL + 4.9, label2, lsize, DIM)


def field(name, x, y, w, h, color):
    """Genre selector: background on the panel, text drawn by the module widget."""
    components.append(("field", name, x, y, w, h))
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="0.9" fill="#07080a" stroke="{color}" stroke-opacity="0.45" stroke-width="0.3"/>')


# ---------------------------------------------------------------- panel
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
if not REAL:
    for sx in (7.5, W - 7.5):
        for sy in (3, H - 3):
            e(f'<circle cx="{sx}" cy="{sy}" r="1.6" fill="#3a3c45"/>')

text(14, 9.2, "GENRA", 5.2, TXT, 800, "start", 0.9)
text(40, 9.0, "GENERATIVE 4-TRACK SEQUENCER", 2.0, DIM, 600, "start", 0.5)
text(W - 48, 9.0, "V3", 2.0, DIM, 800, "end", 0.5)
for i, (_, c) in enumerate(TRACKS):
    e(f'<rect x="{W-42+i*7.5:.2f}" y="6.4" width="5.5" height="1.1" rx="0.5" fill="{c}"/>')

# display area (content is drawn by the module widget)
DISP = (6, 12.5, W - 12, 17)
dx, dy, dw, dh = DISP
e(f'<rect x="{dx}" y="{dy}" width="{dw:.2f}" height="{dh}" rx="1.6" fill="#07080a" stroke="{LINE}" stroke-width="0.4"/>')

# ---------------------------------------------------------------- global row
gy = 40.6
e(f'<rect x="6" y="32.5" width="{W-12:.2f}" height="17" rx="1.6" fill="{PLATE}"/>')
knob("GENRE", 19, gy, R_LARGE, "#ffffff", "GENRE", gy + 8.9)
glob = ["BPM", "KEY", "SCALE", "VARIANT", "COMPLEX", "COLOR", "MUTATE", "SWING", "LENGTH"]
for i, lab in enumerate(glob):
    knob(lab, 36 + i * 15.2, gy, R_SMALL, label=lab, ly=gy + 7.3)
sep = 36 + 8 * 15.2 + 10.5
e(f'<line x1="{sep:.2f}" y1="35" x2="{sep:.2f}" y2="47" stroke="{LINE}" stroke-width="0.3"/>')
button("NEW", sep + 9, gy - 1, "#ffffff", "NEW", label2="ALL")
button("RUN", sep + 22, gy - 1, "#5dff8a", "RUN", True)
# chord group
cg0 = sep + 31
cgw = W - 6 - cg0 - 1.5
e(f'<rect x="{cg0:.2f}" y="34.2" width="{cgw:.2f}" height="13.8" rx="1.3" fill="{CHORDCOL}" fill-opacity="0.13"/>')
text(cg0 + cgw / 2, 36.9, "HARMONY", 1.7, CHORDCOL, 800, "middle", 0.5)
button("CHORDNEW", cg0 + cgw * 0.2, gy, CHORDCOL, "NEW")
button("CHORDLOCK", cg0 + cgw * 0.5, gy, CHORDCOL, "LOCK")
jack("IN_CHORDNEW", cg0 + cgw * 0.8, gy, "TRIG")

# ---------------------------------------------------------------- inputs (3 x 5)
TOP = 51.5
BOT = 127.0
INW = 40
e(f'<rect x="6" y="{TOP}" width="{INW}" height="{BOT-TOP}" rx="1.6" fill="{PLATE}"/>')
text(6 + INW / 2, TOP + 4.6, "IN", 2.0, TXT, 700)
ins = ["CLOCK", "RESET", "RUN",
       "NEW", "FILL", "MUTATE",
       "GENRE", "VARIANT", "TRANSP",
       "COMPLEX", "COLOR", "SWING",
       "BPM", "LENGTH", "CHORD"]
IN_Y0, IN_DY = 62.0, 13.2
for i, lab in enumerate(ins):
    jack("IN_" + lab, 14 + (i % 3) * 12, IN_Y0 + (i // 3) * IN_DY, lab, lsize=1.6, ldy=5.9)
text(38, IN_Y0 + 4 * IN_DY + 8.2, "POLY", 1.3, CHORDCOL, 700)

# ---------------------------------------------------------------- sync / harmony outputs (2 x 4)
OW = 27
OX = W - 6 - OW  # right edge (scenes sit next to the inputs)
e(f'<rect x="{OX:.2f}" y="{TOP}" width="{OW}" height="{BOT-TOP}" rx="1.6" fill="{PLATE}"/>')
text(OX + OW / 2, TOP + 4.6, "OUT", 2.0, TXT, 700)
SYNCCOL = "#c9ccd6"
outs_s = [("CLOCK", SYNCCOL), ("RESET", SYNCCOL), ("RUN", SYNCCOL), ("BAR", SYNCCOL),
          ("END", SYNCCOL), ("FILL", SYNCCOL), ("ROOT", CHORDCOL), ("CHORD", CHORDCOL)]
for i, (lab, c) in enumerate(outs_s):
    jack("OUT_" + lab, OX + 7.5 + (i % 2) * 12, 64.0 + (i // 2) * 14.2, lab, True, c, ldy=5.9)
text(OX + 7.5 + 12, 64.0 + 3 * 14.2 + 9.0, "POLY", 1.3, CHORDCOL, 700)

# ---------------------------------------------------------------- scenes column
SCW = 30
sx0 = 6 + INW + 1
e(f'<rect x="{sx0:.2f}" y="{TOP}" width="{SCW}" height="{BOT-TOP}" rx="1.6" fill="{PLATE}"/>')
e(f'<rect x="{sx0:.2f}" y="{TOP}" width="{SCW}" height="1.3" rx="0.6" fill="{SCENECOL}"/>')
scx = sx0 + SCW / 2
text(scx, TOP + 6.1, "SCENES", 2.6, SCENECOL, 800, "middle", 0.6)
for i in range(8):
    bx = scx - 6.5 + (i % 2) * 13
    by = 65.5 + (i // 2) * 10.4
    button(f"SCENE_{i}", bx, by, SCENECOL, lit=(i == 0))
    text(bx + (-6.2 if i % 2 == 0 else 6.2), by + 0.8, str(i + 1), 2.0, DIM, 700)
button("STORE", scx, 107.2, "#ffffff", "STORE")
jack("IN_SCENE", scx - 6.5, 119.0, "CV", lsize=1.6, ldy=6.0)
jack("IN_NEXT", scx + 6.5, 119.0, "NEXT", lsize=1.6, ldy=6.0)

# ---------------------------------------------------------------- tracks
x0 = sx0 + SCW + 1
TW = OX - 1 - x0
DRW = 63
OTW = (TW - DRW) / 3
Y_TITLE, Y_KNOB, Y_FIELD, Y_BTN = TOP + 5.6, 64.5, 79.6, 91.2
for t, (name, col) in enumerate(TRACKS):
    cw = DRW if name == "DRUMS" else OTW
    xc = x0 + cw / 2
    e(f'<rect x="{x0+0.6:.2f}" y="{TOP}" width="{cw-1.2:.2f}" height="{BOT-TOP}" rx="1.6" fill="{PLATE}"/>')
    e(f'<rect x="{x0+0.6:.2f}" y="{TOP}" width="{cw-1.2:.2f}" height="1.3" rx="0.6" fill="{col}"/>')
    text(xc, Y_TITLE, name, 2.6, col, 800, "middle", 0.6)
    # knobs + density CV
    if name == "DRUMS":
        kx = [xc - 21, xc - 7, xc + 7, xc + 21]
        knob("DENS_0", kx[0], Y_KNOB, R_KNOB, col, "DENSITY", Y_KNOB + 8.8)
        knob("FILL", kx[1], Y_KNOB, R_SMALL, col, "FILL", Y_KNOB + 8.8)
        knob("GHOST", kx[2], Y_KNOB, R_SMALL, col, "GHOST", Y_KNOB + 8.8)
        jack("IN_DENS_0", kx[3], Y_KNOB, "DENS CV", lsize=1.6, ldy=8.8)
    else:
        second = {"BASS": "MOVE", "MELODY": "RANGE", "PADS": "VOICING"}[name]
        kx = [xc - 16.2, xc - 5.4, xc + 4.8, xc + 15.6]
        knob(f"DENS_{t}", kx[0], Y_KNOB, R_KNOB, col, "DENSITY", Y_KNOB + 8.8)
        knob(second, kx[1] + 0.6, Y_KNOB, R_SMALL, col, second, Y_KNOB + 8.8)
        knob(f"OCT_{t-1}", kx[2] + 0.6, Y_KNOB, R_TRIM, "#ffffff", "OCT", Y_KNOB + 8.8)
        jack(f"IN_DENS_{t}", kx[3], Y_KNOB, "DENS CV", lsize=1.5, ldy=8.8)
    # genre field + genre CV
    fw = cw - 7 - 13
    field(f"FIELD_{t}", x0 + 3.5, Y_FIELD - 2.5, fw, 5.0, col)
    text(x0 + 3.5 + 0.2, Y_FIELD - 3.6, "GENRE", 1.4, col, 700, "start", 0.3)
    jack(f"IN_GENRE_{t}", x0 + cw - 3.5 - 5.6, Y_FIELD, "CV", lsize=1.4, ldy=5.6)
    # buttons
    sp = 13.5 if name == "DRUMS" else 10.2
    bx = [xc - 1.5 * sp, xc - 0.5 * sp, xc + 0.5 * sp, xc + 1.5 * sp]
    button(f"MUTE_{t}", bx[0], Y_BTN, col, "MUTE")
    button(f"LOCK_{t}", bx[1], Y_BTN, col, "LOCK", t == 0)
    button(f"NEW_{t}", bx[2], Y_BTN, "#ffffff", "NEW")
    jack(f"IN_NEW_{t}", bx[3], Y_BTN, "TRIG", ldy=6.2)
    e(f'<line x1="{x0+4:.2f}" y1="100.0" x2="{x0+cw-4:.2f}" y2="100.0" stroke="{LINE}" stroke-width="0.3"/>')
    text(xc, 102.6, "OUT  ·  POLY 4" if name == "PADS" else "OUT", 1.6, col if name == "PADS" else DIM, 700, "middle", 0.4)
    r1, r2 = 107.6, 119.0
    if name == "DRUMS":
        names = ["KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"]
        for i, n in enumerate(names):
            jack(f"DRUM_{i}", xc - 21 + (i % 4) * 14, r1 if i < 4 else r2, n, True, col, ldy=5.9)
    else:
        pre = {"BASS": "BASS", "MELODY": "MEL", "PADS": "PAD"}[name]
        outs = ["VOCT", "GATE", "VEL", "ACC", "SLIDE"] if name != "PADS" else ["VOCT", "GATE", "VEL", "MOD"]
        labels = {"VOCT": "V/OCT", "GATE": "GATE", "VEL": "VEL", "ACC": "ACC", "SLIDE": "SLIDE", "MOD": "MOD"}
        for i, o in enumerate(outs):
            if i < 3:
                x, y = xc - 12.4 + i * 12.4, r1
            else:
                x, y = (xc - 6.2 + (i - 3) * 12.4 if len(outs) == 5 else xc), r2
            jack(f"{pre}_{o}", x, y, labels[o], True, col, ldy=5.9)
    x0 += cw

e("</svg>")

if PREVIEW:
    CL = os.path.abspath(os.path.join(ROOT, "..", "ext", "Rack", "res", "ComponentLibrary"))
    PX = 2.9528
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

    STORED = "#ff2a1a"  # scene slot holds a pattern (red)
    lit = {"RUN": "#5dff8a", "LOCK_1": "#b8f23a", "SCENE_0": SCENECOL, "SCENE_1": STORED, "SCENE_2": STORED, "SCENE_5": STORED}
    # mock content of the genre fields: (text, follows global?)
    FIELDS = {0: ("GLOBAL", True), 1: ("DUB · STEPPERS", False), 2: ("LO-FI · JAZZY", False), 3: ("AMBIENT · BRIGHT", False)}
    for c in components:
        kind, name, x, y = c[:4]
        if kind in ("input", "output"):
            img("PJ301M", x, y)
        elif kind == "button":
            img("VCVBezel", x, y)
            col = lit.get(name)
            out.insert(-1, f'<circle cx="{x}" cy="{y}" r="2.6" fill="{col or "#2a2b30"}" fill-opacity="{0.95 if col else 0.6}"/>')
        elif kind == "field":
            w, h = c[4], c[5]
            t = int(name.split("_")[1])
            label, glob_ = FIELDS[t]
            tc = DIM if glob_ else TRACKS[t][1]
            out.insert(-1, f'<text x="{x+1.4:.2f}" y="{y+h/2+0.85:.2f}" font-family="Inter" font-weight="800" font-size="2.0" letter-spacing="0.15" fill="{tc}">{label}</text>')
            ax, ay = x + w - 2.4, y + h / 2
            out.insert(-1, f'<path d="M{ax-1:.2f} {ay-0.5:.2f} L{ax+1:.2f} {ay-0.5:.2f} L{ax:.2f} {ay+0.7:.2f} Z" fill="{tc}"/>')
        else:
            k = "RoundLargeBlackKnob" if name == "GENRE" else ("RoundBlackKnob" if name.startswith("DENS") else ("Trimpot" if name.startswith("OCT") else "RoundSmallBlackKnob"))
            img(k, x, y, "_bg")
            img(k, x, y)
    for sx in (7.62, W - 7.62):
        for sy in (2.54, H - 2.54):
            out.insert(-1, f'<circle cx="{sx:.2f}" cy="{sy}" r="1.7" fill="#2b2c31"/><path d="M{sx-1:.2f} {sy-1} L{sx+1:.2f} {sy+1}" stroke="#111" stroke-width="0.35"/>')
    # display content (approximation of what the module draws)
    D = []
    D.append(f'<text x="{dx+4}" y="{dy+7.2}" font-family="Inter" font-weight="800" font-size="5.2" fill="#f2f3f7" letter-spacing="0.3">JUNGLE</text><text x="{dx+28.5}" y="{dy+7.2}" font-family="Inter" font-weight="700" font-size="2.6" fill="#8a8d99" letter-spacing="0.4">· RAGGA</text>')
    D.append(f'<text x="{dx+4}" y="{dy+12.6}" font-family="DejaVu Sans Mono" font-size="2.6" fill="#8a8d99">F MINOR  170 BPM  SWING 54%</text>')
    chords = ["Fm", "Db", "Fm", "Gb"]
    cx0 = dx + 78
    for i, ch in enumerate(chords):
        x = cx0 + i * 18
        act = i == 1
        D.append(f'<rect x="{x}" y="{dy+2.6}" width="16.8" height="6" rx="0.8" fill="{CHORDCOL if act else "#1a1b21"}"/>')
        D.append(f'<text x="{x+8.4}" y="{dy+6.5}" text-anchor="middle" font-family="Inter" font-weight="800" font-size="2.7" fill="{"#0a0a0d" if act else "#8a8d99"}">{ch}</text>')
    D.append(f'<text x="{cx0}" y="{dy+13}" font-family="DejaVu Sans Mono" font-size="2.4" fill="#8a8d99">BAR 2/4   SCENE 1</text>')
    D.append(f'<text x="{cx0+4*18-1.2}" y="{dy+13}" text-anchor="end" font-family="DejaVu Sans Mono" font-size="2.4" fill="{CHORDCOL}">BASS LOCKED</text>')
    # lanes with per-track genre labels
    lanes = ["X..x..X.x.X..x.x", "X.......X..X....", "......X..X..X...", "X...............", ]
    tg = ["JUNGLE", "DUB", "LO-FI", "AMBIENT"]
    lx = dx + dw - 4 - 16 * 2.3
    for t, (_, c) in enumerate(TRACKS):
        y = dy + 2.8 + t * 3.3
        D.append(f'<text x="{lx-1.6:.2f}" y="{y+2.0:.2f}" text-anchor="end" font-family="DejaVu Sans Mono" font-size="2.1" fill="{c}">{tg[t]}</text>')
        for i in range(16):
            x = lx + i * 2.3
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
    for c in components:
        if c[0] == "field":
            lines.append(f"static const float {c[1]}[4] = {{{c[2]:.3f}f, {c[3]:.3f}f, {c[4]:.3f}f, {c[5]:.3f}f}};")
        else:
            lines.append(f"static const float {c[1]}[2] = {{{c[2]:.3f}f, {c[3]:.3f}f}};")
    lines.append(f"static const float DISPLAY[4] = {{{dx:.3f}f, {dy:.3f}f, {dw:.3f}f, {dh:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    open(os.path.join(ROOT, "src", "Layout.hpp"), "w").write("\n".join(lines) + "\n")
    print(f"wrote res/Genra.svg and src/Layout.hpp ({len(components)} components)")
else:
    open(sys.argv[2] if len(sys.argv) > 2 else "genra_panel_mock.svg", "w").write("\n".join(out))
