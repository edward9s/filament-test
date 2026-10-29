#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace benchmark {
constexpr const char* presets[]={"standard","gpu","baseline","no_shadows","no_aa","no_bloom","scale_65","no_effects"};
struct Statistics {
    size_t sample_count{};
    double duration_ms{},average_fps{},low_1_fps{},mean_ms{},p50_ms{},p95_ms{},p99_ms{},max_ms{};
};
inline Statistics calculate(std::vector<double> intervals) {
    if(intervals.empty())throw std::invalid_argument("No intervals");
    for(auto v:intervals)if(!std::isfinite(v)||v<=0)throw std::invalid_argument("Invalid interval");
    std::sort(intervals.begin(),intervals.end());
    Statistics s;s.sample_count=intervals.size();s.duration_ms=std::accumulate(intervals.begin(),intervals.end(),0.);
    s.mean_ms=s.duration_ms/s.sample_count;s.average_fps=1000/s.mean_ms;
    auto rank=[&](double p){return intervals[size_t(std::ceil(intervals.size()*p))-1];};
    s.p50_ms=rank(.5);s.p95_ms=rank(.95);s.p99_ms=rank(.99);s.max_ms=intervals.back();
    const auto tail=size_t(std::ceil(intervals.size()*.01));
    s.low_1_fps=1000*tail/std::accumulate(intervals.end()-tail,intervals.end(),0.);
    return s;
}
struct Frame {double elapsed,interval,animation,scene,effects,submit,wait;};
struct Metric {uint32_t sample,observed;const char* name;double ms;};
struct Run {
    enum Phase {Idle,Waiting,Warmup,Measure,Finished};
    Phase phase=Idle;
    int64_t epoch=0,previous=0,anchor=0;
    bool valid=false;
    std::string reason;
    std::vector<Frame> frames;
    std::vector<Metric> metrics;
    void start() {phase=Waiting;epoch=previous=anchor=0;valid=false;reason.clear();frames.clear();metrics.clear();frames.reserve(36000);metrics.reserve(180000);}
    void stop(std::string message) {valid=false;reason=std::move(message);phase=Finished;}
    bool sample(int64_t now,bool ready,Frame frame) {
        if(phase==Waiting) {
            if(ready) {epoch=previous=now;phase=Warmup;return true;}
        } else if(phase==Warmup || phase==Measure) {
            if(now<=previous) {stop("nonmonotonic application clock");return false;}
            if(!ready) {if(phase==Measure)stop("initialization changed during measurement");else phase=Waiting;return false;}
            if(phase==Warmup) {previous=now;if(now-epoch>=30'000'000'000ll){anchor=now;phase=Measure;}}
            else {
                if(frames.size()==36000){stop("frame capacity exceeded");return false;}
                frame.elapsed=double(now-anchor)*1e-6;frame.interval=double(now-previous)*1e-6;
                frames.push_back(frame);previous=now;
                if(now-anchor>=60'000'000'000ll) {valid=true;phase=Finished;}
            }
        }
        return false;
    }
};
}
