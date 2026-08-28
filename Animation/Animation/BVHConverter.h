#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "CGLib/File/File/BVHFile.h"
#include "Skeleton.h"
#include "AnimationClip.h"

#include <array>

namespace Phantom::Animation {

// Converts a BVHFile (parsed HIERARCHY + MOTION) into a Skeleton and an
// AnimationClip in a single call. Unlike VMD, BVH carries its own skeleton
// (the HIERARCHY section), so no external bone-name map is needed.
// NOTE: unrelated to Phantom::Space::BVH (bounding-volume-hierarchy).
class BVHConverter {
public:
    bool convert(const Phantom::File::BVHFile& bvh,
                 Skeleton& outSkeleton,
                 AnimationClip& outClip);

private:
    // Composes a rotation quaternion from three Euler angles (degrees), applied
    // in axisOrder[0] -> axisOrder[1] -> axisOrder[2] order, as encoded by a
    // joint's CHANNELS line (e.g. "Zrotation Xrotation Yrotation" -> {'Z','X','Y'}).
    static glm::quat eulerToQuat(double a, double b, double c,
                                  const std::array<char, 3>& axisOrder);
};

} // namespace Phantom::Animation
