# Third-party material in GENRA Instruments

GENRA as a whole is free software under the **GNU General Public License, version 3 only** (`GPL-3.0-only`, full
text in `../LICENSE`). It contains the following parts by other authors. Each keeps its own licence; the texts are
in this folder. All of them are compatible with distributing the plugin under GPL-3.0.

| Component | Author(s) | Licence | Used in | Text |
| --- | --- | --- | --- | --- |
| JIANT FM, a fork of Felucca (firmware engines VOICE, DRUM-X, ANIMAL) | Juan Ignacio Antonucci (Juan Jiant); Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | VOICE, DRUM-X, ANIMAL (`jiantcore/`, `jiant/`) | `../LICENSE`, `JIANT-Felucca-LICENSING.md` |
| msfa (FM synthesis from Dexed), integer port in Felucca | Google Inc., Pascal Gauthier; port by Leo Kuroshita | Apache-2.0 | JIANT engines, K-WASUB, GOLANA core | `Apache-2.0-msfa.txt` |
| SPELLBOX (LPC speech chip, text to speech, its words and dictionary), on foma / Felucca | pjac1810; data made with espeak-ng (GPL-3.0), word choice from wordfreq (CC BY-SA 4.0) | GPL-3.0-only | VOICE SPELL, VOICE BEND (`spellbox/`, `../jiantcore/spellcore.c`) | `../LICENSE`, `spellbox/LICENSE`, `SPELLBOX-LICENSING.md` |
| CMU Pronouncing Dictionary (word list of VOICE) | Carnegie Mellon University | BSD-2-Clause | VOICE | `BSD-2-Clause-CMUdict.txt` |
| Fukiai icon font, fm1-clk320-probe (parts of the JIANT firmware tree) | Hügelton Instruments; curist | MIT | JIANT firmware build | `MIT-Fukiai.txt`, `MIT-fm1-clk320.txt` |
| Chakra Petch, IBM Plex Mono, Michroma (rasterised into the JIANT firmware) | The Chakra Petch / IBM Plex / Michroma Project Authors | SIL OFL 1.1 | JIANT firmware build | `OFL-ChakraPetch.txt`, `OFL-IBMPlexMono.txt`, `OFL-Michroma.txt` |
| K-WASUB / KAWA firmware (sound engines), Felucca platform | Omnipotard; Leo Kuroshita, Hügelton Instruments; isod89 (SLOOP); zvenson (Dinghy) | GPL-3.0-only | K-WASUB, K-WASUB FX (`kwasub/`, `kwcore/`) | `../LICENSE`, `K-WASUB-KAWA-LICENSING.md` |
| Screen fonts rasterised into the K-WASUB firmware (Terminus, IBM Plex Mono, Atkinson Hyperlegible Mono, Press Start 2P, Outfit, Instrument Sans, Tektur, JetBrains Mono, Big Shoulders) | their project authors (see `K-WASUB-KAWA-LICENSING.md`) | SIL OFL 1.1 | K-WASUB display | `OFL-IBMPlexMono.txt` (the OFL 1.1 text; copyright lines in `K-WASUB-KAWA-LICENSING.md`) |
| Golana (FM-1 firmware: the GOLANA engine, voices, arp, effects), a fork of Felucca | Gadlele; Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | GOLANA, GOLANA FX (`golana/`, `glcore/`) | `../LICENSE`, `Golana-Felucca-LICENSING.md` |
| DaisySP, Rings (ported to fixed point in Felucca's PHYS engine, built into the GOLANA core) | Electrosmith, Corp and Emilie Gillet | MIT | GOLANA core | `MIT-DaisySP.txt`, `MIT-Rings.txt` |
| Instrument samples (Versilian Studios VSCO-2 CE, VCSL; in Felucca's SAMPLE engine, built into the GOLANA core) | Versilian Studios | CC0 1.0 | GOLANA core | `CC0-Golana-samples-ATTRIBUTION.txt` |
| MONK-1 (FM-1 firmware: the port of the voice engine) | Charles Vestal | GPL-3.0-only | MONK (`instruments/src/monk/`) | `../LICENSE` |
| MonkSynth (FOF voice DSP) | Jonathan Taylor | MIT | MONK | `MIT-MonkSynth.txt` |
| VADER (orbit vector synth) | L'Cronx (sunskiefer) | MIT | UMLAUF, UMLAUF FX (`vader/`) | `MIT-VADER.txt` |
| mpc-vst-plugins (wrapper used by VADER) | sd88me | MIT | UMLAUF | `MIT-mpc-vst-plugins.txt` |
| Braids, Plaits, stmlib | Emilie Gillet (Mutable Instruments) | MIT | UMLAUF voices | `MIT-MutableInstruments.txt`, `MIT-stmlib.txt` |
| stb_image / stb_image_write | Sean Barrett | MIT or Public Domain | UMLAUF | `MIT-or-PublicDomain-stb.txt` |
| Inter (the panel lettering, drawn as vector paths) | The Inter Project Authors | SIL OFL 1.1 | all panels (`res/*.svg`) | `OFL-Inter.txt` |
| JIANT FM, a fork of Felucca (firmware engines MATH, SWARM; snapshot of branch codex/swarm in `jiant.tar.gz` of this folder, with the firmware around it) | Juan Ignacio Antonucci (Juan Jiant); Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | MATH, SWARM (`mathcore/`, `swarmcore/`, `jiant.tar.gz`) | `../LICENSE`, `JIANT-Felucca-LICENSING-MATH-SWARM.md` |
| msfa (FM synthesis from Dexed), integer port in Felucca | Google Inc., Pascal Gauthier; port by Leo Kuroshita | Apache-2.0 | JIANT firmware build | `Apache-2.0-msfa.txt` |
| CMU Pronouncing Dictionary (word list in the firmware) | Carnegie Mellon University | BSD-2-Clause | JIANT firmware build | `BSD-2-Clause-CMUdict.txt` |
| Fukiai icon font, fm1-clk320-probe (parts of the JIANT firmware tree) | Hügelton Instruments; curist | MIT | JIANT firmware build | `MIT-Fukiai.txt`, `MIT-fm1-clk320.txt` |
| Chakra Petch, IBM Plex Mono, Michroma (rasterised into the JIANT firmware) | their project authors | SIL OFL 1.1 | JIANT firmware build | `OFL-ChakraPetch.txt`, `OFL-IBMPlexMono.txt`, `OFL-Michroma.txt` |
| ESTAMPIE (a Felucca fork with GHOULBOX's hurdy-gurdy): the GURDY engine and the rest of its firmware | ESTAMPIE's authors (github.com/ocean/estampie); GHOULBOX (Jason Persinger); Leo Kuroshita, Hügelton Instruments (Felucca) | GPL-3.0-only | KURBEL (`kurbelcore/`, `estampie.tar.gz`) | `../LICENSE`, `ESTAMPIE-LICENSING.md` |
| Felucca-Engines (TRIO: KURBEL's shawm, and ANALOG, LOFI, WHEEL, NOISE in the firmware tree) | Leo Kuroshita, Hügelton Instruments | MIT | KURBEL core | `estampie/MIT-Felucca-Engines.txt` |
| Further parts of the ESTAMPIE tree (DaisySP, Rings, msfa, fonts rasterised into the firmware, felucca-bsp) | see `ESTAMPIE-LICENSING.md` | MIT, Apache-2.0, SIL OFL 1.1, MPL-2.0 | KURBEL core | `estampie/` |
| eSpeak NG 1.52.0 (speech synthesis, ucd-tools, the compiled language data) | Jonathan Duddington, Reece H. Dunn and the eSpeak NG contributors | GPL-3.0-or-later | LIED (`liedcore/`, `espeak.tar.gz`, `res/espeak-ng-data`) | `../LICENSE`, `ESPEAK-NG-LICENSING.md` |
| MIKU-FM: the singing voice only (fvoice, vfx, the mora table, the lyrics grammar; voice logic of engine.c rebuilt in `cantacore/`). **No art**: MIKU-FM's character art (Hatsune Miku, Crypton Future Media, Piapro Character License) is not used; CANTA's singers are its own | MIKU-FM's author (github.com/MichaelAPerry/MIKU-FM1) | GPL-3.0-only | CANTA (`cantacore/`, `canta.tar.gz`) | `../LICENSE`, `MIKU-FM-LICENSING.md` |
| MIAOU: the five sounds and the toy speaker of `src/toy.c` (commit 417e693), its SVF table (`src/tables.h`) and two helpers of foma's `platform/common.c` (`sine`, `note_inc`); no beats, songs, recorder or sequencer, no art (MIAOU's cat is its own) | MIAOU's author (github.com/pjac1810/miaou), on foma (konsumer) and Felucca (Leo Kuroshita, Hügelton Instruments) | GPL-3.0-only | MIAOU (`miaoucore/`) | `../LICENSE`, `MIAOU-LICENSING.md` |
| FuMi-1: the instrument (pitch core, the ST-50 tunings, trill and ornaments, 6-operator FM, plate reverb) and FuMi-1's six own patches. **Not** its fifteen community DX7 patches (patches.fm, no licence stated by their authors): left out | cartesive (github.com/cartesive/fumi-1); Felucca (Leo Kuroshita, Hügelton Instruments); FoMni (Charles Vestal) | GPL-3.0-only (msfa in it: Apache-2.0) | FUMI (`fumicore/`, `fumi.tar.gz`) | `../LICENSE`, `FUMI-1-LICENSING.md`, `Apache-2.0-msfa.txt` |
| Noto Sans CJK JP Bold: the outlines of the 31 kanji and kana FUMI shows (`src/fumi/FumiGlyphs.hpp`) | Adobe, Google | SIL OFL 1.1 | FUMI | `OFL-NotoSansCJK.txt` |
| LXR firmware: its sound engine (six drum voices, effects, effect bus, init kit), unchanged, with KESSEL's glue and the firmware's parameter handler (`kesselcore/kc_cc.c`) on top. Modified version, not official Erica Synths firmware; KESSEL is not affiliated with or endorsed by Erica Synths ("Erica Synths", "LXR" are their trademarks) | Erica Synths (github.com/erica-synths/lxr-desktop-app) | GPL-3.0-only, with the additional permission and terms of its NOTICE.md | KESSEL, KESSEL FX (`kesselcore/`, `kessel.tar.gz`) | `../LICENSE`, `LXR-LICENSING.md`, `LXR-NOTICE.md` |

UMLAUF does not contain the LXR-02 firmware's wavetables (their licence forbids commercial use); it computes its own
tables (`tools/umlauf_wavetables.py`).

Names such as "FM-1", "M-VAVE", "Felucca", "Hügelton Instruments", "Delay Lama" and "VCV" belong to their owners and
are used only to credit the origin of the code; GENRA is not made or endorsed by them.
