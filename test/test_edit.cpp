// grid editing: freeze sounds identical, note round trip, chord follow, snap, drums, storage, copy/paste
#include "../src/genra/Engine.hpp"
#include <cstdio>
#include <algorithm>
using namespace genra;
static int fails = 0;
#define CHECK(c, ...) do { if(!(c)){ fails++; printf(__VA_ARGS__); printf("\n"); } } while(0)

static std::vector<MidiNote> trackOf(const std::vector<MidiNote>& a, int t){ std::vector<MidiNote> r; for(auto&n:a) if(n.track==t) r.push_back(n);
  std::sort(r.begin(),r.end(),[](const MidiNote&x,const MidiNote&y){ return x.startTick!=y.startTick? x.startTick<y.startTick : x.note<y.note; }); return r; }
static bool sameNotes(const std::vector<MidiNote>& a, const std::vector<MidiNote>& b){
  if(a.size()!=b.size()) return false;
  for(size_t i=0;i<a.size();i++) if(a[i].startTick!=b[i].startTick||a[i].note!=b[i].note||a[i].vel!=b[i].vel||std::abs(a[i].len-b[i].len)>1) return false;
  return true; }
static bool inScale(const Params& P, int pitch){ const int* sc=SCALES[P.scale].iv; int pc=((pitch-P.key-P.transp)%12+12)%12; for(int i=0;i<7;i++) if(sc[i]==pc) return true; return false; }

