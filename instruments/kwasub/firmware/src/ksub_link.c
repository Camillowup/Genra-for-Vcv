/* SPDX-License-Identifier: GPL-3.0-only */
/* K-WASUB: the link with the web page (DATA): the RESAMPLE recording read out; the backup of the sound design (the 32 user slots, SEQ BASS / ARP /
 * MACROS / CC / EQ / MORPH, the slots' rest) read out and written back, over the USB-MIDI SysEx the updater uses.
 *
 * Frames (between F0 and F7, every byte 7 bits): 7D 4B 57 cmd pack7(body)        (7D: non-commercial; "KW")
 * the answer:                                    7D 4B 57 cmd|40 pack7(body)
 *   01 HELLO                         -> proto, blob size (u16), the version (text)
 *   02 READ  off u16, n u8 (<= 128)  -> off u16, n, the bytes
 *   03 BEGIN size u16                -> status
 *   04 WRITE off u16, n u8, bytes    -> off u16, status
 *   05 COMMIT crc16 (u16)            -> status (sent once written: the flash work is done from the main loop)
 *   06 RS INFO                       -> length (u32, samples; 0 while recording), rate (u32)
 *   07 RS READ off u16, n u8         -> off u16, n, the recording's mu-law bytes
 *   08 MULAW k u8, n u8 (<= 64)      -> k, n, the decoding table's int16 values
 *   09 SMP BEGIN len u16 (<= RS_MAX) -> status       a sample of the user's (mu-law, 11025 Hz) into the RESAMPLE memory:
 *   0A SMP WRITE off u16, n u8, data -> off, status  OSC C plays it, until the FM-1 is switched off (or RESAMPLE records)
 *   0B SMP DONE                      -> status
 * status: 0 ok, 1 bad size / offset, 2 bad crc, 3 not a K-WASUB backup (or another layout), 4 flash failed, 5 busy
 * The blob: "KSB1", the sizes of the three parts (u16 each), then ustore, perf, uext, then sqx (SEQ BASS past 16 steps,
 * the chances and conditions), then dk (DRUMS); the total size tells the layouts apart. Little endian. */
#define LK_PROTO 1u
#define LK_HEAD 10u
#define LK_SIZE (LK_HEAD + sizeof ustore + sizeof perf + sizeof uext + sizeof sqx + sizeof dk + sizeof pl)   /* (a backup from before 0.9.57: without pl) */
typedef char lk_fits[LK_SIZE < 65536u ? 1 : -1];
static uint8_t lk_stage[LK_SIZE], lk_out[2 + 3 + 1 + 160];
static uint32_t lk_got, lk_want;                /* staged bytes; the size BEGIN announced (0: no restore under way) */
static uint8_t lk_commit;                       /* 1: COMMIT asked, the main loop writes */
static uint16_t lk_commit_crc;
static uint32_t lk_smp_want;                    /* the sample being sent (bytes; 0: none) */
static uint8_t lk_smp_done;                     /* 1: sent, the main loop puts it on OSC C */

static uint32_t lk_crc16(const uint8_t *p, uint32_t n)   /* (poly 0x1021, init 0: as the updater's) */
{
    uint32_t c = 0, i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++) c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}
