#!/usr/bin/env python3
"""Build one corrected Filament translation unit against the unchanged pinned SDK ABI."""
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
                FVK_LOGI << "Vulkan pipeline eviction: GPU completion confirmed";
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
    print('Prepared pinned Filament 1.78.1 pipeline eviction correction')
