#define GLM_ENABLE_EXPERIMENTAL
#include "SceneV2Merge.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace Phantom::SceneRuntime {

using Json = nlohmann::json;

const char* const kOverridableRigidBodyFields[4] = { "mass", "restitution", "friction", "twoWay" };

bool MergeReport::empty() const
{
    return renamed.empty() && removed.empty() && added.empty() && updated.empty()
        && kept.empty() && conflicts.empty();
}

int MergeReport::count(const std::string& key) const
{
    if (key == "renamed") return static_cast<int>(renamed.size());
    if (key == "removed") return static_cast<int>(removed.size());
    if (key == "added") return static_cast<int>(added.size());
    if (key == "updated") return static_cast<int>(updated.size());
    if (key == "kept") return static_cast<int>(kept.size());
    if (key == "conflicts") return static_cast<int>(conflicts.size());
    return -1;
}

Json MergeReport::toJson() const
{
    return { { "renamed", renamed }, { "removed", removed }, { "added", added },
             { "updated", updated }, { "kept", kept }, { "conflicts", conflicts } };
}

bool sameTransform(const Transform& a, const Transform& b)
{
    const auto close = [](const glm::vec3& x, const glm::vec3& y) { return glm::length(x - y) < 1.e-4f; };
    return close(a.translation, b.translation) && close(a.scale, b.scale)
        && std::abs(glm::dot(glm::normalize(a.rotation), glm::normalize(b.rotation))) > 0.99999f;
}

Json transformToJson(const Transform& t)
{
    return { { "pos", { t.translation.x, t.translation.y, t.translation.z } },
             { "rot", { t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w } },
             { "scale", { t.scale.x, t.scale.y, t.scale.z } } };
}

std::optional<Transform> transformFromJson(const Json& j)
{
    if (!j.is_object()) return std::nullopt;
    const auto read = [](const Json& v, int n, float* result) {
        if (!v.is_array() || v.size() != static_cast<size_t>(n)) return false;
        for (int i = 0; i < n; ++i) {
            if (!v[i].is_number()) return false;
            result[i] = v[i].get<float>();
            if (!std::isfinite(result[i])) return false;
        }
        return true;
    };
    if (!j.contains("pos") || !j.contains("rot") || !j.contains("scale")) return std::nullopt;
    float p[3], r[4], s[3];
    if (!read(j["pos"], 3, p) || !read(j["rot"], 4, r) || !read(j["scale"], 3, s)) return std::nullopt;
    Transform t;
    t.translation = { p[0], p[1], p[2] };
    t.rotation = { r[3], r[0], r[1], r[2] };
    t.scale = { s[0], s[1], s[2] };
    return t;
}

Transform mergeTransform(const std::string& node, const Transform& local,
                         const std::optional<Transform>& base, const Transform& remote,
                         MergeReport& report)
{
    if (!base || !sameTransform(local, *base)) {
        report.kept.push_back(node + ".transform");
        if (base && !sameTransform(remote, *base)) report.conflicts.push_back(node + ".transform");
        return local;
    }
    if (!sameTransform(remote, local)) report.updated.push_back(node + ".transform");
    return remote;
}

namespace {
// A value saved through a float (0.1f -> 0.10000000149...) must still equal the authored double 0.1,
// otherwise every untouched restitution/friction would look like a local override.
bool sameValue(const Json& a, const Json& b)
{
    if (a.is_number() && b.is_number()) return static_cast<float>(a.get<double>()) == static_cast<float>(b.get<double>());
    return a == b;
}
} // namespace

