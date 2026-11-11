#!/usr/bin/env python3
"""Panel of STOTTER, the performance FX (DJ effect pads) in the GENRA look.

Usage:  stotter_panel.py real              -> res/Stotter.svg (text as paths) + src/stotter/StotterLayout.hpp
        stotter_panel.py preview OUT.png   -> preview with mock components, lit pads and display

Coordinates in mm (30 HP, height 128.5 mm). The C++ side converts with mm2px().
Colours follow the GENRA main module: every section its own palette colour, global sections green,
lit buttons white.
"""
import os
import subprocess
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

HP = 30
W, H = HP * 5.08, 128.5
BG, PLATE, LINE, TXT, DIM, DARK = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99", "#07080a"
TITLE, RING = "#e6e7ec", "#4a4c57"
GREEN, ORANGE, RED, BLUE, VIOLET, TEAL = "#5dff8a", "#ff8a3d", "#ff4f6d", "#3dc9ff", "#7b6cff", "#2f9e8f"
ROWS = ["#7b6cff", "#b8f23a", BLUE, "#e0779e"]  # pad rows: stutter, time, gate, fx (the four GENRA track colours)

R = {"small": 3.84, "knob": 4.8, "large": 6.1, "jack": 4.0, "button": 3.6, "sbutton": 2.6}

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_fonts = {}
out, comps, areas = [], [], {}


def font(weight):
    if weight not in _fonts:
        f = TTFont(FONT_DIR + FONTS[weight])
        _fonts[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _fonts[weight]


def e(s):
    out.append(s)


def text(x, y, s, size=1.7, color=TXT, weight=600, anchor="middle", sp=0.3):
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


def mixc(col, f):
    c = [int(col[i:i + 2], 16) for i in (1, 3, 5)]
    p = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(p[i] + (c[i] - p[i]) * f):02x}" for i in range(3))


def section(x, y, w, h, title, cols):
    e(f'<rect x="{x}" y="{y}" width="{w:.2f}" height="{h}" rx="1.2" fill="{PLATE}"/>')
    n = len(cols)
    for i, c in enumerate(cols):
        e(f'<rect x="{x + i * w / n:.2f}" y="{y}" width="{w / n:.2f}" height="0.88" rx="0.44" fill="{c}"/>')
    if title:
        text(x + 2.2, y + 4.6, title, 1.9, cols[0] if n == 1 and cols[0] != RING else TXT, 800, "start", 0.7)


def knob(name, x, y, label, kind="knob", ring=None, side=None):
    comps.append((kind, name, x, y))
    r = R[kind] + 0.88
    e(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r:.2f}" fill="none" stroke="{ring or RING}" stroke-width="0.29" '
      f'stroke-dasharray="0.29 0.73" stroke-linecap="round"/>')
    if side:
        text(x - r - 1.0, y + 0.6, label, 1.6, TXT, 600, "end", 0.3)
    elif label:
        text(x, y + R[kind] + 3.3, label, 1.6, TXT, 600, "middle", 0.3)


def button(name, x, y, label, below=True, kind="button"):
    comps.append((kind, name, x, y))
    if label:
        text(x, y + R[kind] + 2.9 if below else y - R["button"] - 1.2, label, 1.5, TXT, 600, "middle", 0.25)


def jack(name, x, y, label, out_=False, tint=None):
    comps.append(("output" if out_ else "input", name, x, y))
    if tint:
        e(f'<rect x="{x - 4.6:.2f}" y="{y - 4.75}" width="9.2" height="11.4" rx="0.9" fill="{tint}"/>')
    text(x, y + 5.7, label, 1.5, TXT, 600, "middle", 0.25)


# ---------------------------------------------------------------- frame and header
e(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W:.2f}mm" height="{H}mm" viewBox="0 0 {W:.2f} {H}">')
e(f'<rect width="{W:.2f}" height="{H}" fill="{BG}"/>')
text(12.9, 7.6, "STOTTER", 5.1, TITLE, 800, "start", 1.08)
text(12.9 + width("STOTTER", 5.1, 800, 1.08) + 3.0, 7.45, "GENRA · PERFORMANCE FX", 1.76, DIM, 600, "start", 0.47)
for i, c in enumerate([GREEN] + ROWS):
    e(f'<rect x="{W - 55 + i * 7.5:.2f}" y="5.55" width="6.0" height="1.08" rx="0.54" fill="{c}"/>')

