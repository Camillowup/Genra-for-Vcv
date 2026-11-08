#!/usr/bin/env python3
"""MONK's twelve singers, GENRA's own drawings: every character in every pose as a small SVG for the module.

Usage:  monk_singers.py real              -> res/monk/sNN_BLV.svg (NN singer, B blink 0/1, L brows 0..3, V vowel 0..4)
        monk_singers.py sheet [DIR]       -> overview images (sheet_12.png, states_BOLT.png, states_EMBER.png)

The module (src/monk/Monk.cpp) parses the files with NanoSVG, which knows no clip paths and no text: the mouth's
inside (tongue, teeth, BOLT's LED lines) is therefore cut to the mouth's outline here, as polygons.
"""
import os, subprocess, itertools

import math, sys
OUT = "/mnt/user-data/outputs/monk_saenger"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_uid = itertools.count()

def uid(p="c"):
    return f"{p}{next(_uid)}"

DEFS = """
<radialGradient id="glow" cx="50%" cy="35%" r="70%"><stop offset="0" stop-color="#232631"/><stop offset="1" stop-color="#07080a"/></radialGradient>
<radialGradient id="cellglow" cx="50%" cy="45%" r="55%"><stop offset="0" stop-color="#ffffff" stop-opacity="0.05"/><stop offset="1" stop-color="#ffffff" stop-opacity="0"/></radialGradient>
<linearGradient id="robo" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#dfe5ee"/><stop offset="1" stop-color="#7d879a"/></linearGradient>
<linearGradient id="robobody" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6b7488"/><stop offset="1" stop-color="#454c5c"/></linearGradient>
<linearGradient id="frog" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#a6e463"/><stop offset="1" stop-color="#4f9a2a"/></linearGradient>
<linearGradient id="diva" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff7ab6"/><stop offset="1" stop-color="#b83a7a"/></linearGradient>
<linearGradient id="divahair" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#6a2244"/><stop offset="1" stop-color="#3a1026"/></linearGradient>
<linearGradient id="owl" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#c9a072"/><stop offset="1" stop-color="#6e4a2c"/></linearGradient>
<linearGradient id="alien" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#b9a6ff"/><stop offset="1" stop-color="#6a4fd8"/></linearGradient>
<linearGradient id="skull" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fbf6e9"/><stop offset="1" stop-color="#cfc6ae"/></linearGradient>
<linearGradient id="hat" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3c3f4c"/><stop offset="1" stop-color="#1c1e26"/></linearGradient>
<linearGradient id="cactus" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3f9a5c"/><stop offset="0.5" stop-color="#6cc985"/><stop offset="1" stop-color="#3a8a52"/></linearGradient>
<linearGradient id="pot" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#e0855a"/><stop offset="1" stop-color="#a24f2e"/></linearGradient>
<linearGradient id="whale" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5fb4f0"/><stop offset="1" stop-color="#2a63b8"/></linearGradient>
<linearGradient id="tape" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffbe4d"/><stop offset="1" stop-color="#e07a1f"/></linearGradient>
<radialGradient id="moon" cx="40%" cy="35%" r="70%"><stop offset="0" stop-color="#fff6c8"/><stop offset="1" stop-color="#e3c45e"/></radialGradient>
<radialGradient id="moonglow"><stop offset="0.75" stop-color="#ffe9a0" stop-opacity="0.18"/><stop offset="1" stop-color="#ffe9a0" stop-opacity="0"/></radialGradient>
<linearGradient id="cap" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff6a5a"/><stop offset="1" stop-color="#b8282f"/></linearGradient>
<linearGradient id="stem" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff3df"/><stop offset="1" stop-color="#e2cfae"/></linearGradient>
<linearGradient id="dragon" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ff9a4a"/><stop offset="1" stop-color="#d9452b"/></linearGradient>
<linearGradient id="wing" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#b8325a"/><stop offset="1" stop-color="#7a1c40"/></linearGradient>
"""

# ---------------------------------------------------------------- generic parts
VOWEL = {  # width factor, height factor
    "OO": (0.34, 0.62), "OH": (0.52, 0.86), "AH": (0.82, 1.0),
    "EH": (0.92, 0.58), "EE": (1.0, 0.34),
}

def mouth_path(cx, cy, W, H, v):
    fw, fh = VOWEL[v]
    w, h = W * fw / 2, H * fh
    if v in ("OO", "OH"):
        ry = h / 2
        return (f"M{cx-w},{cy} a{w},{ry} 0 1 0 {2*w},0 a{w},{ry} 0 1 0 {-2*w},0 z",
                cy - ry, cy + ry, w)
    t = cy - h * 0.38
    b = t + h
    top_bulge = {"AH": 0.08, "EH": 0.05, "EE": 0.0}[v]
    d = (f"M{cx-w},{t} C{cx-w*0.45},{t-h*top_bulge} {cx+w*0.45},{t-h*top_bulge} {cx+w},{t} "
         f"C{cx+w*1.02},{t+h*0.62} {cx+w*0.6},{b} {cx},{b} "
         f"C{cx-w*0.6},{b} {cx-w*1.02},{t+h*0.62} {cx-w},{t} z")
    return d, t - h * top_bulge * 0.75, b, w

def _bez(p0, p1, p2, p3, n=10):
    out = []
    for i in range(1, n + 1):
        t = i / n
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t * t, t ** 3
        out.append((a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0], a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1]))
    return out


