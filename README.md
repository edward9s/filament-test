# Forest Ruins — Filament + SDL3

Native C++ forest-and-ruins scene for **Android arm64**, using **SDL 3.4.18** for
window/input/lifecycle and **Google Filament 1.78.1** for OpenGL ES rendering.

![Actual Filament OpenGL rendering on Linux llvmpipe](docs/forest-preview.jpg)

This preview is an actual Filament render at 1280×720, using the same scene and materials.
It is not an Android screenshot or an FPS benchmark. The visible FPS is measured on the
Linux software renderer, not a phone.

## Scene

- Two stone arches, broken walls, beveled masonry, scattered rubble and an uneven stone path.
- Undulating forest floor, branching trees with exposed roots, folded individual leaves, ferns and ivy.
- Photograph-based masked pine-sprig canopy cards, with crossed planes for view independence.
- Photograph-based 1K PBR ground, mossy stone and bark maps: albedo, OpenGL normals and AO/roughness/metalness.
- Forest HDR environment, prefiltered reflections, diffuse spherical harmonics, warm directional sunlight,
  three shadow cascades, SSAO, height fog, restrained bloom, temporal anti-aliasing and animated foliage.
- A sunward opening in the canopy lets direct sunlight reach the arches and stone path;
  reduced ambient light and thinner fog preserve the contrast between sunlight and tree shadows.
- Top-left FPS overlay, updated every half second from submitted frames. Skipped frames count
  as elapsed time; backgrounding and surface changes reset the measurement.
- Deterministic generation with spatial/material batching and frustum culling; approximately
  386,000 triangles, 232 batches and 61 MiB of vertex/index data before GPU driver overhead.
  Texture data is about 53 MiB including mipmaps, plus environment maps and render targets.
- One finger rotates the camera around the ruin; two fingers pinch to zoom. Mouse drag/wheel also work.
  This is an orbiting scene viewer; it does not include walking physics or collision detection.
- Assets are bundled in the APK. No network connection is needed while viewing the scene.

The visual target is a realistic forest ruin. Architecture and vegetation are procedural geometry;
this does not contain scanned buildings or production-quality scanned tree models. Actual image
quality and frame rate must be evaluated on the target phone.

## Build

JDK 21, Android SDK 35 / build-tools 35.0.0, **NDK 30.0.16248370**, CMake 3.22.1,
Gradle 8.12.1, Python 3, curl, tar, unzip, and Linux x86-64 for the asset tools:

```sh
bash scripts/download-deps.sh
python3 -m pip install Pillow==11.3.0
python3 scripts/prepare-scene.py
cd android
gradle --no-daemon :app:assembleDebug
```

APK: `android/app/build/outputs/apk/debug/app-debug.apk`.

`prepare-scene.py` downloads SHA-256-pinned CC0 source maps and the matching official Filament
host tools, compiles materials with `matc`, and prefilters the HDR environment with `cmgen`.
It fails on download/hash errors; Gradle fails if scene assets have not been prepared. Generated
files live in `.generated/` and downloaded dependencies in `.deps/`; neither is committed.
The first asset preparation downloads roughly 73 MiB of tools and 12 MiB of source assets.

Filament and SDL use official prebuilt distributions. The NDK must match the pinned Filament
release: using NDK r29 with this release previously caused an unresolved `__hash_memory` symbol.

## GitHub Actions

**Actions → Android test build → Run workflow**. Builds remain **manual only**; neither commits
nor PRs trigger APK builds. Successful builds replace the same
`test-build` prerelease / `FilamentSDL3-test-arm64.apk`. No Actions Artifacts are used.

The workflow prepares all scene assets, checks geometry and FPS accounting, builds the APK, verifies that the
materials/textures/environment maps are packaged, and verifies the APK signature before upload.
The APK remains debug-signed; a changed runner debug key may require uninstalling an older build.

## Validation

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -isystem .deps/host/filament/include \
  -Isrc tests/geometry.cpp src/Geometry.cpp -o /tmp/forest-geometry-test
/tmp/forest-geometry-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc tests/fps.cpp -o /tmp/forest-fps-test
/tmp/forest-fps-test
python3 scripts/check-apk.py android/app/build/outputs/apk/debug/app-debug.apk
```

Geometry checks cover determinism, finite coordinates/UVs/tangents, normalized tangent frames,
index bounds, degenerate triangles, ground winding, layer presence and mobile geometry budgets.
Shader compilation uses the exact engine release. These checks do not establish Android runtime
performance; inspect the built scene on a physical device, including pinch/orbit, portrait/landscape,
background/resume and screen off/on. Dynamic resolution can scale between 65% and 100% per axis.

## Layout

- `src/main.cpp`: SDL event loop, orbit/pinch camera, Android surface recreation and suspension.
- `src/Forest.*`: Filament meshes, textures, materials, environment, sunlight and resource ownership.
- `src/Geometry.*`: deterministic scene generation, independent of SDL and the renderer.
- `src/FpsHud.*`, `src/FpsCounter.h`: Filament text overlay and frame-rate accounting.
- `materials/`: PBR, animated subsurface foliage and FPS text materials.
- `assets/sources.json`, `assets/CREDITS.md`: exact source hashes, credits and CC0 licensing.
- `scripts/prepare-scene.py`, `scripts/check-apk.py`: asset preparation and packaging validation.
- `android/`, `CMakeLists.txt`: existing SDLActivity / native Android build.

SDL does not create an OpenGL context. Filament owns the rendering backend and renders into
SDL's Android native window. Backgrounding releases the swapchain even if Android later reuses
the same native-window address. Renderer resources are destroyed before the engine.

Official references: [Filament](https://github.com/google/filament/releases/tag/v1.78.1),
[SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18),
[asset credits](assets/CREDITS.md).
