#include "McMesh.h"

namespace VolumeView {

PolygonMesh trianglesToPolygonMesh(std::string name,
                                   const std::vector<Phantom::Math::Triangle3df>& tris,
                                   const std::array<float, 4>& rgba)
{
    PolygonMesh mesh;
    mesh.name = std::move(name);
    mesh.positions.reserve(tris.size() * 9);
    mesh.colors.reserve(tris.size() * 12);
    mesh.indices.reserve(tris.size() * 3);

    uint32_t idx = 0;
    for (const auto& tri : tris) {
        const auto verts = tri.getVertices();
        for (int vi = 0; vi < 3; ++vi) {
            mesh.positions.push_back(static_cast<float>(verts[vi].x));
            mesh.positions.push_back(static_cast<float>(verts[vi].y));
            mesh.positions.push_back(static_cast<float>(verts[vi].z));
            mesh.colors.insert(mesh.colors.end(), rgba.begin(), rgba.end());
            mesh.indices.push_back(idx++);
        }
    }
    return mesh;
}

} // namespace VolumeView
