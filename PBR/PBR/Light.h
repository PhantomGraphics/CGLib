#pragma once
// Phantom::PBR -- light description shared by the renderers (CPU only: no Vulkan, no scene types).
//
// A Light is the world-space description of one light as the scene sees it: where it is, which way it shines, how bright.
// Each renderer turns it into what it needs: LightBlock (the viewport's uniform buffer, LightBlock.h), a glTF renderer's
// LightEntry, a path tracer's RtLight, KHR_lights_punctual. Conventions: directions are unit vectors, `direction` is the way
// the light TRAVELS (the object's local -Z), `intensity` is radiometric (a point light's radiance = intensity / distance^2).
#ifndef GLM_FORCE_RADIANS
#define GLM_FORCE_RADIANS
#endif
#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include <glm/glm.hpp>

namespace Phantom::PBR {

	enum class LightType { Directional, Point, Spot, Area };

	struct Light {
		LightType type = LightType::Directional;
		glm::vec3 position{ 0.f };                 // Point / Spot / Area (ignored by Directional)
		glm::vec3 direction{ 0.f, -1.f, 0.f };     // the way the light travels; Directional / Spot / Area
		glm::vec3 up{ 0.f, 1.f, 0.f };             // Area: the rectangle's height axis (the width axis is direction x up)
		glm::vec3 color{ 1.f };                    // linear RGB
		float intensity = 1.f;
		float spotAngleRad = 0.785398163f;         // Spot: cone half-angle
		float spotBlend = 0.2f;                    // Spot: fraction of the cone over which the light fades (inner = angle * (1 - blend))
		float areaWidth = 1.f;                     // Area: rectangle size
		float areaHeight = 1.f;
		bool castShadow = true;
	};

	// The sun the viewport used before light objects existed: white, shining along (-1,-2,-1), intensity 1.
	Light defaultSun();

}