static uint32_t lk_pack7(const uint8_t *in, uint32_t n, uint8_t *out)   /* 8 bits to 7, LSB first */
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8u;
        while (nb >= 7u) { out[o++] = (uint8_t)(acc & 0x7Fu); acc >>= 7; nb -= 7u; }
    }
    if (nb) out[o++] = (uint8_t)(acc & 0x7Fu);
    return o;
}
static uint32_t lk_unpack7(const uint8_t *in, uint32_t n, uint8_t *out, uint32_t max)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)(*in++ & 0x7Fu) << nb;
        nb += 7u;
        if (nb >= 8u) { if (o < max) out[o] = (uint8_t)acc; o++; acc >>= 8; nb -= 8u; }
    }
    return o;
}
static void lk_blob_read(uint32_t off, uint8_t *d, uint32_t n)   /* the backup as it is now, byte by byte */
{
    static const uint32_t S1 = sizeof ustore, S2 = sizeof perf, S3 = sizeof uext, S4 = sizeof sqx, S5 = sizeof dk, S6 = sizeof pl;
    uint32_t i;
    for (i = 0; i < n; i++, off++) {
        uint32_t o = off;
        uint8_t b;
        if (o < LK_HEAD) {
            const uint8_t H[LK_HEAD] = {'K', 'S', 'B', '1', (uint8_t)S1, (uint8_t)(S1 >> 8), (uint8_t)S2, (uint8_t)(S2 >> 8), (uint8_t)S3, (uint8_t)(S3 >> 8)};
            b = H[o];
        } else if ((o -= LK_HEAD) < S1) b = ((const uint8_t *)ustore)[o];
        else if ((o -= S1) < S2) b = ((const uint8_t *)&perf)[o];
        else if ((o -= S2) < S3) b = ((const uint8_t *)&uext)[o];
        else if ((o -= S3) < S4) b = ((const uint8_t *)&sqx)[o];
        else if ((o -= S4) < S5) b = ((const uint8_t *)&dk)[o];
        else if ((o -= S5) < S6) b = ((const uint8_t *)&pl)[o];
        else b = 0;
        d[i] = b;
    }
}

#if defined(KAWA_WASM)
static uint8_t w_link_out[4096];                /* the emulator: the answers wait here for the page */
static uint32_t w_link_n;
static void lk_wire(const uint8_t *p, uint32_t n) { if (w_link_n + n <= sizeof w_link_out) { memcpy(w_link_out + w_link_n, p, n); w_link_n += n; } }
#else
static void lk_wire(const uint8_t *p, uint32_t n) { (void)ota_wire_send(p, n); }
#endif
static void lk_send(uint32_t cmd, const uint8_t *body, uint32_t n)   /* n <= 136 */
{
    uint32_t w = 0;
    lk_out[w++] = 0xF0; lk_out[w++] = 0x7D; lk_out[w++] = 0x4B; lk_out[w++] = 0x57;
    lk_out[w++] = (uint8_t)(0x40u | cmd);
    w += lk_pack7(body, n, lk_out + w);
    lk_out[w++] = 0xF7;
    lk_wire(lk_out, w);
}
static void lk_status(uint32_t cmd, uint32_t st) { uint8_t b = (uint8_t)st; lk_send(cmd, &b, 1); }

