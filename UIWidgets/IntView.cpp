#include "imgui.h"

#include "IntView.h"

using namespace Phantom::UI;

void IntView::onShow()
{
	if (getter_) value = getter_();
	if (ImGui::InputInt(name.c_str(), &value)) {
		if (setter_) setter_(value);
	}
}
