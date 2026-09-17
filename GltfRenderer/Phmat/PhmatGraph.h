#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <string>
#include <vector>

namespace Phantom::Gltf::Phmat
{

// Phase 4C, first vertical slice: a small, versioned material *graph* source format
// (".phmat", JSON) that compiles down to a gltf.frag-compatible GLSL fragment shader (see
// PhmatCompiler.h) -- SPIR-V is a regenerable cache of that compile, never the source of
// truth (docs/todo/PLAN_blender_universe_authoring_loop.md Phase 4C item 1). The node set is
// deliberately limited to what item 2 names: constant, texture, UV, normal (mapping), mix,
// math, and a single terminal PBR-output node. All non-constant node inputs are references to
// another node's `id` (a JSON string) rather than inline literals -- keeps the grammar uniform
// (one rule: "an input is a node id") at the cost of needing an explicit `constant` node for
// every literal, including mix factors and the normal-map scale.

enum class ValueType { Float, Vec2, Vec3, Vec4 };

enum class NodeType { Constant, TextureSlot, Uv, NormalMap, Mix, Math, Custom, PbrOutput };

// Mirrors GltfGpuMaterial::TEXTURE_SLOT_COUNT's 5 fixed slots (GltfMaterial.h) -- a `.phmat`
// graph drives how these already-bound textures are combined, it does not introduce new texture
// bindings of its own (that would need a new/larger set=1 descriptor layout; left for a later
// increment, see the plan's Phase 4C notes).
enum class TextureSlot { BaseColor, MetallicRoughness, Normal, Occlusion, Emissive };

enum class MathOp { Add, Subtract, Multiply, Divide, Min, Max };

// One node. Every node type only reads the fields it needs; the rest stay at their defaults.
// A single flat struct (instead of a variant/subclass hierarchy) matches the small, fixed node
// set this first slice supports -- see PLAN item 2 "最初のnodeは...に限定する".
struct PhmatNode {
    std::string id;
    NodeType    type = NodeType::Constant;

    // Constant: output type is ValueType::Float for a bare JSON number, VecN for an N-element
    // JSON array (N in [2,4]). Unused trailing components of constantValue are ignored.
    ValueType constantType  = ValueType::Float;
    glm::vec4 constantValue{ 0.0f };

    // TextureSlot: samples the given slot at UV0 (fragTexCoord) -- always unconditionally, since
    // GltfGpuMaterial::build() already binds a 1x1 white fallback for any slot the glTF material
    // didn't populate (GltfMaterial.cpp), so there is no "hasXTex" branch to reproduce here.
    TextureSlot textureSlot = TextureSlot::BaseColor;

    // Uv: only set=0 (fragTexCoord) is wired in the base gltf.frag this compiles against (see
    // PhmatCompiler.h's header template) -- set=1 is a validation error in this first slice.
    int uvSet = 0;

    // NormalMap: samples the Normal texture slot's tangent-space normal and transforms it into
    // the same shading-normal space gltf.frag's fixed path uses (TBN from fragTangent/
    // fragBitangent/fragNormal). `normalMapScale` is a required node id (Float) -- the per-slot
    // MaterialUBO::normalScale a fixed material uses is not available to a graph-driven one.
    std::string normalMapScale;

    // Mix: linear-interpolates `mixA`/`mixB` (must share the same ValueType) by `mixFactor`
    // (must be Float). All three are required node id references.
    std::string mixA, mixB, mixFactor;

    // Math: elementwise binary op over `mathA`/`mathB` (must share the same ValueType).
    MathOp      mathOp = MathOp::Add;
    std::string mathA, mathB;

    // Custom: calls a GLSL function defined in a separate ".phshader" file (PhmatCompiler.h,
    // plan item 4 "custom GLSLは`.phshader`参照として別管理する"). Unlike every other node type,
    // a Custom node's input/output types cannot be derived structurally -- the actual .phshader
    // file is only read later, at compile time (PhmatCompiler.h's loadPhmatMaterial(), which stays
    // the one place in this pipeline that touches the filesystem for a graph's own nodes). So the
    // node declares its own signature redundantly here, in customInputTypes/customOutputType, so
    // parsePhmatGraph()/validateAndSort() can still type-check purely from the graph's own JSON --
    // PhmatCompiler.cpp cross-checks these declared types against the .phshader file's own
    // declared signature at compile time and raises a diagnostic on any mismatch.
    std::string              customPhshaderPath; // .phmat-file-relative path to the .phshader
    std::vector<std::string> customInputs;       // node id references, positional
    std::vector<ValueType>   customInputTypes;   // customInputTypes[i] is customInputs[i]'s required type
    ValueType                customOutputType = ValueType::Float;

    // PbrOutput: the graph's terminal node (referenced by PhmatGraph::outputNode). baseColor/
    // metallic/roughness are required; normal/occlusion/emissive/alpha are optional (empty
    // string = use the same default gltf.frag's fixed path uses -- see PhmatCompiler.cpp).
    std::string pbrBaseColor;  // required, Vec4
    std::string pbrMetallic;   // required, Float
    std::string pbrRoughness;  // required, Float
    std::string pbrNormal;     // optional, Vec3 -- default normalize(fragNormal)
    std::string pbrOcclusion;  // optional, Float -- default 1.0 (no attenuation)
    std::string pbrEmissive;   // optional, Vec3 -- default vec3(0.0)
    std::string pbrAlpha;      // optional, Float -- default baseColor.a
};

struct PhmatGraph {
    int                     version = 1;
    std::vector<PhmatNode>  nodes;
    std::string             outputNode; // id of the (sole) PbrOutput node
};

struct PhmatDiagnostic {
    enum class Severity { Error, Warning };
    Severity    severity = Severity::Error;
    std::string nodeId;  // empty for a graph-level diagnostic (e.g. version, missing output)
    std::string message;
};

// Parses a `.phmat` JSON document's text. Returns false (outGraph left partially/fully
// populated, but not to be trusted) on malformed JSON, an unrecognized top-level shape, an
// unknown/unsupported node "type" string, or a missing required per-node-type field --
// diagnostics always explains why. Does not check references, types, or cycles -- see
// validateAndSort() for that.
bool parsePhmatGraph(const std::string& jsonText, PhmatGraph& outGraph,
                      std::vector<PhmatDiagnostic>& outDiagnostics);

// Validates node id uniqueness, that every input references an existing node, that referenced
// types are compatible with how each node type uses them, and that the graph is acyclic
// (Kahn's algorithm doubles as the topological sort below). Returns false (outTopoOrder left
// empty) if any error-severity diagnostic was raised; warnings do not block success.
bool validateAndSort(const PhmatGraph& graph, std::vector<std::string>& outTopoOrder,
                      std::vector<PhmatDiagnostic>& outDiagnostics);

}
