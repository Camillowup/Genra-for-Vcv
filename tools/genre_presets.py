#!/usr/bin/env python3
"""Genre presets (2.31): every genre without a preset of its own gets one in GRUND, FUNKE, SCHMELZ and GENRA MONO.

Each new preset starts from a factory preset of the same instrument (BASE) and moves the knobs that make the genre
(TWEAKS). Its style is the group of the genres it is for, as far as the instrument has none for them yet (so a genre
that has presets keeps them: src/GenreMatch.hpp takes whole names first).

  genre_presets.py write          -> inserts / replaces the block between the GENRE PRESETS markers in the tables
  genre_presets.py calibrate      -> runs the instrument tests and sets each new preset's gain to the others' level
"""
import os, re, subprocess, sys, statistics

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# genre -> group: the menu groups of the new presets (a group's label lists only the genres the instrument lacks)
GROUPS = [
    ["MINIMAL", "ACID"],
    ["TRANCE", "PSYTRANCE"],
    ["SYNTHWAVE", "DARKWAVE", "ELECTRO"],
    ["HARDCORE", "EBM", "INDUSTRIAL"],
    ["WITCH HOUSE", "HORROR", "DUNGEON SYNTH"],
    ["BREAKBEAT", "BREAKS", "BIG BEAT", "BREAKCORE"],
    ["DRUM & BASS", "JUNGLE"],
    ["DOWNTEMPO", "TRIP-HOP"],
    ["HIP-HOP", "TRAP", "DRILL", "PHONK"],
    ["AFROBEAT", "AFROBEATS", "AMAPIANO", "GQOM"],
    ["REGGAETON", "DANCEHALL", "MOOMBAHTON"],
    ["BAILE FUNK", "CUMBIA", "BOSSA NOVA"],
    ["2-STEP", "UK FUNKY", "BASSLINE", "FOOTWORK"],
    ["JERSEY CLUB", "BALTIMORE CLUB"],
    ["AMBIENT", "DRONE", "DARK AMBIENT", "BERLIN SCHOOL"],
    ["ROOTS REGGAE"],
]

# ---------------------------------------------------------------- the instruments' tables
INST = {
    "GRUND": dict(
        file="src/grund/GrundPresets.hpp",
        fields="style name model timbre bite body sub mute glide accent mode amp tone res fenv fdec drive comp key atk dec "
               "sus rel chorus octave width verb fx gain".split(),
        fx="cOn th ra ca cr dOn dt dto dmx hpf lo mi hi chOn rate depth vs vd vp".split(),
        test="test/test_grund.cpp"),
    "FUNKE": dict(
        file="src/funke/FunkePresets.hpp",
        fields="style name model timbre bite width sub mode glide accent ftype cutoff res fenv key fatk fdec drive atk dec "
               "sus rel vdepth vrate vdelay div chorus echo fdbk lofi verb gain".split(),
        test="test/test_funke.cpp"),
    "SCHMELZ": dict(
        file="src/schmelz/SchmelzPresets.hpp",
        fields="style name reg cresc rel tone ensDepth oct ens sound decay sustain trem split atk tremRate vel hold bal anim "
               "animRate phaser phaserRate verb size volume gain".split(),
        test="test/test_schmelz.cpp"),
    "MONO": dict(
        file="src/mono/MonoPresets.hpp",
        fields="style name wave1 wave2 tune1 tune2 level1 level2 glide ftype cutoff res egAmt drive keytrk fA fD fS fR aA aD "
               "aS aR rate lfoTri lfoPitch lfoFilter accent width gain".split(),
        test="test/test_mono.cpp"),
}

