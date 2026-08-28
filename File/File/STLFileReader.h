#pragma once

#include <filesystem>
#include "STLFile.h"

namespace Phantom {
	namespace File {

/// @brief Reads STL files in both ASCII and binary formats.
class STLFileReader
{
public:
	/// @brief Determines whether a file is in binary STL format.
	/// @param filePath Path to the STL file.
	/// @return True if binary, false if ASCII.
	static bool isBinary(const std::filesystem::path& filePath);

	/// @brief Determines whether a stream contains binary STL data.
	/// @param in Input stream.
	/// @return True if binary, false if ASCII.
	static bool isBinary(std::istream& in);

	/// @brief Reads an ASCII STL file from disk.
	/// @param filePath Path to the STL file.
	/// @return True on success, false on failure.
	bool readAscii(const std::filesystem::path& filePath);

	/// @brief Reads ASCII STL data from a stream.
	/// @param stream Input stream.
	/// @return True on success, false on failure.
	bool readAscii(std::istream& stream);

	/// @brief Reads a binary STL file from disk.
	/// @param filePath Path to the STL file.
	/// @return True on success, false on failure.
	bool readBinary(const std::filesystem::path& filePath);

	/// @brief Reads binary STL data from a stream.
	/// @param stream Input stream.
	/// @return True on success, false on failure.
	bool readBinary(std::istream& stream);

	/// @brief Returns the STL data read by the last read call.
	/// @return The parsed STLFile object.
	STLFile getSTL() const { return stl; }

private:
	STLFile stl; ///< Parsed STL data.
};

	}
}
