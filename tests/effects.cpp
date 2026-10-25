#include "EffectsGeometry.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

int main() {
    using namespace forest;
    using namespace filament::math;
    const float3 eye{3.07f,3.16f,9.54f};
    const auto initial=effectMeshes(0,eye);
    const auto same=effectMeshes(0,eye);
    size_t count=0;
    for(size_t i=0;i<initial.size();++i) {
        assert(!initial[i].empty() && initial[i].size()%4==0);
        count+=initial[i].size();
        for(size_t j=0;j<initial[i].size();++j)assert(initial[i][j].position==same[i][j].position);
    }
    assert(count<3000); // Less than 1,500 triangles, with fixed buffer sizes.
    std::array<bool,size_t(Effect::Count)> moved{};
    for(float time:{.12f,.3f,.9f,1.6f,4.799f,4.801f,5.399f,5.401f,120.f,3600.f}) {
        for(float3 camera:{eye,float3{0,14,-10},effectOrigin(Effect::Fire)}) {
            const auto meshes=effectMeshes(time,camera);
            for(size_t i=0;i<meshes.size();++i) {
                const bool hidden=(i==size_t(Effect::Blast) && blastAge(time)>=2.5f) ||
                    (i==size_t(Effect::Lightning) && lightningPulse(time)<.003f);
                if(hidden) {assert(meshes[i].empty());continue;}
                assert(meshes[i].size()==initial[i].size());
                for(size_t j=0;j<meshes[i].size();++j) {
                    const auto& v=meshes[i][j];
                    assert(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z));
                    assert(v.uv.x>=0&&v.uv.x<=1&&v.uv.y>=0&&v.uv.y<=1);
                    assert(std::isfinite(v.color.a)&&v.color.a>=0&&v.color.a<=1.01f);
                    assert(std::abs(length(v.tangent)-1)<.001f);
                    if(v.position!=initial[i][j].position)moved[i]=true;
                }
                for(size_t j=0;j<meshes[i].size();j+=4) {
                    const auto a=meshes[i][j].position,b=meshes[i][j+1].position,d=meshes[i][j+3].position;
                    assert(length(cross(b-a,d-a))>1e-7f);
                }
            }
        }
    }
    for(bool changed:moved)assert(changed);
    // A hidden batch must return at full capacity on the next cycle.
    assert(effectMeshes(3.0f,eye)[size_t(Effect::Blast)].empty());
    assert(effectMeshes(5.52f,eye)[size_t(Effect::Blast)].size()==initial[size_t(Effect::Blast)].size());
    assert(effectMeshes(2.0f,eye)[size_t(Effect::Lightning)].empty());
    assert(effectMeshes(4.92f,eye)[size_t(Effect::Lightning)].size()==initial[size_t(Effect::Lightning)].size());
    EffectMeshes reused;
    effectMeshes(.12f,eye,reused);
    std::array<const EffectVertex*,size_t(Effect::Count)> storage{};
    for(size_t i=0;i<reused.size();++i)storage[i]=reused[i].data();
    for(float time:{.3f,3.f,4.92f,5.52f,120.f}) {
        effectMeshes(time,eye,reused);
        const auto fresh=effectMeshes(time,eye);
        for(size_t i=0;i<reused.size();++i) {
            assert(reused[i].data()==storage[i]);
            assert(reused[i].size()==fresh[i].size());
            for(size_t j=0;j<reused[i].size();++j) {
                const auto& a=reused[i][j];const auto& b=fresh[i][j];
                assert(a.position==b.position && a.uv==b.uv && a.color==b.color && a.tangent==b.tangent);
            }
        }
    }
    for(Effect a:{Effect::Fire,Effect::Blast,Effect::Water,Effect::Lightning,Effect::Smoke})
        for(Effect b:{Effect::Fire,Effect::Blast,Effect::Water,Effect::Lightning,Effect::Smoke})
            if(a!=b)assert(length(effectOrigin(a)-effectOrigin(b))>3);
    assert(lightningPulse(.12f)>.9f&&lightningPulse(2)<.0001f);
    assert(blastAge(5.41f)<.02f);
    // Lit geometry receives a strong local source despite the bright daytime sun.
    assert(effectLightIntensity(Effect::Fire,0)>1000000.f);
    assert(effectLightIntensity(Effect::Fire,.2f)!=effectLightIntensity(Effect::Fire,0));
    assert(effectLightIntensity(Effect::Blast,0)>effectLightIntensity(Effect::Blast,.4f));
    assert(effectLightIntensity(Effect::Blast,2.5f)==0.f);
    assert(effectLightIntensity(Effect::Blast,5.41f)>effectLightIntensity(Effect::Blast,2.f));
    assert(effectLightIntensity(Effect::Lightning,.12f)>effectLightIntensity(Effect::Lightning,2.f));
    assert(effectLightIntensity(Effect::Lightning,.3f)>effectLightIntensity(Effect::Lightning,2.f));
    bool invalidLight=false;
    try{effectLightIntensity(Effect::Smoke,0);}catch(const std::invalid_argument&){invalidLight=true;}
    assert(invalidLight);
    for(float time:{-1.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        bool rejected=false;
        try{effectMeshes(time,eye);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
        rejected=false;
        try{effectLightIntensity(Effect::Fire,time);}catch(const std::invalid_argument&){rejected=true;}
        assert(rejected);
    }
    std::cout<<count/4<<" animated quads, six fixed-size batches; determinism, topology, lighting, timing, separation and invalid inputs passed\n";
}

