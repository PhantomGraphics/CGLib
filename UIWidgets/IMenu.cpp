#include "IMenu.h"
#include "imgui.h"

using namespace Phantom::UI;

void IMenu::onShow()
{
	const auto& c = name.c_str();
	if (ImGui::BeginMenu(c)) {
		for (auto c : children) {
			c->show();
		}
		ImGui::EndMenu();
	}
}