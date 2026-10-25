#include "PackedVertex.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

int main() {
    using namespace forest;
    size_t count=0;
    float colorError=0,frameError=0;
    for(const auto& mesh:generate())for(const auto& v:mesh.vertices) {
        const auto packed=packVertex(v);
        if(packed.position!=v.position || packed.uv!=v.uv)
            throw std::runtime_error("Packing changed a position or UV");
        const float4 color(packed.color);
        const float4 frame=unpackSnorm16(packed.tangent);
        const float4 original{v.tangent.x,v.tangent.y,v.tangent.z,v.tangent.w};
        for(int i=0;i<4;++i) {
            colorError=std::max(colorError,std::abs(color[i]-v.color[i]));
            frameError=std::max(frameError,std::abs(frame[i]-original[i]));
        }
        if(original.w*frame.w<=0)
            throw std::runtime_error("Packing changed tangent handedness");
        ++count;
    }
    if(!count || colorError>.0005f || frameError>1.f/65534.f+1e-7f)
        throw std::runtime_error("Packing exceeded attribute precision bounds");
    Vertex bright{};bright.color={1.1f,.135f,.028f,.35f};
    if(float(packVertex(bright).color.x)<=1.f)
        throw std::runtime_error("Packing clipped a bright stone tint");
    std::cout<<"Packed "<<count<<" vertices: "<<count*sizeof(Vertex)<<" -> "
        <<count*sizeof(PackedVertex)<<" bytes; max color error="<<colorError
        <<", max frame component error="<<frameError<<'\n';
}
