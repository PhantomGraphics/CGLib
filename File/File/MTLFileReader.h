#pragma once

#include "MTLFile.h"

#include <cassert>

#include <fstream>
#include <string>
#include <filesystem>

namespace Phantom {
	namespace File {

/// @brief Reads MTL material files.
class MTLFileReader
{
public:
	/// @brief Reads an MTL file from disk.
	/// @param filePath Path to the MTL file.
	/// @return True on success, false on failure.
	bool read(const std::filesystem::path& filePath);

	/// @brief Reads MTL data from a stream.
	/// @param stream Input stream.
	/// @return True on success, false on failure.
	bool read(std::istream& stream);

	/// @brief Returns the MTL data read by the last read call.
	/// @return The parsed MTLFile object.
	MTLFile getMTL() const { return mtl; }

private:
	MTLFile mtl; ///< Parsed MTL data.
};

	}
}
