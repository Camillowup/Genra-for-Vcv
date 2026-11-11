# Erledigt (aus der Merkliste)

Wünsche aus `docs/MERKLISTE.md`, die gebaut sind – zum Nachschauen.

## Gebaut in GENRA Instruments 2.35.0 (11. Nov. 2026)

### VOICE SPELL + VOICE BEND (aus SPELLBOX)
- **SPELLBOX** (github.com/pjac1810/spellbox): LPC-Sprachstimme im Stil von Speak & Spell (8 kHz), SING, LETTER SYNTH,
  SPEAK, Circuit-Bending-Regler. **Entschieden: kein eigenes Modul, sondern in VOICE eingebaut.**
  **Status: gebaut in 2.35.0** (VOICE LANG SPELL mit SING / LETTER / SPEAK, Expander VOICE BEND mit BEND, CLOCK,
  CORRUPT, STUTTER, CRUSH; ENGINE bei SPELL: POS WORD ARTIC SHIFT SPEED TONE MONO LOOP; drei SPELL-Presets).
  Dabei auch die PRESET-Beschriftung in VOICE, ANIMAL und DRUM-X vereinheitlicht („◀ PRESET ▶“, „◀ LYRIC ▶“, „◀ KIT ▶“).
  - VOICE bekommt neben OFF/ES/EN/JA einen vierten Taster **SPELL** (LPC-Engine, nur Englisch).
  - Gemeinsam: Lyrics-Feld, Wortanzeige, V/OCT, GATE, Presets, GENRA-Leiste; die Regler, die beide haben
    (POS, ARTIC, SHIFT, LOOP), wirken in beiden Engines.
  - Die Bending-Regler **CLOCK, CORRUPT, STUTTER** (evtl. auch SPEAKR, BITS) kommen in einen kleinen Expander
    **VOICE BEND** und wirken nur bei SPELL.
  - VOICE-Akzent dunkler machen: `#c42f3d` statt bisher hellem Rot (gilt fürs ganze VOICE-Panel).
  - Alte VOICE-Patches müssen weiter laden: Params/Buchsen nur hinten anhängen.

## Gebaut in GENRA 2.34.0 (11. Nov. 2026)

### GENRA GRID (GENRA-Plugin)
- **Skalen-Ansicht** im Piano-Roll (Entwurf 10. Nov. 2026): drei Ansichten **ALL** (alle 12 Töne, wie heute) /
  **SCALE** (nur Töne der Tonart, Zeilen dazwischen fallen weg) / **USED** (nur Tonhöhen, auf denen in der Spur Noten
  liegen), Umschalter als Taster in der Kopfleiste. Akkordtöne des jeweiligen Akkords hervorheben.
- **VIEW: ONE · GHOST · ALL 4** (gewählt 11. Nov. 2026, beide Modi; Entwürfe `docs/panels/grid_entwurf_ghost.png`,
  `grid_entwurf_all4.png`):
  - **ONE:** wie heute, eine Spur.
  - **GHOST:** die bearbeitete Spur voll, Melodie/Bass/Pads der anderen Spuren als blasse Umrisse in ihren Spurfarben
    dahinter (nicht anklickbar, nur zur Orientierung; Drums nicht, keine Tonhöhen). Tonbereich passt sich an.
  - **ALL 4:** alle vier Spuren als Streifen untereinander (Drums, Bass, Melodie, Pads), jede in ihrem eigenen
    Tonbereich; Klick = diese Spur bearbeiten (weiss umrandet), Doppelklick = gross in ONE.
- Nicht gewünscht (bleibt so): Teil neu generieren, Euklid/Drum-Zeilen, Akkord-Stempel, weitere Ableton-Ideen.

### SCHMELZ, FUNKE, GRUND, GENRA MONO
- **Preset-Pegel ohne Verzerrung** (11. Nov. 2026): Spitzen unter dem Limiter-Knie (6 V), auch bei überlappenden
  Akkorden; SCHMELZ rund 4 dB leiser.

## Gebaut in GENRA 2.33.0 (11. Nov. 2026)

