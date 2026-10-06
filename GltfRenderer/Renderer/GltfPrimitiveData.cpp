#include "GltfPrimitiveData.h"

#include "../Gltf/GltfAccessorView.h"

#include <limits>

namespace Phantom::Gltf {

glm::vec3 accessorAabbCenter(const GltfDocument& doc, int accessorIndex)
{
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(doc.accessors.size()))
        return glm::vec3(0.f);
    GltfAccessorView view(doc, accessorIndex);
    if (view.count() == 0) return glm::vec3(0.f);
    glm::vec3 mn(std::numeric_limits<float>::max());
    glm::vec3 mx(std::numeric_limits<float>::lowest());
    for (size_t i = 0; i < view.count(); ++i) {
        const glm::vec3 p = view.get<glm::vec3>(i);
        mn = glm::min(mn, p);
        mx = glm::max(mx, p);
    }
    return (mn + mx) * 0.5f;
}

void readPrimitiveGeometry(const GltfDocument& doc, const GltfPrimitive& prim,
                           std::vector<glm::vec3>& positions, std::vector<glm::vec3>& normals)
{
    positions.clear();
    normals.clear();
    if (prim.positionAccessor < 0) return;

    GltfAccessorView posView(doc, prim.positionAccessor);
    positions.resize(posView.count());
    for (size_t i = 0; i < positions.size(); ++i)
        positions[i] = posView.get<glm::vec3>(i);

    if (prim.normalAccessor >= 0) {
        GltfAccessorView nView(doc, prim.normalAccessor);
        normals.resize(nView.count());
        for (size_t i = 0; i < normals.size(); ++i)
            normals[i] = nView.get<glm::vec3>(i);
    } else {
        normals.assign(positions.size(), glm::vec3(0.f, 1.f, 0.f));
    }
}

}
