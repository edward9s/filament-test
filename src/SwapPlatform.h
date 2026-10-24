#pragma once

#include <backend/platforms/VulkanPlatform.h>
#include <utils/Log.h>

namespace forest {
// Vulkan returns a usable image for both SUCCESS and SUBOPTIMAL. Filament 1.78.1
// otherwise retries SUBOPTIMAL, abandons its signaled semaphore and skips presenting.
template<class Platform>
class SwapPlatform : public Platform {
public:
    using SwapChainPtr = filament::backend::VulkanPlatform::SwapChainPtr;
    using ImageSyncData = filament::backend::VulkanPlatform::ImageSyncData;

    VkResult acquire(SwapChainPtr swap, ImageSyncData* image) override {
        const auto result = Platform::acquire(swap, image);
        if(result != VK_SUBOPTIMAL_KHR) return result;
        if(!reported_) {
            utils::slog.i << "Using acquired VK_SUBOPTIMAL_KHR image; presentation remains valid"
                          << utils::io::endl;
            reported_ = true;
        }
        return VK_SUCCESS;
    }
private:
    bool reported_ = false;
};
}
