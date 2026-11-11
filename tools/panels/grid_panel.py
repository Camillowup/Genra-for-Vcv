#!/usr/bin/env python3
"""GENRA GRID – editor expander panel (draft / preview).

Usage:  grid_panel.py real                        -> res/GenraGrid.svg + src/grid/LayoutGrid.hpp
        grid_panel.py preview out.svg [piano|drums] -> preview with components and mock display
Needs the Rack ComponentLibrary SVGs in ../ext/Rack/res/ComponentLibrary.
All coordinates in mm (height 128.5 mm, 1 HP = 5.08 mm).
"""
import os
import re
import sys

MODE = sys.argv[1] if len(sys.argv) > 1 else "preview"
REAL = MODE == "real"
VIEW = sys.argv[3] if len(sys.argv) > 3 else "piano"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
CL = os.path.abspath(os.path.join(ROOT, "..", "ext", "Rack", "res", "ComponentLibrary"))

HP = 50
W = HP * 5.08
H = 128.5
BG, PLATE, LINE, TXT, DIM = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99"
HARM = "#2f9e8f"
TRACKS = [("DRUMS", "#7b6cff"), ("BASS", "#b8f23a"), ("MELODY", "#3dc9ff"), ("PADS", "#e0779e")]
CUR = 0 if VIEW == "drums" else 1

out = []
comps = []


def e(s):
    out.append(s)


FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_fc = {}


def text_path(x, y, s, size, color, weight, anchor, spacing):
    from fontTools.ttLib import TTFont
    from fontTools.pens.svgPathPen import SVGPathPen
    from fontTools.pens.transformPen import TransformPen
    if weight not in _fc:
        f = TTFont(FONT_DIR + FONTS.get(weight, FONTS[600]))
        _fc[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    gs, cmap, upm, hmtx = _fc[weight]
    scale = size / upm
    glyphs = [cmap.get(ord(c), "space") for c in s]
    width = sum(hmtx[g][0] * scale + spacing for g in glyphs) - spacing
    cx = x - (width / 2 if anchor == "middle" else (width if anchor == "end" else 0))
    paths = []
    for g in glyphs:
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, cx, y)))
        if pen.getCommands():
            paths.append(pen.getCommands())
        cx += hmtx[g][0] * scale + spacing
    if paths:
        e(f'<path fill="{color}" d="{" ".join(paths)}"/>')


DISPLAY_TEXT = False  # True while drawing mock display content (never in real mode)


def text(x, y, s, size=2.1, color=TXT, weight=600, anchor="middle", spacing=0.15, family="Inter"):
    if not s:
        return
    if REAL:
        if not DISPLAY_TEXT:
            text_path(x, y, s, size, color, weight, anchor, spacing)
        return
    e(f'<text x="{x:.2f}" y="{y:.2f}" font-family="{family}" font-size="{size}" font-weight="{weight}" '
      f'fill="{color}" text-anchor="{anchor}" letter-spacing="{spacing}">{s}</text>')


def knob(name, x, y, kind="RoundSmallBlackKnob", label=None, ly=None):
    comps.append((kind, x, y, None, name))
    if REAL:
        e(f'<circle cx="{x}" cy="{y}" r="{3.84+0.9}" fill="none" stroke="{LINE}" stroke-width="0.35" stroke-dasharray="0.4 0.9"/>')
    if label:
        text(x, ly or y + 7.4, label, 2.0, DIM)


def jack(x, y, label, name=""):
    comps.append(("PJ301M", x, y, None, name))
    text(x, y + 6.2, label, 1.7, DIM)


def button(x, y, label=None, lit=None, label2=None, name=""):
    comps.append(("VCVBezel", x, y, lit, name))
    if label:
        text(x, y + 6.3, label, 1.75, DIM)
    if label2:
        text(x, y + 8.5, label2, 1.75, DIM)


e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(14, 9.2, "GENRA", 5.2, TXT, 800, "start", 0.9)
text(41.5, 9.0, "GRID EDITOR", 2.0, DIM, 600, "start", 0.5)
# link LED
comps.append(("LED", W - 37, 7.6, "#5dff8a", "LINKLED"))
text(W - 34.6, 8.4, "LINK", 1.8, DIM, 700, "start", 0.4)
for i, (_, c) in enumerate(TRACKS):
    e(f'<rect x="{W-24+i*4.6:.2f}" y="7.0" width="3.4" height="1.1" rx="0.5" fill="{c}"/>')

