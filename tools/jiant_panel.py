#!/usr/bin/env python3
"""Panels of the JIANT voices (VOICE, DRUM-X, ANIMAL) in the GENRA look, one colour per module.

Usage:  jiant_panel.py real                    -> res/JiantVoice.svg, res/JiantDrumX.svg, res/JiantAnimal.svg
                                                  (text as paths) + src/jiant/JiantLayout.hpp
        jiant_panel.py preview OUTDIR [RACK]   -> OUTDIR/<module>.png with Rack components and mock display content
                                                  (RACK: a Rack source tree, for res/ComponentLibrary; default ../ext/Rack)

Coordinates are Rack pixels (1 HP = 15 px, height 380 px), so the C++ side uses them as they are.
The panel holds what never changes; the module widgets draw the display, the knob names that follow MODE / LANG,
the values, the pads and the lit selectors.
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
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = 380

BG, PLATE, LINE, TXT, DIM = "#17181d", "#212329", "#34363f", "#d9dbe2", "#8a8d99"
# keep in step with src/jiant/JiantUI.hpp
COLORS = {"Voice": "#ff4f5a", "DrumX": "#7b6cff", "Animal": "#4fe08a"}

# component radii (px)
R_SMALL, R_TRIM, R_JACK, R_BEZEL = 11.3, 8.9, 11.8, 10.6

FONT_DIR = "/usr/share/fonts/opentype/inter/"
FONTS = {600: "Inter-SemiBold.otf", 700: "Inter-Bold.otf", 800: "Inter-ExtraBold.otf"}
_fonts = {}


def font(weight):
    if weight not in _fonts:
        f = TTFont(FONT_DIR + FONTS.get(weight, FONTS[600]))
        _fonts[weight] = (f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm, f["hmtx"])
    return _fonts[weight]


def tint(hex_, f):
    """The module colour mixed onto the plate (f: the colour's share)."""
    a = [int(hex_[i:i + 2], 16) for i in (1, 3, 5)]
    b = [int(PLATE[i:i + 2], 16) for i in (1, 3, 5)]
    return "#" + "".join(f"{round(b[i] + (a[i] - b[i]) * f):02x}" for i in range(3))


class Panel:
    def __init__(self, slug, title, hp, sub):
        self.slug, self.W, self.C = slug, hp * 15, COLORS[slug]
        self.o, self.comp, self.mock = [], [], []
        W = self.W
        self.e(f'<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" '
               f'width="{W * 25.4 / 75:.3f}mm" height="{H * 25.4 / 75:.3f}mm" viewBox="0 0 {W} {H}">')
        self.e(f'<rect width="{W}" height="{H}" fill="{BG}"/>')
        self.text(38, 22.5, title, 15, "#e6e7ec", 800, "start", 3.2)
        self.text(38 + self.width(title, 15, 800, 3.2) + 10, 22, sub, 5.2, DIM, 600, "start", 1.4)
        self.e(f'<rect x="{W - 80}" y="16.4" width="34" height="3.2" rx="1.6" fill="{self.C}"/>')

    # ------------------------------------------------------------------ drawing
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

    # ------------------------------------------------------------------ components
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

    def button(self, name, x, y, label=None, ly=None, light=False):
        self.add("lightbutton" if light else "button", name, x, y)
        if label:
            self.text(x, ly or y + 19.5, label, 4.8)

    def area(self, name, x, y, w, h):
        """A rectangle the module widget draws into."""
        self.add("area", name, x, y, w=w, h=h)

    def leds(self, name, x0, y, dx, labels, ly):
        self.add("leds", name, x0, y, dx=dx, n=len(labels))
        for i, n in enumerate(labels):
            self.e(f'<circle cx="{x0 + i * dx}" cy="{y}" r="4.6" fill="#0d0e11"/>')
            self.text(x0 + i * dx, ly, n, 4.6, DIM)

    def display(self, x, y, w, h):
        self.e(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="4" fill="#07080a" stroke="{LINE}" stroke-width="1.2"/>')
        sh = h - 8
        sw = round(sh * 232 / 150, 1)
        self.area("SCREEN", x + 5, y + 4, sw, sh)
        self.sep(x + sw + 10, y + 8, y + h - 8)
        self.area("INFO", x + sw + 19, y + 4, x + w - (x + sw + 19) - 5, h - 8)

    # ------------------------------------------------------------------ output
    def svg(self):
        return "\n".join(self.o + ["</svg>"])

    def layout(self):
        """Names ending in a number become arrays (E0, E1 .. -> E[n][2])."""
        L = [f"namespace {self.slug.lower()} {{", f"static const float W = {self.W}.f;"]
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
            groups[key].append(v)
        for key in order:
            vs = groups[key]
            fmt = lambda v: "{" + ", ".join(f"{a:.1f}f" for a in v) + "}"
            if len(vs) == 1 and not re.search(r"\d$", next(n for k, n, *_ in self.comp if n.startswith(key))):
                L.append(f"static const float {key}[{len(vs[0])}] = {fmt(vs[0])};")
            else:
                L.append(f"static const float {key.rstrip('_')}[{len(vs)}][{len(vs[0])}] = {{{', '.join(fmt(v) for v in vs)}}};")
        L.append(f"}}  // namespace {self.slug.lower()}")
        return "\n".join(L)


def io_row(p, ins, outs=("L", "R")):
    """Inputs on the left (plain jacks), the mix out on the right (on tiles), labels below."""
    p.plate(14, 316, 236, 44)
    # the GENRA strip (tools/panellib.py look, in px: 1 mm = 2.9528): V/OCT, GATE, VEL on a tray in the module colour
    SL = 27.76
    notes = [x for x in ins if x[1] in ("V/OCT", "GATE", "VEL")]
    rest = [x for x in ins if x not in notes]
    x0 = 22
    p.e(f'<rect x="{x0 + 0.9}" y="{332 - 13.6}" width="{len(notes) * SL - 1.8:.2f}" height="33" rx="3" fill="#141519"/>')
    p.e(f'<rect x="{x0 + 3.5}" y="{332 - 13.6}" width="{len(notes) * SL - 7:.2f}" height="1.6" rx="0.8" fill="{dim(p.C)}"/>')
    for i, (name, lab) in enumerate(notes):
        p.jack(name, x0 + SL * (i + 0.5), 332, lab, 353.5)
    for i, (name, lab) in enumerate(rest):
        p.jack(name, x0 + len(notes) * SL + 12 + 34 * (i + 0.5), 332, lab, 353.5)
    p.plate(256, 316, 120, 44)
    for i, lab in enumerate(outs):
        p.jack(f"OUT_{lab}", 292 + i * 48, 332, lab, 353.5, out=True)


def env_fx(p):
    p.plate(14, 270, 178, 42, True)
    p.title(103, 280, "ENVELOPE", 5.0, 2.0)
    p.plate(198, 270, 178, 42, True)
    p.title(287, 280, "FX", 5.0, 2.0)
    for i, (n, lab) in enumerate([("ATK", "ATK"), ("DEC", "DEC"), ("SUS", "SUS"), ("REL", "REL")]):
        p.knob(n, 14 + 178 / 4 * (i + 0.5), 294, R_TRIM, lab, 309.5, ring=False)
    for i, (n, lab) in enumerate([("DIST", "DIST"), ("CHOR", "CHORUS"), ("DLY", "DELAY"), ("REV", "REVERB")]):
        p.knob(n, 198 + 178 / 4 * (i + 0.5), 294, R_TRIM, lab, 309.5, ring=False)


def engine(p, n):
    """n knobs; their names and values follow MODE / LANG and are drawn by the widget."""
    p.plate(14, 184, 362, 82, True)
    p.title(195, 194.5, "ENGINE", 6.0)
    for k in range(n):
        x = 14 + 362 / n * (k + 0.5)
        p.knob(f"E{k}", x, 211)
        p.area(f"E_TEXT{k}", x - 362 / n / 2, 224.5, 362 / n, 15)
        p.jack(f"E_IN{k}", x, 253.5)


# ============================================================================ ANIMAL (26 HP)
def animal():
    p = Panel("Animal", "ANIMAL", 26, "JIANT FM · CREATURE VOICE")
    p.display(14, 30, 362, 106)
    p.plate(14, 140, 362, 40)
    p.knob("MODE", 36, 155, label="MODE", ly=175.5, v=0)
    p.sep(58, 146, 174)
    p.button("PREV", 78, 154, "◀ PRE")
    p.button("NEXT", 110, 154, "PRE ▶")
    p.button("CALL", 146, 154, "CALL", light=True)
    p.sep(166, 146, 174)
    p.block(172, 143, 140, 34)
    p.text(242, 151, "VOICE", 4.6, p.C, 800, "middle", 1.6)
    p.leds("VOICE", 200, 160.5, 42, ["POLY", "MONO", "LEGATO"], 172.5)
    p.knob("LEVEL", 346, 155, label="LEVEL", ly=175.5, v=0.82)
    engine(p, 7)
    env_fx(p)
    io_row(p, [("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL"), ("CALL_IN", "CALL")])
    return p


# ============================================================================ VOICE (26 HP)
def voice():
    p = Panel("Voice", "VOICE", 26, "JIANT FM · SINGING VOICE")
    p.display(14, 30, 362, 106)
    p.plate(14, 140, 362, 40)
    p.button("PREV", 32, 154, "◀ PRE")
    p.button("NEXT", 62, 154, "PRE ▶")
    p.sep(80, 146, 174)
    p.button("LPREV", 98, 154, "◀ LYR")
    p.button("LNEXT", 128, 154, "LYR ▶")
    p.button("CALL", 160, 154, "CALL", light=True)
    p.sep(178, 146, 174)
    p.block(184, 143, 128, 34)
    p.text(248, 151, "VOICE", 4.6, p.C, 800, "middle", 1.6)
    p.leds("VOICE", 210, 160.5, 38, ["POLY", "MONO", "LEGATO"], 172.5)
    p.knob("LEVEL", 346, 155, label="LEVEL", ly=175.5, v=0.82)
    engine(p, 8)
    env_fx(p)
    io_row(p, [("VOCT", "V/OCT"), ("GATE", "GATE"), ("VEL", "VEL"), ("CALL_IN", "CALL"), ("WORD", "WORD ↺")])
    return p


# ============================================================================ DRUM-X (32 HP)
LANES = ["KICK", "SNARE", "CLAP", "HAT CL", "HAT OP", "TOM", "RIM", "BELL"]
MACROS = ["TUNE", "TONE", "DECAY", "NOISE", "FM ENV", "FOLD", "DRIVE"]


def drumx():
    p = Panel("DrumX", "DRUM-X", 32, "JIANT FM · 8-LANE DRUM SYNTH")
    W = p.W
    p.display(14, 30, W - 28, 98)
    # strip: kit, morph, notes | the sound of the chosen lane
    p.plate(14, 132, W - 28, 40)
    p.button("PREV", 28, 146, "◀ KIT")
    p.button("NEXT", 54, 146, "KIT ▶")
    p.sep(70, 138, 166)
    p.knob("MORPH", 88, 147, label="MORPH", ly=167.5, v=0.5)
    p.jack("MORPH_IN", 116, 147, "CV", 167.5)
    p.sep(133, 138, 166)
    # the GENRA strip: one poly cable with GENRA's 8 drum tracks in GENRA's order (tools/panellib.py look)
    p.e(f'<rect x="139" y="{147 - 13.6}" width="27.8" height="33" rx="3" fill="#141519"/>')
    p.e(f'<rect x="142.5" y="{147 - 13.6}" width="20.8" height="1.6" rx="0.8" fill="{dim(p.C)}"/>')
    p.jack("GENRA_IN", 152.9, 147, "DRUMS", 167.5)
    # General MIDI notes (MIDI-CV)
    p.jack("VOCT", 180, 147, "V/OCT", 167.5)
    p.jack("GATE", 207, 147, "GATE", 167.5)
    p.sep(223, 138, 166)
    p.block(229, 135, W - 243, 34)
    p.area("SOUND_HEAD", 235, 136, W - 255, 10)
    for k, lab in enumerate(["PITCH", "PMOD", "DECAY", "NOISE", "COLOR"]):
        p.knob(f"SND{k}", 257 + k * 41, 154, R_TRIM, lab, 167, ring=False)
    # kit macros: knob and its CV side by side
    p.plate(14, 176, W - 28, 52, True)
    p.title(W / 2, 186.5, "KIT MACROS", 6.0)
    for i, lab in enumerate(MACROS):
        c = 14 + (W - 28) / 7 * (i + 0.5)
        p.knob(f"MACRO{i}", c - 13, 204)
        p.jack(f"MACRO_IN{i}", c + 15, 204)
        p.text(c - 13, 224, lab, 4.8)
    # lanes
    p.plate(14, 232, 380, 128, True)
    for l, n in enumerate(LANES):
        x = 14 + 47.5 * (l + 0.5)
        p.area(f"LANE_NAME{l}", x - 22, 236.5, 44, 10)
        p.area(f"PAD{l}", x - 17, 248, 34, 11)
        p.jack(f"TRIG{l}", x, 271.5)
        p.knob(f"LEVEL{l}", x, 295.5, R_TRIM, ring=False, v=0.8)
        p.jack(f"LANE_OUT{l}", x, 326, "OUT", 351, out=True)
    # master
    p.plate(400, 232, W - 414, 128)
    p.text((400 + W - 14) / 2, 242.5, "MASTER", 4.8, p.C, 800, "middle", 1.6)
    p.jack("ACC", 418, 258, "ACC", 277.5)
    p.knob("DLY", 450, 258, R_TRIM, "DLY", 277.5, ring=False, v=0.2)
    p.knob("REV", 418, 289, R_TRIM, "REV", 305, ring=False, v=0.3)
    p.knob("MASTER", 450, 289, R_TRIM, "LVL", 305, ring=False, v=0.8)
    p.jack("OUT_L", 418, 326, "L", 351, out=True)
    p.jack("OUT_R", 450, 326, "R", 351, out=True)
    return p


PANELS = [voice(), drumx(), animal()]

# ============================================================================ real
if not PREVIEW:
    for p in PANELS:
        open(os.path.join(ROOT, "res", f"Jiant{p.slug}.svg"), "w").write(p.svg())
    hdr = ["// Generated by tools/jiant_panel.py - do not edit", "// Rack pixels (1 HP = 15 px).", "#pragma once",
           "namespace jlayout {"] + [p.layout() for p in PANELS] + ["}  // namespace jlayout", ""]
    open(os.path.join(ROOT, "src", "jiant", "JiantLayout.hpp"), "w").write("\n".join(hdr))
    print("written: res/JiantVoice.svg res/JiantDrumX.svg res/JiantAnimal.svg src/jiant/JiantLayout.hpp")
    sys.exit(0)

# ============================================================================ preview
OUT = sys.argv[2]
CL = os.path.join(sys.argv[3] if len(sys.argv) > 3 else os.path.join(ROOT, "..", "ext", "Rack"), "res", "ComponentLibrary")
SHOTS = os.path.join(OUT, "shots")          # firmware screens (232x150 PNG), optional: voice.png drum.png animal.png
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


def mono(p, x, y, s, size=5.6, color=DIM, anchor="start"):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="DejaVu Sans Mono" font-size="{size}" fill="{color}" '
            f'text-anchor="{anchor}">{s}</text>')


def bold(x, y, s, size, color, anchor="start", sp=0.5):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="Nunito, Inter" font-weight="800" font-size="{size}" fill="{color}" '
            f'text-anchor="{anchor}" letter-spacing="{sp}">{s}</text>')


