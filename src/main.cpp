#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/Skybox.h>
#include <filament/SwapChain.h>
#include <filament/View.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>

#include <android/native_window.h>
#include <cstdint>

namespace {
using namespace filament;

ANativeWindow* getNativeWindow(SDL_Window* window) {
    return static_cast<ANativeWindow*>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr));
}

class FilamentDemo {
public:
    explicit FilamentDemo(SDL_Window* window) : window_(window) {
        engine_ = Engine::create(backend::Backend::OPENGL);
        if (!engine_) return;

        renderer_ = engine_->createRenderer();
        scene_ = engine_->createScene();
        view_ = engine_->createView();
        cameraEntity_ = utils::EntityManager::get().create();
        camera_ = engine_->createCamera(cameraEntity_);
        view_->setCamera(camera_);
        view_->setScene(scene_);
        setBackground(false);
        syncWindow();
    }

    ~FilamentDemo() {
        if (!engine_) return;
        if (swapChain_) engine_->destroy(swapChain_);
        if (scene_) scene_->setSkybox(nullptr);
        if (skybox_) engine_->destroy(skybox_);
        if (view_) engine_->destroy(view_);
        if (scene_) engine_->destroy(scene_);
        if (renderer_) engine_->destroy(renderer_);
        if (camera_) engine_->destroyCameraComponent(cameraEntity_);
        utils::EntityManager::get().destroy(cameraEntity_);
        Engine::destroy(&engine_);
    }

    bool valid() const { return engine_ && renderer_ && scene_ && view_ && camera_; }

    void toggleColor() {
        alternate_ = !alternate_;
        setBackground(alternate_);
    }

    void frame() {
        syncWindow();
        if (!swapChain_ || !renderer_) {
            SDL_Delay(20);
            return;
        }
        if (renderer_->beginFrame(swapChain_)) {
            renderer_->render(view_);
            renderer_->endFrame();
        }
    }

private:
    void setBackground(bool alternate) {
        if (!engine_ || !scene_) return;
        scene_->setSkybox(nullptr);
        if (skybox_) engine_->destroy(skybox_);
        const auto color = alternate
            ? filament::math::float4{0.09f, 0.30f, 0.23f, 1.0f}
            : filament::math::float4{0.07f, 0.12f, 0.34f, 1.0f};
        skybox_ = Skybox::Builder().color(color).build(*engine_);
        scene_->setSkybox(skybox_);
    }

    void syncWindow() {
        // Android destroys and recreates its ANativeWindow when a surface is lost
        // (rotation, suspend / resume, etc.). Filament must follow that change.
        auto* native = getNativeWindow(window_);
        if (native != nativeWindow_) {
            if (swapChain_) {
                engine_->destroy(swapChain_);
                swapChain_ = nullptr;
            }
            nativeWindow_ = native;
            if (native) {
                swapChain_ = engine_->createSwapChain(native);
                SDL_Log("Filament SwapChain created for Android native window");
            }
        }

        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window_, &w, &h);
        if (w > 0 && h > 0 && (w != width_ || h != height_)) {
            width_ = w;
            height_ = h;
            view_->setViewport({0, 0, static_cast<uint32_t>(w), static_cast<uint32_t>(h)});
            camera_->setProjection(45.0, static_cast<double>(w) / h, 0.1, 100.0,
                    Camera::Fov::VERTICAL);
            camera_->lookAt({0.0, 0.0, 3.0}, {0.0, 0.0, 0.0}, {0.0, 1.0, 0.0});
        }
    }

    SDL_Window* window_ = nullptr;
    Engine* engine_ = nullptr;
    Renderer* renderer_ = nullptr;
    Scene* scene_ = nullptr;
    View* view_ = nullptr;
    Skybox* skybox_ = nullptr;
    SwapChain* swapChain_ = nullptr;
    Camera* camera_ = nullptr;
    utils::Entity cameraEntity_{};
    ANativeWindow* nativeWindow_ = nullptr;
    int width_ = 0, height_ = 0;
    bool alternate_ = false;
};
} // namespace

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Filament + SDL3", 720, 1280,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int result = 0;
    {
        FilamentDemo demo(window);
        if (!demo.valid()) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Filament initialization failed");
            result = 1;
        } else {
            bool running = true;
            while (running) {
                SDL_Event event;
                while (SDL_PollEvent(&event)) {
                    if (event.type == SDL_EVENT_QUIT) running = false;
                    if (event.type == SDL_EVENT_FINGER_DOWN ||
                            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                        demo.toggleColor();
                    }
                }
                if (running) demo.frame();
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
