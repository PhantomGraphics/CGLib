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
		std::vector<Math::Vector2df> texCoords;   // TEXCOORD_0
		std::vector<Math::Vector2df> texCoords1;  // TEXCOORD_1 (secondary UV set); empty if absent
		std::vector<Math::Vector4df> tangents;
		std::vector<std::array<int, 4>> joints;   // JOINTS_0 (node indices into GLTFSkin::joints, widened to int regardless of source component type)
		std::vector<Math::Vector4df> weights;     // WEIGHTS_0
		std::vector<Math::Vector4df> colors;      // COLOR_0 (RGBA, alpha defaults to 1 for a VEC3 source); empty if absent
		std::vector<unsigned int> indices;
		int materialIndex = -1;
		GLTFPrimitiveMode mode = GLTFPrimitiveMode::Triangles;
		// Morph targets: one GLTFMorphTarget per glTF target. Empty = no morph targets (most primitives).
		std::vector<GLTFMorphTarget> targets;
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

	// KHR_texture_transform: a UV-space offset/scale/rotation applied to one texture reference
	// (and, per spec, an optional override of which TEXCOORD_n set it samples -- already folded
	// into the owning texture reference's own texCoord field by the reader, not stored here).
	// Default-constructed = identity (no-op), matching a texture reference without the extension.
	struct GLTFTextureTransform
	{
		bool  present = false; // KHR_texture_transform found on this texture reference
		float offsetX = 0.0f, offsetY = 0.0f;
		float scaleX = 1.0f, scaleY = 1.0f;
		float rotation = 0.0f; // radians, counter-clockwise in UV space per the extension spec
	};

	struct GLTFPBRMetallicRoughness
	{
		std::array<float, 4> baseColorFactor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		int baseColorTextureIndex = -1;
		int baseColorTexCoord = 0;           // which TEXCOORD_n set the texture sampler reads
		GLTFTextureTransform baseColorTransform;
		int metallicRoughnessTextureIndex = -1;
		int metallicRoughnessTexCoord = 0;
		GLTFTextureTransform metallicRoughnessTransform;
	};

	struct GLTFMaterial
	{
		std::string name;
		GLTFPBRMetallicRoughness pbrMetallicRoughness;
		int normalTextureIndex = -1;
		int normalTexCoord = 0;
		float normalTextureScale = 1.0f;
		GLTFTextureTransform normalTransform;
		int occlusionTextureIndex = -1;
		int occlusionTexCoord = 0;
		float occlusionTextureStrength = 1.0f;
		GLTFTextureTransform occlusionTransform;
		int emissiveTextureIndex = -1;
		int emissiveTexCoord = 0;
		GLTFTextureTransform emissiveTransform;
		std::array<float, 3> emissiveFactor = { 0.0f, 0.0f, 0.0f };
		// KHR_materials_emissive_strength multiplier; 1.0 when the extension is absent (spec default).
		float emissiveStrength = 1.0f;
		GLTFAlphaMode alphaMode = GLTFAlphaMode::Opaque;
		float alphaCutoff = 0.5f;             // meaningful only when alphaMode == Mask
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

	// glTF sampler: values are the raw glTF/WebGL enum codes (9728/9729/... for filters,
	// 10497/33071/33648 for wrap modes), matching GltfSampler at the renderer-facing Gltf layer.
	// Defaults are the glTF spec's implied default (LINEAR/REPEAT) for when magFilter/minFilter
	// are omitted in the source (wrap modes always have an explicit spec default of REPEAT, but
	// cgltf itself already normalizes an absent wrap field to that before we ever see it).
	struct GLTFSampler
	{
		int magFilter = 9729;
		int minFilter = 9729;
		int wrapS = 10497;
		int wrapT = 10497;
	};

	struct GLTFTexture
	{
		std::string name;
		int imageIndex = -1;
		int samplerIndex = -1; // index into GLTFFile::samplers; -1 = no sampler node (spec default LINEAR/REPEAT)
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

	// type is "Perspective" | "Orthographic" | "Unknown". Only the fields for the matching type
	// are meaningful (the other type's fields stay at their defaults, unread).
	struct GLTFCamera {
		std::string name;
		std::string type;
		// Perspective
		float yfov = 0.8f;        // radians, vertical FOV
		float aspectRatio = 0.0f; // 0 = unspecified in the source (glTF: use the viewport's own aspect)
		// Orthographic
		float xmag = 1.0f, ymag = 1.0f; // half-extents of the view volume
		// Shared
		float znear = 0.1f;
		float zfar = 0.0f; // 0 = unspecified/infinite (perspective only, per spec)
	};

	// type is "Directional" | "Point" | "Spot" | "Unknown" (KHR_lights_punctual).
	struct GLTFLight {
		std::string name;
		std::string type;
		std::array<float, 3> color = { 1.0f, 1.0f, 1.0f };
		// Directional: lux. Point/Spot: candela. Per KHR_lights_punctual -- a real-world
		// photometric unit, not the small artist-friendly number Phantom's own fixed default
		// light uses; consumers should rescale (see Universe's LightEntry construction).
		float intensity = 1.0f;
		float range = 0.0f; // Point/Spot only; 0 = infinite (no cutoff)
		float innerConeAngle = 0.0f;          // Spot only, radians
		float outerConeAngle = 0.7853981634f; // Spot only, radians (pi/4, the spec's own default)
	};

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
		std::vector<GLTFSampler> samplers;
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
