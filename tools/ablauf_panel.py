#!/usr/bin/env python3
"""Panel of ABLAUF, GENRA's song arranger: song list (left), pattern bank with 4 banks of 4 x 4 pads (right).

Usage:  ablauf_panel.py real              -> res/Ablauf.svg (text as paths) + src/ablauf/AblaufLayout.hpp
        ablauf_panel.py preview OUT.png   -> preview with stand-in components and a mock song

Coordinates in mm (30 HP, height 128.5 mm). The C++ side converts with mm2px().
"""
import math
import os
import subprocess
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

HP = 30
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
ACC = "#ffd23d"  # GENRA's former SCENES yellow
TR = [("D", "#7b6cff"), ("B", "#b8f23a"), ("M", "#3dc9ff"), ("P", "#e0779e")]
# song parts (keep in step with SECTION_COLORS in src/ablauf/Ablauf.cpp)
SECS = [("INTRO", "#6f86b8"), ("VERSE", "#9b7bd4"), ("BUILD", "#e0a526"), ("DROP", "#ff5a4f"), ("BREAK", "#3fae9a"),
        ("OUTRO", "#8a8d99")]
OUT_TINT = "#4a4224"

R = {"small": 3.84, "knob": 4.8, "button": 3.6, "jack": 4.0}

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_f = {}
out, comps, areas = [], [], {}


def font(w):
    if w not in _f:
        f = TTFont(FONT_DIR + FONTS[w])
        _f[w] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _f[w]


def e(s):
    out.append(s)


