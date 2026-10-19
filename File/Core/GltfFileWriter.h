#pragma once

#include <filesystem>
#include <ostream>
#include "GltfFile.h"

namespace Phantom {
	namespace File {

class GltfFileWriter
{
public:
	bool write(const std::filesystem::path& filename, const GltfFile& gltf);

	bool write(std::ostream& stream, const GltfFile& gltf);
private:
	bool writeDocument(std::ostream& stream, const GltfFile& gltf, bool binary);
};

	}
}
