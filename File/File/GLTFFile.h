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

	// One morph target's per-vertex displacements from the primitive's base geometry
	// (glTF "targets" -- see GLTFPrimitive::targets). `normalDeltas` is empty when the source
	// primitive carried no NORMAL morph attribute; `positionDeltas` is always the same length
	// as GLTFPrimitive::positions.
	struct GLTFMorphTarget
	{
		std::vector<Math::Vector3df> positionDeltas;
		std::vector<Math::Vector3df> normalDeltas;
	};

	struct GLTFPrimitive
	{
		std::vector<Math::Vector3df> positions;
		std::vector<Math::Vector3df> normals;
		std::vector<Math::Vector2df> texCoords;   // TEXCOORD_0 only -- see hasSecondUV
		std::vector<Math::Vector4df> tangents;
		std::vector<std::array<int, 4>> joints;   // JOINTS_0 (node indices into GLTFSkin::joints, widened to int regardless of source component type)
		std::vector<Math::Vector4df> weights;     // WEIGHTS_0
		std::vector<unsigned int> indices;
		int materialIndex = -1;
		GLTFPrimitiveMode mode = GLTFPrimitiveMode::Triangles;
		// Morph targets: one GLTFMorphTarget per glTF target. Empty = no morph targets (most primitives).
		std::vector<GLTFMorphTarget> targets;
		// The source primitive also carried a TEXCOORD_1 (and/or higher) attribute, but only
		// TEXCOORD_0 is read into `texCoords` above -- no renderer path samples a second UV set yet.
		// Surfaced by ImportReport so KHR_texture_transform's texCoord override / a material's
		// non-zero texCoord slot does not silently render with the wrong (or no) UVs.
		bool hasSecondUV = false;
		// The source primitive carried a COLOR_0 attribute, but it is not read/applied anywhere
		// in this layer or the renderer -- surfaced by ImportReport instead of silently ignored.
		bool hasVertexColor = false;
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

	enum class GLTFAlphaMode { Opaque, Mask, Blend };

	struct GLTFPBRMetallicRoughness
	{
		std::array<float, 4> baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		int baseColorTextureIndex = -1;
		int baseColorTexCoord = 0;           // which TEXCOORD_n set the texture sampler reads
		int metallicRoughnessTextureIndex = -1;
		int metallicRoughnessTexCoord = 0;
	};

	struct GLTFMaterial
	{
		std::string name;
		GLTFPBRMetallicRoughness pbrMetallicRoughness;
		int normalTextureIndex = -1;
		int normalTexCoord = 0;
		float normalTextureScale = 1.0f;
		int occlusionTextureIndex = -1;
		int occlusionTexCoord = 0;
		float occlusionTextureStrength = 1.0f;
		int emissiveTextureIndex = -1;
		int emissiveTexCoord = 0;
		std::array<float, 3> emissiveFactor = { 0.0f, 0.0f, 0.0f };
		// KHR_materials_emissive_strength multiplier; 1.0 when the extension is absent (spec default).
		float emissiveStrength = 1.0f;
		GLTFAlphaMode alphaMode = GLTFAlphaMode::Opaque;
		float alphaCutoff = 0.5f;             // meaningful only when alphaMode == Mask
		bool doubleSided = false;
		// KHR_texture_transform (UV offset/scale/rotation) was present on at least one of this
		// material's texture slots, but nothing downstream applies it -- surfaced by ImportReport
		// instead of silently rendering with the untransformed UVs.
		bool hasUnsupportedTextureTransform = false;
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
		int cameraIndex = -1;
		int lightIndex = -1;
		int skin = -1;
		std::vector<int> children;
		std::vector<float> morphWeights; // overrides GLTFMesh::morphWeights when non-empty
	};

	struct GLTFCamera { std::string name; std::string type; };
	struct GLTFLight { std::string name; std::string type; };

	enum class GLTFInterpolation { Linear, Step, CubicSpline };
	enum class GLTFAnimationPath { Translation, Rotation, Scale, Weights };

	struct GLTFAnimationSampler
	{
		std::vector<float> times;   // keyframe times, seconds (input accessor, SCALAR/FLOAT)
		std::vector<float> values;  // flattened output accessor values; element stride is
		                            // `components`, tripled for CubicSpline (inTangent, value,
		                            // outTangent per keyframe -- see the glTF spec)
		int components = 0;         // 3 (translation/scale), 4 (rotation xyzw), or the morph
		                            // target count (weights)
		GLTFInterpolation interpolation = GLTFInterpolation::Linear;
	};

	struct GLTFAnimationChannel
	{
		int targetNode = -1;
		GLTFAnimationPath path = GLTFAnimationPath::Translation;
		int sampler = -1;
	};

	struct GLTFAnimation
	{
		std::string name;
		std::vector<GLTFAnimationSampler> samplers;
		std::vector<GLTFAnimationChannel> channels;
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
		std::vector<GLTFCamera> cameras;
		std::vector<GLTFLight> lights;
		std::vector<GLTFAnimation> animations;
		int defaultScene = 0;
		// Unrecognized top-level (root) glTF extensions, verbatim (name -> raw JSON object text).
		// This is where profile extensions like VRM ("VRM" for 0.x, "VRMC_vrm" for 1.0) show up.
		// Empty for ordinary glTF files.
		std::vector<std::pair<std::string, std::string>> rootExtensionsJson;
	};

	}
}
