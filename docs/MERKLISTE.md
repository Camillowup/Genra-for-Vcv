# Merkliste (offen)

Nur offene Wünsche: neue Instrumente und Änderungen an bestehenden Modulen. Gebaut wird erst, wenn ausdrücklich „bauen“ kommt.
Erledigtes wandert nach `docs/archiv/ERLEDIGT.md`.

**Regel Modul-Umzug:** Wandert ein Modul in ein anderes Plugin (z. B. aus GENRA Test nach GENRA Instruments), kommt es im
selben Schritt aus dem alten Plugin heraus – sonst steht es in Rack doppelt. Gilt für alle Plugins.
Neue Instrumente werden zuerst im Testplugin GENRA Test (Repo testmodulesvcv) gebaut und kommen danach nach `instruments/` (GENRA Instruments), im GENRA-Look nach `docs/DESIGN.md`.
Quellen sind FM-1-Firmwares, alle unter GPL-3.0 (Liste: thefirmwarehub.com, github.com/cicloid/awesome-fm-1).

## Im Test (Testplugin GENRA Test, Repo testmodulesvcv)

Gebaut und in Rack zu testen (Stand GENRA Test 2.5.0, 11. Nov. 2026); wandern später nach `instruments/` (dann
gleichzeitig aus GENRA Test entfernen): **MATH**, **KURBEL**, **SWARM** (Details unten), **LIED** (Textsänger auf
eSpeak NG, 2.3.0), **CANTA** (2.4.0), **MIAOU**, **SAURUS** und **FUMI** (alle drei 2.5.0), **KESSEL** mit Expander
**KESSEL FX** (2.6.0, Drum-Modul mit der Klang-Engine der LXR-Firmware von Erica Synths). Beschreibung aller Module
im README von testmodulesvcv.

## Umstellung aller Panels auf den Look der Vorlagen (11. Nov. 2026)

Vorlagen: GENRA MONO, GRUND, FUNKE, SCHMELZ (Regeln in `docs/DESIGN.md` › Aussehen). Muster: MATH (22 HP), freigegeben.
Jedes Panel wird zuerst als Entwurf gezeigt; gebaut wird erst nach Freigabe und „bauen“.
- **Umgestellt in 2.36.0** (in Rack prüfen): VOICE 24 HP, VOICE BEND 10 HP, ANIMAL 22 HP, MONK 26 HP, DRUM-X 28 HP
  (LVL und TRIG unter jedem Pad). GENRA Test: MATH, SWARM, SAURUS, KURBEL, MIAOU, FUMI, CANTA umgestellt, LIED hat
  das Mausrad. Panels aus `tools/panels/gpanel.py` (gleiches Werkzeug wie `tools/gpanel.py` in GENRA Test).
- Ebenfalls umgestellt in 2.36.0: K-WASUB 38 HP, K-WASUB FX 41 HP, GOLANA 45 HP, GOLANA FX 14 HP, UMLAUF 54 HP,
  UMLAUF FX 39 HP (Expander ohne Display). Alle Panels bündig: Sektionen 2 mm über den Buchsen, Display füllt den Rest.
- In Rack prüfen: Schriften in den Displays (dynamisch), GOLANAs schmale Felder, UMLAUFs flacher Orbit-Display,
  UMLAUF-Sample-Ordner, ABLAUF PATT ↔ SONG und RUN/STOP an GENRA.

## Aus JIANT FM (github.com/juanjiant-bit/JIANT, Branch `codex/swarm`)

Gestrichen: STRING und ADD.

### MATH: Bytebeat / Floatbeat
- Quelle: `firmware/src/eng_lofi.c`.
- Akzentfarbe: dunkles Violett-Rosa (gedämpftes Magenta, Entwurf `#c0549a`).
- BYTE / FLOAT, CHIP (4BIT, 4B/2, 8BIT, 1BIT).
- ENGINE mit CV: ALGO, VAR, MOTION, GRIT, BEND, LOOP. Dazu ENVELOPE und FX.
- Schalter **KEYS / THRU**: bei THRU läuft die Formel frei wie ein Oszillator bzw. Loop (V/OCT für die Tonhöhe),
  bei KEYS wird sie wie ein normales Instrument über GATE und die Hüllkurve gespielt (Instrumenten-Gate wie bei SWARM).
- **CLOCK / RESET** in der GENRA-Leiste (statt T RST): Tempo für MOTION SYNC, RESET setzt die Formel auf Anfang.
  Zusätzlich ohne Kabel per Rechtsklick (Follow GENRA: Tempo und Reset von GENRA).
- THRU lohnt sich: mit LOOP 0 entwickelt sich die Formel frei (Rhythmus, Melodie), mit MOTION SYNC im Song-Takt.
- **Status:** im Test seit 2.0.0 (10. Nov. 2026), im Testplugin **GENRA Test** (Repo Camillowup/testmodulesvcv, Slug GenraTest, Modul-Slug Math), gebaut 10. Nov. 2026, wird getestet. JIANT-Quelle: Branch codex/swarm (V10.7.16).

