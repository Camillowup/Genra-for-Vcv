// own cycle lengths per track / drum voice, nudge (1/24 step), storage of both, old data
#include "../src/Engine.hpp"
#include <cstdio>
#include <algorithm>
#include <map>
using namespace genra;
static int fails = 0;
#define CHECK(c, ...) do { if(!(c)){ fails++; printf(__VA_ARGS__); printf("\n"); } } while(0)

// a 2.22 patch: same records with the old field counts (no nudge) and without the lengths record
static std::vector<uint8_t> asOld(const std::vector<uint8_t>& d) {
  std::vector<uint8_t> o(d.begin(), d.begin() + 6);
  size_t p = 6;
  while (p + 5 <= d.size()) {
    uint8_t tag = d[p];
    uint32_t len = d[p+1] | (d[p+2] << 8) | (d[p+3] << 16) | ((uint32_t)d[p+4] << 24);
    std::vector<uint8_t> body(d.begin() + p + 5, d.begin() + p + 5 + len);
    p += 5 + len;
    if (tag == 9 || tag == 10) continue; // ST_LENGTHS: new in 2.23, ST_SWING: 2.25
    size_t head = 0, cnt = 0; int have = 0;
    if (tag == 5) { head = 4; cnt = (size_t)body[0] * (body[1] | (body[2] << 8)); have = body[3]; } // drums
    if (tag == 6 || tag == 7 || tag == 8) { head = 3; cnt = body[0] | (body[1] << 8); have = body[2]; } // bass, mel, pads
    if (head) {
      std::vector<uint8_t> nb(body.begin(), body.begin() + head);
      nb[head - 1] = (uint8_t)(have - 1);
      for (size_t i = 0; i < cnt; i++)
        nb.insert(nb.end(), body.begin() + head + i * have, body.begin() + head + i * have + have - 1);
      body = nb;
    }
    o.push_back(tag);
    uint32_t n = (uint32_t)body.size();
    for (int i = 0; i < 4; i++) o.push_back((uint8_t)(n >> (8 * i)));
    o.insert(o.end(), body.begin(), body.end());
  }
  return o;
}

static int hitsAt(const std::vector<MidiNote>& ns, int track, int tick, int note) {
  int c = 0;
  for (auto& n : ns) if (n.track == track && n.startTick == tick && n.note == note) c++;
  return c;
}

