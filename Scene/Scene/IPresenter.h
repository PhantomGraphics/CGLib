#pragma once

#include "CGLib/Graphics/Camera.h"

namespace Phantom {
	namespace Scene {

/// @brief Interface for scene presenters (Model-View bridge).
///
/// A presenter owns the GPU buffer objects for a scene object and is
/// responsible for three distinct phases: building the CPU-side data,
/// uploading it to the GPU, and issuing the draw call.
class IPresenter
{
public:
	virtual ~IPresenter() = default;

	/// @brief Build CPU-side vertex/index data from the associated scene.
	virtual void build() = 0;

	/// @brief Upload the built data to GPU buffers.
	virtual void send() = 0;

	/// @brief Issue the draw call using the given camera.
	/// @param camera The camera used for view/projection transforms.
	virtual void render(const Graphics::Camera& camera) = 0;

};
	}
}
