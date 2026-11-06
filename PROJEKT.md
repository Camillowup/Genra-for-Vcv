# GENRA – Projektstand

**Stand:** 6. November 2026 · **Plugin 2.8.1: 15 Module** (GENRA, GRID, VOICE, DRUM-X, ANIMAL, MONK, GENRA MONO, FUNKE, SCHMELZ, MIX, K-WASUB, K-WASUB FX, UMLAUF, UMLAUF FX, STOTTER) · Repo: `CamilloGF/genra-v3`
Vorgänger: v1 `CamilloGF/vcv-sequenzer-Genra` (2.0.0), v2 `CamilloGF/Genra-v2` (2.1.0, läuft in Rack)
Plattform: VCV Rack Pro 2.6.6 unter Windows · Build: GitHub Actions (`.github/workflows/main.yml`)

## Konzept
Generativer, genreabhängiger 4-Spur-Sequencer für VCV Rack 2. Erfindet Loops (1/2/4/8 Takte) für **8 Drumstimmen, Bass, Melodie und polyphone Pads (4 Stimmen)**. Alle Spuren teilen Tonart und Akkordfolge.

**Genres:** v1 14 · v2 24 (46 Varianten) · v3 37 (83 Varianten), Liste im README

## Frontplatte (42 HP)
- **Display:** Genre, Tonart/Tonleiter, BPM, Swing, Akkordfolge (klick-/scroll-editierbar), Step-Lauflichter der 4 Spuren
- **Global:** GENRE, KEY, SCALE (Auto + 9 Tonleitern), BPM, SWING, COMPLEX (Rhythmus), COLOR (Harmonik), MUTATE, LENGTH · Taster NEW, RUN, CHORD LOCK
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
- Farben (2.8.1): Jede Spur hat die Farbe ihrer Instrumente – DRUMS blau-violett `#7b6cff` (= DRUM-X), BASS säuregrün `#b8f23a` (= K-WASUB und MONO), MELODY cyan `#3dc9ff` (= FUNKE), PADS rosa `#e0779e` (= SCHMELZ); SCENES gelb `#ffd23d`. Gilt auch für GRID, MIX und die STOTTER-Pads.
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
| `src/Genres.cpp` | Genre-Profile (37 Genres, 83 Varianten), Tonleitern |
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
- [ ] Später: Länge pro Spur (Polymeter), Szenen verketten (Song-Modus), Seed merken, Mikro-Timing
- [ ] Ganz am Schluss: alle Genres nach Gehör feinschleifen

**Offen**
- [ ] Test in Rack (v3.1: neue Ausgänge, Szenen-Farben, CHANCE, Mehrfachauswahl) (Genre-Feld-Menü, CV-Eingänge, Szenen, v2-Patch laden)
- [ ] Länge pro Spur (Polymeter) – zurückgestellt: tonale Spuren müssten Akkorde über den echten Step statt den umgebrochenen lesen
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
- 4-Kanal-Stereomixer, 32 HP, Modul-Slug `GenraMix`. Kanäle in den GENRA-Spurfarben, Master dunkel (Balken
  `#4a4c57`), Ausgänge grün getönt wie die Pegelanzeigen. Panel: `tools/mix_panel.py` → `res/Mix.svg` +
  `src/mix/MixLayout.hpp`.
- Engine `src/mix/MixDsp.hpp`: RBJ-Kuhschwänze 120 Hz / 6 kHz (±12 dB, übersprungen wenn flach), Balance (Gegenseite
  fällt mit Viertelsinus), Pegelkurve quadratisch (0,75 = 0 dB), Gain-Glättung ~10 ms (Mute/Solo klickfrei), Send nach
  dem Fader, Ducking: Trigger ≥ 1 V startet eine Kosinus-Erholung über TIME (Tiefe max. −26 dB), Width über M/S,
  Drive mit Padé-Sättigung und Lautheitsausgleich, Limiter ab 8 V weich gegen 10 V, Spitzenpegel-Anzeigen
  (12 Segmente, −36 … +3 dB, 5 V = 0 dB).
- Test `test/test_mix.cpp`: Einheitsverstärkung, Pegelkurve, EQ ±12 dB, Balance, Mute/Solo, Send/Return, Ducking,
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