# ---------------------------------------------------------------- the new presets: genre: (name, base, tweaks)
SPEC = {
    "GRUND": {
        "2-STEP": ("2-STEP SKIP", "2-STEP SUB", dict(glide=.15, dec=.40, tone=.45, accent=.55)),
        "MINIMAL": ("MINIMAL CLICK", "HYPNOTIC PLUCK", dict(tone=.32, dec=.32, fenv=.45, verb=.12)),
        "ACID": ("ACID RUBBER", "TECHNO ROLL", dict(res=.65, fenv=.70, fdec=.20, glide=.20, mode="MODE_LEGATO", accent=.80)),
        "ELECTRO": ("ELECTRO FUNK", "GRIME SQUARE", dict(timbre=.70, tone=.55, fenv=.50, dec=.30, sus=.20)),
        "TRANCE": ("TRANCE ROLLER", "OFFBEAT SUB", dict(tone=.32, fenv=.30, drive=.15, dec=.25)),
        "BREAKBEAT": ("BREAKBEAT PICK", "ROCK PICK", dict(amp="DI", drive=.25, tone=.50)),
        "DOWNTEMPO": ("DOWNTEMPO ROUND", "LO-FI FINGER", dict(tone=.30, chorus=.20, verb=.15, rel=.40)),
        "SYNTHWAVE": ("SYNTHWAVE BASS", "80S DX BASS", dict(model="SYNTH", timbre=.50, tone=.50, fenv=.35, octave=.30)),
        "AMBIENT": ("AMBIENT SWELL", "ARCO DARK", dict(atk=.45, rel=.70, verb=.45, tone=.35)),
        "AFROBEAT": ("AFROBEAT FINGER", "MOTOWN P-BASS", dict(tone=.50, bite=.50, dec=.50, mute=.40)),
        "AFROBEATS": ("AFROBEATS SUB", "808 BOOM", dict(dec=.55, tone=.40, glide=.15)),
        "AMAPIANO": ("LOG DRUM", "808 BOOM", dict(timbre=.55, bite=.50, dec=.35, sus=.0, fenv=.30, tone=.55, drive=.20)),
        "GQOM": ("GQOM BOOM", "RUMBLE", dict(tone=.30, drive=.50, dec=.50)),
        "REGGAETON": ("DEMBOW 808", "808 BOOM", dict(dec=.45, tone=.45, drive=.25)),
        "DANCEHALL": ("DANCEHALL SUB", "STEPPERS SUB", dict(tone=.30, dec=.45, sus=.50)),
        "MOOMBAHTON": ("MOOMBAH GROWL", "GROWL", dict(tone=.45, fenv=.30)),
        "BAILE FUNK": ("BAILE 808", "808 BOOM", dict(drive=.45, tone=.50, amp="FUZZAMP")),
        "CUMBIA": ("CUMBIA PICK", "MOTOWN P-BASS", dict(model="PICK", tone=.50, dec=.40)),
        "BOSSA NOVA": ("BOSSA UPRIGHT", "JAZZ UPRIGHT", dict(tone=.45, dec=.65)),
        "UK FUNKY": ("UK FUNKY BOUNCE", "DEEP HOUSE", dict(fenv=.45, dec=.35)),
        "BASSLINE": ("NICHE WARP", "ORGAN BASS", dict(fenv=.45, tone=.55, res=.25, glide=.10)),
        "FOOTWORK": ("FOOTWORK 808", "808 BOOM", dict(dec=.35, tone=.50)),
        "JERSEY CLUB": ("JERSEY BOUNCE", "808 BOOM", dict(dec=.30, tone=.50, glide=.20)),
        "BALTIMORE CLUB": ("BALTIMORE SUB", "2-STEP SUB", dict(dec=.35, tone=.45)),
        "BREAKCORE": ("BREAKCORE TEAR", "NEURO REESE", dict(drive=.70, tone=.55)),
        "BIG BEAT": ("BIG BEAT FUZZ", "ROCK PICK", dict(amp="FUZZAMP", drive=.55)),
        "BREAKS": ("BREAKS ROLLER", "TECHNO ROLL", dict(res=.45, fenv=.55)),
        "PHONK": ("PHONK 808", "DRILL SLIDE", dict(drive=.60, tone=.50)),
        "HARDCORE": ("HARDCORE KICKBASS", "HARD RUMBLE", dict(dec=.25, tone=.40)),
        "PSYTRANCE": ("PSY ROLL", "OFFBEAT SUB", dict(dec=.18, fenv=.45, tone=.35, res=.20)),
        "EBM": ("EBM SEQUENCE", "INDUSTRIAL", dict(dec=.25, tone=.45, drive=.45)),
        "DRONE": ("DRONE BOW", "ARCO DARK", dict(atk=.50, rel=.70, sus=.90, tone=.30)),
        "DARK AMBIENT": ("DARK SUB DRONE", "ARCO DARK", dict(model="SYNTH", tone=.22, atk=.40, rel=.70, verb=.50)),
        "BERLIN SCHOOL": ("BERLIN SEQ", "80S DX BASS", dict(model="SYNTH", tone=.45, fenv=.50, dec=.30, res=.30)),
        "WITCH HOUSE": ("WITCH 808 DRAG", "808 BOOM", dict(tone=.30, verb=.35, dec=.90, drive=.20)),
        "DARKWAVE": ("DARKWAVE PICK", "ROCK PICK", dict(chorus=.40, tone=.50, width=.30)),
        "DUNGEON SYNTH": ("DUNGEON CELLO", "ARCO DARK", dict(tone=.40, verb=.40)),
        "INDUSTRIAL": ("INDUSTRIAL GRIND", "INDUSTRIAL", dict(drive=.80, tone=.50)),
        "HORROR": ("HORROR BOW", "ARCO DARK", dict(tone=.25, glide=.40, verb=.45)),
    },
    "FUNKE": {
        "2-STEP": ("2-STEP STAB", "M1 ORGAN", dict(cutoff=.75, dec=.25, sus=.10)),
        "MINIMAL": ("MINIMAL DUB BLIP", "HYPNOTIC BLIP", dict(echo=.50, fdbk=.50)),
        "ACID": ("ACID SQUELCH LEAD", "ACID LEAD", dict(res=.85, fenv=.75)),
        "BREAKBEAT": ("BREAKBEAT HOOVER", "ELECTRO ZAP", dict(model="SAWLEAD", mode="MODE_POLY", sus=.30, width=.60)),
        "JUNGLE": ("JUNGLE RAGGA STAB", "DUB SKANK", dict(echo=.45)),
        "DRUM & BASS": ("LIQUID RHODES", "CLASSIC RHODES", dict(verb=.40, echo=.35)),
        "HIP-HOP": ("BOOM BAP KEYS", "LO-FI KEYS", dict(lofi=.35)),
        "TRAP": ("TRAP BELLS", "KALIMBA", dict(dec=.45, echo=.25)),
        "TRIP-HOP": ("TRIP-HOP RHODES", "LO-FI KEYS", dict(lofi=.40, echo=.45)),
        "DOWNTEMPO": ("DOWNTEMPO RHODES", "SPACE RHODES", dict(verb=.35)),
        "AFROBEAT": ("AFROBEAT ORGAN", "JAZZ ORGAN", dict(bite=.70, chorus=.30)),
        "AFROBEATS": ("AFROBEATS PLUCK", "KALIMBA", dict(dec=.35)),
        "AMAPIANO": ("AMAPIANO KEYS", "CLASSIC RHODES", dict(verb=.35, echo=.25)),
        "BAILE FUNK": ("BAILE STAB", "ELECTRO ZAP", dict(model="SAWLEAD", dec=.25)),
        "REGGAETON": ("REGGAETON PLUCK", "KALIMBA", dict(model="PLUCK")),
        "FOOTWORK": ("FOOTWORK STAB", "DRILL BELL", dict(model="SQUARE", dec=.32, sus=.15)),
        "BREAKCORE": ("BREAKCORE GLITCH", "ELECTRO ZAP", dict(lofi=.50, drive=.50)),
        "UK FUNKY": ("UK FUNKY ORGAN", "M1 ORGAN", dict(dec=.30, sus=.20)),
        "BASSLINE": ("BASSLINE ORGAN STAB", "M1 ORGAN", dict(fenv=.45)),
        "GQOM": ("GQOM SYNTH", "HYPNOTIC BLIP", dict(cutoff=.55, echo=.45)),
        "BIG BEAT": ("BIG BEAT HOOVER", "SUPERSAW", dict(drive=.45)),
        "BREAKS": ("BREAKS STAB", "FILTER HOUSE STAB", dict(res=.50)),
        "JERSEY CLUB": ("JERSEY BELL", "GARAGE BELL", dict(dec=.30)),
        "BALTIMORE CLUB": ("BALTIMORE STAB", "M1 ORGAN", dict(dec=.32, sus=.15)),
        "PHONK": ("PHONK COWBELL", "DRILL BELL", dict(timbre=.30, dec=.30)),
        "HARDCORE": ("HARDCORE HOOVER", "SUPERSAW", dict(drive=.55, width=.90)),
        "PSYTRANCE": ("PSY ZAP", "ACID LEAD", dict(echo=.45, fdbk=.55)),
        "EBM": ("EBM SEQUENCE", "ELECTRO ZAP", dict(drive=.40)),
        "DANCEHALL": ("DANCEHALL SKANK", "DUB SKANK", dict(echo=.25)),
        "MOOMBAHTON": ("MOOMBAH LEAD", "SUPERSAW", dict(dec=.40, sus=.40)),
        "CUMBIA": ("CUMBIA ORGAN", "JAZZ ORGAN", dict(chorus=.50)),
        "BOSSA NOVA": ("BOSSA VIBES", "VIBES", dict(verb=.25)),
        "DRONE": ("DRONE FLUTE", "AIR PAD", dict(atk=.75, rel=.70)),
        "BERLIN SCHOOL": ("BERLIN SEQUENCE", "80S POLY", dict(mode="MODE_MONO", fenv=.50, echo=.45)),
        "DARKWAVE": ("DARKWAVE LEAD", "80S POLY", dict(chorus=.60)),
        "DUNGEON SYNTH": ("DUNGEON FLUTE", "FLUTE SOLO", dict(verb=.55)),
        "INDUSTRIAL": ("INDUSTRIAL CLANG", "ELECTRO ZAP", dict(model="BELL", drive=.60)),
        "HORROR": ("HORROR MUSIC BOX", "AMBIENT BELLS", dict(model="BELL", lofi=.20)),
        "WITCH HOUSE": ("WITCH CHIME", "AMBIENT BELLS", dict(lofi=.30, verb=.55)),
    },
    "SCHMELZ": {
        "2-STEP": ("2-STEP ORGAN", "GARAGE CHORDS", dict(cresc=.15)),
        "MINIMAL": ("MINIMAL CHORD", "DUB TECHNO CHORD", dict(verb=.40, size=.60)),
        "ACID": ("ACID SWEEP PAD", "HYPNOTIC SWEEP", dict(phaser=.60, anim=.60)),
        "ELECTRO": ("ELECTRO PAD", "SYNTHWAVE PAD", dict(verb=.30, cresc=.30)),
        "BREAKBEAT": ("BREAKBEAT STRINGS", "STRING + BRASS", dict(cresc=.20)),
        "JUNGLE": ("JUNGLE STRINGS", "DISCO STRINGS", dict(cresc=.45, verb=.50)),
        "DRUM & BASS": ("LIQUID PAD", "AMBIENT CLOUD", dict(cresc=.60, verb=.55)),
        "HIP-HOP": ("BOOM BAP STRINGS", "EP + STRINGS", dict(verb=.25)),
        "TRAP": ("TRAP CHOIR", "DRILL CHOIR", dict(verb=.50)),
        "DOWNTEMPO": ("DOWNTEMPO PAD", "LO-FI TAPE PAD", dict(verb=.40)),
        "AFROBEAT": ("AFROBEAT HORNS", "ANALOG BRASS", dict(cresc=.10)),
        "AFROBEATS": ("AFROBEATS PAD", "SYNTHWAVE PAD", dict(verb=.35)),
        "AMAPIANO": ("AMAPIANO PIANO", "PIANO HOUSE STAB", dict(sound=.25, verb=.35)),
        "BAILE FUNK": ("BAILE BRASS STAB", "ANALOG BRASS", dict(cresc=.10, rel=.30)),
        "REGGAETON": ("REGGAETON PAD", "SYNTHWAVE PAD", dict(cresc=.35)),
        "FOOTWORK": ("FOOTWORK VOX PAD", "GLASS CHOIR", dict(verb=.40)),
        "BREAKCORE": ("BREAKCORE DREAD", "DARK DRONE", dict(cresc=.50, rel=.60)),
        "UK FUNKY": ("UK FUNKY CHORDS", "GARAGE CHORDS", dict(verb=.35)),
        "BASSLINE": ("BASSLINE STAB", "PIANO HOUSE STAB", dict(rel=.25)),
        "GQOM": ("GQOM DREAD", "DARK DRONE", dict(cresc=.50, rel=.60, phaser=.20)),
        "BIG BEAT": ("BIG BEAT BRASS", "STRING + BRASS", dict(cresc=.15, tone=.85)),
        "BREAKS": ("BREAKS PAD", "TRANCE SWELL", dict(cresc=.50)),
        "JERSEY CLUB": ("JERSEY VOX", "GLASS CHOIR", dict(cresc=.20, rel=.40)),
        "BALTIMORE CLUB": ("BALTIMORE STAB", "PIANO HOUSE STAB", dict(rel=.20)),
        "PHONK": ("PHONK CHOIR", "DRILL CHOIR", dict(tone=.35)),
        "HARDCORE": ("HARDCORE SWELL", "TRANCE SWELL", dict(cresc=.40, tone=.80)),
        "PSYTRANCE": ("PSY SWEEP", "HYPNOTIC SWEEP", dict(animRate=.55)),
        "EBM": ("EBM BRASS", "ANALOG BRASS", dict(cresc=.05, rel=.25)),
        "DANCEHALL": ("DANCEHALL ORGAN", "DUB ORGAN PAD", dict(verb=.40)),
        "MOOMBAHTON": ("MOOMBAH PAD", "SYNTHWAVE PAD", dict(cresc=.30, verb=.35)),
        "CUMBIA": ("CUMBIA ORGAN", "DUB ORGAN PAD", dict(trem=.30)),
        "BOSSA NOVA": ("BOSSA STRINGS", "EP + STRINGS", dict(verb=.40)),
        "DRONE": ("DRONE CHOIR", "DARK DRONE", dict(reg=.86, rel=.70)),
        "BERLIN SCHOOL": ("BERLIN STRINGS", "SOLINA DREAM", dict(phaser=.45)),
        "DARKWAVE": ("DARKWAVE STRINGS", "PHASED STRINGS", dict(tone=.45)),
        "DUNGEON SYNTH": ("DUNGEON CHOIR", "CINEMATIC", dict(reg=.86)),
        "INDUSTRIAL": ("INDUSTRIAL HUM", "DARK DRONE", dict(phaser=.50, tone=.40, rel=.60)),
        "HORROR": ("HORROR STRINGS", "CINEMATIC", dict(reg=.43, trem=.40)),
        "WITCH HOUSE": ("WITCH CHOIR", "GLASS CHOIR", dict(tone=.40, verb=.60)),
    },
    "MONO": {
        "MINIMAL": ("MINIMAL ROLL", "DEEP HOUSE", dict(aD=.42, aS=.30, cutoff=.30, egAmt=.50)),
        "TRANCE": ("TRANCE ROLLER", "TAURUS", dict(aD=.42, aS=.25, aR=.20, cutoff=.35)),
        "BREAKBEAT": ("BREAKBEAT MOOG", "FAT MOOG", dict(aD=.40, aS=.50)),
        "JUNGLE": ("JUNGLE SUB", "DEEP SUB", dict(glide=.15)),
        "DRUM & BASS": ("DNB REESE", "REESE", dict(cutoff=.45, drive=.40)),
        "TRAP": ("TRAP 808", "808 GLIDE", dict(drive=.30)),
        "DRILL": ("DRILL GLIDE", "808 GLIDE", dict(glide=.60)),
        "TRIP-HOP": ("TRIP-HOP MOOG", "TRIP-HOP", dict(cutoff=.30)),
        "DOWNTEMPO": ("DOWNTEMPO ROUND", "LO-FI ROUND", dict(cutoff=.32)),
        "SYNTHWAVE": ("SYNTHWAVE PULSE", "SYNTHWAVE", dict(wave1=.67)),
        "AMBIENT": ("AMBIENT SUB", "ROUND SUB", dict(aA=.40, aR=.60)),
        "AFROBEAT": ("AFROBEAT PLUCK", "FUNK PLUCK", dict(cutoff=.34)),
        "AFROBEATS": ("AFROBEATS SUB", "808 GLIDE", dict(glide=.20)),
        "AMAPIANO": ("LOG DRUM", "808 GLIDE", dict(aD=.55, aS=.0, egAmt=.50, cutoff=.35, res=.30)),
        "BAILE FUNK": ("BAILE 808", "808 GLIDE", dict(drive=.60)),
        "REGGAETON": ("DEMBOW SUB", "DEEP SUB", dict(aD=.40, aS=.60)),
        "FOOTWORK": ("FOOTWORK 808", "808 GLIDE", dict(aD=.58)),
        "BREAKCORE": ("BREAKCORE ACID", "SQUELCH", dict(drive=.60)),
        "UK FUNKY": ("UK FUNKY BASS", "DEEP HOUSE", dict(egAmt=.55, aD=.35)),
        "BASSLINE": ("NICHE WOBBLE", "SPEED REESE", dict(rate=.55, lfoFilter=.40)),
        "GQOM": ("GQOM SUB", "DUB SUB", dict(drive=.50)),
        "BIG BEAT": ("BIG BEAT FUZZ", "FAT MOOG", dict(drive=.80)),
        "BREAKS": ("BREAKS RUBBER", "RUBBER", dict(res=.50)),
        "JERSEY CLUB": ("JERSEY BOUNCE", "2-STEP BOUNCE", dict(aD=.35)),
        "BALTIMORE CLUB": ("BALTIMORE BOUNCE", "2-STEP BOUNCE", dict(glide=.05)),
        "PHONK": ("PHONK 808", "808 GLIDE", dict(drive=.70)),
        "HARDCORE": ("HARDCORE ZAP", "ELECTRO ZAP", dict(drive=.70)),
        "PSYTRANCE": ("PSY ROLLER", "TAURUS", dict(aD=.40, aS=.20, cutoff=.40, egAmt=.60)),
        "EBM": ("EBM SEQUENCE", "GRIME SQUARE", dict(aD=.30, aS=.20)),
        "DANCEHALL": ("DANCEHALL SUB", "DUB SUB", dict(aD=.45)),
        "ROOTS REGGAE": ("ROOTS SUB", "DUB SUB", dict(cutoff=.24)),
        "MOOMBAHTON": ("MOOMBAH WOBBLE", "WOBBLE", dict(rate=.48)),
        "CUMBIA": ("CUMBIA PLUCK", "FUNK PLUCK", dict(aD=.40)),
        "BOSSA NOVA": ("BOSSA ROUND", "ROUND SUB", dict(aD=.45, aS=.60)),
        "DRONE": ("DRONE REESE", "REESE", dict(aA=.50, aR=.60)),
        "DARK AMBIENT": ("DARK SUB", "DEEP SUB", dict(aA=.40, aR=.60)),
        "BERLIN SCHOOL": ("BERLIN SEQUENCE", "TAURUS", dict(aD=.30, aS=.20, egAmt=.60, res=.40)),
        "DARKWAVE": ("DARKWAVE PULSE", "SYNTHWAVE", dict(cutoff=.36, width=.60)),
        "DUNGEON SYNTH": ("DUNGEON DRONE", "ROUND SUB", dict(aA=.30, aR=.55)),
        "INDUSTRIAL": ("INDUSTRIAL SQUARE", "GRIME SQUARE", dict(drive=.80)),
        "HORROR": ("HORROR GROWL", "REESE", dict(rate=.10, lfoPitch=.08)),
        "WITCH HOUSE": ("WITCH 808", "808 GLIDE", dict(cutoff=.22, aR=.30)),
    },
}