# ---------------------------------------------------------------- display (pattern of the running pad, edit page)
e(f'<rect x="3" y="11" width="{W - 6:.2f}" height="16" rx="1.6" fill="{DARK}" stroke="{ORANGE}" '
  f'stroke-opacity="0.45" stroke-width="0.3"/>')
areas["DISPLAY"] = (3.0, 11.0, W - 6, 16.0)

# ---------------------------------------------------------------- edit and tempo strip
section(3, 29, W - 6, 13, None, [RING])
areas["EDITBAR"] = (3.0, 29.0, W - 6, 0.88)   # the module recolours it with the row of the edited pad
button("EDIT", 10, 35.6, None)
text(10, 41.0, "EDIT", 1.5, TXT, 700, "middle", 0.3)
button("PREV", 20, 35.6, None)
button("NEXT", 28.5, 35.6, None)
text(24.25, 41.0, "<  FX  >", 1.5, TXT, 700, "middle", 0.3)
knob("P1", 56, 35.6, "P1", "small", side=True)
knob("P2", 76, 35.6, "P2", "small", side=True)
knob("P3", 96, 35.6, "P3", "small", side=True)
button("INIT", 108, 35.6, None)
text(108, 41.0, "INIT", 1.5, TXT, 700, "middle", 0.3)
e(f'<line x1="117" y1="31.5" x2="117" y2="39.8" stroke="{LINE}" stroke-width="0.3"/>')
button("REC", 128, 35.6, None)
text(128, 41.0, "REC 1 BAR", 1.5, TXT, 700, "middle", 0.25)
button("TAP", 142, 35.6, None)
text(142, 41.0, "TAP", 1.5, TXT, 700, "middle", 0.3)

# ---------------------------------------------------------------- EQ (channel strip, isolator kills)
section(3, 44, 27, 60, "EQ", [GREEN])
knob("GAIN", 10, 54, "GAIN", "small")
button("PRE", 23, 54, "PRE FX")
knob("HIGH", 10, 68, "HIGH")
knob("MID", 23, 68, "MID")
knob("LOW", 10, 83, "LOW")
knob("FREQ", 23, 83, "FREQ", "small")
button("KILL_LO", 8.5, 96.5, None, kind="sbutton")
button("KILL_MID", 16.5, 96.5, None, kind="sbutton")
button("KILL_HI", 24.5, 96.5, None, kind="sbutton")
for x_, l_ in ((8.5, "LO"), (16.5, "MID"), (24.5, "HI")):
    text(x_, 101.8, l_, 1.4, RED, 800, "middle", 0.3)
text(16.5, 92.4, "KILL", 1.4, RED, 800, "middle", 0.4)

# ---------------------------------------------------------------- DJ filter
section(32, 44, 27, 60, "FILTER", [ORANGE])
knob("FILTER", 45.5, 58.5, None, "large", ORANGE)
text(36.0, 66.5, "LP", 1.5, DIM, 700, "middle", 0.3)
text(55.0, 66.5, "HP", 1.5, DIM, 700, "middle", 0.3)
text(45.5, 68.4, "CUT", 1.6, TXT, 600, "middle", 0.3)
knob("RESO", 38.5, 78, "RESO", "small")
knob("DRIVE", 52.5, 78, "DRIVE", "small")
knob("INTENSITY", 45.5, 92, "INTENSITY", "knob", ORANGE)

