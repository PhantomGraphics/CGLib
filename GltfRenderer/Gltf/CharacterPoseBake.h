#pragma once
// CPU bake of a character's pose into static meshes: linear-blend skinning (JOINTS_0 / WEIGHTS_0) of the skin matrices from
// CharacterPose.h, plus morph targets with the same weights as the viewport. Moved from PhantomStudio's CharacterPoseBaker
// (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 3). Used for OBJ / STL "bake current pose" export, since neither
// format has a skinning concept.
#include "CharacterPose.h"
#include "../../Graphics/Core/MeshData.h"

#include <vector>

namespace Phantom::Gltf {

	struct BakedPrimitive {
		Graphics::MeshData mesh;
		int materialIndex = -1;   // index into doc.materials, -1 = none
	};

	// One entry per mesh primitive that has positions and triangles, in meshes[] / primitives[] order. Positions and normals
	// are in the document's local (root-transform-relative) space; callers apply their own object transform on top.
	// Morph weights use the same priority as the viewport (clip, then manual override, then keys); skinning uses
	// doc.skins[0] when the primitive has joints. Returns empty without a document.
	std::vector<BakedPrimitive> bakeSkinnedPose(const CharacterPoseSource& src, double framePosition, double fps);

} // namespace Phantom::Gltf
