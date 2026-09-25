#include "UniverseSceneV2.h"

#include "CGLib/AssetCore/AssetCore/AssetUri.h"

#include "json.hpp"

namespace Phantom::SceneRuntime::Universe {

namespace {

// Mirrors SceneGraph.cpp's own vec3FromJson()/quatFromJson() -- kept as a small local copy
// rather than a shared header since both are a handful of lines and this module otherwise
// has no reason to depend on SceneGraph.cpp's translation unit internals.
Phantom::Math::Vector3df vec3FromJson(const nlohmann::json& parent, const char* key, const Phantom::Math::Vector3df& fallback)
{
    if (!parent.contains(key)) return fallback;
    const nlohmann::json& j = parent[key];
    if (!j.is_array() || j.size() != 3) return fallback;
    return Phantom::Math::Vector3df(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

Phantom::Math::Quaternion quatFromJson(const nlohmann::json& parent, const char* key)
{
    const Phantom::Math::Quaternion identity{ 1.0f, 0.0f, 0.0f, 0.0f };
    if (!parent.contains(key)) return identity;
    const nlohmann::json& j = parent[key];
    if (!j.is_array() || j.size() != 4) return identity;
    return Phantom::Math::Quaternion(j[3].get<float>(), j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

} // namespace

std::string SceneV2::toJson() const
{
    nlohmann::json root;
    root["version"] = kVersion;
    root["assets"] = nlohmann::json::parse(assets.toJson());
    if (!assetRoot.empty()) root["assetRoot"] = assetRoot;
    root["scene"] = nlohmann::json::parse(scene.toJson());
    root["physics"] = physics;
    root["renderSettings"] = renderSettings;
    return root.dump();
}

SceneV2 SceneV2::fromJson(const std::string& json, bool* ok)
{
    if (ok) *ok = false;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return SceneV2{};
    }
    if (!root.is_object() || root.value("version", 0) != kVersion) return SceneV2{};
    if (!root.contains("assets") || !root.contains("scene")) return SceneV2{};

    SceneV2 result;
    result.assetRoot = root.value("assetRoot", std::string());
    bool assetsOk = false;
    result.assets = Phantom::Asset::AssetManifest::fromJson(root["assets"].dump(), &assetsOk);
    if (!assetsOk) return SceneV2{};

    bool sceneOk = false;
    result.scene = SceneGraph::fromJson(root["scene"].dump(), &sceneOk);
    if (!sceneOk) return SceneV2{};

    result.physics = root.value("physics", nlohmann::json::object());
    result.renderSettings = root.value("renderSettings", nlohmann::json::object());

    if (ok) *ok = true;
    return result;
}

MigrationResult migrateV1ToV2(const std::string& v1Json)
{
    MigrationResult result;

    nlohmann::json root;
    try {
        root = nlohmann::json::parse(v1Json);
    } catch (const nlohmann::json::parse_error&) {
        result.diagnostics.push_back("v1 JSON parse error");
        return result;
    }
    if (!root.is_object() || root.value("version", 0) != 1) {
        result.diagnostics.push_back("not a v1 .universe document (missing or wrong \"version\")");
        return result;
    }
    if (!root.contains("entities") || !root["entities"].is_array()) {
        result.diagnostics.push_back("missing \"entities\" array");
        return result;
    }

    SceneV2 v2;
    for (const auto& e : root["entities"]) {
        if (!e.is_object()) {
            result.diagnostics.push_back("skipped a non-object entity");
            continue;
        }

        SceneNode node;
        node.id = Phantom::Asset::AssetId::generate(); // v1 never persisted a stable id
        node.name = e.value("name", "");
        node.enabled = e.value("visible", true);

        if (e.contains("transform") && e["transform"].is_object()) {
            const auto& t = e["transform"];
            node.local.translation = vec3FromJson(t, "pos", { 0.0f, 0.0f, 0.0f });
            node.local.rotation = quatFromJson(t, "rot");
            node.local.scale = vec3FromJson(t, "scale", { 1.0f, 1.0f, 1.0f });
        }

        if (e.contains("mesh") && e["mesh"].is_object()) {
            const auto& mesh = e["mesh"];
            const std::string source = mesh.value("source", "primitive");
            if (source == "gltf") {
                const std::string path = mesh.value("path", "");
                auto uri = Phantom::Asset::AssetUri::parse(path);
                if (uri) {
                    const Phantom::Asset::AssetId assetId = Phantom::Asset::AssetId::generate();
                    Phantom::Asset::AssetManifestEntry entry;
                    entry.id = assetId;
                    entry.uri = *uri;
                    v2.assets.upsert(entry);

                    ComponentRecord comp;
                    comp.type = "meshAsset";
                    comp.data = { { "assetId", assetId.value() }, { "nodeId", "" } };
                    if (mesh.contains("nodeName")) comp.data["nodeName"] = mesh["nodeName"];
                    node.components.push_back(std::move(comp));
                } else {
                    result.diagnostics.push_back("entity '" + node.name + "': gltf mesh path '" + path
                        + "' is not a valid project-relative URI, meshAsset dropped");
                }
            } else if (source == "embedded" || source == "primitive") {
                // Neither has a backing asset file to reference -- embedded mesh data has no
                // source file by design (CGApp/Universe/CLAUDE.md's ".universeシーンファイル
                // 形式" section), primitives are procedural. Both stay inline verbatim.
                ComponentRecord comp;
                comp.type = "mesh";
                comp.data = mesh;
                node.components.push_back(std::move(comp));
            } else {
                result.diagnostics.push_back("entity '" + node.name + "': unknown mesh.source '" + source
                    + "', mesh component dropped");
            }
        }

        for (const char* key : { "rigidBody", "cloth", "fluid", "volume", "light" }) {
            if (e.contains(key) && e[key].is_object()) {
                ComponentRecord comp;
                comp.type = key;
                comp.data = e[key];
                node.components.push_back(std::move(comp));
            }
        }

        const std::string nodeName = node.name; // addNode() may move from `node` below
        if (!v2.scene.addNode(std::move(node))) {
            result.diagnostics.push_back("entity '" + nodeName + "': failed to add to the scene graph");
        }
    }

    v2.physics = root.value("physics", nlohmann::json::object());
    v2.renderSettings = root.value("renderSettings", nlohmann::json::object());

    result.scene = std::move(v2);
    result.ok = true;
    return result;
}

} // namespace Phantom::SceneRuntime::Universe
