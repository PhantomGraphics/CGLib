#include "GltfObjectAnimation.h"
#include "../Gltf/GltfAnimationEvaluator.h"

namespace Phantom::Gltf {

void GltfObjectAnimation::reset(const GltfDocument* doc)
{
    doc_   = doc;
    clip_  = -1;
    time_  = 0.f;
    dirty_ = false;
    animatedNode_.clear();
}

void GltfObjectAnimation::setClip(int clipIndex)
{
    const int clamped = (doc_ && clipIndex >= 0 && clipIndex < (int)doc_->animations.size()) ? clipIndex : -1;
    if (clamped == clip_) return;
    clip_  = clamped;
    dirty_ = true; // force a re-bake (to the clip's t=0, or back to rest)

    // Which nodes does this clip *move*? A TRS channel target plus its whole subtree (a spinning
    // pivot carries its children). Everything else stays at its build-time bake. A Weights
    // channel doesn't move its node -- it drives morph targets, applied separately -- so it must
    // not mark the node for a transform re-bake (that would overwrite the morph with base geometry).
    animatedNode_.assign(doc_ ? doc_->nodes.size() : 0, 0);
    if (clip_ >= 0) {
        for (const auto& ch : doc_->animations[clip_].channels) {
            if (ch.target.path == GltfAnimationPath::Weights) continue;
            markSubtree(ch.target.node);
        }
    }
}

void GltfObjectAnimation::markSubtree(int nodeIndex)
{
    if (nodeIndex < 0 || nodeIndex >= (int)animatedNode_.size() || animatedNode_[nodeIndex]) return;
    animatedNode_[nodeIndex] = 1;
    for (int child : doc_->nodes[nodeIndex].children)
        markSubtree(child);
}

void GltfObjectAnimation::setTime(float seconds)
{
    if (seconds != time_) { time_ = seconds; dirty_ = true; }
}

int GltfObjectAnimation::clipCount() const
{
    return doc_ ? static_cast<int>(doc_->animations.size()) : 0;
}

float GltfObjectAnimation::duration(int clipIndex) const
{
    if (!doc_ || clipIndex < 0 || clipIndex >= (int)doc_->animations.size()) return 0.f;
    return GltfAnimationEvaluator::duration(doc_->animations[clipIndex], *doc_);
}

bool GltfObjectAnimation::consumeDirty()
{
    const bool d = dirty_;
    dirty_ = false;
    return d;
}

bool GltfObjectAnimation::isNodeAnimated(int nodeIndex) const
{
    return nodeIndex >= 0 && nodeIndex < (int)animatedNode_.size() && animatedNode_[nodeIndex] != 0;
}

std::vector<glm::mat4> GltfObjectAnimation::evaluateGlobals() const
{
    if (!doc_ || clip_ < 0) return {};
    return GltfAnimationEvaluator::evaluateNodeGlobalTransforms(*doc_, clip_, time_);
}

} // namespace Phantom::Gltf
