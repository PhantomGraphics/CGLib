#include "Immediate.h"

#include "imgui.h"

#include <cstdarg>

namespace Phantom::UI::Immediate {

void setNextWindowPosition(float x, float y) { ImGui::SetNextWindowPos({x, y}, ImGuiCond_Once); }
void setNextWindowSize(float width, float height) { ImGui::SetNextWindowSize({width, height}, ImGuiCond_Once); }
bool beginWindow(const char* title, bool* visible) { return ImGui::Begin(title, visible); }
void endWindow() { ImGui::End(); }

bool beginMainMenuBar() { return ImGui::BeginMainMenuBar(); }
void endMainMenuBar() { ImGui::EndMainMenuBar(); }
bool beginMenu(const char* label) { return ImGui::BeginMenu(label); }
void endMenu() { ImGui::EndMenu(); }
bool menuItem(const char* label, bool selected) { return ImGui::MenuItem(label, nullptr, selected); }
bool menuItem(const char* label, bool selected, bool enabled)
{
    return ImGui::MenuItem(label, nullptr, selected, enabled);
}

void tooltipOnHover(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("%s", text);
}

void beginDisabled(bool disabled) { ImGui::BeginDisabled(disabled); }
void endDisabled() { ImGui::EndDisabled(); }

void separator() { ImGui::Separator(); }
void sameLine() { ImGui::SameLine(); }
void spacing() { ImGui::Spacing(); }
void pushId(const char* id) { ImGui::PushID(id); }
void popId() { ImGui::PopID(); }

void text(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ImGui::TextV(format, args);
    va_end(args);
}

void textUnformatted(const char* value) { ImGui::TextUnformatted(value); }

void textDisabled(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ImGui::TextDisabledV(format, args);
    va_end(args);
}

void textWrapped(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    ImGui::TextWrappedV(format, args);
    va_end(args);
}

bool button(const char* label) { return ImGui::Button(label); }
bool checkbox(const char* label, bool& value) { return ImGui::Checkbox(label, &value); }
bool combo(const char* label, int& current, const char* const items[], int itemCount)
{
    return ImGui::Combo(label, &current, items, itemCount);
}
bool collapsingHeader(const char* label) { return ImGui::CollapsingHeader(label); }
bool collapsingHeader(const char* label, bool defaultOpen)
{
    return ImGui::CollapsingHeader(
        label, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
}
bool inputText(const char* label, char* buffer, std::size_t bufferSize)
{
    return ImGui::InputText(label, buffer, bufferSize);
}
bool sliderFloat(const char* label, float& value, float minimum, float maximum, const char* format)
{
    return ImGui::SliderFloat(label, &value, minimum, maximum, format);
}
bool sliderInt(const char* label, int& value, int minimum, int maximum)
{
    return ImGui::SliderInt(label, &value, minimum, maximum);
}
bool dragFloat(const char* label, float& value, float speed, float minimum, float maximum)
{
    return ImGui::DragFloat(label, &value, speed, minimum, maximum);
}
bool dragFloat3(const char* label, float value[3], float speed, float minimum, float maximum)
{
    return ImGui::DragFloat3(label, value, speed, minimum, maximum);
}

} // namespace Phantom::UI::Immediate
