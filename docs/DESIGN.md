# GENRA und GENRA Instruments: Design-Leitfaden für neue Module

Alle Entscheidungen, die über die Module hinweg gelten. Ein neues Modul hält sich daran, damit es aussieht und sich
bedient wie die anderen. Stand: GENRA und GENRA Instruments 2.29.1.

## Zwei Plugins, eine Familie

- **GENRA** (Hauptordner): Sequencer und alles drumherum – GENRA, GRID, ABLAUF, MIX, GENRA FX, STOTTER, SCHLAG – und
  die Grund-Instrumente DRUM-X, GRUND (+FX), MONO, FUNKE, SCHMELZ. Es bleibt **schlank**: neue Instrumente kommen nicht
  hierher.
- **GENRA Instruments** (`instruments/`, Slug `GenraInstruments`): alle weiteren Instrumente (K-WASUB, GOLANA, UMLAUF,
  MONK, VOICE, ANIMAL samt Expandern). Neue Instrumente kommen hierher.
- Beide tragen die Marke **Genra** (im Rack-Browser zusammen) und werden im selben Build gebaut und im selben Release
  veröffentlicht. Sie sollen sich später ohne Umbau wieder zu **einem** grossen GENRA-Plugin zusammenführen lassen.
  Dafür gilt für jedes neue Instrument:
  - **Eindeutige Namen** über beide Plugins: Modul-Slug, Klassenname, Panel-Datei, Layout-Header gibt es nur einmal.
  - **Eigener Ordner:** Code unter `instruments/src/<name>/`, Panel unter `instruments/res/`, Panel-Skript
    `tools/<name>_panel.py` schreibt dorthin; vendorte Fremdquellen unter `instruments/<name>/`.
  - **Gemeinsames nicht kopieren:** Follow, Knöpfe, GENRA-Leiste, Preset-Feld, Limiter kommen aus `src/` (über die
    Weiterleitungs-Header in `instruments/src/`, gebaut über `instruments/src/shared.cpp`).
  - **Keine direkten Abhängigkeiten zwischen den Plugins:** GENRA und die Instrumente reden nur über den **Link-Hub**
    (`src/link/Hub.hpp`): Referenzen, Hosts und GENRAs Ausgänge/Tempo (`follow::genraFrame/genraBpm`, Nummern in
    `src/link/GenraPorts.hpp`). Nie eine Klasse des anderen Plugins casten. Ändert sich das Layout von `follow::Ref`,
    `Host`, `Follower` oder `Drive`, prüft der Hub die Grösse; bei echten Protokolländerungen `LINK_VERSION` erhöhen.
  - **Gespeichertes stabil halten** (unten, „Buchsen“): nur hinten anhängen.
  - **Volle GENRA-Leiste**, wo sinnvoll, und das **Preset-Dropdown im Kopf**.
- Bis zum ersten öffentlichen Release dürfen Module noch zwischen den Plugins wandern (dann laden alte Patches sie
  nicht mehr); danach nicht mehr.


## Ablauf bei einem neuen Modul

1. **Zuerst das Panel zeigen** (Entwurf als Bild, mit den echten Rack-Bauteilen), erst nach der Freigabe rechnen und
   Code schreiben.
2. Panel-Skript `tools/<modul>_panel.py` schreibt `res/<Modul>.svg` (Text als Pfade) und den Layout-Header
   `src/<modul>/<Modul>Layout.hpp` (Instrumente: unter `instruments/`); Vorschau nach `docs/panels/`.
3. Bauen und Testen nur über GitHub Actions (Windows); Tests unter `test/` in `.github/workflows/main.yml` eintragen.
4. Version in `plugin.json` erhöhen, in `instruments/plugin.json` dieselbe (Rack 2 lädt nur Versionen 2.x: eine 1.x wird
   mit „does not match Rack ABI version 2“ abgelehnt), `README.md` (Bedienung, deutsch) und `PROJEKT.md` (Technik, deutsch) nachführen.

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
- **Vorlagen für alle Klangmodule (seit 11. Nov. 2026):** GENRA MONO, GRUND, FUNKE und SCHMELZ. Jedes Instrument
  (auch in GENRA Instruments und GENRA Test) sieht so aus wie sie: Preset-Feld im Kopf (statt ◀ PRESET ▶), das
  Display über die volle Breite, Sektionen mit Farblinie oben und Titel, der wichtigste Regler einer Sektion gross
  (`GLargeKnob`) mit gepunktetem Ring in der Akzentfarbe, die anderen mit grauem gepunktetem Ring; Werte stehen im
  Display, nicht unter den Reglern; alle Buchsen unten in zwei Reihen (oben die GENRA-Leiste und die CV-Eingänge,
  unten die Ausgänge auf farbigen Platten). Bestehende Module werden nach und nach umgestellt (`docs/MERKLISTE.md`).
