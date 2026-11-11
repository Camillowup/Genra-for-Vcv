# GENRA – Projektstand

**Stand:** 10. November 2026 · **Plugin 2.31.1: 24 Module, Windows + macOS (arm64/x64, ungetestet)** (GENRA, GRID, ABLAUF, GENRA BOX, ABLAUF MINI, SCHLAG, VOICE, DRUM-X, ANIMAL, MONK, GENRA MONO, FUNKE, GOLANA, GOLANA FX, GRUND, GRUND FX, SCHMELZ, MIX, GENRA FX, K-WASUB, K-WASUB FX, UMLAUF, UMLAUF FX, STOTTER) · Repo: `Camillowup/Genra-for-Vcv` (vorher `CamilloGF/genra-v3`, Account nicht mehr erreichbar)

> Design-Leitfaden für neue Module (Aussehen, GENRA-Leiste, Leuchtsegmente, Follow, englische UI): **docs/DESIGN.md**

Vorgänger: v1 `CamilloGF/vcv-sequenzer-Genra` (2.0.0), v2 `CamilloGF/Genra-v2` (2.1.0, läuft in Rack)
Plattform: VCV Rack Pro 2.6.6 unter Windows · Build: GitHub Actions (`.github/workflows/main.yml`)

## Konzept
Generativer, genreabhängiger 4-Spur-Sequencer für VCV Rack 2. Erfindet Loops (1/2/4/8 Takte) für **8 Drumstimmen, Bass, Melodie und polyphone Pads (4 Stimmen)**. Alle Spuren teilen Tonart und Akkordfolge.

**Genres:** v1 14 · v2 24 (46 Varianten) · v3 37 (83 Varianten) · 2.24 59 (127 Varianten) · 2.30 64 (138 Varianten), Liste im README

## Frontplatte (42 HP)
- **Display:** Genre, Tonart/Tonleiter, BPM, Swing, Akkordfolge (klick-/scroll-editierbar), Step-Lauflichter der 4 Spuren
- **Global:** GENRE, KEY, SCALE (Auto + 12 Tonleitern, ab 2.30), BPM, SWING, COMPLEX (Rhythmus), COLOR (Harmonik), MUTATE, LENGTH · Taster NEW, RUN, CHORD LOCK
- **Drums:** DENSITY, FILL, GHOST, MUTE, LOCK · Ausgänge KICK, SNARE, CLAP, RIM, C.HAT, O.HAT, TOM, PERC (Triggerhöhe = Velocity)
- **Bass:** DENSITY, MOVE, OCT, MUTE, LOCK · V/OCT, GATE, VEL, ACC
- **Melodie:** DENSITY, RANGE, OCT, MUTE, LOCK · V/OCT, GATE, VEL, ACC
- **Pads:** DENSITY, VOICING, OCT, MUTE, LOCK · V/OCT + GATE (poly 4), VEL, MOD
- **Eingänge:** CLOCK, RESET, RUN, NEW, GENRE CV, MUTATE CV, TRANSP (V/Oct), CHORD (poly)
- **Rechtsklick-Menü:** MIDI-Export/-Import, Akkord-Modus (gemischt/Vorlagen/frei), feste Akkordvorlage, nur Akkorde neu würfeln, Clock-Auflösung (16tel/8tel/Viertel/24 PPQN)

## STOTTER (Performance-FX, ab 2.8.0)
DJ-Effektpad nach Polyend Play (Perform) und Akai Flex Beat: 16 Pads in 4 Bänken, Reihen STUTTER · TIME · GATE · FX in den GENRA-Spurfarben, Modi LOOP / ONE SHOT / HOLD (+ LATCH), Start auf dem Raster (QUANT), jedes Pad = 1-Takt-Muster.
- Effekte: Stutter, Halftime, Reverse, Tape Stop, Scratch, Gate, Trance, Pump, Echo Out, Freeze, Bitcrush, Rearrange – je 3 Parameter P1–P3, mit < > umbelegbar, INIT
- EQ wie DJ-Isolator (LR4, −∞ … +6 dB, Kills, FREQ verschiebt die Mitte, PRE/POST), DJ-Filter (LP links/HP rechts), Drive, Intensity, Mix
- REC 1 BAR (Loop statt Eingang), TAP, CLOCK/RESET wie GENRA, PADS (poly Gates), KILLS (poly), MIDI-Noten C1–D#2 mit MIDI learn
- Farben (2.8.1): Jede Spur hat die Farbe ihrer Instrumente – DRUMS blau-violett `#7b6cff` (= DRUM-X), BASS säuregrün `#b8f23a` (= K-WASUB und MONO), MELODY cyan `#3dc9ff` (= FUNKE), PADS rosa `#e0779e` (= SCHMELZ). Gilt auch für GRID, MIX und die STOTTER-Pads.
- K-WASUB (2.8.0): PRESET-Feld statt ◀ ▶, Liste nach Stil sortiert (Kern-API 2: `preset_style`)
- Dateien: `src/stotter/StotterDsp.hpp` (Engine, Test `test/test_stotter.cpp`), `src/stotter/Stotter.cpp`, `tools/stotter_panel.py` → `res/Stotter.svg` + `src/stotter/StotterLayout.hpp` (30 HP)

## Wichtige Designentscheidungen
1. **Zufallszahlen statt fertiger Noten:** Jeder Step speichert Zufallswerte + abstrakte Tonhöhe (Akkordton/Stufe). Ob/welche Note klingt, wird beim Abspielen berechnet → fast alle Regler wirken live (DENSITY, KEY, SCALE, COLOR, VOICING, OCT, RANGE, MOVE, COMPLEX).
2. **Genre-Profile als Wahrscheinlichkeitsraster** (16 Zeichen/Takt, `.`=nie, `1`–`9`) in `src/Genres.cpp`.
3. **Akkorde:** Genre-Vorlagen (römische Stufen) + freie Erzeugung (Funktionsharmonik-Übergangstabelle × Genre-Gewichte). COLOR steuert live 7er/9er, Zwischendominanten, Tritonus-Substitution (jazzy Genres) und geborgte Akkorde.
4. **Bass akkordrelativ** (folgt CHORD-Eingang), an Kick gekoppelt je Genre (folgen/meiden).
5. **Melodie:** Motiv (1 oder 2 Takte) → Phrase A A A' / M M', schwere Zählzeiten rasten auf Akkordtöne; Trance = Arpeggio.
6. **Pads:** Voice Leading (minimale Bewegung), Stile Sustain/Stabs/Skank.
7. **LOCK:** jede Spur merkt sich ihr eigenes Genre; Genrewechsel greift am nächsten Taktanfang.
8. **MIDI:** Export Typ 1 (Drums Kanal 10/GM, Bass 1, Melodie 2, Pads 3, Swing inklusive); Import spielt notengenau ab (Spur „fixed“), Akkorderkennung mit Bass als Grundton-Hinweis.
9. **Pattern-Speicherung:** gesamtes Pattern als Base64 im Patch (`dataToJson`).

## Code-Struktur
| Datei | Inhalt |
|---|---|
| `src/Engine.hpp/.cpp` | Theorie, Akkorde, Generierung (`generate`, `mutate`), Auswertung pro Step (`drumHit`, `bassAt`, `melAt`, `padAt`) – rein C++, ohne Rack testbar |
| `src/Genres.cpp` | Genre-Profile (64 Genres, 138 Varianten), Tonleitern, Genre-Gewohnheiten (`applyFeel`) |
| `src/Midi.cpp` | SMF-Export/-Import |
| `src/GenraModule.hpp` / `src/Genra.cpp` | Rack-Modul GENRA: Clock/Swing, Ausgänge, Display, Menüs, Speichern |
| `src/Store.cpp` | Speicherformat `pattern3`, Übernahme von v2-Patches |
| `src/Edit.cpp` | Bearbeiten: Einfrieren, Noten schreiben, Chord Follow, Snap, Copy/Paste |
| `src/GenraGrid.cpp` | Grid-Editor-Modul (`tools/grid_panel.py` erzeugt Frontplatte + `src/LayoutGrid.hpp`) |
| `src/jiant/` | JIANT-Stimmen VOICE, DRUM-X, ANIMAL: je ein Modul (`Voice.cpp`, `DrumX.cpp`, `Animal.cpp`, jedes in eigenem Namespace), ein Kern-Lader (`*Core.cpp`: lädt pro Modul-Instanz eine eigene Kopie der Kern-Bibliothek nach `Genra/run/` im Rack-Benutzerordner) und der gemeinsame Look `JiantUI.hpp` |
| `jiantcore/` | die Firmware-Kerne als C-Bibliotheken (`res/voicecore.*`, `drumcore.*`, `animalcore.*`), gebaut vom Makefile aus `jiant.tar.gz` |
| `tools/jiant_panel.py` | erzeugt `res/JiantVoice.svg`, `res/JiantDrumX.svg`, `res/JiantAnimal.svg` + `src/jiant/JiantLayout.hpp` (`python3 tools/jiant_panel.py real`), Vorschau mit `preview <ordner> <Rack-Quellen>` |
| `src/mono/` | GENRA MONO: Modul `GenraMono.cpp` (Filter-Dropdown, säuregrüne Knopfzeiger) und Klang-Engine `MonoDsp.hpp` (rein C++, Test `test/test_mono.cpp`) |
| `tools/mono_panel.py` | erzeugt `res/GenraMono.svg` + `src/mono/MonoLayout.hpp` (`python3 tools/mono_panel.py real`), Vorschau mit `preview <datei.png> <Rack-Quellen> [menu]` |
| `tools/panel.py` | erzeugt `res/Genra.svg` + `src/Layout.hpp` (`python3 tools/panel.py real`) und Vorschau (`preview`) |
| `test/` | Engine- und MIDI-Rundlauf-Tests (g++, ohne Rack) |