BEGIN = "\t// ---- GENRE PRESETS (2.31, tools/genre_presets.py): one per genre that had none; do not edit by hand\n"
END = "\t// ---- END GENRE PRESETS\n"


def split_top(s):
    """Split at commas that are not inside parentheses / braces / quotes."""
    out, depth, cur, q = [], 0, "", False
    for c in s:
        if c == '"':
            q = not q
        if not q:
            if c in "({":
                depth += 1
            elif c in ")}":
                depth -= 1
            elif c == "," and depth == 0:
                out.append(cur.strip())
                cur = ""
                continue
        cur += c
    if cur.strip():
        out.append(cur.strip())
    return out


def rows(text):
    """The preset initializers of PRESETS[]: list of (start, end, fields)."""
    a = text.index("static const Preset PRESETS[] = {")
    i = text.index("{", a + 30)
    res = []
    end_arr = text.index("\n};", a)
    while True:
        j = text.find('{"', i)
        if j < 0 or j > end_arr:
            break
        depth, k, q = 0, j, False
        while True:
            c = text[k]
            if c == '"':
                q = not q
            elif not q:
                if c == "{":
                    depth += 1
                elif c == "}":
                    depth -= 1
                    if depth == 0:
                        break
            k += 1
        body = re.sub(r"/\*.*?\*/", "", text[j + 1:k], flags=re.S)
        res.append((j, k + 1, split_top(body)))
        i = k + 1
    return res


