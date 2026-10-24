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
  386,000 triangles, 232 batches and 57 MiB of vertex/index data before GPU driver overhead.
  Texture data is about 53 MiB including mipmaps, plus environment maps and render targets.
- One finger rotates the camera around the ruin; two fingers pinch to zoom. Mouse drag/wheel also work.
  This is an orbiting scene viewer; it does not include walking physics or collision detection.
- Five separated effects: continuous fire and embers at the front left, a repeating explosion on
  the left, an animated water jet and flowing sheet at the front right, branching double-strike
  lightning on the right, and a dense drifting smoke column behind the ruins.
  Fire, explosion and lightning illuminate nearby surfaces. Explosion repeats every 5.4 seconds;
  lightning every 4.8 seconds. These are visual effects without damage or fluid physics.
  The effects add 583 animated quads (1,166 triangles), six batches and about 126 KiB of vertex/index
  data; buffers keep fixed capacity while visible positions update with time and the camera.
  Fully transparent explosion and lightning batches skip geometry generation, uploads and draws.
- Assets are bundled in the APK. No network connection is needed while viewing the scene.

The visual target is a realistic forest ruin. Architecture and vegetation are procedural geometry;
this does not contain scanned buildings or production-quality scanned tree models. Actual image
quality and frame rate must be evaluated on the target phone.

## Performance

Static batches share only bit-identical vertices and use 16-bit indices. Triangle order, UVs,
tangent frames and colors are preserved. Current mesh payload drops from 64,784,496 to 59,593,916
bytes (8.0%); batches that exceed the 16-bit vertex limit fail early. Vertex indexing adds
one-time CPU work during scene loading; it does not run each frame. Particle shaders reject
transparent borders before evaluating noise. Unlit particles do not rebuild unused tangent frames.

The CPU effect benchmark simulates 1,800 frames per trial and reports the median of seven trials.
A Linux comparison measured 103.2 → 68.7 µs per frame; this measures effect mesh generation only,
not total frame time or Android FPS. The 15 GLES particle readbacks at five kinds and three times
were byte-identical before/after the shader change; expanded static triangle attributes and all
visible effect positions/UVs/colors were also compared exactly. Lighting, shadow cascades, wind,
particle counts when visible, post-processing and resolution settings are retained.

Run the reproducible CPU benchmark with the pinned SDK headers:

```sh
g++ -std=c++20 -O2 -isystem .deps/host/filament/include -Isrc \
  tests/benchmark.cpp src/EffectsGeometry.cpp src/Geometry.cpp -o /tmp/forest-benchmark
/tmp/forest-benchmark
```

## Build

JDK 21, Android SDK 35 / build-tools 35.0.0, **NDK 30.0.16248370**, CMake 3.22.1,
Gradle 8.12.1, Python 3, curl, tar, unzip, and Linux x86-64 for the asset tools.
Set the release signing environment variables described below before building:

```sh
bash scripts/download-deps.sh
python3 -m pip install Pillow==11.3.0
python3 scripts/prepare-scene.py
cd android
gradle --no-daemon :app:assembleRelease
```

APK: `android/app/build/outputs/apk/release/app-release.apk`.

`prepare-scene.py` downloads SHA-256-pinned CC0 source maps and the matching official Filament
host tools, compiles materials with `matc`, and prefilters the HDR environment with `cmgen`.
It fails on download/hash errors; Gradle fails if scene assets have not been prepared. Generated
files live in `.generated/` and downloaded dependencies in `.deps/`; neither is committed.
The first asset preparation downloads roughly 73 MiB of tools and 12 MiB of source assets.

Filament uses the official prebuilt Android native SDK. SDL3 is statically linked from
this repository's `sdl3-static` Release. `download-deps.sh` checks the release metadata
and SHA-256, then installs its static library, headers, CMake package and matching SDL
Android Java sources in `.deps/sdl3/`. Because this is a private repository, local builds
require authenticated `gh` CLI access (or a `GH_TOKEN`); GitHub Actions supplies
`github.token`. A missing or invalid SDK fails the build; there is no AAR fallback.
The NDK must match the pinned Filament release: using NDK r29 with this release
previously caused an unresolved `__hash_memory` symbol.

## GitHub Actions

**Actions → Android test build → Run workflow**. Builds remain **manual only**; neither commits
nor PRs trigger APK builds. Successful builds replace the same
`test-build` prerelease / `FilamentSDL3-test-arm64.apk`. No Actions Artifacts are used.

