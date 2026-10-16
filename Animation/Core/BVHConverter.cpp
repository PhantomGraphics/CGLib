#include "BVHConverter.h"

using namespace Phantom::File;

namespace {

glm::quat axisAngleDeg(char axis, double degrees)
{
    const float radians = glm::radians(static_cast<float>(degrees));
    switch (axis) {
        case 'X': return glm::angleAxis(radians, glm::vec3(1.f, 0.f, 0.f));
        case 'Y': return glm::angleAxis(radians, glm::vec3(0.f, 1.f, 0.f));
        case 'Z': return glm::angleAxis(radians, glm::vec3(0.f, 0.f, 1.f));
        default:  return glm::quat(1.f, 0.f, 0.f, 0.f);
    }
}

} // namespace

namespace Phantom::Animation {

glm::quat BVHConverter::eulerToQuat(double a, double b, double c,
                                     const std::array<char, 3>& axisOrder)
{
    return axisAngleDeg(axisOrder[0], a) *
           axisAngleDeg(axisOrder[1], b) *
           axisAngleDeg(axisOrder[2], c);
}

bool BVHConverter::convert(const BVHFile& bvh,
                            Skeleton& outSkeleton,
                            AnimationClip& outClip)
{
    outSkeleton = Skeleton{};
    outClip = AnimationClip{};
    outClip.ticksPerSecond = bvh.frameTime > 0.0
        ? static_cast<float>(1.0 / bvh.frameTime) : 30.f;
    outClip.duration = bvh.numFrames > 1
        ? static_cast<float>((bvh.numFrames - 1) * bvh.frameTime) : 0.f;

    // channelStart[i] = index of joint i's first channel within a MOTION row.
    std::vector<int> boneIndexOf(bvh.joints.size(), -1);
    std::vector<int> channelStart(bvh.joints.size(), 0);

    int channelCursor = 0;
    for (size_t i = 0; i < bvh.joints.size(); ++i) {
        const auto& joint = bvh.joints[i];
        channelStart[i] = channelCursor;
        channelCursor += static_cast<int>(joint.channelNames.size());

        if (joint.isEndSite) continue; // End Site is a terminal marker, not a bone

        Bone bone;
        bone.name = joint.name;
        bone.parentIndex = (joint.parentIndex == -1) ? -1 : boneIndexOf[joint.parentIndex];
        bone.localPosition = glm::vec3(static_cast<float>(joint.offset.x),
                                        static_cast<float>(joint.offset.y),
                                        static_cast<float>(joint.offset.z));
        boneIndexOf[i] = outSkeleton.addBone(bone);
    }

    for (size_t i = 0; i < bvh.joints.size(); ++i) {
        const auto& joint = bvh.joints[i];
        if (joint.isEndSite || joint.channelNames.empty()) continue;

        int posChannel[3] = { -1, -1, -1 }; // per-axis offset within joint.channelNames
        std::array<int, 3> rotChannel = { -1, -1, -1 };
        std::array<char, 3> rotAxisOrder = { 'Z', 'X', 'Y' };
        int rotCount = 0;

        for (int c = 0; c < static_cast<int>(joint.channelNames.size()); ++c) {
            const std::string& name = joint.channelNames[c];
            if (name == "Xposition") posChannel[0] = c;
            else if (name == "Yposition") posChannel[1] = c;
            else if (name == "Zposition") posChannel[2] = c;
            else if (rotCount < 3 && name.size() == 9 && name.compare(1, 8, "rotation") == 0) {
                rotAxisOrder[rotCount] = name[0];
                rotChannel[rotCount] = c;
                ++rotCount;
            }
        }

        const bool hasPosition = posChannel[0] >= 0 && posChannel[1] >= 0 && posChannel[2] >= 0;
        const bool hasRotation = rotCount == 3;
        if (!hasPosition && !hasRotation) continue;

        BoneChannel boneChannel;
        boneChannel.boneIndex = boneIndexOf[i];

        for (int f = 0; f < bvh.numFrames; ++f) {
            const auto& row = bvh.motion[f];
            const float t = static_cast<float>(f * bvh.frameTime);
            const int base = channelStart[i];

            if (hasPosition) {
                const glm::vec3 pos(static_cast<float>(row[base + posChannel[0]]),
                                     static_cast<float>(row[base + posChannel[1]]),
                                     static_cast<float>(row[base + posChannel[2]]));
                boneChannel.positionKeys.push_back({ t, pos });
            }
            if (hasRotation) {
                const double a = row[base + rotChannel[0]];
                const double b = row[base + rotChannel[1]];
                const double c = row[base + rotChannel[2]];
                boneChannel.rotationKeys.push_back({ t, eulerToQuat(a, b, c, rotAxisOrder) });
            }
        }

        outClip.channels.push_back(std::move(boneChannel));
    }

    return true;
}

} // namespace Phantom::Animation