RigidBodyMerge mergeRigidBody(const std::string& node, const Json* local,
                              const SidecarRigidBody* remote, MergeReport& report)
{
    RigidBodyMerge result;
    if (!local) {
        if (remote) {
            result.action = RigidBodyMergeAction::UseMerged;
            result.merged = *remote;
            report.added.push_back(node + ".rigidBody");
        }
        return result;
    }
    const Json base = local->is_object() ? local->value("authored", Json()) : Json();
    if (!base.is_object()) {
        report.kept.push_back(node + ".rigidBody (legacy)");
        return result;
    }
    bool hasOverride = false;
    for (const char* field : kOverridableRigidBodyFields)
        if (local->contains(field) && base.contains(field) && !sameValue((*local)[field], base[field]))
            hasOverride = true;
    if (!remote) {
        result.action = hasOverride ? RigidBodyMergeAction::KeepLocal : RigidBodyMergeAction::Remove;
        (hasOverride ? report.kept : report.removed).push_back(node + ".rigidBody");
        return result;
    }
    result.action = RigidBodyMergeAction::UseMerged;
    result.merged = *remote;
    for (const char* field : kOverridableRigidBodyFields) {
        if (!local->contains(field) || !base.contains(field)) continue;
        const std::string key = node + ".rigidBody." + field;
        if (!sameValue((*local)[field], base[field])) {
            result.merged.raw[field] = (*local)[field];
            if (std::string(field) == "mass") result.merged.mass = (*local)[field].get<float>();
            else if (std::string(field) == "restitution") result.merged.restitution = (*local)[field].get<float>();
            else if (std::string(field) == "friction") result.merged.friction = (*local)[field].get<float>();
            else result.merged.twoWay = (*local)[field].get<bool>();
            report.kept.push_back(key);
            if (remote->raw.contains(field) && !sameValue(remote->raw[field], base[field]))
                report.conflicts.push_back(key);
        } else if (remote->raw.contains(field) && !sameValue(remote->raw[field], base[field])) {
            report.updated.push_back(key);
        }
    }
    return result;
}

Json rigidBodyComponentData(const SidecarRigidBody& body, const Transform& world, const std::string& nodeName)
{
    const ColliderDesc c = body.scaleWithEntity ? body.collider.scaled(world.scale) : body.collider;
    Json result = body.raw;
    result["mass"] = body.mass;
    result["shape"] = c.shape;
    result["size"] = c.shape == "box" ? c.halfExtents.x : c.radius;
    result["halfExtents"] = { c.halfExtents.x, c.halfExtents.y, c.halfExtents.z };
    result["radius"] = c.radius;
    result["halfHeight"] = c.halfHeight;
    result["center"] = { c.center.x, c.center.y, c.center.z };
    result["restitution"] = body.restitution;
    result["friction"] = body.friction;
    result["twoWay"] = body.twoWay;
    if (c.needsGeometry()) {
        result["geometryNode"] = nodeName;
        result["geometryWorld"] = false;
    }
    if (c.shape == "plane") {
        const glm::vec3 normal = glm::normalize(world.rotation * glm::vec3(0.f, 1.f, 0.f));
        result["normal"] = { normal.x, normal.y, normal.z };
        result["offset"] = glm::dot(normal, world.translation + world.rotation * c.center);
    }
    result["authored"] = body.raw;
    return result;
}

namespace {

namespace U = Phantom::SceneRuntime::Universe;

ComponentRecord* findComponent(SceneNode& node, const std::string& type)
{
    for (auto& c : node.components) if (c.type == type) return &c;
    return nullptr;
}

void setComponent(SceneNode& node, const std::string& type, Json data)
{
    if (auto* c = findComponent(node, type)) c->data = std::move(data);
    else node.components.push_back({ type, std::move(data) });
}

void removeComponent(SceneNode& node, const std::string& type)
{
    node.components.erase(std::remove_if(node.components.begin(), node.components.end(),
        [&](const ComponentRecord& c) { return c.type == type; }), node.components.end());
}

bool decomposeMatrix(const glm::mat4& m, Transform& out)
{
    glm::vec3 skew;
    glm::vec4 perspective;
    return glm::decompose(m, out.scale, out.rotation, out.translation, skew, perspective);
}

struct AssetGroup {
    std::filesystem::path path;
    std::string assetId;
    std::vector<std::string> members;             // node ids
    std::vector<SidecarObject> objects;
    std::unordered_map<std::string, Transform> nodeWorlds; // by object uuid
    bool hasSidecar = false;
};

} // namespace

