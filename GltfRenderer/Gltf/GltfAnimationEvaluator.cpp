#include "GltfAnimationEvaluator.h"
#include "GltfAccessorView.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <unordered_map>

using namespace Phantom::Gltf;

namespace {

// Which sampler (if any) drives each TRS component of a node, for one animation.
struct NodeChannels {
    int translationSampler = -1;
    int rotationSampler    = -1;
    int scaleSampler       = -1;
};

std::unordered_map<int, NodeChannels> collectNodeChannels(const GltfAnimation& anim) {
    std::unordered_map<int, NodeChannels> map;
    for (const auto& ch : anim.channels) {
        if (ch.target.node < 0 || ch.samplerIndex < 0) continue;
        NodeChannels& nc = map[ch.target.node];
        switch (ch.target.path) {
        case GltfAnimationPath::Translation: nc.translationSampler = ch.samplerIndex; break;
        case GltfAnimationPath::Rotation:    nc.rotationSampler    = ch.samplerIndex; break;
        case GltfAnimationPath::Scale:       nc.scaleSampler       = ch.samplerIndex; break;
        default: break; // Weights handled separately by evaluateMorphWeights()
        }
    }
    return map;
}

std::vector<float> readTimes(const GltfDocument& doc, int accessorIndex) {
    GltfAccessorView view(doc, accessorIndex);
    std::vector<float> times(view.count());
    for (size_t i = 0; i < times.size(); ++i)
        times[i] = view.get<float>(i);
    return times;
}

// Bracketing keyframe pair [lo, hi] and the linear blend factor between them for timeSec.
// Out-of-range times clamp to the first/last keyframe (lo == hi, alpha == 0), matching
// Phantom::Animation::Animator's interpolation convention.
struct Bracket { size_t lo; size_t hi; float alpha; };

Bracket findBracket(const std::vector<float>& times, float timeSec) {
    if (times.empty()) return {0, 0, 0.f};
    if (times.size() == 1 || timeSec <= times.front()) return {0, 0, 0.f};
    const size_t last = times.size() - 1;
    if (timeSec >= times.back()) return {last, last, 0.f};

    const auto it = std::lower_bound(times.begin(), times.end(), timeSec);
    const size_t hi = static_cast<size_t>(it - times.begin());
    const size_t lo = hi - 1;
    const float alpha = (times[hi] > times[lo]) ? (timeSec - times[lo]) / (times[hi] - times[lo]) : 0.f;
    return {lo, hi, alpha};
}

// Hermite basis for CUBICSPLINE (glTF spec's "Appendix C"): p0/p1 are the keyframe values,
// m0/m1 the (already dt-scaled) out-/in-tangents, s in [0,1].
template<typename T>
T hermite(const T& p0, const T& m0, const T& p1, const T& m1, float s) {
    const float s2 = s * s;
    const float s3 = s2 * s;
    return (2.f*s3 - 3.f*s2 + 1.f) * p0
         + (s3 - 2.f*s2 + s)       * m0
         + (-2.f*s3 + 3.f*s2)      * p1
         + (s3 - s2)               * m1;
}

// For CUBICSPLINE the output accessor holds three elements per keyframe: in-tangent, value,
// out-tangent. LINEAR/STEP hold one (the value).
size_t valueElem(GltfInterpolation interp, size_t keyframe) {
    return interp == GltfInterpolation::CubicSpline ? keyframe * 3 + 1 : keyframe;
}

// STEP holds the value of the last keyframe at-or-before timeSec. findBracket() returns
// alpha==1 exactly when timeSec has reached times[hi] (or is past the range), so hi is the
// right index there; otherwise lo.
size_t stepKeyframe(const Bracket& b) {
    return (b.alpha >= 1.f) ? b.hi : b.lo;
}

glm::vec3 evaluateVec3Sampler(const GltfDocument& doc, const GltfAnimationSampler& sampler,
                               float timeSec, const glm::vec3& fallback) {
    if (sampler.input < 0 || sampler.output < 0) return fallback;
    const std::vector<float> times = readTimes(doc, sampler.input);
    if (times.empty()) return fallback;
    GltfAccessorView values(doc, sampler.output);
    const Bracket b = findBracket(times, timeSec);

    if (sampler.interpolation == GltfInterpolation::Step)
        return values.get<glm::vec3>(valueElem(sampler.interpolation, stepKeyframe(b)));
    if (b.lo == b.hi)
        return values.get<glm::vec3>(valueElem(sampler.interpolation, b.lo));

    if (sampler.interpolation == GltfInterpolation::CubicSpline) {
        const float dt = times[b.hi] - times[b.lo];
        const glm::vec3 p0 = values.get<glm::vec3>(b.lo * 3 + 1);
        const glm::vec3 m0 = values.get<glm::vec3>(b.lo * 3 + 2) * dt;
        const glm::vec3 p1 = values.get<glm::vec3>(b.hi * 3 + 1);
        const glm::vec3 m1 = values.get<glm::vec3>(b.hi * 3 + 0) * dt;
        return hermite(p0, m0, p1, m1, b.alpha);
    }
    return glm::mix(values.get<glm::vec3>(b.lo), values.get<glm::vec3>(b.hi), b.alpha);
}

glm::quat readQuatXYZW(const GltfAccessorView& values, size_t i) {
    const glm::vec4 v = values.get<glm::vec4>(i); // stored xyzw (see GltfNode::rotation)
    return glm::quat(v.w, v.x, v.y, v.z);
}

glm::quat evaluateQuatSampler(const GltfDocument& doc, const GltfAnimationSampler& sampler,
                               float timeSec, const glm::quat& fallback) {
    if (sampler.input < 0 || sampler.output < 0) return fallback;
    const std::vector<float> times = readTimes(doc, sampler.input);
    if (times.empty()) return fallback;
    GltfAccessorView values(doc, sampler.output);
    const Bracket b = findBracket(times, timeSec);

    if (sampler.interpolation == GltfInterpolation::Step)
        return glm::normalize(readQuatXYZW(values, valueElem(sampler.interpolation, stepKeyframe(b))));
    if (b.lo == b.hi)
        return glm::normalize(readQuatXYZW(values, valueElem(sampler.interpolation, b.lo)));

    if (sampler.interpolation == GltfInterpolation::CubicSpline) {
        // Spec: Hermite-interpolate the raw xyzw, then normalize.
        const float dt = times[b.hi] - times[b.lo];
        const glm::vec4 p0 = values.get<glm::vec4>(b.lo * 3 + 1);
        const glm::vec4 m0 = values.get<glm::vec4>(b.lo * 3 + 2) * dt;
        const glm::vec4 p1 = values.get<glm::vec4>(b.hi * 3 + 1);
        const glm::vec4 m1 = values.get<glm::vec4>(b.hi * 3 + 0) * dt;
        const glm::vec4 q = glm::normalize(hermite(p0, m0, p1, m1, b.alpha));
        return glm::quat(q.w, q.x, q.y, q.z);
    }
    return glm::normalize(glm::slerp(readQuatXYZW(values, b.lo), readQuatXYZW(values, b.hi), b.alpha));
}

std::vector<float> evaluateWeightsSampler(const GltfDocument& doc, const GltfAnimationSampler& sampler,
                                           int targetCount, float timeSec) {
    std::vector<float> result(targetCount > 0 ? targetCount : 0, 0.f);
    if (sampler.input < 0 || sampler.output < 0 || targetCount <= 0) return result;
    const std::vector<float> times = readTimes(doc, sampler.input);
    if (times.empty()) return result;
    GltfAccessorView values(doc, sampler.output); // flattened: keyframe-major, target-minor
    const Bracket b = findBracket(times, timeSec);
    const size_t tc     = static_cast<size_t>(targetCount);
    const bool   cubic  = (sampler.interpolation == GltfInterpolation::CubicSpline);
    const bool   isStep = (sampler.interpolation == GltfInterpolation::Step);
    const bool   step   = isStep || (b.lo == b.hi);
    const size_t stride = cubic ? tc * 3 : tc;      // floats per keyframe block
    const size_t vbase  = cubic ? tc : 0;          // offset of the "value" sub-block
    const size_t loKey  = isStep ? stepKeyframe(b) : b.lo;

    for (int i = 0; i < targetCount; ++i) {
        const size_t iu = static_cast<size_t>(i);
        const float w0 = values.get<float>(loKey * stride + vbase + iu);
        if (step) { result[i] = w0; continue; }
        if (cubic) {
            const float dt = times[b.hi] - times[b.lo];
            const float m0 = values.get<float>(b.lo * stride + tc * 2 + iu) * dt; // out-tangent
            const float p1 = values.get<float>(b.hi * stride + tc + iu);
            const float m1 = values.get<float>(b.hi * stride + iu) * dt;          // in-tangent
            result[i] = hermite(w0, m0, p1, m1, b.alpha);
        } else {
            const float w1 = values.get<float>(b.hi * stride + iu);
            result[i] = glm::mix(w0, w1, b.alpha);
        }
    }
    return result;
}

glm::mat4 staticLocalTransform(const GltfNode& node) {
    if (node.hasMatrix) return node.matrix;
    glm::mat4 T = glm::translate(glm::mat4(1.f), node.translation);
    glm::quat q(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
    glm::mat4 R = glm::mat4_cast(q);
    glm::mat4 S = glm::scale(glm::mat4(1.f), node.scale);
    return T * R * S;
}

void traverseGlobal(const GltfDocument& doc, int nodeIndex, const glm::mat4& parentTransform,
                     const std::unordered_map<int, NodeChannels>& nodeChannels,
                     const GltfAnimation* anim, float timeSec,
                     std::vector<glm::mat4>& globalTransforms)
{
    if (nodeIndex < 0 || nodeIndex >= static_cast<int>(doc.nodes.size())) return;
    const GltfNode& node = doc.nodes[nodeIndex];

    glm::mat4 local;
    // Per glTF spec, animated nodes never use the matrix form, so channels only need to be
    // consulted for TRS nodes.
    if (node.hasMatrix) {
        local = node.matrix;
    } else {
        glm::vec3 t = node.translation;
        glm::quat r(node.rotation.w, node.rotation.x, node.rotation.y, node.rotation.z);
        glm::vec3 s = node.scale;

        const auto it = anim ? nodeChannels.find(nodeIndex) : nodeChannels.end();
        if (it != nodeChannels.end()) {
            const NodeChannels& nc = it->second;
            if (nc.translationSampler >= 0)
                t = evaluateVec3Sampler(doc, anim->samplers[nc.translationSampler], timeSec, t);
            if (nc.rotationSampler >= 0)
                r = evaluateQuatSampler(doc, anim->samplers[nc.rotationSampler], timeSec, r);
            if (nc.scaleSampler >= 0)
                s = evaluateVec3Sampler(doc, anim->samplers[nc.scaleSampler], timeSec, s);
        }

        local = glm::translate(glm::mat4(1.f), t) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.f), s);
    }

