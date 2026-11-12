# Übersicht: wo liegt was

Jedes Modul hat einen eigenen Code-Ordner, ein Panel-Skript, eine Panel-Grafik und (wo es eine Klang- oder
Rechen-Engine gibt) einen Test ohne Rack. Panel-Bilder für README und Entwürfe liegen in `docs/panels/`.

## Plugin GENRA (Hauptordner)

| Modul (Slug) | Code | Panel-Skript (`tools/panels/`) | Grafik (`res/`) | Bild (`docs/panels/`) | Test (`test/`) |
| --- | --- | --- | --- | --- | --- |
| GENRA (`Genra`) | `src/genra/` | `genra_panel.py` | `Genra.svg` | `genra.png` | `test_engine`, `test_tracks`, `test_edit`, `test_store`, `test_midi`, `test_cycle`, `test_match`, `test_feel` |
| GRID (`GenraGrid`) | `src/grid/` | `grid_panel.py` | `GenraGrid.svg` | `genra_grid_piano.png`, `genra_grid_drums.png` | – |
| ABLAUF (`GenraAblauf`) | `src/ablauf/` | `ablauf_panel.py` | `Ablauf.svg` | `ablauf.png` | `test_ablauf` |
| GENRA BOX (`GenraBox`), ABLAUF MINI (`GenraAblaufMini`) | `src/box/` | `genrabox_panel.py`, `ablaufmini_panel.py` | `GenraBox.svg`, `AblaufMini.svg` | `genra_box.png`, `ablauf_mini.png` | `test_box` |
| MIX (`GenraMix`) | `src/mix/` | `mix_panel.py` | `Mix.svg` | `mix.png` | `test_mix` |
| GENRA FX (`GenraFx`) | `src/genrafx/` | `genrafx_panel.py` | `GenraFx.svg` | `genra_fx.png` | `test_genrafx` |
| STOTTER (`GenraStotter`) | `src/stotter/` | `stotter_panel.py` | `Stotter.svg` | `stotter.png` | `test_stotter` |
| SCHLAG (`Schlag`) | `src/schlag/` | `schlag_panel.py` | `Schlag.svg` | `schlag.png` | `test_schlag` |
| DRUM-X (`DrumX`) | `src/jiant/` (`DrumX.cpp`, `DrumCore.*`), Kern `jiantcore/drumcore.c` | `jiant_panel.py` | `JiantDrumX.svg` | `jiant_drumx.png` | – |
| GRUND (`Grund`), GRUND FX (`GrundFx`) | `src/grund/` | `grund_panel.py`, `grundfx_panel.py` | `Grund.svg`, `GrundFx.svg` | `grund.png`, `grund_fx.png` | `test_grund` |
| GENRA MONO (`GenraMono`) | `src/mono/` | `mono_panel.py` | `GenraMono.svg` | `genra_mono.png` (+ `_menu`, `_presets`) | `test_mono` |
| FUNKE (`Funke`) | `src/funke/` | `funke_panel.py` | `Funke.svg` | `funke.png` | `test_funke` |
| SCHMELZ (`Schmelz`) | `src/schmelz/` | `schmelz_panel.py` | `Schmelz.svg` | `schmelz.png` | `test_schmelz` |

## Plugin GENRA Instruments (`instruments/`)

Pfade relativ zu `instruments/`; Panel-Skripte und Bilder liegen im Hauptordner (`tools/panels/`, `docs/panels/`). Ausnahme: die zehn Module aus GENRA Test (seit 2.38.0) bringen ihre Panel-Skripte in `instruments/tools/` mit (dort ausführen; gemeinsame Teile `gpanel.py`, `jpanel.py`, `panellib.py`).

