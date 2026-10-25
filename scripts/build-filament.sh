#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
source="$(realpath "${1:?Pass the fork checkout directory}")"
revision="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["revision"])' "$root/filament.json")"
ndk="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["ndk"])' "$root/filament.json")"
[[ "$(git -C "$source" rev-parse HEAD)" == "$revision" ]]
test -x "${ANDROID_HOME:?}/ndk/$ndk/toolchains/llvm/prebuilt/linux-x86_64/bin/clang++"

# Debug tests exercise the real handle allocator, resource manager and Vulkan pipeline.
cmake -S "$source" -B "$source/out/sdk-debug" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON
cmake --build "$source/out/sdk-debug" --target test_vulkan_pipeline test_utils --parallel 2
"$source/out/sdk-debug/filament/backend/test_vulkan_pipeline"
"$source/out/sdk-debug/libs/utils/test_utils"

# Build and install the complete host SDK and material/environment tools together.
cmake -S "$source" -B "$source/out/sdk-host" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$source/out/sdk-linux/filament" \
    -DIMPORT_EXECUTABLES_DIR=out/sdk-host \
    -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON -DFILAMENT_BUILD_TESTING=OFF
cmake --build "$source/out/sdk-host" --target install --parallel 2

# Use this source checkout's exported host tools for Android material generation.
cmake -S "$source" -B "$source/out/sdk-android" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$source/out/sdk-arm64/filament" \
    -DCMAKE_TOOLCHAIN_FILE="$source/build/toolchain-aarch64-linux-android.cmake" \
    -DFILAMENT_NDK_VERSION="$ndk" -DIMPORT_EXECUTABLES_DIR=out/sdk-host \
    -DFILAMENT_SKIP_SAMPLES=ON -DFILAMENT_SKIP_SDL2=ON -DFILAMENT_BUILD_TESTING=OFF
cmake --build "$source/out/sdk-android" --target install --parallel 2

mkdir -p "$root/.generated/sdk"
for platform in linux arm64; do
    sdk="$source/out/sdk-$platform/filament"
    # Public platform headers include BlueVK, Vulkan and OS-specific utils headers.
    cp -a "$source/libs/bluevk/include/." "$sdk/include/"
    cp -a "$source/libs/utils/include/utils/." "$sdk/include/utils/"
    if [[ "$platform" == linux ]]; then archive=filament-linux.tgz; else archive=filament-android-arm64.tgz; fi
    python3 - "$root/filament.json" "$sdk/SDK-INFO.json" "$archive" <<'PY'
import json,sys
from pathlib import Path
metadata=json.loads(Path(sys.argv[1]).read_text())
metadata['archive']=sys.argv[3]
Path(sys.argv[2]).write_text(json.dumps(metadata,indent=2)+'\n')
PY
    tar -czf "$root/.generated/sdk/$archive" -C "$(dirname "$sdk")" filament
done
(cd "$root/.generated/sdk" && sha256sum filament-linux.tgz filament-android-arm64.tgz > SHA256SUMS)
