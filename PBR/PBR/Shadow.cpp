#include "Shadow.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Phantom::PBR {

	bool canCastShadow(const Light& l) {
		return l.castShadow && (l.type == LightType::Directional || l.type == LightType::Spot);
	}

	int pickShadowCaster(const std::vector<Light>& lights) {
		for (size_t i = 0; i < lights.size(); ++i)
			if (canCastShadow(lights[i])) return static_cast<int>(i);
		return -1;
	}

	bool computeShadowSetup(const Light& l, const glm::vec3& lo, const glm::vec3& hi, float mapSize, ShadowSetup& out) {
		if (!canCastShadow(l)) return false;
		const glm::vec3 dir = glm::normalize(l.direction);
		const glm::vec3 up = std::abs(dir.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
		const glm::vec3 centre = 0.5f * (lo + hi);
		const float radius = std::max(0.5f * glm::length(hi - lo), 0.25f);
		ShadowSetup s;
		if (l.type == LightType::Directional) {
			const glm::mat4 view = glm::lookAt(centre - dir * radius * 2.f, centre, up);
			s.viewProj = glm::ortho(-radius, radius, -radius, radius, 0.f, radius * 4.f) * view;
			s.texelBase = 2.f * radius / mapSize;
		} else {
			const glm::vec3 eye = l.position;
			float farZ = 1.f;
			float minDepth = std::numeric_limits<float>::max();
			for (int c = 0; c < 8; ++c) {
				const glm::vec3 delta = glm::vec3((c & 1) ? hi.x : lo.x,
					(c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z) - eye;
				farZ = std::max(farZ, glm::length(delta));
				minDepth = std::min(minDepth, glm::dot(delta, dir));
			}
			farZ *= 1.05f;
			// Keep the full cone even above 85 degrees. Reduce only the extra
			// margin as the half-angle approaches 90, keeping perspective below pi.
			const float fov = std::min(2.f * l.spotAngleRad + 0.1f,
				l.spotAngleRad + glm::radians(90.f));
			const glm::mat4 view = glm::lookAt(eye, eye + dir, up);
			// A distant caster must not push the near plane past a nearby one.
			// If the box crosses the light plane, use the small positive floor.
			const float nearZ = std::max(1.0e-4f, std::min(0.05f, minDepth * 0.5f));
			s.viewProj = glm::perspective(fov, 1.f, nearZ, farZ) * view;
			s.texelPerDistance = 2.f * std::tan(fov * 0.5f) / mapSize;
		}
		out = s;
		return true;
	}

	void setBlockShadow(LightBlock& block, int slot, const ShadowSetup& setup) {
		block.shadowVP = setup.viewProj;
		block.shadowInfo = glm::vec4(static_cast<float>(slot), setup.texelBase, setup.depthBias, setup.texelPerDistance);
	}

}
