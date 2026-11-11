# GENRA BOX + ABLAUF MINI – Spezifikation

Stand: 11. November 2026 · Plugin 2.31.1 · Panels: `docs/entwuerfe/genra_box.png`, `docs/entwuerfe/ablauf_mini.png`
(erzeugt mit `tools/genrabox_panel.py` und `tools/ablaufmini_panel.py`, die auch `src/box/*Layout.hpp` schreiben).

Erstmals gebaut in 2.20.0–2.23.0 (auf dem alten Stand), in 2.31.0 auf 2.30 neu aufgesetzt und erweitert.

## Idee
Eine Groovebox aus zwei Modulen: **GENRA BOX** (40 HP) = GENRAs Generator mit vier eingebauten Stimmen
(DRUM-X, GENRA MONO, FUNKE, SCHMELZ), **ABLAUF MINI** (16 HP) = Songteile auf 16 Pads, kabellos verbunden (LINK).
Alle Regeln aus `docs/DESIGN.md` gelten.

## GENRA BOX
- Code: `src/box/GenraBox.cpp` (erbt von `Genra`, mit zusätzlichen Params/Inputs/Outputs/Lights über den
  Konstruktor `Genra(extraParams, extraInputs, extraOutputs, extraLights)`), Stimmen in `src/box/BoxVoices.hpp`.
- **Kopf:** Genre-Feld wie bei GENRA. **Display:** BPM-Feld (ziehen, Mausrad, Doppelklick = Genre-Tempo, Rechtsklick =
  eintippen), INT/EXT, Tonart · Länge · Takt, Lauflichter, SOUND-Modus („SOUND: SAME GENRE“) bzw. CHORDS LOCKED.
- **Generator-Regler:** KEY, COLOR, SWING, LENGTH, MUTATE (GENRAs MUTATE_PARAM; Modus, Umfang, „Back to the anchor“ im
  Menü). Taster NEW ALL, LOCK, RUN, SOUND.
- **PATTERN pro Spur (obere Sektion):** Genre-Feld, DENSITY, zweiter Regler, NEW, HOLD.
- **SOUND pro Spur (untere Sektion):** Preset-Feld, zwei Makros, LEVEL, MUTE.
  - DRUM-X: Kits, TUNE/DECAY. GENRA MONO (`gmono::Voice`): CUTOFF (nach unten höchstens 35 % des Preset-Werts, damit ein
    dunkler Sub nicht verstummt) / RESO, immer mono. FUNKE: BRIGHT/SPACE. SCHMELZ: MOTION/SPACE.
- **SOUND / Presets würfeln:** Standard „Same genre“ (`soundAll` = 2). `presetModeFor(t)` gibt 0 bei SOUND aus oder
  HOLD. Würfeln: `Genra::presetRoll[t]` zählt NEW ALL (nicht gesperrte Spuren) und NEW einer Spur; die Box ruft dann
  `rollPreset(t)` (Kandidaten aus `genrematch::candidates`, nie das geladene Preset, sonst alle Genres).
- **Grosse Instrumente:** Folgt eines einer Spur, schweigt die eigene Stimme; Preset-Feld und Würfeln gehen über die
  Host-API des Link-Hubs (`hostPresetCount/Info/Current/Load`, char-Puffer, LINK_VERSION 2).
- **DUCK:** Hüllkurve auf der Kick (3 ms an, 150 ms zurück), Bass und Pads × (1 − 0,85 · DUCK · Hülle).
- **Buchsen:** CLOCK, RESET, RUN, NEW rein; CLOCK OUT (GENRAs CLOCK_OUTPUT, Auflösung im Menü), NOTES, GATES, DRUMS,
  Spur-Ausgänge L/R, Summe L/R. Nur L gesteckt = Mono-Summe.

## ABLAUF MINI
- Code: `src/box/AblaufMini.cpp`, Slug `GenraAblaufMini`. Arbeitet über GENRAs Arranger-Mailboxen wie ABLAUF
  (Snapshot, Cue, `arrOwner`), alles auf dem UI-Thread.
- **Regeln = die des grossen ABLAUF (ab 2.31.1).** ABLAUF ist der Host, MINI folgt; eine neue Regel dort gilt auch hier.
- PARTS: jeder Druck = neue Variante aufs nächste freie Pad (02–16, ROOT zuletzt) aus dem spielenden Pattern;
  Session-Regel wie `Ablauf::part` (spielt eine Variante der Session unverändert: dieselbe Quelle). Nichts gespeichert:
  das Spielende wird zuerst ROOT. Break bass / Intro bass im Rechtsklick-Menü (`PartOpts`).
- Pads: leer = speichern (eigene Familie; ROOT auf 01, sonst „GENRA n“), belegt = springen am Pattern-Ende. UNDO, CLEAR.
- **DR · BA · ME · PA:** Keep / Re-roll / Vary (Vary: `varySection` von der letzten Variante desselben Teils der Familie).
- **SONG:** wie `Ablauf::makeSong`, aus der Familie des spielenden Pads (sonst ROOT). Vorhandene Varianten zuerst,
  fehlende neu, Pads voll = Original. BUILD-Länge = `Genre::buildBars`. Fill vor dem Wechsel aus VERSE oder DROP.
  Wiedergabe: pro Teil ein Cue mit `rowBars` und `atBar = rowStartPub + bars`, am Ende ein Stopp-Cue. Song in JSON
  (`song`). GENRAs MAKE-SONG-Pad (`makeSongReq`) startet ihn aus dem laufenden Loop.
- NEXT/RESET rein, DROP/END raus. MIDI ohne Dialog in den Export-Ordner (`midifolder::save`).

## Weiter offen
- GENRA Instruments (K-WASUB, UMLAUF …) haben die Preset-Host-API noch nicht.
