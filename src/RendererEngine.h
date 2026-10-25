#pragma once

#include <filament/Engine.h>

namespace forest {
inline filament::Engine* createRendererEngine(filament::backend::Backend backend,
        filament::backend::Platform* platform = nullptr) {
    filament::Engine::Builder builder;
    builder.backend(backend);
    builder.platform(platform);
    if (backend == filament::backend::Backend::VULKAN) {
        // Avoid the shared, persistently mapped material UBO on Android Vulkan drivers.
        // Individual material buffers use Filament's regular staged upload path.
        builder.feature("material.enable_material_instance_uniform_batching", false);
    }
    return builder.build();
}
}
