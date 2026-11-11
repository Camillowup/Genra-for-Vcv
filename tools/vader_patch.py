#!/usr/bin/env python3
"""The few changes UMLAUF makes to the vendored VADER engine (tools/vader_vendor.sh runs it on vader/).

1. Scratch buffers: VADER keeps some work arrays in function-level statics (one plugin instance on the Force). In VCV
   Rack several UMLAUF modules render at once on different threads, so each thread gets its own (_Thread_local).
2. The corner outputs: each voice also adds what each corner contributes (after its filter, its weight and the
   envelope, before the effects) into a bus the module hands over (vader_set_corner_bus); NULL: nothing extra.
3. A few doors for the module: the corner bus, the shape and sample lists, the voices playing.
4. The samples folder the user picks (one per computer): set_param "samples_dir", scanned after VADER's own places.

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
print("vader_patch: applied")
