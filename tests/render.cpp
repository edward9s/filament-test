#include "Forest.h"
#include "FpsHud.h"
#include "Effects.h"
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
// Optional Linux OpenGL smoke render. Run under Xvfb from the repository root.
// Optional first argument freezes scene time (e.g. 0.3 for a strike, 1.8 for its pause).
int main(int argc,char** argv) {
    const float seconds=argc>1?std::stof(argv[1]):.3f;
    using namespace filament;
    Engine* e=Engine::create(backend::Backend::OPENGL);
    if(!e)throw std::runtime_error("OpenGL engine unavailable");
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
        forest::Forest::configure(*v);
        DynamicResolutionOptions d;d.enabled=false;v->setDynamicResolutionOptions(d);
        std::vector<uint8_t> pixels(1280*720*4);
        for(int i=0;i<16;++i) {
            scene.animate(seconds);effects.animate(seconds,{3.07f,3.16f,9.54f});
            if(r->beginFrame(swap)) {r->render(v);hud.render(*r,1280,720);
                if(i==15)r->readPixels(0,0,1280,720,backend::PixelBufferDescriptor(pixels.data(),pixels.size(),backend::PixelDataFormat::RGBA,backend::PixelDataType::UBYTE));
                r->endFrame();
                hud.completedFrame(std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
            }
            e->flushAndWait();
        }
        if(*std::max_element(pixels.begin(),pixels.end())==0)
            throw std::runtime_error("No pixels read back from Filament");
        std::ofstream output(".generated/forest-preview.ppm",std::ios::binary);
        output<<"P6\n1280 720\n255\n";
        for(size_t i=0;i<pixels.size();i+=4)output.write(reinterpret_cast<const char*>(pixels.data()+i),3);
        if(!output)throw std::runtime_error("Cannot save preview");
    }
    e->destroy(swap);e->destroy(v);e->destroy(s);e->destroy(r);e->destroyCameraComponent(ce);
    utils::EntityManager::get().destroy(ce);Engine::destroy(&e);
}
