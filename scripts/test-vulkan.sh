#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# The fork Linux SDK uses libc++; do not link its C++ API with libstdc++.
host=.deps/host/filament
test -s "$host/lib/x86_64/libfilament.a"
mkdir -p .generated/tests
"${CXX:-clang++-18}" -std=c++20 -O2 -stdlib=libc++ -fno-rtti -Wall -Wextra -Werror \
    -isystem "$host/include" -Isrc tests/render.cpp src/Forest.cpp src/Geometry.cpp \
    src/FpsHud.cpp src/Effects.cpp src/EffectsGeometry.cpp \
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