## Build
- **GitHub Actions:** MSYS2 **MINGW64** (nicht UCRT – CRT muss zu Rack passen), Rack SDK latest, `make RACK_DIR=sdk/Rack-SDK SLUG=Genra VERSION=2.0.0 dist` (SLUG/VERSION direkt übergeben, weil `mingw-w64-x86_64-jq` nicht mehr existiert). Artifact `genra-windows` → `.vcvplugin` direkt nach `%LOCALAPPDATA%\Rack2\plugins-win-x64\`.
- **Neue Version:** `version` in `plugin.json` **und** `VERSION=` im Workflow erhöhen (Hauptversion muss 2 bleiben, z. B. 2.1.0).

## Offene Punkte aus Version 1
- [x] v1 läuft in Rack auf dem PC.
- [ ] Klangtest aller Genres im echten Patch, Feinschliff der Genre-Raster nach Gehör.

## Version 2 (2.1.0) – Stand 4. November 2026
- Spezifikation: Claude-Doc „GENRA v2 – Genre-Spezifikation“ (Entscheidungen am Ende).
- 44 HP; 24 Genres in Familien-Reihenfolge, 46 Profile (Varianten über VARIANT-Regler).
- NEW + TRIG pro Spur (wirkt auch bei LOCK), Akkord-NEW + TRIG, SLIDE-Ausgänge Bass/Melodie.
- Engine: 32-Step-Raster, Roll-Positionen ('r'), Break-Chops, Drops (an FILL gekoppelt), Genre-Fills, Humanize, Polymeter-Bass, MS_ACID-Melodie, padBase (Grime-Pads aus).
- COMPLEX = Rolls/Chops/Melodie-Slides, MOVE = Bass-Slides, FILL = Fill + Drop.
- Workflow liest Version aus plugin.json.
- Status: läuft in Rack.

## Version 3 (2.2.0) – in Arbeit
**Schwerpunkt:** Genre pro Spur und CV-Eingänge. Die Überarbeitung der Genres selbst folgt in einer späteren Version.

**Frontplatte (58 HP):** erzeugt von `tools/panel.py` (`real` → `res/Genra.svg` + `src/Layout.hpp`, `preview <datei.svg>` → Vorschau, braucht `../ext/Rack/res/ComponentLibrary`).
- Globale Reihe: GENRE · BPM · KEY · SCALE · VARIANT · COMPLEX · COLOR · MUTATE · SWING · LENGTH · NEW ALL · RUN
- HARMONY (dunkles Blaugrün, früher CHORDS): NEW, LOCK, TRIG – gemeinsame Akkordfolge für Bass, Melodie, Pads
- Pro Spur: Genre-Feld mit Menü (Global / Genre · Variante), GENRE-CV, DENS-CV, MUTE, LOCK, NEW, TRIG
- Eingänge (12): CLOCK, RESET, RUN, NEW, FILL, MUTATE, GENRE, VARIANT, TRANSP, COMPLEX, COLOR, CHORD
- SCENES: 8 Plätze, STORE, CV, NEXT

**Erledigt – Fehlerkorrekturen**
- [x] MIDI-Export: Noten enden spätestens am Loop-Ende; Track-Ende = Loop-Ende
- [x] MIDI-Import: swingfeste Step-Rundung, Loop-Länge wächst nicht mehr (Ratchets auf geswingtem letztem Step)
- [x] MIDI-Rundlauf: Akkordfolge als GENRA-Metadaten (Meta-Event FF 7F „GNRA“); wird nur genutzt, wenn die Pad-Noten der Datei dazu passen
- [x] Akkord- und Tonartnamen mit # oder b passend zur Tonart (`sharpKey`, `noteName`)
- [x] Akkord-Bearbeitung im Display über eine Warteschlange an den Audio-Thread (keine gleichzeitigen Schreibzugriffe)
- [x] Speicherformat `pattern3` (`src/Store.cpp`): Datensätze mit Tags, Felder pro Step mit Anzahl → künftige Erweiterungen brechen alte Patches nicht. v2-Patches (`pattern`, Binärblock Version 2) werden übernommen.
- [x] Tests: `test/test_store.cpp` neu, `test_midi` prüft auch Dateien ohne Metadaten

**Erledigt – Genre pro Spur & CV (Stand 5. November 2026)**
- [x] Genre-Feld pro Spur (Widget `GenreField`, Menü nach Familien → Genre → Variante); Wahl in `trackSel[t]` (-1 = Global), gespeichert als `trackGenre` im Patch
- [x] Regeln: Spur-Genre (Feld oder CV) wechselt am Taktanfang und würfelt auch bei LOCK neu; globaler GENRE-Wechsel betrifft nur ungesperrte Spuren auf Global; NEW ALL würfelt jede ungesperrte Spur in ihrem Genre; Akkorde kommen immer vom globalen Genre
- [x] GENRE-CV pro Spur (CV > Feld > Global), Modus 0–10 V oder 1/12 V pro Genre (gilt auch für globalen GENRE-CV)
- [x] CV: VARIANT, COMPLEX, COLOR, DENS pro Spur (0–10 V addiert), FILL-Trigger (im letzten Viertel: gilt für den nächsten Durchlauf)
- [x] Swing pro Spur: Steps laufen ungeswingt, jede Spur spielt ihre Events mit eigener Swing-Verzögerung (Modul und MIDI-Export)
- [x] Szenen: 8 Plätze (Pattern, Spur-Genres, Länge), leer = speichern, belegt = abrufen am Taktanfang, STORE zum Überschreiben, NEXT, CV; im Patch gespeichert
- [x] Display: Spur-Genres an den Lauflichtern, Szene, HARMONY-Farbe
- [x] Tests: `test/test_tracks.cpp` (300 Zufallskombinationen Spur-Genres/Swing/Länge: Erzeugung, MIDI, Speichern)

**Erledigt – GENRA Grid Editor (Stand 5. November 2026)**
- [x] Zweites Modul `GenraGrid` (46 HP), Frontplatte aus `tools/grid_panel.py` (`real` → `res/GenraGrid.svg` + `src/LayoutGrid.hpp`, `preview <datei> piano|drums`)
- [x] Verbindung: angedocktes GENRA links/rechts (Expander) oder per Menü gewählte Modul-ID (`linkedId`)
- [x] Bearbeiten im UI-Thread auf einer Kopie; fertige Spur per `Genra::submitTrack()` → Audio-Thread übernimmt sie mit `copyTrack()`
- [x] Erste Änderung friert die Spur ein (`freezeTrack`: klingt identisch – Test für alle 46 Profile × 4 Spuren); Fills/Drops/Humanize bleiben live
- [x] Feste Noten können jetzt Slide, Akzent, Gate, Roll (Drums) und Akkordbezug (`froot`) speichern; Speicherformat erweitert (ältere Daten laden weiter)
- [x] CHORD FOLLOW (`chordFollow[t]`): Grundton → neuer Grundton, sonst diatonische Verschiebung
- [x] SNAP (Tonleiter/Akkord), UNDO/REDO (64 Schritte), COPY/PASTE BAR, CLEAR, GEN
- [x] Tests: `test/test_edit.cpp`

**Erledigt – v3.1 (Stand 5. November 2026)**
- [x] GENRA 64 HP: SCENES neben IN, OUT-Spalte rechts aussen; Eingänge SWING, BPM, LENGTH; Ausgänge CLOCK, RESET, RUN, BAR, END, FILL, ROOT, CHORD (poly)
- [x] Szenen-LEDs zweifarbig: rot = gespeichert, grün = aktiv
- [x] CHANCE + „jeden 2. Durchlauf“ für bearbeitete Noten/Schläge (`fchance`, `fcond`); Durchlaufzähler `Params::pass`, deterministisch pro Durchlauf (`chancePlays`); Editor zeigt alle Noten (`ignoreChance`)
- [x] Grid Editor 50 HP: CHANCE-Regler, untere Spur VEL/CHANCE, Mehrfachauswahl (Rahmen, Strg-Klick, Strg+A), Auswahl verschieben/transponieren/löschen/kopieren/duplizieren per Maus und Tastatur
- [x] Entschieden: kein direkter MIDI-Ausgang (MIDI-Export reicht)

**Als Nächstes**
- [x] 13 neue Genres (Stufe 1, mit vorhandenen Engine-Mitteln), hinten angehängt (Genre 24–36, Profile 46–82), Familien Afro / Techno Styles / Latin & Club; GENRE-CV 0–10 V verteilt jetzt alle 37 Genres: Afrobeat, Afrobeats, Amapiano, Afro House · Dub Techno, Melodic Techno, Hard/Industrial Techno, Tech House, Detroit Techno · Baile Funk, Reggaeton, Footwork, Breakcore
- [ ] Genre-Stufe 2 (Engine): Pad-Echo (Dub Techno), Melodie-Stil CHOP, Kick-Fill, Log-Drum-Bass, Bass folgt Tom-Spur, Gabber-Kick-Layer
- [x] Länge pro Spur/Drum-Stimme (Polymeter) und Mikro-Timing (Nudge): 2.23.0
- [ ] Später: Seed merken
- **2.11.0:** Szenen entfernt (8 Taster, STORE, SCENE CV, NEXT), Panel 58 HP. Parameter/Lichter waren am Listenende und sind ganz weg; die beiden Eingänge bleiben als `UNUSED_SCENE_INPUT`/`UNUSED_NEXT_INPUT` in der Liste, damit SWING, BPM und LENGTH ihre Ids behalten. Gespeicherte Szenen im Patch werden ignoriert. Nachfolger: eigenes Song-Modul (Arbeitsname ABLAUF, Konzept offen).
- [ ] Ganz am Schluss: alle Genres nach Gehör feinschleifen

**Offen**
- [ ] Test in Rack (v3.1: neue Ausgänge, Szenen-Farben, CHANCE, Mehrfachauswahl) (Genre-Feld-Menü, CV-Eingänge, v2-Patch laden)
- [ ] Test in Rack (2.23: Endmarken, Nudge-Wiedergabe mit Swing, Spurwerkzeuge, REC MIDI mit echtem Keyboard, Save to pad)
- [ ] Genres überarbeiten (eigene spätere Version), Genres aus MIDI lernen

**Tests (ohne Rack):**
```
g++ -std=c++11 -O2 -Isrc src/Engine.cpp src/Genres.cpp src/Midi.cpp src/Store.cpp src/Edit.cpp test/test_edit.cpp -o t && ./t
```
(ebenso `test_store.cpp`, `test_midi.cpp`, `test_tracks.cpp`, `test_engine.cpp`)
GENRA MONO: `g++ -std=c++17 -O2 test/test_mono.cpp -o tm && ./tm`

## JIANT-Stimmen (2.3.0) – Stand 5. November 2026
- VOICE, DRUM-X und ANIMAL aus den Einzel-Repos (*Voice-from-Jiant-for-Vcv*, *Drum-X-from-jiant-for-VCV*, *Animal-Engine-from-jiant-for-vcv*) ins GENRA-Plugin übernommen. Die Firmware (`jiant.tar.gz`) war in allen dreien identisch und liegt jetzt nur einmal hier.
- Look wie GENRA: SVG-Frontplatte, Rack-Regler und -Buchsen, Display mit Nunito/ShareTechMono. Jedes Modul einfarbig: VOICE rot `#ff4f5a`, DRUM-X lila `#a98bff`, ANIMAL grün `#4fe08a` (in `tools/jiant_panel.py` und `src/jiant/JiantUI.hpp`).
- Der Screen mit der Zeichnung der Firmware bleibt, etwas kleiner als vorher (links im Display).
- Neu: CALL-Taster mit Licht (VOICE, ANIMAL), MODE bzw. LANG zusätzlich als Chips im Display anklickbar, Spur-Chips im DRUM-X-Display (leuchten beim Schlag, Klick wählt die Spur).
- Plugin-Lizenz jetzt GPL-3.0-only (die JIANT-Teile sind GPL-3.0-only). Patches mit den alten Einzel-Plugins müssen die Module neu einsetzen.

## MONK (2.4.0) – Stand 5. November 2026
- Sechstes Modul: der Vokal-Synth aus der FM-1-Firmware MONK-1 v0.9.1 (`charlesvestal/fm1-monk`, GPL-3.0; Klang MonkSynth, MIT). 26 HP, Farbe safran `#ffb43d` (in `tools/monk_panel.py` und `src/monk/Monk.cpp`).
- `src/monk/MonkDsp.hpp`: `firmware/src/dsp/monk.c` als Klasse (eine pro Modul), für jede Samplerate, Echo in voller Rate, neuer Parameter FEEDBACK, Noten über GATE/V-OCT statt Tasten-Stack. Gegen das Original geprüft: bei 44,1 kHz ohne Echo Abweichung ~0,04 % RMS (Rundung).
- `src/monk/monkface.c` + `src/monk/fw/`: der Zeichencode der Firmware (`app/face.c`, `app/inflate.c`, `dsp/fastmath.h`, unverändert bis auf den Include-Pfad) und die Sänger-Grafiken (`art.h`, erzeugt mit `tools/gen_art.py` aus fm1-monk). Rendert 240×240 RGB565, angezeigt über `jiant::Canvas565`.
- Frontplatte: `python3 tools/monk_panel.py real` → `res/Monk.svg` + `src/monk/MonkLayout.hpp`; Vorschau mit `preview <ordner> [Rack-Quellen]`.
- Offen: Klangtest in Rack (Windows-Build über Actions).

## GENRA MONO (2.5.0) – Stand 5. November 2026
- Monophoner Bass-Synth nach dem Moog Minitaur als siebtes Modul (Branch `mono`, aufbauend auf `monk`). 28 HP, Farbe säuregrün `#b8f23a` wie K-WASUB (bis 2.8.0 türkis `#1fb3a3`) (in `tools/mono_panel.py` und `src/mono/GenraMono.cpp`). Look wie MONK/JIANT: dunkle Kopfleiste mit Titel MONO und Farbbalken, zentrierte Sektionstitel, getönte Ausgänge, normale Rack-Knöpfe. Pro VCO WAVE, TUNE (±12 Halbtöne, fein um die Mitte), LEVEL; GLIDE.
- Engine (`src/mono/MonoDsp.hpp`): 2 BLEP-Oszillatoren mit Wellenform-Morph, Mixer → DRIVE → Filter, 2-fache Überabtastung, 8 Filtermodelle (ZDF-Ladder mit Bassausgleich, Ladder 12, Diode/303, MS-20 mit sättigender Resonanz, SEM-SVF LP/BP/HP, OTA), 2 ADSR mit RC-Kurven, LFO TRI/SQR, VCA mit Sättigung, DC-Filter.
- Filtertyp als Parameter `FTYPE` (speicherbar, Undo), Auswahl über das Feld (Klick = Liste, Mausrad) oder Rechtsklick-Menü.
- Test `test/test_mono.cpp`: Tonhöhe, TUNE, alle Filter bei voller Resonanz/Drive stabil, Selbstoszillation (Ladder, Diode, MS-20; SEM nicht), Bassausgleich, Hüllkurvenzeiten.
- [ ] Klangtest in Rack, Feinschliff nach Gehör (Lautstärke-Ausgleich zwischen den Filtertypen, Kurven von DRIVE und EG AMT).
- **Presets (2.9.0):** PRESET-Feld im Kopf (`PFIELD` aus `tools/mono_panel.py`, Klick = Liste mit Untermenü pro Stil, Mausrad), 23 Werks-Presets in `src/mono/MonoPresets.hpp` (Werte in ParamId-Reihenfolge, Liste muss zu `PRESETS` in `tools/mono_panel.py` passen), Laden mit Undo, Punkt bei geänderten Reglern (Vergleich mit den geladenen Werten, im Patch gespeichert). User-Presets als JSON in `Rack2/Genra/MonoPresets/` (Rechtsklick → Save preset as …, gleicher Name ersetzt). Test spielt eine Basslinie durch jedes Preset (klingt, bleibt unter 6 V, klingt aus). Vorschau mit Liste: `python3 tools/mono_panel.py preview out.png <Rack> presets`.
- [ ] Presets in Rack anhören, nach Gehör nachstellen.
- **2.10.0:** Eingänge ACC und SLIDE (Reihenfolge wie GENRAs Bass-Ausgänge), Regler ACCENT (unter GLIDE), Stereo-Ausgang L/R mit WIDTH (Bereich STEREO; zweites Filter nur bei WIDTH > 0, Seite ab 150 Hz 24 dB/Okt. hochpassgefiltert, Chorus der Höhen auf der Seite), LFO-Ausgang entfernt (Ausgangs-Id 0 bleibt unbenutzt, damit Kabel auf OUT jetzt auf L landen), LFO-Eingänge RETRIG und SYNC (Clock → Taktlänge, RATE wählt aus 17 Notenwerten `DIV_BEATS`, Clock-Auflösung im Menü, nach 2 s ohne Clock frei). Neue Parameter hinten angehängt (ACCENT, WIDTH), Presets haben 27 Werte, ältere User-Presets mit 25 Werten laden mit Standardwerten. Test: ACC, SLIDE, Stereo (WIDTH 0 identisch, tiefe Seite < −25 dB), SYNC 1/4 bei 120 BPM, RETRIG.

