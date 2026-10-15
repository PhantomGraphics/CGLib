#pragma once
// Conversions from Phantom::PBR::Light to the glTF side (header-only; no Vulkan, no File module).
//
//   toLightEntry     -> Phantom::Gltf::LightEntry, what GltfSceneRenderer::setPunctualLights() shades with
//   toGltfPunctual   -> the fields of a KHR_lights_punctual entry (the exporter copies them into its GLTFLight)
//
// Intensity scales: the viewport shades a light as albedo * radiance (no 1/pi), the glTF renderer's BRDF divides the diffuse
// term by pi, so a LightEntry carries intensity * pi to look equally bright; glTF's photometric units are 683 lm/W
// (Blender divides imported intensities by 683).
#include "Light.h"

#include "../../GltfRenderer/Renderer/LightEntry.h"

namespace Phantom::PBR {

	constexpr float kPi = 3.14159265358979f;
	constexpr float kGltfLuminousEfficacy = 683.f;
	// An area light has no counterpart in either type; it is approximated by a spot light at its centre with this cone.
	constexpr float kAreaAsSpotAngleDeg = 89.f;

	inline Phantom::Gltf::LightEntry toLightEntry(const Light& l, bool castShadow = false) {
		Phantom::Gltf::LightEntry e;
		e.color = l.color;
		e.intensity = l.intensity * kPi;
		e.range = 0.f;   // inverse-square all the way, like the viewport
		e.position = l.position;
		e.direction = l.direction;
		e.castShadow = castShadow;
		switch (l.type) {
		case LightType::Directional:
			e.type = Phantom::Gltf::LightEntry::Type::Directional;
			break;
		case LightType::Point:
			e.type = Phantom::Gltf::LightEntry::Type::Point;
			break;
		case LightType::Spot:
			e.type = Phantom::Gltf::LightEntry::Type::Spot;
			e.spotAngleDeg = glm::degrees(l.spotAngleRad);
			e.spotInnerAngleDeg = e.spotAngleDeg * (1.f - l.spotBlend);
			break;
		case LightType::Area:
			e.type = Phantom::Gltf::LightEntry::Type::Spot;
			e.spotAngleDeg = kAreaAsSpotAngleDeg;
			e.spotInnerAngleDeg = 0.f;
			break;
		}
		return e;
	}

	// A KHR_lights_punctual light. `type` is the spec's name ("directional" / "point" / "spot" with the first letter upper-cased
	// as the exporter writes them). glTF has no area light: it becomes a point light and `approximated` is set.
	struct GltfPunctual {
		const char* type = "Directional";
		glm::vec3 color{ 1.f };
		float intensity = 1.f;            // photometric: lux (directional) / candela
		float innerConeAngle = 0.f;       // Spot, radians
		float outerConeAngle = 0.785398163f;
		bool approximated = false;
	};

	inline GltfPunctual toGltfPunctual(const Light& l) {
		GltfPunctual g;
		g.type = l.type == LightType::Directional ? "Directional" : l.type == LightType::Spot ? "Spot" : "Point";
		g.color = l.color;
		g.intensity = l.intensity * kGltfLuminousEfficacy;
		if (l.type == LightType::Spot) {
			g.outerConeAngle = l.spotAngleRad;
			g.innerConeAngle = l.spotAngleRad * (1.f - l.spotBlend);
		}
		g.approximated = l.type == LightType::Area;
		return g;
	}

}