def mouth_poly(cx, cy, W, H, v):
    """The mouth's outline as a polygon (counter-clockwise does not matter: convex)."""
    fw, fh = VOWEL[v]
    w, h = W * fw / 2, H * fh
    if v in ("OO", "OH"):
        ry = h / 2
        return [(cx + w * math.cos(a * math.pi / 24), cy + ry * math.sin(a * math.pi / 24)) for a in range(48)]
    t = cy - h * 0.38
    b = t + h
    tb = {"AH": 0.08, "EH": 0.05, "EE": 0.0}[v]
    pts = [(cx - w, t)]
    pts += _bez((cx - w, t), (cx - w * 0.45, t - h * tb), (cx + w * 0.45, t - h * tb), (cx + w, t))
    pts += _bez((cx + w, t), (cx + w * 1.02, t + h * 0.62), (cx + w * 0.6, b), (cx, b))
    pts += _bez((cx, b), (cx - w * 0.6, b), (cx - w * 1.02, t + h * 0.62), (cx - w, t))[:-1]
    return pts


def _clip(subject, clip):
    """Sutherland-Hodgman: the subject polygon cut to the (convex) clip polygon."""
    def inside(p, a, b, sgn):
        return sgn * ((b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])) >= 0
    area = sum(clip[i][0] * clip[(i + 1) % len(clip)][1] - clip[(i + 1) % len(clip)][0] * clip[i][1] for i in range(len(clip)))
    sgn = 1 if area > 0 else -1
    out = subject
    for i in range(len(clip)):
        a, b = clip[i], clip[(i + 1) % len(clip)]
        inp, out = out, []
        if not inp:
            break
        s0 = inp[-1]
        for e in inp:
            ein, sin_ = inside(e, a, b, sgn), inside(s0, a, b, sgn)
            if ein:
                if not sin_:
                    out.append(_cross(s0, e, a, b))
                out.append(e)
            elif sin_:
                out.append(_cross(s0, e, a, b))
            s0 = e
    return out


def _cross(p, q, a, b):
    x1, y1, x2, y2, x3, y3, x4, y4 = p[0], p[1], q[0], q[1], a[0], a[1], b[0], b[1]
    d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    if abs(d) < 1e-9:
        return q
    t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / d
    return (x1 + t * (x2 - x1), y1 + t * (y2 - y1))


def poly_d(pts):
    return "M" + " L".join(f"{x:.1f},{y:.1f}" for x, y in pts) + "z" if len(pts) > 2 else ""


def ell_poly(cx, cy, rx, ry, n=36):
    return [(cx + rx * math.cos(2 * math.pi * i / n), cy + ry * math.sin(2 * math.pi * i / n)) for i in range(n)]


def rect_poly(x, y, w, h):
    return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)]


def mouth(cx, cy, W, H, v, inner="#3a0f16", tongue="#e8606f", teeth="#ffffff",
          lip=None, lipw=4, bottom_teeth=False, show_teeth=True):
    d, top, bot, w = mouth_path(cx, cy, W, H, v)
    mp = mouth_poly(cx, cy, W, H, v)
    s = [f'<path d="{d}" fill="{inner}"/>']
    hh = bot - top
    if v not in ("OO",):
        tp = ell_poly(cx, bot - hh * 0.05, w * 0.62, hh * 0.38)
    else:
        tp = ell_poly(cx, bot, w * 0.7, hh * 0.3)
    s.append(f'<path d="{poly_d(_clip(tp, mp))}" fill="{tongue}"/>')
    if show_teeth and v in ("AH", "EH", "EE"):
        th = {"AH": 0.2, "EH": 0.26, "EE": 0.42}[v] * hh
        s.append(f'<path d="{poly_d(_clip(rect_poly(cx - w, top - 2, 2 * w, th + 2), mp))}" fill="{teeth}"/>')
        if bottom_teeth:
            s.append(f'<path d="{poly_d(_clip(rect_poly(cx - w, bot - th * 0.8, 2 * w, th), mp))}" fill="{teeth}"/>')
    if lip:
        s.append(f'<path d="{d}" fill="none" stroke="{lip}" stroke-width="{lipw}" stroke-linejoin="round"/>')
    return "".join(s)

def eye(cx, cy, r, blink, sclera="#fff", iris="#14161c", ry=None, lid="#14161c",
        lidw=5, look=(2, 1), iris_r=0.5, ring=None):
    ry = ry or r
    if blink:
        return (f'<path d="M{cx-r*0.9},{cy} q{r*0.9},{ry*0.55} {r*1.8},0" stroke="{lid}" '
                f'stroke-width="{lidw}" fill="none" stroke-linecap="round"/>')
    s = f'<ellipse cx="{cx}" cy="{cy}" rx="{r}" ry="{ry}" fill="{sclera}"/>'
    ir = min(r, ry) * iris_r
    if ring:
        s += f'<circle cx="{cx+look[0]}" cy="{cy+look[1]}" r="{ir*1.45}" fill="{ring}"/>'
    s += f'<circle cx="{cx+look[0]}" cy="{cy+look[1]}" r="{ir}" fill="{iris}"/>'
    s += f'<circle cx="{cx+look[0]+ir*0.38}" cy="{cy+look[1]-ir*0.4}" r="{ir*0.34}" fill="#fff"/>'
    return s

def brows(cx_l, cx_r, y, w, lvl, color, sw=5.5):
    """lvl 0..3: leise -> laut. Hoeher, staerker gewoelbt, aussen angehoben."""
    dy = -lvl * 6.5
    bow = 2 + lvl * 3.2
    inner = (1.2 - lvl) * 3.0  # positive = innen tiefer (angestrengt bei leise? nein: konzentriert)
    out = []
    for cx, side in ((cx_l, -1), (cx_r, 1)):
        xo, xi = cx + side * w / 2, cx - side * w / 2
        yo, yi = y + dy - inner * 0.3, y + dy + inner
        out.append(f'<path d="M{xi},{yi} Q{cx},{y+dy-bow*2} {xo},{yo}" stroke="{color}" '
                   f'stroke-width="{sw}" fill="none" stroke-linecap="round"/>')
    return "".join(out)

