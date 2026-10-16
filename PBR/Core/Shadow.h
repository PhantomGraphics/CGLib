#pragma once
// Shadow-map setup for a light (CPU only): which light casts, and the matrix / bias the viewport's shadow pass and shaders use.
//
// One light casts at a time (the first visible one with castShadow that can: directional or spot). A directional light gets an
// orthographic box fitted around the casters' bounding sphere, a spot light a perspective frustum along its cone. Point and area
// lights never cast in the viewport (no cube map / area shadow yet).
#include "Light.h"
#include "LightBlock.h"

#include <vector>

namespace Phantom::PBR {

	struct ShadowSetup {
		glm::mat4 viewProj{ 1.f };      // world -> light clip space (Vulkan depth range 0..1)
		float texelBase = 0.f;          // world-space size of one shadow-map texel: texelBase + texelPerDistance * distance to the light
		float texelPerDistance = 0.f;   //   (directional: constant; spot: grows with distance)
		float depthBias = 0.0005f;
	};

	// True when `l` can cast a shadow in the viewport (castShadow set, directional or spot).
	bool canCastShadow(const Light& l);

	// Index of the first light that casts, or -1.
	int pickShadowCaster(const std::vector<Light>& lights);

	// Fits the shadow map of `l` around the casters' world-space box [boundsMin, boundsMax]. `mapSize` is the map's resolution in
	// texels. Returns false (out untouched) when `l` cannot cast.
	// Spot half-angle must be in (0, pi/2). The near plane includes the nearest box depth,
	// with a 1e-4 world-unit floor for bounds touching/crossing the light plane.
	bool computeShadowSetup(const Light& l, const glm::vec3& boundsMin, const glm::vec3& boundsMax, float mapSize, ShadowSetup& out);

	// Writes the setup into the block's shadowVP / shadowInfo for light slot `slot` (shadowInfo.x = slot).
	void setBlockShadow(LightBlock& block, int slot, const ShadowSetup& setup);

}