# ---------------------------------------------------------------- display
dx, dy, dw, dh = 6, 12.5, W - 12, 82
e(f'<rect x="{dx}" y="{dy}" width="{dw:.2f}" height="{dh}" rx="1.6" fill="#07080a" stroke="{LINE}" stroke-width="0.4"/>')

name, col = TRACKS[CUR]
DISPLAY_TEXT = True
_mark = len(out)
# header line inside the display
text(dx + 3.5, dy + 5.2, name, 3.2, col, 800, "start", 0.5)
text(dx + 3.5 + (len(name) * 2.55 + 2), dy + 5.2,
     "· JUNGLE · DARK  ·  GLOBAL" if CUR == 0 else "· DUB · STEPPERS", 2.2, DIM, 700, "start", 0.3)
text(dx + dw - 3.5, dy + 5.2, "BEARBEITET  ·  SNAP SCALE  ·  " + ("" if CUR == 0 else "CHORD FOLLOW  ·  ") + "BAR 1-2 / 4  ·  F MINOR", 2.1, DIM, 600, "end", 0.3, "DejaVu Sans Mono")

# grid geometry
gx0 = dx + 15          # left: row labels
gx1 = dx + dw - 12.5   # right: swing badges (drawn by the module)
gy0 = dy + 13.0        # below chord ruler
vel_h = 10.0
gy1 = dy + dh - vel_h - 4.0
steps = 32
sw = (gx1 - gx0) / steps

# chord ruler (piano roll) / bar ruler
chords = [("Fm9", 0, 16), ("Dbmaj7", 16, 8), ("Ebsus2", 24, 8)]
for nm, st, ln in chords:
    x = gx0 + st * sw
    act = st == 16
    e(f'<rect x="{x+0.2:.2f}" y="{dy+7.6:.2f}" width="{ln*sw-0.4:.2f}" height="4.2" rx="0.6" fill="{HARM if act else "#1a1b21"}" fill-opacity="{1 if act else 1}"/>')
    text(x + 1.6, dy + 10.6, nm, 2.2, "#0a0a0d" if act else DIM, 800, "start", 0.1)
for b in range(2):
    text(gx0 + b * 16 * sw + 0.6, gy0 - 0.6, "", 1.6, DIM)

if CUR == 0:
    rows = ["KICK", "SNARE", "CLAP", "RIM", "C.HAT", "O.HAT", "TOM", "PERC"]