### SWARM: Schwarm aus virtuellen Geräten
- Quelle: `firmware/src/eng_input.c` (nur der SWARM-Teil).
- Akzentfarbe: **Petrol** `#13b8a6`.
- **Kein Audio-Eingang** (IN L/R gestrichen), **kein echtes Bluetooth**.
- Bluetooth wird emuliert: Die Engine bleibt unverändert und bekommt erfundene Funkpakete (`sw_heard`).
  Virtuelle Geräte mit Adresse (Ton), Hersteller (Klangfamilie), Datenbytes (FM-Modulator), Sendeabstand
  (Rhythmus, Schwankung = Vibrato), Signalstärke (Nähe, Zufallsweg), Funkkanal (Stereo), Kommen und Gehen.
  Gesteuert über einen Startwert (wird im Patch gespeichert).
- Sektion **AIR**: VOICES (1–8 Geräte), BUSY (Sendedichte), MOVE (Herumlaufen), NEAR (Nähe).
- Eingänge: **NEAR** (CV), **ARRIVE** (Trigger: neues Gerät kommt), **NEW** (Trigger: neue Menge). Taster NEW im Kopf.
- ENGINE mit CV: PLUCK, FM, IMAGE, DRIFT, OCTAVE, CHAOS. Dazu FX.
- PLAY: THRU (summt ständig, ohne Gate) oder KEYS (spielbar wie ein normales Instrument: V/OCT transponiert,
  C4 = Originallage; GATE an = Ton, GATE aus = Ende).
- **GATE = Instrumenten-Gate** wie bei den anderen GENRA-Instrumenten: Länge des Gates = Länge des Tons, kurze Rampe
  gegen Klicks. Zusätzlich stufenlos (Spannung = Lautstärke), damit ein Envelope am GATE die Form bestimmt.
  **Keine eigenen ATK/REL-Regler**, FX bleiben.

## Weitere Instrumente aus FM-1-Firmwares

- **FUMI** (FuMi-1, github.com/cartesive/fumi-1; Indigo `#6f7cf5`): Shigin-Begleitung nach dem Suiko ST-50, Koto und weitere Klänge
  (6-Operator-FM), reine Stimmung (純正律) oder gleichstufig, 本数 als Tonart, 余韻, Vibrato, Triller, Verzierungen.
  **Status:** gebaut als GENRA Test 2.5.0 (Modul-Slug Fumi), wird getestet; nur die sechs eigenen Klänge von FuMi-1.
- **SPELLBOX**: als Stimme SPELL in VOICE gebaut (GENRA Instruments 2.35.0), Details in `docs/archiv/ERLEDIGT.md`.
- **MIAOU** (github.com/pjac1810/miaou; Flieder `#e58cf0`; Rhythmen/Lieder/Recorder weglassen, PAW als Taster + Gate): Spielzeug-Klänge (Piano, Glocken, Orgel, Banjo) und eine Miau-Stimme über
  wandernde Formanten, Spielzeug-Lautsprecher. **Status:** gebaut als GENRA Test 2.5.0 (Modul-Slug Miaou), wird getestet.
- **SAURUS** (aus PALEOTONE, github.com/fantasmoslam/Paleotone, `firmware/src/eng_saurus.c`): **eigenes Modul**, nur die
  Kreaturenrufe. 20 Kreaturen per Dropdown plus KIT (alle Rufe auf den Tasten), ENGINE: SIZE, SWOOP, LEN, GROWL, BRTH,
  MOUTH, WILD; Eingang CALL. Farbe Echsengrün `#5c9c3a` (gewünscht: grün). ROAR aus PALEOTONE gehört nicht dazu (ist die Drehleier).
  **Status:** gebaut als GENRA Test 2.5.0 (Modul-Slug Saurus), wird getestet.
- **KURBEL** (Drehleier-Modul, dunkles Orange `#c25a1c`), aus ESTAMPIE (github.com/ocean/estampie, Mittelalter-Ableger von GHOULBOX):
  - Drehleier-Engine GURDY (`firmware/src/eng_gurdy.c`, = ROAR in PALEOTONE): Melodiesaite, Bordun, Quinte, Raddruck,
    Schnarren (RASP), Puls; Regler DRN, DLVL, 5TH, BRTH, RASP, PULS, BODY, WOBL. Bordun darf in Pausen weiterklingen (RING).
  - Werks-Klänge daraus: Hurdy Gurdy, Drone Wheel, Dance Gurdy, Trompette, Vielle, **Bagpipe (Dudelsack)**, Trotto.
  - **Schalmei:** in ESTAMPIE ein Preset der 3-Oszillator-Engine (TRIO, „a nasal reed“), nicht der Drehleier. Für das
    Modul als zweite kleine Engine (REED) mitnehmen.
  - Mittelalterliche Stimmungen (pythagoreisch, mitteltönig, rein) wie in ESTAMPIE.
  - Panel (Entwurf 10. Nov. 2026): Kopf mit ◀ PRE / PRE ▶, RING (Bordun klingt durch Pausen), Dropdown DRONE (C–B),
    Dropdown COUP (HOLD, 1/4, 1/8, 1/8T, 1/16, CRANK), Regler **CRANK**, LEVEL. Display-Chips WHEEL / REED und
    EQUAL / PYTH / MEAN / JUST. ENGINE mit CV: D LVL, 5TH, WHEEL, BUZZ, BODY, WOBBLE (bei REED andere Namen und
    Funktionen, Anzeige wechselt mit; dass ein Patch dann anders klingt, ist ok). Unten V/OCT · GATE · VEL,
    CLOCK · RESET (Stösse im Takt), **CRANK** (CV), **BUZZ** (Trigger für einen Stoss), L/R. 26 HP.
  - **Kurbel:** Geschwindigkeit als Wert (kein Pulszähler): CRANK-Regler wie ein kleines Tempomodul (Anzeige in
    Umdrehungen pro Minute) plus CRANK-CV dazu addiert; ein schneller Ruck (plötzlicher starker Anstieg) gibt wie in
    GHOULBOX automatisch einen Stoss. Gilt bei COUP = CRANK; sonst dreht das Rad gleichmässig, Stösse im CLOCK-Takt.
  - Schalmei (REED) kommt mit rein. **Status:** gebaut als GENRA Test 2.1.0 (Repo testmodulesvcv, Modul-Slug Kurbel), 10. Nov. 2026, wird getestet. Quelle: ESTAMPIE Commit 6c4f0c7.

