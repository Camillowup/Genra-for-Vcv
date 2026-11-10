# K-WASUB firmware sources (unchanged)

The sound of the K-WASUB module is the K-WASUB firmware for the M-VAVE FM-1 by Omnipotard (Guizmoh),
<https://github.com/Guizmoh/K-WASUB_Firmware_for_FM1>, GPL-3.0 (see LICENSE and LICENSING.md here).
K-WASUB is born from KAWA (Omnipotard / Leo Kuroshita, Hügelton Instruments).

Copied unchanged from `source/firmware/{src,gen,hal}` and `source/web/wasm_host.c` of version 1.0.52.
`kwcore/kwcore.c` builds it as a small library (the browser emulator's host, `KAWA_WASM`), one private copy per
module instance (the firmware keeps its state in static variables).
