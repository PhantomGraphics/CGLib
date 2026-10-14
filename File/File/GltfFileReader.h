#pragma once

#include <filesystem>
#include "GltfFile.h"

namespace Phantom {
	namespace File {

class GltfFileReader
{
public:
	bool read(const std::filesystem::path& filename);

	GltfFile getGLTF() const { return gltf; }

private:
 GltfFile gltf;
};

	}
}
