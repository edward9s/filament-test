#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source="$(realpath "${1:?Pass the fork checkout directory}")"
revision="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["revision"])' "$root/filament.json")"
ndk="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["ndk"])' "$root/filament.json")"
lto="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["lto"])' "$root/filament.json")"
[[ "$(git -C "$source" rev-parse HEAD)" == "$revision" ]]
test -x "${ANDROID_HOME:?}/ndk/$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++"

# Debug tests exercise the real handle allocator, resource manager and Vulkan pipeline.
cmake -S "$source" -B "$source/out/sdk-debug" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON
cmake --build "$source/out/sdk-debug" --target test_vulkan_pipeline test_utils test_frame_info --parallel 2
"$source/out/sdk-debug/filament/backend/test_vulkan_pipeline"
"$source/out/sdk-debug/libs/utils/test_utils"
"$source/out/sdk-debug/filament/test/test_frame_info"

# Build and install the complete host SDK and material/environment tools together.
cmake -S "$source" -B "$source/out/sdk-host" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$source/out/sdk-linux/filament" \
    -DIMPORT_EXECUTABLES_DIR=out/sdk-host \
    -DFILAMENT_ENABLE_LTO="$lto" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="$lto" \
    -DCMAKE_PROJECT_INCLUDE="$root/cmake/Lto.cmake" \
    -DCMAKE_AR=/usr/bin/llvm-ar-18 -DCMAKE_RANLIB=/usr/bin/llvm-ranlib-18 \
    -DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld-18 \
    -DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld-18 \
    -DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld-18 \
    -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON -DFILAMENT_BUILD_TESTING=OFF
cmake --build "$source/out/sdk-host" --target install --parallel 2

# Use this source checkout's exported host tools for Android material generation.
cmake -S "$source" -B "$source/out/sdk-android" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$source/out/sdk-arm64/filament" \
    -DCMAKE_TOOLCHAIN_FILE="$source/build/toolchain-aarch64-linux-android.cmake" \
    -DFILAMENT_NDK_VERSION="$ndk" -DIMPORT_EXECUTABLES_DIR=out/sdk-host \
    -DFILAMENT_ENABLE_LTO="$lto" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION="$lto" \
    -DCMAKE_PROJECT_INCLUDE="$root/cmake/Lto.cmake" \
    -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON -DFILAMENT_BUILD_TESTING=OFF
cmake --build "$source/out/sdk-android" --target install --parallel 2

python3 "$root/scripts/package-filament.py" --source "$source" \
    --sdk "$source/out/sdk-linux/filament" --archive filament-linux.tgz
python3 "$root/scripts/package-filament.py" --source "$source" \
    --sdk "$source/out/sdk-arm64/filament" --archive filament-android-arm64.tgz

