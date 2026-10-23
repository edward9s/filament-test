#include "Geometry.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <array>

void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
int main() {
    const auto meshes=forest::generate();
    const auto repeated=forest::generate();
    require(meshes.size()==repeated.size(),"Non-deterministic batching");
    std::array<size_t,5> counts{};size_t triangles=0,bytes=0;
    for(size_t k=0;k<meshes.size();++k) {
        const auto& mesh=meshes[k];const auto& second=repeated[k];
        require(!mesh.vertices.empty() && mesh.indices.size()%3==0,"Empty or malformed mesh");
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
        triangles+=mesh.indices.size()/3;bytes+=mesh.vertices.size()*sizeof(forest::Vertex)+mesh.indices.size()*4;
    }
    for(auto count:counts)require(count>0,"Missing scene layer");
    require(triangles<450000,"Scene exceeds mobile geometry budget");
    require(bytes<80*1024*1024,"Scene exceeds geometry memory budget");
    require(meshes.size()<300,"Too many render batches");
    std::cout<<meshes.size()<<" batches, "<<triangles<<" triangles, "<<bytes/(1024*1024)<<" MiB geometry\n";
    for(auto count:counts)std::cout<<count<<' ';
    std::cout<<'\n';
}
