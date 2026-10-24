#include "Effects.h"
#include <filament/LightManager.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace forest {
using namespace filament;
using namespace filament::math;
namespace {
constexpr Effect lightEffects[]={Effect::Fire,Effect::Blast,Effect::Lightning};
}
Effects::~Effects() {
    for(auto entity:lights_) {scene_.remove(entity);engine_.destroy(entity);utils::EntityManager::get().destroy(entity);}
    for(auto& batch:batches_) {
        scene_.remove(batch.entity);engine_.destroy(batch.entity);utils::EntityManager::get().destroy(batch.entity);
        if(batch.vertices)engine_.destroy(batch.vertices);
        if(batch.indices)engine_.destroy(batch.indices);
        if(batch.material)engine_.destroy(batch.material);
    }
    for(auto* material:materials_)if(material)engine_.destroy(material);
}
void Effects::load(const AssetReader& read) {
    if(materials_[0])throw std::logic_error("Effects already loaded");
    const char* names[]={"forest/effects.filamat","forest/water.filamat"};
    for(size_t i=0;i<materials_.size();++i) {
        auto bytes=read(names[i]);
        materials_[i]=Material::Builder().package(bytes.data(),bytes.size()).build(engine_);
        if(!materials_[i])throw std::runtime_error("Invalid effects material");
    }
    const auto meshes=effectMeshes(.12f,{3.07f,3.16f,9.54f});
    for(size_t i=0;i<batches_.size();++i) {
        auto& b=batches_[i];b.count=meshes[i].size();
        if(b.count==0||b.count%4||b.count>std::numeric_limits<uint16_t>::max())throw std::logic_error("Invalid effects mesh size");
        b.material=materials_[i==size_t(Effect::Water)?1:0]->createInstance();
        if(i!=size_t(Effect::Water))b.material->setParameter("kind",int(i));
        b.material->setParameter("time",0.f);
        VertexBuffer::Builder builder;
        builder.vertexCount(uint32_t(b.count)).bufferCount(1)
            .attribute(VertexAttribute::POSITION,0,VertexBuffer::AttributeType::FLOAT3,offsetof(EffectVertex,position),sizeof(EffectVertex))
            .attribute(VertexAttribute::UV0,0,VertexBuffer::AttributeType::FLOAT2,offsetof(EffectVertex,uv),sizeof(EffectVertex))
            .attribute(VertexAttribute::COLOR,0,VertexBuffer::AttributeType::FLOAT4,offsetof(EffectVertex,color),sizeof(EffectVertex));
        if(i==size_t(Effect::Water))builder.attribute(VertexAttribute::TANGENTS,0,
            VertexBuffer::AttributeType::FLOAT4,offsetof(EffectVertex,tangent),sizeof(EffectVertex));
        b.vertices=builder.build(engine_);
        auto* indices=new std::vector<uint16_t>();indices->reserve(b.count/4*6);
        for(size_t j=0;j<b.count;j+=4)for(unsigned offset:{0u,1u,2u,0u,2u,3u})indices->push_back(uint16_t(j+offset));
        b.indices=IndexBuffer::Builder().indexCount(uint32_t(indices->size())).bufferType(IndexBuffer::IndexType::USHORT).build(engine_);
        b.indices->setBuffer(engine_,IndexBuffer::BufferDescriptor(indices->data(),indices->size()*sizeof(uint16_t),
            [](void*,size_t,void* user){delete static_cast<std::vector<uint16_t>*>(user);},indices));
        b.entity=utils::EntityManager::get().create();
        // Bounds include the entire trajectory, not just the current particle positions.
        const auto result=RenderableManager::Builder(1).boundingBox({effectOrigin(Effect(i))+float3{0,4,0},{7,7,7}})
            .geometry(0,RenderableManager::PrimitiveType::TRIANGLES,b.vertices,b.indices)
            .material(0,b.material).castShadows(false).receiveShadows(i==size_t(Effect::Water)).build(engine_,b.entity);
        if(result!=RenderableManager::Builder::Success)throw std::runtime_error("Cannot create effects renderable");
        scene_.addEntity(b.entity);
    }
    // The sunny outdoor exposure requires strong local light to tint nearby masonry.
    // Keep the influence local; shadow maps for three animated point lights are costly on mobile.
    const float3 colors[]={{1,.27f,.055f},{1,.46f,.12f},{.55f,.72f,1}};
    const float heights[]={.95f,1.5f,2.7f};
    const float radii[]={7.5f,9.f,8.5f};
    for(size_t i=0;i<lights_.size();++i) {
        lights_[i]=utils::EntityManager::get().create();
        LightManager::Builder(LightManager::Type::POINT)
            .position(effectOrigin(lightEffects[i])+float3{0,heights[i],0})
            .color(colors[i]).intensity(0.f).falloff(radii[i]).castShadows(false)
            .build(engine_,lights_[i]);
        scene_.addEntity(lights_[i]);
    }
    animate(0,{3.07f,3.16f,9.54f});
}
void Effects::animate(float seconds,float3 eye) {
    if(!materials_[0])throw std::logic_error("Effects are not loaded");
    auto meshes=effectMeshes(seconds,eye);
    for(size_t i=0;i<batches_.size();++i) {
        auto& b=batches_[i];
        const bool visible=!meshes[i].empty();
        if(visible!=b.visible) {
            auto& renderables=engine_.getRenderableManager();
            renderables.setLayerMask(renderables.getInstance(b.entity),0xff,visible?0x1:0x0);
            b.visible=visible;
        }
        if(!visible)continue;
        if(meshes[i].size()!=b.count)throw std::logic_error("Effects topology changed");
        auto* vertices=new std::vector<EffectVertex>(std::move(meshes[i]));
        b.vertices->setBufferAt(engine_,0,VertexBuffer::BufferDescriptor(vertices->data(),vertices->size()*sizeof(EffectVertex),
            [](void*,size_t,void* user){delete static_cast<std::vector<EffectVertex>*>(user);},vertices));
        b.material->setParameter("time",seconds);
    }
    auto& manager=engine_.getLightManager();
    for(size_t i=0;i<lights_.size();++i)
        manager.setIntensity(manager.getInstance(lights_[i]),effectLightIntensity(lightEffects[i],seconds));
}
}
