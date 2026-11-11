// genre per track: generation, MIDI round trip with per-track swing, storage
#include "../src/genra/Engine.hpp"
#include <cstdio>
#include <map>
#include <tuple>
using namespace genra;
int main(){
  initGenres(); int fails=0; Rng pick(4242);
  for(int run=0; run<300; run++){
    int gGlobal=pick.range(numProfiles()); int tg[4];
    for(int t=0;t<4;t++) tg[t]=pick.range(numProfiles());
    int bars = 1<<pick.range(4);
    Params P; P.genre=gGlobal; P.scale=prof(gGlobal).scale; P.lengthBars=bars; P.key=pick.range(12); P.swingK=pick.uni()*2-1;
    Pattern pat; initPattern(pat); Rng rng(run*31+7);
    generateChords(pat,gGlobal,rng,P);
    for(int t=0;t<4;t++) generateTrack(pat,t,tg[t],rng,P);
    layoutTimeline(pat,bars*16);
    for(int t=0;t<4;t++) if(pat.trackGenre[t]!=tg[t]){ fails++; printf("trackGenre %d\n",t); }
    if(pat.chordGenre!=gGlobal){ fails++; printf("chordGenre\n"); }
    int L=bars*16;
    // evaluation must not crash and must stay in range
    for(int s=0;s<L;s++){ Hit h[NUM_DRUMS]; drumStep(pat,P,s,L,s>=L-8,h); NoteEv b=bassAt(pat,P,s,L), m=melAt(pat,P,s,L);
      if((b.on&&(b.pitch<0||b.pitch>127))||(m.on&&(m.pitch<0||m.pitch>127))){ fails++; printf("pitch range\n"); } }
    // MIDI round trip (tonal notes)
    auto a=renderLoop(pat,P,L); auto smf=writeSmf(a,120,"x",L,&pat);
    Pattern p2=pat; std::string err; int nb=importSmf(smf,p2,P,err);
    if(nb!=bars){ fails++; printf("run %d bars %d->%d\n",run,bars,nb); }
    auto b2=renderLoop(p2,P,nb*16);
    std::map<std::tuple<int,int,int>,int> ma, mb;
    for(auto&n:a) if(n.track>0) ma[std::make_tuple(n.track,n.startTick,n.note)]++;
    for(auto&n:b2) if(n.track>0) mb[std::make_tuple(n.track,n.startTick,n.note)]++;
    int diff=0; for(auto&kv:ma) if(!mb.count(kv.first)) diff++;
    if(diff){ fails++; printf("run %d tonal missing %d/%zu (genres %s/%s/%s)\n",run,diff,ma.size(),prof(tg[1]).name,prof(tg[2]).name,prof(tg[3]).name); }
    // storage keeps the per-track genres
    Pattern q; if(!loadPattern(savePattern(pat),q) || std::memcmp(&q,&pat,sizeof(Pattern))){ fails++; printf("store\n"); }
  }
  printf("tracks fails=%d\n",fails);
  return fails;
}
