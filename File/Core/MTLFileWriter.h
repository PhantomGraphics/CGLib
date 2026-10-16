#pragma once

#include "MTLFile.h"
#include <filesystem>

namespace Phantom {
	namespace File {

/// @brief Writes MTL material files.
class MTLFileWriter
{
public:
	/// @brief Writes MTL data to a file.
	/// @param filePath Output file path.
	/// @param mtl The MTL data to write.
	/// @return True on success, false on failure.
	bool write(const std::filesystem::path& filePath, const MTLFile& mtl);

	/// @brief Writes MTL data to a stream.
	/// @param stream Output stream.
	/// @param mtl The MTL data to write.
	/// @return True on success, false on failure.
	bool write(std::ostream& stream, const MTLFile& mtl);

private:
};

	}
}