    const glm::mat4 world = parentTransform * local;
    globalTransforms[nodeIndex] = world;

    for (int child : node.children)
        traverseGlobal(doc, child, world, nodeChannels, anim, timeSec, globalTransforms);
}

std::vector<glm::mat4> computeGlobalTransforms(const GltfDocument& doc, int animationIndex, float timeSec) {
    std::vector<glm::mat4> globalTransforms(doc.nodes.size(), glm::mat4(1.f));

    const GltfAnimation* anim = nullptr;
    std::unordered_map<int, NodeChannels> nodeChannels;
    if (animationIndex >= 0 && animationIndex < static_cast<int>(doc.animations.size())) {
        anim = &doc.animations[animationIndex];
        nodeChannels = collectNodeChannels(*anim);
    }

    int sceneIdx = doc.defaultScene;
    if (sceneIdx < 0 || sceneIdx >= static_cast<int>(doc.scenes.size())) sceneIdx = 0;
    if (!doc.scenes.empty()) {
        for (int root : doc.scenes[sceneIdx].nodes)
            traverseGlobal(doc, root, glm::mat4(1.f), nodeChannels, anim, timeSec, globalTransforms);
    }
    return globalTransforms;
}

} // namespace

std::vector<glm::mat4> GltfAnimationEvaluator::evaluateSkin(const GltfDocument& doc,
                                                              int animationIndex, int skinIndex, float timeSec)
{
    std::vector<glm::mat4> skinMatrices;
    if (skinIndex < 0 || skinIndex >= static_cast<int>(doc.skins.size())) return skinMatrices;

    const std::vector<glm::mat4> globalTransforms = computeGlobalTransforms(doc, animationIndex, timeSec);

    const GltfSkin& skin = doc.skins[skinIndex];
    skinMatrices.resize(skin.joints.size(), glm::mat4(1.f));
    for (size_t j = 0; j < skin.joints.size(); ++j) {
        const int node = skin.joints[j];
        if (node < 0 || node >= static_cast<int>(globalTransforms.size())) continue;
        const glm::mat4 ibm = (j < skin.inverseBindMatrices.size()) ? skin.inverseBindMatrices[j] : glm::mat4(1.f);
        skinMatrices[j] = globalTransforms[node] * ibm;
    }
    return skinMatrices;
}

