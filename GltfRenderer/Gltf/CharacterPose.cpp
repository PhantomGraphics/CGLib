#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "CharacterPose.h"

#include "GltfAccessorView.h"
#include "GltfAnimationEvaluator.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <limits>

namespace Phantom::Gltf {

namespace {

using Animation::BoneTrack;
using Animation::CharacterAnimation;
using Animation::TrackTransform;
using Animation::clipLocalTime;
using Animation::eulerToQuaternion;
using Animation::evaluateTracks;

const CharacterAnimation& authoredOf(const CharacterPoseSource& src) {
	static const CharacterAnimation kNone;
	return src.authored ? *src.authored : kNone;
}

glm::mat4 deltaMatrix(const TrackTransform& d) {
	const auto q = eulerToQuaternion(d.rotationEulerDeg);  // w, x, y, z
	const glm::quat rotation(static_cast<float>(q[0]), static_cast<float>(q[1]), static_cast<float>(q[2]), static_cast<float>(q[3]));
	return glm::translate(glm::mat4(1.f), glm::vec3(d.translation[0], d.translation[1], d.translation[2])) *
	       glm::mat4_cast(rotation) *
	       glm::scale(glm::mat4(1.f), glm::vec3(d.scale[0], d.scale[1], d.scale[2]));
}

void propagate(const GltfDocument& doc, int node, const glm::mat4& parent, const std::vector<glm::mat4>& locals,
               std::vector<glm::mat4>& globals) {
	if (node < 0 || node >= static_cast<int>(doc.nodes.size())) return;
	const glm::mat4 world = parent * locals[node];
	globals[node] = world;
	for (int child : doc.nodes[node].children) propagate(doc, child, world, locals, globals);
}

// Applies the bone deltas to the clip's global transforms. Locals are recovered from the clip's globals (the evaluator only
// exposes globals), edited, and the hierarchy is rebuilt from the scene roots the evaluator used.
void applyBoneDeltas(const GltfDocument& doc, const CharacterAnimation& anim, double framePosition,
                     std::vector<glm::mat4>& globals) {
	const size_t n = doc.nodes.size();
	std::vector<int> parent(n, -1);
	for (size_t i = 0; i < n; ++i)
		for (int child : doc.nodes[i].children)
			if (child >= 0 && child < static_cast<int>(n)) parent[child] = static_cast<int>(i);

	std::vector<glm::mat4> locals(n, glm::mat4(1.f));
	for (size_t i = 0; i < n; ++i)
		locals[i] = parent[i] < 0 ? globals[i] : glm::inverse(globals[parent[i]]) * globals[i];

	for (const BoneTrack& bone : anim.bones) {
		if (bone.node < 0 || bone.node >= static_cast<int>(n)) continue;
		TrackTransform delta = bone.pose;
		evaluateTracks(bone.tracks, framePosition, delta);
		locals[bone.node] = locals[bone.node] * deltaMatrix(delta);
	}

	int scene = doc.defaultScene;
	if (scene < 0 || scene >= static_cast<int>(doc.scenes.size())) scene = 0;
	if (doc.scenes.empty()) return;
	for (int root : doc.scenes[scene].nodes) propagate(doc, root, glm::mat4(1.f), locals, globals);
}

// Mixes two sets of node globals per bone: translation and scale lerp, rotation slerp. Locals are recovered from the globals
// (parents first is not required: each local only needs its parent's global), mixed, and the hierarchy is rebuilt.
void mixGlobals(const GltfDocument& doc, const std::vector<glm::mat4>& a, const std::vector<glm::mat4>& b, float w,
                std::vector<glm::mat4>& out) {
	const size_t n = doc.nodes.size();
	std::vector<int> parent(n, -1);
	for (size_t i = 0; i < n; ++i)
		for (int child : doc.nodes[i].children)
			if (child >= 0 && child < static_cast<int>(n)) parent[child] = static_cast<int>(i);
	auto local = [&](const std::vector<glm::mat4>& g, size_t i) { return parent[i] < 0 ? g[i] : glm::inverse(g[parent[i]]) * g[i]; };
	std::vector<glm::mat4> locals(n, glm::mat4(1.f));
	for (size_t i = 0; i < n; ++i) {
		const glm::mat4 la = local(a, i), lb = local(b, i);
		auto split = [](const glm::mat4& m, glm::vec3& t, glm::quat& r, glm::vec3& s) {
			t = glm::vec3(m[3]);
			s = glm::vec3(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
			glm::mat3 rot(glm::vec3(m[0]) / (s.x > 1e-8f ? s.x : 1.f), glm::vec3(m[1]) / (s.y > 1e-8f ? s.y : 1.f), glm::vec3(m[2]) / (s.z > 1e-8f ? s.z : 1.f));
			r = glm::normalize(glm::quat_cast(rot));
		};
		glm::vec3 ta, sa, tb, sb; glm::quat ra, rb;
		split(la, ta, ra, sa); split(lb, tb, rb, sb);
		locals[i] = glm::translate(glm::mat4(1.f), glm::mix(ta, tb, w)) * glm::mat4_cast(glm::slerp(ra, rb, w)) *
		            glm::scale(glm::mat4(1.f), glm::mix(sa, sb, w));
	}
	out.assign(n, glm::mat4(1.f));
	int scene = doc.defaultScene;
	if (scene < 0 || scene >= static_cast<int>(doc.scenes.size())) scene = 0;
	if (doc.scenes.empty()) { out = a; return; }
	for (int root : doc.scenes[scene].nodes) propagate(doc, root, glm::mat4(1.f), locals, out);
}

double blendClipTime(const CharacterPoseSource& src, double framePosition, double fps) {
	const auto& b = authoredOf(src).blend;
	if (!src.doc || b.clip < 0 || b.clip >= static_cast<int>(src.doc->animations.size())) return 0.0;
	return clipLocalTime(b.placement, fps > 0.0 ? framePosition / fps : 0.0, fps,
	                     GltfAnimationEvaluator::duration(src.doc->animations[b.clip], *src.doc));
}

} // namespace

double characterClipDuration(const CharacterPoseSource& src) {
	if (!src.doc || src.activeAnimation < 0 || src.activeAnimation >= static_cast<int>(src.doc->animations.size())) return 0.0;
	return GltfAnimationEvaluator::duration(src.doc->animations[src.activeAnimation], *src.doc);
}

double characterClipTime(const CharacterPoseSource& src, double framePosition, double fps) {
	if (!src.doc || src.activeAnimation < 0 || src.activeAnimation >= static_cast<int>(src.doc->animations.size())) return 0.0;
	return clipLocalTime(authoredOf(src).clip, fps > 0.0 ? framePosition / fps : 0.0, fps, characterClipDuration(src));
}

std::vector<glm::mat4> evaluateCharacterSkin(const CharacterPoseSource& src, int skinIndex, double framePosition, double fps) {
	std::vector<glm::mat4> skinMatrices;
	if (!src.doc) return skinMatrices;
	const GltfDocument& doc = *src.doc;
	if (skinIndex < 0 || skinIndex >= static_cast<int>(doc.skins.size())) return skinMatrices;

	const CharacterAnimation& authored = authoredOf(src);
	const float clipTime = static_cast<float>(characterClipTime(src, framePosition, fps));
	// An unauthored character takes the evaluator's own path, so its output is bit-identical to the plain evaluator.
	const double blendW = authored.blend.weightAt(framePosition);
	if (authored.bones.empty() && blendW <= 0.0)
		return GltfAnimationEvaluator::evaluateSkin(doc, src.activeAnimation, skinIndex, clipTime);

	std::vector<glm::mat4> globals = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, src.activeAnimation, clipTime);
	if (blendW > 0.0) {
		const auto other = GltfAnimationEvaluator::evaluateNodeGlobalTransforms(doc, authored.blend.clip,
			static_cast<float>(blendClipTime(src, framePosition, fps)));
		if (blendW >= 1.0) globals = other;
		else { std::vector<glm::mat4> mixed; mixGlobals(doc, globals, other, static_cast<float>(blendW), mixed); globals = std::move(mixed); }
	}
	applyBoneDeltas(doc, authored, framePosition, globals);
	const GltfSkin& skin = doc.skins[skinIndex];
	skinMatrices.resize(skin.joints.size(), glm::mat4(1.f));
	for (size_t j = 0; j < skin.joints.size(); ++j) {
		const int node = skin.joints[j];
		if (node < 0 || node >= static_cast<int>(globals.size())) continue;
		const glm::mat4 ibm = j < skin.inverseBindMatrices.size() ? skin.inverseBindMatrices[j] : glm::mat4(1.f);
		skinMatrices[j] = globals[node] * ibm;
	}
	return skinMatrices;
}

std::vector<float> evaluateCharacterMorphWeights(const CharacterPoseSource& src, int nodeIndex, int targetCount, int flatBase,
                                                 double framePosition, double fps) {
	std::vector<float> weights(targetCount > 0 ? static_cast<size_t>(targetCount) : 0, 0.f);
	if (!src.doc || weights.empty()) return weights;
	const CharacterAnimation& authored = authoredOf(src);
	if (src.activeAnimation >= 0 && nodeIndex >= 0) {
		weights = GltfAnimationEvaluator::evaluateMorphWeights(*src.doc, src.activeAnimation, nodeIndex, targetCount,
			static_cast<float>(characterClipTime(src, framePosition, fps)));
		const double w = authored.blend.weightAt(framePosition);
		if (w > 0.0) {
			const auto other = GltfAnimationEvaluator::evaluateMorphWeights(*src.doc, authored.blend.clip, nodeIndex, targetCount,
				static_cast<float>(blendClipTime(src, framePosition, fps)));
			for (size_t t = 0; t < weights.size() && t < other.size(); ++t) weights[t] += (other[t] - weights[t]) * static_cast<float>(w);
		}
	}
	for (size_t t = 0; t < weights.size(); ++t) {
		const size_t flat = static_cast<size_t>(flatBase) + t;
		if (src.morphOverride && flat < src.morphOverride->size() && (*src.morphOverride)[flat] != 0.f) weights[t] = (*src.morphOverride)[flat];
		const auto keyed = authored.morphs.find(static_cast<int>(flat));
		if (keyed != authored.morphs.end() && !keyed->second.empty()) {
			TrackTransform w;
			w.translation[0] = weights[t];
			evaluateTracks(keyed->second, framePosition, w);
			weights[t] = std::clamp(w.translation[0], 0.f, 1.f);
		}
	}
	return weights;
}

bool computeBindPoseBounds(const GltfDocument& doc, glm::vec3& lo, glm::vec3& hi) {
	glm::vec3 mn(std::numeric_limits<float>::max()), mx(std::numeric_limits<float>::lowest());
	bool valid = false;
	for (const auto& mesh : doc.meshes) {
		for (const auto& prim : mesh.primitives) {
			if (prim.positionAccessor < 0 || prim.positionAccessor >= static_cast<int>(doc.accessors.size())) continue;
			GltfAccessorView view(doc, prim.positionAccessor);
			for (size_t i = 0; i < view.count(); ++i) {
				const glm::vec3 p = view.get<glm::vec3>(i);
				mn = glm::min(mn, p);
				mx = glm::max(mx, p);
				valid = true;
			}
		}
	}
	if (valid) { lo = mn; hi = mx; }
	return valid;
}

} // namespace Phantom::Gltf
