# GENRA – Projektstand

**Stand:** 11. November 2026 · **GENRA 2.33.0** (15 Module) und **GENRA Instruments 2.33.0** (9 Module), Windows + macOS
(arm64/x64, Mac noch ungetestet) · Repo `Camillowup/Genra-for-Vcv` (vorher `CamilloGF/genra-v3`, Account nicht mehr
erreichbar) · Plattform: VCV Rack Pro 2.6.6 unter Windows

| Wo | Was |
| --- | --- |
| `docs/UEBERSICHT.md` | **Wo liegt was:** jedes Modul → Code-Ordner, Panel-Skript, Panel-Bild, Test |
| `docs/DESIGN.md` | Design-Leitfaden für neue Module (Look, GENRA-Leiste, Leuchtsegmente, Follow, englische UI) |
| `docs/MERKLISTE.md` | offene Wünsche und geplante Instrumente (gebaut wird erst auf „bauen“) |
| `docs/GENRA_BOX.md` | Spezifikation GENRA BOX + ABLAUF MINI |
| `CHANGELOG.md` | Versionsgeschichte (was kam wann dazu) |
| `docs/archiv/` | Erledigtes aus der Merkliste, Freak-Notizen (auf Eis), alte Bilder |

## Plugins

- **GENRA** (Slug `Genra`, Ordner `src/`): GENRA, GRID, ABLAUF, MIX, GENRA FX, STOTTER, SCHLAG, DRUM-X, GRUND, GRUND FX,
  GENRA MONO, FUNKE, SCHMELZ, GENRA BOX, ABLAUF MINI.
- **GENRA Instruments** (Slug `GenraInstruments`, Ordner `instruments/`): K-WASUB, K-WASUB FX, GOLANA, GOLANA FX, UMLAUF,
  UMLAUF FX, MONK, VOICE, ANIMAL. Benutzt die gemeinsamen Teile aus `src/` (Knöpfe, Follow GENRA, Link-Hub, UI) über
  kleine Weiterleitungs-Header, nicht als Kopie.
- **GENRA Test** (eigenes Repo `Camillowup/testmodulesvcv`): neue Instrumente zum Ausprobieren (MATH, KURBEL, SWARM, LIED).
  Holt die gemeinsamen Teile vor jedem Build frisch aus diesem Repo (Branch `main`).
- Später evtl. alles zu einem grossen GENRA-Plugin zusammenführen (offen).

## Arbeitsweise

- **Branches:** `dev` = Arbeitsstand, `main` = getestet, `release` = ein Push dorthin veröffentlicht den GitHub-Release
  (nur, wenn ausdrücklich „Release“ gesagt wurde).
- **Builds:** GitHub Actions bei jedem Push (Windows + Mac), Download über Actions › Artifacts. Wenige Pushes, Änderungen
  gesammelt (der frühere Account wurde wegen zu vieler Builds gesperrt). Commits nur mit Doku: `[skip ci]`.
- **Neue Version:** `version` in `plugin.json` (und `instruments/plugin.json`) erhöhen, Hauptversion bleibt 2.
- **Alte Patches:** Slugs, Modulnamen und Parameter-IDs bleiben; neue Params/Buchsen nur hinten anhängen
  (Regel in `docs/DESIGN.md`).
- **Modul-Umzug:** Wandert ein Modul in ein anderes Plugin, kommt es im selben Schritt aus dem alten heraus.
- **Panels:** Jedes Panel entsteht aus einem Skript in `tools/panels/` (`python3 tools/panels/<modul>_panel.py real` →
  `res/<Modul>.svg` + `src/<modul>/<Modul>Layout.hpp`), nie von Hand.

## Konzept GENRA

Generativer, genreabhängiger 4-Spur-Sequencer für VCV Rack 2. Erfindet Loops (1/2/4/8 Takte) für **8 Drumstimmen, Bass,
Melodie und polyphone Pads (4 Stimmen)**. Alle Spuren teilen Tonart und Akkordfolge. 64 Genres mit 138 Varianten
(Liste im README).

**Wichtige Designentscheidungen**
1. **Zufallszahlen statt fertiger Noten:** Jeder Step speichert Zufallswerte + abstrakte Tonhöhe (Akkordton/Stufe). Ob/welche
   Note klingt, wird beim Abspielen berechnet → fast alle Regler wirken live (DENSITY, KEY, SCALE, COLOR, VOICING, OCT,
   RANGE, MOVE, COMPLEX).
2. **Genre-Profile als Wahrscheinlichkeitsraster** (16 Zeichen/Takt, `.`=nie, `1`–`9`) in `src/genra/Genres.cpp`.
3. **Akkorde:** Genre-Vorlagen (römische Stufen) + freie Erzeugung (Funktionsharmonik × Genre-Gewichte). COLOR steuert live
   7er/9er, Zwischendominanten, Tritonus-Substitution und geborgte Akkorde.
4. **Bass akkordrelativ** (folgt CHORD-Eingang), an Kick gekoppelt je Genre (folgen/meiden).
5. **Melodie:** Motiv (1 oder 2 Takte) → Phrase A A A' / M M', schwere Zählzeiten rasten auf Akkordtöne.
6. **Pads:** Voice Leading (minimale Bewegung), Stile Sustain/Stabs/Skank.
7. **LOCK:** jede Spur merkt sich ihr eigenes Genre; Genrewechsel greift am nächsten Taktanfang.
8. **MIDI:** Export Typ 1 (Drums Kanal 10/GM, Bass 1, Melodie 2, Pads 3, Swing inklusive); Import spielt notengenau ab.
9. **Pattern-Speicherung:** gesamtes Pattern als Base64 im Patch (`dataToJson`).

## Build

- **GitHub Actions** (`.github/workflows/main.yml`): Windows mit MSYS2 **MINGW64** (nicht UCRT – CRT muss zu Rack passen),
  Mac (arm64 + x64). Version wird aus `plugin.json` gelesen. Vorher laufen die Tests aus `test/` (ohne Rack).
- **Von Hand:** `make RACK_DIR=<Rack-SDK> dist` im Hauptordner (GENRA) bzw. in `instruments/` (GENRA Instruments).
  Die Firmware-Quellen (`jiant.tar.gz`) werden beim ersten Build entpackt.
