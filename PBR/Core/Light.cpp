#include "Light.h"

namespace Phantom::PBR {

	Light defaultSun() {
		Light sun;
		sun.type = LightType::Directional;
		sun.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
		sun.castShadow = false;   // the unlit-scene sun never shadowed
		return sun;
	}

}
