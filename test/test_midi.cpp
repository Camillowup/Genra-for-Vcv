#include "../src/Engine.hpp"
#include <cstdio>
#include <map>
#include <algorithm>
using namespace genra;
int main(){
  initGenres(); int fails=0;
  for(int g=0; g<numProfiles(); g++){
    for(int bars : {1,2,4,8}){
      Params P; P.genre=g; P.scale=prof(g).scale; P.lengthBars=bars; P.key=(g*5)%12; 
      Pattern pat; initPattern(pat); Rng rng(99+g*13+bars); bool lock[4]={0,0,0,0};
      generate(pat,g,rng,lock,false,P); int L=bars*16;
      auto a=renderLoop(pat,P,L); auto smf=writeSmf(a,120,"x");
      Pattern p2=pat; std::string err; int nb=importSmf(smf,p2,P,err);
      auto b=renderLoop(p2,P,nb*16);
      // compare tonal notes (track 1..3) as multiset of (track,startTick,note)
      std::map<std::tuple<int,int,int>,int> ma, mb;
      for(auto&n:a) if(n.track>0) ma[std::make_tuple(n.track,n.startTick,n.note)]++;
      for(auto&n:b) if(n.track>0) mb[std::make_tuple(n.track,n.startTick,n.note)]++;
      int diff=0; for(auto&kv:ma) if(!mb.count(kv.first)) diff++;
      // chords
      std::string ca, cb;
      for(int s=0;s<L;s+=16){ ca+=chordName(chordAt(pat,P,s),P.key)+" "; cb+=chordName(chordAt(p2,P,s),P.key)+" "; }
      bool barsOk = (nb==bars) || (nb<bars); // trailing empty bars may shrink
      if(diff>0 || !barsOk || ca!=cb){ fails++; printf("%-12s bars=%d->%d tonal missing=%d/%zu\n   orig: %s\n   imp : %s\n", prof(g).name, bars, nb, diff, ma.size(), ca.c_str(), cb.c_str()); }
    }
  }
  printf("fails=%d\n",fails);
}
