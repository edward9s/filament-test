#!/usr/bin/env python3
"""Prepare corrected Filament translation units with the pinned SDK ABI unchanged."""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / '.deps/pipeline'


def patch(source: str) -> str:
    start = source.index('void VulkanPipelineCache::gc() noexcept {')
    end = source.index('\nbool VulkanPipelineCache::PipelineEqual', start)
    # Keep the class layout, pipeline cache policy and all other SDK code unchanged.
    body = '''void VulkanPipelineCache::gc() noexcept {
    ++mCurrentTime;
    resetBoundPipeline();

    // Cache age is an eviction policy, not proof that GPU execution has finished.
    // Wait once per eviction batch, only when we will actually destroy a pipeline.
    bool completed = false;
    using ConstPipeIterator = decltype(mPipelines)::const_iterator;
    for (ConstPipeIterator iter = mPipelines.begin(); iter != mPipelines.end();) {
        const PipelineCacheEntry& cacheEntry = iter.value();
        if (cacheEntry.lastUsed + FVK_MAX_PIPELINE_AGE < mCurrentTime) {
            if (!completed) {
                const VkResult result = vkDeviceWaitIdle(mDevice);
                FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
                        << "Pipeline eviction wait failed: " << int32_t(result);
                completed = true;
            }
            vkDestroyPipeline(mDevice, iter->second.handle, VKALLOC);
            iter = mPipelines.erase(iter);
        } else {
            ++iter;
        }
    }
}
'''
    return source[:start] + body + source[end:]


def replace(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError(f'Unexpected pinned Filament source: {old[:80]}')
    return source.replace(old, new, 1)


def patch_platform(source: str) -> str:
    source = replace(source, 'props.queueCount != 0 && props.queueFlags & flags',
                     'props.queueCount != 0 && (props.queueFlags & flags) == flags')
    source = replace(source,
        '    if (mImpl->mDevice == VK_NULL_HANDLE) {\n        createLogicalDeviceAndQueues',
        '''    // Protected and ordinary queues in the same family share its total capacity.
    const uint32_t reserved = context.mProtectedMemorySupported &&
            mImpl->mProtectedGraphicsQueueFamilyIndex == mImpl->mGraphicsQueueFamilyIndex ? 1u : 0u;
    FILAMENT_CHECK_PRECONDITION(graphicsQueuesCount >= 1u + reserved)
            << "Queue family cannot provide ordinary and protected graphics queues";
    const bool hasAsyncQueue = graphicsQueuesCount > 1u + reserved;

    if (mImpl->mDevice == VK_NULL_HANDLE) {
        createLogicalDeviceAndQueues''')
    source = replace(source, 'requestedFeatures, graphicsQueuesCount > 1);',
                     'requestedFeatures, hasAsyncQueue);')
    source = replace(source,
        '''    vkGetDeviceQueue(mImpl->mDevice, mImpl->mGraphicsQueueFamilyIndex, mImpl->mGraphicsQueueIndex,
            &mImpl->mGraphicsQueue);''',
        '''    VkDeviceQueueInfo2 graphicsInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
        .flags = 0,
        .queueFamilyIndex = mImpl->mGraphicsQueueFamilyIndex,
        .queueIndex = mImpl->mGraphicsQueueIndex,
    };
    vkGetDeviceQueue2(mImpl->mDevice, &graphicsInfo, &mImpl->mGraphicsQueue);''')
    source = replace(source, '    if (graphicsQueuesCount > 1) {', '    if (hasAsyncQueue) {')
    source = replace(source,
        '''        vkGetDeviceQueue(mImpl->mDevice, mImpl->mGraphicsQueueFamilyIndex, mImpl->mGraphicsQueueIndex + 1,
            &mImpl->mAsyncQueue);''',
        '''        graphicsInfo.queueIndex = mImpl->mGraphicsQueueIndex + 1;
        vkGetDeviceQueue2(mImpl->mDevice, &graphicsInfo, &mImpl->mAsyncQueue);''')
    source = replace(source,
        '    deviceQueueCreateInfo[1].flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;',
        '''    deviceQueueCreateInfo[1].flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    deviceQueueCreateInfo[1].queueFamilyIndex = mImpl->mProtectedGraphicsQueueFamilyIndex;
    deviceQueueCreateInfo[1].queueCount = 1;''')
    source = replace(source,
        '        .multiview = vk11Features.multiview,\n#if defined(__ANDROID__)',
        '        .multiview = vk11Features.multiview,\n        .protectedMemory = hasProtectedQueue ? VK_TRUE : VK_FALSE,\n#if defined(__ANDROID__)')
    start = source.index('    VkPhysicalDeviceMultiviewFeaturesKHR multiview = {',
                         source.index('void VulkanPlatform::createLogicalDeviceAndQueues'))
    end = source.index('    VkPhysicalDeviceGlobalPriorityQueryFeaturesKHR globalPriority', start)
    source = source[:start] + source[end:]
    return source


def fetch(item):
    name, digest = item
    target = DEST / name
    data = target.read_bytes() if target.exists() else b''
    if hashlib.sha256(data).hexdigest() != digest:
        url = f'https://raw.githubusercontent.com/google/filament/v1.78.1/{name}'
        with urllib.request.urlopen(url, timeout=120) as response:
            data = response.read()
    if hashlib.sha256(data).hexdigest() != digest:
        raise RuntimeError(f'Filament source checksum mismatch: {name}')
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(data)


if __name__ == '__main__':
    manifest = json.loads((ROOT / 'scripts/pipeline-files.json').read_text())
    with ThreadPoolExecutor(max_workers=12) as pool:
        list(pool.map(fetch, manifest.items()))
    original = DEST / 'filament/backend/src/vulkan/VulkanPipelineCache.cpp'
    (DEST / 'Pipeline.cpp').write_text(patch(original.read_text()))
    original = DEST / 'filament/backend/src/vulkan/platform/VulkanPlatform.cpp'
    (DEST / 'Platform.cpp').write_text(patch_platform(original.read_text()))
    print('Prepared pinned Filament 1.78.1 pipeline and device corrections')
