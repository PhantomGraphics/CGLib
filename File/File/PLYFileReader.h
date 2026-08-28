#pragma once

#include <string>
#include <filesystem>

#include "CGLib/Math/Vector3d.h"
#include "PLYFile.h"

namespace Phantom {
	namespace File {

/// @brief Reads PLY files in both ASCII and binary formats.
class PLYFileReader
{
public:
	/// @brief Reads a PLY file from disk.
	/// @param filename Path to the PLY file.
	/// @return True on success, false on failure.
	bool read(const std::filesystem::path& filename);

	/// @brief Reads PLY data from a stream.
	/// @param stream Input stream.
	/// @return True on success, false on failure.
	bool read(std::istream& stream);

	/// @brief Returns the PLY data read by the last read call.
	/// @return The parsed PLYFile object.
	PLYFile getPLY() const { return ply; }

private:
	/// @brief Reads vertex and face data in ASCII format.
	/// @param stream Input stream positioned at the start of data.
	/// @param count Number of vertices to read.
	/// @param faceCount Number of faces to read.
	/// @return True on success, false on failure.
	bool readAsciiData(std::istream& stream, const unsigned int count, const unsigned int faceCount);

	/// @brief Reads vertex and face data in binary format.
	/// @param stream Input stream positioned at the start of data.
	/// @param count Number of vertices to read.
	/// @param faceCount Number of faces to read.
	/// @return True on success, false on failure.
	bool readBinaryData(std::istream& stream, const unsigned int count, const unsigned int faceCount);

private:
	PLYFile ply; ///< Parsed PLY data.
};

	}
}
