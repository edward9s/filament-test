#include "Protocol.h"
#include <cassert>
int main() {
    std::vector<double> values(100,10.);values.back()=100.;
    auto s=benchmark::calculate(values);assert(s.p99_ms==10 && s.low_1_fps==10);
    assert(std::abs(s.average_fps-100000./1090)<1e-10);
    values.assign(102,10.);values[100]=50.;values[101]=100.;
    assert(std::abs(benchmark::calculate(values).low_1_fps-1000./75)<1e-10);
    benchmark::Run r;r.start();assert(r.sample(100,true,{}));
    r.sample(30'000'000'099ll,true,{});assert(r.frames.empty() && r.phase==benchmark::Run::Warmup);
    r.sample(30'000'000'100ll,true,{});assert(r.frames.empty() && r.phase==benchmark::Run::Measure);
    for(int i=1;i<=600;++i)r.sample(30'000'000'100ll+i*100'000'000ll,true,{});
    assert(r.valid && r.frames.size()==600);
    r.start();r.sample(100,true,{});r.sample(100,true,{});assert(!r.valid && r.phase==benchmark::Run::Finished);
    r.start();r.sample(100,true,{});r.sample(30'000'000'100ll,true,{});r.sample(30'100'000'100ll,false,{});assert(!r.valid && r.frames.empty());
}