The workflow downloads the SDL3 static SDK, prepares all scene assets, checks geometry FPS
accounting and effects, builds the APK, verifies the exported `JNI_OnLoad` and absence of
`libSDL3.so`, checks packaged materials/textures/environment maps, and verifies ZIP integrity,
manifest parsing,
16 KiB ZIP alignment and the APK signature before upload. Build logs include the signing
certificate digest; release notes include the APK size and SHA-256 so downloads can be checked.
The APK uses the release build type with debugging disabled and is signed with the supplied
repository key. Signing settings are mandatory; missing settings fail the build.

Configure these **Actions Secrets** using your existing keystore:

| Secret | Value |
| --- | --- |
| `KEYSTORE_BASE64` | Base64-encoded keystore file |
| `STORE_PASSWORD` | Keystore password |
| `KEY_ALIAS` | Signing key alias |
| `KEY_PASSWORD` | Signing key password |

The workflow restores the keystore only in the runner's temporary directory and removes it
at job end. It never generates a key. The workflow references the repository Secret names listed above.

For a local release build, set `KEYSTORE_PATH` to the absolute path of your existing
keystore and export `STORE_PASSWORD`, `KEY_ALIAS`, and
`KEY_PASSWORD` before running Gradle. Switching from the previous debug-signed APK
to a different release key requires uninstalling that old APK first.

If Android reports an invalid package, first check that the downloaded APK matches the release
size and SHA-256. The build targets arm64-v8a, Android 8.0 or later. An earlier debug-signed
installation cannot be updated by the release-signed APK; remove the old app before installing
this release (removing an app clears its local data). For a precise installer error, use
`adb install -r FilamentSDL3-test-arm64.apk`; keep its `INSTALL_*` message for diagnosis.

## Validation

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -isystem .deps/host/filament/include \
  -Isrc tests/geometry.cpp src/Geometry.cpp -o /tmp/forest-geometry-test
/tmp/forest-geometry-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc tests/fps.cpp -o /tmp/forest-fps-test
/tmp/forest-fps-test
g++ -std=c++20 -O2 -Wall -Wextra -Werror -isystem .deps/host/filament/include \
  -Isrc tests/effects.cpp src/EffectsGeometry.cpp src/Geometry.cpp -o /tmp/forest-effects-test
/tmp/forest-effects-test
python3 scripts/check-apk.py android/app/build/outputs/apk/release/app-release.apk
```

Particle fragment checks run the actual effect shader code in a surfaceless OpenGL ES 3 context
with the mobile `mediump` default. Fire, explosion, spray, lightning and smoke must produce visible
pixels at short and long running times. Noise uses high-precision integer hashing; emission is
exposed in high precision before being stored in Filament's material inputs, avoiding FP16 overflow.
Run `python3 tests/particles.py` on Linux with Mesa EGL installed.

Geometry checks cover determinism, finite coordinates/UVs/tangents, normalized tangent frames,
index bounds, degenerate triangles, ground winding, layer presence and mobile geometry budgets.
Shader compilation uses the exact engine release. These checks do not establish Android runtime
performance; inspect the built scene on a physical device, including pinch/orbit, portrait/landscape,
background/resume and screen off/on. Dynamic resolution can scale between 65% and 100% per axis.

## Layout

- `src/main.cpp`: SDL event loop, orbit/pinch camera, Android surface recreation and suspension.
- `src/Forest.*`: Filament meshes, textures, materials, environment, sunlight and resource ownership.
- `src/Geometry.*`: deterministic scene generation, independent of SDL and the renderer.
- `src/Effects.*`, `src/EffectsGeometry.*`: effect rendering, light pulses and deterministic animation.
- `src/FpsHud.*`, `src/FpsCounter.h`: Filament text overlay and frame-rate accounting.
- `materials/`: PBR, animated foliage, FPS text, particle effects and flowing water materials.
- `assets/sources.json`, `assets/CREDITS.md`: exact source hashes, credits and CC0 licensing.
- `scripts/prepare-scene.py`, `scripts/check-apk.py`: asset preparation and packaging validation.
- `android/`, `CMakeLists.txt`: existing SDLActivity / native Android build.

SDL does not create an OpenGL context. Filament owns the rendering backend and renders into
SDL's Android native window. Backgrounding releases the swapchain even if Android later reuses
the same native-window address. Renderer resources are destroyed before the engine.

Official references: [Filament](https://github.com/google/filament/releases/tag/v1.78.1),
[SDL3](https://github.com/libsdl-org/SDL/releases/tag/release-3.4.18),
[asset credits](assets/CREDITS.md).
