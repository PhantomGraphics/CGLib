#pragma once
#include <string>
#include <vector>
#include "CGLib/Math/Vector3d.h"

// BVH (Biovision Hierarchy) motion-capture format raw structures.
// NOTE: unrelated to Phantom::Space::BVH (bounding-volume-hierarchy spatial
// structure) - this is the mocap file format, a plain-text skeleton + keyframe
// stream.

namespace Phantom {
    namespace File {

// One node of the HIERARCHY section: ROOT / JOINT / End Site.
struct BVHJoint {
    std::string name;
    Math::Vector3dd offset{ 0.0, 0.0, 0.0 };

    // Channel names in file order, e.g. {"Xposition","Yposition","Zposition",
    // "Zrotation","Xrotation","Yrotation"}. End Site joints have none.
    std::vector<std::string> channelNames;

    int parentIndex = -1;
    std::vector<int> childIndices;
    bool isEndSite = false;
};

struct BVHFile {
    std::vector<BVHJoint> joints; // index 0 = root, depth-first order

    int    numFrames = 0;
    double frameTime = 0.0; // seconds per frame

    // motion[frame][globalChannelIndex]; globalChannelIndex enumerates each
    // joint's channelNames in joint order (End Site joints contribute none).
    std::vector<std::vector<double>> motion;
};

    } // namespace File
} // namespace Phantom
