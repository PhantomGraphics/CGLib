#pragma once

#include <filesystem>
#include "GLTFFile.h"

namespace Phantom {
	namespace File {

class GLTFFileReader
{
public:
	bool read(const std::filesystem::path& filename);

	GLTFFile getGLTF() const { return gltf; }

private:
 GLTFFile gltf;
};

	}
}
