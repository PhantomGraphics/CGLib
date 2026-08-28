#pragma once

#include "VrmTypes.h"
#include "../Gltf/GltfDocument.h"
#include "json.hpp"

namespace Phantom::Gltf {

// Pure functions that turn an already-parsed VRM root extension object (the JSON value at
// extensions.VRM for VrmSpecVersion::V0, or extensions.VRMC_vrm for V1) into VrmTypes.h structs.
// No file I/O, no cgltf dependency -- easy to unit test with inline JSON literals. Malformed
// input (wrong types, missing fields) degrades to empty/default results rather than throwing;
// see VrmExtensionParser.cpp's top comment for why a try/catch is still used internally.
class VrmExtensionParser {
public:
    static VrmHumanoid parseHumanoid(const nlohmann::json& vrmRoot, VrmSpecVersion ver);

    // Needs `doc` (already-built glTF geometry) only to resolve VRM 0.x's ambiguous
    // blendShapeMaster.binds[].mesh field, which some exporters write as a node index and others
    // as a raw glTF mesh index -- see the .cpp for the resolution heuristic.
    static std::vector<VrmExpression> parseExpressions(const nlohmann::json& vrmRoot, VrmSpecVersion ver,
                                                         const GltfDocument& doc);

    static VrmMeta parseMeta(const nlohmann::json& vrmRoot, VrmSpecVersion ver);
};

}
