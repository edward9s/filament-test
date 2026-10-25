#include "../src/Geometry.cpp"
#include <iostream>
#include <stdexcept>

// Include the generator to test isolated rocks before they are batched with masonry.
int main() {
    using namespace forest;
    size_t triangles = 0;
    for (int sample = 0; sample < 64; ++sample) {
        Builder builder;
        builder.random.state += uint32_t(sample);
        const float3 center{3.5f, -.12f, 3.5f};
        const float3 size = sample == 0 ? float3{.55f, .8f, .55f}
            : float3{.5f + sample * .01f, .4f, .7f};
        builder.rock(center, size);
        const auto& mesh = builder.mesh(center, Surface::Stone);
        if (mesh.indices.size() != 330)
            throw std::runtime_error("Rock must have 110 nondegenerate triangles");
        for (size_t i = 0; i < mesh.indices.size(); i += 3) {
            const auto a = mesh.vertices.at(mesh.indices[i]).position;
            const auto b = mesh.vertices.at(mesh.indices[i + 1]).position;
            const auto c = mesh.vertices.at(mesh.indices[i + 2]).position;
            const auto normal = cross(b - a, c - a);
            const auto radial = (a + b + c) / 3.f - center;
            if (!(dot(normal, radial) > 1e-7f))
                throw std::runtime_error("Rock triangle must face outward");
            ++triangles;
        }
    }
    std::cout << "Rocks passed: " << triangles << " outward triangles\n";
}
