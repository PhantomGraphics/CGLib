#pragma once

#include "GltfDocument.h"
#include "../../File/File/STLFile.h"

namespace Phantom::Gltf {

// Converts an already-parsed Phantom::File::STLFile (see Phantom::File::STLFileReader, ASCII or
// binary) into a GltfDocument -- see ObjToGltfConverter.h's header comment for the overall
// rationale (shared GltfDocument output so OBJ/STL flow through the same import path as glTF).
class StlToGltfConverter {
public:
    // All STLFace entries become a single GltfMesh (single GltfPrimitive) + GltfNode under
    // doc.scenes[0]. Each face's 3 vertices are appended as-is (STL is already
    // triangle-only, no fan triangulation needed); vertices are fully expanded (no sharing
    // across triangles), each carrying the face's flat normal, with sequential indices. UV is
    // always (0,0) -- STL has no texture-coordinate concept. No material is assigned
    // (GltfPrimitive::materialIndex stays -1). Returns a document with no meshes if `stl` has no
    // faces.
    static GltfDocument convert(const Phantom::File::STLFile& stl);
};

} // namespace Phantom::Gltf
