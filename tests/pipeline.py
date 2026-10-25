#!/usr/bin/env python3
"""Exercise the actual generated eviction body with pending-GPU and failure cases."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / '.deps/pipeline/Pipeline.cpp').read_text()
start = source.index('void VulkanPipelineCache::gc() noexcept {')
end = source.index('\nbool VulkanPipelineCache::PipelineEqual', start)
# Use a throwing check in the harness to observe the production fatal-error path.
body = source[start:end].replace(' noexcept', '')
harness = r'''
#include <tsl/robin_map.h>
#include <cassert>
#include <cstdint>
#include <stdexcept>
using VkResult = int;
constexpr int VK_SUCCESS=0, VKALLOC=0, FVK_MAX_PIPELINE_AGE=45;
int waits=0, destroys=0, waitResult=0;
bool completed=false;
struct Sink { template<class T> Sink& operator<<(T const&) { return *this; } } sink;
#define FVK_LOGI sink
#define FILAMENT_CHECK_POSTCONDITION(ok) if(!(ok)) throw std::runtime_error("wait failed"); else sink
int vkDeviceWaitIdle(int) { ++waits; completed=waitResult==0; return waitResult; }
void vkDestroyPipeline(int,int,int) { assert(completed); ++destroys; }
class VulkanPipelineCache {
public:
 struct PipelineCacheEntry { int handle; uint32_t lastUsed; };
 tsl::robin_map<int,PipelineCacheEntry> mPipelines;
 uint32_t mCurrentTime=100;
 int mDevice=0;
 void resetBoundPipeline() {}
 void gc();
};
'''
harness += body + r'''
int main() {
 VulkanPipelineCache hot;
 hot.mPipelines.emplace(1,VulkanPipelineCache::PipelineCacheEntry{1,100});
 hot.gc(); assert(waits==0 && destroys==0 && hot.mPipelines.size()==1);
 VulkanPipelineCache cold;
 for(int i=0;i<9;++i) cold.mPipelines.emplace(i,VulkanPipelineCache::PipelineCacheEntry{i,0});
 cold.gc(); assert(waits==1 && destroys==9 && cold.mPipelines.empty());
 cold.gc(); assert(waits==1 && destroys==9);
 VulkanPipelineCache failure;
 failure.mPipelines.emplace(1,VulkanPipelineCache::PipelineCacheEntry{1,0});
 waitResult=-4; completed=false;
 try { failure.gc(); assert(false); } catch(std::runtime_error const&) {}
 assert(waits==2 && destroys==9 && failure.mPipelines.size()==1);
}
'''
with tempfile.TemporaryDirectory() as directory:
    cpp = Path(directory) / 'pipeline.cpp'
    exe = Path(directory) / 'pipeline'
    cpp.write_text(harness)
    subprocess.run(['g++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-isystem', str(root / '.deps/host/filament/include'),
                    str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Pipeline eviction: no hot-cache wait, one wait per batch, no destruction on failure')
