# Merkliste: neue Instrumente für GENRA Instruments

Gesammelte Wünsche. Noch nichts davon ist gebaut. Gebaut wird erst, wenn ausdrücklich „bauen“ kommt.
Alle Instrumente kommen nach `instruments/` (GENRA Instruments), im GENRA-Look nach `docs/DESIGN.md`.
Quellen sind FM-1-Firmwares, alle unter GPL-3.0 (Liste: thefirmwarehub.com, github.com/cicloid/awesome-fm-1).

## Aus JIANT FM (github.com/juanjiant-bit/JIANT, Branch `codex/swarm`)

Gestrichen: STRING und ADD.

### MATH: Bytebeat / Floatbeat
- Quelle: `firmware/src/eng_lofi.c`.
- Akzentfarbe: dunkles Violett-Rosa (gedämpftes Magenta, Entwurf `#c0549a`).
- BYTE / FLOAT, CHIP (4BIT, 4B/2, 8BIT, 1BIT).
- ENGINE mit CV: ALGO, VAR, MOTION, GRIT, BEND, LOOP. Dazu ENVELOPE und FX.
- Schalter **KEYS / THRU**: bei THRU läuft die Formel frei wie ein Oszillator bzw. Loop (V/OCT für die Tonhöhe),
  bei KEYS wird sie wie ein normales Instrument über GATE und die Hüllkurve gespielt (Instrumenten-Gate wie bei SWARM).
- Eingang **T RST**: setzt die Formel auf Anfang (im Takt neu starten).

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

## Weitere Kandidaten (Panel-Entwürfe folgen)

- **FUMI** (FuMi-1, github.com/cartesive/fumi-1; Farbvorschlag Indigo `#6f7cf5`): Shigin-Begleitung nach dem Suiko ST-50, Koto und weitere Klänge
  (6-Operator-FM), reine Stimmung (純正律) oder gleichstufig, 本数 als Tonart, 余韻, Vibrato, Triller, Verzierungen.
- **SPELLBOX** (github.com/pjac1810/spellbox): LPC-Sprachstimme im Stil von Speak & Spell (8 kHz), SING, LETTER SYNTH,
  SPEAK, Circuit-Bending-Regler. **Entschieden: kein eigenes Modul, sondern in VOICE eingebaut.**
  - VOICE bekommt neben OFF/ES/EN/JA einen vierten Taster **SPELL** (LPC-Engine, nur Englisch).
  - Gemeinsam: Lyrics-Feld, Wortanzeige, V/OCT, GATE, Presets, GENRA-Leiste; die Regler, die beide haben
    (POS, ARTIC, SHIFT, LOOP), wirken in beiden Engines.
  - Die Bending-Regler **CLOCK, CORRUPT, STUTTER** (evtl. auch SPEAKR, BITS) kommen in einen kleinen Expander
    **VOICE BEND** und wirken nur bei SPELL.
  - VOICE-Akzent dunkler machen: `#c42f3d` statt bisher hellem Rot (gilt fürs ganze VOICE-Panel).
  - Alte VOICE-Patches müssen weiter laden: Params/Buchsen nur hinten anhängen.
- **MIAOU** (github.com/pjac1810/miaou; Farbvorschlag Flieder `#e58cf0`; Rhythmen/Lieder/Recorder weglassen, PAW als Taster + Gate): Spielzeug-Klänge (Piano, Glocken, Orgel, Banjo) und eine Miau-Stimme über
  wandernde Formanten, Spielzeug-Lautsprecher.
- **SAURUS** (aus PALEOTONE, github.com/fantasmoslam/Paleotone, `firmware/src/eng_saurus.c`): **eigenes Modul**, nur die
  Kreaturenrufe. 20 Kreaturen per Dropdown plus KIT (alle Rufe auf den Tasten), ENGINE: SIZE, SWOOP, LEN, GROWL, BRTH,
  MOUTH, WILD; Eingang CALL. Farbe Echsengrün `#5c9c3a` (gewünscht: grün). ROAR aus PALEOTONE gehört nicht dazu (ist die Drehleier).
- **Drehleier-Modul**, Name: eigener (Vorschlag **KURBEL**, Alternativen BORDUN, SCHNARR; Farbe dunkles Orange `#c25a1c`), aus ESTAMPIE (github.com/ocean/estampie, Mittelalter-Ableger von GHOULBOX):
  - Drehleier-Engine GURDY (`firmware/src/eng_gurdy.c`, = ROAR in PALEOTONE): Melodiesaite, Bordun, Quinte, Raddruck,
    Schnarren (RASP), Puls; Regler DRN, DLVL, 5TH, BRTH, RASP, PULS, BODY, WOBL. Bordun darf in Pausen weiterklingen (RING).
  - Werks-Klänge daraus: Hurdy Gurdy, Drone Wheel, Dance Gurdy, Trompette, Vielle, **Bagpipe (Dudelsack)**, Trotto.
  - **Schalmei:** in ESTAMPIE ein Preset der 3-Oszillator-Engine (TRIO, „a nasal reed“), nicht der Drehleier. Für das
    Modul als zweite kleine Engine (REED) mitnehmen.
  - Mittelalterliche Stimmungen (pythagoreisch, mitteltönig, rein) wie in ESTAMPIE.

Panel-Entwürfe aller geplanten Module wurden am 10. Nov. 2026 im Chat gezeigt (MATH, SWARM, VOICE mit SPELL + VOICE BEND,
FUMI, MIAOU, SAURUS, KURBEL). Noch nicht freigegeben.
