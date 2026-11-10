# Third-party material in GENRA

GENRA as a whole is free software under the **GNU General Public License, version 3 only** (`GPL-3.0-only`, full
text in `../LICENSE`). It contains the following parts by other authors. Each keeps its own licence; the texts are
in this folder. All of them are compatible with distributing the plugin under GPL-3.0.

| Component | Author(s) | Licence | Used in | Text |
| --- | --- | --- | --- | --- |
| JIANT FM, a fork of Felucca (firmware engines VOICE, DRUM-X, ANIMAL) | Juan Ignacio Antonucci (Juan Jiant); Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | VOICE, DRUM-X, ANIMAL (`jiantcore/`, `jiant/`) | `../LICENSE`, `JIANT-Felucca-LICENSING.md` |
| msfa (FM synthesis from Dexed), integer port in Felucca | Google Inc., Pascal Gauthier; port by Leo Kuroshita | Apache-2.0 | JIANT engines, K-WASUB, GOLANA core | `Apache-2.0-msfa.txt` |
| CMU Pronouncing Dictionary (word list of VOICE) | Carnegie Mellon University | BSD-2-Clause | VOICE | `BSD-2-Clause-CMUdict.txt` |
| Fukiai icon font, fm1-clk320-probe (parts of the JIANT firmware tree) | Hügelton Instruments; curist | MIT | JIANT firmware build | `MIT-Fukiai.txt`, `MIT-fm1-clk320.txt` |
| Chakra Petch, IBM Plex Mono, Michroma (rasterised into the JIANT firmware) | The Chakra Petch / IBM Plex / Michroma Project Authors | SIL OFL 1.1 | JIANT firmware build | `OFL-ChakraPetch.txt`, `OFL-IBMPlexMono.txt`, `OFL-Michroma.txt` |
| K-WASUB / KAWA firmware (sound engines), Felucca platform | Omnipotard; Leo Kuroshita, Hügelton Instruments; isod89 (SLOOP); zvenson (Dinghy) | GPL-3.0-only | K-WASUB, K-WASUB FX (`kwasub/`, `kwcore/`) | `../LICENSE`, `K-WASUB-KAWA-LICENSING.md` |
| Screen fonts rasterised into the K-WASUB firmware (Terminus, IBM Plex Mono, Atkinson Hyperlegible Mono, Press Start 2P, Outfit, Instrument Sans, Tektur, JetBrains Mono, Big Shoulders) | their project authors (see `K-WASUB-KAWA-LICENSING.md`) | SIL OFL 1.1 | K-WASUB display | `OFL-IBMPlexMono.txt` (the OFL 1.1 text; copyright lines in `K-WASUB-KAWA-LICENSING.md`) |
| Golana (FM-1 firmware: the GOLANA engine, voices, arp, effects), a fork of Felucca | Gadlele; Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | GOLANA, GOLANA FX (`golana/`, `glcore/`) | `../LICENSE`, `Golana-Felucca-LICENSING.md` |
| DaisySP, Rings (ported to fixed point in Felucca's PHYS engine, built into the GOLANA core) | Electrosmith, Corp and Emilie Gillet | MIT | GOLANA core | `MIT-DaisySP.txt`, `MIT-Rings.txt` |
| Instrument samples (Versilian Studios VSCO-2 CE, VCSL; in Felucca's SAMPLE engine, built into the GOLANA core) | Versilian Studios | CC0 1.0 | GOLANA core | `CC0-Golana-samples-ATTRIBUTION.txt` |
| MONK-1 (FM-1 firmware: the port of the voice engine) | Charles Vestal | GPL-3.0-only | MONK (`src/monk/`) | `../LICENSE` |
| MonkSynth (FOF voice DSP) | Jonathan Taylor | MIT | MONK | `MIT-MonkSynth.txt` |
| VADER (orbit vector synth) | L'Cronx (sunskiefer) | MIT | UMLAUF, UMLAUF FX (`vader/`) | `MIT-VADER.txt` |
| mpc-vst-plugins (wrapper used by VADER) | sd88me | MIT | UMLAUF | `MIT-mpc-vst-plugins.txt` |
| Braids, Plaits, stmlib | Emilie Gillet (Mutable Instruments) | MIT | UMLAUF voices | `MIT-MutableInstruments.txt`, `MIT-stmlib.txt` |
| stb_image / stb_image_write | Sean Barrett | MIT or Public Domain | UMLAUF | `MIT-or-PublicDomain-stb.txt` |
| DARK-1 (genre habits: chord moves, step tricks and sections per genre, the five dark genres; adapted as data, no code) | Polystellarflow; PERF-1 by deamonSLINKY, HYP-1 by RedtailWorks, AMB-1 by Charles Vestal | GPL-3.0-only | GENRA engine (`src/Genres.cpp`, `src/Engine.cpp`) | `../LICENSE` |
| Inter (the panel lettering, drawn as vector paths) | The Inter Project Authors | SIL OFL 1.1 | all panels (`res/*.svg`) | `OFL-Inter.txt` |

UMLAUF does not contain the LXR-02 firmware's wavetables (their licence forbids commercial use); it computes its own
tables (`tools/umlauf_wavetables.py`).

Names such as "FM-1", "M-VAVE", "Felucca", "Hügelton Instruments", "Delay Lama" and "VCV" belong to their owners and
are used only to credit the origin of the code; GENRA is not made or endorsed by them.