### FUNKE und SCHMELZ (GENRA-Plugin)
- **Schmaler** (Stand 11. Nov. 2026, Entwurf freigegeben): beide **28 HP** (FUNKE war 30, SCHMELZ 32). Alle Regler und
  Buchsen bleiben, Abstände enger; SCHMELZ-Preset-Feld kürzer (36 mm, nach dem Untertitel). Display-Inhalte im Code
  mitrücken (FUNKE: Scope ab 52 mm, 42 mm breit; SCHMELZ: Registerleiste ab 38 mm, 46 mm breit, SOLO bei 90 mm,
  VOICES bei 114 mm mit 1,2 mm Abstand). Panel-Skripte `tools/panels/funke_panel.py`, `tools/panels/schmelz_panel.py`.

### SCHLAG (GENRA-Plugin), Panel
- **Regler nicht überlappend** (Stand 11. Nov. 2026, Entwurf freigegeben): die vier frei belegbaren Regler pro Spur
  stiessen beim Hineinzoomen aneinander (8,4 mm Abstand). Neu `ROWS = [62.6, 71.6, 80.6, 89.6]` (9 mm), Ring dieser
  Regler enger (R + 0,4 statt + 0,88), TRIG/OUT und MASTER L/R von 98,4 auf 99,4 mm. Sonst alles gleich.

### ABLAUF und ABLAUF MINI (GENRA-Plugin)
- **Pad-Sprung am Ende des Patterns** (entschieden 11. Nov. 2026), bei beiden gleich: ein angetipptes Pad, eine neue
  Variante (Teil-Taster), NEXT und PATT-CV im Pattern-Modus springen erst, wenn das laufende Pattern zu Ende ist (wie
  ABLAUF MINI schon heute; grosser ABLAUF bisher am nächsten Takt, `cuePattern` mit `barPub + 1`). Geprüft: passt zu
  den Breaks, eher besser – im Pattern-Modus läuft der Verlauf eines Teils (Pickup im Break, Roll im Build) über den
  Loop; beim Sprung am nächsten Takt beginnt das neue Pad mitten in seinem Loop, am Pattern-Ende von vorn. Sprünge in
  der Song-Liste (Zeilen, NEXT im Song, ROW-CV) bleiben wie sie sind.

## Gebaut in GENRA 2.32.0 (11. Nov. 2026)

### SCHLAG
- **RND ALL** (Stand 11. Nov. 2026): statt alle 8 Spuren auf 50 % bekommt bei jedem Druck jede Spur ihren eigenen
  Zufallswert **zwischen 10 und 100 %** und wird damit neu ausgewürfelt; beliebig oft hintereinander drückbar, UNDO
  holt den vorherigen Stand zurück. **Gebaut in 2.32.0.**

### GENRA (Hauptmodul)
- **NEW ALL** (Stand 11. Nov. 2026): setzt zuerst die Genre-Felder von Drums, Bass, Melodie und Pads wieder auf das
  Hauptgenre (Global) zurück und würfelt dann neu. Gesperrte Spuren (LOCK) bleiben, wie sie sind; eine Spur mit Kabel
  am Genre-CV-Eingang folgt weiter dem CV. **Gebaut in 2.32.0.**


## Gebaut in GENRA Test 2.2.1 (11. Nov. 2026)

### KURBEL (GENRA Test, Repo testmodulesvcv)
Rückmeldung zu 2.1.0: ohne Gate kein Ton (erwartet beim Kurbeln), mit Follow GENRA nur sehr leise (nicht nachgestellt;
der Kern liefert in Tests vollen Pegel; Verdacht COUP CRANK mit zu wenig Kurbel).
- Rechtsklick-Option **Crank plays without gate** (Haken, Standard **aus**, im Patch gespeichert): bei COUP CRANK
  klingen Bordun und leere Melodiesaite, sobald die Kurbel dreht; ein Gate spielt die Melodiesaite auf V/OCT.
- **CALL**-Taster zum Anspielen (wie MATH und VOICE).
- Lautstärke bei COUP CRANK so abstimmen, dass mittleres Kurbeln schon gut hörbar ist.
