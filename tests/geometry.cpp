#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <array>
#include <bit>

void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main() {
    // Identical records may merge; UV, tangent and color seams must survive.
    forest::Mesh sample{forest::Surface::Stone,{}, {0,1,2,3,4,5}};
    forest::Vertex vertex{{0,2,3},{0,0,0,1},{.25f,.5f},{1,1,1,1}};
    sample.vertices.assign(6,vertex);
    sample.vertices[2].uv.x=.75f;
    sample.vertices[3].tangent.x=.5f;
    sample.vertices[4].color.x=.5f;
    sample.vertices[5].position.x=-0.f;
    const auto before=sample;
    forest::compact(sample);
    require(sample.vertices.size()==5,"Vertex seams were merged or duplicates retained");
    for(size_t i=0;i<sample.indices.size();++i)
        require(std::bit_cast<std::array<uint32_t,13>>(sample.vertices[sample.indices[i]])==
                std::bit_cast<std::array<uint32_t,13>>(before.vertices[before.indices[i]]),
                "Compaction changed a triangle corner");
    const auto compacted=sample;
    forest::compact(sample);
    require(sample.indices==compacted.indices && sample.vertices.size()==compacted.vertices.size(),"Compaction is not idempotent");
    bool rejected=false;
    sample.indices.push_back(999);
    try{forest::compact(sample);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Invalid vertex index accepted");
    const auto meshes=forest::generate();
    const auto repeated=forest::generate();
    require(meshes.size()==repeated.size(),"Non-deterministic batching");
    std::array<size_t,5> counts{};size_t triangles=0,bytes=0;
    for(size_t k=0;k<meshes.size();++k) {
        const auto& mesh=meshes[k];const auto& second=repeated[k];
        require(!mesh.vertices.empty() && mesh.indices.size()%3==0,"Empty or malformed mesh");
        require(mesh.vertices.size()<=65536,"Batch exceeds 16-bit vertex limit");
        require(mesh.vertices.size()==second.vertices.size() && mesh.indices==second.indices,"Non-deterministic topology");
        for(size_t j=0;j<mesh.vertices.size();++j) {
            const auto& v=mesh.vertices[j];const auto& w=second.vertices[j];
            require(v.position==w.position && v.tangent==w.tangent && v.uv==w.uv && v.color==w.color,"Non-deterministic vertex");
            for(int a=0;a<3;++a)require(std::isfinite(v.position[a]),"Invalid position");
            for(int a=0;a<4;++a)require(std::isfinite(v.tangent[a]),"Invalid tangent");
            require(std::abs(length(v.tangent)-1)<.001f,"Tangent frame is not normalized");
            require(std::isfinite(v.uv.x) && std::isfinite(v.uv.y),"Invalid UV");
        }
        for(size_t j=0;j<mesh.indices.size();j+=3) {
            for(int a=0;a<3;++a)require(mesh.indices[j+a]<mesh.vertices.size(),"Index outside mesh");
            auto a=mesh.vertices[mesh.indices[j]].position,b=mesh.vertices[mesh.indices[j+1]].position,c=mesh.vertices[mesh.indices[j+2]].position;
            auto n=cross(b-a,c-a);
            require(length(n)>1e-9f,"Degenerate triangle");
            if(mesh.surface==forest::Surface::Ground)require(n.y>0,"Terrain faces downward");
        }
        counts[size_t(mesh.surface)]+=mesh.indices.size()/3;
        triangles+=mesh.indices.size()/3;bytes+=mesh.vertices.size()*sizeof(forest::Vertex)+mesh.indices.size()*sizeof(uint16_t);
    }
    for(auto count:counts)require(count>0,"Missing scene layer");
    require(triangles<450000,"Scene exceeds mobile geometry budget");
    require(bytes<80*1024*1024,"Scene exceeds geometry memory budget");
    require(meshes.size()<300,"Too many render batches");
    std::cout<<meshes.size()<<" batches, "<<triangles<<" triangles, "<<bytes/(1024*1024)<<" MiB geometry\n";
    for(auto count:counts)std::cout<<count<<' ';
    std::cout<<'\n';
}