- **Tasten gross genug:** Mindestens so gross wie im MATH-Muster (Taster Ø 9,2 mm, Spur-/Auswahltasten etwa
  14,6 × 6,6 mm), Schrift in der Taste. **Schriften dürfen sich nie überlappen** (die Panel-Skripte prüfen das).
- **So schmal wie möglich**, ohne dass es eng wird (MATH-Muster: 22 HP statt 26).
- **Alle Dropdown-Felder** (Presets, Modelle, Filtertypen, Effekte …) wechseln auch per **Mausrad**: Maus drüber,
  drehen.
- **Überall bündig:** Die Sektionen sitzen auf jedem Instrument 2 mm über den Buchsen, das Display oben bekommt die
  übrige Höhe (`tools/panels/gpanel.py` rechnet das aus; unter 13 mm Display bricht es ab). Alle Instrumente haben
  denselben Aufbau.
- **Mini-Regler** (Grösse der LVL-Trimmer von DRUM-X) sind erlaubt, wo sie HP sparen (K-WASUB, GOLANA, UMLAUF).
- **Instrument-Expander** (K-WASUB FX, GOLANA FX, UMLAUF FX) bleiben **ohne Display**; die Sektionen füllen die Höhe
  bis zu den Buchsen, LINK steht im Kopf.
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
- **Alte Patches müssen laden.** Nichts umsortieren, was gespeichert wird (Enum-Reihenfolge, Auswahllisten wie
  Register, Modelle, Presets, JSON-Schlüssel); Neues hinten anhängen. Wenn eine Umstellung nötig ist, alte Patches
  beim Laden umrechnen (z. B. Versionsfeld im JSON).
- Polyphone Eingänge wie bei GENRA: Pads poly 4, DRUMS poly 8 (Reihenfolge Kick, Snare, Clap, Rim, CH, OH, Tom, Perc;
  Spannung = Velocity).

## Verbindungen ohne Kabel (Follow GENRA, MIX)

- Ein neues Instrument ist ein `follow::Host` (`src/Follow.hpp`): im Konstruktor `fol.bind(INPUT, follow::VOCT …)`
  für jede Leisten-Buchse, `audioL/audioR`, `srcName`; `fol.process(this, args.frame)` als erste Zeile in `process()`;
  `onPortChange` → `fol.portChange(e)`; `onRemove` → `follow::gone(this)`; JSON `fol.toJson/fromJson`;
  Menü `fol.appendMenu`; Widget `follow::addStrip(this, module, {Buchsen-Positionen})` nach `setPanel`.
- Stereo-Ausgang: rechnet stereo, wenn R gesteckt ist **oder** `pulled(args.frame)` (MIX nimmt es ohne Kabel).
  **Nur L gesteckt** (R frei, kein MIX): L gibt die Mono-Summe (L + R) / 2 aus – bei allen Instrumenten.
- Ein gestecktes Kabel hat immer Vorrang.
- **Anzeige:** eine Verbindung ohne Kabel ist an beiden Enden als **kleines Leuchtsegment über der Buchse** zu sehen
  (`follow::segment`): an Eingängen in der Spurfarbe, CLOCK/RESET weiss; an Ausgängen dasselbe, dazu ein kurzer Text,
  wohin (GENRA: „OUT › MONO“, Instrument: „› MIX CH 2“). Keine Ringe um ganze Buchsen.

## Bedienung

- Tooltips, Menüs und Panel-Beschriftung **auf Englisch**; README/PROJEKT auf Deutsch.
- Verbindung zu GENRA/anderen Modulen per **Rechtsklick** („Connect to…“, „Follow GENRA“), nicht nur per Andocken.
- Löschen von viel (z. B. CLR ALL): zweimal drücken, mit UNDO zurückholbar.
- **Endlos-Regler** (Drehgeber) für Werte, bei denen jeder Dreh etwas auslöst (SCHLAG RANDOM): ohne Zeiger, Wert als
  Zahl in der Mitte und als Bogen; eine ganze Drehung = ein UNDO-Schritt.
- Presets per Dropdown im Kopf des Panels (Werks-Presets nach Stil, eigene speicherbar): `src/ui/PresetBank.hpp`, wenn die
  Regler den ganzen Klang tragen; `src/ui/StateBank.hpp`, wenn er in einer Engine liegt (K-WASUB, UMLAUF).

## Rechenzeit

- Teure Formeln (`pow`, `exp`, `tan`) nur bei Änderung oder im Steuertakt, nicht pro Sample.
- In Stille ruhen (Effekte nach dem Ausklingen schlafen lassen, stumme Stimmen überspringen).
- Displays nur rechnen, wenn sie sichtbar sind; keine Engine-Aufrufe (`APP->…`) pro Sample.