static int app_sysex(const uint8_t *p, uint32_t n)
{
    uint8_t b[160];
    uint32_t d;
    if (n < 4u || p[0] != 0x7Du || p[1] != 0x4Bu || p[2] != 0x57u) return 0;   /* not ours: the updater's */
    d = lk_unpack7(p + 4, n - 4u, b, sizeof b);
    if (d > sizeof b) return 1;
    switch (p[3]) {
    case 0x01: {                                /* HELLO */
        uint8_t r[40];
        uint32_t k = 0, i;
        const char *v = APP_VER;
        r[k++] = LK_PROTO; r[k++] = (uint8_t)LK_SIZE; r[k++] = (uint8_t)(LK_SIZE >> 8);
        for (i = 0; v[i] && k < sizeof r; i++) r[k++] = (uint8_t)v[i];
        lk_send(0x01, r, k);
        break;
    }
    case 0x02: {                                /* READ */
        uint8_t r[3 + 128];
        uint32_t off = d >= 3u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0xFFFFu, m = d >= 3u ? b[2] : 0u;
        if (m > 128u) m = 128u;
        if (off >= LK_SIZE) m = 0;
        else if (off + m > LK_SIZE) m = LK_SIZE - off;
        r[0] = (uint8_t)off; r[1] = (uint8_t)(off >> 8); r[2] = (uint8_t)m;
        lk_blob_read(off, r + 3, m);
        lk_send(0x02, r, 3u + m);
        break;
    }
    case 0x03: {                                /* BEGIN */
        uint32_t sz = d >= 2u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0u;
        if (lk_commit) { lk_status(0x03, 5); break; }
        if (sz != LK_SIZE && sz != LK_SIZE - sizeof pl) { lk_want = 0; lk_status(0x03, 3); break; }
        lk_want = sz; lk_got = 0;
        lk_status(0x03, 0);
        break;
    }
    case 0x04: {                                /* WRITE */
        uint8_t r[3];
        uint32_t off = d >= 3u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0xFFFFu, m = d >= 3u ? b[2] : 0u;
        r[0] = (uint8_t)off; r[1] = (uint8_t)(off >> 8); r[2] = 0;
        if (!lk_want || lk_commit || d < 3u + m || off + m > lk_want) r[2] = 1;
        else {
            memcpy(lk_stage + off, b + 3, m);
            if (off + m > lk_got) lk_got = off + m;
        }
        lk_send(0x04, r, 3);
        break;
    }
    case 0x05:                                  /* COMMIT: checked here, written by the main loop (link_service) */
        if (!lk_want || lk_got != lk_want || d < 2u) { lk_status(0x05, 1); break; }
        lk_commit_crc = (uint16_t)(b[0] | b[1] << 8);
        lk_commit = 1;
        break;
    case 0x06: {                                /* RS INFO: the RESAMPLE recording's length (samples) and rate; 0 while recording */
        uint32_t n2 = rs_want ? 0u : rs_desc.len;
        uint8_t r[8] = {(uint8_t)n2, (uint8_t)(n2 >> 8), (uint8_t)(n2 >> 16), (uint8_t)(n2 >> 24), (uint8_t)RS_RATE, (uint8_t)(RS_RATE >> 8), (uint8_t)(RS_RATE >> 16), 0};
        lk_send(0x06, r, 8);
        break;
    }
    case 0x07: {                                /* RS READ off u16, n u8 (<= 128): the recording's mu-law bytes */
        uint8_t r[3 + 128];
        uint32_t off = d >= 3u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0xFFFFu, m = d >= 3u ? b[2] : 0u, len = rs_want ? 0u : rs_desc.len;
        if (m > 128u) m = 128u;
        if (off >= len) m = 0;
        else if (off + m > len) m = len - off;
        r[0] = (uint8_t)off; r[1] = (uint8_t)(off >> 8); r[2] = (uint8_t)m;
        memcpy(r + 3, rs_buf + (off < len ? off : 0u), m);
        lk_send(0x07, r, 3u + m);
        break;
    }
    case 0x08: {                                /* MULAW from u8, n u8 (<= 64): the decoding table, as int16 */
        uint8_t r[2 + 128];
        uint32_t k0 = d >= 2u ? b[0] : 0u, m = d >= 2u ? b[1] : 0u, i;
        if (m > 64u) m = 64u;
        if (k0 + m > 256u) m = 256u - k0;
        r[0] = (uint8_t)k0; r[1] = (uint8_t)m;
        for (i = 0; i < m; i++) { int32_t v = KS_MULAW[k0 + i]; r[2 + 2 * i] = (uint8_t)v; r[3 + 2 * i] = (uint8_t)((uint32_t)v >> 8); }
        lk_send(0x08, r, 2u + 2u * m);
        break;
    }
    case 0x09: {                                /* SMP BEGIN: the RESAMPLE memory emptied for it */
        uint32_t len = d >= 2u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0u;
        if (rs_want || lk_smp_done) { lk_status(0x09, 5); break; }
        if (!len || len > RS_MAX) { lk_status(0x09, 1); break; }
        rs_desc.len = 0;                        /* (OSC C stops playing the old one first) */
        lk_smp_want = len;
        lk_status(0x09, 0);
        break;
    }
    case 0x0A: {                                /* SMP WRITE */
        uint8_t r[3];
        uint32_t off = d >= 3u ? (uint32_t)b[0] | (uint32_t)b[1] << 8 : 0xFFFFu, m = d >= 3u ? b[2] : 0u;
        r[0] = (uint8_t)off; r[1] = (uint8_t)(off >> 8); r[2] = 0;
        if (!lk_smp_want || rs_want || d < 3u + m || off + m > lk_smp_want) r[2] = 1;
        else memcpy(rs_buf + off, b + 3, m);
        lk_send(0x0A, r, 3);
        break;
    }
    case 0x0B:                                  /* SMP DONE */
        if (!lk_smp_want || rs_want) { lk_status(0x0B, 1); break; }
        lk_smp_done = 1;
        break;
    default:
        break;
    }
    return 1;
}