def sparkle(x, y, s, c="#ffd84a", o=0.9):
    return (f'<path transform="translate({x},{y}) scale({s})" d="M0,-12 L3,-3 12,0 3,3 0,12 -3,3 -12,0 -3,-3z" '
            f'fill="{c}" opacity="{o}"/>')

# ---------------------------------------------------------------- characters
def bolt(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<line x1="0" y1="-128" x2="0" y2="-92" stroke="#9aa3b5" stroke-width="5"/>',
         f'<circle cx="0" cy="-132" r="{9+l}" fill="#ff5f5f"/>',
         f'<circle cx="0" cy="-132" r="{16+l*3}" fill="#ff5f5f" opacity="{0.1+l*0.05}"/>',
         '<rect x="-14" y="58" width="28" height="12" fill="#3b4252"/>',
         '<rect x="-62" y="66" width="124" height="74" rx="16" fill="url(#robobody)"/>',
         '<rect x="-40" y="84" width="40" height="8" rx="4" fill="#353b49"/><rect x="-40" y="100" width="28" height="8" rx="4" fill="#353b49"/>',
         '<circle cx="34" cy="102" r="12" fill="#ffd84a"/><circle cx="30" cy="98" r="4" fill="#fff6c0"/>',
         '<rect x="-98" y="-36" width="14" height="44" rx="5" fill="#5b6475"/><rect x="84" y="-36" width="14" height="44" rx="5" fill="#5b6475"/>',
         '<circle cx="-91" cy="-14" r="3" fill="#8fd3ff"/><circle cx="91" cy="-14" r="3" fill="#8fd3ff"/>',
         '<rect x="-86" y="-92" width="172" height="152" rx="24" fill="url(#robo)" stroke="#eef2f8" stroke-width="3"/>',
         '<rect x="-74" y="-84" width="60" height="8" rx="4" fill="#ffffff" opacity="0.35"/>',
         '<g fill="#8b94a6"><circle cx="-72" cy="-78" r="3.5"/><circle cx="72" cy="-78" r="3.5"/><circle cx="-72" cy="46" r="3.5"/><circle cx="72" cy="46" r="3.5"/></g>',
         '<rect x="-64" y="-64" width="48" height="36" rx="9" fill="#0d1018"/><rect x="16" y="-64" width="48" height="36" rx="9" fill="#0d1018"/>']
    for ex in (-40, 40):
        if b:
            s.append(f'<rect x="{ex-14}" y="-48" width="28" height="5" rx="2.5" fill="#8fd3ff"/>')
        else:
            s.append(f'<circle cx="{ex}" cy="-46" r="11" fill="#8fd3ff"/><circle cx="{ex}" cy="-46" r="16" fill="#8fd3ff" opacity="0.15"/>'
                     f'<circle cx="{ex+4}" cy="-50" r="3.5" fill="#e8f7ff"/>')
    # Brauen: Metallbalken
    dy, ang = -l * 4.5, -4 + l * 5
    s.append(f'<rect x="-62" y="{-80+dy}" width="44" height="7" rx="3.5" fill="#3b4252" transform="rotate({-ang},-40,{-76+dy})"/>')
    s.append(f'<rect x="18" y="{-80+dy}" width="44" height="7" rx="3.5" fill="#3b4252" transform="rotate({ang},40,{-76+dy})"/>')
    # Mund: LED-Bildschirm mit Vokalform
    s.append('<rect x="-46" y="-14" width="92" height="62" rx="13" fill="#0d1018"/>')
    d, *_ = mouth_path(0, 16, 76, 46, v)
    mp = mouth_poly(0, 16, 76, 46, v)
    s.append(f'<path d="{d}" fill="#8fd3ff" opacity="0.25"/>')
    s.append("".join(f'<path d="{poly_d(_clip(rect_poly(-44, y, 88, 3.2), mp))}" fill="#8fd3ff"/>' for y in range(-12, 48, 6)))
    s.append(f'<path d="{d}" fill="none" stroke="#bfe7ff" stroke-width="2.5"/>')
    return "".join(s)

def croak(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<ellipse cx="0" cy="56" rx="96" ry="84" fill="url(#frog)"/>',
         '<ellipse cx="0" cy="88" rx="56" ry="46" fill="#e8f7c8" opacity="0.9"/>',
         '<ellipse cx="0" cy="2" rx="114" ry="72" fill="url(#frog)"/>',
         '<ellipse cx="-30" cy="-36" rx="40" ry="12" fill="#ffffff" opacity="0.14"/>',
         '<circle cx="-56" cy="-56" r="36" fill="#a6e463"/><circle cx="56" cy="-56" r="36" fill="#a6e463"/>']
    for ex in (-56, 56):
        if b:
            s.append(f'<circle cx="{ex}" cy="-56" r="25" fill="#7cc24a"/>'
                     f'<path d="M{ex-20},-54 q20,14 40,0" stroke="#24401a" stroke-width="5" fill="none" stroke-linecap="round"/>')
        else:
            s.append(eye(ex, -56, 25, False, look=(4 if ex > 0 else 5, 2)))
    s.append(brows(-56, 56, -98, 34, l, "#2f5e1e", 6))
    s.append('<circle cx="-14" cy="-6" r="3" fill="#2f5e1e"/><circle cx="14" cy="-6" r="3" fill="#2f5e1e"/>')
    s.append('<circle cx="-78" cy="18" r="11" fill="#ff9a9a" opacity="0.55"/><circle cx="78" cy="18" r="11" fill="#ff9a9a" opacity="0.55"/>')
    s.append(mouth(0, 32, 110, 54, v, inner="#2b1414"))
    return "".join(s)

