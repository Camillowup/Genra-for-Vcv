# GENRA v3 – Generativer 4-Spur-Sequencer für VCV Rack 2

Breite: **64 HP** (GENRA) und **50 HP** (Grid Editor). Dazu im selben Plugin die drei JIANT-Stimmen **VOICE** (26 HP), **DRUM-X** (32 HP) und **ANIMAL** (26 HP), **MONK** (26 HP) der Bass-Synth **GENRA MONO** (28 HP) und die Klangmodule aus GENRA SYNTHIS: **SCHMELZ** (32 HP), **MIX** (32 HP), **K-WASUB** und **K-WASUB FX** (je 40 HP), **UMLAUF** (54 HP) und **UMLAUF FX** (36 HP). Projektstand und Entscheidungen: `PROJEKT.md`.

## GENRA Grid Editor (zweites Modul)

![Grid Editor](docs/entwuerfe/genra_grid_piano.png)

Ein zweites Modul im selben Plugin (50 HP) zum vollständigen Bearbeiten des Loops: Drum-Raster, Piano-Roll für Bass, Melodie und Pads, Velocity-Spur.

- **Verbindung:** direkt links oder rechts neben GENRA setzen (LINK-LED leuchtet). Oder im Rechtsklick-Menü *Verbinden mit…* ein beliebiges GENRA im Patch wählen.
- **TRACK:** Spur wählen. **VIEW:** BAR (erster sichtbarer Takt), ZOOM (1/2/4/8 Takte), PITCH (Tonbereich), AUTO SCROLL (Ansicht läuft mit).
- **Drums:** Klick setzt/löscht einen Schlag, Ziehen malt bzw. radiert, Mausrad = Velocity, Rechtsklick = Roll 2–4×, Velocity, Löschen. Klick auf den Stimmennamen wählt die Stimme der Velocity-Spur.
- **Piano-Roll:** Klick setzt eine Note (Länge LEN, Velocity VEL), Ziehen am rechten Rand ändert die Länge, Ziehen der Note verschiebt sie, Doppelklick löscht. Rechtsklick: Akzent, Slide, Velocity. Tonleitertöne sind heller, Akkordtöne blaugrün hinterlegt, die Akkordfolge steht darüber.
- **SNAP:** AUS / SCALE / CHORD für neue und verschobene Noten; mit Shift gedrückt: alle Noten der Spur einrasten.
- **CHORD FOLLOW:** bearbeitete Noten folgen neuen Akkorden (HARMONY NEW): Grundtöne werden zum neuen Grundton, alle anderen Töne wandern in der Tonleiter mit.
- **CHANCE:** Wahrscheinlichkeit neuer Noten (Regler). Jede Note/jeder Schlag kann eine eigene Wahrscheinlichkeit haben und „nur jeden 2. Durchlauf“ spielen (Rechtsklick). Solche Noten sind gestrichelt und zeigen ihren Wert. Die untere Spur schaltet per Klick auf „VEL“/„CHANCE“ um – dort zieht man Velocity bzw. Chance mit der Maus.
- **Mehrfachauswahl:** Shift + Ziehen = Rahmen, Strg + Klick = Note an-/abwählen, Strg+A = alles. Ausgewählte Noten zusammen ziehen; Pfeile ↑↓ = Tonhöhe (Shift: Oktave; Drums: Stimme), ←→ = Position (Shift: ein Takt); Entf = löschen; Strg+C / Strg+V = kopieren / an der Mausposition einfügen; Strg+D = dahinter duplizieren; Esc = Auswahl aufheben. Mausrad und Rechtsklick-Menü wirken auf die ganze Auswahl.
- **EDIT:** UNDO, REDO, COPY/PASTE BAR (erster sichtbarer Takt), CLEAR TRACK, GEN TRACK (Spur wieder generativ).
- **Was „bearbeitet“ heisst:** Sobald du etwas änderst, spielt die Spur feste Noten – genau das, was vorher zu hören war, plus deine Änderungen. DENSITY/COMPLEX wirken dann nicht mehr auf diese Spur, FILL (Fills und Drops) und Humanize schon. MUTATE verändert auch bearbeitete Spuren – LOCK schützt sie vor MUTATE und NEW ALL.

## JIANT-Stimmen: VOICE, DRUM-X, ANIMAL

| VOICE | DRUM-X | ANIMAL |
|---|---|---|
| ![VOICE](docs/entwuerfe/jiant_voice.png) | ![DRUM-X](docs/entwuerfe/jiant_drumx.png) | ![ANIMAL](docs/entwuerfe/jiant_animal.png) |

