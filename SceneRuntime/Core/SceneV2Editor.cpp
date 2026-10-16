#include "SceneV2Editor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Phantom::SceneRuntime::Universe {

namespace {

using Json = nlohmann::json;

Json vec3ToJson(const Phantom::Math::Vector3df& v) { return Json::array({ v.x, v.y, v.z }); }

bool finite3(const Phantom::Math::Vector3df& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool parseNumber(const std::string& text, double& out)
{
    if (text.empty()) return false;
    char* end = nullptr;
    out = std::strtod(text.c_str(), &end);
    return end && *end == '\0' && std::isfinite(out);
}

ComponentRecord* findComponent(SceneNode& node, const char* type)
{
    for (auto& c : node.components) {
        if (c.type == type) return &c;
    }
    return nullptr;
}

const ComponentRecord* findComponent(const SceneNode& node, const char* type)
{
    for (const auto& c : node.components) {
        if (c.type == type) return &c;
    }
    return nullptr;
}

void collect(const SceneV2& doc, const NodeId& parent, std::vector<const SceneNode*>& out)
{
    std::vector<const SceneNode*> kids;
    for (const NodeId& id : doc.scene.children(parent)) {
        if (const SceneNode* n = doc.scene.find(id)) kids.push_back(n);
    }
    std::sort(kids.begin(), kids.end(), [](const SceneNode* a, const SceneNode* b) {
        return a->name != b->name ? a->name < b->name : a->id.value() < b->id.value();
    });
    for (const SceneNode* n : kids) {
        out.push_back(n);
        collect(doc, n->id, out);
    }
}

} // namespace

const SceneNode* findNodeByRef(const SceneV2& doc, const std::string& ref, EditResult* error)
{
    if (ref.empty()) {
        if (error) *error = EditResult::failure("not_found", "empty node reference");
        return nullptr;
    }
    if (const SceneNode* byId = doc.scene.find(NodeId(ref))) return byId;

    const SceneNode* match = nullptr;
    int count = 0;
    for (const SceneNode* n : listNodes(doc)) {
        if (n->name == ref) { match = n; ++count; }
    }
    if (count == 1) return match;
    if (error) {
        *error = count == 0
            ? EditResult::failure("not_found", "no node named or identified '" + ref + "'")
            : EditResult::failure("ambiguous", "'" + ref + "' names " + std::to_string(count)
                                                   + " nodes; use the node UUID");
    }
    return nullptr;
}

SceneNode* findNodeByRef(SceneV2& doc, const std::string& ref, EditResult* error)
{
    return const_cast<SceneNode*>(findNodeByRef(static_cast<const SceneV2&>(doc), ref, error));
}

std::vector<const SceneNode*> listNodes(const SceneV2& doc)
{
    std::vector<const SceneNode*> out;
    collect(doc, NodeId(), out);
    return out;
}

Json describeNode(const SceneV2& doc, const SceneNode& node)
{
    const glm::vec3 euler = glm::degrees(glm::eulerAngles(node.local.rotation));
    Json j;
    j["id"] = node.id.value();
    j["name"] = node.name;
    j["parent"] = node.parent.isValid() ? node.parent.value() : std::string();
    j["enabled"] = node.enabled;
    j["translation"] = vec3ToJson(node.local.translation);
    j["rotationEulerDeg"] = vec3ToJson(euler);
    j["scale"] = vec3ToJson(node.local.scale);
    Json types = Json::array();
    for (const auto& c : node.components) types.push_back(c.type);
    j["components"] = std::move(types);

    if (const ComponentRecord* mesh = findComponent(node, "meshAsset")) {
        const std::string assetId = mesh->data.value("assetId", std::string());
        Json m = { { "assetId", assetId }, { "nodeId", mesh->data.value("nodeId", std::string()) } };
        if (const auto* asset = doc.assets.find(Phantom::Asset::AssetId(assetId))) m["uri"] = asset->uri.value();
        j["meshAsset"] = std::move(m);
    }
    if (const ComponentRecord* rb = findComponent(node, "rigidBody")) j["rigidBody"] = rb->data;
    if (const ComponentRecord* ap = findComponent(node, "animationPlayer")) j["animationPlayer"] = ap->data;
    return j;
}

Json describeDocument(const SceneV2& doc)
{
    Json nodes = Json::array();
    for (const SceneNode* n : listNodes(doc)) nodes.push_back(describeNode(doc, *n));
    return { { "version", SceneV2::kVersion }, { "assetRoot", doc.assetRoot },
             { "assetCount", doc.assets.size() }, { "nodes", std::move(nodes) } };
}

EditResult setNodeTransform(SceneV2& doc, const std::string& ref, const TransformEdit& edit)
{
    EditResult error;
    SceneNode* node = findNodeByRef(doc, ref, &error);
    if (!node) return error;

    for (const auto* v : { &edit.translation, &edit.rotationEulerDeg, &edit.scale }) {
        if (v->has_value() && !finite3(**v))
            return EditResult::failure("out_of_range", "transform values must be finite");
    }
    if (edit.scale && (edit.scale->x == 0.f || edit.scale->y == 0.f || edit.scale->z == 0.f))
        return EditResult::failure("out_of_range", "scale components must be non-zero");

    if (edit.translation) node->local.translation = *edit.translation;
    if (edit.rotationEulerDeg)
        node->local.rotation = Phantom::Math::Quaternion(glm::radians(*edit.rotationEulerDeg));
    if (edit.scale) node->local.scale = *edit.scale;
    return EditResult::success();
}

EditResult setRigidBodyField(SceneV2& doc, const std::string& ref, const std::string& field,
                             const std::string& value)
{
    EditResult error;
    SceneNode* node = findNodeByRef(doc, ref, &error);
    if (!node) return error;
    ComponentRecord* rb = findComponent(*node, "rigidBody");
    if (!rb) return EditResult::failure("no_component", "'" + node->name + "' has no rigidBody component");

    Json parsed;
    if (field == "twoWay") {
        if (value == "1" || value == "true") parsed = true;
        else if (value == "0" || value == "false") parsed = false;
        else return EditResult::failure("out_of_range", "twoWay must be 0/1/true/false");
    } else if (field == "mass" || field == "friction" || field == "restitution") {
        double n = 0;
        if (!parseNumber(value, n)) return EditResult::failure("out_of_range", field + " must be a finite number");
        if (n < 0.0) return EditResult::failure("out_of_range", field + " must be >= 0");
        if (field == "restitution" && n > 1.0) return EditResult::failure("out_of_range", "restitution must be in [0,1]");
        parsed = n;
    } else {
        return EditResult::failure("unknown_field", "unknown rigidBody field '" + field
                                                        + "' (mass, friction, restitution, twoWay)");
    }
    if (!rb->data.is_object()) rb->data = Json::object();
    rb->data[field] = std::move(parsed);
    return EditResult::success();
}

} // namespace Phantom::SceneRuntime::Universe