else:
    # piano roll: F minor, two octaves around the bass register (top = high)
    names = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
    scale = {5, 7, 8, 10, 0, 1, 3}  # F minor pitch classes
    rows = []
    for midi in range(50, 26, -1):  # D3 .. Eb1
        rows.append((midi, names[midi % 12] + str(midi // 12 - 1), (midi % 12) in scale, midi % 12 == 5))
nrows = len(rows)
rh = (gy1 - gy0) / nrows

# chord background tint (piano roll: chord tones of each chord)
CHORD_PCS = [{5, 8, 0, 3, 7}, {1, 5, 8, 0}, {3, 5, 10}]
for r in range(nrows):
    y = gy0 + r * rh
    if CUR == 0:
        fill = "#101116" if r % 2 == 0 else "#0c0d11"
        e(f'<rect x="{gx0:.2f}" y="{y:.2f}" width="{gx1-gx0:.2f}" height="{rh:.2f}" fill="{fill}"/>')
        text(gx0 - 1.6, y + rh / 2 + 0.8, rows[r], 2.0, DIM, 700, "end", 0.2)
    else:
        midi, nm, inscale, root = rows[r]
        fill = "#13141a" if inscale else "#0a0a0d"
        e(f'<rect x="{gx0:.2f}" y="{y:.2f}" width="{gx1-gx0:.2f}" height="{rh:.2f}" fill="{fill}"/>')
        for ci, (cn, st, ln) in enumerate(chords):
            if midi % 12 in CHORD_PCS[ci]:
                e(f'<rect x="{gx0+st*sw:.2f}" y="{y:.2f}" width="{ln*sw:.2f}" height="{rh:.2f}" fill="{HARM}" fill-opacity="0.10"/>')
        # keyboard column
        black = nm[1:2] in ("#", "b")
        e(f'<rect x="{gx0-13.2:.2f}" y="{y+0.1:.2f}" width="{12.6 if not black else 8:.2f}" height="{rh-0.2:.2f}" rx="0.3" fill="{"#2a2b33" if black else "#c9ccd6"}"/>')
        if root or not black:
            text(gx0 - 1.4, y + rh / 2 + 0.7, nm, 1.6, "#b8f23a" if root else "#3a3c46", 800, "end", 0.1)
# vertical lines
for s in range(steps + 1):
    x = gx0 + s * sw
    wcol, ww = (LINE, 0.12) if s % 4 else (("#4a4d58", 0.3) if s % 16 else ("#7a7e8c", 0.45))
    e(f'<line x1="{x:.2f}" y1="{gy0:.2f}" x2="{x:.2f}" y2="{gy1:.2f}" stroke="{wcol}" stroke-width="{ww}"/>')
for b in range(2):
    text(gx0 + b * 16 * sw + 1.0, gy1 + 3.0, f"BAR {b+1}", 1.6, DIM, 700, "start", 0.3, "DejaVu Sans Mono")

# notes
playhead = 21
if CUR == 0:
    pat = ["X......x..X.....X.........X..x..",
           "....X.......X.......X.......X..R",
           "................................",
           "..o....o.....o.....o..o.....o...",
           "x.x.x.x.x.x.x.x.x.x.x.x.x.x.RRRR",
           "......x...............x.........",
           ".............................oo.",
           ".o...o...o...o...o...o...o..o..."]
    for r, line in enumerate(pat):
        for s, ch in enumerate(line):
            if ch == ".":
                continue
            v = {"X": 1.0, "x": 0.75, "o": 0.4, "R": 0.85}[ch]
            x, y = gx0 + s * sw + 0.45, gy0 + r * rh + 0.9
            e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{sw-0.9:.2f}" height="{rh-1.8:.2f}" rx="0.7" fill="{col}" fill-opacity="{0.35+0.65*v:.2f}"/>')
            if ch == "R":  # roll / ratchet marks
                for k in range(1, 3):
                    xx = x + k * (sw - 0.9) / 3
                    e(f'<line x1="{xx:.2f}" y1="{y+0.6:.2f}" x2="{xx:.2f}" y2="{y+rh-2.4:.2f}" stroke="#07080a" stroke-width="0.35"/>')
            if ch == "o":
                e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{sw-0.9:.2f}" height="{rh-1.8:.2f}" rx="0.7" fill="none" stroke="{col}" stroke-width="0.25"/>')
    vel = [(i, {"X": 1.0, "x": 0.75, "o": 0.4, "R": 0.85}[c]) for i, c in enumerate(pat[0]) if c != "."]
else:
    # (start, len, midi, vel, slide, accent)
    notes = [(0, 3, 29, 1.0, False, True), (3, 1, 29, 0.6, True, False), (4, 2, 36, 0.8, False, False),
             (8, 4, 29, 0.9, False, False), (12, 2, 32, 0.7, True, False), (14, 2, 34, 0.75, False, False),
             (16, 3, 37, 1.0, False, True), (19, 1, 37, 0.55, False, False), (20, 3, 41, 0.85, True, False),
             (23, 1, 44, 0.7, False, False), (24, 4, 39, 0.9, False, False), (28, 2, 34, 0.7, False, False),
             (30, 2, 41, 0.8, False, False)]
    rowOf = {r[0]: i for i, r in enumerate(rows)}
    CHANCE = {3: 50, 5: 75, 11: 25, 12: 60}
    for i, (st, ln, midi, v, sl, acc) in enumerate(notes):
        x, y = gx0 + st * sw + 0.3, gy0 + rowOf[midi] * rh + 0.25
        sel = i == 8
        e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{ln*sw-0.6:.2f}" height="{rh-0.5:.2f}" rx="0.6" fill="{col}" fill-opacity="{0.4+0.6*v:.2f}"'
          + (' stroke="#ffffff" stroke-width="0.35"' if sel else "") + "/>")
        # resize handle
        e(f'<rect x="{x+ln*sw-1.5:.2f}" y="{y+0.5:.2f}" width="0.5" height="{rh-1.5:.2f}" fill="#07080a" fill-opacity="0.5"/>')
        if acc:
            text(x + 1.2, y + rh - 0.9, ">", 1.8, "#07080a", 800, "start", 0)
        if i in CHANCE:
            e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{ln*sw-0.6:.2f}" height="{rh-0.5:.2f}" rx="0.6" fill="#07080a" fill-opacity="0.45" stroke="{col}" stroke-width="0.3" stroke-dasharray="0.7 0.45"/>')
            text(x + ln * sw - 1.9, y + rh - 1.05, f"{CHANCE[i]}%", 1.45, "#ffffff", 700, "end", 0)
        if sl and i + 1 < len(notes):
            nst, _, nmidi, *_ = notes[i + 1]
            x2, y2 = gx0 + nst * sw + 0.6, gy0 + rowOf[nmidi] * rh + rh / 2
            x1, y1 = x + ln * sw - 0.6, y + rh / 2
            e(f'<path d="M{x1:.2f} {y1:.2f} C{x1+1.5:.2f} {y1:.2f} {x2-1.5:.2f} {y2:.2f} {x2:.2f} {y2:.2f}" fill="none" stroke="#ffffff" stroke-width="0.4" stroke-dasharray="0.6 0.4"/>')
    vel = [(st, CHANCE.get(i, 100) / 100.0) for i, (st, ln, midi, v, sl, acc) in enumerate(notes)]
# playhead
px = gx0 + playhead * sw
e(f'<rect x="{px:.2f}" y="{gy0:.2f}" width="{sw:.2f}" height="{gy1-gy0:.2f}" fill="#ffffff" fill-opacity="0.06"/>')
e(f'<line x1="{px:.2f}" y1="{gy0-1:.2f}" x2="{px:.2f}" y2="{gy1:.2f}" stroke="#ffffff" stroke-width="0.3"/>')

# velocity lane
vy0, vy1 = gy1 + 4.0, gy1 + 4.0 + vel_h
e(f'<rect x="{gx0:.2f}" y="{vy0:.2f}" width="{gx1-gx0:.2f}" height="{vel_h:.2f}" fill="#0c0d11"/>')
lane_chance = CUR != 0
text(gx0 - 1.6, vy0 + 3.0, "VEL", 1.7, DIM if lane_chance else col, 700, "end", 0.3)
text(gx0 - 1.6, vy0 + 6.4, "CHANCE", 1.5, col if lane_chance else DIM, 700, "end", 0.2)
if CUR == 0:
    text(gx0 - 1.6, vy0 + vel_h / 2 + 2.4, "KICK", 1.6, col, 700, "end", 0.3)
    e(f'<rect x="{gx0-14.5:.2f}" y="{gy0+0.4:.2f}" width="{gx1-gx0+14.5:.2f}" height="{rh-0.8:.2f}" rx="0.6" fill="none" stroke="{col}" stroke-opacity="0.5" stroke-width="0.3"/>')
for st, v in vel:
    x = gx0 + st * sw + sw * 0.3
    hgt = v * (vel_h - 1.2)
    e(f'<rect x="{x:.2f}" y="{vy1-hgt-0.4:.2f}" width="{sw*0.4:.2f}" height="{hgt:.2f}" rx="0.3" fill="{col}" fill-opacity="0.85"/>')
    e(f'<circle cx="{x+sw*0.2:.2f}" cy="{vy1-hgt-0.4:.2f}" r="0.55" fill="{col}"/>')

DISPLAY_TEXT = False
if REAL:
    del out[_mark:]  # display content is drawn by the module widget
# ---------------------------------------------------------------- controls
cy = 111.0
top = 97.5
# track selector
e(f'<rect x="6" y="{top}" width="54" height="{H-top-4}" rx="1.6" fill="{PLATE}"/>')
text(33, top + 4.2, "TRACK", 1.8, TXT, 800, "middle", 0.5)
for i, (n, c) in enumerate(TRACKS):
    x = 13 + i * 13
    button(x, cy, n, c if i == CUR else None, name=f"TRACK_{i}")
    e(f'<rect x="{x-4.2:.2f}" y="{top+6.2:.2f}" width="8.4" height="0.9" rx="0.4" fill="{c}"/>')

# view
vx = 62
e(f'<rect x="{vx}" y="{top}" width="48" height="{H-top-4}" rx="1.6" fill="{PLATE}"/>')
text(vx + 24, top + 4.2, "VIEW", 1.8, TXT, 800, "middle", 0.5)
knob("BAR", vx + 7.5, cy, label="BAR")
knob("ZOOM", vx + 18.5, cy, label="ZOOM")
knob("PITCH", vx + 29.5, cy, label="PITCH")
button(vx + 40.5, cy, "AUTO", "#ffffff", "SCROLL", name="AUTO")

# notes
nx = 112
e(f'<rect x="{nx}" y="{top}" width="61" height="{H-top-4}" rx="1.6" fill="{PLATE}"/>')
text(nx + 30.5, top + 4.2, "NOTES", 1.8, TXT, 800, "middle", 0.5)
knob("VEL", nx + 7.5, cy, label="VEL")
knob("LEN", nx + 18.5, cy, label="LEN")
knob("CHANCE", nx + 29.5, cy, label="CHANCE")
nx += 11
button(nx + 30.5, cy, "SNAP", None, name="SNAP")
for i, m in enumerate(["OFF", "SCALE", "CHORD"]):
    yy = cy - 3.4 + i * 3.4
    comps.append(("TINY", nx + 35.6, yy, "#e6e7ec" if i == 1 else None, f"SNAPLED_{i}"))
text(nx + 30.5, cy + 8.5, "", 1.6, DIM)
button(nx + 43.5, cy, "CHORD", HARM if CUR != 0 else None, "FOLLOW", name="FOLLOW")

# edit
ex = 175
ew = W - 6 - ex
e(f'<rect x="{ex}" y="{top}" width="{ew:.2f}" height="{H-top-4}" rx="1.6" fill="{PLATE}"/>')
text(ex + ew / 2, top + 4.2, "EDIT", 1.8, TXT, 800, "middle", 0.5)
labels = ["UNDO", "REDO", "COPY", "PASTE", "CLEAR", "GEN", "REC"]
sp = (ew - 4) / len(labels)
for i, lab in enumerate(labels):
    x = ex + 2 + sp * (i + 0.5)
    button(x, cy, lab, "#ff4a4a" if lab == "REC" else None, "MIDI" if lab == "REC" else ("BAR" if lab in ("COPY", "PASTE") else ("TRACK" if lab in ("CLEAR", "GEN") else None)), name=lab)
    if lab == "REC":
        e(f'<line x1="{x - sp / 2:.2f}" y1="{cy - 5:.2f}" x2="{x - sp / 2:.2f}" y2="{cy + 6:.2f}" stroke="#2a2c33" stroke-width="0.3"/>')

e("</svg>")

if REAL:
    open(os.path.join(ROOT, "res", "GenraGrid.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/panels/grid_panel.py - do not edit", "#pragma once", "namespace glayout {"]
    for kind, x, y, lit, nm in comps:
        if nm:
            lines.append(f"static const float {nm}[2] = {{{x:.3f}f, {y:.3f}f}};")
    lines.append(f"static const float DISPLAY[4] = {{{dx:.3f}f, {dy:.3f}f, {dw:.3f}f, {dh:.3f}f}};")
    lines.append(f"static const float GRID[4] = {{{gx0-dx:.3f}f, {gy0-dy:.3f}f, {gx1-dx:.3f}f, {gy1-dy:.3f}f}}; // grid area inside the display")
    lines.append(f"static const float VELLANE[2] = {{{vy0-dy:.3f}f, {vy1-dy:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    open(os.path.join(ROOT, "src", "grid", "LayoutGrid.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/GenraGrid.svg and src/grid/LayoutGrid.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- components
_cache = {}


def img(name, x, y, layer=""):
    fn = f"{CL}/{name}{layer}.svg"
    if fn not in _cache:
        src = open(fn).read()
        src = re.sub(r"<\?xml[^>]*\?>", "", src)
        src = re.sub(r"<!DOCTYPE[^>]*>", "", src)
        m = re.search(r"<svg[^>]*>", src)
        tag = m.group(0)
        vb = re.search(r'viewBox="([^"]+)"', tag)
        w = float(re.search(r'width="([0-9.]+)', tag).group(1))
        h = float(re.search(r'height="([0-9.]+)', tag).group(1))
        body = src[m.end():src.rindex("</svg>")]
        body = re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)", "", body, flags=re.S)
        body = re.sub(r"<metadata.*?</metadata>", "", body, flags=re.S)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w, h, body)
    vbs, w, h, body = _cache[fn]
    sw_, sh_ = w / 2.9528, h / 2.9528
    out.insert(-1, f'<svg x="{x-sw_/2:.3f}" y="{y-sh_/2:.3f}" width="{sw_:.3f}" height="{sh_:.3f}" viewBox="{vbs}">{body}</svg>')


for kind, x, y, lit, nm in comps:
    if kind in ("LED", "TINY"):
        out.insert(-1, f'<circle cx="{x}" cy="{y}" r="{1.1 if kind == "LED" else 0.55}" fill="{lit or "#3a3c46"}"/>')
    elif kind == "VCVBezel":
        img("VCVBezel", x, y)
        out.insert(-1, f'<circle cx="{x}" cy="{y}" r="2.6" fill="{lit or "#2a2b30"}" fill-opacity="{0.95 if lit else 0.6}"/>')
    elif kind == "PJ301M":
        img("PJ301M", x, y)
    else:
        img(kind, x, y, "_bg")
        img(kind, x, y)
for sx in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        out.insert(-1, f'<circle cx="{sx:.2f}" cy="{sy}" r="1.7" fill="#2b2c31"/><path d="M{sx-1:.2f} {sy-1} L{sx+1:.2f} {sy+1}" stroke="#111" stroke-width="0.35"/>')
open(sys.argv[2], "w").write("\n".join(out))
