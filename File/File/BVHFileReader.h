#pragma once
#include <filesystem>
#include <istream>
#include "BVHFile.h"

namespace Phantom {
    namespace File {

/// @brief Reads BVH (Biovision Hierarchy) motion-capture files (plain text).
/// Not to be confused with Phantom::Space::BVH (bounding-volume-hierarchy).
class BVHFileReader
{
public:
    /// @brief Reads a BVH file from disk.
    /// @param path Path to the .bvh file.
    /// @return True on success, false on failure (missing file, malformed grammar).
    bool read(const std::filesystem::path& path);

    /// @brief Reads BVH data from a stream.
    /// @param stream Input stream.
    /// @return True on success, false on failure.
    bool read(std::istream& stream);

    /// @brief Returns the BVH data read by the last successful read call.
    const BVHFile& getBVH() const { return bvh_; }

private:
    bool readHierarchy(std::istream& stream);
    bool readJoint(std::istream& stream, int parentIndex);
    bool readEndSite(std::istream& stream, int parentIndex);
    bool readMotion(std::istream& stream);

    BVHFile bvh_;
};

    } // namespace File
} // namespace Phantom
