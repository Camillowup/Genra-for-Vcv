# vader/ in GENRA SYNTHIS

The engine of [VADER](https://github.com/sunskiefer/VADER) by L'Cronx (MIT, see `LICENSE`), commit in `VENDORED_COMMIT`,
copied by `tools/vader_vendor.sh` and built into the plugin by the Makefile the way VADER's own desktop plugin builds it
(`desktop/view_bus.c` in place of the Force's display plane and web server, `desktop/compat.h` on Windows).

Changes (every one marked `UMLAUF` in the source, applied by `tools/vader_patch.py`):

1. Function-level scratch buffers (`static`) are `_Thread_local`: several UMLAUF modules render on different threads.
2. `vader_t` has a corner bus (`cbus`): each voice also adds every corner's share (after its filter, weight and the
   envelope, before the effects) for the outputs A–D.
3. A few functions at the end of `src/engine.c` for the module: `vader_set_corner_bus`, `vader_list_count`,
   `vader_list_path`, `vader_voices_on`.

Left out: the Force's display plane (`src/overlay.c`), the web server (`src/web.c`), the VST2 wrapper and the DPF
desktop plugin, the example samples (from commercial sample packs), and the LXR02 firmware's wavetables (LXR02
Open-Source licence: no commercial use, which cannot be part of a GPL-3.0 plugin). `src/zynth/lxrtab/` holds tables of
the same shape computed by `tools/umlauf_wavetables.py` (correlation with the originals above 0.998).

Test: `test/vader_test.c` (every engine, mode, shape and preset; the corner bus; the state; two instances on two
threads), run by the GitHub Actions build.
