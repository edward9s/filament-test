#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Forest.h"
#include "FpsHud.h"
#include "Effects.h"
#include <filament/Camera.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <android/native_window.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <stdexcept>

namespace {
using namespace filament;
using namespace filament::math;
std::vector<uint8_t> readAsset(const std::string& path) {
    size_t size=0;
    void* data=SDL_LoadFile(path.c_str(),&size);
    if(!data) throw std::runtime_error("Cannot load "+path+": "+SDL_GetError());
    std::unique_ptr<void,decltype(&SDL_free)> owner(data,SDL_free);
    const auto* bytes=static_cast<const uint8_t*>(data);
    return {bytes,bytes+size};
}
class Demo {
public:
    explicit Demo(SDL_Window* window):window_(window) {}
    void initialize() {
        engine_=Engine::create(backend::Backend::OPENGL);
        if(!engine_) throw std::runtime_error("Filament OpenGL initialization failed");
        renderer_=engine_->createRenderer();scene_=engine_->createScene();view_=engine_->createView();
        cameraEntity_=utils::EntityManager::get().create();camera_=engine_->createCamera(cameraEntity_);
        view_->setCamera(camera_);view_->setScene(scene_);
        camera_->setExposure(16.0f,1.0f/125.0f,100.0f);
        forest::Forest::configure(*view_);
        forest_=std::make_unique<forest::Forest>(*engine_,*scene_);
        forest_->load(readAsset);
        effects_=std::make_unique<forest::Effects>(*engine_,*scene_);
        effects_->load(readAsset);
        hud_=std::make_unique<forest::FpsHud>(*engine_);
        hud_->load(readAsset);
        Renderer::FrameRateOptions rate;rate.interval=1;renderer_->setFrameRateOptions(rate);
        start_=SDL_GetTicks();
    }
    ~Demo() {
        if(!engine_)return;
        suspend();
        hud_.reset();
        effects_.reset();
        forest_.reset();
        if(view_)engine_->destroy(view_);
        if(scene_)engine_->destroy(scene_);
        if(renderer_)engine_->destroy(renderer_);
        if(camera_)engine_->destroyCameraComponent(cameraEntity_);
        utils::EntityManager::get().destroy(cameraEntity_);
        Engine::destroy(&engine_);
    }
    void suspend() {
        active_=false;fingers_.clear();
        if(hud_)hud_->reset();
        if(swapChain_) {engine_->destroy(swapChain_);swapChain_=nullptr;}
        native_=nullptr;
    }
    void event(const SDL_Event& e) {
        switch(e.type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND: case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_MINIMIZED: suspend();break;
        case SDL_EVENT_DID_ENTER_FOREGROUND: case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_RESTORED: active_=true;break;
        case SDL_EVENT_FINGER_DOWN: fingers_[e.tfinger.fingerID]={e.tfinger.x,e.tfinger.y};break;
        case SDL_EVENT_FINGER_UP: case SDL_EVENT_FINGER_CANCELED: fingers_.erase(e.tfinger.fingerID);break;
        case SDL_EVENT_FINGER_MOTION: {
            auto finger=fingers_.find(e.tfinger.fingerID);
            if(finger==fingers_.end())break;
            float oldDistance=pinchDistance();
            finger->second={e.tfinger.x,e.tfinger.y};
            if(fingers_.size()==1) orbit(e.tfinger.dx,e.tfinger.dy);
            else if(fingers_.size()==2) {
                float next=pinchDistance();
                if(next>.01f && oldDistance>.01f)distance_=std::clamp(distance_*oldDistance/next,4.f,24.f);
            }
            break;
        }
        case SDL_EVENT_MOUSE_MOTION:
            if(e.motion.which!=SDL_TOUCH_MOUSEID && (e.motion.state&SDL_BUTTON_LMASK) && width_>0 && height_>0)
                orbit(e.motion.xrel/width_,e.motion.yrel/height_);
            break;
        case SDL_EVENT_MOUSE_WHEEL: distance_=std::clamp(distance_-e.wheel.y,4.f,24.f);break;
        default: break;
        }
    }
    void frame() {
        if(!active_) {SDL_Delay(30);return;}
        auto* native=static_cast<ANativeWindow*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_),
            SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER,nullptr));
        if(native!=native_) {
            hud_->reset();
            if(swapChain_)engine_->destroy(swapChain_);
            native_=native;swapChain_=native?engine_->createSwapChain(native):nullptr;
        }
        SDL_GetWindowSizeInPixels(window_,&width_,&height_);
        if(!swapChain_ || width_<=0 || height_<=0) {hud_->reset();SDL_Delay(20);return;}
        view_->setViewport({0,0,uint32_t(width_),uint32_t(height_)});
        camera_->setProjection(52.0,double(width_)/height_,.1,100,Camera::Fov::VERTICAL);
        const double horizontal=distance_*std::cos(pitch_);
        const float3 eye{float(horizontal*std::sin(yaw_)),1.6f+distance_*std::sin(pitch_),float(-3+horizontal*std::cos(yaw_))};
        camera_->lookAt(eye,{0,1.6,-3},{0,1,0});
        const float seconds=float(SDL_GetTicks()-start_)*.001f;
        forest_->animate(seconds);
        if(renderer_->beginFrame(swapChain_)) {
            effects_->animate(seconds,eye);
            renderer_->render(view_);
            hud_->render(*renderer_,width_,height_);
            renderer_->endFrame();
            hud_->completedFrame(double(SDL_GetTicksNS())*1e-9);
        }
    }
private:
    float pinchDistance() const {
        if(fingers_.size()!=2)return 0;
        auto i=fingers_.begin();float2 a=i->second;float2 b=(++i)->second;
        // Touch coordinates are normalized separately by screen width and height.
        const float aspect=height_>0?float(width_)/height_:1.f;
        return length((a-b)*float2{aspect,1});
    }
    void orbit(float x,float y) {yaw_-=x*4;pitch_=std::clamp(pitch_+y*2,.03f,.85f);}
    SDL_Window* window_;
    Engine* engine_=nullptr;
    Renderer* renderer_=nullptr;
    Scene* scene_=nullptr;
    View* view_=nullptr;
    Camera* camera_=nullptr;
    SwapChain* swapChain_=nullptr;
    ANativeWindow* native_=nullptr;
    utils::Entity cameraEntity_{};
    std::unique_ptr<forest::Forest> forest_;
    std::unique_ptr<forest::Effects> effects_;
    std::unique_ptr<forest::FpsHud> hud_;
    std::map<SDL_FingerID,float2> fingers_;
    float yaw_=.24f,pitch_=.12f,distance_=13;
    int width_=0,height_=0;
    bool active_=true;
    uint64_t start_=0;
};
}
int main(int,char**) {
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)) {SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",SDL_GetError());return 1;}
    SDL_Window* window=SDL_CreateWindow("Forest Ruins · Filament",1280,720,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!window) {SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",SDL_GetError());SDL_Quit();return 1;}
    int result=0;
    try {
        Demo demo(window);demo.initialize();bool running=true;
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_KEY_DOWN &&
                    (event.key.key==SDLK_ESCAPE || event.key.key==SDLK_AC_BACK)))running=false;
                demo.event(event);
            }
            if(running)demo.frame();
        }
    } catch(const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Forest scene failed: %s",error.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Forest scene failed",error.what(),window);
        result=1;
    }
    SDL_DestroyWindow(window);SDL_Quit();return result;
}
