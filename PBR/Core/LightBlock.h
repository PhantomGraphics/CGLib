#pragma once
// The lighting part of a viewport's camera uniform buffer (std140). One definition for the C++ side; the GLSL declarations in
// the viewport shaders (PhantomStudio cgstudio.frag, CGLib ShaderGraph SurfaceFragment.cpp) must list the same members in the
// same order, right after the camera block -- keep the two in step with this struct (LightBlockTest pins the layout).
#include "Light.h"
#include <cstddef>
#include <vector>

namespace Phantom::PBR {

	constexpr int kMaxViewportLights = 4;

	struct LightBlock {
		glm::vec4 ambient{ 0.15f, 0.15f, 0.15f, 0.f };  // rgb = colour * intensity
		glm::vec4 lightInfo{ 0.f };                      // x = number of lights used (0..kMaxViewportLights)
		glm::vec4 lightVec[kMaxViewportLights]{};        // xyz = direction TO the light (w = 0, directional) or its position (w = 1)
		glm::vec4 lightColor[kMaxViewportLights]{};      // rgb = colour * intensity; w = cos(inner cone half-angle) of a spot light
		glm::vec4 lightDir[kMaxViewportLights]{};        // spot: xyz = direction the light travels, w = cos(outer half-angle); w <= -1.5 = not a spot
		// Shadow map of the one shadow-casting light: world -> light clip space (filled by the caller, see shadowInfo).
		glm::mat4 shadowVP{ 1.f };
		glm::vec4 shadowInfo{ -1.f, 0.f, 0.f, 0.f };     // x = light slot that casts (-1 none), y = texel size (world units) + w * distance, z = depth bias
		// Area lights: xyz = half-width / half-height axis in world space; areaU.w = 1 marks the slot as an area light.
		glm::vec4 areaU[kMaxViewportLights]{};
		glm::vec4 areaV[kMaxViewportLights]{};
	};

	// Writes `lights` (at most kMaxViewportLights; the rest are ignored) and `ambient` (already colour * intensity) into
	// `block`: lightInfo.x, lightVec / lightColor / lightDir and the area axes. Slot i is lights[i]. Leaves the shadow members
	// untouched. Returns the number of lights written.
	int packLights(LightBlock& block, const std::vector<Light>& lights, const glm::vec3& ambient);

}
