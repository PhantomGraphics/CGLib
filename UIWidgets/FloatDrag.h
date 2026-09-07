#pragma once

#include "IWindow.h"

#include <functional>
#include <string>

namespace Phantom {
	namespace UI {

/**
 * @brief float 値をドラッグで編集するウィジェット（宣言的構成用）．
 *
 * ImGui::DragFloat．速度・範囲・書式は構築時に固定（min == max で範囲なし）．
 * bind() を使わずフォームの下書き値として `getValue()` を読むだけでも使える
 * （その場合も範囲があればクランプされる）．
 */
class FloatDrag : public IWindow
{
public:
	FloatDrag(const std::string& name, float speed,
	          float minValue = 0.f, float maxValue = 0.f,
	          std::string format = "%.3f") :
		IWindow(name),
		speed_(speed),
		min_(minValue),
		max_(maxValue),
		format_(std::move(format))
	{}

	float getValue() const { return value_; }
	void  setValue(float v) { value_ = v; }

	void bind(std::function<float()> getter, std::function<void(float)> setter) {
		getter_ = std::move(getter);
		setter_ = std::move(setter);
	}

	void onShow() override;

private:
	float value_ = 0.f;
	float speed_;
	float min_;
	float max_;
	std::string format_;
	std::function<float()> getter_;
	std::function<void(float)> setter_;
};

	}
}