Drei Klangerzeuger aus der Firmware von [JIANT FM](https://github.com/juanjiant-bit/JIANT) (Juan Jiant, ein Fork von [Felucca](https://github.com/hugelton/Felucca), GPL-3.0), im GENRA-Look und jeweils in einer eigenen Farbe: **VOICE rot**, **DRUM-X lila**, **ANIMAL grün**. Das Display zeigt links die Zeichnung der Firmware (Vokaltrakt, Kit-Kolonie, Kreatur), rechts Preset bzw. Kit und die Einstellungen.

- **VOICE** (26 HP): Formant-Sänger. ◀ PRE ▶ (15 Presets), LANG im Display (OFF singt Vokale, ES/EN/JA singen Wörter), ◀ LYR ▶ wählt TEXT, W1–W16 oder S1–S8, das Textfeld im Display nimmt einen eigenen Text (bis 63 Zeichen). Jede neue Note singt die nächste Silbe, WORD ↺ startet den Text neu. 8 Klangregler mit CV, ihre Namen wechseln mit LANG.
- **DRUM-X** (32 HP): 8 synthetische Drum-Spuren mit Varianten A/B und MORPH, 9 Kits (◀ KIT ▶). Klick auf Spurname, Pad oder Spur-Chip im Display wählt die Spur für SOUND (PITCH … COLOR, A | B). 7 Kit-Makros mit CV, pro Spur TRIG, LEVEL und Einzelausgang (OUT), V/OCT + GATE nach General MIDI, ACC, DLY, REV, LVL, Mix L/R.
- **ANIMAL** (26 HP): Tierrufe aus physikalischen Modellen. MODE als Regler oder per Klick im Display (CMOS, PULSE, WHSTL, WING, GLOTT), ◀ PRE ▶ (24 Presets), 7 Klangregler mit CV, ihre Namen wechseln mit MODE.
- **Gemeinsam (VOICE, ANIMAL):** CALL spielt einen Ton ohne Kabel (auch per Gate am CALL-Eingang), POLY · MONO · LEGATO per Klick auf die Lampen, Hüllkurve ATK–REL, FX DIST–REVERB, V/OCT, GATE, VEL polyphon, LEVEL und Stereo-Ausgang (nur L gesteckt = Mono).
- Presets bzw. Kits gibt es auch im Rechtsklick-Menü. Alte Patches mit den einzelnen Plugins *JiantVoice*, *JiantDrumX*, *JiantAnimal* finden die Module hier nicht automatisch – dort einmal neu einsetzen.

## MONK: der Sänger aus MONK-1

![MONK](docs/entwuerfe/monk.png)

Der Vokal-Synthesizer aus der FM-1-Firmware [MONK-1](https://github.com/charlesvestal/fm1-monk) (Charles Vestal, GPL-3.0), dessen Klang [MonkSynth](https://github.com/JonET/monksynth) ist (Jonathan Taylor, MIT; eine Hommage an Delay Lama). FOF-Formantsynthese: pro Periode ein Korn aus drei abklingenden Formant-Sinus plus Atem, fünf Vokale OO–OH–AH–EH–EE. Farbe: **safran**.

- **Display:** links singt der Sänger, gezeichnet vom Originalcode der Firmware (Mund folgt Vokal und Lautstärke, ohne Ton blinzelt er und probt Vokale), rechts Name, Note, Stimmen, ob die Stimme bearbeitet ist, Vokal-Chips (Klick setzt VOWEL), PRESS und ENV.
- **◀ SING ▶:** die zwölf Sänger (Jerry, Guppo, Philip, Suze, Gerald, Bess, EeOo, Snider, Fipsy, Peppo, Marley, Razor). Ein Sänger lädt seine Stimme auf alle Regler. Auch per SINGER-CV (0–10 V) und Rechtsklick-Menü, dort auch *Stimme wie geliefert*.
- **SING · CHOIR** (je mit CV, ±5 V = ganzer Reglerweg): VOWEL, HEAD (Vokaltrakt in cm), BREATH, GLIDE, VOICES (Unisono 1–10), DETUNE (Cent), SPREAD (Kopfgrössen der Stimmen), TUNE (±12 Halbtöne).
- **PRESS →** VOWEL · PITCH · BOTH · INV: wohin der PRESS-Eingang (Aftertouch, 0–10 V) wirkt; P.DEPTH wie weit, VEL wie stark VEL die Lautstärke setzt.
- **ENVELOPE** ATK DEC SUS REL (bis 5 s), **VIBRATO · ECHO** DEPTH, RATE, ECHO (Menge), TIME, FDBK (neu, im Original fest 50 %).
- **Ein/Aus:** V/OCT (C4 = 0 V), GATE, VEL, PRESS, SINGER · ENV (0–10 V), DRY (mono ohne Echo), L, R. Einstimmig wie das Original: GATE startet die Note, V/OCT bei gehaltenem Gate gleitet ohne neuen Anschlag (GLIDE).
- Mit MIDI: MIDI-CV-Modul → V/OCT, GATE, VEL und AFT → PRESS.

## GENRA MONO: Bass-Synth nach dem Moog Minitaur

| | |
|---|---|
| ![GENRA MONO](docs/entwuerfe/genra_mono.png) | ![Filter-Menü](docs/entwuerfe/genra_mono_menu.png) |

Monophoner Synth (28 HP) im Look von MONK und den JIANT-Stimmen, Farbe **türkis**.

- **Oszillatoren:** pro VCO WAVE (stufenlos Dreieck → Sägezahn → Rechteck → schmaler Puls), TUNE (±12 Halbtöne, fein um die Mitte – zum leichten Verstimmen) und LEVEL (hohe Pegel übersteuern das Filter); GLIDE.
- **Filter:** Klick auf das Feld öffnet die Liste, das Mausrad blättert: LADDER 24 (Moog, Standard, Bass bleibt bei Resonanz erhalten), LADDER 12, DIODE (303), MS-20, SVF LP/BP/HP (Oberheim SEM), OTA (Juno). CUTOFF, RES, EG AMT (bipolar), DRIVE, KEY TRK.
- **Hüllkurven:** Filter- und Amp-ADSR mit analogen Kurven (1 ms … 10 s).
- **LFO:** RATE (0,05–50 Hz), TRI/SQR, Stärke auf Tonhöhe und Filter, eigener LFO-Ausgang.
- **Eingänge:** V/OCT (0 V = C4), GATE, VEL, CV für CUTOFF (1 V/Okt.), RES, DRIVE, RATE (1 V/Okt.), VCA, EXT (externes Audio durchs Filter). **Ausgänge:** LFO, OUT.
- Oszillatoren und Filter laufen mit 2-facher Überabtastung.

## Aus GENRA SYNTHIS

Die folgenden Module entstanden im eigenen Repo `CamilloGF/vcv-synthis` (Plugin GENRA SYNTHIS) und sind seit 2.6.0 Teil
des GENRA-Plugins. Ihre Modul-Slugs sind gleich geblieben; Patches, die sie aus dem GENRA-SYNTHIS-Plugin geladen haben,
müssen sie neu einsetzen. Das GENRA-SYNTHIS-Plugin am besten deinstallieren, sonst stehen die Module doppelt im Browser.

### SCHMELZ: String Machine

![SCHMELZ](docs/entwuerfe/schmelz.png)

String Machine im Geist der 70er/80er (Vorbild: Waldorf Streichfett), 32 HP, Farbe **rosa**. Zwei Klangteile, je bis
16 Stimmen, dazu Effekte.

- **Strings:** REGISTRATION überblendet stufenlos VIOLIN · VIOLA · CELLO · BASS · BRASS · ORGAN · CHOIR · SYNTH.
  CRESC (Einschwingen 3 ms … 6 s), RELEASE (10 ms … 8 s), TONE (dunkel … hell). OCT: BASE · BOTH · 8VA.
  ENS: OFF · ENS (Ensemble, drei modulierte Verzögerungen wie bei den String-Ensembles) · E+C (Ensemble + Chorus) ·
  CHO (Chorus); ENS DEPTH.
- **Solo:** SOUND überblendet BASS · E-PIANO · CLAVI · SYNTH · BELL. ATTACK, DECAY (zugleich Release), SUSTAIN,
  TREMOLO mit T.RATE (Stereo), VEL (Anschlagsempfindlichkeit), SUSTAIN-Taster (hält die Solo-Noten wie ein Pedal).
  SPLIT (LAYER · LOW · HIGH, Splitpunkt C4) gilt, wenn das Solo über die Strings-Eingänge spielt.
- **Effects · Master:** BALANCE Strings/Solo, ANIMATE (LFO bewegt die Registration) mit A.RATE, PHASER (nur Strings)
  mit P.RATE, REVERB mit SIZE, VOLUME.
- **Eingänge:** Strings V/OCT, GATE, VEL · Solo V/OCT, GATE, VEL (alle polyphon; ohne Solo-GATE spielt das Solo die
  Strings-Noten) · CV für REG (polyphon, pro Stimme), CRESC, REL, TONE, ENS, OCT, SOUND, DECAY, TREM, BAL, ANIM, VERB
  (0–10 V zum Regler addiert; ENS und OCT schalten in Stufen).
- **Ausgänge:** ENV (Strings-Hüllkurve, polyphon, 0–10 V), STR (Strings mit Ensemble, ohne Phaser/Hall), SOLO
  (trocken), L, R.
- **Mit GENRA:** PADS (V/OCT + GATE, poly) → Strings, MELODY → Solo, MOD → REG oder ANIM.

### MIX: 4-Kanal-Stereomixer

![MIX](docs/entwuerfe/mix.png)

Mixer für die vier GENRA-Spuren, 32 HP. Jeder Kanal in der Farbe seiner Spur (DRUMS orange, BASS rot, MELODY blau,
PADS lila), der Master dunkel.

- **Pro Kanal:** IN L/R (R unbelegt = L; polyphone Eingänge werden summiert), HIGH/LOW (Kuhschwanz-EQ bei 6 kHz /
  120 Hz, ±12 dB), SEND (nach dem Fader), DUCK (wie stark der Kanal beim Duck-Trigger zurückgeht), PAN (Balance),
  LEVEL (0,75 = 0 dB, voll = +5 dB) mit Pegelanzeige, MUTE und SOLO (beleuchtet). CV: LVL (0–10 V, wie ein VCA),
  PAN (±5 V).
- **Master:** DUCK (Tiefe) und TIME (20 ms … 1 s, Erholzeit) – den KICK-Ausgang von GENRA in die DUCK-Buchse stecken,
  dann pumpen die anderen Spuren mit. RETURN (Pegel des Effekt-Rückwegs RET L/R), SEND (Pegel von SEND L/R), WIDTH
  (Mono … doppelt breit), DRIVE (Sättigung), MASTER mit Stereo-Pegelanzeige, OUT L/R (weicher Limiter ab 8 V).

### K-WASUB: Modern-Bass-Synth (Firmware von Omnipotard)

![K-WASUB](docs/entwuerfe/kwasub.png)

Die Firmware [K-WASUB](https://github.com/Guizmoh/K-WASUB_Firmware_for_FM1) für den M-VAVE FM-1 von Omnipotard (GPL-3.0)
als Modul, 40 HP, Farbe **säuregrün**. Der Klang ist die Original-Firmware, unverändert (`kwasub/`): Sie läuft als
eigene kleine Bibliothek (`res/kwcore.*`, eine Kopie pro Modul) mit 44,1 kHz, das Modul rechnet auf die Rack-Rate um.

- **OSC A / OSC B:** ENGINE-Feld (Klick = Liste, Mausrad): ANALOG, 808, CS, WAVETABLE, FM, GROWL, ADDITIVE, PHASE DIST,
  HARD SYNC. SHAPE, COLOR, EDGE, FOLD sind die vier Klangregler der Engine (was sie bei der gewählten Engine heissen
  und welchen Wert sie haben, steht im Display und im Tooltip), LEVEL, OCT, SEMI, FINE.
- **FILTER:** Typ-Feld (LP 24, LP 12, BAND, HIGH, NOTCH, VOWEL, COMB), CUTOFF, RES, DRIVE, ENV, KEY, Filterhüllkurve.
- **SUB · X:** Sub, Rauschen, X-Mod · **WOBBLE:** LFO 1 im Tempo (RATE = Notenwert), DEPTH, SHAPE, DEST ·
  **AMP:** ADSR · **FX:** SAT, DELAY, VERB, GLUE (schalten ihren Effekt ein) · **PRESET:** ◀ ▶ durch die 97 Werksklänge
  (auch im Rechtsklick-Menü).
- **Eingänge:** V/OCT, GATE, VEL (bis 4 Stimmen, auf Halbtöne gerundet), CLOCK (setzt das Tempo; Standard 16tel wie
  GENRA, im Menü 8tel, Viertel, 24 PPQN), RESET (Taktanfang), CV für CUT, RES, DRIVE, ENV, A/B SHAPE und COLOR, WOB,
  DEPTH, PRESET (0–10 V über alle Presets), SUB, NOISE, X-MOD, ATK, REL, SAT, DELAY, VERB, GLIDE; MOD (Modrad),
  PRESS (Druck), BEND (±5 V). **Ausgänge:** ENV, LFO, L, R.
- Rechtsklick: Preset-Liste, Stimmen (Mono, Legato, Poly), Clock-Auflösung. Der ganze Klang wird im Patch gespeichert,
  auch was kein Regler zeigt (LFO 2, Matrix, OSC C …).

### K-WASUB FX: Effekt-Expander

![K-WASUB FX](docs/entwuerfe/kwasub_fx.png)

Rechts neben K-WASUB setzen (LINK leuchtet): alle Einstellungen des Effekt-Racks der Firmware. SATURATE (Typ, Amount,
Mix), CRUSH, CHORUS, PHASER, DELAY (Typ, Zeit im Tempo, Feedback, Ton, Mix), REVERB (Typ, Grösse, Dämpfung, Mix),
COMPRESSOR (mit PUMP und PUMP RATE), MULTIBAND, EQ (4 Bänder), MASTER (Glue, Sub-Boost, Mono-Bass). Jeder Block mit
EIN-Taster; 16 CV-Eingänge, nach Effekt gruppiert. Regler, die es auf beiden Panels gibt (SAT, DELAY, VERB, GLUE),
folgen einander. Ohne Expander gelten die Effekte des Presets.

### UMLAUF: Orbit-Vektor-Synth (Engine von VADER)

![UMLAUF](docs/entwuerfe/umlauf.png)

Die Engine von [VADER](https://github.com/sunskiefer/VADER) (L'Cronx, MIT) als Modul, 54 HP, Farbe **Sith-Rot**. Ein
Punkt, der **Orbiter**, läuft auf einer Bahn in einem Quadrat; in den vier Ecken spielt je ein Klang, und wo der
Orbiter gerade ist, entscheidet, wie viel man von jeder Ecke hört. Die Engine läuft wie im Original mit 44,1 kHz
(`vader/`, bis auf wenige markierte Änderungen, siehe `vader/README-UMLAUF.md`), das Modul rechnet auf die Rack-Rate um.

- **Display:** das Quadrat mit Bahn, Orbiter, Sub-Orbit-Punkt und den Bällen der Modi; daneben Form, Modus, Clock und
  die Live-Namen von M1–M8 (sonst die Engines der Ecken), unten der Anteil jeder Ecke.
- **ORBIT · MODE:** SHAPE (15 eingebaute Formen, USER = eigene Datei, 35 Mathe-Formen), SPEED, SIZE, ROTATE, 3D SPIN,
  3D TILT (für Würfel, Ikosaeder, OBJ), SUB SIZE, SUB SPEED, ENV>SIZE (die Hüllkurve öffnet oder schliesst die Bahn).
  MODE: PATH (die Form) oder eine von 26 Simulationen (CHASE, KEPLER, LORENZ, PENDULUM, BOIDS, MEDUSA, THUNDER …);
  M1–M8 sind ihre Regler, die Namen stehen im Display.
- **CORNER A–D:** ENGINE: SYNTH (VADERs zwei Oberton-Generatoren mit je 32 Spektren und BLEND: MIX, AM, PM, AM+PM, PD,
  RING, FOLD), die 14 Stimmen ANALOG, TRIO, WAVE, CHIP, MACRO, WHEEL, VOICE, NOISE, TONE, METAL, BAZZ, DRONE,
  TEXTURE, FX oder WAV (ein Sample). Die Regler 1–5 wechseln mit der Engine, das Mini-Display zeigt ihre Namen
  (SYNTH: GEN 1, GEN 2, BLEND, B.MODE, G2 TUNE · Stimmen: ihre vier Klangregler + LIMIT · WAV: START, END, LENGTH,
  LIMIT). Darunter FILTER (LP12, LP24, BP, HP, LADDER), ON (nach dem Blend, auf GEN 1 oder GEN 2), CUTOFF, RES, OCT,
  LEVEL. Klick aufs Mini-Display: Engine- und Sample-Auswahl.
- **Eingänge:** V/OCT, GATE (polyphon, bis 16 Stimmen), VEL, CLOCK (Tempo und Takt für Arpeggiator, synchrone Bahnen,
  Delay; Standard 16tel wie GENRA), RESET, X/Y (steuern den Orbiter von Hand, ±5 V), CV für SHAPE, MODE, SPEED, SIZE,
  ROT, SUB, M1–M4, Pegel A–D, VOL, Cutoff A–D und Regler 1 jeder Ecke.
- **Ausgänge:** X, Y (wo der Orbiter ist, ±5 V), WT A–D (Anteil jeder Ecke, 0–10 V), ENV, A–D (jede Ecke einzeln,
  trocken), L, R.
- **Ordner:** Beim ersten Start legt UMLAUF `Rack2/GenraSynthis/Umlauf/` an: `Samples` (eigene WAVs, auch in
  Unterordnern) und `Shapes` (die 7 Beispiel-Formen von VADER; eigene SVG, OBJ, PNG, JPG dazulegen). Die
  Beispiel-Samples von VADER sind nicht dabei (sie stammen aus kommerziellen Sample-Packs).
- **Rechtsklick:** 22 Presets von VADER (die sechs Sample-Presets brauchen eigene WAVs), Formdatei für USER wählen oder
  laden, pro Ecke Engine, Sample und Abspielart (One Shot, Loop 1–4 Takte), Ordner öffnen, neu einlesen,
  Clock-Auflösung, Delay im Takt.
- Pro Modul ein Timbre (VADERs Timbre B, Layer/Split, gibt es hier nicht: für Layer zwei UMLAUF nehmen).

### UMLAUF FX: Expander

![UMLAUF FX](docs/entwuerfe/umlauf_fx.png)

Rechts neben UMLAUF setzen, 36 HP. ENVELOPE (ADSR) · VOICE (UNISON 1/2/4/8, VOICES 8/16, DETUNE, GLIDE, OCTAVE,
VELOCITY, Vibrato und Tremolo mit Rate) · ARPEGGIATOR (ON, LATCH, Richtung UP/DOWN/U·D/RND/ORDER/CHORD, RATE im
Takt, OCT, LENGTH, STEPS, HUMAN und 16 Schritt-Taster: Klick schaltet OFF → ON → ACCENT → TIE) · FX · MASTER (DRIVE,
CHORUS mit Rate, DELAY mit TIME und FEEDBACK, REVERB mit SIZE, M.RES = Resonanz aller Eckfilter, VOLUME) · 20
CV-Eingänge. Beim Andocken übernehmen die Regler den aktuellen Klang; ohne Expander gelten die Werte des Presets.

## Neu in v3

- **Genre pro Spur:** Unter den Reglern jeder Spur sitzt ein Genre-Feld. Klick öffnet das Menü: *Global (folgt GENRE)* oder ein eigenes Genre, nach Familien sortiert, mit Varianten. Der Wechsel greift am nächsten Taktanfang (Feld und Display zeigen „…“). Ein eigenes Genre-Feld würfelt die Spur auch bei LOCK neu – die Auswahl ist ein bewusster Befehl. Der globale GENRE-Regler ändert nur Spuren auf *Global*.
- **GENRE-CV pro Spur** (Buchse neben dem Feld): Mit Kabel wählt die Spannung das Genre der Spur, die Variante kommt aus dem Feld (falls gleiches Genre) oder vom VARIANT-Regler. Rechtsklick-Menü → *Genre-CV-Eingänge*: 0–10 V über alle 37 Genres oder 1/12 V pro Genre (Halbtöne – ein Keyboard über MIDI-CV wählt Genres per Taste, C = House).
- **Swing pro Spur:** Jede Spur swingt wie das Genre, das sie spielt (Lo-Fi-Melodie über geradem Techno). SWING verschiebt alle gemeinsam. Gilt auch für den MIDI-Export.
- **Neue CV-Eingänge:** FILL (Trigger: Fill/Drop am Loop-Ende), VARIANT, COMPLEX, COLOR (0–10 V, zum Regler addiert), DENS-CV pro Spur.
- **HARMONY** (früher CHORDS, jetzt blaugrün): NEW, LOCK, TRIG für die gemeinsame Akkordfolge, der Bass, Melodie und Pads folgen.
- **Ein- und Ausgänge (v3.1):** neue CV-Eingänge SWING (0–10 V addiert), BPM (interne Clock, 10 BPM pro Volt), LENGTH. Spalte **OUT**: CLOCK (16tel), RESET (Loop-Anfang), RUN (Gate), BAR (pro Takt), END (letzter Step), FILL (Gate während Fill/Drop), ROOT (Grundton des aktuellen Akkords, V/Oct zwischen C4 und B4), CHORD (ganzer Akkord, polyphon).
- **Szenen (8 Plätze):** Taster-Farbe: dunkel = leer, rot = Pattern gespeichert, grün = aktiv (blinkt bis zum Wechsel). Leerer Platz drücken = aktuellen Loop speichern (alle Spuren, Akkorde, Spur-Genres, Länge). Belegter Platz = abrufen, beim Spielen am nächsten Taktanfang (blinkt bis dahin). Überschreiben: STORE drücken (blinkt), dann den Platz. NEXT-Trigger = nächste belegte Szene, CV 0–10 V = Szene 1–8. Rechtsklick-Menü: *Alle Szenen löschen*. Szenen werden im Patch gespeichert.
- **Display:** Genre jeder Spur neben ihren Lauflichtern, aktive Szene, Akkordnamen mit # oder b passend zur Tonart.
- **Fehlerkorrekturen:** MIDI-Export/-Import mit exakter Länge und Akkordfolge, threadsichere Akkord-Bearbeitung, neues Speicherformat (v2-Patches werden übernommen; Spuren stehen dann auf *Global*).

![GENRA](docs/entwuerfe/genra_v3_entwurf.png)

GENRA erfindet genretypische Loops: **8 Drumspuren, Bass, Melodie und polyphone Pads**, die sich eine gemeinsame Tonart und Akkordfolge teilen. **37 Genres mit 83 Varianten**, alle Regler wirken live. Breite: 44 HP.

| Familie | Genres (Varianten) |
|---|---|
| House | House (Classic, Piano) · Deep House (Stabs, Pads) · Speed Garage (Warp, Organ) · UK Garage (4x4) · 2-Step |
| Techno | Techno (Rolling, Peak, Hypnotic) · Minimal · Acid (Classic, Double, Poly 12) · Electro (Classic, Funk, Detroit) · Trance (Uplifting, Psy) |
| Breaks & Bass | Breakbeat (Classic, Nu-Skool, Big Beat) · Jungle (Classic, Ragga, Dark) · Drum & Bass (Classic, Liquid, Neuro) · Dubstep (Deep, Heavy) · Grime (Eski, Sino) |
| Hip-Hop & Downtempo | Hip-Hop (Boom Bap) · Lo-Fi · Trap · Drill (UK) · Trip-Hop · Downtempo (Chillout, Organic) |
| Weitere | Dub (Roots, Steppers) · Synthwave (Classic, Outrun) · Ambient (Classic, Bright) |
| Afro (neu in v3) | Afrobeat (Classic, Tony Allen, Afro-Funk) · Afrobeats (Afropop, Afro-Swing, Afro-Piano) · Amapiano (Classic, Soulful, Log Drum) · Afro House (Classic, Organic, 3-Step) |
| Techno Styles (neu in v3) | Dub Techno (Classic, Deep Chord) · Melodic Techno (Afterlife, House) · Hard Techno (Industrial, Schranz, Rave) · Tech House (Classic, Rolling, Minimal) · Detroit Techno (Strings, Electro, Deep) |
| Latin & Club (neu in v3) | Baile Funk (Tamborzao, 150 BPM, Volt Mix) · Reggaeton (Perreo, Clasico, Dominicano) · Footwork (Battle, Soul, Juke) · Breakcore (Amen, Gabber, Melodic) |

---

## Installation unter Windows (über GitHub)

1. Diese Dateien im Repository ersetzen bzw. hochladen: `src/`, `res/`, `plugin.json`, `Makefile`, `.github/workflows/main.yml` (und optional `README.md`, `docs/`, `tools/`, `test/`).
2. Unter **Actions** startet der Build automatisch (3–5 Minuten).
3. Grünen Lauf öffnen → unten bei **Artifacts** `genra-windows` laden → Zip entpacken.
4. **Nur** `Genra-2.3.0-win-x64.vcvplugin` nach `%LOCALAPPDATA%\Rack2\plugins-win-x64\` kopieren. Ältere `Genra-2.x-…vcvplugin`-Dateien und den Ordner `Genra` dort vorher löschen.
5. Rack neu starten.

Die Versionsnummer liest der Workflow jetzt selbst aus `plugin.json`, für künftige Versionen genügt es, sie dort zu erhöhen.

> Patches aus v1 laden, aber das gespeicherte Pattern wird neu erzeugt (das Format hat sich geändert), und die Genre-Nummern sind neu sortiert.

---

## Neu in v2

- **NEW pro Spur + TRIG-Eingang:** würfelt nur Drums, Bass, Melodie oder Pads neu – auch wenn LOCK an ist (LOCK schützt nur vor NEW ALL, Genrewechsel und MUTATE).
- **Akkord-Bereich:** NEW (neue Akkordfolge, Spuren folgen live), CHORD LOCK, TRIG-Eingang.
- **VARIANT:** Spielart des Genres; Name im Display und beim Darüberfahren. Wechsel greift wie GENRE am nächsten Taktanfang.
- **SLIDE-Ausgänge** (Bass, Melodie): 10 V zu Beginn eines Glides, z. B. für Portamento am VCO oder einen Slew-Limiter. Der Gate bleibt dabei durchgehend offen (303-Stil).
- **Genre-Merkmale:** 2-Takt-Raster (Drill), Hi-Hat-Rolls/Triolen, Break-Chops (Jungle, D&B, Breakbeat), Drops und Breaks am Loop-Ende (Speed Garage, Jungle, Grime), Humanize (Lo-Fi, Hip-Hop, Trip-Hop), Polymeter-Bass (Acid Poly 12), zweite 303-Linie (Acid), genre-eigene Fills.

**Welcher Regler steuert was?**
- **COMPLEX** → Synkopen, Hi-Hat-Rolls/Triolen, wie wild Breaks gechoppt werden, Slides der Melodie
- **MOVE** → Bassbewegung und Bass-Slides
- **FILL** → Wahrscheinlichkeit für Fill **und** Drop am Loop-Ende
- **DENSITY** (Pads) → bei Grime sind Pads standardmässig aus; über der Mitte kommen sie dazu

---

## Bedienung (alle Elemente)

**Global:** GENRE, BPM (interne Clock), KEY, SCALE (Auto = Genre), VARIANT, COMPLEX, COLOR (Harmonik), MUTATE, SWING (relativ zum Genre jeder Spur), LENGTH (1/2/4/8 Takte), NEW ALL, RUN.
**Harmony:** NEW, LOCK, TRIG.
**Pro Spur:** DENSITY + DENS-CV · zweiter Regler (Drums: FILL, GHOST; Bass: MOVE; Melodie: RANGE; Pads: VOICING) · OCT (Bass/Melodie/Pads) · Genre-Feld + GENRE-CV · MUTE · LOCK · NEW · TRIG.
**Szenen:** 8 Plätze, STORE, CV, NEXT.
**Eingänge:** CLOCK, RESET, RUN, NEW, FILL, MUTATE, GENRE, VARIANT, TRANSP (V/Oct), COMPLEX, COLOR, CHORD (poly) · pro Spur GENRE-CV, DENS-CV, TRIG · Szenen CV, NEXT · HARMONY TRIG.
**Ausgänge:** 8 Drum-Trigger (Höhe = Velocity) · Bass/Melodie: V/OCT, GATE, VEL, ACC, SLIDE · Pads: V/OCT + GATE (poly 4), VEL, MOD.

**Display:** Akkord anklicken = nächste Alternative, Rechtsklick = Liste, Mausrad = mehr/weniger Farbe.
**Rechtsklick-Menü:** MIDI-Export/-Import, Akkord-Modus, Akkord-Vorlage, nur Akkorde neu würfeln, Genre-CV-Modus, alle Spuren auf Global, alle Szenen löschen, Clock-Auflösung.

---

## Projektstruktur
```
src/Engine.hpp/.cpp   Theorie, Akkorde, Generierung, Auswertung pro Step
src/Genres.cpp        24 Genres / 46 Profile (Raster in derselben Notation wie die Spezifikation)
src/Midi.cpp          MIDI-Export/-Import
src/Store.cpp         Speicherformat (pattern3) + Übernahme von v2-Patches
src/Edit.cpp          Bearbeiten: Einfrieren, Noten schreiben, Chord Follow, Snap, Copy/Paste
src/GenraModule.hpp   GENRA-Moduldefinition (auch vom Editor genutzt)
src/GenraGrid.cpp     Grid-Editor-Modul
res/GenraGrid.svg     Frontplatte des Editors (tools/grid_panel.py real)
src/Genra.cpp         Rack-Modul, Display, Menüs
res/Genra.svg         Frontplatte (tools/panel.py real), Vorschau: tools/panel.py preview <datei.svg>
src/jiant/            JIANT-Stimmen VOICE, DRUM-X, ANIMAL (Module, Kern-Lader, gemeinsamer Look JiantUI.hpp)
jiantcore/            die drei Firmware-Kerne als eigene kleine Bibliotheken (res/*core.dll beim Build)
jiant.tar.gz          Firmware-Quellen von JIANT FM (beim ersten Build nach jiant/ entpackt)
res/Jiant*.svg        Frontplatten (tools/jiant_panel.py real), Vorschau: tools/jiant_panel.py preview <ordner> <Rack>
src/mono/             GENRA MONO: Modul (GenraMono.cpp) und Klang-Engine (MonoDsp.hpp, ohne Rack testbar)
res/GenraMono.svg     Frontplatte (tools/mono_panel.py real), Vorschau: tools/mono_panel.py preview <datei.png> <Rack> [menu]
src/ui/GenraUI.hpp    GENRA-Look für SCHMELZ, MIX, K-WASUB, UMLAUF (Farben, Schriften, Lampenreihe)
src/schmelz/          SCHMELZ: Modul und Klang-Engine (SchmelzDsp.hpp, ohne Rack testbar)
src/mix/              MIX: Modul und Engine (MixDsp.hpp, ohne Rack testbar)
src/kwasub/           K-WASUB und K-WASUB FX: Module, Kern-Lader (KwCore), Parameter-Ids der Firmware
kwcore/               der K-WASUB-Kern: die Firmware als Bibliothek (res/kwcore.dll beim Build)
kwasub/               die K-WASUB-Firmware, unverändert (GPL-3.0, Omnipotard)
src/umlauf/           UMLAUF und UMLAUF FX (Module, Schnittstelle zu VADER)
vader/                VADER (MIT), gepatcht (tools/vader_vendor.sh, tools/vader_patch.py), in plugin.dll gelinkt
res/Schmelz.svg …     Frontplatten: tools/schmelz_panel.py, mix_panel.py, kwasub_panel.py, umlauf_panel.py,
                      umlauf_fx_panel.py (jeweils real | preview <datei.png> <Rack-Quellen>)
test/                 Engine- und MIDI-Tests (g++, ohne Rack); kwcore_test.c und vader_test.c (gcc), siehe Workflow
```

## Lizenzen

Das Plugin steht unter GPL-3.0. Darin enthalten (neben der JIANT- und der MONK-1-Firmware, siehe oben): die K-WASUB-Firmware (GPL-3.0, `kwasub/`) und VADER (MIT, `vader/`)
mit den LXRXXL-ZYNTH-Stimmen, Code von Mutable Instruments (MIT), stb_image (Public Domain/MIT) und der
FM-Algorithmus-Tabelle von music-synthesizer-for-android (Apache 2.0). Die Wavetables der LXR02-Firmware, die VADER
für TONE, METAL und BAZZ nutzt, sind nicht dabei (ihre Lizenz verbietet kommerzielle Nutzung): UMLAUF rechnet
gleichartige Tabellen selbst (`tools/umlauf_wavetables.py`).
