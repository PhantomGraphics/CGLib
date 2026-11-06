#pragma once
// Pose of an imported character (a GltfDocument) at a scene time: the ONE place that combines the imported clip with the
// hand-authored bone / morph data. Moved from PhantomStudio (docs/todo/PLAN_phantomstudio_library_extraction.md Phase 3);
// the inputs are explicit (no scene object), so the viewport renderer, the OBJ/STL pose baker and the glTF exporter share it.
//
// Composition order
//   bones : bind pose -> placed clip (ClipPlacement) -> bone delta (static pose, overridden per channel by keys)
//           A delta is applied in the bone's local space: local = clipLocal * T * R * S. No keys and an identity pose
//           leave the clip untouched, so an unauthored character looks exactly like the plain GltfAnimationEvaluator.
//   morphs: clip weights -> manual override (a non-zero value wins) -> morph keys (a keyed morph wins over both, the way
//           a keyed object channel wins over its base value).
#include "GltfDocument.h"
#include "../../Animation/Core/AnimationTrack.h"

#include <glm/glm.hpp>

#include <vector>

namespace Phantom::Gltf {

	// Everything the pose needs. Pointers are borrowed for the duration of a call.
	struct CharacterPoseSource {
		const GltfDocument* doc = nullptr;
		int activeAnimation = -1;                                          // doc->animations index, -1 = bind pose
		const Animation::CharacterAnimation* authored = nullptr;           // null = unauthored (clip only)
		const std::vector<float>* morphOverride = nullptr;                 // flat morph index -> manual weight (0 = no override)
	};

	// Clip time (seconds) for the active clip at `framePosition`; 0 without an active clip.
	double characterClipTime(const CharacterPoseSource& src, double framePosition, double fps);

	// Length in seconds of the active clip (0 without one).
	double characterClipDuration(const CharacterPoseSource& src);

	// Skin matrices (joint global * inverse bind) of doc.skins[skinIndex]; empty if the index is out of range.
	std::vector<glm::mat4> evaluateCharacterSkin(const CharacterPoseSource& src, int skinIndex, double framePosition, double fps);

	// The document-space global transform of every node at `framePosition`: the placed clip (and blend), then the bone deltas.
	// The same composition as evaluateCharacterSkin, which takes the joints' entries of it times their inverse binds.
	// Nodes outside the default scene keep their identity transform (as in GltfAnimationEvaluator). Empty without a document.
	std::vector<glm::mat4> evaluateCharacterNodeGlobals(const CharacterPoseSource& src, double framePosition, double fps);

	// Every morph primitive's target weights at `framePosition`, concatenated in the order Phantom.Rendering's host pose expects:
	// nodes in document order, a node's mesh primitives in order, a primitive counted when it has a POSITION accessor and at least
	// one morph target (the same rule the renderer uses to build its morph list). Each weight is evaluateCharacterMorphWeights.
	std::vector<float> evaluateCharacterMorphPose(const CharacterPoseSource& src, double framePosition, double fps);

	// Weights of one mesh's `targetCount` morph targets whose first target has flat index `flatBase` (the traversal order of
	// meshes[] / primitives[] / targets). `nodeIndex` is the node owning the mesh (-1 = none: no clip weights).
	std::vector<float> evaluateCharacterMorphWeights(const CharacterPoseSource& src, int nodeIndex, int targetCount, int flatBase,
	                                                  double framePosition, double fps);

	// Bind-pose box of every mesh primitive's POSITION accessor. False when the document has no geometry.
	bool computeBindPoseBounds(const GltfDocument& doc, glm::vec3& lo, glm::vec3& hi);

} // namespace Phantom::Gltf
