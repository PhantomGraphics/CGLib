#pragma once

#include "OBJFile.h"
#include <filesystem>

namespace Phantom {
	namespace File {

/// @brief Writes OBJ files.
class OBJFileWriter
{
public:
	/// @brief Writes OBJ data to a file.
	/// @param filePath Output file path.
	/// @param obj The OBJ data to write.
	/// @return True on success, false on failure.
	bool write(const std::filesystem::path& filePath, const OBJFile& obj);

	/// @brief Writes OBJ data to a stream.
	/// @param stream Output stream.
	/// @param obj The OBJ data to write.
	/// @return True on success, false on failure.
	bool write(std::ostream& stream, const OBJFile& obj);

private:
};

	}
}
