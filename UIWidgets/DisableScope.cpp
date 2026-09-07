#include "DisableScope.h"

#include "imgui.h"

using namespace Phantom::UI;

void DisableScope::onShow()
{
	const bool disabled = disabledWhen_ && disabledWhen_();

	ImGui::BeginDisabled(disabled);
	for (auto c : children) {
		c->show();
	}
	ImGui::EndDisabled();

	const std::string tip = tooltipProvider_ ? tooltipProvider_() : tooltip_;
	if (!tip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", tip.c_str());
	}
}
