# Erledigt (aus der Merkliste)

Wünsche aus `docs/MERKLISTE.md`, die gebaut sind – zum Nachschauen.

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
