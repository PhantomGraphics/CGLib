#include "imgui.h"

#include "IntView.h"

using namespace Phantom::UI;

void IntView::onShow()
{
	ImGui::InputInt(name.c_str(), &value);
}