Alle oben genannten Module sind inzwischen gebaut (VOICE SPELL in GENRA Instruments, die übrigen in GENRA Test).

## CANTA (Arbeitsname KANA): japanische Sing-Engine aus MIKU-FM (11. Nov. 2026)
- Quelle: github.com/MichaelAPerry/MIKU-FM1 (Code GPL-3.0-only). Nur die Gesangsstimme; die Band (FM-Akkorde, Bass,
  Drums) fällt weg, das übernimmt GENRA.
- **Keine Grafiken aus MIKU-FM:** die Sängerin dort ist Hatsune Miku (Crypton, Piapro Character License, nicht GPL,
  keine Spenden erlaubt). Stattdessen **vier eigene Pixel-Sängerinnen im Chibi-/Manga-Stil**, je eine pro Sprachmodus:
  **MOMO** (Bärchen-Hoodie, rosa), **SORA** (violett, Seitenknoten, Sternclip, Matrosenkragen), **KIKO** (Fuchsohren,
  Yukata, orange Haare), **NANA** (Brille, braune Zöpfe, Pullover, Mikrofon). Zuordnung: JAPANESE = KIKO,
  ANIMAL = MOMO, SCAT = NANA, VOWELS = SORA. Animationen wie in MIKU-FM nachgebaut: Mundformen pro Vokal, Blinzeln,
  DANCE-Modus (ein Schritt pro Schlag, Arme hoch im Refrain, Verbeugung am Ende).
- Panel-Entwurf (Farbe Koralle `#ff7a59`): Display mit Sängerin und Silben-Rad, Chips Sprache
  (JAPAN, ANIMAL, SCAT, VOWELS) + DANCE, Charakter (DEEP, LOW, MID, YOUNG, TINY); Kopf PRE, CALL, SYLLABLE-Dropdown,
  LYRICS FOLLOW/FIXED (Text per Rechtsklick), LEVEL; VOICE: BREATH, BRIGHT, GLIDE, TENSE, FRY, WHISPR, FORMNT, HARM
  (je CV); SHAPE: ATK, REL, SCOOP, VIB; FX: CHORUS, DELAY, REVERB, DRIVE; Buchsen GENRA-Leiste, CLOCK/RESET, SYL (CV),
  NEXT (Trigger), L/R.
- Entschieden: eigenes Modul **CANTA**, Koralle `#ff7a59`, 26 HP. Panel nach Entwurf v2: Kopf ◀ PRE, PRE ▶, CALL,
  LYRICS FOLLOW/FIXED, SYLLABLE, HARMONY (OFF −12 +3 +4 +5 +7 +12) + MIX, LEVEL; VOICE: BREATH BRIGHT GLIDE TENSE FRY
  WHISPR FORMNT HUMAN (je CV); SHAPE, FX wie oben; Buchsen GENRA-Leiste, CLOCK/RESET, SYL, NEXT, L/R. Liedtext per
  Klick auf die Textzeile im Display.
- **Status:** gebaut als GENRA Test 2.4.0 (Repo testmodulesvcv, Modul-Slug Canta), 11. Nov. 2026, wird getestet.
  Quelle: MIKU-FM Commit 781760c (nur Stimmen-Code). 12 eigene Presets. Die vier Sängerinnen sind beim Bau neu
  gezeichnet (16 × 21 Pixel, Paletten wie im Entwurf); DANCE = ein Schritt pro Schlag. Noch nicht: „Arme hoch im
  Refrain“, „Verbeugung am Ende“.

## Offene Änderungen an bestehenden Modulen (gesammelt, noch nicht gebaut)

### FUNKE, SCHMELZ, GRUND, GENRA MONO (GENRA-Plugin)
- **Cutoff der Presets** etwas höher (+10–15 %), Lautstärke danach neu einmessen: nur die Presets, die der Nutzer noch
  auflistet (Liste folgt).