# ---------------------------------------------------------------- 16 pads
section(61, 44, 56, 60, None, ROWS)
text(63.2, 48.6, "PADS", 1.9, TXT, 800, "start", 0.7)
text(115.0, 48.6, "STUTTER · TIME · GATE · FX", 1.35, DIM, 700, "end", 0.3)
PAD = 11.0
for r in range(4):
    for c in range(4):
        n = r * 4 + 1 + c
        x, y = 63.25 + c * 13.5, 50.5 + r * 13.3
        areas[f"PAD{n}"] = (x, y, PAD, PAD)
        e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{PAD}" height="{PAD}" rx="1.2" fill="{mixc(ROWS[r], 0.22)}" '
          f'stroke="{ROWS[r]}" stroke-opacity="0.7" stroke-width="0.25"/>')

# ---------------------------------------------------------------- perform
section(119, 44, W - 3 - 119, 60, "PERFORM", [GREEN])
button("M_LOOP", 125.8, 56, "LOOP")
button("M_ONE", 134.2, 56, "1-SHOT")
button("M_HOLD", 142.6, 56, "HOLD")
knob("QUANT", 127.5, 70.5, "QUANT", "small")
knob("MIX", 141.0, 70.5, "MIX", "small")
button("LATCH", 127.5, 84, "LATCH")
button("KILL", 141.0, 84, "KILL")
text(134.2, 93.6, "BANK", 1.4, DIM, 800, "middle", 0.4)
for i, b in enumerate("ABCD"):
    button(f"BANK_{b}", 124.6 + i * 6.4, 98.5, None, kind="sbutton")

# ---------------------------------------------------------------- jacks
section(3, 106, W - 6, 19.5, None, [RING])
text(5.5, 109.7, "IN", 1.4, DIM, 800, "start", 0.4)
for i, (n, l) in enumerate([("IN_L", "IN L"), ("IN_R", "IN R"), ("CLOCK", "CLOCK"), ("RESET", "RESET"),
                            ("PADS", "PADS"), ("FILT_CV", "FILT CV"), ("INT_CV", "INT CV"), ("KILLS", "KILLS")]):
    jack(n, 11 + i * 13, 115.5, l)
e(f'<line x1="117" y1="109" x2="117" y2="123" stroke="{LINE}" stroke-width="0.3"/>')
text(122.0, 109.7, "OUT", 1.4, TEAL, 800, "start", 0.4)
jack("OUT_L", 128, 115.5, "OUT L", True, mixc(TEAL, 0.35))
jack("OUT_R", 142, 115.5, "OUT R", True, mixc(TEAL, 0.35))

e("</svg>")

if not PREVIEW:
    open(os.path.join(ROOT, "res", "Stotter.svg"), "w").write("\n".join(out))
    lines = ["// Generated by tools/panels/stotter_panel.py - do not edit", "#pragma once", "namespace stlayout {"]
    for kind, n, x, y in comps:
        lines.append(f"static const float {n}[2] = {{{x:.3f}f, {y:.3f}f}};")
    for n, (x, y, w, h) in areas.items():
        lines.append(f"static const float {n}[4] = {{{x:.3f}f, {y:.3f}f, {w:.3f}f, {h:.3f}f}};")
    lines.append(f"static const int HP = {HP};")
    lines.append("}")
    os.makedirs(os.path.join(ROOT, "src", "stotter"), exist_ok=True)
    open(os.path.join(ROOT, "src", "stotter", "StotterLayout.hpp"), "w").write("\n".join(lines) + "\n")
    print(f"wrote res/Stotter.svg and src/stotter/StotterLayout.hpp "
          f"({sum(k not in ('input', 'output') for k, *_ in comps)} controls, "
          f"{sum(k == 'input' for k, *_ in comps)} inputs, {sum(k == 'output' for k, *_ in comps)} outputs)")
    sys.exit(0)

# ---------------------------------------------------------------- preview: mock components, lit pads, display
out.pop()  # reopen the svg, everything below draws on top
NAMES = ["1/4", "1/8", "1/16", "1/32", "HALF", "REVERSE", "TAPE", "SCRATCH",
         "1/8", "1/16", "TRANCE", "PUMP", "ECHO", "FREEZE", "CRUSH", "REARR"]
