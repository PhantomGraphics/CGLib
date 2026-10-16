#include "ColorTable.h"

using namespace Phantom::Graphics;

ColorTable::ColorTable(const int resolution) :
	colors(resolution)
{
}

int ColorTable::getResolution() const
{
	return static_cast<int>(colors.size());
}

void ColorTable::setColor(const int index, const ColorRGBAf& color)
{
	colors[index] = color;
}

ColorRGBAf ColorTable::getColor(const int i) const
{
	if (i >= colors.size()) {
		return colors.back();
	}
	return colors[i];
}

ColorTable ColorTable::createJetTable(const int resolution)
{
	ColorTable table(resolution);
	for (int i = 0; i < resolution; ++i) {
		const float v = static_cast<float>(i) / static_cast<float>(resolution - 1);
		float r = std::min(std::max(1.5f - std::abs(4.0f * v - 3.0f), 0.0f), 1.0f);
		float g = std::min(std::max(1.5f - std::abs(4.0f * v - 2.0f), 0.0f), 1.0f);
		float b = std::min(std::max(1.5f - std::abs(4.0f * v - 1.0f), 0.0f), 1.0f);
		table.setColor(i, ColorRGBAf(r, g, b, 1.0f));
	}
	return table;
}
