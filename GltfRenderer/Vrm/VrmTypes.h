#pragma once

#include "../Gltf/GltfDocument.h"

#include <map>
#include <string>
#include <vector>

namespace Phantom::Gltf {

// VRM ("Virtual Reality Model") is a glTF 2.0 profile: an ordinary glTF/glb document plus a root
// extension carrying a humanoid bone map, named facial expressions (built from the document's own
// glTF morph targets), and metadata. Two incompatible JSON shapes exist in the wild:
// VRM 0.x ("extensions.VRM") and VRM 1.0 ("extensions.VRMC_vrm") -- see VrmReader.
enum class VrmSpecVersion { Unknown, V0, V1 };

// Standard VRM humanoid bone name (e.g. "hips", "spine", "leftUpperArm", ...) -> glTF node index.
// Mirrors Phantom::Animation::Skeleton::boneNameToIndex's shape, but keyed on VRM's own bone
// vocabulary and pointing directly at glTF nodes (VRM bones ARE glTF nodes, so no conversion is
// needed the way PMX bones need PMXConverter).
struct VrmHumanoid {
    std::map<std::string, int> boneNameToNode;
};

// One glTF morph target contribution to a named expression. `weight` is normalized to 0.0-1.0
// regardless of source spec (VRM 0.x binds store 0-100 in JSON; this is divided down at parse
// time -- see VrmExtensionParser).
struct VrmMorphBind {
    int   node        = -1; // glTF node index (owns the mesh whose primitives carry the target)
    int   targetIndex = -1; // index into that mesh's primitives[].targets
    float weight      = 1.0f;
};

// One named facial expression (VRM 1.0 "expression") / blend shape group (VRM 0.x). `presetName`
// is the standard preset id (e.g. "happy", "blink", "aa") when the expression maps to one, empty
// for a custom/unmapped expression -- UI should prefer presetName as the display label when set.
struct VrmExpression {
    std::string               name;
    std::string               presetName;
    std::vector<VrmMorphBind> binds;
    bool                      isBinary = false; // true = on/off only, no intermediate blending
};

// Best-effort common subset of VRM 0.x ("title"/"author"/"licenseName") and VRM 1.0
// ("name"/"authors[]"/"licenseUrl") metadata field names, normalized to one shape.
struct VrmMeta {
    std::string title;
    std::string version;
    std::string author;
    std::string licenseName;
};

// A fully-loaded VRM asset: ordinary glTF geometry/materials/skins (renderable as-is through
// GltfSceneRenderer, no changes needed there) plus the VRM-specific metadata layered on top.
struct VrmDocument {
    GltfDocument   gltf;
    VrmSpecVersion specVersion = VrmSpecVersion::Unknown;
    VrmHumanoid    humanoid;
    std::vector<VrmExpression> expressions;
    VrmMeta        meta;
};

}
