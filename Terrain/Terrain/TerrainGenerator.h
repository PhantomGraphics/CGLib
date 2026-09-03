#pragma once

// Phantom::Terrain public API -- height-field terrain mesh generation.
//
// Exception-free (repo convention): every failure is reported through the
// TerrainError return value. generate() replaces `out` only on success.

#include "TerrainTypes.h"

namespace Phantom::Terrain {

// Checks every field of `settings` without allocating. Size-related problems
// (grid too large, index/vertex overflow) return MeshTooLarge; every other
// out-of-range or non-finite value returns InvalidSettings.
TerrainError validate(const TerrainSettings& settings);

// Generates a deterministic indexed triangle mesh on the XZ plane with +Y up,
// origin at the terrain centre. Topology and winding match
// PrimitiveBuilder::buildPlane() (declared normals face +Y). The same settings
// and generatorVersion yield the same topology and, within floating-point
// tolerance, the same heights across OS and build configuration.
//
// Returns the same codes as validate(); on anything other than None, `out` is
// left untouched.
TerrainError generate(const TerrainSettings& settings, TerrainMesh& out);

}  // namespace Phantom::Terrain
