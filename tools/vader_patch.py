#!/usr/bin/env python3
"""The few changes UMLAUF makes to the vendored VADER engine (tools/vader_vendor.sh runs it on vader/).

1. Scratch buffers: VADER keeps some work arrays in function-level statics (one plugin instance on the Force). In VCV
   Rack several UMLAUF modules render at once on different threads, so each thread gets its own (_Thread_local).
2. The corner outputs: each voice also adds what each corner contributes (after its filter, its weight and the
   envelope, before the effects) into a bus the module hands over (vader_set_corner_bus); NULL: nothing extra.
3. A few doors for the module: the corner bus, the shape and sample lists, the voices playing.
4. The samples folder the user picks (one per computer): set_param "samples_dir", scanned after VADER's own places.
5. (2.37) The folders scanned off the audio thread (vader_scan / vader_lists_swap), a parameter set by its index.

Every change is marked "UMLAUF" in the source. Each patch must apply exactly once, else the script stops."""
import re
import sys

D = sys.argv[1]


def edit(rel, fn):
    p = f"{D}/{rel}"
    s = open(p).read()
    t = fn(s)
    if t == s:
        sys.exit(f"vader_patch: nothing changed in {rel}")
    open(p, "w").write(t)


def once(s, old, new):
    if s.count(old) != 1:
        sys.exit(f"vader_patch: expected one {old[:60]!r}, found {s.count(old)}")
    return s.replace(old, new)


# 1. per-thread scratch buffers
TLS = re.compile(r"^([ \t]+)static (float|char|int|walk_t|ovl_shape_t) ", re.M)
for rel, n in (("src/paths.c", 7), ("src/screen.c", 1), ("src/engine.c", 4), ("src/pictures.c", 1)):
    def tls(s, n=n, rel=rel):
        t, k = TLS.subn(r"\1static _Thread_local /* UMLAUF */ \2 ", s)
        if k != n:
            sys.exit(f"vader_patch: {rel}: {k} function statics, expected {n}")
        return t
    edit(rel, tls)
edit("src/engine.c", lambda s: once(s, "static int g_scan_wav;", "static _Thread_local /* UMLAUF */ int g_scan_wav;"))


# 2. the corner bus
def corner_bus(s):
    s = once(s, "} vader_t;", "    float (*cbus)[128];   /* UMLAUF: each corner's share of the voices (dry), or NULL */\n} vader_t;")
    s = once(s, "out += corner_lim(corner_filter(v, c, &F[c], ebuf[c][i]), P[S_A_LIM + c * 3]) * wc * level[c];",
             "{ float cv_ = corner_lim(corner_filter(v, c, &F[c], ebuf[c][i]), P[S_A_LIM + c * 3]) * wc * level[c];"
             " out += cv_; if (s->cbus) s->cbus[c][i] += cv_ * v->env * amp; }   /* UMLAUF */")
    s = once(s, "out += corner_lim(x, P[S_A_LIM + c * 3]) * wc * level[c];",
             "{ float cv_ = corner_lim(x, P[S_A_LIM + c * 3]) * wc * level[c];"
             " out += cv_; if (s->cbus) s->cbus[c][i] += cv_ * v->env * amp; }   /* UMLAUF */")
    return s


edit("src/engine.c", corner_bus)

# 3. the module's doors
API = r'''

/* ---- UMLAUF (GENRA SYNTHIS, VCV Rack): what the module needs beyond the desktop plugin's doors */
void vader_set_corner_bus(void *inst, float (*bus)[128]) { ((vader_t *)inst)->cbus = bus; }
/* the shape files (wav 0) or the samples (wav 1) found by the last scan; call on the audio thread */
int vader_list_count(void *inst, int wav) { vader_t *s = inst; return wav ? s->nwavs : s->nfiles; }
const char *vader_list_path(void *inst, int wav, int i) {
    vader_t *s = inst;
    int n = wav ? s->nwavs : s->nfiles;
    return i >= 0 && i < n ? (wav ? s->wavs[i].path : s->files[i].path) : "";
}
int vader_voices_on(void *inst) {
    vader_t *s = inst;
    int n = 0;
    for (int i = 0; i < MAXV; i++) n += s->v[i].active;
    return n;
}
'''
edit("src/engine.c", lambda s: s.rstrip("\n") + API)


# 4. the user's samples folder
def samples_dir(s):
    s = once(s, "    char pdir[600];\n",
             "    char pdir[600];\n    char sdir[600];   /* UMLAUF: the samples folder the user picked (one per computer), "
             "scanned after the others */\n")
    s = once(s, "    {   /* the desktop plugin: samples shipped inside the plugin",
             "    if (s->sdir[0]) scan_dir(s, s->sdir, \"\", \"\", 0);   /* UMLAUF */\n"
             "    {   /* the desktop plugin: samples shipped inside the plugin")
    s = once(s, "        restore_state(s, val);\n        return;\n    }\n    int i = find(key);",
             "        restore_state(s, val);\n        return;\n    }\n"
             "    if (!strcmp(key, \"samples_dir\")) {   /* UMLAUF: the user's samples folder (\"\" none); \"rescan\" reads it */\n"
             "        snprintf(s->sdir, sizeof s->sdir, \"%s\", val ? val : \"\");\n        return;\n    }\n    int i = find(key);")
    return s


