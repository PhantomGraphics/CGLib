#include "Button.h"
#include "imgui.h"

using namespace Phantom::UI;

void Button::onShow()
{
	auto str = name.c_str();
	if (ImGui::Button(str)) {
		func();
	}
}