## FUNKE (2.7.0) – Stand 6. November 2026
- Melodiestimme für GENRAs Melodiespur, 30 HP, Akzent cyan `#3dc9ff` (in `tools/funke_panel.py` und
  `src/funke/Funke.cpp`). Set: SCHMELZ Pads, K-WASUB oder GENRA MONO Bass, DRUM-X Drums, FUNKE Melodie.
- Engine `src/funke/FunkeDsp.hpp` (2-fach überabgetastet wie MONO, nutzt Filter/Hüllkurven aus `MonoDsp.hpp` und den
  Reverb aus `SchmelzDsp.hpp`): 8 Modelle, Modell wird pro Note festgehalten; Stimmverteilung MONO (neueste Note),
  LEGATO (kein neuer Anschlag bei gehaltenen Noten), POLY (8 Stimmen, leiseste/älteste wird geklaut). Glide läuft
  nach GENRAs kurzem SLIDE-Impuls bis zum Ziel weiter.
- Bus: LO-FI → WIDTH → CHORUS → ECHO (Pingpong, Zeit gleitet bei Tempowechsel) → VERB, DC-Filter, Soft-Limit ±5 V.
  ≈ 1 % eines Kerns pro Stimme.
- Test `test/test_funke.cpp`: Stimmung aller Modelle bei 3 Tonhöhen (±6 Cent), Pegel (1,2–1,6 V RMS), TIMBRE/BITE
  wirken bei allen Modellen, Glide auf SLIDE, POLY/MONO/LEGATO, Echozeit, Stabilität aller Modelle × Filter bei
  44,1–192 kHz. Mit Ordnerargument schreibt er eine Demo-Phrase pro Modell als WAV.
- [ ] Klangtest in Rack, Feinschliff nach Gehör.

## GENRA SYNTHIS übernommen (2.6.0) – Stand 6. November 2026
- SCHMELZ, MIX, K-WASUB, K-WASUB FX, UMLAUF und UMLAUF FX aus `CamilloGF/vcv-synthis` (Stand 958ee81) übernommen,
  mit Panels-Generatoren, Tests, `kwcore/` + `kwasub/` (K-WASUB-Kern, eigene DLL wie die JIANT-Kerne) und `vader/`
  (in plugin.dll gelinkt). Modul-Slugs unverändert; nur der Plugin-Slug ist jetzt `Genra`.
- Benutzerordner: K-WASUB-Kopien unter `Genra/run/kwcore`; UMLAUF-Formen/-Samples unter `Genra/Umlauf` (ein schon
  vorhandener Ordner `GenraSynthis/Umlauf` wird weiter benutzt, solange es den neuen nicht gibt).
- Build: Tests von MONO, SCHMELZ, MIX, K-WASUB-Kern und VADER laufen im Workflow; geprüft wird auch, dass plugin.dll
  keine MinGW-Laufzeit (winpthread, libstdc++) nachlädt.
