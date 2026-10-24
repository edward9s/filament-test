#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// Inspect the center of the scene, excluding every corner where a HUD could appear.
// Alpha alone, a clear color, or an isolated HUD must never pass the rendering test.
inline bool hasScene(const std::vector<uint8_t>& pixels, unsigned width, unsigned height) {
    if(width<4 || height<4 || pixels.size()!=size_t(width)*height*4) return false;
    unsigned lit=0, total=0, low=255, high=0;
    for(unsigned y=height/4;y<height*3/4;++y) {
        for(unsigned x=width/4;x<width*3/4;++x) {
            const auto i=(size_t(y)*width+x)*4;
            const unsigned value=std::max({pixels[i],pixels[i+1],pixels[i+2]});
            low=std::min(low,value);high=std::max(high,value);
            lit+=value>8;++total;
        }
    }
    return lit>total/100 && high-low>12;
}