def chips(x, y, names, active, w, h, color, gap=3, size=5.2):
    s = []
    for i, n in enumerate(names):
        act = i in active
        s.append(f'<rect x="{x:.1f}" y="{y}" width="{w}" height="{h}" rx="2.2" fill="{color if act else "#1a1b21"}"/>')
        s.append(bold(x + w / 2, y + h / 2 + size * 0.36, n, size, "#0a0a0d" if act else DIM, "middle", 0.3))
        x += w + gap
    return "".join(s)


MOCK = {
    "Animal": dict(shot="animal.png", name="CMOS BIRD", sub="· 1/24", info="CMOS · POLY · CALL C5",
                   labels=["TOUCH", "COUPLE", "CAP", "HYST", "RATE", "GROUP", "FM"],
                   values=["71%", "0%", "29%", "+25%", "2.46Hz", "0", "20"]),
    "Voice": dict(shot="voice.png", name="CANTOR ES", sub="· 8/15", info="ES · W1 · POLY",
                  labels=["POS", "THEME", "ARTIC", "SHIFT", "BUZZ", "BREATH", "Q", "LOOP"],
                  values=["AUTO", "ANY", "100%", "0st", "63%", "5%", "57%", "FREE"]),
    "DrumX": dict(shot="drum.png", name="909 CLUB", sub="· KIT 2/9", info="MORPH 50% · SOUND KICK A"),
}

