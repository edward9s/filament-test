#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "Forest.h"
#include "Benchmark.h"
#include "FpsHud.h"
#include "Effects.h"
#include "RendererEngine.h"
#include "SwapPlatform.h"
#if FOREST_VULKAN_VALIDATION
#include "ValidationPlatform.h"
#endif
#include <backend/platforms/VulkanPlatformAndroid.h>
#include <filament/Camera.h>
#include <filament/Renderer.h>
#include <filament/SwapChain.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>
#include <android/native_window.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

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
struct CameraOrbit { float yaw=.24f, pitch=.12f, distance=13.f; };
class FrameStats {
public:
    void reset() {*this=FrameStats{};}
    void skipped() {++skips_;}
    void submitted(uint64_t now,uint64_t cpu) {
        if(!start_)start_=now;
        if(previous_)intervals_[count_++%intervals_.size()]=double(now-previous_)*1e-6;
        previous_=now;++frames_;cpuNs_+=cpu;
    }
    void report(Renderer& renderer,View& view,bool vulkan,int width,int height,float refreshRate) {
        if(!start_ || previous_-start_<2'000'000'000ull)return;
        const auto samples=std::min(count_,intervals_.size());
        auto sorted=intervals_;
        std::sort(sorted.begin(),sorted.begin()+samples);
        const auto history=renderer.getFrameInfoHistory(1);
        char main[32]="n/a",backend[32]="n/a",gpu[32]="n/a";
        uint32_t frameId=0;
        if(!history.empty()) {
            const auto& frame=history[0];frameId=frame.frameId;
            if(frame.beginFrame>0 && frame.endFrame>=frame.beginFrame)formatMs(main,frame.endFrame-frame.beginFrame);
            if(frame.backendBeginFrame>0 && frame.backendEndFrame>=frame.backendBeginFrame)
                formatMs(backend,frame.backendEndFrame-frame.backendBeginFrame);
            // Some drivers expose no GPU timer; zero is not a measured zero-cost frame.
            if(frame.gpuFrameDuration>0)formatMs(gpu,frame.gpuFrameDuration);
        }
        const auto scale=view.getLastDynamicResolutionScale();
        SDL_Log("ForestPerf backend=%s mode=%s surface=%dx%d scale=%.3fx%.3f refresh-hz=%.2f "
            "submitted-fps=%.2f interval-p50-ms=%.3f interval-p95-ms=%.3f cpu-submit-average-ms=%.3f "
            "skipped-attempts=%llu latest-frame=%u filament-main-ms=%s backend-ms=%s gpu-ms=%s",
            vulkan?"Vulkan":"OpenGL-ES",FOREST_FIXED_RESOLUTION?"fixed100":"dynamic",width,height,
            double(scale.x),double(scale.y),double(refreshRate),
            double(frames_-1)*1e9/double(previous_-start_),
            samples?sorted[(samples-1)/2]:0.,samples?sorted[size_t(std::ceil(samples*.95))-1]:0.,
            double(cpuNs_)*1e-6/double(frames_),static_cast<unsigned long long>(skips_),frameId,main,backend,gpu);
        reset();
    }
private:
    static void formatMs(char (&out)[32],int64_t ns) {std::snprintf(out,sizeof(out),"%.3f",double(ns)*1e-6);}
    std::array<double,512> intervals_{};
    size_t count_=0;
    uint64_t start_=0,previous_=0,frames_=0,skips_=0,cpuNs_=0;
};
class Demo {
public:
    Demo(SDL_Window* window, backend::Backend backend, CameraOrbit orbit, uint64_t start)
        : window_(window), backend_(backend), yaw_(orbit.yaw), pitch_(orbit.pitch),
          distance_(orbit.distance), start_(start) {}
    CameraOrbit cameraOrbit() const { return {yaw_,pitch_,distance_}; }
    bool consumeBackendSwitch() { return std::exchange(switchRequested_,false); }
    void initialize() {
        SDL_Log("Initializing Filament %s", backend_==backend::Backend::VULKAN?"Vulkan":"OpenGL ES");
        if(backend_==backend::Backend::VULKAN) {
            SDL_Log("Vulkan GPU synchronization diagnostic: %s",FOREST_VULKAN_SYNC?"ON":"OFF");
            #if FOREST_VULKAN_VALIDATION
            SDL_Log("Vulkan validation diagnostic: ON (queue wait OFF)");
            platform_=std::make_unique<forest::ValidationPlatform<backend::VulkanPlatformAndroid>>();
            #else
            platform_=std::make_unique<forest::SwapPlatform<backend::VulkanPlatformAndroid>>(
                bool(FOREST_VULKAN_SYNC));
            #endif
        }
        engine_=forest::createRendererEngine(backend_,platform_.get());
        if(!engine_) throw std::runtime_error(backend_==backend::Backend::VULKAN
            ? "Filament Vulkan initialization failed (check Android Vulkan support and SDK)"
            : "Filament OpenGL ES initialization failed");
        renderer_=engine_->createRenderer();scene_=engine_->createScene();view_=engine_->createView();
        cameraEntity_=utils::EntityManager::get().create();camera_=engine_->createCamera(cameraEntity_);
        view_->setCamera(camera_);view_->setScene(scene_);
        camera_->setExposure(16.0f,1.0f/125.0f,100.0f);
        forest::Forest::configure(*view_);
        if(FOREST_FIXED_RESOLUTION) {
            auto resolution=view_->getDynamicResolutionOptions();resolution.enabled=false;
            view_->setDynamicResolutionOptions(resolution);
        }
        forest_=std::make_unique<forest::Forest>(*engine_,*scene_);
        forest_->load(readAsset);
        effects_=std::make_unique<forest::Effects>(*engine_,*scene_);
        effects_->load(readAsset);
        hud_=std::make_unique<forest::FpsHud>(*engine_);
        hud_->load(readAsset);
        hud_->setVulkan(backend_==backend::Backend::VULKAN);
        Renderer::FrameRateOptions rate;rate.interval=1;renderer_->setFrameRateOptions(rate);
        updateDisplayInfo();
        SDL_Log("Resolution mode: %s",FOREST_FIXED_RESOLUTION?"fixed 100%":"dynamic 65-100% per axis");
        engine_->flushAndWait();
        SDL_Log("Filament %s scene uploads complete (material UBO batching %s)",
            backend_==backend::Backend::VULKAN?"Vulkan":"OpenGL ES",
            backend_==backend::Backend::VULKAN?"enabled":"default");
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
        invalidateBenchmark("application paused or Surface hidden");
        active_=false;fingers_.clear();hudFingers_.clear();mouseHudDown_=false;
        if(hud_)hud_->reset();
        stats_.reset();
        releaseSwapChain();
    }
    void event(const SDL_Event& e) {
        if(benchmarkActive_ && (e.type==SDL_EVENT_FINGER_DOWN || e.type==SDL_EVENT_FINGER_UP || e.type==SDL_EVENT_FINGER_MOTION || e.type==SDL_EVENT_MOUSE_BUTTON_DOWN || e.type==SDL_EVENT_MOUSE_BUTTON_UP || e.type==SDL_EVENT_MOUSE_MOTION || e.type==SDL_EVENT_MOUSE_WHEEL || e.type==SDL_EVENT_KEY_DOWN))return;
        switch(e.type) {
        case SDL_EVENT_WILL_ENTER_BACKGROUND: case SDL_EVENT_WINDOW_HIDDEN:
        case SDL_EVENT_WINDOW_MINIMIZED: suspend();break;
        case SDL_EVENT_DID_ENTER_FOREGROUND: case SDL_EVENT_WINDOW_SHOWN:
        case SDL_EVENT_WINDOW_RESTORED: active_=true;updateDisplayInfo();break;
        case SDL_EVENT_WINDOW_DISPLAY_CHANGED: case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
            updateDisplayInfo();break;
        case SDL_EVENT_FINGER_DOWN:
            if(hud_->contains(e.tfinger.x,e.tfinger.y,width_,height_))
                hudFingers_.insert(e.tfinger.fingerID);
            else fingers_[e.tfinger.fingerID]={e.tfinger.x,e.tfinger.y};
            break;
        case SDL_EVENT_FINGER_UP:
            if(hudFingers_.erase(e.tfinger.fingerID) &&
               hud_->contains(e.tfinger.x,e.tfinger.y,width_,height_))
                switchRequested_=true;
            fingers_.erase(e.tfinger.fingerID);
            break;
        case SDL_EVENT_FINGER_CANCELED:
            hudFingers_.erase(e.tfinger.fingerID);
            fingers_.erase(e.tfinger.fingerID);
            break;
        case SDL_EVENT_FINGER_MOTION: {
            auto finger=fingers_.find(e.tfinger.fingerID);
            if(finger==fingers_.end())break;
            float oldDistance=pinchDistance();
            finger->second={e.tfinger.x,e.tfinger.y};
            if(fingers_.size()==1) orbit(e.tfinger.dx,e.tfinger.dy);
            else if(fingers_.size()==2) {
                float next=pinchDistance();
                if(next>.01f && oldDistance>.01f) {
                    distance_=std::clamp(distance_*oldDistance/next,4.f,24.f);cameraDirty_=true;
                }
            }
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if(e.button.which!=SDL_TOUCH_MOUSEID && e.button.button==SDL_BUTTON_LEFT)
                mouseHudDown_=mouseOnHud(e.button.x,e.button.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if(e.button.which!=SDL_TOUCH_MOUSEID && e.button.button==SDL_BUTTON_LEFT) {
                if(mouseHudDown_ && mouseOnHud(e.button.x,e.button.y)) switchRequested_=true;
                mouseHudDown_=false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if(e.motion.which!=SDL_TOUCH_MOUSEID && !mouseHudDown_ &&
               (e.motion.state&SDL_BUTTON_LMASK) && width_>0 && height_>0)
                orbit(e.motion.xrel/width_,e.motion.yrel/height_);
            break;
        case SDL_EVENT_KEY_DOWN:
            if(!e.key.repeat && e.key.key==SDLK_V) switchRequested_=true;
            break;
        case SDL_EVENT_MOUSE_WHEEL: distance_=std::clamp(distance_-e.wheel.y,4.f,24.f);cameraDirty_=true;break;
        default: break;
        }
    }
    void frame() {
        if(!active_) {SDL_Delay(30);return;}
        auto* native=static_cast<ANativeWindow*>(SDL_GetPointerProperty(SDL_GetWindowProperties(window_),
            SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER,nullptr));
        if(native!=native_) {
            invalidateBenchmark("native Surface recreated");
            hud_->reset();
            stats_.reset();
            releaseSwapChain();
            if(native) {
                ANativeWindow_acquire(native);
                native_=native;
                swapChain_=engine_->createSwapChain(native_);
                engine_->flushAndWait();
                SDL_Log("Filament %s swapchain ready: %dx%d, native format %d",
                    backend_==backend::Backend::VULKAN?"Vulkan":"OpenGL ES",
                    ANativeWindow_getWidth(native_),ANativeWindow_getHeight(native_),
                    ANativeWindow_getFormat(native_));
            }
        }
        int width=0,height=0;
        SDL_GetWindowSizeInPixels(window_,&width,&height);
        if(width!=width_ || height!=height_) {
            invalidateBenchmark("Surface dimensions changed");
            width_=width;height_=height;
            if(width_>0 && height_>0) {
                view_->setViewport({0,0,uint32_t(width_),uint32_t(height_)});
                camera_->setProjection(52.0,double(width_)/height_,.1,100,Camera::Fov::VERTICAL);
            }
            hud_->reset();stats_.reset();
        }
        if(!swapChain_ || width_<=0 || height_<=0) {hud_->reset();SDL_Delay(20);return;}
        benchmarkCommand();
        const uint64_t attemptStart=SDL_GetTicksNS();
        const auto waitStart=benchmarkActive_?benchmark::now():0;
        if(!renderer_->beginFrame(swapChain_)) {
            stats_.skipped();
            // Back off only rejected attempts. Never add a wait to a rendered frame.
            SDL_Delay(1);return;
        }
        const auto beginDone=benchmarkActive_?benchmark::now():0;
        const double horizontal=distance_*std::cos(pitch_);
        const float3 eye{float(horizontal*std::sin(yaw_)),1.6f+distance_*std::sin(pitch_),float(-3+horizontal*std::cos(yaw_))};
        if(cameraDirty_) {camera_->lookAt(eye,{0,1.6,-3},{0,1,0});cameraDirty_=false;}
        const float seconds=benchmarkActive_ ? (reference_?38.52f:float(benchmark::now()-animationOrigin_)*1e-9f) : float(SDL_GetTicks()-start_)*.001f;
        const auto sceneStart=benchmarkActive_?benchmark::now():0;
        forest_->animate(seconds);
        const auto effectsStart=benchmarkActive_?benchmark::now():0;
        effects_->animate(seconds,eye);
        const auto renderStart=benchmarkActive_?benchmark::now():0;
        renderer_->render(view_);
        hud_->render(*renderer_,width_,height_);
        renderer_->endFrame();
        const uint64_t submitted=SDL_GetTicksNS();
        hud_->completedFrame(double(submitted)*1e-9);
        if(benchmarkActive_) {
            const auto completed=benchmark::now();
            benchmarkFrame(completed,{0,0,seconds,double(effectsStart-sceneStart)*1e-6,double(renderStart-effectsStart)*1e-6,double(completed-renderStart)*1e-6,double(beginDone-waitStart)*1e-6});
        } else {
            stats_.submitted(submitted,submitted-attemptStart);
            stats_.report(*renderer_,*view_,backend_==backend::Backend::VULKAN,width_,height_,refreshRate_);
        }
    }
private:

    struct SavedBenchmark {
        CameraOrbit camera;
        DynamicResolutionOptions resolution;
        TemporalAntiAliasingOptions taa;
        MultiSampleAntiAliasingOptions msaa;
        BloomOptions bloom;
        AntiAliasing aa;
        bool shadows;
    } saved_;
    bool benchmarkActive_=false,reference_=false;
    unsigned preset_=0,drawn_=0;
    std::map<uint32_t,uint8_t> observedHistory_;
    int64_t animationOrigin_=0,startedAt_=0;
    benchmark::Run run_;
    std::string runId_,device_,thermal_,gpu_="null",driver_="null",driverInfo_="null";
    std::string root() const {return std::string(SDL_GetAndroidInternalStoragePath())+"/benchmarks";}
    void benchmarkCommand() {
        const int command=benchmark::command.exchange(0);
        if(command==10 && benchmarkActive_ && reference_){restoreBenchmark();return;}
        if(command==90 || command==98 || command==99) {
            invalidateBenchmark(command==90?"cancelled by user":command==98?"display refresh rate changed":"application paused");return;
        }
        if(command<1 || command>9 || benchmarkActive_)return;
        if(FOREST_VULKAN_SYNC || FOREST_VULKAN_VALIDATION) {
            benchmark::status=4000;SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Benchmark unavailable","Build with Vulkan synchronization and validation diagnostics OFF.",window_);return;
        }
        saved_={cameraOrbit(),view_->getDynamicResolutionOptions(),view_->getTemporalAntiAliasingOptions(),
            view_->getMultiSampleAntiAliasingOptions(),view_->getBloomOptions(),view_->getAntiAliasing(),view_->isShadowingEnabled()};
        preset_=command==9?0:unsigned(command-1);reference_=command==9;
        benchmarkActive_=true;benchmark::repetition=1;
        fingers_.clear();hudFingers_.clear();mouseHudDown_=false;switchRequested_=false;
        beginBenchmarkRun();
    }
    void beginBenchmarkRun() {
        run_.start();startedAt_=benchmark::unixMs();
        runId_="filament-"+std::to_string(startedAt_)+"-"+std::to_string(benchmark::now());
        const std::string storage=SDL_GetAndroidInternalStoragePath();
        if(backend_==backend::Backend::VULKAN) {
            auto value=[&](backend::Platform::DeviceInfoType type){const auto v=engine_->getPlatform()->getDeviceInfo(type,nullptr);return v.empty()?std::string("null"):benchmark::quote(v.c_str());};
            gpu_=value(backend::Platform::DeviceInfoType::VULKAN_DEVICE_NAME);
            driver_=value(backend::Platform::DeviceInfoType::VULKAN_DRIVER_NAME);
            driverInfo_=value(backend::Platform::DeviceInfoType::VULKAN_DRIVER_INFO);
        }
        device_=benchmark::read(storage+"/benchmark-device.json");
        thermal_=benchmark::read(storage+"/benchmark-thermal.json");
        yaw_=.24f;pitch_=.12f;distance_=13;cameraDirty_=true;
        auto resolution=saved_.resolution;resolution.enabled=preset_==6;
        if(preset_==6){resolution.minScale={.65f,.65f};resolution.maxScale={.65f,.65f};resolution.homogeneousScaling=true;}
        view_->setDynamicResolutionOptions(resolution);
        view_->setShadowingEnabled(preset_!=3);
        auto taa=saved_.taa;if(preset_==4)taa.enabled=false;view_->setTemporalAntiAliasingOptions(taa);
        auto msaa=saved_.msaa;if(preset_==4)msaa.enabled=false;view_->setMultiSampleAntiAliasingOptions(msaa);
        view_->setAntiAliasing(preset_==4?AntiAliasing::NONE:saved_.aa);
        auto bloom=saved_.bloom;bloom.enabled=preset_!=5;view_->setBloomOptions(bloom);
        effects_->setEnabled(preset_!=7);
        animationOrigin_=benchmark::now();drawn_=0;observedHistory_.clear();benchmark::status=7000;
    }
    void restoreBenchmark() {
        view_->setDynamicResolutionOptions(saved_.resolution);view_->setTemporalAntiAliasingOptions(saved_.taa);
        view_->setMultiSampleAntiAliasingOptions(saved_.msaa);view_->setAntiAliasing(saved_.aa);
        view_->setBloomOptions(saved_.bloom);view_->setShadowingEnabled(saved_.shadows);effects_->setEnabled(true);
        yaw_=saved_.camera.yaw;pitch_=saved_.camera.pitch;distance_=saved_.camera.distance;cameraDirty_=true;
        benchmarkActive_=false;reference_=false;run_.frames.clear();run_.metrics.clear();stats_.reset();hud_->reset();
    }
    std::string identity() const {
        return "\"schema_version\":\"1.0.0\",\"run_id\":"+benchmark::quote(runId_)+",\"preset_id\":"+benchmark::quote(benchmark::presets[preset_])+",\"valid\":"+benchmark::boolean(run_.valid)+",\"invalid_reason\":"+(run_.reason.empty()?"null":benchmark::quote(run_.reason));
    }
    std::string metadata() const {
        using benchmark::boolean;using benchmark::quote;using benchmark::metric;
        const auto scale=view_->getLastDynamicResolutionScale();
        const auto msaa=view_->getMultiSampleAntiAliasingOptions();
        const auto actualMsaa=msaa.enabled?unsigned(msaa.sampleCount):1u;
        std::string engine=benchmark::engineJson;
        #ifndef NDEBUG
        const auto build=engine.find("\"build\": \"release\"");
        if(build!=std::string::npos)engine.replace(build,18,"\"build\": \"debug\"");
        #endif
        std::ostringstream out;out<<std::setprecision(12)<<"{"<<identity()<<",\"repetition\":"<<benchmark::repetition.load()<<",\"started_at\":"<<startedAt_
            <<",\"warmup_seconds\":30,\"measure_seconds\":60,\"engine\":"<<engine<<",\"scene\":"<<benchmark::sceneJson
            <<",\"device\":"<<device_<<",\"environment\":{\"backend\":"<<quote(backend_==backend::Backend::VULKAN?"Vulkan":"OpenGL ES")
            <<",\"surface_size\":["<<width_<<','<<height_<<"],\"render_size\":["<<std::lround(width_*scale.x)<<','<<std::lround(height_*scale.y)
            <<"],\"dynamic_resolution\":false,\"gpu_profiling\":"<<boolean(preset_==1)<<",\"vsync_requested\":\"engine_default\",\"presentation_mode\":"<<(backend_==backend::Backend::VULKAN?"\"FIFO\"":"null")
            <<",\"refresh_limited_possible\":true,\"refresh_hz\":"<<(refreshRate_>0?std::to_string(refreshRate_):"null")<<",\"gpu\":"<<gpu_<<",\"driver\":"<<driver_<<",\"driver_info\":"<<driverInfo_<<",\"thermal_status\":"<<thermal_<<"},\"settings\":{\"pipeline\":\"Filament native\",\"msaa_samples\":"<<actualMsaa
            <<",\"taa\":"<<boolean(view_->getTemporalAntiAliasingOptions().enabled)<<",\"fxaa\":"<<boolean(view_->getAntiAliasing()==AntiAliasing::FXAA)
            <<",\"shadows\":"<<boolean(view_->isShadowingEnabled())<<",\"bloom\":"<<boolean(view_->getBloomOptions().enabled)<<",\"effects\":"<<boolean(preset_!=7)
            <<",\"shadow_distance_m\":65,\"shadow_cascades\":3,\"shadow_map_size\":2048,\"ssao\":"<<boolean(view_->getAmbientOcclusionOptions().enabled)<<",\"fog\":"<<boolean(view_->getFogOptions().enabled)
            <<",\"fixed_scale_via_dynamic_resolution_api\":"<<boolean(preset_==6)<<"},\"metrics\":{"
            <<"\"cpu_scene_ms\":"<<metric("cpu","steady_clock","Forest.animate material uniform updates only","filament.forest.animate")
            <<",\"cpu_effects_ms\":"<<metric("cpu","steady_clock","Effects.animate CPU geometry, sorting, uploads and local light updates","filament.effects.animate")
            <<",\"cpu_submit_ms\":"<<metric("cpu","steady_clock","before renderer.render(main view) through HUD render and renderer.endFrame; includes internal waits","filament.render.endFrame")
            <<",\"cpu_wait_ms\":"<<metric("cpu","steady_clock","renderer.beginFrame accepted attempt only; includes CPU work and waiting, not pure synchronization","filament.beginFrame");
        if(preset_==1)out<<",\"filament_main\":"<<metric("cpu","Renderer.getFrameInfoHistory","native FrameInfo beginFrame to endFrame; asynchronous history","filament.history.main")
            <<",\"filament_backend\":"<<metric("cpu","Renderer.getFrameInfoHistory","native backendBeginFrame to backendEndFrame; asynchronous backend thread","filament.history.backend")
            <<",\"filament_gpu\":"<<metric("gpu_frame","Renderer.getFrameInfoHistory.gpuFrameDuration","native renderer frame GPU duration, includes main view and HUD, not presentation","filament.history.gpu");
        out<<"},\"limitations\":[\"Submitted application frame intervals include rejected attempts; not display presentation FPS.\",\"Public FrameInfo exposes no per-pass breakdown in the pinned SDK; no forced GPU wait added.\",\"Vulkan FIFO is hard-coded in the pinned SDK; GLES uses EGL default swap interval. Refresh limits may apply.\",\"Initialization readiness uses 120 successful renders after synchronous asset upload, then 30 second warmup; no public pipeline compilation completion signal.\",\"65 percent uses fixed min=max bounds on the native resolution API; reported dimensions derive from its actual last scale.\",\"GPU and driver strings unavailable on GLES; Vulkan uses cached platform properties. Thermal snapshot captured during initialization.\",\"CPU beginFrame is not pure wait time; CPU render submission includes internal waits.\",\"Native frame timers may remain internally active; detailed history is collected only in GPU Diagnostic.\",\"No-effects removes six batches and three lights from the scene and skips their CPU animation; vegetation remains.\"]}";
        return out.str();
    }
    void finishBenchmark() {
        const bool valid=run_.valid;
        try {
            const std::string storage=SDL_GetAndroidInternalStoragePath();
            device_=benchmark::read(storage+"/benchmark-device.json");
            thermal_=benchmark::read(storage+"/benchmark-thermal.json");
            benchmark::save(run_,root(),runId_,identity(),metadata());
        }
        catch(const std::exception& e){restoreBenchmark();benchmark::status=4000;SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Benchmark save failed",e.what(),window_);return;}
        if(valid && benchmark::repetition<3){++benchmark::repetition;beginBenchmarkRun();}
        else {const bool cancelled=run_.reason=="cancelled by user";restoreBenchmark();benchmark::status=valid?3000:cancelled?5000:4000;}
    }
    void invalidateBenchmark(const char* reason) {if(benchmarkActive_){if(reference_){restoreBenchmark();benchmark::status=4000;}else{run_.stop(reason);finishBenchmark();}}}
    void benchmarkFrame(int64_t now,benchmark::Frame frame) {
        ++drawn_;
        if(reference_) {
            if(drawn_>=120 && benchmark::status!=6000){std::filesystem::create_directories(root());benchmark::write(std::filesystem::path(root())/"reference-settings.json",metadata());benchmark::status=6000;}
            return;
        }
        const bool ready=drawn_>=120;
        if(run_.sample(now,ready,frame))animationOrigin_=now;
        if(preset_==1 && run_.phase==benchmark::Run::Measure && !run_.frames.empty()) {
            const auto history=renderer_->getFrameInfoHistory(16);
            for(auto it=history.rbegin();it!=history.rend();++it) {
                const auto& f=*it;
                if(f.beginFrame<run_.anchor)continue;
                auto& mask=observedHistory_[f.frameId];
                auto append=[&](const char* name,int64_t ns,uint8_t bit) {if(ns>0 && !(mask&bit)){if(run_.metrics.size()>=180000){run_.stop("metric capacity exceeded");return;}run_.metrics.push_back({f.frameId,uint32_t(run_.frames.size()-1),name,double(ns)*1e-6});mask|=bit;}};
                if(f.endFrame>=f.beginFrame)append("filament_main",f.endFrame-f.beginFrame,1);
                if(f.backendBeginFrame>0 && f.backendEndFrame>=f.backendBeginFrame)append("filament_backend",f.backendEndFrame-f.backendBeginFrame,2);
                append("filament_gpu",f.gpuFrameDuration,4);
            }
        }
        if(run_.phase==benchmark::Run::Finished){finishBenchmark();return;}
        benchmark::status=run_.phase==benchmark::Run::Waiting?7000:run_.phase==benchmark::Run::Warmup?1000+int((now-run_.epoch)/1'000'000'000ll):2000+int((now-run_.anchor)/1'000'000'000ll);
    }

    void updateDisplayInfo() {
        const auto* mode=SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window_));
        const float next=mode && mode->refresh_rate>0?mode->refresh_rate:0;
        if(next!=refreshRate_) {invalidateBenchmark("display refresh rate changed");stats_.reset();if(hud_)hud_->reset();}
        refreshRate_=next;
        Renderer::DisplayInfo info;info.refreshRate=refreshRate_;renderer_->setDisplayInfo(info);
        if(refreshRate_>0)SDL_Log("Display refresh rate: %.3f Hz",double(refreshRate_));
        else SDL_Log("Display refresh rate unavailable; Filament display pacing disabled");
    }
    void releaseSwapChain() {
        if(swapChain_) {
            engine_->destroy(swapChain_);
            swapChain_=nullptr;
            // destroy() is queued. Keep the native window alive until the GPU driver detaches.
            engine_->flushAndWait();
        }
        if(native_) {ANativeWindow_release(native_);native_=nullptr;}
    }
    bool mouseOnHud(float x, float y) const {
        int width=0,height=0;
        if(!SDL_GetWindowSize(window_,&width,&height) || width<=0 || height<=0)
            return false;
        return hud_->contains(x/width,y/height,width_,height_);
    }
    float pinchDistance() const {
        if(fingers_.size()!=2)return 0;
        auto i=fingers_.begin();float2 a=i->second;float2 b=(++i)->second;
        // Touch coordinates are normalized separately by screen width and height.
        const float aspect=height_>0?float(width_)/height_:1.f;
        return length((a-b)*float2{aspect,1});
    }
    void orbit(float x,float y) {yaw_-=x*4;pitch_=std::clamp(pitch_+y*2,.03f,.85f);cameraDirty_=true;}
    SDL_Window* window_;
    backend::Backend backend_;
    // The supplied platform must outlive Engine::destroy() in this object\'s destructor.
    std::unique_ptr<backend::Platform> platform_;
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
    std::set<SDL_FingerID> hudFingers_;
    bool mouseHudDown_=false;
    bool switchRequested_=false;
    float yaw_,pitch_,distance_;
    bool cameraDirty_=true;
    float refreshRate_=0;
    FrameStats stats_;
    int width_=0,height_=0;
    bool active_=true;
    uint64_t start_;
};
}
int main(int,char**) {
    if(!SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)) {SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",SDL_GetError());return 1;}
    SDL_Window* window=SDL_CreateWindow("Forest Ruins · Filament",1280,720,
        SDL_WINDOW_RESIZABLE|SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if(!window) {SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"%s",SDL_GetError());SDL_Quit();return 1;}
    int result=0;
    try {
        const uint64_t start=SDL_GetTicks();
        auto backendChoice=backend::Backend::OPENGL;
        auto demo=std::make_unique<Demo>(window,backendChoice,CameraOrbit{},start);
        demo->initialize();
        bool running=true;
        while(running) {
            SDL_Event event;
            while(SDL_PollEvent(&event)) {
                if(event.type==SDL_EVENT_QUIT || (event.type==SDL_EVENT_KEY_DOWN &&
                    (event.key.key==SDLK_ESCAPE || event.key.key==SDLK_AC_BACK)))running=false;
                if(running)demo->event(event);
            }
            if(!running)break;
            if(demo->consumeBackendSwitch()) {
                const auto orbit=demo->cameraOrbit();
                demo.reset(); // Fully release the previous GPU driver before creating the next.
                backendChoice=backendChoice==backend::Backend::OPENGL
                    ? backend::Backend::VULKAN : backend::Backend::OPENGL;
                demo=std::make_unique<Demo>(window,backendChoice,orbit,start);
                demo->initialize(); // Fail visibly; never silently substitute another backend.
            }
            demo->frame();
        }
    } catch(const std::exception& error) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,"Forest scene failed: %s",error.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,"Forest scene failed",error.what(),window);
        result=1;
    }
    SDL_DestroyWindow(window);SDL_Quit();return result;
}

