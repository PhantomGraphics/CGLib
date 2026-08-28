#pragma once

#pragma once

#include <vector>
#include <string>
#include <array>
#include "CGLib/Math/Vector2d.h"
#include "CGLib/Math/Vector3d.h"
#include "CGLib/Math/Vector4d.h"

namespace Phantom {
	namespace File {

	enum class GLTFPrimitiveMode
	{
		Points = 0,
		Lines = 1,
		LineLoop = 2,
		LineStrip = 3,
		Triangles = 4,
		TriangleStrip = 5,
		TriangleFan = 6,
	};

	struct GLTFPrimitive
	{
		std::vector<Math::Vector3df> positions;
		std::vector<Math::Vector3df> normals;
		std::vector<Math::Vector2df> texCoords;
		std::vector<Math::Vector4df> tangents;
		std::vector<std::array<int, 4>> joints;   // JOINTS_0 (node indices into GLTFSkin::joints, widened to int regardless of source component type)
		std::vector<Math::Vector4df> weights;     // WEIGHTS_0
		std::vector<unsigned int> indices;
		int materialIndex = -1;
		GLTFPrimitiveMode mode = GLTFPrimitiveMode::Triangles;
		// Morph targets: each element is one target's per-vertex POSITION displacement, same
		// length as `positions`. Empty = no morph targets (most primitives).
		std::vector<std::vector<Math::Vector3df>> targets;
	};

	struct GLTFSkin
	{
		std::string name;
		std::vector<int> joints;                        // node indices; position in this vector = JOINTS_0 value
		std::vector<std::array<float, 16>> inverseBindMatrices; // column-major 4x4 per joint; identity-filled if the source omitted them
		int skeletonRoot = -1;
	};

	struct GLTFMesh
	{
		std::string name;
		std::vector<GLTFPrimitive> primitives;
		std::vector<float> morphWeights; // default morph target weights, one per target index (usually all 0)
	};

	struct GLTFPBRMetallicRoughness
	{
		std::array<float, 4> baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		int baseColorTextureIndex = -1;
		int metallicRoughnessTextureIndex = -1;
	};

	struct GLTFMaterial
	{
		std::string name;
		GLTFPBRMetallicRoughness pbrMetallicRoughness;
		int normalTextureIndex = -1;
		int emissiveTextureIndex = -1;
		std::array<float, 3> emissiveFactor = { 0.0f, 0.0f, 0.0f };
		bool doubleSided = false;
		// Unrecognized glTF extensions on this material, verbatim (name -> raw JSON object text,
		// null-terminated substring as extracted by cgltf). Empty for ordinary glTF files. This
		// layer has no concept of what any given extension means (e.g. VRMC_materials_mtoon) --
		// it just carries the text through for a higher layer to interpret.
		std::vector<std::pair<std::string, std::string>> extensionsJson;
	};

	struct GLTFImage
	{
		std::string name;
		std::string uri;
		std::string mimeType;
		std::vector<uint8_t> data; // raw encoded bytes for GLB-embedded images
	};

	struct GLTFTexture
	{
		std::string name;
		int imageIndex = -1;
	};

	struct GLTFNode
	{
		std::string name;
		std::array<float, 3> translation = { 0.0f, 0.0f, 0.0f };
		std::array<float, 4> rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
		std::array<float, 3> scale = { 1.0f, 1.0f, 1.0f };
		// A node may specify either a matrix or TRS (never both, per glTF spec). hasMatrix=true
		// means `matrix` (column-major, glTF's own storage order) is authoritative and
		// translation/rotation/scale above are left at their defaults, unused.
		bool hasMatrix = false;
		std::array<float, 16> matrix = { 1.f,0.f,0.f,0.f, 0.f,1.f,0.f,0.f, 0.f,0.f,1.f,0.f, 0.f,0.f,0.f,1.f };
		int meshIndex = -1;
		int skin = -1;
		std::vector<int> children;
		std::vector<float> morphWeights; // overrides GLTFMesh::morphWeights when non-empty
	};

	struct GLTFScene
	{
		std::string name;
		std::vector<int> nodes;
	};

	struct GLTFFile
	{
		std::vector<GLTFMesh> meshes;
		std::vector<GLTFMaterial> materials;
		std::vector<GLTFTexture> textures;
		std::vector<GLTFImage> images;
		std::vector<GLTFNode> nodes;
		std::vector<GLTFScene> scenes;
		std::vector<GLTFSkin> skins;
		int defaultScene = 0;
		// Unrecognized top-level (root) glTF extensions, verbatim (name -> raw JSON object text).
		// This is where profile extensions like VRM ("VRM" for 0.x, "VRMC_vrm" for 1.0) show up.
		// Empty for ordinary glTF files.
		std::vector<std::pair<std::string, std::string>> rootExtensionsJson;
	};

	}
}
