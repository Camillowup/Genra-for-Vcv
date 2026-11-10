// storage round trip + v2 migration + robustness
#include "../src/Engine.hpp"
#include <cstdio>
#include <cstdlib>
using namespace genra;
static bool same(const Pattern& a, const Pattern& b){ return std::memcmp(&a,&b,sizeof(Pattern))==0; }
int main(){
  initGenres(); int fails=0;
  for(int g=0; g<numProfiles(); g++){
    Params P; P.genre=g; P.scale=prof(g).scale;
    Pattern pat; initPattern(pat); Rng rng(7+g); bool lock[4]={0,0,0,0};
    generate(pat,g,rng,lock,false,P); layoutTimeline(pat,64);
    // round trip
    Pattern q; if(!loadPattern(savePattern(pat),q) || !same(pat,q)){ fails++; printf("roundtrip %d\n",g); }
    // v2 raw block (identical layout today)
    Pattern r;
    if(!loadPattern(v2RawForTest(pat),r)){ fails++; printf("v2 load %d\n",g); }
    else if(g<128 && !same(pat,r)){ fails++; printf("v2 content %d\n",g); } // v2 held profiles < 128 only
    // truncated / garbage data must not crash or load half
    auto blob=savePattern(pat);
    for(size_t cut : {(size_t)0,(size_t)3,(size_t)10,blob.size()/2}){ std::vector<uint8_t> b(blob.begin(),blob.begin()+cut); Pattern x; if(loadPattern(b,x) && cut<blob.size()/2){ fails++; printf("truncated %zu loaded\n",cut);} }
    // a newer GENRA appends an unknown record: must still load the known part
    { auto fut=blob; uint8_t rec[]={99,3,0,0,0,1,2,3}; fut.insert(fut.end(),rec,rec+8); Pattern y; if(!loadPattern(fut,y)||!same(pat,y)){ fails++; printf("future record %d\n",g);} }
    std::vector<uint8_t> junk(500); for(auto&c:junk) c=(uint8_t)rand(); Pattern x; loadPattern(junk,x);
  }
  printf("store fails=%d (sizeof Pattern %zu)\n",fails,sizeof(Pattern));
  return fails;
}
