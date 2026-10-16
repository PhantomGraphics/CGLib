#include "LightBlock.h"

#include <algorithm>
#include <cmath>

namespace Phantom::PBR {

	int packLights(LightBlock& block, const std::vector<Light>& lights, const glm::vec3& ambient) {
		block.ambient = glm::vec4(ambient, 0.f);
		const int count = std::min(static_cast<int>(lights.size()), kMaxViewportLights);
		for (int i = 0; i < kMaxViewportLights; ++i) {
			block.lightVec[i] = glm::vec4(0.f);
			block.lightColor[i] = glm::vec4(0.f);
			block.lightDir[i] = glm::vec4(0.f, -1.f, 0.f, -2.f);
			block.areaU[i] = glm::vec4(0.f);
			block.areaV[i] = glm::vec4(0.f);
		}
		for (int i = 0; i < count; ++i) {
			const Light& l = lights[i];
			if (l.type == LightType::Directional) block.lightVec[i] = glm::vec4(-l.direction, 0.f);
			else block.lightVec[i] = glm::vec4(l.position, 1.f);
			block.lightColor[i] = glm::vec4(l.color * l.intensity, 0.f);
			if (l.type == LightType::Spot) {
				block.lightDir[i] = glm::vec4(l.direction, std::cos(l.spotAngleRad));
				block.lightColor[i].w = std::cos(l.spotAngleRad * (1.f - l.spotBlend));
			} else if (l.type == LightType::Area) {
				const glm::vec3 dir = glm::normalize(l.direction);
				glm::vec3 right = glm::cross(dir, l.up);
				// The default up is parallel to the default direction. Pick a stable
				// alternative also for nearly parallel or zero height axes.
				if (glm::dot(right, right) < 1.0e-8f) {
					const glm::vec3 fallback = std::abs(dir.y) > 0.99f
						? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
					right = glm::cross(dir, fallback);
				}
				right = glm::normalize(right);
				const glm::vec3 up = glm::normalize(glm::cross(right, dir));
				block.areaU[i] = glm::vec4(right * (0.5f * l.areaWidth), 1.f);
				block.areaV[i] = glm::vec4(up * (0.5f * l.areaHeight), 0.f);
			}
		}
		block.lightInfo = glm::vec4(static_cast<float>(count), 0.f, 0.f, 0.f);
		return count;
	}

}