for p in PANELS:
    m = MOCK[p.slug]
    pos = {name: (x, y, kw) for kind, name, x, y, kw in p.comp}
    s = []
    for kind, name, x, y, kw in p.comp:
        rot = (-135 + kw.get("v", 0.5) * 270) if kind in ("knob", "trim") else 0
        if kind in ("input", "output"):
            s.append(comp_svg("PJ301M", x, y))
        elif kind == "knob":
            s.append(comp_svg("RoundSmallBlackKnob_bg", x, y) + comp_svg("RoundSmallBlackKnob", x, y, rot))
        elif kind == "trim":
            s.append(comp_svg("Trimpot_bg", x, y) + comp_svg("Trimpot", x, y, rot))
        elif kind in ("button", "lightbutton"):
            s.append(comp_svg("VCVBezel", x, y))
            if kind == "lightbutton":
                s.append(f'<circle cx="{x}" cy="{y}" r="6.6" fill="{p.C}"/>')
        elif kind == "leds":
            s.append(f'<circle cx="{x}" cy="{y}" r="3.6" fill="{p.C}"/>')
            for i in range(1, kw["n"]):
                s.append(f'<circle cx="{x + i * kw["dx"]}" cy="{y}" r="3.6" fill="#3d3f48"/>')
        elif kind == "area" and name.startswith("E_TEXT"):
            k = int(name[6:])
            cx = x + kw["w"] / 2
            s.append(bold(cx, y + 6.5, m["labels"][k], 5.3, TXT, "middle", 0.4))
            s.append(mono(p, cx, y + 13.5, m["values"][k], 5.0, p.C, "middle"))
        elif kind == "area" and name.startswith("LANE_NAME"):
            l = int(name[9:])
            s.append(bold(x + kw["w"] / 2, y + 7.5, LANES[l], 5.0, p.C if l == 0 else TXT, "middle", 0.3))
        elif kind == "area" and name.startswith("PAD"):
            l = int(name[3:])
            s.append(f'<rect x="{x}" y="{y}" width="{kw["w"]}" height="{kw["h"]}" rx="2.2" '
                     f'fill="{p.C if l in (0, 1, 3) else "#33353e"}" stroke="{"#f2f3f7" if l == 0 else "#0d0e11"}" stroke-width="0.8"/>')
        elif kind == "area" and name == "SOUND_HEAD":
            s.append(bold(x, y + 7, "SOUND · KICK", 4.8, p.C, "start", 1.4))
            s.append(chips(x + kw["w"] - 30, y - 0.5, ["A", "B"], {0}, 14, 10, p.C, 2, 4.6))
        elif kind == "area" and name == "SCREEN":
            shot = os.path.join(SHOTS, m["shot"])
            if os.path.exists(shot):
                b64 = base64.b64encode(open(shot, "rb").read()).decode()
                s.append(f'<image x="{x}" y="{y}" width="{kw["w"]}" height="{kw["h"]}" preserveAspectRatio="none" '
                         f'xlink:href="data:image/png;base64,{b64}"/>')
        elif kind == "area" and name == "INFO":
            ix, iy, iw = x, y, kw["w"]
            s.append(bold(ix, iy + 20, m["name"], 13, "#f2f3f7", "start", 0.6))
            s.append(bold(ix + iw, iy + 20, m["sub"], 6, DIM, "end", 0.8))
            s.append(mono(p, ix, iy + 33, m["info"]))
            if p.slug == "Animal":
                s.append(chips(ix, iy + 41, ["CMOS", "PULSE", "WHSTL", "WING", "GLOTT"], {0}, (iw - 12) / 5, 14, p.C))
                s.append(mono(p, ix, iy + 71, "VOICES 2/8"))
                for i in range(8):
                    s.append(f'<rect x="{ix + 66 + i * 13.5}" y="{iy + 64}" width="10" height="9" rx="1.5" fill="{p.C if i < 2 else "#22232a"}"/>')
                s.append(mono(p, ix, iy + 92, "TOUCH 71%  COUPLE 0%  CAP 29%", 5.2, p.C))
            elif p.slug == "Voice":
                s.append(chips(ix, iy + 41, ["OFF", "ES", "EN", "JA"], {1}, 29, 14, p.C))
                s.append(f'<rect x="{ix + 128}" y="{iy + 41}" width="{iw - 128}" height="14" rx="2.2" fill="#0d0e12" stroke="{LINE}" stroke-width="0.8"/>')
                s.append(mono(p, ix + 128 + (iw - 128) / 2, iy + 50.6, "W1", 5.6, p.C, "middle"))
                s.append(f'<rect x="{ix}" y="{iy + 60}" width="{iw}" height="14" rx="2.2" fill="#0d0e12" stroke="{tint(p.C, 0.5)}" stroke-width="0.8"/>')
                s.append(mono(p, ix + 4, iy + 69.6, "la luna canta", 5.6, TXT))
                s.append(bold(ix, iy + 92, "pero", 9, "#f2f3f7", "start", 0.5))
                s.append(f'<circle cx="{ix + 32}" cy="{iy + 89}" r="2.3" fill="{p.C}"/><circle cx="{ix + 40}" cy="{iy + 89}" r="2.3" fill="#33343c"/>')
                s.append(mono(p, ix + iw, iy + 92, "SYL 1/2", 5.2, DIM, "end"))
            else:
                n = len(LANES)
                cw = (iw - 2.5 * (n - 1)) / n
                s.append(chips(ix, iy + 41, [l.replace("HAT ", "H.") for l in LANES], {0, 1, 3}, cw, 13, p.C, 2.5, 4.6))
                s.append(mono(p, ix, iy + 72, "TUNE 0   TONE 52%   DECAY 64%", 5.2, p.C))
                s.append(mono(p, ix, iy + 86, "DLY 0%   REV 16%   LVL -3dB"))
    for sx in (15, p.W - 30):
        for sy in (0, H - 15):
            s.append(comp_svg("ScrewBlack", sx + 7.5, sy + 7.5))
    svg = p.svg().replace("</svg>", "".join(s) + "</svg>")
    fn = os.path.join(OUT, f"Jiant{p.slug}")
    open(fn + ".svg", "w").write(svg)
    subprocess.run(["rsvg-convert", "-z", "3", fn + ".svg", "-o", fn + ".png"], check=True)
    print(fn + ".png")
