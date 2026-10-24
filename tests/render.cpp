#include "Forest.h"
#include "FpsHud.h"
#include "Effects.h"
#include "RendererEngine.h"
#include <chrono>
#include <filament/Camera.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <thread>
// Optional Linux smoke render. OpenGL needs Xvfb; Vulkan can use Mesa lavapipe headlessly.
// Optional first argument freezes scene time (e.g. 0.3 for a strike, 1.8 for its pause).
// Optional second argument chooses opengl (default) or vulkan.
int main(int argc,char** argv) {
    const float seconds=argc>1?std::stof(argv[1]):.3f;
    using namespace filament;
    const std::string backendName=argc>2?argv[2]:"opengl";
    if(backendName!="opengl" && backendName!="vulkan")throw std::runtime_error("Unknown renderer backend");
    const auto backendChoice=backendName=="vulkan"?backend::Backend::VULKAN:backend::Backend::OPENGL;
    Engine* e=forest::createRendererEngine(backendChoice);
    if(!e)throw std::runtime_error("Renderer engine unavailable");
    auto* r=e->createRenderer();auto* s=e->createScene();auto* v=e->createView();
    auto ce=utils::EntityManager::get().create();auto* c=e->createCamera(ce);
    auto* swap=e->createSwapChain(1280,720,SwapChain::CONFIG_READABLE);
    v->setScene(s);v->setCamera(c);v->setViewport({0,0,1280,720});
    c->setProjection(52,1280./720,.1,100,Camera::Fov::VERTICAL);
    c->setExposure(16,1.f/125,100);c->lookAt({3.07,3.16,9.54},{0,1.6,-3},{0,1,0});
    {
        forest::Forest scene(*e,*s);const auto read=[](const std::string& name){
            std::ifstream f(".generated/assets/"+name,std::ios::binary);
            if(!f)throw std::runtime_error(name);
            return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),{});
        };
        scene.load(read);
        forest::Effects effects(*e,*s);effects.load(read);
        forest::FpsHud hud(*e);hud.load(read);
        hud.setVulkan(backendChoice==backend::Backend::VULKAN);
        forest::Forest::configure(*v);
        DynamicResolutionOptions d;d.enabled=false;v->setDynamicResolutionOptions(d);
        e->flushAndWait();
        std::vector<uint8_t> pixels(1280*720*4);
        int completed=0;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(3);
        while(completed<16) {
            if(std::chrono::steady_clock::now()>deadline)throw std::runtime_error("Renderer timed out");
            scene.animate(seconds);
            if(r->beginFrame(swap)) {effects.animate(seconds,{3.07f,3.16f,9.54f});r->render(v);hud.render(*r,1280,720);
                if(completed==15)r->readPixels(0,0,1280,720,backend::PixelBufferDescriptor(pixels.data(),pixels.size(),backend::PixelDataFormat::RGBA,backend::PixelDataType::UBYTE));
                r->endFrame();
                hud.completedFrame(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
                ++completed;
                if(completed==8) {
                    e->destroy(swap);e->flushAndWait();
                    swap=e->createSwapChain(1280,720,SwapChain::CONFIG_READABLE);
                    e->flushAndWait();
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        // Keep multiple frames in flight, as the Android app does. Wait only for final readback.
        e->flushAndWait();
        if(*std::max_element(pixels.begin(),pixels.end())==0)
            throw std::runtime_error("No pixels read back from Filament");
        std::ofstream output(".generated/forest-preview.ppm",std::ios::binary);
        output<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<pixels.size();i+=4)output.write(reinterpret_cast<const char*>(pixels.data()+i),3);
        if(!output)throw std::runtime_error("Cannot save preview");
    }
    e->destroy(swap);e->destroy(v);e->destroy(s);e->destroy(r);e->destroyCameraComponent(ce);
    utils::EntityManager::get().destroy(ce);Engine::destroy(&e);
    std::cout<<backendName<<": 16 rendered frames and GPU readback passed\n";
}