static void link_service(void)                  /* the main loop: a restore asked for, written to the flash */
{
    uint32_t st = 0;
    if (lk_smp_done) {                          /* a sample sent: OSC C plays it */
        rs_desc.name = "MY SAMPLE";
        rs_desc.len = lk_smp_want;
        lk_smp_want = 0; lk_smp_done = 0;
        patch[SM_SEL] = SM_RS;
        if (!patch[SM_LVL]) patch[SM_LVL] = 90;
        prev_dirty = 1; g_force = 1;
        toast("SAMPLE LOADED \xB7 OSC C");
        lk_status(0x0B, 0);
    }
    if (!lk_commit) return;
    if (lk_crc16(lk_stage, lk_want) != lk_commit_crc) st = 2;
    else {
        uint8_t h[LK_HEAD];
        uint32_t i;
        lk_blob_read(0, h, LK_HEAD);
        for (i = 0; i < LK_HEAD; i++) if (h[i] != lk_stage[i]) st = 3;   /* ("KSB1" and this firmware's sizes) */
    }
    if (!st) {
        memcpy(ustore, lk_stage + LK_HEAD, sizeof ustore);
        memcpy(&perf, lk_stage + LK_HEAD + sizeof ustore, sizeof perf);
        memcpy(&uext, lk_stage + LK_HEAD + sizeof ustore + sizeof perf, sizeof uext);
        memcpy(&sqx, lk_stage + LK_HEAD + sizeof ustore + sizeof perf + sizeof uext, sizeof sqx);
        memcpy(&dk, lk_stage + LK_HEAD + sizeof ustore + sizeof perf + sizeof uext + sizeof sqx, sizeof dk);
        sqx_dirty = 1;
        dk_dirty = 1;
        if (lk_want == LK_SIZE) memcpy(&pl, lk_stage + LK_HEAD + sizeof ustore + sizeof perf + sizeof uext + sizeof sqx + sizeof dk, sizeof pl);
        else pl_defaults();
        pl_dirty = 1;
        store_fixup();
        if (store_save() || perf_save()) st = 4;
        perf_dirty = 0;
        toast(st ? "RESTORE FAILED" : "BACKUP RESTORED");
        g_force = 1;
    }
    lk_commit = 0; lk_want = 0; lk_got = 0;
    lk_status(0x05, st);
}

#if defined(KAWA_WASM)
static uint8_t w_link_in[640];                  /* the page writes one frame here (the bytes between F0 and F7) */
WASM_EXPORT("ksub_link_in") uint8_t *ksub_link_in(void) { return w_link_in; }
WASM_EXPORT("ksub_link_frame") void ksub_link_frame(uint32_t n) { if (n <= sizeof w_link_in) app_sysex(w_link_in, n); link_service(); }
WASM_EXPORT("ksub_link_out") uint8_t *ksub_link_out(void) { return w_link_out; }
WASM_EXPORT("ksub_link_take") uint32_t ksub_link_take(void) { uint32_t n = w_link_n; w_link_n = 0; return n; }   /* (read the bytes before the next frame) */
#endif
