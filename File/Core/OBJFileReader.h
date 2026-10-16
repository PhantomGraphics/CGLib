#pragma once

#include "OBJFile.h"
#include <filesystem>
#include <optional>

namespace Phantom {
	namespace File {

/// @brief Reads OBJ files.
class OBJFileReader
{
public:

	/// @brief Reads an OBJ file from disk.
	/// @param filePath Path to the OBJ file.
	/// @return True on success, false on failure.
	bool read(const std::filesystem::path& filePath);

	/// @brief Reads OBJ data from a stream.
	/// @param stream Input stream.
	/// @return True on success, false on failure.
	bool read(std::istream& stream);

	/// @brief Returns the OBJ data read by the last read call.
	/// @return The parsed OBJFile object.
	OBJFile getOBJ() const { return obj; }

private:

	/// @brief Parses a vertex position from a token list.
	/// @param strs Token list.
	/// @return The parsed 3D position vector, or std::nullopt if the tokens are malformed.
	std::optional<Math::Vector3df> readVertices(const std::vector<std::string>& strs);

	/// @brief Parses a 3D vector from a token list.
	/// @param strs Token list.
	/// @return The parsed 3D vector, or std::nullopt if the tokens are malformed.
	std::optional<Math::Vector3df> readVector3d(const std::vector<std::string>& strs);

	/// @brief Parses a 2D vector from a token list.
	/// @param strs Token list.
	/// @return The parsed 2D vector, or std::nullopt if the tokens are malformed.
	std::optional<Math::Vector2df> readVector2d(const std::vector<std::string>& strs);

	OBJFile obj; ///< Parsed OBJ data.
};

	}
}
