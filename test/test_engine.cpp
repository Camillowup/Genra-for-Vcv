#include "../src/Engine.hpp"
#include <cstdio>
#include <string>
#include <cstdlib>
using namespace genra;
static std::string nn(int p){ char b[16]; snprintf(b,16,"%s%d",NOTE_NAMES[p%12],p/12-1); return b; }
int main(int argc, char** argv){
  initGenres();
  int only = argc>1 ? atoi(argv[1]) : -1;
  printf("profiles: %d\n", numProfiles());
  for(int pi=0; pi<numProfiles(); pi++){
    if(only>=0 && pi!=only) continue;
    const Genre& G = prof(pi);
    Params P; P.genre=pi; P.scale=G.scale; P.lengthBars=4; P.key=9; P.fill=0.5f; P.fillActive=false;
    Pattern pat; initPattern(pat);
    Rng rng(1234+pi*77);
    bool lock[4]={0,0,0,0};
    generate(pat,pi,rng,lock,false,P);
    int L=64;
    printf("\n=== %s / %s (drumLen %d, harm %d) ===\nChords: ", G.name, G.variant, G.drumLen, pat.harm);
    for(int e=0;e<pat.nEvents;e++){ Chord c=resolveEvent(pat,P,e); printf("[%d:%s] ", pat.ev[e].start, chordName(c,P.key).c_str()); }
    printf("\n");
    for(int v=0; v<NUM_DRUMS; v++){
      printf("%-10s ", DRUM_NAMES[v]);
      for(int s=0;s<32;s++){ Hit h[NUM_DRUMS]; drumStep(pat,P,s,L,false,h); putchar(h[v].on ? (h[v].vel<0.45f?'o':(h[v].ratchet>1?'R':'X')) : (s%4==0?'|':'.')); }
      printf("\n");
    }
    printf("Bass   : "); for(int s=0;s<32;s++){ NoteEv n=bassAt(pat,P,s,L); if(n.on) printf("%s%s ", nn(n.pitch).c_str(), n.slide?"~":""); } printf("\n");
    printf("Melody : "); for(int s=0;s<32;s++){ NoteEv n=melAt(pat,P,s,L); if(n.on) printf("%d:%s%s ", s, nn(n.pitch).c_str(), n.slide?"~":""); } printf("\n");
    int prev[4]; bool hp=false; int npads=0;
    for(int s=0;s<64;s++){ PadEv p=padAt(pat,P,s,L,prev,hp); if(p.on) npads++; } printf("Pads: %d hits\n", npads);
    // fill + drops at end of loop
    printf("Fill   : "); for(int s=48;s<64;s++){ Hit h[NUM_DRUMS]; drumStep(pat,P,s,L,true,h); int c=0; for(int v=0;v<8;v++) c+=h[v].on; printf("%d", c); } printf("\n");
    auto notes=renderLoop(pat,P,L); auto smf=writeSmf(notes, G.bpm, G.name);
    Pattern p2=pat; std::string err; int bars=importSmf(smf,p2,P,err);
    printf("MIDI: %zu notes, reimport bars=%d %s\n", notes.size(), bars, err.c_str());
    for(int t=0;t<4;t++) generateTrack(pat,t,pi,rng,P);
    for(int i=0;i<10;i++) mutate(pat,0.8f,rng,lock,false,P);
  }
}
