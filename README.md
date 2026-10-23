# Filament + SDL3 Android C++ starter

Minimal, native-C++ Android application using **SDL 3.4.18** (Android window, input, lifecycle) and **Google Filament 1.78.1** (actual GPU rendering into SDL's native Android window).

- Android `arm64-v8a`, OpenGL ES backend, API 26+, C++20.
- **Prebuilt dependencies:** use SDL3's official Android AAR and Filament's official Android native release. Neither dependency is recompiled for each test build.
- **Gradle + CMake:** build only the application's `libmain.so`, package `libSDL3.so` from SDL's AAR, and produce a debug-signed APK.
- **Visible smoke demo:** Filament renders a colored skybox; touching the screen changes its color. SDL handles the events. This verifies the SDL native window ↔ Filament SwapChain path, but is not yet a model/PBR scene.
- **Manual GitHub Actions only**. After a successful build, upload `FilamentSDL3-test-arm64.apk` to the *single, rolling* prerelease tag `test-build` (same filename, clobber old asset). No Actions Artifacts.

## Build (Android)

Prerequisites: JDK 21, Android SDK platform 35, Android build-tools 35.0.0, NDK `29.0.14206865`, CMake 3.22.1, Gradle 8.12.1, curl, unzip, tar.

```sh
bash scripts/download-deps.sh
cd android
gradle --no-daemon :app:assembleDebug
# APK: app/build/outputs/apk/debug/app-debug.apk
```

Alternatively, visit **Actions → Android test build → Run workflow** and download the APK from the `test-build` prerelease after the job succeeds.

## What the application does

1. SDL3's `SDLActivity` loads its native runtime and the C++ `libmain.so`.
2. C++ creates a standard SDL3 window (without creating an SDL OpenGL context).
3. SDL's Android native-window pointer is passed to `filament::Engine::createSwapChain`.
4. Filament creates its own rendering backend (OpenGL), camera, scene and skybox, and submits frames.
5. SDL3 sends touch / window lifecycle events to the main loop. Native-window changes rebuild the Filament SwapChain.

SDL3 GPU rendering is deliberately **not** enabled; Filament exclusively owns the GPU pipeline.

## Layout

- `src/main.cpp`: SDL3 event loop and native Filament renderer.
- `CMakeLists.txt`: arm64 native C++ library linking SDL3's Prefab target and Filament's native static libraries.
- `android/`: Android Gradle application, SDLActivity subclass and manifest.
- `scripts/download-deps.sh`: retrieve *pinned*, official prebuilt library distributions, not in Git.
- `.github/workflows/android.yml`: manually triggered APK build and rolling prerelease publish.

## Constraints / validation

The starter uses only Android arm64; it is not yet a desktop build. `Skybox` is a rendering smoke test; materials, meshes, glTF, lighting, touch camera controls, audio and physics can be added separately.

GitHub source commits **do not** prove that the APK compiles or starts on an Android phone. Check the workflow log and test the built APK on a physical device. A successful `assembleDebug` produces a debug-signed APK; the runner's default signing key may change from run to run, so an installed older build may need uninstalling before an upgrade.

Official references: [Filament native distribution](https://github.com/google/filament/releases/tag/v1.78.1), [SDL3 Android package](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18), [SDL Android guide](https://wiki.libsdl.org/SDL3/README-android), [Filament C++ rendering](https://github.com/google/filament).
