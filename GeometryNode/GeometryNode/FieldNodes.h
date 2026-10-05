#pragma once

// Field nodes (docs/todo/PLAN_geometry_node.md Phase 4), registered by NodeRegistry::builtin().
//
//   Sources    Position, Index, Random Value (seeded, see Field.h), Float Field, Vector Field
//   Math       Add, Subtract, Multiply, Divide, Minimum, Maximum  (-> Float field)
//              Greater Than, Less Than                            (-> Bool field = a Selection)
//              Operand B is the optional field input; when it is not linked the scalar "Value"
//              is used. Divide by 0 yields 0 (never Inf/NaN).
//   Vectors    Combine XYZ Field (absent component = 0), Separate XYZ, Vector Add, Vector Scale
//   Consumers  Set Position (Selection / Position / Offset), Delete Geometry (Selection)
//
// Fields are evaluated by the consumer on its own geometry's Point domain. A Bool field is the
// "Selection": elements where it is true are affected. Set Position recomputes smooth vertex normals
// from the new positions (flat per-face normals stay flat because the Box/Grid vertices are not shared).

#include "NodeRegistry.h"

namespace Phantom::GeometryNode {

void registerFieldNodes(NodeRegistry& registry);

}  // namespace Phantom::GeometryNode
