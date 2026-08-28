#include "ObjToGltfConverter.h"
#include "GltfAccessorBuilder.h"

using namespace Phantom::Gltf;
using namespace Phantom::File;

namespace {

// Fan-triangulates one OBJFace, appending one (position, normal, uv) triple per triangle corner
// to the flattened out-arrays. Skips the whole face (rather than emitting a partial/degenerate
// triangle) if any position reference is missing or out of range. OBJFace indices are stored
// 1-based (raw from the file, see OBJSyntaxParser); -1 in normalIndices/texCoordIndices marks
// "absent" (OBJFileReader's own sentinel).
void appendFace(const OBJFile& obj, const OBJFace& face,
                 std::vector<glm::vec3>& positions,
                 std::vector<glm::vec3>& normals,
                 std::vector<glm::vec2>& texCoords)
{
    const size_t n = face.positionIndices.size();
    if (n < 3) return;

    std::vector<glm::vec3> facePos(n);
    std::vector<glm::vec3> faceNrm(n);
    std::vector<glm::vec2> faceUv(n);
    for (size_t c = 0; c < n; ++c) {
        const unsigned int pi1 = face.positionIndices[c];
        if (pi1 == 0 || pi1 > obj.positions.size()) return; // malformed face: skip entirely
        const auto& p = obj.positions[pi1 - 1];
        facePos[c] = glm::vec3(p.x, p.y, p.z);

        faceNrm[c] = glm::vec3(0.f, 1.f, 0.f); // default: matches GltfImporter::extractPrimitive's fallback
        if (c < face.normalIndices.size()) {
            const int ni1 = face.normalIndices[c];
            if (ni1 >= 1 && static_cast<size_t>(ni1) <= obj.normals.size()) {
                const auto& nrm = obj.normals[ni1 - 1];
                faceNrm[c] = glm::vec3(nrm.x, nrm.y, nrm.z);
            }
        }

        faceUv[c] = glm::vec2(0.f, 0.f);
        if (c < face.texCoordIndices.size()) {
            const int ti1 = face.texCoordIndices[c];
            if (ti1 >= 1 && static_cast<size_t>(ti1) <= obj.texCoords.size()) {
                const auto& uv = obj.texCoords[ti1 - 1];
                faceUv[c] = glm::vec2(uv.x, uv.y);
            }
        }
    }

    for (size_t i = 1; i + 1 < n; ++i) {
        for (size_t idx : { size_t(0), i, i + 1 }) {
            positions.push_back(facePos[idx]);
            normals.push_back(faceNrm[idx]);
            texCoords.push_back(faceUv[idx]);
        }
    }
}

} // namespace

GltfDocument ObjToGltfConverter::convert(const OBJFile& obj)
{
    GltfDocument doc;
    if (obj.positions.empty()) return doc;

    GltfScene scene;

    for (const auto& group : obj.groups) {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        for (const auto& face : group.faces)
            appendFace(obj, face, positions, normals, texCoords);
        if (positions.empty()) continue;

        std::vector<uint32_t> indices(positions.size());
        for (uint32_t i = 0; i < static_cast<uint32_t>(indices.size()); ++i) indices[i] = i;

        GltfPrimitive prim;
        prim.positionAccessor  = appendAccessor(doc, positions, GltfComponentType::Float, GltfAccessorType::Vec3);
        prim.normalAccessor    = appendAccessor(doc, normals,   GltfComponentType::Float, GltfAccessorType::Vec3);
        prim.texCoord0Accessor = appendAccessor(doc, texCoords, GltfComponentType::Float, GltfAccessorType::Vec2);
        prim.indicesAccessor   = appendAccessor(doc, indices,   GltfComponentType::UnsignedInt, GltfAccessorType::Scalar);

        GltfMesh mesh;
        mesh.name = group.name;
        mesh.primitives.push_back(prim);
        const int meshIndex = static_cast<int>(doc.meshes.size());
        doc.meshes.push_back(std::move(mesh));

        GltfNode node;
        node.name      = group.name;
        node.meshIndex = meshIndex;
        const int nodeIndex = static_cast<int>(doc.nodes.size());
        doc.nodes.push_back(std::move(node));

        scene.nodes.push_back(nodeIndex);
    }

    if (scene.nodes.empty()) return GltfDocument{};

    doc.scenes.push_back(std::move(scene));
    doc.defaultScene = 0;
    return doc;
}
