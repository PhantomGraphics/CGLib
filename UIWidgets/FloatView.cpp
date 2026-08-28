#include "FloatView.h"

#include "imgui.h"

using namespace Phantom::UI;

void FloatView::onShow()
{
	ImGui::InputFloat(name.c_str(), &value);
}