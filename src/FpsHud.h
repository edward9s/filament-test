#pragma once
#include "Forest.h"
#include "FpsCounter.h"
#include <filament/Camera.h>
#include <filament/Renderer.h>

namespace forest {
class FpsHud {
public:
    explicit FpsHud(filament::Engine& engine) : engine_(engine) {}
    ~FpsHud();
    FpsHud(const FpsHud&) = delete;
    FpsHud& operator=(const FpsHud&) = delete;
    void load(const AssetReader& read);
    void render(filament::Renderer& renderer, int width, int height);
    void completedFrame(double seconds);
    void reset();
private:
    filament::Engine& engine_;
    filament::Scene* scene_ = nullptr;
    filament::View* view_ = nullptr;
    filament::Camera* camera_ = nullptr;
    filament::Material* material_ = nullptr;
    filament::MaterialInstance* instance_ = nullptr;
    filament::VertexBuffer* vertices_ = nullptr;
    filament::IndexBuffer* indices_ = nullptr;
    utils::Entity cameraEntity_{}, quad_{};
    FpsCounter counter_;
};
}
