#include "VrmMToonFallback.h"

using namespace Phantom::Gltf;
using Json = nlohmann::json;

namespace {

bool hasExtension(const std::vector<std::pair<std::string, std::string>>& exts, const char* name) {
    for (const auto& e : exts)
        if (e.first == name) return true;
    return false;
}

bool isMToonOrUnlitShader(const std::string& shader) {
    return shader == "VRM/MToon" || shader == "VRM/UnlitTexture" ||
           shader == "VRM/UnlitTransparent" || shader == "VRM/UnlitCutout" ||
           shader == "VRM/UnlitTransparentZWrite";
}

} // namespace

void VrmMToonFallback::apply(GltfDocument& doc, const Json& vrmRoot, VrmSpecVersion ver,
                              const Phantom::File::GLTFFile& src) {
    // Malformed materialProperties/extension JSON degrades to "leave whatever materials were
    // already updated"; not fatal -- same tolerant-input convention as VrmExtensionParser.
    try {
        if (ver == VrmSpecVersion::V1) {
            for (size_t i = 0; i < doc.materials.size() && i < src.materials.size(); ++i) {
                if (!hasExtension(src.materials[i].extensionsJson, "VRMC_materials_mtoon")) continue;
                doc.materials[i].pbrMetallicRoughness.metallicFactor  = 0.0f;
                doc.materials[i].pbrMetallicRoughness.roughnessFactor = 1.0f;
            }
            return;
        }

        if (ver != VrmSpecVersion::V0) return;
        if (!vrmRoot.is_object() || !vrmRoot.contains("materialProperties")) return;
        const Json& props = vrmRoot["materialProperties"];
        if (!props.is_array()) return;

        for (size_t i = 0; i < props.size() && i < doc.materials.size(); ++i) {
            const Json& p = props[i];
            if (!p.is_object()) continue;
            if (!isMToonOrUnlitShader(p.value("shader", std::string()))) continue;

            if (p.contains("vectorProperties") && p["vectorProperties"].is_object()) {
                const Json& vp = p["vectorProperties"];
                if (vp.contains("_Color") && vp["_Color"].is_array()) {
                    const Json& c = vp["_Color"];
                    glm::vec4 color = doc.materials[i].pbrMetallicRoughness.baseColorFactor;
                    for (size_t k = 0; k < c.size() && k < 4; ++k)
                        if (c[k].is_number()) color[static_cast<int>(k)] = c[k].get<float>();
                    doc.materials[i].pbrMetallicRoughness.baseColorFactor = color;
                }
            }
            if (p.contains("textureProperties") && p["textureProperties"].is_object()) {
                const Json& tp = p["textureProperties"];
                if (tp.contains("_MainTex") && tp["_MainTex"].is_number()) {
                    doc.materials[i].pbrMetallicRoughness.baseColorTexture.index = tp["_MainTex"].get<int>();
                }
            }
            doc.materials[i].pbrMetallicRoughness.metallicFactor  = 0.0f;
            doc.materials[i].pbrMetallicRoughness.roughnessFactor = 1.0f;
        }
    } catch (const Json::exception&) {
        return;
    }
}
