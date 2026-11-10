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
  bei KEYS wird über GATE und die Hüllkurve gespielt.
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
- PLAY: THRU (summt ständig, ohne Gate) oder KEYS (V/OCT transponiert, GATE öffnet; feste weiche Hüllkurve).
- Offen: ATK/REL statt FX?

## Weitere Kandidaten (Panel-Entwürfe folgen)

- **FuMi-1** (github.com/cartesive/fumi-1): Shigin-Begleitung nach dem Suiko ST-50, Koto und weitere Klänge
  (6-Operator-FM), reine Stimmung (純正律) oder gleichstufig, 本数 als Tonart, 余韻, Vibrato, Triller, Verzierungen.
- **SPELLBOX** (github.com/pjac1810/spellbox): LPC-Sprachstimme im Stil von Speak & Spell (8 kHz), SING, LETTER SYNTH,
  SPEAK, Circuit-Bending-Regler.
- **MIAOU** (github.com/pjac1810/miaou): Spielzeug-Klänge (Piano, Glocken, Orgel, Banjo) und eine Miau-Stimme über
  wandernde Formanten, Spielzeug-Lautsprecher.
- **PALEOTONE** (github.com/fantasmoslam/Paleotone): SAURUS-Kreaturenrufe (20 Kreaturen, CALL KIT). Daneben gibt es dort
  ROAR (Drehleier aus GHOULBOX) für Drones.
