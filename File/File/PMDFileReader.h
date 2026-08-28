#pragma once
#include <filesystem>
#include <istream>
#include "PMDFile.h"

namespace Phantom {
    namespace File {

class PMDFileReader {
public:
    bool read(const std::filesystem::path& path);
    bool read(std::istream& stream);
    const PMDFile& getPMD() const { return pmd_; }

private:
    bool readFrom(std::istream& s);
    PMDFile pmd_;
};

    } // namespace File
} // namespace Phantom