def diva(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<path d="M-70,88 q70,60 140,0 l8,56 l-156,0 z" fill="url(#diva)"/>',
         '<circle cx="0" cy="-128" r="34" fill="url(#divahair)"/>',
         '<path d="M-20,-102 q20,-10 40,0" stroke="#ffd84a" stroke-width="6" fill="none" stroke-linecap="round"/>',
         '<ellipse cx="0" cy="-36" rx="100" ry="84" fill="url(#divahair)"/>',
         '<ellipse cx="0" cy="-2" rx="72" ry="84" fill="#f6c9a6"/>',
         
         '<g fill="#ffffff"><circle cx="-40" cy="96" r="6.5"/><circle cx="-20" cy="104" r="6.5"/><circle cx="0" cy="107" r="6.5"/><circle cx="20" cy="104" r="6.5"/><circle cx="40" cy="96" r="6.5"/></g>']
    for ex, side in ((-32, -1), (32, 1)):
        if b:
            s.append(f'<path d="M{ex-15},-24 q15,11 30,0" stroke="#2a0e1c" stroke-width="5" fill="none" stroke-linecap="round"/>'
                     f'<line x1="{ex+side*15}" y1="-24" x2="{ex+side*21}" y2="-30" stroke="#2a0e1c" stroke-width="3" stroke-linecap="round"/>')
        else:
            s.append(eye(ex, -24, 15, False, ry=12, iris="#3a1a2a", look=(1, 1), iris_r=0.7))
            s.append(f'<path d="M{ex-16},-28 q16,-12 32,0" stroke="#2a0e1c" stroke-width="4" fill="none" stroke-linecap="round"/>'
                     f'<line x1="{ex+side*15}" y1="-30" x2="{ex+side*22}" y2="-37" stroke="#2a0e1c" stroke-width="3" stroke-linecap="round"/>')
    s.append(brows(-34, 34, -56, 32, l, "#2a0e1c", 5))
    s.append('<path d="M0,-8 q-5,12 0,16" stroke="#d99a7a" stroke-width="3" fill="none" stroke-linecap="round"/>')
    s.append('<circle cx="-48" cy="10" r="11" fill="#ff8aa8" opacity="0.55"/><circle cx="48" cy="10" r="11" fill="#ff8aa8" opacity="0.55"/>')
    s.append(mouth(0, 40, 66, 46, v, inner="#5a1022", lip="#d8336a", lipw=5))
    s.append(sparkle(100, -70, 1.1) + sparkle(-108, -10, 0.7))
    return "".join(s)

def hoot(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<path d="M-88,-78 l-14,-48 44,26z M88,-78 l14,-48 -44,26z" fill="#5e3e24"/>',
         '<ellipse cx="0" cy="12" rx="102" ry="118" fill="url(#owl)"/>',
         '<path d="M-102,30 q-24,40 4,90 q20,-30 22,-70z M102,30 q24,40 -4,90 q-20,-30 -22,-70z" fill="#7a5434"/>',
         '<ellipse cx="0" cy="72" rx="62" ry="58" fill="#ecdabd"/>',
         '<g stroke="#c4a27a" stroke-width="3" fill="none" stroke-linecap="round"><path d="M-40,92 q8,8 16,0"/><path d="M-8,104 q8,8 16,0"/><path d="M24,92 q8,8 16,0"/><path d="M-24,116 q8,8 16,0"/><path d="M8,116 q8,8 16,0"/></g>',
         '<circle cx="-42" cy="-26" r="40" fill="#f2e6cf"/><circle cx="42" cy="-26" r="40" fill="#f2e6cf"/>']
    for ex in (-42, 42):
        if b:
            s.append(f'<path d="M{ex-28},-26 q28,16 56,0" stroke="#3a2614" stroke-width="6" fill="none" stroke-linecap="round"/>')
        else:
            s.append(eye(ex, -26, 30, False, sclera="#f2e6cf", ring="#ffb22e", iris="#1a120a", iris_r=0.46, look=(0, 2)))
    s.append(brows(-44, 44, -72, 48, l, "#4a301a", 7))
    s.append(mouth(0, 34, 50, 38, v, inner="#3a1a10", tongue="#e07a6a", show_teeth=False))
    s.append('<path d="M-13,0 l13,24 13,-24 q-13,-8 -26,0z" fill="#ffb22e"/>')
    return "".join(s)