| Modul (Slug) | Code | Firmware / Kern | Panel-Skript | Grafik (`res/`) | Test (`test/`) |
| --- | --- | --- | --- | --- | --- |
| K-WASUB (`Kwasub`), K-WASUB FX (`KwasubFx`) | `src/kwasub/` | `kwasub/` (Firmware, unverändert), `kwcore/` | `kwasub_panel.py` | `Kwasub.svg`, `KwasubFx.svg` | `kwcore_test.c` |
| GOLANA (`Golana`), GOLANA FX (`GolanaFx`) | `src/golana/` | `golana/` (Firmware, unverändert), `glcore/` | `golana_panel.py`, `golana_fx_panel.py` | `Golana.svg`, `GolanaFx.svg` | `glcore_test.c` |
| UMLAUF (`Umlauf`), UMLAUF FX (`UmlaufFx`) | `src/umlauf/` | `vader/` (VADER, per `tools/vader_vendor.sh`) | `umlauf_panel.py`, `umlauf_fx_panel.py` | `Umlauf.svg`, `UmlaufFx.svg`, `umlauf/shapes` | `vader_test.c` |
| MONK (`Monk`) | `src/monk/` | – (Sänger-Bilder `res/monk/`, aus `tools/monk_singers.py`) | `monk_panel.py` | `Monk.svg` | – |
| VOICE (`Voice`), VOICE BEND (`VoiceBend`), ANIMAL (`Animal`) | `src/jiant/` | `../jiantcore/` (`voicecore.c`, `spellcore.c` mit `spellbox/`, `animalcore.c`) | `jiant_panel.py` | `JiantVoice.svg`, `JiantVoiceBend.svg`, `JiantAnimal.svg` | `test/spellcore_test.c` |
| MATH (`Math`) | `src/math/` | `mathcore/` + `jiant.tar.gz` (JIANT, Branch codex/swarm) | `tools/math_panel.py` | `Math.svg` | `test/mathcore_test.c` |
| KURBEL (`Kurbel`) | `src/kurbel/` | `kurbelcore/` + `estampie.tar.gz` | `tools/kurbel_panel.py` | `Kurbel.svg` | `test/kurbelcore_test.c` |
| SWARM (`Swarm`) | `src/swarm/` | `swarmcore/` + `jiant.tar.gz` | `tools/swarm_panel.py` | `Swarm.svg` | `test/swarmcore_test.c` |
| LIED (`Lied`) | `src/lied/` | `liedcore/` + `espeak.tar.gz`, Sprachdaten `res/espeak-ng-data` | `tools/lied_panel.py` | `Lied.svg` | `test/liedcore_test.c`, `test/lied_test.cpp` |
| CANTA (`Canta`) | `src/canta/` | `cantacore/` + `canta.tar.gz` (Sängerinnen: `tools/canta_sprites.py`) | `tools/canta_panel.py` | `Canta.svg` | `test/cantacore_test.c` |
| MIAOU (`Miaou`) | `src/miaou/` | `miaoucore/` (direkt ins Plugin gebaut) | `tools/miaou_panel.py` | `Miaou.svg` | `test/miaoucore_test.c` |
| SAURUS (`Saurus`) | `src/saurus/` | `sauruscore/` + `paleotone.tar.gz` | `tools/saurus_panel.py` | `Saurus.svg` | `test/sauruscore_test.c` |
| FUMI (`Fumi`) | `src/fumi/` | `fumicore/` + `fumi.tar.gz` (Pegel: `tools/fumi_levels.c`) | `tools/fumi_panel.py` | `Fumi.svg` | `test/fumicore_test.c` |
| KESSEL (`Kessel`), KESSEL FX (`KesselFx`) | `src/kessel/` | `kesselcore/` + `kessel.tar.gz` (Kits: `tools/kessel_kits.py`, `res/kessel/`) | `tools/kessel_panel.py` | `Kessel.svg`, `KesselFx.svg` | `test/kesselcore_test.c` |

## Gemeinsame Teile (in `src/`, von allen GENRA-Plugins benutzt)

| Datei | Inhalt |
| --- | --- |
| `src/plugin.hpp`, `src/plugin.cpp` | die dunklen Knöpfe (`res/knob/`), Modell-Liste von GENRA |
| `src/Follow.hpp/.cpp` | Follow GENRA (Verbindungen ohne Kabel) |
| `src/link/` | Link-Hub zwischen den Plugins (`Hub.*`, `GenraPorts.hpp`) |
| `src/ui/` | gemeinsame Bedienteile (`GenraUI`, Preset-/State-Bänke, grosse Pads) |
| `src/OutLimit.hpp` | Sicherheitslimiter an den Instrumenten-Ausgängen |
| `src/GenreMatch.hpp` | Presets passend zum Genre wählen |
| `src/jiant/JiantUI.hpp`, `JiantLayout.hpp` | Look der JIANT-Module (auch MONK und GENRA Test) |

`instruments/src/` reicht diese Teile über kleine Weiterleitungs-Header durch (keine Kopien). Das Testplugin GENRA Test
(Repo testmodulesvcv) holt sich `src/` und `res/knob/` vor jedem Build frisch aus diesem Repo; es ist seit 2.38.0 leer (Ablage für neue Test-Module).

## Weitere Ordner

| Ordner / Datei | Inhalt |
| --- | --- |
| `jiant.tar.gz`, `jiantcore/` | JIANT-FM-Firmware (beim Bauen entpackt) und die Kerne für DRUM-X, VOICE, ANIMAL |
| `tools/` | übrige Werkzeuge: Genre-Presets (`genre_presets.py`), Genre-Hörbeispiele (`genre_demo.cpp`), MONK-Sänger, UMLAUF-Wavetables, VADER einbinden |
| `licenses/`, `instruments/licenses/` | Lizenztexte der fremden Teile; jedes Plugin bringt seine eigene Kopie im Paket mit |
| `docs/` | `UEBERSICHT.md` (diese Datei), `DESIGN.md`, `MERKLISTE.md`, `GENRA_BOX.md`, `panels/`, `archiv/` |
| `PROJEKT.md`, `CHANGELOG.md` | Projektstand und Arbeitsweise, Versionsgeschichte |
