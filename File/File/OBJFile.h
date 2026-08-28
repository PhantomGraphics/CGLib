#pragma once

#include "CGLib/Math/Vector2d.h"
#include "CGLib/Math/Vector3d.h"
#include "MTLFile.h"
#include <string>
#include <vector>

namespace Phantom {
	namespace File {

/// @brief Holds the index data of a single OBJ face.
struct OBJFace
{
	std::vector<unsigned int> positionIndices; ///< Vertex position indices.
	std::vector<int> normalIndices;            ///< Normal indices (-1 if absent).
	std::vector<int> texCoordIndices;          ///< Texture coordinate indices (-1 if absent).
};

/// @brief Holds the data of an OBJ group.
struct OBJGroup
{
	std::string name;               ///< Group name.
	std::string usemtl;             ///< Name of the material to use.
	std::vector< OBJFace > faces;   ///< Faces belonging to this group.
};

/// @brief Holds the entire contents of an OBJ file.
struct OBJFile
{
	std::string comment; ///< Comment string at the top of the file.

	std::vector< Math::Vector3df > positions;  ///< Vertex positions.
	std::vector< Math::Vector3df > normals;    ///< Vertex normals.
	std::vector< Math::Vector2df > texCoords;  ///< Texture coordinates.

	std::vector< OBJGroup > groups;            ///< Groups.
	std::vector< std::string > mtllibs;        ///< Referenced MTL file names.

	MTLFile mtl;                               ///< Materials loaded from the referenced MTL files (empty when read from a stream).
};

	}
}
