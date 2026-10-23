#include "FpsHud.h"
#include <filament/RenderableManager.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <algorithm>
#include <stdexcept>

namespace forest {
using namespace filament;
namespace {
struct HudVertex { math::float3 position; math::float2 uv; };
// Static storage stays valid until Filament finishes the asynchronous upload.
const HudVertex quadVertices[] = {
    {{-1,-1,0},{0,0}}, {{1,-1,0},{1,0}}, {{1,1,0},{1,1}}, {{-1,1,0},{0,1}}
};
const uint16_t quadIndices[] = {0,1,2,0,2,3};
}
FpsHud::~FpsHud() {
    if (view_) engine_.destroy(view_);
    if (scene_) { scene_->remove(quad_); engine_.destroy(scene_); }
    engine_.destroy(quad_);
    utils::EntityManager::get().destroy(quad_);
    if (camera_) engine_.destroyCameraComponent(cameraEntity_);
    utils::EntityManager::get().destroy(cameraEntity_);
    if (vertices_) engine_.destroy(vertices_);
    if (indices_) engine_.destroy(indices_);
    if (instance_) engine_.destroy(instance_);
    if (material_) engine_.destroy(material_);
}
void FpsHud::load(const AssetReader& read) {
    if (material_) throw std::logic_error("FPS HUD already loaded");
    const auto bytes = read("forest/hud.filamat");
    material_ = Material::Builder().package(bytes.data(),bytes.size()).build(engine_);
    if (!material_) throw std::runtime_error("Invalid FPS HUD material");
    instance_ = material_->createInstance();
    reset();
    scene_ = engine_.createScene();
    view_ = engine_.createView();
    cameraEntity_ = utils::EntityManager::get().create();
    camera_ = engine_.createCamera(cameraEntity_);
    camera_->setProjection(Camera::Projection::ORTHO,-1,1,-1,1,0.1,10);
    camera_->lookAt({0,0,1},{0,0,0});
    camera_->setExposure(1.0f);
    view_->setScene(scene_); view_->setCamera(camera_);
    // Composite this small translucent view through Filament's color pass.
    view_->setPostProcessingEnabled(true);
    view_->setBlendMode(View::BlendMode::TRANSLUCENT);
    view_->setShadowingEnabled(false);
    vertices_ = VertexBuffer::Builder().vertexCount(4).bufferCount(1)
        .attribute(VertexAttribute::POSITION,0,VertexBuffer::AttributeType::FLOAT3,offsetof(HudVertex,position),sizeof(HudVertex))
        .attribute(VertexAttribute::UV0,0,VertexBuffer::AttributeType::FLOAT2,offsetof(HudVertex,uv),sizeof(HudVertex)).build(engine_);
    vertices_->setBufferAt(engine_,0,VertexBuffer::BufferDescriptor(quadVertices,sizeof(quadVertices)));
    indices_ = IndexBuffer::Builder().indexCount(6).bufferType(IndexBuffer::IndexType::USHORT).build(engine_);
    indices_->setBuffer(engine_,IndexBuffer::BufferDescriptor(quadIndices,sizeof(quadIndices)));
    quad_ = utils::EntityManager::get().create();
    const auto result = RenderableManager::Builder(1).boundingBox({{0,0,0},{1,1,1}})
        .geometry(0,RenderableManager::PrimitiveType::TRIANGLES,vertices_,indices_)
        .material(0,instance_).culling(false).castShadows(false).receiveShadows(false).build(engine_,quad_);
    if (result != RenderableManager::Builder::Success) throw std::runtime_error("Cannot create FPS HUD");
    scene_->addEntity(quad_);
}
void FpsHud::render(Renderer& renderer, int width, int height) {
    if (width <= 0 || height <= 0) return;
    const int margin = std::max(8,std::min(width,height)/40);
    const int w = std::min(std::clamp(std::min(width,height)/3,180,270),width-2*margin);
    const int h = w*15/54;
    if (w <= 0 || height < h+2*margin) return;
    view_->setViewport({margin,height-margin-h,uint32_t(w),uint32_t(h)});
    renderer.render(view_);
}
void FpsHud::completedFrame(double seconds) {
    if (counter_.record(seconds)) instance_->setParameter("fps",counter_.value());
}
void FpsHud::reset() {
    counter_.reset();
    if (instance_) instance_->setParameter("fps",-1.f);
}
}
