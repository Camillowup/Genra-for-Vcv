# GENRA v3 – Generativer 4-Spur-Sequencer für VCV Rack 2

Breite: **64 HP** (GENRA) und **50 HP** (Grid Editor). Projektstand und Entscheidungen: `PROJEKT.md`.

## GENRA Grid Editor (zweites Modul)

![Grid Editor](docs/entwuerfe/genra_grid_piano.png)

Ein zweites Modul im selben Plugin (50 HP) zum vollständigen Bearbeiten des Loops: Drum-Raster, Piano-Roll für Bass, Melodie und Pads, Velocity-Spur.

- **Verbindung:** direkt links oder rechts neben GENRA setzen (LINK-LED leuchtet). Oder im Rechtsklick-Menü *Verbinden mit…* ein beliebiges GENRA im Patch wählen.
- **TRACK:** Spur wählen. **VIEW:** BAR (erster sichtbarer Takt), ZOOM (1/2/4/8 Takte), PITCH (Tonbereich), AUTO SCROLL (Ansicht läuft mit).
- **Drums:** Klick setzt/löscht einen Schlag, Ziehen malt bzw. radiert, Mausrad = Velocity, Rechtsklick = Roll 2–4×, Velocity, Löschen. Klick auf den Stimmennamen wählt die Stimme der Velocity-Spur.
- **Piano-Roll:** Klick setzt eine Note (Länge LEN, Velocity VEL), Ziehen am rechten Rand ändert die Länge, Ziehen der Note verschiebt sie, Doppelklick löscht. Rechtsklick: Akzent, Slide, Velocity. Tonleitertöne sind heller, Akkordtöne blaugrün hinterlegt, die Akkordfolge steht darüber.
- **SNAP:** AUS / SCALE / CHORD für neue und verschobene Noten; mit Shift gedrückt: alle Noten der Spur einrasten.
- **CHORD FOLLOW:** bearbeitete Noten folgen neuen Akkorden (HARMONY NEW): Grundtöne werden zum neuen Grundton, alle anderen Töne wandern in der Tonleiter mit.
- **CHANCE:** Wahrscheinlichkeit neuer Noten (Regler). Jede Note/jeder Schlag kann eine eigene Wahrscheinlichkeit haben und „nur jeden 2. Durchlauf“ spielen (Rechtsklick). Solche Noten sind gestrichelt und zeigen ihren Wert. Die untere Spur schaltet per Klick auf „VEL“/„CHANCE“ um – dort zieht man Velocity bzw. Chance mit der Maus.
- **Mehrfachauswahl:** Shift + Ziehen = Rahmen, Strg + Klick = Note an-/abwählen, Strg+A = alles. Ausgewählte Noten zusammen ziehen; Pfeile ↑↓ = Tonhöhe (Shift: Oktave; Drums: Stimme), ←→ = Position (Shift: ein Takt); Entf = löschen; Strg+C / Strg+V = kopieren / an der Mausposition einfügen; Strg+D = dahinter duplizieren; Esc = Auswahl aufheben. Mausrad und Rechtsklick-Menü wirken auf die ganze Auswahl.
- **EDIT:** UNDO, REDO, COPY/PASTE BAR (erster sichtbarer Takt), CLEAR TRACK, GEN TRACK (Spur wieder generativ).
- **Was „bearbeitet“ heisst:** Sobald du etwas änderst, spielt die Spur feste Noten – genau das, was vorher zu hören war, plus deine Änderungen. DENSITY/COMPLEX wirken dann nicht mehr auf diese Spur, FILL (Fills und Drops) und Humanize schon. MUTATE verändert auch bearbeitete Spuren – LOCK schützt sie vor MUTATE und NEW ALL.

## Neu in v3

