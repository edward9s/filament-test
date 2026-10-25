#!/usr/bin/env python3
"""Validate the generated device-create function using captured Vulkan requests."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / '.deps/pipeline/Platform.cpp').read_text()
a = s.index('void VulkanPlatform::createLogicalDeviceAndQueues(')
b = s.index('\nbool VulkanPlatform::isPresentationTimeSupported', a)
body = s[a:b]
a = s.index('uint32_t identifyGraphicsQueueFamilyIndex(')
b = s.index('\n// Provide a preference ordering', a)
selector = s[a:b]
harness = r'''
#include <bluevk/BlueVK.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cassert>
#include <cstdint>
constexpr uint32_t INVALID_VK_INDEX=0xffffffff;
using ExtensionSet=std::vector<std::string>;
template<class T> using FixedCapacityVector=std::vector<T>;
#define assert_invariant(x) assert(x)
struct Platform { enum class GpuContextPriority { DEFAULT }; };
VkQueueGlobalPriorityKHR getVkQueueGlobalPriority(Platform::GpuContextPriority) { return VK_QUEUE_GLOBAL_PRIORITY_MEDIUM_KHR; }
bool setContains(ExtensionSet const& s, char const* k) { return std::find(s.begin(),s.end(),k)!=s.end(); }
template<class A,class B> void chainStruct(A* a,B* b) { b->pNext=const_cast<void*>(a->pNext); a->pNext=b; }
uint32_t protectedFamily=0;
FixedCapacityVector<VkQueueFamilyProperties> getPhysicalDeviceQueueFamilyPropertiesHelper(VkPhysicalDevice) {
 VkQueueFamilyProperties normal{}; normal.queueFlags=VK_QUEUE_GRAPHICS_BIT; normal.queueCount=3;
 VkQueueFamilyProperties secure=normal; secure.queueFlags|=VK_QUEUE_PROTECTED_BIT;
 if(protectedFamily==0) return {secure};
 return {normal,secure};
}
__SELECTOR__
class VulkanPlatform {
public:
 struct MiscDeviceFeatures { bool dynamicRendering=false,imageView2Don3DImage=false; Platform::GpuContextPriority gpuContextPriority=Platform::GpuContextPriority::DEFAULT; };
 struct Context {
 VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT mVertexInputDynamicStateFeatures{};
 VkPhysicalDeviceExtendedDynamicStateFeaturesEXT mExtendedDynamicStateFeatures{};
 VkPhysicalDeviceExtendedDynamicState2FeaturesEXT mExtendedDynamicState2Features{};
 VkPhysicalDeviceColorWriteEnableFeaturesEXT mColorWriteEnableFeatures{};
 };
 struct Impl { VkPhysicalDevice mPhysicalDevice{}; VkDevice mDevice{};
 uint32_t mGraphicsQueueFamilyIndex=INVALID_VK_INDEX,mGraphicsQueueIndex=INVALID_VK_INDEX,mProtectedGraphicsQueueFamilyIndex=INVALID_VK_INDEX,mProtectedGraphicsQueueIndex=INVALID_VK_INDEX;
 Context mContext;
 } storage;
 Impl* mImpl=&storage;
 bool expectProtected=false; uint32_t capacity=3;
 VkDevice createVkDevice(VkDeviceCreateInfo const& info) {
  unsigned normal=0,protectedCount=0;
  for(unsigned i=0;i<info.queueCreateInfoCount;++i) {
   auto const& q=info.pQueueCreateInfos[i];
   if(q.flags & VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT) { protectedCount+=q.queueCount; assert(q.queueFamilyIndex==protectedFamily); }
   else { normal+=q.queueCount; assert(q.queueFamilyIndex==0); }
  }
  assert(protectedCount==(expectProtected?1u:0u));
  assert(normal+(protectedFamily==0?protectedCount:0)<=capacity);
  bool found=false;
  for(auto p=static_cast<VkBaseInStructure const*>(info.pNext);p;p=p->pNext) {
   assert(p->sType!=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROTECTED_MEMORY_FEATURES);
   assert(p->sType!=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES);
   if(p->sType==VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES) {
    found=true; auto v=reinterpret_cast<VkPhysicalDeviceVulkan11Features const*>(p);
    assert(bool(v->protectedMemory)==expectProtected); assert(v->multiview);
   }
  }
  assert(found); return VK_NULL_HANDLE;
 }
 void createLogicalDeviceAndQueues(ExtensionSet const&,VkPhysicalDeviceFeatures const&,VkPhysicalDeviceVulkan11Features const&,bool,MiscDeviceFeatures const&,bool) noexcept;
};
'''
harness = harness.replace('__SELECTOR__', selector)
harness += body + r'''
int main() {
 VkPhysicalDeviceFeatures features{};
 VkPhysicalDeviceVulkan11Features vk11{}; vk11.multiview=VK_TRUE;
 for(unsigned count=1;count<=3;++count) {
  VulkanPlatform p; p.capacity=count;
  p.createLogicalDeviceAndQueues({},features,vk11,false,{},count>1);
 }
 for(unsigned count=2;count<=3;++count) {
  VulkanPlatform p; p.capacity=count; p.expectProtected=true;
  p.createLogicalDeviceAndQueues({},features,vk11,true,{},count>2);
 }
 protectedFamily=1;
 VulkanPlatform p; p.capacity=2; p.expectProtected=true;
 p.createLogicalDeviceAndQueues({VK_KHR_MULTIVIEW_EXTENSION_NAME},features,vk11,true,{},true);
}
'''
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'device.cpp'; exe=Path(d)/'device';cpp.write_text(harness)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-isystem',str(root/'.deps/host/filament/include'),'-isystem',str(root/'.deps/headers/include'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Device requests: queue capacity/family/count and promoted feature chain checked')