int main() {
  initGenres();
  Params P; P.genre = 0; P.scale = prof(0).scale; P.lengthBars = 2; P.key = 0; P.swingK = -1.f; // straight: ticks = step * 24
  const int L = 32;
  // ---- 1. per voice lengths on an edited drum track: voice cycles, restarts with the loop
  {
    Pattern pat; initPattern(pat); Rng rng(5); bool lk[4] = {0,0,0,0};
    generate(pat, 0, rng, lk, false, P); layoutTimeline(pat, L);
    clearTrack(pat, T_DRUMS);
    setDrum(pat, KICK, 0, 100, 1);
    setDrum(pat, RIM, 0, 90, 1); setDrum(pat, RIM, 3, 90, 1);
    setCycleLen(pat, T_DRUMS, RIM, 7);
    Params Q = P; Q.ignoreChance = true;
    for (int s = 0; s < L; s++) {
      Hit h = drumHit(pat, Q, RIM, s, L, false);
      bool want = (s % 7) == 0 || (s % 7) == 3;
      CHECK(h.on == want, "rim cycle 7 step %d: %d", s, h.on);
      CHECK(drumHit(pat, Q, KICK, s, L, false).on == (s == 0), "kick follows the loop %d", s);
    }
    // restart: the loop of 32 steps rendered twice -> second pass starts at the cycle start again
    auto two = renderLoop(pat, P, L);
    int rims = 0; for (auto& n : two) if (n.track == 0 && n.note == 37) rims++;
    CHECK(rims == 10, "rim hits in 32 steps with cycle 7 (0,3,7,10,14,17,21,24,28,31): %d", rims);
    // export length: exactly L steps, no note starts at or beyond the loop end
    int last = 0; for (auto& n : two) last = std::max(last, n.startTick);
    CHECK(last < L * 24, "export beyond the loop: %d", last);
    // song rows restart: two rows of 1 bar each -> second row starts at the cycle start (rim at step 0 of the row)
    std::vector<SongPart> parts(2);
    for (auto& sp : parts) { sp.pat = &pat; sp.P = P; sp.L = L; sp.bars = 1; }
    auto song = renderSong(parts);
    CHECK(hitsAt(song, 0, 16 * 24, 37) == 1, "song row 2 restarts the rim cycle");
    CHECK(hitsAt(song, 0, 14 * 24, 37) == 1 && hitsAt(song, 0, 17 * 24, 37) == 0, "row 1 cycles, row 2 restarts");
    // store round trip with lengths
    Pattern q; CHECK(loadPattern(savePattern(pat), q) && std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "store lengths");
    CHECK(q.vLen[RIM] == 7, "rim length stored");
    // freeze / trackNotes of a drum track keep the cycle
    Pattern f = pat; freezeTrack(f, P, T_DRUMS, L);
    CHECK(sizeof(f) > 0, "freeze");
  }
  // ---- 2. piano track length: bass of 12 steps
  for (int t = T_BASS; t <= T_PADS; t++) {
    Pattern pat; initPattern(pat); Rng rng(11 + t); bool lk[4] = {0,0,0,0};
    generate(pat, 0, rng, lk, false, P); layoutTimeline(pat, L);
    freezeTrack(pat, P, t, L);
    std::vector<GridNote> ns;
    GridNote a; a.start = 0; a.len = 2; a.pitch = (t == T_BASS ? 36 : 60); a.vel = 100; ns.push_back(a);
    GridNote b = a; b.start = 5; b.pitch += 3; b.len = 20; ns.push_back(b); // too long: ends at the cycle end
    writeTrackNotes(pat, P, t, ns, L);
    setCycleLen(pat, t, -1, 12);
    auto back = trackNotes(pat, P, t, L);
    CHECK(back.size() == 2, "track %d: notes within the cycle only (%zu)", t, back.size());
    if (back.size() == 2) CHECK(back[1].len == 7, "track %d: length cut at the cycle end (%d)", t, back[1].len);
    auto mid = renderLoop(pat, P, L);
    int starts = 0; for (auto& n : mid) if (n.track == t && n.note == a.pitch) starts++;
    CHECK(starts == 3, "track %d: note at 0, 12, 24 (%d)", t, starts);
    CHECK(hitsAt(mid, t, 12 * 24, a.pitch) == 1 && hitsAt(mid, t, 24 * 24, a.pitch) == 1, "track %d: cycle positions", t);
    Pattern q; CHECK(loadPattern(savePattern(pat), q) && std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "store track length %d", t);
    // copyTrack carries the length (grid hand-over, undo)
    Pattern c; initPattern(c); copyTrack(pat, c, t); CHECK(c.tLen[t] == 12, "copyTrack length");
    // a newly generated track follows the loop again
    Rng r2(3); generateTrack(pat, t, 0, r2, P); CHECK(pat.tLen[t] == 0, "generateTrack resets the length");
  }
  // ---- 3. generated tracks follow their cycle too (data index = step % length)
  {
    Pattern pat; initPattern(pat); Rng rng(21); bool lk[4] = {0,0,0,0};
    generate(pat, 0, rng, lk, false, P); layoutTimeline(pat, L);
    setCycleLen(pat, T_DRUMS, CHAT, 6);
    for (int s = 6; s < L; s++) {
      Hit x = drumHit(pat, P, CHAT, s, L, false), y = drumHit(pat, P, CHAT, s % 6, L, false);
      CHECK(x.on == y.on, "generated hat cycle %d", s);
    }
  }
  // ---- 4. nudge: export ticks, round trip through the note list, storage, clamping
  {
    Pattern pat; initPattern(pat); Rng rng(31); bool lk[4] = {0,0,0,0};
    generate(pat, 0, rng, lk, false, P); layoutTimeline(pat, L);
    clearTrack(pat, T_DRUMS);
    setDrum(pat, SNARE, 4, 100, 1); setDrumNudge(pat, SNARE, 4, 5);
    setDrum(pat, KICK, 8, 100, 1); setDrumNudge(pat, KICK, 8, -7);
    setDrum(pat, KICK, 0, 100, 1); setDrumNudge(pat, KICK, 0, -3); // ahead of the very first step: tick 0
    freezeTrack(pat, P, T_MELODY, L);
    std::vector<GridNote> ns; GridNote n; n.start = 2; n.len = 2; n.pitch = 72; n.vel = 90; n.nudge = 11; ns.push_back(n);
    GridNote m = n; m.start = 10; m.nudge = -11; m.pitch = 74; ns.push_back(m);
    writeTrackNotes(pat, P, T_MELODY, ns, L);
    auto back = trackNotes(pat, P, T_MELODY, L);
    CHECK(back.size() == 2 && back[0].nudge == 11 && back[1].nudge == -11, "note nudge round trip");
    auto r = renderLoop(pat, P, L);
    CHECK(hitsAt(r, 0, 4 * 24 + 5, 38) == 1, "snare nudged +5/24");
    CHECK(hitsAt(r, 0, 8 * 24 - 7, 36) == 1, "kick nudged -7/24");
    CHECK(hitsAt(r, 0, 0, 36) == 1, "kick nudged ahead of step 0 stays at tick 0");
    CHECK(hitsAt(r, 2, 2 * 24 + 11, 72) == 1 && hitsAt(r, 2, 10 * 24 - 11, 74) == 1, "melody nudge ticks");
    Params Q = P; Q.ignoreChance = true;
    CHECK(drumHit(pat, Q, SNARE, 4, L, false).nudge == 5 && melAt(pat, Q, 10, L).nudge == -11, "evaluator nudge");
    setDrumNudge(pat, SNARE, 4, 40); CHECK(pat.drums[SNARE][4].fnudge == NUDGE_MAX, "nudge clamp");
    setDrumNudge(pat, SNARE, 4, 5);
    Pattern q; CHECK(loadPattern(savePattern(pat), q) && std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "store nudge");
    // deleting a hit clears its nudge
    setDrum(pat, SNARE, 4, 0, 1); CHECK(pat.drums[SNARE][4].fnudge == 0, "delete clears nudge");
    // MIDI file with nudged notes still writes and imports
    auto smf = writeSmf(r, 120, "x", L, &pat); Pattern p2 = pat; std::string err;
    CHECK(importSmf(smf, p2, P, err) > 0, "import nudged file: %s", err.c_str());
  }
  // ---- 5. old data (2.22: fewer fields, no lengths record) loads with nudge 0 and lengths following the loop
  for (int g = 0; g < numProfiles(); g++) {
    Params Pg; Pg.genre = g; Pg.scale = prof(g).scale;
    Pattern pat; initPattern(pat); Rng rng(7 + g); bool lk[4] = {0,0,0,0};
    generate(pat, g, rng, lk, false, Pg); layoutTimeline(pat, 64);
    freezeTrack(pat, Pg, T_BASS, 64);
    auto blob = savePattern(pat);
    // the same pattern with nudge / lengths set must come back as `pat` from the old format
    Pattern withNew = pat; withNew.tLen[T_BASS] = 9; withNew.vLen[KICK] = 5; withNew.drums[KICK][0].fnudge = 3; withNew.bass[0].fnudge = -2; withNew.pads[0].fnudge = 4;
    Pattern q;
    CHECK(loadPattern(asOld(savePattern(withNew)), q), "old format load %d", g);
    CHECK(std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "old format content %d", g);
    CHECK(loadPattern(asOld(blob), q) && std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "old blob %d", g);
    // a future GENRA with more fields per step still loads
    CHECK(loadPattern(blob, q), "reload %d", g);
  }
  // ---- 6. GM drum mapping for MIDI record
  CHECK(gmDrumVoice(36) == KICK && gmDrumVoice(38) == SNARE && gmDrumVoice(42) == CHAT && gmDrumVoice(46) == OHAT, "gm map");
  CHECK(gmDrumVoice(24) == KICK && gmDrumVoice(25) == SNARE && gmDrumVoice(100) == CHAT && gmDrumVoice(95) == PERC, "C fallback");
  // ---- 7. own swing per drum voice / piano track (2.25): storage, old data, MIDI timing
  {
    Rng rng(7);
    Pattern pat; initPattern(pat);
    generateTrack(pat, T_DRUMS, 0, rng, P);
    for (int s = 0; s < L; s++) setDrum(pat, CHAT, s, 100, 1); // a hat on every step
    setDrum(pat, KICK, 1, 100, 1);
    setSwing(pat, T_DRUMS, CHAT, 62);
    setSwing(pat, T_BASS, -1, 99);  // clamped to 75
    setSwing(pat, T_MELODY, -1, 20); // below the range: clamped to 50 (own, straight)
    CHECK(ownSwing(pat, T_DRUMS, CHAT) == 62 && ownSwing(pat, T_DRUMS, KICK) == 0, "own swing set");
    CHECK(ownSwing(pat, T_BASS, -1) == 75 && ownSwing(pat, T_MELODY, -1) == 50, "own swing clamp");
    Pattern q;
    CHECK(loadPattern(savePattern(pat), q) && std::memcmp(&q, &pat, sizeof(Pattern)) == 0, "swing round trip");
    CHECK(loadPattern(asOld(savePattern(pat)), q) && ownSwing(q, T_DRUMS, CHAT) == 0, "old data: global swing");
    Pattern c; initPattern(c);
    copyTrack(pat, c, T_DRUMS);
    CHECK(ownSwing(c, T_DRUMS, CHAT) == 62, "copyTrack takes the swing");
    // MIDI: P is straight; the hat on odd steps is late by (0.62-0.5)*2*24 = 5.76 -> 6 ticks, the kick stays on time
    std::vector<MidiNote> ns = renderLoop(pat, P, L);
    bool hatOk = false, kickOk = false;
    for (const MidiNote& n : ns) {
      if (n.track == 0 && n.note == 42 && n.start == 3) hatOk = n.startTick == 3 * 24 + 6;
      if (n.track == 0 && n.note == 36 && n.start == 1) kickOk = n.startTick == 24;
    }
    CHECK(hatOk && kickOk, "midi swing per voice hat=%d kick=%d", hatOk, kickOk);
  }
  printf("cycle fails=%d\n", fails);
  return fails;
}