- **Genre pro Spur:** Unter den Reglern jeder Spur sitzt ein Genre-Feld. Klick öffnet das Menü: *Global (folgt GENRE)* oder ein eigenes Genre, nach Familien sortiert, mit Varianten. Der Wechsel greift am nächsten Taktanfang (Feld und Display zeigen „…“). Ein eigenes Genre-Feld würfelt die Spur auch bei LOCK neu – die Auswahl ist ein bewusster Befehl. Der globale GENRE-Regler ändert nur Spuren auf *Global*.
- **GENRE-CV pro Spur** (Buchse neben dem Feld): Mit Kabel wählt die Spannung das Genre der Spur, die Variante kommt aus dem Feld (falls gleiches Genre) oder vom VARIANT-Regler. Rechtsklick-Menü → *Genre-CV-Eingänge*: 0–10 V über alle 24 Genres oder 1/12 V pro Genre (Halbtöne – ein Keyboard über MIDI-CV wählt Genres per Taste, C = House).
- **Swing pro Spur:** Jede Spur swingt wie das Genre, das sie spielt (Lo-Fi-Melodie über geradem Techno). SWING verschiebt alle gemeinsam. Gilt auch für den MIDI-Export.
- **Neue CV-Eingänge:** FILL (Trigger: Fill/Drop am Loop-Ende), VARIANT, COMPLEX, COLOR (0–10 V, zum Regler addiert), DENS-CV pro Spur.
- **HARMONY** (früher CHORDS, jetzt blaugrün): NEW, LOCK, TRIG für die gemeinsame Akkordfolge, der Bass, Melodie und Pads folgen.
- **Ein- und Ausgänge (v3.1):** neue CV-Eingänge SWING (0–10 V addiert), BPM (interne Clock, 10 BPM pro Volt), LENGTH. Spalte **OUT**: CLOCK (16tel), RESET (Loop-Anfang), RUN (Gate), BAR (pro Takt), END (letzter Step), FILL (Gate während Fill/Drop), ROOT (Grundton des aktuellen Akkords, V/Oct zwischen C4 und B4), CHORD (ganzer Akkord, polyphon).
- **Szenen (8 Plätze):** Taster-Farbe: dunkel = leer, rot = Pattern gespeichert, grün = aktiv (blinkt bis zum Wechsel). Leerer Platz drücken = aktuellen Loop speichern (alle Spuren, Akkorde, Spur-Genres, Länge). Belegter Platz = abrufen, beim Spielen am nächsten Taktanfang (blinkt bis dahin). Überschreiben: STORE drücken (blinkt), dann den Platz. NEXT-Trigger = nächste belegte Szene, CV 0–10 V = Szene 1–8. Rechtsklick-Menü: *Alle Szenen löschen*. Szenen werden im Patch gespeichert.
- **Display:** Genre jeder Spur neben ihren Lauflichtern, aktive Szene, Akkordnamen mit # oder b passend zur Tonart.
- **Fehlerkorrekturen:** MIDI-Export/-Import mit exakter Länge und Akkordfolge, threadsichere Akkord-Bearbeitung, neues Speicherformat (v2-Patches werden übernommen; Spuren stehen dann auf *Global*).

![GENRA](docs/entwuerfe/genra_v3_entwurf.png)

GENRA erfindet genretypische Loops: **8 Drumspuren, Bass, Melodie und polyphone Pads**, die sich eine gemeinsame Tonart und Akkordfolge teilen. **24 Genres mit 46 Varianten**, alle Regler wirken live. Breite: 44 HP.

| Familie | Genres (Varianten) |
|---|---|
| House | House (Classic, Piano) · Deep House (Stabs, Pads) · Speed Garage (Warp, Organ) · UK Garage (4x4) · 2-Step |
| Techno | Techno (Rolling, Peak, Hypnotic) · Minimal · Acid (Classic, Double, Poly 12) · Electro (Classic, Funk, Detroit) · Trance (Uplifting, Psy) |
| Breaks & Bass | Breakbeat (Classic, Nu-Skool, Big Beat) · Jungle (Classic, Ragga, Dark) · Drum & Bass (Classic, Liquid, Neuro) · Dubstep (Deep, Heavy) · Grime (Eski, Sino) |
| Hip-Hop & Downtempo | Hip-Hop (Boom Bap) · Lo-Fi · Trap · Drill (UK) · Trip-Hop · Downtempo (Chillout, Organic) |
| Weitere | Dub (Roots, Steppers) · Synthwave (Classic, Outrun) · Ambient (Classic, Bright) |

---

## Installation unter Windows (über GitHub)

