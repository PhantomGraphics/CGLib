#include "VrmExtensionParser.h"

using namespace Phantom::Gltf;
using Json = nlohmann::json;

namespace {

std::string jStr(const Json& j, const char* key, const std::string& fallback = "") {
    if (!j.is_object() || !j.contains(key) || !j[key].is_string()) return fallback;
    return j[key].get<std::string>();
}

int jInt(const Json& j, const char* key, int fallback = -1) {
    if (!j.is_object() || !j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<int>();
}

float jFloat(const Json& j, const char* key, float fallback = 0.f) {
    if (!j.is_object() || !j.contains(key) || !j[key].is_number()) return fallback;
    return j[key].get<float>();
}

bool jBool(const Json& j, const char* key, bool fallback = false) {
    if (!j.is_object() || !j.contains(key) || !j[key].is_boolean()) return fallback;
    return j[key].get<bool>();
}

// VRM 0.x blendShapeMaster.binds[].mesh is documented inconsistently across exporters: most
// (including the reference UniVRM implementation) write the glTF *node* index of the node that
// owns the mesh, not the raw glTF meshes[] index. Try that interpretation first; fall back to
// treating it as a mesh index and finding the (first) node that references it.
int resolveVrm0BindNode(const GltfDocument& doc, int meshOrNode) {
    if (meshOrNode >= 0 && meshOrNode < static_cast<int>(doc.nodes.size()) &&
        doc.nodes[meshOrNode].meshIndex >= 0) {
        return meshOrNode;
    }
    for (size_t i = 0; i < doc.nodes.size(); ++i) {
        if (doc.nodes[i].meshIndex == meshOrNode) return static_cast<int>(i);
    }
    return -1;
}

void parseBindsV1(const Json& binds, std::vector<VrmMorphBind>& out) {
    if (!binds.is_array()) return;
    for (const auto& b : binds) {
        VrmMorphBind bind;
        bind.node        = jInt(b, "node");
        bind.targetIndex = jInt(b, "index");
        bind.weight      = jFloat(b, "weight", 1.0f);
        if (bind.node >= 0 && bind.targetIndex >= 0) out.push_back(bind);
    }
}

void parseBindsV0(const Json& binds, const GltfDocument& doc, std::vector<VrmMorphBind>& out) {
    if (!binds.is_array()) return;
    for (const auto& b : binds) {
        VrmMorphBind bind;
        bind.node        = resolveVrm0BindNode(doc, jInt(b, "mesh"));
        bind.targetIndex = jInt(b, "index");
        bind.weight      = jFloat(b, "weight", 100.0f) / 100.0f;
        if (bind.node >= 0 && bind.targetIndex >= 0) out.push_back(bind);
    }
}

} // namespace

VrmHumanoid VrmExtensionParser::parseHumanoid(const Json& vrmRoot, VrmSpecVersion ver) {
    VrmHumanoid humanoid;
    // Malformed/unexpected-shape VRM JSON degrades to an empty humanoid rather than throwing --
    // per the project's "try/catch is the one allowed exception boundary, for
    // external library input" convention.
    try {
        if (!vrmRoot.is_object() || !vrmRoot.contains("humanoid")) return humanoid;
        const Json& humanBones = vrmRoot["humanoid"].value("humanBones", Json());

        if (ver == VrmSpecVersion::V1) {
            if (!humanBones.is_object()) return humanoid;
            for (auto it = humanBones.begin(); it != humanBones.end(); ++it) {
                const int node = jInt(it.value(), "node");
                if (node >= 0) humanoid.boneNameToNode[it.key()] = node;
            }
        } else if (ver == VrmSpecVersion::V0) {
            if (!humanBones.is_array()) return humanoid;
            for (const auto& b : humanBones) {
                const std::string bone = jStr(b, "bone");
                const int node = jInt(b, "node");
                if (!bone.empty() && node >= 0) humanoid.boneNameToNode[bone] = node;
            }
        }
    } catch (const Json::exception&) {
        return VrmHumanoid{};
    }
    return humanoid;
}

std::vector<VrmExpression> VrmExtensionParser::parseExpressions(const Json& vrmRoot, VrmSpecVersion ver,
                                                                  const GltfDocument& doc) {
    std::vector<VrmExpression> result;
    try {
        if (!vrmRoot.is_object()) return result;

        if (ver == VrmSpecVersion::V1) {
            if (!vrmRoot.contains("expressions")) return result;
            const Json& expr = vrmRoot["expressions"];
            auto collect = [&](const Json& group, bool isPreset) {
                if (!group.is_object()) return;
                for (auto it = group.begin(); it != group.end(); ++it) {
                    VrmExpression e;
                    e.name       = it.key();
                    e.presetName = isPreset ? it.key() : std::string();
                    e.isBinary   = jBool(it.value(), "isBinary", false);
                    parseBindsV1(it.value().value("morphTargetBinds", Json()), e.binds);
                    result.push_back(std::move(e));
                }
            };
            collect(expr.value("preset", Json()), true);
            collect(expr.value("custom", Json()), false);
        } else if (ver == VrmSpecVersion::V0) {
            if (!vrmRoot.contains("blendShapeMaster")) return result;
            const Json& groups = vrmRoot["blendShapeMaster"].value("blendShapeGroups", Json());
            if (!groups.is_array()) return result;
            for (const auto& g : groups) {
                VrmExpression e;
                e.name       = jStr(g, "name");
                e.presetName = jStr(g, "presetName");
                e.isBinary   = jBool(g, "isBinary", false);
                parseBindsV0(g.value("binds", Json()), doc, e.binds);
                result.push_back(std::move(e));
            }
        }
    } catch (const Json::exception&) {
        return {};
    }
    return result;
}

VrmMeta VrmExtensionParser::parseMeta(const Json& vrmRoot, VrmSpecVersion ver) {
    VrmMeta meta;
    try {
        if (!vrmRoot.is_object() || !vrmRoot.contains("meta")) return meta;
        const Json& m = vrmRoot["meta"];

        if (ver == VrmSpecVersion::V1) {
            meta.title = jStr(m, "name");
            meta.version = jStr(m, "version");
            if (m.is_object() && m.contains("authors") && m["authors"].is_array() && !m["authors"].empty()) {
                std::string joined;
                for (const auto& a : m["authors"]) {
                    if (!a.is_string()) continue;
                    if (!joined.empty()) joined += ", ";
                    joined += a.get<std::string>();
                }
                meta.author = joined;
            }
            meta.licenseName = jStr(m, "licenseUrl"); // VRM1 has no plain license name, only a URL
        } else if (ver == VrmSpecVersion::V0) {
            meta.title       = jStr(m, "title");
            meta.version     = jStr(m, "version");
            meta.author      = jStr(m, "author");
            meta.licenseName = jStr(m, "licenseName");
        }
    } catch (const Json::exception&) {
        return VrmMeta{};
    }
    return meta;
}
