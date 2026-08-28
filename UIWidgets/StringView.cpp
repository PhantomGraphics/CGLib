#include "StringView.h"

#include "imgui.h"

#include <cstring>

using namespace Phantom::UI;

void StringView::onShow()
{
	const char* s = value.c_str();
	char ss[256];
#ifdef _WIN32
	strcpy_s(ss, s);
#else
	std::strncpy(ss, s, sizeof(ss) - 1);
	ss[sizeof(ss) - 1] = '\0';
#endif
	if (ImGui::InputText(name.c_str(), ss, 256)) {
		value = ss;
	}
}