#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

namespace Phantom::Gltf
{

    // glTF 2.0 data structure definitions

    enum class GltfComponentType : uint32_t {
        Byte = 5120,
        UnsignedByte = 5121,
        Short = 5122,
        UnsignedShort = 5123,
        UnsignedInt = 5125,
        Float = 5126,
    };

    enum class GltfAccessorType {
        Scalar, Vec2, Vec3, Vec4, Mat2, Mat3, Mat4
    };

    // Returns the number of components for a given accessor type
    inline int componentCount(GltfAccessorType t) {
        switch (t) {
        case GltfAccessorType::Scalar: return 1;
        case GltfAccessorType::Vec2:   return 2;
        case GltfAccessorType::Vec3:   return 3;
        case GltfAccessorType::Vec4:   return 4;
        case GltfAccessorType::Mat2:   return 4;
        case GltfAccessorType::Mat3:   return 9;
        case GltfAccessorType::Mat4:   return 16;
        default: return 0;
        }
    }

    // Returns the byte size of a single component
    inline int componentByteSize(GltfComponentType ct) {
        switch (ct) {
        case GltfComponentType::Byte:
        case GltfComponentType::UnsignedByte:  return 1;
        case GltfComponentType::Short:
        case GltfComponentType::UnsignedShort: return 2;
        case GltfComponentType::UnsignedInt:
        case GltfComponentType::Float:         return 4;
        default: return 0;
        }
    }

    struct GltfBuffer {
        std::vector<uint8_t> data;
    };

    struct GltfBufferView {
        int    bufferIndex = -1;
        size_t byteOffset = 0;
        size_t byteLength = 0;
        int    byteStride = 0; // 0 = tightly packed
        int    target = 0; // 34962=ARRAY_BUFFER, 34963=ELEMENT_ARRAY_BUFFER
    };

    struct GltfAccessor {
        int               bufferViewIndex = -1;
        size_t            byteOffset = 0;
        GltfComponentType componentType = GltfComponentType::Float;
        GltfAccessorType  type = GltfAccessorType::Scalar;
        size_t            count = 0;
        bool              normalized = false;
    };

    struct GltfImage {
        std::string uri;            // external file or data URI
        int         bufferViewIndex = -1;
        std::string mimeType;
        // Decoded pixel data (filled by GltfReader)
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        int channels = 4;
    };

    struct GltfSampler {
        int magFilter = 9729; // LINEAR
        int minFilter = 9729;
        int wrapS = 10497; // REPEAT
        int wrapT = 10497;
    };

    struct GltfTexture {
        int samplerIndex = -1;
        int imageIndex = -1;
    };

    struct GltfTextureInfo {
        int   index = -1;
        int   texCoord = 0;
        float scale = 1.0f; // for normal map
        float strength = 1.0f; // for occlusion
    };

    struct GltfPbrMetallicRoughness {
        glm::vec4        baseColorFactor = { 1.f, 1.f, 1.f, 1.f };
        GltfTextureInfo  baseColorTexture;
        float            metallicFactor = 1.0f;
        float            roughnessFactor = 1.0f;
        GltfTextureInfo  metallicRoughnessTexture;
    };

    struct GltfMaterial {
        std::string             name;
        GltfPbrMetallicRoughness pbrMetallicRoughness;
        GltfTextureInfo         normalTexture;
        GltfTextureInfo         occlusionTexture;
        GltfTextureInfo         emissiveTexture;
        glm::vec3               emissiveFactor = { 0.f, 0.f, 0.f };
        bool                    doubleSided = false;
    };

    // A morph target displaces POSITION by a per-vertex offset, blended by the owning mesh/node's
    // weight for that target index. MMD has no normal/tangent morphs, so only POSITION is modeled
    // (see docs/todo/PLAN_mmd_gltf_unification.md Phase 2).
    struct GltfMorphTarget {
        int positionAccessor = -1; // POSITION displacement (Vec3), one element per base vertex
    };

    struct GltfPrimitive {
        // Attribute accessors (index into GltfDocument::accessors, -1 = absent)
        int positionAccessor = -1;
        int normalAccessor = -1;
        int texCoord0Accessor = -1;
        int tangentAccessor = -1;
        int jointsAccessor = -1;  // JOINTS_0, read as glm::ivec4 (widened to UnsignedInt regardless of source type)
        int weightsAccessor = -1; // WEIGHTS_0, read as glm::vec4
        int indicesAccessor = -1;
        int materialIndex = -1;
        std::vector<GltfMorphTarget> targets; // empty = no morph targets
    };

    struct GltfMesh {
        std::string                name;
        std::vector<GltfPrimitive> primitives;
        std::vector<float>         weights; // default morph weights, one per target index (usually all 0)
    };

    struct GltfNode {
        std::string       name;
        int               meshIndex = -1;
        int               skin = -1; // index into GltfDocument::skins, -1 = not skinned
        std::vector<int>  children;
        // Transform (either matrix or TRS)
        bool      hasMatrix = false;
        glm::mat4 matrix = glm::mat4(1.f);
        glm::vec3 translation = { 0.f, 0.f, 0.f };
        glm::vec4 rotation = { 0.f, 0.f, 0.f, 1.f }; // quaternion xyzw
        glm::vec3 scale = { 1.f, 1.f, 1.f };
        std::vector<float> weights; // overrides GltfMesh::weights when non-empty
    };

    // A skin binds a mesh to a set of joint nodes for GPU skinning. joints[i]'s
    // inverseBindMatrices[i] transforms a vertex from bind-pose mesh space into joint i's local
    // space; the renderer combines it with that joint's current global transform each frame.
    // Position within `joints` (not the node index itself) is what JOINTS_0 vertex values refer to.
    struct GltfSkin {
        std::string             name;
        std::vector<int>        joints;               // node indices
        std::vector<glm::mat4>  inverseBindMatrices;   // one per joints[i]; identity-filled if the source omitted them
        int                     skeletonRoot = -1;     // node index, informational only
    };

    struct GltfScene {
        std::string      name;
        std::vector<int> nodes;
    };

    enum class GltfInterpolation { Linear, Step, CubicSpline };
    enum class GltfAnimationPath { Translation, Rotation, Scale, Weights };

    struct GltfAnimationSampler {
        int input  = -1; // accessor: keyframe times (Scalar, Float, seconds)
        int output = -1; // accessor: values (Translation/Scale=Vec3, Rotation=Vec4 xyzw, Weights=flattened Scalar array)
        GltfInterpolation interpolation = GltfInterpolation::Linear;
    };

    struct GltfAnimationChannelTarget {
        int node = -1;
        GltfAnimationPath path = GltfAnimationPath::Translation;
    };

    struct GltfAnimationChannel {
        int samplerIndex = -1;
        GltfAnimationChannelTarget target;
    };

    struct GltfAnimation {
        std::string                       name;
        std::vector<GltfAnimationSampler> samplers;
        std::vector<GltfAnimationChannel> channels;
    };

}