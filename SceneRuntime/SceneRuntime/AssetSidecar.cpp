#define GLM_ENABLE_EXPERIMENTAL
#include "AssetSidecar.h"

#include "CGLib/AssetCore/AssetCore/ContentHash.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>

namespace Phantom::SceneRuntime {

using Json = nlohmann::json;

namespace {

bool readVec3(const Json& j, glm::vec3& out)
{
    if (!j.is_array() || j.size() != 3) return false;
    for (int i = 0; i < 3; ++i) {
        if (!j[i].is_number()) return false;
        out[i] = j[i].get<float>();
    }
    return true;
}

bool finite3(const glm::vec3& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

std::string readAll(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

} // namespace

std::filesystem::path sidecarPathFor(const std::filesystem::path& assetPath)
{
    auto sidecar = assetPath;
    sidecar.replace_extension(".phantom.json");
    return sidecar;
}

bool parseSidecarRigidBody(const Json& body, SidecarRigidBody& parsed,
                           std::string& diagnostic, SidecarSeverity& severity)
{
    severity = SidecarSeverity::Warn;
    if (body.contains("schema") && body.value("schema", "") != "phantom.component_schema/1") {
        diagnostic = "unsupported rigidBody component schema";
        return false;
    }
    if (body.contains("version") && !body["version"].is_number_integer()) {
        diagnostic = "rigidBody component \"version\" is not an integer";
        return false;
    }
    parsed.version = body.value("version", 1);
    if (parsed.version < 1 || parsed.version > kRigidBodySchemaVersion) {
        severity = SidecarSeverity::Error;
        diagnostic = "rigidBody component schema version " + std::to_string(parsed.version)
                   + " is not supported (this build reads versions 1-"
                   + std::to_string(kRigidBodySchemaVersion) + ")";
        return false;
    }
    parsed.raw = body;

    if (body.contains("mass") && body["mass"].is_number()) parsed.mass = body["mass"].get<float>();
    if (body.contains("restitution") && body["restitution"].is_number()) parsed.restitution = body["restitution"].get<float>();
    if (body.contains("friction") && body["friction"].is_number()) parsed.friction = body["friction"].get<float>();
    if (body.contains("twoWay") && body["twoWay"].is_boolean()) parsed.twoWay = body["twoWay"].get<bool>();

    ColliderDesc& c = parsed.collider;
    if (parsed.version == 1) {
        float size = 0.5f;
        if (body.contains("shape") && body["shape"].is_string()) c.shape = body["shape"].get<std::string>();
        if (body.contains("size") && body["size"].is_number()) size = body["size"].get<float>();
        if (c.shape == "capsule") c.shape = "unsupported-in-v1";
        c.halfExtents = { size, size, size };
        c.radius = size;
        if (!std::isfinite(size) || size <= 0.f) {
            diagnostic = "rigidBody sidecar contains an invalid physical value";
            return false;
        }
    } else {
        if (!body.contains("collider") || !body["collider"].is_object()) {
            diagnostic = "rigidBody component version " + std::to_string(parsed.version)
                       + " has no \"collider\" block";
            return false;
        }
        const Json& jc = body["collider"];
        c.shape = jc.value("shape", body.value("shape", std::string("box")));
        bool ok = true;
        if (jc.contains("halfExtents")) ok = readVec3(jc["halfExtents"], c.halfExtents) && ok;
        if (jc.contains("center")) ok = readVec3(jc["center"], c.center) && ok;
        if (jc.contains("radius")) ok = jc["radius"].is_number() && ok;
        if (jc.contains("halfHeight")) ok = jc["halfHeight"].is_number() && ok;
        if (ok && jc.contains("radius")) c.radius = jc["radius"].get<float>();
        if (ok && jc.contains("halfHeight")) c.halfHeight = jc["halfHeight"].get<float>();
        if (!ok || !finite3(c.halfExtents) || !finite3(c.center)
            || !std::isfinite(c.radius) || !std::isfinite(c.halfHeight)
            || c.halfExtents.x <= 0.f || c.halfExtents.y <= 0.f || c.halfExtents.z <= 0.f
            || c.radius <= 0.f || c.halfHeight < 0.f) {
            diagnostic = "rigidBody collider block contains an invalid value";
            return false;
        }
        parsed.scaleWithEntity = true;
    }

    if (!isKnownColliderShape(c.shape)) {
        diagnostic = "unsupported rigidBody shape '" + c.shape + "'";
        return false;
    }
    if (!std::isfinite(parsed.mass) || !std::isfinite(parsed.restitution) || !std::isfinite(parsed.friction)
        || parsed.mass < 0.f
        || parsed.restitution < 0.f || parsed.restitution > 1.f
        || parsed.friction < 0.f || parsed.friction > 1.f) {
        diagnostic = "rigidBody sidecar contains an invalid physical value";
        return false;
    }
    return true;
}

bool verifyPublishedPair(const std::filesystem::path& sidecar, const std::filesystem::path& glb,
                         std::string& diagnostic)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(sidecar, ec)) return true;
    std::ifstream input(sidecar, std::ios::binary);
    if (!input) return true; // reported by readSidecarObjects()
    const Json root = Json::parse(input, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return true; // ditto
    const std::string expected = root.value("contentHash", std::string());
    if (expected.empty()) return true;
    const auto actual = Phantom::Asset::ContentHash::fromFile(glb);
    if (!actual) {
        diagnostic = "publish_incomplete: cannot read '" + glb.string() + "' to verify its sidecar";
        return false;
    }
    if (actual->value() != expected) {
        diagnostic = "publish_incomplete: '" + sidecar.filename().string() + "' describes a different export of '"
                   + glb.filename().string() + "' (hash mismatch); the re-export is probably still being written";
        return false;
    }
    return true;
}

bool verifyPublishedGeneration(const std::filesystem::path& glb, std::string& diagnostic)
{
    std::error_code ec;
    std::filesystem::path manifest = glb;
    manifest.replace_extension(".publish.json");
    if (!std::filesystem::is_regular_file(manifest, ec)) return true;
    const std::string name = manifest.filename().string();
    std::ifstream input(manifest, std::ios::binary);
    const Json root = input ? Json::parse(input, nullptr, false) : Json();
    if (root.is_discarded() || !root.is_object() || root.value("schema", "") != "phantom.publish/1" ||
        !root.contains("artifacts") || !root["artifacts"].is_array()) {
        diagnostic = "publish_incomplete: '" + name + "' is not a valid phantom.publish/1 manifest";
        return false;
    }
    const std::string generation = root.value("generation", std::string());
    for (const auto& artifact : root["artifacts"]) {
        const std::string rel = artifact.is_object() ? artifact.value("path", std::string()) : std::string();
        const std::string expected = artifact.is_object() ? artifact.value("sha256", std::string()) : std::string();
        if (rel.empty() || expected.empty()) {
            diagnostic = "publish_incomplete: '" + name + "' has an artifact without path or sha256";
            return false;
        }
        const auto actual = Phantom::Asset::ContentHash::fromFile(glb.parent_path() / std::filesystem::path(std::u8string(rel.begin(), rel.end())));
        if (!actual || actual->value() != expected) {
            diagnostic = "publish_incomplete: '" + rel + "' does not match generation " + generation + " in '" + name +
                         "'; the export is probably still being written";
            return false;
        }
    }
    return true;
}

bool readSidecarObjects(const std::filesystem::path& path, std::vector<SidecarObject>& result,
                        std::string& diagnostic, SidecarSeverity* severityOut)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) return true;

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        diagnostic = "could not open sidecar '" + path.string() + "'";
        return false;
    }
    Json root = Json::parse(input, nullptr, false);
    if (root.is_discarded() || !root.is_object() || root.value("schema", "") != "phantom.asset/1") {
        diagnostic = "invalid phantom.asset/1 sidecar";
        return false;
    }
    if (!root.contains("objects") || !root["objects"].is_array()) {
        diagnostic = "sidecar has no objects array";
        return false;
    }

    for (const auto& object : root["objects"]) {
        if (!object.is_object()) continue;
        const auto name = object.value("name", "");
        if (name.empty()) continue;
        auto objectIt = std::find_if(result.begin(), result.end(), [&](const SidecarObject& item) {
            return item.name == name;
        });
        if (objectIt == result.end()) {
            result.push_back({ name, std::nullopt, {} });
            objectIt = std::prev(result.end());
        }
        objectIt->uuid = object.value("uuid", "");
        if (!object.contains("components") || !object["components"].is_object()) continue;
        const auto& components = object["components"];
        if (!components.contains("rigidBody")) continue;
        if (!components["rigidBody"].is_object()) {
            diagnostic = "rigidBody component is not an object";
            return false;
        }
        SidecarRigidBody parsed;
        parsed.object = name;
        SidecarSeverity severity = SidecarSeverity::Warn;
        if (!parseSidecarRigidBody(components["rigidBody"], parsed, diagnostic, severity)) {
            diagnostic = "'" + name + "': " + diagnostic;
            if (severityOut) *severityOut = severity;
            return false;
        }
        objectIt->rigidBody = std::move(parsed);
    }
    return true;
}