def zorp(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = []
    for x, tx in ((-36, -66), (36, 66)):
        s.append(f'<path d="M{x},-92 Q{x*1.2},-130 {tx},-140" stroke="#7b62e6" stroke-width="6" fill="none" stroke-linecap="round"/>')
        s.append(f'<circle cx="{tx}" cy="-142" r="{11+l}" fill="#7dffcf"/><circle cx="{tx}" cy="-142" r="{18+l*3}" fill="#7dffcf" opacity="{0.12+l*0.05}"/>')
    s += ['<path d="M-40,92 q40,24 80,0 l14,46 l-108,0z" fill="#2c2f3d"/>',
          '<path d="M-16,96 l16,28 16,-28" fill="#7dffcf" opacity="0.8"/>',
          '<path d="M0,-104 C82,-104 112,-30 100,16 C88,72 46,104 0,104 C-46,104 -88,72 -100,16 C-112,-30 -82,-104 0,-104z" fill="url(#alien)"/>',
          '<ellipse cx="-34" cy="-74" rx="30" ry="10" fill="#fff" opacity="0.18"/>',
          '<g fill="#5a3fc0" opacity="0.5"><circle cx="-78" cy="-20" r="5"/><circle cx="-70" cy="-4" r="3"/><circle cx="76" cy="-24" r="4"/></g>']
    for ex, rot in ((-40, -14), (40, 14)):
        if b:
            s.append(f'<path d="M{ex-22},-14 q22,14 44,0" stroke="#2a1a66" stroke-width="5.5" fill="none" stroke-linecap="round"/>')
        else:
            s.append(f'<g transform="rotate({rot},{ex},-16)">{eye(ex, -16, 24, False, ry=19, iris="#15102e", iris_r=0.72, look=(0, 1))}</g>')
    s.append(brows(-40, 40, -52, 34, l, "#2a1a66", 5.5))
    s.append(mouth(0, 48, 64, 40, v, inner="#1e1240", tongue="#ff7fae"))
    return "".join(s)

def bones(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<path d="M-56,96 l112,0 l18,44 l-148,0z" fill="#1f2230"/>',
         '<path d="M-14,96 l14,40 14,-40z" fill="#f2efe6"/>',
         '<path d="M-36,104 l30,-10 l0,20z M36,104 l-30,-10 l0,20z" fill="#d6324a"/><circle cx="0" cy="104" r="6" fill="#a8233a"/>',
         # Schaedel + Kiefer
         '<path d="M-58,40 q0,62 58,62 q58,0 58,-62z" fill="url(#skull)"/>',
         '<ellipse cx="0" cy="-20" rx="90" ry="86" fill="url(#skull)"/>',
         '<ellipse cx="-30" cy="-70" rx="30" ry="12" fill="#fff" opacity="0.6"/>',
         # Hut
         '<g transform="translate(0,-12)"><ellipse cx="0" cy="-74" rx="122" ry="20" fill="#1c1e26"/>',
         '<path d="M-70,-76 C-70,-150 70,-150 70,-76z" fill="url(#hat)"/>',
         '<path d="M-68,-92 q68,10 136,0 l1,16 q-69,10 -138,0z" fill="#d6324a"/>',
         '<path d="M-20,-136 q20,10 40,0" stroke="#4d5162" stroke-width="4" fill="none" stroke-linecap="round"/></g>']
    for ex in (-36, 36):
        s.append(f'<ellipse cx="{ex}" cy="-16" rx="26" ry="24" fill="#2a2430"/>')
        if b:
            s.append(f'<path d="M{ex-14},-14 q14,9 28,0" stroke="#7dffcf" stroke-width="4.5" fill="none" stroke-linecap="round"/>')
        else:
            s.append(f'<circle cx="{ex+3}" cy="-14" r="9" fill="#7dffcf"/><circle cx="{ex+3}" cy="-14" r="15" fill="#7dffcf" opacity="0.15"/><circle cx="{ex+6}" cy="-17" r="3" fill="#fff"/>')
    s.append(brows(-36, 36, -46, 40, l, "#b8ad92", 6))
    s.append('<path d="M0,14 l-8,14 q8,4 16,0z" fill="#2a2430"/>')
    s.append(mouth(0, 56, 76, 44, v, inner="#2a2430", tongue="#5a4a66", teeth="#fbf6e9", bottom_teeth=True))
    # Zahnfugen

    return "".join(s)

def spike(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<path d="M-60,96 l120,0 l-12,48 l-96,0z" fill="url(#pot)"/>',
         '<rect x="-68" y="88" width="136" height="20" rx="6" fill="#c9653d"/>',
         # Arme
         '<path d="M-58,10 l-30,0 q-16,0 -16,-16 l0,-30 q0,-14 13,-14 q13,0 13,14 l0,18 l20,0z" fill="#4fae6c"/>',
         '<path d="M58,30 l30,0 q16,0 16,-16 l0,-24 q0,-14 -13,-14 q-13,0 -13,14 l0,12 l-20,0z" fill="#4fae6c"/>',
         '<rect x="-66" y="-112" width="132" height="210" rx="64" fill="url(#cactus)"/>',
         '<g stroke="#2f7a46" stroke-width="3" opacity="0.6" fill="none"><path d="M-34,-96 q-10,90 0,186"/><path d="M34,-96 q10,90 0,186"/></g>']
    sp = [(-66, -50), (-66, 40), (-66, 70), (66, -40), (66, 0), (66, 70), (-50, -96), (50, -96), (-104, -40), (104, -20)]
    for x, y in sp:
        dx = 8 if x > 0 else -8
        s.append(f'<path d="M{x-dx*0.3},{y} l{dx},-5 M{x-dx*0.3},{y} l{dx},5" stroke="#f4f0d8" stroke-width="2.5" stroke-linecap="round"/>')
    s.append('<g transform="translate(14,-114)"><g fill="#ff6fa0">' + "".join(
        f'<ellipse cx="0" cy="-12" rx="9" ry="14" transform="rotate({a})"/>' for a in range(0, 360, 60)) +
        '</g><circle r="8" fill="#ffd84a"/></g>')
    for ex in (-28, 28):
        s.append(eye(ex, -32, 17, b, lid="#173d24", look=(2, 2)))
    s.append(brows(-28, 28, -62, 28, l, "#173d24", 5.5))
    s.append('<circle cx="-46" cy="-4" r="9" fill="#ff8aa8" opacity="0.5"/><circle cx="46" cy="-4" r="9" fill="#ff8aa8" opacity="0.5"/>')
    s.append(mouth(0, 18, 64, 44, v, inner="#2b1414"))
    return "".join(s)

def blub(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = [# Fontaene
         f'<g fill="#9fdcff" opacity="0.85"><path d="M0,-96 q-6,-30 -30,-44 q22,4 30,26 q8,-22 30,-26 q-24,14 -30,44z"/>'
         f'<circle cx="-36" cy="-146" r="{5+l}"/><circle cx="36" cy="-146" r="{5+l}"/><circle cx="0" cy="-150" r="{4+l}"/></g>',
         # Flossen
         '<path d="M-110,50 q-36,-6 -40,30 q30,10 50,-6z M110,50 q36,-6 40,30 q-30,10 -50,-6z" fill="#2a63b8"/>',
         '<path d="M0,-100 C100,-100 124,-30 120,30 C116,100 64,128 0,128 C-64,128 -116,100 -120,30 C-124,-30 -100,-100 0,-100z" fill="url(#whale)"/>',
         '<ellipse cx="-40" cy="-72" rx="40" ry="12" fill="#fff" opacity="0.18"/>',
         '<path d="M-84,60 C-60,128 60,128 84,60 C40,80 -40,80 -84,60z" fill="#d6ecff"/>',
         '<g stroke="#9cc4ea" stroke-width="3" fill="none" stroke-linecap="round"><path d="M-40,84 q0,16 4,28"/><path d="M-14,88 l0,30"/><path d="M14,88 l0,30"/><path d="M40,84 q0,16 -4,28"/></g>']
    for ex in (-48, 48):
        s.append(eye(ex, -24, 18, b, lid="#0f2b55", look=(2, 2)))
    s.append(brows(-48, 48, -56, 30, l, "#0f2b55", 5.5))
    s.append('<circle cx="-78" cy="6" r="11" fill="#ff8aa8" opacity="0.45"/><circle cx="78" cy="6" r="11" fill="#ff8aa8" opacity="0.45"/>')
    s.append(mouth(0, 26, 84, 50, v, inner="#0f1e3d", tongue="#ff7f8f"))
    return "".join(s)

def loop(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<rect x="-128" y="-84" width="256" height="166" rx="16" fill="url(#tape)" stroke="#ffd98a" stroke-width="3"/>',
         '<g fill="#a85412"><circle cx="-114" cy="-70" r="4"/><circle cx="114" cy="-70" r="4"/><circle cx="-114" cy="68" r="4"/><circle cx="114" cy="68" r="4"/></g>',
         '<rect x="-108" y="-72" width="216" height="98" rx="10" fill="#fff4dd"/>',
         '<rect x="-108" y="-72" width="216" height="16" rx="8" fill="#2f8f9d"/><rect x="-108" y="-64" width="216" height="8" fill="#2f8f9d"/>',
         '<rect x="-74" y="-46" width="148" height="56" rx="26" fill="#2a2430"/>',
         # Bandfenster unten = Mundzone
         '<path d="M-84,82 l14,-50 l140,0 l14,50z" fill="#c4661a"/>']
    for ex in (-46, 46):
        if b:
            s.append(f'<circle cx="{ex}" cy="-18" r="22" fill="#5b3b20"/>'
                     f'<path d="M{ex-18},-18 q18,12 36,0" stroke="#fff4dd" stroke-width="5" fill="none" stroke-linecap="round"/>')
        else:
            s.append(f'<circle cx="{ex}" cy="-18" r="22" fill="#5b3b20"/><circle cx="{ex}" cy="-18" r="15" fill="#fff"/>'
                     f'<circle cx="{ex+2}" cy="-17" r="8" fill="#14161c"/>'
                     + "".join(f'<rect x="{ex+2-1.5}" y="-29" width="3" height="5" fill="#fff" transform="rotate({a},{ex+2},-17)"/>' for a in (0, 120, 240))
                     + f'<circle cx="{ex+5}" cy="-21" r="2.6" fill="#fff"/>')
    # Brauen auf dem Etikett
    s.append(brows(-46, 46, -48, 30, l, "#2a2430", 5))
    s.append(mouth(0, 52, 92, 36, v, inner="#2a1208", tongue="#ff7f8f"))
    s.append('<circle cx="-62" cy="66" r="5" fill="#8a420f"/><circle cx="62" cy="66" r="5" fill="#8a420f"/>')
    return "".join(s)

def luna(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = [
         '<circle cx="0" cy="0" r="140" fill="url(#moonglow)"/>', '<circle cx="0" cy="0" r="112" fill="url(#moon)"/>',
         '<g fill="#d6b44e" opacity="0.55"><circle cx="-62" cy="-62" r="14"/><circle cx="-80" cy="40" r="10"/><circle cx="70" cy="-70" r="8"/><circle cx="84" cy="50" r="13"/><circle cx="30" cy="88" r="7"/></g>',
         # Schlafmuetze
         '<path d="M-96,-56 C-80,-130 40,-150 92,-60 q-90,-26 -188,4z" fill="#3b4fa8"/>',
         '<path d="M40,-122 q60,-14 82,40" stroke="#3b4fa8" stroke-width="22" fill="none" stroke-linecap="round"/>',
         '<circle cx="124" cy="-76" r="14" fill="#f2f4ff"/>',
         '<path d="M-100,-56 q96,-36 196,-6" stroke="#f2f4ff" stroke-width="14" fill="none" stroke-linecap="round"/>',
         '<g fill="#ffd84a"><circle cx="-30" cy="-100" r="3"/><circle cx="20" cy="-110" r="3"/><circle cx="60" cy="-96" r="3"/></g>']
    for ex in (-38, 38):
        s.append(eye(ex, -14, 17, b, lid="#6a4e10", look=(1, 2), iris="#2a2140"))
        if not b:
            s.append(f'<path d="M{ex-18},-20 q18,-12 36,0" stroke="#6a4e10" stroke-width="3" fill="none" stroke-linecap="round"/>')
    s.append(brows(-38, 38, -42, 30, l, "#6a4e10", 5))
    s.append('<circle cx="-64" cy="20" r="12" fill="#ff9aa8" opacity="0.45"/><circle cx="64" cy="20" r="12" fill="#ff9aa8" opacity="0.45"/>')
    s.append(mouth(0, 44, 72, 46, v, inner="#4a2a1a"))
    s.append(sparkle(-118, 70, 0.8, "#cfe0ff") + sparkle(122, 104, 0.6, "#cfe0ff"))
    return "".join(s)

def spore(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = ['<path d="M-80,-6 C-86,60 -70,124 0,124 C70,124 86,60 80,-6z" fill="url(#stem)"/>',
         '<path d="M-74,96 q-22,14 -10,30 l168,0 q12,-16 -10,-30z" fill="#5fae5a"/>',
         '<path d="M-60,110 l-6,-22 l10,14 M50,108 l8,-24 l2,20" stroke="#4a9a46" stroke-width="5" fill="none" stroke-linecap="round"/>',
         # Hut
         '<path d="M-128,-2 C-136,-110 -50,-150 0,-150 C50,-150 136,-110 128,-2 C80,14 -80,14 -128,-2z" fill="url(#cap)"/>',
         '<path d="M-118,-6 C-80,8 80,8 118,-6" stroke="#8e1e26" stroke-width="5" fill="none" opacity="0.6"/>',
         '<g fill="#fff4ea"><ellipse cx="-62" cy="-82" rx="20" ry="15"/><ellipse cx="18" cy="-118" rx="16" ry="11"/><ellipse cx="72" cy="-62" rx="17" ry="13"/><ellipse cx="-12" cy="-58" rx="12" ry="9"/><ellipse cx="-104" cy="-30" rx="9" ry="8"/><ellipse cx="106" cy="-26" rx="8" ry="7"/></g>',
         '<ellipse cx="-50" cy="-120" rx="26" ry="8" fill="#fff" opacity="0.25" transform="rotate(-20,-50,-120)"/>']
    for ex in (-30, 30):
        s.append(eye(ex, 34, 15, b, lid="#4a2a14", look=(1, 2), iris="#2a1a10"))
    s.append(brows(-30, 30, 8, 26, l, "#6a4228", 5))
    s.append('<circle cx="-54" cy="58" r="10" fill="#ff8aa8" opacity="0.5"/><circle cx="54" cy="58" r="10" fill="#ff8aa8" opacity="0.5"/>')
    s.append(mouth(0, 76, 60, 38, v, inner="#3a1a10"))
    return "".join(s)

def ember(st):
    b, l, v = st["blink"], st["brow"], st["vowel"]
    s = [# Fluegel
         '<path d="M-70,10 L-140,-60 L-128,-14 L-150,-4 L-120,20 L-130,40z" fill="url(#wing)"/>',
         '<path d="M70,10 L140,-60 L128,-14 L150,-4 L120,20 L130,40z" fill="url(#wing)"/>',
         # Hoerner
         '<path d="M-52,-80 q-14,-40 -36,-58 q8,40 14,66z M52,-80 q14,-40 36,-58 q-8,40 -14,66z" fill="#ffe2a8"/>',
         # Koerper
         '<path d="M-64,70 C-74,120 -44,140 0,140 C44,140 74,120 64,70z" fill="url(#dragon)"/>',
         '<path d="M-30,84 C-36,120 -20,136 0,136 C20,136 36,120 30,84z" fill="#ffd9a0"/>',
         '<g stroke="#e8b878" stroke-width="2.5"><line x1="-24" y1="100" x2="24" y2="100"/><line x1="-26" y1="116" x2="26" y2="116"/></g>',
         # Kopf
         '<path d="M0,-104 C74,-104 104,-58 102,-4 C100,56 60,92 0,92 C-60,92 -100,56 -102,-4 C-104,-58 -74,-104 0,-104z" fill="url(#dragon)"/>',
         '<path d="M-10,-104 l10,-20 l10,20z M-36,-100 l6,-14 l10,12z M36,-100 l-6,-14 l-10,12z" fill="#b8325a"/>',
         '<ellipse cx="-40" cy="-76" rx="28" ry="9" fill="#fff" opacity="0.2"/>',
         # Schnauze
         '<ellipse cx="0" cy="40" rx="64" ry="44" fill="#ffb066"/>',
         '<ellipse cx="-16" cy="12" rx="5" ry="3.5" fill="#7a2a14"/><ellipse cx="16" cy="12" rx="5" ry="3.5" fill="#7a2a14"/>']
    for ex in (-38, 38):
        s.append(eye(ex, -32, 20, b, lid="#5a1408", sclera="#fff8e6", iris="#2a0e06", ring="#ffcc33",
                     iris_r=0.42, look=(2, 2)))
    s.append(brows(-38, 38, -66, 34, l, "#7a1c14", 6))
    s.append(mouth(0, 46, 82, 46, v, inner="#4a0e10", tongue="#ff7f8f", bottom_teeth=False))
    # Fangzaehnchen
    return "".join(s)

CHARS = [
    ("BOLT", "TIN TENOR", bolt, 2),
    ("CROAK", "SWAMP BASS", croak, 1),
    ("DIVA", "OPERA STAR", diva, 3),
    ("HOOT", "NIGHT ALTO", hoot, 1),
    ("ZORP", "SPACE SOPRANO", zorp, 2),
    ("BONES", "GRAVE CROONER", bones, 2),
    ("SPIKE", "DESERT BALLAD", spike, 1),
    ("BLUB", "DEEP BARITONE", blub, 1),
    ("LOOP", "TAPE LOOPER", loop, 2),
    ("LUNA", "LULLABY MOON", luna, 0),
    ("SPORE", "FOREST CHOIR", spore, 2),
    ("EMBER", "FIRE BELTER", ember, 3),
]

def svg_doc(w, h, body):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">'
            f'<defs>{DEFS}</defs><rect width="{w}" height="{h}" fill="#07080a"/>'
            f'<rect width="{w}" height="{h}" fill="url(#glow)"/>{body}</svg>')

def render(svg, name):
    p = os.path.join("/tmp", name + ".svg")
    open(p, "w").write(svg)
    subprocess.run(["rsvg-convert", "-o", os.path.join(OUT, name + ".png"), p], check=True)

def sheet():
    CW, CH, TOP = 380, 420, 90
    W, H = CW * 4, TOP + CH * 3 + 20
    body = [f'<text x="{W/2}" y="52" text-anchor="middle" font-family="Inter" font-weight="800" font-size="26" letter-spacing="8" fill="#e4e6ec">MONK · VOICE CAST</text>',
            f'<text x="{W/2}" y="76" text-anchor="middle" font-family="Inter" font-weight="600" font-size="12" letter-spacing="4" fill="#6e7180">12 SÄNGER · ALLE SINGEN „AH“</text>']
    for i, (name, role, fn, lvl) in enumerate(CHARS):
        cx = CW * (i % 4) + CW / 2
        cy = TOP + CH * (i // 4)
        body.append(f'<rect x="{cx-CW/2+10}" y="{cy+6}" width="{CW-20}" height="{CH-12}" rx="22" fill="#ffffff" fill-opacity="0.025" stroke="#ffffff" stroke-opacity="0.06"/>')
        body.append(f'<g transform="translate({cx},{cy+184}) scale(0.92)">{fn(dict(blink=False, brow=lvl, vowel="AH"))}</g>')
        body.append(f'<text x="{cx}" y="{cy+362}" text-anchor="middle" font-family="Inter" font-weight="700" font-size="12" letter-spacing="3.5" fill="#8a8d99">{role}</text>')
        body.append(f'<text x="{cx}" y="{cy+392}" text-anchor="middle" font-family="Inter" font-weight="800" font-size="22" letter-spacing="5" fill="#e4e6ec">{name}</text>')
    render(svg_doc(W, H, "".join(body)), "sheet_12")

def states(name):
    fn = next(c[2] for c in CHARS if c[0] == name)
    role = next(c[1] for c in CHARS if c[0] == name)
    frames = [("AUGEN AUF", dict(blink=False, brow=1, vowel="OH")),
              ("BLINZELN", dict(blink=True, brow=1, vowel="OH")),
              ("BRAUE 1 · PP", dict(blink=False, brow=0, vowel="OH")),
              ("BRAUE 2 · P", dict(blink=False, brow=1, vowel="OH")),
              ("BRAUE 3 · F", dict(blink=False, brow=2, vowel="OH")),
              ("BRAUE 4 · FF", dict(blink=False, brow=3, vowel="OH")),
              ("OO", dict(blink=False, brow=1, vowel="OO")),
              ("OH", dict(blink=False, brow=1, vowel="OH")),
              ("AH", dict(blink=False, brow=1, vowel="AH")),
              ("EH", dict(blink=False, brow=1, vowel="EH")),
              ("EE", dict(blink=False, brow=1, vowel="EE"))]
    FW, TOP, FH = 210, 136, 340
    W = FW * len(frames) + 40
    H = TOP + FH + 30
    body = [f'<text x="40" y="52" font-family="Inter" font-weight="800" font-size="26" letter-spacing="6" fill="#e4e6ec">{name}</text>',
            f'<text x="40" y="76" font-family="Inter" font-weight="700" font-size="12" letter-spacing="3.5" fill="#8a8d99">{role} · ANIMATIONSZUSTÄNDE</text>']
    groups = [("AUGEN", 0, 2), ("BRAUEN NACH LAUTSTÄRKE", 2, 6), ("VOKALE (MUND)", 6, 11)]
    for g, a, e in groups:
        x0, x1 = 20 + FW * a + 8, 20 + FW * e - 8
        body.append(f'<line x1="{x0}" y1="{TOP-10}" x2="{x1}" y2="{TOP-10}" stroke="#3a3d4a" stroke-width="2"/>')
        body.append(f'<text x="{x0}" y="{TOP-18}" font-family="Inter" font-weight="700" font-size="11" letter-spacing="3" fill="#6e7180">{g}</text>')
    for i, (lab, st) in enumerate(frames):
        cx = 20 + FW * i + FW / 2
        body.append(f'<rect x="{cx-FW/2+6}" y="{TOP}" width="{FW-12}" height="{FH}" rx="18" fill="#ffffff" fill-opacity="0.03" stroke="#ffffff" stroke-opacity="0.06"/>')
        k = uid("f")
        body.append(f'<clipPath id="{k}"><rect x="{cx-FW/2+6}" y="{TOP}" width="{FW-12}" height="{FH}" rx="18"/></clipPath>')
        body.append(f'<g clip-path="url(#{k})"><g transform="translate({cx},{TOP+158}) scale(0.64)">{fn(st)}</g></g>')
        body.append(f'<text x="{cx}" y="{TOP+FH-18}" text-anchor="middle" font-family="Inter" font-weight="800" font-size="14" letter-spacing="3" fill="#d9dbe2">{lab}</text>')
    render(svg_doc(W, H, "".join(body)), f"states_{name}")

VOWELS = ["OO", "OH", "AH", "EH", "EE"]


def frame_svg(fn, blink, brow, vowel):
    """One pose, centred, in a 320 x 320 box (the display shows it in its square)."""
    body = fn(dict(blink=bool(blink), brow=brow, vowel=VOWELS[vowel]))
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="320" height="320" viewBox="-160 -168 320 320">'
            f'<defs>{DEFS}</defs><rect x="-160" y="-168" width="320" height="320" fill="url(#glow)"/>{body}</svg>')


def real():
    d = os.path.join(ROOT, "res", "monk")
    os.makedirs(d, exist_ok=True)
    n = 0
    for k, (name, role, fn, lvl) in enumerate(CHARS):
        for b in range(2):
            for l in range(4):
                for v in range(5):
                    open(os.path.join(d, f"s{k:02d}_{b}{l}{v}.svg"), "w").write(frame_svg(fn, b, l, v))
                    n += 1
    print(f"wrote {n} files to res/monk")


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "real":
        real()
    else:
        if len(sys.argv) > 2:
            OUT = sys.argv[2]
        os.makedirs(OUT, exist_ok=True)
        sheet()
        states("BOLT")
        states("EMBER")
        print("ok")
