#pragma once
#include <filesystem>
#include <istream>
#include "VMDFile.h"

namespace Phantom {
    namespace File {

class VMDFileReader {
public:
    bool read(const std::filesystem::path& path);
    bool read(std::istream& stream);
    const VMDFile& getVMD() const { return vmd_; }

private:
    bool readFrom(std::istream& s);
    VMDFile vmd_;
};

    } // namespace File
} // namespace Phantom
