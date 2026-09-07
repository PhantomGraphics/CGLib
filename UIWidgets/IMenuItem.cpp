#include "IMenuItem.h"
#include "imgui.h"

using namespace Phantom::UI;

void IMenuItem::onShow()
{
	const bool enabled = isEnabled();
	if (ImGui::MenuItem(name.c_str(), nullptr, isSelected(), enabled)) {
		onPushed();
	}
	const std::string tip = tooltipText();
	if (!tip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", tip.c_str());
	}
}
