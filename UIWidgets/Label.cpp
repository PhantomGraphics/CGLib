#include "Label.h"

#include "imgui.h"

using namespace Phantom::UI;

void Label::onShow()
{
	if (provider_) text_ = provider_();

	switch (style_) {
	case Style::Disabled:
		ImGui::TextDisabled("%s", text_.c_str());
		break;
	case Style::Wrapped:
		ImGui::TextWrapped("%s", text_.c_str());
		break;
	case Style::Normal:
	default:
		ImGui::TextUnformatted(text_.c_str());
		break;
	}
}
