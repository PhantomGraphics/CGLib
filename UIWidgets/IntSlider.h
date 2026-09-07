#pragma once

#include "IWindow.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief int 値をスライダーで編集するウィジェット（宣言的構成用）．
 *
 * ImGui::SliderInt．範囲は構築時に固定．bind() の契約は FloatSlider と同じ．
 */
class IntSlider : public IWindow
{
public:
	IntSlider(const std::string& name, int minValue, int maxValue) :
		IWindow(name),
		min_(minValue),
		max_(maxValue)
	{}

	int  getValue() const { return value_; }
	void setValue(int v) { value_ = v; }

	void bind(std::function<int()> getter, std::function<void(int)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

	void onShow() override;

private:
	int value_ = 0;
	int min_;
	int max_;
	std::function<int()> getter_;
	std::function<void(int)> setter_;
};

	}
}
