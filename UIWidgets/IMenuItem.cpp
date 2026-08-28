#include "IMenuItem.h"
#include "imgui.h"

using namespace Phantom::UI;

void IMenuItem::onShow()
{
	auto str = name.c_str();
	if (ImGui::MenuItem(str)) {
		onPushed();
	}
}