bool readGltfNodeWorldTransforms(const std::filesystem::path& path,
                                 std::unordered_map<std::string, Transform>& out,
                                 std::string& diagnostic)
{
    const std::string data = readAll(path);
    if (data.empty()) { diagnostic = "could not read '" + path.string() + "'"; return false; }

    Json doc;
    if (data.size() >= 20 && std::memcmp(data.data(), "glTF", 4) == 0) {
        // GLB: 12-byte header, then chunks; the first chunk is the JSON.
        std::uint32_t length = 0;
        std::memcpy(&length, data.data() + 12, 4);
        if (20u + length > data.size()) { diagnostic = "truncated GLB JSON chunk"; return false; }
        doc = Json::parse(data.begin() + 20, data.begin() + 20 + length, nullptr, false);
    } else {
        doc = Json::parse(data, nullptr, false);
    }
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("nodes") || !doc["nodes"].is_array()) {
        diagnostic = "'" + path.filename().string() + "' is not a parsable glTF";
        return false;
    }
    const auto& nodes = doc["nodes"];
    const int count = static_cast<int>(nodes.size());

    std::vector<int> parent(count, -1);
    for (int i = 0; i < count; ++i) {
        if (!nodes[i].is_object() || !nodes[i].contains("children") || !nodes[i]["children"].is_array()) continue;
        for (const auto& child : nodes[i]["children"])
            if (child.is_number_integer() && child.get<int>() >= 0 && child.get<int>() < count)
                parent[child.get<int>()] = i;
    }
    const auto local = [&](int i) {
        const Json& n = nodes[i];
        if (n.is_object() && n.contains("matrix") && n["matrix"].is_array() && n["matrix"].size() == 16) {
            glm::mat4 m(1.f);
            for (int k = 0; k < 16; ++k) m[k / 4][k % 4] = n["matrix"][k].get<float>();
            return m;
        }
        glm::vec3 t(0.f), s(1.f);
        glm::quat r(1.f, 0.f, 0.f, 0.f);
        if (n.is_object()) {
            if (n.contains("translation")) readVec3(n["translation"], t);
            if (n.contains("scale")) readVec3(n["scale"], s);
            if (n.contains("rotation") && n["rotation"].is_array() && n["rotation"].size() == 4)
                r = glm::quat(n["rotation"][3].get<float>(), n["rotation"][0].get<float>(),
                              n["rotation"][1].get<float>(), n["rotation"][2].get<float>());
        }
        return glm::translate(glm::mat4(1.f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.f), s);
    };
    std::vector<int> state(count, 0); // 0 new, 1 visiting
    std::function<glm::mat4(int)> world = [&](int i) {
        if (state[i] == 1) return glm::mat4(1.f); // cycle guard
        state[i] = 1;
        const glm::mat4 result = parent[i] < 0 ? local(i) : world(parent[i]) * local(i);
        state[i] = 0;
        return result;
    };

    for (int i = 0; i < count; ++i) {
        if (!nodes[i].is_object()) continue;
        const std::string name = nodes[i].value("name", std::string());
        if (name.empty() || out.count(name)) continue;
        Transform t;
        glm::vec3 skew;
        glm::vec4 perspective;
        if (!glm::decompose(world(i), t.scale, t.rotation, t.translation, skew, perspective)) {
            diagnostic = "node '" + name + "' has a degenerate transform";
            return false;
        }
        out.emplace(name, t);
    }
    return true;
}

} // namespace Phantom::SceneRuntime
