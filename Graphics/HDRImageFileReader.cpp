#include "ImageFileReader.h"

// Deliberately NOT `#define STB_IMAGE_IMPLEMENTATION` here -- ImageFileReader.cpp already
// provides the one and only implementation for GraphicsCore (see its own comment), and this file
// just calls stbi_loadf() as an ordinary extern, the same pattern GltfRenderer/Gltf/GltfReader.cpp
// and GltfRenderer/Gltf/SkeletonGltfConverter.cpp already use for stbi_load_from_memory().
//
// Split out of ImageFileReader.cpp on 2026-09-15: HDRImageFileReader was unused anywhere in this
// codebase until CGLib/GltfRenderer/IBL/GltfEnvironmentCubemap.cpp started calling it. That new
// reference to a symbol that only exists in ImageFileReader.cpp.obj (a C++ class method, not a
// plain stbi_* function) forced the *entire* ImageFileReader.cpp.obj -- STB_IMAGE_IMPLEMENTATION
// included -- into any executable that needed it. Apps that (like GltfViewer, via
// AnimationCore's VulkanTextureHelper.cpp) already pull in VulkanGraphics/VulkanCubeMap.cpp.obj
// -- which independently carries its *own* full STB_IMAGE_IMPLEMENTATION copy, by design, so
// VulkanGraphicsCore stays linkable without depending on GraphicsCore (e.g. VulkanGraphicsTest
// links VulkanGraphicsCore alone) -- got LNK2005 duplicate-symbol errors once both objects landed
// in the same link. Keeping this method in its own IMPLEMENTATION-free file means its stbi_loadf()
// call resolves from *whichever* stb implementation the final executable already has on hand
// (VulkanCubeMap.cpp.obj's in that combined case, or ImageFileReader.cpp.obj's own -- same
// GraphicsCore.lib archive -- in a GraphicsCore-only/CPU-only build), so no second copy of the
// implementation needs to be pulled in either way.
#include "../ThirdParty/stb/stb_image.h"

using namespace Phantom::Graphics;

bool HDRImageFileReader::read(const std::string& path)
{
	// Explicitly false (stb's own default), not true: an equirectangular environment panorama's
	// row 0 is conventionally its zenith (the direction-to-UV formula used to project it onto a
	// cube -- see GltfIBLPrecomputer::computeEnvironmentCube()'s equirect_to_cube.frag consumers --
	// assumes that unflipped orientation). Also avoids leaking global flip state (stb_image's
	// flip flag is process-wide, not per-call) into unrelated stbi_load()/stbi_load_from_memory()
	// calls elsewhere (e.g. GltfReader.cpp/SkeletonGltfConverter.cpp's glTF texture loading),
	// which would otherwise load upside-down after any HDR file is read first in the same process.
	stbi_set_flip_vertically_on_load(false);
	float* d = stbi_loadf(path.c_str(), &width, &height, &bpp, 0);
	if (d == nullptr) {
		return false;
	}
	const auto size = width * height * bpp;
	this->data.resize(size);
	for (int i = 0; i < size; ++i) {
		this->data[i] = d[i];
	}
	stbi_image_free(d);
	return true;
}

Imagef HDRImageFileReader::toImage() const
{
	int i = 0;
	Imagef image(width, height);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const auto r = data[i];
			const auto g = data[i + 1];
			const auto b = data[i + 2];
			if (bpp == 3) {
				image.setColor(x, y, ColorRGBAf(r, g, b, 1));
			}
			else if (bpp == 4) {
				const auto a = data[i + 3];
				image.setColor(x, y, ColorRGBAf(r, g, b, a));
			}
			i += bpp;
		}
	}
	return image;

}
