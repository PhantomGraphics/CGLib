#include "Shadow.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

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
			for (int c = 0; c < 8; ++c)
				farZ = std::max(farZ, glm::length(glm::vec3((c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z) - eye));
			farZ *= 1.05f;
			const float fov = std::min(2.f * l.spotAngleRad + 0.1f, glm::radians(170.f));
			const glm::mat4 view = glm::lookAt(eye, eye + dir, up);
			s.viewProj = glm::perspective(fov, 1.f, std::max(0.05f, farZ * 0.002f), farZ) * view;
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
