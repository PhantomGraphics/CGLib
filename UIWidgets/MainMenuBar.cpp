#include "MainMenuBar.h"

#include "imgui.h"

using namespace Phantom::UI;

void MainMenuBar::onShow()
{
	if (ImGui::BeginMainMenuBar()) {
		for (auto c : children) {
			c->show();
		}
		ImGui::EndMainMenuBar();
	}
}
