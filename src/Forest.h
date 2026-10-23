#pragma once
#include <filament/Engine.h>
#include <filament/Scene.h>
#include <filament/View.h>
#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <filament/VertexBuffer.h>
#include <filament/IndexBuffer.h>
#include <filament/IndirectLight.h>
#include <filament/Skybox.h>
#include <utils/Entity.h>
#include <functional>
#include <string>
#include <vector>

namespace forest {
using AssetReader = std::function<std::vector<uint8_t>(const std::string&)>;
class Forest {
public:
    Forest(filament::Engine& engine, filament::Scene& scene);
    ~Forest();
    Forest(const Forest&) = delete;
    Forest& operator=(const Forest&) = delete;
    void load(const AssetReader& read);
    void animate(float seconds);
    static void configure(filament::View& view);
private:
    filament::Engine& engine_;
    filament::Scene& scene_;
    std::vector<utils::Entity> entities_;
    std::vector<filament::VertexBuffer*> vertices_;
    std::vector<filament::IndexBuffer*> indices_;
    std::vector<filament::Material*> materials_;
    std::vector<filament::MaterialInstance*> instances_;
    std::vector<filament::Texture*> textures_;
    filament::MaterialInstance* leaves_ = nullptr;
    filament::MaterialInstance* canopy_ = nullptr;
    filament::IndirectLight* light_ = nullptr;
    filament::Skybox* sky_ = nullptr;
};
}
