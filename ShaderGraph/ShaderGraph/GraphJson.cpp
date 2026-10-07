#include <cstdlib>
#include "ShaderGraph.h"

namespace Phantom::ShaderGraph {

using nlohmann::json;

json graphToJson(const Graph& g) {
    json j = g.extra.is_object() ? g.extra : json::object();
    j["schema"] = g.schema;
    json nodes = json::array();
    for (const Node& n : g.nodes) {
        json nj = n.extra.is_object() ? n.extra : json::object();
        nj["id"] = n.id;
        nj["type"] = n.type;
        nj["version"] = n.version;
        json p = json::object();
        for (const auto& [k, v] : n.params) p[k] = v;
        nj["params"] = p;
        nodes.push_back(nj);
    }
    j["nodes"] = nodes;
    json links = json::array();
    for (const Link& l : g.links)
        links.push_back({{"from", {{"node", l.fromNode}, {"socket", l.fromSocket}}},
                         {"to", {{"node", l.toNode}, {"socket", l.toSocket}}}});
    j["links"] = links;
    if (!g.exposed.empty()) {
        json ex = json::array();
        for (const ExposedParam& e : g.exposed) ex.push_back({{"name", e.name}, {"node", e.node}, {"param", e.param}});
        j["exposed"] = ex;
    }
    json ln = json::object();
    for (const auto& [id, l] : g.layout) ln[std::to_string(id)] = {{"x", l.x}, {"y", l.y}};
    j["layout"] = {{"nodes", ln}};
    return j;
}

namespace {

ParseResult fail(std::string msg) { return {false, std::move(msg)}; }

bool endpoint(const json& e, NodeId& node, std::string& socket) {
    if (!e.is_object() || !e.contains("node") || !e["node"].is_number_integer() || !e.contains("socket") ||
        !e["socket"].is_string())
        return false;
    node = e["node"].get<int>();
    socket = e["socket"].get<std::string>();
    return true;
}

}  // namespace

ParseResult graphFromJson(const json& j, Graph& out) {
    if (!j.is_object()) return fail("graph must be a JSON object");
    Graph g;
    if (j.contains("schema")) {
        if (!j["schema"].is_number_integer()) return fail("schema must be an integer");
        g.schema = j["schema"].get<int>();
        if (g.schema > kSchemaVersion)
            return fail("graph schema " + std::to_string(g.schema) + " is newer than supported " +
                        std::to_string(kSchemaVersion));
        if (g.schema < 1) return fail("invalid schema");
    }
    for (auto it = j.begin(); it != j.end(); ++it)
        if (it.key() != "schema" && it.key() != "nodes" && it.key() != "links" && it.key() != "layout" &&
            it.key() != "exposed")
            g.extra[it.key()] = it.value();

    if (j.contains("nodes")) {
        if (!j["nodes"].is_array()) return fail("nodes must be an array");
        for (const json& nj : j["nodes"]) {
            if (!nj.is_object() || !nj.contains("id") || !nj["id"].is_number_integer() || !nj.contains("type") ||
                !nj["type"].is_string())
                return fail("each node needs integer 'id' and string 'type'");
            Node n;
            n.id = nj["id"].get<int>();
            n.type = nj["type"].get<std::string>();
            if (nj.contains("version")) {
                if (!nj["version"].is_number_integer()) return fail("node version must be an integer");
                n.version = nj["version"].get<int>();
            }
            if (nj.contains("params")) {
                if (!nj["params"].is_object()) return fail("node params must be an object");
                for (auto it = nj["params"].begin(); it != nj["params"].end(); ++it) n.params[it.key()] = it.value();
            }
            for (auto it = nj.begin(); it != nj.end(); ++it)
                if (it.key() != "id" && it.key() != "type" && it.key() != "version" && it.key() != "params")
                    n.extra[it.key()] = it.value();
            g.nodes.push_back(std::move(n));
        }
    }
    if (j.contains("links")) {
        if (!j["links"].is_array()) return fail("links must be an array");
        for (const json& lj : j["links"]) {
            Link l;
            if (!lj.is_object() || !lj.contains("from") || !lj.contains("to") ||
                !endpoint(lj["from"], l.fromNode, l.fromSocket) || !endpoint(lj["to"], l.toNode, l.toSocket))
                return fail("each link needs from/to with integer 'node' and string 'socket'");
            g.links.push_back(std::move(l));
        }
    }
    if (j.contains("exposed")) {
        if (!j["exposed"].is_array()) return fail("exposed must be an array");
        for (const json& ej : j["exposed"]) {
            if (!ej.is_object() || !ej.contains("name") || !ej["name"].is_string() || !ej.contains("node") ||
                !ej["node"].is_number_integer() || !ej.contains("param") || !ej["param"].is_string())
                return fail("each exposed entry needs string 'name', integer 'node' and string 'param'");
            g.exposed.push_back({ej["name"].get<std::string>(), ej["node"].get<int>(), ej["param"].get<std::string>()});
        }
    }
    if (j.contains("layout") && j["layout"].is_object() && j["layout"].contains("nodes") &&
        j["layout"]["nodes"].is_object()) {
        for (auto it = j["layout"]["nodes"].begin(); it != j["layout"]["nodes"].end(); ++it) {
            char* end = nullptr;
            const long id = std::strtol(it.key().c_str(), &end, 10);
            if (end == it.key().c_str() || *end != '\0') continue;  // layout is advisory: skip junk
            NodeLayout l;
            if (it.value().is_object()) {
                if (it.value().contains("x") && it.value()["x"].is_number()) l.x = it.value()["x"].get<double>();
                if (it.value().contains("y") && it.value()["y"].is_number()) l.y = it.value()["y"].get<double>();
            }
            g.layout[static_cast<NodeId>(id)] = l;
        }
    }
    out = std::move(g);
    return {true, ""};
}

std::string serializeGraph(const Graph& g, int indent) { return graphToJson(g).dump(indent); }

ParseResult parseGraph(const std::string& text, Graph& out) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return fail("invalid JSON");
    return graphFromJson(j, out);
}

}  // namespace Phantom::ShaderGraph