def num(v):
    return f"{v:.2f}f" if isinstance(v, float) else str(v)


def groups_for(inst):
    """genre -> the style label for this instrument."""
    have = set(SPEC[inst].keys())
    lab = {}
    for g in GROUPS:
        mine = [x for x in g if x in have]
        for x in mine:
            lab[x] = " · ".join(mine)
    return lab


def build(inst, gains=None):
    cfg = INST[inst]
    path = os.path.join(ROOT, cfg["file"])
    text = open(path, encoding="utf-8").read()
    if BEGIN in text:  # the old block out first (its bases stay above it)
        a, b = text.index(BEGIN), text.index(END) + len(END)
        text = text[:a] + text[b:]
    base = {}
    for _, _, f in rows(text):
        base[f[1].strip('"')] = f
    fields = cfg["fields"]
    lab = groups_for(inst)
    lines = [BEGIN]
    for genre, (name, bname, tw) in SPEC[inst].items():
        if bname not in base:
            raise SystemExit(f"{inst}: base preset {bname} not found")
        f = list(base[bname])
        if len(f) != len(fields):
            raise SystemExit(f"{inst}: {bname} has {len(f)} fields, expected {len(fields)}")
        f[0] = '"' + lab[genre] + '"'
        f[1] = '"' + name + '"'
        for k, v in tw.items():
            if k not in fields:
                raise SystemExit(f"{inst}: unknown field {k}")
            f[fields.index(k)] = num(v)
        g = (gains or {}).get(name)
        if g is not None:
            f[-1] = f"{g:.3f}f"
        f[-1] = "/*gain*/ " + f[-1].replace("/*gain*/", "").strip()
        lines.append("\t{" + ", ".join(f) + "},\n")
    lines.append(END)
    k = text.index("\n};", text.index("static const Preset PRESETS[] = {"))
    text = text[:k + 1] + "".join(lines) + text[k + 1:]
    open(path, "w", encoding="utf-8").write(text)
    return [v[0] for v in SPEC[inst].values()]


