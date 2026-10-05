#include "GraphJson.h"

#include <cstdlib>
#include <limits>

namespace Phantom::GeometryNode {

namespace {

using json = nlohmann::json;

GraphParseResult fail(std::string msg) {
    GraphParseResult r;
    r.ok = false;
    r.error = std::move(msg);
    return r;
}

bool readEndpoint(const json& j, Endpoint& out) {
    if (!j.is_object()) return false;
    auto n = j.find("node");
    auto s = j.find("socket");
    if (n == j.end() || s == j.end() || !n->is_number_unsigned() || !s->is_string()) return false;
    out.node = n->get<uint64_t>();
    out.socket = s->get<std::string>();
    return true;
}

json writeEndpoint(const Endpoint& e) {
    json j = json::object();
    j["node"] = e.node;
    j["socket"] = e.socket;
    return j;
}

}  // namespace

json graphToJson(const Graph& graph) {
    json root = graph.extra.is_object() ? graph.extra : json::object();
    root["schema"] = graph.schema;

    json nodes = json::array();
    for (const Node& n : graph.nodes) {
        json j = n.extra.is_object() ? n.extra : json::object();
        j["id"] = n.id;
        j["type"] = n.type;
        j["version"] = n.version;
        json params = json::object();
        for (const auto& [k, v] : n.params) params[k] = v;
        j["params"] = std::move(params);
        nodes.push_back(std::move(j));
    }
    root["nodes"] = std::move(nodes);

    json links = json::array();
    for (const Link& l : graph.links) {
        json j = json::object();
        j["from"] = writeEndpoint(l.from);
        j["to"] = writeEndpoint(l.to);
        links.push_back(std::move(j));
    }
    root["links"] = std::move(links);

    json layoutNodes = json::object();
    for (const auto& [id, p] : graph.layout) {
        json j = json::object();
        j["x"] = p.x;
        j["y"] = p.y;
        layoutNodes[std::to_string(id)] = std::move(j);
    }
    json layout = json::object();
    layout["nodes"] = std::move(layoutNodes);
    root["layout"] = std::move(layout);
    return root;
}

GraphParseResult graphFromJson(const json& j, Graph& out) {
    if (!j.is_object()) return fail("graph: root must be an object");
    Graph g;

    auto schema = j.find("schema");
    if (schema == j.end() || !schema->is_number_unsigned()) return fail("graph: missing or invalid 'schema'");
    const uint64_t sv = schema->get<uint64_t>();
    if (sv > kGraphSchemaVersion) {
        return fail("graph: schema " + std::to_string(sv) + " is newer than supported schema " +
                    std::to_string(kGraphSchemaVersion));
    }
    g.schema = static_cast<uint32_t>(sv);

    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string& k = it.key();
        if (k != "schema" && k != "nodes" && k != "links" && k != "layout") g.extra[k] = it.value();
    }

    auto nodes = j.find("nodes");
    if (nodes != j.end()) {
        if (!nodes->is_array()) return fail("graph: 'nodes' must be an array");
        for (const json& nj : *nodes) {
            if (!nj.is_object()) return fail("graph: node must be an object");
            Node n;
            auto id = nj.find("id");
            auto type = nj.find("type");
            if (id == nj.end() || !id->is_number_unsigned()) return fail("graph: node without valid 'id'");
            if (type == nj.end() || !type->is_string()) return fail("graph: node without valid 'type'");
            n.id = id->get<uint64_t>();
            n.type = type->get<std::string>();
            auto ver = nj.find("version");
            if (ver != nj.end()) {
                if (!ver->is_number_unsigned() || ver->get<uint64_t>() > std::numeric_limits<uint32_t>::max())
                    return fail("graph: node has invalid 'version'");
                n.version = static_cast<uint32_t>(ver->get<uint64_t>());
            }
            auto params = nj.find("params");
            if (params != nj.end()) {
                if (!params->is_object()) return fail("graph: node 'params' must be an object");
                for (auto p = params->begin(); p != params->end(); ++p) n.params[p.key()] = p.value();
            }
            for (auto p = nj.begin(); p != nj.end(); ++p) {
                const std::string& k = p.key();
                if (k != "id" && k != "type" && k != "version" && k != "params") n.extra[k] = p.value();
            }
            g.nodes.push_back(std::move(n));
        }
    }

    auto links = j.find("links");
    if (links != j.end()) {
        if (!links->is_array()) return fail("graph: 'links' must be an array");
        for (const json& lj : *links) {
            Link l;
            if (!lj.is_object()) return fail("graph: link must be an object");
            auto from = lj.find("from");
            auto to = lj.find("to");
            if (from == lj.end() || to == lj.end() || !readEndpoint(*from, l.from) || !readEndpoint(*to, l.to))
                return fail("graph: link needs valid 'from' and 'to' endpoints");
            g.links.push_back(std::move(l));
        }
    }

    auto layout = j.find("layout");
    if (layout != j.end()) {
        if (!layout->is_object()) return fail("graph: 'layout' must be an object");
        auto ln = layout->find("nodes");
        if (ln != layout->end()) {
            if (!ln->is_object()) return fail("graph: 'layout.nodes' must be an object");
            for (auto p = ln->begin(); p != ln->end(); ++p) {
                char* end = nullptr;
                const unsigned long long id = std::strtoull(p.key().c_str(), &end, 10);
                if (p.key().empty() || *end != '\0') return fail("graph: layout key '" + p.key() + "' is not a node id");
                const json& v = p.value();
                auto x = v.is_object() ? v.find("x") : v.end();
                auto y = v.is_object() ? v.find("y") : v.end();
                if (!v.is_object() || x == v.end() || y == v.end() || !x->is_number() || !y->is_number())
                    return fail("graph: layout entry for node " + p.key() + " needs numeric x and y");
                g.layout[id] = NodeLayout{x->get<double>(), y->get<double>()};
            }
        }
    }

    out = std::move(g);
    GraphParseResult r;
    r.ok = true;
    return r;
}

std::string serializeGraph(const Graph& graph, int indent) { return graphToJson(graph).dump(indent); }

GraphParseResult parseGraph(const std::string& text, Graph& out) {
    json j = json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) return fail("graph: invalid JSON");
    return graphFromJson(j, out);
}

}  // namespace Phantom::GeometryNode
