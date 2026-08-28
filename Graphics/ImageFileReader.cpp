#include "ImageFileReader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/stb/stb_image.h"

using namespace Phantom::Graphics;

bool ImageFileReader::read(const std::string& filepath)
{
	unsigned char* pixels;

	const auto str = filepath.c_str();
	pixels = stbi_load(str, &width, &height, &bpp, 0);
	if (pixels == nullptr) {
		return false;
	}

	this->data.resize(width * height * bpp);
	for (int i = 0; i < this->data.size(); ++i) {
		this->data[i] = pixels[i];
	}

	stbi_image_free(pixels);
	return true;
}

Imageuc ImageFileReader::toImage() const
{
	int i = 0;
	Imageuc image(width, height);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const auto r = data[i];
			const auto g = data[i + 1];
			const auto b = data[i + 2];
			if (bpp == 3) {
				image.setColor(x, y, ColorRGBAuc(r, g, b, 255));
			}
			else if (bpp == 4) {
				const auto a = data[i + 3];
				image.setColor(x, y, ColorRGBAuc(r, g, b, a));
			}
			i += bpp;
		}
	}
	return image;
}

bool HDRImageFileReader::read(const std::string& path)
{
	stbi_set_flip_vertically_on_load(true);
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
