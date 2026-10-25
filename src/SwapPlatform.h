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

    explicit SwapPlatform(bool synchronized = false) : synchronized_(synchronized) {}

    VkResult acquire(SwapChainPtr swap, ImageSyncData* image) override {
        const auto result = Platform::acquire(swap, image);
        if(acquireReports_++ < 4)
            utils::slog.i << "Vulkan acquire result=" << int32_t(result)
                          << " image=" << image->imageIndex << utils::io::endl;
        if(result != VK_SUBOPTIMAL_KHR) return result;
        if(!reported_) {
            utils::slog.i << "Using acquired VK_SUBOPTIMAL_KHR image; presentation remains valid"
                          << utils::io::endl;
            reported_ = true;
        }
        return VK_SUCCESS;
    }
    VkResult present(SwapChainPtr swap, uint32_t image, VkSemaphore finished) override {
        // Diagnostic only: finish submitted drawing BEFORE endFrame's cache GC can run.
        // Waiting in the application after endFrame would be too late for that GC.
        if(synchronized_) {
            const auto result = bluevk::vkQueueWaitIdle(this->getGraphicsQueue());
            if(result != VK_SUCCESS) {
                utils::slog.e << "Vulkan diagnostic queue wait failed: " << int32_t(result)
                              << utils::io::endl;
                return result;
            }
        }
        const auto result = Platform::present(swap, image, finished);
        if(presentReports_++ < 4)
            utils::slog.i << "Vulkan present result=" << int32_t(result)
                          << " image=" << image << " synchronized=" << synchronized_
                          << utils::io::endl;
        return result;
    }
private:
    bool reported_ = false;
    bool synchronized_ = false;
    unsigned acquireReports_ = 0, presentReports_ = 0;
};
}
