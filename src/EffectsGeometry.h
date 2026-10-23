#pragma once
#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>
#include <math/quat.h>
#include <array>
#include <vector>

namespace forest {
enum class Effect { Fire, Blast, Spray, Lightning, Smoke, Water, Count };
struct EffectVertex {
    filament::math::float3 position;
    filament::math::float2 uv;
    filament::math::float4 color;
    filament::math::quatf tangent;
};
using EffectMeshes = std::array<std::vector<EffectVertex>, size_t(Effect::Count)>;
// Water surface and spray share one site; the five displays are spatially separate.
filament::math::float3 effectOrigin(Effect effect);
float blastAge(float seconds);
float lightningPulse(float seconds);
EffectMeshes effectMeshes(float seconds, filament::math::float3 eye);
}
