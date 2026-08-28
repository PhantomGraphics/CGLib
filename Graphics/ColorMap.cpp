#include "ColorMap.h"

#include <vector>
#include <cassert>

using namespace Phantom::Graphics;

ColorMap::ColorMap() :
	min_(0.0),
	max_(1.0)
{}

ColorMap::ColorMap(const float min, const float max, const ColorTable& table) :
	min_(min),
	max_(max),
	table(table)
{
}

float ColorMap::getNormalized(const float value) const
{
	return (value - min_) / (max_ - min_);
}

int ColorMap::getIndex(const float value) const
{
	return static_cast<int>(getNormalized(value) * (table.getResolution() - 1));
}

ColorRGBAf ColorMap::getColor(const float v) const
{
	const int index = getIndex(v);
	return table.getColor(index);
}

ColorRGBAf ColorMap::getInterpolatedColor(const float v) const
{
	const auto& colors = table.getColors();
	if (v <= min_) {
		return colors.front();// getColor( min_ );
	}
	if (v >= max_) {
		return colors.back();
	}
	const int index1 = getIndex(v);
	const int index2 = getIndex(v) + 1;

	if (index1 < 0) {
		return colors.front();// getColor( min_ );
	}
	if (index2 >= table.getResolution()) {
		return colors.back();
	}
	const float v1 = getValueFromIndex(index1);
	const float v2 = getValueFromIndex(index2);

	const float ratio = getNormalized(v);

	auto c1 = table.getColor(index1) * (ratio);
	auto c2 = table.getColor(index2) * (1.0f - ratio);
	return c1 + c2;
}

float ColorMap::getValueFromIndex(const int i) const
{
	const float dt = (max_ - min_) / (table.getResolution() - 1);
	return (dt * i + min_);
}

void ColorMap::setMin(const float m)
{
	min_ = m;
	//assert(isValid());
}

float ColorMap::getMin() const
{
	return min_;
}

void ColorMap::setMax(const float m)
{
	max_ = m;
	//assert(isValid());
}

float ColorMap::getMax() const
{
	return max_;
}

void ColorMap::setMinMax(const float min__, const float max__)
{
	this->min_ = min__;
	this->max_ = max__;
	assert(isValid());
}

bool ColorMap::isValid()
{
	return (min_ <= max_);
}