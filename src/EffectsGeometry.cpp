#include "EffectsGeometry.h"
#include "Geometry.h"
#include <math/mat3.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>

namespace forest {
using namespace filament::math;
namespace {
constexpr float pi=3.14159265359f;
float random(unsigned i) {
    i ^= i >> 16; i *= 0x7feb352du; i ^= i >> 15; i *= 0x846ca68bu; i ^= i >> 16;
    return float(i >> 8)/16777216.f;
}
void quad(std::vector<EffectVertex>& mesh,float3 a,float3 b,float3 c,float3 d,float4 color,bool lit=false) {
    quatf frame{0,0,0,1};
    if(lit) {
        const float3 t=normalize(b-a),n=normalize(cross(b-a,d-a));
        frame=mat3f::packTangentFrame(mat3f{t,cross(n,t),n});
    }
    mesh.insert(mesh.end(),{{a,{0,0},color,frame},{b,{1,0},color,frame},{c,{1,1},color,frame},{d,{0,1},color,frame}});
}
struct Puff { float3 position; float width,height; float4 color; };
struct Puffs {
    std::array<Puff,108> values;
    size_t size=0;
    void clear() {size=0;}
    void push_back(Puff puff) {assert(size<values.size());values[size++]=puff;}
};
void billboards(std::vector<EffectVertex>& mesh,Puffs& puffs,float3 eye) {
    // Stable back-to-front merge with stack scratch: equal distances keep their original order.
    std::array<unsigned,108> order,scratch;
    std::array<float,108> distances;
    for(size_t i=0;i<puffs.size;++i) {order[i]=unsigned(i);distances[i]=length2(puffs.values[i].position-eye);}
    auto farther=[&](unsigned a,unsigned b){return distances[a]>distances[b];};
    for(size_t width=1;width<puffs.size;width*=2) {
        for(size_t first=0;first<puffs.size;first+=width*2) {
            const size_t middle=std::min(first+width,puffs.size),last=std::min(first+width*2,puffs.size);
            std::merge(order.begin()+first,order.begin()+middle,
                order.begin()+middle,order.begin()+last,scratch.begin()+first,farther);
        }
        std::copy_n(scratch.begin(),puffs.size,order.begin());
    }
    for(size_t i=0;i<puffs.size;++i) {
        const auto& p=puffs.values[order[i]];
        float3 direction=eye-p.position;
        if(length2(direction)<1e-8f)direction={0,0,1};
        // A vertical camera direction still needs a well-defined billboard basis.
        float3 right=cross(float3{0,1,0},direction);
        right=length2(right)>1e-8f?normalize(right):float3{1,0,0};
        const float3 up=normalize(cross(normalize(direction),right));
        const float3 x=right*p.width*.5f,y=up*p.height*.5f;
        quad(mesh,p.position-x-y,p.position+x-y,p.position+x+y,p.position-x+y,p.color);
    }
}
void ribbon(std::vector<EffectVertex>& mesh,float3 a,float3 b,float width,float4 color,float3 eye) {
    float3 side=cross(b-a,eye-(a+b)*.5f);
    side=length2(side)>1e-8f?normalize(side):float3{1,0,0};
    side*=width*.5f;
    quad(mesh,a-side,a+side,b+side,b-side,color);
}
}
float3 effectOrigin(Effect effect) {
    float3 p;
    switch(effect) {
        case Effect::Fire:p={-3.2f,0,3.1f};break;
        case Effect::Blast:p={-6.3f,0,-1.6f};break;
        case Effect::Spray:case Effect::Water:p={3.5f,0,3.5f};break;
        case Effect::Lightning:p={6.3f,0,-3.2f};break;
        case Effect::Smoke:p={-7.f,0,-6.2f};break;
        default:throw std::invalid_argument("Unknown effect");
    }
    p.y=elevation(p.x,p.z)+.12f;
    return p;
}
float blastAge(float seconds) { return std::fmod(seconds,5.4f); }
float lightningPulse(float seconds) {
    const float phase=std::fmod(seconds,4.8f);
    const float a=(phase-.12f)/.055f,b=(phase-.3f)/.08f;
    return std::exp(-a*a)+.75f*std::exp(-b*b);
}
float effectLightIntensity(Effect effect,float seconds) {
    if(!std::isfinite(seconds)||seconds<0)throw std::invalid_argument("Invalid effect light time");
    // Match the local/direct-sun illumination ratio at 3 m: Godot uses energy/r,
    // Filament uses lumens/(4*pi*r*r). Both have the same range cutoff shape.
    // Reference suns: Godot 1.1 energy, Filament 110000 lux. This is a local
    // brightness calibration, not equality of the two distance curves or images.
    constexpr float lumensPerEnergy=4.f*pi*3.f*(110000.f/1.1f);
    switch(effect) {
        case Effect::Fire:
            return lumensPerEnergy*1.2f*(1.f+.13f*std::sin(seconds*13.f)+.09f*std::sin(seconds*23.f));
        case Effect::Blast: {
            const float age=blastAge(seconds);
            return age<2.5f?lumensPerEnergy*8.f*std::exp(-age*7.f):0.f;
        }
        case Effect::Lightning:
            return lumensPerEnergy*4.f*lightningPulse(seconds);
        default:throw std::invalid_argument("Effect does not emit light");
    }
}
void effectMeshes(float seconds,float3 eye,EffectMeshes& meshes) {
    if(!std::isfinite(seconds)||seconds<0||!std::isfinite(eye.x)||!std::isfinite(eye.y)||!std::isfinite(eye.z))
        throw std::invalid_argument("Invalid effect time or camera");
    for(auto& mesh:meshes)mesh.clear();
    const float age=blastAge(seconds),flash=lightningPulse(seconds);
    const std::array<bool,size_t(Effect::Count)> visible{true,age<2.5f,true,flash>=.003f,true,true};
    constexpr std::array<size_t,size_t(Effect::Count)> counts{312,432,180,144,208,1056};
    for(size_t i=0;i<meshes.size();++i)if(visible[i])meshes[i].reserve(counts[i]);
    Puffs puffs;
    const auto fire=effectOrigin(Effect::Fire);
    for(unsigned i=0;i<54;++i) {
        const float seed=random(i+1),age=std::fmod(seconds*.75f+seed,1.f),angle=random(i+91)*2*pi;
        const float radius=(1-age)*.35f;
        float3 p=fire+float3{std::cos(angle)*radius+.15f*std::sin(seconds*2+seed*12),.12f+age*1.9f,std::sin(angle)*radius};
        const float alpha=std::sin(pi*age)*.62f;
        puffs.push_back({p,.48f*(1-age)+.16f,.7f*(1-age)+.35f,{1,.12f+.32f*(1-age),.015f,alpha}});
    }
    for(unsigned i=0;i<24;++i) {
        const float seed=random(i+901),age=std::fmod(seconds*.3f+seed,1.f);
        const float angle=seed*2*pi;
        puffs.push_back({fire+float3{.45f*std::sin(angle+age*3),age*3,.4f*std::cos(angle)},.025f,.09f,{1,.4f,.06f,(1-age)*.85f}});
    }
    billboards(meshes[size_t(Effect::Fire)],puffs,eye);puffs.clear();

    // At zero opacity the fragment shader would discard every particle anyway.
    if(visible[size_t(Effect::Blast)]) {
        const auto blast=effectOrigin(Effect::Blast);
        const float fade=std::clamp(1-age/2.5f,0.f,1.f);
        for(unsigned i=0;i<72;++i) {
            const float a=random(i+1301)*2*pi,lift=random(i+701),radius=std::sqrt(std::min(age,2.5f))*1.25f;
            float3 p=blast+float3{std::cos(a)*radius*(.35f+lift*.65f),.6f+age*(1.4f+lift*2.6f),std::sin(a)*radius};
            const float size=.2f+std::min(age,2.5f)*(.5f+lift*.4f);
            puffs.push_back({p,size,size,{1,.10f+.3f*fade,.012f,fade*.65f}});
        }
        for(unsigned i=0;i<36;++i) {
            const float a=random(i+4101)*2*pi,speed=1.6f+random(i+151)*2;
            const float t=std::min(age,2.f);
            float3 p=blast+float3{std::cos(a)*t*speed,std::max(.04f,t*(2.5f+random(i+9)*2)-2.6f*t*t),std::sin(a)*t*speed};
            puffs.push_back({p,.035f,.15f,{1,.6f,.1f,std::clamp(1-age/1.5f,0.f,1.f)}});
        }
        billboards(meshes[size_t(Effect::Blast)],puffs,eye);puffs.clear();
    }

    const auto water=effectOrigin(Effect::Water);
    for(unsigned i=0;i<45;++i) {
        const float seed=random(i+2001),t=std::fmod(seconds*1.5f+seed,1.f),a=seed*2*pi;
        float3 p=water+float3{.75f+std::cos(a)*t*.7f,-.07f+std::max(0.f,t*1.3f-1.25f*t*t),1.45f+std::sin(a)*t*.7f};
        puffs.push_back({p,.04f+.08f*t,.08f+.09f*t,{.6f,.8f,.86f,(1-t)*.75f}});
    }
    billboards(meshes[size_t(Effect::Spray)],puffs,eye);puffs.clear();
    auto& surface=meshes[size_t(Effect::Water)];
    // Shallow flowing sheet, with an arc feeding it from a raised spring.
    auto point=[&](int x,int z) {
        float u=float(x)/6,v=float(z)/12;
        return water+float3{(u-.5f)*1.45f+.75f,-.085f+.012f*std::sin(v*25-seconds*4+u*8),v*3.3f+.25f};
    };
    for(int z=0;z<12;++z)for(int x=0;x<6;++x) {
        const size_t base=surface.size();
        quad(surface,point(x,z),point(x+1,z),point(x+1,z+1),point(x,z+1),{.065f,.17f,.16f,.62f},true);
        surface[base].uv={float(x)/6,float(z)/12};
        surface[base+1].uv={float(x+1)/6,float(z)/12};
        surface[base+2].uv={float(x+1)/6,float(z+1)/12};
        surface[base+3].uv={float(x)/6,float(z+1)/12};
    }
    auto jet=[&](int i){float t=float(i)/24;return water+float3{t*.75f,1.15f+t*.55f-1.8f*t*t,t*1.45f};};
    for(int i=0;i<24;++i) {
        const float radius=.048f+.006f*std::sin(seconds*8+i);
        const float3 a=jet(i),b=jet(i+1),tangent=normalize(b-a);
        const float3 side=normalize(cross(tangent,float3{1,0,0})),up=cross(tangent,side);
        for(int j=0;j<8;++j) {
            const float angle0=2*pi*j/8,angle1=2*pi*(j+1)/8;
            const float3 x=(side*std::cos(angle0)+up*std::sin(angle0))*radius;
            const float3 y=(side*std::cos(angle1)+up*std::sin(angle1))*radius;
            const size_t base=surface.size();
            quad(surface,a+x,a+y,b+y,b+x,{.16f,.35f,.38f,.56f},true);
            surface[base].uv={float(j)/8,float(i)/24};
            surface[base+1].uv={float(j+1)/8,float(i)/24};
            surface[base+2].uv={float(j+1)/8,float(i+1)/24};
            surface[base+3].uv={float(j)/8,float(i+1)/24};
        }
    }

    if(visible[size_t(Effect::Lightning)]) {
        const auto bolt=effectOrigin(Effect::Lightning);
        const unsigned strike=unsigned(std::floor(seconds/4.8f));
        auto boltPoint=[&](unsigned i) {
            float t=float(i)/18;
            return bolt+float3{i==0||i==18?0:(random(i+strike*73+891)-.5f)*.8f,5.2f*(1-t),i==0||i==18?0:(random(i+strike*91+477)-.5f)*.5f};
        };
        auto& lightning=meshes[size_t(Effect::Lightning)];
        for(unsigned i=0;i<18;++i)ribbon(lightning,boltPoint(i),boltPoint(i+1),.1f,{.5f,.7f,1,flash},eye);
        for(unsigned branch=0;branch<3;++branch) {
            float3 a=boltPoint(4+branch*4);
            for(unsigned i=0;i<6;++i) {
                float3 b=a+float3{(branch%2?1.f:-1.f)*(.13f+random(i+branch*11)*.22f),-.28f,(random(i+2)*2-1)*.25f};
                ribbon(lightning,a,b,.045f,{.35f,.55f,1,flash*(1-float(i)/7)},eye);a=b;
            }
        }
    }
    const auto smoke=effectOrigin(Effect::Smoke);
    for(unsigned i=0;i<52;++i) {
        const float seed=random(i+3001),age=std::fmod(seconds*.12f+seed,1.f);
        const float a=random(i+1801)*2*pi,size=.75f+age*1.8f;
        float3 p=smoke+float3{age*1.1f+std::sin(a+seconds*.3f)*(.15f+age*.55f),.35f+age*5.5f,std::cos(a+age*2)*(.2f+age*.55f)};
        const float shade=.055f+age*.08f;
        puffs.push_back({p,size,size*1.15f,{shade,shade*1.01f,shade*1.04f,std::sin(pi*age)*.7f}});
    }
    billboards(meshes[size_t(Effect::Smoke)],puffs,eye);
}
EffectMeshes effectMeshes(float seconds,float3 eye) {
    EffectMeshes meshes;
    effectMeshes(seconds,eye,meshes);
    return meshes;
}
}

