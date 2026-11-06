# GENRA-Plugin: Design-Leitfaden für neue Module

Alle Entscheidungen, die über die Module hinweg gelten. Ein neues Modul hält sich daran, damit es aussieht und sich
bedient wie die anderen. Stand: Plugin 2.15.2.

## Ablauf bei einem neuen Modul

1. **Zuerst das Panel zeigen** (Entwurf als Bild, mit den echten Rack-Bauteilen), erst nach der Freigabe rechnen und
   Code schreiben.
2. Panel-Skript `tools/<modul>_panel.py` schreibt `res/<Modul>.svg` (Text als Pfade) und den Layout-Header
   `src/<modul>/<Modul>Layout.hpp`; Vorschau nach `docs/entwuerfe/`.
3. Bauen und Testen nur über GitHub Actions (Windows); Tests unter `test/` in `.github/workflows/main.yml` eintragen.
4. Version in `plugin.json` erhöhen, `README.md` (Bedienung, deutsch) und `PROJEKT.md` (Technik, deutsch) nachführen.

## Aussehen

- **Hintergrund und Flächen:** BG `#17181d`, Platten `#212329`, Linien `#34363f`, Text `#d9dbe2`, gedimmt `#8a8d99`,
  Display/dunkle Felder `#07080a`. Titel gross in Nunito, darunter „GENRA · …“ als Untertitel.
- **Eine Akzentfarbe pro Modul**, die nach der Spur gewählt wird, die das Modul meist spielt:
  Drums `#7b6cff` (violett, DRUM-X), Bass `#b8f23a` (grün, MONO, K-WASUB), Melodie `#3dc9ff` (cyan, FUNKE),
  Pads `#e0779e` (rosa, SCHMELZ). Bernstein ist nicht erwünscht.
- **Drehregler:** nur die eigenen dunklen Knöpfe mit weissem Zeiger (`GLargeKnob`, `GKnob`, `GSmallKnob`, `GTrimpot`
  aus `src/plugin.hpp`), nie Racks silberne/helle.
- **Ausgänge** auf farbigen Platten (Akzent auf der Platte gemischt), Eingänge ohne Platte; Ein- und Ausgänge farblich
  klar getrennt. Audio-Ausgänge L/R unten rechts.
- Breite nach Inhalt, so schmal wie sinnvoll.
- **Expander** (z. B. GRUND FX): eine Sektion pro Reihe, kleine Regler; die CV-Buchsen sitzen **in ihrer Sektion** rechts
  auf einem dunklen Feld (keine Buchsenleiste unten). Ein/Aus-Taster links oben in jeder Sektion, LINK-Anzeige im Kopf.
  Instrumente selbst behalten die GENRA-Leiste unten.

## Buchsen

- **GENRA-Leiste** unten links an jedem Instrument (`panellib.genra_strip`): Ablage `#141519`, Raster 9,4 mm,
  Reihenfolge **V/OCT · GATE · VEL · ACC · SLIDE · MOD** (nur die, die das Modul hat), Linie in der Spurfarbe,
  **gedruckt gedimmt** (`panellib.dim`, 35 %). Daneben mit 2,2 mm Abstand **CLOCK · RESET** mit grauer Linie.
  Weitere CV-Eingänge rechts davon, dann L/R.
- **Alle Ein- und Ausgänge bleiben immer erhalten** (auch für fremde VCV-Module); Neues wird nur ergänzt.
- **Enums nur hinten anhängen** (Params, Inputs, Outputs, Lights): alte Patches behalten ihre Kabel.
- Polyphone Eingänge wie bei GENRA: Pads poly 4, DRUMS poly 8 (Reihenfolge Kick, Snare, Clap, Rim, CH, OH, Tom, Perc;
  Spannung = Velocity).

## Verbindungen ohne Kabel (Follow GENRA, MIX)

- Ein neues Instrument ist ein `follow::Host` (`src/Follow.hpp`): im Konstruktor `fol.bind(INPUT, follow::VOCT …)`
  für jede Leisten-Buchse, `audioL/audioR`, `srcName`; `fol.process(this, args.frame)` als erste Zeile in `process()`;
  `onPortChange` → `fol.portChange(e)`; `onRemove` → `follow::gone(this)`; JSON `fol.toJson/fromJson`;
  Menü `fol.appendMenu`; Widget `follow::addStrip(this, module, {Buchsen-Positionen})` nach `setPanel`.
- Stereo-Ausgang: rechnet stereo, wenn R gesteckt ist **oder** `pulled(args.frame)` (MIX nimmt es ohne Kabel).
- Ein gestecktes Kabel hat immer Vorrang.
- **Anzeige:** eine Verbindung ohne Kabel ist an beiden Enden als **kleines Leuchtsegment über der Buchse** zu sehen
  (`follow::segment`): an Eingängen in der Spurfarbe, CLOCK/RESET weiss; an Ausgängen dasselbe, dazu ein kurzer Text,
  wohin (GENRA: „OUT › MONO“, Instrument: „› MIX CH 2“). Keine Ringe um ganze Buchsen.

## Bedienung

- Tooltips, Menüs und Panel-Beschriftung **auf Englisch**; README/PROJEKT auf Deutsch.
- Verbindung zu GENRA/anderen Modulen per **Rechtsklick** („Connect to…“, „Follow GENRA“), nicht nur per Andocken.
- Löschen von viel (z. B. CLR ALL): zweimal drücken, mit UNDO zurückholbar.
- Presets per Dropdown im Kopf des Panels (Werks-Presets nach Stil, eigene speicherbar).

## Rechenzeit

- Teure Formeln (`pow`, `exp`, `tan`) nur bei Änderung oder im Steuertakt, nicht pro Sample.
- In Stille ruhen (Effekte nach dem Ausklingen schlafen lassen, stumme Stimmen überspringen).
- Displays nur rechnen, wenn sie sichtbar sind; keine Engine-Aufrufe (`APP->…`) pro Sample.
