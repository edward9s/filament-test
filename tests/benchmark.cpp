#include "EffectsGeometry.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>

// CPU mesh-generation timing only. This is not a rendering or phone FPS benchmark.
int main() {
    using Clock=std::chrono::steady_clock;
    constexpr int frames=1800;
    std::array<double,7> times{};
    size_t vertices=0;
    for(auto& time:times) {
        const auto start=Clock::now();
        for(int frame=0;frame<frames;++frame) {
            const auto meshes=forest::effectMeshes(frame/60.f,{3.07f,3.16f,9.54f});
            for(const auto& batch:meshes)vertices+=batch.size();
        }
        time=std::chrono::duration<double,std::micro>(Clock::now()-start).count()/frames;
    }
    std::sort(times.begin(),times.end());
    std::cout<<"Median effect generation: "<<times[times.size()/2]<<" us/frame\n"
             <<"Generated vertices across seven trials: "<<vertices<<'\n';
}
