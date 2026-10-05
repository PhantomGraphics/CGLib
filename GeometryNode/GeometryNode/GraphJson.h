#pragma once

// Graph <-> JSON. The format separates the computation (nodes, links) from the
// editor layout, carries a schema version, and round-trips unknown node types,
// unknown parameters and unknown keys unchanged.
//
//   {
//     "schema": 1,
//     "nodes":  [ {"id":1, "type":"Box", "version":1, "params":{"Size":[1,2,3]}} ],
//     "links":  [ {"from":{"node":1,"socket":"Geometry"}, "to":{"node":2,"socket":"Geometry"}} ],
//     "layout": { "nodes": { "1": {"x":0, "y":0} } }
//   }
//
// Exception-free: parsing reports failure through the return value.

#include <string>

#include "GraphTypes.h"

namespace Phantom::GeometryNode {

struct GraphParseResult {
    bool ok = false;
    std::string error;  // human-readable, empty when ok
};

nlohmann::json graphToJson(const Graph& graph);
// Replaces `out` only on success. Rejects a schema newer than kGraphSchemaVersion.
GraphParseResult graphFromJson(const nlohmann::json& j, Graph& out);

std::string serializeGraph(const Graph& graph, int indent = 2);
GraphParseResult parseGraph(const std::string& text, Graph& out);

}  // namespace Phantom::GeometryNode
