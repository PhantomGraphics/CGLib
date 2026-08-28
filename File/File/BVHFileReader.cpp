#include "BVHFileReader.h"

#include <charconv>
#include <fstream>

using namespace Phantom::File;

namespace {

// Reads one whitespace-delimited token and parses it as a number via
// std::from_chars (exceptions are forbidden in this codebase, see
// docs/guide/conventions.md). Returns false if the token is missing or
// not a valid number.
template<typename T>
bool readNumber(std::istream& stream, T& out)
{
    std::string tok;
    if (!(stream >> tok)) return false;

    const auto begin = tok.data();
    const auto end = tok.data() + tok.size();
    const auto result = std::from_chars(begin, end, out);
    return result.ec == std::errc() && result.ptr == end;
}

} // namespace

bool BVHFileReader::read(const std::filesystem::path& path)
{
    std::ifstream stream(path);
    if (!stream.is_open()) {
        return false;
    }
    return read(stream);
}

bool BVHFileReader::read(std::istream& stream)
{
    bvh_ = BVHFile{};
    return readHierarchy(stream) && readMotion(stream);
}

bool BVHFileReader::readHierarchy(std::istream& stream)
{
    std::string tok;

    if (!(stream >> tok) || tok != "HIERARCHY") return false;
    if (!(stream >> tok) || tok != "ROOT") return false;

    return readJoint(stream, -1);
}

bool BVHFileReader::readJoint(std::istream& stream, int parentIndex)
{
    std::string name;
    if (!(stream >> name)) return false;

    std::string brace;
    if (!(stream >> brace) || brace != "{") return false;

    const int thisIndex = static_cast<int>(bvh_.joints.size());
    BVHJoint joint;
    joint.name = name;
    joint.parentIndex = parentIndex;
    bvh_.joints.push_back(std::move(joint));

    for (;;) {
        std::string token;
        if (!(stream >> token)) return false;

        if (token == "}") {
            break;
        } else if (token == "OFFSET") {
            Math::Vector3dd offset;
            if (!readNumber(stream, offset.x) ||
                !readNumber(stream, offset.y) ||
                !readNumber(stream, offset.z)) {
                return false;
            }
            bvh_.joints[thisIndex].offset = offset;
        } else if (token == "CHANNELS") {
            int channelCount = 0;
            if (!readNumber(stream, channelCount) || channelCount < 0) return false;

            std::vector<std::string> channelNames(channelCount);
            for (auto& channelName : channelNames) {
                if (!(stream >> channelName)) return false;
            }
            bvh_.joints[thisIndex].channelNames = std::move(channelNames);
        } else if (token == "JOINT") {
            const int childIndex = static_cast<int>(bvh_.joints.size());
            if (!readJoint(stream, thisIndex)) return false;
            bvh_.joints[thisIndex].childIndices.push_back(childIndex);
        } else if (token == "End") {
            std::string site;
            if (!(stream >> site) || site != "Site") return false;

            const int childIndex = static_cast<int>(bvh_.joints.size());
            if (!readEndSite(stream, thisIndex)) return false;
            bvh_.joints[thisIndex].childIndices.push_back(childIndex);
        } else {
            return false;
        }
    }

    return true;
}

bool BVHFileReader::readEndSite(std::istream& stream, int parentIndex)
{
    std::string brace;
    if (!(stream >> brace) || brace != "{") return false;

    BVHJoint joint;
    joint.name = "End Site";
    joint.parentIndex = parentIndex;
    joint.isEndSite = true;

    std::string tok;
    if (!(stream >> tok) || tok != "OFFSET") return false;
    if (!readNumber(stream, joint.offset.x) ||
        !readNumber(stream, joint.offset.y) ||
        !readNumber(stream, joint.offset.z)) {
        return false;
    }

    if (!(stream >> tok) || tok != "}") return false;

    bvh_.joints.push_back(std::move(joint));
    return true;
}

bool BVHFileReader::readMotion(std::istream& stream)
{
    std::string tok;
    if (!(stream >> tok) || tok != "MOTION") return false;

    if (!(stream >> tok) || tok != "Frames:") return false;
    int numFrames = 0;
    if (!readNumber(stream, numFrames) || numFrames < 0) return false;

    if (!(stream >> tok) || tok != "Frame") return false;
    if (!(stream >> tok) || tok != "Time:") return false;
    double frameTime = 0.0;
    if (!readNumber(stream, frameTime)) return false;

    int totalChannels = 0;
    for (const auto& joint : bvh_.joints) {
        totalChannels += static_cast<int>(joint.channelNames.size());
    }

    std::vector<std::vector<double>> motion(
        numFrames, std::vector<double>(totalChannels));
    for (int frame = 0; frame < numFrames; ++frame) {
        for (int channel = 0; channel < totalChannels; ++channel) {
            if (!readNumber(stream, motion[frame][channel])) return false;
        }
    }

    bvh_.numFrames = numFrames;
    bvh_.frameTime = frameTime;
    bvh_.motion = std::move(motion);
    return true;
}
