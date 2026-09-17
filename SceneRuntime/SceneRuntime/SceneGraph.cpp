#include "SceneGraph.h"

#include "json.hpp"

#include <fstream>
#include <sstream>
#include <unordered_set>

namespace Phantom::SceneRuntime {

namespace {

nlohmann::json quatToJson(const Phantom::Math::Quaternion& q)
{
    return nlohmann::json::array({ q.x, q.y, q.z, q.w });
}

// Reads `parent[key]` (an [x,y,z,w] array) if present and well-formed, else the identity.
Phantom::Math::Quaternion quatFromJson(const nlohmann::json& parent, const char* key)
{
    const Phantom::Math::Quaternion identity{ 1.0f, 0.0f, 0.0f, 0.0f };
    if (!parent.contains(key)) return identity;
    const nlohmann::json& j = parent[key];
    if (!j.is_array() || j.size() != 4) return identity;
    return Phantom::Math::Quaternion(j[3].get<float>(), j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

nlohmann::json vec3ToJson(const Phantom::Math::Vector3df& v)
{
    return nlohmann::json::array({ v.x, v.y, v.z });
}

// Reads `parent[key]` (an [x,y,z] array) if present and well-formed, else `fallback`.
Phantom::Math::Vector3df vec3FromJson(const nlohmann::json& parent, const char* key, const Phantom::Math::Vector3df& fallback)
{
    if (!parent.contains(key)) return fallback;
    const nlohmann::json& j = parent[key];
    if (!j.is_array() || j.size() != 3) return fallback;
    return Phantom::Math::Vector3df(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
}

} // namespace

bool SceneGraph::addNode(SceneNode node)
{
    if (!node.id.isValid()) return false;
    if (nodes_.count(node.id.value()) != 0) return false;
    if (node.parent.isValid() && nodes_.count(node.parent.value()) == 0) return false;
    nodes_.emplace(node.id.value(), std::move(node));
    return true;
}

bool SceneGraph::removeNode(const NodeId& id)
{
    if (nodes_.count(id.value()) == 0) return false;
    // Collect descendants via children() (which itself scans nodes_) before erasing --
    // erasing mid-scan would invalidate the range children() walks.
    for (const NodeId& child : children(id)) removeNode(child);
    nodes_.erase(id.value());
    return true;
}

bool SceneGraph::reparent(const NodeId& id, const NodeId& newParent)
{
    auto it = nodes_.find(id.value());
    if (it == nodes_.end()) return false;
    if (newParent.isValid()) {
        if (nodes_.count(newParent.value()) == 0) return false;
        if (isSelfOrDescendant(id, newParent)) return false;
    }
    it->second.parent = newParent;
    return true;
}

const SceneNode* SceneGraph::find(const NodeId& id) const
{
    auto it = nodes_.find(id.value());
    return it == nodes_.end() ? nullptr : &it->second;
}

SceneNode* SceneGraph::find(const NodeId& id)
{
    auto it = nodes_.find(id.value());
    return it == nodes_.end() ? nullptr : &it->second;
}

std::vector<NodeId> SceneGraph::children(const NodeId& parent) const
{
    std::vector<NodeId> result;
    for (const auto& entry : nodes_) {
        if (entry.second.parent == parent) result.push_back(entry.second.id);
    }
    return result;
}

bool SceneGraph::isSelfOrDescendant(const NodeId& start, const NodeId& candidate) const
{
    if (start == candidate) return true;
    for (const NodeId& child : children(start)) {
        if (isSelfOrDescendant(child, candidate)) return true;
    }
    return false;
}

std::optional<Phantom::Math::Matrix4df> SceneGraph::worldTransform(const NodeId& id) const
{
    // chain[0] == id's own node, chain.back() == the root it eventually reaches.
    std::vector<const SceneNode*> chain;
    std::unordered_set<std::string> visited;
    NodeId current = id;
    while (current.isValid()) {
        if (!visited.insert(current.value()).second) return std::nullopt; // cycle
        const SceneNode* node = find(current);
        if (!node) return std::nullopt; // stale parent reference
        chain.push_back(node);
        current = node->parent;
    }
    // World(node) = Local(root) * ... * Local(parent) * Local(node) -- fold root-to-node,
    // i.e. walk `chain` back to front.
    Phantom::Math::Matrix4df world(1.0f);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        world = world * (*it)->local.toMatrix();
    }
    return world;
}

std::string SceneGraph::toJson() const
{
    nlohmann::json root;
    root["schema"] = "phantom.scene/1";
    nlohmann::json nodesJson = nlohmann::json::array();
    for (const auto& entry : nodes_) {
        const SceneNode& node = entry.second;
        nlohmann::json j;
        j["id"] = node.id.value();
        j["parent"] = node.parent.value();
        j["name"] = node.name;
        j["enabled"] = node.enabled;
        nlohmann::json t;
        t["pos"] = vec3ToJson(node.local.translation);
        t["rot"] = quatToJson(node.local.rotation);
        t["scale"] = vec3ToJson(node.local.scale);
        j["transform"] = std::move(t);
        nlohmann::json comps = nlohmann::json::array();
        for (const auto& c : node.components) {
            nlohmann::json cj;
            cj["type"] = c.type;
            cj["data"] = c.data;
            comps.push_back(std::move(cj));
        }
        j["components"] = std::move(comps);
        nodesJson.push_back(std::move(j));
    }
    root["nodes"] = std::move(nodesJson);
    return root.dump();
}

SceneGraph SceneGraph::fromJson(const std::string& json, bool* ok)
{
    SceneGraph graph;
    if (ok) *ok = false;
    nlohmann::json root;
    try {
        root = nlohmann::json::parse(json);
    } catch (const nlohmann::json::parse_error&) {
        return graph;
    }
    if (!root.is_object() || root.value("schema", "") != "phantom.scene/1") return graph;
    if (!root.contains("nodes") || !root["nodes"].is_array()) return graph;

    // Two passes: a serialized graph has no ordering guarantee (unlike addNode()'s
    // top-down-only contract), so a node's parent may appear later in the array. Every
    // node is added parentless first, then reparented once every id exists.
    struct Pending {
        NodeId id;
        NodeId parent;
        SceneNode node;
    };
    std::vector<Pending> pending;
    for (const auto& j : root["nodes"]) {
        if (!j.is_object() || !j.contains("id")) continue;
        SceneNode node;
        node.id = NodeId(j.value("id", ""));
        if (!node.id.isValid()) continue;
        node.name = j.value("name", "");
        node.enabled = j.value("enabled", true);
        if (j.contains("transform") && j["transform"].is_object()) {
            const auto& t = j["transform"];
            node.local.translation = vec3FromJson(t, "pos", { 0.0f, 0.0f, 0.0f });
            node.local.rotation = quatFromJson(t, "rot");
            node.local.scale = vec3FromJson(t, "scale", { 1.0f, 1.0f, 1.0f });
        }
        if (j.contains("components") && j["components"].is_array()) {
            for (const auto& cj : j["components"]) {
                if (!cj.is_object() || !cj.contains("type")) continue;
                ComponentRecord c;
                c.type = cj.value("type", "");
                c.data = cj.value("data", nlohmann::json::object());
                node.components.push_back(std::move(c));
            }
        }
        NodeId parent(j.value("parent", ""));
        node.parent = NodeId(); // resolved in the reparent pass below
        pending.push_back({ node.id, parent, std::move(node) });
    }

    for (auto& p : pending) {
        if (!graph.addNode(std::move(p.node))) return SceneGraph{}; // duplicate id -- reject whole file
    }
    for (auto& p : pending) {
        if (!p.parent.isValid()) continue;
        if (!graph.reparent(p.id, p.parent)) return SceneGraph{}; // unknown/cyclic parent
    }

    if (ok) *ok = true;
    return graph;
}

bool SceneGraph::saveToFile(const std::filesystem::path& path) const
{
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << toJson();
    return static_cast<bool>(out);
}

SceneGraph SceneGraph::loadFromFile(const std::filesystem::path& path, bool* ok)
{
    if (ok) *ok = false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return SceneGraph{};
    std::ostringstream buf;
    buf << in.rdbuf();
    return fromJson(buf.str(), ok);
}

} // namespace Phantom::SceneRuntime
