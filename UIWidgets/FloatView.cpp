#include "FloatView.h"

#include "imgui.h"

using namespace Phantom::UI;

void FloatView::onShow()
{
	if (getter_) value = getter_();
	if (ImGui::InputFloat(name.c_str(), &value)) {
		if (setter_) setter_(value);
	}
}