CATS = ["STUTTER", "TIME", "GATE", "FX"]
LIT = {"M_LOOP", "BANK_A", "PRE", "KILL_LO"}
for kind, n, x, y in comps:
    if kind in ("small", "knob", "large"):
        r = R[kind]
        e(f'<circle cx="{x}" cy="{y}" r="{r}" fill="#0d0e11" stroke="#3b3e45" stroke-width="0.35"/>')
        e(f'<line x1="{x}" y1="{y}" x2="{x + r * 0.55:.2f}" y2="{y - r * 0.75:.2f}" stroke="{TXT}" '
          f'stroke-width="0.55" stroke-linecap="round"/>')
    elif kind in ("button", "sbutton"):
        lit = n in LIT
        col = RED if n.startswith("KILL") and lit else (TXT if lit else "#2a2c33")
        e(f'<circle cx="{x}" cy="{y}" r="{R[kind]}" fill="#111215" stroke="#3b3e45" stroke-width="0.3"/>')
        e(f'<circle cx="{x}" cy="{y}" r="{R[kind] * 0.64:.2f}" fill="{col}"/>')
    else:
        e(f'<circle cx="{x}" cy="{y}" r="3.1" fill="#9a9da8"/><circle cx="{x}" cy="{y}" r="1.7" fill="#07080a"/>')
for i in range(16):
    x, y, w, h = areas[f"PAD{i + 1}"]
    on = i == 2
    if on:
        e(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w}" height="{h}" rx="1.2" fill="{ROWS[i // 4]}"/>')
        e(f'<rect x="{x + 1:.2f}" y="{y + h - 1.8:.2f}" width="{(w - 2) * 0.4:.2f}" height="0.6" rx="0.3" fill="{DARK}"/>')
    text(x + w / 2, y + 4.2, CATS[i // 4], 1.15, DARK if on else ROWS[i // 4], 800, "middle", 0.2)
    text(x + w / 2, y + 7.6, NAMES[i], 1.6 if len(NAMES[i]) < 6 else 1.3, DARK if on else TXT, 800, "middle", 0.2)
for i, b in enumerate("ABCD"):
    text(124.6 + i * 6.4, 99.1, b, 1.5, DARK if b == "A" else DIM, 800, "middle", 0)
dx, dy, dw, dh = areas["DISPLAY"]
x0, x1, y0, y1 = dx + 4, dx + dw - 4, dy + 6.5, dy + 14.5
for q in range(5):
    xq = x0 + (x1 - x0) * q / 4
    e(f'<line x1="{xq:.2f}" y1="{y0 - 0.5}" x2="{xq:.2f}" y2="{y1 + 0.5}" stroke="#212329" stroke-width="0.3"/>')
pts = " ".join(f"M{x0 + (x1 - x0) * k / 16:.2f} {y1:.2f} L{x0 + (x1 - x0) * (k + 1) / 16:.2f} {y0:.2f}" for k in range(16))
e(f'<path d="{pts}" fill="none" stroke="{ORANGE}" stroke-width="0.35"/>')
px = x0 + (x1 - x0) * 0.4
e(f'<line x1="{px:.2f}" y1="{y0 - 1}" x2="{px:.2f}" y2="{y1 + 1}" stroke="{TXT}" stroke-width="0.35"/>')
text(dx + 4, dy + 4.3, "STUTTER 1/16", 1.9, ORANGE, 800, "start", 0.4)
text(dx + dw - 4, dy + 4.3, "LOOP  ·  QUANT 1/4  ·  124 BPM  ·  EQ PRE", 1.6, DIM, 700, "end", 0.3)
for sx_ in (7.62, W - 7.62):
    for sy in (2.54, H - 2.54):
        e(f'<circle cx="{sx_}" cy="{sy}" r="1.5" fill="#0d0e11" stroke="#3b3e45" stroke-width="0.3"/>')
e("</svg>")
svg = sys.argv[2].rsplit(".", 1)[0] + ".svg"
open(svg, "w").write("\n".join(out))
subprocess.run(["rsvg-convert", "-w", "1400", svg, "-o", sys.argv[2]], check=True)
print("wrote", sys.argv[2])
