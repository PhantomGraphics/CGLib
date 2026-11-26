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
    // doc.skins[0] when the primitive has joints. Returns empty without a document or for an out-of-range weighted joint.
	std::vector<BakedPrimitive> bakeSkinnedPose(const CharacterPoseSource& src, double framePosition, double fps);

	// The pose sampled on every integer frame of [startFrame, endFrame] as node-local TRS and morph weights: the animation a
	// skinned export writes so the file plays the motion the viewport shows. Moved from PhantomStudio's glTF exporter.
	struct CharacterJointSamples {
		int node = -1;                       // a joint of one of the skins; each node once, ascending
		std::vector<glm::vec3> translations; // one per time
		std::vector<glm::vec4> rotations;    // quaternion xyzw, kept in the hemisphere of the previous frame
		std::vector<glm::vec3> scales;
	};
	struct CharacterMorphSamples {
		int node = -1;                       // the first node showing the mesh
		int targetCount = 0;
		std::vector<float> weights;          // targetCount values per time
	};
	struct CharacterChannelSamples {
		std::vector<float> times;            // seconds (frame / fps)
		std::vector<CharacterJointSamples> joints;
		std::vector<CharacterMorphSamples> morphs; // in mesh order
	};
	// Empty without a document or when endFrame < startFrame.
	CharacterChannelSamples sampleCharacterChannels(const CharacterPoseSource& src, int startFrame, int endFrame, double fps);

} // namespace Phantom::Gltf
