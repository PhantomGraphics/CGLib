#include "FloatSlider.h"

#include "imgui.h"

using namespace Phantom::UI;

void FloatSlider::onShow()
{
	if (getter_) value_ = getter_();
	if (ImGui::SliderFloat(name.c_str(), &value_, min_, max_, format_.c_str())) {
		if (setter_) setter_(value_);
	}
}
