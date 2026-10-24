#include "Pixels.h"
#include <stdexcept>
#include <iostream>

int main() {
    constexpr unsigned width=80,height=48;
    std::vector<uint8_t> pixels(width*height*4,0);
    for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
    if(hasScene(pixels,width,height))throw std::runtime_error("Opaque black passed");
    for(unsigned y=0;y<8;++y)for(unsigned x=0;x<20;++x)
        for(unsigned c=0;c<3;++c)pixels[(y*width+x)*4+c]=255;
    if(hasScene(pixels,width,height))throw std::runtime_error("Black scene with HUD passed");
    std::fill(pixels.begin(),pixels.end(),128);
    if(hasScene(pixels,width,height))throw std::runtime_error("Uniform clear color passed");
    for(unsigned y=height/4;y<height*3/4;++y)for(unsigned x=width/4;x<width*3/4;++x)
        pixels[(y*width+x)*4]=uint8_t(x*3);
    if(!hasScene(pixels,width,height))throw std::runtime_error("Visible scene rejected");
    std::cout<<"RGB check rejects opaque black, HUD-only and clear-color frames\n";
}