edit("src/engine.c", samples_dir)
# 5. (2.37) the folders scanned off the audio thread, and a parameter set by its index
def offthread(s):
    s = once(s, "static void rescan(vader_t *s) {\n    for (int i = 0; i < s->nfiles; i++)",
             "static void scan_lists(vader_t *s) {   /* UMLAUF: the scan alone (vader_scan runs it off the audio thread) */\n"
             "    for (int i = 0; i < s->nfiles; i++)")
    s = once(s, "    for (int c = 0; c < 4; c++) mark(s, P_SMP_A + c);\n    smp_sync_all(s);\n}\n",
             "    for (int c = 0; c < 4; c++) mark(s, P_SMP_A + c);\n}\n"
             "static void rescan(vader_t *s) { scan_lists(s); smp_sync_all(s); }   /* UMLAUF */\n")
    s = once(s, "static void set_param(void *p, const char *key, const char *val) {\n",
             "static void set_value(vader_t *s, int i, float v);   /* UMLAUF */\n"
             "static void set_param(void *p, const char *key, const char *val) {\n")
    s = once(s, "    int i = find(key);\n    if (i < 0) return;\n    const vp_t *q = &VP[i];\n    float v = strtof(val, NULL);\n",
             "    int i = find(key);\n    if (i < 0) return;\n    set_value(s, i, strtof(val, NULL));\n}\n"
             "/* UMLAUF: parameter i takes v (set_param's second half: vader_set_index comes in here without the text) */\n"
             "static void set_value(vader_t *s, int i, float v) {\n    const vp_t *q = &VP[i];\n")
    return s


edit("src/engine.c", offthread)

API2 = r'''

/* ---- UMLAUF 2.37: the folders are scanned off the audio thread, which only swaps the finished lists in */
typedef struct { shapefile_t files[MAXFILES], wavs[MAXFILES]; int nfiles, nwavs; char sdir[600]; } vader_lists_t;
/* any thread but the audio thread: the shape files and samples found now (sdir: the user's samples folder, "" none).
 * Reads the instance's folders only (set at create). Free with vader_lists_free, or hand to vader_lists_swap. */
void *vader_scan(void *inst, const char *sdir) {
    vader_t *s = inst, *t = calloc(1, sizeof *t);
    vader_lists_t *L = calloc(1, sizeof *L);
    if (!t || !L) { free(t); free(L); return NULL; }
    snprintf(t->dir, sizeof t->dir, "%s", s->dir);
    snprintf(t->pdir, sizeof t->pdir, "%s", s->pdir);
    snprintf(t->sdir, sizeof t->sdir, "%s", sdir ? sdir : "");
    scan_lists(t);
    memcpy(L->files, t->files, sizeof L->files);
    memcpy(L->wavs, t->wavs, sizeof L->wavs);
    L->nfiles = t->nfiles;
    L->nwavs = t->nwavs;
    snprintf(L->sdir, sizeof L->sdir, "%s", t->sdir);
    free(t);
    return L;
}
int vader_lists_count(void *lists, int wav) { vader_lists_t *L = lists; return wav ? L->nwavs : L->nfiles; }
const char *vader_lists_path(void *lists, int wav, int i) {
    vader_lists_t *L = lists;
    int n = wav ? L->nwavs : L->nfiles;
    return i >= 0 && i < n ? (wav ? L->wavs[i].path : L->files[i].path) : "";
}
/* the audio thread: the instance takes the lists, `lists` then holds the ones it had (to free off the audio thread);
 * the rest of what a rescan did (no file is read, nothing is allocated here) */
void vader_lists_swap(void *inst, void *lists) {
    vader_t *s = inst;
    vader_lists_t *L = lists;
    static _Thread_local shapefile_t tmp[MAXFILES];
    int n;
    memcpy(tmp, s->files, sizeof tmp); memcpy(s->files, L->files, sizeof tmp); memcpy(L->files, tmp, sizeof tmp);
    memcpy(tmp, s->wavs, sizeof tmp); memcpy(s->wavs, L->wavs, sizeof tmp); memcpy(L->wavs, tmp, sizeof tmp);
    n = s->nfiles; s->nfiles = L->nfiles; L->nfiles = n;
    n = s->nwavs; s->nwavs = L->nwavs; L->nwavs = n;
    snprintf(s->sdir, sizeof s->sdir, "%s", L->sdir);
    s->status[0] = 0;
    mark(s, P_FILE);
    for (int c = 0; c < 4; c++) mark(s, P_SMP_A + c);
    memset(s->uknown, 0, sizeof s->uknown);
    smp_sync_all(s);
}
void vader_lists_free(void *lists) {
    vader_lists_t *L = lists;
    if (!L) return;
    for (int i = 0; i < L->nfiles; i++) { free(L->files[i].path); free(L->files[i].label); }
    for (int i = 0; i < L->nwavs; i++) { free(L->wavs[i].path); free(L->wavs[i].label); }
    free(L);
}
/* a parameter's index (-1: none), and its value set by that index: as set_param, without the text on the way */
int vader_param_index(const char *key) { pthread_once(&g_hash_once, hash_build); return find(key); }
void vader_set_index(void *inst, int i, float v) {
    vader_t *s = inst;
    if (i < 0 || i >= NP) return;
    collect_load(s);
    set_value(s, i, v);
}
'''
edit("src/engine.c", lambda s: s.rstrip("\n") + API2)

print("vader_patch: applied")
