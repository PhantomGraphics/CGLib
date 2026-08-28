#pragma once

#include <filesystem>
#include <ostream>
#include "GLTFFile.h"

namespace Phantom {
	namespace File {

class GLTFFileWriter
{
public:
	bool write(const std::filesystem::path& filename, const GLTFFile& gltf);

	bool write(std::ostream& stream, const GLTFFile& gltf);
};

	}
}