1. Diese Dateien im Repository ersetzen bzw. hochladen: `src/`, `res/`, `plugin.json`, `Makefile`, `.github/workflows/main.yml` (und optional `README.md`, `docs/`, `tools/`, `test/`).
2. Unter **Actions** startet der Build automatisch (3–5 Minuten).
3. Grünen Lauf öffnen → unten bei **Artifacts** `genra-windows` laden → Zip entpacken.
4. **Nur** `Genra-2.2.0-win-x64.vcvplugin` nach `%LOCALAPPDATA%\Rack2\plugins-win-x64\` kopieren. Ältere `Genra-2.x-…vcvplugin`-Dateien und den Ordner `Genra` dort vorher löschen.
5. Rack neu starten.

Die Versionsnummer liest der Workflow jetzt selbst aus `plugin.json`, für künftige Versionen genügt es, sie dort zu erhöhen.

> Patches aus v1 laden, aber das gespeicherte Pattern wird neu erzeugt (das Format hat sich geändert), und die Genre-Nummern sind neu sortiert.

---

## Neu in v2

- **NEW pro Spur + TRIG-Eingang:** würfelt nur Drums, Bass, Melodie oder Pads neu – auch wenn LOCK an ist (LOCK schützt nur vor NEW ALL, Genrewechsel und MUTATE).
- **Akkord-Bereich:** NEW (neue Akkordfolge, Spuren folgen live), CHORD LOCK, TRIG-Eingang.
- **VARIANT:** Spielart des Genres; Name im Display und beim Darüberfahren. Wechsel greift wie GENRE am nächsten Taktanfang.
- **SLIDE-Ausgänge** (Bass, Melodie): 10 V zu Beginn eines Glides, z. B. für Portamento am VCO oder einen Slew-Limiter. Der Gate bleibt dabei durchgehend offen (303-Stil).
- **Genre-Merkmale:** 2-Takt-Raster (Drill), Hi-Hat-Rolls/Triolen, Break-Chops (Jungle, D&B, Breakbeat), Drops und Breaks am Loop-Ende (Speed Garage, Jungle, Grime), Humanize (Lo-Fi, Hip-Hop, Trip-Hop), Polymeter-Bass (Acid Poly 12), zweite 303-Linie (Acid), genre-eigene Fills.

**Welcher Regler steuert was?**
- **COMPLEX** → Synkopen, Hi-Hat-Rolls/Triolen, wie wild Breaks gechoppt werden, Slides der Melodie
- **MOVE** → Bassbewegung und Bass-Slides
- **FILL** → Wahrscheinlichkeit für Fill **und** Drop am Loop-Ende
- **DENSITY** (Pads) → bei Grime sind Pads standardmässig aus; über der Mitte kommen sie dazu

---

## Bedienung (alle Elemente)

**Global:** GENRE, BPM (interne Clock), KEY, SCALE (Auto = Genre), VARIANT, COMPLEX, COLOR (Harmonik), MUTATE, SWING (relativ zum Genre jeder Spur), LENGTH (1/2/4/8 Takte), NEW ALL, RUN.
**Harmony:** NEW, LOCK, TRIG.
**Pro Spur:** DENSITY + DENS-CV · zweiter Regler (Drums: FILL, GHOST; Bass: MOVE; Melodie: RANGE; Pads: VOICING) · OCT (Bass/Melodie/Pads) · Genre-Feld + GENRE-CV · MUTE · LOCK · NEW · TRIG.
**Szenen:** 8 Plätze, STORE, CV, NEXT.
**Eingänge:** CLOCK, RESET, RUN, NEW, FILL, MUTATE, GENRE, VARIANT, TRANSP (V/Oct), COMPLEX, COLOR, CHORD (poly) · pro Spur GENRE-CV, DENS-CV, TRIG · Szenen CV, NEXT · HARMONY TRIG.
**Ausgänge:** 8 Drum-Trigger (Höhe = Velocity) · Bass/Melodie: V/OCT, GATE, VEL, ACC, SLIDE · Pads: V/OCT + GATE (poly 4), VEL, MOD.

**Display:** Akkord anklicken = nächste Alternative, Rechtsklick = Liste, Mausrad = mehr/weniger Farbe.
**Rechtsklick-Menü:** MIDI-Export/-Import, Akkord-Modus, Akkord-Vorlage, nur Akkorde neu würfeln, Genre-CV-Modus, alle Spuren auf Global, alle Szenen löschen, Clock-Auflösung.

---

## Projektstruktur
```
src/Engine.hpp/.cpp   Theorie, Akkorde, Generierung, Auswertung pro Step
src/Genres.cpp        24 Genres / 46 Profile (Raster in derselben Notation wie die Spezifikation)
src/Midi.cpp          MIDI-Export/-Import
src/Store.cpp         Speicherformat (pattern3) + Übernahme von v2-Patches
src/Edit.cpp          Bearbeiten: Einfrieren, Noten schreiben, Chord Follow, Snap, Copy/Paste
src/GenraModule.hpp   GENRA-Moduldefinition (auch vom Editor genutzt)
src/GenraGrid.cpp     Grid-Editor-Modul
res/GenraGrid.svg     Frontplatte des Editors (tools/grid_panel.py real)
src/Genra.cpp         Rack-Modul, Display, Menüs
res/Genra.svg         Frontplatte (tools/panel.py real), Vorschau: tools/panel.py preview <datei.svg>
test/                 Engine- und MIDI-Tests (g++, ohne Rack)
```
