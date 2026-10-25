#pragma once

#include "SwapPlatform.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace forest {
// Only instantiated in the validation build. Normal and synchronized builds keep
// their existing instance, device and rendering behavior.
template<class Platform>
class ValidationPlatform final : public SwapPlatform<Platform> {
public:
    void terminate() override {
        if(messenger_ != VK_NULL_HANDLE) {
            const auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                bluevk::vkGetInstanceProcAddr(this->getInstance(), "vkDestroyDebugUtilsMessengerEXT"));
            destroy(this->getInstance(), messenger_, nullptr);
            messenger_ = VK_NULL_HANDLE;
        }
        Platform::terminate();
    }

protected:
    VkInstance createVkInstance(const VkInstanceCreateInfo& original) noexcept override {
        // Request explicitly: a missing layer must fail initialization, never
        // silently turn this into another unvalidated tablet test.
        constexpr auto layer = "VK_LAYER_KHRONOS_validation";
        std::vector<const char*> layers;
        for(uint32_t i=0; i<original.enabledLayerCount; ++i)
            layers.push_back(original.ppEnabledLayerNames[i]);
        append(layers, layer);
        std::vector<const char*> extensions;
        for(uint32_t i=0; i<original.enabledExtensionCount; ++i)
            extensions.push_back(original.ppEnabledExtensionNames[i]);
        append(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        append(extensions, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);

        VkDebugUtilsMessengerCreateInfoEXT debug{};
        debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = report;

        const VkValidationFeatureEnableEXT enabled =
            VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
        VkValidationFeaturesEXT validation{};
        validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        validation.pNext = original.pNext;
        validation.enabledValidationFeatureCount = 1;
        validation.pEnabledValidationFeatures = &enabled;
        debug.pNext = &validation;

        auto info = original;
        info.pNext = &debug;
        info.enabledLayerCount = uint32_t(layers.size());
        info.ppEnabledLayerNames = layers.data();
        info.enabledExtensionCount = uint32_t(extensions.size());
        info.ppEnabledExtensionNames = extensions.data();
        const auto instance = Platform::createVkInstance(info);
        if(instance == VK_NULL_HANDLE) return instance;
        const auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            bluevk::vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        debug.pNext = nullptr;
        const auto result = create ? create(instance, &debug, nullptr, &messenger_)
                                   : VK_ERROR_EXTENSION_NOT_PRESENT;
        if(result != VK_SUCCESS) {
            utils::slog.e << "Vulkan validation messenger failed: " << int32_t(result)
                          << utils::io::endl;
            const auto destroy = reinterpret_cast<PFN_vkDestroyInstance>(
                bluevk::vkGetInstanceProcAddr(instance, "vkDestroyInstance"));
            destroy(instance, nullptr);
            return VK_NULL_HANDLE;
        }
        utils::slog.i << "Vulkan validation active: object lifetime + synchronization; queue wait OFF"
                      << utils::io::endl;
        return instance;
    }

private:
    static void append(std::vector<const char*>& names, const char* name) {
        if(std::none_of(names.begin(), names.end(), [name](const char* item) {
            return std::strcmp(item, name) == 0;
        })) names.push_back(name);
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL report(VkDebugUtilsMessageSeverityFlagBitsEXT,
            VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*) {
        utils::slog.e << "Vulkan validation: " << data->pMessage << utils::io::endl;
        return VK_FALSE;
    }
    VkDebugUtilsMessengerEXT messenger_ = VK_NULL_HANDLE;
};
}