def levels(inst):
    """Run the instrument's test; preset name -> level (rms V) as the test prints it."""
    cfg = INST[inst]
    exe = f"/tmp/gp_{inst.lower()}"
    subprocess.run(["g++", "-O2", "-std=c++17", os.path.join(ROOT, cfg["test"]), "-o", exe], check=True, cwd=ROOT)
    out = subprocess.run([exe], capture_output=True, text=True, cwd=ROOT).stdout
    lv = {}
    for ln in out.splitlines():
        m = re.match(r"\s*(?:preset\s+)?\d+\s+(.+?)\s+rms ([0-9.]+) V", ln)
        if m:
            lv[m.group(1).strip()] = float(m.group(2))
    return lv, out


def current_gains(inst):
    text = open(os.path.join(ROOT, INST[inst]["file"]), encoding="utf-8").read()
    return {f[1].strip('"'): float(f[-1].replace("/*gain*/", "").strip().rstrip("f")) for _, _, f in rows(text)}


def calibrate(inst, rounds=4):
    new = build(inst)
    for r in range(rounds):
        lv, out = levels(inst)
        gains = current_gains(inst)
        old = [lv[n] for n in lv if n not in new]
        target = statistics.median(old)
        upd = {}
        for n in new:
            if n not in lv:
                raise SystemExit(f"{inst}: no level for {n}\n{out[-2000:]}")
            g = gains[n] * target / max(lv[n], 1e-3)
            upd[n] = max(0.2, min(6.0, g))
        build(inst, upd)
        wn = max(new, key=lambda n: abs(lv[n] / target - 1))
        print(f"{inst} round {r + 1}: target {target:.2f} V, worst new {wn} {abs(lv[wn] / target - 1) * 100:.1f} %")
    lv, out = levels(inst)
    return out


if __name__ == "__main__":
    what = sys.argv[1] if len(sys.argv) > 1 else "write"
    insts = sys.argv[2:] or list(INST)
    for i in insts:
        if what == "write":
            build(i)
            print("wrote", INST[i]["file"])
        else:
            out = calibrate(i)
            fails = [l for l in out.splitlines() if l.startswith("FAIL")]
            print("\n".join(fails[:20]) or f"{i}: all checks pass")
