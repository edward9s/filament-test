#pragma once
#include "Geometry.h"
#include <math/half.h>
#include <math/norm.h>
#include <cstddef>
#include <type_traits>

namespace forest {
// Keep positions and UVs exact. Quaternions use Filament's normalized SHORT4
// format; half colors retain values above 1 without UNORM clamping.
struct PackedVertex {
    float3 position;
    short4 tangent;
    float2 uv;
    half4 color;
};
static_assert(std::is_standard_layout_v<PackedVertex> && sizeof(PackedVertex)==36);

inline PackedVertex packVertex(const Vertex& vertex) {
    return {vertex.position, packSnorm16(float4{vertex.tangent.x,vertex.tangent.y,
        vertex.tangent.z,vertex.tangent.w}), vertex.uv, half4{vertex.color}};
}
}