bool mergeReexportedAssets(U::SceneV2& doc, const std::filesystem::path& scenePath,
                           MergeReport& report, std::string* error)
{
    const auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    const auto base = doc.assetRoot.empty() ? std::filesystem::current_path()
                                            : scenePath.parent_path() / doc.assetRoot;

    // A per-object asset node is one whose meshAsset.nodeId is its own UUID.
    std::map<std::string, AssetGroup> groups;
    for (const SceneNode* node : U::listNodes(doc)) {
        for (const auto& c : node->components) {
            if (c.type != "meshAsset") continue;
            const std::string assetId = c.data.value("assetId", std::string());
            if (c.data.value("nodeId", std::string()).empty() || c.data.value("nodeId", std::string()) != node->id.value())
                continue;
            const auto* entry = doc.assets.find(Phantom::Asset::AssetId(assetId));
            if (!entry) continue;
            const auto path = (base / entry->uri.value()).lexically_normal();
            auto& group = groups[path.string()];
            group.path = path;
            group.assetId = assetId;
            group.members.push_back(node->id.value());
        }
    }

    bool anySidecar = false;
    for (auto& pair : groups) {
        auto& group = pair.second;
        std::error_code ec;
        const auto sidecar = sidecarPathFor(group.path);
        if (!std::filesystem::is_regular_file(sidecar, ec)) continue;
        std::string diagnostic;
        if (!verifyPublishedPair(sidecar, group.path, diagnostic)) return fail(diagnostic);
        if (!verifyPublishedGeneration(group.path, diagnostic)) return fail(diagnostic);
        if (!readSidecarObjects(sidecar, group.objects, diagnostic))
            return fail(sidecar.string() + ": " + diagnostic);
        std::unordered_map<std::string, Transform> byName;
        if (!readGltfNodeWorldTransforms(group.path, byName, diagnostic)) return fail(diagnostic);
        std::unordered_set<std::string> uuids;
        for (const auto& object : group.objects) {
            if (object.uuid.empty() || !uuids.insert(object.uuid).second)
                return fail("sidecar object '" + object.name + "' has an empty or duplicate uuid");
            const auto it = byName.find(object.name);
            if (it == byName.end()) return fail("glTF node '" + object.name + "' is missing from " + group.path.filename().string());
            group.nodeWorlds.emplace(object.uuid, it->second);
        }
        group.hasSidecar = true;
        anySidecar = true;
    }
    if (!anySidecar) return true;

    // Work on a copy so a failure part-way leaves `doc` untouched.
    U::SceneV2 work = doc;
    MergeReport local;

    // World transforms are what the base ("authoredTransform") and the remote both express, and
    // what every non-asset node must keep, so the whole merge is done in world space and local
    // TRS is recomputed at the end.
    std::unordered_map<std::string, Transform> world;
    for (const SceneNode* node : U::listNodes(work)) {
        const auto matrix = work.scene.worldTransform(node->id);
        Transform t;
        if (!matrix || !decomposeMatrix(*matrix, t)) return fail("node '" + node->name + "' has an invalid transform chain");
        world[node->id.value()] = t;
    }
    const auto original = world;
    std::unordered_set<std::string> touched, removedIds;

    for (auto& pair : groups) {
        const auto& group = pair.second;
        if (!group.hasSidecar) continue;
        std::unordered_set<std::string> present;
        for (const auto& object : group.objects) {
            present.insert(object.uuid);
            const Transform& remote = group.nodeWorlds.at(object.uuid);
            SceneNode* node = work.scene.find(NodeId(object.uuid));
            const bool isNew = node == nullptr || std::find(group.members.begin(), group.members.end(), object.uuid) == group.members.end();
            if (isNew) {
                if (node) return fail("sidecar uuid " + object.uuid + " collides with an unrelated node");
                SceneNode fresh;
                fresh.id = NodeId(object.uuid);
                fresh.name = object.name;
                if (!work.scene.addNode(std::move(fresh))) return fail("could not add node " + object.name);
                node = work.scene.find(NodeId(object.uuid));
                local.added.push_back(object.name);
            } else if (node->name != object.name) {
                local.renamed.push_back(node->name + " -> " + object.name);
            }
            node->name = object.name;

            Json mesh = Json::object();
            if (const auto* existing = findComponent(*node, "meshAsset"); existing && existing->data.is_object())
                mesh = existing->data;
            const auto authored = mesh.contains("authoredTransform")
                ? transformFromJson(mesh["authoredTransform"]) : std::nullopt;
            mesh["assetId"] = group.assetId;
            mesh["nodeId"] = object.uuid;
            mesh["nodeName"] = object.name;
            mesh["authoredTransform"] = transformToJson(remote);
            setComponent(*node, "meshAsset", std::move(mesh));

            const Transform merged = isNew ? remote
                : mergeTransform(object.name, world.at(object.uuid), authored, remote, local);
            world[object.uuid] = merged;
            touched.insert(object.uuid);

            const ComponentRecord* body = findComponent(*node, "rigidBody");
            const Json* localBody = body && body->data.is_object() ? &body->data : nullptr;
            const auto mergedBody = mergeRigidBody(object.name, localBody,
                object.rigidBody ? &*object.rigidBody : nullptr, local);
            if (mergedBody.action == RigidBodyMergeAction::Remove) {
                removeComponent(*node, "rigidBody");
            } else if (mergedBody.action == RigidBodyMergeAction::UseMerged) {
                Json data = rigidBodyComponentData(mergedBody.merged, merged, object.name);
                // The merge base stays the unmodified new Blender input, not the override.
                data["authored"] = object.rigidBody->raw;
                setComponent(*node, "rigidBody", std::move(data));
            }
        }
        for (const auto& id : group.members) {
            if (present.count(id)) continue;
            if (const SceneNode* gone = work.scene.find(NodeId(id))) local.removed.push_back(gone->name);
            removedIds.insert(id);
        }
    }

    // A deleted Blender node must not take its children (or leave a dangling parent UUID): they
    // keep their world transform and become roots.
    for (const auto& id : removedIds) {
        for (const NodeId& child : work.scene.children(NodeId(id))) {
            if (removedIds.count(child.value())) continue;
            if (const SceneNode* c = work.scene.find(child)) local.updated.push_back(c->name + ".parent");
            if (!work.scene.reparent(child, NodeId())) return fail("could not reparent a child of a removed node");
            touched.insert(child.value());
        }
    }
    for (const auto& id : removedIds) work.scene.removeNode(NodeId(id));
    for (const auto& id : removedIds) { world.erase(id); touched.erase(id); }

    // Local TRS from the final world transforms. Untouched nodes whose parent did not move keep
    // their stored local values bit for bit.
    std::vector<std::string> ids;
    for (const SceneNode* node : U::listNodes(work)) ids.push_back(node->id.value());
    for (const auto& id : ids) {
        SceneNode* node = work.scene.find(NodeId(id));
        const bool parentMoved = node->parent.isValid() && touched.count(node->parent.value())
            && (!original.count(node->parent.value())
                || !sameTransform(original.at(node->parent.value()), world.at(node->parent.value())));
        if (!touched.count(id) && !parentMoved) continue;
        glm::mat4 parentWorld(1.f);
        if (node->parent.isValid()) {
            Transform pt = world.at(node->parent.value());
            parentWorld = pt.toMatrix();
            if (std::abs(glm::determinant(parentWorld)) < 1.e-8f)
                return fail("parent of '" + node->name + "' has a singular transform");
        }
        if (!decomposeMatrix(glm::inverse(parentWorld) * world.at(id).toMatrix(), node->local))
            return fail("could not derive the local transform of '" + node->name + "'");
    }

    doc = std::move(work);
    report.renamed.insert(report.renamed.end(), local.renamed.begin(), local.renamed.end());
    report.removed.insert(report.removed.end(), local.removed.begin(), local.removed.end());
    report.added.insert(report.added.end(), local.added.begin(), local.added.end());
    report.updated.insert(report.updated.end(), local.updated.begin(), local.updated.end());
    report.kept.insert(report.kept.end(), local.kept.begin(), local.kept.end());
    report.conflicts.insert(report.conflicts.end(), local.conflicts.begin(), local.conflicts.end());
    return true;
}

} // namespace Phantom::SceneRuntime
