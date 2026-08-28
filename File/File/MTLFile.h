#pragma once

#include "CGLib/Graphics/Image.h"

#include <algorithm>
#include <string>
#include <vector>

namespace Phantom {
	namespace File {

/// @brief Holds the data for a single MTL material.
struct MTL
{
	/// @brief Illumination model types.
	enum class Illumination
	{
		COLOR_ON_AND_AMBIENT_OFF = 0,              ///< Color on, ambient off.
		COLOR_ON_AND_AMBIENT_ON = 1,               ///< Color on, ambient on.
		HIGHLIGHT_ON = 2,                          ///< Highlight on.
		REFRECTION_ON_AND_RAY_TRACE_ON = 3,        ///< Reflection on, ray trace on.

		REFLECTION_ON_AND_RAY_TRACE_OFF = 8,       ///< Reflection on, ray trace off.

		CAST_SHADOWS_ONTO_INVISIBLE_SURFACES = 10, ///< Cast shadows onto invisible surfaces.
	};

	/// @brief Constructor. Initializes all fields to default values.
	MTL() {
		ambient = Graphics::ColorRGBAf(0.0f, 0.0f, 0.0f, 0.0f);
		diffuse = Graphics::ColorRGBAf(0.0f, 0.0f, 0.0f, 0.0f);
		specular = Graphics::ColorRGBAf(0.0f, 0.0f, 0.0f, 0.0f);
		specularExponent = 1.0f;
		transparent = 0.0f;
		opticalDensity = 0.0f;

		illumination = Illumination::COLOR_ON_AND_AMBIENT_OFF;
	}

	/*
	void setOpticalDensity(const float d) {
		assert((0.001f <= d) && (d <= 10.0f));
		this->opticalDensity = d;
	}
	float getOpticalDensity() const { return opticalDensity; }
	*/

	//Graphics::Material toMaterial(const std::string& directory) const;

public:
	std::string name;                  ///< Material name.
	Graphics::ColorRGBAf ambient;      ///< Ambient color (Ka).
	Graphics::ColorRGBAf diffuse;      ///< Diffuse color (Kd).
	Graphics::ColorRGBAf specular;     ///< Specular color (Ks).

	float specularExponent;            ///< Specular exponent (Ns).
	float transparent;                 ///< Transparency (d / Tr).
	float opticalDensity;              ///< Optical density / index of refraction (Ni).

	std::string ambientTexture;        ///< Ambient texture file path (map_Ka).
	std::string diffuseTexture;        ///< Diffuse texture file path (map_Kd).
	std::string shininessTexture;      ///< Shininess texture file path (map_Ns).
	std::string bumpTexture;           ///< Bump map file path (map_bump).

	Illumination illumination;         ///< Illumination model (illum).
};

/// @brief Holds the entire contents of an MTL file.
struct MTLFile
{
	std::vector<MTL> materials; ///< List of materials.

	/// @brief Finds and returns the material with the given name.
	/// @param name The material name to search for.
	/// @return The matching MTL object. Behavior is undefined if not found.
	MTL find(const std::string& name) {
		auto iter = std::find_if(materials.begin(), materials.end(), [=](const auto& m) { return m.name == name; });
		return *iter;
	}
};

	}
}
