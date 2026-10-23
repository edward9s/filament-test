#include "FpsCounter.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message) { if(!ok) throw std::runtime_error(message); }
int main() {
    forest::FpsCounter counter;
    require(counter.value()<0,"Warm-up must not invent an FPS value");
    for(int i=0;i<=60;++i) counter.record(i/60.0);
    require(std::abs(counter.value()-60)<.001f,"60 submitted frames/sec");
    counter.reset();
    counter.record(10);
    for(int i=1;i<=15;++i)counter.record(10+i/60.0);
    counter.record(10.5); // The missing frames are not submitted or counted.
    require(std::abs(counter.value()-32)<.001f,"Skipped frames must reduce FPS");
    counter.reset();
    require(counter.value()<0,"Resume must clear the old value");
    counter.record(1000);
    for(int i=1;i<=30;++i)counter.record(1000+i/60.0);
    require(std::abs(counter.value()-60)<.001f,"Background time must not affect resumed FPS");
    bool threw=false;
    try { counter.record(0); } catch(const std::invalid_argument&) { threw=true; }
    require(threw,"Reject a backwards clock");
    std::cout<<"FPS: stable rate, skipped frames, resume and clock validation passed\n";
}
