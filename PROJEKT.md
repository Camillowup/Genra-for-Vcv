# GENRA – Projektstand

**Stand:** 5. November 2026 · **Version 3 in Arbeit (2.2.0)** · Repo: `CamilloGF/genra-v3`
Vorgänger: v1 `CamilloGF/vcv-sequenzer-Genra` (2.0.0), v2 `CamilloGF/Genra-v2` (2.1.0, läuft in Rack)
Plattform: VCV Rack Pro 2.6.6 unter Windows · Build: GitHub Actions (`.github/workflows/main.yml`)

## Konzept
Generativer, genreabhängiger 4-Spur-Sequencer für VCV Rack 2. Erfindet Loops (1/2/4/8 Takte) für **8 Drumstimmen, Bass, Melodie und polyphone Pads (4 Stimmen)**. Alle Spuren teilen Tonart und Akkordfolge.

**Genres (14):** Techno, House, UK Garage, 2-Step, Dubstep, Dub, Hip-Hop, Lo-Fi, Trap, Drum & Bass, Breakbeat, Trance, Synthwave, Ambient

## Frontplatte (42 HP)
- **Display:** Genre, Tonart/Tonleiter, BPM, Swing, Akkordfolge (klick-/scroll-editierbar), Step-Lauflichter der 4 Spuren
- **Global:** GENRE, KEY, SCALE (Auto + 9 Tonleitern), BPM, SWING, COMPLEX (Rhythmus), COLOR (Harmonik), MUTATE, LENGTH · Taster NEW, RUN, CHORD LOCK
- **Drums:** DENSITY, FILL, GHOST, MUTE, LOCK · Ausgänge KICK, SNARE, CLAP, RIM, C.HAT, O.HAT, TOM, PERC (Triggerhöhe = Velocity)
- **Bass:** DENSITY, MOVE, OCT, MUTE, LOCK · V/OCT, GATE, VEL, ACC
- **Melodie:** DENSITY, RANGE, OCT, MUTE, LOCK · V/OCT, GATE, VEL, ACC
- **Pads:** DENSITY, VOICING, OCT, MUTE, LOCK · V/OCT + GATE (poly 4), VEL, MOD
- **Eingänge:** CLOCK, RESET, RUN, NEW, GENRE CV, MUTATE CV, TRANSP (V/Oct), CHORD (poly)
- **Rechtsklick-Menü:** MIDI-Export/-Import, Akkord-Modus (gemischt/Vorlagen/frei), feste Akkordvorlage, nur Akkorde neu würfeln, Clock-Auflösung (16tel/8tel/Viertel/24 PPQN)

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
| `src/Genres.cpp` | 14 Genre-Profile, Tonleitern |
| `src/Midi.cpp` | SMF-Export/-Import |
| `src/Genra.cpp` | Rack-Modul: Clock/Swing, Ausgänge, Display, Menüs, Speichern |
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

**Frontplatte (Entwurf, 58 HP):** `tools/panel.py` ist schon auf das neue Layout umgestellt, `res/Genra.svg` und `src/Layout.hpp` sind aber noch v2 – erst neu erzeugen (`python3 tools/panel.py real`), wenn `Genra.cpp` die neuen Elemente kennt.
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

**Offen**
- [ ] Genre pro Spur (Feld + Menü, Wechsel am Taktanfang), GENRE-CV pro Spur
- [ ] CV: DENSITY pro Spur, VARIANT, COMPLEX, COLOR, FILL-Trigger
- [ ] Swing pro Spur, Länge pro Spur (Polymeter)
- [ ] Szenen (8 Plätze, STORE, CV, NEXT)
- [ ] Frontplatte erzeugen, Build, Test in Rack

**Tests (ohne Rack):**
```
g++ -std=c++11 -O2 -Isrc src/Engine.cpp src/Genres.cpp src/Midi.cpp src/Store.cpp test/test_store.cpp -o t && ./t
```
(ebenso `test_midi.cpp`, `test_engine.cpp`)