std::vector<glm::mat4> GltfAnimationEvaluator::evaluateNodeGlobalTransforms(const GltfDocument& doc,
                                                                            int animationIndex, float timeSec)
{
    return computeGlobalTransforms(doc, animationIndex, timeSec);
}

std::vector<float> GltfAnimationEvaluator::evaluateMorphWeights(const GltfDocument& doc,
                                                                  int animationIndex, int nodeIndex,
                                                                  int targetCount, float timeSec)
{
    std::vector<float> result(targetCount > 0 ? targetCount : 0, 0.f);
    if (nodeIndex >= 0 && nodeIndex < static_cast<int>(doc.nodes.size())) {
        const auto& node = doc.nodes[nodeIndex];
        const std::vector<float>* defaults = &node.weights;
        if (defaults->empty() && node.meshIndex >= 0 && node.meshIndex < static_cast<int>(doc.meshes.size()))
            defaults = &doc.meshes[node.meshIndex].weights;
        std::copy_n(defaults->begin(), std::min(result.size(), defaults->size()), result.begin());
    }
    if (animationIndex < 0 || animationIndex >= static_cast<int>(doc.animations.size())) return result;

    const GltfAnimation& anim = doc.animations[animationIndex];
    for (const auto& ch : anim.channels) {
        if (ch.target.path != GltfAnimationPath::Weights || ch.target.node != nodeIndex) continue;
        if (ch.samplerIndex < 0 || ch.samplerIndex >= static_cast<int>(anim.samplers.size())) continue;
        return evaluateWeightsSampler(doc, anim.samplers[ch.samplerIndex], targetCount, timeSec);
    }
    return result;
}

float GltfAnimationEvaluator::duration(const GltfAnimation& anim, const GltfDocument& doc) {
    float maxTime = 0.f;
    for (const auto& sampler : anim.samplers) {
        if (sampler.input < 0) continue;
        GltfAccessorView times(doc, sampler.input);
        if (times.count() == 0) continue;
        maxTime = std::max(maxTime, times.get<float>(times.count() - 1));
    }
    return maxTime;
}
