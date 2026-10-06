#pragma once

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

#include "../Gltf/GltfDocument.h"

namespace Phantom::Gltf {

    // CPU-only state for object (node TRS) animation of unskinned meshes, split out of
    // GltfSceneRenderer so the clip selection / animated-node mask / dirty tracking can be
    // tested without a GPU. The renderer still owns the primitives and their re-bake; it asks
    // this class which nodes move and what their world matrices are at the current time.
    //
    // The document is a non-owning pointer: it must outlive the next reset().
    class GltfObjectAnimation {
    public:
        // Drops the clip, time and mask; rebinds to `doc` (nullptr = no document).
        void reset(const GltfDocument* doc);

        // Out-of-range indices (and -1) disable the clip. Changing the clip marks the state dirty
        // and rebuilds the animated-node mask: TRS channel targets plus their whole subtree.
        // Weights channels do not move their node (they drive morph targets) and are skipped.
        void setClip(int clipIndex);
        void setTime(float seconds);

        int   clip() const { return clip_; }
        float time() const { return time_; }
        int   clipCount() const;
        float duration(int clipIndex) const;

        // Returns true once after setClip()/setTime() changed something, then clears the flag.
        bool consumeDirty();

        // True if the active clip moves `nodeIndex` (or one of its ancestors).
        bool isNodeAnimated(int nodeIndex) const;

        // Node world matrices at the current clip/time. Empty when no clip is active.
        std::vector<glm::mat4> evaluateGlobals() const;

    private:
        void markSubtree(int nodeIndex);

        const GltfDocument*  doc_   = nullptr;
        int                  clip_  = -1;
        float                time_  = 0.f;
        bool                 dirty_ = false;
        std::vector<uint8_t> animatedNode_;
    };

} // namespace Phantom::Gltf
