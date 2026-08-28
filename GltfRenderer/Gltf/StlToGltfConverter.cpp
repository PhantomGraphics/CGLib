#include "StlToGltfConverter.h"
#include "GltfAccessorBuilder.h"

using namespace Phantom::Gltf;
using namespace Phantom::File;

GltfDocument StlToGltfConverter::convert(const STLFile& stl)
{
    GltfDocument doc;
    if (stl.faces.empty()) return doc;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    positions.reserve(stl.faces.size() * 3);
    normals.reserve(stl.faces.size() * 3);
    texCoords.reserve(stl.faces.size() * 3);

    for (const auto& face : stl.faces) {
        const glm::vec3 n(face.normal.x, face.normal.y, face.normal.z);
        for (const auto& p : face.triangle.getVertices()) {
            positions.emplace_back(p.x, p.y, p.z);
            normals.push_back(n);
            texCoords.emplace_back(0.f, 0.f);
        }
    }
    if (positions.empty()) return doc;

    std::vector<uint32_t> indices(positions.size());
    for (uint32_t i = 0; i < static_cast<uint32_t>(indices.size()); ++i) indices[i] = i;

    GltfPrimitive prim;
    prim.positionAccessor  = appendAccessor(doc, positions, GltfComponentType::Float, GltfAccessorType::Vec3);
    prim.normalAccessor    = appendAccessor(doc, normals,   GltfComponentType::Float, GltfAccessorType::Vec3);
    prim.texCoord0Accessor = appendAccessor(doc, texCoords, GltfComponentType::Float, GltfAccessorType::Vec2);
    prim.indicesAccessor   = appendAccessor(doc, indices,   GltfComponentType::UnsignedInt, GltfAccessorType::Scalar);

    GltfMesh mesh;
    mesh.primitives.push_back(prim);
    doc.meshes.push_back(std::move(mesh));

    GltfNode node;
    node.meshIndex = 0;
    doc.nodes.push_back(std::move(node));

    GltfScene scene;
    scene.nodes.push_back(0);
    doc.scenes.push_back(std::move(scene));
    doc.defaultScene = 0;
    return doc;
}
