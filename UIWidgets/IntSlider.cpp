#include "IntSlider.h"

#include "imgui.h"

using namespace Phantom::UI;

void IntSlider::onShow()
{
	if (getter_) value_ = getter_();
	if (ImGui::SliderInt(name.c_str(), &value_, min_, max_)) {
		if (setter_) setter_(value_);
	}
}
