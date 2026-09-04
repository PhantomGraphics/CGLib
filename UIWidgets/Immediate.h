#pragma once

#include <cstddef>

namespace Phantom::UI::Immediate {

void setNextWindowPosition(float x, float y);
void setNextWindowSize(float width, float height);
bool beginWindow(const char* title, bool* visible = nullptr);
void endWindow();

bool beginMainMenuBar();
void endMainMenuBar();
bool beginMenu(const char* label);
void endMenu();
bool menuItem(const char* label, bool selected = false);
bool menuItem(const char* label, bool selected, bool enabled);

// Shows a tooltip for the most recently submitted item while it is hovered
// (including when that item was disabled via beginDisabled()).
void tooltipOnHover(const char* text);

// RAII-free disabled scope: widgets submitted between these calls are greyed
// out and non-interactive. Calls must be balanced.
void beginDisabled(bool disabled = true);
void endDisabled();

void separator();
void sameLine();
void spacing();
void pushId(const char* id);
void popId();

void text(const char* format, ...);
void textUnformatted(const char* value);
void textDisabled(const char* format, ...);
void textWrapped(const char* format, ...);

bool button(const char* label);
bool checkbox(const char* label, bool& value);
bool combo(const char* label, int& current, const char* const items[], int itemCount);
bool collapsingHeader(const char* label);
bool collapsingHeader(const char* label, bool defaultOpen);
bool inputText(const char* label, char* buffer, std::size_t bufferSize);
bool sliderFloat(const char* label, float& value, float minimum, float maximum,
                 const char* format = "%.3f");
bool sliderInt(const char* label, int& value, int minimum, int maximum);
bool dragFloat(const char* label, float& value, float speed,
               float minimum = 0.0f, float maximum = 0.0f);
bool dragFloat3(const char* label, float value[3], float speed,
                float minimum = 0.0f, float maximum = 0.0f);

} // namespace Phantom::UI::Immediate
