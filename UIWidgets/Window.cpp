#include "Window.h"

#include "imgui.h"

using namespace Phantom::UI;

void Window::onShow()
{
	if (open_ && !*open_) return;

	if (hasPos_)  ImGui::SetNextWindowPos({ posX_, posY_ }, ImGuiCond_Once);
	if (hasSize_) ImGui::SetNextWindowSize({ width_, height_ }, ImGuiCond_Once);

	if (ImGui::Begin(name.c_str(), open_)) {
		for (auto c : children) {
			c->show();
		}
	}
	ImGui::End();
}
