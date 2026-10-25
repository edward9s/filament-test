#pragma once

#include <filament/Engine.h>

namespace forest {
inline filament::Engine* createRendererEngine(filament::backend::Backend backend,
        filament::backend::Platform* platform = nullptr) {
    filament::Engine::Builder builder;
    builder.backend(backend);
    builder.platform(platform);
    if (backend == filament::backend::Backend::VULKAN) {
        builder.feature("material.enable_material_instance_uniform_batching", true);
    }
    return builder.build();
}
}

