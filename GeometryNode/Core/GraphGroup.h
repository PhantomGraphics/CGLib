#pragma once

// Node groups (docs/todo/PLAN_geometry_node.md Phase 6). A group is a named, immutable sub-graph with an interface,
// stored in the owning graph's library (Graph::groups). A node of type "Group:<name>" evaluates the sub-graph like a
// function: its inputs become the outputs of the inner GroupInput node, and the inner GroupOutput node's inputs become
// its outputs. Groups can use other groups of the same library (nesting depth is limited, so a cycle fails with a
// diagnostic instead of recursing forever).
//
// Because group node types depend on the graph, evaluation needs a registry that knows them: registryFor(graph).
// Editing a group's contents is done by ungrouping and grouping again (the definition is never edited in place), so a
// definition change always shows up as a new node, never as a silent change under existing ones.

#include <string>
#include <vector>

#include "GraphTypes.h"
#include "NodeRegistry.h"

namespace Phantom::GeometryNode {

inline constexpr int kMaxGroupDepth = 16;

inline const char* kGroupNodePrefix = "Group:";

bool isGroupNodeType(const std::string& type);
std::string groupNodeType(const std::string& groupName);   // "Group:<name>"
std::string groupNameOf(const std::string& nodeType);      // "" if not a group node type

// The built-in registry plus one "Group:<name>" definition per group of `graph` (a plain copy of the built-in one when
// the graph has no groups). Use it wherever a graph is validated or evaluated.
NodeRegistry registryFor(const Graph& graph);

const GroupDef* findGroup(const Graph& graph, const std::string& name);

// Replaces `selection` by one group node. Links between selected nodes move into the group; every link crossing the
// boundary becomes an interface socket (inputs from outside, outputs to outside). Fails (graph untouched) when the
// name is invalid/used, a selected node is the Output node or has an exposed parameter, nothing connects to the rest of
// the graph, or the grouping would create a cycle. `groupNode` receives the new node's id.
bool makeGroup(Graph& graph, const std::string& name, const std::vector<NodeId>& selection, std::string* error = nullptr,
               NodeId* groupNode = nullptr);

// Expands a group node back into its inner nodes (the group definition stays in the library). Unlinked inputs of the
// group node become parameters of the inner nodes they fed.
bool ungroup(Graph& graph, NodeId groupNode, std::string* error = nullptr);

// Deletes an unused group definition (no node in the graph or in another group refers to it).
bool removeGroup(Graph& graph, const std::string& name, std::string* error = nullptr);

}  // namespace Phantom::GeometryNode
