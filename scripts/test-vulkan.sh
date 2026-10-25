#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# The official Linux SDK uses libc++; do not link its C++ API with libstdc++.
host=.deps/host/filament
test -s "$host/lib/x86_64/libfilament.a"
mkdir -p .generated/tests
pipeline_includes=(
    -isystem .deps/pipeline/filament/backend/src/vulkan
    -isystem .deps/pipeline/filament/backend/src
    -isystem .deps/pipeline/filament/backend/include
    -isystem .deps/pipeline/libs/utils/include
    -isystem .deps/pipeline/third_party/vkmemalloc/include
    -isystem "$host/include" -isystem .deps/headers/include
)
# Check Android-only private headers too; the software renderer uses generic mutexes.
"${CXX:-clang++-20}" -std=c++20 -stdlib=libc++ -fno-rtti -DNDEBUG -D__ANDROID__ \
    -Wall -Wextra -Werror -Wno-missing-field-initializers "${pipeline_includes[@]}" \
    -fsyntax-only .deps/pipeline/Pipeline.cpp
"${CXX:-clang++-20}" -std=c++20 -O2 -stdlib=libc++ -fno-rtti -DNDEBUG \
    -Wall -Wextra -Werror -Wno-missing-field-initializers "${pipeline_includes[@]}" \
    -c .deps/pipeline/Pipeline.cpp -o .generated/tests/pipeline.o
"${CXX:-clang++-20}" -std=c++20 -O2 -stdlib=libc++ -fno-rtti -Wall -Wextra -Werror \
    -isystem "$host/include" -isystem .deps/headers/include -Isrc tests/render.cpp src/Forest.cpp src/Geometry.cpp \
    src/FpsHud.cpp src/Effects.cpp src/EffectsGeometry.cpp .generated/tests/pipeline.o \
    -Wl,--start-group "$host"/lib/x86_64/*.a -Wl,--end-group -Wl,--gc-sections \
    -lc++abi -lpthread -ldl -l:libX11.so.6 -o .generated/tests/render

# Force the software ICD so the test also works on a GitHub runner without a GPU.
if [[ -z "${VK_ICD_FILENAMES:-}" ]]; then
    icds=(/usr/share/vulkan/icd.d/lvp_icd*.json)
    test -f "${icds[0]}"
    export VK_ICD_FILENAMES="${icds[0]}"
fi
timeout 240 .generated/tests/render 0.3 vulkan
timeout 240 .generated/tests/render 0.3 vulkan suboptimal
