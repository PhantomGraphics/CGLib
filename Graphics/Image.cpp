#include "Image.h"

using namespace Phantom::Graphics;

template<typename T>
Image<T>::Image() :
	width(0),
	height(0)
{}

template<typename T>
Image<T>::Image(const int width, const int height) :
	width(width),
	height(height),
	values(width* height * 4)
{}

template<typename T>
Image<T>::Image(const int width, const int height, const std::vector<T>& values) :
	width(width),
	height(height),
	values(values)
{
	assert((width * height * 4) == values.size());
}

template<typename T>
void Image<T>::fill(const ColorRGBA<T>& c)
{
	for (int j = 0; j < height; ++j) {
		for (int i = 0; i < width; ++i) {
			setColor(i, j, c);
		}
	}
}

template<typename T>
void Image<T>::setColor(const int i, const int j, const ColorRGBA<T>& c)
{
	const auto index = getIndex1d(i, j);
	values[index] = c.r;
	values[index + 1] = c.g;
	values[index + 2] = c.b;
	values[index + 3] = c.a;
}

template<typename T>
ColorRGBA<T> Image<T>::getColor(const int x, const int y) const
{
	const auto index = getIndex1d(x, y);
	const auto r = values[index];
	const auto g = values[index + 1];
	const auto b = values[index + 2];
	const auto a = values[index + 3];
	return ColorRGBAuc(r, g, b, a);
}

template<typename T>
bool Image<T>::isSame(const Image<T>& rhs) const {
	if (width != rhs.width) return false;
	if (height != rhs.height) return false;
	for (int j = 0; j < height; ++j) {
		for (int i = 0; i < width; ++i) {
			const auto c1 = getColor(i, j);
			const auto c2 = rhs.getColor(i, j);
			if (c1 != c2) return false;
		}
	}
	return true;
}


template class Phantom::Graphics::Image<float>;
template class Phantom::Graphics::Image<unsigned char>;