#!/usr/bin/env python3
"""Panel of MONK (MONK-1's formant voice) in the GENRA look, like the JIANT voices (tools/panels/jiant_panel.py).

Usage:  monk_panel.py real                     -> instruments/res/Monk.svg (text as paths) + instruments/src/monk/MonkLayout.hpp
        monk_panel.py preview OUTDIR [RACK]    -> OUTDIR/Monk.png with Rack components and mock display content
                                                  (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack;
                                                  OUTDIR/shots/monk.png, a 240x240 singer, goes into the screen if present)

Coordinates are Rack pixels (1 HP = 15 px, height 380 px), so the C++ side uses them as they are.
The panel holds what never changes; the module widget draws the singer, the info, the knob values and the lamps.
"""
import base64
import os
import re
import subprocess
import sys

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from panellib import dim  # noqa: E402

MODE = sys.argv[1] if len(sys.argv) > 1 else "real"
PREVIEW = MODE == "preview"
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
H = 380

BG, PLATE, LINE, TXT, DIM = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99"
COLOR = "#ffb43d"                     # saffron; keep in step with instruments/src/monk/Monk.cpp (MONK_RGB)
R_SMALL, R_TRIM = 11.3, 8.9

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_fonts = {}


def font(weight):
    if weight not in _fonts:
        f = TTFont(FONT_DIR + FONTS.get(weight, FONTS[600]))
        _fonts[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _fonts[weight]


def tint(hex_, f):
    a = [int(hex_[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(b[i] + (a[i] - b[i]) * f):02x}" for i in range(3))


class Panel:
    def __init__(self, hp):
        self.W, self.C = hp * 15, COLOR
        self.o, self.comp = [], []
        W = self.W
        self.e(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
               f'width="{W * 25.4 / 75:.3f}mm" height="{H * 25.4 / 75:.3f}mm" viewBox="0 0 {W} {H}">')
        self.e(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

    def e(self, s):
        self.o.append(s)

    def width(self, s, size, weight, sp):
        gs, cmap, upm, hmtx = font(weight)
        return sum(hmtx[cmap.get(ord(c), "space")][0] * size / upm + sp for c in s) - sp

    def text(self, x, y, s, size=5.0, color=TXT, weight=600, anchor="middle", sp=0.6):
        gs, cmap, upm, hmtx = font(weight)
        scale = size / upm
        w = self.width(s, size, weight, sp)
        cx = x - (w / 2 if anchor == "middle" else (w if anchor == "end" else 0))
        paths = []
        for c in s:
            g = cmap.get(ord(c), "space")
            pen = SVGPathPen(gs)
            gs[g].draw(TransformPen(pen, (scale, 0, 0, -scale, cx, y)))
            if pen.getCommands():
                paths.append(pen.getCommands())
            cx += hmtx[g][0] * scale + sp
        if paths:
            self.e(f'<path fill="{color}" d="{" ".join(paths)}"/>')

    def plate(self, x, y, w, h, bar=False):
        self.e(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3.5" fill="{PLATE}"/>')
        if bar:
            self.e(f'<rect x="{x}" y="{y}" width="{w}" height="2.6" rx="1.3" fill="{self.C}"/>')

    def title(self, x, y, s, size=6.0, sp=2.2):
        self.text(x, y, s, size, self.C, 800, "middle", sp)

    def sep(self, x, y0, y1):
        self.e(f'<line x1="{x}" y1="{y0}" x2="{x}" y2="{y1}" stroke="{LINE}" stroke-width="0.8"/>')

    def block(self, x, y, w, h):
        self.e(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="3" fill="{tint(self.C, 0.13)}"/>')

    def add(self, kind, name, x, y, **kw):
        self.comp.append((kind, name, x, y, kw))

    def knob(self, name, x, y, r=R_SMALL, label=None, ly=None, ring=True, v=0.5):
        self.add("trim" if r == R_TRIM else "knob", name, x, y, v=v)
        if ring:
            self.e(f'<circle cx="{x}" cy="{y}" r="{r + 2.6}" fill="none" stroke="#4a4c57" stroke-width="0.85" '
                   f'stroke-dasharray="0.85 2.15" stroke-linecap="round"/>')
        if label:
            self.text(x, ly, label, 5.0 if r > R_TRIM else 4.7)

    def jack(self, name, x, y, label=None, ly=None, out=False):
        self.add("output" if out else "input", name, x, y)
        if out:
            self.e(f'<rect x="{x - 12.5}" y="{y - 14}" width="25" height="{(ly or y + 21) - y + 19}" rx="2.6" fill="{tint(self.C, 0.32)}"/>')
        if label:
            self.text(x, ly or y + 21, label, 4.8, TXT)

    def button(self, name, x, y, label=None, ly=None):
        self.add("button", name, x, y)
        if label:
            self.text(x, ly or y + 19.5, label, 4.8)

    def area(self, name, x, y, w, h):
        self.add("area", name, x, y, w=w, h=h)

    def leds(self, name, x0, y, dx, labels, ly):
        self.add("leds", name, x0, y, dx=dx, n=len(labels))
        for i, n in enumerate(labels):
            self.e(f'<circle cx="{x0 + i * dx}" cy="{y}" r="4.6" fill="#0d0e11"/>')
            self.text(x0 + i * dx, ly, n, 4.6, DIM)

    def svg(self):
        return "\n".join(self.o + ["</svg>"])

    def layout(self):
        """Names ending in a number become arrays (K0, K1 .. -> K[n][2])."""
        L = ["namespace monk {", f"static const float W = {self.W}.f;"]
        groups, order = {}, []
        for kind, name, x, y, kw in self.comp:
            if kind == "area":
                v = [x, y, kw["w"], kw["h"]]
            elif kind == "leds":
                v = [x, y, kw["dx"]]
            else:
                v = [x, y]
            m = re.match(r"(.*?[A-Z_])(\d+)$", name)
            key = m.group(1) if m else name
            if key not in groups:
                groups[key] = []
                order.append(key)
            groups[key].append((v, bool(m)))
        fmt = lambda v: "{" + ", ".join(f"{a:.1f}f" for a in v) + "}"
        for key in order:
            vs = groups[key]
            if len(vs) == 1 and not vs[0][1]:
                L.append(f"static const float {key}[{len(vs[0][0])}] = {fmt(vs[0][0])};")
            else:
                L.append(f"static const float {key.rstrip('_')}[{len(vs)}][{len(vs[0][0])}] = "
                         f"{{{', '.join(fmt(v) for v, _ in vs)}}};")
        L.append("}  // namespace monk")
        return "\n".join(L)


# ============================================================================ MONK (26 HP)
SING = ["VOWEL", "HEAD", "BREATH", "GLIDE", "VOICES", "DETUNE", "SPREAD", "TUNE"]
VOWELS = ["OO", "OH", "AH", "EH", "EE"]

p = Panel(26)
W = p.W
p.text(38, 22.5, "MONK", 15, "#e6e7ec", 800, "start", 3.2)
p.text(38 + p.width("MONK", 15, 800, 3.2) + 10, 22, "MONK-1 · FORMANT VOICE", 5.2, DIM, 600, "start", 1.4)
p.e(f'<rect x="{W - 80}" y="16.4" width="34" height="3.2" rx="1.6" fill="{p.C}"/>')

# display: the singer (square, as on the FM-1) | the info
p.e(f'<rect x="14" y="30" width="362" height="106" rx="4" fill="#07080a" stroke="{LINE}" stroke-width="1.2"/>')
p.area("SCREEN", 18, 34, 98, 98)
p.sep(125, 38, 128)
p.area("INFO", 134, 34, 237, 98)

# strip: singer | pressure routing | touch | level
p.plate(14, 140, 362, 40)
p.button("PREV", 32, 154, "◀ SING")
p.button("NEXT", 64, 154, "SING ▶")
p.sep(83, 146, 174)
p.block(89, 143, 152, 34)
p.text(165, 151, "PRESS →", 4.6, p.C, 800, "middle", 1.6)
p.leds("ROUTE", 108, 160.5, 38, ["VOWEL", "PITCH", "BOTH", "INV"], 172.5)
p.sep(247, 146, 174)
p.knob("PDEPTH", 265, 155, R_TRIM, "P.DEPTH", 175.5, ring=False, v=0.5)
p.knob("VELSENS", 300, 155, R_TRIM, "VEL", 175.5, ring=False, v=0.5)
p.knob("LEVEL", 346, 155, label="LEVEL", ly=175.5, v=1.0)

# the voice: 8 knobs, each with its value and its CV
p.plate(14, 184, 362, 82, True)
p.title(195, 194.5, "SING · CHOIR", 6.0)
for k, n in enumerate(SING):
    x = 14 + 362 / 8 * (k + 0.5)
    p.knob(f"K{k}", x, 211)
    p.area(f"K_TEXT{k}", x - 362 / 16, 224.5, 362 / 8, 15)
    p.jack(f"K_IN{k}", x, 253.5)

# envelope | vibrato and echo
p.plate(14, 270, 178, 42, True)
p.title(103, 280, "ENVELOPE", 5.0, 2.0)
for i, lab in enumerate(["ATK", "DEC", "SUS", "REL"]):
    p.knob(lab, 14 + 178 / 4 * (i + 0.5), 294, R_TRIM, lab, 309.5, ring=False, v=1.0 if lab == "SUS" else 0.0)
p.plate(198, 270, 178, 42, True)
p.title(287, 280, "VIBRATO · ECHO", 5.0, 2.0)
for i, (n, lab) in enumerate([("VIBD", "DEPTH"), ("VIBR", "RATE"), ("ECHO", "ECHO"), ("TIME", "TIME"), ("FB", "FDBK")]):
    p.knob(n, 198 + 178 / 5 * (i + 0.5), 294, R_TRIM, lab, 309.5, ring=False, v=[0, 0.5, 0.8, 0.5, 0.5][i])

# in | out
p.plate(14, 316, 200, 44)
# the GENRA strip (tools/panels/panellib.py look, in px: 1 mm = 2.9528): V/OCT, GATE, VEL on a tray in the module colour
SL = 27.76
p.e(f'<rect x="{22 + 0.9}" y="{332 - 13.6}" width="{3 * SL - 1.8:.2f}" height="33" rx="3" fill="#141519"/>')
p.e(f'<rect x="{22 + 3.5}" y="{332 - 13.6}" width="{3 * SL - 7:.2f}" height="1.6" rx="0.8" fill="{dim(p.C)}"/>')
for i, (n, lab) in enumerate([("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL")]):
    p.jack(n, 22 + SL * (i + 0.5), 332, lab, 353.5)
for i, (n, lab) in enumerate([("PRESS", "PRESS"), ("SINGER", "SINGER")]):
    p.jack(n, 22 + 3 * SL + 10 + 36 * (i + 0.5), 332, lab, 353.5)
p.plate(220, 316, 156, 44)
for i, lab in enumerate(["ENV", "DRY", "L", "R"]):
    p.jack(f"OUT_{lab}", 220 + 39 * (i + 0.5), 332, lab, 353.5, out=True)

# ============================================================================ real
if not PREVIEW:
    open(os.path.join(ROOT, "instruments", "res", "Monk.svg"), "w").write(p.svg())
    os.makedirs(os.path.join(ROOT, "instruments", "src", "monk"), exist_ok=True)
    hdr = ["// Generated by tools/panels/monk_panel.py - do not edit", "// Rack pixels (1 HP = 15 px).", "#pragma once",
           p.layout(), ""]
    open(os.path.join(ROOT, "instruments", "src", "monk", "MonkLayout.hpp"), "w").write("\n".join(hdr))
    print("written: instruments/res/Monk.svg instruments/src/monk/MonkLayout.hpp")
    sys.exit(0)

# ============================================================================ preview
OUT = sys.argv[2]
CL = os.path.join(sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack"), "res", "ComponentLibrary")
_cache = {}


def comp_svg(name, x, y, rot=0.0):
    fn = f"{CL}/{name}.svg"
    if fn not in _cache:
        src = open(fn).read()
        src = re.sub(r"<\?xml[^>]*\?>", "", src)
        src = re.sub(r"<!DOCTYPE[^>]*>", "", src)
        m = re.search(r"<svg[^>]*>", src)
        tag = m.group(0)
        w = float(re.search(r'\swidth="([0-9.]+)', tag).group(1))
        h = float(re.search(r'\sheight="([0-9.]+)', tag).group(1))
        if "mm" in re.search(r'\swidth="([^"]+)"', tag).group(1):
            w, h = w * 75 / 25.4, h * 75 / 25.4
        vb = re.search(r'viewBox="([^"]+)"', tag)
        body = src[m.end():src.rindex("</svg>")]
        body = re.sub(r"<sodipodi:namedview.*?(/>|</sodipodi:namedview>)", "", body, flags=re.S)
        body = re.sub(r"<metadata.*?</metadata>", "", body, flags=re.S)
        body = re.sub(r'\s(inkscape|sodipodi):[a-zA-Z-]+="[^"]*"', "", body)
        _cache[fn] = (vb.group(1) if vb else f"0 0 {w} {h}", w, h, body)
    vbs, w, h, body = _cache[fn]
    g = f'<svg x="{x - w / 2:.2f}" y="{y - h / 2:.2f}" width="{w:.2f}" height="{h:.2f}" viewBox="{vbs}">{body}</svg>'
    if rot:
        g = f'<g transform="rotate({rot:.1f} {x} {y})">{g}</g>'
    return g


def mono(x, y, s, size=5.6, color=DIM, anchor="start"):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="DejaVu Sans Mono" font-size="{size}" fill="{color}" '
            f'text-anchor="{anchor}">{s}</text>')


def bold(x, y, s, size, color, anchor="start", sp=0.5):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="Nunito, Inter" font-weight="800" font-size="{size}" fill="{color}" '
            f'text-anchor="{anchor}" letter-spacing="{sp}">{s}</text>')


def chips(x, y, names, active, w, h, gap=3, size=5.2):
    s = []
    for i, n in enumerate(names):
        act = i in active
        s.append(f'<rect x="{x:.1f}" y="{y}" width="{w}" height="{h}" rx="2.2" fill="{COLOR if act else "#1a1b21"}"/>')
        s.append(bold(x + w / 2, y + h / 2 + size * 0.36, n, size, "#0a0a0d" if act else DIM, "middle", 0.3))
        x += w + gap
    return "".join(s)


VALUES = ["AH", "15cm", "50%", "50%", "1", "0c", "0%", "0st"]
s = []
for kind, name, x, y, kw in p.comp:
    rot = (-135 + kw.get("v", 0.5) * 270) if kind in ("knob", "trim") else 0
    if kind in ("input", "output"):
        s.append(comp_svg("PJ301M", x, y))
    elif kind == "knob":
        s.append(comp_svg("RoundSmallBlackKnob_bg", x, y) + comp_svg("RoundSmallBlackKnob", x, y, rot))
    elif kind == "trim":
        s.append(comp_svg("Trimpot_bg", x, y) + comp_svg("Trimpot", x, y, rot))
    elif kind == "button":
        s.append(comp_svg("VCVBezel", x, y))
    elif kind == "leds":
        s.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="{COLOR}"/>')
        for i in range(1, kw["n"]):
            s.append(f'<circle cx="{x + i * kw["dx"]}" cy="{y}" r="3.6" fill="#3d3f48"/>')
    elif kind == "area" and name.startswith("K_TEXT"):
        k = int(name[6:])
        cx = x + kw["w"] / 2
        s.append(bold(cx, y + 6.5, SING[k], 5.3, TXT, "middle", 0.4))
        s.append(mono(cx, y + 13.5, VALUES[k], 5.0, COLOR, "middle"))
    elif kind == "area" and name == "SCREEN":
        shot = os.path.join(OUT, "shots", "monk.png")
        if os.path.exists(shot):
            b64 = base64.b64encode(open(shot, "rb").read()).decode()
            s.append(f'<image x="{x}" y="{y}" width="{kw["w"]}" height="{kw["h"]}" '
                     f'xlink:href="data:image/png;base64,{b64}"/>')
    elif kind == "area" and name == "INFO":
        ix, iy, iw = x, y, kw["w"]
        s.append(bold(ix, iy + 20, "JERRY", 13, "#f2f3f7", "start", 0.6))
        s.append(bold(ix + iw, iy + 20, "· 1/12", 6, DIM, "end", 0.8))
        s.append(mono(ix, iy + 33, "THE MONK · A3 · SINGING"))
        s.append(chips(ix, iy + 41, VOWELS, {2}, (iw - 12) / 5, 14))
        s.append(mono(ix, iy + 71, "PRESS"))
        s.append(f'<rect x="{ix + 30}" y="{iy + 65.5}" width="{iw - 30}" height="6" rx="1.5" fill="#22232a"/>')
        s.append(f'<rect x="{ix + 30}" y="{iy + 65.5}" width="{(iw - 30) * 0.35:.1f}" height="6" rx="1.5" fill="{COLOR}"/>')
        s.append(mono(ix, iy + 86, "ENV  "))
        s.append(f'<rect x="{ix + 30}" y="{iy + 80.5}" width="{iw - 30}" height="6" rx="1.5" fill="#22232a"/>')
        s.append(f'<rect x="{ix + 30}" y="{iy + 80.5}" width="{(iw - 30) * 0.8:.1f}" height="6" rx="1.5" fill="{tint(COLOR, 0.7)}"/>')
for sx in (15, W - 30):
    for sy in (0, H - 15):
        s.append(comp_svg("ScrewBlack", sx + 7.5, sy + 7.5))
svg = p.svg().replace("</svg>", "".join(s) + "</svg>")
fn = os.path.join(OUT, "Monk")
open(fn + ".svg", "w").write(svg)
subprocess.run(["rsvg-convert", "-z", "3", fn + ".svg", "-o", fn + ".png"], check=True)
print(fn + ".png")