def width(s, size, w=600, sp=0.25):
    gs, cmap, upm, hm = font(w)
    return sum(hm[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp


def text(x, y, s, size=1.8, color=DIM, w=600, anchor="middle", sp=0.25):
    gs, cmap, upm, hm = font(w)
    sc = size / upm
    gl = [cmap.get(ord(c), "space") for c in s]
    wd = sum(hm[g][0] * sc + sp for g in gl) - sp
    cx = x - (wd / 2 if anchor == "middle" else (wd if anchor == "end" else 0))
    ps = []
    for g in gl:
        pen = SVGPathPen(gs)
        gs[g].draw(TransformPen(pen, (sc, 0, 0, -sc, cx, y)))
        if pen.getCommands():
            ps.append(pen.getCommands())
        cx += hm[g][0] * sc + sp
    if ps:
        e(f'<path fill="{color}" d="{" ".join(ps)}"/>')


def rect(x, y, w, h, fill, rx=0.6, extra=""):
    e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" fill="{fill}" {extra}/>')


def section(x, y, w, h, title=None, col=ACC):
    rect(x, y, w, h, PLATE, 1.2)
    rect(x, y, w, 0.88, col, 0.44)
    if title:
        text(x + w / 2, y + 5.0, title, 2.03, col, 800, "middle", 0.75)


def knob(name, x, y, label, kind="small", ring=RING, ly=None):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="none" stroke="{ring}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    if label:
        text(x, ly if ly else y + R[kind] + 3.3, label, 1.7, TXT, 600, "middle", 0.3)


def button(name, x, y, label=None, size=1.4, color=TXT, dy=4.9):
    comps.append(("button", name, x, y))
    if label:
        text(x, y + dy, label, size, color, 600 if color == TXT else 800, "middle", 0.25)


def jack(name, x, y, label, out_=False):
    comps.append(("output" if out_ else "input", name, x, y))
    if out_:
        rect(x - 4.6, y - 4.75, 9.2, 11.4, OUT_TINT, 0.9)
    text(x, y + 5.7, label, 1.55, TXT if out_ else DIM, 600, "middle", 0.25)


def area(name, x, y, w, h):
    areas[name] = (x, y, w, h)


# ---------------------------------------------------------------- frame and header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
rect(0, 0, W, H, BG, 0)
text(12.9, 7.6, "ABLAUF", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("ABLAUF", 5.1, 800, 1.08) + 3.4, 7.45, "GENRA · SONG ARRANGER", 1.76, DIM, 600, "start", 0.47)
text(W - 16.4, 7.45, "LINK", 1.5, DIM, 700, "end", 0.4)
comps.append(("light", "LINKLED", W - 14, 6.9))

# ---------------------------------------------------------------- song (left, compact)
sx, sw = 3, 63
section(sx, 11, sw, 91, "SONG")
dx, dy, dw, dh = sx + 2.5, 18.5, sw - 5, 43.5
rect(dx, dy, dw, dh, DARK, 1.0)
area("SONGLIST", dx, dy, dw, dh)
ky = 70
knob("ZEILE", sx + 8.5, ky, "ROW", "knob", ACC)
knob("PATTERN", sx + 21, ky, "PATTERN")
knob("TAKTE", sx + 32, ky, "BARS")
# ABSCHNITT: the module draws the six coloured lamps around the knob and the name below it
ax = sx + 47
knob("ABSCHNITT", ax, ky, None, "small", "#00000000")
area("SECRING", ax - 7.5, ky - 7.5, 15, 15)
rect(ax - 5.2, ky + 5.4, 10.4, 3.4, DARK, 0.6)
area("SECNAME", ax - 5.2, ky + 5.4, 10.4, 3.4)
text(ax, ky + 11.4, "PART", 1.45, TXT, 600, "middle", 0.3)
ry = 85.5
button("FILL", sx + 6.5, ry)
text(sx + 6.5, ry + 5.0, "FILL", 1.3, TXT, 600, "middle", 0.2)
# MODE: a large two-way switch PATT | SONG (the module draws both halves, the chosen one lit; a click picks a half)
mx0, mw, mh = sx + 12.5, 17.0, 6.6
rect(mx0, ry - mh / 2, mw, mh, DARK, 1.2)
area("MODESW", mx0, ry - mh / 2, mw, mh)
text(mx0 + mw / 2, ry + 5.6, "MODE", 1.2, DIM, 700, "middle", 0.3)
e(f'<line x1="{sx + 30.5}" y1="{ry - 4}" x2="{sx + 30.5}" y2="{ry + 5}" stroke="{LINE}" stroke-width="0.3"/>')
for k, (nm, lab) in enumerate([("PLAY", "PLAY"), ("STOP", "STOP"), ("LOOP", "LOOP")]):
    button(nm, sx + 37 + k * 9.3, ry)
    text(sx + 37 + k * 9.3, ry + 5.0, lab, 1.35, TXT, 600, "middle", 0.25)
by = 96.5
for k, (nm, lab) in enumerate([("INS", "INSERT"), ("DEL", "DELETE"), ("UNDO", "UNDO")]):
    button(nm, sx + 6.5 + k * 9, by)
    text(sx + 6.5 + k * 9, by + 5.0, lab, 1.25, TXT, 600, "middle", 0.2)
e(f'<line x1="{sx + 30.5}" y1="{by - 4}" x2="{sx + 30.5}" y2="{by + 5}" stroke="{LINE}" stroke-width="0.3"/>')
# MAKE SONG: builds a song from the selected pad in the SONG FORM chosen right next to it (lit once a pattern is stored)
button("MAKESONG", sx + 37, by)
text(sx + 42.5, by - 0.3, "MAKE SONG", 1.7, ACC, 800, "start", 0.35)
text(sx + 42.5, by + 2.6, "from the selected pad", 1.15, DIM, 600, "start", 0.15)

# ---------------------------------------------------------------- patterns (right, wide)
bx = sx + sw + 1
bw = W - 3 - bx
bc = bx + bw / 2
section(bx, 11, bw, 91, "PATTERNS")
area("BANKS", bc - 19, 17.4, 38, 4.6)  # four boxes A-D, drawn and clicked by the module
pw_, ph_, gap = 9.0, 9.0, 1.0  # square pads, 8 x 4 per bank
gw = 8 * pw_ + 7 * gap
x0 = bc - gw / 2
y0 = 24.0
area("PADS", x0, y0, gw, 4 * ph_ + 3 * gap)  # 8 columns, 4 rows
areas["PADSTEP"] = (pw_ + gap, ph_ + gap, pw_, ph_)
py = y0 + 4 * (ph_ + gap) + 0.6
rect(x0, py, gw, 5.6, DARK, 0.7)
area("INFO", x0, py, gw, 5.6)
ty = py + 8.4
text(x0, ty + 0.6, "RE-ROLL", 1.3, DIM, 700, "start", 0.3)
area("REROLL", x0 + 14, ty - 1.6, 4 * 4.6 - 1.0, 3.2)
vy = ty + 8.0
for k, (nm, col) in enumerate(SECS):
    x = bc - 32.5 + k * 13
    comps.append(("button", f"PART{k}", x, vy))
    text(x, vy + 5.2, nm, 1.25, col, 800, "middle", 0.2)
sy_ = vy + 11.6
# SONG FORM: which genre's song structure MAKE SONG (just left of it) uses (AUTO = the genre of the pattern)
fx_, fw_ = x0, 44
text(fx_, sy_ - 2.7, "SONG FORM  ·  for MAKE SONG", 1.2, DIM, 700, "start", 0.3)
rect(fx_, sy_ - 1.4, fw_, 5.4, DARK, 0.9)
area("SONGFORM", fx_, sy_ - 1.4, fw_, 5.4)
e(f'<polyline points="{fx_ + fw_ - 4.2:.2f},{sy_ + 0.7:.2f} {fx_ + fw_ - 3:.2f},{sy_ + 2.0:.2f} {fx_ + fw_ - 1.8:.2f},{sy_ + 0.7:.2f}" '
  f'fill="none" stroke="{ACC}" stroke-width="0.4" stroke-linecap="round" stroke-linejoin="round"/>')
e(f'<line x1="{x0 + 47}" y1="{sy_ - 4}" x2="{x0 + 47}" y2="{sy_ + 5}" stroke="{LINE}" stroke-width="0.3"/>')
for k, (nm, lab) in enumerate([("COPY", "COPY"), ("CLEAR", "CLEAR"), ("CLEARALL", "CLR ALL")]):
    button(nm, x0 + 53 + k * 11, sy_)
    text(x0 + 53 + k * 11, sy_ + 5.0, lab, 1.25, TXT, 600, "middle", 0.25)

# ---------------------------------------------------------------- jacks
section(3, 104, W - 6, 21.5, None, RING)
for i, (nm, lab) in enumerate([("IN_RUN", "RUN"), ("IN_RESET", "RESET"), ("IN_NEXT", "NEXT"), ("IN_PATT", "PATT CV"),
                               ("IN_ROW", "ROW CV")]):
    jack(nm, 10 + i * 10.6, 115.2, lab)
text(10 + 2 * 10.6, 108.4, "INPUTS", 1.4, DIM, 700, "middle", 0.4)
e(f'<line x1="63" y1="108" x2="63" y2="122.5" stroke="{LINE}" stroke-width="0.3"/>')
for i, (nm, lab) in enumerate([("OUT_END", "END"), ("OUT_ROW", "ROW"), ("OUT_BUILD", "BUILD"), ("OUT_DROP", "DROP"),
                               ("OUT_POS", "POS"), ("OUT_CVA", "CV A"), ("OUT_CVB", "CV B")]):
    jack(nm, 70 + i * 12.0, 115.2, lab, True)
text(70 + 3 * 12.0, 108.4, "OUTPUTS", 1.4, DIM, 700, "middle", 0.4)
e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "Ablauf.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/ablauf_panel.py - do not edit", "#pragma once", "namespace ablayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, w, h) in areas.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "ablauf"), exist_ok=True)
    open(os.path.join(ROOT, "src", "ablauf", "AblaufLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print("wrote res/Ablauf.svg and src/ablauf/AblaufLayout.hpp")
    sys.exit(0)

# ---------------------------------------------------------------- preview: stand-in components + a mock song


def put(s):
    out.insert(-1, s)


def ptext(*a, **k):
    n = len(out)
    text(*a, **k)
    out.insert(-1, out.pop(n))


SECC = dict(SECS)
LIT = {"PLAY": "#5dff8a", "LOOP": ACC, "FILL": ACC, "STORE": None, "TRACK0": TR[0][1], "TRACK1": TR[1][1],
       "TRACK2": TR[2][1], "TRACK3": TR[3][1]}
for k, (nm, col) in enumerate(SECS):
    LIT[f"PART{k}"] = col
for kind, n, x, y in comps:
    if kind in ("small", "knob"):
        r = R[kind]
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="#141417" stroke="#2c2d33" stroke-width="0.4"/>'
            f'<line x1="{x:.2f}" y1="{y - r * 0.9:.2f}" x2="{x:.2f}" y2="{y - r * 0.35:.2f}" stroke="#eee" '
            f'stroke-width="0.6" stroke-linecap="round"/>')
    elif kind == "button":
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.6" fill="#0d0e11"/><circle cx="{x:.2f}" cy="{y:.2f}" r="2.7" '
            f'fill="{LIT.get(n) or "#4a4b52"}"/>')
    elif kind in ("input", "output"):
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="3.1" fill="#c9cbd1"/><circle cx="{x:.2f}" cy="{y:.2f}" r="2.2" '
            f'fill="#8d9097"/><circle cx="{x:.2f}" cy="{y:.2f}" r="1.45" fill="#050505"/>')
    elif kind == "light":
        put(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="1.2" fill="#5dff8a"/>')
    elif kind == "lamps":
        put(f'<circle cx="{x + 7.5:.2f}" cy="{y}" r="1.35" fill="{ACC}"/>')
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        put(f'<circle cx="{sx_}" cy="{sy}" r="1.6" fill="#3a3b42"/>')
# section ring
cx, cy = ax, ky
for k, (nm, col) in enumerate(SECS):
    ang = math.radians(-135 + k * 54)
    put(f'<circle cx="{cx + 6.6 * math.sin(ang):.2f}" cy="{cy - 6.6 * math.cos(ang):.2f}" r="{0.95 if k == 3 else 0.7}" '
        f'fill="{col}" opacity="{1 if k == 3 else 0.4}"/>')
ptext(ax, ky + 7.95, "DROP", 1.6, SECC["DROP"], 800, "middle", 0.35)
# mock song list
X, Y, WW, HH = areas["SONGLIST"]
for x_, t_, a_ in [(2.2, "#", "start"), (7, "PATTERN", "start"), (36, "BARS", "middle"),
                   (46, "PART", "start")]:
    ptext(X + x_, Y + 3.6, t_, 1.2, DIM, 700, a_, 0.3)
SONG = [("A2", "INTRO 1", 8, [1, 0, 0, 0], "INTRO", False), ("A3", "VERSE 1", 16, [1, 1, 0, 1], "VERSE", True),
        ("A4", "BUILD 1", 8, [1, 1, 1, 1], "BUILD", True), ("A5", "DROP 1", 16, [1, 1, 1, 1], "DROP", False),
        ("A6", "BREAK 1", 8, [0, 0, 1, 1], "BREAK", False), ("A7", "BUILD 2", 8, [1, 1, 1, 1], "BUILD", True),
        ("A8", "DROP 2", 16, [1, 1, 1, 1], "DROP", False), ("A9", "OUTRO 1", 8, [1, 1, 0, 1], "OUTRO", False)]
for i, (pp, nm, bars, m, sec, fill) in enumerate(SONG):
    y = Y + 6.2 + i * 4.3
    if i == 3:
        put(f'<rect x="{X + 1}" y="{y - 0.2:.2f}" width="{WW - 2}" height="4.1" rx="0.5" fill="#2e2a17"/>'
            f'<rect x="{X + 1}" y="{y - 0.2:.2f}" width="0.7" height="4.1" rx="0.3" fill="{ACC}"/>'
            f'<rect x="{X + 1.7}" y="{y + 3.5:.2f}" width="{(WW - 2.7) * 0.4:.2f}" height="0.35" fill="{ACC}"/>')
    c = ACC if i == 3 else TXT
    ptext(X + 2.2, y + 2.8, f"{i + 1:02d}", 1.4, ACC if i == 3 else DIM, 700, "start", 0.2)
    ptext(X + 7, y + 2.8, pp, 1.5, c, 800, "start", 0.25)
    put(f'<rect x="{X + 13.2}" y="{y + 0.7:.2f}" width="1.0" height="2.4" rx="0.3" fill="#ff9d2e"/>')
    ptext(X + 15.2, y + 2.8, nm, 1.5, c, 600, "start", 0.25)
    ptext(X + 36, y + 2.8, str(bars), 1.5, c, 700, "middle", 0.2)
    put(f'<rect x="{X + 45.5}" y="{y + 0.7:.2f}" width="1.0" height="2.4" rx="0.3" fill="{SECC[sec]}"/>')
    ptext(X + 47.4, y + 2.8, sec, 1.35, SECC[sec] if i == 3 else TXT, 700, "start", 0.25)
MX_, MY_, MW_, MH_ = areas["MODESW"]
put(f'<rect x="{MX_ + MW_ / 2 + 0.4:.2f}" y="{MY_ + 0.4:.2f}" width="{MW_ / 2 - 0.8:.2f}" height="{MH_ - 0.8:.2f}" rx="0.9" fill="#ffd84a"/>')
ptext(MX_ + MW_ / 4, MY_ + MH_ / 2 + 0.6, "PATT", 1.55, "#8a8c96", 800, "middle", 0.25)
ptext(MX_ + MW_ * 3 / 4, MY_ + MH_ / 2 + 0.6, "SONG", 1.55, "#141519", 800, "middle", 0.25)
FX_, FY_, FW_, FH_ = areas["SONGFORM"]
ptext(FX_ + 1.6, FY_ + 3.65, "AUTO", 1.7, "#ffd84a", 800, "start", 0.4)
ptext(FX_ + 10.5, FY_ + 3.65, "HOUSE · TECHNO", 1.6, "#e6e7ec", 700, "start", 0.35)
ptext(X + WW - 2.2, Y + HH - 1.2, "BAR 37 / 92  ·  ROW 4 / 9", 1.15, DIM, 700, "end", 0.3)
# banks, pads, info, reroll
X, Y, WW, HH = areas["BANKS"]
for k in range(4):
    on = k == 0
    put(f'<rect x="{X + k * 10:.2f}" y="{Y}" width="8" height="4.6" rx="0.7" fill="{"#3a3520" if on else "#1a1b20"}" '
        f'stroke="{ACC if on else "#2c2d33"}" stroke-width="0.4"/>')
    ptext(X + 4 + k * 10, Y + 3.25, "ABCD"[k], 2.0, ACC if on else DIM, 800, "middle", 0)
X, Y, _, _ = areas["PADS"]


def tint(col, f):
    c = [int(col[k:k + 2], 16) for k in (1, 3, 5)]
    b = [int(PLATE[k:k + 2], 16) for k in (1, 3, 5)]
    return "#" + "".join(f"{round(b[k] + (c[k] - b[k]) * f):02x}" for k in range(3))


TAGS = {1: "INTRO", 2: "VERSE", 3: "BUILD", 4: "DROP", 5: "BREAK", 6: "BUILD", 7: "DROP", 8: "OUTRO", 9: "INTRO",
        10: "DROP", 17: "INTRO", 18: "DROP"}
NAMES = {0: "GENRA 1", 1: "INTRO 1", 2: "VERSE 1", 3: "BUILD 1", 4: "DROP 1", 5: "BREAK 1", 6: "BUILD 2", 7: "DROP 2",
         8: "OUTRO 1", 9: "INTRO 2", 10: "DROP 3", 16: "GENRA 2", 17: "INTRO 1", 18: "DROP 1"}
FAM = {n: ("G2" if n >= 16 else "G1") for n in NAMES}
FAMC = {"G1": "#ff9d2e", "G2": "#e8e8ee"}  # family colours: one per GENRA original
USED = set(range(11)) | {16, 17, 18}
stx, sty, pw_, ph_ = areas["PADSTEP"]
for n_ in range(32):
    x, y = X + (n_ % 8) * stx, Y + (n_ // 8) * sty
    used = n_ in USED
    sec = TAGS.get(n_)
    fill = "#1a1b20" if not used else (tint(SECC[sec], 0.5 if n_ == 4 else 0.3) if sec else "#2a2b33")
    st = ' stroke="#ffffff" stroke-width="0.55"' if n_ == 4 else (' stroke="#ffffff" stroke-width="0.4" stroke-dasharray="0.8 0.5"' if n_ == 5 else "")
    put(f'<rect x="{x:.2f}" y="{y:.2f}" width="{pw_}" height="{ph_}" rx="0.8" fill="{fill}"{st}/>')
    if used and n_ in FAM:  # the top bar: the colour of the GENRA original this pad comes from
        put(f'<rect x="{x + 0.6:.2f}" y="{y + 0.6:.2f}" width="{pw_ - 1.2}" height="1.0" rx="0.4" fill="{FAMC[FAM[n_]]}"/>')
    ptext(x + 0.9, y + 3.2, f"A{n_ + 1}", 1.05, TXT if used else "#4a4c57", 700, "start", 0.1)
    if used and NAMES.get(n_):
        ptext(x + 0.9, y + 5.3, NAMES[n_], 1.35, "#ffffff", 800, "start", 0.15)
    for t in range(4):
        on = used and (t != 2 or n_ % 3)
        put(f'<rect x="{x + 0.9 + t * 1.5:.2f}" y="{y + 7.7:.2f}" width="1.05" height="0.6" rx="0.25" fill="{TR[t][1] if on else "#2f3038"}"/>')
X, Y, WW, HH = areas["INFO"]
ptext(X + 1.6, Y + 3.7, "A5  DROP 1", 1.45, TXT, 800, "start", 0.25)
ptext(X + 14.0, Y + 3.7, "DROP", 1.45, SECC["DROP"], 800, "start", 0.25)
ptext(X + WW - 1.6, Y + 3.7, "> A4", 1.45, TXT, 700, "end", 0.25)
X, Y, WW, HH = areas["REROLL"]
for t in range(4):
    on = t != 2
    put(f'<rect x="{X + t * 4.6:.2f}" y="{Y}" width="3.6" height="3.2" rx="0.5" fill="{TR[t][1] if on else "#24252c"}"/>')
    ptext(X + t * 4.6 + 1.8, Y + 2.25, TR[t][0], 1.3, "#07080a" if on else "#4a4c57", 800, "middle", 0)
print("wrote preview (stand-ins only)")
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1500", svg, "-o", sys.argv[2]], check=True)
