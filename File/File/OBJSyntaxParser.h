#pragma once

#include "OBJFile.h"
#include <optional>

namespace Phantom {
	namespace File {

/// @brief Parses OBJ file syntax.
class OBJSyntaxParser
{
public:
	/// @brief Parses an OBJ "f" line and returns an OBJFace.
	/// @param line The line string to parse (including the "f " prefix).
	/// @return The parsed OBJFace object, or std::nullopt if the line is malformed.
	static std::optional<OBJFace> parseFaceLine(const std::string& line);

private:
	/// @brief Builds an OBJFace from a token list.
	/// @param strs Token list.
	/// @return The constructed OBJFace object, or std::nullopt if a token is malformed.
	static std::optional<OBJFace> parseFace(std::vector< std::string >& strs);
};

	}
}
