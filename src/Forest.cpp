#include "Forest.h"
#include "Geometry.h"
#include "PackedVertex.h"
#include <filament/LightManager.h>
#include <filament/RenderableManager.h>
#include <filament/TextureSampler.h>
#include <image/Ktx1Bundle.h>
#include <ktxreader/Ktx1Reader.h>
#include <ktxreader/Ktx2Reader.h>
#include <utils/EntityManager.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace forest {
using namespace filament;
Forest::Forest(Engine& engine, Scene& scene) : engine_(engine),scene_(scene) {}
Forest::~Forest() {
    scene_.setIndirectLight(nullptr);scene_.setSkybox(nullptr);
    for(auto entity:entities_) {scene_.remove(entity);engine_.destroy(entity);utils::EntityManager::get().destroy(entity);}
    for(auto* v:vertices_) engine_.destroy(v);
    for(auto* i:indices_) engine_.destroy(i);
    if(light_) engine_.destroy(light_);
    if(sky_) engine_.destroy(sky_);
    for(auto* m:instances_) engine_.destroy(m);
    for(auto* m:materials_) engine_.destroy(m);
    for(auto* t:textures_) engine_.destroy(t);
}
void Forest::load(const AssetReader& read) {
    if(!materials_.empty()) throw std::logic_error("Forest already loaded");
    auto material=[&](const char* name) {
        const auto data=read(std::string("forest/")+name+".filamat");
        auto* m=Material::Builder().package(data.data(),data.size()).build(engine_);
        if(!m) throw std::runtime_error(std::string("Invalid material: ")+name);
        materials_.push_back(m);return m;
    };
    ktxreader::Ktx2Reader reader(engine_);
    // The same UASTC source targets native blocks supported by either backend.
    // ASTC / BC7 are preferred; ETC2 and RGBA8 cover other hardware capabilities.
    for(auto format:{Texture::InternalFormat::SRGB8_ALPHA8_ASTC_4x4,Texture::InternalFormat::RGBA_ASTC_4x4,
            Texture::InternalFormat::SRGB_ALPHA_BPTC_UNORM,Texture::InternalFormat::RGBA_BPTC_UNORM,
            Texture::InternalFormat::ETC2_EAC_SRGBA8,Texture::InternalFormat::ETC2_EAC_RGBA8,
            Texture::InternalFormat::SRGB8_A8,Texture::InternalFormat::RGBA8})
        reader.requestFormat(format);
    auto texture=[&](const std::string& name,bool srgb) {
        const auto bytes=read("forest/"+name+".ktx2");
        auto* t=reader.load(bytes.data(),bytes.size(),srgb?ktxreader::Ktx2Reader::TransferFunction::sRGB:
            ktxreader::Ktx2Reader::TransferFunction::LINEAR);
        if(!t)throw std::runtime_error("Invalid KTX2 texture: "+name);
        textures_.push_back(t);
        if(t->getWidth()!=1024 || t->getHeight()!=1024 || t->getLevels()!=11)
            throw std::runtime_error("Invalid KTX2 dimensions or mip levels: "+name);
        return t;
    };
    auto normal=[&](const std::string& name) {
        // A fixed 1024x1024 RGBA8 payload avoids a runtime image-decoder dependency.
        auto bytes=std::make_unique<std::vector<uint8_t>>(read("forest/"+name+".rgba"));
        if(bytes->size()!=1024u*1024u*4u) throw std::runtime_error("Invalid texture size: "+name);
        auto* t=Texture::Builder().width(1024).height(1024).levels(0xff)
            .sampler(Texture::Sampler::SAMPLER_2D)
            .usage(Texture::Usage::DEFAULT | Texture::Usage::GEN_MIPMAPPABLE)
            .format(Texture::InternalFormat::RGBA8).build(engine_);
        textures_.push_back(t);
        auto* payload=bytes.release();
        t->setImage(engine_,0,Texture::PixelBufferDescriptor(payload->data(),payload->size(),
            Texture::Format::RGBA,Texture::Type::UBYTE,
            [](void*,size_t,void* user){delete static_cast<std::vector<uint8_t>*>(user);},payload));
        t->generateMipmaps(engine_);return t;
    };
    auto* surface=material("surface");auto* foliage=material("foliage");
    TextureSampler sampler(TextureSampler::MinFilter::LINEAR_MIPMAP_LINEAR,
        TextureSampler::MagFilter::LINEAR,TextureSampler::WrapMode::REPEAT);
    sampler.setAnisotropy(4);
    for(const char* name:{"ground","stone","bark"}) {
        auto* instance=surface->createInstance();instances_.push_back(instance);
        instance->setParameter("albedo",texture(std::string(name)+"_albedo",true),sampler);
        instance->setParameter("normalMap",normal(std::string(name)+"_normal"),sampler);
        instance->setParameter("arm",texture(std::string(name)+"_arm",false),sampler);
    }
    leaves_=foliage->createInstance();instances_.push_back(leaves_);
    canopy_=material("canopy")->createInstance();instances_.push_back(canopy_);
    canopy_->setParameter("albedo",texture("canopy",true),sampler);
    animate(0);
    auto meshes=generate();
    for(auto& mesh:meshes) {
        if(mesh.indices.empty()) continue;
        if(mesh.vertices.size()>65536)throw std::runtime_error("Forest batch exceeds 16-bit vertex limit");
        float3 lo{std::numeric_limits<float>::max()},hi{-std::numeric_limits<float>::max()};
        for(const auto& v:mesh.vertices) {lo=min(lo,v.position);hi=max(hi,v.position);}
        // Wind remains within this expanded box, so moving foliage is never culled early.
        lo-=float3{.12f};hi+=float3{.12f};
        auto* vb=VertexBuffer::Builder().vertexCount(uint32_t(mesh.vertices.size())).bufferCount(1)
            .attribute(VertexAttribute::POSITION,0,VertexBuffer::AttributeType::FLOAT3,offsetof(PackedVertex,position),sizeof(PackedVertex))
            .attribute(VertexAttribute::TANGENTS,0,VertexBuffer::AttributeType::SHORT4,offsetof(PackedVertex,tangent),sizeof(PackedVertex))
            .normalized(VertexAttribute::TANGENTS)
            .attribute(VertexAttribute::UV0,0,VertexBuffer::AttributeType::FLOAT2,offsetof(PackedVertex,uv),sizeof(PackedVertex))
            .attribute(VertexAttribute::COLOR,0,VertexBuffer::AttributeType::HALF4,offsetof(PackedVertex,color),sizeof(PackedVertex)).build(engine_);
        vertices_.push_back(vb);
        auto* ib=IndexBuffer::Builder().indexCount(uint32_t(mesh.indices.size()))
            .bufferType(IndexBuffer::IndexType::USHORT).build(engine_);indices_.push_back(ib);
        auto* v=new std::vector<PackedVertex>();v->reserve(mesh.vertices.size());
        for(const auto& vertex:mesh.vertices)v->push_back(packVertex(vertex));
        vb->setBufferAt(engine_,0,VertexBuffer::BufferDescriptor(v->data(),v->size()*sizeof(PackedVertex),
            [](void*,size_t,void* u){delete static_cast<std::vector<PackedVertex>*>(u);},v));
        auto* i=new std::vector<uint16_t>(mesh.indices.begin(),mesh.indices.end());
        ib->setBuffer(engine_,IndexBuffer::BufferDescriptor(i->data(),i->size()*sizeof(uint16_t),
            [](void*,size_t,void* u){delete static_cast<std::vector<uint16_t>*>(u);},i));
        auto e=utils::EntityManager::get().create();entities_.push_back(e);
        auto result=RenderableManager::Builder(1).boundingBox({(lo+hi)*.5f,(hi-lo)*.5f})
            .material(0,instances_[size_t(mesh.surface)]).geometry(0,RenderableManager::PrimitiveType::TRIANGLES,vb,ib)
            .castShadows(mesh.surface!=Surface::Ground).receiveShadows(true).build(engine_,e);
        if(result!=RenderableManager::Builder::Success) throw std::runtime_error("Failed to create forest mesh");
        scene_.addEntity(e);
    }
    auto environment=read("forest/environment_ibl.ktx");
    auto bundle=std::make_unique<image::Ktx1Bundle>(environment.data(),uint32_t(environment.size()));
    float3 sh[9];
    if(!bundle->getSphericalHarmonics(sh)) throw std::runtime_error("Environment is missing irradiance SH");
    auto* reflection=ktxreader::Ktx1Reader::createTexture(&engine_,bundle.release(),false);
    if(!reflection) throw std::runtime_error("Failed to load environment");
    textures_.push_back(reflection);
    light_=IndirectLight::Builder().reflections(reflection).irradiance(3,sh).intensity(9000).build(engine_);
    scene_.setIndirectLight(light_);
    auto skyData=read("forest/environment_skybox.ktx");
    auto* skyTexture=ktxreader::Ktx1Reader::createTexture(&engine_,
        new image::Ktx1Bundle(skyData.data(),uint32_t(skyData.size())),false);
    if(!skyTexture) throw std::runtime_error("Failed to load sky");
    textures_.push_back(skyTexture);
    sky_=Skybox::Builder().environment(skyTexture).showSun(true).intensity(9000).build(engine_);scene_.setSkybox(sky_);
    auto sun=utils::EntityManager::get().create();entities_.push_back(sun);
    LightManager::ShadowOptions shadows;
    shadows.mapSize=2048;shadows.shadowCascades=3;
    shadows.cascadeSplitPositions[0]=.12f;shadows.cascadeSplitPositions[1]=.38f;
    shadows.shadowFar=65;
    LightManager::Builder(LightManager::Type::SUN).color({1.0f,.88f,.70f}).intensity(110000)
        .direction({-.32f,-.82f,-.47f}).sunAngularRadius(.53f)
        .castShadows(true).shadowOptions(shadows).build(engine_,sun);
    scene_.addEntity(sun);
}
void Forest::animate(float seconds) {
    if(leaves_)leaves_->setParameter("time",seconds);
    if(canopy_)canopy_->setParameter("time",seconds);
}
void Forest::configure(View& view) {
    view.setPostProcessingEnabled(true);
    view.setAntiAliasing(AntiAliasing::FXAA);
    TemporalAntiAliasingOptions taa;taa.enabled=true;
    view.setTemporalAntiAliasingOptions(taa);
    AmbientOcclusionOptions ao;ao.enabled=true;ao.radius=.75f;ao.power=1.2f;ao.quality=QualityLevel::MEDIUM;
    view.setAmbientOcclusionOptions(ao);
    BloomOptions bloom;bloom.enabled=true;bloom.strength=.065f;bloom.threshold=true;
    view.setBloomOptions(bloom);
    FogOptions fog;fog.enabled=true;fog.distance=18;fog.density=.004f;fog.height=1;
    fog.heightFalloff=.2f;fog.color={.36f,.43f,.44f};fog.inScatteringStart=12;fog.inScatteringSize=20;
    view.setFogOptions(fog);
    DynamicResolutionOptions resolution;resolution.enabled=true;resolution.minScale={.65f,.65f};
    resolution.maxScale={1,1};resolution.quality=QualityLevel::MEDIUM;
    view.setDynamicResolutionOptions(resolution);
}
}