- STRANG wurde verworfen (Branch und PR #2 gelöscht bzw. geschlossen).

### SCHMELZ
- String Machine nach dem Vorbild Waldorf Streichfett, 32 HP, Akzent rosa `#e0779e` (in `tools/schmelz_panel.py` und
  `src/schmelz/Schmelz.cpp`).
- Engine `src/schmelz/SchmelzDsp.hpp`: Strings pro Stimme Säge, Puls, Sub, Oktave hoch (PolyBLEP), Registration als
  Tabelle (Mischung, Tief-/Hochpass, Vokal-Formant, Verstimmung, Brass-Filterhub) mit linearer/logarithmischer
  Überblendung; OCT BASE/BOTH/8VA; Ensemble (3 Abgriffe, LFO 0,63 Hz + 5,7 Hz, 120°), Chorus, 6-stufiger Phaser,
  ANIMATE. Solo: FM-Sinus + Säge/Puls/Sub durch resonanten SVF mit Filterhüllkurve, 5 Klänge überblendet, ADSR,
  Tremolo als Stereo-Autopan. Reverb: 4 Kamm- + 2 Allpassfilter pro Kanal. DC-Filter, Soft-Limit ±10 V.
  16 + 16 Stimmen mit allen Effekten ≈ 6 % eines Kerns.
- Solo ohne eigenes GATE: spielt die Strings-Noten, SPLIT (Layer / unter C4 / ab C4) wird beim Anschlag pro Note
  festgelegt. SUSTAIN-Taster hält die Solo-Noten bis zum Ausschalten.
- Test `test/test_schmelz.cpp`: Stimmung aller 8 Registrationen × 3 Oktavlagen und aller 5 Solo-Klänge (±3 Cent),
  8VA ohne Grundton, Crescendo-/Release-Zeiten, Split, Pegel (Akkord ≈ 1,5 V RMS), Stabilität der ganzen Kette bei
  44,1/96/192 kHz und Extremwerten.
- [ ] Klangtest in Rack, Feinschliff nach Gehör (Registrationen, Pegel zwischen den Klängen, Ensemble-Tiefe).

### MIX
- 4-Kanal-Stereomixer, 28 HP (ab 2.10.0, vorher 32 HP), Modul-Slug `GenraMix`. Kanäle in den GENRA-Spurfarben, Master dunkel (Balken
  `#4a4c57`), Ausgänge grün getönt wie die Pegelanzeigen. Panel: `tools/mix_panel.py` → `res/Mix.svg` +
  `src/mix/MixLayout.hpp`.
- Engine `src/mix/MixDsp.hpp`: RBJ-Kuhschwänze 120 Hz / 6 kHz (±12 dB, übersprungen wenn flach), Balance (Gegenseite
  fällt mit Viertelsinus), Pegelkurve quadratisch (0,75 = 0 dB), Gain-Glättung ~10 ms (Mute/Solo klickfrei), Send nach
  dem Fader – zwei Sends A/B mit je eigenem Return (Send B, Return B und ihre Buchsen hinten an die
  Parameter-/Port-Listen gehängt, alte Patches laden unverändert), Ducking: Trigger ≥ 1 V startet eine Kosinus-Erholung über TIME (Tiefe max. −26 dB), Width über M/S,
  Drive mit Padé-Sättigung und Lautheitsausgleich, Limiter ab 8 V weich gegen 10 V, Spitzenpegel-Anzeigen
  (12 Segmente, −36 … +3 dB, 5 V = 0 dB).
- Test `test/test_mix.cpp`: Einheitsverstärkung, Pegelkurve, EQ ±12 dB, Balance, Mute/Solo, Send/Return A und B, Ducking,
  Width 0 = Mono, alles voll aufgedreht begrenzt, Anzeige.
- [ ] Test in Rack mit GENRA (Ducking-Gefühl, EQ-Frequenzen nach Gehör).

### K-WASUB + K-WASUB FX
- Port der FM-1-Firmware K-WASUB 1.0.52 (Omnipotard/Guizmoh, GPL-3.0, github.com/Guizmoh/K-WASUB_Firmware_for_FM1).
  Quellen unverändert in `kwasub/` (firmware/src, gen, hal, web/wasm_host.c). 40 HP je Modul, Akzent `#b8f23a`.
- Kern: `kwcore/kwcore.c` übersetzt die Firmware wie für ihren Browser-Emulator (`KAWA_WASM`) und legt eine
  Schnittstelle darüber (`kw_get`: render 256 Frames, MIDI, Patch-Parameter mit Bereich/Name/Text der Firmware,
  Presets, EQ, Tempo, Sync, Hüllkurve, LFO). Gebaut vom Makefile als `res/kwcore.{dll,so,dylib}`, pro Instanz eine
  Kopie nach `GenraSynthis/run/kwcore` im Rack-Benutzerordner (wie die JIANT-Kerne in genra-v3). ≈ 1 % eines Kerns.
- Modul (`src/kwasub/Kwasub.cpp`): eine Tabelle bindet Regler (beider Panels) und CV an Firmware-Parameter; ein
  bewegter Regler setzt den Wert, andere Regler auf demselben Parameter folgen; CV wird in Firmware-Einheiten addiert
  (10 V = ganzer Bereich). Gates → MIDI-Noten (C4 = 0 V, auf Halbtöne gerundet, Tonhöhenwechsel unter dem Gate =
  legato), CLOCK → Tempo (BPM aus der Periode, PPQN im Menü), RESET → EV_SYNC. Ausgang kubisch von 44,1 kHz auf die
  Rack-Rate umgerechnet. Patch speichert alle 140 Parameter und den EQ.
- Test `test/kwcore_test.c` (läuft im Build): alle 97 Presets klingen, enden, bleiben in −1..1; Clamping, EQ, Tempo,
  4 Stimmen.
- [ ] Test in Rack (Regler-Gefühl, Tooltips, Expander-Kopplung, Pegel 5 V).
- [ ] Ideen: FX LIVE (DJ-Filter, Stutter, Tape-Stop, Freeze) als Taster/Gates; MORPH A/B.

### UMLAUF + UMLAUF FX
- Engine von VADER 0.9.0 (L'Cronx, MIT, github.com/sunskiefer/VADER) in `vader/`, kopiert mit `tools/vader_vendor.sh`,
  Änderungen nur per `tools/vader_patch.py` (Thread-lokale Puffer, Corner-Bus, Listen-Funktionen; siehe
  `vader/README-UMLAUF.md`). LXR02-Wavetables ersetzt durch eigene (`tools/umlauf_wavetables.py`, wegen Lizenz).
  Gebaut wie VADERs Desktop-Plugin als `build/vader/libvader.a`, ins Plugin gelinkt. 54 HP + 36 HP, Akzent `#ff2e43`.
- Modul (`src/umlauf/Umlauf.cpp`): Schlüssel-Tabelle Regler/CV → Engine-Parameter (Text-Schlüssel wie im Original),
  geschrieben nur bei Änderung, 32-Frame-Blöcke mit 44,1 kHz, kubisch auf die Rack-Rate. Eckregler 1–5 wechseln ihre
  Bedeutung mit der Engine (SYNTH, 14 Stimmen, WAV). Gates → MIDI (16 Stimmen), CLOCK → `host_time` (Tempo + Beat),
  X/Y → FOCUS. Ausgänge aus dem View-Bus (Orbiter, Gewichte, ENV) und dem Corner-Bus. Zustand der Engine (`state`) im
  Patch; Menüs laufen über eine Befehlsliste im Audio-Thread.
- Ordner `Rack2/GenraSynthis/Umlauf/` (Shapes mit den 7 Beispielen, Samples, User Presets).
- Test `test/vader_test.c` (läuft im Build): alle 15 Engines, 27 Modi, 50 Formen, 22 Presets klingen und bleiben
  endlich; Orbiter bewegt sich, Gewichte = 1; Corner-Bus; State-Rundreise; Namen; zwei Instanzen in zwei Threads.
  8 Stimmen ≈ 4 % eines Kerns.
- [ ] Test in Rack (Pegel, Regler-Gefühl, Expander-Kopplung, Clock/Arp, Samples, eigene Formen).
- [ ] Ideen: Timbre B als zweites Modul koppeln, IMAGE-Seite (Bild moduliert den Klang), TRAVEL-Seite (Speed-LFO,
  Stop an Ecken, Kaleidoskop) als zweiter Expander.

## ABLAUF (2.12.0) – Stand 6. November 2026
- Song-Arranger für GENRA, 30 HP, Akzent gelb `#ffd23d`, Modul-Slug `GenraAblauf`. Panel: `tools/ablauf_panel.py` → `res/Ablauf.svg` + `src/ablauf/AblaufLayout.hpp`. Ersetzt die Szenen von GENRA (2.11.0 entfernt).
- Verbindung wie GRID (angedockt oder Rechtsklick › Verbinden mit…). Alle Bearbeitung auf dem UI-Thread (`AblaufWidget::step` → `uiFrame`); GENRA übernimmt Patterns über zwei Postfächer in `GenraModule.hpp`: `snapState`/`snap` (Abzug dessen, was läuft) und `cueState`/`cue` (CAS-Protokoll 0 frei, 1 wartet, 2 wird geschrieben, 3 wird übernommen). GENRA zählt Taktstriche (`barIndex`/`barPub`) und übernimmt eine Cue am Taktstrich `atBar` (oder sofort bei `now`), startet das Pattern dann bei Schritt 0. Song-Zeile: Mutes (`arrMute`), Fill am Ende der Zeile (`arrFill`, letzte volle Runde), Stop am Song-Ende (`cue.stop`).
- Dichte-Versätze pro Pattern (`ArrMods`: dens pro Spur, ghost, fill) addiert `buildParams()`; `buildParams(false)` ohne. Gelten, solange ABLAUF angehängt ist (`arrAlive`), werden beim Entfernen von ABLAUF (`onRemove`) verworfen und im Patch von GENRA mitgespeichert.
- 64 Plätze (Slot = Pattern + Spur-Genres + Länge + Versätze + Abschnitt), Song bis 64 Zeilen (Pattern, Takte 1–32, Abschnitt, Spuren, Fill, CV A/B). Undo: `AblaufAction` in Rack's History (UNDO-Taster nimmt den obersten Schritt, wenn er von diesem ABLAUF ist).
- Varianten (`src/Arranger.cpp`, `makeSection`): neu gewürfelte Spuren im Stil des Teils, die anderen exakt. VERSE/DROP bleiben generiert (nur Versätze), INTRO/BUILD/BREAK/OUTRO werden eingefroren und Schritt für Schritt geformt (Pads bleiben generiert). Sitzung: solange eine Variante unverändert läuft (`patChanges` = `patChangesAtCue`), kommen weitere Teile aus derselben Quelle, ein zweiter Druck würfelt dasselbe Pad neu.
- Test `test/test_ablauf.cpp` (im Workflow): alle Genres × Längen × Teile – behaltene Spuren byte-genau, Intro ohne Snare/Bass/Melodie in der ersten Hälfte, Build-Wirbel wird dichter, Lücke ohne Kick und Bass, 32tel am Ende, Break ohne Kick mit Grundtönen, Outro-Schlusstakt nur Kick, Drop mit Bass auf der Eins, Speichern.
- [ ] Test in Rack: Song mit GENRA (Taktgenauigkeit bei interner und externer Clock, 1-Takt-Zeilen), Varianten nach Gehör (Build-Länge, Intro-Dichte je Genre), Undo, Laden/Speichern grosser Bänke.

- Tooltips, Menüs und Meldungen von GENRA, GRID und ABLAUF auf Englisch (wie die übrigen Module).

## GENRA-Leiste und Aufräumen (2.13.0) – Stand 6. November 2026
- Entscheidungen (Bericht „GENRA-Plugin: Zusammenarbeit und Streamlining“): einheitliche GENRA-Leiste an allen Instrumenten, alle Ein-/Ausgänge bleiben (auch für fremde Module), Verbindung *Spur von GENRA* per Rechtsklick zusätzlich (folgt), Tooltips/Menüs Englisch. Sound-Snapshots in ABLAUF gestrichen (Klangwahl bleibt frei bei den Instrumenten).
- Leiste: `tools/panellib.py` (`genra_strip`): V/OCT · GATE · VEL · ACC · SLIDE · MOD (Spurfarbe), CLOCK · RESET (grau), Raster 9,4 mm, unten links. Umgesetzt in MONO, K-WASUB, FUNKE, SCHMELZ, UMLAUF; VOICE, ANIMAL, MONK (Leistenoptik in px); DRUM-X: Poly-Eingang DRUMS.
- Neue Buchsen (hinten an die Listen gehängt, alte Patches laden): GENRA `DRUMS_OUTPUT` (8 Kanäle), DRUM-X `GENRA_INPUT` (GENRA-Reihenfolge → Spuren 0,1,2,6,3,4,5,7), SCHMELZ `MOD_INPUT` (Ziel im Menü, im Patch), FUNKE `RESET_INPUT` (Tempo neu messen), K-WASUB `ACC_INPUT` (Velocity +30) und `SLIDE_INPUT` (Glide +35 % des Bereichs, solange hoch).
- Eigene dunkle Regler `res/knob/{large,medium,small,trim}{,_bg}.svg` in Rack-Grössen, Klassen `GLargeKnob`, `GKnob`, `GSmallKnob`, `GTrimpot` in `plugin.hpp` – ersetzen Rack's Round*BlackKnob/Trimpot in allen Modulen (MONK wirkte mit Rack's Reglern silbern).
- Workflow: GENRAs Engine-Tests (Spuren, Grid-Bearbeitung, Speicherformat, MIDI) laufen mit. Vorschaubilder ohne Rack-Quellen: `../ext/Rack/res/ComponentLibrary` mit Rack's Grafiken plus den eigenen Reglern.
- [ ] *Spur von GENRA* (Verbindung per Rechtsklick) für alle Instrumente.
- [ ] Test in Rack: neue Buchsen, Regler-Optik, K-WASUB ACC/SLIDE nach Gehör.


### 2.13.1 – ABLAUF-Bedienung
- Leeres Pad antippen = speichern, was GENRA gerade spielt (`padTap`: `pendingStore` + Snapshot). Belegte Pads überschreibt weiterhin nur STORE + Pad.
- STOP: Markierung bleibt auf der Zeile, die lief (`setRow(song.row)`); STOP im gestoppten Zustand = `setRow(0)`, Song-Anfang.

### 2.13.2 – ABLAUF-Panel englisch
- Beschriftung wie die anderen Module: ROW, BARS, PART, TRACKS ON / OFF, INSERT, DELETE, RE-ROLL, ROW CV, INPUTS/OUTPUTS, END, ROW. Param-Namen im Code unverändert (Patches laden weiter).

### 2.13.3 – Fehlerprüfung aller Module
- GENRA: CV an GENRE/Spur-Genre würfelt das gespeicherte Pattern beim Laden nicht mehr neu (`adoptGenre`, 0,1 s nach dem Laden bzw. 1 Sample nach einem Cue); Spur-Genre-CV regeneriert nur bei echtem Zielwechsel (`lastTarget`) und nicht bei LOCK. `initGenres()` in `init()` (ABLAUF vor GENRA im Patch). MIDI-Import maskiert Datenbytes, v2-Format klemmt `chop`.
- GENRA/ABLAUF: `cueCancel` wartet den Prüf-Moment des Audio-Threads ab; ein wartender Cue wird beim Loslassen (ABLAUF gelöscht/umgelinkt) verworfen; ein ABLAUF pro GENRA (`arrOwner`); Snapshot-Anfrage nimmt einen liegengebliebenen Snapshot nicht mehr; Stop auf GENRA selbst wird erkannt; NEXT in der letzten Zeile beachtet LOOP und END; `cuedRow` folgt Einfügen/Löschen; Ausgänge halten ihre Werte über ein Undo; JSON-Klemmen.
- GRID: Wechsel des GENRA leert Undo/Redo; noch nicht übernommene Edits gehen nicht verloren.
- DRUM-X: Lane-Core wird nach Fehler nicht jedes Sample neu geladen; Trigger-Schwelle 0,5 V; TRIG + DRUMS gleichzeitig = ein Schlag; Kit aus Patch begrenzt; Preset auf laufendes Modul wirkt (auch VOICE, ANIMAL).
- K-WASUB: Effekte gehen nach dem Laden nicht von selbst an; SLIDE/GLIDE-CV wird nicht mitgespeichert; RESET verfälscht das Tempo nicht; ACC auch bei Legato.
- MONO: Chorus-Puffer reicht bis 192 kHz; RESET verfälscht das Tempo nicht.
- STOTTER: Samplerate-Wechsel setzt Pads zurück; HOLD-Pad hängt nicht bei Bank-/Moduswechsel; externes Tempo 48–300 BPM.
- UMLAUF: JSON-Strings und Preset-Index geprüft.

### 2.13.4 – ABLAUF CLR ALL
- Taster CLR ALL (zweimal drücken, 3 s): leert alle 64 Pads als ein Undo-Schritt (`clearSlots`, history::ComplexAction aus AblaufActions). Rechtsklick: Clear bank A–D. Song-Liste bleibt. `CLEARALL_PARAM`/`CLEARALL_LIGHT` hinten angehängt.

### 2.14.0 – Follow GENRA (verbinden ohne Kabel)
- `src/Follow.hpp/.cpp`: „virtuelles Kabel“. Solange kein echtes Kabel steckt, schreibt das Modul die Quellspannungen am Anfang von `process()` selbst in seinen Input (`channels`/`voltages` direkt, `setChannels()` würde nicht verbinden). Ein echtes Kabel meldet `onPortChange` (exakt, unter dem Engine-Lock) → `Drive::cable`; dann schreibt das Modul nichts.
- Quellen über `follow::Ref` (atomarer Zeiger, Registry mit Mutex); `follow::gone(this)` in `onRemove` jedes beteiligten Moduls (GENRA, Instrumente, STOTTER, MIX) nullt alle Zeiger auf das Modul (Rack hält dabei den Engine-Lock).
- Instrumente: `struct X : Module, follow::Host` mit `fol.bind(input, Sig)`, `audioL/audioR`, `srcName`; `fol.process(this)` als erste Zeile in `process()`; JSON-Schlüssel `followGenra`; Menü `fol.appendMenu`; `follow::addStrip()` zeichnet die Segmente (Layer 1) und sucht das GENRA (UI-Thread, alle 0,5 s).
- GENRA veröffentlicht am Ende von `process()` seine Ausgänge in `followPub[frame & 1]`; Follower lesen den vorigen, vollständigen Frame (`genraFrame`), auch mit mehreren Engine-Threads nie halb geschrieben (ein Sample später als ein Kabel).
- GENRA-Ausgänge: Pad-V/OCT/GATE werden immer als 4 Kanäle gelesen, Drums aus den 8 Einzelausgängen (DRUMS poly schreibt GENRA nur, wenn gesteckt).
- MIX: `ChSrc` pro Kanal (Off/Auto/fest, JSON `inputFrom`, `duckFrom`), setzt bei der Quelle `Host::pulledAt`; die Instrumente rechnen stereo, wenn R gesteckt ist oder `pulled()` (statt fremde `channels` zu verändern). Fehlt ein gewähltes Modul (z. B. eingefügte Kopie), gilt Auto. `SourceTags` zeichnet Namen/„CABLE“ über „CH n“ und den Ring an DUCK.
- Panels: Leistenlinie gedimmt (`panellib.dim()`, 35 % auf der Ablage), auch JIANT/MONK.

### 2.14.1 – FUNKE ohne Knacksen beim Neuanschlag
- Eine klingende Stimme, die neu angeschlagen wird (Mono-Retrigger, Voice-Stealing in POLY), setzte Phasen, Saite (PLUCK) und Anregung hart zurück: Sprung in der Wellenform. `Voice::process` misst den Sprung im ersten Sample und blendet ihn über ~3 ms aus (Offset), der Anschlag bleibt scharf.

### 2.15.0 – Weniger CPU, gleicher Klang
- K-WASUB: Der Kern zeichnete ~30×/s sein (unsichtbares) Display im Audio-Thread, ~75–80 % seiner Zeit. `kwcore.c` setzt `ui_frame_ms = fm1_ms` vor `ksub_step()` und liest MIDI mit `app_poll()` jeden Block (Noten ~6 ms statt bis ~33 ms).
- FUNKE: Zeiten/Raten nur bei Knopfänderung (`mk/mv`), Filter-`tan` einmal pro Sample (`gmono::Filter::coef/processG`), Effekte schlafen nach 3,4 s Stille; Declick wie 2.14.1.
- MONO: Koeffizienten gecacht, `tan` einmal pro Sample, Oszillatoren/Filter ruhen nach 0,25 s Stille (nicht bei Resonanz ≥ 0,9).
- JIANT (VOICE, ANIMAL, DRUM-X inkl. Lanes): `jiant::Resampler` pausiert den Speex-Konverter bei Stille; Firmware-Screen nur, wenn er in den letzten 200 ms gezeichnet wurde (`ScreenClock`).
- UMLAUF: UI-Befehle über `cmdsPending`, `refreshState()` nur nach Änderungen (`stateOwed`, try_lock), Display nur bei Sichtbarkeit, CV-Werte nur bei Änderung > 1e-4.
- SCHMELZ: Stimmen-Koeffizienten nur bei Änderung, stumme Stimmen nur Hüllkurve, Ensemble 4 statt 6 `sin`, Phaser alle 16 Samples interpoliert, Bus schläft in Stille (Zustände < 1e-7), Reverb ohne `%`.
- MONK: DRY-`tanh` nur wenn gesteckt, Stille/Echo-Ruhe, Delay ohne `%`, Grain-Cache bei gleichen Unison-Stimmen.
- STOTTER: Konstanten vorberechnet, DJ-Filter nur bei Änderung, keine 64-Bit-`%`. MIX: Duck-Rate gecacht, ruht wenn eingeschwungen.
- Follow: Frame wird durchgereicht (kein `APP->engine->getFrame()` pro Sample), MIX schreibt `pulledAt` nur alle 1024 Frames. GENRA: Flags erst lesen, dann `exchange`.

### 2.15.1 – Verbindungen ohne Kabel an beiden Enden sichtbar
- GENRA (`LinkLights` in Genra.cpp): Ausgänge, die ein Instrument ohne Kabel liest, leuchten als Ring in der Spurfarbe (CLOCK/RESET weiss); über „OUT“ der Gruppe steht, wer liest.
- Instrumente und STOTTER (`follow::OutLight`): OUT L/R leuchten in der Kanalfarbe, solange der MIX sie nimmt (`Host::pulledBy`), darüber „› MIX CH n“.
- MIX: über IN L/R (und DUCK) leuchtet das Segment wie an der Leiste der Instrumente; das Namensfeld über dem Kanal wird auf dem Panel gezeichnet (deckt „CH n“ ab), Name und Schein als Licht.

### 2.15.2
- Ausgänge (GENRA, Instrumente, STOTTER) zeigen die Verbindung ohne Kabel als kleines Leuchtsegment über der Buchse statt als Ring, wie die Eingänge.

### 2.16.0 – GRUND und GRUND FX
- GRUND (`src/grund/`, 30 HP, Bassgrün): 10 Modelle (Saiten als Karplus-Strong mit Allpass-Bruchteil, Synth polyBLEP, Reese, 2-Op-FM, 808 mit Pitch-Drop), Stimme 2× überabgetastet, Bus in Samplerate (Oktaver, Amp mit Cab, Drive, Kompressor mit Sidechain, 3-Band-EQ, Chorus/Width nur Höhen, Reverb aus SCHMELZ), Bus schläft nach 4 s Stille, Reverb-Ausklang als Zähler. Pegel pro Modell (`LEVEL`), Softlimit ±10 V.
- Presets wie GENRA MONO (30 ab Werk, Stil-Untermenüs, User-JSON in `Genra/GrundPresets` mit `params` und `fx`); Laden ist ein Undo-Schritt für GRUND und Expander (`ComplexAction`).
- GRUND FX (`gfx`, 14 HP): nur Regler; GRUND hält die FX-Einstellungen (`fxStore`, im Patch-JSON), liest beim Andocken Knöpfe + CV und schreibt beim ersten Andocken bzw. beim Preset-Laden die Knöpfe. SC-Eingang wird von GRUND pro Sample gelesen, GR zurück über `std::atomic`.
- Expander-Layout: CV-Eingänge in den Sektionen statt in einer Buchsenreihe (schmaler). Panels: `tools/grund_panel.py`, `tools/grundfx_panel.py`. Test: `test/test_grund.cpp` (Modelle, Presets, Stabilität 44,1–192 kHz, Oktaver, Sidechain).

### 2.16.1 – GRUND: Rauschen behoben
- `ggrund::Ring::read` konnte einen Index hinter dem Pufferende lesen (w − d knapp unter 0, nach += n auf n gerundet). Unter Linux stehen dort meist Nullen, unter Windows Speicherreste → Rauschen/Zerren je nach Speicherinhalt, auch ohne Note. Gefunden mit AddressSanitizer; alle Engine-Tests (GRUND, FUNKE, MONO, SCHMELZ, MIX, STOTTER) laufen jetzt auch unter ASan/UBSan sauber.

### 2.16.2 – GRUND: VOICE-Regler wirken deutlich
- Gemessen (Spektrum + Pegel, Regler 0 gegen 1, jedes Preset): TIMBRE/BITE/BODY/MUTE änderten oft < 3 dB, BITE bei SYNTH/REESE nichts, CHORUS bei fast allen Presets nichts (Chorus-Sektion in den Presets aus).
- TIMBRE verschiebt zusätzlich den TONE (±1,2 Oktaven); BITE öffnet den TONE beim Anschlag (eigene kurze Hüllkurve, alle Modelle); BODY-Resonanzen stärker; MUTE 2,5 s bis 60 ms, auch bei den Saiten; SYNTH: Säge → Rechteck → schmaler Puls; REESE: Verstimmung bis 0,9 Halbtöne; 808: mehr Drive.
- Presets: Chorus- und Hall-Sektion immer an, die Regler auf GRUND bestimmen die Menge.
- Danach ändern alle VOICE-Regler bei Saiten, SYNTH, FM und REESE typisch 5–15 dB; bei stark verzerrten Presets (RUMBLE, NEURO REESE, GROWL) prägen DRIVE/TONE den Klang, VOICE-Regler wirken dort weniger.

### 2.17.0 – Presets für FUNKE und SCHMELZ
- FUNKE: 26 Werks-Presets (`src/funke/FunkePresets.hpp`, Stile SOUL · JAZZ · LO-FI, HOUSE · GARAGE, DUB · REGGAE, TECHNO · ELECTRO, GRIME · DUBSTEP · DRILL, POP · SYNTHWAVE · TRANCE, AMBIENT); SCHMELZ: 25 (`src/schmelz/SchmelzPresets.hpp`, STRING MACHINE CLASSICS bis POP · SYNTHWAVE · TRANCE). Jedes Preset setzt alle Regler und Schalter.
- Gemeinsamer Rack-Teil `src/ui/PresetBank.hpp` (Bank, Header-Feld, Kontextmenü): Liste nach Stil, Nummer + Name + Punkt bei Änderung, Mausrad blättert, User-Presets als JSON in `Genra/FunkePresets` bzw. `Genra/SchmelzPresets` (Save preset as …, Delete user preset, Open user preset folder). Laden ist ein Undo-Schritt (`ModuleChange`), Name und geladene Werte im Patch-JSON (`preset`, `loaded`); Patches ohne Preset zeigen „--“/INIT.
- SCHMELZ-Registrierungen neu sortiert: BASS · CELLO · VIOLA · VIOLIN · BRASS · ORGAN · CHOIR · SYNTH (vorher BASS an vierter Stelle, das Drehen überblendete schlecht). Keine Umrechnung alter Patches (Entwicklungsphase, siehe DESIGN.md).
- Pegel abgestimmt per Rendering: Tests spielen jedes Preset (FUNKE Akkord bzw. Einzelton bei MONO/LEGATO, SCHMELZ Vierklang), prüfen endlich, hörbar, Spitze, Lautheit (lautestes 50-ms-Fenster; SCHMELZ alle ≈ 1,3 V über VOLUME, FUNKE 1,0–2,4 V, Einzeltöne leiser als Akkorde) und Stille nach dem Release. Auch unter ASan/UBSan sauber.

### 2.18.0 – Presets im Kopf: K-WASUB und UMLAUF
- Beide haben jetzt das Preset-Feld im Kopf wie GRUND, FUNKE und SCHMELZ (Nummer, Name, Stil, Punkt bei verstellten Reglern, Klick = Liste nach Stil, Mausrad, Undo, User-Presets per Rechtsklick in `Genra/KwasubPresets` bzw. `Genra/UmlaufPresets`). K-WASUB: der PRESET-Kasten unten ist weg, SUB · X, WOBBLE, AMP und FX teilen sich die Breite; alle Buchsen (auch PRESET CV) und Param-Ids bleiben (PREV/NEXT ohne Taster, per MIDI-Map nutzbar).
- Gemeinsamer Teil `src/ui/StateBank.hpp`: für Instrumente, deren Klang in einer Engine liegt. Werks-Presets lädt der Audio-Thread (`request`), das Modul meldet jede Ladung (`loadSerial`, auch per CV und Reset), die Bank holt im UI-Thread auf und legt einen Undo-Schritt an. User-Presets sind das ganze Modul-JSON (Regler + Engine-Zustand, ohne `followGenra`) und werden wie Racks eigene Presets geladen (`Engine::moduleFromJson`). Undo/Preset-JSON trägt `engineWins`: dann übernimmt der Expander die Werte der Engine statt sie mit seinen Reglern zu überschreiben.
- UMLAUF: 28 eigene Werks-Presets (`src/umlauf/UmlaufPresets.h`, reines C: INIT ORBIT + Engine-Werte) in TECHNO, HOUSE · DEEP, DUB · DUB TECHNO, AMBIENT · DRONE, BASS, KEYS · LEADS, CINEMATIC · FX; dahinter VADERs 22 als Gruppe VADER. Alte Patches: `preset` (VADER-Nummer) bleibt im JSON und wird gelesen, neu kommt `presetName`. K-WASUB: Nummern nach der sortierten Liste, `preset` bleibt die Firmware-Nummer.
- `test/vader_test.c` spielt jedes UMLAUF-Preset (Akkord und tiefer Ton): endlich, Spitze 0,08–0,9 (×10 V im Modul), nach dem Loslassen still. Hüllkurven-/Cutoff-Kurven der Engine: Decay/Release 5 ms·2000^x, Attack 1 ms·6000^x, Cutoff 20 Hz·2^(9,9x).

### 2.19.0 – ABLAUF: breites Pattern-Fenster, 128 Pads, Namen
- Panel neu aufgeteilt: Song-Fenster schmal (63 mm, Liste kompakt: #, Pad + Name, Takte, Spuren als Farbpunkte, Fill als gelbes Quadrat, Teil), Pattern-Fenster breit mit quadratischen Pads 8 × 4 pro Bank. 4 Banks × 32 = 128 Plätze. Bedienelemente im Song-Teil in drei Reihen (ROW/PATTERN/BARS/PART · D B M P FILL PATT/SONG · INSERT DELETE UNDO PLAY STOP LOOP). Param-/Port-Ids unverändert.
- Patch-JSON hat jetzt `version: 2`. Alte Patches (ohne Version, 4 × 16) werden umgerechnet: Platz b·16+p → b·32+p, also behalten A1–A16, B1–B16 … ihre Namen; Song-Zeilen, gewähltes Pad und das von GENRA gemeldete laufende Pad folgen.
- Pad-Namen (`Slot::name`, 8 Zeichen A–Z 0–9 Leerzeichen -, im JSON `name`): das erste gespeicherte Pattern in einem leeren ABLAUF heisst ROOT; Varianten erben den Namen des Ausgangs-Pads (bzw. ROOT, wenn noch nichts gespeichert war), COPY kopiert ihn, Überschreiben per STORE behält den Namen des Pads. Umbenennen per Rechtsklick (Textfeld, Enter) und *Clear name*, jeweils ein Undo-Schritt.
- Teil-Taster: jeder Druck schreibt eine neue Variante auf das nächste freie Pad (ab der aktuellen Bank, dann die folgenden), nichts wird mehr überschrieben; alle 128 voll → Hinweis „All 128 pads are full“. Die Quelle bleibt das ursprüngliche Pattern, solange eine der Varianten unverändert läuft (`Session::made`).
- Bass in BREAK und INTRO variiert pro Druck (`PartOpts`, `BREAK_BASS_NAMES`/`INTRO_BASS_NAMES` in `Arranger.hpp`): BREAK zufällig aus lange Grundtöne (bei Akkorden über 2 Takte jetzt alle 2 Takte neu angeschlagen, kein stiller Takt), ausgedünnte Original-Linie (Quell-Bass ohne Neu-Würfeln, nur 1/3 und Akkordwechsel, max. 6 Steps, ×0,8), Puls (Viertel oder Achtel, gelegentlich Oktave), Dub-Offbeats (1–3 Töne pro Takt auf „und“ von 2/4 bzw. 16tel-Versätzen, Grundton/Quinte), Bass raus (1–2 Töne Auftakt im letzten Schlag); INTRO aus gebundene Grundtöne, ausgedünnte Linie, Puls – jeweils ab der Hälfte. Rechtsklick › Break bass / Intro bass (Random oder fest), JSON `breakBass`/`introBass`. `test_ablauf` prüft jede Art (kein leerer Takt ab dem Einsatz außer „Bass raus“, Drums unberührt) und dass Random variiert.


### 2.19.1 / 2.19.2 – Namen
- Varianten heissen nach ihrem Teil mit laufender Nummer (INTRO 1, INTRO 2 …); Patterns von GENRA auf leeren Pads heissen GENRA 1, GENRA 2 … (statt ROOT). `Store.cpp`: Record-Länge Byte für Byte gelesen (Operanden-Reihenfolge war nicht festgelegt).

### 2.20.0 – ABLAUF: MAKE SONG, SONG FORM, Familien, MIDI-Export, Pads in den Song ziehen
- Panel: Taster D B M P und STORE sind weg (Param-Ids bleiben für alte Patches, ohne Widget). Song-Fenster: FILL, PATT/SONG, PLAY/STOP/LOOP in der Mitte; INSERT/DELETE/UNDO und **MAKE SONG** (neuer `MAKESONG_PARAM`/`_LIGHT`, angehängt) unten. Pattern-Fenster unten: **SONG FORM**-Feld (`SONGFORM`), daneben COPY/CLEAR/CLR ALL.
- Familien: `Slot::family` (JSON `family`). Ein Pattern von GENRA auf leerem Pad beginnt Familie n (Name „GENRA n“, n = eins über allen Familien und GENRA-Namen). Varianten (Teil-Taster, MAKE SONG) und COPY übernehmen sie, Umbenennen nicht. Nummern der Varianten zählen pro Familie. Balken oben auf dem Pad und Strich in der Song-Liste in der Familienfarbe (8 Farben, getrennt von Teil- und Spurfarben); ohne Familie grau. Alte Patches: „GENRA n“ → Familie n, sonst keine.
- MAKE SONG: aus dem gewählten Pad (Quelle = Original seiner Familie, `tag < 0`), Form = SONG FORM (AUTO: `songFormForGenre` nach dem Genre der Drum-Spur bzw. der Akkorde). Pro Teil eine vorhandene Variante der Familie (kleinste Nummer, Wiederholung nimmt die nächste) oder neu per `makeSection` mit GENRAs Reglern (`buildParams(false)`), RE-ROLL und Bass-Arten. Song-Liste ersetzt, ein `ComplexAction` (neue Pads + Liste). Ohne GENRA bzw. bei vollen Pads spielt dort das Original (Meldung). Formen in `Arranger.cpp` (`SONG_FORMS`, 8 Stück, Tabelle in der README), JSON `songForm` (-1 = AUTO).
- Stumme Spuren pro Zeile: nicht mehr einstellbar; Zeilen aus alten Patches behalten sie (Liste zeigt ein kleines M), neue Zeilen spielen alle Spuren.
- Rechtsklick aufs Pad: *Overwrite with GENRA* (statt STORE; Name, Teil, Familie bleiben), *Export as MIDI…* und *Export drums/bass/melody/pads only…*. Doppelklick aufs Pad: Umbenennen.
- Rechtsklick aufs Modul: *Export song as MIDI…* (`renderSong` in `Midi.cpp`: Zeilen hintereinander, Loop wiederholt bzw. abgeschnitten, Fill im letzten Takt, stumme Spuren, Marker pro Zeile im Tempo-Track; `writeSmf` hat dafür einen optionalen `markers`-Parameter). `renderLoop` nutzt intern denselben Weg (`renderRange`), Ergebnis unverändert.
- Pads per Drag & Drop in die Song-Liste: zwischen Zeilen einfügen (Linie), auf eine Zeile ersetzen (Rahmen), Undo. Ein belegtes Pad löst sein Antippen jetzt beim Loslassen aus (sonst wäre jeder Drag ein Antippen); leere Pads speichern weiter beim Drücken.
- Song-Liste unten: Takte und Spieldauer aus GENRAs Tempo (`Genra::currentBpm`, auch für GENRAs MIDI-Export), z. B. „BAR 37 / 92 · 1:12 / 2:59“.
- `test_ablauf`: jede Genre-Variante hat eine Form, Formlängen, `renderSong` (eine Zeile = Loop, Marker, stumme Drums, Notenbereich, Marker in der Datei).

### 2.31.1 – ABLAUF MINI nach den Regeln des grossen ABLAUF, Mac-Builds
- **ABLAUF MINI** folgt ABLAUF (der Host gibt die Regeln vor): Teil-Taster = sofort neue Variante aufs nächste freie Pad
  (02–16, ROOT zuletzt) aus dem spielenden Pattern, mit derselben Session-Regel (spielt eine Variante der Session
  unverändert: dieselbe Quelle; `slotVer` pro Pad, `patChanges == patChangesAtCue`). Nichts gespeichert: das Spielende
  wird ROOT. Leeres Pad = speichern (eigene Familie, `Slot::family`, JSON `family`; ROOT bzw. „GENRA n“). Namen und VARY
  pro Familie (`nextName`, `lastVariant`). SONG wie `Ablauf::makeSong`: Quelle = Familie des spielenden Pads (sonst
  ROOT, sonst erst speichern), vorhandene Varianten zuerst, fehlende neu, Pads voll = Original. `makeSongReq` (MAKE-SONG-
  Pad an GENRA / Box) wirkt auch auf MINI. Rechtsklick Break bass / Intro bass (`PartOpts`, JSON `breakBass`/`introBass`).
  MIDI-Export über `midifolder::save` statt Dateidialog. Ein Cue-Zähler für Pads und Song.
- **CI:** Job `mac` (macos-latest, Matrix arm64/x64; x64 per `clang -arch x86_64` gegen das x64-SDK), prüft mit `lipo`
  die Architektur von plugin.dylib und allen Cores, Artifacts `genra(-instruments)-mac-arm64/-x64`. Release ist ein
  eigener Job (`needs: [windows, mac]`), der alle Artifacts einsammelt. Mac noch auf keinem echten Gerät getestet.

### 2.31.0 – GENRA BOX und ABLAUF MINI zurück, Presets nach Genre
- **GENRA BOX** (`src/box/GenraBox.cpp`, 40 HP) auf 2.30 neu aufgesetzt: Stimmen DRUM-X, **GENRA MONO** (statt GRUND,
  Makros CUTOFF/RESO), FUNKE, SCHMELZ. NEW und HOLD in der PATTERN-Sektion. SOUND Standard „Same genre“. Neu: MUTATE-
  Regler, CLOCK OUT, DUCK (Kick drückt Bass und Pads), Mono-Summe auf L. `Genra`-Konstruktor nimmt zusätzliche
  Params/Inputs/Outputs/Lights. `isGenraModel()` statt `== modelGenra` (Grid, Ablauf, Follow erkennen die Box).
- **ABLAUF MINI** (`src/box/AblaufMini.cpp`, 16 HP): Spur-Taster Keep/Re-roll/Vary, neuer Taster **SONG** (MAKE SONG
  mit Wiedergabe der Teile der Reihe nach, Stopp am Ende, Song im Patch, MIDI-Export des Songs).
- **Presets nach Genre:** `src/GenreMatch.hpp` (ohne Rack, Namen kommen per `setGenreNames` aus `initGenres`): ganzer
  Genre-Name = 2, Wort ≥ 3 Buchstaben = 1, Namen anderer Genres zählen nicht. `tools/genre_presets.py` schreibt
  159 neue Presets (GRUND 39, FUNKE 39, SCHMELZ 39, MONO 42) zwischen Marker in die Preset-Tabellen und misst sie ein
  (`calibrate`). Jedes Genre hat jetzt in allen vier mindestens einen Kandidaten (`test/test_match.cpp`).
- **NEW picks presets** (GENRA, Rechtsklick: Off / All genres / Same genre, Standard Same genre): NEW wählt für folgende
  Instrumente ein Preset (`Genra::presetRoll`, `follow::rollFollowerPresets`, UI-Thread). Follower-Option „Presets follow
  GENRA“ (JSON `presets`). Host-API im Link-Hub (LINK_VERSION 2): GRUND, FUNKE, SCHMELZ, MONO, DRUM-X.
- Tests: `test/test_box.cpp` (alle Box-Presets mit Makros 0/0,5/1, Match-Fälle), `test/test_match.cpp`; beide in CI.

### 2.30.0 – Genre-Gewohnheiten, Melodie-Phrasen, MUTATE um einen Anker, VARY, 5 Dark-Genres
Ideen aus DARK-1 (Polystellarflow, GPL-3.0-only; Tabellen pro Genre als Daten übernommen, kein Code), JIANT FM
(MUTATE-Modi), Makohai (Perioden) und Tessera (DEVELOP). Alles **zusätzlich**: ohne Gewohnheit gelten die alten Regeln.
- **`Genre`** (Engine.hpp) hat neue Felder: `habit[8]`/`nHabit` (Akkordschritte von → nach, Gewicht), `cadTo` (der
  letzte Akkord führt von dieser Stufe heim), `noBack` (kein A–B–A), `raiseV` (V in Moll als Dur mit Dominantseptime),
  `accents` (`AccentBits`), `breakKeep` (1 Kick, 2 Hats), `buildKick`, `padRise`, `buildBars`. Gesetzt in
  `applyFeel()` (Genres.cpp) nach Genre-Namen, am Ende von `defineGenres()`.
- **Akkorde:** `generateChords`, freier Weg: `habitFactor()` multipliziert die alten Gewichte (TRANS × degW) – eigener
  Schritt × (1 + 0,4·w), A–B–A × 0, verminderter Akkord × 0,15, letzter Akkord = `cadTo` × 7 (wenn die Folge auf der
  Tonika beginnt; vermindert → V bzw. IV). Nur in Moll-artigen Genre-Tonleitern (Terz = 3). Tonika als erster Akkord
  90 % statt 75 %.
- **Akzente:** `accentK()` in `drumHit` (nur generierte Drums): AC_ONE, AC_BACKBEAT, AC_SOFT_OFFHAT, AC_OFFHAT als
  Velocity-Faktoren; AC_LATE_SNARE +6–12 ms auf Snare/Clap; AC_GHOST_HATS leise C.Hats auf leeren ungeraden 16teln
  (Wahrscheinlichkeit 0,15 + 0,5·GHOST); AC_FLAM_PERC Ratchet 2 auf Perc/Tom auf dem Schlag (30 %).
- **Song-Teile:** `PartPos` trägt `keep`/`buildKick` des Drum-Genres. `breakDrums` behält Kick bzw. Hats,
  `shapeDrums` BREAK schaltet sie in der ersten Hälfte nicht ab, BUILD ohne Kick im letzten Takt bei `buildKick`
  false. `makeSection`: Pads im BREAK = `padRise`. MAKE SONG: BUILD-Zeilen mit `buildBars` des Genres.
- **Melodie-Phrasen:** `phraseMelody()` für MS_LEAD/MS_SPARSE (wenn `Params::melPhrase`, Menü *Melody*): Motiv 1 Takt
  (Walk), Takt 2 Antwort (Motiv −1, endet auf Stufe 4), Takt 3 Motiv +2, Takt 4 Schluss auf 0/7 (`r = 0` auf dem
  Landeschritt, danach `r = 255` Pause), Takte 5–8: Motiv, Antwort +1 auf Stufe 2, Motiv +4, Schluss. Landeschritt =
  spätester starker Schritt, den das Melodie-Raster spielt (12, 8, 14 …).
- **MUTATE-Anker** (GenraModule.hpp): `anchor`, `anchorStamp` = `patChanges` nach MUTATEs letzter Änderung; jede andere
  Änderung (und `applyCue`, Laden) macht das Pattern zum neuen Anker. *MUTATE mode* Drift/Orbit/Pulse, *MUTATE
  changes* (sperrt Spuren/Akkorde nur für MUTATE), *Back to the anchor*. JSON `mutateMode`, `mutateWhat`, `melPhrase`.
- **ABLAUF VARY:** REROLL-Schalter 0/1/2 (Keep/Re-roll/Vary), Klick schaltet weiter, Vary als farbiger Rahmen.
  `varySection()` (Arranger.cpp): Spur aus der letzten Variante desselben Teils (`lastVariant`, MAKE SONG:
  `have[sec].back()`), dann MUTATE 0,3 auf dieser Spur; Drum-Stimmen, die der Teil ganz weglässt, bleiben leer.
- **5 Dark-Genres** (59–63, Familie „Dark“): Witch House, Darkwave, Dungeon Synth, Industrial, Horror. **Tonleitern**
  hinten angehängt: Locrian, Phryg. Dom., Dbl. Harmonic (`NUM_SCALES` 12, `REL_MAJOR` erweitert).
- **`Pattern::trackGenre`/`chordGenre` jetzt `uint8_t`** (138 Profile > 127). Speicherformat: dasselbe Byte, jetzt
  unsigned gelesen; alte Patches bleiben gleich. Der v2-Rohblock kann nur Profile < 128 (alte v2-Patches hatten nie mehr).
- `genreCvSpan`: alles außer 37 heißt „alle Genres“ (gespeicherte 59 → 64).
- Tests: `test/test_feel.cpp` (neue Genres/Tonleitern, Dominante, kein A–B–A, Phrasenziel, Akzente, Break/Build pro
  Genre, Phrasen schließen auf dem Grundton, VARY nah am ersten und kein Neuwurf); `test_ablauf` und `test_store`
  angepasst.

### 2.29.1 – SCHLAG-RANDOM als Endlos-Regler, RND ALL, Mono-Summe
- **GENRA Instruments trägt jetzt dieselbe Version wie GENRA (2.29.1).** 1.0.0/1.0.1 lud Rack nicht: Rack 2 verlangt
  Version 2.x („Plugin version 1.0.1 does not match Rack ABI version 2“ im log.txt). Die Datei im Release v2.29.0
  (`GenraInstruments-1.0.0`) ist deshalb unbrauchbar.
- **SCHLAG RANDOM:** `RandKnob` ist jetzt ein eigener `ParamWidget` (Drehgeber wie beim Play): kein Zeiger, Wert als Zahl
  und Bogen (`drawLayer` 1), Ziehen ändert in 1-%-Schritten (Strg: feiner), jeder Schritt würfelt (höchstens 25/s,
  der letzte Schritt und 0 % immer), Mausrad 5 %, Doppelklick 0 %; eine Drehung = ein UNDO-Eintrag. Panel: Kind
  `enc` in `tools/schlag_panel.py` (nur Beschriftung, der Ring kommt vom Modul). `Snap` hält auch die RANDOM-Werte
  (`restore()` für UNDO und Strg+Z); BASIC setzt RANDOM auf 0 %.
- **RND ALL** (`rollAll()`): alle Spuren auf 50 % und neu gewürfelt (vorher mit dem eigenen Wert – bei 0 % passierte
  nichts, das sah aus wie ein Fehler).
- **Mono-Summe auf L**, wenn R frei ist und MIX nicht abgreift: SCHMELZ, K-WASUB, GOLANA, UMLAUF, MONK (wie FUNKE,
  GRUND, MONO).
- `docs/DESIGN.md`: zwei Plugins, Regeln für neue Instrumente (zusammenführbar), Mono-Summe, Endlos-Regler.

### 2.29.0 – Zweites Plugin GENRA Instruments
- K-WASUB (+FX), GOLANA (+FX), UMLAUF (+FX), MONK, VOICE und ANIMAL sind ein eigenes Plugin **GENRA Instruments** (`instruments/`, Slug `GenraInstruments`, Marke `Genra`, Version 1.0.0), im selben Repo. Im GENRA-Plugin bleiben GENRA, GRID, ABLAUF, MIX, GENRA FX, STOTTER, SCHLAG, DRUM-X, GRUND (+FX), MONO, FUNKE, SCHMELZ. Alte Patches mit den sechs Modulen finden sie nicht mehr (bewusst: kaum Downloads bisher).
- `instruments/`: eigenes `plugin.json`, `Makefile` (Kerne voice/animal aus `../jiantcore` + `../jiant.tar.gz`, kwcore/glcore, VADER), `src/plugin.cpp`, `res/` (Panels, `knob/`, `monk/`, `umlauf/`), `licenses/`, Tests (`test/kwcore_test.c`, `glcore_test.c`, `vader_test.c`). Gemeinsamer Code bleibt in `src/` und wird über Weiterleitungs-Header (`instruments/src/Follow.hpp`, `ui/…`, `jiant/JiantUI.hpp`) und `instruments/src/shared.cpp` (Follow.cpp + link/Hub.cpp) mitgebaut. Die Panel-Werkzeuge schreiben nach `instruments/`.
- **Kabellos über die Plugin-Grenze:** `src/link/Hub.hpp/.cpp` – ein Register für Referenzen (`follow::Ref`), Hosts (Instrumente für MIX) und GENRAs, das beide Plugins teilen: das zuerst geladene legt es an und hinterlegt die Adresse in der Umgebungsvariable `GENRA_LINK_HUB`; das andere nutzt es, wenn die Version (LINK_VERSION + Grössen der geteilten Strukturen) stimmt, sonst ein eigenes (dann keine Verbindungen über die Grenze, nie ein Absturz). `follow::isGenra` per Slug, `genraFrame/genraBpm` über `Ref::genra` (GENRAs `followPub`/`stepPeriod`, von GENRA im Konstruktor gemeldet: `follow::addGenra`), GENRAs Ausgangsnummern in `link/GenraPorts.hpp` (static_assert in GenraModule.hpp). `follow::host()` nimmt für Module des anderen Plugins das Register (das Panel meldet sie: `OutLight::step` → `knownHost`).
- CI baut beide, Artifacts `genra-windows` und `genra-instruments-windows`; ein Release enthält beide Dateien. Die Paketprüfung bricht jetzt wirklich ab (vorher lief `! cmd` ohne Wirkung); geprüft wird nur noch, dass kein libstdc++ nachgeladen wird – libwinpthread-1.dll liefert Rack mit (wie bei Surge XT, Bogaudio, Stoermelder), früher hatte VADER es zufällig statisch hereingezogen.

### 2.28.1 – SCHLAG: Fehler aus der Code-Prüfung
- **Datenverlust beim Speichern:** `resolve()` behält `pending`, solange die Bank leer ist (Ordner fehlt / leer); `dataToJson` schreibt bei `pending.on` immer Namen, Map **und** PRINTs, wie sie kamen; `kitToJson` nimmt dann auch den Grundnamen aus `pending` (Preset auf ein bestehendes SCHLAG).
- **Kein Freigeben im Audio-Thread:** alte Banken werden erst gelöscht, wenn nach 3 s keine Stimme mehr eins ihrer Samples hält (`use_count`).
- **Riesige Dateien:** `system::getFileSize` vor dem Lesen (> 512 MB wird übersprungen).
- **Vorgezogene GENRA-Drums:** GENRA veröffentlicht pro Drum-Stimme den Schritt des zuletzt gefeuerten Treffers (`drumStepPub`, gesetzt in `fireDrum`; vorgezogene Treffer tragen `next`, verzögerte `pStep`). SCHLAG nimmt für Treffer von GENRA (`fromGenra`) diesen Schritt; liegt er vor der Position, zählt er zum nächsten Durchlauf.
- **PLAY/SKIP ohne GENRA:** nur Trigger/MIDI: alle 16 Treffer einer Spur ein Durchlauf; CLOCK ohne RESET: alle 128 Schritte.
- **RANDOM gleichmässig** im Bereich um das Grund-Sample, der im Ordner liegt (nicht mehr an den Rändern gestaut).
- **UNDO:** RND-Buchse nur in den eigenen Stapel (nicht Strg+Z), ein gedrehter RANDOM-Regler ein Eintrag pro Drehung; neuer Ordner / Kit leert den eigenen Stapel. 12/20-Bit-WAV (Container 2/3 Bytes) richtig gelesen. Choke stoppt auch ein per HUMANIZE verzögertes O.HAT. `print()` schreibt `lastSlot` nicht mehr aus dem UI-Thread.

### 2.28.0 – Song-Teile über die Zeile statt über den Loop
- Problem (Rückmeldung): MAKE SONG mit kurzem Loop – ein BREAK aus 1 Takt lief in einer 8/16-Takt-Zeile 8–16-mal hintereinander, jedes Mal mit dem Snare-Auftakt; BUILD rollte pro Loop neu an.
- `Pattern::partTag` (1 + Abschnitt) und `partMask` (geformte Spuren), Record `ST_PART = 11`. `makeSection` setzt sie; `generateTrack` löscht das Bit seiner Spur, `copyTrack` nimmt es mit.
- Das Backen (`introDrums` … `outroMelody`) legt nur noch das Material fest (ohne Verlauf). Der Verlauf läuft beim Abspielen: `shapeDrums` / `shapeNote` (Edit.cpp) mit `PartPos` (Position und Länge: in einer Song-Zeile die Zeile, sonst der Loop). GENRA: `curPartPos()` aus `barIndex`, `arrRowEnd`, `arrRowBars`. MIDI: `renderRange(..., asRow)`.
- BREAK ab 8 Takten: erste Hälfte ohne Drums, danach Hats/Perc lauter werdend; Auftakt nur auf dem letzten Schlag der Zeile. BUILD: ein Wirbel über die Zeile. INTRO/OUTRO wie vorher, aber über die Zeile.
- Phrasen-Fills: VERSE/DROP-Zeilen ab 16 Takten füllen jeden 8. Takt (`partPhraseFill`, nicht der letzte Takt). MAKE SONG setzt FILL auf VERSE/DROP vor einem anderen Teil. TRANCE-Form: BREAK 32 → 16 (danach BUILD 16).
- Alte Teile (ohne `partTag`) spielen wie bisher. Test: `test_ablauf` „parts over the row“.

### 2.27.1 – ABLAUF: PLAY / STOP / LOOP als grosse Pads
- `BigPad` aus `Genra.cpp` nach `src/ui/BigPad.hpp` (Namespace `genraui`), neu: `setSize()`, `latch` (Schalter, für LOOP), Symbol `I_LOOP`. ABLAUF nimmt 8-mm-Pads; die Positionen kommen aus `tools/ablauf_panel.py` (`bigpad()`), Params/Lichter unverändert.
- PLAY leuchtet im SONG-Modus, solange der Song läuft, im PATT-Modus, solange GENRA läuft; STOP blinkt kurz (`stopFlash`). Verhalten: PLAY startet immer ab der gewählten Zeile (auch beim Laufen), STOP wie bisher (zweimal = Song-Anfang), LOOP ist ein Schalter.

### 2.27.0 – GOLANA und GOLANA FX
- Port der FM-1-Firmware **Golana 1.3** (Gadlele, Fork von Felucca, GPL-3.0, github.com/Gadlele/Golana): Quellen
  unverändert in `golana/` (firmware/src, hal; `golana/gen`: die erzeugten Tabellen aus `tools/build.py generate`,
  ohne Bildschirm-Schriften; Sample-Herkunft in `golana/assets/samples-cc0`). 42 HP + Expander 14 HP, Akzent cyan.
- Kern `glcore/glcore.c`: übersetzt die Firmware wie ihre Host-Renders (`tests/hostsim.c`, `tests/golana_sound.c`):
  Engines, Stimmen, Sequencer/Arp und MIDI, Effekte – ohne Bildschirm, Tasten und Hauptschleife. Teil 1 spielt GOLANA,
  Noten kommen als MIDI (der Arp hört sie), Regler schreiben die Parameter, `gl_sync` leitet daraus ab, was die
  Firmware liest (wie Golanas Oberfläche nach einer Änderung). Schnittstelle `gl_get` (API 1): render 128 Frames,
  MIDI, Parameter (Bereich, Text der Firmware), `GLK_*` → Parameter-Id, virtuelle Werte (Arp-RES, Delay-Farbe/-Pegel,
  BPM), Presets, LFO-Retrigger, Sync, Stimmen/Noten. Gebaut als `res/glcore.{dll,so,dylib}`, eine Kopie pro Instanz
  (`Genra/run/glcore`), wie K-WASUB. ≈ 0,3 % eines Kerns.
- Presets: 30 eigene nach Stil (`glcore/golana_genra.c`, im Format von `golana_factory.c`) + Golanas 25 Werksklänge
  (GOLANA ORIGINALS). Pegel pro Preset aus `test/glcore_test.c levels` → `src/golana/GolanaLevels.hpp`.
- Modul (`src/golana/Golana.cpp`): Bindungstabelle Regler/CV → Parameter (wie K-WASUB); Felder per `GolanaUI.hpp`;
  Screen (Spur mit Nachleuchten aus dem Ausgang, Filterkurve, klingende Noten); GENRA-Leiste mit MOD und RESET;
  RETRIG-Eingänge je LFO (setzen Phase aller Stimmen auf 0); FX-Summen = INSERT-MIX, DLY- und REV-Send; die
  EIN-Taster des Expanders schalten sie stumm. Delay-COLOR und -MIX (bei Golana fest) sind im Expander frei.
- Test `test/glcore_test.c` (läuft im Build): alle 55 Presets klingen, bleiben endlich, klingen aus; Clamping,
  virtuelle Werte, Retrigger/Sync.
- [ ] Test in Rack (Regler-Gefühl, Arp an GENRAs Clock, Expander-Kopplung, Pegel).

### 2.26.1 – SCHLAG: Samples und Root im Windows-Fenster
- Die aufklappenden Ordner-Menüs waren zu unübersichtlich: Klick auf eine Spur im Display öffnet jetzt `osdialog_file(OSDIALOG_OPEN)` (WAV-Filter, Start im Ordner der Spur bzw. im Root), ausgelöst im nächsten Frame (`Display::pickTrack`, nicht im Mausdruck). Ohne Root kommt zuerst das Ordnerfenster (`chooseRoot()`, `OSDIALOG_OPEN_DIR`). Das Eintippen des Roots ist weg.
- Rechtsklick: eigene Abschnitte *SAMPLE ROOT FOLDER* (Root anzeigen, *Choose another root folder …*) und *SAMPLES* (die acht Spuren mit ihrem Sample).

### 2.26.0 – SCHLAG: Sample-Drums nach dem Polyend Play
- Neues Modul **SCHLAG** (46 HP, Drum-Farbe `#7b6cff`). Panel: `tools/schlag_panel.py real` → `res/Schlag.svg`, `src/schlag/SchlagLayout.hpp` (mit Tabellen `*_T[track]`, `K_T/FN_T[track][row]`); Vorschau `docs/entwuerfe/schlag.png`.
- **Klang ohne Rack** (`src/schlag/SchlagDsp.hpp`, Test `test/test_schlag.cpp` in CI): WAV-Leser (PCM 8/16/24/32, Float 32/64, Extensible, max. 30 s, erste zwei Kanäle), Stimme mit Hermite-Interpolation, Start/End (rückwärts), Tune, Attack/Decay, DJ-Filter (TPT-SVF, `tan` nur bei Änderung), Drive (tanh), Bits; 18 Funktionen (`Fn`, Reihenfolge wird gespeichert – nur anhängen), Listen für Repeat Type (18), Repeat Grid (19), Chance (18), Action (10); `repeatMod()` (Repeat-Arten nach Gehör), `rollMap()` (RANDOM: pro Schritt Index = Grund ± round(Stärke·(n−1)), gleich = −1).
- **Ordner:** ein Lade-Thread pro Modul (`Loader`) liest einen Ordner (max. 512 WAV) in eine `Bank`; der UI-Thread tauscht sie ein (`std::atomic<Bank*>`), alte Banken werden nach 3 s gelöscht, Stimmen halten ihr Sample per `shared_ptr`. Haupt-Ordner pro PC in `asset::user("Genra/schlag.json")` (`sampleRoot`), Patches speichern `folder` relativ dazu und `folderAbs`. Ordnerzugriffe über `entries()/isDir()` mit try/catch (Rack wirft bei unlesbaren Ordnern).
- **Schritte:** `map[track][128]` (−1 = Grund-Sample). Position: GENRAs `posPub` (Follow), sonst CLOCK/RESET, sonst Trefferzähler mod 16. Durchläufe (`loopCount`) für Play/Skip: GENRAs Schritt springt zurück bzw. RESET. Im Patch: Namen statt Indizes (`names`, `map`, `prints`), aufgelöst, sobald der Ordner gelesen ist (`Pending`/`resolve`).
- **Würfeln/BASIC/UNDO im UI-Thread** (RANDOM über `RandKnob::onDragMove`, also nur echtes Drehen, nicht Undo/Preset), eigener UNDO-Stapel (32) plus `history::ModuleAction` für Strg+Z. RND-Eingang/KIT-Eingang setzen Anfragen (`rndMask`, `kitNext`).
- **PRINT:** `printMap[128 Slots]`; `processPrints()` lädt beim Wechsel von `Genra::arrSlotPub` (nur mit ABLAUF, `arrOwner`). ABLAUF selbst bleibt unverändert (Pads tragen weiter nur MIDI-Daten; die Samples liegen im SCHLAG pro Pad-Nummer).
- **Knöpfe:** `FN_PARAM` (Funktion, Tausch per `history::ComplexAction`) und `K_PARAM` (zeigt den Wert der Funktion: beim Wechsel setzt `controlTick()` den Regler auf `fnVal`), CV 1–4 poly. Retrigger mit 3-ms-Ausblendung (zweite Stimme `tail`), Choke C.HAT → O.HAT, Mute/Solo, HUMANIZE über eine Verzögerung, Repeats über `Track::Rep`. Ausgänge mit `genra_out::limit`.
- **MIDI IN** (`midi::InputQueue`, `gmDrumVoice`). **GRID:** `midiAuto` (JSON, alte Patches mit gewähltem Gerät bleiben manuell) – der Widget-`step()` übernimmt alle 0,5 s Treiber/Gerät/Kanal von `schlag_link::midiSource()`; Kopfzeile „MIDI ‹ SCHLAG“ bei REC.
- **GENRA FX:** `schlagRef`; `feedInputs()` summiert MIX-Send und `schlag_link::sendFrame()` (nicht, wenn im SCHLAG ein Kabel im Send steckt oder *Sends to GENRA FX* aus ist). SCHLAG zeigt eisblaue Segmente über SEND A/B.
- Modul-Reihenfolge: SCHLAG vor DRUM-X.

### 2.25.0 – GENRA: Transport-Pads, Swing pro Spur/Stimme
- **Transport:** sechs große Pads FILL · NEW ALL · RUN · STOP · TAP · MAKE SONG (`BigPad` in `Genra.cpp`: Panel zeigt das unbeleuchtete Pad mit Symbol, das Modul füllt es beim Leuchten). Neue Params/Lichter angehängt: `FILLPAD/STOP/TAP/MAKESONG` (NEW und RUN behalten ihre Ids). HARMONY kompakter.
  - RUN-Pad startet immer vom ersten Schritt (`restart()`, auch beim Laufen); der RUN-Eingang schaltet weiter um. STOP hält an.
  - GENRA ist nach Laden/Einsetzen gestoppt (`running` aus dem Patch wird ignoriert; ein ABLAUF-Song startet es wie bisher).
  - FILL: Fill über den nächsten Takt (`fillPadReq` → am Taktstrich `arrFillEnd = step + 16`, danach gilt wieder die Entscheidung des Durchlaufs, `fillWas`).
  - TAP: Mittel der letzten bis zu 4 Abstände, Pause > 2 s beginnt neu; setzt BPM nur ohne Kabel an CLOCK. Leuchtet im Takt.
  - MAKE SONG: leuchtet, wenn ein ABLAUF angedockt ist (`arrAlive`). ABLAUF liest `makeSongReq`: ist der Loop ein unverändertes Pad, MAKE SONG von dort; sonst erst auf das nächste freie Pad speichern („GENRA n“), dann MAKE SONG (`songAfterStore`). Ohne ABLAUF: Meldung im Display.
- **Clock output:** Rechtsklick › *Clock output* 16tel/8tel/Viertel/24 PPQN (`clockOutMode`, JSON, alte Patches 16tel). Gerade Pulse ab Schritt 1; 24 PPQN: 6 Pulse pro Schritt, 5 davon zwischen den Ticks (`ppqnLeft/ppqnIv`).
- **Modul-Reihenfolge** in `plugin.json` und `plugin.cpp`: GENRA, GRID, ABLAUF, MIX, GENRA FX, STOTTER, DRUM-X, GRUND, GRUND FX, GENRA MONO, K-WASUB, K-WASUB FX, FUNKE, UMLAUF, UMLAUF FX, MONK, VOICE, ANIMAL, SCHMELZ.
- **Genre sets tempo:** Rechtsklick-Untermenü On/Off, Standard Off, JSON `genreTempo`. Wechselt das globale Genre (bzw. die Variante), setzt der Widget-`step()` BPM auf das typische Tempo des Profils (nicht mit Kabel an CLOCK).
- **Swing pro Spur / Drum-Stimme:** `Pattern::tSwing[NUM_TRACKS]`, `vSwing[NUM_DRUMS]` in Prozent 50–75, 0 = global (Genre + SWING-Regler). `swingOf()`/`ownSwing()`/`setSwing()`, Record `ST_SWING = 10`, `copyTrack` nimmt sie mit, `generateTrack` lässt sie stehen. GENRA: Drum-Stimmen mit eigenem Swing laufen wie genudgte Treffer einzeln (`queueDrum`). MIDI-Export: Swing pro Stimme. Grid: Badges rechts vom Raster (Raster 9,5 mm schmaler), hoch/runter ziehen, Doppelklick = global; „SW -“ zeigt beim Überfahren den aktuellen Wert. Test: `test_cycle` Teil 7.

### 2.24.0 – 22 neue Genres
- Hinten angehängt (Genre 37–58, Profile 83–126), je 2 Varianten: Progressive House, Organic House, UK Funky, Bassline, Gqom · Big Beat, Breaks, Jersey Club, Baltimore Club, Phonk · Hardcore (Gabber/Happy), Psytrance, EBM · Dancehall, Roots Reggae, Moombahton, Cumbia, Bossa Nova · Drone, Dark Ambient, Ambient Techno, Berlin School. Nur mit vorhandenen Engine-Mitteln (Raster, Stile, Drops, Chops).
- `NUM_GENRES` 59, `MAX_PROFILES` 160. Fünf neue Familien in `FAMILIES` (Genre-Menüs).
- GENRE-CV 0–10 V: `genreCvSpan` (JSON). Fehlt das Feld (Patch vor 2.24), bleibt die Spanne 37, damit gleiche Spannungen die gleichen Genres treffen; neue Module/Initialize: alle 59. Menü *Genre CV 0–10 V range*. 1/12-V-Modus unverändert.
- MAKE SONG: `songFormForGenre` kennt alle neuen Genres (Hardcore/EBM bewusst HOUSE · TECHNO). Tests: `test_engine` prüft, dass Genre 0–36 ihren Platz behalten; `test_ablauf` neue Formen.
- Hörbeispiele: `tools/genre_demo.cpp` (ohne Rack: `generate` + `renderSong`, Bass GRUND, Melodie FUNKE, Pads SCHMELZ bzw. FUNKE-Poly für Stabs/Skanks, einfacher Drum-Synth; −14 LUFS, Spitze −1 dBFS).

### 2.23.0 – Grid: Längen pro Spur/Stimme, Nudge, Spurwerkzeuge, Save to pad, REC MIDI
- **Längen:** `Pattern::tLen[NUM_TRACKS]` (Piano-Spuren) und `vLen[NUM_DRUMS]` (Drum-Stimmen), 0 = folgt dem Loop. Die Auswerter lesen die Pattern-Daten an `cycleStep()` = Loop-Schritt % Länge (Daten, Wahrscheinlichkeitstabellen, Chops); Positionslogik (Fills, Drops, Akkorde, Event-Starts, Chance pro Durchlauf) bleibt am echten Schritt – so lesen tonale Spuren die Akkorde am echten Step. Feste Notenlängen enden am Zyklusende (`cycleCut`). Neustart mit jedem Loop und Pattern-Wechsel ergibt sich daraus (der Schritt beginnt bei 0). `trackNotes`/`drumHits` liefern nur Noten im Zyklus. `generateTrack` setzt die Länge zurück, `copyTrack` nimmt sie mit. Speicher: neuer Record `ST_LENGTHS = 9`.
- **Nudge:** `fnudge` (int8, ±11 in 1/24 Schritt) an DrumStep/NoteStep/PadStep, Feldzahlen 10/21/15 (alte Daten: 0). Auswerter liefern `Hit/NoteEv/PadEv::nudge`, `GridNote::nudge`. GENRA: Verzögerung = Swing + Nudge; liegt sie vor dem Schritt, startet das Ereignis schon im Tick davor (`queueEarly`, eigene Warteplätze `qBassE/qMelE/qPadE`, Drums über `queueDrum` pro Stimme, Flags `earlyDone/earlyDrum`). MIDI-Export: `startTick += nudge` (nie unter 0).
- **Grid:** Endmarken (Griff ziehen, Doppelklick = folgen; `beginRaw()` ohne Einfrieren), Abdunkeln + blasse Wiederholungen, Kopfzeile aus Segmenten (Pad, EDITED, ● REC, SNAP, CHORD FOLLOW, LEN …, BAR, Tonart – Tonart/Takt fallen weg, wenn es zu lang wird). Alt+←/→ Nudge. Rechtsklick › Track tools (Rotieren, Umkehren, doppelt/halb, ±1 Stufe, Velocity ±15 %/angleichen), je ein Undo-Schritt.
- **Save to pad:** ABLAUF schreibt jeden Frame `Genra::arrPadSlot/arrPadLabel` (UI-Thread) und erledigt `padSaveReq` mit `overwrite()`; das Grid setzt die Anfrage erst, wenn GENRA alle Änderungen übernommen hat (`gridState == 0`).
- **REC MIDI:** `REC_PARAM`/`REC_LIGHT` angehängt, `midi::InputQueue` im Grid-Modul (JSON `midi`); `process()` legt Noten mit `Genra::posPub` (Schritt + Bruchteil, nur bei angedocktem GENRA, sonst liest der UI-Thread) in einen Ring, der UI-Thread schreibt sie in die Spur (`recNote`). Drums: `gmDrumVoice()`. Eine Sitzung = ein Undo-Schritt.
- Panel: REC-Taster mit Trennstrich am Ende von EDIT (`tools/grid_panel.py real`). Test: `test/test_cycle.cpp` (in CI).

### 2.22.0 – GENRA FX
- Neues Modul **GENRA FX** (24 HP, eisblau): Send-Effekte für MIX. REVERB (eigener FDN-Hall, 8 Leitungen; HALL/ROOM/PLATE/SPACE, DECAY 0,1–30 s, SIZE, PRE-DELAY, DAMP, WIDTH, SHIMMER, LEVEL, FREEZE mit Gate) auf A, DELAY (1/32–1/1 gerade/punktiert/triolisch oder FREE ms; FEEDBACK bis 110 %, TONE, TAPE, DUCK, WIDTH, LEVEL, Stereo/Ping-Pong) auf B. DSP: `src/genrafx/GenraFxDsp.hpp`, Test `test/test_genrafx.cpp` (in CI). Hallzeit pro Typ korrigiert (gemessene RT60 ≈ Einstellung ±5 %).
- Ohne Kabel: Tempo von GENRA (`follow::genraBpm`), IN A/B von MIX SEND A/B, MIX RETURN A/B von GENRA FX OUT A/B (MIX › RETURN A/B from, Auto als Grundeinstellung). Eisblaue Segmente an beiden Enden; Kabel haben Vorrang.
- Panel: `tools/genrafx_panel.py` → `res/GenraFx.svg`, `src/genrafx/GenraFxLayout.hpp`.

### 2.21.0 – Schalter PATT | SONG, MIDI-Ordner, lautere Presets, eigene MONK-Sänger, licenses/
- ABLAUF: PATT/SONG-Lampen ersetzt durch grossen Zweiweg-Schalter (`ModeSwitch`, gelbe aktive Hälfte `#ffd84a`, Klick wählt die Hälfte; `MODE_PARAM` bleibt). Song-Export pro Spur: Rechtsklick *Export song – drums/bass/melody/pads only* (`exportSong(track)`).
- MIDI-Export ohne Dateidialog (ASIO-Treiber hielt die Engine beim Dialog an): `src/MidiFolder.*` speichert in `<Dokumente>/Midi Files from Genra` als `JJJJ-MM-TT_<name>.mid`, nie überschreiben (-2, -3 …), Meldung „Saved: …“ (GENRA-Display, ABLAUF), Menü *MIDI export folder…* (Einstellung `midiExportFolder` in `asset::user("Genra/plugin-settings.json")`) und *Open MIDI export folder*. Import unverändert mit Dialog.
- Presets laut und gleichmässig: GRUND, MONO, FUNKE, SCHMELZ, K-WASUB je Preset ein versteckter Pegel (`gain` in den Preset-Tabellen, `kwasub/KwasubLevels.hpp`), im Patch und in eigenen Presets als `presetGain`/`gain` (fehlt er: Werks-Pegel des Presets). Sicherheitslimiter `src/OutLimit.hpp` (linear bis 6 V, tanh-Knie bis 8 V) am Ausgang. Messbericht: `preset_lautheit.md`. Tests prüfen das neue Pegelfenster.
- MONK: zwölf eigene Sänger (BOLT, CROAK, DIVA, HOOT, ZORP, BONES, SPIKE, BLUB, LOOP, LUNA, SPORE, EMBER) statt der MONK-1-Zeichnungen. Bilder: `tools/monk_singers.py real` → `res/monk/sNN_BLV.svg` (480 Posen: Blinzeln × 4 Brauen × 5 Vokale, Mund-Innenleben vorab auf den Mund zugeschnitten, weil NanoSVG keine clipPaths kann). `ScreenWidget`: eigene Idle-Animation, Pose höchstens 30× pro Sekunde, Framebuffer neu nur bei Posenwechsel; BOLTs Antenne glüht mit der Lautstärke. Stimmen in `MonkSingers.hpp`, gleicher Index wie vorher. `src/monk/fw/` und `monkface.*` entfernt.
- `licenses/` im Paket mit allen Fremd-Lizenztexten, `licenses/README.md`. plugin.json: Links aufs Repo (authorEmail leer).