int main(){
  initGenres(); Rng pick(99);
  for(int g=0; g<numProfiles(); g++){
    for(int bars : {1,2,4}){
      Params P; P.genre=g; P.scale=prof(g).scale; P.lengthBars=bars; P.key=pick.range(12); P.swingK=pick.uni()*2-1;
      P.oct[0]=pick.range(3)-1; P.oct[1]=pick.range(3)-1; P.oct[2]=pick.range(3)-1;
      Pattern pat; initPattern(pat); Rng rng(g*7+bars); bool lk[4]={0,0,0,0};
      generate(pat,g,rng,lk,false,P); int L=bars*16; layoutTimeline(pat,L);
      auto before=renderLoop(pat,P,L);
      // 1. freezing does not change the sound
      for(int t=0;t<4;t++){
        Pattern f=pat; freezeTrack(f,P,t,L);
        CHECK(f.fixed[t]==1, "freeze flag");
        auto after=renderLoop(f,P,L);
        CHECK(sameNotes(trackOf(before,t),trackOf(after,t)), "freeze changes sound: %s/%s track %d bars %d (%zu vs %zu)", prof(g).name, prof(g).variant, t, bars, trackOf(before,t).size(), trackOf(after,t).size());
        // longer loop after freezing: the loop repeats
        if(bars<4){ auto twice=renderLoop(f,P,L*2); CHECK(trackOf(twice,t).size()==2*trackOf(after,t).size(), "tile %d", t); }
      }
      // 2. note list round trip
      for(int t=1;t<4;t++){
        Pattern f=pat; freezeTrack(f,P,t,L);
        auto notes=trackNotes(f,P,t,L);
        writeTrackNotes(f,P,t,notes,L);
        auto again=trackNotes(f,P,t,L);
        bool ok=notes.size()==again.size();
        for(size_t i=0; ok && i<notes.size(); i++) ok = notes[i].start==again[i].start && notes[i].pitch==again[i].pitch && notes[i].len==again[i].len && notes[i].vel==again[i].vel && notes[i].slide==again[i].slide && notes[i].acc==again[i].acc;
        CHECK(ok, "note round trip %s track %d (%zu/%zu)", prof(g).name, t, notes.size(), again.size());
      }
      // 3. chord follow: new chords -> chord roots stay roots, scale notes stay in the scale
      {
        Pattern f=pat; int t=T_BASS;
        std::vector<GridNote> ns; for(int s=0;s<L;s+=4){ GridNote n; n.start=s; n.len=2; Chord c=chordAt(f,P,s); n.pitch=36+P.key+P.transp+12*P.oct[0]+c.root; if(s%8==4) n.pitch=snapPitch(f,P,s,n.pitch+2,1); ns.push_back(n);} 
        writeTrackNotes(f,P,t,ns,L); setChordFollow(f,P,t,true,L);
        Rng r2(g+1000); P.templateSel=-1; generateChords(f,g,r2,P); layoutTimeline(f,L);
        for(int s=0;s<L;s+=8){ NoteEv e=bassAt(f,P,s,L); Chord c=chordAt(f,P,s);
          CHECK(e.on && (((e.pitch-P.key-P.transp-c.root)%12+12)%12)==0, "follow root %s step %d", prof(g).name, s); }
        for(int s=4;s<L;s+=8){ NoteEv e=bassAt(f,P,s,L); Chord c=chordAt(f,P,s); bool diat=true; const int* sc=SCALES[P.scale].iv; bool rootIn=false; for(int i=0;i<7;i++) rootIn|= sc[i]==c.root; diat=rootIn;
          if(diat) CHECK(e.on && inScale(P,e.pitch), "follow scale %s step %d pitch %d", prof(g).name, s, e.pitch); }
        // follow off keeps the notes where they sound now
        auto sound=trackNotes(f,P,t,L); setChordFollow(f,P,t,false,L); auto kept=trackNotes(f,P,t,L);
        bool same=sound.size()==kept.size(); for(size_t i=0;same&&i<sound.size();i++) same=sound[i].pitch==kept[i].pitch;
        CHECK(same, "follow off moved notes %s", prof(g).name);
      }
    }
  }
  // 4. snap
  { Params P; P.key=2; P.scale=1; Pattern pat; initPattern(pat); layoutTimeline(pat,64);
    for(int p=30;p<90;p++){ int q=snapPitch(pat,P,0,p,1); CHECK(inScale(P,q) && std::abs(q-p)<=1, "snap scale %d->%d",p,q);
      int c=snapPitch(pat,P,0,p,2); Chord ch=chordAt(pat,P,0); bool tone=false; for(int i=0;i<ch.n;i++) tone|= (((c-P.key-ch.root-ch.iv[i])%12+12)%12)==0; CHECK(tone && std::abs(c-p)<=3, "snap chord %d->%d",p,c); } }
  // 5. drums: hits, rolls, storage, copy/paste
  { Params P; Pattern pat; initPattern(pat); Rng rng(5); bool lk[4]={0,0,0,0}; generate(pat,23,rng,lk,false,P);
    freezeTrack(pat,P,T_DRUMS,64); clearTrack(pat,T_DRUMS);
    setDrum(pat,KICK,0,127,1); setDrum(pat,CHAT,3,90,3); setDrum(pat,SNARE,4,40,1);
    Hit h=drumHit(pat,P,CHAT,3,64,false); CHECK(h.on && h.ratchet==3, "roll");
    h=drumHit(pat,P,SNARE,4,64,false); CHECK(h.on && std::abs(h.vel-40/127.f)<0.01f, "ghost vel");
    CHECK(!drumHit(pat,P,KICK,1,64,false).on, "empty");
    BarClip clip; copyBar(pat,T_DRUMS,0,clip); CHECK(pasteBar(pat,T_DRUMS,2,clip), "paste");
    CHECK(drumHit(pat,P,CHAT,35,64,false).ratchet==3, "pasted roll");
    // edited notes with slide/accent survive storage
    std::vector<GridNote> ns(2); ns[0].start=0; ns[0].len=2; ns[0].pitch=40; ns[0].slide=true; ns[1].start=4; ns[1].pitch=43; ns[1].acc=true;
    writeTrackNotes(pat,P,T_BASS,ns,64); setChordFollow(pat,P,T_BASS,true,64);
    Pattern q; CHECK(loadPattern(savePattern(pat),q) && std::memcmp(&q,&pat,sizeof(Pattern))==0, "store edited");
    NoteEv e=bassAt(q,P,0,64); CHECK(e.on && e.slide && e.len==4, "slide len %d", e.len);
    e=bassAt(q,P,4,64); CHECK(e.on && e.acc, "accent");
    // copy one track into another pattern (the editor's hand-off)
    Pattern other; initPattern(other); generate(other,5,rng,lk,false,P); Pattern keep=other;
    copyTrack(pat,other,T_BASS);
    CHECK(other.fixed[T_BASS] && std::memcmp(other.bass,pat.bass,sizeof(pat.bass))==0 && std::memcmp(other.drums,keep.drums,sizeof(keep.drums))==0, "copyTrack");
  }
  // 6. chance and "every 2nd pass" on edited notes
  { Params P; Pattern pat; initPattern(pat); Rng rng(9); bool lk[4]={0,0,0,0}; generate(pat,5,rng,lk,false,P); int L=64;
    std::vector<GridNote> ns(3); ns[0].start=0; ns[0].pitch=40; ns[0].chance=50; ns[1].start=8; ns[1].pitch=43; ns[1].cond=NC_EVERY2; ns[2].start=16; ns[2].pitch=45;
    writeTrackNotes(pat,P,T_BASS,ns,L);
    freezeTrack(pat,P,T_DRUMS,L); clearTrack(pat,T_DRUMS); setDrum(pat,SNARE,4,100,1); setDrumChance(pat,SNARE,4,25,0);
    int a=0,b=0,c=0,d=0,N=400;
    for(int pass=0; pass<N; pass++){ P.pass=pass; a+=bassAt(pat,P,0,L).on; b+=bassAt(pat,P,8,L).on; c+=bassAt(pat,P,16,L).on; d+=drumHit(pat,P,SNARE,4,L,false).on;
      CHECK(bassAt(pat,P,8,L).on == (pass%2==0), "every2 pass %d", pass); }
    CHECK(a>N*0.4 && a<N*0.6, "chance 50: %d/%d", a, N);
    CHECK(c==N, "always: %d", c);
    CHECK(d>N*0.15 && d<N*0.35, "drum chance 25: %d/%d", d, N);
    P.pass=1; auto shown=trackNotes(pat,P,T_BASS,L);
    CHECK(shown.size()==3 && shown[0].chance==50 && shown[1].cond==NC_EVERY2 && shown[2].chance==100, "editor shows all notes with chance (%zu)", shown.size());
    P.ignoreChance=true; CHECK(drumHit(pat,P,SNARE,4,L,false).on, "editor shows drum hit"); P.ignoreChance=false;
    writeTrackNotes(pat,P,T_BASS,shown,L); auto again=trackNotes(pat,P,T_BASS,L);
    CHECK(again.size()==3 && again[0].chance==50 && again[1].cond==NC_EVERY2, "chance round trip");
    Pattern q; CHECK(loadPattern(savePattern(pat),q) && std::memcmp(&q,&pat,sizeof(Pattern))==0, "store chance");
    // pads: one chance per chord onset
    std::vector<GridNote> pn(3); for(int i=0;i<3;i++){ pn[i].start=0; pn[i].pitch=60+i*4; pn[i].len=8; pn[i].chance= i==1? 30 : 60; }
    writeTrackNotes(pat,P,T_PADS,pn,L); CHECK(pat.pads[0].fchance==60, "pad chance %d", pat.pads[0].fchance);
  }
  printf("edit fails=%d\n",fails);
  return fails;
}
