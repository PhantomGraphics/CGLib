#pragma once

#include "GltfDocument.h"
#include "../../File/File/OBJFile.h"

namespace Phantom::Gltf {

// Converts an already-parsed Phantom::File::OBJFile (see Phantom::File::OBJFileReader) into a
// GltfDocument, so OBJ meshes can flow through the same GltfDocument-consuming code every glTF
// asset already does (CGStudio's GltfImporter::extract(), GltfSceneRenderer::traverseNode()) --
// see docs/todo/PLAN_obj_stl_gltfrenderer_shared_import.md. File I/O stays the caller's
// responsibility (OBJFileReader::read()) so this converter is trivially unit-testable.
class ObjToGltfConverter {
public:
    // One GltfMesh (single GltfPrimitive) + GltfNode per non-empty OBJGroup, all parented under
    // doc.scenes[0]. Each face is fan-triangulated (mirrors the old
    // CGApp/CGStudio/MeshImporter.cpp::appendObjFace() logic); vertices are fully expanded (no
    // sharing across triangles) with sequential indices, matching how OBJFace's separate
    // position/normal/texCoord index streams don't line up cleanly with a single shared vertex
    // buffer. A group name of "" is left as-is (glTF's own "unnamed mesh" convention) --
    // filename-based fallback naming, if desired, is the caller's concern.
    //
    // Missing normals/UVs fall back to (0,1,0)/(0,0), matching GltfImporter::extractPrimitive()'s
    // own fallback for primitives without a NORMAL/TEXCOORD_0 accessor, so an imported OBJ looks
    // the same whether or not it carried normals/UVs to begin with.
    //
    // No material is assigned (GltfPrimitive::materialIndex stays -1); relative/negative OBJ
    // index references are not resolved (matches OBJFileReader/OBJSyntaxParser's own existing
    // limitation -- see PLAN's "スコープ外" section). Returns a document with no meshes if `obj`
    // has no positions or no non-empty group.
    static GltfDocument convert(const Phantom::File::OBJFile& obj);
};

} // namespace Phantom::Gltf
