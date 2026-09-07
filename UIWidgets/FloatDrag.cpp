#include "FloatDrag.h"

#include "imgui.h"

using namespace Phantom::UI;

void FloatDrag::onShow()
{
	if (getter_) value_ = getter_();
	if (ImGui::DragFloat(name.c_str(), &value_, speed_, min_, max_, format_.c_str())) {
		if (setter_) setter_(value_);
	}
}
