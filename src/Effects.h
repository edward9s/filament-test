#pragma once
#include "Forest.h"
#include "EffectsGeometry.h"
#include "UploadPool.h"
#include <array>

namespace forest {
class Effects {
public:
    Effects(filament::Engine& engine,filament::Scene& scene):engine_(engine),scene_(scene) {}
    ~Effects();
    Effects(const Effects&)=delete;
    Effects& operator=(const Effects&)=delete;
    void load(const AssetReader& read);
    void setEnabled(bool enabled);
    void animate(float seconds,filament::math::float3 eye);
private:
    struct Batch {
        utils::Entity entity{};
        filament::VertexBuffer* vertices=nullptr;
        filament::IndexBuffer* indices=nullptr;
        filament::MaterialInstance* material=nullptr;
        size_t count=0;
        bool visible=true;
    };
    bool enabled_=true;
    filament::Engine& engine_;
    filament::Scene& scene_;
    std::array<Batch,size_t(Effect::Count)> batches_{};
    std::array<filament::Material*,size_t(Effect::Count)> materials_{};
    std::array<utils::Entity,3> lights_{};
    UploadPool<EffectMeshes> uploads_;
};
}
